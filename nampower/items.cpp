//
// Created by pmacc on 9/21/2024.
//

#include "items.hpp"
#include "offsets.hpp"
#include "logging.hpp"
#include "helper.hpp"

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "dbc_fields.hpp"

namespace Nampower {
    // Item data caches for change detection
    std::array<std::array<CachedItemData, MAX_BAG_SLOTS>, MAX_BAGS> bagItemDataCache{};
    std::array<CachedItemData, MAX_EQUIPPED_SLOTS> equippedItemDataCache{};
    std::array<std::array<CachedTrinketData, MAX_BAG_SLOTS>, MAX_BAGS> bagTrinketDataCache{};
    std::array<CachedTrinketData, MAX_EQUIPPED_SLOTS> equippedTrinketDataCache{};

    // Global dictionary to store itemId -> ItemStats_C mappings
    static std::unordered_map<uint32_t, game::ItemStats_C *> itemStatsCache;

    // Local cache for item name lookups
    static std::unordered_map<std::string, uint32_t> itemNameToIdCache;

    // Track pending async loads (can have multiple at once)
    static std::unordered_set<uint32_t> pendingItemIds;

    // Export state tracking
    static bool isExporting = false;
    static uint32_t currentExportItemId = 0;
    static constexpr uint32_t MAX_EXPORT_ITEM_ID = 100000;
    static constexpr uint32_t ITEMS_PER_FRAME = 20;

    auto const getRow = reinterpret_cast<DBCache_ItemCacheDBGetRowT>(Offsets::DBCache_ItemCacheDBGetRow);
    auto const itemCache = reinterpret_cast<void *>(Offsets::ItemDBCache);

    void ClearItemCaches() {
        // Clear all item data caches (called on UI reload)
        for (auto& bagCache : bagItemDataCache) {
            bagCache.fill(CachedItemData());
        }
        equippedItemDataCache.fill(CachedItemData());
        for (auto& bagCache : bagTrinketDataCache) {
            bagCache.fill(CachedTrinketData());
        }
        equippedTrinketDataCache.fill(CachedTrinketData());
    }

    std::string ToLowerCase(const char *str) {
        std::string result;
        if (!str) return result;

        while (*str) {
            result += static_cast<char>(tolower(*str));
            ++str;
        }
        return result;
    }

    uint32_t GetItemIdFromCache(const char *itemName) {
        if (!itemName) return 0;

        std::string lowerName = ToLowerCase(itemName);
        auto it = itemNameToIdCache.find(lowerName);
        if (it != itemNameToIdCache.end()) {
            return it->second;
        }
        return 0;
    }

    void CacheItemNameToId(const char *itemName, uint32_t itemId) {
        if (!itemName || itemId == 0) return;

        std::string lowerName = ToLowerCase(itemName);
        itemNameToIdCache[lowerName] = itemId;
    }

    bool DoesItemMatch(uint32_t itemId, uint32_t searchItemId, const char *searchItemName) {
        if (searchItemId != 0) {
            return itemId == searchItemId;
        }

        if (searchItemName) {
            auto itemStats = GetItemStats(itemId);
            if (itemStats && itemStats->m_displayName[0]) {
                bool matches = _stricmp(itemStats->m_displayName[0], searchItemName) == 0;
                if (matches) {
                    CacheItemNameToId(searchItemName, itemId);
                }
                return matches;
            }
        }

        return false;
    }

    uintptr_t *GetBagPtrFromContainer(uintptr_t *containerPtr) {
        auto vftable = game::GetObjectVFTable(containerPtr);
        using GetBagPtrT = uintptr_t * (__thiscall *)(uintptr_t *);
        auto getBagPtrFunc = reinterpret_cast<GetBagPtrT>(vftable[4]);
        return getBagPtrFunc(containerPtr);
    }

    PlayerItemSearchResult FindPlayerItem(uint32_t searchItemId, const char *searchItemName) {
        PlayerItemSearchResult result{};

        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
        auto const getContainerGuid = reinterpret_cast<GetContainerGuidT>(Offsets::GetContainerGuid);

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto playerUnit = game::GetObjectPtr(playerGuid);
        if (!playerUnit) {
            return result;
        }

        auto inventory = game::GetPlayerInventoryPtr(playerUnit);

        auto matchesItem = [&](game::CGItem_C *item) {
            return item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName);
        };

        for (uint32_t slot = 0; slot <= 18; slot++) {
            auto item = getBagItem(inventory, slot);
            if (matchesItem(item)) {
                result.item = item;
                result.bagIndex = EQUIPPED_BAG_INDEX;
                result.slot = slot;
                return result;
            }
        }

        for (uint32_t slot = 23; slot <= 38; slot++) {
            auto item = getBagItem(inventory, slot);
            if (matchesItem(item)) {
                result.item = item;
                result.bagIndex = 0;
                result.slot = slot;
                return result;
            }
        }

        for (int32_t bagIndex = 1; bagIndex <= 4; bagIndex++) {
            uint64_t containerGuid = getContainerGuid(bagIndex - 1);
            if (containerGuid == 0) continue;

            auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
            if (!containerPtr) continue;

            auto bagPtr = GetBagPtrFromContainer(containerPtr);
            if (!bagPtr) continue;

            auto bagSize = *bagPtr;
            for (uint32_t slot = 0; slot < bagSize; slot++) {
                auto item = getBagItem(bagPtr, slot);
                if (matchesItem(item)) {
                    result.item = item;
                    result.bagIndex = bagIndex;
                    result.slot = slot;
                    return result;
                }
            }
        }

        uint64_t bankGuid = *reinterpret_cast<uint64_t *>(Offsets::BankGuid);
        if (bankGuid > 0) {
            for (uint32_t slot = 39; slot <= 62; slot++) {
                auto item = getBagItem(inventory, slot);
                if (matchesItem(item)) {
                    result.item = item;
                    result.bagIndex = BANK_BAG_INDEX;
                    result.slot = slot;
                    return result;
                }
            }

            for (int32_t bagIndex = 5; bagIndex <= 9; bagIndex++) {
                uint64_t containerGuid = getContainerGuid(bagIndex - 1); // bagIndex 5-9 maps to container 4-8
                if (containerGuid == 0) continue;

                auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
                if (!containerPtr) continue;

                auto bagPtr = GetBagPtrFromContainer(containerPtr);
                if (!bagPtr) continue;

                auto bagSize = *bagPtr;
                for (uint32_t slot = 0; slot < bagSize; slot++) {
                    auto item = getBagItem(bagPtr, slot);
                    if (matchesItem(item)) {
                        result.item = item;
                        result.bagIndex = bagIndex;
                        result.slot = slot;
                        return result;
                    }
                }
            }
        }

        for (uint32_t slot = 81; slot <= 96; slot++) {
            auto item = getBagItem(inventory, slot);
            if (matchesItem(item)) {
                result.item = item;
                result.bagIndex = KEYRING_BAG_INDEX;
                result.slot = slot;
                return result;
            }
        }

        return result;
    }

    PlayerItemSearchResult FindPlayerDisenchantItem(uint32_t qualityBitmask, bool includeSoulbound) {
        PlayerItemSearchResult result{};

        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
        auto const getContainerGuid = reinterpret_cast<GetContainerGuidT>(Offsets::GetContainerGuid);

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto playerUnit = game::GetObjectPtr(playerGuid);
        if (!playerUnit) {
            return result;
        }

        auto inventory = game::GetPlayerInventoryPtr(playerUnit);

        auto const itemIsQuestOrSoulbound = reinterpret_cast<CGItem_C_ItemIsQuestOrSoulboundT>(
            Offsets::CGItem_C_ItemIsQuestOrSoulbound);

        auto matchesItem = [&](game::CGItem_C *item) {
            if (!item) {
                return false;
            }

            uint32_t itemId = game::GetItemId(item);
            auto *itemStats = GetItemStats(itemId);
            if (!itemStats) {
                return false;
            }

            // Always skip quest items (check by class or startQuestID)
            if (itemStats->m_class == game::ITEM_CLASS_QUEST || itemStats->m_startQuestID > 0) {
                return false;
            }

            // Skip soulbound items only if includeSoulbound is false
            if (!includeSoulbound && itemIsQuestOrSoulbound(item)) {
                return false;
            }

            // Check if item is weapon or armor
            if (itemStats->m_class != game::ITEM_CLASS_WEAPON &&
                itemStats->m_class != game::ITEM_CLASS_ARMOR) {
                return false;
            }

            // Check if item quality matches the bitmask
            uint32_t qualityBit = 0;
            if (itemStats->m_quality == game::ITEM_QUALITY_UNCOMMON) {
                qualityBit = 0x01;  // Green
            } else if (itemStats->m_quality == game::ITEM_QUALITY_RARE) {
                qualityBit = 0x02;  // Blue
            } else if (itemStats->m_quality == game::ITEM_QUALITY_EPIC) {
                qualityBit = 0x04;  // Purple
            }

            if ((qualityBitmask & qualityBit) == 0) {
                return false;
            }

            return true;
        };

        // Skip equipped gear slots (0-18) - only search bags

        for (uint32_t slot = 23; slot <= 38; slot++) {
            auto item = getBagItem(inventory, slot);
            if (matchesItem(item)) {
                result.item = item;
                result.bagIndex = 0;
                result.slot = slot;
                return result;
            }
        }

        for (int32_t bagIndex = 1; bagIndex <= 4; bagIndex++) {
            uint64_t containerGuid = getContainerGuid(bagIndex - 1);
            if (containerGuid == 0) continue;

            auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
            if (!containerPtr) continue;

            auto bagPtr = GetBagPtrFromContainer(containerPtr);
            if (!bagPtr) continue;

            auto bagSize = *bagPtr;
            for (uint32_t slot = 0; slot < bagSize; slot++) {
                auto item = getBagItem(bagPtr, slot);
                if (matchesItem(item)) {
                    result.item = item;
                    result.bagIndex = bagIndex;
                    result.slot = slot;
                    return result;
                }
            }
        }

        // don't search bank

        return result;
    }

    std::string escapeJsonString(const char *str) {
        if (!str) return "null";

        std::string result = "\"";
        for (const char *p = str; *p; ++p) {
            switch (*p) {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\b':
                    result += "\\b";
                    break;
                case '\f':
                    result += "\\f";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(*p) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(*p));
                        result += buf;
                    } else {
                        result += *p;
                    }
            }
        }
        result += "\"";
        return result;
    }

    // Callback function for DBCache
    void __fastcall ItemStatsCallback(uint32_t itemId, const uint64_t *ownerGuid, void *userData, bool resultFlag) {
        if (resultFlag) {
            uint64_t guid;
            auto itemStats = getRow(itemCache, itemId, &guid, &ItemStatsCallback, nullptr, false);
            if (itemStats) {
                // Store in cache
                DEBUG_LOG("Added itemId " << itemId);
                itemStatsCache[itemId] = reinterpret_cast<game::ItemStats_C *>(itemStats);
            } else {
                DEBUG_LOG("Still missing " << itemId);
            }
        }

        // Remove from pending set
        pendingItemIds.erase(itemId);
    }

    bool LoadItem(uint32_t itemId) {
        // Check if already in cache
        auto it = itemStatsCache.find(itemId);
        if (it != itemStatsCache.end()) {
            return false; // Already loaded, no wait needed
        }

        uint64_t guid;
        auto itemStats = getRow(itemCache, itemId, &guid, &ItemStatsCallback, nullptr, true);
        if (itemStats) {
            // Item was available immediately, store in cache
            itemStatsCache[itemId] = reinterpret_cast<game::ItemStats_C *>(itemStats);
            return false; // Loaded immediately, no wait needed
        } else {
            // Item not immediately available, need to wait for callback
            pendingItemIds.insert(itemId);
            return true; // Caller should wait for callback
        }
    }

    bool ProcessItemExport() {
        if (!isExporting) {
            return false; // Not currently exporting
        }

        // Try to load items until we have 20 pending or run out of items
        uint32_t itemsRequestedThisFrame = 0;
        while (pendingItemIds.size() < ITEMS_PER_FRAME && currentExportItemId <= MAX_EXPORT_ITEM_ID) {
            LoadItem(currentExportItemId);

            // Log progress every 1000 items
            if (currentExportItemId % 1000 == 0) {
                DEBUG_LOG("Progress: " << currentExportItemId << " / " << MAX_EXPORT_ITEM_ID
                    << " items requested, " << pendingItemIds.size() << " pending callbacks");
            }

            currentExportItemId++;
            itemsRequestedThisFrame++;

            // Safety check - don't request too many in a single frame
            if (itemsRequestedThisFrame >= ITEMS_PER_FRAME) {
                break;
            }
        }

        // Check if we're done (all items requested and no pending callbacks)
        if (currentExportItemId > MAX_EXPORT_ITEM_ID && pendingItemIds.empty()) {
            // Fall through to export completion
        } else {
            return true; // Still exporting, more items to process or waiting for callbacks
        }

        // Export complete, write to file
        DEBUG_LOG("Finished fetching items, found " << itemStatsCache.size() << " valid items");
        DEBUG_LOG("Writing items to items.json");

        // Write all cached items to JSON file (overwrite mode)
        try {
            std::ofstream jsonFile("items.json", std::ios::trunc);
            if (!jsonFile.is_open()) {
                DEBUG_LOG("Failed to open items.json for writing");
                isExporting = false;
                return false;
            }

            jsonFile << "[\n";

            bool first = true;
            for (const auto &pair: itemStatsCache) {
                if (!first) {
                    jsonFile << ",\n";
                }
                first = false;

                uint32_t itemId = pair.first;
                game::ItemStats_C *item = pair.second;

                if (!item) continue;

                std::ostringstream json;
                json << "  {\n";
                json << "    \"id\": " << itemId << ",\n";
                json << "    \"class\": " << item->m_class << ",\n";
                json << "    \"subclass\": " << item->m_subclass << ",\n";
                json << "    \"displayName\": " << escapeJsonString(item->m_displayName[0]) << ",\n";
                json << "    \"displayInfoID\": " << item->m_displayInfoID << ",\n";
                json << "    \"quality\": " << item->m_quality << ",\n";
                json << "    \"flags\": " << static_cast<int>(item->m_flags) << ",\n";
                json << "    \"buyPrice\": " << item->m_buyPrice << ",\n";
                json << "    \"sellPrice\": " << item->m_sellPrice << ",\n";
                json << "    \"inventoryType\": " << item->m_inventoryType << ",\n";
                json << "    \"allowableClass\": " << item->m_allowableClass << ",\n";
                json << "    \"allowableRace\": " << item->m_allowableRace << ",\n";
                json << "    \"itemLevel\": " << item->m_itemLevel << ",\n";
                json << "    \"requiredLevel\": " << item->m_requiredLevel << ",\n";
                json << "    \"requiredSkill\": " << item->m_requiredSkill << ",\n";
                json << "    \"requiredSkillRank\": " << item->m_requiredSkillRank << ",\n";
                json << "    \"requiredSpell\": " << item->m_requiredSpell << ",\n";
                json << "    \"requiredHonorRank\": " << item->m_requiredHonorRank << ",\n";
                json << "    \"requiredCityRank\": " << item->m_requiredCityRank << ",\n";
                json << "    \"requiredRep\": " << item->m_requiredRep << ",\n";
                json << "    \"requiredRepRank\": " << item->m_requiredRepRank << ",\n";
                json << "    \"maxCount\": " << item->m_maxCount << ",\n";
                json << "    \"stackable\": " << item->m_stackable << ",\n";
                json << "    \"containerSlots\": " << item->m_containerSlots << ",\n";

                // Arrays
                json << "    \"bonusStat\": [";
                for (int i = 0; i < 10; ++i) {
                    json << item->m_bonusStat[i];
                    if (i < 9) json << ", ";
                }
                json << "],\n";

                json << "    \"bonusAmount\": [";
                for (int i = 0; i < 10; ++i) {
                    json << item->m_bonusAmount[i];
                    if (i < 9) json << ", ";
                }
                json << "],\n";

                json << "    \"minDamage\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_minDamage[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"maxDamage\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_maxDamage[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"damageType\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_damageType[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"resistances\": [";
                for (int i = 0; i < 7; ++i) {
                    json << item->m_resistances[i];
                    if (i < 6) json << ", ";
                }
                json << "],\n";

                json << "    \"delay\": " << item->m_delay << ",\n";
                json << "    \"ammoType\": " << item->m_ammoType << ",\n";
                json << "    \"rangedModRange\": " << item->m_rangedModRange << ",\n";

                json << "    \"spellID\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellID[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellTrigger\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellTrigger[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellCharges\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellCharges[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellCooldown\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellCooldown[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellCategory\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellCategory[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellCategoryCooldown\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellCategoryCooldown[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"bonding\": " << item->m_bonding << ",\n";
                json << "    \"description\": " << escapeJsonString(item->m_description) << ",\n";
                json << "    \"pageText\": " << item->m_pageText << ",\n";
                json << "    \"languageID\": " << item->m_languageID << ",\n";
                json << "    \"pageMaterial\": " << item->m_pageMaterial << ",\n";
                json << "    \"startQuestID\": " << item->m_startQuestID << ",\n";
                json << "    \"lockID\": " << item->m_lockID << ",\n";
                json << "    \"material\": " << item->m_material << ",\n";
                json << "    \"sheatheType\": " << item->m_sheatheType << ",\n";
                json << "    \"randomProperty\": " << item->m_randomProperty << ",\n";
                json << "    \"block\": " << item->m_block << ",\n";
                json << "    \"itemSet\": " << item->m_itemSet << ",\n";
                json << "    \"maxDurability\": " << item->m_maxDurability << ",\n";
                json << "    \"area\": " << item->m_area << ",\n";
                json << "    \"map\": " << item->m_map << ",\n";
                json << "    \"duration\": " << item->m_duration << ",\n";
                json << "    \"bagFamily\": " << item->m_bagFamily << "\n";
                json << "  }";

                jsonFile << json.str();
            }

            jsonFile << "\n]\n";
            jsonFile.close();

            DEBUG_LOG("Successfully wrote " << itemStatsCache.size() << " items to items.json");
        } catch (const std::exception &e) {
            DEBUG_LOG("Exception writing to items.json: " << e.what());
        }

        isExporting = false;
        return false; // Export complete
    }

    game::ItemStats_C *GetItemStats(uint32_t itemId) {
        // Load item if not cached
        LoadItem(itemId);

        auto it = itemStatsCache.find(itemId);
        if (it != itemStatsCache.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Register a function if you want to use this
    // Don't want everyone scraping all items.
    void ExportAllItems() {
        DEBUG_LOG(
            "Starting item export - will fetch items 1 to " << MAX_EXPORT_ITEM_ID << " with up to " << ITEMS_PER_FRAME
            << " concurrent requests");

        // Initialize export state
        isExporting = true;
        currentExportItemId = 1;
        pendingItemIds.clear();

        DEBUG_LOG("Export initialized. ProcessItemExport will be called each frame from processQueues.");
    }

    void StartItemExport() {
        isExporting = true;
        currentExportItemId = 1;
        pendingItemIds.clear();
    }
}
