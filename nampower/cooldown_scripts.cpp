#include "cooldown_scripts.hpp"

#include "helper.hpp"
#include "game.hpp"
#include "items.hpp"
#include "lua_refs.hpp"
#include "offsets.hpp"

#include <cstring>

#include "cooldown.hpp"

namespace Nampower {
    // Reusable table reference to reduce memory allocations
    static int cooldownDetailTableRef = LUA_REFNIL;

    game::SpellHistoryEntry *GetSpellHistoryHead() {
        auto const spellHistoryAddr = static_cast<uintptr_t>(Offsets::SpellHistories);

        // Try treating the offset as the object itself first.
        auto head = *reinterpret_cast<game::SpellHistoryEntry **>(spellHistoryAddr + 8);

        if (!head) {
            // Some clients store a pointer at the address instead of the object.
            auto const maybePtr = *reinterpret_cast<uintptr_t *>(spellHistoryAddr);
            if (maybePtr) {
                head = *reinterpret_cast<game::SpellHistoryEntry **>(maybePtr + 8);
            }
        }

        if (reinterpret_cast<uintptr_t>(head) & 0x1) {
            return nullptr;
        }

        return head;
    }

    CooldownDetail GetCooldownFromSpellHistory(uint32_t spellId, uint32_t itemId, uint32_t itemCategoryOverride) {
        CooldownDetail detail{};

        auto const spellRec = game::GetSpellInfo(spellId);
        if (!spellRec) {
            return detail;
        }

        detail.itemId = itemId;

        // Set base durations from spell record so we know what they are even if not on cooldown
        detail.individualDurationMs = spellRec->RecoveryTime;
        detail.categoryDurationMs = spellRec->CategoryRecoveryTime;
        detail.gcdCategoryDurationMs = spellRec->StartRecoveryTime;

        auto const category = itemCategoryOverride ? itemCategoryOverride : spellRec->Category;
        auto const startRecoveryCategory = spellRec->StartRecoveryCategory;

        detail.categoryId = category;
        detail.gcdCategoryId = startRecoveryCategory;

        auto const now = static_cast<uint32_t>(GetWowTimeMs() & 0xFFFFFFFF); // get bottom 32 bits of time in ms

        int entryCount = 0;
        for (auto entry = GetSpellHistoryHead();
             entry && ((reinterpret_cast<uintptr_t>(entry) & 0x1) == 0);
             entry = reinterpret_cast<game::SpellHistoryEntry *>(entry->field4_0x4)) {
            // Individual spell cooldown
            if (entry->spellID == spellId && entry->itemID == itemId &&
                (entry->recoveryTime != 0 || entry->onHold)) {
                auto end = entry->onHold
                               ? uint64_t(now) + entry->recoveryTime
                               : uint64_t(entry->recoveryStart) + entry->recoveryTime;

                if (end > now) {
                    detail.individualStartMs = entry->onHold ? now : entry->recoveryStart;
                    detail.individualDurationMs = entry->recoveryTime;
                    detail.individualRemainingMs = static_cast<uint32_t>(end - now);
                    detail.isOnIndividualCooldown = true;
                }
            }

            // Category cooldown
            if (entry->categoryRecoveryTime != 0 && entry->category == category && category != 0) {
                auto start = entry->onHold ? now : entry->categoryRecoveryStart;
                auto end = uint64_t(start) + entry->categoryRecoveryTime;

                if (end > now) {
                    detail.categoryId = entry->category;
                    detail.categoryStartMs = start;
                    detail.categoryDurationMs = entry->categoryRecoveryTime;
                    detail.categoryRemainingMs = static_cast<uint32_t>(end - now);
                    detail.isOnCategoryCooldown = true;
                }
            }

            // GCD category cooldown (startRecoveryCategory)
            if (entry->startRecoveryCategory == startRecoveryCategory && entry->startRecoveryTime != 0 &&
                startRecoveryCategory != 0) {
                auto start = entry->onHold ? now : entry->recoveryStart;
                auto end = entry->onHold
                               ? uint64_t(now) + entry->startRecoveryTime
                               : uint64_t(entry->recoveryStart) + entry->startRecoveryTime;

                if (end > now) {
                    detail.gcdCategoryId = entry->startRecoveryCategory;
                    detail.gcdCategoryStartMs = start;
                    detail.gcdCategoryDurationMs = entry->startRecoveryTime;
                    detail.gcdCategoryRemainingMs = static_cast<uint32_t>(end - now);
                    detail.isOnGcdCategoryCooldown = true;
                }
            }
        }

        // Calculate overall cooldown status
        detail.isOnCooldown = detail.isOnIndividualCooldown || detail.isOnCategoryCooldown || detail.
                              isOnGcdCategoryCooldown;

        // Find the maximum remaining cooldown
        detail.cooldownRemainingMs = detail.individualRemainingMs;
        if (detail.categoryRemainingMs > detail.cooldownRemainingMs) {
            detail.cooldownRemainingMs = detail.categoryRemainingMs;
        }
        if (detail.gcdCategoryRemainingMs > detail.cooldownRemainingMs) {
            detail.cooldownRemainingMs = detail.gcdCategoryRemainingMs;
        }

        if (itemId > 0) {
            // get info on active spell for item
            auto *itemStats = GetItemStats(itemId);
            if (itemStats) {
                for (int i = 0; i < 5; ++i) {
                    if (itemStats->m_spellCooldown[i] > 0 || itemStats->m_spellCategoryCooldown[i] > 0) {
                        detail.itemHasActiveSpell = true;
                        detail.itemActiveSpellId = itemStats->m_spellID[i];
                        detail.individualDurationMs = itemStats->m_spellCooldown[i];
                        detail.categoryDurationMs = itemStats->m_spellCategoryCooldown[i];
                        return detail;
                    }
                }
                // didn't have active spell, clear any spell gcd info for passive effects
                detail.gcdCategoryDurationMs = 0;
                detail.gcdCategoryId = 0;
            }
        }

        return detail;
    }

    void PushCooldownDetailTable(uintptr_t *luaState, const CooldownDetail &detail) {
        static char isOnCooldownKey[] = "isOnCooldown";
        static char cooldownRemainingMsKey[] = "cooldownRemainingMs";
        static char itemIdKey[] = "itemId";
        static char itemHasActiveSpellKey[] = "itemHasActiveSpell";
        static char itemActiveSpellIdKey[] = "itemActiveSpellId";

        static char individualStartSKey[] = "individualStartS";
        static char individualDurationMsKey[] = "individualDurationMs";
        static char individualRemainingMsKey[] = "individualRemainingMs";
        static char isOnIndividualCooldownKey[] = "isOnIndividualCooldown";

        static char categoryIdKey[] = "categoryId";
        static char categoryStartSKey[] = "categoryStartS";
        static char categoryDurationMsKey[] = "categoryDurationMs";
        static char categoryRemainingMsKey[] = "categoryRemainingMs";
        static char isOnCategoryCooldownKey[] = "isOnCategoryCooldown";

        static char gcdCategoryIdKey[] = "gcdCategoryId";
        static char gcdCategoryStartSKey[] = "gcdCategoryStartS";
        static char gcdCategoryDurationMsKey[] = "gcdCategoryDurationMs";
        static char gcdCategoryRemainingMsKey[] = "gcdCategoryRemainingMs";
        static char isOnGcdCategoryCooldownKey[] = "isOnGcdCategoryCooldown";

        // Get or create reusable table
        GetTableRef(luaState, cooldownDetailTableRef);

        // Overall cooldown status
        PushTableValue(luaState, isOnCooldownKey, detail.isOnCooldown ? 1 : 0);
        PushTableValue(luaState, cooldownRemainingMsKey, detail.cooldownRemainingMs);
        PushTableValue(luaState, itemIdKey, detail.itemId);
        PushTableValue(luaState, itemHasActiveSpellKey, detail.itemHasActiveSpell ? 1 : 0);
        PushTableValue(luaState, itemActiveSpellIdKey, detail.itemActiveSpellId);

        // Individual cooldown
        PushTableValue(luaState, individualStartSKey, detail.individualStartMs / 1000.0);
        PushTableValue(luaState, individualDurationMsKey, detail.individualDurationMs);
        PushTableValue(luaState, individualRemainingMsKey, detail.individualRemainingMs);
        PushTableValue(luaState, isOnIndividualCooldownKey, detail.isOnIndividualCooldown ? 1 : 0);

        // Category cooldown
        PushTableValue(luaState, categoryIdKey, detail.categoryId);
        PushTableValue(luaState, categoryStartSKey, detail.categoryStartMs / 1000.0);
        PushTableValue(luaState, categoryDurationMsKey, detail.categoryDurationMs);
        PushTableValue(luaState, categoryRemainingMsKey, detail.categoryRemainingMs);
        PushTableValue(luaState, isOnCategoryCooldownKey, detail.isOnCategoryCooldown ? 1 : 0);

        // GCD category cooldown
        PushTableValue(luaState, gcdCategoryIdKey, detail.gcdCategoryId);
        PushTableValue(luaState, gcdCategoryStartSKey, detail.gcdCategoryStartMs / 1000.0);
        PushTableValue(luaState, gcdCategoryDurationMsKey, detail.gcdCategoryDurationMs);
        PushTableValue(luaState, gcdCategoryRemainingMsKey, detail.gcdCategoryRemainingMs);
        PushTableValue(luaState, isOnGcdCategoryCooldownKey, detail.isOnGcdCategoryCooldown ? 1 : 0);
    }

    CooldownDetail GetItemCooldownDetail(uint32_t itemId) {
        auto *itemStats = GetItemStats(itemId);

        CooldownDetail activeCooldownDetail{};
        uint32_t longestCooldownMs = 0;

        if (itemStats) {
            for (int i = 0; i < 5; ++i) {
                auto const spellId = static_cast<uint32_t>(itemStats->m_spellID[i]);
                if (spellId == 0) {
                    continue;
                }

                auto const categoryOverride = static_cast<uint32_t>(itemStats->m_spellCategory[i]);
                auto const detail = GetCooldownFromSpellHistory(spellId, itemId, categoryOverride);

                if (detail.cooldownRemainingMs >= longestCooldownMs) {
                    activeCooldownDetail = detail;
                    longestCooldownMs = detail.cooldownRemainingMs;
                }
            }
        } else {
            activeCooldownDetail = GetCooldownFromSpellHistory(0, itemId, 0);
        }

        return activeCooldownDetail;
    }

    uint32_t Script_GetSpellIdCooldown(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: GetSpellCooldown(spellId)");
            return 0;
        }

        uint32_t spellId = static_cast<uint32_t>(lua_tonumber(luaState, 1));

        auto const cooldown = GetCooldownFromSpellHistory(spellId, 0, 0);

        PushCooldownDetailTable(luaState, cooldown);
        return 1;
    }

    uint32_t Script_GetItemIdCooldown(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        auto param1IsString = lua_isstring(luaState, 1);
        auto param1IsNumber = lua_isnumber(luaState, 1);

        if (!param1IsString && !param1IsNumber) {
            lua_error(luaState, "Usage: GetItemCooldown(itemId) or GetItemCooldown(itemName)");
            return 0;
        }

        uint32_t itemId = 0;

        if (param1IsNumber) {
            itemId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        } else {
            lua_error(luaState, "Item name lookup not yet implemented");
            return 0;
        }

        // Item cooldowns rely on the associated spell entries being in the history with the item id set.
        auto const cooldown = GetItemCooldownDetail(itemId);

        PushCooldownDetailTable(luaState, cooldown);
        return 1;
    }

    static bool TrinketMatches(uint32_t itemId, uint32_t searchItemId, const char *searchItemName) {
        if (searchItemId != 0) {
            return itemId == searchItemId;
        }

        if (searchItemName) {
            auto itemStats = GetItemStats(itemId);
            if (itemStats && itemStats->m_displayName[0]) {
                return _stricmp(itemStats->m_displayName[0], searchItemName) == 0;
            }
        }

        return false;
    }

    uint32_t Script_GetTrinketCooldown(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isnumber(luaState, 1) && !lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: GetTrinketCooldown(slot|itemIdOrName)");
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
            for (uint32_t invSlot: {12u, 13u}) {
                auto candidate = getBagItem(inventory, invSlot);
                if (candidate && TrinketMatches(game::GetItemId(candidate), searchItemId, searchItemName)) {
                    item = candidate;
                    break;
                }
            }
        }

        // if looking for trinket by name and not equipped, search bags to try to find item id
        if (!item && searchItemName != nullptr) {
            // look for item name in bags
            auto result = FindPlayerItem(0, searchItemName);
            if (result.found()) {
                searchItemId = game::GetItemId(result.item);
            }
        }

        uint32_t itemId = (item) ? game::GetItemId(item) : searchItemId;
        auto const cooldown = GetItemCooldownDetail(itemId);

        PushCooldownDetailTable(luaState, cooldown);
        return 1;
    }

}
