//
// Created by pmacc on 1/8/2025.
//

#include "misc_scripts.hpp"
#include "offsets.hpp"
#include "items.hpp"
#include "dbc_fields.hpp"
#include "unit_fields.hpp"
#include "helper.hpp"
#include "lua_refs.hpp"
#include "auras.hpp"
#include <cstring>

namespace Nampower {
    // Lua table field name constants
    namespace LuaFields {
        static char castId[] = "castId";
        static char spellId[] = "spellId";
        static char guid[] = "guid";
        static char castType[] = "castType";
        static char castStartS[] = "castStartS";
        static char castEndS[] = "castEndS";
        static char castRemainingMs[] = "castRemainingMs";
        static char castDurationMs[] = "castDurationMs";
        static char gcdEndS[] = "gcdEndS";
        static char gcdRemainingMs[] = "gcdRemainingMs";
    }

    bool gScriptQueued;
    int gScriptPriority = 1;
    char *queuedScript;

    // Reusable table references to reduce memory allocations
    static int castInfoTableRef = LUA_REFNIL;
    static int unitDataTableRef = LUA_REFNIL;

    // Maps to store separate references for each array field name (for Field functions)
    static std::unordered_map<std::string, int> unitFieldsArrayFieldRefs;

    // Maps to store separate references for nested array fields (for main table functions)
    static std::unordered_map<std::string, int> unitFieldsNestedArrayRefs;

    uint32_t Script_GetCurrentCastingInfo(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        auto const castingSpellId = reinterpret_cast<uint32_t *>(Offsets::CastingSpellId);
        lua_pushnumber(luaState, *castingSpellId);

        auto const isCasting = gCastData.castEndMs > GetTime();
        auto const isChanneling = gCastData.channeling;

        auto const visualSpellId = reinterpret_cast<uint32_t *>(Offsets::VisualSpellId);
        lua_pushnumber(luaState, *visualSpellId);

        auto const autoRepeatingSpellId = reinterpret_cast<uint32_t *>(Offsets::AutoRepeatingSpellId);
        lua_pushnumber(luaState, *autoRepeatingSpellId);

        auto playerUnit = game::GetObjectPtr(game::ClntObjMgrGetActivePlayerGuid());
        if (isCasting) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnumber(luaState, 0);
        }

        if (isChanneling) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnumber(luaState, 0);
        }

        if (gCastData.pendingOnSwingCast) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnumber(luaState, 0);
        }

        auto const attackPtr = playerUnit + 0x312; // auto attacking
        if (attackPtr && *reinterpret_cast<uint32_t *>(attackPtr) > 0) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnumber(luaState, 0);
        }

        return 7;
    }

    uint32_t Script_GetCastInfo(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        // Check if there's an active cast or channel
        uint32_t activeSpellId = 0;
        uint32_t castEndTime = 0;

        CastSpellParams* castParams = nullptr;

        // Check for active channeling spell first
        if (gCastData.channeling && gCastData.channelSpellId != 0) {
            activeSpellId = gCastData.channelSpellId;
            castEndTime = gCastData.channelEndMs;
            // cast will be finished for channels
            castParams = gCastHistory.findNewestSuccessfulSpellId(activeSpellId);
        }
        // Check for active cast spell
        else if (gCastData.castSpellId != 0) {
            activeSpellId = gCastData.castSpellId;
            // Use the max of castEndMs and gcdEndMs
            castEndTime = (gCastData.castEndMs > gCastData.gcdEndMs) ? gCastData.castEndMs : gCastData.gcdEndMs;
            // cast won't be finished yet
            castParams = gCastHistory.findNewestWaitingForServerSpellId(activeSpellId);
        }

        // If no active cast or channel, return nil
        if (activeSpellId == 0) {
            lua_pushnil(luaState);
            return 1;
        }

        if (castParams == nullptr || castParams->castId == 0) {
            lua_pushnil(luaState);
            return 1;
        }

        // Get or create reusable table
        GetTableRef(luaState, castInfoTableRef);

        // Get current time and calculate offset to convert to WoW time
        uint32_t currentTime = GetTime();
        uint64_t currentWowTime = GetWowTimeMs();
        int64_t timeOffset = static_cast<int64_t>(currentWowTime) - static_cast<int64_t>(currentTime);

        // Convert all timestamps to WoW time and convert to seconds
        double castStartTimeWow = (castParams->castStartTimeMs + timeOffset) / 1000.0;
        double castEndTimeWow = (castEndTime + timeOffset) / 1000.0;
        double gcdEndTimeWow = (gCastData.gcdEndMs + timeOffset) / 1000.0;

        // Add fields to table
        PushTableValue(luaState, LuaFields::castId, castParams->castId);
        PushTableValue(luaState, LuaFields::spellId, castParams->spellId);
        PushTableValue(luaState, LuaFields::guid, ConvertGuidToString(castParams->guid));
        PushTableValue(luaState, LuaFields::castType, static_cast<uint32_t>(castParams->castType));
        PushTableValue(luaState, LuaFields::castStartS, castStartTimeWow);
        PushTableValue(luaState, LuaFields::castEndS, castEndTimeWow);

        uint32_t timeRemaining = (castEndTime > currentTime) ? (castEndTime - currentTime) : 0;
        PushTableValue(luaState, LuaFields::castRemainingMs, timeRemaining);

        uint32_t duration = (castEndTime > castParams->castStartTimeMs) ?
                            (castEndTime - castParams->castStartTimeMs) : 0;
        PushTableValue(luaState, LuaFields::castDurationMs, duration);

        // Add GCD info
        PushTableValue(luaState, LuaFields::gcdEndS, gcdEndTimeWow);
        uint32_t gcdRemaining = (gCastData.gcdEndMs > currentTime) ? (gCastData.gcdEndMs - currentTime) : 0;
        PushTableValue(luaState, LuaFields::gcdRemainingMs, gcdRemaining);

        return 1; // Return the table
    }

    uint32_t Script_ChannelStopCastingNextTick(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (gCastData.channeling) {
            DEBUG_LOG("ChannelStopCastingNextTick activated, canceling next tick");
            gCastData.cancelChannelNextTick = true;
        }

        return 0;
    }

    uint32_t Script_GetNampowerVersion(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        lua_pushnumber(luaState, MAJOR_VERSION);
        lua_pushnumber(luaState, MINOR_VERSION);
        lua_pushnumber(luaState, PATCH_VERSION);

        return 3;
    }

    uint32_t Script_QueueScript(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        DEBUG_LOG("Trying to queue script");

        auto const currentTime = GetTime();
        auto effectiveCastEndMs = EffectiveCastEndMs();
        auto remainingEffectiveCastTime = (effectiveCastEndMs > currentTime) ? effectiveCastEndMs - currentTime : 0;
        auto remainingGcd = (gCastData.gcdEndMs > currentTime) ? gCastData.gcdEndMs - currentTime : 0;
        auto inSpellQueueWindow = InSpellQueueWindow(remainingEffectiveCastTime, remainingGcd, false);

        if (inSpellQueueWindow) {
            // check if valid string
            if (lua_isstring(luaState, 1)) {
                auto script = lua_tostring(luaState, 1);

                if (script != nullptr && strlen(script) > 0) {
                    // save the script to be run later
                    queuedScript = script;
                    gScriptQueued = true;

                    // check if priority is set
                    if (lua_isnumber(luaState, 2)) {
                        gScriptPriority = (int) lua_tonumber(luaState, 2);

                        DEBUG_LOG("Queuing script priority " << gScriptPriority << ": " << script);
                    } else {
                        DEBUG_LOG("Queuing script: " << script);
                    }
                }
            } else {
                DEBUG_LOG("Invalid script");
                lua_error(luaState, "Usage: QueueScript(\"script\", (optional)priority)");
            }
        } else {
            // just call regular runscript
            auto const runScript = reinterpret_cast<LuaScriptT >(Offsets::Script_RunScript);
            return runScript(luaState);
        }

        return 0;
    }

    uint32_t Script_LearnTalentRank(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1) || !lua_isnumber(luaState, 2) || !lua_isnumber(luaState, 3)) {
            lua_error(luaState, "Usage: LearnTalentRank(talentPage, talentIndex, rank)");
            return 0;
        }

        constexpr uintptr_t CGTalentInfo_m_talentTabs = 0x00BDCD24;
        constexpr uintptr_t CGTalentInfo_m_talents = 0x00BDCD28;
        constexpr uint32_t CMSG_LEARN_TALENT = 0x0251;

        auto talentPage = static_cast<int32_t>(lua_tonumber(luaState, 1));
        auto talentIndex = static_cast<int32_t>(lua_tonumber(luaState, 2));
        auto requestedRank = static_cast<int32_t>(lua_tonumber(luaState, 3));

        if (talentPage < 1 || talentPage > 3) {
            lua_error(luaState, "LearnTalentRank: talentPage must be in range 1-3");
            return 0;
        }
        if (talentIndex < 1 || talentIndex > 32) {
            lua_error(luaState, "LearnTalentRank: talentIndex must be in range 1-32");
            return 0;
        }
        if (requestedRank < 1 || requestedRank > 5) {
            lua_error(luaState, "LearnTalentRank: rank must be in range 1-5");
            return 0;
        }

        auto const playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto *playerUnit = game::ClntObjMgrObjectPtr(game::TYPEMASK_PLAYER, playerGuid);
        if (!playerUnit) {
            lua_error(luaState, "LearnTalentRank: active player not available");
            return 0;
        }

        auto talentPage0Index = talentPage - 1;
        auto talentIndex0 = talentIndex - 1;
        int32_t talentTabCount = 0;
        if (!SafeRead(reinterpret_cast<void *>(CGTalentInfo_m_talentTabs), talentTabCount)) {
            lua_error(luaState, "LearnTalentRank: unable to read talent tab count");
            return 0;
        }

        if (talentPage0Index < 0 || talentPage0Index >= talentTabCount || talentIndex0 < 0) {
            lua_error(luaState, "LearnTalentRank: talent page/index out of bounds");
            return 0;
        }

        uintptr_t talentsBase = 0;
        if (!SafeRead(reinterpret_cast<void *>(CGTalentInfo_m_talents), talentsBase)) {
            lua_error(luaState, "LearnTalentRank: unable to read talents base pointer");
            return 0;
        }

        if (talentsBase == 0) {
            lua_error(luaState, "LearnTalentRank: talents base pointer is null");
            return 0;
        }

        auto talentTabSlotAddr = talentsBase + static_cast<uintptr_t>(talentPage0Index) * sizeof(uint32_t);
        uintptr_t talentTabPtr = 0;
        if (!SafeRead(reinterpret_cast<void *>(talentTabSlotAddr), talentTabPtr)) {
            lua_error(luaState, "LearnTalentRank: unable to read talent tab pointer");
            return 0;
        }

        if (talentTabPtr == 0) {
            lua_error(luaState, "LearnTalentRank: talent tab pointer is null");
            return 0;
        }

        uint32_t talentCountInTab = 0;
        uintptr_t talentListBase = 0;
        if (!SafeRead(reinterpret_cast<void *>(talentTabPtr + 0x8), talentCountInTab) ||
            !SafeRead(reinterpret_cast<void *>(talentTabPtr + 0xC), talentListBase)) {
            lua_error(luaState, "LearnTalentRank: unable to read talent tab metadata");
            return 0;
        }

        if (talentCountInTab == 0 || talentCountInTab >= 512 || talentListBase == 0) {
            lua_error(luaState, "LearnTalentRank: invalid talent tab metadata");
            return 0;
        }

        if (talentIndex0 >= static_cast<int32_t>(talentCountInTab)) {
            lua_error(luaState, "LearnTalentRank: talent index not present in selected tab");
            return 0;
        }

        auto talentEntryPtr = talentListBase + static_cast<uintptr_t>(talentIndex0) * 0x54;
        int32_t talentId = 0;
        if (!SafeRead(reinterpret_cast<void *>(talentEntryPtr), talentId)) {
            lua_error(luaState, "LearnTalentRank: unable to read talent entry");
            return 0;
        }
        if (talentId <= 0 || talentId >= 200000) {
            lua_error(luaState, "LearnTalentRank: resolved invalid talentId");
            return 0;
        }

        auto requestedRank0Index = requestedRank - 1;
        auto dataStore = CDataStore();
        dataStore.Put<uint32_t>(CMSG_LEARN_TALENT);
        dataStore.Put<uint32_t>(static_cast<uint32_t>(talentId));
        dataStore.Put<uint32_t>(static_cast<uint32_t>(requestedRank0Index));
        dataStore.Finalize();

        auto const clientServicesSend = reinterpret_cast<ClientServices_SendT>(Offsets::ClientServices_Send);
        clientServicesSend(&dataStore);

        lua_pushnumber(luaState, 1);
        return 1;
    }

    bool RunQueuedScript(int priority) {
        if (gScriptQueued && gScriptPriority == priority) {
            auto currentTime = GetTime();

            auto effectiveCastEndMs = EffectiveCastEndMs();
            // get max of cooldown and gcd
            auto delay = effectiveCastEndMs > gCastData.gcdEndMs ? effectiveCastEndMs : gCastData.gcdEndMs;

            if (delay <= currentTime) {
                DEBUG_LOG("Running queued script priority " << gScriptPriority << ": " << queuedScript);
                LuaCall(queuedScript);
                gScriptQueued = false;
                gScriptPriority = 1;
                return true;
            }
        }

        return false;
    }

    uint32_t Script_GetUnitData(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: GetUnitData(unitToken, [copy]) - unitToken can be 'player', 'target', 'pet', etc., or a GUID string");
            return 0;
        }

        const char *unitToken = lua_tostring(luaState, 1);

        // Check for optional copy parameter
        bool useCopy = false;
        if (lua_isnumber(luaState, 2)) {
            useCopy = static_cast<int>(lua_tonumber(luaState, 2)) != 0;
        }

        uint64_t guid = GetUnitGuidFromString(unitToken);

        if (guid == 0) {
            lua_pushnil(luaState);
            return 1;
        }

        // Get unit object pointer
        auto unit = game::GetObjectPtr(guid);
        if (!unit) {
            lua_pushnil(luaState);
            return 1;
        }

        // Get unit fields (offset 68 from unit pointer)
        auto *unitFields = *reinterpret_cast<game::UnitFields **>(unit + 68);
        if (!unitFields) {
            lua_pushnil(luaState);
            return 1;
        }

        // Create new table or get reusable table based on copy parameter
        if (useCopy) {
            lua_newtable(luaState);
        } else {
            GetTableRef(luaState, unitDataTableRef);
        }

        // Push all simple fields using descriptors.
        // UnitFields UINT64 members are GUID/object references and must be Lua strings.
        for (size_t i = 0; i < unitFieldsFieldsCount; ++i) {
            const auto &field = unitFieldsFields[i];
            lua_pushstring(luaState, const_cast<char *>(field.name));

            const char *fieldPtr = reinterpret_cast<const char *>(unitFields) + field.offset;
            switch (field.type) {
                case FieldType::UINT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint32_t *>(fieldPtr));
                    break;
                case FieldType::UINT8:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint8_t *>(fieldPtr));
                    break;
                case FieldType::FLOAT:
                    lua_pushnumber(luaState, *reinterpret_cast<const float *>(fieldPtr));
                    break;
                case FieldType::UINT64:
                    PushGuidString(luaState, *reinterpret_cast<const uint64_t *>(fieldPtr));
                    break;
                default:
                    lua_pushnil(luaState);
                    break;
            }

            lua_settable(luaState, -3);
        }

        // Push all array fields using descriptors with or without references based on copy parameter
        if (useCopy) {
            PushArrayFieldsToLua(luaState, unitFields, unitFieldsArrayFields, unitFieldsArrayFieldsCount);
        } else {
            PushArrayFieldsToLuaWithRefs(luaState, unitFields, unitFieldsArrayFields, unitFieldsArrayFieldsCount, unitFieldsNestedArrayRefs);
        }

        return 1; // Return the table
    }

    uint32_t Script_GetUnitField(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isstring(luaState, 1) || !lua_isstring(luaState, 2)) {
            lua_error(luaState, "Usage: GetUnitField(unitToken, fieldName, [copy]) - unitToken can be 'player', 'target', 'pet', etc., or a GUID string");
            return 0;
        }

        InitializeUnitFieldMaps();

        const char *unitToken = lua_tostring(luaState, 1);
        const char *fieldName = lua_tostring(luaState, 2);

        // Check for optional copy parameter
        bool useCopy = false;
        if (lua_isnumber(luaState, 3)) {
            useCopy = static_cast<int>(lua_tonumber(luaState, 3)) != 0;
        }

        uint64_t guid = GetUnitGuidFromString(unitToken);

        if (guid == 0) {
            lua_pushnil(luaState);
            return 1;
        }

        // Get unit object pointer
        auto unit = game::GetObjectPtr(guid);
        if (!unit) {
            lua_pushnil(luaState);
            return 1;
        }

        // Get unit fields (offset 68 from unit pointer)
        auto *unitFields = *reinterpret_cast<game::UnitFields **>(unit + 68);
        if (!unitFields) {
            lua_pushnil(luaState);
            return 1;
        }

        // O(1) hash table lookup for simple fields
        auto simpleIt = unitFieldsFieldMap.find(fieldName);
        if (simpleIt != unitFieldsFieldMap.end()) {
            size_t i = simpleIt->second;
            const char *fieldPtr = reinterpret_cast<const char *>(unitFields) + unitFieldsFields[i].offset;

            switch (unitFieldsFields[i].type) {
                case FieldType::UINT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint32_t *>(fieldPtr));
                    return 1;
                case FieldType::UINT8:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint8_t *>(fieldPtr));
                    return 1;
                case FieldType::UINT64:
                    PushGuidString(luaState, *reinterpret_cast<const uint64_t *>(fieldPtr));
                    return 1;
                case FieldType::FLOAT:
                    lua_pushnumber(luaState, *reinterpret_cast<const float *>(fieldPtr));
                    return 1;
                default:
                    break;
            }
        }

        // O(1) hash table lookup for array fields
        auto arrayIt = unitFieldsArrayFieldMap.find(fieldName);
        if (arrayIt != unitFieldsArrayFieldMap.end()) {
            size_t i = arrayIt->second;
            const auto &field = unitFieldsArrayFields[i];

            // Create new table or get reusable table based on copy parameter
            if (useCopy) {
                lua_newtable(luaState);
            } else {
                // Get or create reusable table for this specific field name
                GetTableRef(luaState, unitFieldsArrayFieldRefs[fieldName]);
            }

            const char *fieldPtr = reinterpret_cast<const char *>(unitFields) + field.offset;

            for (size_t j = 0; j < field.count; ++j) {
                lua_pushnumber(luaState, j + 1);

                switch (field.type) {
                    case FieldType::UINT32:
                        lua_pushnumber(luaState, reinterpret_cast<const uint32_t *>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT8:
                        lua_pushnumber(luaState, reinterpret_cast<const uint8_t *>(fieldPtr)[j]);
                        break;
                    case FieldType::FLOAT:
                        lua_pushnumber(luaState, reinterpret_cast<const float *>(fieldPtr)[j]);
                        break;
                    default:
                        lua_pushnumber(luaState, 0);
                        break;
                }
                lua_settable(luaState, -3);
            }
            return 1;
        }

        // Field not found
        lua_error(luaState, "Unknown field name");
        return 0;
    }

    bool TryDisenchant() {
        DEBUG_LOG("Trying disenchant with quality filter " << gDisenchantQuality << " itemId " << gDisenchantItemId);
        if (gDisenchantQuality == 0 && gDisenchantItemId == 0) {
            ResetDisenchantState();
            return false;
        }

        // Find the item in player inventory
        PlayerItemSearchResult itemSearchResult;
        if (gDisenchantItemId != 0) {
            // Item-specific disenchanting
            itemSearchResult = FindPlayerItem(gDisenchantItemId, nullptr);
        } else {
            // Quality-based disenchanting (weapons and armor only)
            itemSearchResult = FindPlayerDisenchantItem(gDisenchantQuality, gDisenchantIncludeSoulbound);
        }

        if (!itemSearchResult.found()) {
            // Item not found, stop disenchanting
            DEBUG_LOG("No disenchantable item found, stopping disenchant");
            LuaCall("DEFAULT_CHAT_FRAME:AddMessage(\"No more items to disenchant.\")");
            ResetDisenchantState();
            return false;
        }

        // Get the item GUID
        auto item = itemSearchResult.item;
        if (!item || !item->object.m_obj) {
            // Invalid item, stop disenchanting
            DEBUG_LOG("Invalid item, stopping disenchant");
            ResetDisenchantState();
            return false;
        }
        uint64_t itemGuid = item->object.m_obj->m_guid;

        // Get player unit for casting
        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto playerUnit = game::GetObjectPtr(playerGuid);

        if (!playerUnit) {
            // No player unit, stop disenchanting
            DEBUG_LOG("No player unit, stopping disenchant");
            ResetDisenchantState();
            return false;
        }

        // Display which item is being disenchanted
        char luaCode[256];
        snprintf(luaCode, sizeof(luaCode),
                 "local itemLink = GetContainerItemLink(%d, %d); "
                 "if itemLink then DEFAULT_CHAT_FRAME:AddMessage(\"Disenchanting \" .. itemLink .. \" move during cast to cancel.\") end",
                 itemSearchResult.bagIndex, itemSearchResult.slot + 1);  // Lua slots are 1-indexed
        LuaCall(luaCode);

        // Set timer for next disenchant attempt (5 seconds to allow time for inventory to update)
        auto currentTime = GetTime();
        gNextDisenchantTimeMs = currentTime + 5000;

        // Cast Disenchant spell (13262) on the item
        auto const castSpell = reinterpret_cast<Spell_C_CastSpellT>(Offsets::Spell_C_CastSpell);
        bool success = castSpell(playerUnit, 13262, nullptr, itemGuid);

        if (!success) {
            // Cast failed, stop disenchanting
            DEBUG_LOG("Cast failed, stopping disenchant");
            LuaCall("DEFAULT_CHAT_FRAME:AddMessage(\"Disenchant interrupted or failed.\")");
            ResetDisenchantState();
            return false;
        }

        return true;
    }

    uint32_t Script_DisenchantAll(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1) && !lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: DisenchantAll(itemIdOrName, [includeSoulbound]) or DisenchantAll(quality, [includeSoulbound])\n"
                "quality: \"greens\", \"blues\", \"purples\", or combinations like \"greens|blues\" or \"greens|blues|purples\"\n"
                "itemIdOrName: item ID (number) or item name (string)\n"
                "includeSoulbound: optional number - pass 1 to include soulbound items (defaults to 0)");
            return 0;
        }

        // Check if player has Disenchant spell (13262)
        constexpr uint32_t DISENCHANT_SPELL_ID = 13262;
        bool hasDisenchant = false;
        for (uint32_t slot = 0; slot < 1024; slot++) {
            uint32_t spellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownSpells) + slot * 4);
            if (spellId == DISENCHANT_SPELL_ID) {
                hasDisenchant = true;
                break;
            }
        }

        if (!hasDisenchant) {
            lua_error(luaState, "You do not have the Disenchant spell");
            return 0;
        }

        // Reset all modes
        ResetDisenchantState();

        // Check number first (lua_isstring will also match numbers)
        if (lua_isnumber(luaState, 1)) {
            // It's an item ID
            gDisenchantItemId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        } else if (lua_isstring(luaState, 1)) {
            // It's a string - check if it's quality-based or item name
            const char *param = lua_tostring(luaState, 1);

            // Parse quality combinations (e.g., "greens", "blues|purples", "greens|blues|purples")
            bool isQualityString = false;
            uint32_t qualityBitmask = 0;

            // Make a mutable copy of the string for tokenizing
            std::string paramStr(param);
            size_t start = 0;
            size_t end = 0;

            while (end != std::string::npos) {
                end = paramStr.find('|', start);
                std::string token = paramStr.substr(start, (end == std::string::npos) ? std::string::npos : end - start);

                if (_stricmp(token.c_str(), "greens") == 0) {
                    qualityBitmask |= DISENCHANT_QUALITY_GREEN;
                    isQualityString = true;
                } else if (_stricmp(token.c_str(), "blues") == 0) {
                    qualityBitmask |= DISENCHANT_QUALITY_BLUE;
                    isQualityString = true;
                } else if (_stricmp(token.c_str(), "purples") == 0) {
                    qualityBitmask |= DISENCHANT_QUALITY_PURPLE;
                    isQualityString = true;
                }

                start = (end == std::string::npos) ? std::string::npos : end + 1;
            }

            if (isQualityString) {
                gDisenchantQuality = qualityBitmask;
            } else {
                // It's an item name
                uint32_t cachedItemId = GetItemIdFromCache(param);
                if (cachedItemId != 0) {
                    gDisenchantItemId = cachedItemId;
                } else {
                    // Try to find the item to get its ID
                    auto itemSearchResult = FindPlayerItem(0, param);
                    if (itemSearchResult.found()) {
                        gDisenchantItemId = game::GetItemId(itemSearchResult.item);
                        // Cache the item name for future lookups
                        CacheItemNameToId(param, gDisenchantItemId);
                    } else {
                        // Item not found
                        lua_pushnumber(luaState, 0);
                        return 1;
                    }
                }
            }
        }

        // Parse optional includeSoulbound parameter (defaults to false)
        // Any non-zero integer enables it
        if (lua_isnumber(luaState, 2)) {
            gDisenchantIncludeSoulbound = (lua_tonumber(luaState, 2) != 0);
        }

        // Try the first disenchant
        bool success = TryDisenchant();

        lua_pushnumber(luaState, success ? 1 : 0);
        return 1;
    }

    uint32_t Script_GetItemIconTexture(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: GetItemIconTexture(displayInfoId)");
            return 0;
        }

        uint32_t displayInfoId = static_cast<uint32_t>(lua_tonumber(luaState, 1));

        auto const getInventoryArt = reinterpret_cast<GetInventoryArtT>(Offsets::CGItem_C_GetInventoryArt);
        char *texturePath = getInventoryArt(displayInfoId);

        if (texturePath && texturePath[0] != '\0' && !strstr(texturePath, "QuestionMark")) {
            lua_pushstring(luaState, texturePath);
        } else {
            lua_pushnil(luaState);
        }

        return 1;
    }

    uint32_t Script_GetSpellIconTexture(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: GetSpellIconTexture(spellIconId)");
            return 0;
        }

        int32_t iconId = static_cast<int32_t>(lua_tonumber(luaState, 1));

        int32_t maxIconId = *reinterpret_cast<int32_t *>(Offsets::SpellIconDBMaxId);
        uintptr_t *iconArray = *reinterpret_cast<uintptr_t **>(Offsets::SpellIconDBArray);

        if (iconId < 0 || iconId > maxIconId) {
            lua_pushnil(luaState);
            return 1;
        }

        uintptr_t iconEntry = reinterpret_cast<uintptr_t *>(iconArray)[iconId];
        if (iconEntry == 0) {
            lua_pushnil(luaState);
            return 1;
        }

        char *texturePath = *reinterpret_cast<char **>(iconEntry + 4);
        if (texturePath && texturePath[0] != '\0' && !strstr(texturePath, "QuestionMark")) {
            lua_pushstring(luaState, texturePath);
        } else {
            lua_pushnil(luaState);
        }

        return 1;
    }

    uint32_t Script_GetPlayerAuraDuration(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: GetPlayerAuraDuration(auraSlot)");
            return 0;
        }

        auto auraSlot = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        if (auraSlot >= MAX_AURA_SLOTS) {
            lua_pushnil(luaState);
            return 1;
        }

        // Get spellId from player unit fields
        auto *unitFields = game::GetActivePlayerUnitFields();
        if (!unitFields) {
            lua_pushnil(luaState);
            return 1;
        }
        uint32_t spellId = unitFields->aura[auraSlot];

        uint32_t expirationTime = gAuraExpirationTime[auraSlot];
        auto now = static_cast<uint32_t>(GetWowTimeMs() & 0xFFFFFFFF);
        int remainingDurationMs = expirationTime > now ? static_cast<int>(expirationTime - now) : 0;

        lua_pushnumber(luaState, spellId);
        lua_pushnumber(luaState, remainingDurationMs);
        lua_pushnumber(luaState, expirationTime);
        return 3;
    }

    uint32_t Script_CancelPlayerAuraSlot(uintptr_t *luaState) {
        constexpr uint32_t kMaxCancelAuraSlots = 32;
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: CancelPlayerAuraSlot(auraSlot)");
            return 0;
        }

        auto auraSlot = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        if (auraSlot >= kMaxCancelAuraSlots) {
            lua_pushnumber(luaState, 0);
            return 1;
        }

        auto *unitFields = game::GetActivePlayerUnitFields();
        if (!unitFields) {
            lua_pushnumber(luaState, 0);
            return 1;
        }

        auto spellId = static_cast<int>(unitFields->aura[auraSlot]);
        if (spellId == 0) {
            lua_pushnumber(luaState, 0);
            return 1;
        }

        auto const cancelAura = reinterpret_cast<void(__fastcall *)(int)>(Offsets::Spell_C_CancelAura);
        cancelAura(spellId);

        lua_pushnumber(luaState, 1);
        return 1;
    }

    uint32_t Script_CancelPlayerAuraSpellId(uintptr_t *luaState) {
        constexpr uint32_t kMaxCancelAuraSlots = 32;
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: CancelPlayerAuraSpellId(spellId, [ignoreMissing])");
            return 0;
        }

        auto spellId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        if (spellId == 0) {
            lua_pushnumber(luaState, 0);
            return 1;
        }

        // Optional arg2: 1 skips aura-presence check, 0/missing keeps default behavior.
        bool ignoreMissing = false;
        if (lua_isnumber(luaState, 2)) {
            ignoreMissing = (static_cast<int>(lua_tonumber(luaState, 2)) == 1);
        }

        if (!ignoreMissing) {
            auto *unitFields = game::GetActivePlayerUnitFields();
            if (!unitFields) {
                lua_pushnumber(luaState, 0);
                return 1;
            }

            bool foundSpell = false;
            for (uint32_t auraSlot = 0; auraSlot < kMaxCancelAuraSlots; ++auraSlot) {
                if (unitFields->aura[auraSlot] == spellId) {
                    foundSpell = true;
                    break;
                }
            }

            if (!foundSpell) {
                lua_pushnumber(luaState, 0);
                return 1;
            }
        }

        auto const cancelAura = reinterpret_cast<void(__fastcall *)(uint32_t)>(Offsets::Spell_C_CancelAura);
        cancelAura(spellId);

        lua_pushnumber(luaState, 1);
        return 1;
    }

    uint32_t Script_SetMouseoverUnit(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        uint64_t guid = 0;
        if (lua_isstring(luaState, 1)) {
            const char *unitToken = lua_tostring(luaState, 1);
            auto const getGUIDFromName = reinterpret_cast<GetGUIDFromNameT>(Offsets::GetGUIDFromName);
            guid = getGUIDFromName(unitToken);
        }

        auto const handleObjectTrackChange = reinterpret_cast<CGGameUI_HandleObjectTrackChangeT>(
            Offsets::CGGameUI_HandleObjectTrackChange);
        handleObjectTrackChange(guid, 0, 0);

        auto const mouseoverLow = *reinterpret_cast<uint32_t *>(Offsets::MouseoverGuidLow);
        auto const mouseoverHigh = *reinterpret_cast<uint32_t *>(Offsets::MouseoverGuidHigh);

        if (mouseoverLow != 0 || mouseoverHigh != 0) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnil(luaState);
        }

        return 1;
    }

    uint32_t CSimpleFrame_GetNameHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        // Type ID lazy initialization (mirrors original at 0x00CF0C3C / 0x00CEEF6C)
        static auto typeIdCounter = reinterpret_cast<uint32_t *>(0x00CEEF6C);
        static auto nameplateTypeId = reinterpret_cast<uint32_t *>(0x00CF0C3C);

        if (*nameplateTypeId == 0) {
            *typeIdCounter = *typeIdCounter + 1;
            *nameplateTypeId = *typeIdCounter;
        }
        uint32_t typeId = *nameplateTypeId;

        uintptr_t *frameObj = nullptr;

        int luaArgType = lua_type(luaState, 1);
        if (luaArgType == LUA_TTABLE) {
            lua_rawgeti(luaState, 1, 0);

            frameObj = reinterpret_cast<uintptr_t *>(lua_touserdata(luaState, -1));
            lua_settop(luaState, -2);

            if (frameObj == nullptr) {
                lua_error(luaState, "Attempt to find 'this' in non-frame table");
            } else {
                // Type check via vtable[4] (offset 0x10)
                using IsTypeT = char (__thiscall *)(uintptr_t *thisPtr, uint32_t typeId);
                auto vtable = reinterpret_cast<uintptr_t *>(*frameObj);
                auto isType = reinterpret_cast<IsTypeT>(vtable[4]);
                if (isType(frameObj, typeId) == 0) {
                    lua_error(luaState, "Wrong object type for member function");
                }
            }
        } else {
            lua_error(luaState, "Attempt to find 'this' in non-table value");
        }

        // Check if second arg is passed (e.g. nameplate:GetName(1) to get GUID)
        if (lua_isnumber(luaState, 2) && static_cast<int>(lua_tonumber(luaState, 2)) != 0) {
            // Return GUID string from frame object at offset 0x4E8/0x4EC
            uint32_t guidLow = frameObj[0x13a];
            uint32_t guidHigh = frameObj[0x13b];
            uint64_t guid = (static_cast<uint64_t>(guidHigh) << 32) | guidLow;

            PushGuidString(luaState, guid);
        } else {
            // GetName via vtable[1] (offset 0x4)
            using GetNameT = char *(__thiscall *)(uintptr_t *thisPtr);
            auto vtable = reinterpret_cast<uintptr_t *>(*frameObj);
            auto getName = reinterpret_cast<GetNameT>(vtable[1]);
            char *name = getName(frameObj);

            if (name != nullptr && name[0] != '\0') {
                lua_pushstring(luaState, name);
            } else {
                lua_pushnil(luaState);
            }
        }

        return 1;
    }

}
