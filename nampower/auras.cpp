//
// Created by pmacc on 12/7/2025.
//

#include "auras.hpp"
#include "offsets.hpp"
#include "logging.hpp"
#include "game.hpp"

namespace Nampower {
    // Helper function to trigger buff/debuff events
    void TriggerAuraEvent(uintptr_t *unit, uint32_t slot, uint32_t spellId, bool wasAdded) {
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

        auto luaStacks = auraStacks[slot];

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
                luaStacks += 1; // count offset by 1

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
                luaStacks += 1; // count offset by 1

                // Debuff added
                eventToTrigger = isSelf ? game::DEBUFF_ADDED_SELF : game::DEBUFF_ADDED_OTHER;
            } else {
                // Debuff removed
                eventToTrigger = isSelf ? game::DEBUFF_REMOVED_SELF : game::DEBUFF_REMOVED_OTHER;
            }
        }

        char format[] = "%s%d%d%d%d";
        char *guidStr = new char[21]; // 2 for 0x prefix, 18 for the number, and 1 for '\0'
        if (isSelf) {
            std::snprintf(guidStr, 21, "0x%016llX", static_cast<unsigned long long>(playerGuid));
        } else {
            std::snprintf(guidStr, 21, "0x%016llX", static_cast<unsigned long long>(unitGuid));
        }

        // Trigger the event with spellId as parameter
            ((int (__cdecl *)(int, char *, char *, uint32_t, uint32_t, uint32_t, uint32_t)) Offsets::SignalEventParam)(
                eventToTrigger,
                format,
                guidStr,
                luaUnitSlot,
                spellId,
                luaStacks,
                auraLevels[slot]);

        delete[] guidStr;
    }

    void CGUnit_C_OnAuraRemovedHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx, uint32_t slot,
                                    uint32_t spellId) {
        auto const onAuraRemoved = detour->GetTrampolineT<CGUnit_C_OnAuraRemovedT>();

        // Call the original function
        onAuraRemoved(unit, dummy_edx, slot, spellId);

        TriggerAuraEvent(unit, slot, spellId, false);
    }

    void CGUnit_C_OnAuraAddedHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx, uint32_t slot,
                                  uint32_t spellId) {
        auto const onAuraAdded = detour->GetTrampolineT<CGUnit_C_OnAuraAddedT>();

        // Call the original function
        onAuraAdded(unit, dummy_edx, slot, spellId);

        TriggerAuraEvent(unit, slot, spellId, true);
    }

    void CGUnit_C_OnAuraAddedStackHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, int slot) {
        auto const unitAuraAddedStack = detour->GetTrampolineT<CGUnit_C_OnAuraAddedStackT>();

        // Call the original function
        unitAuraAddedStack(unit, slot);

        // Get spell ID from the aura slot
        auto *unitFields = *reinterpret_cast<game::UnitFields **>(unit + 68);
        auto auras = unitFields->aura;
        uint32_t spellId = auras[slot];

        // Trigger the aura event for stack added
        TriggerAuraEvent(unit, slot, spellId, true);
    }

    void UnitCombatLogUnitDeadHook(hadesmem::PatchDetourBase *detour, uint64_t guid) {
        auto const unitCombatLogUnitDead = detour->GetTrampolineT<UnitCombatLogUnitDeadT>();

        // Call the original function
        unitCombatLogUnitDead(guid);

        // Trigger UNIT_DIED event
        char format[] = "%s";
        char *guidStr = new char[21]; // 2 for 0x prefix, 18 for the number, and 1 for '\0'
        std::snprintf(guidStr, 21, "0x%016llX", static_cast<unsigned long long>(guid));

        ((int (__cdecl *)(int, char *, char *)) Offsets::SignalEventParam)(
            game::UNIT_DIED,
            format,
            guidStr);

        delete[] guidStr;
    }
}
