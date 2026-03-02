//
// Created by pmacc on 1/15/2025.
//

#pragma once

#include <Windows.h>
#include "main.hpp"

namespace Nampower {
    uint32_t Script_CastSpellByNameNoQueue(uintptr_t *luaState);

    uint32_t Script_CastSpellNoQueue(uintptr_t *luaState);

    uint32_t Script_CastSpellByNameHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState);

    uint32_t Script_QueueSpellByName(uintptr_t *luaState);

    uint32_t Script_IsSpellInRange(uintptr_t *luaState);

    uint32_t Script_IsSpellUsable(uintptr_t *luaState);

    uint32_t Script_SpellStopCastingHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState);

    uint32_t Script_GetSpellIdForName(uintptr_t *luaState);

    uint32_t Script_GetSpellNameAndRankForId(uintptr_t *luaState);

    uint32_t Script_GetSpellSlotTypeIdForName(uintptr_t *luaState);

    uint32_t Script_GetSpellRec(uintptr_t *luaState);

    uint32_t Script_GetSpellRecField(uintptr_t *luaState);

    uint32_t Script_GetSpellModifiers(uintptr_t *luaState);

    uint32_t Script_GetSpellPower(uintptr_t *luaState);

    uint32_t Script_GetSpellDuration(uintptr_t *luaState);

    uint32_t GetSpellSlotFromLuaHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState, uint32_t *slot, uint32_t *type);
}
