//
// Created by pmacc on 1/8/2025.
//

#include "dbc_fields.hpp"
#include "offsets.hpp"

namespace Nampower {
    // Hash tables for O(1) field lookups
    std::unordered_map<std::string, size_t> itemStatsFieldMap;
    std::unordered_map<std::string, size_t> itemStatsArrayFieldMap;
    std::unordered_map<std::string, size_t> spellRecFieldMap;
    std::unordered_map<std::string, size_t> spellRecArrayFieldMap;
    static bool fieldMapsInitialized = false;

    // ItemStats_C field descriptors
    const FieldDescriptor itemStatsFields[] = {
        {"class", offsetof(game::ItemStats_C, m_class), FieldType::INT32},
        {"subclass", offsetof(game::ItemStats_C, m_subclass), FieldType::INT32},
        {"displayInfoID", offsetof(game::ItemStats_C, m_displayInfoID), FieldType::INT32},
        {"quality", offsetof(game::ItemStats_C, m_quality), FieldType::INT32},
        {"flags", offsetof(game::ItemStats_C, m_flags), FieldType::INT32},
        {"buyPrice", offsetof(game::ItemStats_C, m_buyPrice), FieldType::INT32},
        {"sellPrice", offsetof(game::ItemStats_C, m_sellPrice), FieldType::INT32},
        {"inventoryType", offsetof(game::ItemStats_C, m_inventoryType), FieldType::INT32},
        {"allowableClass", offsetof(game::ItemStats_C, m_allowableClass), FieldType::INT32},
        {"allowableRace", offsetof(game::ItemStats_C, m_allowableRace), FieldType::INT32},
        {"itemLevel", offsetof(game::ItemStats_C, m_itemLevel), FieldType::INT32},
        {"requiredLevel", offsetof(game::ItemStats_C, m_requiredLevel), FieldType::INT32},
        {"requiredSkill", offsetof(game::ItemStats_C, m_requiredSkill), FieldType::INT32},
        {"requiredSkillRank", offsetof(game::ItemStats_C, m_requiredSkillRank), FieldType::INT32},
        {"requiredSpell", offsetof(game::ItemStats_C, m_requiredSpell), FieldType::INT32},
        {"requiredHonorRank", offsetof(game::ItemStats_C, m_requiredHonorRank), FieldType::INT32},
        {"requiredCityRank", offsetof(game::ItemStats_C, m_requiredCityRank), FieldType::INT32},
        {"requiredRep", offsetof(game::ItemStats_C, m_requiredRep), FieldType::INT32},
        {"requiredRepRank", offsetof(game::ItemStats_C, m_requiredRepRank), FieldType::INT32},
        {"maxCount", offsetof(game::ItemStats_C, m_maxCount), FieldType::INT32},
        {"stackable", offsetof(game::ItemStats_C, m_stackable), FieldType::INT32},
        {"containerSlots", offsetof(game::ItemStats_C, m_containerSlots), FieldType::INT32},
        {"delay", offsetof(game::ItemStats_C, m_delay), FieldType::INT32},
        {"ammoType", offsetof(game::ItemStats_C, m_ammoType), FieldType::INT32},
        {"rangedModRange", offsetof(game::ItemStats_C, m_rangedModRange), FieldType::INT32},
        {"bonding", offsetof(game::ItemStats_C, m_bonding), FieldType::INT32},
        {"pageText", offsetof(game::ItemStats_C, m_pageText), FieldType::INT32},
        {"languageID", offsetof(game::ItemStats_C, m_languageID), FieldType::INT32},
        {"pageMaterial", offsetof(game::ItemStats_C, m_pageMaterial), FieldType::INT32},
        {"startQuestID", offsetof(game::ItemStats_C, m_startQuestID), FieldType::INT32},
        {"lockID", offsetof(game::ItemStats_C, m_lockID), FieldType::INT32},
        {"material", offsetof(game::ItemStats_C, m_material), FieldType::INT32},
        {"sheatheType", offsetof(game::ItemStats_C, m_sheatheType), FieldType::INT32},
        {"randomProperty", offsetof(game::ItemStats_C, m_randomProperty), FieldType::INT32},
        {"block", offsetof(game::ItemStats_C, m_block), FieldType::INT32},
        {"itemSet", offsetof(game::ItemStats_C, m_itemSet), FieldType::INT32},
        {"maxDurability", offsetof(game::ItemStats_C, m_maxDurability), FieldType::INT32},
        {"area", offsetof(game::ItemStats_C, m_area), FieldType::INT32},
        {"map", offsetof(game::ItemStats_C, m_map), FieldType::INT32},
        {"duration", offsetof(game::ItemStats_C, m_duration), FieldType::INT32},
        {"bagFamily", offsetof(game::ItemStats_C, m_bagFamily), FieldType::INT32},
    };
    const size_t itemStatsFieldsCount = sizeof(itemStatsFields) / sizeof(itemStatsFields[0]);

    const ArrayFieldDescriptor itemStatsArrayFields[] = {
        {"bonusStat", offsetof(game::ItemStats_C, m_bonusStat), FieldType::INT32, 10},
        {"bonusAmount", offsetof(game::ItemStats_C, m_bonusAmount), FieldType::INT32, 10},
        {"minDamage", offsetof(game::ItemStats_C, m_minDamage), FieldType::FLOAT, 5},
        {"maxDamage", offsetof(game::ItemStats_C, m_maxDamage), FieldType::FLOAT, 5},
        {"damageType", offsetof(game::ItemStats_C, m_damageType), FieldType::INT32, 5},
        {"resistances", offsetof(game::ItemStats_C, m_resistances), FieldType::INT32, 7},
        {"spellID", offsetof(game::ItemStats_C, m_spellID), FieldType::INT32, 5},
        {"spellTrigger", offsetof(game::ItemStats_C, m_spellTrigger), FieldType::INT32, 5},
        {"spellCharges", offsetof(game::ItemStats_C, m_spellCharges), FieldType::INT32, 5},
        {"spellCooldown", offsetof(game::ItemStats_C, m_spellCooldown), FieldType::INT32, 5},
        {"spellCategory", offsetof(game::ItemStats_C, m_spellCategory), FieldType::INT32, 5},
        {"spellCategoryCooldown", offsetof(game::ItemStats_C, m_spellCategoryCooldown), FieldType::INT32, 5},
    };
    const size_t itemStatsArrayFieldsCount = sizeof(itemStatsArrayFields) / sizeof(itemStatsArrayFields[0]);

    // SpellRec field descriptors
    const FieldDescriptor spellRecFields[] = {
        {"id", offsetof(game::SpellRec, Id), FieldType::UINT32},
        {"school", offsetof(game::SpellRec, School), FieldType::UINT32},
        {"category", offsetof(game::SpellRec, Category), FieldType::UINT32},
        {"castUI", offsetof(game::SpellRec, castUI), FieldType::UINT32},
        {"dispel", offsetof(game::SpellRec, Dispel), FieldType::UINT32},
        {"mechanic", offsetof(game::SpellRec, Mechanic), FieldType::UINT32},
        {"attributes", offsetof(game::SpellRec, Attributes), FieldType::UINT32},
        {"attributesEx", offsetof(game::SpellRec, AttributesEx), FieldType::UINT32},
        {"attributesEx2", offsetof(game::SpellRec, AttributesEx2), FieldType::UINT32},
        {"attributesEx3", offsetof(game::SpellRec, AttributesEx3), FieldType::UINT32},
        {"attributesEx4", offsetof(game::SpellRec, AttributesEx4), FieldType::UINT32},
        {"stances", offsetof(game::SpellRec, Stances), FieldType::UINT32},
        {"stancesNot", offsetof(game::SpellRec, StancesNot), FieldType::UINT32},
        {"targets", offsetof(game::SpellRec, Targets), FieldType::UINT32},
        {"targetCreatureType", offsetof(game::SpellRec, TargetCreatureType), FieldType::UINT32},
        {"requiresSpellFocus", offsetof(game::SpellRec, RequiresSpellFocus), FieldType::UINT32},
        {"casterAuraState", offsetof(game::SpellRec, CasterAuraState), FieldType::UINT32},
        {"targetAuraState", offsetof(game::SpellRec, TargetAuraState), FieldType::UINT32},
        {"castingTimeIndex", offsetof(game::SpellRec, CastingTimeIndex), FieldType::UINT32},
        {"recoveryTime", offsetof(game::SpellRec, RecoveryTime), FieldType::UINT32},
        {"categoryRecoveryTime", offsetof(game::SpellRec, CategoryRecoveryTime), FieldType::UINT32},
        {"interruptFlags", offsetof(game::SpellRec, InterruptFlags), FieldType::UINT32},
        {"auraInterruptFlags", offsetof(game::SpellRec, AuraInterruptFlags), FieldType::UINT32},
        {"channelInterruptFlags", offsetof(game::SpellRec, ChannelInterruptFlags), FieldType::UINT32},
        {"procFlags", offsetof(game::SpellRec, procFlags), FieldType::UINT32},
        {"procChance", offsetof(game::SpellRec, procChance), FieldType::UINT32},
        {"procCharges", offsetof(game::SpellRec, procCharges), FieldType::UINT32},
        {"maxLevel", offsetof(game::SpellRec, maxLevel), FieldType::UINT32},
        {"baseLevel", offsetof(game::SpellRec, baseLevel), FieldType::UINT32},
        {"spellLevel", offsetof(game::SpellRec, spellLevel), FieldType::UINT32},
        {"durationIndex", offsetof(game::SpellRec, DurationIndex), FieldType::UINT32},
        {"powerType", offsetof(game::SpellRec, powerType), FieldType::UINT32},
        {"manaCost", offsetof(game::SpellRec, manaCost), FieldType::UINT32},
        {"manaCostPerlevel", offsetof(game::SpellRec, manaCostPerlevel), FieldType::UINT32},
        {"manaPerSecond", offsetof(game::SpellRec, manaPerSecond), FieldType::UINT32},
        {"manaPerSecondPerLevel", offsetof(game::SpellRec, manaPerSecondPerLevel), FieldType::UINT32},
        {"rangeIndex", offsetof(game::SpellRec, rangeIndex), FieldType::UINT32},
        {"speed", offsetof(game::SpellRec, speed), FieldType::FLOAT},
        {"modalNextSpell", offsetof(game::SpellRec, modalNextSpell), FieldType::UINT32},
        {"stackAmount", offsetof(game::SpellRec, StackAmount), FieldType::UINT32},
        {"equippedItemClass", offsetof(game::SpellRec, EquippedItemClass), FieldType::INT32},
        {"equippedItemSubClassMask", offsetof(game::SpellRec, EquippedItemSubClassMask), FieldType::INT32},
        {"equippedItemInventoryTypeMask", offsetof(game::SpellRec, EquippedItemInventoryTypeMask), FieldType::INT32},
        {"spellVisual", offsetof(game::SpellRec, SpellVisual), FieldType::UINT32},
        {"spellVisual2", offsetof(game::SpellRec, SpellVisual2), FieldType::UINT32},
        {"spellIconID", offsetof(game::SpellRec, SpellIconID), FieldType::UINT32},
        {"activeIconID", offsetof(game::SpellRec, activeIconID), FieldType::UINT32},
        {"spellPriority", offsetof(game::SpellRec, spellPriority), FieldType::UINT32},
        {"manaCostPercentage", offsetof(game::SpellRec, ManaCostPercentage), FieldType::UINT32},
        {"startRecoveryCategory", offsetof(game::SpellRec, StartRecoveryCategory), FieldType::UINT32},
        {"startRecoveryTime", offsetof(game::SpellRec, StartRecoveryTime), FieldType::UINT32},
        {"maxTargetLevel", offsetof(game::SpellRec, MaxTargetLevel), FieldType::UINT32},
        {"spellFamilyName", offsetof(game::SpellRec, SpellFamilyName), FieldType::UINT32},
        {"spellFamilyFlags", offsetof(game::SpellRec, SpellFamilyFlags), FieldType::UINT64},
        {"maxAffectedTargets", offsetof(game::SpellRec, MaxAffectedTargets), FieldType::UINT32},
        {"dmgClass", offsetof(game::SpellRec, DmgClass), FieldType::UINT32},
        {"preventionType", offsetof(game::SpellRec, PreventionType), FieldType::UINT32},
        {"stanceBarOrder", offsetof(game::SpellRec, StanceBarOrder), FieldType::UINT32},
        {"minFactionId", offsetof(game::SpellRec, MinFactionId), FieldType::UINT32},
        {"minReputation", offsetof(game::SpellRec, MinReputation), FieldType::UINT32},
        {"requiredAuraVision", offsetof(game::SpellRec, RequiredAuraVision), FieldType::UINT32},
    };
    const size_t spellRecFieldsCount = sizeof(spellRecFields) / sizeof(spellRecFields[0]);

    const ArrayFieldDescriptor spellRecArrayFields[] = {
        {"totem", offsetof(game::SpellRec, Totem), FieldType::UINT32, 2},
        {"reagent", offsetof(game::SpellRec, Reagent), FieldType::INT32, 8},
        {"reagentCount", offsetof(game::SpellRec, ReagentCount), FieldType::UINT32, 8},
        {"effect", offsetof(game::SpellRec, Effect), FieldType::UINT32, 3},
        {"effectDieSides", offsetof(game::SpellRec, EffectDieSides), FieldType::INT32, 3},
        {"effectBaseDice", offsetof(game::SpellRec, EffectBaseDice), FieldType::UINT32, 3},
        {"effectDicePerLevel", offsetof(game::SpellRec, EffectDicePerLevel), FieldType::FLOAT, 3},
        {"effectRealPointsPerLevel", offsetof(game::SpellRec, EffectRealPointsPerLevel), FieldType::FLOAT, 3},
        {"effectBasePoints", offsetof(game::SpellRec, EffectBasePoints), FieldType::INT32, 3},
        {"effectMechanic", offsetof(game::SpellRec, EffectMechanic), FieldType::UINT32, 3},
        {"effectImplicitTargetA", offsetof(game::SpellRec, EffectImplicitTargetA), FieldType::UINT32, 3},
        {"effectImplicitTargetB", offsetof(game::SpellRec, EffectImplicitTargetB), FieldType::UINT32, 3},
        {"effectRadiusIndex", offsetof(game::SpellRec, EffectRadiusIndex), FieldType::UINT32, 3},
        {"effectApplyAuraName", offsetof(game::SpellRec, EffectApplyAuraName), FieldType::UINT32, 3},
        {"effectAmplitude", offsetof(game::SpellRec, EffectAmplitude), FieldType::UINT32, 3},
        {"effectMultipleValue", offsetof(game::SpellRec, EffectMultipleValue), FieldType::FLOAT, 3},
        {"effectChainTarget", offsetof(game::SpellRec, EffectChainTarget), FieldType::UINT32, 3},
        {"effectItemType", offsetof(game::SpellRec, EffectItemType), FieldType::UINT32, 3},
        {"effectMiscValue", offsetof(game::SpellRec, EffectMiscValue), FieldType::INT32, 3},
        {"effectTriggerSpell", offsetof(game::SpellRec, EffectTriggerSpell), FieldType::UINT32, 3},
        {"effectPointsPerComboPoint", offsetof(game::SpellRec, EffectPointsPerComboPoint), FieldType::FLOAT, 3},
        {"dmgMultiplier", offsetof(game::SpellRec, DmgMultiplier), FieldType::FLOAT, 3},
    };
    const size_t spellRecArrayFieldsCount = sizeof(spellRecArrayFields) / sizeof(spellRecArrayFields[0]);

    void InitializeFieldMaps() {
        if (fieldMapsInitialized) return;

        // Build hash maps for O(1) lookups
        for (size_t i = 0; i < itemStatsFieldsCount; ++i) {
            itemStatsFieldMap[itemStatsFields[i].name] = i;
        }
        for (size_t i = 0; i < itemStatsArrayFieldsCount; ++i) {
            itemStatsArrayFieldMap[itemStatsArrayFields[i].name] = i;
        }
        for (size_t i = 0; i < spellRecFieldsCount; ++i) {
            spellRecFieldMap[spellRecFields[i].name] = i;
        }
        for (size_t i = 0; i < spellRecArrayFieldsCount; ++i) {
            spellRecArrayFieldMap[spellRecArrayFields[i].name] = i;
        }

        fieldMapsInitialized = true;
    }
}
