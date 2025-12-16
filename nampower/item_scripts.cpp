#include "item_scripts.hpp"

#include "helper.hpp"
#include "items.hpp"
#include "logging.hpp"
#include "offsets.hpp"

#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>

namespace Nampower {
    // Local cache for item name lookups
    static std::unordered_map<std::string, uint32_t> itemNameToIdCache;

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

    void PushItemFoundResult(uintptr_t *luaState, int32_t bagIndex, uint32_t slot) {
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

    uintptr_t *GetBagPtrFromContainer(uintptr_t *containerPtr) {
        auto vftable = game::GetObjectVFTable(containerPtr);
        using GetBagPtrT = uintptr_t * (__thiscall *)(uintptr_t *);
        auto getBagPtrFunc = reinterpret_cast<GetBagPtrT>(vftable[4]);
        return getBagPtrFunc(containerPtr);
    }

    void CreateBasicItemInfoTable(uintptr_t *luaState, game::CGItem *cgItem) {
        if (!cgItem) {
            lua_pushnil(luaState);
            return;
        }

        lua_newtable(luaState);

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

        lua_newtable(luaState);

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

        lua_pushnumber(luaState, static_cast<double>(adjustedSlot + 1)); // lua is 1 indexed
        CreateItemInfoTable(luaState, item);
        lua_settable(luaState, -3);
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

        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);
        auto const getContainerGuid = reinterpret_cast<GetContainerGuidT>(Offsets::GetContainerGuid);

        auto inventory = game::GetPlayerInventoryPtr(playerUnit);

        for (uint32_t slot = 0; slot <= 18; slot++) {
            auto item = getBagItem(inventory, slot);
            if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                lua_pushnil(luaState);
                lua_pushnumber(luaState, static_cast<double>(slot+1)); // lua is 1 indexed
                return 2;
            }
        }

        for (uint32_t slot = 23; slot <= 38; slot++) {
            auto item = getBagItem(inventory, slot);
            if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                PushItemFoundResult(luaState, 0, slot);
                return 2;
            }
        }

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
            for (uint32_t slot = 39; slot <= 62; slot++) {
                auto item = getBagItem(inventory, slot);
                if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                    PushItemFoundResult(luaState, -1, slot);
                    return 2;
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
                    if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                        PushItemFoundResult(luaState, bagIndex, slot);
                        return 2;
                    }
                }
            }
        }

        for (uint32_t slot = 81; slot <= 96; slot++) {
            auto item = getBagItem(inventory, slot);
            if (item && DoesItemMatch(game::GetItemId(item), searchItemId, searchItemName)) {
                PushItemFoundResult(luaState, -2, slot);
                return 2;
            }
        }

        lua_pushnil(luaState);
        lua_pushnil(luaState);
        return 2;
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

        lua_newtable(luaState);

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

    uint32_t Script_GetBagItems(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto const getContainerGuid = reinterpret_cast<GetContainerGuidT>(Offsets::GetContainerGuid);
        auto const getBagItem = reinterpret_cast<CGBag_C_GetItemAtSlotT>(Offsets::CGBag_C_GetItemAtSlot);

        lua_newtable(luaState);

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto player = game::GetObjectPtr(playerGuid);
        auto inventory = game::GetPlayerInventoryPtr(player);

        lua_pushnumber(luaState, static_cast<double>(0));
        lua_newtable(luaState);

        for (uint32_t slot = 23; slot <= 38; slot++) {
            auto item = getBagItem(inventory, slot);
            if (item) {
                PushBagCGItemToTable(luaState, 0, slot, item);
            }
        }

        lua_settable(luaState, -3);

        for (int32_t bagIndex = 1; bagIndex <= 4; bagIndex++) {
            uint64_t containerGuid = getContainerGuid(bagIndex - 1); // bagIndex 1-4 maps to container 0-3
            if (containerGuid == 0) continue;

            auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
            if (!containerPtr) continue;

            auto bagPtr = GetBagPtrFromContainer(containerPtr);
            if (!bagPtr) continue;

            lua_pushnumber(luaState, static_cast<double>(bagIndex));
            lua_newtable(luaState);

            auto bagSize = *bagPtr;
            for (uint32_t slot = 0; slot < bagSize; slot++) {
                auto item = getBagItem(bagPtr, slot);
                if (item) {
                    PushBagCGItemToTable(luaState, bagIndex, slot, item);
                }
            }

            lua_settable(luaState, -3);
        }

        uint64_t bankGuid = *reinterpret_cast<uint64_t *>(Offsets::BankGuid);
        if (bankGuid > 0) {
            for (int32_t bagIndex = 5; bagIndex <= 9; bagIndex++) {
                uint64_t containerGuid = getContainerGuid(bagIndex - 1); // bagIndex 5-9 maps to container 4-8
                if (containerGuid == 0) continue;

                auto containerPtr = game::ClntObjMgrObjectPtr(game::TYPEMASK_CONTAINER, containerGuid);
                if (!containerPtr) continue;

                auto bagPtr = GetBagPtrFromContainer(containerPtr);
                if (!bagPtr) continue;

                lua_pushnumber(luaState, static_cast<double>(bagIndex));
                lua_newtable(luaState);

                auto bagSize = *bagPtr;
                for (uint32_t slot = 0; slot < bagSize; slot++) {
                    auto item = getBagItem(bagPtr, slot);
                    if (item) {
                        PushBagCGItemToTable(luaState, bagIndex, slot, item);
                    }
                }

                lua_settable(luaState, -3);
            }
        }

        return 1;
    }

}
