//
// Created by pmacc on 1/8/2025.
//

#pragma once

#include <Windows.h>
#include "main.hpp"

namespace Nampower {
    uint32_t Script_CastSpellByNameNoQueue(uintptr_t *luaState);

    uint32_t Script_QueueSpellByName(uintptr_t *luaState);

    uint32_t Script_IsSpellInRange(uintptr_t *luaState);

    uint32_t Script_IsSpellUsable(uintptr_t *luaState);

    uint32_t Script_SpellStopCastingHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState);

    uint32_t Script_GetCurrentCastingInfo(uintptr_t *luaState);

    uint32_t Script_GetSpellIdForName(uintptr_t *luaState);

    uint32_t Script_GetSpellNameAndRankForId(uintptr_t *luaState);

    uint32_t Script_GetSpellSlotTypeIdForName(uintptr_t *luaState);

    uint32_t Script_ChannelStopCastingNextTick(uintptr_t *luaState);

    uint32_t Script_GetNampowerVersion(uintptr_t *luaState);

    uint32_t Script_GetItemLevel(uintptr_t *luaState);

    uint32_t Script_QueueScript(uintptr_t *luaState);

    uint32_t Script_GetItemStats(uintptr_t *luaState);

    uint32_t Script_GetSpellRec(uintptr_t *luaState);

    uint32_t Script_GetItemStatsField(uintptr_t *luaState);

    uint32_t Script_GetSpellRecField(uintptr_t *luaState);

    uint32_t GetSpellSlotFromLuaHook(hadesmem::PatchDetourBase *detour, int param_1, uint32_t *slot, uint32_t *type);

    bool RunQueuedScript(int priority);
}