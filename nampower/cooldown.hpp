//
// Cooldown-related types and functions
//

#pragma once

#include <cstdint>

namespace Nampower {
    struct CooldownDetail {
        bool isOnCooldown = false;
        uint32_t cooldownRemainingMs = 0;

        bool itemHasActiveSpell = false;
        uint32_t itemId = 0;
        uint32_t itemActiveSpellId = 0;

        uint32_t individualStartMs = 0;
        uint32_t individualDurationMs = 0;
        uint32_t individualRemainingMs = 0;
        bool isOnIndividualCooldown = false;

        uint32_t categoryId = 0;
        uint32_t categoryStartMs = 0;
        uint32_t categoryDurationMs = 0;
        uint32_t categoryRemainingMs = 0;
        bool isOnCategoryCooldown = false;

        uint32_t gcdCategoryId = 0;
        uint32_t gcdCategoryStartMs = 0;
        uint32_t gcdCategoryDurationMs = 0;
        uint32_t gcdCategoryRemainingMs = 0;
        bool isOnGcdCategoryCooldown = false;
    };

    CooldownDetail GetItemSpellCooldownDetail(uint32_t itemId, uint32_t spellId);
    CooldownDetail GetItemCooldownDetail(uint32_t itemId);
}
