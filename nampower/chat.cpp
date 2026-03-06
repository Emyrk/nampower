#include "chat.hpp"

#include "logging.hpp"
#include "offsets.hpp"

#include <cstdint>
#include <cstring>
#include <windows.h>

namespace Nampower {
    // Server-side context for why this patch exists:
    // The packet builder sends senderGuid twice for a subset of message types:
    //   case CHAT_MSG_SAY:
    //   case CHAT_MSG_PARTY:
    //   case CHAT_MSG_YELL:
    //       data << ObjectGuid(senderGuid);
    //       data << ObjectGuid(senderGuid);
    //       break;
    //
    // The second senderGuid is used by the client chat-bubble path. Many other chat
    // message types only send senderGuid once:
    //   default:
    //       data << ObjectGuid(senderGuid);
    //       break;
    //
    // For those single-guid messages, chat bubbles typically do not appear unless we
    // mirror the expected second value at this callsite.
    __declspec(naked) void OnChatMessagePatchFn() {
        // Handle selected chat types:
        // 0x02 = CHAT_CMD_RAID
        // 0x06 = CHAT_CMD_WHISPER
        // 0x57 = CHAT_CMD_RAID_LEADER
        // 0x58 = CHAT_CMD_RAID_WARNING
        // 0x5C = CHAT_CMD_BATTLEGROUND
        // 0x5D = CHAT_CMD_BATTLEGROUND_LEADER
        __asm {
            mov ecx, dword ptr [ebp - 0x8]
            mov edx, dword ptr [ebp - 0xC]
            movzx eax, byte ptr [ebp + 0xF]

            cmp al, 0x2
            je patch_set
            cmp al, 0x6
            je patch_set
            cmp al, 0x57
            je patch_set
            cmp al, 0x58
            je patch_set
            cmp al, 0x5C
            je patch_set
            cmp al, 0x5D
            jne patch_done

        patch_set:
            mov dword ptr [ebp - 0x34], ecx
            mov dword ptr [ebp - 0x38], edx

        patch_done:
            ret
        }
    }

    void ApplyOnChatMessagePatch() {
        auto *patchAddr = reinterpret_cast<std::uint8_t *>(
            static_cast<std::uintptr_t>(Offsets::OnChatMessagePatchCallSite));
        auto const patchTarget = reinterpret_cast<std::uintptr_t>(&OnChatMessagePatchFn);
        auto const rel32 = static_cast<std::int32_t>(
            patchTarget - (reinterpret_cast<std::uintptr_t>(patchAddr) + 5));

        DWORD oldProtect = 0;
        if (!VirtualProtect(patchAddr, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            DEBUG_LOG("Failed to apply OnChatMessage patch: VirtualProtect RW failed");
            return;
        }

        patchAddr[0] = 0xE8;
        std::memcpy(patchAddr + 1, &rel32, sizeof(rel32));
        patchAddr[5] = 0x90;

        DWORD restoreProtect = 0;
        VirtualProtect(patchAddr, 6, oldProtect, &restoreProtect);
        FlushInstructionCache(GetCurrentProcess(), patchAddr, 6);

        DEBUG_LOG("Applied OnChatMessage patch at 0x" << std::hex
            << static_cast<std::uint32_t>(Offsets::OnChatMessagePatchCallSite)
            << std::dec);
    }

    void CGUnit_C_AddChatBubbleHook(hadesmem::PatchDetourBase *detour,
                                    uintptr_t *unitPtr,
                                    void *dummy_edx,
                                    game::CHAT_COMMAND_ID chatType,
                                    char *msg) {
        const char *bubbleEnabledCvar = nullptr;
        switch (chatType) {
            case game::CHAT_CMD_SAY:
            case game::CHAT_CMD_YELL:
                bubbleEnabledCvar = "ChatBubbles";
                break;
            case game::CHAT_CMD_PARTY:
                bubbleEnabledCvar = "ChatBubblesParty";
                break;
            case game::CHAT_CMD_WHISPER:
            case game::CHAT_CMD_WHISPER_INFORM:
                bubbleEnabledCvar = "NP_ChatBubblesWhisper";
                break;
            case game::CHAT_CMD_RAID:
            case game::CHAT_CMD_RAID_LEADER:
            case game::CHAT_CMD_RAID_WARNING:
                bubbleEnabledCvar = "NP_ChatBubblesRaid";
                break;
            case game::CHAT_CMD_BATTLEGROUND:
            case game::CHAT_CMD_BATTLEGROUND_LEADER:
                bubbleEnabledCvar = "NP_ChatBubblesBattleground";
                break;
            default:
                break;
        }

        if (bubbleEnabledCvar) {
            // Check if the bubble cvar is enabled.
            auto const cvarLookup = hadesmem::detail::AliasCast<CVarLookupT>(Offsets::CVarLookup);
            auto *cvar = cvarLookup(bubbleEnabledCvar);
            if (!cvar || cvar->m_intValue == 0) return;

            // Get active player object.
            uint64_t playerGuid = game::ClntObjMgrGetActivePlayerGuid();
            auto *playerObj = game::GetObjectPtr(playerGuid);
            if (!playerObj) return;

            auto unitPos = game::UnitGetPosition(unitPtr);
            auto playerPos = game::UnitGetPosition(playerObj);

            // Distance check against NP_ChatBubbleDistance (cached as squared float).
            float distLimitSq = *reinterpret_cast<float *>(Offsets::ChatBubbleDistanceSq);
            float dx = playerPos.x - unitPos.x;
            float dy = playerPos.y - unitPos.y;
            float dz = playerPos.z - unitPos.z;
            if (dx * dx + dy * dy + dz * dz > distLimitSq) return;

            // Get chat color.
            uint32_t color = 0;
            auto const getChatColor = hadesmem::detail::AliasCast<CGChat_GetChatColorT>(Offsets::CGChat_GetChatColor);
            getChatColor(&color, chatType);

            // Remove any existing bubble on this unit.
            auto const removeBubble = hadesmem::detail::AliasCast<CGUnit_RemoveChatBubbleT>(Offsets::CGUnit_RemoveChatBubble);
            removeBubble(unitPtr, nullptr);

            // Get a new bubble frame from the current frame.
            auto *currentFrame = *reinterpret_cast<void **>(Offsets::CurrentFramePointer);
            if (!currentFrame) return;

            auto const getNewBubble = hadesmem::detail::AliasCast<CGChatBubbleFrame_GetNewChatBubbleT>(Offsets::CGChatBubbleFrame_GetNewChatBubble);
            uintptr_t *bubbleFrame = getNewBubble(currentFrame);
            if (!bubbleFrame) return;
            unitPtr[0x399] = reinterpret_cast<uintptr_t>(bubbleFrame); // store bubble frame pointer in unit

            // Read unit GUID from CGObject::m_obj->m_guid.
            uint64_t unitGuid = game::UnitGetGuid(unitPtr);
            if (unitGuid == 0) return;

            // Initialize the bubble with the unit GUID, message, and color.
            auto const initBubble = hadesmem::detail::AliasCast<CGChatBubbleFrame_InitializeT>(Offsets::CGChatBubbleFrame_Initialize);
            initBubble(bubbleFrame, &unitGuid, msg, &color);
        } else {
            // Just call original.
            auto const orig = detour->GetTrampolineT<CGUnit_C_AddChatBubbleT>();
            orig(unitPtr, dummy_edx, chatType, msg);
        }
    }
}
