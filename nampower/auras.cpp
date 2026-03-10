//
// Created by pmacc on 12/7/2025.
//

#include "auras.hpp"
#include "offsets.hpp"
#include "logging.hpp"
#include "game.hpp"
#include "helper.hpp"

namespace Nampower {
    constexpr uint32_t SPELL_AURA_TRACK_CREATURES = 44;
    constexpr uint32_t SPELL_AURA_TRACK_RESOURCES = 45;
    constexpr uint32_t SPELL_AURA_TRACK_STEALTHED = 151;

    uint32_t gAuraExpirationTime[MAX_AURA_SLOTS] = {};

    static bool IsTrackingAura(const game::SpellRec *spellRec) {
        if (!spellRec) {
            return false;
        }

        for (uint32_t auraName: spellRec->EffectApplyAuraName) {
            if (auraName == SPELL_AURA_TRACK_CREATURES ||
                auraName == SPELL_AURA_TRACK_RESOURCES ||
                auraName == SPELL_AURA_TRACK_STEALTHED) {
                return true;
            }
        }

        return false;
    }

    bool IsAuraHiddenForLua(uint32_t spellId) {
        if (spellId == 0) {
            return true;
        }

        auto const *spellRec = game::GetSpellInfo(spellId);
        if (!spellRec) {
            return false;
        }

        if ((spellRec->Attributes & game::SPELL_ATTR_HIDDEN_CLIENTSIDE) != 0) {
            return true;
        }

        if ((spellRec->AttributesEx & game::SPELL_ATTR_EX_NO_AURA_ICON) != 0) {
            return true;
        }

        if (IsTrackingAura(spellRec)) {
            return true;
        }

        return false;
    }

    static bool IsAuraVisibleInPackedFlags(const game::UnitFields *unitFields, uint32_t slot, uint8_t *packedOut = nullptr,
                                    uint8_t *nibbleOut = nullptr) {
        auto *auraFlags = reinterpret_cast<const uint8_t *>(unitFields->auraFlags);
        uint8_t packedSlotFlags = auraFlags[slot / 2];
        uint8_t nibble = (slot & 1u) != 0 ? (packedSlotFlags >> 4) : packedSlotFlags;
        if (packedOut) {
            *packedOut = packedSlotFlags;
        }
        if (nibbleOut) {
            *nibbleOut = nibble;
        }
        return (nibble & 0x0Eu) != 0; // (flags & (NEGATIVE | PASSIVE | PERMANENT)) != 0
    }

    static bool CountsAsEmptyForLua(const game::UnitFields *unitFields, uint32_t slot) {
        uint32_t spellId = unitFields->aura[slot];
        if (spellId == 0) {
            return true;
        }

        if (IsAuraHiddenForLua(spellId)) {
            return true;
        }

        uint8_t packedSlotFlags = 0;
        uint8_t nibble = 0;
        bool isVisible = IsAuraVisibleInPackedFlags(unitFields, slot, &packedSlotFlags, &nibble);
        return !isVisible;
    }

    static bool IsHiddenAura(const game::UnitFields *unitFields, uint32_t slot, uint32_t spellId, uint32_t state) {
        if (spellId != 0 && IsAuraHiddenForLua(spellId)) {
            return true;
        }

        // Removed events run after the aura was cleared, so do not use slot/flags visibility there.
        if (state == 1) {
            return false;
        }

        return CountsAsEmptyForLua(unitFields, slot);
    }

    static int CalculateLuaUnitSlotFromAuraSlot(const game::UnitFields *unitFields, uint32_t slot) {
        bool isBuff = slot < 32;
        int luaUnitSlot = static_cast<int>(slot) + 1;

        if (!isBuff) {
            luaUnitSlot -= 32;
        }

        uint32_t firstSlot = isBuff ? 0 : 32;
        int emptyCount = 0;
        for (uint32_t i = firstSlot; i < slot; ++i) {
            if (CountsAsEmptyForLua(unitFields, i)) {
                ++emptyCount;
            }
        }

        return luaUnitSlot - emptyCount;
    }

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

        auto *unitFields = *reinterpret_cast<game::UnitFields **>(unit + 68);

        auto auraLevels = unitFields->auraLevels;
        auto auraStacks = unitFields->auraApplications;
        int luaUnitSlot = CalculateLuaUnitSlotFromAuraSlot(unitFields, slot);

        auto luaStacks = (state == 1) ? auraStacks[slot] : auraStacks[slot] + 1;
        // for some crazy reason the stack count is 0 indexed but also 0 instead of -1 when removed

        if (isBuff) {
            if (wasAdded) {
                // Buff added
                eventToTrigger = isSelf ? game::BUFF_ADDED_SELF : game::BUFF_ADDED_OTHER;
            } else {
                // Buff removed
                eventToTrigger = isSelf ? game::BUFF_REMOVED_SELF : game::BUFF_REMOVED_OTHER;
            }
        } else {
            if (wasAdded) {
                // Debuff added
                eventToTrigger = isSelf ? game::DEBUFF_ADDED_SELF : game::DEBUFF_ADDED_OTHER;
            } else {
                // Debuff removed
                eventToTrigger = isSelf ? game::DEBUFF_REMOVED_SELF : game::DEBUFF_REMOVED_OTHER;
            }
        }

        bool isHidden = IsHiddenAura(unitFields, slot, spellId, state);
        static char format[] = "%s%d%d%d%d%d%d";
        char *guidStr = ConvertGuidToString(isSelf ? playerGuid : unitGuid);
        int eventLuaSlot = isHidden ? 0 : luaUnitSlot;

        // Trigger the event with spellId as parameter
        ((int (__cdecl *)(int, char *, char *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))
            Offsets::SignalEventParam)(
            eventToTrigger,
            format,
            guidStr,
            eventLuaSlot,
            spellId,
            luaStacks,
            auraLevels[slot],
            slot,
            state);

        FreeGuidString(guidStr);
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

    void CGUnit_C_OnAuraStacksChangedHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx, int slot,
                                          uint8_t stackCount) {
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
        char *guidStr = ConvertGuidToString(guid);

        ((int (__cdecl *)(int, char *, char *)) Offsets::SignalEventParam)(
            game::UNIT_DIED,
            format,
            guidStr);

        FreeGuidString(guidStr);
    }
}
