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

    uint32_t Script_QueueScript(uintptr_t *luaState);

    uint32_t Script_GetUnitData(uintptr_t *luaState);

    uint32_t Script_GetUnitField(uintptr_t *luaState);

    bool TryDisenchant();

    uint32_t Script_DisenchantAll(uintptr_t *luaState);

    uint32_t Script_GetItemIconTexture(uintptr_t *luaState);

    uint32_t Script_GetSpellIconTexture(uintptr_t *luaState);
    uint32_t Script_LearnTalentRank(uintptr_t *luaState);

    bool RunQueuedScript(int priority);

    uint32_t Script_GetPlayerAuraDuration(uintptr_t *luaState);
    uint32_t Script_CancelPlayerAuraSlot(uintptr_t *luaState);
    uint32_t Script_CancelPlayerAuraSpellId(uintptr_t *luaState);

    uint32_t Script_SetMouseoverUnit(uintptr_t *luaState);
    uint32_t Script_GetUnitGUID(uintptr_t *luaState);
    uint32_t Script_IsAuraHidden(uintptr_t *luaState);

    uint32_t CSimpleFrame_GetNameHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState);
    uint64_t GetGUIDFromNameHook(hadesmem::PatchDetourBase *detour, const char *nameStr);
    bool CSimpleTop_OnKeyDownHook(hadesmem::PatchDetourBase *detour, EVENT_DATA_KEY *keyData, int param_2);
    bool CSimpleTop_OnKeyUpHook(hadesmem::PatchDetourBase *detour, EVENT_DATA_KEY *keyData, int param_2);
}
