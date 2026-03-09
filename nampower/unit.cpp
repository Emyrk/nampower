//
// Created by pmacc on 2/19/2026.
//

#include "unit.hpp"
#include "offsets.hpp"
#include "game.hpp"
#include "helper.hpp"
#include "logging.hpp"

namespace Nampower {
    UnitNames GetUnitNames(uint64_t *guid) {
        UnitNames names;

        if (!guid || *guid == 0) return names;

        names.guid = *guid;

        uint64_t playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        if (playerGuid == 0) return names;

        uintptr_t *playerObj = game::GetObjectPtr(playerGuid);
        if (!playerObj) return names;

        // player: compare guid directly against active player guid
        if (*guid == playerGuid) {
            names.player = true;
        }

        // pet: playerObj+0x110 is a pointer to the pet GUID; advance past null entry if present
        // (mirrors: puVar2 = *(uint**)(uVar1+0x110); if (!*puVar2 && !puVar2[1]) puVar2+=2)
        auto *petGuidPtr = *reinterpret_cast<uint64_t **>(reinterpret_cast<uint8_t *>(playerObj) + 0x110);
        if (petGuidPtr) {
            if (*petGuidPtr == 0) petGuidPtr++; // skip null slot (uint64_t++ == uint32_t+=2)
            if (*petGuidPtr != 0 && *guid == *petGuidPtr) {
                names.pet = true;
            }
        }

        // target (locked target)
        uint64_t targetGuid = *reinterpret_cast<uint64_t *>(Offsets::LockedTargetGuid);
        if (*guid == targetGuid) {
            names.target = true;
        }

        // party members (0-4, stored 1-indexed in struct)
        auto const GetPartyMember = reinterpret_cast<CGGameUI_GetPartyMemberT>(Offsets::CGGameUI_GetPartyMember);
        auto const GetPartyMemberPet = reinterpret_cast<CGGameUI_GetPartyMemberPetT>(
            Offsets::CGGameUI_GetPartyMemberPet);

        if (GetPartyMember && GetPartyMemberPet) {
            for (uint32_t i = 0; i < 4; i++) {
                if (*guid == GetPartyMember(i)) {
                    names.partyIndex = static_cast<int>(i) + 1;
                    break;
                }
                if (*guid == GetPartyMemberPet(i)) {
                    names.partyPetIndex = static_cast<int>(i) + 1;
                    break;
                }
            }
        }

        // raid members (0-39, stored 1-indexed in struct)
        auto const GetRaidMember = reinterpret_cast<CGGameUI_GetRaidMemberT>(Offsets::CGGameUI_GetRaidMember);
        auto const GetRaidMemberPet = reinterpret_cast<CGGameUI_GetRaidMemberPetT>(Offsets::CGGameUI_GetRaidMemberPet);

        if (GetRaidMember && GetRaidMemberPet) {
            for (uint32_t i = 0; i < 40; i++) {
                if (*guid == GetRaidMember(i)) {
                    names.raidIndex = static_cast<int>(i) + 1;
                    break;
                }
                if (*guid == GetRaidMemberPet(i)) {
                    names.raidPetIndex = static_cast<int>(i) + 1;
                    break;
                }
            }
        }

        // npc (separate tracked GUID at 0x00B4E2D0)
        uint64_t npcGuid = *reinterpret_cast<uint64_t *>(Offsets::NpcGuidLow);
        if (npcGuid != 0 && *guid == npcGuid) {
            names.npc = true;
        }

        // mouseover
        uint64_t mouseoverGuid = *reinterpret_cast<uint64_t *>(Offsets::MouseoverGuidLow);
        if (mouseoverGuid != 0 && *guid == mouseoverGuid) {
            names.mouseover = true;
        }

        return names;
    }

    // Applies per-category filtering and invokes callback(name) for each passing name.
    // Category mapping mirrors the original string-prefix logic:
    void TriggerUnitEvents(const UnitNames &names, uint32_t eventCode) {
        auto const SignalEventParam = reinterpret_cast<SignalEventParamSingleStringT>(Offsets::SignalEventParam);
        auto *format = reinterpret_cast<char *>(Offsets::StringParamFormat);

        if (names.player)
            SignalEventParam(eventCode, format, UNIT_PLAYER); // always trigger for the current player
        if (names.pet)
            SignalEventParam(eventCode, format, UNIT_PET); // always trigger for your own pet
        if (names.target)
            SignalEventParam(eventCode, format, UNIT_TARGET); // always trigger for your target
        if (names.npc)
            SignalEventParam(eventCode, format, UNIT_NPC); // always trigger for interact npc
        if (names.mouseover && gUserSettings.enableUnitEventsMouseover)
            SignalEventParam(eventCode, format, UNIT_MOUSEOVER);
        if (names.partyIndex > 0 && gUserSettings.enableUnitEventsParty)
            SignalEventParam(eventCode, format, UNIT_PARTY[names.partyIndex - 1]);
        if (names.partyPetIndex > 0 && gUserSettings.enableUnitEventsParty && gUserSettings.enableUnitEventsPet)
            SignalEventParam(eventCode, format, UNIT_PARTYPET[names.partyPetIndex - 1]);
        if (names.raidIndex > 0 && gUserSettings.enableUnitEventsRaid)
            SignalEventParam(eventCode, format, UNIT_RAID[names.raidIndex - 1]);
        if (names.raidPetIndex > 0 && gUserSettings.enableUnitEventsRaid && gUserSettings.enableUnitEventsPet)
            SignalEventParam(eventCode, format, UNIT_RAIDPET[names.raidPetIndex - 1]);
        if (names.guid != 0 && gUserSettings.enableUnitEventsGuid) {
            // when filtering is enabled, suppress high-frequency guid events that cause spam in older addons:
            // skip eventCodes below UNIT_COMBAT(182), and skip UNIT_NAME_UPDATE(183), UNIT_PORTRAIT_UPDATE(184),
            // UNIT_INVENTORY_CHANGED(186), PLAYER_GUILD_UPDATE(345) — these cover UNIT_AURA/UNIT_HEALTH/UNIT_MANA etc.
            bool filtered = gUserSettings.enableUnitEventsGuidFiltering &&
                            (eventCode < 182 || eventCode == 183 || eventCode == 184 ||
                             eventCode == 186 || eventCode == 345);
            if (!filtered) {
                char *guidStr = ConvertGuidToString(names.guid);
                SignalEventParam(eventCode, format, guidStr);
                FreeGuidString(guidStr);
            }
        }

        // trigger the appropriate guid event for curated subset of the most useful unit events
        if (names.guid != 0) {
            uint32_t guidEventCode = 0;
            switch (eventCode) {
                case game::UNIT_PET:
                case game::UNIT_PET_02:
                    guidEventCode = game::UNIT_PET_GUID;
                    break;
                case game::UNIT_HEALTH:
                    guidEventCode = game::UNIT_HEALTH_GUID;
                    break;
                case game::UNIT_MANA:
                case game::UNIT_MANA1:
                case game::UNIT_MANA2:
                    guidEventCode = game::UNIT_MANA_GUID;
                    break;
                case game::UNIT_RAGE:
                    guidEventCode = game::UNIT_RAGE_GUID;
                    break;
                case game::UNIT_ENERGY:
                    guidEventCode = game::UNIT_ENERGY_GUID;
                    break;
                case game::UNIT_FLAGS:
                    guidEventCode = game::UNIT_FLAGS_GUID;
                    break;
                case game::UNIT_AURA:
                case game::UNIT_AURA_02:
                    guidEventCode = game::UNIT_AURA_GUID;
                    break;
                case game::UNIT_DYNAMIC_FLAGS:
                    guidEventCode = game::UNIT_DYNAMIC_FLAGS_GUID;
                    break;
                case game::UNIT_NAME_UPDATE:
                    guidEventCode = game::UNIT_NAME_UPDATE_GUID;
                    break;
                case game::UNIT_PORTRAIT_UPDATE:
                    guidEventCode = game::UNIT_PORTRAIT_UPDATE_GUID;
                    break;
                case game::UNIT_MODEL_CHANGED:
                    guidEventCode = game::UNIT_MODEL_CHANGED_GUID;
                    break;
                case game::UNIT_INVENTORY_CHANGED:
                    guidEventCode = game::UNIT_INVENTORY_CHANGED_GUID;
                    break;
                case game::PLAYER_GUILD_UPDATE:
                    guidEventCode = game::PLAYER_GUILD_UPDATE_GUID;
                    break;
                default: break;
            }

            if (guidEventCode != 0) {
                FireUnitGuidEvent(names, guidEventCode);
            }
        }
    }

    void FireUnitGuidEvent(const UnitNames &names, uint32_t guidEventCode) {
        static constexpr char kGuidFormat[] = "%s%d%d%d%d%d%d";
        auto const SignalGuidEvent = reinterpret_cast<SignalEventParamGuidT>(Offsets::SignalEventParam);
        char *guidStr = ConvertGuidToString(names.guid);
        SignalGuidEvent(guidEventCode, kGuidFormat,
                        guidStr,
                        names.player ? 1 : 0,
                        names.target ? 1 : 0,
                        names.mouseover ? 1 : 0,
                        names.pet ? 1 : 0,
                        names.partyIndex,
                        names.raidIndex);
        FreeGuidString(guidStr);
    }

    void SendUnitSignalHook(hadesmem::PatchDetourBase *detour, uint64_t *guid, uint32_t eventCode) {
        if (!guid || *guid == 0) return;

        auto names = GetUnitNames(guid);
        TriggerUnitEvents(names, eventCode);
    }

    void CGGameUI_ShowCombatFeedbackHook(hadesmem::PatchDetourBase *detour, uint64_t *guid,
                                         const char *action, int damage, int school, int hitInfo) {
        if (!guid || *guid == 0) return;

        auto names = GetUnitNames(guid);

        // derive critIndicator string from hitInfo bitfield (mirrors WoW internal logic)
        auto lo = static_cast<uint8_t>(hitInfo);
        auto hi = static_cast<uint8_t>(hitInfo >> 8);
        const char *crit = "";
        if (damage > 0) {
            if (lo & 0x80) crit = "CRITICAL";
            else if (hi & 0x40) crit = "GLANCING";
            else if (hi & 0x80) crit = "CRUSHING";
        } else {
            if (lo & 0x20) crit = "ABSORB";
            else if (hi & 0x08) crit = "BLOCK";
            else if (lo & 0x40) crit = "RESIST";
        }

        auto const SignalEventParam = reinterpret_cast<SignalEventParamUnitCombatT>(Offsets::SignalEventParam);
        auto *format = reinterpret_cast<const char *>(Offsets::UnitCombatParamFormat);

        if (names.player)
            SignalEventParam(game::UNIT_COMBAT, format, UNIT_PLAYER, action, crit, damage, school);
        if (names.pet && gUserSettings.enableUnitEventsPet)
            SignalEventParam(game::UNIT_COMBAT, format, UNIT_PET, action, crit, damage, school);
        if (names.target)
            SignalEventParam(game::UNIT_COMBAT, format, UNIT_TARGET, action, crit, damage, school);
        if (names.npc)
            SignalEventParam(game::UNIT_COMBAT, format, UNIT_NPC, action, crit, damage, school);
        if (names.mouseover && gUserSettings.enableUnitEventsMouseover)
            SignalEventParam(game::UNIT_COMBAT, format, UNIT_MOUSEOVER, action, crit, damage, school);
        if (names.partyIndex > 0 && gUserSettings.enableUnitEventsParty)
            SignalEventParam(game::UNIT_COMBAT, format, UNIT_PARTY[names.partyIndex - 1], action, crit, damage, school);
        if (names.partyPetIndex > 0 && gUserSettings.enableUnitEventsPet)
            SignalEventParam(game::UNIT_COMBAT, format, UNIT_PARTYPET[names.partyPetIndex - 1], action, crit, damage,
                             school);
        if (names.raidIndex > 0 && gUserSettings.enableUnitEventsRaid)
            SignalEventParam(game::UNIT_COMBAT, format, UNIT_RAID[names.raidIndex - 1], action, crit, damage, school);
        if (names.raidPetIndex > 0 && gUserSettings.enableUnitEventsRaid)
            SignalEventParam(game::UNIT_COMBAT, format, UNIT_RAIDPET[names.raidPetIndex - 1], action, crit, damage,
                             school);

        if (names.guid != 0) {
            static constexpr char kCombatGuidFormat[] = "%s%s%d%d%d%d%d%d";
            auto const SignalCombatGuid = reinterpret_cast<SignalEventParamUnitCombatGuidT>(Offsets::SignalEventParam);
            char *guidStr = ConvertGuidToString(names.guid);
            SignalCombatGuid(game::UNIT_COMBAT_GUID, kCombatGuidFormat,
                             guidStr,
                             action,
                             damage,
                             school,
                             hitInfo,
                             names.pet ? 1 : 0,
                             names.partyIndex,
                             names.raidIndex);
            FreeGuidString(guidStr);
        }
    }

    void CGUnit_C_HandleEnvironmentDamageHook(hadesmem::PatchDetourBase *detour, uintptr_t *unit, void *dummy_edx,
                                              int dmgType, int damage, int absorb, int resist) {
        auto const original = detour->GetTrampolineT<CGUnit_C_HandleEnvironmentDamageT>();
        original(unit, dummy_edx, dmgType, damage, absorb, resist);

        auto unitGuid = game::UnitGetGuid(unit);
        if (unitGuid == 0) return;

        static char format[] = "%s%d%d%d%d";
        char *unitGuidStr = ConvertGuidToString(unitGuid);

        auto event = game::ENVIRONMENTAL_DMG_OTHER;
        if (unitGuid == game::ClntObjMgrGetActivePlayerGuid())
            event = game::ENVIRONMENTAL_DMG_SELF;

        ((int (__cdecl *)(int, char *, char *, int, int, int, int)) Offsets::SignalEventParam)(
            event,
            format,
            unitGuidStr,
            dmgType,
            damage,
            absorb,
            resist);

        FreeGuidString(unitGuidStr);
    }

    void UnitCombatLogDamageShieldHook(hadesmem::PatchDetourBase *detour, uint64_t *unitGuid, uint64_t *targetGuid,
                                        uint32_t damage, uint32_t spellSchool) {
        auto const original = detour->GetTrampolineT<UnitCombatLogDamageShieldT>();
        original(unitGuid, targetGuid, damage, spellSchool);

        if (!unitGuid || *unitGuid == 0) return;

        static char format[] = "%s%s%d%d";
        char *targetGuidString = targetGuid ? ConvertGuidToString(*targetGuid) : nullptr;
        char *unitGuidStr   = ConvertGuidToString(*unitGuid);

        auto event = game::DAMAGE_SHIELD_OTHER;
        if (*unitGuid == game::ClntObjMgrGetActivePlayerGuid())
            event = game::DAMAGE_SHIELD_SELF;

        ((int (__cdecl *)(int, char *, char *, char *, uint32_t, uint32_t)) Offsets::SignalEventParam)(
            event,
            format,
            unitGuidStr,
            targetGuidString,
            damage,
            spellSchool);

        FreeGuidString(unitGuidStr);
        FreeGuidString(targetGuidString);
    }
}
