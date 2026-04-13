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
    CGlueMgr_DefaultServerLogin = 0x0046AFB0,
    GetUnitFromName = 0x00515940,
    GetNamesFromGUID = 0x00000000, // TODO: set correct offset
    SendUnitSignal = 0x00515e50,
    GetDurationObject = 0x00C0D828,
    GetCastingTimeIndex = 0x2D,
    Language = 0xC0E080,
    SpellDb = 0xC0D780,
    CursorMode = 0xBE2C4C,
    CursorType = 0X00BE2C2C,
    SpellNeedTargets = 0xCECAC0,
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
    NameplateDistanceSq = 0xC4D988, // float containing the distance squared ready to be pythagorean'd
    ChatBubbleDistanceSq = 0x00806798,
    DBCacheGetRecord = 0X0055BA30,
    InvQuestionMark = 0X00847FE4,

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
    CGGameUI_HandleObjectTrackChange = 0x00492890,
    CGGameUI_Target = 0X00493540,
    CGGameUI_ShowCombatFeedback = 0x00494600,
    CGUnit_C_HandleEnvironmentDamage = 0x00624f30,
    UnitCombatLogDamageShield = 0x0062ca20,
    ScriptTargetUnit = 0X00489A40,

    MouseoverGuidLow = 0x00B4E2C8,
    MouseoverGuidHigh = 0x00B4E2CC,
    NpcGuidLow = 0x00B4E2D0,
    NpcGuidHigh = 0x00B4E2D4,

    CGGameUI_GetPartyMember = 0X004E81A0,
    CGGameUI_GetPartyMemberPet = 0X004E81D0,
    CGGameUI_GetRaidMember = 0X00491940,
    CGGameUI_GetRaidMemberPet = 0X00491960,
    CGGameUI_GetPartyOrRaidMember = 0x00496400,
    CGGameUI_GetPartyOrRaidPet = 0x00496420,

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
    SpellHealingLogHandler = 0x005E89C0,
    SpellEnergizeLogHandler = 0x005E8A90,
    ProcResistHandler = 0x006289A0,
    SpellLogMissHandler = 0x005E7E00,
    SpellOrDamageImmuneHandler = 0x005E8CA0,
    UnitCombatLogDispelled = 0x0062d480,

    Spell_C_GetCastTime = 0X006E3340,
    Spell_C_CastSpell = 0x6E4B60,
    Spell_C_CooldownEventTriggered = 0X006E3050,
    Spell_C_SpellFailed = 0x006E1A00,
    Spell_C_CastSpellByID = 0x6E5A90,
    Spell_C_GetAutoRepeatingSpell = 0x006E9FD0,
    Spell_C_HandleSpriteClick = 0x006E5B10,
    Spell_C_HandleTerrainClick = 0x006E60F0,
    Spell_C_CancelAura = 0x006E7040,
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
    SLogFlush = 0x0065A970,
    CombatLogFileHandle = 0x00B50544,

    GetClientConnection = 0X005AB490,
    GetNetStats = 0X00537F20,
    ClientServices_Send = 0X005AB630,

    Player_LoadScriptFunctions = 0x00490250,
    Glue_LoadScriptFunctions = 0x0046ABB0,
    FrameScript_RegisterFunction = 0x00704120,
    FrameScript_Object_RegisterScriptEvent = 0x00702140,
    FrameScript_CreateEvents = 0X00703D90,
    Framescript_SetEventCount = 0X007053B0,
    Framescript_EventObject_Data = 0x00CEEF68,

    FrameScript_Execute = 0x00704CF0,
    FrameScript_ExecuteFile = 0x00704BC0,

    // Existing script functions
    GetGUIDFromName = 0X00515970,
    Script_CastSpell = 0x004B42F0,
    Script_CastSpellByName = 0x004B4AB0,
    Script_SetRaidTarget = 0x004BBEC0,
    Script_SpellTargetUnit = 0x006E6D90,
    Script_SetCVar = 0x00488C10,
    Script_RunScript = 0x0048B980,
    Script_SpellStopCasting = 0x006E6E80,
    Script_IsAltKeyDown = 0x0041F8F0,
    Script_UseInventoryItem = 0x004c8de0,
    Script_UseContainerItem = 0x004fa0e0,

    SStrDupA = 0X0064A620,

    lua_state_ptr = 0x7040D0,

    lua_type = 0X006F3400,
    lua_isstring = 0x006F3510,
    lua_isnumber = 0X006F34D0,
    lua_toflag = 0X006F1C10,
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
    lua_touserdata = 0x006F3740,
    lua_gettop = 0x006F3070,
    luaL_ref = 0x006F5310,
    luaL_unref = 0x006F5400,

    CGInputControlGetActive = 0XBE1148,
    UpdateSyncKeyState = 0x00424810,
    ClientEventContext = 0x00882670,
    LastHardwareAction = 0X00CF0BC8,
    CGInputControlSetReleaseAction = 0x514810,
    CGInputControlSetControlBit = 0x515090,

    RangeCheckSelected = 0x6E4440,

    SpellVisualsInitialize = 0x006ec0e0,
    WowSysMessageOutputInitialize = 0x00409040,

    UnitCombatParamFormat = 0x0084333c,
    IntIntParamFormat = 0X00843342,
    StringIntParamFormat = 0X00847FBC,
    StringParamFormat = 0X0082E280,

    QueueEventStringPtr = 0X00BE175C, // unused event 369 0x171
    CastEventStringPtr = 0X00BE1A08, // unused event 540 0x21C

    CGBuffBar__m_buffs = 0X00BC6040,
    CGBuffBar_UpdateDuration = 0x004E4390,
    GetBuffByIndex = 0X004E4430,

    CGUnit_C_AddChatBubble = 0x00608ac0,
    CGChat_GetChatColor = 0x0049e990,
    CGUnit_RemoveChatBubble = 0x00608c00,
    CGChatBubbleFrame_GetNewChatBubble = 0x004b1870,
    CGChatBubbleFrame_Initialize = 0x004b1600,
    OnChatMessagePatchCallSite = 0x0049DB9F,
    CurrentFramePointer = 0x00b4b2bc,
    CGUnit_C_ClearCastingSpell = 0x0060d040,
    CGUnit_C_ClearSpellEffect = 0x00614150,
    CGUnit_C_GetEquippedItemAtSlot = 0x005f0d60,
    CGBag_C_GetItemAtSlot = 0x006228a0,
    GetContainerGuid = 0x004f93e0,
    CanInspectUnit = 0x004944a0,
    CGUnit_C_CanInteract = 0x00606ba0,
    CGUnit_C_NamePlateUpdateRaidTarget = 0x00608a90,
    CGUnit_C_OnAuraRemoved = 0x00612320,
    CGUnit_C_OnAuraAdded = 0x006123f0,
    CGUnit_C_OnAuraStacksChanged = 0x00612450,
    UnitCombatLogUnitDead = 0x0062c160,
    CGUnitGetUnitName = 0x00609210,
    CGUnitCanAttack = 0x00606980,

    CGPlayer_C_OnAttackIconPressed = 0X006131A0,

    AttackRoundInfo_ReadPacket = 0x00625c60,

    CGCharacterInfo_UseItem = 0x004c7970,

    CGItem_C_Use = 0x005D8D00,
    CGItem_C_GetInventoryArt = 0x005D88B0,
    CGItem_C_ItemIsQuestOrSoulbound = 0x005DA2C0,

    SpellRangeDBMaxId = 0x00c0d7a0,
    SpellRangeDBArray = 0x00c0d79c,

    SpellIconDBMaxId = 0x00c0d7f0,
    SpellIconDBArray = 0x00c0d7ec,

    DBCache_ItemCacheDBGetRow = 0x0055BA30,

    InvalidFunctionPtrCheck = 0x0042A320,

    ClearCursor = 0x00495190,
    CursorSetCursorMode = 0x00523d20,
    CursorModelSetSequence = 0x00523c20,
    CGGameUI_DisplayError = 0x00496720,

    CSimpleTop_OnKeyDown = 0x00765f10,
    CSimpleTop_OnKeyUp = 0x00765fd0,

    CSimpleFrame_GetName = 0x007a1390,

    XMLNode_GetAttributeByName = 0x006f2cf0,

    CGRaidInfo_m_raidtargets = 0xB71368,
    CGRaidInfo_m_numRaidMembers = 0x00B713E0,
    CGGameUI_IsPartyMember = 0x004E7F70,
    CGPartyInfo_NumMembers = 0x004E86D0,

    CGPetInfo_SetPet = 0x004bc7e0,
    TogglePetSlotAutocast = 0x004bcbb0,
    CGPetInfo_GetPetSpellAction = 0x004bd190,
    CGPetInfo_SendPetAction = 0x004bd1d0,
    CGTooltip_SetItem = 0x0052B650,
    SpellParserParseText = 0x005075F0,
    PetActionBarSlots = 0x00b71438,
    PetReactionMode = 0x00b71468,
    ActivePetGuid = 0x00b714a0,
    
    QuestLogEntries = 0x00BB71C0,
    QuestLogEntryCount = 0x00BB7478,
    QuestDialogState = 0x00BE0818,
    QuestDialogCurrentQuestId = 0x00BE081C,
};
