# Nampower Custom Events

This document describes all custom events added by Nampower that you can register and listen to in your addons.

For custom Lua functions, see [SCRIPTS.md](SCRIPTS.md). For general usage information, see [README.md](README.md).

> **Note on examples:** The examples in this document use the **Ace2/Ace3 event format** where event parameters are passed directly as function arguments. In vanilla WoW's native event system, parameters are instead available as global variables `arg1`, `arg2`, `arg3`, etc.
>
> **Ace format (used in examples):**
> ```lua
> frame:RegisterEvent("SPELL_MISS_SELF", function(casterGuid, targetGuid, spellId, missInfo)
>     -- args passed directly
> end)
> ```
>
> **Vanilla native format:**
> ```lua
> local frame = CreateFrame("Frame")
> frame:RegisterEvent("SPELL_MISS_SELF")
> frame:SetScript("OnEvent", function()
>     -- use arg1, arg2, arg3, arg4 instead
>     local casterGuid = arg1
>     local targetGuid = arg2
>     local spellId = arg3
>     local missInfo = arg4
> end)
> ```

## Table of Contents
- [SPELL_QUEUE_EVENT](#spell_queue_event)
- [SPELL_CAST_EVENT](#spell_cast_event)
- [SPELL_START_SELF and SPELL_START_OTHER](#spell_start_self-and-spell_start_other)
- [SPELL_GO_SELF and SPELL_GO_OTHER](#spell_go_self-and-spell_go_other)
- [SPELL_FAILED_SELF and SPELL_FAILED_OTHER](#spell_failed_self-and-spell_failed_other)
- [SPELL_DELAYED_SELF and SPELL_DELAYED_OTHER](#spell_delayed_self-and-spell_delayed_other)
- [SPELL_CHANNEL_START and SPELL_CHANNEL_UPDATE](#spell_channel_start-and-spell_channel_update)
- [SPELL_DAMAGE_EVENT_SELF and SPELL_DAMAGE_EVENT_OTHER](#spell_damage_event_self-and-spell_damage_event_other)
- [Buff/Debuff Events](#buffdebuff-events)
- [BUFF_UPDATE_DURATION_SELF and DEBUFF_UPDATE_DURATION_SELF](#buff_update_duration_self-and-debuff_update_duration_self)
- [AURA_CAST_ON_SELF and AURA_CAST_ON_OTHER](#aura_cast_on_self-and-aura_cast_on_other)
- [AUTO_ATTACK_SELF and AUTO_ATTACK_OTHER](#auto_attack_self-and-auto_attack_other)
- [SPELL_HEAL_BY_SELF, SPELL_HEAL_BY_OTHER, and SPELL_HEAL_ON_SELF](#spell_heal_by_self-spell_heal_by_other-and-spell_heal_on_self)
- [SPELL_ENERGIZE_BY_SELF, SPELL_ENERGIZE_BY_OTHER, and SPELL_ENERGIZE_ON_SELF](#spell_energize_by_self-spell_energize_by_other-and-spell_energize_on_self)
- [SPELL_MISS_SELF and SPELL_MISS_OTHER](#spell_miss_self-and-spell_miss_other)
- [UNIT_DIED](#unit_died)
- [Unit GUID Events](#unit-guid-events)

---

## Custom Events

### SPELL_QUEUE_EVENT
I've added a new event you can register in game to get updates when spells are added and popped from the queue.

The event is `SPELL_QUEUE_EVENT` and has 2 parameters:
1.  int eventCode - see below
2.  int spellId

Possible Event codes:
```
 ON_SWING_QUEUED = 0
 ON_SWING_QUEUE_POPPED = 1
 NORMAL_QUEUED = 2
 NORMAL_QUEUE_POPPED = 3
 NON_GCD_QUEUED = 4
 NON_GCD_QUEUE_POPPED = 5
```

Example from NampowerSettings:
```
local ON_SWING_QUEUED = 0
local ON_SWING_QUEUE_POPPED = 1
local NORMAL_QUEUED = 2
local NORMAL_QUEUE_POPPED = 3
local NON_GCD_QUEUED = 4
local NON_GCD_QUEUE_POPPED = 5

local function spellQueueEvent(eventCode, spellId)
	if eventCode == NORMAL_QUEUED or eventCode == NON_GCD_QUEUED then
		local _, _, texture = SpellInfo(spellId) -- superwow function
		Nampower.queued_spell.texture:SetTexture(texture)
		Nampower.queued_spell:Show()
	elseif eventCode == NORMAL_QUEUE_POPPED or eventCode == NON_GCD_QUEUE_POPPED then
		Nampower.queued_spell:Hide()
	end
end

NampowerSettings:RegisterEvent("SPELL_QUEUE_EVENT", spellQueueEvent)
```

### SPELL_CAST_EVENT
Event you can register in game to get updates when you cast spells with some additional information.  This will only fire for spells you (and certain pets) initiated. Triggered when you start casting a spell in the client before it is sent to the server.

The event is `SPELL_CAST_EVENT` and has 5 parameters:
1.  int success - 1 if cast succeeded, 0 if failed
2.  int spellId
3.  int castType - see below
4.  string targetGuid - guid string like "0xF5300000000000A5"
5.  int itemId - the id of the item that triggered the spell, 0 if it wasn't triggered by an item

Possible Cast Types:
```
NORMAL=1
NON_GCD=2
ON_SWING=3
CHANNEL=4
TARGETING=5 (targeting is the term I used for spells with terrain targeting)
TARGETING_NON_GCD=6
```

targetGuid will be "0x000000000" unless an explicit target is specified which currently only happens in 2 circumstances:
- It was specified as the 2nd param of CastSpellByName (added by superwow)
- Mouseover casts that use SpellTargetUnit to specify a target

Example (uses ace RegisterEvent):
```
Cursive:RegisterEvent("SPELL_CAST_EVENT", function(success, spellId, castType, targetGuid, itemId)
	print(success)
	print(spellId)
	print(castType)
	print(targetGuid)
	print(itemId)
end);
```

### SPELL_START_SELF and SPELL_START_OTHER
Fire when a spell start packet is received. "Self" fires when the active player is the caster, "Other" fires for any other caster. Triggered by the server to notify that a spell with a cast time has begun. Note: SPELL_START_OTHER also fires for channeling spells cast by other players, as there is no separate SPELL_CHANNEL_START packet for other players — channeling and cast time spells are both bundled into SPELL_START. Use the `spellType` parameter to distinguish channeling spells.

These events are gated behind the `NP_EnableSpellStartEvents` CVar (default 0). Set it to `1` to enable.

Parameters:
1.  int itemId - the id of the item that triggered the spell, 0 if it wasn't triggered by an item
2.  int spellId
3.  string casterGuid - caster guid like "0xF5300000000000A5"
4.  string targetGuid - target guid like "0xF5300000000000A5" or "0x0000000000000000" if none
5.  int castFlags
6.  int castTime - cast time in milliseconds
7.  int duration - channel duration in milliseconds (only set for channeling spells, 0 otherwise.  See GetSpellDuration if you want it for other spell types)
8.  int spellType - 0 = Normal, 1 = Channeling, 2 = Autorepeating 

### SPELL_GO_SELF and SPELL_GO_OTHER
Fire when a spell go packet is received. "Self" fires when the active player is the caster, "Other" fires for any other caster. Triggered by the server to indicate a spell completed casting.

These events are gated behind the `NP_EnableSpellGoEvents` CVar (default 0). Set it to `1` to enable.

Parameters:
1.  int itemId - the id of the item that triggered the spell, 0 if it wasn't triggered by an item
2.  int spellId
3.  string casterGuid - caster guid like "0xF5300000000000A5"
4.  string targetGuid - target guid like "0xF5300000000000A5" or "0x0000000000000000" if none
5.  int castFlags
6.  int numTargetsHit
7.  int numTargetsMissed

#### CastFlags (bitmask)
```
CAST_FLAG_NONE = 0 (0x00000000)
CAST_FLAG_HIDDEN_COMBATLOG = 1 (0x00000001)
CAST_FLAG_UNKNOWN2 = 2 (0x00000002)
CAST_FLAG_UNKNOWN3 = 4 (0x00000004)
CAST_FLAG_UNKNOWN4 = 8 (0x00000008)
CAST_FLAG_UNKNOWN5 = 16 (0x00000010)
CAST_FLAG_AMMO = 32 (0x00000020)
CAST_FLAG_UNKNOWN7 = 64 (0x00000040)
CAST_FLAG_UNKNOWN8 = 128 (0x00000080)
CAST_FLAG_UNKNOWN9 = 256 (0x00000100)
```

### SPELL_FAILED_SELF and SPELL_FAILED_OTHER
Fire when a spell failure is reported. "Self" is fired from the client spell failure hook, "Other" is fired from the server handler (only includes caster guid and spell id).

Parameters for `SPELL_FAILED_SELF`:
1.  int spellId
2.  int spellResult - SpellCastResult enum value
3.  int failedByServer - 1 if failed by server, 0 otherwise

Parameters for `SPELL_FAILED_OTHER`:
1.  string casterGuid - caster guid like "0xF5300000000000A5"
2.  int spellId

### SPELL_DELAYED_SELF and SPELL_DELAYED_OTHER
Fire when a spell is delayed, generally due to taking damage. "Self" fires when the active player is affected, "Other" fires for other players.

**Note:** `SPELL_DELAYED_OTHER` currently does not work because the server only sends the spell delayed packet to the affected player. This means you will only ever receive `SPELL_DELAYED_SELF` events for your own spell pushbacks. Leaving `SPELL_DELAYED_OTHER` in case this changes in the future.

Parameters:
1.  string casterGuid - caster guid like "0xF5300000000000A5"
2.  int delayMs - delay in milliseconds

### SPELL_CHANNEL_START and SPELL_CHANNEL_UPDATE
Fire when the active player starts or updates a channeled spell. These are self-only events.

Channel target guid is read from `ChannelTargetGuid = 0xC4D980` and included in these events.

Parameters for `SPELL_CHANNEL_START`:
1.  int spellId
2.  string targetGuid - target guid like "0xF5300000000000A5" or "0x0000000000000000" if none
3.  int durationMs - channel duration in milliseconds

Parameters for `SPELL_CHANNEL_UPDATE`:
1.  int spellId
2.  string targetGuid - target guid like "0xF5300000000000A5" or "0x0000000000000000" if none
3.  int remainingMs - remaining channel time in milliseconds

### SPELL_DAMAGE_EVENT_SELF and SPELL_DAMAGE_EVENT_OTHER
New events you can register in game to get updates whenever spell damage occurs. SPELL_DAMAGE_EVENT_SELF will only trigger for damage you deal, while SPELL_DAMAGE_EVENT_OTHER will only trigger for damage dealt by others.

Both of these events have the following parameters:
1.  string targetGuid - guid string like "0xF5300000000000A5"
2.  string casterGuid - guid string like "0xF5300000000000A5"
3.  int spellId
4.  int amount - the amount of damage dealt.  If the 4th value in effectAuraStr is 89 (SPELL_AURA_PERIODIC_DAMAGE_PERCENT) I believe this is the percentage of health lost.
5.  string mitigationStr - comma separated string containing "aborb,block,resist" amounts
6.  int hitInfo - see below but generally 0 unless the spell was a crit in which case it will be 2
7.  int spellSchool - the damage school of the spell, see below
8.  string effectAuraStr - comma separated string containing the three spell effect numbers and the aura type (usually means a Dot but not all Dots will have an aura type) if applicable.  So "effect1,effect2,effect3,auraType"

Spell hit info enum: https://github.com/vmangos/core/blob/94f05231d4f1b160468744d4caa398cf8b337c48/src/game/Spells/SpellDefines.h#L109

Spell school enum:  https://github.com/vmangos/core/blob/94f05231d4f1b160468744d4caa398cf8b337c48/src/game/Spells/SpellDefines.h#L641

Spell effect enum: https://github.com/vmangos/core/blob/94f05231d4f1b160468744d4caa398cf8b337c48/src/game/Spells/SpellDefines.h#L142

Aura type enum: https://github.com/vmangos/core/blob/94f05231d4f1b160468744d4caa398cf8b337c48/src/game/Spells/SpellAuraDefines.h#L43

Example (uses ace RegisterEvent):
```
Cursive:RegisterEvent("SPELL_DAMAGE_EVENT_SELF",
    function(targetGuidStr,
             casterGuidStr,
             spellId,
             amount,
             mitigationStr,
             hitInfo,
             spellSchool,
             effectAuraStr)
        print(targetGuidStr .. " " .. casterGuidStr .. " " .. tostring(spellId) .. " " .. tostring(amount) .. " " .. tostring(spellSchool) .. " " .. mitigationStr .. " " .. hitInfo .. " " .. effectAuraStr)
    end);
```

### Buff/Debuff Events
New events fire whenever a buff/buff stack or debuff/debuff stack is added or removed on you or any other unit that the client tracks.

Events:
```
BUFF_ADDED_SELF
BUFF_REMOVED_SELF
BUFF_ADDED_OTHER
BUFF_REMOVED_OTHER
DEBUFF_ADDED_SELF
DEBUFF_REMOVED_SELF
DEBUFF_ADDED_OTHER
DEBUFF_REMOVED_OTHER
```

All eight events pass the same parameters:
1.  string guid - unit guid like "0xF5300000000000A5"
2.  int luaSlot - 1-based Lua slot index for the buff/debuff (skips empty slots to match UnitBuff/UnitDebuff ordering). This is the slot you would pass to UnitBuff/UnitDebuff.
3.  int spellId
4.  int stackCount - current stack count for the aura 
5.  int auraLevel - caster level for the aura from UnitFields.auraLevels (uint8 per slot, 48 entries)
6.  int auraSlot - the raw 0-based aura slot index (0-31 for buffs, 32-47 for debuffs). This is the raw internal slot, not the Lua slot. Consistent with unit data fields, GetPlayerAuraDuration, and BUFF/DEBUFF_UPDATE_DURATION_SELF events.
7.  int state - indicates why the event fired: `0` = newly added, `1` = newly removed, `2` = modified (stack change). When state is `2`, the event type (*_ADDED_* or *_REMOVED_*) reflects whether stacks increased or decreased.

Example:
```
local AURA_STATE_ADDED = 0
local AURA_STATE_REMOVED = 1
local AURA_STATE_MODIFIED = 2

local function onAuraEvent(eventName, guid, luaSlot, spellId, stacks, auraLevel, auraSlot, state)
    local stateNames = { [0] = "added", [1] = "removed", [2] = "modified" }
    DEFAULT_CHAT_FRAME:AddMessage(string.format("[%s] %s luaSlot=%d spell=%d stacks=%d level=%d auraSlot=%d state=%s", eventName, guid, luaSlot, spellId, stacks, auraLevel, auraSlot, stateNames[state] or "unknown"))
end

for _, eventName in ipairs({"BUFF_ADDED_SELF", "BUFF_REMOVED_SELF", "DEBUFF_ADDED_OTHER", "DEBUFF_REMOVED_OTHER"}) do
    frame:RegisterEvent(eventName, function(...) onAuraEvent(eventName, ...) end)
end
```

### BUFF_UPDATE_DURATION_SELF and DEBUFF_UPDATE_DURATION_SELF
Fire when the client updates the duration of a buff or debuff on the active player. This happens when the server refreshes an aura's duration (e.g., reapplying a buff that already exists). Only fires for the active player's own auras.

**Note:** This event fires before the aura is actually added to the unit fields, so for newly added auras the spellId will be 0. It will only have a value for auras that are being refreshed (i.e., the aura already exists in the slot). Use the BUFF/DEBUFF_ADDED_SELF events for tracking newly applied auras.

Events:
```
BUFF_UPDATE_DURATION_SELF    -- aura slot 0-31
DEBUFF_UPDATE_DURATION_SELF  -- aura slot 32-47
```

Parameters:
1.  int auraSlot - the raw 0-based aura slot index (0-31 for buffs, 32-47 for debuffs). This is not the slot used by GetPlayerBuff that has its own sorting, this is the raw aura slot index which is consistent with the unit data fields and the internal aura storage.
2.  int durationMs - the updated duration in milliseconds
3.  int expirationTimeMs - the calculated expiration time (GetWowTimeMs() + durationMs), or 0 if no duration
4.  int spellId - the spell ID of the aura in this slot. Will be 0 for newly added buffs/debuffs (the slot hasn't been populated yet). Will only have a value for refreshed auras that already exist in the slot.

Example:
```lua
local function onDurationUpdate(auraSlot, durationMs, expirationTimeMs, spellId)
    local isBuff = auraSlot < 32
    local type = isBuff and "Buff" or "Debuff"
    local refreshed = spellId > 0
    DEFAULT_CHAT_FRAME:AddMessage(string.format(
        "[%s] slot=%d duration=%dms expires=%d spellId=%d refreshed=%s",
        type, auraSlot, durationMs, expirationTimeMs, spellId, tostring(refreshed)
    ))
end

frame:RegisterEvent("BUFF_UPDATE_DURATION_SELF", onDurationUpdate)
frame:RegisterEvent("DEBUFF_UPDATE_DURATION_SELF", onDurationUpdate)
```

### AURA_CAST_ON_SELF and AURA_CAST_ON_OTHER
Fire when a spell cast applies an aura. "Self" covers casts that land on the active player (including cases where the active player is the caster with no explicit target); "Other" covers all other targets.

These events are gated behind the `NP_EnableAuraCastEvents` CVar (default 0). Set it to `1` to enable.
**Note:** some auras do not have spell effects and won't trigger these events; the BUFF/DEBUFF gain events are the only way to track those.

These events are primarily intended for basic tracking of aura applications when buff/debuff caps prevent normal GAINS events from firing.

#### Multi-Target Behavior
These events fire separately for each target affected by the spell:
- **Self-targeting spells** (e.g., buffs with TARGET_UNIT_CASTER) trigger once on the caster
- **Single-target spells** (e.g., spells that target specific units) trigger once on the primary target
- **AOE spells** (e.g., area buffs/debuffs) trigger once for each affected target based on the spell's implicit targeting rules

Each event fires once per qualifying spell effect per target. A spell with multiple aura-applying effects will trigger multiple events for the same target.

**Note:** There may be edge cases or bugs with certain spells where targeting logic doesn't work as expected. If you encounter issues with specific spells not firing events correctly or firing too many/too few times, please report them in the issues tracker.

Parameters:
1.  int spellId
2.  string casterGuid - caster guid like "0xF5300000000000A5"
3.  string targetGuid - target guid like "0xF5300000000000A5"
4.  int effect - aura-applying effect id (event fires once for each qualifying effect in the spell)
5.  int effectAuraName - corresponding entry from EffectApplyAuraName
6.  int effectAmplitude - EffectAmplitude entry for the selected aura effect
7.  int effectMiscValue - EffectMiscValue entry for the selected aura effect
8.  int durationMs - spell duration in milliseconds (includes client modifiers if you are the caster)
9.  int auraCapStatus - bitfield: 1 = buff bar full, 2 = debuff bar full (3 means both)

### AUTO_ATTACK_SELF and AUTO_ATTACK_OTHER
Fire when auto attack damage is processed. "Self" fires when the active player is the attacker; "Other" fires someone other than the active player is the attacker.

These events are gated behind the `NP_EnableAutoAttackEvents` CVar (default 0). Set it to `1` to enable.

These events provide detailed information about auto attack rounds including damage, hit type (critical, glancing, crushing), victim state (dodged, parried, blocked), and mitigation amounts.

Parameters:
1.  string attackerGuid - attacker guid like "0xF5300000000000A5"
2.  string targetGuid - target guid like "0xF5300000000000A5"
3.  int totalDamage - total damage dealt by the attack
4.  int hitInfo - bitfield containing hit flags (see below)
5.  int victimState - state of the victim after the attack (see below)
6.  int subDamageCount - number of damage components (usually 1, can be up to 3 for weapons with elemental damages and maybe other cases).  The sub-damage components are not provided in this event as they didn't seem that useful and limited to 9 parameters on events.
7.  int blockedAmount - amount of damage blocked
8.  int totalAbsorb - total damage absorbed across all sub-damage components
9.  int totalResist - total damage resisted across all sub-damage components

#### HitInfo Flags
Bitfield values that can be combined:
```
HITINFO_NORMALSWING = 0 (0x0)
HITINFO_UNK0 = 1 (0x1)
HITINFO_AFFECTS_VICTIM = 2 (0x2)
HITINFO_LEFTSWING = 4 (0x4)        -- Off-hand attack
HITINFO_UNK3 = 8 (0x8)
HITINFO_MISS = 16 (0x10)
HITINFO_ABSORB = 32 (0x20)
HITINFO_RESIST = 64 (0x40)
HITINFO_CRITICALHIT = 128 (0x80)
HITINFO_UNK8 = 256 (0x100)
HITINFO_UNK9 = 8192 (0x2000)
HITINFO_GLANCING = 16384 (0x4000)
HITINFO_CRUSHING = 32768 (0x8000)
HITINFO_NOACTION = 65536 (0x10000)
HITINFO_SWINGNOHITSOUND = 524288 (0x80000)
```

#### Victim States
```
VICTIMSTATE_UNAFFECTED = 0  -- Seen with HITINFO_MISS
VICTIMSTATE_NORMAL = 1
VICTIMSTATE_DODGE = 2
VICTIMSTATE_PARRY = 3
VICTIMSTATE_INTERRUPT = 4
VICTIMSTATE_BLOCKS = 5
VICTIMSTATE_EVADES = 6
VICTIMSTATE_IS_IMMUNE = 7
VICTIMSTATE_DEFLECTS = 8
```

Example:
```
local HITINFO_CRITICALHIT = 128  -- 0x80
local HITINFO_GLANCING = 16384   -- 0x4000
local HITINFO_CRUSHING = 32768   -- 0x8000

local function onAutoAttack(attackerGuid, targetGuid, totalDamage, hitInfo, victimState, subDamageCount, blockedAmount, totalAbsorb, totalResist)
    local isCrit = bit.band(hitInfo, HITINFO_CRITICALHIT) ~= 0
    local isGlancing = bit.band(hitInfo, HITINFO_GLANCING) ~= 0
    local isCrushing = bit.band(hitInfo, HITINFO_CRUSHING) ~= 0

    local hitType = "Normal"
    if isCrit then hitType = "Critical"
    elseif isGlancing then hitType = "Glancing"
    elseif isCrushing then hitType = "Crushing"
    end

    local victimStateNames = {
        [0] = "Unaffected", [1] = "Normal", [2] = "Dodged", [3] = "Parried",
        [4] = "Interrupted", [5] = "Blocked", [6] = "Evaded", [7] = "Immune", [8] = "Deflected"
    }

    DEFAULT_CHAT_FRAME:AddMessage(string.format(
        "%s hit for %d (%s) - State: %s, Absorbed: %d, Resisted: %d, Blocked: %d",
        hitType, totalDamage, attackerGuid, victimStateNames[victimState] or "Unknown",
        totalAbsorb, totalResist, blockedAmount
    ))
end

frame:RegisterEvent("AUTO_ATTACK_SELF", onAutoAttack)
frame:RegisterEvent("AUTO_ATTACK_OTHER", onAutoAttack)
```

### SPELL_HEAL_BY_SELF, SPELL_HEAL_BY_OTHER, and SPELL_HEAL_ON_SELF
Fire when a spell healing log is received from the server. These events let you track heals in real-time.

- `SPELL_HEAL_BY_SELF` - Fires when the active player is the caster (you healed someone)
- `SPELL_HEAL_BY_OTHER` - Fires when someone other than the active player is the caster (someone else healed someone)
- `SPELL_HEAL_ON_SELF` - Fires when the active player is the target (you received a heal)

Note that `SPELL_HEAL_BY_SELF` and `SPELL_HEAL_ON_SELF` can both fire for the same heal if you heal yourself.

These events are gated behind the `NP_EnableSpellHealEvents` CVar (default 0). Set it to `1` to enable.

Parameters (same for all three events):
1.  string targetGuid - guid of the heal target like "0xF5300000000000A5"
2.  string casterGuid - guid of the healer like "0xF5300000000000A5"
3.  int spellId - the spell ID that caused the heal
4.  int amount - the amount healed
5.  int critical - 1 if the heal was a critical, 0 otherwise
6.  int periodic - 1 if the heal came from a periodic aura, 0 otherwise

Example:
```lua
local function onHeal(targetGuid, casterGuid, spellId, amount, critical, periodic)
    local critText = critical == 1 and " (CRIT)" or ""
    local periodicText = periodic == 1 and " (PERIODIC)" or ""
    local spellName = GetSpellInfo(spellId) or "Unknown"
    DEFAULT_CHAT_FRAME:AddMessage(string.format(
        "%s healed %s for %d with %s%s%s",
        casterGuid, targetGuid, amount, spellName, critText, periodicText
    ))
end

frame:RegisterEvent("SPELL_HEAL_BY_SELF", onHeal)    -- Heals you cast
frame:RegisterEvent("SPELL_HEAL_BY_OTHER", onHeal)  -- Heals others cast
frame:RegisterEvent("SPELL_HEAL_ON_SELF", onHeal)   -- Heals you receive
```

### SPELL_ENERGIZE_BY_SELF, SPELL_ENERGIZE_BY_OTHER, and SPELL_ENERGIZE_ON_SELF
Fire when a spell energize log is received from the server (power restoration like mana, rage, energy gains).

- `SPELL_ENERGIZE_BY_SELF` - Fires when the active player is the caster (you restored power to someone)
- `SPELL_ENERGIZE_BY_OTHER` - Fires when someone other than the active player is the caster (someone else restored power)
- `SPELL_ENERGIZE_ON_SELF` - Fires when the active player is the target (you received power)

Note that `SPELL_ENERGIZE_BY_SELF` and `SPELL_ENERGIZE_ON_SELF` can both fire for the same energize if you restore power to yourself.

These events are gated behind the `NP_EnableSpellEnergizeEvents` CVar (default 0). Set it to `1` to enable.

Parameters (same for all three events):
1.  string targetGuid - guid of the power recipient like "0xF5300000000000A5"
2.  string casterGuid - guid of the caster like "0xF5300000000000A5"
3.  int spellId - the spell ID that caused the energize
4.  int powerType - the type of power restored (see below)
5.  int amount - the amount of power restored
6.  int periodic - 1 if the energize came from a periodic aura, 0 otherwise

#### Power Types
```
POWER_MANA      = 0  -- Mana
POWER_RAGE      = 1  -- Rage
POWER_FOCUS     = 2  -- Focus (hunter pets)
POWER_ENERGY    = 3  -- Energy
POWER_HAPPINESS = 4  -- Happiness (hunter pets)
POWER_HEALTH    = -2 -- Health (0xFFFFFFFE as unsigned)
```

Example:
```lua
local POWER_NAMES = {
    [0] = "Mana",
    [1] = "Rage",
    [2] = "Focus",
    [3] = "Energy",
    [4] = "Happiness",
}

local function onEnergize(targetGuid, casterGuid, spellId, powerType, amount, periodic)
    local powerName = POWER_NAMES[powerType] or "Unknown"
    local spellName = GetSpellInfo(spellId) or "Unknown"
    local periodicText = periodic == 1 and " (PERIODIC)" or ""
    DEFAULT_CHAT_FRAME:AddMessage(string.format(
        "%s restored %d %s to %s with %s%s",
        casterGuid, amount, powerName, targetGuid, spellName, periodicText
    ))
end

frame:RegisterEvent("SPELL_ENERGIZE_BY_SELF", onEnergize)    -- Power you restore
frame:RegisterEvent("SPELL_ENERGIZE_BY_OTHER", onEnergize)  -- Power others restore
frame:RegisterEvent("SPELL_ENERGIZE_ON_SELF", onEnergize)   -- Power you receive
```

### SPELL_MISS_SELF and SPELL_MISS_OTHER
Fire when a spell miss or resist occurs. `SPELL_MISS_SELF` fires when the active player is the caster, `SPELL_MISS_OTHER` fires when someone else is the caster.

These events are triggered by `SMSG_SPELL_GO` (missed targets in spell go), `SMSG_SPELLLOGMISS` (miss, dodge, parry, etc.), `SMSG_PROCRESIST` (resist on proc effects), and `SMSG_SPELLORDAMAGE_IMMUNE` (immune).

Parameters:
1.  string casterGuid - guid of the caster like "0xF5300000000000A5"
2.  string targetGuid - guid of the target like "0xF5300000000000A5"
3.  int spellId - the spell ID that missed
4.  int missInfo - the type of miss (see SpellMissInfo below)

#### SpellMissInfo
```
SPELL_MISS_NONE    = 0   -- No miss (shouldn't normally appear)
SPELL_MISS_MISS    = 1   -- Miss
SPELL_MISS_RESIST  = 2   -- Resist (also used for SMSG_PROCRESIST)
SPELL_MISS_DODGE   = 3   -- Dodge
SPELL_MISS_PARRY   = 4   -- Parry
SPELL_MISS_BLOCK   = 5   -- Block
SPELL_MISS_EVADE   = 6   -- Evade
SPELL_MISS_IMMUNE  = 7   -- Immune
SPELL_MISS_IMMUNE2 = 8   -- Immune (variant)
SPELL_MISS_DEFLECT = 9   -- Deflect
SPELL_MISS_ABSORB  = 10  -- Absorb
SPELL_MISS_REFLECT = 11  -- Reflect
```

Example:
```lua
local MISS_NAMES = {
    [0] = "None", [1] = "Miss", [2] = "Resist", [3] = "Dodge",
    [4] = "Parry", [5] = "Block", [6] = "Evade", [7] = "Immune",
    [8] = "Immune", [9] = "Deflect", [10] = "Absorb", [11] = "Reflect",
}

local function onSpellMiss(casterGuid, targetGuid, spellId, missInfo)
    local missName = MISS_NAMES[missInfo] or "Unknown"
    DEFAULT_CHAT_FRAME:AddMessage(string.format(
        "Spell %d on %s: %s", spellId, targetGuid, missName
    ))
end

frame:RegisterEvent("SPELL_MISS_SELF", onSpellMiss)    -- Your spells that missed
frame:RegisterEvent("SPELL_MISS_OTHER", onSpellMiss)  -- Others' spells that missed
```

### UNIT_DIED
Fires when a unit death is recorded in the combat log.

Parameters:
1.  string guid - guid of the unit that died

Example:
```
frame:RegisterEvent("UNIT_DIED", function(guid)
    DEFAULT_CHAT_FRAME:AddMessage("Unit died: " .. guid)
end)
```

---

## Unit GUID Events

These events fire once per unit state change, identified by GUID rather than unit token. Unlike the standard WoW unit events (e.g. `UNIT_HEALTH`) which fire per registered token (player, target, party1, etc.), each GUID event fires exactly once and carries flags indicating which tokens the unit currently matches.

All GUID events share the same parameter format:
1.  string guid - unit guid like `"0xF5300000000000A5"`
2.  int isPlayer - 1 if the unit is the active player, 0 otherwise
3.  int isTarget - 1 if the unit is the current locked target, 0 otherwise
4.  int isMouseover - 1 if the unit is the current mouseover, 0 otherwise
5.  int isPet - 1 if the unit is the active player's pet, 0 otherwise
6.  int partyIndex - party slot (1–4) if the unit is a party member, 0 otherwise
7.  int raidIndex - raid slot (1–40) if the unit is a raid member, 0 otherwise

Events (8-parameter format):
```
UNIT_HEALTH_GUID         -- unit health changed
UNIT_MANA_GUID           -- unit mana changed
UNIT_RAGE_GUID           -- unit rage changed
UNIT_ENERGY_GUID         -- unit energy changed
UNIT_PET_GUID            -- unit pet assignment changed
UNIT_FLAGS_GUID          -- unit flags changed
UNIT_AURA_GUID           -- unit aura set changed
UNIT_DYNAMIC_FLAGS_GUID  -- unit dynamic flags changed
UNIT_NAME_UPDATE_GUID        -- unit name changed
UNIT_PORTRAIT_UPDATE_GUID    -- unit portrait changed
UNIT_MODEL_CHANGED_GUID      -- unit model changed
UNIT_INVENTORY_CHANGED_GUID  -- unit inventory changed
PLAYER_GUILD_UPDATE_GUID     -- unit guild changed
```

Example:
```lua
local function onUnitGuid(event, guid, isPlayer, isTarget, isMouseover, isPet, partyIndex, raidIndex)
    local tokens = {}
    if isPlayer    == 1 then table.insert(tokens, "player") end
    if isTarget    == 1 then table.insert(tokens, "target") end
    if isMouseover == 1 then table.insert(tokens, "mouseover") end
    if isPet       == 1 then table.insert(tokens, "pet") end
    if partyIndex  >  0 then table.insert(tokens, "party"..partyIndex) end
    if raidIndex   >  0 then table.insert(tokens, "raid"..raidIndex) end
    DEFAULT_CHAT_FRAME:AddMessage(string.format("[%s] %s (%s)", event, guid, table.concat(tokens, ", ")))
end

frame:RegisterEvent("UNIT_HEALTH_GUID", function(...) onUnitGuid("UNIT_HEALTH_GUID", ...) end)
frame:RegisterEvent("UNIT_MANA_GUID",   function(...) onUnitGuid("UNIT_MANA_GUID", ...) end)
```

### UNIT_COMBAT_GUID
Fires once per combat feedback event, identified by GUID. Unlike the other GUID events, this carries the full combat detail (action, damage, school, hitInfo).

Parameters:
1.  string guid - unit guid like `"0xF5300000000000A5"`
2.  string action - action string (`"WOUND"`, `"MISS"`, `"DODGE"`, `"PARRY"`, `"BLOCK"`, `"EVADE"`, `"IMMUNE"`, `"REFLECT"`, `"ABSORB"`, `"INTERRUPT"`)
3.  int damage - raw damage amount (0 for non-damaging outcomes like dodge/miss)
4.  int school - damage school (0=Physical, 1=Holy, 2=Fire, 3=Nature, 4=Frost, 5=Shadow, 6=Arcane)
5.  int hitInfo - bitfield (see below)
6.  int isPet - 1 if the unit is the active player's pet, 0 otherwise
7.  int partyIndex - party slot (1–4) if the unit is a party member, 0 otherwise
8.  int raidIndex - raid slot (1–40) if the unit is a raid member, 0 otherwise

#### HitInfo Flags (UNIT_COMBAT_GUID)
The `hitInfo` field is a 16-bit value from `CGGameUI::ShowCombatFeedback`:
```
-- Low byte (hitInfo & 0xFF)
COMBATHIT_MISS      = 0x10   -- Miss
COMBATHIT_ABSORB    = 0x20   -- Absorbed (damage == 0)
COMBATHIT_RESIST    = 0x40   -- Resisted (damage == 0)
COMBATHIT_CRITICAL  = 0x80   -- Critical hit

-- High byte ((hitInfo >> 8) & 0xFF)
COMBATHIT_BLOCK     = 0x08   -- Blocked (damage == 0)
COMBATHIT_GLANCING  = 0x40   -- Glancing blow
COMBATHIT_CRUSHING  = 0x80   -- Crushing blow
```

Example:
```lua
local function onUnitCombatGuid(guid, action, damage, school, hitInfo, isPet, partyIndex, raidIndex)
    local lo = bit.band(hitInfo, 0xFF)
    local hi = bit.rshift(hitInfo, 8)

    local hitType = "Normal"
    if damage > 0 then
        if   bit.band(lo, 0x80) ~= 0 then hitType = "Critical"
        elseif bit.band(hi, 0x40) ~= 0 then hitType = "Glancing"
        elseif bit.band(hi, 0x80) ~= 0 then hitType = "Crushing"
        end
    else
        if   bit.band(lo, 0x20) ~= 0 then hitType = "Absorb"
        elseif bit.band(hi, 0x08) ~= 0 then hitType = "Block"
        elseif bit.band(lo, 0x40) ~= 0 then hitType = "Resist"
        end
    end

    DEFAULT_CHAT_FRAME:AddMessage(string.format(
        "[UNIT_COMBAT_GUID] %s %s %d (%s) school=%d",
        guid, action, damage, hitType, school
    ))
end

frame:RegisterEvent("UNIT_COMBAT_GUID", onUnitCombatGuid)
```
