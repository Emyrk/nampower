# Unit Fields Reference

This document lists all available unit fields that can be accessed via `GetUnitData()` and `GetUnitField()` functions.

## Overview

Unit fields provide low-level access to game data for any unit (player, target, pet, NPCs, etc.). These fields are part of the WoW 1.12 client's internal data structures.

## Usage

```lua
-- Get all fields at once
local data = GetUnitData("target")
if data then
    print("Health: " .. data.health)
end

-- Get a single field (more efficient)
local health = GetUnitField("target", "health")
```

## Simple Fields

These fields return a single value (number).

### Object References (UINT64)
These fields contain GUIDs (globally unique identifiers) for game objects:

- **charm** - GUID of the unit that has charmed this unit
- **summon** - GUID of the unit's current summon
- **charmedBy** - GUID of the unit that charmed this unit
- **summonedBy** - GUID of the unit that summoned this unit
- **createdBy** - GUID of the unit that created this unit
- **target** - GUID of the unit's current target
- **persuaded** - GUID of the unit being persuaded
- **channelObject** - GUID of the object being channeled (for channeling spells)

### Health and Power (UINT32)
- **health** - Current health points
- **power1** - Current mana (or energy/rage depending on class)
- **power2** - Current rage
- **power3** - Current focus (hunter pets)
- **power4** - Current energy (rogues, druids in cat/bear form)
- **power5** - Current happiness (hunter pets)
- **maxHealth** - Maximum health points
- **maxPower1** - Maximum mana
- **maxPower2** - Maximum rage
- **maxPower3** - Maximum focus
- **maxPower4** - Maximum energy
- **maxPower5** - Maximum happiness

### Basic Unit Info (UINT32)
- **level** - Unit's level
- **factionTemplate** - Unit's faction template ID
- **flags** - Unit flags (combat, dead, mounted, etc.)
- **dynamicFlags** - Dynamic unit flags (lootable, tapped, etc.)
- **auraState** - Aura state flags (reactive abilities like Revenge, Overpower)
- **npcFlags** - NPC flags (vendor, trainer, quest giver, etc.)
- **npcEmoteState** - Current emote state ID

### Display and Appearance (UINT32)
- **displayId** - Current display model ID
- **nativeDisplayId** - Original/native display model ID
- **mountDisplayId** - Mount display model ID (if mounted)

### Combat Stats (UINT32)
- **baseAttackTime** - Base main-hand attack time in milliseconds
- **offhandAttackTime** - Off-hand attack time in milliseconds
- **rangedAttackTime** - Ranged attack time in milliseconds

### Combat Stats (FLOAT)
- **boundingRadius** - Unit's bounding radius for collision
- **combatReach** - Unit's combat reach distance
- **minDamage** - Minimum main-hand damage
- **maxDamage** - Maximum main-hand damage
- **minOffhandDamage** - Minimum off-hand damage
- **maxOffhandDamage** - Maximum off-hand damage
- **minRangedDamage** - Minimum ranged damage
- **maxRangedDamage** - Maximum ranged damage

### Pet Info (UINT32)
- **petNumber** - Pet's number identifier
- **petNameTimestamp** - Timestamp of pet name
- **petExperience** - Pet's current experience
- **petNextLevelExp** - Experience needed for pet's next level

### Spell/Channel Info (UINT32)
- **channelSpell** - Spell ID of currently channeling spell
- **createdBySpell** - Spell ID that created this unit (for summons)

### Casting Speed (FLOAT)
- **modCastSpeed** - Cast speed modifier (1.0 = normal, <1.0 = faster, >1.0 = slower)

### Training (UINT32)
- **trainingPoints** - Available training points (for pets and talents)

### Base Stats (UINT32)
- **stat0** - Strength
- **stat1** - Agility
- **stat2** - Stamina
- **stat3** - Intellect
- **stat4** - Spirit
- **baseMana** - Base mana before modifiers
- **baseHealth** - Base health before modifiers

### Attack Power (UINT32)
- **attackPower** - Total melee attack power
- **attackPowerMods** - Attack power modifiers
- **rangedAttackPower** - Total ranged attack power
- **rangedAttackPowerMods** - Ranged attack power modifiers

### Attack Power Multipliers (FLOAT)
- **attackPowerMultiplier** - Melee attack power multiplier
- **rangedAttackPowerMultiplier** - Ranged attack power multiplier

### Packed Byte Fields (UINT32)
These are special fields that pack multiple values into a single 32-bit integer:

- **bytes0** - Contains race, class, gender, and power type
  - Byte 0: Race
  - Byte 1: Class
  - Byte 2: Gender
  - Byte 3: Power type

- **bytes1** - Contains stand state, pet talent points, vis flags, and anim tier
  - Byte 0: Stand state
  - Byte 1: Pet talent points
  - Byte 2: Vis flags
  - Byte 3: Anim tier

- **bytes2** - Contains sheath state, pvp flags, pet flags, and shape shift form
  - Byte 0: Sheath state
  - Byte 1: PvP flags
  - Byte 2: Pet flags
  - Byte 3: Shape shift form

## Array Fields

These fields return a Lua table with numeric indices (1-based).

### Visual Items (UINT32 arrays)
- **virtualItemDisplay** [3] - Display IDs for virtual items (weapon visuals, etc.)
- **virtualItemInfo** [6] - Additional virtual item information

### Auras/Buffs/Debuffs (UINT32 arrays)
- **aura** [48] - Array of aura (buff/debuff) spell IDs on the unit
  - Index 1-48 contains spell IDs (0 = no aura in that slot)

- **auraFlags** [6] - Bit flags for aura properties (positive, negative, cancelable, etc.)
  - Each UINT32 contains flags for 8 auras (4 bits per aura)

- **auraLevels** [48] - Aura caster levels (UINT8 per aura slot)

- **auraApplications** [48] - Aura stack counts (UINT8 per aura slot)

### Resistances (UINT32 arrays)
- **resistances** [7] - Resistance values for all schools
  - Index 1: Armor (physical resistance)
  - Index 2: Holy resistance
  - Index 3: Fire resistance
  - Index 4: Nature resistance
  - Index 5: Frost resistance
  - Index 6: Shadow resistance
  - Index 7: Arcane resistance

### Spell Power Modifiers (FLOAT arrays)
- **powerCostModifier** [7] - Flat mana cost modifications per school
  - Index 1-7: Physical, Holy, Fire, Nature, Frost, Shadow, Arcane

- **powerCostMultiplier** [7] - Percent mana cost modifications per school
  - Index 1-7: Physical, Holy, Fire, Nature, Frost, Shadow, Arcane

## Examples

### Health Monitoring
```lua
-- Monitor target health percentage
local health = GetUnitField("target", "health")
local maxHealth = GetUnitField("target", "maxHealth")
local healthPct = (health / maxHealth) * 100
print("Target health: " .. healthPct .. "%")
```

### Checking Auras
```lua
-- Check if target has a specific debuff
local auras = GetUnitField("target", "aura")
local CURSE_OF_AGONY = 980

for i, spellId in ipairs(auras) do
    if spellId == CURSE_OF_AGONY then
        print("Target has Curse of Agony!")
        break
    end
end
```

### Reading Resistances
```lua
-- Get all player resistances
local resistances = GetUnitField("player", "resistances")
print("Armor: " .. resistances[1])
print("Holy resistance: " .. resistances[2])
print("Fire resistance: " .. resistances[3])
print("Nature resistance: " .. resistances[4])
print("Frost resistance: " .. resistances[5])
print("Shadow resistance: " .. resistances[6])
print("Arcane resistance: " .. resistances[7])
```

### Checking Combat State
```lua
-- Check if unit is in combat (flags field bit check)
local flags = GetUnitField("target", "flags")
local UNIT_FLAG_IN_COMBAT = 0x00080000
local inCombat = bit.band(flags, UNIT_FLAG_IN_COMBAT) ~= 0
print("In combat: " .. tostring(inCombat))
```

### Monitoring Cast Speed
```lua
-- Check your current cast speed modifier
local modCastSpeed = GetUnitField("player", "modCastSpeed")
-- 1.0 = normal, 0.9 = 10% faster, 1.1 = 10% slower
print("Cast speed modifier: " .. modCastSpeed)
```

### Attack Power Info
```lua
-- Get your attack power
local ap = GetUnitField("player", "attackPower")
local apMods = GetUnitField("player", "attackPowerMods")
local apMult = GetUnitField("player", "attackPowerMultiplier")
print("Base AP: " .. ap)
print("AP Mods: " .. apMods)
print("AP Multiplier: " .. apMult)
```

### Pet Information
```lua
-- Check pet experience
if UnitExists("pet") then
    local petXP = GetUnitField("pet", "petExperience")
    local petNextXP = GetUnitField("pet", "petNextLevelExp")
    local progress = (petXP / petNextXP) * 100
    print("Pet XP: " .. progress .. "%")
end
```

## Notes

- GUID fields return numbers but represent 64-bit values. Lua handles these as doubles.
- Array indices are 1-based (Lua convention), not 0-based.
- Returns `nil` if the unit doesn't exist or the field cannot be read.
- Some fields may require specific game state to be meaningful (e.g., pet fields only work when you have a pet).
- Unit flags and other bit fields require bitwise operations to interpret properly.

## Related Functions

- `GetUnitData(unitToken)` - Get all fields at once
- `GetUnitField(unitToken, fieldName)` - Get a single field efficiently
- `GetSpellModifiers(spellId, modifierType)` - Get spell modifiers
- `GetItemStats(itemId)` - Get item data
- `GetSpellRec(spellId)` - Get spell data
