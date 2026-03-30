//
// Created by pmacc on 9/21/2024.
//

#include "spellevents.hpp"
#include "offsets.hpp"
#include "logging.hpp"
#include "spellcast.hpp"
#include "helper.hpp"
#include "cooldown.hpp"

namespace Nampower {
    uint32_t lastCastResultTimeMs;
    constexpr int MAX_ALLOWED_SPELL_TARGETS = 100;

    enum SpellType : uint32_t {
        SPELL_TYPE_NORMAL = 0,
        SPELL_TYPE_CHANNELING = 1,
        SPELL_TYPE_AUTOREPEATING = 2,
    };


    void SignalEventHook(hadesmem::PatchDetourBase *detour, game::Events eventId) {
        auto const signalEvent = detour->GetTrampolineT<SignalEventT>();
        signalEvent(eventId);
    }

    uint32_t Script_SpellTargetUnitHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState) {
        auto const spellTargetUnit = detour->GetTrampolineT<LuaScriptT>();

        // check if valid string
        auto const lua_isstring = reinterpret_cast<lua_isstringT>(Offsets::lua_isstring);
        if (lua_isstring(luaState, 1)) {
            auto const lua_tostring = reinterpret_cast<lua_tostringT>(Offsets::lua_tostring);
            auto const unitName = lua_tostring(luaState, 1);

            auto const guid = GetUnitGuidFromString(unitName);
            if (guid) {
                DEBUG_LOG("Spell target unit " << unitName << " guid " << guid);
                // update all cast params so we don't have to figure out which one to use
                gLastNormalCastParams.guid = guid;
                gLastOnSwingCastParams.guid = guid;

                auto nonGcdCastParams = gNonGcdCastQueue.peek();
                if (nonGcdCastParams) {
                    nonGcdCastParams->guid = guid;
                }
            }
        }

        return spellTargetUnit(luaState);
    }


    void TriggerSpellFailedSelfEvent(uint32_t spellId,
                                     game::SpellCastResult spellResult,
                                     bool failedByServer) {
        static char format[] = "%d%d%d";

        ((int (__cdecl *)(int eventCode,
                          char *fmt,
                          uint32_t spellIdParam,
                          uint32_t spellResultParam,
                          uint32_t failedByServerParam)) Offsets::SignalEventParam)(
            game::SPELL_FAILED_SELF,
            format,
            spellId,
            static_cast<uint32_t>(spellResult),
            failedByServer ? 1 : 0);
    }

    void TriggerSpellFailedOtherEvent(uint64_t casterGuid,
                                      uint32_t spellId) {
        static char format[] = "%s%d";

        char *casterGuidStr = ConvertGuidToString(casterGuid);

        ((int (__cdecl *)(int eventCode,
                          char *fmt,
                          char *casterGuidParam,
                          uint32_t spellIdParam)) Offsets::SignalEventParam)(
            game::SPELL_FAILED_OTHER,
            format,
            casterGuidStr,
            spellId);

        FreeGuidString(casterGuidStr);
    }


    void TriggerSpellHealEvent(uint64_t targetGuid,
                               uint64_t casterGuid,
                               uint32_t spellId,
                               uint32_t amount,
                               uint8_t critical,
                               uint8_t periodic) {
        static char format[] = "%s%s%d%d%d%d";

        char *targetGuidStr = ConvertGuidToString(targetGuid);
        char *casterGuidStr = ConvertGuidToString(casterGuid);

        auto const activePlayerGuid = game::ClntObjMgrGetActivePlayerGuid();

        // SPELL_HEAL_BY_SELF/SPELL_HEAL_BY_OTHER based on caster
        auto casterEvent = game::SPELL_HEAL_BY_OTHER;
        if (casterGuid == activePlayerGuid) {
            casterEvent = game::SPELL_HEAL_BY_SELF;
        }

        ((int (__cdecl *)(int eventCode,
                          char *format,
                          char *targetGuid,
                          char *casterGuid,
                          uint32_t spellId,
                          uint32_t amount,
                          uint32_t critical,
                          uint32_t periodic)) Offsets::SignalEventParam)(
            casterEvent,
            format,
            targetGuidStr,
            casterGuidStr,
            spellId,
            amount,
            critical,
            periodic);

        // SPELL_HEAL_ON_SELF based on target
        if (targetGuid == activePlayerGuid) {
            ((int (__cdecl *)(int eventCode,
                              char *format,
                              char *targetGuid,
                              char *casterGuid,
                              uint32_t spellId,
                              uint32_t amount,
                              uint32_t critical,
                              uint32_t periodic)) Offsets::SignalEventParam)(
                game::SPELL_HEAL_ON_SELF,
                format,
                targetGuidStr,
                casterGuidStr,
                spellId,
                amount,
                critical,
                periodic);
        }

        FreeGuidString(targetGuidStr);
        FreeGuidString(casterGuidStr);
    }


    void TriggerSpellEnergizeEvent(uint64_t targetGuid,
                                   uint64_t casterGuid,
                                   uint32_t spellId,
                                   uint32_t powerType,
                                   uint32_t amount,
                                   uint8_t periodic) {
        static char format[] = "%s%s%d%d%d%d";

        char *targetGuidStr = ConvertGuidToString(targetGuid);
        char *casterGuidStr = ConvertGuidToString(casterGuid);

        auto const activePlayerGuid = game::ClntObjMgrGetActivePlayerGuid();

        // SPELL_ENERGIZE_BY_SELF/SPELL_ENERGIZE_BY_OTHER based on caster
        auto casterEvent = game::SPELL_ENERGIZE_BY_OTHER;
        if (casterGuid == activePlayerGuid) {
            casterEvent = game::SPELL_ENERGIZE_BY_SELF;
        }

        ((int (__cdecl *)(int eventCode,
                          char *format,
                          char *targetGuid,
                          char *casterGuid,
                          uint32_t spellId,
                          uint32_t powerType,
                          uint32_t amount,
                          uint32_t periodic)) Offsets::SignalEventParam)(
            casterEvent,
            format,
            targetGuidStr,
            casterGuidStr,
            spellId,
            powerType,
            amount,
            periodic);

        // SPELL_ENERGIZE_ON_SELF based on target
        if (targetGuid == activePlayerGuid) {
            ((int (__cdecl *)(int eventCode,
                              char *format,
                              char *targetGuid,
                              char *casterGuid,
                              uint32_t spellId,
                              uint32_t powerType,
                              uint32_t amount,
                              uint32_t periodic)) Offsets::SignalEventParam)(
                game::SPELL_ENERGIZE_ON_SELF,
                format,
                targetGuidStr,
                casterGuidStr,
                spellId,
                powerType,
                amount,
                periodic);
        }

        FreeGuidString(targetGuidStr);
        FreeGuidString(casterGuidStr);
    }

    void Spell_C_SpellFailedHook(hadesmem::PatchDetourBase *detour, uint32_t spellId,
                                 game::SpellCastResult spellResult, int unk1, int unk2, bool failedByServer) {
        auto const spellFailed = detour->GetTrampolineT<Spell_C_SpellFailedT>();
        spellFailed(spellId, spellResult, unk1, unk2, failedByServer);

        // ignore fake failure (used by Unleashed Potential and not sure what else)
        if (spellResult == game::SpellCastResult::SPELL_FAILED_DONT_REPORT) {
            return;
        }

        // ignore autoshot errors
        if (spellId == 75) {
            DEBUG_LOG("Ignoring autoshot failure with result " << int(spellResult));
            return;
        }

        // Check for Disenchant (13262) failure and stop the disenchant loop
        if (spellId == 13262) {
            DEBUG_LOG("Disenchant spell failed with result " << int(spellResult) << ", stopping disenchant loop");
            ResetDisenchantState();
        }

        // ignore SPELL_FAILED_CANT_DO_THAT_YET for arcane surge gets sent all the time after success
        if (spellResult == game::SpellCastResult::SPELL_FAILED_CANT_DO_THAT_YET &&
            (spellId == 51933 ||
             spellId == 51934 ||
             spellId == 51935 ||
             spellId == 51936)
        ) {
            return;
        }

        TriggerSpellFailedSelfEvent(spellId, spellResult, failedByServer);

        ResetCastFlags();

        if (spellId == gCastData.onSwingSpellId) {
            ResetOnSwingFlags();
        }

        if ((spellResult == game::SpellCastResult::SPELL_FAILED_NOT_READY ||
             spellResult == game::SpellCastResult::SPELL_FAILED_ITEM_NOT_READY ||
             spellResult == game::SpellCastResult::SPELL_FAILED_SPELL_IN_PROGRESS)
        ) {
            auto const currentTime = GetTime();

            if (spellResult == game::SpellCastResult::SPELL_FAILED_NOT_READY) {
                // check if spell is just on cooldown still
                auto const spellCooldown = GetRemainingCooldownForSpell(spellId);
                if (spellCooldown > 0) {
                    auto castParams = gCastHistory.findSpellId(spellId);
                    if (castParams) {
                        // mark as failed so it can be cast again
                        castParams->castResult = CastResult::SERVER_FAILURE;
                        // set flag to avoid canceling spell cast
                        gCastData.ignoreCancelDueToCooldown = true;
                    }

                    // check if we should do cooldown queuing
                    if (gUserSettings.queueSpellsOnCooldown && spellCooldown < gUserSettings.cooldownQueueWindowMs) {
                        if (castParams) {
                            if (castParams->castType == CastType::NON_GCD ||
                                castParams->castType == CastType::TARGETING_NON_GCD) {
                                DEBUG_LOG("Non gcd spell " << game::GetSpellName(spellId) << " is still on cooldown "
                                    << spellCooldown
                                    << " queuing retry");
                                gCastData.cooldownNonGcdSpellQueued = true;
                                gCastData.cooldownNonGcdEndMs = spellCooldown + currentTime;

                                TriggerSpellQueuedEvent(QueueEvents::NON_GCD_QUEUED, spellId);
                                gNonGcdCastQueue.push(*castParams, gUserSettings.replaceMatchingNonGcdCategory);
                            } else {
                                DEBUG_LOG("Spell " << game::GetSpellName(spellId) << " is still on cooldown "
                                    << spellCooldown
                                    << " queuing retry");
                                gCastData.cooldownNormalSpellQueued = true;
                                gCastData.cooldownNormalEndMs = spellCooldown + currentTime;

                                TriggerSpellQueuedEvent(QueueEvents::NORMAL_QUEUED, spellId);
                                gLastNormalCastParams = *castParams;
                            }
                            return;
                        }
                    } else {
                        DEBUG_LOG("Spell " << game::GetSpellName(spellId) << " is still on cooldown " << spellCooldown);
                        return;
                    }
                }
            } else if (spellResult == game::SpellCastResult::SPELL_FAILED_ITEM_NOT_READY) {
                // check if item is just on cooldown still
                auto castParams = gCastHistory.findSpellId(spellId);
                if (castParams && castParams->item) {
                    auto const itemId = game::GetItemId(castParams->item);
                    auto const itemCooldown = GetItemCooldownDetail(itemId);
                    if (itemCooldown.cooldownRemainingMs > 0) {
                        // mark as failed so it can be cast again
                        castParams->castResult = CastResult::SERVER_FAILURE;
                        // set flag to avoid canceling spell cast
                        gCastData.ignoreCancelDueToCooldown = true;
                        DEBUG_LOG("Item " << itemId << " is still on cooldown " << itemCooldown.cooldownRemainingMs);
                        return;
                    }
                }
            }

            if (!gUserSettings.retryServerRejectedSpells) {
                DEBUG_LOG("Cast failed for " << game::GetSpellName(spellId)
                    << " code " << int(spellResult)
                    << " not queuing retry due to retryServerRejectedSpells=false");
                return;
            }
            gLastErrorTimeMs = currentTime;

            uint64_t lastCastId = 0;

            // otherwise see if we should retry the cast
            auto castParams = gCastHistory.findSpellId(spellId);
            if (castParams) {
                lastCastId = castParams->castId;
                // if we find non retried cast params and the original cast time is within the last 500ms, retry the cast
                if (castParams->castStartTimeMs > currentTime - 500) {
                    // allow 3 retries
                    if (castParams->numRetries < 3) {
                        castParams->numRetries++; // mark as retried

                        if (castParams->castType == CastType::NON_GCD ||
                            castParams->castType == CastType::TARGETING_NON_GCD) {
                            // see if we have a recent successful cast of this spell
                            auto successfulCastParams = gCastHistory.findNewestSuccessfulSpellId(spellId);
                            if (successfulCastParams && successfulCastParams->castStartTimeMs > currentTime - 1000) {
                                // we found a recent successful cast of this spell, ignore the failure that was the result
                                // of spamming the cast
                                DEBUG_LOG("Cast failed for #" << lastCastId << " " << game::GetSpellName(spellId) << " "
                                    << " non gcd code " << int(spellResult)
                                    << ", but a recent cast succeeded, not retrying");
                                return;
                            }

                            DEBUG_LOG("Cast failed for #" << lastCastId << " " << game::GetSpellName(spellId) << " "
                                << " non gcd code " << int(spellResult)
                                << ", retry " << castParams->numRetries
                                << " result " << castParams->castResult);
                            gCastData.delayEndMs = currentTime + gBufferTimeMs; // retry after buffer delay
                            TriggerSpellQueuedEvent(QueueEvents::NON_GCD_QUEUED, spellId);
                            gCastData.nonGcdSpellQueued = true;
                            gNonGcdCastQueue.push(*castParams, gUserSettings.replaceMatchingNonGcdCategory);
                        } else {
                            // if gcd is active, do nothing
                            if (gCastData.gcdEndMs > currentTime) {
                                DEBUG_LOG("Cast failed for #" << lastCastId << " " << game::GetSpellName(spellId) << " "
                                    << " code " << int(spellResult)
                                    << ", gcd active not retrying");
                            } else {
                                DEBUG_LOG("Cast failed for #" << lastCastId << " " << game::GetSpellName(spellId) << " "
                                    << " code " << int(spellResult)
                                    << ", retry " << castParams->numRetries
                                    << " result " << castParams->castResult);

                                TriggerSpellQueuedEvent(QueueEvents::NORMAL_QUEUED, spellId);
                                gCastData.normalSpellQueued = true;
                                gLastNormalCastParams = *castParams;
                            }
                        }
                    } else {
                        DEBUG_LOG("Cast failed for #" << lastCastId << " " << game::GetSpellName(spellId) << " "
                            << " code " << int(spellResult)
                            << ", not retrying as it has already been retried 3 times");
                    }
                } else {
                    DEBUG_LOG("Cast failed for #" << lastCastId << " " << game::GetSpellName(spellId) << " "
                        << " code " << int(spellResult)
                        << ", no recent cast params found in history");
                }


                // check if we should increase the buffer time
                if (currentTime - gLastBufferIncreaseTimeMs > BUFFER_INCREASE_FREQUENCY) {
                    // check if gBufferTimeMs is already at max
                    if (gBufferTimeMs - gUserSettings.minBufferTimeMs < gUserSettings.maxBufferIncreaseMs) {
                        gBufferTimeMs += DYNAMIC_BUFFER_INCREMENT;
                        DEBUG_LOG("Increasing buffer to " << gBufferTimeMs);
                        gLastBufferIncreaseTimeMs = currentTime;
                    }
                }
            }
        } else if (gCastData.normalSpellQueued) {
            DEBUG_LOG("Cast failed for " << game::GetSpellName(spellId)
                << " spell queued + ignored code " << int(spellResult));
        } else {
            DEBUG_LOG("Cast failed for " << game::GetSpellName(spellId)
                << " ignored code " << int(spellResult));
        }
    }

    int SpellCooldownHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const spellCooldownHandler = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        if (packet->m_size == 16) {
            uint64_t targetGuid;
            packet->Get(targetGuid);

            uint32_t spellId;
            packet->Get(spellId);

            uint32_t cooldown;
            packet->Get(cooldown);

            packet->m_read = rpos;

            DEBUG_LOG("Spell cooldown opcode:" << opCode << " for " << game::GetSpellName(spellId) << " cooldown "
                << cooldown);
        }

        return spellCooldownHandler(opCode, packet);
    }


    void TriggerSpellDelayedEvent(uint64_t casterGuid,
                                  uint32_t delayMs) {
        static char format[] = "%s%d";

        char *casterGuidStr = ConvertGuidToString(casterGuid);

        auto event = game::SPELL_DELAYED_OTHER;
        if (casterGuid == game::ClntObjMgrGetActivePlayerGuid()) {
            event = game::SPELL_DELAYED_SELF;
        }

        ((int (__cdecl *)(int eventCode,
                          char *fmt,
                          char *casterGuidParam,
                          uint32_t delayParam)) Offsets::SignalEventParam)(
            event,
            format,
            casterGuidStr,
            delayMs);

        FreeGuidString(casterGuidStr);
    }

    void Spell_C_CooldownEventTriggeredHook(hadesmem::PatchDetourBase *detour,
                                            uint32_t spellId,
                                            uint64_t *targetGUID,
                                            int param_3,
                                            int clearCooldowns) {
        DEBUG_LOG("Cooldown event triggered for " << game::GetSpellName(spellId) << " " <<
            targetGUID << " " << param_3 << " " << clearCooldowns);

        auto const cooldownEventTriggered = detour->GetTrampolineT<Spell_C_CooldownEventTriggeredT>();
        cooldownEventTriggered(spellId, targetGUID, param_3, clearCooldowns);
    }

    int SpellDelayedHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const spellDelayed = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        uint64_t guid;
        packet->Get(guid);

        uint32_t delay;
        packet->Get(delay);

        packet->m_read = rpos;

        TriggerSpellDelayedEvent(guid, delay);

        auto const activePlayer = game::ClntObjMgrGetActivePlayerGuid();

        if (guid == activePlayer) {
            auto const currentTime = GetTime();

            // if we are casting a spell, and it was delayed, update our own state so we do not allow a cast too soon
            if (currentTime < gCastData.castEndMs) {
                gCastData.castEndMs += delay;

                auto lastCastParams = gCastHistory.peek();
                if (lastCastParams != nullptr) {
                    DEBUG_LOG("Spell delayed by " << delay << " for cast #" << lastCastParams->castId << " "
                        << game::GetSpellName(lastCastParams->spellId)
                        << " cast end " << gCastData.castEndMs);
                }
            }
        }

        return spellDelayed(opCode, packet);
    }

    int CastResultHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const castResultHandler = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        uint32_t spellId;
        packet->Get(spellId);

        uint8_t status;
        packet->Get(status);

        uint8_t spellCastResult;

        if (status != 0) {
            packet->Get(spellCastResult);
        } else {
            spellCastResult = 0;
        }

        packet->Get(spellCastResult);

        packet->m_read = rpos;

        auto const currentLatency = GetLatencyMs();
        auto currentTime = GetTime();

        // Reset the server delay in case we aren't able to calculate it
        gLastServerSpellDelayMs = 0;

        // if the cast was successful and the spell cast result was successful, and we have a latency
        // attempt to calculate the server delay
        if (status == 0 && spellCastResult == 0 && currentLatency > 0) {
            // try to find the cast in the history
            auto maxStartTime = currentTime - currentLatency;
            auto castParams = gCastHistory.findOldestWaitingForServerSpellId(spellId);

            if (castParams) {
                // running spellResponseTimeMs average
                auto const lastCastTime = castParams->castStartTimeMs;
                auto const spellResponseTimeMs = int32_t(currentTime - lastCastTime);

                auto const serverDelay = spellResponseTimeMs - int32_t(currentLatency + castParams->castTimeMs);

                if (serverDelay > 0) {
                    gLastServerSpellDelayMs = serverDelay + 15;
                } else {
                    DEBUG_LOG("Negative server delay using 1 ms " << serverDelay);
                    gLastServerSpellDelayMs = 1;
                }
            }
        }

        uint64_t matchingCastId = 0;

        // update cast history
        if (status == 0) {
            auto castParams = gCastHistory.findOldestWaitingForServerSpellId(spellId);
            if (castParams) {
                // successes normally for the oldest cast
                castParams->castResult = CastResult::SERVER_SUCCESS;
                matchingCastId = castParams->castId;
            }
        } else {
            auto castParams = gCastHistory.findNewestWaitingForServerSpellId(spellId);
            if (castParams) {
                // failures normally caused by the latest cast
                castParams->castResult = CastResult::SERVER_FAILURE;
                matchingCastId = castParams->castId;
            }
        }

        if (gLastServerSpellDelayMs > 0) {
            DEBUG_LOG("Cast result for #" << matchingCastId << " "
                << game::GetSpellName(spellId)
                << "(" << spellId << ")"
                << " status " << int(status)
                << " result " << int(spellCastResult) << " latency " << currentLatency
                << " server delay " << gLastServerSpellDelayMs
                << " since last cast result " << currentTime - lastCastResultTimeMs);
        } else {
            DEBUG_LOG("Cast result for #" << matchingCastId << " "
                << game::GetSpellName(spellId)
                << "(" << spellId << ")"
                << " status " << int(status)
                << " result " << int(spellCastResult) << " latency " << currentLatency
                << " since last cast result " << currentTime - lastCastResultTimeMs);
        }

        lastCastResultTimeMs = currentTime;

        return castResultHandler(opCode, packet);
    }

    int SpellFailedOtherHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const spellFailedOtherHandler = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        uint64_t guid;
        packet->Get(guid);

        uint32_t spellId;
        packet->Get(spellId);

        packet->m_read = rpos;

        TriggerSpellFailedOtherEvent(guid, spellId);

        return spellFailedOtherHandler(opCode, packet);
    }

    bool doesSpellApplyAura(const game::SpellRec *spell) {
        if (!spell) {
            return false;
        }

        for (unsigned int i: spell->Effect) {
            switch (i) {
                case game::SPELL_EFFECT_APPLY_AURA:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_RAID:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_PET:
                    return true;
                default:
                    break;
            }
        }

        return false;
    }

    void triggerAuraCastEventOnTarget(const game::SpellRec *spell,
                                      int spellEffectIndex,
                                      char *casterGuidStr,
                                      char *targetGuidStr, // if provided use this instead of creating one
                                      uint64_t targetGuid,
                                      uint64_t activePlayerGuid,
                                      uint32_t effectType,
                                      uint32_t duration) {
        static char format[] = "%d%s%s%d%d%d%d%d%d";

        auto eventToTrigger = game::AURA_CAST_ON_OTHER;
        if (targetGuid == activePlayerGuid) {
            eventToTrigger = game::AURA_CAST_ON_SELF;
        }

        auto auraName = spell->EffectApplyAuraName[spellEffectIndex];
        auto effectAmplitude = spell->EffectAmplitude[spellEffectIndex];
        auto effectMiscValue = spell->EffectMiscValue[spellEffectIndex];

        auto *targetUnit = game::ClntObjMgrObjectPtr(
            static_cast<game::TypeMask>(game::TYPEMASK_PLAYER | game::TYPEMASK_UNIT), targetGuid);

        bool targetIsBuffCapped = game::UnitIsBuffCapped(targetUnit);
        bool targetIsDebuffCapped = game::UnitIsDebuffCapped(targetUnit);

        bool createdGuidStr = false;
        if (!targetGuidStr) {
            if (targetGuid > 0) {
                targetGuidStr = ConvertGuidToString(targetGuid);
                createdGuidStr = true;
            }
        }

        auto auraCapStatus = static_cast<uint32_t>(targetIsBuffCapped) |
                             (static_cast<uint32_t>(targetIsDebuffCapped) << 1);

        reinterpret_cast<int (__cdecl *)(int eventCode,
                                         char *fmt,
                                         uint32_t spellIdParam,
                                         char *casterGuidStrParam,
                                         char *targetGuidStrParam,
                                         uint32_t effectParam,
                                         uint32_t auraNameParam,
                                         uint32_t effectAmplitudeParam,
                                         uint32_t effectMiscValueParam,
                                         uint32_t durationParam,
                                         uint32_t auraCapStatusParam)>(Offsets::SignalEventParam)(
            eventToTrigger,
            format,
            spell->Id,
            casterGuidStr,
            targetGuidStr,
            effectType,
            auraName,
            effectAmplitude,
            effectMiscValue,
            duration,
            auraCapStatus);

        if (createdGuidStr) {
            FreeGuidString(targetGuidStr);
        }
    }

    void TriggerAuraCastEvent(const game::SpellRec *spell,
                              uint64_t casterGuid,
                              uint64_t targetGuids[MAX_ALLOWED_SPELL_TARGETS],
                              uint8_t numTargets,
                              uint64_t activePlayerGuid,
                              bool castByActivePlayer) {
        // ignore modifiers if not cast by active player
        auto duration = game::GetSpellDuration(spell, !castByActivePlayer);
        char *casterGuidStr = ConvertGuidToString(casterGuid);

        for (int i = 0; i < 3; ++i) {
            auto const effectType = spell->Effect[i];
            switch (effectType) {
                case game::SPELL_EFFECT_APPLY_AURA:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_RAID:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_PET: {
                    auto const implicitTargetA = spell->EffectImplicitTargetA[i];
                    auto const implicitTargetB = spell->EffectImplicitTargetB[i];

                    if (implicitTargetA == game::TARGET_UNIT_CASTER) {
                        triggerAuraCastEventOnTarget(spell, i, casterGuidStr, casterGuidStr, casterGuid,
                                                     activePlayerGuid,
                                                     effectType, duration);
                        break;
                    }

                    // handle no targets
                    if (numTargets == 0 || implicitTargetA == game::TARGET_NONE) {
                        triggerAuraCastEventOnTarget(spell, i, casterGuidStr, nullptr, 0, activePlayerGuid,
                                                     effectType, duration);
                    } else if ((implicitTargetA >= game::TARGET_UNIT_ENEMY_NEAR_CASTER && implicitTargetA <=
                                game::TARGET_UNIT_ENEMY) ||
                               (implicitTargetA >= game::TARGET_PLAYER_NYI && implicitTargetA <=
                                game::TARGET_PLAYER_FRIEND_NYI) ||
                               implicitTargetA == game::TARGET_UNIT_FRIEND ||
                               implicitTargetA == game::TARGET_UNIT ||
                               implicitTargetA == game::TARGET_UNIT_CASTER_MASTER) {
                        // trigger on first target
                        triggerAuraCastEventOnTarget(spell, i, casterGuidStr, nullptr, targetGuids[0], activePlayerGuid,
                                                     effectType, duration);
                    } else if (implicitTargetA == game::TARGET_ENUM_UNITS_ENEMY_AOE_AT_SRC_LOC ||
                               implicitTargetA == game::TARGET_ENUM_UNITS_ENEMY_AOE_AT_DEST_LOC ||
                               implicitTargetA == game::TARGET_ENUM_UNITS_ENEMY_IN_CONE_24 ||
                               implicitTargetA == game::TARGET_ENUM_UNITS_ENEMY_AOE_AT_DYNOBJ_LOC ||
                               implicitTargetA == game::TARGET_ENUM_UNITS_ENEMY_WITHIN_CASTER_RANGE ||
                               implicitTargetA == game::TARGET_ENUM_UNITS_ENEMY_IN_CONE_54 ||
                               implicitTargetB == game::TARGET_ENUM_UNITS_ENEMY_AOE_AT_SRC_LOC ||
                               implicitTargetB == game::TARGET_ENUM_UNITS_ENEMY_AOE_AT_DEST_LOC ||
                               implicitTargetB == game::TARGET_ENUM_UNITS_ENEMY_IN_CONE_24 ||
                               implicitTargetB == game::TARGET_ENUM_UNITS_ENEMY_AOE_AT_DYNOBJ_LOC ||
                               implicitTargetB == game::TARGET_ENUM_UNITS_ENEMY_WITHIN_CASTER_RANGE ||
                               implicitTargetB == game::TARGET_ENUM_UNITS_ENEMY_IN_CONE_54) {
                        // trigger on all targets except caster
                        for (int t = 0; t < numTargets; ++t) {
                            if (targetGuids[t] != casterGuid) {
                                triggerAuraCastEventOnTarget(spell, i, casterGuidStr, nullptr, targetGuids[t],
                                                             activePlayerGuid,
                                                             effectType, duration);
                            }
                        }
                    } else {
                        // trigger on all targets (probably some exceptions but good enough for now)
                        for (int t = 0; t < numTargets; ++t) {
                            triggerAuraCastEventOnTarget(spell, i, casterGuidStr, nullptr, targetGuids[t],
                                                         activePlayerGuid,
                                                         effectType, duration);
                        }
                    }

                    break;
                }
                default:
                    break;
            }
        }

        FreeGuidString(casterGuidStr);
    }

    void TriggerSpellStartEvent(uint32_t itemId,
                                uint32_t spellId,
                                uint64_t casterGuid,
                                uint64_t targetGuid,
                                uint64_t corpseOwnerGuid,
                                uint16_t castFlags,
                                uint32_t castTime) {
        static char format[] = "%d%d%s%s%d%d%d%d%s";

        char *casterGuidStr = ConvertGuidToString(casterGuid);
        char *targetGuidStr = ConvertGuidToString(targetGuid);
        char *corpseOwnerGuidStr = corpseOwnerGuid ? ConvertGuidToString(corpseOwnerGuid) : nullptr;

        uint32_t duration = 0;
        SpellType spellType = SPELL_TYPE_NORMAL;
        auto spellRec = game::GetSpellInfo(spellId);
        if (spellRec) {
            if (spellRec->AttributesEx2 & game::SpellAttributesEx2::SPELL_ATTR_EX2_AUTO_REPEAT) {
                spellType = SPELL_TYPE_AUTOREPEATING;
            } else if (spellRec->AttributesEx & (game::SpellAttributesEx::SPELL_ATTR_EX_IS_CHANNELED | game::SpellAttributesEx::SPELL_ATTR_EX_IS_SELF_CHANNELED)) {
                spellType = SPELL_TYPE_CHANNELING;
                auto dur = game::GetSpellDuration(spellRec, casterGuid != game::ClntObjMgrGetActivePlayerGuid());
                if (dur > 0) {
                    duration = static_cast<uint32_t>(dur);
                }
            }
        }

        auto event = game::SPELL_START_OTHER;
        if (casterGuid == game::ClntObjMgrGetActivePlayerGuid()) {
            event = game::SPELL_START_SELF;
        }

        ((int (__cdecl *)(int eventCode,
                          char *fmt,
                          uint32_t itemIdParam,
                          uint32_t spellIdParam,
                          char *casterGuidParam,
                          char *targetGuidParam,
                          uint32_t castFlagsParam,
                          uint32_t castTimeParam,
                          uint32_t durationParam,
                          uint32_t spellTypeParam,
                          char *corpseTargetGuidParam)) Offsets::SignalEventParam)(
            event,
            format,
            itemId,
            spellId,
            casterGuidStr,
            targetGuidStr,
            castFlags,
            castTime,
            duration,
            spellType,
            corpseOwnerGuidStr);

        FreeGuidString(casterGuidStr);
        FreeGuidString(targetGuidStr);
        FreeGuidString(corpseOwnerGuidStr);
    }

    void TriggerSpellGoEvent(uint32_t itemId,
                             uint32_t spellId,
                             const uint64_t *casterGuid,
                             uint64_t targetGuid,
                             uint64_t corpseOwnerGuid,
                             uint16_t castFlags,
                             uint8_t numTargetsHit,
                             uint8_t numTargetsMissed) {
        static char format[] = "%d%d%s%s%d%d%d%s";

        uint64_t casterGuidValue = casterGuid ? *casterGuid : 0;
        char *casterGuidStr = ConvertGuidToString(casterGuidValue);
        char *targetGuidStr = ConvertGuidToString(targetGuid);
        char *corpseOwnerGuidStr = corpseOwnerGuid ? ConvertGuidToString(corpseOwnerGuid) : nullptr;

        auto event = game::SPELL_GO_OTHER;
        if (casterGuidValue == game::ClntObjMgrGetActivePlayerGuid()) {
            event = game::SPELL_GO_SELF;
        }

        ((int (__cdecl *)(int eventCode,
                          char *fmt,
                          uint32_t itemIdParam,
                          uint32_t spellIdParam,
                          char *casterGuidParam,
                          char *targetGuidParam,
                          uint32_t castFlagsParam,
                          uint32_t numTargetsHitParam,
                          uint32_t numTargetsMissedParam,
                          char *corpseTargetGuidParam)) Offsets::SignalEventParam)(
            event,
            format,
            itemId,
            spellId,
            casterGuidStr,
            targetGuidStr,
            castFlags,
            numTargetsHit,
            numTargetsMissed,
            corpseOwnerGuidStr);

        FreeGuidString(casterGuidStr);
        FreeGuidString(targetGuidStr);
        FreeGuidString(corpseOwnerGuidStr);
    }

    void TriggerSpellMissEvent(uint64_t casterGuid,
                               uint64_t targetGuid,
                               uint32_t spellId,
                               uint32_t missInfo) {
        static char format[] = "%s%s%d%d";

        char *casterGuidStr = ConvertGuidToString(casterGuid);
        char *targetGuidStr = ConvertGuidToString(targetGuid);

        auto event = game::SPELL_MISS_OTHER;
        if (casterGuid == game::ClntObjMgrGetActivePlayerGuid()) {
            event = game::SPELL_MISS_SELF;
        }

        ((int (__cdecl *)(int eventCode,
                          char *fmt,
                          char *casterGuidParam,
                          char *targetGuidParam,
                          uint32_t spellIdParam,
                          uint32_t missInfoParam)) Offsets::SignalEventParam)(
            event,
            format,
            casterGuidStr,
            targetGuidStr,
            spellId,
            missInfo);

        FreeGuidString(casterGuidStr);
        FreeGuidString(targetGuidStr);
    }

    void SpellGoHook(hadesmem::PatchDetourBase *detour, uint64_t *itemGUID, uint64_t *casterGUID,
                     uint32_t spellId, CDataStore *packet) {
        auto const rpos = packet->m_read;

        int16_t castFlags;
        packet->Get(castFlags);

        uint8_t numTargetsHit;
        packet->Get(numTargetsHit);

        static uint64_t targetHitGuids[MAX_ALLOWED_SPELL_TARGETS]; // don't allocate new array each time
        // each target corresponds to a spell effect, should never have more than 3
        for (int i = 0; i < numTargetsHit && i < MAX_ALLOWED_SPELL_TARGETS; ++i) {
            targetHitGuids[i] = 0;
            packet->Get(targetHitGuids[i]);
        }

        uint64_t casterGuidValue = casterGUID ? *casterGUID : 0;

        uint8_t numTargetsMissed;
        packet->Get(numTargetsMissed);
        for (int i = 0; i < numTargetsMissed; ++i) {
            uint64_t missedTargetGuid;
            packet->Get(missedTargetGuid);

            uint8_t missCondition;
            packet->Get(missCondition);
            // Reflect entries include an extra result byte in SpellGoTargets.
            if (missCondition == game::SPELL_MISS_REFLECT) {
                uint8_t reflectResult;
                packet->Get(reflectResult);
            }

            TriggerSpellMissEvent(casterGuidValue, missedTargetGuid, spellId, missCondition);
        }

        int16_t spellCastTargetMask;
        packet->Get(spellCastTargetMask);

        uint64_t unitTargetGUID = 0;
        uint64_t corpseTargetGUID = 0;
        if (spellCastTargetMask & (game::TARGET_FLAG_UNIT | game::TARGET_FLAG_PVP_CORPSE | game::TARGET_FLAG_OBJECT | game::TARGET_FLAG_CORPSE | game::TARGET_FLAG_UNK2)) {
            if (spellCastTargetMask & game::TARGET_FLAG_UNIT) {
                packet->GetPackedGuid(unitTargetGUID);
            } else if (spellCastTargetMask & (game::TARGET_FLAG_OBJECT | game::TARGET_FLAG_OBJECT_UNK)) {
                uint64_t goTargetGUID = 0;
                packet->GetPackedGuid(goTargetGUID);
            } else if (spellCastTargetMask & (game::TARGET_FLAG_CORPSE | game::TARGET_FLAG_PVP_CORPSE)) {
                uint64_t corpseGUID = 0;
                packet->GetPackedGuid(corpseGUID);
                auto *corpse = game::ClntObjMgrObjectPtr(game::TYPEMASK_CORPSE, corpseGUID);
                if (corpse) {
                    corpseTargetGUID = game::GetCorpseOwner(corpse);
                }
            }
        }

        uint32_t itemId = 0;
        if (itemGUID && *itemGUID != 0 && *itemGUID != casterGuidValue) {
            auto *item = reinterpret_cast<game::CGItem_C *>(game::ClntObjMgrObjectPtr(
                game::TYPEMASK_ITEM, *itemGUID));
            if (item) {
                itemId = game::GetItemId(item);
            }
        }

        if (gUserSettings.enableSpellGoEvents) {
            TriggerSpellGoEvent(itemId, spellId, casterGUID, unitTargetGUID, corpseTargetGUID, castFlags,
                                numTargetsHit, numTargetsMissed);
        }

        // reset read pointer
        packet->m_read = rpos;

        auto const spellGo = detour->GetTrampolineT<SpellGoT>();
        // if this wasn't triggered by an item, itemGUID will just be the casterGUID duplicated
        spellGo(itemGUID, casterGUID, spellId, packet);

        auto const activePlayerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto const castByActivePlayer = activePlayerGuid == casterGuidValue;
        auto const spell = game::GetSpellInfo(spellId);

        if (!spell || spell->Id <= 0) {
            DEBUG_LOG("Unable to determine spell information in SpellGo for " << spellId);
            return;
        }

        if (castByActivePlayer) {
            auto const currentTime = GetTime();
            // only care about our own casts
            if (!gCastData.channeling) {
                // check if spell is on swing
                if (spell->Attributes & game::SPELL_ATTR_ON_NEXT_SWING_1) {
                    gLastCastData.onSwingStartTimeMs = currentTime;

                    gCastData.pendingOnSwingCast = false;

                    if (gCastData.onSwingQueued) {
                        DEBUG_LOG("On swing spell " << game::GetSpellName(spellId) <<
                            " resolved, casting queued on swing spell "
                            << game::GetSpellName(gLastOnSwingCastParams.spellId));

                        TriggerSpellQueuedEvent(ON_SWING_QUEUE_POPPED, gLastOnSwingCastParams.spellId);
                        Spell_C_CastSpellHook(castSpellDetour, gLastOnSwingCastParams.casterUnit,
                                              gLastOnSwingCastParams.spellId,
                                              gLastOnSwingCastParams.item, gLastOnSwingCastParams.guid);
                        gCastData.onSwingQueued = false;
                    }
                }
            }
        }

        if (gUserSettings.enableAuraCastEvents && doesSpellApplyAura(spell)) {
            TriggerAuraCastEvent(spell, *casterGUID, targetHitGuids, numTargetsHit, activePlayerGuid,
                                 castByActivePlayer);
        }
    }

    int SpellStartHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t unk, uint32_t opCode, uint32_t unk2,
                              CDataStore *packet) {
        auto const spellStartHandler = detour->GetTrampolineT<FastCallPacketHandlerT>();

        auto const rpos = packet->m_read;

        uint32_t previousVisualSpellId = 0;

        if (opCode == 0x131) {
            // 8 + 8 + 4 + 2 + 4 but first 2 guids are packed
            uint64_t itemGuid;
            packet->GetPackedGuid(itemGuid);

            uint64_t casterGuid;
            packet->GetPackedGuid(casterGuid);

            uint32_t spellId;
            packet->Get(spellId);

            uint16_t castFlags;
            packet->Get(castFlags);

            uint32_t castTime;
            packet->Get(castTime);

            int16_t spellCastTargetMask;
            packet->Get(spellCastTargetMask);

            uint64_t unitTargetGUID = 0;
            uint64_t goTargetGUID = 0;
            uint64_t corpseTargetGUID = 0;
            uint64_t itemTargetGUID = 0;

            if (spellCastTargetMask & (game::TARGET_FLAG_UNIT | game::TARGET_FLAG_PVP_CORPSE | game::TARGET_FLAG_OBJECT | game::TARGET_FLAG_CORPSE | game::TARGET_FLAG_UNK2)) {
                if (spellCastTargetMask & game::TARGET_FLAG_UNIT) {
                    packet->GetPackedGuid(unitTargetGUID);
                } else if (spellCastTargetMask & (game::TARGET_FLAG_OBJECT | game::TARGET_FLAG_OBJECT_UNK)) {
                    packet->GetPackedGuid(goTargetGUID);
                } else if (spellCastTargetMask & (game::TARGET_FLAG_CORPSE | game::TARGET_FLAG_PVP_CORPSE)) {
                    uint64_t corpseGUID = 0;
                    packet->GetPackedGuid(corpseGUID);
                    auto *corpse = game::ClntObjMgrObjectPtr(game::TYPEMASK_CORPSE, corpseGUID);
                    if (corpse) {
                        corpseTargetGUID = game::GetCorpseOwner(corpse);
                    }
                }
            }

            if (spellCastTargetMask & (game::TARGET_FLAG_ITEM | game::TARGET_FLAG_TRADE_ITEM)) {
                packet->GetPackedGuid(itemTargetGUID);
            }

            uint32_t itemId = 0;
            if (itemGuid != 0 && itemGuid != casterGuid) {
                auto *item = reinterpret_cast<game::CGItem_C *>(game::ClntObjMgrObjectPtr(
                    game::TYPEMASK_ITEM, itemGuid));
                if (item) {
                    itemId = game::GetItemId(item);
                }
            }

            if (gUserSettings.enableSpellStartEvents) {
                TriggerSpellStartEvent(itemId, spellId, casterGuid, unitTargetGUID, corpseTargetGUID, castFlags, castTime);
            }

            if (casterGuid == game::ClntObjMgrGetActivePlayerGuid()) {
                bool isAutoRepeat = false;

                auto spellInfo = game::GetSpellInfo(spellId);
                if (spellInfo) {
                    isAutoRepeat = spellInfo->AttributesEx2 & game::SpellAttributesEx2::SPELL_ATTR_EX2_AUTO_REPEAT;
                }

                auto castParams = gCastHistory.findSpellId(spellId);

                // check if cast time differed from what we expected, ignore haste rounding errors
                if (castParams && castParams->spellId == spellId) {
                    if (!isAutoRepeat && castParams->castTimeMs < castTime) {
                        // server cast time increased
                        auto castTimeDifference = castTime - castParams->castTimeMs;

                        if (castTimeDifference > 5) {
                            gCastData.castEndMs = castParams->castStartTimeMs + castTime;
                            if (castTime > 0) {
                                gCastData.castEndMs += gBufferTimeMs;
                            }

                            castParams->castTimeMs = castTime;
                            DEBUG_LOG("Server cast time for " << game::GetSpellName(spellId) << " increased by "
                                << castTimeDifference << "ms.  Updated cast end time to "
                                << gCastData.castEndMs);
                        }
                    } else if (!isAutoRepeat && castParams->castTimeMs > castTime) {
                        // server cast time decreased
                        auto castTimeDifference = castParams->castTimeMs - castTime;

                        if (castTimeDifference > 5) {
                            auto gcdTime = GetGcdOrCooldownForSpell(spellId);
                            if (gcdTime > 1500) {
                                gcdTime = 1500;
                                // items with spells on gcd will return their item gcd, make sure not to use that
                            }

                            if (gcdTime > castTime) {
                                DEBUG_LOG("Server cast time " << castTime << " was reduced below gcd of " << gcdTime
                                    << " using gcd instead");
                                castTime = gcdTime;
                                gCastData.castEndMs = castParams->castStartTimeMs + gcdTime;
                            } else {
                                gCastData.castEndMs = castParams->castStartTimeMs + castTime + gBufferTimeMs;
                            }

                            castParams->castTimeMs = castTime;
                            DEBUG_LOG("Server cast time for " << game::GetSpellName(spellId) << " decreased by "
                                << castTimeDifference << "ms.  Updated cast end time to "
                                << gCastData.castEndMs);
                        }
                    }

                    // spell start successful, reset castParams->numRetries
                    castParams->numRetries = 0;
                }

                // spell start successful, reset gLastNormalCastParams.numRetries
                gLastNormalCastParams.numRetries = 0;

                // avoid clearing visual spell id as much as possible as it can cause weird sound/animation issues
                // only do it for normal gcd spells with a cast time
                if (castTime > 0 && gLastCastData.wasQueued && gLastCastData.wasOnGcd && !gLastCastData.wasItem) {
                    auto visualSpellId = reinterpret_cast<uint32_t *>(Offsets::VisualSpellId);
                    // only clear if the current visual spell id is the same as the spell we are casting
                    // as that seems to be when cast animation breaks
                    if (*visualSpellId == spellId) {
                        *visualSpellId = 0;

                        previousVisualSpellId = spellId;
                    }
                }
            }
        }

        packet->m_read = rpos;

        auto result = spellStartHandler(unk, opCode, unk2, packet);

        if (previousVisualSpellId > 0) {
            auto currentSpellId = reinterpret_cast<uint32_t *>(Offsets::VisualSpellId);
            *currentSpellId = previousVisualSpellId;
        }

        return result;
    }

    void
    TriggerSpellDamageEvent(uint64_t targetGuid,
                            uint64_t casterGuid,
                            uint32_t spellId,
                            uint32_t amount,
                            uint32_t spellSchool,
                            uint32_t absorb,
                            uint32_t blocked,
                            int32_t resist,
                            uint32_t auraType,
                            uint32_t hitInfo) {
        static char format[] = "%s%s%d%d%s%d%d%s";

        char *targetGuidStr = ConvertGuidToString(targetGuid);

        char *casterGuidStr = ConvertGuidToString(casterGuid);

        auto event = game::SPELL_DAMAGE_EVENT_OTHER;

        if (casterGuid == game::ClntObjMgrGetActivePlayerGuid()) {
            event = game::SPELL_DAMAGE_EVENT_SELF;

            if (gCastData.channeling && gCastData.channelSpellId == spellId) {
                gCastData.channelNumTicks++;
            }
        }

        auto spell = game::GetSpellInfo(spellId);

        std::ostringstream mitigationStream;
        mitigationStream << absorb << "," << blocked << "," << resist;

        std::ostringstream effectsAuraTypeStream;
        if (spell) {
            effectsAuraTypeStream << spell->Effect[0] << "," << spell->Effect[1] << "," << spell->Effect[2];
        } else {
            effectsAuraTypeStream << "0,0,0";
        }
        // add aura type to the end
        effectsAuraTypeStream << "," << auraType;

        ((int (__cdecl *)(int eventCode,
                          char *format,
                          char *targetGuid,
                          char *casterGuid,
                          uint32_t spellId,
                          uint32_t amount,
                          const char *mitigationStr,
                          uint32_t hitInfo,
                          uint32_t spellSchool,
                          const char *effectAuraStr)) Offsets::SignalEventParam)(
            event,
            format,
            targetGuidStr,
            casterGuidStr,
            spellId,
            amount,
            mitigationStream.str().c_str(),
            hitInfo,
            spellSchool,
            effectsAuraTypeStream.str().c_str());

        FreeGuidString(targetGuidStr);
        FreeGuidString(casterGuidStr);
    }

    int PeriodicAuraLogHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t unk, uint32_t opCode, uint32_t unk2,
                                   CDataStore *packet) {
        auto const rpos = packet->m_read;

        uint64_t targetGuid;
        packet->GetPackedGuid(targetGuid);

        uint64_t casterGuid;
        packet->GetPackedGuid(casterGuid);

        uint32_t spellId;
        packet->Get(spellId);

        uint32_t count;
        packet->Get(count);

        uint32_t auraType;
        packet->Get(auraType);

        uint32_t amount;
        uint32_t powerType;

        switch (auraType) {
            case 3: // SPELL_AURA_PERIODIC_DAMAGE
            case 89: // SPELL_AURA_PERIODIC_DAMAGE_PERCENT
                packet->Get(amount); // damage amount

                uint32_t spellSchool;
                packet->Get(spellSchool);

                uint32_t absorb;
                packet->Get(absorb);

                int32_t resist;
                packet->Get(resist);

                TriggerSpellDamageEvent(targetGuid, casterGuid, spellId, amount, spellSchool, absorb, 0, resist,
                                        auraType,
                                        0);
                break;
            case 8: // SPELL_AURA_PERIODIC_HEAL
            case 20: // SPELL_AURA_OBS_MOD_HEALTH
                packet->Get(amount); // heal amount

                if (gUserSettings.enableSpellHealEvents) {
                    TriggerSpellHealEvent(targetGuid, casterGuid, spellId, amount, 0, 1);
                }
                break;
            case 21: // SPELL_AURA_OBS_MOD_MANA
            case 24: // SPELL_AURA_PERIODIC_ENERGIZE
                packet->Get(powerType);

                packet->Get(amount); // power type amount

                if (gUserSettings.enableSpellEnergizeEvents) {
                    TriggerSpellEnergizeEvent(targetGuid, casterGuid, spellId, powerType, amount, 1);
                }
                break;
            case 64: // SPELL_AURA_PERIODIC_MANA_LEECH
                packet->Get(powerType);
                packet->Get(amount); // power type amount

                uint32_t multiplier;
                packet->Get(multiplier); // gain multiplier

                // No custom event for these for now
                break;
            default:
                break;
        }

        packet->m_read = rpos;

        auto const periodicAuraLogHandler = detour->GetTrampolineT<FastCallPacketHandlerT>();
        return periodicAuraLogHandler(unk, opCode, unk2, packet);
    }

    int SpellNonMeleeDmgLogHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t unk, uint32_t opCode, uint32_t unk2,
                                       CDataStore *packet) {
        auto const rpos = packet->m_read;

        uint64_t targetGuid;
        packet->GetPackedGuid(targetGuid);

        uint64_t casterGuid;
        packet->GetPackedGuid(casterGuid);

        uint32_t spellId;
        packet->Get(spellId);

        uint32_t damage;
        packet->Get(damage);

        uint8_t school;
        packet->Get(school);

        uint32_t absorb;
        packet->Get(absorb);

        int32_t resist;
        packet->Get(resist);

        uint8_t periodicLog;
        packet->Get(periodicLog);

        uint8_t unused;
        packet->Get(unused);

        uint32_t blocked;
        packet->Get(blocked);

        uint32_t hitInfo;
        packet->Get(hitInfo);

        uint8_t extendData;
        packet->Get(extendData);

        packet->m_read = rpos;

        TriggerSpellDamageEvent(targetGuid, casterGuid, spellId, damage, school, absorb, blocked, resist, 0, hitInfo);

        auto const spellNonMeleeDmgLogHandler = detour->GetTrampolineT<FastCallPacketHandlerT>();
        return spellNonMeleeDmgLogHandler(unk, opCode, unk2, packet);
    }

    int PlaySpellVisualHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const playSpellVisualHandler = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        uint64_t targetGuid;
        packet->GetPackedGuid(targetGuid);

        uint32_t spellVisualKitIndex;
        packet->Get(spellVisualKitIndex);

        packet->m_read = rpos;

        DEBUG_LOG("Play spell visual id " << spellVisualKitIndex << " for guid " << targetGuid);

        //        return playSpellVisualHandler(opCode, packet);

        return 1;
    }

    int SpellHealingLogHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const spellHealingLogHandler = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        uint64_t targetGuid;
        packet->GetPackedGuid(targetGuid);

        uint64_t casterGuid;
        packet->GetPackedGuid(casterGuid);

        uint32_t spellId;
        packet->Get(spellId);

        uint32_t amount;
        packet->Get(amount);

        uint8_t critical;
        packet->Get(critical);

        packet->m_read = rpos;

        if (gUserSettings.enableSpellHealEvents) {
            TriggerSpellHealEvent(targetGuid, casterGuid, spellId, amount, critical, 0);
        }

        return spellHealingLogHandler(opCode, packet);
    }

    int SpellEnergizeLogHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const spellEnergizeLogHandler = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        uint64_t targetGuid;
        packet->GetPackedGuid(targetGuid);

        uint64_t casterGuid;
        packet->GetPackedGuid(casterGuid);

        uint32_t spellId;
        packet->Get(spellId);

        uint32_t powerType;
        packet->Get(powerType);

        uint32_t amount;
        packet->Get(amount);

        packet->m_read = rpos;

        if (gUserSettings.enableSpellEnergizeEvents) {
            TriggerSpellEnergizeEvent(targetGuid, casterGuid, spellId, powerType, amount, 0);
        }

        return spellEnergizeLogHandler(opCode, packet);
    }

    int ProcResistHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const procResistHandler = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        uint64_t casterGuid;
        packet->Get(casterGuid);

        uint64_t targetGuid;
        packet->Get(targetGuid);

        uint32_t spellId;
        packet->Get(spellId);

        uint8_t isDebug;
        packet->Get(isDebug);

        packet->m_read = rpos;

        // ProcResist is always SPELL_MISS_RESIST (2)
        TriggerSpellMissEvent(casterGuid, targetGuid, spellId, 2);

        return procResistHandler(opCode, packet);
    }

    int SpellLogMissHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const spellLogMissHandler = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        uint32_t spellId;
        packet->Get(spellId);

        uint64_t casterGuid;
        packet->Get(casterGuid);

        uint8_t unk;
        packet->Get(unk);

        uint32_t targetCount;
        packet->Get(targetCount);

        uint64_t targetGuid;
        packet->Get(targetGuid);

        uint8_t missCondition;
        packet->Get(missCondition);

        TriggerSpellMissEvent(casterGuid, targetGuid, spellId, missCondition);

        packet->m_read = rpos;

        return spellLogMissHandler(opCode, packet);
    }

    int SpellOrDamageImmuneHandlerHook(hadesmem::PatchDetourBase *detour, uint32_t *opCode, CDataStore *packet) {
        auto const spellOrDamageImmuneHandler = detour->GetTrampolineT<PacketHandlerT>();

        auto const rpos = packet->m_read;

        uint64_t casterGuid;
        packet->Get(casterGuid);

        uint64_t targetGuid;
        packet->Get(targetGuid);

        uint32_t spellId;
        packet->Get(spellId);

        uint8_t unk;
        packet->Get(unk);

        packet->m_read = rpos;

        // SPELL_MISS_IMMUNE = 7
        TriggerSpellMissEvent(casterGuid, targetGuid, spellId, 7);

        return spellOrDamageImmuneHandler(opCode, packet);
    }

    void UnitCombatLogDispelledHook(hadesmem::PatchDetourBase *detour, uint64_t *casterGuid, uint64_t *targetGuid,
                                    uint32_t spellId) {
        auto const original = detour->GetTrampolineT<UnitCombatLogDispelledT>();
        original(casterGuid, targetGuid, spellId);

        if (!casterGuid || *casterGuid == 0) return;

        static char format[] = "%s%s%d";
        char *casterGuidStr = ConvertGuidToString(*casterGuid);
        char *targetGuidStr = targetGuid ? ConvertGuidToString(*targetGuid) : nullptr;

        auto event = game::SPELL_DISPEL_BY_OTHER;
        if (*casterGuid == game::ClntObjMgrGetActivePlayerGuid())
            event = game::SPELL_DISPEL_BY_SELF;

        ((int (__cdecl *)(int, char *, char *, char *, uint32_t)) Offsets::SignalEventParam)(
            event,
            format,
            casterGuidStr,
            targetGuidStr,
            spellId);

        FreeGuidString(casterGuidStr);
        FreeGuidString(targetGuidStr);
    }
}
