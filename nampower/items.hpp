//
// Created by pmacc on 9/21/2024.
//

#pragma once

#include "main.hpp"

namespace Nampower {
    using TooltipItemStatsCallbackT = void (__fastcall *)(uint32_t itemId, const uint64_t *ownerGuid, void *userData, bool resultFlag);

    using DBCache_ItemCacheDBGetRowT = uint32_t * (__thiscall  *)(void *this_ptr, uint32_t itemId, uint64_t *guid, TooltipItemStatsCallbackT callback, uintptr_t *userData, bool requestIfMissing);

    using GetInventoryArtT = char * (__fastcall *)(uint32_t displayId);

    constexpr int32_t EQUIPPED_BAG_INDEX = -3;
    constexpr int32_t BANK_BAG_INDEX = -1;
    constexpr int32_t KEYRING_BAG_INDEX = -2;

    struct PlayerItemSearchResult {
        game::CGItem_C *item = nullptr;
        int32_t bagIndex = 0;   // -3 equipped, -1 bank main slots, -2 keyring, 0 backpack, 1-9 bag indices
        uint32_t slot = 0;

        bool found() const { return item != nullptr; }
    };

    void ExportAllItems();
    void StartItemExport();
    bool LoadItem(uint32_t itemId);  // Returns true if item needs async load (caller should wait)
    bool ProcessItemExport();  // Process one item export per frame, returns true if still exporting
    game::ItemStats_C* GetItemStats(uint32_t itemId);
    uint32_t GetItemIdFromCache(const char *itemName);
    void CacheItemNameToId(const char *itemName, uint32_t itemId);
    bool DoesItemMatch(uint32_t itemId, uint32_t searchItemId, const char *searchItemName);
    PlayerItemSearchResult FindPlayerItem(uint32_t searchItemId, const char *searchItemName);
    uintptr_t *GetBagPtrFromContainer(uintptr_t *containerPtr);

}
