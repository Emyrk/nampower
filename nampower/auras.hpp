//
// Created by pmacc on 12/7/2025.
//

#pragma once

#include <Windows.h>
#include "main.hpp"

namespace Nampower {
    constexpr int MAX_AURA_SLOTS = 48;

    extern uint32_t gAuraExpirationTime[MAX_AURA_SLOTS];

    using CGUnit_C_OnAuraRemovedT = void (__fastcall *)(uintptr_t *unit, void *dummy_edx,uint32_t slot, uint32_t spellId);
    using CGUnit_C_OnAuraAddedT = void (__fastcall *)(uintptr_t *unit, void *dummy_edx,uint32_t slot, uint32_t spellId);
    using CGUnit_C_OnAuraAddedStackT = void (__fastcall *)(uintptr_t *unit, int slot);
    using UnitCombatLogUnitDeadT = void (__fastcall *)(uint64_t guid);
    using CGBuffBar_UpdateDurationT = void (__fastcall *)(uint8_t auraSlot, int durationMs);

    void CGUnit_C_OnAuraRemovedHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx, uint32_t slot, uint32_t spellId);
    void CGUnit_C_OnAuraAddedHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx, uint32_t slot, uint32_t spellId);
    void CGUnit_C_OnAuraAddedStackHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, int slot);
    void UnitCombatLogUnitDeadHook(hadesmem::PatchDetourBase *detour, uint64_t guid);
    void CGBuffBar_UpdateDurationHook(hadesmem::PatchDetourBase *detour, uint8_t auraSlot, int durationMs);
}
