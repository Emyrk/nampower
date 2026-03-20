//
// Created by pmacc on 2/19/2026.
//

#pragma once

#include "main.hpp"

namespace Nampower {
    constexpr const char *UNIT_PLAYER   = "player";
    constexpr const char *UNIT_PET      = "pet";
    constexpr const char *UNIT_TARGET   = "target";
    constexpr const char *UNIT_NPC      = "npc";
    constexpr const char *UNIT_MOUSEOVER = "mouseover";

    constexpr const char *UNIT_PARTY[4] = {
        "party1", "party2", "party3", "party4"
    };
    constexpr const char *UNIT_PARTYPET[4] = {
        "partypet1", "partypet2", "partypet3", "partypet4"
    };
    constexpr const char *UNIT_RAID[40] = {
        "raid1",  "raid2",  "raid3",  "raid4",  "raid5",
        "raid6",  "raid7",  "raid8",  "raid9",  "raid10",
        "raid11", "raid12", "raid13", "raid14", "raid15",
        "raid16", "raid17", "raid18", "raid19", "raid20",
        "raid21", "raid22", "raid23", "raid24", "raid25",
        "raid26", "raid27", "raid28", "raid29", "raid30",
        "raid31", "raid32", "raid33", "raid34", "raid35",
        "raid36", "raid37", "raid38", "raid39", "raid40"
    };
    constexpr const char *UNIT_RAIDPET[40] = {
        "raidpet1",  "raidpet2",  "raidpet3",  "raidpet4",  "raidpet5",
        "raidpet6",  "raidpet7",  "raidpet8",  "raidpet9",  "raidpet10",
        "raidpet11", "raidpet12", "raidpet13", "raidpet14", "raidpet15",
        "raidpet16", "raidpet17", "raidpet18", "raidpet19", "raidpet20",
        "raidpet21", "raidpet22", "raidpet23", "raidpet24", "raidpet25",
        "raidpet26", "raidpet27", "raidpet28", "raidpet29", "raidpet30",
        "raidpet31", "raidpet32", "raidpet33", "raidpet34", "raidpet35",
        "raidpet36", "raidpet37", "raidpet38", "raidpet39", "raidpet40"
    };

    struct UnitNames {
        uint64_t guid = 0;
        bool player = false;
        bool pet = false;
        bool target = false;
        bool npc = false;
        bool mouseover = false;
        int partyIndex = 0;     // 1-4 (party has no 5th slot), 0 = none
        int partyPetIndex = 0;  // 1-4, 0 = none
        int raidIndex = 0;      // 1-40, 0 = none
        int raidPetIndex = 0;   // 1-40, 0 = none

        bool any() const {
            return player || pet || target || npc || mouseover ||
                   partyIndex > 0 || partyPetIndex > 0 ||
                   raidIndex > 0 || raidPetIndex > 0;
        }
    };

    using CGGameUI_GetPartyMemberT    = uint64_t (__fastcall *)(uint32_t index);
    using CGGameUI_GetPartyMemberPetT = uint64_t (__fastcall *)(uint32_t index);
    using CGGameUI_GetRaidMemberT     = uint64_t (__fastcall *)(uint32_t index);
    using CGGameUI_GetRaidMemberPetT  = uint64_t (__fastcall *)(uint32_t index);

    using CGUnit_C_HandleEnvironmentDamageT = void (__fastcall *)(uintptr_t *unit, void *dummy_edx, int dmgType, int damage, int absorb, int resist);
    using UnitCombatLogDamageShieldT = void (__fastcall *)(uint64_t *victimGuid, uint64_t *unitGuid, uint32_t damage, uint32_t spellSchool);

    using CGGameUI_ShowCombatFeedbackT = void (__fastcall *)(uint64_t *guid, const char *action, int damage, int school, int hitInfo);
    using SignalEventParamUnitCombatT  = int (__cdecl *)(uint32_t eventCode, const char *format, const char *unit, const char *action, const char *critIndicator, int damage, int school);
    using SignalEventParamGuidT        = int (__cdecl *)(uint32_t eventCode, const char *format, const char *guid, int isPlayer, int isTarget, int isMouseover, int isPet, int partyIndex, int raidIndex);
    using SignalEventParamUnitCombatGuidT = int (__cdecl *)(uint32_t eventCode, const char *format, const char *guid, const char *action, int damage, int school, int hitInfo, int isPet, int partyIndex, int raidIndex);

    UnitNames GetUnitNames(uint64_t *guid);
    void FireUnitGuidEvent(const UnitNames &names, uint32_t guidEventCode);
    void TriggerUnitEvents(const UnitNames &names, uint32_t eventCode);

    void SendUnitSignalHook(hadesmem::PatchDetourBase *detour, uint64_t *guid, uint32_t eventCode);
    void CGGameUI_ShowCombatFeedbackHook(hadesmem::PatchDetourBase *detour, uint64_t *guid, const char *action, int damage, int school, int hitInfo);
    void CGUnit_C_HandleEnvironmentDamageHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx, int dmgType, int damage, int absorb, int resist);
    void UnitCombatLogDamageShieldHook(hadesmem::PatchDetourBase *detour, uint64_t *targetGuid, uint64_t *unitGuid, uint32_t damage, uint32_t spellSchool);
}
