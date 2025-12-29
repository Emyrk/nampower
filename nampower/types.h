//
// Created by pmacc on 9/25/2024.
//

#pragma once

#include <minwindef.h>
#include <cstdint>

struct UserSettings {
    bool queueCastTimeSpells = true;
    bool queueInstantSpells = true;
    bool queueOnSwingSpells = false;
    bool queueChannelingSpells = true;
    bool queueTargetingSpells = true;
    bool queueSpellsOnCooldown = true;

    bool interruptChannelsOutsideQueueWindow = false;
    bool retryServerRejectedSpells = true;
    bool quickcastTargetingSpells = false;
    bool replaceMatchingNonGcdCategory = false;
    bool optimizeBufferUsingPacketTimings = false;

    bool preventRightClickTargetChange = false;
    bool preventRightClickPvPAttack = false;

    bool doubleCastToEndChannelEarly = false;
    bool quickcastOnDoubleCast = false;

    bool spamProtectionEnabled = true;
    bool enableAuraCastEvents = false;
    bool preventMountingWhenBuffCapped = true;

    uint32_t spellQueueWindowMs = 500;
    uint32_t onSwingBufferCooldownMs = 500;
    uint32_t channelQueueWindowMs = 1500;
    uint32_t targetingQueueWindowMs = 500;
    uint32_t cooldownQueueWindowMs = 250;

    uint32_t minBufferTimeMs = 55;
    uint32_t maxBufferIncreaseMs = 30;
    uint32_t nonGcdBufferTimeMs = 100;

    int32_t channelLatencyReductionPercentage = 75;
};

enum CastType {
    NORMAL,
    NON_GCD,
    ON_SWING,
    CHANNEL,
    TARGETING,
    TARGETING_NON_GCD
};

enum QueueEvents {
    ON_SWING_QUEUED,
    ON_SWING_QUEUE_POPPED,
    NORMAL_QUEUED,
    NORMAL_QUEUE_POPPED,
    NON_GCD_QUEUED,
    NON_GCD_QUEUE_POPPED,
    QUEUE_EVENT_COUNT // Keep track of the number of events
};

enum CastResult {
    WAITING_FOR_CAST,
    WAITING_FOR_SERVER,
    SERVER_SUCCESS,
    SERVER_FAILURE
};

struct CastSpellParams {
    /* Original cast spell function arguments */
    uint64_t castId;

    uint32_t *casterUnit;
    uint32_t spellId;
    game::CGItem_C *item;
    uint64_t guid;
    /* *********************** */

    /* Additional data */
    uint32_t gcdCategory; // comes from spell->StartRecoveryCategory
    uint32_t castTimeMs; // spell's cast time in ms
    uint32_t castStartTimeMs; // event time in ms
    CastType castType;
    uint32_t numRetries;
    CastResult castResult;
};

struct LastCastData {
    uint32_t attemptTimeMs;  // last cast attempt time in ms
    uint32_t attemptSpellId; // last cast attempt spell id

    uint32_t castTimeMs;  // spell's cast time in ms

    uint32_t startTimeMs;  // event time in ms
    uint32_t channelStartTimeMs; // event time in ms
    uint32_t onSwingStartTimeMs; // event time in ms

    bool wasItem;
    bool wasOnGcd;
    bool wasQueued;
};

struct CastData {
    uint32_t delayEndMs; // can't be reset by looking at CastingSpellId
    uint32_t castEndMs;
    uint32_t gcdEndMs;
    uint32_t attemptedCastTimeMs; // this ignoring on swing spells as they are independent

    uint32_t bufferMs;

    uint32_t castSpellId; // spell id for the active cast (when castEndMs is active)

    bool onSwingQueued;
    bool pendingOnSwingCast;
    uint32_t onSwingSpellId;

    uint32_t cooldownNormalEndMs;
    uint32_t cooldownNonGcdEndMs;
    bool cooldownNormalSpellQueued;
    bool cooldownNonGcdSpellQueued;

    bool normalSpellQueued;
    bool nonGcdSpellQueued;
    bool targetingSpellQueued;

    uint32_t targetingSpellId;

    bool castingQueuedSpell;
    uint32_t numRetries;

    bool cancellingSpell;
    bool ignoreCancelDueToCooldown;

    bool channeling;
    bool cancelChannelNextTick;
    uint32_t channelStartMs;
    uint32_t channelEndMs;
    uint32_t channelTickTimeMs;
    uint32_t channelNumTicks;
    uint32_t channelSpellId;
    uint32_t channelDuration;
};

