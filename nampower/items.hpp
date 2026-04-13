//
// Created by pmacc on 9/21/2024.
//

#pragma once

#include "main.hpp"
#include <array>

namespace Nampower {
    using TooltipItemStatsCallbackT = void (__fastcall *)(uint32_t itemId, const uint64_t *ownerGuid, void *userData, bool resultFlag);

    using DBCache_ItemCacheDBGetRowT = uint32_t * (__thiscall  *)(void *this_ptr, uint32_t itemId, uint64_t *guid, TooltipItemStatsCallbackT callback, uintptr_t *userData, bool requestIfMissing);

    using GetInventoryArtT = char * (__fastcall *)(uint32_t displayId);
    using CGItem_C_ItemIsQuestOrSoulboundT = uint32_t (__fastcall *)(game::CGItem_C *item);

    constexpr int32_t EQUIPPED_BAG_INDEX = -3;
    constexpr int32_t BANK_BAG_INDEX = -1;
    constexpr int32_t KEYRING_BAG_INDEX = -2;

    // Cache constants
    constexpr uint32_t MAX_BAGS = 13;  // 0-10 for regular bags, 11 for -1 (bank), 12 for -2 (keyring)
    constexpr uint32_t BANK_CACHE_INDEX = 11;
    constexpr uint32_t KEYRING_CACHE_INDEX = 12;
    constexpr uint32_t MAX_BAG_SLOTS = 64;  // Increased to 64 for future-proofing
    constexpr uint32_t MAX_EQUIPPED_SLOTS = 19;

    struct PlayerItemSearchResult {
        game::CGItem_C *item = nullptr;
        int32_t bagIndex = 0;   // -3 equipped, -1 bank main slots, -2 keyring, 0 backpack, 1-9 bag indices
        uint32_t slot = 0;

        bool found() const { return item != nullptr; }
    };

    // Cached item data for change detection
    struct CachedItemData {
        uint32_t itemId;
        uint32_t stackCount;
        uint32_t duration;
        int32_t spellChargesRemaining;
        uint32_t flags;
        uint32_t randomPropertiesId;
        uint32_t permanentEnchantId;
        uint32_t tempEnchantId;
        uint32_t tempEnchantmentTimeLeftMs;
        uint32_t tempEnchantmentCharges;
        uint32_t durability;
        uint32_t maxDurability;

        CachedItemData() : itemId(0), stackCount(0), duration(0), spellChargesRemaining(0), flags(0),
                           randomPropertiesId(0),
                           permanentEnchantId(0), tempEnchantId(0),
                           tempEnchantmentTimeLeftMs(0), tempEnchantmentCharges(0),
                           durability(0), maxDurability(0) {}

        bool operator==(const CachedItemData& other) const {
            return itemId == other.itemId &&
                   stackCount == other.stackCount &&
                   duration == other.duration &&
                   spellChargesRemaining == other.spellChargesRemaining &&
                   flags == other.flags &&
                   randomPropertiesId == other.randomPropertiesId &&
                   permanentEnchantId == other.permanentEnchantId &&
                   tempEnchantId == other.tempEnchantId &&
                   tempEnchantmentTimeLeftMs == other.tempEnchantmentTimeLeftMs &&
                   tempEnchantmentCharges == other.tempEnchantmentCharges &&
                   durability == other.durability &&
                   maxDurability == other.maxDurability;
        }

        bool operator!=(const CachedItemData& other) const {
            return !(*this == other);
        }
    };

    // Cached trinket data for change detection (2D matrix by [bagIndex][slot])
    struct CachedTrinketData {
        uint32_t itemId;

        CachedTrinketData() : itemId(0) {}

        bool operator==(const CachedTrinketData& other) const {
            return itemId == other.itemId;
        }

        bool operator!=(const CachedTrinketData& other) const {
            return !(*this == other);
        }
    };

    // Cache arrays (defined in items.cpp)
    extern std::array<std::array<CachedItemData, MAX_BAG_SLOTS>, MAX_BAGS> bagItemDataCache;
    extern std::array<CachedItemData, MAX_EQUIPPED_SLOTS> equippedItemDataCache;
    extern std::array<std::array<CachedTrinketData, MAX_BAG_SLOTS>, MAX_BAGS> bagTrinketDataCache;
    extern std::array<CachedTrinketData, MAX_EQUIPPED_SLOTS> equippedTrinketDataCache;

    void ClearItemCaches();
    void ExportAllItems();
    void StartItemExport();
    bool LoadItem(uint32_t itemId);  // Returns true if item needs async load (caller should wait)
    bool ProcessItemExport();  // Process one item export per frame, returns true if still exporting
    game::ItemStats_C* GetItemStats(uint32_t itemId);
    uint32_t GetItemIdFromCache(const char *itemName);
    void CacheItemNameToId(const char *itemName, uint32_t itemId);
    bool DoesItemMatch(uint32_t itemId, uint32_t searchItemId, const char *searchItemName);
    PlayerItemSearchResult FindPlayerItem(uint32_t searchItemId, const char *searchItemName);
    PlayerItemSearchResult FindPlayerDisenchantItem(uint32_t qualityBitmask, bool includeSoulbound = false);
    uintptr_t *GetBagPtrFromContainer(uintptr_t *containerPtr);

}
