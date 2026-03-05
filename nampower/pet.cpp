//
// Created by pmacc on 3/1/2026.
//

#include "pet.hpp"

#include "game.hpp"
#include "helper.hpp"
#include "logging.hpp"
#include "offsets.hpp"

#include <map>
#include <cstdlib>
#include <sstream>
#include <string>

namespace Nampower {
    constexpr uint32_t kAutocastEnabledType = 0xC1;
    constexpr uint32_t kAutocastDisabledType = 0x81;
    constexpr uint32_t kReactStateAggressive = 0x06000002; // REACT_AGGRESSIVE = 2
    constexpr uint32_t kReactStateDefensive  = 0x06000001; // REACT_DEFENSIVE  = 1
    constexpr uint32_t kReactStatePassive    = 0x06000000; // REACT_PASSIVE    = 0
    constexpr uint32_t kFelguardDisplayId = 7970;
    constexpr uint32_t kDoomguardDisplayId = 1912;
    // constexpr uint32_t kDoomguardDisplayId = 4449;  // imp for testing
    constexpr uint32_t kInfernalDisplayId = 169;
    uint32_t gPendingToggleAutocastSlot = 0;

    uint32_t GetActivePetDisplayId() {
        auto const activePetGuid = *reinterpret_cast<uint64_t *>(Offsets::ActivePetGuid);
        if (activePetGuid == 0) {
            return 0;
        }

        auto *petUnit = game::GetObjectPtr(activePetGuid);
        if (!petUnit) {
            return 0;
        }

        auto *unitFields = *reinterpret_cast<game::UnitFields **>(petUnit + 68);
        if (!unitFields) {
            return 0;
        }

        return unitFields->displayId;
    }

    const char *GetAutocastCvarNameForDisplayId(uint32_t displayId) {
        if (displayId == kFelguardDisplayId) {
            return "NP_FelguardAutocastData";
        }
        if (displayId == kDoomguardDisplayId) {
            return "NP_DoomguardAutocastData";
        }
        if (displayId == kInfernalDisplayId) {
            return "NP_InfernalAutocastData";
        }
        return nullptr;
    }

    std::string SerializeAutocastData(const std::map<uint32_t, uint32_t> &autocastData) {
        std::ostringstream out;
        bool first = true;
        for (auto const &entry: autocastData) {
            if (!first) {
                out << ",";
            }
            out << entry.first << ":" << entry.second;
            first = false;
        }
        return out.str();
    }

    void ParseAutocastData(const std::string &serialized, std::map<uint32_t, uint32_t> &autocastData) {
        autocastData.clear();
        if (serialized.empty()) {
            return;
        }

        size_t start = 0;
        while (start < serialized.size()) {
            size_t end = serialized.find(',', start);
            if (end == std::string::npos) {
                end = serialized.size();
            }

            std::string token = serialized.substr(start, end - start);
            size_t colonPos = token.find(':');
            if (colonPos != std::string::npos && colonPos > 0 && colonPos + 1 < token.size()) {
                std::string slotStr = token.substr(0, colonPos);
                std::string actionStr = token.substr(colonPos + 1);

                char *slotEnd = nullptr;
                char *actionEnd = nullptr;
                unsigned long slotValue = std::strtoul(slotStr.c_str(), &slotEnd, 10);
                unsigned long actionValue = std::strtoul(actionStr.c_str(), &actionEnd, 10);

                if (slotEnd && *slotEnd == '\0' &&
                    actionEnd && *actionEnd == '\0' &&
                    slotValue <= 0xFFFFFFFFul &&
                    actionValue <= 0xFFFFFFFFul) {
                    autocastData[static_cast<uint32_t>(slotValue)] = static_cast<uint32_t>(actionValue);
                }
            }

            start = end + 1;
        }
    }

    void SaveGreaterDemonPreferences(uint32_t spellSlot, uint32_t actionData) {
        uint32_t displayId = GetActivePetDisplayId();
        if (displayId != kFelguardDisplayId && displayId != kDoomguardDisplayId && displayId != kInfernalDisplayId) {
            return;
        }

        const char *cvarName = GetAutocastCvarNameForDisplayId(displayId);
        if (!cvarName) {
            return;
        }

        const char *cvarDataPtr = GetStringCvar(cvarName);
        std::string cvarData = cvarDataPtr ? cvarDataPtr : "";

        std::map<uint32_t, uint32_t> autocastData;
        ParseAutocastData(cvarData, autocastData);

        uint32_t actionId = game::UnitActionButtonAction(actionData);
        uint32_t type = game::UnitActionButtonType(actionData);

        if (spellSlot >= 3 && spellSlot <= 6) {
            if (type == kAutocastEnabledType) {
                autocastData[spellSlot] = actionData;
            } else if (type == kAutocastDisabledType) {
                autocastData.erase(spellSlot);
            }
        } else if (spellSlot >= 7 && spellSlot <= 9) {
            // React states are mutually exclusive — clear all three, save the selected one
            autocastData.erase(7);
            autocastData.erase(8);
            autocastData.erase(9);
            autocastData[spellSlot] = actionData;
        } else {
            return;
        }

        std::string serialized = SerializeAutocastData(autocastData);
        std::string lua = "SetCVar(\"";
        lua += cvarName;
        lua += "\",\"";
        lua += serialized;
        lua += "\")";
        LuaCall(lua.c_str());

        DEBUG_LOG("Saved " << cvarName << "=" << serialized
            << " (slot=" << spellSlot << ", actionData=" << actionData
            << ", actionId=" << actionId << ", type=0x" << std::hex << type << std::dec << ")");
    }

    void RestoreGreaterDemonAutocastPreferences() {
        uint32_t displayId = GetActivePetDisplayId();
        const char *cvarName = GetAutocastCvarNameForDisplayId(displayId);

        if (!cvarName) {
            return;
        }

        DEBUG_LOG("Restoring greater demon autocast for displayId=" << displayId << " using cvar " << cvarName);

        const char *cvarDataPtr = GetStringCvar(cvarName);
        std::string cvarData = cvarDataPtr ? cvarDataPtr : "";

        if (cvarData.empty()) {
            return;
        }

        std::map<uint32_t, uint32_t> autocastData;
        ParseAutocastData(cvarData, autocastData);
        if (autocastData.empty()) {
            return;
        }

        auto const petGuid = *reinterpret_cast<uint64_t *>(Offsets::ActivePetGuid);
        if (petGuid == 0) {
            return;
        }

        auto const clientServicesSend = reinterpret_cast<ClientServices_SendT>(Offsets::ClientServices_Send);
        if (!clientServicesSend) {
            return;
        }

        constexpr uint32_t CMSG_PET_SET_ACTION = 372;
        constexpr uint32_t CMSG_PET_ACTION = 373;
        uint32_t packetsSent = 0;

        for (auto const &entry: autocastData) {
            uint32_t spellSlot = entry.first;
            uint32_t actionData = entry.second;

            if (spellSlot < 3 || spellSlot > 9) {
                continue;
            }

            auto *petActionBarSlots = reinterpret_cast<uint32_t *>(Offsets::PetActionBarSlots);
            petActionBarSlots[spellSlot] = actionData;

            if (spellSlot >= 7) {
                // React state: replicate SetPetReactionMode — write react state into lower byte of PetReactionMode
                auto *reactionMode = reinterpret_cast<uint32_t *>(Offsets::PetReactionMode);
                uint32_t reactState = game::UnitActionButtonAction(actionData);
                *reactionMode = (*reactionMode & 0xffffff00u) | reactState;

                // Also notify the server: CMSG_PET_ACTION (petGuid, actionData, targetGuid)
                CDataStore dataStore;
                dataStore.Put(CMSG_PET_ACTION);
                dataStore.Put(petGuid);
                dataStore.Put(actionData);
                dataStore.Put(uint64_t(0)); // targetGuid = 0
                dataStore.Finalize();

                clientServicesSend(&dataStore);
                packetsSent++;
            } else {
                // Autocast: CMSG_PET_SET_ACTION (petGuid, slot, actionData)
                CDataStore dataStore;
                dataStore.Put(CMSG_PET_SET_ACTION);
                dataStore.Put(petGuid);
                dataStore.Put(spellSlot);
                dataStore.Put(actionData);
                dataStore.Finalize();

                clientServicesSend(&dataStore);
                packetsSent++;
            }
        }

        auto const signalEvent = reinterpret_cast<SignalEventT>(Offsets::SignalEvent);
        signalEvent(game::PET_BAR_UPDATE);

        DEBUG_LOG("Restored greater demon autocast for displayId=" << displayId
            << " petGuid=" << petGuid
            << " cvar=" << cvarName
            << " packetsSent=" << packetsSent);
    }

    void CGPetInfo_SetPetHook(hadesmem::PatchDetourBase *detour, void *thisptr, void *dummy_edx,
                              uint32_t guid1, uint32_t guid2) {
        auto activePetGuid = *reinterpret_cast<uint64_t *>(Offsets::ActivePetGuid);

        auto const origSetPet = detour->GetTrampolineT<CGPetInfo_SetPetT>();
        origSetPet(thisptr, dummy_edx, guid1, guid2);

        uint64_t newPetGuid = *reinterpret_cast<uint64_t *>(Offsets::ActivePetGuid);
        if (activePetGuid != newPetGuid) {
            DEBUG_LOG("Active pet guid changed from " << activePetGuid << " to " << newPetGuid);

            if (gUserSettings.preserveGreaterDemonAutocast && newPetGuid != 0) {
                RestoreGreaterDemonAutocastPreferences();
            }
        }
    }

    void TogglePetSlotAutocastHook(hadesmem::PatchDetourBase *detour, uint32_t slot) {
        auto const orig = detour->GetTrampolineT<TogglePetSlotAutocastT>();
        gPendingToggleAutocastSlot = slot;
        orig(slot);
        gPendingToggleAutocastSlot = 0;
    }

    void CGPetInfo_GetPetSpellActionHook(hadesmem::PatchDetourBase *detour, uint32_t *action) {
        auto const orig = detour->GetTrampolineT<CGPetInfo_GetPetSpellActionT>();
        orig(action);

        if (!action) return;
        uint32_t spellSlot = gPendingToggleAutocastSlot;

        uint32_t actionData = action[0];

        // save autocast preferences
        auto activePetGuid = *reinterpret_cast<uint64_t *>(Offsets::ActivePetGuid);
        if (gUserSettings.preserveGreaterDemonAutocast && activePetGuid != 0) {
            DEBUG_LOG("Saving greater demon autocast preferences for slot " << spellSlot);
            SaveGreaterDemonPreferences(spellSlot, actionData);
        }
    }

    void CGPetInfo_SendPetActionHook(hadesmem::PatchDetourBase *detour, uint32_t *action, uint64_t *target) {
        auto const orig = detour->GetTrampolineT<CGPetInfo_SendPetActionT>();
        orig(action, target);

        if (!action) return;

        auto actionData = action[0];

        auto const activePetGuid = *reinterpret_cast<uint64_t *>(Offsets::ActivePetGuid);
        if (!gUserSettings.preserveGreaterDemonAutocast || activePetGuid == 0) return;

        auto spellSlot = 0;

        switch (actionData) {
            case kReactStateAggressive:
                spellSlot = 7;
                break;
            case kReactStateDefensive:
                spellSlot = 8;
                break;
            case kReactStatePassive:
                spellSlot = 9;
                break;
            default:
                return;
        }

        DEBUG_LOG("Saving greater demon react state slot=" << spellSlot << " actionData=" << actionData);
        SaveGreaterDemonPreferences(spellSlot, actionData);
    }
}
