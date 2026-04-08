//
// Created by pmacc on 1/8/2025.
//

#pragma once

#include <Windows.h>
#include "main.hpp"

namespace Nampower {
    uint32_t Script_GetNampowerVersion(uintptr_t *luaState);

    uint32_t Script_QueueScript(uintptr_t *luaState);

    bool RunQueuedScript(int priority);

    uint32_t Script_IsAuraHidden(uintptr_t *luaState);
    uint32_t Script_CombatLogFlush(uintptr_t *luaState);
}
