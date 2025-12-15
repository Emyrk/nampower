//
// Created by pmacc on 9/21/2024.
//

#include "items.hpp"
#include "offsets.hpp"
#include "logging.hpp"
#include "helper.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "dbc_fields.hpp"

namespace Nampower {
    // Global dictionary to store itemId -> ItemStats_C mappings
    static std::unordered_map<uint32_t, game::ItemStats_C *> itemStatsCache;

    // Track pending async loads (can have multiple at once)
    static std::unordered_set<uint32_t> pendingItemIds;

    // Cache for itemName -> itemId mappings (case-insensitive)
    static std::unordered_map<std::string, uint32_t> itemNameToIdCache;

    // Export state tracking
    static bool isExporting = false;
    static uint32_t currentExportItemId = 0;
    static const uint32_t MAX_EXPORT_ITEM_ID = 100000;
    static const uint32_t ITEMS_PER_FRAME = 20;

    auto const getRow = reinterpret_cast<DBCache_ItemCacheDBGetRowT>(Offsets::DBCache_ItemCacheDBGetRow);
    auto const itemCache = reinterpret_cast<void *>(Offsets::ItemDBCache);

    // Define string keys as char arrays
    static char itemIdKey[] = "itemId";
    static char permanentEnchantIdKey[] = "permanentEnchantId";
    static char tempEnchantIdKey[] = "tempEnchantId";
    static char stackCountKey[] = "stackCount";
    static char durationKey[] = "duration";
    static char flagsKey[] = "flags";
    static char durabilityKey[] = "durability";
    static char maxDurabilityKey[] = "maxDurability";
    static char tempEnchantmentTimeLeftMsKey[] = "tempEnchantmentTimeLeftMs";
    static char tempEnchantmentChargesKey[] = "tempEnchantmentCharges";

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

    // Helper function to convert string to lowercase
    std::string ToLowerCase(const char *str) {
        std::string result;
        if (!str) return result;

        while (*str) {
            result += static_cast<char>(tolower(*str));
            ++str;
        }
        return result;
    }

    // Helper function to get itemId from itemName using cache
    // Returns 0 if not found in cache
    uint32_t GetItemIdFromCache(const char *itemName) {
        if (!itemName) return 0;

        std::string lowerName = ToLowerCase(itemName);
        auto it = itemNameToIdCache.find(lowerName);
        if (it != itemNameToIdCache.end()) {
            return it->second;
        }
        return 0;
    }

    // Helper function to add itemName -> itemId mapping to cache
    void CacheItemNameToId(const char *itemName, uint32_t itemId) {
        if (!itemName || itemId == 0) return;

        std::string lowerName = ToLowerCase(itemName);
        itemNameToIdCache[lowerName] = itemId;
    }

    // Helper function to check if an item matches search criteria
    bool DoesItemMatch(uint32_t itemId, uint32_t searchItemId, const char *searchItemName) {
        if (searchItemId != 0) {
            // Searching by ID
            return itemId == searchItemId;
        } else if (searchItemName) {
            // Searching by name
            auto itemStats = GetItemStats(itemId);
            if (itemStats && itemStats->m_displayName[0]) {
                bool matches = _stricmp(itemStats->m_displayName[0], searchItemName) == 0;
                // Cache the mapping if we found a match
                if (matches) {
                    CacheItemNameToId(searchItemName, itemId);
                }
                return matches;
            }
        }
        return false;
    }

    // Helper function to push item found result to Lua stack
    void PushItemFoundResult(uintptr_t *luaState, int32_t bagIndex, uint32_t slot) {
        // Adjust slot for special bags (assembly expects relative positions)
        uint32_t adjustedSlot = slot;
        if (bagIndex == 0) {
            adjustedSlot = slot - 0x17; // subtract 23
        } else if (bagIndex == -1) {
            adjustedSlot = slot - 0x27; // subtract 39
        } else if (bagIndex == -2) {
            adjustedSlot = slot - 0x51; // subtract 81
        }

        lua_pushnumber(luaState, static_cast<double>(bagIndex));
        lua_pushnumber(luaState, static_cast<double>(adjustedSlot + 1)); // lua is 1 indexed
    }

    // Helper function to get bag pointer from container
    uintptr_t *GetBagPtrFromContainer(uintptr_t *containerPtr) {
        auto vftable = game::GetObjectVFTable(containerPtr);
        using GetBagPtrT = uintptr_t * (__thiscall *)(uintptr_t *);
        auto getBagPtrFunc = reinterpret_cast<GetBagPtrT>(vftable[4]);
        return getBagPtrFunc(containerPtr);
    }

    // Helper function to create basic item info table for CGItem (equipped items from other units)
    void CreateBasicItemInfoTable(uintptr_t *luaState, game::CGItem *cgItem) {
        if (!cgItem) {
            lua_pushnil(luaState);
            return;
        }

        lua_newtable(luaState);

        // Add itemId
        lua_pushstring(luaState, itemIdKey);
        lua_pushnumber(luaState, static_cast<double>(cgItem->itemId));
        lua_settable(luaState, -3);

        // Add permanentEnchantId
        lua_pushstring(luaState, permanentEnchantIdKey);
        lua_pushnumber(luaState, static_cast<double>(cgItem->permanentEnchantId));
        lua_settable(luaState, -3);

        // Add tempEnchantId
        lua_pushstring(luaState, tempEnchantIdKey);
        lua_pushnumber(luaState, static_cast<double>(cgItem->tempEnchantId));
        lua_settable(luaState, -3);
    }

    // Helper function to create item info table on Lua stack (leaves table on stack)
    void CreateItemInfoTable(uintptr_t *luaState, game::CGItem_C *item) {
        if (!item || !item->itemFields) {
            lua_pushnil(luaState);
            return;
        }

        auto itemId = game::GetItemId(item);
        auto itemFields = item->itemFields;

        // Create item info table
        lua_newtable(luaState);

        // Add itemId
        lua_pushstring(luaState, itemIdKey);
        lua_pushnumber(luaState, static_cast<double>(itemId));
        lua_settable(luaState, -3);

        // Add stackCount
        lua_pushstring(luaState, stackCountKey);
        lua_pushnumber(luaState, static_cast<double>(itemFields->stackCount));
        lua_settable(luaState, -3);

        // Add duration
        lua_pushstring(luaState, durationKey);
        lua_pushnumber(luaState, static_cast<double>(itemFields->duration));
        lua_settable(luaState, -3);

        // Add flags
        lua_pushstring(luaState, flagsKey);
        lua_pushnumber(luaState, static_cast<double>(itemFields->flags));
        lua_settable(luaState, -3);

        // Add permanent enchantment
        lua_pushstring(luaState, permanentEnchantIdKey);
        lua_pushnumber(luaState, static_cast<double>(itemFields->permEnchantmentSlot.id));
        lua_settable(luaState, -3);

        // Add temp enchantment
        lua_pushstring(luaState, tempEnchantIdKey);
        lua_pushnumber(luaState, static_cast<double>(itemFields->tempEnchantmentSlot.id));
        lua_settable(luaState, -3);

        // Add temp enchantment time left
        lua_pushstring(luaState, tempEnchantmentTimeLeftMsKey);
        lua_pushnumber(luaState, static_cast<double>(itemFields->tempEnchantmentSlot.duration));
        lua_settable(luaState, -3);

        // Add temp enchantment charges
        lua_pushstring(luaState, tempEnchantmentChargesKey);
        lua_pushnumber(luaState, static_cast<double>(itemFields->tempEnchantmentSlot.charges));
        lua_settable(luaState, -3);

        // Add durability
        lua_pushstring(luaState, durabilityKey);
        lua_pushnumber(luaState, static_cast<double>(itemFields->durability));
        lua_settable(luaState, -3);

        // Add max durability
        lua_pushstring(luaState, maxDurabilityKey);
        lua_pushnumber(luaState, static_cast<double>(itemFields->maxDurability));
        lua_settable(luaState, -3);
    }

    // Helper function to push bag item to parent table on Lua stack
    void PushBagCGItemToTable(uintptr_t *luaState, int32_t bagIndex, uint32_t slot, game::CGItem_C *item) {
        if (!item) {
            DEBUG_LOG("missing item at slot " << slot);
            return;
        }
        if (!item->itemFields) {
            DEBUG_LOG("missing itemfields at slot " << slot << " itemId " << game::GetItemId(item));
            return;
        }

        // Adjust slot for special bags (assembly expects relative positions)
        uint32_t adjustedSlot = slot;
        if (bagIndex == 0) {
            adjustedSlot = slot - 0x17; // subtract 23
        } else if (bagIndex == -1) {
            adjustedSlot = slot - 0x27; // subtract 39
        } else if (bagIndex == -2) {
            adjustedSlot = slot - 0x51; // subtract 81
        }

        // Push slot number as key
        lua_pushnumber(luaState, static_cast<double>(adjustedSlot + 1)); // lua is 1 indexed

        // Create and push item info table
        CreateItemInfoTable(luaState, item);

        // Set parent_table[slot] = itemInfo
        lua_settable(luaState, -3);
    }

    uint32_t FindPlayerItemSlot(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        // Get player unit
        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto playerUnit = game::GetObjectPtr(playerGuid);

        if (!playerUnit) {
            lua_error(luaState, "Unable to get player unit");
            return 0;
        }

        uint32_t searchItemId = 0;
        const char *searchItemName = nullptr;

        // Check if first param is string (item name) or number (item id)
        if (lua_isnumber(luaState, 1)) {
            searchItemId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        } else if (lua_isstring(luaState, 1)) {
            searchItemName = lua_tostring(luaState, 1);
            // Check cache first to avoid string comparisons
            uint32_t cachedItemId = GetItemIdFromCache(searchItemName);
            if (cachedItemId != 0) {
                searchItemId = cachedItemId;
                searchItemName = nullptr; // Use ID instead for faster search
            }
        } else {
            lua_error(luaState, "Usage: FindPlayerItemSlot(itemName) or FindPlayerItemSlot(itemId)");
            return 0;
        }

        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
        auto const getContainerGuid = reinterpret_cast<GetContainerGuidT>(Offsets::GetContainerGuid);

        auto inventory = game::GetPlayerInventoryPtr(playerUnit);

        // Loop through equipment slots 0->18
        for (uint32_t slot = 0; slot <= 18; slot++) {
            auto item = getBagItem(inventory, slot);
            if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                lua_pushnil(luaState); // bagIndex = nil for equipped items
                lua_pushnumber(luaState, static_cast<double>(slot+1)); // lua is 1 indexed
                return 2;
            }
        }

        // Loop through inventory pack slots 23-38 (bag 0)
        for (uint32_t slot = 23; slot <= 38; slot++) {
            auto item = getBagItem(inventory, slot);
            if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                PushItemFoundResult(luaState, 0, slot);
                return 2;
            }
        }

        // Loop through bags 1-4
        for (int32_t bagIndex = 1; bagIndex <= 4; bagIndex++) {
            uint64_t containerGuid = getContainerGuid(bagIndex - 1); // bagIndex 1-4 maps to container 0-3
            if (containerGuid == 0) continue;

            auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
            if (!containerPtr) continue;

            auto bagPtr = GetBagPtrFromContainer(containerPtr);
            if (!bagPtr) continue;

            auto bagSize = *bagPtr;
            for (uint32_t slot = 0; slot < bagSize; slot++) {
                auto item = getBagItem(bagPtr, slot);
                if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                    PushItemFoundResult(luaState, bagIndex, slot);
                    return 2;
                }
            }
        }

        uint64_t bankGuid = *reinterpret_cast<uint64_t *>(Offsets::BankGuid);

        if (bankGuid > 0) {
            // Loop through bank item slots 39-62 (bag -1)
            for (uint32_t slot = 39; slot <= 62; slot++) {
                auto item = getBagItem(inventory, slot);
                if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                    PushItemFoundResult(luaState, -1, slot);
                    return 2;
                }
            }

            // Loop through bank bags 5-9
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
                    if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                        PushItemFoundResult(luaState, bagIndex, slot);
                        return 2;
                    }
                }
            }
        }

        // Loop through keyring slots 81-96 (bag -2)
        for (uint32_t slot = 81; slot <= 96; slot++) {
            auto item = getBagItem(inventory, slot);
            if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                PushItemFoundResult(luaState, -2, slot);
                return 2;
            }
        }

        // Item not found
        lua_pushnil(luaState);
        lua_pushnil(luaState);
        return 2;
    }

    uint32_t GetEquippedItems(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        uint64_t guid;

        // Check if a parameter was provided
        if (lua_gettop(luaState) >= 1) {
            guid = GetUnitGuidFromLuaParam(luaState, 1);
            if (guid == 0) {
                lua_error(luaState, "Usage: GetEquippedItems() or GetEquippedItems(unitStr) or GetEquippedItems(guid)");
                return 0;
            }
        } else {
            // No parameter, use player
            guid = game::ClntObjMgrGetActivePlayerGuid();
        }

        auto unit = game::GetObjectPtr(guid);

        if (!unit) {
            lua_error(luaState, "Unable to get unit");
            return 0;
        }

        auto const canInspectUnit = reinterpret_cast<CanInspectUnitT>(Offsets::CanInspectUnit);
        if (!canInspectUnit(unit)) {
            lua_error(luaState, "Cannot inspect unit");
            return 0;
        }

        // Create a new Lua table
        lua_newtable(luaState);

        // Check if this is the active player
        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        bool isPlayer = (guid == playerGuid);

        if (isPlayer) {
            // For player, use getBagItem with inventory to get full item data
            auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
            auto inventory = game::GetPlayerInventoryPtr(unit);

            for (uint32_t slot = 0; slot <= 18; slot++) {
                auto item = getBagItem(inventory, slot);
                if (item) {
                    // Equipment slots don't need bagIndex adjustment (pass -999 as a flag)
                    PushBagCGItemToTable(luaState, -999, slot, item);
                }
            }
        } else {
            // For other units, use CGUnit_C_GetEquippedItemAtSlot (limited data)
            auto const getEquippedItem = reinterpret_cast<CGUnit_C_GetEquippedItemAtSlotT>(
                Offsets::CGUnit_C_GetEquippedItemAtSlot);

            for (uint32_t slot = 0; slot <= 18; slot++) {
                auto cgItem = getEquippedItem(unit, slot);
                if (cgItem) {
                    // Push slot number as key
                    lua_pushnumber(luaState, static_cast<double>(slot+1)); // lua is 1 indexed

                    // Create and push basic item info table
                    CreateBasicItemInfoTable(luaState, cgItem);

                    // Set result[slot] = item info table
                    lua_settable(luaState, -3);
                }
            }
        }

        return 1;
    }

    uint32_t GetEquippedItem(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        // Check for required parameters
        if (lua_gettop(luaState) < 2) {
            lua_error(luaState, "Usage: GetEquippedItem(unitStr, slot) or GetEquippedItem(guid, slot)");
            return 0;
        }

        // Get unit GUID from first parameter
        uint64_t guid;
        if (lua_gettop(luaState) >= 1) {
            guid = GetUnitGuidFromLuaParam(luaState, 1);
            if (guid == 0) {
                lua_error(luaState, "Usage: GetEquippedItem(unitStr, slot) or GetEquippedItem(guid, slot)");
                return 0;
            }
        } else {
            // No parameter, use player
            guid = game::ClntObjMgrGetActivePlayerGuid();
        }

        // Get slot from second parameter
        if (!lua_isnumber(luaState, 2)) {
            lua_error(luaState, "Slot must be a number between 1 and 19");
            return 0;
        }

        auto luaSlot = static_cast<uint32_t>(lua_tonumber(luaState, 2));

        // Check slot is in valid range
        if (luaSlot < 1 || luaSlot > 19) {
            lua_error(luaState, "Slot must be between 1 and 19");
            return 0;
        }

        auto unit = game::GetObjectPtr(guid);
        if (!unit) {
            lua_error(luaState, "Unable to get unit");
            return 0;
        }

        auto const canInspectUnit = reinterpret_cast<CanInspectUnitT>(Offsets::CanInspectUnit);
        if (!canInspectUnit(unit)) {
            lua_error(luaState, "Cannot inspect unit");
            return 0;
        }

        // Check if this is the active player
        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        bool isPlayer = (guid == playerGuid);

        if (isPlayer) {
            // For player, use getBagItem with inventory to get full item data
            auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
            auto inventory = game::GetPlayerInventoryPtr(unit);

            auto item = getBagItem(inventory, luaSlot-1); // luaSlot is 1-indexed
            CreateItemInfoTable(luaState, item);
        } else {
            // For other units, use CGUnit_C_GetEquippedItemAtSlot (limited data)
            auto const getEquippedItem = reinterpret_cast<CGUnit_C_GetEquippedItemAtSlotT>(
                Offsets::CGUnit_C_GetEquippedItemAtSlot);

            auto cgItem = getEquippedItem(unit, luaSlot - 1); // luaSlot is 1-indexed
            CreateBasicItemInfoTable(luaState, cgItem);
        }

        return 1;
    }

    // Helper function to convert Lua slot (1-indexed, relative) to absolute slot
    uint32_t ConvertLuaSlot(int32_t bagIndex, uint32_t luaSlot) {
        // Reverse the adjustments done in PushItemFoundResult/PushBagCGItemToTable
        // luaSlot is 1-indexed, so subtract 1 first to get 0-indexed relative slot
        uint32_t relativeSlot = luaSlot - 1;

        // Add the offset for special bags to get absolute slot
        if (bagIndex == 0) {
            return relativeSlot + 0x17; // add 23 -> absolute slots 23-38
        } else if (bagIndex == -1) {
            return relativeSlot + 0x27; // add 39 -> absolute slots 39-62
        } else if (bagIndex == -2) {
            return relativeSlot + 0x51; // add 81 -> absolute slots 81-96
        } else {
            // Regular bags (1-4, 5-9): slots are already relative (0-indexed)
            return relativeSlot;
        }
    }

    uint32_t GetBagItem(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        // Check for required parameters
        if (lua_gettop(luaState) < 2) {
            lua_error(luaState, "Usage: GetBagItem(bagIndex, slot)");
            return 0;
        }

        // Get bag index from first parameter
        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Bag index must be a number");
            return 0;
        }
        auto bagIndex = static_cast<int32_t>(lua_tonumber(luaState, 1));

        // Get slot from second parameter
        if (!lua_isnumber(luaState, 2)) {
            lua_error(luaState, "Slot must be a number");
            return 0;
        }
        auto luaSlot = static_cast<uint32_t>(lua_tonumber(luaState, 2));

        auto slot = ConvertLuaSlot(bagIndex, luaSlot);

        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
        auto const getContainerGuid = reinterpret_cast<GetContainerGuidT>(Offsets::GetContainerGuid);

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto player = game::GetObjectPtr(playerGuid);
        auto inventory = game::GetPlayerInventoryPtr(player);

        game::CGItem_C *item = nullptr;

        if (bagIndex == 0) {
            // Bag 0: inventory pack slots 23-38 (Lua slots 1-16)
            if (slot < 23 || slot > 38) {
                lua_error(luaState, "Slot must be between 1 and 16 for bag 1");
                return 0;
            }
            item = getBagItem(inventory, slot);
        } else if (bagIndex >= 1 && bagIndex <= 4) {
            // Bags 1-4: regular bags
            uint64_t containerGuid = getContainerGuid(bagIndex-1); // bagIndex 1-4 maps to container 0-3
            if (containerGuid == 0) {
                lua_pushnil(luaState);
                return 1;
            }

            auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
            if (!containerPtr) {
                lua_pushnil(luaState);
                return 1;
            }

            auto bagPtr = GetBagPtrFromContainer(containerPtr);
            if (!bagPtr) {
                lua_pushnil(luaState);
                return 1;
            }

            auto bagSize = *bagPtr;
            if (slot >= bagSize) {
                lua_error(luaState, "Slot exceeds bag size");
                return 0;
            }

            item = getBagItem(bagPtr, slot);
        } else if (bagIndex == -1) {
            // Bag -1: bank item slots 39-62 or buyback slots 69-80
            uint64_t bankGuid = *reinterpret_cast<uint64_t *>(Offsets::BankGuid);
            if (bankGuid == 0 && slot >= 39 && slot <= 62) {
                lua_error(luaState, "Bank is not open");
                return 0;
            }

            if ((slot >= 39 && slot <= 62) || (slot >= 69 && slot <= 80)) {
                item = getBagItem(inventory, slot);
            } else {
                lua_error(luaState, "For bag -1, slot must be 1-24 (bank) or 31-42 (buyback) (Lua 1-indexed)");
                return 0;
            }
        } else if (bagIndex >= 4 && bagIndex <= 8) {
            // Bank bags 4-8
            uint64_t bankGuid = *reinterpret_cast<uint64_t *>(Offsets::BankGuid);
            if (bankGuid == 0) {
                lua_error(luaState, "Bank is not open");
                return 0;
            }

            uint64_t containerGuid = getContainerGuid(bagIndex);
            if (containerGuid == 0) {
                lua_pushnil(luaState);
                return 1;
            }

            auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
            if (!containerPtr) {
                lua_pushnil(luaState);
                return 1;
            }

            auto bagPtr = GetBagPtrFromContainer(containerPtr);
            if (!bagPtr) {
                lua_pushnil(luaState);
                return 1;
            }

            auto bagSize = *bagPtr;
            if (slot >= bagSize) {
                lua_error(luaState, "Slot exceeds bag size");
                return 0;
            }

            item = getBagItem(bagPtr, slot);
        } else if (bagIndex == -2) {
            // Bag -2: keyring slots 81-96 (Lua slots 1-16)
            if (slot < 81 || slot > 96) {
                lua_error(luaState, "For bag -2 (keyring), slot must be 1-16 (Lua 1-indexed)");
                return 0;
            }
            item = getBagItem(inventory, slot);
        } else {
            lua_error(luaState, "Invalid bag index. Valid values: 0, 1-4, -1, 5-9 (bank), -2 (keyring)");
            return 0;
        }

        CreateItemInfoTable(luaState, item);
        return 1;
    }

    uint32_t GetBagItems(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto const getContainerGuid = reinterpret_cast<GetContainerGuidT>(Offsets::GetContainerGuid);
        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);

        // Create main result table: { [bagIndex] = { [slot] = itemInfo } }
        lua_newtable(luaState);

        // bag 0 is special
        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto player = game::GetObjectPtr(playerGuid);
        auto inventory = game::GetPlayerInventoryPtr(player);

        // Push bag index 0 as key
        lua_pushnumber(luaState, static_cast<double>(0));

        // Create bag table for this bag
        lua_newtable(luaState);

        // look through inventory pack slots 23 -> 38
        for (uint32_t slot = 23; slot <= 38; slot++) {
            auto item = getBagItem(inventory, slot);
            if (item) {
                PushBagCGItemToTable(luaState, 0, slot, item);
            }
        }

        // Set result[bagIndex] = bag table
        lua_settable(luaState, -3);

        // Process regular bags index 1-4
        for (int32_t bagIndex = 1; bagIndex <= 4; bagIndex++) {
            uint64_t containerGuid = getContainerGuid(bagIndex - 1); // bagIndex 1-4 maps to container 0-3
            if (containerGuid == 0) continue;

            auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
            if (!containerPtr) continue;

            auto bagPtr = GetBagPtrFromContainer(containerPtr);
            if (!bagPtr) continue;

            // Push bag index as key
            lua_pushnumber(luaState, static_cast<double>(bagIndex));

            // Create bag table for this bag
            lua_newtable(luaState);

            auto bagSize = *bagPtr;
            for (uint32_t slot = 0; slot < bagSize; slot++) {
                auto item = getBagItem(bagPtr, slot);
                if (item) {
                    PushBagCGItemToTable(luaState, bagIndex, slot, item);
                }
            }

            // Set result[bagIndex] = bag table
            lua_settable(luaState, -3);
        }

        // Check if bank is available
        uint64_t bankGuid = *reinterpret_cast<uint64_t *>(Offsets::BankGuid);
        if (bankGuid > 0) {
            // Process bank bags 5-9
            for (int32_t bagIndex = 5; bagIndex <= 9; bagIndex++) {
                uint64_t containerGuid = getContainerGuid(bagIndex - 1); // bagIndex 5-9 maps to container 4-8
                if (containerGuid == 0) continue;

                auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
                if (!containerPtr) continue;

                auto bagPtr = GetBagPtrFromContainer(containerPtr);
                if (!bagPtr) continue;

                // Push bag index as key
                lua_pushnumber(luaState, static_cast<double>(bagIndex));

                // Create bag table for this bank bag
                lua_newtable(luaState);

                auto bagSize = *bagPtr;
                for (uint32_t slot = 0; slot < bagSize; slot++) {
                    auto item = getBagItem(bagPtr, slot);
                    if (item) {
                        PushBagCGItemToTable(luaState, bagIndex, slot, item);
                    }
                }

                // Set result[bagIndex] = bag table
                lua_settable(luaState, -3);
            }
        }

        return 1;
    }
}
