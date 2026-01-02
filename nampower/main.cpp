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

#include "logging.hpp"
#include "offsets.hpp"
#include "game.hpp"
#include "main.hpp"
#include "spellevents.hpp"
#include "spellcast.hpp"
#include "misc_scripts.hpp"
#include "spell_scripts.hpp"
#include "spellchannel.hpp"
#include "helper.hpp"
#include "items.hpp"
#include "item_scripts.hpp"
#include "cooldown_scripts.hpp"
#include "item_scripts.hpp"
#include "auras.hpp"
#include "lua_refs.hpp"

#include <cstdint>
#include <memory>
#include <atomic>

#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <windows.h>
#include <sys/stat.h>

BOOL WINAPI DllMain(HINSTANCE, uint32_t, void *);

namespace Nampower {
    uint32_t gLastErrorTimeMs;
    uint32_t gLastBufferIncreaseTimeMs;
    uint32_t gLastBufferDecreaseTimeMs;

    uint32_t gBufferTimeMs; // adjusts dynamically depending on errors

    uint32_t gDisenchantItemId = 0;
    uint32_t gDisenchantQuality = 0;  // 0 = unset
    bool gDisenchantIncludeSoulbound = false;
    uint32_t gNextDisenchantTimeMs = 0;

    bool gForceQueueCast;
    bool gNoQueueCast;
    bool gQueuesProcessed;

    bool lastCastUsedServerDelay;

    uint64_t gNextCastId = 1;

    uint32_t gRunningAverageLatencyMs;
    uint32_t gLastServerSpellDelayMs;

    hadesmem::PatchDetourBase *castSpellDetour;

    UserSettings gUserSettings;

    LastCastData gLastCastData;
    CastData gCastData;

    CastSpellParams gLastNormalCastParams;
    CastSpellParams gLastOnSwingCastParams;

    CastQueue gNonGcdCastQueue = CastQueue(6);

    CastQueue gCastHistory = CastQueue(30);


    std::unique_ptr<hadesmem::PatchDetour<SysMsgInitializeT> > gSysMsgInitDetour;
    std::unique_ptr<hadesmem::PatchDetour<LoadScriptFunctionsT> > gLoadScriptFunctionsDetour;
    std::unique_ptr<hadesmem::PatchDetour<FrameScript_CreateEventsT> > gCreateEventsDetour;
    std::unique_ptr<hadesmem::PatchDetour<FramescriptSetEventCountT> > gSetEventCountDetour;

    std::unique_ptr<hadesmem::PatchDetour<SetCVarT> > gSetCVarDetour;
    std::unique_ptr<hadesmem::PatchDetour<CGSpellBook_CastSpellT> > gCGSpellBook_CastSpellDetour;
    std::unique_ptr<hadesmem::PatchDetour<Spell_C_CastSpellT> > gCastDetour;
    std::unique_ptr<hadesmem::PatchDetour<SendCastT> > gSendCastDetour;
    std::unique_ptr<hadesmem::PatchDetour<CancelSpellT> > gCancelSpellDetour;
    std::unique_ptr<hadesmem::PatchDetour<SignalEventT> > gSignalEventDetour;
    std::unique_ptr<hadesmem::PatchDetour<Spell_C_SpellFailedT> > gSpellFailedDetour;
    std::unique_ptr<hadesmem::PatchDetour<Spell_C_GetSpellModifiersT> > gSpell_C_GetSpellModifiersDetour;
    std::unique_ptr<hadesmem::PatchDetour<Spell_C_GetSpellRadiusT> > gSpell_C_GetSpellRadiusDetour;
    std::unique_ptr<hadesmem::PatchRaw> gCastbarPatch;
    std::unique_ptr<hadesmem::PatchDetour<ISceneEndT> > gIEndSceneDetour;
    std::unique_ptr<hadesmem::PatchDetour<Spell_C_GetAutoRepeatingSpellT> > gSpell_C_GetAutoRepeatingSpellDetour;
    std::unique_ptr<hadesmem::PatchDetour<Spell_C_CooldownEventTriggeredT> > gSpell_C_CooldownEventTriggeredDetour;
    std::unique_ptr<hadesmem::PatchDetour<SpellGoT> > gSpellGoDetour;
    std::unique_ptr<hadesmem::PatchDetour<LuaScriptT> > gSpellTargetUnitDetour;
    std::unique_ptr<hadesmem::PatchDetour<LuaScriptT> > gSpellStopCastingDetour;
    std::unique_ptr<hadesmem::PatchDetour<OnSpriteRightClickT> > gOnSpriteRightClickDetour;
    std::unique_ptr<hadesmem::PatchDetour<Spell_C_HandleSpriteClickT> > gSpell_C_HandleSpriteClickDetour;
    std::unique_ptr<hadesmem::PatchDetour<Spell_C_HandleTerrainClickT> > gSpell_C_HandleTerrainClickDetour;
    std::unique_ptr<hadesmem::PatchDetour<CGWorldFrame_OnLayerTrackTerrainT> > gCGWorldFrame_OnLayerTrackTerrainDetour;
    std::unique_ptr<hadesmem::PatchDetour<Spell_C_TargetSpellT> > gSpell_C_TargetSpellDetour;

    std::unique_ptr<hadesmem::PatchDetour<PacketHandlerT> > gSpellCooldownDetour;
    std::unique_ptr<hadesmem::PatchDetour<PacketHandlerT> > gSpellDelayedDetour;
    std::unique_ptr<hadesmem::PatchDetour<PacketHandlerT> > gCastResultHandlerDetour;
    std::unique_ptr<hadesmem::PatchDetour<PacketHandlerT> > gSpellFailedHandlerDetour;
    std::unique_ptr<hadesmem::PatchDetour<PacketHandlerT> > gSpellChannelStartHandlerDetour;
    std::unique_ptr<hadesmem::PatchDetour<PacketHandlerT> > gSpellChannelUpdateHandlerDetour;
    std::unique_ptr<hadesmem::PatchDetour<PacketHandlerT> > gPlaySpellVisualHandlerDetour;

    std::unique_ptr<hadesmem::PatchDetour<FastCallPacketHandlerT> > gSpellStartHandlerDetour;
    std::unique_ptr<hadesmem::PatchDetour<FastCallPacketHandlerT> > gPeriodicAuraLogHandlerDetour;
    std::unique_ptr<hadesmem::PatchDetour<FastCallPacketHandlerT> > gSpellNonMeleeDmgLogHandlerDetour;

    std::unique_ptr<hadesmem::PatchDetour<CGPlayer_C_OnAttackIconPressedT> > gCGPlayer_C_OnAttackIconPressedDetour;
    std::unique_ptr<hadesmem::PatchDetour<CGActionBar_UseActionT> > gCGActionBar_UseActionDetour;
    std::unique_ptr<hadesmem::PatchDetour<CGUnit_C_OnAuraRemovedT> > gCGUnit_C_OnAuraRemovedDetour;
    std::unique_ptr<hadesmem::PatchDetour<CGUnit_C_OnAuraAddedT> > gCGUnit_C_OnAuraAddedDetour;
    std::unique_ptr<hadesmem::PatchDetour<CGUnit_C_OnAuraAddedStackT> > gCGUnit_C_OnAuraAddedStackDetour;
    std::unique_ptr<hadesmem::PatchDetour<UnitCombatLogUnitDeadT> > gUnitCombatLogUnitDeadDetour;

    std::unique_ptr<hadesmem::PatchDetour<InvalidFunctionPtrCheckT> > gInvalidFunctionPtrCheckDetour;

    std::unique_ptr<hadesmem::PatchDetour<GetSpellSlotFromLuaT> > gGetSpellSlotFromLuaDetour;

    // Flags for one-time initialization
    std::once_flag loadFlag;
    std::once_flag initHooksFlag;

    // Forward declarations for hook functions
    void LoadScriptFunctionsHook(hadesmem::PatchDetourBase *detour);

    void FrameScript_CreateEventsHook(hadesmem::PatchDetourBase *detour, int param_1, uint32_t maxEventId);

    void Framescript_SetEventCountHook(hadesmem::PatchDetourBase *detour, void *thisPtr, void *dummy_edx,
                                       uint32_t count);

    uint32_t GetTime() {
        return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::high_resolution_clock::now().time_since_epoch()).count()) - gStartTime;
    }

    std::string GetHumanReadableTime() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm buf;
        localtime_s(&buf, &in_time_t);

        std::stringstream ss;
        ss << std::put_time(&buf, "%Y-%m-%d %X");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    uint64_t GetWowTimeMs() {
        auto const osGetAsyncTimeMs = reinterpret_cast<GetTimeMsT>(Offsets::OsGetAsyncTimeMs);
        return osGetAsyncTimeMs();
    }

    void LuaCall(const char *code) {
        auto function = (LuaCallT) Offsets::lua_call;
        function(code, "Unused");
    }

    uintptr_t *GetLuaStatePtr() {
        typedef uintptr_t *(__fastcall *GETCONTEXT)(void);
        static auto p_GetContext = reinterpret_cast<GETCONTEXT>(Offsets::lua_state_ptr);
        return p_GetContext();
    }

    void SetTarget(uint64_t target) {
        auto setTargetFn = reinterpret_cast<TargetUnitT>(Offsets::ScriptTargetUnit);
        setTargetFn(&target);
    }

    void SetSelectionTarget(uint64_t target) {
        auto dataStore = CDataStore();
        uint32_t opcode = 317; // CMSG_SET_SELECTION
        dataStore.Put(opcode);
        dataStore.Put(target);

        dataStore.Finalize();

        auto const clientServicesSend = reinterpret_cast<ClientServices_SendT>(Offsets::ClientServices_Send);
        clientServicesSend(&dataStore);
    }

    void SetAttackTarget(uint64_t target) {
        auto dataStore = CDataStore();
        uint32_t opcode = 0x141; // CMSG_ATTACKSWING
        dataStore.Put(opcode);
        dataStore.Put(target);

        dataStore.Finalize();

        auto const clientServicesSend = reinterpret_cast<ClientServices_SendT>(Offsets::ClientServices_Send);
        clientServicesSend(&dataStore);
    }

    void RegisterLuaFunction(char *name, uintptr_t *func) {
        DEBUG_LOG("Registering " << name << " to " << func);
        auto const registerFunction = reinterpret_cast<FrameScript_RegisterFunctionT>(
            Offsets::FrameScript_RegisterFunction);
        registerFunction(name, func);
    }

    // when the game first launches there won't be a cached latency value for 10-20 seconds and this will return 0
    uint32_t GetLatencyMs() {
        auto const getConnection = reinterpret_cast<GetClientConnectionT>(Offsets::GetClientConnection);
        auto const getNetStats = reinterpret_cast<GetNetStatsT>(Offsets::GetNetStats);

        auto connectionPtr = getConnection();

        float bytesIn, bytesOut;
        uint32_t latency;

        getNetStats(connectionPtr, &bytesIn, &bytesOut, &latency);

        // update running average
        if (latency > 0) {
            if (gRunningAverageLatencyMs == 0) {
                // first time
                gRunningAverageLatencyMs = latency;
                // ignore big spikes
            } else if (latency < gRunningAverageLatencyMs * 2) {
                gRunningAverageLatencyMs = (gRunningAverageLatencyMs * 9 + latency) / 10;
            }
        }

        return gRunningAverageLatencyMs;
    }

    uint32_t GetServerDelayMs() {
        // if we have a gLastServerSpellDelayMs that seems reasonable, use it
        // occasionally the server will take a long time to respond to a spell cast, in which case default to gBufferTimeMs
        // if gLastServerSpellDelayMs == 0, then we don't have a valid value for the last cast
        if (gUserSettings.optimizeBufferUsingPacketTimings && !lastCastUsedServerDelay) {
            if (gLastServerSpellDelayMs > 0 && gLastServerSpellDelayMs < 200) {
                lastCastUsedServerDelay = true;
                return gLastServerSpellDelayMs;
            }
        }

        lastCastUsedServerDelay = false;

        return gBufferTimeMs;
    }

    bool InSpellQueueWindow(uint32_t remainingCastTime, uint32_t remainingGcd, bool spellIsTargeting) {
        auto currentTime = GetTime();

        uint32_t queueWindow = 0;

        if (gCastData.channeling) {
            if (gUserSettings.queueChannelingSpells) {
                if (gCastData.cancelChannelNextTick) {
                    return true;
                }

                auto const remainingChannelTime = (gCastData.channelEndMs > currentTime)
                                                      ? gCastData.channelEndMs -
                                                        currentTime
                                                      : 0;
                return remainingChannelTime < gUserSettings.channelQueueWindowMs;
            }
        } else if (spellIsTargeting) {
            queueWindow = gUserSettings.targetingQueueWindowMs;
        } else {
            queueWindow = gUserSettings.spellQueueWindowMs;
        }

        if (remainingCastTime > 0) {
            return remainingCastTime < queueWindow || gForceQueueCast;
        }

        if (remainingGcd > 0) {
            return remainingGcd < queueWindow || gForceQueueCast;
        }

        return false;
    }

    bool IsNonSwingSpellQueued() {
        return gCastData.nonGcdSpellQueued || gCastData.normalSpellQueued;
    }

    uint32_t EffectiveCastEndMs() {
        if (gCastData.channeling && gUserSettings.queueChannelingSpells) {
            if (gUserSettings.interruptChannelsOutsideQueueWindow) {
                auto currentTime = GetTime();
                auto const remainingChannelTime = (gCastData.channelEndMs > currentTime)
                                                      ? gCastData.channelEndMs -
                                                        currentTime
                                                      : 0;
                if (remainingChannelTime < gUserSettings.channelQueueWindowMs) {
                    return gCastData.channelEndMs;
                }
            } else {
                return gCastData.channelEndMs;
            }
        }

        return max(gCastData.castEndMs, gCastData.delayEndMs);
    }

    void ResetChannelingFlags() {
        gCastData.cancelChannelNextTick = false;

        gCastData.channeling = false;
        gCastData.channelStartMs = 0;
        gCastData.channelEndMs = 0;
        gCastData.channelSpellId = 0;

        gCastData.channelTickTimeMs = 0;
        gCastData.channelNumTicks = 0;
    }

    void ResetCastFlags() {
        // don't reset delayEndMs
        gCastData.castEndMs = 0;
        gCastData.castSpellId = 0;
        gCastData.gcdEndMs = 0;
        ResetChannelingFlags();
    }

    void ResetOnSwingFlags() {
        gCastData.onSwingQueued = false;
        gCastData.onSwingSpellId = 0;
    }

    void ClearQueuedSpells() {
        if (gCastData.normalSpellQueued || gCastData.cooldownNormalSpellQueued) {
            TriggerSpellQueuedEvent(NORMAL_QUEUE_POPPED, gLastNormalCastParams.spellId);
            gCastData.normalSpellQueued = false;
            gCastData.cooldownNormalSpellQueued = false;
        }

        if (gCastData.nonGcdSpellQueued || gCastData.cooldownNonGcdSpellQueued) {
            while (!gNonGcdCastQueue.isEmpty()) {
                auto castParams = gNonGcdCastQueue.pop();
                TriggerSpellQueuedEvent(NON_GCD_QUEUE_POPPED, castParams.spellId);
            }
            gCastData.nonGcdSpellQueued = false;
            gCastData.cooldownNonGcdSpellQueued = false;
        }
        gCastData.targetingSpellQueued = false;
        gCastData.targetingSpellId = 0;
    }

    void ResetDisenchantState() {
        gDisenchantItemId = 0;
        gDisenchantQuality = 0;
        gDisenchantIncludeSoulbound = false;
        gNextDisenchantTimeMs = 0;
    }

    void checkForStopChanneling() {
        if (gUserSettings.queueChannelingSpells && IsNonSwingSpellQueued()) {
            // for channels just end channeling
            auto const currentTime = GetTime();
            auto const elapsed = currentTime - gLastCastData.channelStartTimeMs;

            auto remainingChannelTime = (gCastData.channelEndMs > currentTime)
                                            ? gCastData.channelEndMs - currentTime
                                            : 0;

            auto const currentLatency = GetLatencyMs();
            uint32_t latencyReduction = 0;
            if (currentLatency > 0 && gUserSettings.channelLatencyReductionPercentage != 0) {
                latencyReduction = ((uint32_t) currentLatency * gUserSettings.channelLatencyReductionPercentage) / 100;

                if (remainingChannelTime > latencyReduction) {
                    remainingChannelTime -= latencyReduction;
                } else {
                    remainingChannelTime = 0;
                }
            }


            if (remainingChannelTime <= 0) {
                DEBUG_LOG("Ending channel [" << elapsed << " elapsed > "
                    << gCastData.channelDuration << " original duration "
                    << " latency reduction " << latencyReduction << "]"
                    << " triggering queued spells");

                ResetChannelingFlags();
            } else if (gCastData.cancelChannelNextTick &&
                       gCastData.channelStartMs > 0 &&
                       currentTime - gCastData.channelStartMs < 60000) {
                auto nextTickTimeMs = gCastData.channelStartMs;

                // find the next tick time but don't choose the next tick until gBufferTimeMs has passed after a tick
                while (nextTickTimeMs < currentTime - gBufferTimeMs) {
                    nextTickTimeMs += gCastData.channelTickTimeMs;
                }

                uint32_t remainingTickTime = 0;
                if (nextTickTimeMs > currentTime) {
                    remainingTickTime = nextTickTimeMs - currentTime;
                }

                if (remainingTickTime > 0) {
                    if (currentLatency > 0 && gUserSettings.channelLatencyReductionPercentage != 0) {
                        if (remainingTickTime > latencyReduction) {
                            remainingTickTime -= latencyReduction;
                        } else {
                            remainingTickTime = 0;
                        }
                    } else {
                        // default to reducing by gBufferTimeMs
                        if (remainingTickTime > gBufferTimeMs) {
                            remainingTickTime -= gBufferTimeMs;
                        } else {
                            remainingTickTime = 0;
                        }
                    }
                }

                if (remainingTickTime <= 0) {
                    DEBUG_LOG("Ending channel due to cancelChannelNextTick. "
                        << "Remaining tick time: " << nextTickTimeMs - currentTime
                        << " latency reduction: " << latencyReduction);
                    ResetChannelingFlags();
                }
            }
        }
    }

    bool processQueues() {
        if (!gCastData.channeling && !gQueuesProcessed) {
            gQueuesProcessed = true;

            // check for high priority script
            if (RunQueuedScript(1)) {
                // script ran, stop processing
                return true;
            }

            // check for non gcd spell
            if (gCastData.nonGcdSpellQueued) {
                auto currentTime = GetTime();

                if (EffectiveCastEndMs() <= currentTime) {
                    CastQueuedNonGcdSpell();
                    return true;
                }
            }

            // check for cooldown non gcd spell
            if (gCastData.cooldownNonGcdSpellQueued) {
                auto currentTime = GetTime();

                if (gCastData.cooldownNonGcdEndMs <= currentTime) {
                    DEBUG_LOG("Non gcd spell cooldown up, casting queued spell");
                    // trigger the regular non gcd queuing and turn off cooldownNonGcdSpellQueued
                    gCastData.cooldownNonGcdSpellQueued = false;
                    gCastData.nonGcdSpellQueued = true;
                    CastQueuedNonGcdSpell();
                    return true;
                }
            }

            // check for medium priority script
            if (RunQueuedScript(2)) {
                // script ran, stop processing
                return true;
            }

            // check for normal spell
            if (gCastData.normalSpellQueued) {
                auto currentTime = GetTime();

                auto effectiveCastEndMs = EffectiveCastEndMs();
                // get max of cooldown and gcd
                auto delay = effectiveCastEndMs > gCastData.gcdEndMs ? effectiveCastEndMs : gCastData.gcdEndMs;

                if (delay <= currentTime) {
                    // if more than MAX_TIME_SINCE_LAST_CAST_FOR_QUEUE seconds have passed since the last cast, ignore
                    if (currentTime - gLastCastData.startTimeMs < MAX_TIME_SINCE_LAST_CAST_FOR_QUEUE) {
                        CastQueuedNormalSpell();
                        return true;
                    } else {
                        DEBUG_LOG("Ignoring queued cast of " << game::GetSpellName(gLastNormalCastParams.spellId)
                            << " due to max time since last cast");
                        TriggerSpellQueuedEvent(NORMAL_QUEUE_POPPED, gLastNormalCastParams.spellId);
                        gCastData.normalSpellQueued = false;
                        gCastData.targetingSpellQueued = false;
                        gCastData.targetingSpellId = 0;
                    }
                }
            }

            // check for cooldown normal spell
            if (gCastData.cooldownNormalSpellQueued) {
                auto currentTime = GetTime();

                if (gCastData.cooldownNormalEndMs <= currentTime) {
                    DEBUG_LOG("Spell cooldown up, casting queued spell");
                    // trigger the regular non gcd queuing and turn off cooldownNonGcdSpellQueued
                    gCastData.cooldownNormalSpellQueued = false;
                    gCastData.normalSpellQueued = true;
                    CastQueuedNormalSpell();
                    return true;
                }
            }

            if (RunQueuedScript(3)) {
                // script ran, stop processing
                return true;
            }

            // Check for disenchant all
            if (gDisenchantItemId != 0 || gDisenchantQuality != 0) {
                auto currentTime = GetTime();

                // Try to disenchant when timer has elapsed
                if (gNextDisenchantTimeMs <= currentTime) {
                    if (TryDisenchant()) {
                        return true;
                    }
                }
            }
        }

        gQueuesProcessed = true;

        // Process item export if active (low priority, runs when nothing else is queued)
        // ProcessItemExport()

        return false;
    }

    int OnSpriteRightClickHook(hadesmem::PatchDetourBase *detour, uint64_t objectGUID) {
        auto const onSpriteRightClick = detour->GetTrampolineT<OnSpriteRightClickT>();

        auto const currentTargetGuid = game::GetCurrentTargetGuid();

        // if we have a target that is not the right click target, ignore the right click
        if (gUserSettings.preventRightClickTargetChange && currentTargetGuid &&
            currentTargetGuid != objectGUID) {
            auto unitOrPlayer = game::ClntObjMgrObjectPtr(
                static_cast<game::TypeMask>(game::TYPEMASK_PLAYER | game::TYPEMASK_UNIT), objectGUID);

            // only prevent right click if guid is a unit/player
            if (unitOrPlayer) {
                auto GetUnitFromName = reinterpret_cast<GetUnitFromNameT>(Offsets::GetUnitFromName);
                auto const unit = GetUnitFromName("player");

                // combat check from Script_UnitAffectingCombat
                // only prevent right click if in combat
                if (unit && game::UnitIsInCombat(unit)) {
                    return 1;
                }
            }
        }

        if (gUserSettings.preventRightClickPvPAttack) {
            // check if clicking player
            auto playerUnit = game::ClntObjMgrObjectPtr(game::TYPEMASK_PLAYER, objectGUID);

            if (playerUnit && game::UnitIsPvpFlagged(playerUnit)) {
                // check if they have pvp enabled
                return 1;
            }
        }

        return onSpriteRightClick(objectGUID);
    }

    int *ISceneEndHook(hadesmem::PatchDetourBase *detour, uintptr_t *ptr) {
        auto const iSceneEnd = detour->GetTrampolineT<ISceneEndT>();

        // check if it's time to end channeling
        if (gCastData.channeling) {
            checkForStopChanneling();
        }

        // process any queued spells/scripts
        processQueues();

        gQueuesProcessed = false;

        return iSceneEnd(ptr);
    }

    void InvalidFunctionPtrCheckHook(hadesmem::PatchDetourBase *detour, uint32_t param_1) {
        // remove original call to allow registering lua functions at any address range
    }

    void
    Spell_C_GetSpellModifiersHook(hadesmem::PatchDetourBase *detour, const game::SpellRec *spellRec, int *returnVal,
                                  game::SpellModOp modOp) {
        auto const getSpellModifiers = detour->GetTrampolineT<Spell_C_GetSpellModifiersT>();
        getSpellModifiers(spellRec, returnVal, modOp);
    }

    void updateFromCvar(const char *cvar, const char *value) {
        if (strcmp(cvar, "NP_QueueCastTimeSpells") == 0) {
            gUserSettings.queueCastTimeSpells = atoi(value) != 0;
            DEBUG_LOG("Set NP_QueueCastTimeSpells to " << gUserSettings.queueCastTimeSpells);
        } else if (strcmp(cvar, "NP_QueueInstantSpells") == 0) {
            gUserSettings.queueInstantSpells = atoi(value) != 0;
            DEBUG_LOG("Set NP_QueueInstantSpells to " << gUserSettings.queueInstantSpells);
        } else if (strcmp(cvar, "NP_QueueOnSwingSpells") == 0) {
            gUserSettings.queueOnSwingSpells = atoi(value) != 0;
            DEBUG_LOG("Set NP_QueueOnSwingSpells to " << gUserSettings.queueOnSwingSpells);
        } else if (strcmp(cvar, "NP_QueueChannelingSpells") == 0) {
            gUserSettings.queueChannelingSpells = atoi(value) != 0;
            DEBUG_LOG("Set NP_QueueChannelingSpells to " << gUserSettings.queueChannelingSpells);
        } else if (strcmp(cvar, "NP_QueueTargetingSpells") == 0) {
            gUserSettings.queueTargetingSpells = atoi(value) != 0;
            DEBUG_LOG("Set NP_QueueTargetingSpells to " << gUserSettings.queueTargetingSpells);
        } else if (strcmp(cvar, "NP_QueueSpellsOnCooldown") == 0) {
            gUserSettings.queueSpellsOnCooldown = atoi(value) != 0;
            DEBUG_LOG("Set NP_QueueSpellsOnCooldown to " << gUserSettings.queueSpellsOnCooldown);
        } else if (strcmp(cvar, "NP_InterruptChannelsOutsideQueueWindow") == 0) {
            gUserSettings.interruptChannelsOutsideQueueWindow = atoi(value) != 0;
            DEBUG_LOG("Set NP_InterruptChannelsOutsideQueueWindow to "
                << gUserSettings.interruptChannelsOutsideQueueWindow);
        } else if ((strcmp(cvar, "NP_RetryServerRejectedSpells") == 0)) {
            gUserSettings.retryServerRejectedSpells = atoi(value) != 0;
            DEBUG_LOG("Set NP_RetryServerRejectedSpells to " << gUserSettings.retryServerRejectedSpells);
        } else if (strcmp(cvar, "NP_QuickcastTargetingSpells") == 0) {
            gUserSettings.quickcastTargetingSpells = atoi(value) != 0;
            DEBUG_LOG("Set NP_QuickcastTargetingSpells to " << gUserSettings.quickcastTargetingSpells);
        } else if (strcmp(cvar, "NP_ReplaceMatchingNonGcdCategory") == 0) {
            gUserSettings.replaceMatchingNonGcdCategory = atoi(value) != 0;
            DEBUG_LOG("Set NP_ReplaceMatchingNonGcdCategory to " << gUserSettings.replaceMatchingNonGcdCategory);
        } else if (strcmp(cvar, "NP_OptimizeBufferUsingPacketTimings") == 0) {
            gUserSettings.optimizeBufferUsingPacketTimings = atoi(value) != 0;
            DEBUG_LOG("Set NP_OptimizeBufferUsingPacketTimings to " << gUserSettings.optimizeBufferUsingPacketTimings);
        } else if (strcmp(cvar, "NP_PreventRightClickTargetChange") == 0) {
            gUserSettings.preventRightClickTargetChange = atoi(value) != 0;
            DEBUG_LOG("Set NP_PreventRightClickTargetChange to " << gUserSettings.preventRightClickTargetChange);
        } else if (strcmp(cvar, "NP_PreventRightClickPvPAttack") == 0) {
            gUserSettings.preventRightClickPvPAttack = atoi(value) != 0;
            DEBUG_LOG("Set NP_PreventRightClickPvPAttack to " << gUserSettings.preventRightClickPvPAttack);
        } else if (strcmp(cvar, "NP_DoubleCastToEndChannelEarly") == 0) {
            gUserSettings.doubleCastToEndChannelEarly = atoi(value) != 0;
            DEBUG_LOG("Set NP_DoubleCastToEndChannelEarly to " << gUserSettings.doubleCastToEndChannelEarly);
        } else if (strcmp(cvar, "NP_QuickcastOnDoubleCast") == 0) {
            gUserSettings.quickcastOnDoubleCast = atoi(value) != 0;
            DEBUG_LOG("Set NP_QuickcastOnDoubleCast to " << gUserSettings.quickcastOnDoubleCast);
        } else if (strcmp(cvar, "NP_SpamProtectionEnabled") == 0) {
            gUserSettings.spamProtectionEnabled = atoi(value) != 0;
            DEBUG_LOG("Set NP_SpamProtectionEnabled to " << gUserSettings.spamProtectionEnabled);
        } else if (strcmp(cvar, "NP_EnableAuraCastEvents") == 0) {
            gUserSettings.enableAuraCastEvents = atoi(value) != 0;
            DEBUG_LOG("Set NP_EnableAuraCastEvents to " << gUserSettings.enableAuraCastEvents);
        } else if (strcmp(cvar, "NP_PreventMountingWhenBuffCapped") == 0) {
            gUserSettings.preventMountingWhenBuffCapped = atoi(value) != 0;
            DEBUG_LOG("Set NP_PreventMountingWhenBuffCapped to " << gUserSettings.preventMountingWhenBuffCapped);
        } else if (strcmp(cvar, "NP_MinBufferTimeMs") == 0) {
            gUserSettings.minBufferTimeMs = atoi(value);
            DEBUG_LOG("Set NP_MinBufferTimeMs and current buffer to " << gUserSettings.minBufferTimeMs);
            gBufferTimeMs = gUserSettings.minBufferTimeMs;
        } else if (strcmp(cvar, "NP_NonGcdBufferTimeMs") == 0) {
            gUserSettings.nonGcdBufferTimeMs = atoi(value);
            DEBUG_LOG("Set NP_NonGcdBufferTimeMs to " << gUserSettings.nonGcdBufferTimeMs);
        } else if (strcmp(cvar, "NP_MaxBufferIncreaseMs") == 0) {
            gUserSettings.maxBufferIncreaseMs = atoi(value);
            DEBUG_LOG("Set NP_MaxBufferIncreaseMs to " << gUserSettings.maxBufferIncreaseMs);
        } else if (strcmp(cvar, "NP_SpellQueueWindowMs") == 0) {
            gUserSettings.spellQueueWindowMs = atoi(value);
            DEBUG_LOG("Set NP_SpellQueueWindowMs to " << gUserSettings.spellQueueWindowMs);
        } else if (strcmp(cvar, "NP_OnSwingBufferCooldownMs") == 0) {
            gUserSettings.onSwingBufferCooldownMs = atoi(value);
            DEBUG_LOG("Set NP_OnSwingBufferCooldownMs to " << gUserSettings.onSwingBufferCooldownMs);
        } else if (strcmp(cvar, "NP_ChannelQueueWindowMs") == 0) {
            gUserSettings.channelQueueWindowMs = atoi(value);
            DEBUG_LOG("Set NP_ChannelQueueWindowMs to " << gUserSettings.channelQueueWindowMs);
        } else if (strcmp(cvar, "NP_TargetingQueueWindowMs") == 0) {
            gUserSettings.targetingQueueWindowMs = atoi(value);
            DEBUG_LOG("Set NP_TargetingQueueWindowMs to " << gUserSettings.targetingQueueWindowMs);
        } else if (strcmp(cvar, "NP_CooldownQueueWindowMs") == 0) {
            gUserSettings.cooldownQueueWindowMs = atoi(value);
            DEBUG_LOG("Set NP_CooldownQueueWindowMs to " << gUserSettings.cooldownQueueWindowMs);
        } else if (strcmp(cvar, "NP_ChannelLatencyReductionPercentage") == 0) {
            gUserSettings.channelLatencyReductionPercentage = atoi(value);
            DEBUG_LOG(
                "Set NP_ChannelLatencyReductionPercentage to " << gUserSettings.channelLatencyReductionPercentage);
        } else if (strcmp(cvar, "NP_NameplateDistance") == 0) {
            auto distance = std::stof(value);
            SetNameplateDistance(distance);
            DEBUG_LOG(
                "Set NP_NameplateDistance to " << distance);
        }
    }

    int Script_SetCVarHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState) {
        auto const cvarSetOrig = gSetCVarDetour->GetTrampolineT<SetCVarT>();

        auto const lua_isstring = reinterpret_cast<lua_isstringT>(Offsets::lua_isstring);
        if (lua_isstring(luaState, 1)) {
            auto const lua_tostring = reinterpret_cast<lua_tostringT>(Offsets::lua_tostring);
            auto const cVarName = lua_tostring(luaState, 1);
            auto const cVarValue = lua_tostring(luaState, 2);
            // if cvar starts with "NP_", then we need to handle it
            if (strncmp(cVarName, "NP_", 3) == 0) {
                updateFromCvar(cVarName, cVarValue);
            }
        } // original function handles errors

        return cvarSetOrig(luaState);
    }

    int *getCvar(const char *cvar) {
        auto const cvarLookup = hadesmem::detail::AliasCast<CVarLookupT>(Offsets::CVarLookup);
        uintptr_t *cvarPtr = cvarLookup(cvar);

        if (cvarPtr) {
            return reinterpret_cast<int *>(cvarPtr +
                                           10); // get intValue from CVar which is consistent, strValue more complicated
        }
        return nullptr;
    }

    void loadUserVar(const char *cvar) {
        int *value = getCvar(cvar);
        if (value) {
            updateFromCvar(cvar, std::to_string(*value).c_str());
        } else {
            DEBUG_LOG("Using default value for " << cvar);
        }
    }

    bool isFileInUse(const char *filename) {
        HANDLE file = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE,
                                  0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) {
                return true;
            }
            return false;
        }
        CloseHandle(file);
        return false;
    }

    bool safeRemove(const char *filename) {
        if (isFileInUse(filename)) {
            return false;
        }
        return remove(filename) == 0;
    }

    bool safeRename(const char *oldname, const char *newname) {
        if (isFileInUse(oldname) || isFileInUse(newname)) {
            return false;
        }
        return rename(oldname, newname) == 0;
    }

    void loadConfig() {
        gStartTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count());

        // create logs directory if it doesn't exist
        CreateDirectoryA("Logs", NULL);

        // move any existing logs from root directory to Logs folder
        if (GetFileAttributesA("nampower_debug.log") != INVALID_FILE_ATTRIBUTES)
            safeRename("nampower_debug.log", "Logs\\nampower_debug.log");
        if (GetFileAttributesA("nampower_debug.log.1") != INVALID_FILE_ATTRIBUTES)
            safeRename("nampower_debug.log.1", "Logs\\nampower_debug.log.1");
        if (GetFileAttributesA("nampower_debug.log.2") != INVALID_FILE_ATTRIBUTES)
            safeRename("nampower_debug.log.2", "Logs\\nampower_debug.log.2");
        if (GetFileAttributesA("nampower_debug.log.3") != INVALID_FILE_ATTRIBUTES)
            safeRename("nampower_debug.log.3", "Logs\\nampower_debug.log.3");

        // remove/rename previous logs
        safeRemove("Logs\\nampower_debug.log.3");
        safeRename("Logs\\nampower_debug.log.2", "Logs\\nampower_debug.log.3");
        safeRename("Logs\\nampower_debug.log.1", "Logs\\nampower_debug.log.2");
        safeRename("Logs\\nampower_debug.log", "Logs\\nampower_debug.log.1");


        // open new log file
        debugLogFile.open("Logs\\nampower_debug.log");

        DEBUG_LOG("Loading nampower v" << MAJOR_VERSION << "." << MINOR_VERSION << "." << PATCH_VERSION);

        char defaultTrue[] = "1";
        char defaultFalse[] = "0";

        DEBUG_LOG("Registering/Loading CVars");

        // register cvars
        auto const CVarRegister = hadesmem::detail::AliasCast<CVarRegisterT>(Offsets::RegisterCVar);

        char NP_QueueCastTimeSpells[] = "NP_QueueCastTimeSpells";
        CVarRegister(NP_QueueCastTimeSpells, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.queueCastTimeSpells ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     5, // category
                     0, // unk2
                     0); // unk3

        char NP_QueueInstantSpells[] = "NP_QueueInstantSpells";
        CVarRegister(NP_QueueInstantSpells, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.queueInstantSpells ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_QueueChannelingSpells[] = "NP_QueueChannelingSpells";
        CVarRegister(NP_QueueChannelingSpells, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.queueChannelingSpells ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_QueueTargetingSpells[] = "NP_QueueTargetingSpells";
        CVarRegister(NP_QueueTargetingSpells, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.queueTargetingSpells ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_QueueOnSwingSpells[] = "NP_QueueOnSwingSpells";
        CVarRegister(NP_QueueOnSwingSpells, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.queueOnSwingSpells ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_QueueSpellsOnCooldown[] = "NP_QueueSpellsOnCooldown";
        CVarRegister(NP_QueueSpellsOnCooldown, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.queueSpellsOnCooldown ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_InterruptChannelsOutsideQueueWindow[] = "NP_InterruptChannelsOutsideQueueWindow";
        CVarRegister(NP_InterruptChannelsOutsideQueueWindow, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.interruptChannelsOutsideQueueWindow
                         ? defaultTrue
                         : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_RetryServerRejectedSpells[] = "NP_RetryServerRejectedSpells";
        CVarRegister(NP_RetryServerRejectedSpells, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.retryServerRejectedSpells ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_QuickcastTargetingSpells[] = "NP_QuickcastTargetingSpells";
        CVarRegister(NP_QuickcastTargetingSpells, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.quickcastTargetingSpells ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_MinBufferTimeMs[] = "NP_MinBufferTimeMs";
        CVarRegister(NP_MinBufferTimeMs, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(gUserSettings.minBufferTimeMs).c_str(), // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_NonGcdBufferTimeMs[] = "NP_NonGcdBufferTimeMs";
        CVarRegister(NP_NonGcdBufferTimeMs, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(gUserSettings.nonGcdBufferTimeMs).c_str(), // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_MaxBufferIncreaseMs[] = "NP_MaxBufferIncreaseMs";
        CVarRegister(NP_MaxBufferIncreaseMs, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(gUserSettings.maxBufferIncreaseMs).c_str(), // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_SpellQueueWindowMs[] = "NP_SpellQueueWindowMs";
        CVarRegister(NP_SpellQueueWindowMs, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(gUserSettings.spellQueueWindowMs).c_str(), // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_ChannelQueueWindowMs[] = "NP_ChannelQueueWindowMs";
        CVarRegister(NP_ChannelQueueWindowMs, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(gUserSettings.channelQueueWindowMs).c_str(), // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_TargetingQueueWindowMs[] = "NP_TargetingQueueWindowMs";
        CVarRegister(NP_TargetingQueueWindowMs, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(gUserSettings.targetingQueueWindowMs).c_str(), // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_CooldownQueueWindowMs[] = "NP_CooldownQueueWindowMs";
        CVarRegister(NP_CooldownQueueWindowMs, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(gUserSettings.cooldownQueueWindowMs).c_str(), // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_EnableAuraCastEvents[] = "NP_EnableAuraCastEvents";
        CVarRegister(NP_EnableAuraCastEvents, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.enableAuraCastEvents ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_PreventMountingWhenBuffCapped[] = "NP_PreventMountingWhenBuffCapped";
        CVarRegister(NP_PreventMountingWhenBuffCapped, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.preventMountingWhenBuffCapped ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_OnSwingBufferCooldownMs[] = "NP_OnSwingBufferCooldownMs";
        CVarRegister(NP_OnSwingBufferCooldownMs, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(gUserSettings.onSwingBufferCooldownMs).c_str(), // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_ReplaceMatchingNonGcdCategory[] = "NP_ReplaceMatchingNonGcdCategory";
        CVarRegister(NP_ReplaceMatchingNonGcdCategory, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.replaceMatchingNonGcdCategory ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_OptimizeBufferUsingPacketTimings[] = "NP_OptimizeBufferUsingPacketTimings";
        CVarRegister(NP_OptimizeBufferUsingPacketTimings, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.optimizeBufferUsingPacketTimings
                         ? defaultTrue
                         : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_PreventRightClickTargetChange[] = "NP_PreventRightClickTargetChange";
        CVarRegister(NP_PreventRightClickTargetChange, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.preventRightClickTargetChange ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_PreventRightClickPvPAttack[] = "NP_PreventRightClickPvPAttack";
        CVarRegister(NP_PreventRightClickPvPAttack, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.preventRightClickPvPAttack ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_DoubleCastToEndChannelEarly[] = "NP_DoubleCastToEndChannelEarly";
        CVarRegister(NP_DoubleCastToEndChannelEarly, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.doubleCastToEndChannelEarly ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_QuickcastOnDoubleCast[] = "NP_QuickcastOnDoubleCast";
        CVarRegister(NP_QuickcastOnDoubleCast, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.quickcastOnDoubleCast ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_SpamProtectionEnabled[] = "NP_SpamProtectionEnabled";
        CVarRegister(NP_SpamProtectionEnabled, // name
                     nullptr, // help
                     0, // unk1
                     gUserSettings.spamProtectionEnabled ? defaultTrue : defaultFalse, // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_ChannelLatencyReductionPercentage[] = "NP_ChannelLatencyReductionPercentage";
        CVarRegister(NP_ChannelLatencyReductionPercentage, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(gUserSettings.channelLatencyReductionPercentage).c_str(), // default value address
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        char NP_NameplateDistance[] = "NP_NameplateDistance";
        CVarRegister(NP_NameplateDistance, // name
                     nullptr, // help
                     0, // unk1
                     std::to_string(GetNameplateDistance()).c_str(), // use the game's DAT value as the default
                     nullptr, // callback
                     1, // category
                     0, // unk2
                     0); // unk3

        // update from cvars
        loadUserVar("NP_QueueCastTimeSpells");
        loadUserVar("NP_QueueInstantSpells");
        loadUserVar("NP_QueueOnSwingSpells");
        loadUserVar("NP_QueueChannelingSpells");
        loadUserVar("NP_QueueTargetingSpells");
        loadUserVar("NP_QueueSpellsOnCooldown");

        loadUserVar("NP_InterruptChannelsOutsideQueueWindow");

        loadUserVar("NP_RetryServerRejectedSpells");
        loadUserVar("NP_QuickcastTargetingSpells");
        loadUserVar("NP_QuickcastOnDoubleCast");
        loadUserVar("NP_ReplaceMatchingNonGcdCategory");
        loadUserVar("NP_OptimizeBufferUsingPacketTimings");

        loadUserVar("NP_PreventRightClickTargetChange");

        loadUserVar("NP_PreventRightClickPvPAttack");

        loadUserVar("NP_DoubleCastToEndChannelEarly");

        loadUserVar("NP_SpamProtectionEnabled");
        loadUserVar("NP_EnableAuraCastEvents");
        loadUserVar("NP_PreventMountingWhenBuffCapped");

        loadUserVar("NP_MinBufferTimeMs");
        loadUserVar("NP_NonGcdBufferTimeMs");
        loadUserVar("NP_MaxBufferIncreaseMs");

        loadUserVar("NP_SpellQueueWindowMs");
        loadUserVar("NP_ChannelQueueWindowMs");
        loadUserVar("NP_TargetingQueueWindowMs");
        loadUserVar("NP_OnSwingBufferCooldownMs");
        loadUserVar("NP_CooldownQueueWindowMs");

        loadUserVar("NP_ChannelLatencyReductionPercentage");

        loadUserVar("NP_NameplateDistance");

        gBufferTimeMs = gUserSettings.minBufferTimeMs;
    }

    // Template function to simplify hook initialization with specific storage
    template<typename FuncT, typename HookT>
    std::unique_ptr<hadesmem::PatchDetour<FuncT> >
    createHook(const hadesmem::Process &process, Offsets offset, HookT hookFunc) {
        auto const originalFunc = hadesmem::detail::AliasCast<FuncT>(offset);

        const int maxRetries = 10;
        std::string lastError;

        for (int attempt = 0; attempt < maxRetries; ++attempt) {
            try {
                auto detour = std::make_unique<hadesmem::PatchDetour<FuncT> >(process, originalFunc, hookFunc);
                detour->Apply();

                if (attempt > 0) {
                    DEBUG_LOG("  Hook succeeded after " << (attempt + 1) << " attempts (offset 0x"
                        << std::hex << static_cast<uint32_t>(offset) << std::dec << ")");
                }

                return detour;
            } catch (const hadesmem::Error& e) {
                lastError = e.what();

                if (attempt == 0) {
                    DEBUG_LOG("  Hook failed on offset 0x" << std::hex << static_cast<uint32_t>(offset)
                        << std::dec << ": " << lastError << " - retrying...");
                }

                if (attempt < maxRetries - 1) {
                    Sleep(10);  // Wait 10ms before retry
                    continue;
                } else {
                    // Out of retries
                    DEBUG_LOG("  FAILED after " << maxRetries << " attempts (offset 0x"
                        << std::hex << static_cast<uint32_t>(offset) << std::dec
                        << "): " << lastError);
                    throw;
                }
            } catch (const std::exception& e) {
                lastError = e.what();

                if (attempt == 0) {
                    DEBUG_LOG("  Hook failed with std::exception on offset 0x"
                        << std::hex << static_cast<uint32_t>(offset) << std::dec
                        << ": " << lastError << " - retrying...");
                }

                if (attempt < maxRetries - 1) {
                    Sleep(10);  // Wait 10ms before retry
                    continue;
                } else {
                    DEBUG_LOG("  FAILED after " << maxRetries << " attempts (offset 0x"
                        << std::hex << static_cast<uint32_t>(offset) << std::dec
                        << "): " << lastError);
                    throw;
                }
            } catch (...) {
                lastError = "Unknown exception";

                if (attempt == 0) {
                    DEBUG_LOG("  Hook failed with unknown exception on offset 0x"
                        << std::hex << static_cast<uint32_t>(offset) << std::dec << " - retrying...");

                    // Try to get more info about the memory at this address
                    MEMORY_BASIC_INFORMATION mbi;
                    if (VirtualQuery(reinterpret_cast<LPCVOID>(offset), &mbi, sizeof(mbi))) {
                        DEBUG_LOG("  Memory info - Base: 0x" << std::hex << mbi.BaseAddress
                            << " Size: " << mbi.RegionSize
                            << " State: " << mbi.State
                            << " Protect: " << mbi.Protect
                            << " Type: " << mbi.Type << std::dec);
                    }
                }

                if (attempt < maxRetries - 1) {
                    Sleep(10);  // Wait 10ms before retry
                    continue;
                } else {
                    DEBUG_LOG("  FAILED after " << maxRetries << " attempts (offset 0x"
                        << std::hex << static_cast<uint32_t>(offset) << std::dec
                        << "): " << lastError);
                    throw;
                }
            }
        }

        // Shouldn't reach here, but just in case
        HADESMEM_DETAIL_THROW_EXCEPTION(
            hadesmem::Error{} << hadesmem::ErrorString{"Failed to create hook after all retry attempts"});
    }

    void initHooks() {
        const hadesmem::Process process(::GetCurrentProcessId());

        DEBUG_LOG("=== Starting hook initialization ===");

        gSetCVarDetour = createHook<SetCVarT>(process, Offsets::Script_SetCVar, &Script_SetCVarHook);
        gCGSpellBook_CastSpellDetour = createHook<CGSpellBook_CastSpellT>(
            process, Offsets::CGSpellBook_CastSpell, &CGSpellBook_CastSpellHook);
        gCGActionBar_UseActionDetour = createHook<CGActionBar_UseActionT>(
            process, Offsets::CGActionBar_UseAction, &CGActionBar_UseActionHook);
        gCastDetour = createHook<Spell_C_CastSpellT>(process, Offsets::Spell_C_CastSpell, &Spell_C_CastSpellHook);
        gSendCastDetour = createHook<SendCastT>(process, Offsets::SendCast, &SendCastHook);
        gCancelSpellDetour = createHook<CancelSpellT>(process, Offsets::CancelSpell, &CancelSpellHook);
        gCastResultHandlerDetour = createHook<PacketHandlerT>(process, Offsets::CastResultHandler,
                                                              &CastResultHandlerHook);
        gSpellStartHandlerDetour = createHook<FastCallPacketHandlerT>(process, Offsets::SpellStartHandler,
                                                                      &SpellStartHandlerHook);
        gPeriodicAuraLogHandlerDetour = createHook<FastCallPacketHandlerT>(process, Offsets::PeriodicAuraLogHandler,
                                                                           &PeriodicAuraLogHandlerHook);
        gSpellNonMeleeDmgLogHandlerDetour = createHook<FastCallPacketHandlerT>(process,
                                                                               Offsets::SpellNonMeleeDmgLogHandler,
                                                                               &SpellNonMeleeDmgLogHandlerHook);
        gSpellChannelStartHandlerDetour = createHook<PacketHandlerT>(process, Offsets::SpellChannelStartHandler,
                                                                     &SpellChannelStartHandlerHook);
        gSpellChannelUpdateHandlerDetour = createHook<PacketHandlerT>(process, Offsets::SpellChannelUpdateHandler,
                                                                      &SpellChannelUpdateHandlerHook);
        gSpellFailedDetour = createHook<Spell_C_SpellFailedT>(process, Offsets::Spell_C_SpellFailed,
                                                              &Spell_C_SpellFailedHook);
        gSpellGoDetour = createHook<SpellGoT>(process, Offsets::SpellGo, &SpellGoHook);
        gSpellDelayedDetour = createHook<PacketHandlerT>(process, Offsets::SpellDelayed, &SpellDelayedHook);
        gSpellTargetUnitDetour = createHook<LuaScriptT>(process, Offsets::Script_SpellTargetUnit,
                                                        &Script_SpellTargetUnitHook);
        gSpellStopCastingDetour = createHook<LuaScriptT>(process, Offsets::Script_SpellStopCasting,
                                                         &Script_SpellStopCastingHook);
        gSpell_C_TargetSpellDetour = createHook<Spell_C_TargetSpellT>(process, Offsets::Spell_C_TargetSpell,
                                                                      &Spell_C_TargetSpellHook);
        gSpell_C_HandleTerrainClickDetour = createHook<Spell_C_HandleTerrainClickT>(
            process, Offsets::Spell_C_HandleTerrainClick,
            &Spell_C_HandleTerrainClickHook);
        gCGWorldFrame_OnLayerTrackTerrainDetour = createHook<CGWorldFrame_OnLayerTrackTerrainT>(
            process, Offsets::CGWorldFrame_OnLayerTrackTerrain,
            &CGWorldFrame_OnLayerTrackTerrainHook);
        gOnSpriteRightClickDetour = createHook<OnSpriteRightClickT>(process, Offsets::OnSpriteRightClick,
                                                                    OnSpriteRightClickHook);
        gIEndSceneDetour = createHook<ISceneEndT>(process, Offsets::ISceneEndPtr, &ISceneEndHook);
        gInvalidFunctionPtrCheckDetour = createHook<InvalidFunctionPtrCheckT>(process, Offsets::InvalidFunctionPtrCheck,
                                                                              &InvalidFunctionPtrCheckHook);
        gGetSpellSlotFromLuaDetour = createHook<GetSpellSlotFromLuaT>(process, Offsets::GetSpellSlotFromLua,
                                                                      &GetSpellSlotFromLuaHook);
        gCGUnit_C_OnAuraRemovedDetour = createHook<CGUnit_C_OnAuraRemovedT>(process, Offsets::CGUnit_C_OnAuraRemoved,
                                                                            &CGUnit_C_OnAuraRemovedHook);
        gCGUnit_C_OnAuraAddedDetour = createHook<CGUnit_C_OnAuraAddedT>(process, Offsets::CGUnit_C_OnAuraAdded,
                                                                        &CGUnit_C_OnAuraAddedHook);
        gCGUnit_C_OnAuraAddedStackDetour = createHook<CGUnit_C_OnAuraAddedStackT>(
            process, Offsets::CGUnit_C_OnAuraAddedStack,
            &CGUnit_C_OnAuraAddedStackHook);
        gUnitCombatLogUnitDeadDetour = createHook<UnitCombatLogUnitDeadT>(process, Offsets::UnitCombatLogUnitDead,
                                                                          &UnitCombatLogUnitDeadHook);
        gLoadScriptFunctionsDetour = createHook<LoadScriptFunctionsT>(process, Offsets::LoadScriptFunctions,
                                                                      &LoadScriptFunctionsHook);
        gCreateEventsDetour = createHook<FrameScript_CreateEventsT>(process, Offsets::FrameScript_CreateEvents,
                                                                    &FrameScript_CreateEventsHook);
        gSetEventCountDetour = createHook<FramescriptSetEventCountT>(process, Offsets::Framescript_SetEventCount,
                                                                     &Framescript_SetEventCountHook);

        DEBUG_LOG("=== All hooks initialized successfully ===");
    }

    void SysMsgInitializeHook(hadesmem::PatchDetourBase *detour) {
        auto const sysMsgInitialize = detour->GetTrampolineT<SysMsgInitializeT>();
        sysMsgInitialize();
        std::call_once(initHooksFlag, []() {
            loadConfig();
            initHooks();
        });
    }

    void addCustomEvent(uint32_t code, char *name) {
        char **FrameScript_EventObject_Data = *reinterpret_cast<char ***>(Offsets::Framescript_EventObject_Data);

        auto SStrDupA = reinterpret_cast<char* (__stdcall *)(char *, char *, int)>(Offsets::SStrDupA);
        FrameScript_EventObject_Data[code * 4] = SStrDupA(name, name, 1308);

        DEBUG_LOG("Added custom event code:" << code << " " << name);
    }


    void initCustomEvents() {
        char SPELL_QUEUE_EVENT[] = "SPELL_QUEUE_EVENT";
        addCustomEvent(game::SPELL_QUEUE_EVENT, SPELL_QUEUE_EVENT);

        char SPELL_CAST_EVENT[] = "SPELL_CAST_EVENT";
        addCustomEvent(game::SPELL_CAST_EVENT, SPELL_CAST_EVENT);

        char SPELL_DAMAGE_EVENT_SELF[] = "SPELL_DAMAGE_EVENT_SELF";
        addCustomEvent(game::SPELL_DAMAGE_EVENT_SELF, SPELL_DAMAGE_EVENT_SELF);

        char SPELL_DAMAGE_EVENT_OTHER[] = "SPELL_DAMAGE_EVENT_OTHER";
        addCustomEvent(game::SPELL_DAMAGE_EVENT_OTHER, SPELL_DAMAGE_EVENT_OTHER);

        char SPELL_BEGIN_CAST_EVENT[] = "SPELL_BEGIN_CAST_EVENT";
        addCustomEvent(game::SPELL_BEGIN_CAST_EVENT, SPELL_BEGIN_CAST_EVENT);

        char DEBUFF_ADDED_SELF[] = "DEBUFF_ADDED_SELF";
        addCustomEvent(game::DEBUFF_ADDED_SELF, DEBUFF_ADDED_SELF);

        char DEBUFF_REMOVED_SELF[] = "DEBUFF_REMOVED_SELF";
        addCustomEvent(game::DEBUFF_REMOVED_SELF, DEBUFF_REMOVED_SELF);

        char DEBUFF_ADDED_OTHER[] = "DEBUFF_ADDED_OTHER";
        addCustomEvent(game::DEBUFF_ADDED_OTHER, DEBUFF_ADDED_OTHER);

        char DEBUFF_REMOVED_OTHER[] = "DEBUFF_REMOVED_OTHER";
        addCustomEvent(game::DEBUFF_REMOVED_OTHER, DEBUFF_REMOVED_OTHER);

        char BUFF_ADDED_SELF[] = "BUFF_ADDED_SELF";
        addCustomEvent(game::BUFF_ADDED_SELF, BUFF_ADDED_SELF);

        char BUFF_REMOVED_SELF[] = "BUFF_REMOVED_SELF";
        addCustomEvent(game::BUFF_REMOVED_SELF, BUFF_REMOVED_SELF);

        char BUFF_ADDED_OTHER[] = "BUFF_ADDED_OTHER";
        addCustomEvent(game::BUFF_ADDED_OTHER, BUFF_ADDED_OTHER);

        char BUFF_REMOVED_OTHER[] = "BUFF_REMOVED_OTHER";
        addCustomEvent(game::BUFF_REMOVED_OTHER, BUFF_REMOVED_OTHER);

        char UNIT_DIED[] = "UNIT_DIED";
        addCustomEvent(game::UNIT_DIED, UNIT_DIED);

        char AURA_CAST_ON_SELF[] = "AURA_CAST_ON_SELF";
        addCustomEvent(game::AURA_CAST_ON_SELF, AURA_CAST_ON_SELF);

        char AURA_CAST_ON_OTHER[] = "AURA_CAST_ON_OTHER";
        addCustomEvent(game::AURA_CAST_ON_OTHER, AURA_CAST_ON_OTHER);
    }

    void FrameScript_CreateEventsHook(hadesmem::PatchDetourBase *detour, int param_1, uint32_t maxEventId) {
        auto const createEvents = detour->GetTrampolineT<FrameScript_CreateEventsT>();
        createEvents(param_1, maxEventId);

        if (maxEventId > 200) {
            initCustomEvents();
        }
    }

    void Framescript_SetEventCountHook(hadesmem::PatchDetourBase *detour, void *thisPtr, void *dummy_edx,
                                       uint32_t count) {
        auto const setEventCount = detour->GetTrampolineT<FramescriptSetEventCountT>();

        if (count > 200) {
            DEBUG_LOG("Increasing event count from " << count << " to 700");
            count = 700; // extend to support additional custom events (this matches how superwow does it)
        }

        setEventCount(thisPtr, dummy_edx, count);
    }

    void LoadScriptFunctionsHook(hadesmem::PatchDetourBase *detour) {
        auto const loadScriptFunctions = detour->GetTrampolineT<LoadScriptFunctionsT>();
        loadScriptFunctions();

        CleanupAllLuaTableRefs();
        ClearItemCaches();

        // register our own lua functions
        DEBUG_LOG("Registering Custom Lua functions");
        char queueSpellByName[] = "QueueSpellByName";
        RegisterLuaFunction(queueSpellByName, reinterpret_cast<uintptr_t *>(Script_QueueSpellByName));

        char castSpellByNameNoQueue[] = "CastSpellByNameNoQueue";
        RegisterLuaFunction(castSpellByNameNoQueue, reinterpret_cast<uintptr_t *>(Script_CastSpellByNameNoQueue));

        char queueScript[] = "QueueScript";
        RegisterLuaFunction(queueScript, reinterpret_cast<uintptr_t *>(Script_QueueScript));

        char isSpellInRange[] = "IsSpellInRange";
        RegisterLuaFunction(isSpellInRange, reinterpret_cast<uintptr_t *>(Script_IsSpellInRange));

        char isSpellUsable[] = "IsSpellUsable";
        RegisterLuaFunction(isSpellUsable, reinterpret_cast<uintptr_t *>(Script_IsSpellUsable));

        char getCurrentCastingInfo[] = "GetCurrentCastingInfo";
        RegisterLuaFunction(getCurrentCastingInfo, reinterpret_cast<uintptr_t *>(Script_GetCurrentCastingInfo));

        char getSpellIdForName[] = "GetSpellIdForName";
        RegisterLuaFunction(getSpellIdForName, reinterpret_cast<uintptr_t *>(Script_GetSpellIdForName));

        char getSpellNameAndRankForId[] = "GetSpellNameAndRankForId";
        RegisterLuaFunction(getSpellNameAndRankForId, reinterpret_cast<uintptr_t *>(Script_GetSpellNameAndRankForId));

        char getSpellSlotTypeIdForName[] = "GetSpellSlotTypeIdForName";
        RegisterLuaFunction(getSpellSlotTypeIdForName, reinterpret_cast<uintptr_t *>(Script_GetSpellSlotTypeIdForName));

        char channelStopCastingNextTick[] = "ChannelStopCastingNextTick";
        RegisterLuaFunction(channelStopCastingNextTick,
                            reinterpret_cast<uintptr_t *>(Script_ChannelStopCastingNextTick));

        char getNampowerVersion[] = "GetNampowerVersion";
        RegisterLuaFunction(getNampowerVersion, reinterpret_cast<uintptr_t *>(Script_GetNampowerVersion));

        char getItemILevel[] = "GetItemLevel";
        RegisterLuaFunction(getItemILevel, reinterpret_cast<uintptr_t *>(Script_GetItemLevel));

        // 2.14 additions
        char getItemStats[] = "GetItemStats";
        RegisterLuaFunction(getItemStats, reinterpret_cast<uintptr_t *>(Script_GetItemStats));

        char startItemExport[] = "StartItemExport";
        RegisterLuaFunction(startItemExport, reinterpret_cast<uintptr_t *>(Script_StartItemExport));

        char getSpellRec[] = "GetSpellRec";
        RegisterLuaFunction(getSpellRec, reinterpret_cast<uintptr_t *>(Script_GetSpellRec));

        char getItemStatsField[] = "GetItemStatsField";
        RegisterLuaFunction(getItemStatsField, reinterpret_cast<uintptr_t *>(Script_GetItemStatsField));

        char getSpellRecField[] = "GetSpellRecField";
        RegisterLuaFunction(getSpellRecField, reinterpret_cast<uintptr_t *>(Script_GetSpellRecField));

        char getUnitData[] = "GetUnitData";
        RegisterLuaFunction(getUnitData, reinterpret_cast<uintptr_t *>(Script_GetUnitData));

        char getUnitField[] = "GetUnitField";
        RegisterLuaFunction(getUnitField, reinterpret_cast<uintptr_t *>(Script_GetUnitField));

        char getSpellModifiers[] = "GetSpellModifiers";
        RegisterLuaFunction(getSpellModifiers, reinterpret_cast<uintptr_t *>(Script_GetSpellModifiers));

        // 2.16 additions
        char findPlayerItemSlot[] = "FindPlayerItemSlot";
        RegisterLuaFunction(findPlayerItemSlot, reinterpret_cast<uintptr_t *>(Script_FindPlayerItemSlot));

        char getEquippedItems[] = "GetEquippedItems";
        RegisterLuaFunction(getEquippedItems, reinterpret_cast<uintptr_t *>(Script_GetEquippedItems));

        char getEquippedItem[] = "GetEquippedItem";
        RegisterLuaFunction(getEquippedItem, reinterpret_cast<uintptr_t *>(Script_GetEquippedItem));

        char getBagItems[] = "GetBagItems";
        RegisterLuaFunction(getBagItems, reinterpret_cast<uintptr_t *>(Script_GetBagItems));

        char getBagItem[] = "GetBagItem";
        RegisterLuaFunction(getBagItem, reinterpret_cast<uintptr_t *>(Script_GetBagItem));

        // 2.17 additions
        char GetCastInfo[] = "GetCastInfo";
        RegisterLuaFunction(GetCastInfo, reinterpret_cast<uintptr_t *>(Script_GetCastInfo));

        char getSpellIdCooldown[] = "GetSpellIdCooldown";
        RegisterLuaFunction(getSpellIdCooldown, reinterpret_cast<uintptr_t *>(Script_GetSpellIdCooldown));

        char getItemIdCooldown[] = "GetItemIdCooldown";
        RegisterLuaFunction(getItemIdCooldown, reinterpret_cast<uintptr_t *>(Script_GetItemIdCooldown));

        char getTrinketCooldown[] = "GetTrinketCooldown";
        RegisterLuaFunction(getTrinketCooldown, reinterpret_cast<uintptr_t *>(Script_GetTrinketCooldown));

        // 2.19 additions
        char useItemIdOrName[] = "UseItemIdOrName";
        RegisterLuaFunction(useItemIdOrName, reinterpret_cast<uintptr_t *>(Script_UseItemIdOrName));

        char useTrinket[] = "UseTrinket";
        RegisterLuaFunction(useTrinket, reinterpret_cast<uintptr_t *>(Script_UseTrinket));

        char getTrinkets[] = "GetTrinkets";
        RegisterLuaFunction(getTrinkets, reinterpret_cast<uintptr_t *>(Script_GetTrinkets));

        char disenchantAll[] = "DisenchantAll";
        RegisterLuaFunction(disenchantAll, reinterpret_cast<uintptr_t *>(Script_DisenchantAll));
    }

    void load() {
        std::call_once(loadFlag, []() {
                           // hook SysMsgInitialize (bootstrap hook)
                           const hadesmem::Process process(::GetCurrentProcessId());

                           auto const sysMsgInitOrig = hadesmem::detail::AliasCast<SysMsgInitializeT>(
                               Offsets::SysMsgInitialize);
                           gSysMsgInitDetour = std::make_unique<hadesmem::PatchDetour<SysMsgInitializeT> >(
                               process,
                               sysMsgInitOrig,
                               &SysMsgInitializeHook);
                           gSysMsgInitDetour->Apply();
                       }
        );
    }
}

extern "C" __declspec(dllexport) uint32_t Load() {
    try {
        Nampower::load();
    } catch (const std::exception &e) {
        std::string errorMsg = "Exception in Nampower::load(): ";
        errorMsg += e.what();
        MessageBoxA(nullptr, errorMsg.c_str(), "Nampower Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    } catch (...) {
        MessageBoxA(nullptr, "Unknown exception in Nampower::load()", "Nampower Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
