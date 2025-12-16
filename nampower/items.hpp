//
// Created by pmacc on 9/21/2024.
//

#pragma once

#include "main.hpp"

namespace Nampower {
    using TooltipItemStatsCallbackT = void (__fastcall *)(uint32_t itemId, const uint64_t *ownerGuid, void *userData, bool resultFlag);

    using DBCache_ItemCacheDBGetRowT = uint32_t * (__thiscall  *)(void *this_ptr, uint32_t itemId, uint64_t *guid, TooltipItemStatsCallbackT callback, uintptr_t *userData, bool requestIfMissing);

    void ExportAllItems();
    bool LoadItem(uint32_t itemId);  // Returns true if item needs async load (caller should wait)
    bool ProcessItemExport();  // Process one item export per frame, returns true if still exporting
    game::ItemStats_C* GetItemStats(uint32_t itemId);

}
