//
// Created by pmacc on 12/7/2025.
//

#include "auras.hpp"
#include "offsets.hpp"
#include "logging.hpp"
#include "game.hpp"

namespace Nampower {
    uint32_t gAuraExpirationTime[MAX_AURA_SLOTS] = {};

    // Helper function to trigger buff/debuff events
    // state: 0 = newly added, 1 = newly removed, 2 = modified (stack change)
    void TriggerAuraEvent(uintptr_t *unit, uint32_t slot, uint32_t spellId, bool wasAdded, uint32_t state) {
        // Determine if the unit is the active player (self) or another unit (other)
        auto unitGuid = game::UnitGetGuid(unit);
        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        bool isSelf = (unitGuid == playerGuid);
        bool isBuff = slot < 32; // slots 0-31 are buffs

        // Trigger the appropriate event based on buff/debuff, add/remove, and self/other
        game::Events eventToTrigger;

        int luaUnitSlot = slot + 1; // lua uses 1-based indexing for slots.  This is the slot for UnitBuff/UnitDebuff

        auto *unitFields = *reinterpret_cast<game::UnitFields **>(unit + 68);

        auto auras = unitFields->aura;
        auto auraLevels = unitFields->auraLevels;
        auto auraStacks = unitFields->auraApplications;

        auto luaStacks = (state == 1) ? auraStacks[slot] : auraStacks[slot] + 1; // for some crazy reason the stack count is 0 indexed but also 0 instead of -1 when removed

        if (isBuff) {
            // count empty buff slots before slot to adjust luaSlot for actual buff/debuff index
            int emptyCount = 0;
            for (uint32_t i = 0; i < slot; ++i) {
                if (auras[i] == 0) {
                    emptyCount++;
                }
            }
            luaUnitSlot = luaUnitSlot - emptyCount;

            if (wasAdded) {
                // Buff added
                eventToTrigger = isSelf ? game::BUFF_ADDED_SELF : game::BUFF_ADDED_OTHER;
            } else {
                // Buff removed
                eventToTrigger = isSelf ? game::BUFF_REMOVED_SELF : game::BUFF_REMOVED_OTHER;
            }
        } else {
            luaUnitSlot = luaUnitSlot - 32; // adjust slot for debuffs

            // count empty debuff slots before slot to adjust luaSlot for actual buff/debuff index
            int emptyCount = 0;
            for (uint32_t i = 32; i < slot; ++i) {
                if (auras[i] == 0) {
                    emptyCount++;
                }
            }
            luaUnitSlot = luaUnitSlot - emptyCount;

            if (wasAdded) {
                // Debuff added
                eventToTrigger = isSelf ? game::DEBUFF_ADDED_SELF : game::DEBUFF_ADDED_OTHER;
            } else {
                // Debuff removed
                eventToTrigger = isSelf ? game::DEBUFF_REMOVED_SELF : game::DEBUFF_REMOVED_OTHER;
            }
        }

        static char format[] = "%s%d%d%d%d%d%d";
        char *guidStr = new char[21]; // 2 for 0x prefix, 18 for the number, and 1 for '\0'
        if (isSelf) {
            std::snprintf(guidStr, 21, "0x%016llX", static_cast<unsigned long long>(playerGuid));
        } else {
            std::snprintf(guidStr, 21, "0x%016llX", static_cast<unsigned long long>(unitGuid));
        }

        // Trigger the event with spellId as parameter
            ((int (__cdecl *)(int, char *, char *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)) Offsets::SignalEventParam)(
                eventToTrigger,
                format,
                guidStr,
                luaUnitSlot,
                spellId,
                luaStacks,
                auraLevels[slot],
                slot,
                state);

        delete[] guidStr;
    }

    void CGUnit_C_OnAuraRemovedHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx, uint32_t slot,
                                    uint32_t spellId) {
        auto const onAuraRemoved = detour->GetTrampolineT<CGUnit_C_OnAuraRemovedT>();

        // Call the original function
        onAuraRemoved(unit, dummy_edx, slot, spellId);

        TriggerAuraEvent(unit, slot, spellId, false, 1);
    }

    void CGUnit_C_OnAuraAddedHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx, uint32_t slot,
                                  uint32_t spellId) {
        auto const onAuraAdded = detour->GetTrampolineT<CGUnit_C_OnAuraAddedT>();

        // Call the original function
        onAuraAdded(unit, dummy_edx, slot, spellId);

        TriggerAuraEvent(unit, slot, spellId, true, 0);
    }

    void CGUnit_C_OnAuraStacksChangedHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx, int slot, uint8_t stackCount) {
        auto const onAuraStacksChanged = detour->GetTrampolineT<CGUnit_C_OnAuraStacksChangedT>();

        // Get spell ID from the aura slot
        auto *unitFields = *reinterpret_cast<game::UnitFields **>(unit + 68);
        auto auras = unitFields->aura;
        uint32_t spellId = auras[slot];

        // Check if stacks increased or decreased (auraApplications is 0-indexed, convert to 1-indexed)
        uint8_t currentStacks = unitFields->auraApplications[slot];

        // Call the original function
        onAuraStacksChanged(unit, dummy_edx, slot, stackCount);

        bool wasAdded = stackCount < currentStacks;

        TriggerAuraEvent(unit, slot, spellId, wasAdded, 2);
    }

    void CGBuffBar_UpdateDurationHook(hadesmem::PatchDetourBase *detour, uint8_t auraSlot, int durationMs) {
        auto const updateDuration = detour->GetTrampolineT<CGBuffBar_UpdateDurationT>();

        // Call the original function
        updateDuration(auraSlot, durationMs);

        // Store expiration time indexed by aura slot
        if (auraSlot < MAX_AURA_SLOTS) {
            auto now = static_cast<uint32_t>(GetWowTimeMs() & 0xFFFFFFFF);
            uint32_t expirationTime = durationMs > 0 ? now + durationMs : 0;
            gAuraExpirationTime[auraSlot] = expirationTime;

            bool isBuff = auraSlot < 32;
            game::Events eventToTrigger = isBuff ? game::BUFF_UPDATE_DURATION_SELF : game::DEBUFF_UPDATE_DURATION_SELF;

            uint32_t spellId = 0;
            auto *unitFields = game::GetActivePlayerUnitFields();
            if (unitFields) {
                spellId = unitFields->aura[auraSlot];
            }

            static char format[] = "%d%d%d%d";
            ((int (__cdecl *)(int, char *, uint32_t, int, uint32_t, uint32_t)) Offsets::SignalEventParam)(
                eventToTrigger,
                format,
                auraSlot,
                durationMs,
                expirationTime,
                spellId);
        }
    }

    void UnitCombatLogUnitDeadHook(hadesmem::PatchDetourBase *detour, uint64_t guid) {
        auto const unitCombatLogUnitDead = detour->GetTrampolineT<UnitCombatLogUnitDeadT>();

        // Call the original function
        unitCombatLogUnitDead(guid);

        // Trigger UNIT_DIED event
        static char format[] = "%s";
        char *guidStr = new char[21]; // 2 for 0x prefix, 18 for the number, and 1 for '\0'
        std::snprintf(guidStr, 21, "0x%016llX", static_cast<unsigned long long>(guid));

        ((int (__cdecl *)(int, char *, char *)) Offsets::SignalEventParam)(
            game::UNIT_DIED,
            format,
            guidStr);

        delete[] guidStr;
    }
}
