# Nampower Custom Lua Functions

This document describes all custom Lua functions and events added by Nampower.

For installation, configuration, and general usage information, see the main [README.md](README.md).

## Table of Contents
- [Performance Optimization - Table References](#performance-optimization---table-references)
- [Custom Lua Functions](#custom-lua-functions)
  - [Spell/Item/Unit Information](#spellitemunit-information)
    - [GetItemStats](#getitemstatsitemid-copy)
    - [GetItemStatsField](#getitemstatsfielditemid-fieldname-copy)
    - [FindPlayerItemSlot](#findplayeritemslotitemid-or-itemname)
    - [UseItemIdOrName](#useitemidornameitemidorname-target)
    - [GetEquippedItems](#getequippeditemsunittoken)
    - [GetEquippedItem](#getequippeditemunittoken-slot)
    - [GetBagItems](#getbagitemsbagindex)
    - [GetBagItem](#getbagitembagindex-slot)
    - [GetSpellRec](#getspellrecspellid-copy)
    - [GetSpellRecField](#getspellrecfieldspellid-fieldname-copy)
    - [GetSpellModifiers](#getspellmodifiersspellid-modifiertype)
    - [GetUnitData](#getunitdataunittoken-copy)
    - [GetUnitField](#getunitfieldunittoken-fieldname-copy)
    - [GetSpellIdForName](#getspellidfornamenspellname)
    - [GetSpellNameAndRankForId](#getspellnameandrankforidid)
    - [GetSpellSlotTypeIdForName](#getspellslottypeidfornamenspellname)
    - [GetNampowerVersion](#getnampowerversion)
    - [GetItemLevel](#getitemlevelitemid)
    - [GetItemIconTexture](#getitemicontexturedisplayinfoid)
    - [GetSpellIconTexture](#getspellicontexturespelliconid)
  - [Spell Casting and Queuing](#spell-casting-and-queuing)
    - [QueueSpellByName](#queuespellbynamespellname)
    - [CastSpellByNameNoQueue](#castspellbynamenoqueuespellname)
    - [QueueScript](#queuescriptscript-priority)
    - [IsSpellInRange](#isspellinrangespellname-target-or-isspellinrangespellid-target)
    - [IsSpellUsable](#isspellusablespellname-or-isspellusablespellid)
    - [ChannelStopCastingNextTick](#channelstopcastingnexttick)
  - [Cast Information](#cast-information)
    - [GetCurrentCastingInfo](#getcurrentcastinginfo)
    - [GetCastInfo](#getcastinfo)
  - [Cooldown Information](#cooldown-information)
    - [GetSpellIdCooldown](#getspellidcooldownspellid)
    - [GetItemIdCooldown](#getitemidcooldownitemid)
    - [GetTrinkets](#gettrinketscopy)
    - [GetTrinketCooldown](#gettrinketcooldownslotitemidorname)
    - [UseTrinket](#usetrinketslotitemidorname-target)
  - [Utility Functions](#utility-functions)
    - [DisenchantAll](#disenchantallitemidorname-includesoulbound-or-disenchantallquality-includesoulbound)
---

## Performance Optimization - Table References

Nampower functions that return tables use **reusable table references** to reduce memory allocations and improve performance. This means the same table object is reused across multiple function calls, with its contents updated each time.

### Functions Using Reusable Table References

The following functions use reusable table references:

- **`GetCastInfo()`** - Returns cast information table
- **`GetEquippedItems([unitToken])`** - Returns equipped items table
- **`GetBagItems([bagIndex])`** - Returns bag items table
- **`GetBagItem(bagIndex, slot)`** - Returns item info table
- **`GetEquippedItem(unitToken, slot)`** - Returns item info table
- **`GetSpellIdCooldown(spellId)`** - Returns cooldown detail table
- **`GetItemIdCooldown(itemId)`** - Returns cooldown detail table
- 
- **`GetItemStats(itemId, [copy])`** - Returns item stats table
- **`GetUnitData(unitToken, [copy])`** - Returns unit data table
- **`GetSpellRec(spellId, [copy])`** - Returns spell record table
- **`GetItemStatsField(itemId, fieldName, [copy])`** - Returns individual item field value
- **`GetUnitField(unitToken, fieldName, [copy])`** - Returns individual unit field value
- **`GetSpellRecField(spellId, fieldName, [copy])`** - Returns individual spell field value
- **`GetTrinkets([copy])`** - Returns trinket list table

**Important:** When using these functions without the `copy` parameter, **immediately copy or extract** any values you need to store for later use. Do not store references to the returned tables themselves. Alternatively, pass `1` as the `copy` parameter to get an independent table that is safe to store.

**Note:** Functions like `GetItemStats`, `GetUnitData`, and `GetSpellRec` also use reusable references for their nested array fields (e.g., `bonusStat`, `auras`, `EffectImplicitTargetA`). Each nested array field name has its own dedicated reference that is reused across calls.

```lua
-- ✓ SAFE - Extract values immediately
local castInfo = GetCastInfo()
if castInfo then
    local spellId = castInfo.spellId
    local castEnd = castInfo.castEndS
    -- Use spellId and castEnd later
end

-- ✓ SAFE - Extract nested array values immediately
local itemStats = GetItemStats(19019)
if itemStats then
    local bonusStats = {}
    for i = 1, #itemStats.bonusStat do
        bonusStats[i] = itemStats.bonusStat[i]
    end
    -- Now bonusStats is a safe independent copy
end

-- ✗ UNSAFE - Storing table references from the same function
local cast1 = GetCastInfo()  -- Gets table reference
-- ... later ...
local cast2 = GetCastInfo()  -- Gets SAME table reference with new data
-- cast1 and cast2 both point to the same table with cast2's data!

-- ✗ UNSAFE - Storing nested array references
local item1 = GetItemStats(19019)
local item1BonusStats = item1.bonusStat  -- Stores reference to nested array
local item2 = GetItemStats(22589)
-- item1BonusStats was overwritten! The "bonusStat" nested array reference is reused

-- ✓ SAFE - Using copy parameter for nested arrays
local item1 = GetItemStats(19019, 1)  -- Pass 1 to get independent copy
local item1BonusStats = item1.bonusStat  -- Safe to store, it's an independent copy
local item2 = GetItemStats(22589, 1)  -- Another independent copy
-- Both item1BonusStats and item2.bonusStat are independent tables
```

**Important for array field functions:** Each field name gets its own dedicated table reference, but the table is still reused across calls with the same field name. **Always extract values immediately - never store the table reference itself.** Alternatively, pass `1` as the `copy` parameter to get an independent table copy:

```lua
-- ✓ SAFE - Extract array values immediately
local bonusStats = {}
local tempTable = GetItemStatsField(itemId, "bonusStat")
for i = 1, #tempTable do
    bonusStats[i] = tempTable[i]
end
-- Now bonusStats is a safe independent copy

-- ✓ EASIER - Use copy parameter to get independent table
local bonusStats = GetItemStatsField(itemId, "bonusStat", 1)
-- Safe to store, no manual copying needed!

-- ✗ UNSAFE - Storing table references (even with different field names)
local bonusStats = GetItemStatsField(itemId, "bonusStat")
local bonusAmounts = GetItemStatsField(itemId, "bonusAmount")
-- Later...
local newBonusStats = GetItemStatsField(otherItemId, "bonusStat")
-- bonusStats was overwritten! The "bonusStat" reference is reused across calls

-- ✗ ALSO UNSAFE - Same field name, multiple calls
local item1Stats = GetItemStatsField(19019, "bonusStat")
local item2Stats = GetItemStatsField(22589, "bonusStat")
-- item1Stats was immediately overwritten by the second call!
```

---

### Custom Lua Functions

### Spell/Item/Unit information

#### GetItemStats(itemId, [copy])
Returns a Lua table reference containing all fields for the item's `ItemStats` record (including localized `displayName` and `description`). Returns nil if the item cannot be found or loaded.

**Optional parameter:** Pass `1` for `copy` to get an independent table copy instead of a reusable reference.

Full field name lists are in [`DBC_FIELDS.md`](DBC_FIELDS.md).

#### GetItemStatsField(itemId, fieldName, [copy])
Fast lookup for a single field on an item. Returns the requested field value; returns nil if the item is not found; raises a Lua error if the field name is invalid.

**Optional parameter:** Pass `1` for `copy` to get an independent table copy (for array fields only).

Full field name lists are in [`DBC_FIELDS.md`](DBC_FIELDS.md).

**Examples:**
```lua
-- Get item name
local name = GetItemStatsField(19019, "displayName")
print(name) -- "Thunderfury, Blessed Blade of the Windseeker"

-- Get item level
local ilvl = GetItemStatsField(22589, "itemLevel")
print("Atiesh item level: " .. ilvl) -- 90

-- Get item quality (0=Poor, 1=Common, 2=Uncommon, 3=Rare, 4=Epic, 5=Legendary)
local quality = GetItemStatsField(19019, "quality")
print("Quality: " .. quality) -- 5 (Legendary)

-- Get item delay (weapon speed in milliseconds)
local delay = GetItemStatsField(19019, "delay")
print("Weapon speed: " .. (delay / 1000) .. " seconds") -- 1.9 seconds
```

#### FindPlayerItemSlot(itemId or itemName)
Searches the player's inventory for an item by ID or name and returns its location.

**Parameters:**
- `itemId` (number): The item ID to search for, OR
- `itemName` (string): The item name to search for (case-insensitive)

**Returns:**
- 1st param (number or nil): Bag index where the item was found
  - `nil` = Equipped item (check 2nd param for equipment slot 0-18)
  - `0` = Inventory pack
  - `1-4` = Regular bags
  - `-1` = Bank item slots
  - `5-10` = Bank bags
  - `-2` = Keyring
- 2nd param (number): Slot number within the bag (or equipment slot if 1st param is nil)
  - For equipped items: 0-18 (equipment slots are 0-indexed)
  - For bag 0, -1, -2: Returns **relative slot position** (1-indexed, 0-based within bag + 1)
    - Bag 0: slots 1-16 (corresponding to absolute slots 23-38)
    - Bag -1: slots 1-24 (corresponding to absolute bank slots 39-62)
    - Bag -2: slots 1-16 (corresponding to absolute keyring slots 81-96)
  - For regular bags (1-4) and bank bags (5-10): Returns 1-indexed slot within the bag
- Returns `nil,nil` if the item is not found

**Examples:**
```lua
-- Find Thunderfury in player inventory
local bag, slot = FindPlayerItemSlot(19019)
if bag then
    print("Found in bag " .. bag .. " slot " .. slot)
    if bag == -1 or (bag >= 5 and bag <= 9) then
        print("Item is in bank")
    end
elseif bag == nil and slot then
    print("Item is equipped in slot " .. slot)
else
    print("Item not found")
end

-- Find item by name (uses cache for performance after first lookup)
local bag, slot = FindPlayerItemSlot("Hearthstone")
if slot then
    if bag == nil then
        print("Hearthstone is equipped in slot " .. slot)
    elseif bag == 0 then
        print("Hearthstone is in inventory pack slot " .. slot .. " (1-16)")
    elseif bag == -1 then
        print("Hearthstone is in bank slot " .. slot .. " (1-24)")
    elseif bag == -2 then
        print("Hearthstone is in keyring slot " .. slot .. " (1-16)")
    else
        print("Hearthstone is in bag " .. bag .. " slot " .. slot)
    end
end
```

#### UseItemIdOrName(itemIdOrName, [target])
Uses the first matching item found in the player's inventory (including equipped items) by item ID or name.

**Parameters:**
- `itemIdOrName` (number|string): Item ID or item name (case-insensitive)
- `target` (optional, string|number): Unit token (e.g. `"target"`, `"player"`) or GUID
  - If omitted, uses `LockedTargetGuid` if set; otherwise falls back to the active player GUID.

**Returns:**
- `1` if the item was found and `CGItem_C::Use(...)` returned non-zero
- `0` if the item was not found or use failed

**Examples:**
```lua
-- Use Hearthstone
UseItemIdOrName("Hearthstone")

-- Use a healing potion on yourself (if the item requires a target)
UseItemIdOrName(13446, "player")
```

#### GetEquippedItems(unitToken)
Returns a table reference containing all equipped items for the specified unit.

**Parameters:**
- `unitToken` (string): Can be a standard unit token ("player", "target", "pet", etc.) or a GUID string

**Returns:**
- A Lua table reference with equipment slot indices as keys (0-18) and item info tables as values
- Returns nil if the unit cannot be found or inspected

For the player, item info includes:
- `itemId`: The item's ID
- `stackCount`: Number of items in the stack
- `duration`: Item duration in milliseconds
- `spellCharges`: Table of spell charges (indices 1-5)
- `flags`: Item flags
- `permanentEnchantId`: Permanent enchantment ID
- `tempEnchantId`: Temporary enchantment ID
- `tempEnchantmentTimeLeftMs`: Time remaining on temp enchant in milliseconds
- `tempEnchantmentCharges`: Charges remaining on temp enchant
- `durability`: Current durability
- `maxDurability`: Maximum durability

For other inspected units (limited data):
- `itemId`: The item's ID
- `permanentEnchantId`: Permanent enchantment ID
- `tempEnchantId`: Temporary enchantment ID

**Examples:**
```lua
-- Get all equipped items for your target
local items = GetEquippedItems("target")
if items then
    for slot, itemInfo in pairs(items) do
        print("Slot " .. slot .. ": Item ID " .. itemInfo.itemId)
        if itemInfo.permanentEnchantId and itemInfo.permanentEnchantId > 0 then
            print("  Permanent enchant: " .. itemInfo.permanentEnchantId)
        end
    end
end

-- Check player's weapon durability
local items = GetEquippedItems("player")
if items and items[15] then -- slot 15 is main hand
    local weapon = items[15]
    print("Weapon durability: " .. weapon.durability .. "/" .. weapon.maxDurability)
end
```

#### GetEquippedItem(unitToken, slot)
Returns item info for a specific equipment slot on the specified unit.

**Parameters:**
- `unitToken` (string): Can be a standard unit token ("player", "target", "pet", etc.) or a GUID string
- `slot` (number): Equipment slot number (0-18)
  - 1 = Head, 2 = Neck, 3 = Shoulder, 4 = Shirt, 5 = Chest
  - 6 = Waist, 7 = Legs, 8 = Feet, 9 = Wrist, 10 = Hands
  - 11 = Finger 1, 12 = Finger 2, 13 = Trinket 1, 14 = Trinket 2
  - 15 = Back, 16 = Main Hand, 17 = Off Hand, 18 = Ranged, 19 = Tabard

**Returns:**
- A Lua table reference containing the item info (same fields as GetEquippedItems)
- Returns nil if the slot is empty, unit cannot be found, or unit cannot be inspected

**Examples:**
```lua
-- Check target's main hand weapon
local weapon = GetEquippedItem("target", 16)
if weapon then
    print("Target has weapon: " .. weapon.itemId)
else
    print("Target has no main hand weapon")
end

-- Check your own helmet
local helm = GetEquippedItem("player", 1)
if helm and helm.durability then
    local durabilityPercent = (helm.durability / helm.maxDurability) * 100
    print("Helmet durability: " .. string.format("%.1f%%", durabilityPercent))
end
```

#### GetBagItems([bagIndex])
If no bagIndex is specified, the function returns a nested table reference containing all items in all bags (including bank if open). With specified index, it only returns the contents of that bag

**Returns:**
- A Lua table reference with bag indices as keys and bag contents as values
- Each bag contains **1-indexed** slot numbers as keys and item info tables as values
- Bag indices:
  - 0 = Inventory pack (16 slots)
  - 1-4 = Regular bags
  - -1 = Bank item slots (24 slots, only if bank is open)
  - 5-10 = Bank bags (only if bank is open)
  - -2 = Keyring

Item info table fields (same as GetEquippedItems for player):
- `itemId`, `stackCount`, `duration`, `spellCharges`, `flags`
- `permanentEnchantId`, `tempEnchantId`, `tempEnchantmentTimeLeftMs`, `tempEnchantmentCharges`
- `durability`, `maxDurability`

**Examples:**
```lua
-- Get all items in all bags
local allItems = GetBagItems()
for bagIndex, bagContents in pairs(allItems) do
    print("Bag " .. bagIndex .. ":")
    for slot, itemInfo in pairs(bagContents) do
        print("  Slot " .. slot .. ": " .. itemInfo.itemId .. " (x" .. itemInfo.stackCount .. ")")
    end
end

-- Get all items in backbag
local bagContents = GetBagItems(0)
for slot, itemInfo in pairs(bagContents) do
    print("  Slot " .. slot .. ": " .. itemInfo.itemId .. " (x" .. itemInfo.stackCount .. ")")
end

-- Count total number of a specific item
local function CountItem(itemId)
    local total = 0
    local allItems = GetBagItems()
    for bagIndex, bagContents in pairs(allItems) do
        for slot, itemInfo in pairs(bagContents) do
            if itemInfo.itemId == itemId then
                total = total + itemInfo.stackCount
            end
        end
    end
    return total
end

local soulShardCount = CountItem(6265)
print("Soul Shards: " .. soulShardCount)
```

#### GetBagItem(bagIndex, slot)
Returns item info for a specific slot in a specific bag.

**Parameters:**
- `bagIndex` (number): The bag to check
  - 0 = Inventory pack
  - 1-4 = Regular bags
  - -1 = Bank item slots or buyback slots
  - 5-10 = Bank bags (requires bank to be open)
  - -2 = Keyring
- `slot` (number): **1-indexed** slot number within the bag

**Returns:**
- A Lua table reference containing the item info (same fields as GetBagItems)
- Returns nil if the slot is empty or invalid

**Examples:**
```lua
-- Get item in first slot of first bag
local item = GetBagItem(1, 1)
if item then
    print("Item ID: " .. item.itemId)
    print("Stack count: " .. item.stackCount)
else
    print("Slot is empty")
end

-- Check durability of an item in inventory pack
local item = GetBagItem(0, 1)
if item and item.durability then
    print("Durability: " .. item.durability .. "/" .. item.maxDurability)
end

-- Check if a specific bank slot has an item (bank must be open)
local bankItem = GetBagItem(-1, 1)
if bankItem then
    print("Bank slot 1 contains: " .. bankItem.itemId)
end
```

#### GetSpellRec(spellId, [copy])
Returns a Lua table reference containing all fields for the spell's `SpellRec` record (including localized `name` and `rank`). Returns nil if the spell cannot be found.

**Optional parameter:** Pass `1` for `copy` to get an independent table copy instead of a reusable reference.

Full field name lists are in [`DBC_FIELDS.md`](DBC_FIELDS.md).

#### GetSpellRecField(spellId, fieldName, [copy])
Fast lookup for a single field on a spell. Returns the requested field value; returns nil if the spell is not found; raises a Lua error if the field name is invalid.

**Optional parameter:** Pass `1` for `copy` to get an independent table copy (for array fields only).

Full field name lists are in [`DBC_FIELDS.md`](DBC_FIELDS.md).

**Examples:**
```lua
-- Get spell name
local name = GetSpellRecField(116, "name")
print(name) -- "Frostbolt"

-- Get spell rank
local rank = GetSpellRecField(116, "rank")
print(rank) -- "Rank 1"

-- Get spell cast time in milliseconds
local castTime = GetSpellRecField(133, "castTime")
print("Fireball cast time: " .. (castTime / 1000) .. " seconds") -- 3.5 seconds

-- Get spell range (max range in yards * 10, so divide by 10)
local maxRange = GetSpellRecField(116, "rangeMax")
print("Frostbolt max range: " .. (maxRange / 10) .. " yards") -- 30 yards

-- Get spell mana cost
local manaCost = GetSpellRecField(116, "manaCost")
print("Mana cost: " .. manaCost)

-- Get spell school (0=Physical, 1=Holy, 2=Fire, 3=Nature, 4=Frost, 5=Shadow, 6=Arcane)
local school = GetSpellRecField(116, "school")
print("School: " .. school) -- 4 (Frost)

-- Get spell icon ID
local spellIconID = GetSpellRecField(116, "spellIconID")
print("Icon ID: " .. spellIconID)
```

#### GetSpellModifiers(spellId, modifierType)
Returns the current spell modifiers applied to a spell for the player. This includes buffs, talents, and other effects that modify spell behavior.

**Parameters:**
- `spellId` (number): The spell ID to check
- `modifierType` (number): The type of modifier to check (see list below)

**Returns:**
- 1st param (number): Flat modification value (e.g., +50 damage)
- 2nd param (number): Percent modification value (e.g., 10 for +10%)
- 3rd param (number): Return value from the function (whether there was any percent or flat modifier)

**Modifier Types:**
- 0 = DAMAGE
- 1 = DURATION
- 2 = THREAT
- 3 = ATTACK_POWER
- 4 = CHARGES
- 5 = RANGE
- 6 = RADIUS
- 7 = CRITICAL_CHANCE
- 8 = ALL_EFFECTS
- 9 = NOT_LOSE_CASTING_TIME
- 10 = CASTING_TIME
- 11 = COOLDOWN
- 12 = SPEED
- 14 = COST
- 15 = CRIT_DAMAGE_BONUS
- 16 = RESIST_MISS_CHANCE
- 17 = JUMP_TARGETS
- 18 = CHANCE_OF_SUCCESS
- 19 = ACTIVATION_TIME
- 20 = EFFECT_PAST_FIRST
- 21 = CASTING_TIME_OLD
- 22 = DOT
- 23 = HASTE
- 24 = SPELL_BONUS_DAMAGE
- 27 = MULTIPLE_VALUE
- 28 = RESIST_DISPEL_CHANCE

**Example:**
```lua
-- Check damage modifiers on Frostbolt (spell ID 116)
local flatMod, percentMod, ret = GetSpellModifiers(116, 0)
print("Flat damage bonus: " .. flatMod)
print("Percent damage bonus: " .. percentMod .. "%")
```

#### GetUnitData(unitToken, [copy])
Returns a Lua table reference containing all unit fields for the specified unit. This provides access to low-level unit data like health, mana, stats, auras, resistances, and more.

**Parameters:**
- `unitToken` (string): Can be a standard unit token ("player", "target", "pet", "mouseover", etc.) or a GUID string (e.g., "0xF5300000000000A5")
- `copy` (number, optional): Pass `1` to get an independent table copy instead of a reusable reference

**Returns:**
- A Lua table reference containing all unit fields, or nil if the unit cannot be found

Full field name lists are in [`UNIT_FIELDS.md`](UNIT_FIELDS.md).

**Example:**
```lua
-- Get all unit data for your current target
local data = GetUnitData("target")
if data then
    print("Target health: " .. data.health .. "/" .. data.maxHealth)
    print("Target level: " .. data.level)
    print("Target display ID: " .. data.displayId)
end

-- Using a GUID
local data = GetUnitData("0xF5300000000000A5")
```

#### GetUnitField(unitToken, fieldName, [copy])
Fast lookup for a single field on a unit. More efficient than GetUnitData when you only need one specific field.

**Parameters:**
- `unitToken` (string): Can be a standard unit token ("player", "target", "pet", "mouseover", etc.) or a GUID string
- `fieldName` (string): The name of the field to retrieve
- `copy` (number, optional): Pass `1` to get an independent table copy (for array fields only)

**Returns:**
- The requested field value; returns nil if the unit is not found; raises a Lua error if the field name is invalid
- For array fields (like "aura", "resistances"), returns a Lua table with numeric indices

Full field name lists are in [`UNIT_FIELDS.md`](UNIT_FIELDS.md).

**Examples:**
```lua
-- Get target's current health
local health = GetUnitField("target", "health")
print("Target health: " .. health)

-- Get player's current mana (power1)
local mana = GetUnitField("player", "power1")
print("Player mana: " .. mana)

-- Get all auras on target (returns a table)
local auras = GetUnitField("target", "aura")
for i, auraId in ipairs(auras) do
    print("Aura " .. i .. ": " .. auraId)
end

-- Get all resistances (returns a table)
local resistances = GetUnitField("player", "resistances")
-- resistances[1] = armor, [2] = holy, [3] = fire, [4] = nature, [5] = frost, [6] = shadow, [7] = arcane
```

#### QueueSpellByName(spellName)
Will force queue a spell regardless of the appropriate queue window.  If no spell is currently being cast it will be cast immediately.
For example can make a macro with 
```
/run QueueSpellByName("Frostbolt");QueueSpellByName("Frostbolt")
```
to cast 2 frostbolts in a row.  Currently, can only queue 1 GCD spell at a time and 5 non gcd spells.  This means you can't do 3 frostbolts in a row with one macro.

#### CastSpellByNameNoQueue(spellName)
Will force a spell cast to never queue even if your settings would normally queue.  Can be used to fix addons that don't work with queued spells.

#### QueueScript(script, [priority])
Queues any arbitrary script using the same logic as a regular spell using NP_SpellQueueWindowMs as the window.  If no spell is being cast and you are not on the gcd the script will be run immediately.

Priority is optional and defaults to 1.  
Priority 1 means the script will run before any other queued spells.
Priority 2 means the script will run after any queued non gcd spells but before any queued normal spells.
Priority 3 means the script will run after any type of queued spells.

Convert slash commands from other addons like `/equip` to their function form `SlashCmdList.EQUIP` to use them inside QueueScript.

For example, you can equip a libram before casting a queued heal using
```
/run QueueScript('SlashCmdList.EQUIP("Libram of +heal")')
```

#### IsSpellInRange(spellName, [target]) or IsSpellInRange(spellId, [target])
Takes a spell name or spell id and an optional target.  Target can the usual UNIT tokens like "player", "target", "mouseover", etc or a unit guid.

If using spell name it must be a spell you have in your spellbook.  If using spell id it can be any spell id.

Returns 1 if the spell is in range, 0 if not in range, and -1 if the spell is not valid for this check (must be TARGET_UNIT_PET, TARGET_UNIT_TARGET_ENEMY, TARGET_UNIT_TARGET_ALLY, TARGET_UNIT_TARGET_ANY).
This is because this uses the same underlying function as `IsActionInRange` which returns 1 for spells that are not single target which can be misleading.

Examples:
```
/run local result=IsSpellInRange("Frostbolt"); if result == 1 then print("In range") else if result == 0 then print("Out of range") else print("Not single target") end
```

#### IsSpellUsable(spellName) or IsSpellUsable(spellId)
Takes a spell name or spell id.  

Usable does not equal castable.  This is most often used to check if a reactive spell is usable.

If using spell name it must be a spell you have in your spellbook.  If using spell id it can be any spell id.

Returns: 

1st param: 1 if the spell is usable, 0 if not usable.
2nd param: Always 0 if spell is not usable for a different reason other than mana.  1 if out of mana, 0 if not out of mana.

Examples:
```
/run local result=IsSpellUsable("Frostbolt"); if result == 1 then print("Frostbolt usable") else print("Frostbolt not usable") end
```

#### GetCurrentCastingInfo()
Returns:

1st param: Casting spell id or 0
2nd param: Visual spell id or 0.  This won't always get cleared after a spell finishes.
3rd param: Auto repeating spell id or 0.
4th param: 1 if casting spell with a cast time, 0 if not.
5th param: 1 if channeling, 0 if not.
6th param: 1 if on swing spell is pending, 0 if not.
7th param: 1 if auto attacking, 0 if not.

For normal spells these will be the same.  For some spells like auto-repeating and channeling spells only the visual spell id will be set.

Examples:
```
/run local castId,visId,autoId,casting,channeling,onswing,autoattack=GetCurrentCastingInfo();print(castId);print(visId);print(autoId);print(casting);print(channeling);print(onswing);print(autoattack);
```

#### GetCastInfo()
Returns detailed information about the currently active cast or channel. Returns nil if there is no active cast or channel.
GetCurrentCastingInfo was made very early on and doesn't provide enough information for many use cases, but still has some uses and is available for backwards compatibility.

**Returns:**
A Lua table reference with the following fields, or nil if no cast is active:

- `castId` (number): Unique identifier for this cast
- `spellId` (number): The spell ID being cast
- `guid` (number): Target GUID (0 if no explicit target)
- `castType` (number): Type of cast - 0=NORMAL, 3=CHANNEL, 4=TARGETING
- `castStartS` (number): When the cast started in WoW time (seconds with decimals, e.g., 1234567.890)
- `castEndS` (number): When the cast will end in WoW time (seconds with decimals)
- `castRemainingMs` (number): Milliseconds remaining until cast ends
- `castDurationMs` (number): Total cast duration in milliseconds
- `gcdEndS` (number): When the GCD will end in WoW time (seconds with decimals)
- `gcdRemainingMs` (number): Milliseconds remaining until GCD expires

**Notes:**
- Time fields ending in `S` (castStartS, castEndS, gcdEndS) are absolute timestamps in **seconds** with decimal precision to match GetTime() in Lua
- Duration and remaining fields ending in `Ms` (castRemainingMs, castDurationMs, gcdRemainingMs) are in **milliseconds** for precision
- Returns nil if there is no active cast (castSpellId is 0) and no active channel (channelSpellId is 0)

**Examples:**
```lua
-- Check current cast information
local info = GetCastInfo()
if info then
    print("Casting spell: " .. info.spellId)
    print("Cast ends at: " .. info.castEndS)
    print("Time remaining: " .. info.castRemainingMs .. "ms")
    print("GCD ends at: " .. info.gcdEndS)
    print("GCD remaining: " .. info.gcdRemainingMs .. "ms")
else
    print("No active cast")
end

-- Check if you can cast another spell (GCD check)
local info = GetCastInfo()
if not info or info.gcdRemainingMs == 0 then
    print("Ready to cast!")
else
    print("On GCD for " .. info.gcdRemainingMs .. "ms more")
end

-- Monitor cast progress
local info = GetCastInfo()
if info and info.castDurationMs > 0 then
    local progress = ((info.castDurationMs - info.castRemainingMs) / info.castDurationMs) * 100
    print("Cast progress: " .. string.format("%.1f%%", progress))
end
```

#### GetSpellIdCooldown(spellId)
Returns detailed cooldown information for a spell from the spell history. This provides precise timing data for individual spell cooldowns, category cooldowns, and GCD.

**Parameters:**
- `spellId` (number): The spell ID to check

**Returns:**
A Lua table reference with the following fields:

- `isOnCooldown` (number): 1 if any cooldown is active, 0 otherwise
- `cooldownRemainingMs` (number): Maximum remaining time across all cooldown types in milliseconds
- `itemId` (number): Item ID tied to the cooldown (0 if none)
- `itemHasActiveSpell` (number): 1 if the item has an on-use spell, 0 otherwise
- `itemActiveSpellId` (number): Spell ID of the active item spell (0 if none)

**Individual Spell Cooldown:**
- `individualStartS` (number): When the individual spell cooldown started (seconds, WoW time)
- `individualDurationMs` (number): Total duration of the individual spell cooldown in milliseconds
- `individualRemainingMs` (number): Milliseconds remaining on the individual spell cooldown
- `isOnIndividualCooldown` (number): 1 if the spell-specific cooldown is active, 0 otherwise

**Category Cooldown:**
- `categoryId` (number): The cooldown category ID (0 if no category cooldown)
- `categoryStartS` (number): When the category cooldown started (seconds, WoW time)
- `categoryDurationMs` (number): Total duration of the category cooldown in milliseconds
- `categoryRemainingMs` (number): Milliseconds remaining on the category cooldown
- `isOnCategoryCooldown` (number): 1 if the category cooldown is active, 0 otherwise

**GCD (Global Cooldown):**
- `gcdCategoryId` (number): The GCD category ID (typically 133 for most spells)
- `gcdCategoryStartS` (number): When the GCD started (seconds, WoW time)
- `gcdCategoryDurationMs` (number): Total GCD duration in milliseconds (typically 1500ms)
- `gcdCategoryRemainingMs` (number): Milliseconds remaining on the GCD
- `isOnGcdCategoryCooldown` (number): 1 if the GCD is active, 0 otherwise

**Notes:**
- Time fields ending in `S` are absolute timestamps in **seconds** to match GetTime() in Lua
- Fields ending in `Ms` are in **milliseconds** for precision
- The spell must have been cast at least once for accurate data to be available
- `cooldownRemainingMs` is the maximum of all three cooldown types

**Example:**
```lua
-- Check if Frostbolt is ready to cast
local cd = GetSpellIdCooldown(116) -- Frostbolt
if cd.isOnCooldown == 0 then
    print("Frostbolt is ready!")
else
    print("Frostbolt on cooldown for " .. cd.cooldownRemainingMs .. "ms")
    if cd.isOnGcdCategoryCooldown == 1 then
        print("  GCD: " .. cd.gcdCategoryRemainingMs .. "ms remaining")
    end
    if cd.isOnIndividualCooldown == 1 then
        print("  Spell CD: " .. cd.individualRemainingMs .. "ms remaining")
    end
    if cd.isOnCategoryCooldown == 1 then
        print("  Category CD: " .. cd.categoryRemainingMs .. "ms remaining")
    end
end
```

#### GetItemIdCooldown(itemId)
Returns detailed cooldown information for an item from the spell history. Works similarly to GetSpellIdCooldown but for items.

**Parameters:**
- `itemId` (number): The item ID to check

**Returns:**
A Lua table reference with the same structure as GetSpellIdCooldown (see above).

**Notes:**
- Returns the longest cooldown among all spells associated with the item
- If the item has multiple on-use effects, returns information for the one with the longest remaining cooldown
- Item cooldowns are tracked through their associated spell entries in the spell history

**Example:**
```lua
-- Check if a trinket is ready
local cd = GetItemIdCooldown(12345) -- Replace with your trinket ID
if cd.isOnCooldown == 0 then
    print("Trinket is ready to use!")
else
    print("Trinket on cooldown for " .. cd.cooldownRemainingMs .. "ms")
end
```

#### GetTrinkets([copy])
Returns a table of trinkets from equipped trinket slots and carried bags.

**Parameters:**
- `[copy]` (number|boolean, optional): Pass `1` (or any truthy value) to force creation of a fresh Lua table. By default the function reuses an internal table and entry tables for performance.

**Returns:**
A Lua table where each entry contains:
- `itemId` (number)
- `trinketName` (string, `"Unknown"` if no name available)
- `texture` (string): Texture name for the item icon
- `bagIndex` (number|nil): `nil` when equipped; `0` for backpack; `1-4` for equipped bags
- `slotIndex` (number): Lua 1-based slot within the container (or 1/2 for equipped trinket slots)

**Notes:**
- Scans only equipped trinket slots and bags 0-4 (backpack + equipped bags). Does not scan bank or keyring.
- Reuses cached Lua tables unless `copyTable` is truthy; prefer copies if you will mutate the returned tables.
- 
#### GetTrinketCooldown(slot|itemIdOrName)
Returns cooldown information for the equipped trinket(s) in slots 13 or 14. Accepts slot shortcuts or item identifiers.

**Parameters:**
- `slot|itemIdOrName` (number|string):
  - `1` or `13` => first trinket slot
  - `2` or `14` => second trinket slot
  - Any other number => treat as item ID to match against trinket slots
  - String => item name (case-insensitive) to match against trinket slots

**Returns:**
- If no matching trinket is equipped in slots 13/14: returns `-1`
- Otherwise: a cooldown detail table with the same structure as `GetSpellIdCooldown` / `GetItemIdCooldown`

**Example:**
```lua
-- Get cooldown for first trinket slot
local cd = GetTrinketCooldown(1)
if cd ~= -1 and cd.isOnCooldown == 0 then
    print("Trinket ready")
end

-- Check by name
local cd = GetTrinketCooldown("Royal Seal of Eldre'Thalas")
if cd ~= -1 then
    print("Remaining: " .. cd.cooldownRemainingMs .. "ms")
end
```

#### UseTrinket(slot|itemIdOrName, [target])
Uses a trinket from the equipped trinket slots (13 and 14 only).

**Parameters:**
- `slot|itemIdOrName` (number|string):
  - `1` or `13` => use first trinket slot
  - `2` or `14` => use second trinket slot
  - Any other number => treat as item ID to find in trinket slots
  - String => item name (case-insensitive) to find in trinket slots
- `target` (optional, string|number): Unit token or GUID. If omitted, uses `LockedTargetGuid` if set; otherwise falls back to active player GUID.

**Returns:**
- `1` if the trinket was found and `CGItem_C::Use(...)` returned non-zero
- `0` if the trinket was found but use returned zero
- `-1` if no matching trinket was found in slots 13/14

**Examples:**
```lua
-- Use first trinket slot
UseTrinket(1)
-- Use second trinket slot on current target
UseTrinket(2, "target")
-- Use by item id if present in either trinket slot
UseTrinket(18406)
-- Use by name
UseTrinket("Royal Seal of Eldre'Thalas")
```

#### GetSpellIdForName(spellName)
Returns:

1st param: the max rank spell id for a spell name if it exists in your spellbook.  Returns 0 if the spell is not in your spellbook.

Examples:
```
/run local spellId=GetSpellIdForName("Frostbolt");print(spellId)
/run local spellId=GetSpellIdForName("Frostbolt(Rank 1)");print(spellId)
```

#### GetSpellNameAndRankForId(id)
Returns:

1st param: the spell name for a spell id
2nd param: the spell rank for a spell id as a string such as "Rank 1"

Examples:
```
/run local spellName,spellRank=GetSpellNameAndRankForId(116);print(spellName);print(spellRank)
prints "Frostbolt" and "Rank 1"
```

#### GetSpellSlotTypeIdForName(spellName)
Returns:

1st param: the 1 indexed (lua calls expect this) spell slot number for a spell name if it exists in your spellbook.  Returns 0 if the spell is not in your spellbook.
2nd param: the book type of the spell, either "spell", "pet" or "unknown".
3rd param: the spell id of the spell.  Returns 0 if the spell is not in your spellbook.

Examples:
```
/run local slot, bookType, spellId=GetSpellSlotTypeIdForName("Frostbolt");print(slot);print(bookType);print(spellId)
```

#### GetNampowerVersion()
Returns the current version of Nampower split into major, minor and patch numbers.

So if version was v2.8.6 it would return 2, 8, 6 as integers.

Examples:
```
/run local major, minor, patch=GetNampowerVersion();print(major);print(minor);print(patch)
```

The previous version of this `GetSpellSlotAndTypeForName` was removed as it was returning a 0 indexed slot number which was confusing to use in lua.

#### GetItemLevel(itemId)
Returns the item level of an item.  Returns an error if the item id is invalid.

Examples:
```
/run local itemLevel=GetItemLevel(22589);print(itemLevel)
should print 90 for atiesh
```

#### GetItemIconTexture(displayInfoId)
Returns the texture path for an item given its display info ID. Returns nil if the texture is not found or is the question mark placeholder texture.

**Parameters:**
- `displayInfoId` (number): The item's display info ID (can be obtained from `GetItemStatsField(itemId, "displayInfoID")`)

**Returns:**
- The texture path string (e.g., "Interface\\Icons\\INV_Sword_04"), or nil if not found

**Examples:**
```lua
-- Get texture for an item
local displayInfoId = GetItemStatsField(19019, "displayInfoID")
local texture = GetItemIconTexture(displayInfoId)
if texture then
    print("Texture: " .. texture)
else
    print("No texture found")
end
```

#### GetSpellIconTexture(spellIconId)
Returns the texture path for a spell given its spell icon ID. Returns nil if the texture is not found or is the question mark placeholder texture.

**Parameters:**
- `spellIconId` (number): The spell's icon ID (can be obtained from `GetSpellRecField(spellId, "spellIconID")`)

**Returns:**
- The texture path string with `Interface\Icons\` prefix (e.g., "Interface\\Icons\\Spell_Frost_FrostBolt02"), or nil if not found

**Examples:**
```lua
-- Get texture for Frostbolt
local spellIconId = GetSpellRecField(116, "spellIconID")
local texture = GetSpellIconTexture(spellIconId)
if texture then
    print("Texture: " .. texture)
else
    print("No texture found")
end
```

#### ChannelStopCastingNextTick()
Will stop channeling early on the next tick if you have queue channeling spells enabled and try to cast a spell before the next tick (didn't know how to cancel channels without casting another spell).  Uses your ChannelLatencyReductionPercentage to determine when to stop the channel.

---

### Utility Functions

#### DisenchantAll(itemIdOrName, [includeSoulbound]) or DisenchantAll(quality, [includeSoulbound])
Automatically disenchants items in your inventory. Can disenchant a specific item by ID/name, or all weapons and armor of a specified quality.

**⚠️ WARNING ⚠️**
**THIS FUNCTION WILL AUTOMATICALLY DISENCHANT ITEMS WITHOUT CONFIRMATION!**
- **Use at your own risk** - there is no undo for disenchanting
- Only disenchants items from **player inventory bags (backpack and bags 1-4)**
- **Equipped items, bank items, and keyring are PROTECTED** - will not be touched
- **Quest items are ALWAYS protected** regardless of settings
- **Soulbound items are protected by default** (can be overridden with optional parameter)
- Make sure you have the Disenchant spell and the items are disenchantable before using
- **Always double-check your bags** before running this command

**Parameters:**

**Mode 1: Disenchant by Item ID or Name**
- `itemIdOrName` (number|string): Item ID (number) or item name (string)
  - Disenchants all copies of the specified item found in your bags
  - Works on any disenchantable item type (weapons, armor, etc.)
- `includeSoulbound` (number, optional): Pass any non-zero value (e.g., `1`) to include soulbound items (defaults to `0`)

**Mode 2: Disenchant by Quality** *(weapons and armor only)*
- `quality` (string): Can be a single quality or combination (pipe-separated):
  - `"greens"` - Disenchants all uncommon (green) quality weapons and armor
  - `"blues"` - Disenchants all rare (blue) quality weapons and armor
  - `"purples"` - Disenchants all epic (purple) quality weapons and armor
  - `"greens|blues"` - Disenchants both greens and blues
  - `"blues|purples"` - Disenchants both blues and purples
  - `"greens|blues|purples"` - Disenchants greens, blues, and purples
  - Only affects **weapons** (class 2) and **armor** (class 4)
- `includeSoulbound` (number, optional): Pass any non-zero value (e.g., `1`) to include soulbound items (defaults to `0`)

**Returns:**
- `1` if the first disenchant succeeded
- `0` if no matching items were found or the disenchant failed

**Behavior:**
- Searches player inventory bags (backpack and bags 1-4) only - **equipped items, bank, and keyring are protected**
- Finds the first matching item in your inventory
- Displays a chat message showing which item is being disenchanted
- Casts Disenchant spell on that item
- Automatically continues disenchanting matching items every 5 seconds
- Stops when no more matching items are found or an error occurs
- Displays completion or error messages in chat

**Examples:**
```lua
-- Disenchant all green weapons and armor in your bags (excluding soulbound)
DisenchantAll("greens")

-- Disenchant all blue weapons and armor including soulbound items
DisenchantAll("blues", 1)

-- Disenchant all purple (epic) weapons and armor
DisenchantAll("purples")

-- Disenchant both greens and blues
DisenchantAll("greens|blues")

-- Disenchant blues and purples including soulbound items
DisenchantAll("blues|purples", 1)

-- Disenchant all greens, blues, and purples
DisenchantAll("greens|blues|purples")

-- Disenchant a specific item by ID (excluding soulbound)
DisenchantAll(12345)

-- Disenchant a specific item by name including soulbound items
DisenchantAll("Glowing Brightwood Staff", 1)
```

**Important Notes:**
- The function runs continuously until all matching items are disenchanted
- **Only searches inventory bags (backpack and bags 1-4)** - equipped items, bank, and keyring are protected
- When using quality mode ("greens"/"blues"/"purples" or combinations), only weapons and armor are affected
- When using item ID/name mode, any disenchantable item can be targeted
- Make sure you have enough bag space for the disenchanting materials
- The function will stop if you run out of matching items or if the disenchant spell fails
- Displays chat messages: "Disenchanting [Item Link] move during cast to cancel.", "No more items to disenchant.", and "Disenchant interrupted or failed."
- **REVIEW YOUR BAGS CAREFULLY BEFORE USE** - disenchanting cannot be undone!
