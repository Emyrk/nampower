//
// Created by pmacc on 4/7/2026.
//

#pragma once

#include <Windows.h>
#include "main.hpp"

namespace Nampower {
    uint32_t Script_GetRaidTargets(uintptr_t *luaState);
    uint32_t Script_GetUnitData(uintptr_t *luaState);
    uint32_t Script_GetUnitField(uintptr_t *luaState);
    uint32_t Script_SetMouseoverUnit(uintptr_t *luaState);
    uint32_t Script_SetLocalRaidTargetIndex(uintptr_t *luaState);
    uint32_t Script_SetRaidTargetHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState);
    uint32_t Script_GetUnitGUID(uintptr_t *luaState);

    uint32_t CSimpleFrame_GetNameHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState);
    uint64_t GetGUIDFromNameHook(hadesmem::PatchDetourBase *detour, const char *nameStr);
}
