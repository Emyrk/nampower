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
}
