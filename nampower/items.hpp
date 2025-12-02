//
// Created by pmacc on 9/21/2024.
//

#pragma once

#include "main.hpp"

namespace Nampower {
    using TooltipItemStatsCallbackT = void (__stdcall *)(uintptr_t *tooltip, char param_2);

    using DBCache_ItemCacheDBGetRowT = uint32_t * (__fastcall *)(void *this_ptr, void *dummy_edx, uint32_t itemId, int *param_2, TooltipItemStatsCallbackT callback, int *param_4, char param_5);

    void ExportAllItems();
    void LoadItem(uint32_t itemId);
    game::ItemStats_C* GetItemStats(uint32_t itemId);
}
