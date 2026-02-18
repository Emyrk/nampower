# v2.0.0 Changes

Added spell queuing, automatic retry on error, and quickcasting with lots of customization.  

Some other key improvements over Namreeb's version:
- Using a buffer to avoid server rejections from casting too quickly (namreeb's uses 0 buffer).  See 'Why do I need a buffer?' below for more info.
- Using high_resolution_clock instead of GetTickCount for faster timing on when to start casts
- Fix broken cast animations when casting spells back to back

## Compatability with other addons
Queuing can cause issues with some addons that also manage spell casting.  Quickheal/Healbot/Quiver do not work well with queuing.  Check github issues for other potential incompatibilities.
If someone rewrites these addons to use guids from superwow that would likely fix all issues.

Additionally, if you use pfui mouseover macros there is a timing issue that can occur causing it to target yourself instead of your mouseover target.  <b>If you have superwow and latest pfui issue is fixed</b>.

<b>Notgrid</b> mouseover needs to be updated to take advantage of superwow the default version won't work well with queuing.

<b>If you use healcomm can replace all instances of it in your addons with this version to work well with queuing </b> https://github.com/MarcelineVQ/LunaUnitFrames/blob/TurtleWoW/libs/HealComm-1.0/HealComm-1.0.lua (requires superwow).

If all else fails can turn off queuing for a specific macro like so depending on the spell being cast:
```
/run SetCVar("NP_QueueCastTimeSpells", "0")
/run SetCVar("NP_QueueInstantSpells", "0")
/pfcast YOUR_SPELL
/run SetCVar("NP_QueueCastTimeSpells", "1")
/run SetCVar("NP_QueueInstantSpells", "1")
```
## Installation
Grab the latest nampower.dll from https://gitea.com/avitasia/nampower/releases and place in the same directory as WoW.exe.  You can also get the helper addon mentioned below and place that in Interface/Addons.

<b>You will need launch the game with a launcher like Vanillafixes https://github.com/hannesmann/vanillafixes or Unitxp https://github.com/allfoxwy/UnitXP_SP3</b> to actually have the nampower dll get loaded.

If you would prefer to compile yourself you will need to get:
- boost 1.80 32 bit from https://www.boost.org/users/history/version_1_80_0.html
- hadesmem from https://github.com/namreeb/hadesmem

CMakeLists.txt is currently looking for boost at `set(BOOST_INCLUDEDIR "C:/software/boost_1_80_0")` and hadesmem at `set(HADESMEM_ROOT "C:/software/hadesmem-v142-Debug-Win32")`.  Edit as needed.

## Configuration

### Configure with addon
There is a companion addon to make it easy to check/change the settings in game.  You can download it here - [nampowersettings](https://gitea.com/avitasia/nampowersettings).

### Manual Configuration
The following CVars control the behavior of the spell queuing system:

You can access CVars in game with `/run DEFAULT_CHAT_FRAME:AddMessage(GetCVar("CVarName"))`<br>
and set them with `/run SetCVar("CVarName", "Value")`

You can also just place them in your Config.wtf file in your WTF folder.  If they are the default value they will not be written to the file.
Example:
```
SET EnableMusic "0"
SET MasterSoundEffects "0"

SET NP_QuickcastTargetingSpells "1"
SET NP_SpellQueueWindowMs "1000"
SET NP_TargetingQueueWindowMs "1000"
```

- `NP_QueueCastTimeSpells` - Whether to enable spell queuing for spells with a cast time.  0 to disable, 1 to enable. Default is 1.
- `NP_QueueInstantSpells` - Whether to enable spell queuing for instant cast spells tied to gcd.  0 to disable, 1 to enable.  Default is 1.
- `NP_QueueChannelingSpells` - Whether to enable channeling spell queuing as well as whether to allow any queuing during channels.  0 to disable, 1 to enable. Default is 1.
- `NP_QueueTargetingSpells` - Whether to enable terrain targeting spell queuing.  0 to disable, 1 to enable. Default is 1.
- `NP_QueueOnSwingSpells` - Whether to enable on swing spell queuing.  0 to disable, 1 to enable. Default is 0 (changed with 1.17.2 due to changes to on swing spells).
- `NP_QueueSpellsOnCooldown` - Whether to enable queuing for spells coming off cooldown.  0 to disable, 1 to enable. Default is 1.

- `NP_InterruptChannelsOutsideQueueWindow` - Whether to allow interrupting channels (the original client behavior) when trying to cast a spell outside the channeling queue window. Default is 0.

- `NP_SpellQueueWindowMs` - The window in ms before a cast finishes where the next will get queued. Default is 500.
- `NP_OnSwingBufferCooldownMs` - The cooldown time in ms after an on swing spell before you can queue on swing spells. Default is 500.
- `NP_ChannelQueueWindowMs` - The window in ms before a channel finishes where the next will get queued. Default is 1500.
- `NP_TargetingQueueWindowMs` - The window in ms before a terrain targeting spell finishes where the next will get queued. Default is 500.
- `NP_CooldownQueueWindowMs` - The window in ms of remaining cooldown where a spell will get queued instead of failing with 'Spell not Ready Yet'. Default is 250.


- `NP_MinBufferTimeMs` - The minimum buffer delay in ms added to each cast (covered more below).  The dynamic buffer adjustments will not go below this value. Default is 55.
- `NP_NonGcdBufferTimeMs` - The buffer delay in ms added AFTER each cast that is not tied to the gcd. Default is 100.
- `NP_MaxBufferIncreaseMs` - The maximum amount of time in ms to increase the buffer by when the server rejects a cast. This prevents getting too long of a buffer if you happen to get a ton of rejections in a row. Default is 30.


- `NP_RetryServerRejectedSpells` - Whether to retry spells that are rejected by the server for these reasons: SPELL_FAILED_ITEM_NOT_READY, SPELL_FAILED_NOT_READY, SPELL_FAILED_SPELL_IN_PROGRESS.  0 to disable, 1 to enable. Default is 1.
- `NP_QuickcastTargetingSpells` - Whether to enable quick casting for ALL spells with terrain targeting.  This will cause the spell to instantly cast on your cursor without waiting for you to confirm the targeting circle.  Queuing targeting spells will use quickcasting regardless of this value.  0 to disable, 1 to enable. Default is 0.
- `NP_QuickcastOnDoubleCast` - Whether to allow casting targeting spells by attempting to cast them twice, as opposed to the default client behavior which cancels the targeting indicator on double cast.  This provides an alternative way to quickcast targeting spells without enabling it for all targeting spells.  0 to disable, 1 to enable. Default is 0.
- `NP_ReplaceMatchingNonGcdCategory` - Whether to replace any queued non gcd spell when a new non gcd spell with the same StartRecoveryCategory is cast (more explanation below).  0 to disable, 1 to enable. Default is 0.
- `NP_OptimizeBufferUsingPacketTimings` - Whether to attempt to optimize your buffer using your latency and server packet timings (more explanation below).  0 to disable, 1 to enable. Default is 0.

- `NP_PreventRightClickTargetChange` - Whether to prevent right-clicking from changing your current target when in combat.  If you don't have a target right click will still change your target even with this on.  This is mainly to prevent accidentally changing targets in combat when trying to adjust your camera.  0 to disable, 1 to enable. Default is 0.

- `NP_PreventRightClickPvPAttack` - Whether to prevent right-clicking on PvP flagged players to avoid accidental PvP attacks.  0 to disable, 1 to enable. Default is 0.

- `NP_DoubleCastToEndChannelEarly` - Whether to allow double casting a spell within 350ms to end channeling on the next tick.  Takes into account your ChannelLatencyReductionPercentage.  0 to disable, 1 to enable. Default is 0.

- `NP_SpamProtectionEnabled` - Whether to enable spam protection functionality that blocks spamming spells while waiting for the server to respond to your initial cast due to issues spamming can cause.  0 to disable, 1 to enable. Default is 1.

- `NP_PreventMountingWhenBuffCapped` - Whether to prevent mounting when you have 32 buffs (buff capped) and are not already mounted. This prevents the issue where you mount but cannot dismount because the mount aura fails to apply due to the buff cap. When blocked, displays an error message. 0 to disable, 1 to enable. Default is 1.

- `NP_EnableAuraCastEvents` - Whether to enable AURA_CAST_ON_SELF and AURA_CAST_ON_OTHER events. 0 to disable, 1 to enable. Default is 0.

- `NP_EnableAutoAttackEvents` - Whether to enable AUTO_ATTACK_SELF and AUTO_ATTACK_OTHER events. 0 to disable, 1 to enable. Default is 0.

- `NP_EnableSpellStartEvents` - Whether to enable SPELL_START_SELF and SPELL_START_OTHER events. 0 to disable, 1 to enable. Default is 0.

- `NP_EnableSpellGoEvents` - Whether to enable SPELL_GO_SELF and SPELL_GO_OTHER events. 0 to disable, 1 to enable. Default is 0.

- `NP_EnableSpellHealEvents` - Whether to enable SPELL_HEAL_BY_SELF, SPELL_HEAL_BY_OTHER, and SPELL_HEAL_ON_SELF events. 0 to disable, 1 to enable. Default is 0.

- `NP_EnableSpellEnergizeEvents` - Whether to enable SPELL_ENERGIZE_BY_SELF, SPELL_ENERGIZE_BY_OTHER, and SPELL_ENERGIZE_ON_SELF events. 0 to disable, 1 to enable. Default is 0.

- `NP_ChannelLatencyReductionPercentage` - The percentage of your latency to subtract from the end of a channel duration to optimize cast time while hopefully not losing any ticks (more explanation below). Default is 75.

- `NP_NameplateDistance` - The distance in yards to display nameplates.  Defaults to whatever was set by the game or vanilla tweaks.

## Existing Lua Changes

### Improved flexibility on spellbook Lua functions
These built-in Lua spell APIs now accept any of the following as their first argument: 1) spell slot (original behavior), 2) spell name, or 3) `spellId:number`. 

Name and spellId lookups are cached internally and validated against current spellbook contents before reuse so you don't have to worry about performance implications or issues after respec'ing.

See examples below for differences in how BOOKTYPE works.

Functions: `GetSpellTexture`, `GetSpellName`, `GetSpellCooldown`, `GetSpellAutocast`, `ToggleSpellAutocast`, `PickupSpell`, `CastSpell`, `IsCurrentCast`, `IsSpellPassive`.

Examples:
```
/run print(GetSpellTexture(1, "spell")) -- booktype required
/run print(GetSpellTexture("spellId:25978")) -- defaults to BOOKTYPE_SPELL
/run print(GetSpellTexture("spellId:6268", "pet")) -- "pet" needed for pet spells
/run print(GetSpellTexture("Fireball")) -- name search
```


### CSimpleFrame:GetName() Enhancement
The built-in `CSimpleFrame:GetName()` method is hooked to support an optional argument for retrieving the GUID of the unit associated with a nameplate frame.

- `frame:GetName()` - Returns the frame name (original behavior).
- `frame:GetName(1)` - Returns the GUID string (e.g., `"0x0000000000000001"`) of the unit associated with the nameplate frame.

Example:
```
/run local f = GetMouseFocus(); if f then DEFAULT_CHAT_FRAME:AddMessage(f:GetName(1) or "no guid") end
```

## Custom Lua Functions

For complete documentation of all custom Lua functions added by Nampower, see **[SCRIPTS.md](SCRIPTS.md)**.

This includes functions for:
- Spell/item/unit information (GetItemStats, GetSpellRec, GetUnitData, etc.)
- Spell casting and queuing (QueueSpellByName, QueueScript, etc.)
- Cast information (GetCastInfo, GetCurrentCastingInfo)
- Cooldown tracking (GetSpellIdCooldown, GetItemIdCooldown), including item metadata on cooldown detail tables
- Inventory helpers (GetTrinketCooldown, GetTrinkets, GetAmmo)
- Aura duration tracking and cancel helpers (GetPlayerAuraDuration, CancelPlayerAuraSlot, CancelPlayerAuraSpellId)
- Spell duration lookup (GetSpellDuration) - returns channel duration for channeling spells and the first aura effect duration for non-channeling spells
- Player movement state queries (PlayerIsMoving, PlayerIsRooted, PlayerIsSwimming)
- Talent helpers (LearnTalentRank)
- Spell lookups and utilities

`LearnTalentRank(talentPage, talentIndex, rank)` learns a specific talent rank directly by tab/index.
Valid ranges are: `talentPage` = `1-3`, `talentIndex` = `1-32`, `rank` = `1-5`.

For unit data APIs, object-reference GUID fields are returned as strings (hex format) rather than numbers to avoid Lua 64-bit precision issues.
This applies to both `GetUnitData` and `GetUnitField` for fields such as: `charm`, `summon`, `charmedBy`, `summonedBy`, `createdBy`, `target`, `persuaded`, and `channelObject`.

`CancelPlayerAuraSpellId(spellId, [ignoreMissing])` supports an optional second parameter where `1` skips the aura-slot presence check (useful for buff-capped cases) and `0`/omitted keeps the default check.

Cooldown detail tables now also expose `itemId`, `itemHasActiveSpell`, and `itemActiveSpellId` alongside the existing per-category timing data.

Use `GetTrinkets([copy])` to enumerate equipped trinkets and bagged trinkets (backpack/bags 1-4) with `itemId`, `trinketName`, `texture`, `itemLevel`, `bagIndex` (nil when equipped), and 1-based `slotIndex`. It reuses cached tables by default; pass `1` (or any truthy value) to force a fresh copy.

## Custom Events

For complete documentation of all custom events added by Nampower, see **[EVENTS.md](EVENTS.md)**.

Available events:
- SPELL_QUEUE_EVENT - Fires when spells are queued or dequeued
- SPELL_CAST_EVENT - Fires when you cast a spell with additional information. Triggered when you start casting a spell in the client before it is sent to the server.
- SPELL_START_SELF and SPELL_START_OTHER - Triggered by the server to notify that a spell with a cast time has begun.
- SPELL_GO_SELF and SPELL_GO_OTHER - Triggered by the server to indicate a spell completed casting.
- SPELL_DAMAGE_EVENT_SELF and SPELL_DAMAGE_EVENT_OTHER - Combat damage events
- Buff/Debuff Events (BUFF_ADDED_SELF, BUFF_REMOVED_SELF, BUFF_UPDATE_DURATION_SELF, etc.)
- AURA_CAST_ON_SELF and AURA_CAST_ON_OTHER - Aura application events (fires once per aura effect per target; "Self" fires when the aura lands on the active player, including self-cast with no explicit target; includes aura metadata + amplitude/misc + aura cap bitfield for buff/debuff slots). Handles single-target and multi-target (AOE) spells. Some auras don't have spell effects and won't trigger this; use BUFF/DEBUFF gains for those. Set `NP_EnableAuraCastEvents=1` to enable. See EVENTS.md for details on multi-target behavior and known limitations.
- AUTO_ATTACK_SELF and AUTO_ATTACK_OTHER - Auto attack round events (fires when auto attack hits are processed; includes damage, hit info, victim state, absorb/resist/block amounts). Set `NP_EnableAutoAttackEvents=1` to enable. See EVENTS.md for details.
- SPELL_HEAL_BY_SELF, SPELL_HEAL_BY_OTHER, and SPELL_HEAL_ON_SELF - Spell healing events (fires when spell heals are processed; includes target, caster, spell ID, amount, critical flag, and periodic flag). Set `NP_EnableSpellHealEvents=1` to enable. See EVENTS.md for details.
- SPELL_ENERGIZE_BY_SELF, SPELL_ENERGIZE_BY_OTHER, and SPELL_ENERGIZE_ON_SELF - Spell energize events (fires when power is restored via spells; includes target, caster, spell ID, power type, amount, and periodic flag). Set `NP_EnableSpellEnergizeEvents=1` to enable. See EVENTS.md for details.
- UNIT_DIED - Fires when a unit dies

## Bug Reporting
If you encounter any bugs please report them in the issues tab.  Please include the nampower_debug.txt file in the same directory as your WoW.exe to help me diagnose the issue.  If you are able to reproduce the bug please include the steps to reproduce it.  In a future version once bugs are ironed out I'll make logging optional.

## FAQ & Additional Info

### How does queuing work?
Trying to cast a spell within the appropriate window before your current spell finishes will queue your new spell.  
The spell will be cast as soon as possible after the current spell finishes.  

There are separate configurable queue windows for:
- Normal spells
- On swing spells (the window functions as a cooldown instead where you cannot immediately double queue on swing spells so that I don't have to track swing timers)
- Channeling spells
- Spells with terrain targeting

There are 3 separate queues for the following types of spells: GCD(max size:1), non GCD(max size:6), and on-hit(max size:1).

Additionally the queuing system will ignore spells with any of the following attributes/effects to avoid issues with tradeskills/enchants/other out of combat activities:
- SpellAttributes::SPELL_ATTR_TRADESPELL
- SpellEffects::SPELL_EFFECT_TRADE_SKILL
- SpellEffects::SPELL_EFFECT_ENCHANT_ITEM
- SpellEffects::SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY
- SpellEffects::SPELL_EFFECT_CREATE_ITEM
- SpellEffects::SPELL_EFFECT_OPEN_LOCK
- SpellEffects::SPELL_EFFECT_OPEN_LOCK_ITEM

### Why do I need a buffer?
From my own testing it seems that a buffer is required on spells to avoid "This ability isn't ready yet"/"Another action in progress" errors.  
By that I mean that if you cast a 1.5 second cast time spell every 1.5 seconds without your ping changing you will occasionally get
errors from the server and your cast will get rejected.  If you have 150ms+ ping this can be very punishing.

I believe this is related to the server tick during which incoming spells are processed.  There is logic to
subtract the server processing time from your gcd in vmangos but other servers do not appear to be doing this.

To compensate for what seems to be a 50ms server tick the default buffer in nampower.cfg is 55ms.  If you are close to the server
you can experiment with lowering this value.  You will occasionally get errors but if they are infrequent enough for you
the time saved will be worth it.

Non gcd spells also seem to be affected by this.  I suspect that only one spell can be processed per server tick.  
This means that if you try to cast 2 non gcd spells in the same server tick only one will be processed.  
To avoid this happening there is `NP_NonGcdBufferTimeMs` which is added after each non gcd spell.  There might be more to
it than this as using the normal buffer of 55ms was still resulting in skipped casts for me.  I found 100ms to be a safe value.

### GCD Spells
Only one gcd spell can be queued at a time.  Pressing a new gcd spell will replace any existing queued gcd spell.

As of 5/13/2025 the server tick is now subtracted from the gcd timer so a buffer is no longer required for spells with a cast time at least ~50ms less than their gcd :)

### Non GCD Spells
Non gcd spells have special handling.  You can queue up to 6 non gcd spells, 
and they will execute in the order queued with `NP_NonGcdBufferTimeMs` delay after each of them to help avoid server rejection.  
The non gcd queue always has priority over queued normal spells.  
You can only queue a given spellId once in the non gcd queue, any subsequent attempts will just replace the existing entry in the queue.

`NP_ReplaceMatchingNonGcdCategory` will cause non gcd spells with the same non zero StartRecoveryCategory to replace each other in the queue.  
The vast majority of spells not on the gcd have category '0' so it ignores them to avoid causing issues.  
One notable exception is shaman totems that were changed to have separate categories according to their elements.

This can be useful if you want to change your mind about the non gcd spell you have queued.  For example, if you queue a mana potion and decide you want to use LIP instead last minute.

### On hit Spells
Only one on hit spell can be queued at a time.  Pressing a new on hit spell will replace any existing queued on hit spell.  
On hit spells have no effect on the gcd or non gcd queues as they are handled entirely separately and are resolved by your auto attack.

### Channeling Spells
Channeling spells function differently than other spells in that the channel in the client actually begins when you receive 
the CHANNEL_START packet from the server.  This means the client channel is happening 1/2 your latency after the server channel 
and that server tick delay is already included in the cast, whereas regular spells are the other way around (the client is ahead of the server).  

From my testing it seems that you can usually subtract your full latency from the end of the channel duration without losing
any ticks.  Since your latency can vary it is safer to do a percentage of your latency instead to minimize the chance of 
having a tick cut off.  This is controlled by the cvar `NP_ChannelLatencyReductionPercentage` which defaults to 75.

Channeling spells can be interrupted outside the channel queue window by casting any spell if `NP_InterruptChannelsOutsideQueueWindow` is set to 1.  During the channel queue window
you cannot interrupt the channel unless you turn off `NP_QueueChannelingSpells`.  You can always move to interrupt a channel at any time.

### Spells on Cooldown
If using `NP_QueueSpellsOnCooldown` when you attempt to cast a spell that has a remaining cooldown of less than `NP_CooldownQueueWindowMs` it will be queued instead of failing with 'Spell not Ready Yet'.
There is a separate queue of size 1 for normal spells and non gcd spells.  If something is in either of these cooldown queues and you try to cast a spell that is not on cooldown it will be cast immediately and clear the appropriate cooldown queue.

For example, if Fire Blast is on cooldown and I queue it and then try to cast Fireball it will cast Fireball immediately and Fire Blast will not get automatically cast anymore.

This currently doesn't work for item cooldowns as they work differently, will add in the future.

### NP_OptimizeBufferUsingPacketTimings
This feature will attempt to optimize your buffer on individual casts using your latency and server packet timings.  
After you begin to cast a spell you will get a cast result packet back from the server letting you know if the cast was successful.
The time between when you send your start cast packet and when you receive the cast result packet consists of:
- The time it takes for your packet to reach the server
- The time it takes for the server to process the packet (see Why do I need a buffer?)
- The time it takes for the server to send the result packet back to you
- Other delays I'm not sure about like the time it takes for the client to process packets from the server due to being single threaded

If we take this 'Spell Response Time' and subtract your regular latency from it, we should be able to get a rough idea of the time it
took for the server to process the cast.  If that time was less than your current default buffer we can use that time as the new buffer
for the next cast only.  In theory if it is more than your current buffer we should also use it, but in 
practice it seems to regularly be way larger than expected and using the default buffer doesn't result in an error.

Due to this delay varying wildly in testing I'm unsure how reliable this technique is.  It needs more testing
and a better understanding of all the factors introducing delay.  It is disabled by default for now.

# v1.0.0 Changes

Now looks for a nampower.cfg file in the same directory with two lines:

1.  The first line should contain the "buffer" time between each cast.  This is the amount of time to delay casts to ensure you don't try to cast again too early due to server/packet lag and get rejected by the server with a "This ability isn't ready yet" error.  For 150ms I found 30ms to be a reasonable buffer.
2.  The second line is the window in ms before each cast during which nampower will delay cast attempts to send them to the server at the perfect time.  So 300 would mean if you cast anytime in the 300ms window before your next optimal cast your cast will be sent at the idea time.  This means you don't have to spam cast as aggressively.  This feature will cause a small stutter because it is pausing your UI (I couldn't findSpellId a better way to do this but I'm sure one exists now with superwow) so if you don't like that set this to 0.

# Namreeb readme

**Please consider donating if you use this tool. - Namreeb**

[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=QFWZUEMC5N3SW)

An auto stop-cast tool for World of Warcraft 1.12.1.5875 (for Windows)

There is a design flaw in this version of the client.  A player is not allowed to cast a
second spell until after the client receives word of the completion of the previous spell.
This means that in addition to the cast time, you have to wait for the time it takes a
message to arrive from the server.  For many U.S. based players connected to E.U. based
realms, this can result in approximately a 20% drop in effective DPS.

Consider the following timeline, assuming a latency of 200ms.

* t = 0, the player begins casting fireball (assume a cast time of one second or 1000ms)
       and spell cast message is sent to the server.  at this time, the client places
       a lock on itself, preventing the player from requesting another spell cast.
* t = 200, the spell cast message arrives at the server, and the spell cast begins
* t = 1200, the spell cast finishes and a finish message is sent to the client
* t = 1400, the client receives the finish message and removes the lock it had placed
          1400ms ago.
		  
In this scenario, a 1000ms spell takes 1400ms to cast.  This tool will work around that
design flaw by altering the client behavior to not wait for the server to acknowledge
anything.

## Using ##

If you use my launcher, known as [wowreeb](https://github.com/namreeb/wowreeb), you can add
the following line to a `<Realm>` block to tell the launcher to include this tool:

```xml
    <DLL Path="c:\path\to\nampower.dll" Method="Load" />
```

To launch with the built-in launcher, run loader.exe -p c:\path\to\wow.exe (or just loader.exe
with it inside the main wow folder)
