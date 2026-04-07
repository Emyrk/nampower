//
// Created by pmacc on 2/15/2026.
//

#include "player_scripts.hpp"
#include "auras.hpp"
#include "helper.hpp"
#include "items.hpp"
#include "offsets.hpp"
#include <string>

namespace Nampower {
    static uint32_t GetPlayerMovementFlags() {
        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        if (playerGuid == 0) return 0;

        auto unit = game::GetObjectPtr(playerGuid);
        if (!unit) return 0;

        // Movement flags are at offset 0x9E8 from unit base (divide by 4 for uintptr_t* indexing)
        return *(unit + 0x27A);
    }

    uint32_t Script_PlayerIsMoving(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto movementFlags = GetPlayerMovementFlags();
        constexpr uint32_t MOVEFLAG_MASK_MOVING =
            game::MOVEFLAG_FORWARD | game::MOVEFLAG_BACKWARD |
            game::MOVEFLAG_STRAFE_LEFT | game::MOVEFLAG_STRAFE_RIGHT |
            game::MOVEFLAG_PITCH_UP | game::MOVEFLAG_PITCH_DOWN |
            game::MOVEFLAG_JUMPING | game::MOVEFLAG_FALLINGFAR |
            game::MOVEFLAG_SPLINE_ELEVATION;

        if (movementFlags & MOVEFLAG_MASK_MOVING) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnil(luaState);
        }
        return 1;
    }

    uint32_t Script_PlayerIsRooted(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto movementFlags = GetPlayerMovementFlags();
        if (movementFlags & game::MOVEFLAG_ROOT) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnil(luaState);
        }
        return 1;
    }

    uint32_t Script_PlayerIsSwimming(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto movementFlags = GetPlayerMovementFlags();
        if (movementFlags & game::MOVEFLAG_SWIMMING) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnil(luaState);
        }
        return 1;
    }

    uint32_t Script_LearnTalentRank(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1) || !lua_isnumber(luaState, 2) || !lua_isnumber(luaState, 3)) {
            lua_error(luaState, "Usage: LearnTalentRank(talentPage, talentIndex, rank)");
            return 0;
        }

        constexpr uintptr_t CGTalentInfo_m_talentTabs = 0x00BDCD24;
        constexpr uintptr_t CGTalentInfo_m_talents = 0x00BDCD28;
        constexpr uint32_t CMSG_LEARN_TALENT = 0x0251;

        auto talentPage = static_cast<int32_t>(lua_tonumber(luaState, 1));
        auto talentIndex = static_cast<int32_t>(lua_tonumber(luaState, 2));
        auto requestedRank = static_cast<int32_t>(lua_tonumber(luaState, 3));

        if (talentPage < 1 || talentPage > 3) {
            lua_error(luaState, "LearnTalentRank: talentPage must be in range 1-3");
            return 0;
        }
        if (talentIndex < 1 || talentIndex > 32) {
            lua_error(luaState, "LearnTalentRank: talentIndex must be in range 1-32");
            return 0;
        }
        if (requestedRank < 1 || requestedRank > 5) {
            lua_error(luaState, "LearnTalentRank: rank must be in range 1-5");
            return 0;
        }

        auto const playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto *playerUnit = game::ClntObjMgrObjectPtr(game::TYPEMASK_PLAYER, playerGuid);
        if (!playerUnit) {
            lua_error(luaState, "LearnTalentRank: active player not available");
            return 0;
        }

        auto talentPage0Index = talentPage - 1;
        auto talentIndex0 = talentIndex - 1;
        int32_t talentTabCount = 0;
        if (!SafeRead(reinterpret_cast<void *>(CGTalentInfo_m_talentTabs), talentTabCount)) {
            lua_error(luaState, "LearnTalentRank: unable to read talent tab count");
            return 0;
        }

        if (talentPage0Index < 0 || talentPage0Index >= talentTabCount || talentIndex0 < 0) {
            lua_error(luaState, "LearnTalentRank: talent page/index out of bounds");
            return 0;
        }

        uintptr_t talentsBase = 0;
        if (!SafeRead(reinterpret_cast<void *>(CGTalentInfo_m_talents), talentsBase)) {
            lua_error(luaState, "LearnTalentRank: unable to read talents base pointer");
            return 0;
        }

        if (talentsBase == 0) {
            lua_error(luaState, "LearnTalentRank: talents base pointer is null");
            return 0;
        }

        auto talentTabSlotAddr = talentsBase + static_cast<uintptr_t>(talentPage0Index) * sizeof(uint32_t);
        uintptr_t talentTabPtr = 0;
        if (!SafeRead(reinterpret_cast<void *>(talentTabSlotAddr), talentTabPtr)) {
            lua_error(luaState, "LearnTalentRank: unable to read talent tab pointer");
            return 0;
        }

        if (talentTabPtr == 0) {
            lua_error(luaState, "LearnTalentRank: talent tab pointer is null");
            return 0;
        }

        uint32_t talentCountInTab = 0;
        uintptr_t talentListBase = 0;
        if (!SafeRead(reinterpret_cast<void *>(talentTabPtr + 0x8), talentCountInTab) ||
            !SafeRead(reinterpret_cast<void *>(talentTabPtr + 0xC), talentListBase)) {
            lua_error(luaState, "LearnTalentRank: unable to read talent tab metadata");
            return 0;
        }

        if (talentCountInTab == 0 || talentCountInTab >= 512 || talentListBase == 0) {
            lua_error(luaState, "LearnTalentRank: invalid talent tab metadata");
            return 0;
        }

        if (talentIndex0 >= static_cast<int32_t>(talentCountInTab)) {
            lua_error(luaState, "LearnTalentRank: talent index not present in selected tab");
            return 0;
        }

        auto talentEntryPtr = talentListBase + static_cast<uintptr_t>(talentIndex0) * 0x54;
        int32_t talentId = 0;
        if (!SafeRead(reinterpret_cast<void *>(talentEntryPtr), talentId)) {
            lua_error(luaState, "LearnTalentRank: unable to read talent entry");
            return 0;
        }
        if (talentId <= 0 || talentId >= 200000) {
            lua_error(luaState, "LearnTalentRank: resolved invalid talentId");
            return 0;
        }

        auto requestedRank0Index = requestedRank - 1;
        auto dataStore = CDataStore();
        dataStore.Put<uint32_t>(CMSG_LEARN_TALENT);
        dataStore.Put<uint32_t>(static_cast<uint32_t>(talentId));
        dataStore.Put<uint32_t>(static_cast<uint32_t>(requestedRank0Index));
        dataStore.Finalize();

        auto const clientServicesSend = reinterpret_cast<ClientServices_SendT>(Offsets::ClientServices_Send);
        clientServicesSend(&dataStore);

        lua_pushnumber(luaState, 1);
        return 1;
    }

    uint32_t Script_DisenchantAll(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1) && !lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: DisenchantAll(itemIdOrName, [includeSoulbound]) or DisenchantAll(quality, [includeSoulbound])\n"
                "quality: \"greens\", \"blues\", \"purples\", or combinations like \"greens|blues\" or \"greens|blues|purples\"\n"
                "itemIdOrName: item ID (number) or item name (string)\n"
                "includeSoulbound: optional number - pass 1 to include soulbound items (defaults to 0)");
            return 0;
        }

        constexpr uint32_t DISENCHANT_SPELL_ID = 13262;
        bool hasDisenchant = false;
        for (uint32_t slot = 0; slot < 1024; slot++) {
            uint32_t spellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownSpells) + slot * 4);
            if (spellId == DISENCHANT_SPELL_ID) {
                hasDisenchant = true;
                break;
            }
        }

        if (!hasDisenchant) {
            lua_error(luaState, "You do not have the Disenchant spell");
            return 0;
        }

        ResetDisenchantState();

        if (lua_isnumber(luaState, 1)) {
            gDisenchantItemId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        } else if (lua_isstring(luaState, 1)) {
            const char *param = lua_tostring(luaState, 1);

            bool isQualityString = false;
            uint32_t qualityBitmask = 0;

            std::string paramStr(param);
            size_t start = 0;
            size_t end = 0;

            while (end != std::string::npos) {
                end = paramStr.find('|', start);
                std::string token = paramStr.substr(start, (end == std::string::npos) ? std::string::npos : end - start);

                if (_stricmp(token.c_str(), "greens") == 0) {
                    qualityBitmask |= DISENCHANT_QUALITY_GREEN;
                    isQualityString = true;
                } else if (_stricmp(token.c_str(), "blues") == 0) {
                    qualityBitmask |= DISENCHANT_QUALITY_BLUE;
                    isQualityString = true;
                } else if (_stricmp(token.c_str(), "purples") == 0) {
                    qualityBitmask |= DISENCHANT_QUALITY_PURPLE;
                    isQualityString = true;
                }

                start = (end == std::string::npos) ? std::string::npos : end + 1;
            }

            if (isQualityString) {
                gDisenchantQuality = qualityBitmask;
            } else {
                uint32_t cachedItemId = GetItemIdFromCache(param);
                if (cachedItemId != 0) {
                    gDisenchantItemId = cachedItemId;
                } else {
                    auto itemSearchResult = FindPlayerItem(0, param);
                    if (itemSearchResult.found()) {
                        gDisenchantItemId = game::GetItemId(itemSearchResult.item);
                        CacheItemNameToId(param, gDisenchantItemId);
                    } else {
                        lua_pushnumber(luaState, 0);
                        return 1;
                    }
                }
            }
        }

        if (lua_isnumber(luaState, 2)) {
            gDisenchantIncludeSoulbound = (lua_tonumber(luaState, 2) != 0);
        }

        bool success = TryDisenchant();

        lua_pushnumber(luaState, success ? 1 : 0);
        return 1;
    }

    bool TryDisenchant() {
        DEBUG_LOG("Trying disenchant with quality filter " << gDisenchantQuality << " itemId " << gDisenchantItemId);
        if (gDisenchantQuality == 0 && gDisenchantItemId == 0) {
            ResetDisenchantState();
            return false;
        }

        PlayerItemSearchResult itemSearchResult;
        if (gDisenchantItemId != 0) {
            itemSearchResult = FindPlayerItem(gDisenchantItemId, nullptr);
        } else {
            itemSearchResult = FindPlayerDisenchantItem(gDisenchantQuality, gDisenchantIncludeSoulbound);
        }

        if (!itemSearchResult.found()) {
            DEBUG_LOG("No disenchantable item found, stopping disenchant");
            LuaCall("DEFAULT_CHAT_FRAME:AddMessage(\"No more items to disenchant.\")");
            ResetDisenchantState();
            return false;
        }

        auto item = itemSearchResult.item;
        if (!item || !item->object.m_obj) {
            DEBUG_LOG("Invalid item, stopping disenchant");
            ResetDisenchantState();
            return false;
        }
        uint64_t itemGuid = item->object.m_obj->m_guid;

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto playerUnit = game::GetObjectPtr(playerGuid);

        if (!playerUnit) {
            DEBUG_LOG("No player unit, stopping disenchant");
            ResetDisenchantState();
            return false;
        }

        char luaCode[256];
        snprintf(luaCode, sizeof(luaCode),
                 "local itemLink = GetContainerItemLink(%d, %d); "
                 "if itemLink then DEFAULT_CHAT_FRAME:AddMessage(\"Disenchanting \" .. itemLink .. \" move during cast to cancel.\") end",
                 itemSearchResult.bagIndex, itemSearchResult.slot + 1);
        LuaCall(luaCode);

        auto currentTime = GetTime();
        gNextDisenchantTimeMs = currentTime + 5000;

        auto const castSpell = reinterpret_cast<Spell_C_CastSpellT>(Offsets::Spell_C_CastSpell);
        bool success = castSpell(playerUnit, 13262, nullptr, itemGuid);

        if (!success) {
            DEBUG_LOG("Cast failed, stopping disenchant");
            LuaCall("DEFAULT_CHAT_FRAME:AddMessage(\"Disenchant interrupted or failed.\")");
            ResetDisenchantState();
            return false;
        }

        return true;
    }

    uint32_t Script_GetPlayerAuraDuration(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: GetPlayerAuraDuration(auraSlot)");
            return 0;
        }

        auto auraSlot = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        if (auraSlot >= MAX_AURA_SLOTS) {
            lua_pushnil(luaState);
            return 1;
        }

        auto *unitFields = game::GetActivePlayerUnitFields();
        if (!unitFields) {
            lua_pushnil(luaState);
            return 1;
        }
        uint32_t spellId = unitFields->aura[auraSlot];

        uint32_t expirationTime = gAuraExpirationTime[auraSlot];
        auto now = static_cast<uint32_t>(GetWowTimeMs() & 0xFFFFFFFF);
        int remainingDurationMs = expirationTime > now ? static_cast<int>(expirationTime - now) : 0;

        lua_pushnumber(luaState, spellId);
        lua_pushnumber(luaState, remainingDurationMs);
        lua_pushnumber(luaState, expirationTime);
        return 3;
    }

    uint32_t Script_CancelPlayerAuraSlot(uintptr_t *luaState) {
        constexpr uint32_t kMaxCancelAuraSlots = 32;
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: CancelPlayerAuraSlot(auraSlot)");
            return 0;
        }

        auto auraSlot = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        if (auraSlot >= kMaxCancelAuraSlots) {
            lua_pushnumber(luaState, 0);
            return 1;
        }

        auto *unitFields = game::GetActivePlayerUnitFields();
        if (!unitFields) {
            lua_pushnumber(luaState, 0);
            return 1;
        }

        auto spellId = static_cast<int>(unitFields->aura[auraSlot]);
        if (spellId == 0) {
            lua_pushnumber(luaState, 0);
            return 1;
        }

        auto const cancelAura = reinterpret_cast<void(__fastcall *)(int)>(Offsets::Spell_C_CancelAura);
        cancelAura(spellId);

        lua_pushnumber(luaState, 1);
        return 1;
    }

    uint32_t Script_CancelPlayerAuraSpellId(uintptr_t *luaState) {
        constexpr uint32_t kMaxCancelAuraSlots = 32;
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: CancelPlayerAuraSpellId(spellId, [ignoreMissing])");
            return 0;
        }

        auto spellId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        if (spellId == 0) {
            lua_pushnumber(luaState, 0);
            return 1;
        }

        bool ignoreMissing = false;
        if (lua_isnumber(luaState, 2)) {
            ignoreMissing = (static_cast<int>(lua_tonumber(luaState, 2)) == 1);
        }

        if (!ignoreMissing) {
            auto *unitFields = game::GetActivePlayerUnitFields();
            if (!unitFields) {
                lua_pushnumber(luaState, 0);
                return 1;
            }

            bool foundSpell = false;
            for (uint32_t auraSlot = 0; auraSlot < kMaxCancelAuraSlots; ++auraSlot) {
                if (unitFields->aura[auraSlot] == spellId) {
                    foundSpell = true;
                    break;
                }
            }

            if (!foundSpell) {
                lua_pushnumber(luaState, 0);
                return 1;
            }
        }

        auto const cancelAura = reinterpret_cast<void(__fastcall *)(uint32_t)>(Offsets::Spell_C_CancelAura);
        cancelAura(spellId);

        lua_pushnumber(luaState, 1);
        return 1;
    }
}
