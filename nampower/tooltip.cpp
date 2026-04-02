#include "tooltip.hpp"

#include "cooldown.hpp"
#include "items.hpp"

#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace Nampower {
    static const std::unordered_map<uint32_t, uint32_t> gKnownSpellIcdSeconds = {
        {12322, 3}, // Unbridled Wrath
        {18137, 3}, // Shadowguard
        {28780, 1}, // The Eye of the Dead
        {44096, 2}, // Ancient Accord Passive
        {48003, 1}, // Burning Hatred
        {51061, 18}, // Uncontained Magic Passive
        {51272, 3}, // Wisdom of the Mak'aru Passive
        {52864, 1}, // Midnight Comet
        {52987, 8}, // Command Whelp Passive
        {52998, 5}, // Decayed Heart Passive
        {58182, 20}, // Parasite of Aln Passive
        {58226, 4}, // Nordrassil's Reprieve Passive
    };
    uint32_t gActiveTooltipItemId = 0;

    static bool ItemHasOnUseSpellId(uint32_t itemId, uint32_t spellId) {
        auto const *itemStats = GetItemStats(itemId);
        if (!itemStats || spellId == 0) {
            return false;
        }

        for (int i = 0; i < 5; ++i) {
            auto const spellCooldownMs = itemStats->m_spellCooldown[i] > 0 ? itemStats->m_spellCooldown[i] : 0;
            auto const spellCategoryCooldownMs = itemStats->m_spellCategoryCooldown[i] > 0
                                                     ? itemStats->m_spellCategoryCooldown[i]
                                                     : 0;
            auto const spellTrigger = itemStats->m_spellTrigger[i];
            if (static_cast<uint32_t>(itemStats->m_spellID[i]) == spellId &&
                spellTrigger != game::ITEM_SPELLTRIGGER_ON_EQUIP &&
                spellTrigger != game::ITEM_SPELLTRIGGER_CHANCE_ON_HIT &&
                (spellCooldownMs > 0 || spellCategoryCooldownMs > 0)) {
                return true;
            }
        }

        return false;
    }

    int CGTooltip_SetItemHook(hadesmem::PatchDetourBase *detour,
                              void *thisptr,
                              void *dummy_edx,
                              uint32_t itemId,
                              int **param_2,
                              int **param_3,
                              int param_4,
                              int param_5,
                              int param_6,
                              int param_7,
                              uint32_t param_8,
                              int ****param_9) {
        auto const cgTooltipSetItem = detour->GetTrampolineT<CGTooltip_SetItemT>();
        gActiveTooltipItemId = itemId;

        auto const result = cgTooltipSetItem(thisptr, dummy_edx, itemId, param_2, param_3, param_4, param_5, param_6,
                                             param_7, param_8, param_9);

        gActiveTooltipItemId = 0;
        return result;
    }

    static uint32_t GetTooltipItemCooldownDurationMs(uint32_t itemId, uint32_t spellId) {
        auto const detail = GetItemSpellCooldownDetail(itemId, spellId);
        auto durationMs = detail.individualDurationMs;
        if (detail.categoryDurationMs > durationMs) {
            durationMs = detail.categoryDurationMs;
        }
        return durationMs;
    }

    static uint32_t GetKnownSpellIcdDurationMs(uint32_t spellId) {
        auto const it = gKnownSpellIcdSeconds.find(spellId);
        if (it == gKnownSpellIcdSeconds.end()) {
            return 0;
        }
        return it->second * 1000;
    }

    static bool AppendTooltipText(char *buffer, size_t bufferSize, const char *text) {
        if (!buffer || !text || bufferSize == 0) {
            return false;
        }

        auto const currentLength = std::strlen(buffer);
        auto const textLength = std::strlen(text);
        auto const maxLength = bufferSize - 1;

        if (currentLength >= maxLength || textLength > (maxLength - currentLength)) {
            return false;
        }

        std::strncat(buffer, text, textLength);
        return true;
    }

    static bool AppendTooltipDurationText(char *buffer,
                                          size_t bufferSize,
                                          const char *label,
                                          uint32_t durationMs) {
        if (durationMs == 0) {
            return false;
        }

        char text[32] = "";
        if (durationMs >= 60000) {
            std::snprintf(text, sizeof(text), " %s: %.1f min", label, durationMs / 60000.0f);
        } else {
            std::snprintf(text, sizeof(text), " %s: %u sec", label, static_cast<uint32_t>((durationMs + 500) / 1000));
        }

        return AppendTooltipText(buffer, bufferSize, text);
    }

    static bool AppendTooltipIcdText(char *buffer, size_t bufferSize, uint32_t durationMs) {
        if (durationMs == 0) {
            return false;
        }

        char text[24] = "";
        if (durationMs >= 60000) {
            std::snprintf(text, sizeof(text), " ICD: %.1fm", durationMs / 60000.0f);
        } else {
            std::snprintf(text, sizeof(text), " ICD: %us", static_cast<uint32_t>((durationMs + 500) / 1000));
        }

        return AppendTooltipText(buffer, bufferSize, text);
    }

    static bool AppendTooltipProcChanceText(char *buffer, size_t bufferSize, uint32_t procChance) {
        if (!buffer || procChance == 0 || procChance >= 100) {
            return false;
        }

        char procChanceText[16] = "";
        int procChanceTextLength = std::snprintf(procChanceText, sizeof(procChanceText), "%u%%", procChance);
        if (procChanceTextLength <= 0 || std::strstr(buffer, procChanceText) != nullptr) {
            return false;
        }

        char chanceText[24] = "";
        std::snprintf(chanceText, sizeof(chanceText), " Chance: %u%%", procChance);
        return AppendTooltipText(buffer, bufferSize, chanceText);
    }

    bool SpellParserParseTextHook(hadesmem::PatchDetourBase *detour,
                                  game::SpellRec *spellRec,
                                  char *buffer,
                                  int bufferSize,
                                  float param_4,
                                  float param_5,
                                  float param_6,
                                  float param_7,
                                  float param_8) {
        auto const spellParserParseText = detour->GetTrampolineT<SpellParserParseTextT>();
        auto const result = spellParserParseText(spellRec, buffer, bufferSize, param_4, param_5, param_6, param_7,
                                                 param_8);

        if (!result || !gUserSettings.enableEnhancedTooltips || !spellRec || !buffer || bufferSize <= 0) {
            return result;
        }

        // Add item cooldown if on use spell
        if (gActiveTooltipItemId != 0 && ItemHasOnUseSpellId(gActiveTooltipItemId, spellRec->Id)) {
            AppendTooltipDurationText(buffer,
                                      static_cast<size_t>(bufferSize),
                                      "CD",
                                      GetTooltipItemCooldownDurationMs(gActiveTooltipItemId, spellRec->Id));
        }

        // Add ICD if available
        AppendTooltipIcdText(buffer, static_cast<size_t>(bufferSize), GetKnownSpellIcdDurationMs(spellRec->Id));

        // Add proc chance from dbc if available
        AppendTooltipProcChanceText(buffer, static_cast<size_t>(bufferSize), spellRec->procChance);

        return result;
    }
}
