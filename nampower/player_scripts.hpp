//
// Created by pmacc on 2/15/2026.
//

#pragma once

#include <Windows.h>
#include "main.hpp"

namespace Nampower {
    uint32_t Script_PlayerIsMoving(uintptr_t *luaState);
    uint32_t Script_PlayerIsRooted(uintptr_t *luaState);
    uint32_t Script_PlayerIsSwimming(uintptr_t *luaState);
    bool TryDisenchant();
    uint32_t Script_DisenchantAll(uintptr_t *luaState);
    uint32_t Script_GetPlayerAuraDuration(uintptr_t *luaState);
    uint32_t Script_CancelPlayerAuraSlot(uintptr_t *luaState);
    uint32_t Script_CancelPlayerAuraSpellId(uintptr_t *luaState);
    uint32_t Script_LearnTalentRank(uintptr_t *luaState);
}
