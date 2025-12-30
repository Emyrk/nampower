#include "item_scripts.hpp"

#include "helper.hpp"
#include "items.hpp"
#include "logging.hpp"
#include "lua_refs.hpp"
#include "offsets.hpp"

#include <string>
#include <array>

namespace Nampower {
    // Reusable table references to reduce memory allocations
    static int basicItemInfoTableRef = LUA_REFNIL;
    static int itemInfoTableRef = LUA_REFNIL;
    static int equippedItemsTableRef = LUA_REFNIL;
    static int bagItemsTableRef = LUA_REFNIL;
    static int trinketsTableRef = LUA_REFNIL;
    static uint32_t lastTrinketCount = 0;

    // Bag items table references (10 bags: 0-9, plus special indices for -1 bank, -2 keyring)
    static std::array<int, MAX_BAGS> bagTableRefs{};

    // Item entry table references: 2D matrix [bagIndex][slot] for direct mapping
    static std::array<std::array<int, MAX_BAG_SLOTS>, MAX_BAGS> itemEntryTableRefs{};

    // Equipped item table references (slots 0-18)
    static std::array<int, MAX_EQUIPPED_SLOTS> equippedItemTableRefs{};

    // String keys used when pushing item data to Lua
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
    static char bagIndexKey[] = "bagIndex";
    static char slotIndexKey[] = "slotIndex";
    static char trinketNameKey[] = "trinketName";
    static char textureKey[] = "texture";
    static char itemLevelKey[] = "itemLevel";

    void PushItemFoundResult(uintptr_t *luaState, int32_t bagIndex, uint32_t slot) {
        uint32_t adjustedSlot = slot;
        if (bagIndex == 0) {
            adjustedSlot = slot - 0x17; // subtract 23
        } else if (bagIndex == BANK_BAG_INDEX) {
            adjustedSlot = slot - 0x27; // subtract 39
        } else if (bagIndex == KEYRING_BAG_INDEX) {
            adjustedSlot = slot - 0x51; // subtract 81
        }

        lua_pushnumber(luaState, static_cast<double>(bagIndex));
        lua_pushnumber(luaState, static_cast<double>(adjustedSlot + 1)); // lua is 1 indexed
    }

    void CreateBasicItemInfoTable(uintptr_t *luaState, game::CGItem *cgItem) {
        if (!cgItem) {
            lua_pushnil(luaState);
            return;
        }

        // Get or create reusable table
        GetTableRef(luaState, basicItemInfoTableRef);

        PushTableValue(luaState, itemIdKey, cgItem->itemId);
        PushTableValue(luaState, permanentEnchantIdKey, cgItem->permanentEnchantId);
        PushTableValue(luaState, tempEnchantIdKey, cgItem->tempEnchantId);
    }

    void CreateItemInfoTable(uintptr_t *luaState, game::CGItem_C *item) {
        if (!item || !item->itemFields) {
            lua_pushnil(luaState);
            return;
        }

        auto itemId = game::GetItemId(item);
        auto itemFields = item->itemFields;

        // Get or create reusable table
        GetTableRef(luaState, itemInfoTableRef);

        PushTableValue(luaState, itemIdKey, itemId);
        PushTableValue(luaState, stackCountKey, itemFields->stackCount);
        PushTableValue(luaState, durationKey, itemFields->duration);
        PushTableValue(luaState, flagsKey, itemFields->flags);
        PushTableValue(luaState, permanentEnchantIdKey, itemFields->permEnchantmentSlot.id);
        PushTableValue(luaState, tempEnchantIdKey, itemFields->tempEnchantmentSlot.id);
        PushTableValue(luaState, tempEnchantmentTimeLeftMsKey, itemFields->tempEnchantmentSlot.duration);
        PushTableValue(luaState, tempEnchantmentChargesKey, itemFields->tempEnchantmentSlot.charges);
        PushTableValue(luaState, durabilityKey, itemFields->durability);
        PushTableValue(luaState, maxDurabilityKey, itemFields->maxDurability);
    }

    void ClearBagSlot(uintptr_t *luaState, int32_t bagIndex, uint32_t adjustedSlot) {
        // Clear from cache
        CachedItemData* cachedData = nullptr;

        if (bagIndex == -1) {
            // Bank (map to cache index 10)
            if (adjustedSlot < MAX_BAG_SLOTS) {
                cachedData = &bagItemDataCache[BANK_CACHE_INDEX][adjustedSlot];
            }
        } else if (bagIndex == -2) {
            // Keyring (map to cache index 11)
            if (adjustedSlot < MAX_BAG_SLOTS) {
                cachedData = &bagItemDataCache[KEYRING_CACHE_INDEX][adjustedSlot];
            }
        } else if (bagIndex >= 0 && bagIndex < 10 && adjustedSlot < MAX_BAG_SLOTS) {
            // Regular bags (0-9)
            cachedData = &bagItemDataCache[bagIndex][adjustedSlot];
        }

        if (cachedData && cachedData->itemId != 0) {
            // Clear the cache
            *cachedData = CachedItemData();

            // Clear from Lua table (set to nil)
            lua_pushnumber(luaState, static_cast<double>(adjustedSlot + 1));
            lua_pushnil(luaState);
            lua_settable(luaState, -3);
        }
    }

    void PushBagCGItemToTable(uintptr_t *luaState, int32_t bagIndex, uint32_t slot, game::CGItem_C *item) {
        if (!item) {
            DEBUG_LOG("missing item at slot " << slot);
            return;
        }
        if (!item->itemFields) {
            DEBUG_LOG("missing itemfields at slot " << slot << " itemId " << game::GetItemId(item));
            return;
        }

        uint32_t adjustedSlot = slot;
        if (bagIndex == 0) {
            adjustedSlot = slot - 0x17; // subtract 23
        } else if (bagIndex == -1) {
            adjustedSlot = slot - 0x27; // subtract 39
        } else if (bagIndex == -2) {
            adjustedSlot = slot - 0x51; // subtract 81
        }

        // Extract current item data
        auto itemId = game::GetItemId(item);
        auto itemFields = item->itemFields;

        CachedItemData currentData;
        currentData.itemId = itemId;
        currentData.stackCount = itemFields->stackCount;
        currentData.duration = itemFields->duration;
        currentData.flags = itemFields->flags;
        currentData.permanentEnchantId = itemFields->permEnchantmentSlot.id;
        currentData.tempEnchantId = itemFields->tempEnchantmentSlot.id;
        currentData.tempEnchantmentTimeLeftMs = itemFields->tempEnchantmentSlot.duration;
        currentData.tempEnchantmentCharges = itemFields->tempEnchantmentSlot.charges;
        currentData.durability = itemFields->durability;
        currentData.maxDurability = itemFields->maxDurability;

        // Get cached data for comparison
        CachedItemData* cachedData = nullptr;

        if (bagIndex == -999) {
            // Equipped items
            if (slot < MAX_EQUIPPED_SLOTS) {
                cachedData = &equippedItemDataCache[slot];
            }
        } else if (bagIndex == -1) {
            // Bank (map to cache index 10)
            if (adjustedSlot < MAX_BAG_SLOTS) {
                cachedData = &bagItemDataCache[BANK_CACHE_INDEX][adjustedSlot];
            }
        } else if (bagIndex == -2) {
            // Keyring (map to cache index 11)
            if (adjustedSlot < MAX_BAG_SLOTS) {
                cachedData = &bagItemDataCache[KEYRING_CACHE_INDEX][adjustedSlot];
            }
        } else if (bagIndex >= 0 && bagIndex < 10) {
            // Regular bag items (0-9)
            if (adjustedSlot < MAX_BAG_SLOTS) {
                cachedData = &bagItemDataCache[bagIndex][adjustedSlot];
            }
        }

        // Check if data has changed
        bool hasChanged = true;
        if (cachedData != nullptr) {
            hasChanged = (currentData != *cachedData);
        }

        // Only update Lua table if data has changed
        if (hasChanged) {
            lua_pushnumber(luaState, static_cast<double>(adjustedSlot + 1)); // lua is 1 indexed

            // Get table ref based on bag position (direct 2D mapping)
            if (bagIndex == -999) {
                // Equipped items
                if (slot < MAX_EQUIPPED_SLOTS) {
                    GetTableRef(luaState, equippedItemTableRefs[slot]);
                } else {
                    lua_newtable(luaState);
                }
            } else {
                // Bag items - map to cache index
                uint32_t cacheIndexForTableRef = 0;
                if (bagIndex == -1) {
                    cacheIndexForTableRef = BANK_CACHE_INDEX;
                } else if (bagIndex == -2) {
                    cacheIndexForTableRef = KEYRING_CACHE_INDEX;
                } else if (bagIndex >= 0 && bagIndex < 10) {
                    cacheIndexForTableRef = bagIndex;
                }

                if (cacheIndexForTableRef < MAX_BAGS && adjustedSlot < MAX_BAG_SLOTS) {
                    GetTableRef(luaState, itemEntryTableRefs[cacheIndexForTableRef][adjustedSlot]);
                } else {
                    lua_newtable(luaState);
                }
            }

            PushTableValue(luaState, itemIdKey, itemId);
            PushTableValue(luaState, stackCountKey, itemFields->stackCount);
            PushTableValue(luaState, durationKey, itemFields->duration);
            PushTableValue(luaState, flagsKey, itemFields->flags);
            PushTableValue(luaState, permanentEnchantIdKey, itemFields->permEnchantmentSlot.id);
            PushTableValue(luaState, tempEnchantIdKey, itemFields->tempEnchantmentSlot.id);
            PushTableValue(luaState, tempEnchantmentTimeLeftMsKey, itemFields->tempEnchantmentSlot.duration);
            PushTableValue(luaState, tempEnchantmentChargesKey, itemFields->tempEnchantmentSlot.charges);
            PushTableValue(luaState, durabilityKey, itemFields->durability);
            PushTableValue(luaState, maxDurabilityKey, itemFields->maxDurability);

            lua_settable(luaState, -3);

            // Update cache
            if (cachedData != nullptr) {
                *cachedData = currentData;
            }
        }
    }

    uint32_t Script_FindPlayerItemSlot(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto playerUnit = game::GetObjectPtr(playerGuid);

        if (!playerUnit) {
            lua_error(luaState, "Unable to get player unit");
            return 0;
        }

        uint32_t searchItemId = 0;
        const char *searchItemName = nullptr;

        if (lua_isnumber(luaState, 1)) {
            searchItemId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        } else if (lua_isstring(luaState, 1)) {
            searchItemName = lua_tostring(luaState, 1);
            uint32_t cachedItemId = GetItemIdFromCache(searchItemName);
            if (cachedItemId != 0) {
                searchItemId = cachedItemId;
                searchItemName = nullptr;
            }
        } else {
            lua_error(luaState, "Usage: FindPlayerItemSlot(itemName) or FindPlayerItemSlot(itemId)");
            return 0;
        }

        auto result = FindPlayerItem(searchItemId, searchItemName);
        if (!result.found()) {
            lua_pushnil(luaState);
            lua_pushnil(luaState);
            return 2;
        }

        if (result.bagIndex == EQUIPPED_BAG_INDEX) {
            lua_pushnil(luaState);
            lua_pushnumber(luaState, static_cast<double>(result.slot + 1));
            return 2;
        }

        PushItemFoundResult(luaState, result.bagIndex, result.slot);
        return 2;
    }

    uint32_t Script_UseItemIdOrName(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1) && !lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: UseItemIdOrName(itemIdOrName, [target])");
            return 0;
        }

        uint32_t searchItemId = 0;
        const char *searchItemName = nullptr;

        if (lua_isnumber(luaState, 1)) {
            searchItemId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        } else {
            searchItemName = lua_tostring(luaState, 1);
            uint32_t cachedItemId = GetItemIdFromCache(searchItemName);
            if (cachedItemId != 0) {
                searchItemId = cachedItemId;
                searchItemName = nullptr;
            }
        }

        uint64_t targetGuid = 0;
        if (lua_gettop(luaState) >= 2) {
            if (!lua_isnumber(luaState, 2) && !lua_isstring(luaState, 2)) {
                lua_error(luaState, "Usage: UseItemIdOrName(itemIdOrName, [target])");
                return 0;
            }

            targetGuid = GetUnitGuidFromLuaParam(luaState, 2);
            if (targetGuid == 0) {
                lua_error(luaState, "Unable to determine target guid");
                return 0;
            }
        } else {
            targetGuid = *reinterpret_cast<uint64_t *>(Offsets::LockedTargetGuid);
            if (targetGuid == 0) {
                targetGuid = game::ClntObjMgrGetActivePlayerGuid();
            }
        }

        auto itemSearchResult = FindPlayerItem(searchItemId, searchItemName);
        if (!itemSearchResult.found()) {
            lua_pushnumber(luaState, 0);
            return 1;
        }

        using CGItem_C_UseT = uint32_t(__thiscall *)(game::CGItem_C *this_ptr, uint64_t *targetGuid, int useBindConfirm);
        auto const useItem = reinterpret_cast<CGItem_C_UseT>(Offsets::CGItem_C_Use);

        auto const result = useItem(itemSearchResult.item, &targetGuid, 0);
        lua_pushnumber(luaState, result);
        return 1;
    }

    uint32_t Script_UseTrinket(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1) && !lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: UseTrinket(slot|itemIdOrName, [target])");
            return 0;
        }

        uint32_t trinketInvSlot = 0;
        uint32_t searchItemId = 0;
        const char *searchItemName = nullptr;

        if (lua_isnumber(luaState, 1)) {
            auto param = static_cast<uint32_t>(lua_tonumber(luaState, 1));
            if (param == 1 || param == 13) {
                trinketInvSlot = 12;
            } else if (param == 2 || param == 14) {
                trinketInvSlot = 13;
            } else {
                searchItemId = param;
            }
        } else {
            searchItemName = lua_tostring(luaState, 1);
            uint32_t cachedItemId = GetItemIdFromCache(searchItemName);
            if (cachedItemId != 0) {
                searchItemId = cachedItemId;
                searchItemName = nullptr;
            }
        }

        uint64_t targetGuid = 0;
        if (lua_gettop(luaState) >= 2) {
            if (!lua_isnumber(luaState, 2) && !lua_isstring(luaState, 2)) {
                lua_error(luaState, "Usage: UseTrinket(slot|itemIdOrName, [target])");
                return 0;
            }

            targetGuid = GetUnitGuidFromLuaParam(luaState, 2);
            if (targetGuid == 0) {
                lua_error(luaState, "Unable to determine target guid");
                return 0;
            }
        } else {
            targetGuid = *reinterpret_cast<uint64_t *>(Offsets::LockedTargetGuid);
            if (targetGuid == 0) {
                targetGuid = game::ClntObjMgrGetActivePlayerGuid();
            }
        }

        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto playerUnit = game::GetObjectPtr(playerGuid);
        if (!playerUnit) {
            lua_pushnumber(luaState, -1);
            return 1;
        }

        auto inventory = game::GetPlayerInventoryPtr(playerUnit);

        game::CGItem_C *item = nullptr;

        if (trinketInvSlot == 12 || trinketInvSlot == 13) {
            item = getBagItem(inventory, trinketInvSlot);
        } else {
            for (uint32_t slot : {12u, 13u}) {
                auto candidate = getBagItem(inventory, slot);
                if (candidate && DoesItemMatch(game::GetItemId(candidate), searchItemId, searchItemName)) {
                    item = candidate;
                    break;
                }
            }
        }

        if (!item) {
            lua_pushnumber(luaState, -1);
            return 1;
        }

        using CGItem_C_UseT = uint32_t(__thiscall *)(game::CGItem_C *this_ptr, uint64_t *targetGuid, int useBindConfirm);
        auto const useItem = reinterpret_cast<CGItem_C_UseT>(Offsets::CGItem_C_Use);

        auto const result = useItem(item, &targetGuid, 0);
        lua_pushnumber(luaState, result);
        return 1;
    }

    uint32_t Script_GetTrinkets(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
        auto const getContainerGuid = reinterpret_cast<GetContainerGuidT>(Offsets::GetContainerGuid);
        auto const getInventoryArt = reinterpret_cast<GetInventoryArtT>(Offsets::CGItem_C_GetInventoryArt);

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto playerUnit = game::GetObjectPtr(playerGuid);

        bool copyTable = false;
        if (lua_isnumber(luaState, 1)) {
            copyTable = static_cast<int>(lua_tonumber(luaState, 1)) != 0;
        }

        if (copyTable) {
            lua_newtable(luaState);
        } else {
            GetTableRef(luaState, trinketsTableRef);
        }

        if (!playerUnit) {
            return 1;
        }

        auto inventory = game::GetPlayerInventoryPtr(playerUnit);
        uint32_t luaIndex = 1;

        auto pushTrinket = [&](int32_t bagIndex, uint32_t slot, game::CGItem_C *item) {
            auto itemId = item ? game::GetItemId(item) : 0;

            // Calculate adjusted slot for cache indexing
            uint32_t adjustedSlot = slot;
            uint32_t luaSlot = slot + 1;
            if (bagIndex == 0) {
                adjustedSlot = slot - 0x17; // backpack absolute slots 23-38
                luaSlot = adjustedSlot + 1;
            } else if (bagIndex == BANK_BAG_INDEX) {
                adjustedSlot = slot - 0x27; // bank absolute slots 39-62
                luaSlot = adjustedSlot + 1;
            } else if (bagIndex == KEYRING_BAG_INDEX) {
                adjustedSlot = slot - 0x51; // keyring absolute slots 81-96
                luaSlot = adjustedSlot + 1;
            }

            // Get cached data pointer based on location
            CachedTrinketData* cachedData = nullptr;
            if (bagIndex == EQUIPPED_BAG_INDEX) {
                if (slot < MAX_EQUIPPED_SLOTS) {
                    cachedData = &equippedTrinketDataCache[slot];
                }
            } else if (bagIndex == BANK_BAG_INDEX) {
                if (adjustedSlot < MAX_BAG_SLOTS) {
                    cachedData = &bagTrinketDataCache[BANK_CACHE_INDEX][adjustedSlot];
                }
            } else if (bagIndex == KEYRING_BAG_INDEX) {
                if (adjustedSlot < MAX_BAG_SLOTS) {
                    cachedData = &bagTrinketDataCache[KEYRING_CACHE_INDEX][adjustedSlot];
                }
            } else if (bagIndex >= 0 && bagIndex < 10) {
                if (adjustedSlot < MAX_BAG_SLOTS) {
                    cachedData = &bagTrinketDataCache[bagIndex][adjustedSlot];
                }
            }

            // Check if this is a trinket and if data has changed
            bool isTrinket = false;
            if (item) {
                auto itemStats = GetItemStats(itemId);
                if (itemStats && itemStats->m_inventoryType == game::INVTYPE_TRINKET) {
                    isTrinket = true;
                }
            }

            // Build current trinket data for comparison
            CachedTrinketData currentData;
            currentData.itemId = isTrinket ? itemId : 0;

            // Check if data has changed
            bool hasChanged = true;
            if (!copyTable && cachedData != nullptr) {
                hasChanged = (currentData != *cachedData);
            }

            // Always push trinkets to maintain sequential indices
            if (isTrinket) {
                auto itemStats = GetItemStats(itemId);

                lua_pushnumber(luaState, static_cast<double>(luaIndex));

                if (copyTable) {
                    lua_newtable(luaState);
                } else {
                    // Get table ref based on location (direct 2D mapping)
                    if (bagIndex == EQUIPPED_BAG_INDEX) {
                        if (slot < MAX_EQUIPPED_SLOTS) {
                            GetTableRef(luaState, equippedItemTableRefs[slot]);
                        } else {
                            lua_newtable(luaState);
                        }
                    } else {
                        uint32_t cacheIndexForTableRef = 0;
                        if (bagIndex == BANK_BAG_INDEX) {
                            cacheIndexForTableRef = BANK_CACHE_INDEX;
                        } else if (bagIndex == KEYRING_BAG_INDEX) {
                            cacheIndexForTableRef = KEYRING_CACHE_INDEX;
                        } else if (bagIndex >= 0 && bagIndex < 10) {
                            cacheIndexForTableRef = bagIndex;
                        }

                        if (adjustedSlot < MAX_BAG_SLOTS) {
                            GetTableRef(luaState, itemEntryTableRefs[cacheIndexForTableRef][adjustedSlot]);
                        } else {
                            lua_newtable(luaState);
                        }
                    }
                }

                // Only update trinket details if data changed
                if (copyTable || hasChanged) {
                    PushTableValue(luaState, itemIdKey, itemId);
                    if (bagIndex == EQUIPPED_BAG_INDEX) {
                        lua_pushstring(luaState, bagIndexKey);
                        lua_pushnil(luaState);
                        lua_settable(luaState, -3);
                    } else {
                        PushTableValue(luaState, bagIndexKey, bagIndex);
                    }
                    PushTableValue(luaState, slotIndexKey, luaSlot);
                    const char *trinketName = itemStats->m_displayName[0] ? itemStats->m_displayName[0] : "Unknown";
                    PushTableValue(luaState, trinketNameKey, trinketName);
                    PushTableValue(luaState, itemLevelKey, itemStats->m_itemLevel);

                    // Get texture path using display ID
                    if (itemStats && itemStats->m_displayInfoID > 0) {
                        char *texturePath = getInventoryArt(itemStats->m_displayInfoID);
                        if (texturePath && texturePath[0] != '\0') {
                            PushTableValue(luaState, textureKey, texturePath);
                        } else {
                            PushTableValue(luaState, textureKey, reinterpret_cast<const char *>(Offsets::InvQuestionMark));
                        }
                    } else {
                        PushTableValue(luaState, textureKey, reinterpret_cast<const char *>(Offsets::InvQuestionMark));
                    }
                }

                lua_settable(luaState, -3);

                luaIndex++;
            }

            // Update cache
            if (!copyTable && cachedData != nullptr) {
                *cachedData = currentData;
            }
        };

        // Equipped trinket slots (0-based slots 12 and 13)
        for (uint32_t slot : {12u, 13u}) {
            auto item = getBagItem(inventory, slot);
            pushTrinket(EQUIPPED_BAG_INDEX, slot, item);
        }

        // Backpack (bagIndex 0, absolute slots 23-38)
        for (uint32_t slot = 23; slot <= 38; ++slot) {
            auto item = getBagItem(inventory, slot);
            pushTrinket(0, slot, item);
        }

        // Equipped bags 1-4
        for (int32_t bagIndex = 1; bagIndex <= 4; ++bagIndex) {
            uint64_t containerGuid = getContainerGuid(bagIndex - 1); // bagIndex 1-4 maps to container 0-3
            if (containerGuid == 0) continue;

            auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
            if (!containerPtr) continue;

            auto bagPtr = GetBagPtrFromContainer(containerPtr);
            if (!bagPtr) continue;

            auto bagSize = *bagPtr;
            for (uint32_t slot = 0; slot < bagSize; ++slot) {
                auto item = getBagItem(bagPtr, slot);
                pushTrinket(bagIndex, slot, item);
            }
        }

        // Clear any stale entries from the Lua table (when trinkets are removed)
        if (!copyTable) {
            uint32_t currentTrinketCount = luaIndex - 1;
            for (uint32_t i = currentTrinketCount; i < lastTrinketCount; ++i) {
                lua_pushnumber(luaState, static_cast<double>(i + 1));
                lua_pushnil(luaState);
                lua_settable(luaState, -3);
            }
            lastTrinketCount = currentTrinketCount;
        }

        return 1;
    }

    uint32_t Script_GetEquippedItems(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        uint64_t guid;

        if (lua_gettop(luaState) >= 1) {
            guid = GetUnitGuidFromLuaParam(luaState, 1);
            if (guid == 0) {
                lua_error(luaState, "Usage: GetEquippedItems() or GetEquippedItems(unitStr) or GetEquippedItems(guid)");
                return 0;
            }
        } else {
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

        // Get or create reusable table
        GetTableRef(luaState, equippedItemsTableRef);

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        bool isPlayer = (guid == playerGuid);

        if (isPlayer) {
            auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
            auto inventory = game::GetPlayerInventoryPtr(unit);

            for (uint32_t slot = 0; slot <= 18; slot++) {
                auto item = getBagItem(inventory, slot);
                if (item) {
                    PushBagCGItemToTable(luaState, -999, slot, item);
                }
            }
        } else {
            auto const getEquippedItem = reinterpret_cast<CGUnit_C_GetEquippedItemAtSlotT>(
                Offsets::CGUnit_C_GetEquippedItemAtSlot);

            for (uint32_t slot = 0; slot <= 18; slot++) {
                auto cgItem = getEquippedItem(unit, slot);
                if (cgItem) {
                    lua_pushnumber(luaState, static_cast<double>(slot+1)); // lua is 1 indexed
                    CreateBasicItemInfoTable(luaState, cgItem);
                    lua_settable(luaState, -3);
                }
            }
        }

        return 1;
    }

    uint32_t Script_GetEquippedItem(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (lua_gettop(luaState) < 2) {
            lua_error(luaState, "Usage: GetEquippedItem(unitStr, slot) or GetEquippedItem(guid, slot)");
            return 0;
        }

        uint64_t guid;
        if (lua_gettop(luaState) >= 1) {
            guid = GetUnitGuidFromLuaParam(luaState, 1);
            if (guid == 0) {
                lua_error(luaState, "Usage: GetEquippedItem(unitStr, slot) or GetEquippedItem(guid, slot)");
                return 0;
            }
        } else {
            guid = game::ClntObjMgrGetActivePlayerGuid();
        }

        if (!lua_isnumber(luaState, 2)) {
            lua_error(luaState, "Slot must be a number between 1 and 19");
            return 0;
        }

        auto luaSlot = static_cast<uint32_t>(lua_tonumber(luaState, 2));

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

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        bool isPlayer = (guid == playerGuid);

        if (isPlayer) {
            auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
            auto inventory = game::GetPlayerInventoryPtr(unit);

            auto item = getBagItem(inventory, luaSlot-1); // luaSlot is 1-indexed
            CreateItemInfoTable(luaState, item);
        } else {
            auto const getEquippedItem = reinterpret_cast<CGUnit_C_GetEquippedItemAtSlotT>(
                Offsets::CGUnit_C_GetEquippedItemAtSlot);

            auto cgItem = getEquippedItem(unit, luaSlot - 1); // luaSlot is 1-indexed
            CreateBasicItemInfoTable(luaState, cgItem);
        }

        return 1;
    }

    uint32_t ConvertLuaSlot(int32_t bagIndex, uint32_t luaSlot) {
        uint32_t relativeSlot = luaSlot - 1;

        if (bagIndex == 0) {
            return relativeSlot + 0x17; // add 23 -> absolute slots 23-38
        } else if (bagIndex == -1) {
            return relativeSlot + 0x27; // add 39 -> absolute slots 39-62
        } else if (bagIndex == -2) {
            return relativeSlot + 0x51; // add 81 -> absolute slots 81-96
        } else {
            return relativeSlot;
        }
    }



    uint32_t Script_GetBagItem(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (lua_gettop(luaState) < 2) {
            lua_error(luaState, "Usage: GetBagItem(bagIndex, slot)");
            return 0;
        }

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Bag index must be a number");
            return 0;
        }
        auto bagIndex = static_cast<int32_t>(lua_tonumber(luaState, 1));

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
            if (slot < 23 || slot > 38) {
                lua_error(luaState, "Slot must be between 1 and 16 for bag 1");
                return 0;
            }
            item = getBagItem(inventory, slot);
        } else if (bagIndex >= 1 && bagIndex <= 4) {
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

    uint32_t GetSingleBagItems(uintptr_t *luaState, int32_t bagIndex) {
        auto const getContainerGuid = reinterpret_cast<GetContainerGuidT>(Offsets::GetContainerGuid);
        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);


        if (bagIndex == 0) {
            const auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
            const auto player = game::GetObjectPtr(playerGuid);
            const auto inventory = game::GetPlayerInventoryPtr(player);
            GetTableRef(luaState, bagTableRefs[0]);

            for (uint32_t slot = 23; slot <= 38; slot++) {
                auto item = getBagItem(inventory, slot);
                uint32_t adjustedSlot = slot - 0x17;
                if (item) {
                    PushBagCGItemToTable(luaState, 0, slot, item);
                } else {
                    // Check if item was removed from this slot
                    ClearBagSlot(luaState, 0, adjustedSlot);
                }
            }
            return 1;
        }

        if (bagIndex >= 1 && bagIndex <= 4) {
            uint64_t containerGuid = getContainerGuid(bagIndex - 1); // bagIndex 1-4 maps to container 0-3
            if (containerGuid == 0) {
                // Bag slot is empty, clear all cached items for this bag
                GetTableRef(luaState, bagTableRefs[bagIndex]);
                for (uint32_t slot = 0; slot < MAX_BAG_SLOTS; slot++) {
                    ClearBagSlot(luaState, bagIndex, slot);
                }
                return 1;
            }

            auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
            if (!containerPtr) {
                return 0;
            }

            auto bagPtr = GetBagPtrFromContainer(containerPtr);
            if (!bagPtr) {
                return 0;
            }

            GetTableRef(luaState, bagTableRefs[bagIndex]);

            auto bagSize = *bagPtr;
            // Check all possible slots up to bag size
            for (uint32_t slot = 0;
                 slot < bagSize && slot < MAX_BAG_SLOTS; slot++) {
                auto item = getBagItem(bagPtr, slot);
                if (item) {
                    PushBagCGItemToTable(luaState, bagIndex, slot, item);
                } else {
                    ClearBagSlot(luaState, bagIndex, slot);
                }
            }
            return 1;
        }

        if (bagIndex >= 5 && bagIndex <= 9) {
            uint64_t bankGuid = *reinterpret_cast<uint64_t *>(Offsets::BankGuid);
            if (bankGuid > 0) {
                uint64_t containerGuid = getContainerGuid(bagIndex - 1); // bagIndex 5-9 maps to container 4-8
                if (containerGuid == 0) {
                    // Bag slot is empty, clear all cached items for this bag
                    GetTableRef(luaState, bagTableRefs[bagIndex]);
                    for (uint32_t slot = 0; slot < MAX_BAG_SLOTS; slot++) {
                        ClearBagSlot(luaState, bagIndex, slot);
                    }
                    lua_settable(luaState, -3);
                    return 1;
                }

                auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
                if (!containerPtr) {
                    return 0;
                }

                auto bagPtr = GetBagPtrFromContainer(containerPtr);
                if (!bagPtr) {
                    return 0;
                }

                GetTableRef(luaState, bagTableRefs[bagIndex]);

                auto bagSize = *bagPtr;
                // Check all possible slots up to bag size
                for (uint32_t slot = 0; slot < bagSize && slot < MAX_BAG_SLOTS; slot++) {
                    auto item = getBagItem(bagPtr, slot);
                    if (item) {
                        PushBagCGItemToTable(luaState, bagIndex, slot, item);
                    } else {
                        ClearBagSlot(luaState, bagIndex, slot);
                    }
                }

                return 1;
            }
        }


        // TODO Error for not finding the bag
        return 0;
    }

    uint32_t GetAllBagItems(uintptr_t* luaState) {
        // Get or create reusable main table
        GetTableRef(luaState, bagItemsTableRef);

        // Backpack (bag 0)
        lua_pushnumber(luaState, static_cast<double>(0));
        GetSingleBagItems(luaState, 0);

        lua_settable(luaState, -3);

        // Bags 1-4
        for (int32_t bagIndex = 1; bagIndex <= 4; bagIndex++) {

            const auto top = lua_gettop(luaState);
            lua_pushnumber(luaState, static_cast<double>(bagIndex));
            const auto bagResult = GetSingleBagItems(luaState, bagIndex);
            if (!bagResult) {
                lua_settop(luaState, top);
            }
            else {
                lua_settable(luaState, -3);
            }
        }

        // Bank bags 5-9
        uint64_t bankGuid = *reinterpret_cast<uint64_t*>(Offsets::BankGuid);
        if (bankGuid > 0) {
            for (int32_t bagIndex = 5; bagIndex <= 9; bagIndex++) {
                const auto top = lua_gettop(luaState);
                lua_pushnumber(luaState, static_cast<double>(bagIndex));
                const auto bagResult = GetSingleBagItems(luaState, bagIndex);
                if (!bagResult) {
                    lua_settop(luaState, top);
                }
                else {
                    lua_settable(luaState, -3);
                }
            }
        }

        return 1;
    }

    uint32_t Script_GetBagItems(uintptr_t* luaState) {
        luaState = GetLuaStatePtr();

        if (lua_gettop(luaState) == 1) {

            if (!lua_isnumber(luaState, 1)) {
                lua_error(luaState, "Bag index must be a number");
                return 0;
            }

            const auto bagIndex = static_cast<int32_t>(lua_tonumber(luaState, 1));
            return GetSingleBagItems(luaState, bagIndex);
        }

        return GetAllBagItems(luaState);
    }


}
