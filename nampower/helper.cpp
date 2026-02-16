//
// Created by pmacc on 9/21/2024.
//

#include "helper.hpp"
#include "offsets.hpp"
#include "main.hpp"
#include "dbc_fields.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>
#include <string>

namespace Nampower {
    // Lua function pointers
    lua_errorT lua_error = reinterpret_cast<lua_errorT>(Offsets::lua_error);
    lua_gettopT lua_gettop = reinterpret_cast<lua_gettopT>(Offsets::lua_gettop);
    lua_settopT lua_settop = reinterpret_cast<lua_settopT>(Offsets::lua_settop);
    lua_typeT lua_type = reinterpret_cast<lua_typeT>(Offsets::lua_type);
    lua_isstringT lua_isstring = reinterpret_cast<lua_isstringT>(Offsets::lua_isstring);
    lua_isnumberT lua_isnumber = reinterpret_cast<lua_isnumberT>(Offsets::lua_isnumber);
    lua_tostringT lua_tostring = reinterpret_cast<lua_tostringT>(Offsets::lua_tostring);
    lua_tonumberT lua_tonumber = reinterpret_cast<lua_tonumberT>(Offsets::lua_tonumber);
    lua_pushnumberT lua_pushnumber = reinterpret_cast<lua_pushnumberT>(Offsets::lua_pushnumber);
    lua_pushstringT lua_pushstring = reinterpret_cast<lua_pushstringT>(Offsets::lua_pushstring);
    // lua_pushbooleanT lua_pushboolean = reinterpret_cast<lua_pushbooleanT>(Offsets::lua_pushboolean);  doesn't seem to work properly
    lua_pushnilT lua_pushnil = reinterpret_cast<lua_pushnilT>(Offsets::lua_pushnil);
    lua_newtableT lua_newtable = reinterpret_cast<lua_newtableT>(Offsets::lua_newtable);
    lua_settableT lua_settable = reinterpret_cast<lua_settableT>(Offsets::lua_settable);
    luaL_refT luaL_ref = reinterpret_cast<luaL_refT>(Offsets::luaL_ref);
    lua_touserdataT lua_touserdata = reinterpret_cast<lua_touserdataT>(Offsets::lua_touserdata);
    lua_rawgetiT lua_rawgeti = reinterpret_cast<lua_rawgetiT>(Offsets::lua_rawgeti);
    luaL_unrefT luaL_unref = reinterpret_cast<luaL_unrefT>(Offsets::luaL_unref);


    int32_t GetSpellSlotAndTypeForName(const char *spellName, uint32_t *spellType) {
        struct CachedEntry {
            int32_t slot;
            uint32_t type;
            std::string cachedNameLower;
        };
        static std::unordered_map<std::string, CachedEntry> spellSlotCache;

        auto const normalize = [](const char *name) -> std::string {
            std::string out = name ? name : "";
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            return out;
        };

        const std::string key = normalize(spellName);

        auto const spellNameMatchesAtSlot = [&normalize](const char *requestedLower, uint32_t slot,
                                                         uint32_t type) -> bool {
            if (!requestedLower) {
                return false;
            }

            uint32_t spellId = 0;
            if (type == 0) {
                spellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownSpells) + slot * 4);
            } else if (type == 1) {
                spellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownPetSpells) + slot * 4);
            } else {
                return false;
            }

            if (spellId == 0) {
                return false;
            }

            auto const spell = game::GetSpellInfo(spellId);
            if (!spell) {
                return false;
            }

            auto const language = *reinterpret_cast<uint32_t *>(Offsets::Language);
            auto const actualName = reinterpret_cast<const char *>(spell->Name[language]);
            if (!actualName) {
                return false;
            }

            auto const actualLower = normalize(actualName);
            return _stricmp(actualLower.c_str(), requestedLower) == 0;
        };

        if (!key.empty()) {
            auto cacheIt = spellSlotCache.find(key);
            if (cacheIt != spellSlotCache.end()) {
                auto const &entry = cacheIt->second;
                if (entry.slot < 1024 && spellNameMatchesAtSlot(key.c_str(), entry.slot, entry.type)) {
                    if (spellType) {
                        *spellType = entry.type;
                    }
                    return entry.slot;
                }
            }
        }

        auto const getSpellSlotAndType = reinterpret_cast<GetSpellSlotAndTypeT>(Offsets::GetSpellSlotAndType);
        auto const slot = getSpellSlotAndType(spellName, spellType);

        if (slot < 1024 && spellType && !key.empty()) {
            spellSlotCache[key] = {slot, *spellType, key};
        }

        return slot;
    }

    uint32_t GetSpellIdFromSpellName(const char *spellName) {
        uint32_t bookType;
        int32_t spellSlot = GetSpellSlotAndTypeForName(spellName, &bookType);

        uint32_t spellId = 0;
        if (spellSlot >= 0 && spellSlot < 1024) {
            if (bookType == 0) {
                spellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownSpells) +
                                                        spellSlot * 4);
            } else {
                spellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownPetSpells) +
                                                        spellSlot * 4);
            }
        }

        return spellId;
    }

    uint32_t ConvertSpellIdToSpellSlot(uint32_t spellId, uint32_t bookType) {
        static std::unordered_map<uint64_t, uint32_t> spellIdToSlotCache;
        auto const cacheKey = (uint64_t(spellId) << 1) | (bookType & 0x1);

        auto const slotHasSpellId = [bookType](uint32_t slot, uint32_t expectedId) -> bool {
            if (slot >= 1024) {
                return false;
            }
            uint32_t slotSpellId = 0;
            if (bookType == 0) {
                slotSpellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownSpells) + slot * 4);
            } else {
                slotSpellId = *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGSpellBook_mKnownPetSpells) + slot * 4);
            }
            return slotSpellId == expectedId;
        };

        auto cacheIt = spellIdToSlotCache.find(cacheKey);
        if (cacheIt != spellIdToSlotCache.end()) {
            if (slotHasSpellId(cacheIt->second, spellId)) {
                return cacheIt->second;
            }
        }

        // Search through player spellbook
        if (bookType == 0) {
            for (uint32_t spellSlot = 0; spellSlot < 1024; spellSlot++) {
                uint32_t slotSpellId = *reinterpret_cast<uint32_t *>(uint32_t(Offsets::CGSpellBook_mKnownSpells) +
                                                                     spellSlot * 4);
                if (slotSpellId == spellId) {
                    spellIdToSlotCache[cacheKey] = spellSlot;
                    return spellSlot;
                }
            }
        } else {
            // Search through pet spellbook
            for (uint32_t spellSlot = 0; spellSlot < 1024; spellSlot++) {
                uint32_t slotSpellId = *reinterpret_cast<uint32_t *>(uint32_t(Offsets::CGSpellBook_mKnownPetSpells) +
                                                                     spellSlot * 4);
                if (slotSpellId == spellId) {
                    spellIdToSlotCache[cacheKey] = spellSlot;
                    return spellSlot;
                }
            }
        }

        return 0;
    }


    bool SpellIsOnGcd(const game::SpellRec *spell) {
        if (spell->Id == 51714) {
            // power overwhelming gcd removed but client not updated
            return false;
        }

        return spell->StartRecoveryCategory == 133;
    }

    bool SpellIsChanneling(const game::SpellRec *spell) {
        return spell->AttributesEx & game::SPELL_ATTR_EX_IS_CHANNELED ||
               spell->AttributesEx & game::SPELL_ATTR_EX_IS_SELF_CHANNELED;
    }

    bool SpellIsTargeting(const game::SpellRec *spell) {
        return spell->Targets & game::SpellCastTargetFlags::TARGET_FLAG_SOURCE_LOCATION ||
               spell->Targets & game::SpellCastTargetFlags::TARGET_FLAG_DEST_LOCATION;
    }

    bool SpellIsOnSwing(const game::SpellRec *spell) {
        return spell->Attributes & game::SPELL_ATTR_ON_NEXT_SWING_1;
    }

    bool SpellIsAttackTradeskillOrEnchant(const game::SpellRec *spell) {
        return (
                spell->Effect[0] == game::SpellEffects::SPELL_EFFECT_ATTACK ||
                spell->Attributes & game::SpellAttributes::SPELL_ATTR_TRADESPELL ||
                spell->Effect[0] == game::SpellEffects::SPELL_EFFECT_TRADE_SKILL ||
                spell->Effect[0] == game::SpellEffects::SPELL_EFFECT_TRANS_DOOR ||
                spell->Effect[0] == game::SpellEffects::SPELL_EFFECT_ENCHANT_ITEM ||
                spell->Effect[0] == game::SpellEffects::SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY ||
                spell->Effect[0] == game::SpellEffects::SPELL_EFFECT_CREATE_ITEM ||
                spell->Effect[0] == game::SpellEffects::SPELL_EFFECT_OPEN_LOCK ||
                spell->Effect[0] == game::SpellEffects::SPELL_EFFECT_OPEN_LOCK_ITEM);
    }

    bool SpellIsMounting(const game::SpellRec *spell) {
        for (int i = 0; i < 3; ++i) {
            if (spell->Effect[i] == game::SPELL_EFFECT_APPLY_AURA && spell->EffectApplyAuraName[i] == 78) {
                return true;
            }
        }
        return false;
    }

    // if the spell is off cooldown, this will return the gcd, otherwise the cooldown
    uint32_t GetGcdOrCooldownForSpell(uint32_t spellId) {
        uint32_t duration;
        uint64_t startTime;
        uint32_t enable;

        auto const getSpellCooldown = reinterpret_cast<Spell_C_GetSpellCooldownT>(Offsets::Spell_C_GetSpellCooldown);
        getSpellCooldown(spellId, 0, &duration, &startTime, &enable);

        if (spellId == 51714 && duration == 1500) {
            // power overwhelming gcd removed but client not updated
            return 0;
        }

        return duration;
    }

    uint32_t GetRemainingGcdOrCooldownForSpell(uint32_t spellId) {
        uint32_t duration;
        uint64_t startTime;
        uint32_t enable;

        auto const getSpellCooldown = reinterpret_cast<Spell_C_GetSpellCooldownT>(Offsets::Spell_C_GetSpellCooldown);
        getSpellCooldown(spellId, 0, &duration, &startTime, &enable);
        startTime = startTime & 0XFFFFFFFF; // only look at same bits that lua does

        if (startTime != 0) {
            auto currentLuaTime = GetWowTimeMs() & 0XFFFFFFFF;
            auto remaining = (startTime + duration) - currentLuaTime;
            return uint32_t(remaining);
        }

        return 0;
    }

    uint32_t GetRemainingCooldownForSpell(uint32_t spellId) {
        uint32_t duration;
        uint64_t startTime;
        uint32_t enable;

        auto const getSpellCooldown = reinterpret_cast<Spell_C_GetSpellCooldownT>(Offsets::Spell_C_GetSpellCooldown);
        getSpellCooldown(spellId, 0, &duration, &startTime, &enable);

        startTime = startTime & 0XFFFFFFFF; // only look at same bits that lua does

        // ignore gcd cooldown by looking for duration > 1.5
        if (startTime != 0 && duration > 1.5) {
            auto currentLuaTime = GetWowTimeMs() & 0XFFFFFFFF;
            auto remaining = (startTime + duration) - currentLuaTime;
            return uint32_t(remaining);
        }

        return 0;
    }

    bool IsSpellOnCooldown(uint32_t spellId) {
        uint32_t duration;
        uint64_t startTime;
        uint32_t enable;

        auto const getSpellCooldown = reinterpret_cast<Spell_C_GetSpellCooldownT>(Offsets::Spell_C_GetSpellCooldown);
        getSpellCooldown(spellId, 0, &duration, &startTime, &enable);

        return startTime != 0 && duration > 1.5;
    }

    char *ConvertGuidToString(uint64_t guid) {
        char *guidStr = new char[21]; // 2 for 0x prefix, 18 for the number, and 1 for '\0'
        std::snprintf(guidStr, 21, "0x%016llX", static_cast<unsigned long long>(guid));
        return guidStr;
    }

    void PushGuidString(uintptr_t *luaState, uint64_t guid) {
        char guidStr[21] = {0};
        std::snprintf(guidStr, sizeof(guidStr), "0x%016llX", static_cast<unsigned long long>(guid));
        lua_pushstring(luaState, guidStr);
    }

    bool IsReadableMemory(const void *ptr, size_t size) {
        if (!ptr || size == 0) {
            return false;
        }

        auto current = reinterpret_cast<uintptr_t>(ptr);
        auto remaining = size;
        while (remaining > 0) {
            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(current), &mbi, sizeof(mbi)) == 0) {
                return false;
            }

            if (mbi.State != MEM_COMMIT) {
                return false;
            }

            if ((mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
                return false;
            }

            auto regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (regionEnd <= current) {
                return false;
            }

            auto chunk = static_cast<size_t>(regionEnd - current);
            if (chunk > remaining) {
                chunk = remaining;
            }

            remaining -= chunk;
            current += chunk;
        }

        return true;
    }

    bool SafeReadMemory(const void *address, void *out, size_t size) {
        if (!out || size == 0) {
            return false;
        }

        if (!IsReadableMemory(address, size)) {
            return false;
        }

        std::memcpy(out, address, size);
        return true;
    }

    uint64_t GetUnitGuidFromString(const char *unitToken) {
        if (!unitToken) {
            return 0;
        }

        // Check if it's a GUID string (starts with "0x" or "0X")
        if (strncmp(unitToken, "0x", 2) == 0 || strncmp(unitToken, "0X", 2) == 0) {
            return std::stoull(unitToken, nullptr, 16);
        } else {
            // Get GUID from unit token
            auto const getGUIDFromName = reinterpret_cast<GetGUIDFromNameT>(Offsets::GetGUIDFromName);
            return getGUIDFromName(unitToken);
        }
    }

    uint64_t GetUnitGuidFromLuaParam(uintptr_t *luaState, int paramIndex) {
        if (lua_isnumber(luaState, paramIndex)) {
            // Parameter is a GUID number
            return static_cast<uint64_t>(lua_tonumber(luaState, paramIndex));
        } else if (lua_isstring(luaState, paramIndex)) {
            // Parameter is a unit token or GUID string
            const char *unitToken = lua_tostring(luaState, paramIndex);
            return GetUnitGuidFromString(unitToken);
        }
        return 0;
    }

    float GetNameplateDistance() {
        auto const distanceSquared = *reinterpret_cast<float *>(Offsets::NameplateDistance);
        return sqrtf(distanceSquared);
    }

    void SetNameplateDistance(float distance) {
        *reinterpret_cast<float *>(Offsets::NameplateDistance) = distance * distance;
    }

    bool IsTargetingTerrainSpell() {
        const auto s_needTargets = *reinterpret_cast<uint32_t *>(Offsets::SpellNeedsTargets);
        return (s_needTargets & game::SpellCastTargetFlags::TARGET_FLAG_SOURCE_LOCATION) ||
               (s_needTargets & game::SpellCastTargetFlags::TARGET_FLAG_DEST_LOCATION);
    }
}
