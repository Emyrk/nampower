# Nampower Custom Events

This document describes all custom events added by Nampower that you can register and listen to in your addons.

For custom Lua functions, see [SCRIPTS.md](SCRIPTS.md). For general usage information, see [README.md](README.md).

## Table of Contents
- [SPELL_QUEUE_EVENT](#spell_queue_event)
- [SPELL_CAST_EVENT](#spell_cast_event)
- [SPELL_DAMAGE_EVENT_SELF and SPELL_DAMAGE_EVENT_OTHER](#spell_damage_event_self-and-spell_damage_event_other)
- [Buff/Debuff Events](#buffdebuff-events)
- [AURA_CAST_ON_SELF and AURA_CAST_ON_OTHER](#aura_cast_on_self-and-aura_cast_on_other)
- [AUTO_ATTACK_SELF and AUTO_ATTACK_OTHER](#auto_attack_self-and-auto_attack_other)
- [UNIT_DIED](#unit_died)

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
Event you can register in game to get updates when you cast spells with some additional information.  This will only fire for spells you (and certain pets) initiated.

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
New events fire whenever a buff or debuff is added or removed on you or any other unit that the client tracks.

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
2.  int slot - 1-based Lua slot index for the buff/debuff (skips empty slots to match UnitBuff/UnitDebuff ordering)
3.  int spellId
4.  int stackCount - current stack count for the aura (1 for a new aura; 0 when fully removed)
5.  int auraLevel - caster level for the aura from UnitFields.auraLevels (uint8 per slot, 48 entries)

Buff stack gains also fire the appropriate *_ADDED_* events.

Example:
```
local function onAuraEvent(eventName, guid, slot, spellId, stacks, auraLevel)
    DEFAULT_CHAT_FRAME:AddMessage(string.format("[%s] %s slot=%d spell=%d stacks=%d level=%d", eventName, guid, slot, spellId, stacks, auraLevel))
end

for _, eventName in ipairs({"BUFF_ADDED_SELF", "BUFF_REMOVED_SELF", "DEBUFF_ADDED_OTHER", "DEBUFF_REMOVED_OTHER"}) do
    frame:RegisterEvent(eventName, function(...) onAuraEvent(eventName, ...) end)
end
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
