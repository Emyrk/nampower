//
// Cooldown-related Lua script bindings
//

#pragma once

#include "main.hpp"

namespace Nampower {
    uint32_t Script_GetSpellIdCooldown(uintptr_t *luaState);
    uint32_t Script_GetItemIdCooldown(uintptr_t *luaState);
}
