//
// Created by pmacc on 1/15/2025.
//

#include "spell_scripts.hpp"
#include "helper.hpp"
#include "lua_refs.hpp"
#include "offsets.hpp"
#include "dbc_fields.hpp"
#include <cstring>

namespace Nampower {
    // Reusable table references to reduce memory allocations
    static int spellRecTableRef = LUA_REFNIL;

    // Map to store separate references for each array field name (for Field function)
    static std::unordered_map<std::string, int> spellRecArrayFieldRefs;

    // Map to store separate references for nested array fields (for main table function)
    static std::unordered_map<std::string, int> spellRecNestedArrayRefs;

    uint32_t Script_CastSpellByNameNoQueue(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        DEBUG_LOG("Casting next spell without queuing");
        // turn on forceQueue and then call regular CastSpellByName
        gNoQueueCast = true;
        auto const Script_CastSpellByName = reinterpret_cast<LuaScriptT>(Offsets::Script_CastSpellByName);
        auto result = Script_CastSpellByName(luaState);
        gNoQueueCast = false;

        return result;
    }

    uint32_t Script_QueueSpellByName(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

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
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

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

                uint64_t targetGUID = GetUnitGuidFromString(target);

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
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

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

    uint32_t Script_GetSpellIdForName(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

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
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (lua_isnumber(luaState, 1)) {
            auto const spellId = uint32_t(lua_tonumber(luaState, 1));
            auto const spell = game::GetSpellInfo(spellId);

            if (spell) {
                auto const language = *reinterpret_cast<std::uint32_t *>(Offsets::Language);
                lua_pushstring(luaState, (char *) spell->Name[language]);
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
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

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
                    spellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownSpells) +
                                                            spellSlot * 4);
                } else {
                    spellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownPetSpells) +
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

    uint32_t Script_GetSpellRec(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: GetSpellRec(spellId, [copy])");
            return 0;
        }

        uint32_t spellId = static_cast<uint32_t>(lua_tonumber(luaState, 1));

        // Check for optional copy parameter
        bool useCopy = false;
        if (lua_isnumber(luaState, 2)) {
            useCopy = static_cast<int>(lua_tonumber(luaState, 2)) != 0;
        }

        // Get spell info
        auto spell = game::GetSpellInfo(spellId);
        if (!spell) {
            lua_pushnil(luaState);
            return 1;
        }

        // Create new table or get reusable table based on copy parameter
        if (useCopy) {
            lua_newtable(luaState);
        } else {
            GetTableRef(luaState, spellRecTableRef);
        }

        // Push all simple fields using descriptors
        PushFieldsToLua(luaState, spell, spellRecFields, spellRecFieldsCount);

        // Push string fields manually (require language)
        auto const language = *reinterpret_cast<uint32_t *>(Offsets::Language);
        PushTableValue(luaState, const_cast<char *>("name"),
                       spell->Name[language] ? const_cast<char *>(spell->Name[language])
                                             : const_cast<char *>(""));
        PushTableValue(luaState, const_cast<char *>("rank"),
                       spell->Rank[language] ? const_cast<char *>(spell->Rank[language])
                                             : const_cast<char *>(""));
        PushTableValue(luaState, const_cast<char *>("description"),
                       spell->Description[language] ? const_cast<char *>(spell->Description[language])
                                                    : const_cast<char *>(""));
        PushTableValue(luaState, const_cast<char *>("tooltip"),
                       spell->ToolTip[language] ? const_cast<char *>(spell->ToolTip[language])
                                                : const_cast<char *>(""));

        // Push all array fields using descriptors with or without references based on copy parameter
        if (useCopy) {
            PushArrayFieldsToLua(luaState, spell, spellRecArrayFields, spellRecArrayFieldsCount);
        } else {
            PushArrayFieldsToLuaWithRefs(luaState, spell, spellRecArrayFields, spellRecArrayFieldsCount, spellRecNestedArrayRefs);
        }

        return 1; // Return the table
    }

    uint32_t Script_GetSpellRecField(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isnumber(luaState, 1) || !lua_isstring(luaState, 2)) {
            lua_error(luaState, "Usage: GetSpellRecField(spellId, fieldName, [copy])");
            return 0;
        }

        InitializeFieldMaps();

        uint32_t spellId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        const char *fieldName = lua_tostring(luaState, 2);

        // Check for optional copy parameter
        bool useCopy = false;
        if (lua_isnumber(luaState, 3)) {
            useCopy = static_cast<int>(lua_tonumber(luaState, 3)) != 0;
        }

        // Get spell info
        auto spell = game::GetSpellInfo(spellId);
        if (!spell) {
            lua_pushnil(luaState);
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
                case FieldType::UINT8:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint8_t *>(fieldPtr));
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

            // Create new table or get reusable table based on copy parameter
            if (useCopy) {
                lua_newtable(luaState);
            } else {
                // Get or create reusable table for this specific field name
                GetTableRef(luaState, spellRecArrayFieldRefs[fieldName]);
            }

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

        // Check for special string fields (localized, need language index)
        auto const language = *reinterpret_cast<uint32_t *>(Offsets::Language);
        if (strcmp(fieldName, "name") == 0) {
            lua_pushstring(luaState, spell->Name[language] ? const_cast<char *>(spell->Name[language])
                                                           : const_cast<char *>(""));
            return 1;
        }
        if (strcmp(fieldName, "rank") == 0) {
            lua_pushstring(luaState, spell->Rank[language] ? const_cast<char *>(spell->Rank[language])
                                                           : const_cast<char *>(""));
            return 1;
        }
        if (strcmp(fieldName, "description") == 0) {
            lua_pushstring(luaState, spell->Description[language] ? const_cast<char *>(spell->Description[language])
                                                                  : const_cast<char *>(""));
            return 1;
        }
        if (strcmp(fieldName, "tooltip") == 0) {
            lua_pushstring(luaState, spell->ToolTip[language] ? const_cast<char *>(spell->ToolTip[language])
                                                              : const_cast<char *>(""));
            return 1;
        }

        // Field not found
        lua_error(luaState, "Unknown field name");
        return 0;
    }

    uint32_t Script_GetSpellModifiers(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isnumber(luaState, 1) || !lua_isnumber(luaState, 2)) {
            lua_error(luaState, "Usage: GetSpellModifiers(spellId, modifierType) - modifierType: 0=DAMAGE, 1=DURATION, 2=THREAT, 3=ATTACK_POWER, 4=CHARGES, 5=RANGE, 6=RADIUS, 7=CRITICAL_CHANCE, 8=ALL_EFFECTS, 9=NOT_LOSE_CASTING_TIME, 10=CASTING_TIME, 11=COOLDOWN, 12=SPEED, 14=COST, 15=CRIT_DAMAGE_BONUS, 16=RESIST_MISS_CHANCE, 17=JUMP_TARGETS, 18=CHANCE_OF_SUCCESS, 19=ACTIVATION_TIME, 20=EFFECT_PAST_FIRST, 21=CASTING_TIME_OLD, 22=DOT, 23=HASTE, 24=SPELL_BONUS_DAMAGE, 27=MULTIPLE_VALUE, 28=RESIST_DISPEL_CHANCE");
            return 0;
        }

        uint32_t spellId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        int modifierType = static_cast<int>(lua_tonumber(luaState, 2));

        // Validate modifier type
        if (modifierType < 0 || modifierType >= game::MAX_SPELLMOD) {
            lua_error(luaState, "Invalid modifier type. Must be between 0 and 28.");
            return 0;
        }

        // Get spell info
        auto spell = game::GetSpellInfo(spellId);
        if (!spell) {
            lua_error(luaState, "Invalid spell id");
            return 0;
        }

        // Call Spell_C_GetSpellModifierValues
        using Spell_C_GetSpellModifierValuesT = uint32_t (__fastcall *)(game::SpellRec *spellRec, int mod, int *flatMod, uint32_t *percentMod);
        auto const getModifierValues = reinterpret_cast<Spell_C_GetSpellModifierValuesT>(Offsets::Spell_C_GetSpellModifierValues);

        int flatModificationValue = 0;
        uint32_t percentModificationValue = 0;

        auto ret = getModifierValues(const_cast<game::SpellRec *>(spell), modifierType, &flatModificationValue, &percentModificationValue);

        // show flat modifications
        lua_pushnumber(luaState, flatModificationValue);

        if (flatModificationValue != 0 && percentModificationValue >= 100) {
            percentModificationValue -= 100;
        } else if (modifierType == 9 && percentModificationValue >= 100) {
            percentModificationValue -= 100;
        }

        // show percent modifications
        lua_pushnumber(luaState, percentModificationValue);

        // show return
        lua_pushnumber(luaState, ret);

        return 3;
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
