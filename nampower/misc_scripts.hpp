//
// Created by pmacc on 1/8/2025.
//

#pragma once

#include <Windows.h>
#include "main.hpp"

namespace Nampower {
    uint32_t Script_GetCurrentCastingInfo(uintptr_t *luaState);

    uint32_t Script_GetCastInfo(uintptr_t *luaState);

    uint32_t Script_ChannelStopCastingNextTick(uintptr_t *luaState);

    uint32_t Script_GetNampowerVersion(uintptr_t *luaState);

    uint32_t Script_GetItemLevel(uintptr_t *luaState);

    uint32_t Script_QueueScript(uintptr_t *luaState);

    uint32_t Script_GetItemStats(uintptr_t *luaState);

    uint32_t Script_GetItemStatsField(uintptr_t *luaState);

    uint32_t Script_GetUnitData(uintptr_t *luaState);

    uint32_t Script_GetUnitField(uintptr_t *luaState);

    uint32_t Script_StartItemExport(uintptr_t *luaState);

    bool TryDisenchant();

    uint32_t Script_DisenchantAll(uintptr_t *luaState);

    uint32_t Script_GetItemIconTexture(uintptr_t *luaState);

    uint32_t Script_GetSpellIconTexture(uintptr_t *luaState);

    bool RunQueuedScript(int priority);

    uint32_t Script_GetPlayerAuraDuration(uintptr_t *luaState);

    uint32_t CSimpleFrame_GetNameHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState);
}
