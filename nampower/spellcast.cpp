//
// Created by pmacc on 9/21/2024.
//

#include "spellcast.hpp"
#include "helper.hpp"
#include "offsets.hpp"
#include "logging.hpp"

namespace Nampower {
    auto const APPLY_BUFFER_TO_GCD = false; // gcd issue seems fixed for now

    void SetReleaseAction(uint32_t input) {
        uint32_t activeControl = *reinterpret_cast<uint32_t *>(Offsets::CGInputControlGetActive);

        typedef void (__thiscall *SetReleaseActionT)(uint32_t, uint32_t);
        auto SetReleaseAction = reinterpret_cast<SetReleaseActionT>(Offsets::CGInputControlSetReleaseAction);
        SetReleaseAction(activeControl, input);
    }

    void SetControlBit(uint32_t input) {
        uint32_t activeControl = *reinterpret_cast<uint32_t *>(Offsets::CGInputControlGetActive);
        auto *LastHardwareAction = reinterpret_cast<uintptr_t *>(Offsets::LastHardwareAction);

        typedef void (__thiscall *SetControlBitT)(uint32_t, uint32_t, uint32_t, uintptr_t *, int);
        auto SetControlBit = reinterpret_cast<SetControlBitT>(Offsets::CGInputControlSetControlBit);
        SetControlBit(activeControl, 2, input, LastHardwareAction, 0);
    }

    void CameraOrSelectOrMoveStart() {
        SetReleaseAction(1);
        SetControlBit(1);
    }

    void CameraOrSelectOrMoveStop() {
        SetControlBit(0);
    }

    void EnableSpellTargeting(const game::SpellRec *spell) {
        // set s_needtargets 00cecac0
        auto s_needtargets = reinterpret_cast<uint32_t *>(Offsets::SpellNeedsTargets);
        *s_needtargets = spell->Targets;

        auto const CursorSetCursorMode = reinterpret_cast<void (__fastcall *)(uint32_t)>(Offsets::CursorSetCursorMode);
        CursorSetCursorMode(2); // CAST_CURSOR
        auto const CursorModelSetSequence = reinterpret_cast<void (__fastcall *)(uint32_t)>(
            Offsets::CursorModelSetSequence);
        CursorModelSetSequence(2); // CAST_CURSOR
    }

    uint32_t Spell_C_HandleTerrainClickHook(hadesmem::PatchDetourBase *detour, game::CTerrainClickEvent *event) {
        if (!gCastData.targetingSpellQueued) {
            auto const handleTerrainClick = detour->GetTrampolineT<Spell_C_HandleTerrainClickT>();
            return handleTerrainClick(event);
        }
        return 0;
    }

    void CGWorldFrame_OnLayerTrackTerrainHook(hadesmem::PatchDetourBase *detour, void *thisptr, int dummy_edx,
                                              int param_1) {
        auto originalSpellId = *reinterpret_cast<uint32_t *>(Offsets::VisualSpellId);
        auto originalCasterGuid = *reinterpret_cast<uint64_t *>(Offsets::CasterGuid);

        if (gCastData.targetingSpellQueued) {
            // switch out visual spellid to the queued spell so that we get the correct radius/range
            *reinterpret_cast<uint32_t *>(Offsets::VisualSpellId) = gCastData.targetingSpellId;
            // set casterguid to active player so range checks work correctly
            *reinterpret_cast<uint64_t *>(Offsets::CasterGuid) = game::ClntObjMgrGetActivePlayerGuid();
        }

        auto const onLayerTrackTerrain = detour->GetTrampolineT<CGWorldFrame_OnLayerTrackTerrainT>();
        onLayerTrackTerrain(thisptr, dummy_edx, param_1);

        if (gCastData.targetingSpellQueued) {
            // switch back to originalSpellId
            *reinterpret_cast<uint32_t *>(Offsets::VisualSpellId) = originalSpellId;
            // restore original caster guid
            *reinterpret_cast<uint64_t *>(Offsets::CasterGuid) = originalCasterGuid;
        }
    }

    float Spell_C_GetSpellRadiusHook(hadesmem::PatchDetourBase *detour) {
        auto originalSpellId = *reinterpret_cast<uint32_t *>(Offsets::VisualSpellId);
        if (gCastData.targetingSpellQueued) {
            // switch out visual spellid to the queued spell so that we get the correct radius
            *reinterpret_cast<uint32_t *>(Offsets::VisualSpellId) = gCastData.targetingSpellId;
        }

        auto const getSpellRadius = detour->GetTrampolineT<Spell_C_GetSpellRadiusT>();
        auto radius = getSpellRadius();

        return radius;
    }

    void TriggerQuickcast() {
        // store the current target
        auto const targetGuid = game::GetCurrentTargetGuid();

        CameraOrSelectOrMoveStart();
        CameraOrSelectOrMoveStop();

        // check if target changed
        if (targetGuid != game::GetCurrentTargetGuid()) {
            DEBUG_LOG("Target changed during quick cast, restoring previous target " << targetGuid);
            auto const targetUnit = reinterpret_cast<CGGameUI_TargetT>(Offsets::CGGameUI_Target);
            targetUnit(targetGuid);
        }
    }

    void
    TriggerSpellBeginCastEvent(uint32_t spellId, std::uint64_t guid) {
        char format[] = "%d%s";
        char *guidStr = new char[21]; // 2 for 0x prefix, 18 for the number, and 1 for '\0'
        std::snprintf(guidStr, 21, "0x%016llX", static_cast<unsigned long long>(guid));

        ((int (__cdecl *)(int, char *, uint32_t, char *)) Offsets::SignalEventParam)(
         game::SPELL_BEGIN_CAST_EVENT, // SPELL_BEGIN_CAST_EVENT event we are adding
         format,
         spellId,
         guidStr);

        delete[] guidStr;
    }

    bool Spell_C_TargetSpellHook(hadesmem::PatchDetourBase *detour,
                                 uint32_t *player,
                                 uint32_t *spellId,
                                 uint32_t unk3,
                                 float unk4) {
        auto const spellTarget = detour->GetTrampolineT<Spell_C_TargetSpellT>();
        auto result = spellTarget(player, spellId, unk3, unk4);

        if (!result) {
            auto const spellName = game::GetSpellName(*spellId);
            auto const spell = game::GetSpellInfo(*spellId);

            if (spell->Targets == game::SpellCastTargetFlags::TARGET_FLAG_DEST_LOCATION &&
                spell->Effect[0] != game::SPELL_EFFECT_SUMMON_GUARDIAN) {
                // if quickcast is on instantly trigger all casts
                // otherwise if this is a queued cast, trigger it instant cast
                if (gUserSettings.quickcastTargetingSpells ||
                    (gUserSettings.queueTargetingSpells && gCastData.castingQueuedSpell)) {
                    DEBUG_LOG("Quickcasting terrain spell " << spellName
                        << " quickcast: "
                        << gUserSettings.quickcastTargetingSpells
                        << " queuetrigger: " << gCastData.castingQueuedSpell);
                    TriggerQuickcast();
                }
            }
        }
        return result;
    }

    uint32_t GetChannelBaseDuration(const game::SpellRec *spell) {
        auto const duration = game::GetDurationObject(spell->DurationIndex);
        if (duration == nullptr) {
            DEBUG_LOG("GetChannelBaseDuration: Duration object is null for spell " << game::GetSpellName(spell->Id));
            return 0;
        }
        return duration->m_Duration;
    }

    void BeginCast(uint32_t castTime, const game::SpellRec *spell, const game::SpellCast *cast) {
        if (cast != nullptr && cast->itemTarget == 0 && cast->caster != game::ClntObjMgrGetActivePlayerGuid()) {
            DEBUG_LOG("Ignoring non active player begin cast of spell " << game::GetSpellName(cast->spellId) << " "
                << cast->spellId);
            return;
        }

        auto currentTime = GetTime();

        gLastCastData.castTimeMs = castTime;

        gCastData.channeling = SpellIsChanneling(spell);
        gLastCastData.wasQueued = false; // reset the last spell queued flag

        if (gCastData.channeling) {
            gCastData.channelDuration = GetChannelBaseDuration(spell);
            gCastData.channelEndMs = currentTime + gCastData.channelDuration;
        }

        auto const spellOnGcd = SpellIsOnGcd(spell);
        gLastCastData.wasOnGcd = spellOnGcd;

        auto lastCastParams = gCastHistory.peek();
        uint64_t lastCastId = 0;
        if (lastCastParams != nullptr) {
            gLastCastData.wasItem = lastCastParams->item != nullptr;
            lastCastParams->castResult = CastResult::WAITING_FOR_SERVER;
            lastCastId = lastCastParams->castId;
        }

        auto bufferMs = GetServerDelayMs();
        // Reset the server delay
        gLastServerSpellDelayMs = 0;

        if (spellOnGcd) {
            auto gcdTime = GetGcdOrCooldownForSpell(spell->Id);
            if (gcdTime > 1500) {
                gcdTime = 1500; // items with spells on gcd will return their item gcd, make sure not to use that
            }

            if (!APPLY_BUFFER_TO_GCD) {
                if (castTime < gcdTime - 50) {
                    bufferMs = 0; // no longer need to buffer spells with cast time 50ms < gcd
                } else if (castTime < gcdTime) {
                    auto const diff = gcdTime - castTime;
                    if (bufferMs > diff) {
                        bufferMs -= diff; // subtract the difference from the buffer
                    } else {
                        bufferMs = 0; // if the buffer is less than the difference, set it to 0
                    }
                }
            }

            gCastData.gcdEndMs = currentTime + gcdTime + bufferMs;
            DEBUG_LOG("BeginCast #" << lastCastId
                << " " << game::GetSpellName(spell->Id)
                << "(" << spell->Id << ")"
                << " cast time: " << castTime
                << " buffer: " << bufferMs
                << " Gcd: " << gcdTime
                << " latency: " << GetLatencyMs()
                << " time since last cast " << currentTime - gLastCastData.startTimeMs);
        } else {
            gCastData.delayEndMs = currentTime +
                                   gUserSettings.nonGcdBufferTimeMs;
            // set small "cast time" to avoid attempting next spell too fast
            DEBUG_LOG("BeginCast #" << lastCastId
                << " " << game::GetSpellName(spell->Id)
                << "(" << spell->Id << ")"
                << " cast time: " << castTime
                << " buffer: " << bufferMs
                << " NO Gcd"
                << " latency: " << GetLatencyMs()
                << " time since last cast " << currentTime - gLastCastData.startTimeMs);
        }

        gCastData.castEndMs = castTime ? currentTime + castTime + bufferMs : 0;
        gCastData.castSpellId = castTime ? spell->Id : 0;
        gCastData.bufferMs = bufferMs;

        // check if we can lower buffers
        if (currentTime - gLastBufferDecreaseTimeMs > BUFFER_DECREASE_FREQUENCY) {
            if (gBufferTimeMs > gUserSettings.minBufferTimeMs) {
                gBufferTimeMs -= DYNAMIC_BUFFER_INCREMENT;
                DEBUG_LOG("Decreasing default buffer to " << gBufferTimeMs);
            }

            gLastBufferDecreaseTimeMs = currentTime; // update the last error time to prevent lowering buffer too often
        }

        gLastCastData.startTimeMs = currentTime;

        
        TriggerSpellBeginCastEvent(cast->spellId, cast->unitTarget);
    }

    void CastQueuedNonGcdSpell() {
        if (gCastData.nonGcdSpellQueued) {
            auto nonGcdCastParams = gNonGcdCastQueue.pop();
            if (nonGcdCastParams.spellId > 0) {
                DEBUG_LOG("Triggering queued non gcd cast of " << game::GetSpellName(nonGcdCastParams.spellId));

                gCastData.castingQueuedSpell = true;
                gCastData.targetingSpellQueued = false; // make sure this is off so SendCast is allowed
                gCastData.targetingSpellId = 0;
                gCastData.numRetries = nonGcdCastParams.numRetries;
                Spell_C_CastSpellHook(castSpellDetour, nonGcdCastParams.casterUnit, nonGcdCastParams.spellId,
                                      nonGcdCastParams.item, nonGcdCastParams.guid);
                gLastCastData.wasQueued = true;
            } else {
                DEBUG_LOG("Ignoring queued non gcd cast, no spell id");
                gLastCastData.wasQueued = false;
            }
            TriggerSpellQueuedEvent(NON_GCD_QUEUE_POPPED, nonGcdCastParams.spellId);
            gCastData.nonGcdSpellQueued = !gNonGcdCastQueue.isEmpty();
            gCastData.castingQueuedSpell = false;
            gCastData.targetingSpellQueued = false;
            gCastData.targetingSpellId = 0;
            gCastData.numRetries = 0;
        }
    }

    void CastQueuedNormalSpell() {
        if (gCastData.normalSpellQueued) {
            if (gLastNormalCastParams.spellId > 0) {
                DEBUG_LOG("Triggering queued cast of " << game::GetSpellName(gLastNormalCastParams.spellId));
                gCastData.castingQueuedSpell = true;
                gCastData.targetingSpellQueued = false; // make sure this is off so SendCast is allowed
                gCastData.targetingSpellId = 0;
                gCastData.numRetries = gLastNormalCastParams.numRetries;
                Spell_C_CastSpellHook(castSpellDetour, gLastNormalCastParams.casterUnit, gLastNormalCastParams.spellId,
                                      gLastNormalCastParams.item, gLastNormalCastParams.guid);
                gLastCastData.wasQueued = true;
            } else {
                DEBUG_LOG("Ignoring queued cast, no spell id");
                gLastCastData.wasQueued = false;
            }
            TriggerSpellQueuedEvent(NORMAL_QUEUE_POPPED, gLastNormalCastParams.spellId);
            gCastData.normalSpellQueued = false;
            gCastData.castingQueuedSpell = false;
            gCastData.targetingSpellQueued = false;
            gCastData.targetingSpellId = 0;
            gCastData.numRetries = 0;
        }
    }

    void CastQueuedSpells() {
        if (gCastData.nonGcdSpellQueued) {
            CastQueuedNonGcdSpell();
        } else {
            CastQueuedNormalSpell();
        }
    }

    void SaveCastParams(CastSpellParams *params,
                        uint32_t *casterUnit,
                        uint32_t spellId,
                        game::CGItem_C *item,
                        std::uint64_t guid,
                        uint32_t gcDCategory,
                        uint32_t castTimeMs,
                        uint32_t castStartTimeMs,
                        CastType castType,
                        uint32_t numRetries) {
        params->casterUnit = casterUnit;
        params->spellId = spellId;
        params->item = item;
        params->guid = guid;

        params->gcdCategory = gcDCategory;
        params->castTimeMs = castTimeMs;
        params->castStartTimeMs = castStartTimeMs;
        params->castType = castType;
        params->numRetries = numRetries;
    }

    void TriggerSpellQueuedEvent(QueueEvents queueEventCode, uint32_t spellId) {
        ((int (__cdecl *)(int, char *, uint32_t, uint32_t)) Offsets::SignalEventParam)(
            game::SPELL_QUEUE_EVENT, // SPELL_QUEUE_EVENT event we are adding
            (char *) Offsets::IntIntParamFormat,
            queueEventCode,
            spellId);
    }

    void
    TriggerSpellCastEvent(bool result, uint32_t spellId, CastType castType, std::uint64_t guid, uint32_t itemId) {
        char format[] = "%d%d%d%s%d";
        char *guidStr = new char[21]; // 2 for 0x prefix, 18 for the number, and 1 for '\0'
        std::snprintf(guidStr, 21, "0x%016llX", static_cast<unsigned long long>(guid));

        ((int (__cdecl *)(int, char *, uint32_t, uint32_t, uint32_t, char *, uint32_t)) Offsets::SignalEventParam)(
            game::SPELL_CAST_EVENT, // SPELL_CAST_EVENT event we are adding
            format,
            result,
            spellId,
            castType,
            guidStr,
            itemId);

        delete[] guidStr;
    }

    void clearCastingSpell() {
        // clearing current casting spell id if needed
        // this prevents client from failing to cast spells without a casttime
        // due to not receiving spell result yet
        auto const castingSpellId = reinterpret_cast<uint32_t *>(Offsets::CastingSpellId);
        if (*castingSpellId > 0) {
            *castingSpellId = 0;
        }
    }

    void CGSpellBook_CastSpellHook(hadesmem::PatchDetourBase *detour, uint32_t spellSlot, int bookType,
                                   uint64_t target) {
        auto const cgSpellBookCastSpell = detour->GetTrampolineT<CGSpellBook_CastSpellT>();

        // check for double cast to trigger quickcast on targeting spells
        if (gUserSettings.quickcastOnDoubleCast) {
            auto currentTime = GetTime();

            uint32_t spellId = 0;
            if (bookType == 0) {
                spellId = *reinterpret_cast<uint32_t *>(
                    static_cast<uint32_t>(Offsets::CGSpellBook_mKnownSpells) + spellSlot * 4);
            } else if (bookType == 1) {
                spellId = *reinterpret_cast<uint32_t *>(
                    static_cast<uint32_t>(Offsets::CGSpellBook_mKnownPetSpells) + spellSlot * 4);
            } else {
                // call original
                cgSpellBookCastSpell(spellSlot, bookType, target);
                return;
            }

            if (IsTargetingTerrainSpell() && spellId > 0) {
                if (gCastData.channeling || EffectiveCastEndMs() >= currentTime) {
                    // if we are already casting block action as that will interrupt
                    return;
                }

                // check if aoe spell targeting is active
                if (gLastCastData.attemptSpellId == spellId) {
                    auto spell = game::GetSpellInfo(spellId);
                    auto spellName = game::GetSpellName(spellId);

                    // don't mess with summon guardian
                    if (spell->Effect[0] != game::SPELL_EFFECT_SUMMON_GUARDIAN) {
                        DEBUG_LOG(
                            "CastSpell double cast detected for targeting spell " << spellName <<
                            ", triggering quickcast");
                        TriggerQuickcast();
                        return; // don't cast again
                    }
                }
            }
        }
        cgSpellBookCastSpell(spellSlot, bookType, target);
    }

    void CGActionBar_UseActionHook(hadesmem::PatchDetourBase *detour, uint32_t actionSlot, int param_2, int param_3) {
        if (-1 > actionSlot && actionSlot < 120) {
            uint32_t refValue = 0;

            typedef uint32_t (__fastcall *GetSpellIdFromActionT)(uint32_t actionSlot, uint32_t *refParam);
            auto getSpellIdFromAction = reinterpret_cast<GetSpellIdFromActionT>(Offsets::GetSpellIdFromAction);

            uint32_t spellId = getSpellIdFromAction(actionSlot, &refValue);
            if (spellId > 0) {
                // check for double cast to trigger quickcast on targeting spells
                if (gUserSettings.quickcastOnDoubleCast) {
                    auto currentTime = GetTime();

                    if (IsTargetingTerrainSpell()) {
                        if (gCastData.channeling || EffectiveCastEndMs() >= currentTime) {
                            // if we are already casting block action as that will interrupt
                            return;
                        }

                        // check if aoe spell targeting is active
                        if (gLastCastData.attemptSpellId == spellId) {
                            auto spell = game::GetSpellInfo(spellId);
                            auto spellName = game::GetSpellName(spellId);

                            // don't mess with summon guardian
                            if (spell->Effect[0] != game::SPELL_EFFECT_SUMMON_GUARDIAN) {
                                DEBUG_LOG(
                                    "Use action double cast detected for targeting spell " << spellName <<
                                    ", triggering quickcast");
                                TriggerQuickcast();
                                return; // don't cast again
                            }
                        }
                    }
                }
            }
        }

        auto const useAction = detour->GetTrampolineT<CGActionBar_UseActionT>();
        return useAction(actionSlot, param_2, param_3);
    }

    bool
    Spell_C_CastSpellHook(hadesmem::PatchDetourBase *detour, uint32_t *casterUnit, uint32_t spellId,
                          game::CGItem_C *item,
                          std::uint64_t guid) {
        // save the detour to allow quickly calling this hook
        castSpellDetour = detour;

        // it is possible when spamming to attempt to cast before queues are processed
        processQueues();

        // clear flag to avoid canceling spell cast due to cooldown
        gCastData.ignoreCancelDueToCooldown = false;

        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto playerUnit = (playerGuid > 0) ? game::GetObjectPtr(playerGuid) : nullptr;

        if (item == nullptr && casterUnit != playerUnit) {
            DEBUG_LOG("Ignoring non active player cast of spell " << game::GetSpellName(spellId) << " " << spellId);
            // just call original function if caster is not the active player
            // happens with Doomguard rain of fire
            auto const castSpell = detour->GetTrampolineT<Spell_C_CastSpellT>();
            return castSpell(casterUnit, spellId, item, guid);
        }

        auto const spell = game::GetSpellInfo(spellId);
        auto const spellIsOnSwing = SpellIsOnSwing(spell);
        auto const spellName = game::GetSpellName(spellId);
        auto currentTime = GetTime();
        auto const castTime = game::GetCastTime(casterUnit, spellId);
        gCastData.attemptedCastTimeMs = castTime;

        auto const spellOnGcd = SpellIsOnGcd(spell);
        auto const spellIsChanneling = SpellIsChanneling(spell);
        auto const spellIsTargeting = SpellIsTargeting(spell);
        auto const isSpecialSpell = SpellIsAttackTradeskillOrEnchant(spell);

        if (casterUnit == playerUnit && gUserSettings.preventMountingWhenBuffCapped) {
            // Check for mount spell when player is buff capped
            if (SpellIsMounting(spell)) {
                // check if they are not already mounted
                if (playerUnit && game::UnitIsBuffCapped(playerUnit) && game::UnitGetMountDisplayId(playerUnit) == 0) {
                    DEBUG_LOG("Blocking mount spell " << spellName << " due to buff cap");

                    static char message[] = "Preventing mounting due to buff cap (breaks dismount)";
                    ((int (__cdecl *)(int, char *, char *)) Offsets::SignalEventParam)(
                        game::UI_ERROR_MESSAGE,
                        (char *) Offsets::StringParamFormat,
                        message);

                    return false;
                }
            }
        }

        auto const currentTargetGuid = game::GetCurrentTargetGuid();

        if (spellIsChanneling) {
            auto casterGuid = game::UnitGetGuid(casterUnit);
            if (casterGuid == game::ClntObjMgrGetActivePlayerGuid()) {
                // check that locked target guid matches our unit target guid
                auto unitTargetGuid = game::UnitGetTargetGuid(casterUnit);
                if (unitTargetGuid != currentTargetGuid) {
                    DEBUG_LOG("Updating selection target to " << currentTargetGuid << " from " << unitTargetGuid);
                    SetSelectionTarget(currentTargetGuid);
                }
            }
        }

        // check for double press to interrupt channeling early
        if (gCastData.channeling && !gCastData.cancelChannelNextTick &&
            gCastData.numRetries == 0 &&
            gUserSettings.doubleCastToEndChannelEarly &&
            gCastData.channelStartMs > 0) {
            // wait 500ms after the start of a channel before allowing double cast to end it early
            if (currentTime - gCastData.channelStartMs > 500) {
                // check if same spell is being cast again within 350ms
                if (gLastCastData.attemptSpellId == spellId && currentTime - gLastCastData.attemptTimeMs < 350) {
                    DEBUG_LOG("Double cast detected for " << spellName << ", ending channel early");
                    gCastData.cancelChannelNextTick = true;
                }
            }
        }

        gLastCastData.attemptTimeMs = currentTime;
        gLastCastData.attemptSpellId = spellId;

        uint32_t itemId = 0;
        if (item) {
            itemId = game::GetItemId(reinterpret_cast<game::CGItem_C *>(item));
        }

        DEBUG_LOG("Attempt cast " << spellName << " itemId " << itemId << " on guid " << guid << " target "
            << currentTargetGuid
            << ", time since last cast " << currentTime - gLastCastData.startTimeMs);

        // clear cooldown queue if we are casting a spell
        if (spellOnGcd && gCastData.cooldownNormalSpellQueued) {
            gCastData.cooldownNormalSpellQueued = false;
            TriggerSpellQueuedEvent(NORMAL_QUEUE_POPPED, gLastNormalCastParams.spellId);
        } else if (gCastData.cooldownNonGcdSpellQueued) {
            gCastData.cooldownNonGcdSpellQueued = false;
            // pop the params
            auto nonGcdParams = gNonGcdCastQueue.pop();
            TriggerSpellQueuedEvent(NON_GCD_QUEUE_POPPED, nonGcdParams.spellId);
        }

        // on swing spells are independent of cast bar / gcd, handle them separately
        if (spellIsOnSwing) {
            SaveCastParams(&gLastOnSwingCastParams, casterUnit, spellId, item, guid, spell->StartRecoveryCategory,
                           castTime,
                           currentTime, ON_SWING, 0);

            gCastHistory.pushFront({
                gNextCastId, casterUnit, spellId, item, guid,
                spell->StartRecoveryCategory,
                castTime,
                currentTime,
                ON_SWING,
                gCastData.numRetries,
                CastResult::WAITING_FOR_CAST
            });
            gNextCastId++;

            // try to cast the spell
            auto const castSpell = detour->GetTrampolineT<Spell_C_CastSpellT>();
            auto ret = castSpell(casterUnit, spellId, item, guid);

            TriggerSpellCastEvent(ret, spellId, ON_SWING, guid, itemId);

            if (ret) {
                gCastData.pendingOnSwingCast = true;
                gCastData.onSwingSpellId = spellId;
            }

            if (!ret && gUserSettings.queueOnSwingSpells && !gNoQueueCast) {
                // if not in cooldown window
                if (currentTime - gLastCastData.onSwingStartTimeMs > gUserSettings.onSwingBufferCooldownMs) {
                    DEBUG_LOG("Queuing on swing spell " << spellName);
                    TriggerSpellQueuedEvent(ON_SWING_QUEUED, spellId);
                    gCastData.onSwingQueued = true;
                }
            } else {
                gLastCastData.onSwingStartTimeMs = GetTime();
                DEBUG_LOG("Successful on swing spell " << spellName);
            }

            return ret;
        }

        auto const castSpell = detour->GetTrampolineT<Spell_C_CastSpellT>();

        auto effectiveCastEndMs = EffectiveCastEndMs();
        auto remainingEffectiveCastTime = (effectiveCastEndMs > currentTime) ? effectiveCastEndMs - currentTime : 0;
        auto remainingGcd = (gCastData.gcdEndMs > currentTime) ? gCastData.gcdEndMs - currentTime : 0;

        auto remainingCD = (remainingEffectiveCastTime > remainingGcd) ? remainingEffectiveCastTime : remainingGcd;

        auto inSpellQueueWindow = InSpellQueueWindow(remainingEffectiveCastTime, remainingGcd, spellIsTargeting);

        // don't queue trade skills or enchants
        if (isSpecialSpell) {
            inSpellQueueWindow = false;
        }

        if (spellOnGcd) {
            auto castType = NORMAL;
            if (spellIsTargeting) {
                castType = TARGETING;
            } else if (spellIsChanneling) {
                castType = CHANNEL;
            }

            SaveCastParams(&gLastNormalCastParams, casterUnit, spellId, item, guid,
                           spell->StartRecoveryCategory,
                           castTime,
                           currentTime, castType, 0);
        }

        // skip queueing if gNoQueueCast is set
        // skip queueing if spellIsChanneling and gUserSettings.queueChannelingSpells is false
        if (!gNoQueueCast && (!spellIsChanneling || gUserSettings.queueChannelingSpells)) {
            if (spellIsTargeting) {
                if (gUserSettings.queueTargetingSpells) {
                    if (castTime > 0 && inSpellQueueWindow) {
                        if (gUserSettings.queueCastTimeSpells) {
                            // call EnableSpellTargeting to set s_needTargets and trigger the targeting indicator
                            EnableSpellTargeting(spell);

                            DEBUG_LOG(
                                "Queuing targeting for after cast/gcd: " << remainingCD << "ms " << spellName);
                            TriggerSpellQueuedEvent(NORMAL_QUEUED, spellId);
                            gCastData.normalSpellQueued = true;
                            gCastData.targetingSpellQueued = true;
                            gCastData.targetingSpellId = spellId;
                            return false;
                        }
                    } else if (inSpellQueueWindow) {
                        if (gUserSettings.queueInstantSpells) {
                            if (spellOnGcd) {
                                // call EnableSpellTargeting to set s_needTargets and trigger the targeting indicator
                                EnableSpellTargeting(spell);

                                DEBUG_LOG(
                                    "Queuing instant cast targeting for after cast/gcd: " << remainingCD << "ms "
                                    << spellName);
                                TriggerSpellQueuedEvent(NORMAL_QUEUED, spellId);
                                gCastData.normalSpellQueued = true;
                                gCastData.targetingSpellQueued = true;
                                gCastData.targetingSpellId = spellId;
                                return false;
                            } else if (remainingEffectiveCastTime > 0) {
                                auto castParams = gNonGcdCastQueue.findSpellId(spellId);
                                if (castParams) {
                                    DEBUG_LOG("Updating instant cast non GCD targeting params for " << spellName);
                                    castParams->guid = guid;
                                    return false;
                                } else {
                                    // call EnableSpellTargeting to set s_needTargets and trigger the targeting indicator
                                    EnableSpellTargeting(spell);

                                    DEBUG_LOG("Queuing instant cast non GCD targeting for after cast/gcd: "
                                        << remainingEffectiveCastTime << "ms " << spellName
                                        << " gcd category "
                                        << spell->StartRecoveryCategory);

                                    gNonGcdCastQueue.push({
                                                              0, casterUnit, spellId, item, guid,
                                                              spell->StartRecoveryCategory,
                                                              castTime,
                                                              0,
                                                              ::NON_GCD,
                                                              false
                                                          }, gUserSettings.replaceMatchingNonGcdCategory);
                                    TriggerSpellQueuedEvent(NON_GCD_QUEUED, spellId);
                                    gCastData.nonGcdSpellQueued = true;
                                    gCastData.targetingSpellQueued = true;
                                    gCastData.targetingSpellId = spellId;
                                    return false;
                                }
                            }
                        }
                    }
                }
            } else if (castTime > 0 && inSpellQueueWindow) {
                if (gUserSettings.queueCastTimeSpells) {
                    if (spellOnGcd) {
                        DEBUG_LOG("Queuing for after cast/gcd: " << remainingCD << "ms " << spellName);
                        TriggerSpellQueuedEvent(NORMAL_QUEUED, spellId);
                        gCastData.normalSpellQueued = true;
                        return false;
                    } else if (remainingEffectiveCastTime > 0) {
                        auto castParams = gNonGcdCastQueue.findSpellId(spellId);
                        if (castParams) {
                            DEBUG_LOG("Updating non GCD params for " << spellName);
                            castParams->guid = guid;
                            return false;
                        } else {
                            DEBUG_LOG("Queuing non GCD for after cast/gcd: "
                                << remainingEffectiveCastTime << "ms " << spellName << " gcd category "
                                << spell->StartRecoveryCategory);

                            gNonGcdCastQueue.push({
                                                      0, casterUnit, spellId, item, guid,
                                                      spell->StartRecoveryCategory,
                                                      castTime,
                                                      0,
                                                      ::NON_GCD,
                                                      false
                                                  }, gUserSettings.replaceMatchingNonGcdCategory);
                            TriggerSpellQueuedEvent(NON_GCD_QUEUED, spellId);
                            gCastData.nonGcdSpellQueued = true;
                            return false;
                        }
                    }
                }
            } else if (inSpellQueueWindow) {
                if ((spellIsChanneling && gUserSettings.queueChannelingSpells) ||
                    (!spellIsChanneling && gUserSettings.queueInstantSpells)) {
                    auto desc = "instant cast";
                    if (spellIsChanneling) {
                        desc = "channeling";
                    }

                    if (spellOnGcd) {
                        DEBUG_LOG(
                            "Queuing " << desc << " for after cast/gcd: " << remainingCD << "ms " << spellName);
                        TriggerSpellQueuedEvent(NORMAL_QUEUED, spellId);
                        gCastData.normalSpellQueued = true;
                        return false;
                    } else if (remainingEffectiveCastTime > 0) {
                        auto castParams = gNonGcdCastQueue.findSpellId(spellId);
                        if (castParams) {
                            DEBUG_LOG("Updating " << desc << " non GCD params for " << spellName);
                            castParams->guid = guid;
                            return false;
                        } else {
                            DEBUG_LOG("Queuing " << desc << " non GCD for after cast/gcd: "
                                << remainingEffectiveCastTime << "ms " << spellName << " gcd category "
                                << spell->StartRecoveryCategory);

                            gNonGcdCastQueue.push({
                                                      0, casterUnit, spellId, item, guid,
                                                      spell->StartRecoveryCategory,
                                                      castTime,
                                                      0,
                                                      ::NON_GCD,
                                                      false
                                                  }, gUserSettings.replaceMatchingNonGcdCategory);
                            TriggerSpellQueuedEvent(NON_GCD_QUEUED, spellId);
                            gCastData.nonGcdSpellQueued = true;
                            return false;
                        }
                    }
                }
            }
        }

        if (!isSpecialSpell) {
            // is there a cast? (ignore for on swing spells)
            if (remainingEffectiveCastTime) {
                DEBUG_LOG("Cast or delay active " << remainingEffectiveCastTime << "ms remaining");
                return false;
            } else {
                gCastData.castEndMs = 0;
                gCastData.castSpellId = 0;
            }

            // is there a Gcd?
            if (spellOnGcd && remainingGcd) {
                DEBUG_LOG("Gcd active " << remainingGcd << "ms remaining");
                return false;
            } else {
                gCastData.gcdEndMs = 0;
            }
        }

        // prevent casting instant cast spells and spells with SPELL_ATTR_DISABLED_WHILE_ACTIVE
        // if cast in the last second and still waiting for server result or succeeded
        // otherwise can break cooldown in the client and cause unnecessary errors
        if (castTime == 0 || spell->Attributes & game::SPELL_ATTR_DISABLED_WHILE_ACTIVE) {
            // check if spam protection is enabled
            if (gUserSettings.spamProtectionEnabled) {
                auto castParams = gCastHistory.findNewestWaitingForServerSpellId(spellId);
                if (castParams &&
                    currentTime - castParams->castStartTimeMs < 500) {
                    DEBUG_LOG("Ignoring " << spellName
                        << " cast still waiting for server result for the same spell");
                    return false;
                } else {
                    castParams = gCastHistory.findNewestSuccessfulSpellId(spellId);
                    if (castParams &&
                        castParams->guid == guid &&
                        currentTime - castParams->castStartTimeMs < 500) {
                        DEBUG_LOG("Ignoring " << spellName
                            << " cast recently succeeded for the same spell and target");
                        return false;
                    }
                }
            }
        }

        // try clearing current casting spell id if
        // not using tradeskill or enchant
        // no on swing spell queued (will interrupt them)
        if (!isSpecialSpell && !gCastData.pendingOnSwingCast) {
            clearCastingSpell();
        }

        // add to cast history
        auto castType = CastType::NORMAL;
        if (spellIsChanneling) {
            castType = CastType::CHANNEL;
        } else if (spellIsTargeting) {
            if (spellOnGcd) {
                castType = CastType::TARGETING;
            } else {
                castType = CastType::TARGETING_NON_GCD;
            }
        } else if (!spellOnGcd) {
            castType = CastType::NON_GCD;
        }

        gCastHistory.pushFront({
            gNextCastId, casterUnit, spellId, item, guid,
            spell->StartRecoveryCategory,
            castTime,
            currentTime,
            castType,
            gCastData.numRetries,
            CastResult::WAITING_FOR_CAST
        });
        gNextCastId++;

        auto ret = castSpell(casterUnit, spellId, item, guid);

        // if this is a trade skill or item enchant, do nothing further
        if (isSpecialSpell) {
            TriggerSpellCastEvent(ret, spellId, castType, guid, itemId);
            return ret;
        }

        // haven't gotten spell result from the previous cast yet, probably due to latency.
        // simulate a cancel to clear the cast bar but only when there should be a cast time
        // mining/herbing have cast time but aren't on Gcd, don't cancel them
        if (!ret && gLastCastData.castTimeMs > 0 && gLastCastData.wasOnGcd && !gCastData.ignoreCancelDueToCooldown) {
            if (*reinterpret_cast<int *>(Offsets::SpellIsTargeting) == 0 && !gCastData.pendingOnSwingCast &&
                !IsSpellOnCooldown(spellId)) {
                DEBUG_LOG("Canceling spell cast due to previous spell having cast time of "
                    << gLastCastData.castTimeMs);

                //JT: Suggest replacing CancelSpell with InterruptSpell (the API called when moving during casting).
                // The address of InterruptSpell needs to be dug out. It could possibly fix the sometimes broken animations.
                gCastData.cancellingSpell = true;

                auto const cancelSpell = reinterpret_cast<CancelSpellT>(Offsets::CancelSpell);
                cancelSpell(false, false, game::SPELL_FAILED_ERROR);

                gCastData.cancellingSpell = false;

                clearCastingSpell();

                // try again now that cast bar is gone
                ret = castSpell(casterUnit, spellId, item, guid);

                auto const cursorMode = *reinterpret_cast<int *>(Offsets::CursorMode);
                if (!ret && !(spell->Attributes & game::SPELL_ATTR_RANGED) && cursorMode != 2) {
                    DEBUG_LOG("Retry cast after cancel still failed");
                }
            } else {
                DEBUG_LOG("Initial cast failed but not canceling spell cast");
            }
        }

        TriggerSpellCastEvent(ret, spellId, castType, guid, itemId);

        return ret;
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

    void TriggerAuraCastEvent(const game::SpellRec *spell,
                              uint64_t casterGuid,
                              uint64_t targetGuid,
                              uint64_t activePlayerGuid,
                              bool castByActivePlayer) {
        auto eventToTrigger = game::AURA_CAST_ON_OTHER;
        if (targetGuid == activePlayerGuid) {
            eventToTrigger = game::AURA_CAST_ON_SELF;
        }

        // ignore modifiers if not cast by active player
        auto duration = game::GetSpellDuration(spell, !castByActivePlayer);

        auto *targetUnit = game::ClntObjMgrObjectPtr(
            static_cast<game::TypeMask>(game::TYPEMASK_PLAYER | game::TYPEMASK_UNIT), targetGuid);

        bool targetIsBuffCapped = game::UnitIsBuffCapped(targetUnit);
        bool targetIsDebuffCapped = game::UnitIsDebuffCapped(targetUnit);

        char *casterGuidStr = ConvertGuidToString(casterGuid);
        char *targetGuidStr = ConvertGuidToString(targetGuid);
        char format[] = "%d%s%s%d%d%d%d%d%d";

        for (int i = 0; i < 3; ++i) {
            auto const effectType = spell->Effect[i];
            switch (effectType) {
                case game::SPELL_EFFECT_APPLY_AURA:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_RAID:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
                case game::SPELL_EFFECT_APPLY_AREA_AURA_PET: {
                    auto auraName = spell->EffectApplyAuraName[i];
                    auto effectAmplitude = spell->EffectAmplitude[i];
                    auto effectMiscValue = spell->EffectMiscValue[i];

                    auto auraCapStatus = static_cast<uint32_t>(targetIsBuffCapped) |
                                         (static_cast<uint32_t>(targetIsDebuffCapped) << 1);

                    ((int (__cdecl *)(int eventCode,
                                      char *fmt,
                                      uint32_t spellIdParam,
                                      char *casterGuidStrParam,
                                      char *targetGuidStrParam,
                                      uint32_t effectParam,
                                      uint32_t auraNameParam,
                                      uint32_t effectAmplitudeParam,
                                      uint32_t effectMiscValueParam,
                                      uint32_t durationParam,
                                      uint32_t auraCapStatusParam)) Offsets::SignalEventParam)(
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
                    break;
                }
                default:
                    break;
            }
        }

        delete[] casterGuidStr;
        delete[] targetGuidStr;
    }

    void
    SpellGoHook(hadesmem::PatchDetourBase *detour, uint64_t *casterGUID, uint64_t *targetGUID,
                uint32_t spellId,
                CDataStore *spellData) {
        auto const spellGo = detour->GetTrampolineT<SpellGoT>();
        spellGo(casterGUID, targetGUID, spellId, spellData);

        auto const activePlayerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto const castByActivePlayer = activePlayerGuid == *casterGUID;
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
            auto targetGuidVal = targetGUID ? *targetGUID : *casterGUID;

            TriggerAuraCastEvent(spell, *casterGUID, targetGuidVal, activePlayerGuid, castByActivePlayer);
        }
    }

    void
    CancelSpellHook(hadesmem::PatchDetourBase *detour, bool failed, bool notifyServer,
                    game::SpellCastResult reason) {
        // avoid canceling spell cast if it failed due to cooldown
        if (gCastData.ignoreCancelDueToCooldown &&
            (reason == game::SpellCastResult::SPELL_FAILED_NOT_READY ||
             reason == game::SpellCastResult::SPELL_FAILED_ITEM_NOT_READY)) {
            DEBUG_LOG("Ignoring cancel spell cast due to cooldown, reason:" << int(reason));
            return;
        }

        // triggered by us, reset the cast bar
        if (notifyServer) {
            ResetCastFlags();
        } else if (failed) {
            DEBUG_LOG("Cancel spell cast failed:" << failed <<
                " notifyServer:" << notifyServer << " reason:" << int(reason));
        }

        auto const cancelSpell = detour->GetTrampolineT<CancelSpellT>();
        return cancelSpell(failed, notifyServer, reason);
    }

    void SendCastHook(hadesmem::PatchDetourBase *detour, game::SpellCast *cast, char unk) {

        auto const sendCast = detour->GetTrampolineT<SendCastT>();
        sendCast(cast, unk);

        auto const spell = game::GetSpellInfo(cast->spellId);
        BeginCast(gCastData.attemptedCastTimeMs, spell, cast);
    }
}
