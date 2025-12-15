//
// Created by pmacc on 9/21/2024.
//

#pragma once

#include "game.hpp"
#include "main.hpp"

namespace Nampower {
    // Lua function pointers
    extern lua_errorT lua_error;
    extern lua_gettopT lua_gettop;
    extern lua_isstringT lua_isstring;
    extern lua_isnumberT lua_isnumber;
    extern lua_tostringT lua_tostring;
    extern lua_tonumberT lua_tonumber;
    extern lua_pushnumberT lua_pushnumber;
    extern lua_pushstringT lua_pushstring;
    extern lua_pushbooleanT lua_pushboolean;
    extern lua_pushnilT lua_pushnil;
    extern lua_newtableT lua_newtable;
    extern lua_settableT lua_settable;

    uint32_t GetSpellSlotAndTypeForName(const char *spellName, uint32_t *spellType);

    uint32_t GetSpellIdFromSpellName(const char *spellName);

    uint32_t ConvertSpellIdToSpellSlot(uint32_t spellId, uint32_t bookType);

    bool SpellIsOnGcd(const game::SpellRec *spell);

    bool SpellIsChanneling(const game::SpellRec *spell);

    bool SpellIsTargeting(const game::SpellRec *spell);

    bool SpellIsOnSwing(const game::SpellRec *spell);

    bool SpellIsAttackTradeskillOrEnchant(const game::SpellRec *spell);

    bool IsTargetingTerrainSpell();

    uint32_t GetGcdOrCooldownForSpell(uint32_t spellId);

    uint32_t GetRemainingGcdOrCooldownForSpell(uint32_t spellId);

    uint32_t GetRemainingCooldownForSpell(uint32_t spellId);

    bool IsSpellOnCooldown(uint32_t spellId);

    char *ConvertGuidToString(uint64_t guid);

    uint64_t GetUnitGuidFromLuaParam(uintptr_t *luaState, int paramIndex);

    uint64_t GetUnitGuidFromString(const char *unitToken);

    float GetNameplateDistance();

    void SetNameplateDistance(float distance);
}