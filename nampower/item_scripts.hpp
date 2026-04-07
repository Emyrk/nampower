//
// Item-related Lua script bindings
//

#pragma once

#include "main.hpp"

namespace Nampower {
    uint32_t Script_GetItemIconTexture(uintptr_t *luaState);
    uint32_t Script_GetItemLevel(uintptr_t *luaState);
    uint32_t Script_GetItemStats(uintptr_t *luaState);
    uint32_t Script_GetItemStatsField(uintptr_t *luaState);
    uint32_t Script_StartItemExport(uintptr_t *luaState);
    uint32_t Script_FindPlayerItemSlot(uintptr_t *luaState);
    uint32_t Script_UseItemIdOrName(uintptr_t *luaState);
    uint32_t Script_UseTrinket(uintptr_t *luaState);
    uint32_t Script_GetTrinkets(uintptr_t *luaState);
    uint32_t Script_GetEquippedItems(uintptr_t *luaState);
    uint32_t Script_GetEquippedItem(uintptr_t *luaState);
    uint32_t Script_GetBagItem(uintptr_t *luaState);
    uint32_t Script_GetBagItems(uintptr_t *luaState);
    uint32_t Script_GetAmmo(uintptr_t *luaState);
}
