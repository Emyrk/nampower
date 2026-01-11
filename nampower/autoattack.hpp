//
// Created for autoattack hook
//

#pragma once

#include "game.hpp"
#include <Windows.h>
#include "main.hpp"
#include "cdatastore.hpp"

#define MAX_ATTACK 3

namespace Nampower {
    struct SubDamageInfo {
        uint32_t damageSchool;
        float coefficient;
        uint32_t damage;
        uint32_t absorb;
        int32_t resist;
    };

    void TriggerAutoAttackEvent(uint64_t attackerGuid, uint64_t targetGuid, uint32_t totalDamage,
                                uint32_t hitInfo, uint32_t victimState, uint8_t subDamageCount,
                                uint32_t blockedAmount, uint32_t totalAbsorb, uint32_t totalResist);

    void AttackRoundInfo_ReadPacketHook(hadesmem::PatchDetourBase *detour, void *thisptr, void *dummy_edx, CDataStore *param_2);
}
