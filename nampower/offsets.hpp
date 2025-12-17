/*
    Copyright (c) 2017-2023, namreeb (legal@namreeb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    The views and conclusions contained in the software and documentation are those
    of the authors and should not be interpreted as representing official policies,
    either expressed or implied, of the FreeBSD Project.
*/

#pragma once

#include <cstdint>

enum class Offsets : std::uint32_t {
    StartSpellVisualCheck = 0X006E79B5,

    ClntObjMgrObjectPtr = 0x00468460,
    GetObjectPtr = 0x464870,
    GetActivePlayer = 0x468550,
    GetUnitFromName = 0x00515940,
    GetDurationObject = 0x00C0D828,
    GetCastingTimeIndex = 0x2D,
    Language = 0xC0E080,
    SpellDb = 0xC0D780,
    CursorMode = 0xBE2C4C,
    CursorType = 0X00BE2C2C,
    SpellIsTargeting = 0xCECAC0,
    CastingItemIdPtr = 0X00CECAB0,
    CastingSpellId = 0xCECA88,
    AutoRepeatingSpellId = 0xCEAC30,
    SpellNeedsTargets = 0X00CECAC0,
    VisualSpellId = 0X00CEAC58,
    CasterGuid = 0X00CEAC50,
    BankGuid = 0X00BDD038,
    GetSpellSlotAndType = 0X004B3950,
    GetSpellSlotFromLua = 0X004B3EC0,
    OsGetAsyncTimeMs = 0X0042B790,
    ChannelTargetGuid = 0xC4D980,
    NameplateDistance = 0xC4D988,  // float containing the distance squared ready to be pythagorean'd
    DBCacheGetRecord = 0X0055BA30,

    ItemDBCache = 0xC0E2A0,

    CGSpellBook_mKnownSpells = 0xB700F0,
    CGSpellBook_mKnownPetSpells = 0XB6F098,
    CGSpellBook_CastSpell = 0x004b3300,

    CGActionBar_UseAction = 0x004E5EE0,
    CGActionBar_mSlotActions = 0X004E5F52,
    GetSpellIdFromAction = 0x004E5A50,

    IsSpellInRangeOfUnit = 0X004E56F0,
    CancelSpell = 0x6E4940,
    CancelAutoRepeatSpell = 0X006EA080,
    SpellDelayed = 0x6E74F0,
    SignalEvent = 0x703E50,
    SignalEventParam = 0x703F50,
    ISceneEndPtr = 0x5A17A0,
    SpellStart = 0X006E7700,
    SpellGo = 0x006E7A70,
    CooldownEvent = 0x006E9670,
    SendCast = 0x6E54F0,
    CreateCastbar = 0x6E7A53,
    CheckAndReportSpellInhibitFlags = 0x006094f0,
    SpellHistories = 0X00CECAEC,

    LockedTargetGuid = 0x00B4E2D8,
    OnSpriteRightClick = 0x00492820,
    CGGameUI_Target = 0X00493540,
    ScriptTargetUnit = 0X00489A40,

    SpellVisualsHandleCastStart = 0X006EC220,
    PlaySpellVisualHandler = 0X006E98D0,
    PlaySpellVisual = 0X0060EDF0,

    SpellChannelStartHandler = 0x006E7550,
    SpellChannelUpdateHandler = 0x006E75F0,
    SpellFailedHandler = 0x006E8D80,
    SpellFailedOtherHandler = 0X006E8E40,
    CastResultHandler = 0x006E7330,
    SpellStartHandler = 0x006E7640,
    SpellCooldownHandler = 0X006E9460,
    PeriodicAuraLogHandler = 0X00626DD0,
    SpellNonMeleeDmgLogHandler = 0X005E85E0,

    Spell_C_GetCastTime = 0X006E3340,
    Spell_C_CastSpell = 0x6E4B60,
    Spell_C_CooldownEventTriggered = 0X006E3050,
    Spell_C_SpellFailed = 0x006E1A00,
    Spell_C_CastSpellByID = 0x6E5A90,
    Spell_C_GetAutoRepeatingSpell = 0x006E9FD0,
    Spell_C_HandleSpriteClick = 0x006E5B10,
    Spell_C_HandleTerrainClick = 0x006E60F0,
    CGWorldFrame_OnLayerTrackTerrain = 0x004820F0,
    Spell_C_TargetSpell = 0x006E5250,
    Spell_C_GetSpellCooldown = 0X006E2EA0,
    Spell_C_IsSpellUsable = 0X006E3D60,
    Spell_C_GetDuration = 0X006EA000,
    Spell_C_GetSpellRadius = 0x006E6350,
    Spell_C_ApplySpellModifiers = 0x006e6af0,
    Spell_C_GetSpellModifierValues = 0x006e6b30,

    CVarLookup = 0x0063DEC0,
    RegisterCVar = 0X0063DB90,

    GetClientConnection = 0X005AB490,
    GetNetStats = 0X00537F20,
    ClientServices_Send = 0X005AB630,

    LoadScriptFunctions = 0x00490250,
    FrameScript_RegisterFunction = 0x00704120,
    FrameScript_CreateEvents = 0X00703D90,
    Framescript_SetEventCount = 0X007053B0,
    Framescript_EventObject_Data = 0x00CEEF68,

    // Existing script functions
    GetGUIDFromName = 0X00515970,
    Script_CastSpellByName = 0x004B4AB0,
    Script_SpellTargetUnit = 0x006E6D90,
    Script_SetCVar = 0x00488C10,
    Script_RunScript = 0x0048B980,
    Script_SpellStopCasting = 0x006E6E80,

    SStrDupA = 0X0064A620,

    lua_state_ptr = 0x7040D0,

    lua_isstring = 0x006F3510,
    lua_isnumber = 0X006F34D0,
    lua_tostring = 0x006F3690,
    lua_tonumber = 0X006F3620,
    lua_pushnumber = 0X006F3810,
    lua_gettable = 0X6F3A40,
    lua_pushstring = 0X006F3890,
    lua_pushnil = 0X006F37F0,
    lua_call = 0x00704CD0,
    lua_pcall = 0x006F41A0,
    lua_error = 0X006F4940,
    lua_settop = 0X006F3080,
    lua_newtable = 0x006F3C90,
    lua_settable = 0x006F3E20,
    lua_pushboolean = 0x006F39F0,
    lua_rawseti = 0x006f3f60,
    lua_rawgeti = 0x006f3bc0,
    lua_gettop = 0x006F3070,
    luaL_ref = 0x006F5310,
    luaL_unref = 0x006F5400,


    CGInputControlGetActive = 0XBE1148,
    LastHardwareAction = 0X00CF0BC8,
    CGInputControlSetReleaseAction = 0x514810,
    CGInputControlSetControlBit = 0x515090,

    RangeCheckSelected = 0x6E4440,

    SpellVisualsInitialize = 0x006ec0e0,

    IntIntParamFormat = 0X00843342,
    StringIntParamFormat = 0X00847FBC,

    QueueEventStringPtr = 0X00BE175C, // unused event 369 0x171
    CastEventStringPtr = 0X00BE1A08, // unused event 540 0x21C

    GetBuffByIndex = 0X004E4430,

    CGUnit_C_ClearCastingSpell = 0x0060d040,
    CGUnit_C_ClearSpellEffect = 0x00614150,
    CGUnit_C_GetEquippedItemAtSlot = 0x005f0d60,
    CGBag_C_GetItemAtSlot = 0x006228a0,
    GetContainerGuid = 0x004f93e0,
    CanInspectUnit = 0x004944a0,
    CGUnit_C_OnAuraRemoved = 0x00612320,
    CGUnit_C_OnAuraAdded = 0x006123f0,
    CGUnit_C_OnAuraAddedStack = 0x0062b800,
    UnitCombatLogUnitDead = 0x0062c160,

    CGPlayer_C_OnAttackIconPressed = 0X006131A0,

    DBCache_ItemCacheDBGetRow = 0x0055BA30,

    InvalidFunctionPtrCheck = 0x0042A320,

    ClearCursor = 0x00495190,
    CursorSetCursorMode = 0x00523d20,
    CursorModelSetSequence = 0x00523c20,
};
