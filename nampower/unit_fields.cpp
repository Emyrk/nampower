//
// Created by pmacc on 1/8/2025.
//

#include "unit_fields.hpp"
#include "game.hpp"

namespace Nampower {
    // Hash tables for O(1) field lookups
    std::unordered_map<std::string, size_t> unitFieldsFieldMap;
    std::unordered_map<std::string, size_t> unitFieldsArrayFieldMap;
    static bool unitFieldMapsInitialized = false;

    // UnitFields field descriptors
    const FieldDescriptor unitFieldsFields[] = {
        {"charm", offsetof(game::UnitFields, charm), FieldType::UINT64},
        {"summon", offsetof(game::UnitFields, summon), FieldType::UINT64},
        {"charmedBy", offsetof(game::UnitFields, charmedBy), FieldType::UINT64},
        {"summonedBy", offsetof(game::UnitFields, summonedBy), FieldType::UINT64},
        {"createdBy", offsetof(game::UnitFields, createdBy), FieldType::UINT64},
        {"target", offsetof(game::UnitFields, target), FieldType::UINT64},
        {"persuaded", offsetof(game::UnitFields, persuaded), FieldType::UINT64},
        {"channelObject", offsetof(game::UnitFields, channelObject), FieldType::UINT64},
        {"health", offsetof(game::UnitFields, health), FieldType::UINT32},
        {"power1", offsetof(game::UnitFields, power1), FieldType::UINT32},
        {"power2", offsetof(game::UnitFields, power2), FieldType::UINT32},
        {"power3", offsetof(game::UnitFields, power3), FieldType::UINT32},
        {"power4", offsetof(game::UnitFields, power4), FieldType::UINT32},
        {"power5", offsetof(game::UnitFields, power5), FieldType::UINT32},
        {"maxHealth", offsetof(game::UnitFields, maxHealth), FieldType::UINT32},
        {"maxPower1", offsetof(game::UnitFields, maxPower1), FieldType::UINT32},
        {"maxPower2", offsetof(game::UnitFields, maxPower2), FieldType::UINT32},
        {"maxPower3", offsetof(game::UnitFields, maxPower3), FieldType::UINT32},
        {"maxPower4", offsetof(game::UnitFields, maxPower4), FieldType::UINT32},
        {"maxPower5", offsetof(game::UnitFields, maxPower5), FieldType::UINT32},
        {"level", offsetof(game::UnitFields, level), FieldType::UINT32},
        {"factionTemplate", offsetof(game::UnitFields, factionTemplate), FieldType::UINT32},
        {"bytes0", offsetof(game::UnitFields, bytes0), FieldType::UINT32},
        {"flags", offsetof(game::UnitFields, flags), FieldType::UINT32},
        {"auraState", offsetof(game::UnitFields, auraState), FieldType::UINT32},
        {"baseAttackTime", offsetof(game::UnitFields, baseAttackTime), FieldType::UINT32},
        {"offhandAttackTime", offsetof(game::UnitFields, offhandAttackTime), FieldType::UINT32},
        {"rangedAttackTime", offsetof(game::UnitFields, rangedAttackTime), FieldType::UINT32},
        {"boundingRadius", offsetof(game::UnitFields, boundingRadius), FieldType::FLOAT},
        {"combatReach", offsetof(game::UnitFields, combatReach), FieldType::FLOAT},
        {"displayId", offsetof(game::UnitFields, displayId), FieldType::UINT32},
        {"nativeDisplayId", offsetof(game::UnitFields, nativeDisplayId), FieldType::UINT32},
        {"mountDisplayId", offsetof(game::UnitFields, mountDisplayId), FieldType::UINT32},
        {"minDamage", offsetof(game::UnitFields, minDamage), FieldType::FLOAT},
        {"maxDamage", offsetof(game::UnitFields, maxDamage), FieldType::FLOAT},
        {"minOffhandDamage", offsetof(game::UnitFields, minOffhandDamage), FieldType::FLOAT},
        {"maxOffhandDamage", offsetof(game::UnitFields, maxOffhandDamage), FieldType::FLOAT},
        {"bytes1", offsetof(game::UnitFields, bytes1), FieldType::UINT32},
        {"petNumber", offsetof(game::UnitFields, petNumber), FieldType::UINT32},
        {"petNameTimestamp", offsetof(game::UnitFields, petNameTimestamp), FieldType::UINT32},
        {"petExperience", offsetof(game::UnitFields, petExperience), FieldType::UINT32},
        {"petNextLevelExp", offsetof(game::UnitFields, petNextLevelExp), FieldType::UINT32},
        {"dynamicFlags", offsetof(game::UnitFields, dynamicFlags), FieldType::UINT32},
        {"channelSpell", offsetof(game::UnitFields, channelSpell), FieldType::UINT32},
        {"modCastSpeed", offsetof(game::UnitFields, modCastSpeed), FieldType::FLOAT},
        {"createdBySpell", offsetof(game::UnitFields, createdBySpell), FieldType::UINT32},
        {"npcFlags", offsetof(game::UnitFields, npcFlags), FieldType::UINT32},
        {"npcEmoteState", offsetof(game::UnitFields, npcEmoteState), FieldType::UINT32},
        {"trainingPoints", offsetof(game::UnitFields, trainingPoints), FieldType::UINT32},
        {"stat0", offsetof(game::UnitFields, stat0), FieldType::UINT32},
        {"stat1", offsetof(game::UnitFields, stat1), FieldType::UINT32},
        {"stat2", offsetof(game::UnitFields, stat2), FieldType::UINT32},
        {"stat3", offsetof(game::UnitFields, stat3), FieldType::UINT32},
        {"stat4", offsetof(game::UnitFields, stat4), FieldType::UINT32},
        {"baseMana", offsetof(game::UnitFields, baseMana), FieldType::UINT32},
        {"baseHealth", offsetof(game::UnitFields, baseHealth), FieldType::UINT32},
        {"bytes2", offsetof(game::UnitFields, bytes2), FieldType::UINT32},
        {"attackPower", offsetof(game::UnitFields, attackPower), FieldType::UINT32},
        {"attackPowerMods", offsetof(game::UnitFields, attackPowerMods), FieldType::UINT32},
        {"attackPowerMultiplier", offsetof(game::UnitFields, attackPowerMultiplier), FieldType::FLOAT},
        {"rangedAttackPower", offsetof(game::UnitFields, rangedAttackPower), FieldType::UINT32},
        {"rangedAttackPowerMods", offsetof(game::UnitFields, rangedAttackPowerMods), FieldType::UINT32},
        {"rangedAttackPowerMultiplier", offsetof(game::UnitFields, rangedAttackPowerMultiplier), FieldType::FLOAT},
        {"minRangedDamage", offsetof(game::UnitFields, minRangedDamage), FieldType::FLOAT},
        {"maxRangedDamage", offsetof(game::UnitFields, maxRangedDamage), FieldType::FLOAT},
    };
    const size_t unitFieldsFieldsCount = sizeof(unitFieldsFields) / sizeof(unitFieldsFields[0]);

    const ArrayFieldDescriptor unitFieldsArrayFields[] = {
        {"virtualItemDisplay", offsetof(game::UnitFields, virtualItemDisplay), FieldType::UINT32, 3},
        {"virtualItemInfo", offsetof(game::UnitFields, virtualItemInfo), FieldType::UINT32, 6},
        {"aura", offsetof(game::UnitFields, aura), FieldType::UINT32, 48},
        {"auraFlags", offsetof(game::UnitFields, auraFlags), FieldType::UINT32, 6},
        {"auraLevels", offsetof(game::UnitFields, auraLevels), FieldType::UINT8, 48},
        {"auraApplications", offsetof(game::UnitFields, auraApplications), FieldType::UINT8, 48},
        {"resistances", offsetof(game::UnitFields, resistances), FieldType::UINT32, 7},
        {"powerCostModifier", offsetof(game::UnitFields, powerCostModifier), FieldType::FLOAT, 7},
        {"powerCostMultiplier", offsetof(game::UnitFields, powerCostMultiplier), FieldType::FLOAT, 7},
    };
    const size_t unitFieldsArrayFieldsCount = sizeof(unitFieldsArrayFields) / sizeof(unitFieldsArrayFields[0]);

    void InitializeUnitFieldMaps() {
        if (unitFieldMapsInitialized) return;

        // Build hash maps for O(1) lookups
        for (size_t i = 0; i < unitFieldsFieldsCount; ++i) {
            unitFieldsFieldMap[unitFieldsFields[i].name] = i;
        }
        for (size_t i = 0; i < unitFieldsArrayFieldsCount; ++i) {
            unitFieldsArrayFieldMap[unitFieldsArrayFields[i].name] = i;
        }

        unitFieldMapsInitialized = true;
    }
}
