//
// Created by pmacc on 1/8/2025.
//

#include "scripts.hpp"
#include "offsets.hpp"
#include "items.hpp"
#include "dbc_fields.hpp"
#include "helper.hpp"
#include <cstring>

namespace Nampower {
    auto const lua_error = reinterpret_cast<lua_errorT>(Offsets::lua_error);

    auto const lua_isstring = reinterpret_cast<lua_isstringT>(Offsets::lua_isstring);
    auto const lua_isnumber = reinterpret_cast<lua_isnumberT>(Offsets::lua_isnumber);

    auto const lua_tostring = reinterpret_cast<lua_tostringT>(Offsets::lua_tostring);
    auto const lua_tonumber = reinterpret_cast<lua_tonumberT>(Offsets::lua_tonumber);

    // Export Lua functions for dbc_fields.hpp templates
    lua_pushnumberT lua_pushnumber = reinterpret_cast<lua_pushnumberT>(Offsets::lua_pushnumber);
    lua_pushstringT lua_pushstring = reinterpret_cast<lua_pushstringT>(Offsets::lua_pushstring);
    lua_newtableT lua_newtable = reinterpret_cast<lua_newtableT>(Offsets::lua_newtable);
    lua_settableT lua_settable = reinterpret_cast<lua_settableT>(Offsets::lua_settable);

    bool gScriptQueued;
    int gScriptPriority = 1;
    char *queuedScript;

    uint32_t Script_CastSpellByNameNoQueue(uintptr_t *luaState) {
        DEBUG_LOG("Casting next spell without queuing");
        // turn on forceQueue and then call regular CastSpellByName
        gNoQueueCast = true;
        auto const Script_CastSpellByName = reinterpret_cast<LuaScriptT>(Offsets::Script_CastSpellByName);
        auto result = Script_CastSpellByName(luaState);
        gNoQueueCast = false;

        return result;
    }

    uint32_t Script_QueueSpellByName(uintptr_t *luaState) {
        DEBUG_LOG("Force queuing next cast spell");
        // turn on forceQueue and then call regular CastSpellByName
        gForceQueueCast = true;
        auto const Script_CastSpellByName = reinterpret_cast<LuaScriptT>(Offsets::Script_CastSpellByName);
        auto result = Script_CastSpellByName(luaState);
        gForceQueueCast = false;

        return result;
    }

    uint32_t Script_SpellStopCastingHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState) {
        DEBUG_LOG("SpellStopCasting called");

        ClearQueuedSpells();
        ResetCastFlags();
        ResetChannelingFlags();

        auto const spellStopCasting = detour->GetTrampolineT<LuaScriptT>();
        return spellStopCasting(luaState);
    }

    uint32_t Script_IsSpellInRange(uintptr_t *luaState) {
        auto param1IsString = lua_isstring(luaState, 1);
        auto param1IsNumber = lua_isnumber(luaState, 1);
        if (param1IsString || param1IsNumber) {
            uint32_t spellId = 0;

            if (param1IsNumber) {
                spellId = uint32_t(lua_tonumber(luaState, 1));

                if (spellId == 0) {
                    lua_error(luaState, "Unable to parse spell id");
                    return 0;
                }
            } else {
                auto const spellName = lua_tostring(luaState, 1);

                spellId = GetSpellIdFromSpellName(spellName);
                if (spellId == 0) {
                    lua_error(luaState,
                              "Unable to determine spell id from spell name, possibly because it isn't in your spell book.  Try IsSpellInRange(SPELL_ID) instead");
                    return 0;
                }
            }

            auto spell = game::GetSpellInfo(spellId);
            if (spell) {
                std::set<uint32_t> validTargetTypes = {5, 6, 21, 25};
                if (sizeof spell->EffectImplicitTargetA == 0 ||
                    validTargetTypes.count(spell->EffectImplicitTargetA[0]) == 0) {
                    lua_pushnumber(luaState, -1.0);
                    return 1;
                }

                char *target;

                if (lua_isstring(luaState, 2)) {
                    target = lua_tostring(luaState, 2);
                } else {
                    char defaultTarget[] = "target";
                    target = defaultTarget;
                }

                uint64_t targetGUID;
                if (strncmp(target, "0x", 2) == 0 || strncmp(target, "0X", 2) == 0) {
                    // already a guid
                    targetGUID = std::stoull(target, nullptr, 16);
                } else {
                    auto const getGUIDFromName = reinterpret_cast<GetGUIDFromNameT>(Offsets::GetGUIDFromName);
                    targetGUID = getGUIDFromName(target);
                }

                auto playerUnit = game::GetObjectPtr(game::ClntObjMgrGetActivePlayerGuid());

                auto const RangeCheckSelected = reinterpret_cast<RangeCheckSelectedT>(Offsets::RangeCheckSelected);
                auto const result = RangeCheckSelected(playerUnit, spell, targetGUID, '\0');

                if (result != 0) {
                    lua_pushnumber(luaState, 1.0);
                } else {
                    lua_pushnumber(luaState, 0);
                }
                return 1;
            } else {
                lua_error(luaState, "Spell not found");
            }
        } else {
            lua_error(luaState, "Usage: IsSpellInRange(spellName)");
        }

        return 0;
    }

    uint32_t Script_IsSpellUsable(uintptr_t *luaState) {
        auto param1IsString = lua_isstring(luaState, 1);
        auto param1IsNumber = lua_isnumber(luaState, 1);
        if (param1IsString || param1IsNumber) {
            uint32_t spellId = 0;

            if (param1IsNumber) {
                spellId = uint32_t(lua_tonumber(luaState, 1));

                if (spellId == 0) {
                    lua_error(luaState, "Unable to parse spell id");
                    return 0;
                }
            } else {
                auto const spellName = lua_tostring(luaState, 1);

                spellId = GetSpellIdFromSpellName(spellName);
                if (spellId == 0) {
                    lua_error(luaState,
                              "Unable to determine spell id from spell name, possibly because it isn't in your spell book.  Try IsSpellUsable(SPELL_ID) instead");
                    return 0;
                }
            }

            auto spell = game::GetSpellInfo(spellId);
            if (spell) {
                auto const IsSpellUsable = reinterpret_cast<Spell_C_IsSpellUsableT>(Offsets::Spell_C_IsSpellUsable);

                uint32_t outOfMana = 0;
                auto const result = IsSpellUsable(spell, &outOfMana) & 0xFF;

                if (result != 0) {
                    lua_pushnumber(luaState, 1.0);
                } else {
                    lua_pushnumber(luaState, 0);
                }

                if (outOfMana) {
                    lua_pushnumber(luaState, 1.0);
                } else {
                    lua_pushnumber(luaState, 0);
                }

                return 2;
            } else {
                lua_error(luaState, "Spell not found");
            }
        } else {
            lua_error(luaState, "Usage: IsSpellUsable(spellName)");
        }

        return 0;
    }

    uint32_t Script_GetCurrentCastingInfo(uintptr_t *luaState) {
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

    uint32_t Script_GetSpellIdForName(uintptr_t *luaState) {
        if (lua_isstring(luaState, 1)) {
            auto const spellName = lua_tostring(luaState, 1);
            auto const spellId = GetSpellIdFromSpellName(spellName);
            lua_pushnumber(luaState, spellId);
            return 1;
        } else {
            lua_error(luaState, "Usage: GetSpellIdForName(spellName)");
        }

        return 0;
    }

    uint32_t Script_GetSpellNameAndRankForId(uintptr_t *luaState) {
        if (lua_isnumber(luaState, 1)) {
            auto const spellId = uint32_t(lua_tonumber(luaState, 1));
            auto const spell = game::GetSpellInfo(spellId);

            if (spell) {
                auto const language = *reinterpret_cast<std::uint32_t *>(Offsets::Language);
                lua_pushstring(luaState, (char *) spell->SpellName[language]);
                lua_pushstring(luaState, (char *) spell->Rank[language]);
                return 2;
            } else {
                DEBUG_LOG("Spell not found for id: " << spellId);
                lua_error(luaState, "Spell not found");
            }
        } else {
            lua_error(luaState, "Usage: GetSpellNameAndRankForId(spellId)");
        }

        return 0;
    }

    uint32_t Script_GetSpellSlotTypeIdForName(uintptr_t *luaState) {
        if (lua_isstring(luaState, 1)) {
            auto const spellName = lua_tostring(luaState, 1);
            uint32_t bookType;
            auto spellSlot = GetSpellSlotAndTypeForName(spellName, &bookType);

            // returns large number if spell not found
            if (spellSlot > 100000) {
                lua_pushnumber(luaState, 0);
                spellSlot = 0;
                bookType = 999;

                lua_pushnumber(luaState, spellSlot);
            } else {
                lua_pushnumber(luaState, spellSlot + 1); // lua is 1 indexed
            }

            if (bookType == 0) {
                char spell[] = "spell";
                lua_pushstring(luaState, spell);
            } else if (bookType == 1) {
                char pet[] = "pet";
                lua_pushstring(luaState, pet);
            } else {
                char unknown[] = "unknown";
                lua_pushstring(luaState, unknown);
            }

            if (spellSlot > 0 && spellSlot < 1024) {
                uint32_t spellId = 0;
                if (bookType == 0) {
                    spellId = *reinterpret_cast<uint32_t *>(uint32_t(Offsets::CGSpellBook_mKnownSpells) +
                                                            spellSlot * 4);
                } else {
                    spellId = *reinterpret_cast<uint32_t *>(uint32_t(Offsets::CGSpellBook_mKnownPetSpells) +
                                                            spellSlot * 4);
                }
                lua_pushnumber(luaState, spellId);
            } else {
                lua_pushnumber(luaState, 0);
            }

            return 3;
        } else {
            lua_error(luaState, "Usage: GetSpellSlotTypeIdForName(spellName)");
        }

        return 0;
    }

    uint32_t Script_ChannelStopCastingNextTick(uintptr_t *luaState) {
        if (gCastData.channeling) {
            DEBUG_LOG("ChannelStopCastingNextTick activated, canceling next tick");
            gCastData.cancelChannelNextTick = true;
        }

        return 0;
    }

    uint32_t Script_GetNampowerVersion(uintptr_t *luaState) {
        lua_pushnumber(luaState, MAJOR_VERSION);
        lua_pushnumber(luaState, MINOR_VERSION);
        lua_pushnumber(luaState, PATCH_VERSION);

        return 3;
    }

    uint32_t Script_GetItemLevel(uintptr_t *luaState) {
        if (lua_isnumber(luaState, 1)) {
            auto const itemId = static_cast<uint32_t>(lua_tonumber(luaState, 1));

            // Pointer to ItemDBCache
            void *itemDbCache = reinterpret_cast<void *>(Offsets::ItemDBCache);

            // Parameters for the DBCache<>::GetRecord function
            int **param2 = nullptr;
            int *param3 = nullptr;
            int *param4 = nullptr;
            char param5 = 0;

            // Call the DBCache<>::GetRecord function
            auto getRecord = reinterpret_cast<uintptr_t *(__thiscall *)(void *, uint32_t, int **, int *, int *, char)>(
                    Offsets::DBCacheGetRecord
            );
            uintptr_t *itemObject = getRecord(itemDbCache, itemId, param2, param3, param4, param5);
            if (itemObject == nullptr) {
                lua_error(luaState, "Item not found in DBCache");
                return 0;
            }

            uint32_t itemLevel = *reinterpret_cast<uint32_t *>(itemObject + 14);
            lua_pushnumber(luaState, itemLevel);
            return 1;
        } else {
            lua_error(luaState, "Usage: GetItemLevel(itemId)");
        }

        return 0;
    }

    uint32_t Script_QueueScript(uintptr_t *luaState) {
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

    uint32_t Script_GetItemStats(uintptr_t *luaState) {
        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: GetItemStats(itemId)");
            return 0;
        }

        uint32_t itemId = static_cast<uint32_t>(lua_tonumber(luaState, 1));

        // Load item if not cached
        LoadItem(itemId);

        // Get from cache
        game::ItemStats_C *item = GetItemStats(itemId);
        if (!item) {
            lua_error(luaState, "Item not found");
            return 0;
        }

        // Create new table
        lua_newtable(luaState);

        // Push all simple fields using descriptors
        PushFieldsToLua(luaState, item, itemStatsFields, itemStatsFieldsCount);

        // Push string fields manually (require language)
        auto const language = *reinterpret_cast<uint32_t *>(Offsets::Language);
        lua_pushstring(luaState, const_cast<char *>("displayName"));
        lua_pushstring(luaState,
                       item->m_displayName[language] ? item->m_displayName[language] : const_cast<char *>(""));
        lua_settable(luaState, -3);

        lua_pushstring(luaState, const_cast<char *>("description"));
        lua_pushstring(luaState, item->m_description ? item->m_description : const_cast<char *>(""));
        lua_settable(luaState, -3);

        // Push all array fields using descriptors
        PushArrayFieldsToLua(luaState, item, itemStatsArrayFields, itemStatsArrayFieldsCount);

        return 1; // Return the table
    }

    uint32_t Script_GetSpellRec(uintptr_t *luaState) {
        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: GetSpellRec(spellId)");
            return 0;
        }

        uint32_t spellId = static_cast<uint32_t>(lua_tonumber(luaState, 1));

        // Get spell info
        auto spell = game::GetSpellInfo(spellId);
        if (!spell) {
            lua_error(luaState, "Spell not found");
            return 0;
        }

        // Create new table
        lua_newtable(luaState);

        // Push all simple fields using descriptors
        PushFieldsToLua(luaState, spell, spellRecFields, spellRecFieldsCount);

        // Push string fields manually (require language)
        auto const language = *reinterpret_cast<uint32_t *>(Offsets::Language);
        lua_pushstring(luaState, const_cast<char *>("name"));
        lua_pushstring(luaState, spell->SpellName[language] ? const_cast<char *>(spell->SpellName[language])
                                                            : const_cast<char *>(""));
        lua_settable(luaState, -3);

        lua_pushstring(luaState, const_cast<char *>("rank"));
        lua_pushstring(luaState, reinterpret_cast<const char *>(spell->Rank[language])
                                 ? const_cast<char *>(reinterpret_cast<const char *>(spell->Rank[language]))
                                 : const_cast<char *>(""));
        lua_settable(luaState, -3);

        // Push all array fields using descriptors
        PushArrayFieldsToLua(luaState, spell, spellRecArrayFields, spellRecArrayFieldsCount);

        return 1; // Return the table
    }

    uint32_t Script_GetItemStatsField(uintptr_t *luaState) {
        if (!lua_isnumber(luaState, 1) || !lua_isstring(luaState, 2)) {
            lua_error(luaState, "Usage: GetItemStatsField(itemId, fieldName)");
            return 0;
        }

        InitializeFieldMaps();

        uint32_t itemId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        const char *fieldName = lua_tostring(luaState, 2);

        // Load item if not cached
        LoadItem(itemId);

        // Get from cache
        game::ItemStats_C *item = GetItemStats(itemId);
        if (!item) {
            lua_pushnumber(luaState, 0); // Return nil/false to indicate not found
            return 1;
        }

        // O(1) hash table lookup for simple fields
        auto simpleIt = itemStatsFieldMap.find(fieldName);
        if (simpleIt != itemStatsFieldMap.end()) {
            size_t i = simpleIt->second;
            const char *fieldPtr = reinterpret_cast<const char *>(item) + itemStatsFields[i].offset;

            switch (itemStatsFields[i].type) {
                case FieldType::INT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const int32_t *>(fieldPtr));
                    return 1;
                case FieldType::UINT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint32_t *>(fieldPtr));
                    return 1;
                case FieldType::FLOAT:
                    lua_pushnumber(luaState, *reinterpret_cast<const float *>(fieldPtr));
                    return 1;
                case FieldType::STRING: {
                    const char *str = *reinterpret_cast<const char *const *>(fieldPtr);
                    lua_pushstring(luaState, str ? const_cast<char *>(str) : const_cast<char *>(""));
                    return 1;
                }
                default:
                    break;
            }
        }

        // O(1) hash table lookup for array fields
        auto arrayIt = itemStatsArrayFieldMap.find(fieldName);
        if (arrayIt != itemStatsArrayFieldMap.end()) {
            size_t i = arrayIt->second;
            const auto &field = itemStatsArrayFields[i];
            lua_newtable(luaState);

            const char *fieldPtr = reinterpret_cast<const char *>(item) + field.offset;

            for (size_t j = 0; j < field.count; ++j) {
                lua_pushnumber(luaState, j + 1);

                switch (field.type) {
                    case FieldType::INT32:
                        lua_pushnumber(luaState, reinterpret_cast<const int32_t *>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT32:
                        lua_pushnumber(luaState, reinterpret_cast<const uint32_t *>(fieldPtr)[j]);
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

        // Check for special string fields
        if (strcmp(fieldName, "displayName") == 0) {
            auto const language = *reinterpret_cast<uint32_t *>(Offsets::Language);
            lua_pushstring(luaState,
                           item->m_displayName[language] ? item->m_displayName[language] : const_cast<char *>(""));
            return 1;
        }
        if (strcmp(fieldName, "description") == 0) {
            lua_pushstring(luaState, item->m_description ? item->m_description : const_cast<char *>(""));
            return 1;
        }

        // Field not found
        lua_error(luaState, "Unknown field name");
        return 0;
    }

    uint32_t Script_GetSpellRecField(uintptr_t *luaState) {
        if (!lua_isnumber(luaState, 1) || !lua_isstring(luaState, 2)) {
            lua_error(luaState, "Usage: GetSpellRecField(spellId, fieldName)");
            return 0;
        }

        InitializeFieldMaps();

        uint32_t spellId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        const char *fieldName = lua_tostring(luaState, 2);

        // Get spell info
        auto spell = game::GetSpellInfo(spellId);
        if (!spell) {
            lua_pushnumber(luaState, 0); // Return nil/false to indicate not found
            return 1;
        }

        // O(1) hash table lookup for simple fields
        auto simpleIt = spellRecFieldMap.find(fieldName);
        if (simpleIt != spellRecFieldMap.end()) {
            size_t i = simpleIt->second;
            const char *fieldPtr = reinterpret_cast<const char *>(spell) + spellRecFields[i].offset;

            switch (spellRecFields[i].type) {
                case FieldType::INT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const int32_t *>(fieldPtr));
                    return 1;
                case FieldType::UINT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint32_t *>(fieldPtr));
                    return 1;
                case FieldType::FLOAT:
                    lua_pushnumber(luaState, *reinterpret_cast<const float *>(fieldPtr));
                    return 1;
                case FieldType::UINT64:
                    lua_pushnumber(luaState, static_cast<double>(*reinterpret_cast<const uint64_t *>(fieldPtr)));
                    return 1;
                case FieldType::STRING: {
                    const char *str = *reinterpret_cast<const char *const *>(fieldPtr);
                    lua_pushstring(luaState, str ? const_cast<char *>(str) : const_cast<char *>(""));
                    return 1;
                }
            }
        }

        // O(1) hash table lookup for array fields
        auto arrayIt = spellRecArrayFieldMap.find(fieldName);
        if (arrayIt != spellRecArrayFieldMap.end()) {
            size_t i = arrayIt->second;
            const auto &field = spellRecArrayFields[i];
            lua_newtable(luaState);

            const char *fieldPtr = reinterpret_cast<const char *>(spell) + field.offset;

            for (size_t j = 0; j < field.count; ++j) {
                lua_pushnumber(luaState, j + 1);

                switch (field.type) {
                    case FieldType::INT32:
                        lua_pushnumber(luaState, reinterpret_cast<const int32_t *>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT32:
                        lua_pushnumber(luaState, reinterpret_cast<const uint32_t *>(fieldPtr)[j]);
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

        // Check for special string fields
        if (strcmp(fieldName, "name") == 0) {
            auto const language = *reinterpret_cast<uint32_t *>(Offsets::Language);
            lua_pushstring(luaState, spell->SpellName[language] ? const_cast<char *>(spell->SpellName[language])
                                                                : const_cast<char *>(""));
            return 1;
        }
        if (strcmp(fieldName, "rank") == 0) {
            auto const language = *reinterpret_cast<uint32_t *>(Offsets::Language);
            lua_pushstring(luaState, reinterpret_cast<const char *>(spell->Rank[language])
                                     ? const_cast<char *>(reinterpret_cast<const char *>(spell->Rank[language]))
                                     : const_cast<char *>(""));
            return 1;
        }

        // Field not found
        lua_error(luaState, "Unknown field name");
        return 0;
    }

    uint32_t GetSpellSlotFromLuaHook(hadesmem::PatchDetourBase *detour, int param_1, uint32_t *slot, uint32_t *type) {
        // Get Lua state pointer
        uintptr_t *luaState = GetLuaStatePtr();

        // Check if first parameter is not a number
        if (lua_isnumber(luaState, 1) == 0) {
            const char *spellStr = lua_tostring(luaState, 1);

            // Check if string starts with "spellId:" (case-insensitive)
            if (_strnicmp(spellStr, "spellId:", 8) == 0) {
                // Extract spell ID from string
                uint32_t spellId = atoi(spellStr + 8);

                if (spellId > 0) {
                    // Check if second parameter has "pet"
                    uint32_t bookType = 0;
                    if (lua_isstring(luaState, 2)) {
                        const char *bookTypeStr = lua_tostring(luaState, 2);
                        if (_strcmpi(bookTypeStr, "pet") == 0) {
                            bookType = 1;
                        }
                    }

                    uint32_t spellSlot = ConvertSpellIdToSpellSlot(spellId, bookType);
                    if(!spellSlot){
                        lua_error(luaState, "Unable convert spell id to spell slot.");
                        return 0;
                    }

                    if (slot) {
                        *slot = spellSlot;
                    }
                    if (type) {
                        *type = bookType; // Use the requested bookType
                    }

                    return 1;
                } else {
                    lua_error(luaState, "Unable parse spell id.  Format is spellId:123.");
                    return 0;
                }
            } else {
                uint32_t bookType;
                auto spellSlot =  GetSpellSlotAndTypeForName(spellStr, &bookType);
                // returns large number if spell not found
                if (spellSlot == 0 || spellSlot > 100000) {
                    lua_error(luaState, "Unable to find spell slot for spell name.");
                    return 0;
                } else {
                    if (slot) {
                        *slot = spellSlot;
                    }
                    if (type) {
                        *type = bookType;
                    }
                }

                return 1;
            }
        } else {
            auto const getSpellSlotFromLua = detour->GetTrampolineT<GetSpellSlotFromLuaT>();
            // Fall back to original function
            return getSpellSlotFromLua(param_1, slot, type);
        }

        return 0;
    }
}
