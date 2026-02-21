//
// Created for autoattack hook
//

#include "autoattack.hpp"
#include "helper.hpp"
#include "offsets.hpp"
#include "logging.hpp"

namespace Nampower {
    void TriggerAutoAttackEvent(uint64_t attackerGuid, uint64_t targetGuid, uint32_t totalDamage,
                                uint32_t hitInfo, uint32_t victimState, uint8_t subDamageCount,
                                uint32_t blockedAmount, uint32_t totalAbsorb, uint32_t totalResist) {
        static char format[] = "%s%s%d%d%d%d%d%d%d";

        auto const activePlayerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto eventToTrigger = game::AUTO_ATTACK_OTHER;
        if (attackerGuid == activePlayerGuid) {
            eventToTrigger = game::AUTO_ATTACK_SELF;
        }

        char *attackerGuidStr = ConvertGuidToString(attackerGuid);
        char *targetGuidStr = ConvertGuidToString(targetGuid);

        reinterpret_cast<int (__cdecl *)(int eventCode,
                                         char *fmt,
                                         char *attackerGuidParam,
                                         char *targetGuidParam,
                                         uint32_t totalDamageParam,
                                         uint32_t hitInfoParam,
                                         uint32_t victimStateParam,
                                         uint32_t subDamageCountParam,
                                         uint32_t blockedAmountParam,
                                         uint32_t totalAbsorbParam,
                                         uint32_t totalResistParam)>(Offsets::SignalEventParam)(
            eventToTrigger,
            format,
            attackerGuidStr,
            targetGuidStr,
            totalDamage,
            hitInfo,
            victimState,
            static_cast<uint32_t>(subDamageCount),
            blockedAmount,
            totalAbsorb,
            totalResist);

        FreeGuidString(attackerGuidStr);
        FreeGuidString(targetGuidStr);
    }

    void AttackRoundInfo_ReadPacketHook(hadesmem::PatchDetourBase *detour, void *thisptr, void *dummy_edx,
                                        CDataStore *param_2) {
        auto const attackRoundInfoReadPacket = detour->GetTrampolineT<AttackRoundInfo_ReadPacketT>();
        if (gUserSettings.enableAutoAttackEvents) {
            // Save the read position to restore it later
            auto const rpos = param_2->m_read;

            // Parse the packet data
            uint32_t hitInfo;
            param_2->Get(hitInfo);

            uint64_t attackerGuid;
            param_2->GetPackedGuid(attackerGuid);

            uint64_t targetGuid;
            param_2->GetPackedGuid(targetGuid);

            uint32_t totalDamage;
            param_2->Get(totalDamage);

            uint8_t subDamageCount;
            param_2->Get(subDamageCount);

            uint32_t totalAbsorb = 0;
            uint32_t totalResist = 0;

            // Read sub damage info
            SubDamageInfo subDamageArray[MAX_ATTACK];
            for (uint8_t i = 0; i < subDamageCount && i < MAX_ATTACK; i++) {
                param_2->Get(subDamageArray[i].damageSchool);
                param_2->Get(subDamageArray[i].coefficient);
                param_2->Get(subDamageArray[i].damage);
                param_2->Get(subDamageArray[i].absorb);
                param_2->Get(subDamageArray[i].resist);

                totalAbsorb += subDamageArray[i].absorb;
                totalResist += subDamageArray[i].resist;
            }

            uint32_t victimState;
            param_2->Get(victimState);

            uint32_t unused1;
            param_2->Get(unused1);

            uint32_t unused2;
            param_2->Get(unused2);

            uint32_t blockedAmount;
            param_2->Get(blockedAmount);

            // Reset read pointer for the original function
            param_2->m_read = rpos;

            // Trigger auto attack event
            TriggerAutoAttackEvent(attackerGuid, targetGuid, totalDamage, hitInfo, victimState,
                                   subDamageCount, blockedAmount, totalAbsorb, totalResist);
        }

        // Call original function
        attackRoundInfoReadPacket(thisptr, dummy_edx, param_2);
    }
}
