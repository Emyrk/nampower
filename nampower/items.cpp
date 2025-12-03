//
// Created by pmacc on 9/21/2024.
//

#include "items.hpp"
#include "offsets.hpp"
#include "logging.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace Nampower {
    // Global dictionary to store itemId -> ItemStats_C mappings
    static std::unordered_map<uint32_t, game::ItemStats_C *> itemStatsCache;

    auto const getRow = reinterpret_cast<DBCache_ItemCacheDBGetRowT>(Offsets::DBCache_ItemCacheDBGetRow);
    auto const itemCache = reinterpret_cast<void *>(Offsets::ItemDBCache);

    // Callback function for DBCache
    void __stdcall ItemStatsCallback(uintptr_t *tooltip, char param_2) {
        if (tooltip != nullptr) {
            auto itemId = *reinterpret_cast<uint32_t *>(tooltip + 0xE6);

            int ptr;
            auto itemStats = getRow(itemCache, nullptr, itemId, &ptr, &ItemStatsCallback, nullptr, 0);
            if (itemStats) {
                DEBUG_LOG("Queried itemId " << itemId << " -> " << itemStats);
                // Store in cache
                itemStatsCache[itemId] = reinterpret_cast<game::ItemStats_C *>(itemStats);
            }
        }
    }

    void LoadItem(uint32_t itemId) {
        // Check if already in cache
        auto it = itemStatsCache.find(itemId);
        if (it != itemStatsCache.end()) {
            return;
        }

        if (!itemCache) {
            return;
        }

        int ptr;
        auto itemStats = getRow(itemCache, nullptr, itemId, &ptr, &ItemStatsCallback, nullptr, 0);
        if (itemStats) {
            // Store in cache
            itemStatsCache[itemId] = reinterpret_cast<game::ItemStats_C *>(itemStats);
        } else {
            // wait for .005 sec
            Sleep(5);
        }
    }

    game::ItemStats_C* GetItemStats(uint32_t itemId) {
        auto it = itemStatsCache.find(itemId);
        if (it != itemStatsCache.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::string escapeJsonString(const char *str) {
        if (!str) return "null";

        std::string result = "\"";
        for (const char *p = str; *p; ++p) {
            switch (*p) {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\b':
                    result += "\\b";
                    break;
                case '\f':
                    result += "\\f";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(*p) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(*p));
                        result += buf;
                    } else {
                        result += *p;
                    }
            }
        }
        result += "\"";
        return result;
    }

    // Register a function if you want to use this
    // Don't want everyone scraping all items.
    void ExportAllItems() {
        DEBUG_LOG("Starting getAllItems - fetching items 1 to 100000");

        // Loop through all item IDs
        for (uint32_t itemId = 1; itemId <= 100000; ++itemId) {
            LoadItem(itemId);

            // Log progress every 1000 items
            if (itemId % 1000 == 0) {
                DEBUG_LOG("Progress: " << itemId << " / 100000 items processed");
            }
        }

        DEBUG_LOG("Finished fetching items, found " << itemStatsCache.size() << " valid items");
        DEBUG_LOG("Writing items to items.json");

        // Write all cached items to JSON file (overwrite mode)
        try {
            std::ofstream jsonFile("items.json", std::ios::trunc);
            if (!jsonFile.is_open()) {
                DEBUG_LOG("Failed to open items.json for writing");
                return;
            }

            jsonFile << "[\n";

            bool first = true;
            for (const auto &pair: itemStatsCache) {
                if (!first) {
                    jsonFile << ",\n";
                }
                first = false;

                uint32_t itemId = pair.first;
                game::ItemStats_C *item = pair.second;

                if (!item) continue;

                std::ostringstream json;
                json << "  {\n";
                json << "    \"id\": " << itemId << ",\n";
                json << "    \"class\": " << item->m_class << ",\n";
                json << "    \"subclass\": " << item->m_subclass << ",\n";
                json << "    \"displayName\": " << escapeJsonString(item->m_displayName[0]) << ",\n";
                json << "    \"displayInfoID\": " << item->m_displayInfoID << ",\n";
                json << "    \"quality\": " << item->m_quality << ",\n";
                json << "    \"flags\": " << static_cast<int>(item->m_flags) << ",\n";
                json << "    \"buyPrice\": " << item->m_buyPrice << ",\n";
                json << "    \"sellPrice\": " << item->m_sellPrice << ",\n";
                json << "    \"inventoryType\": " << item->m_inventoryType << ",\n";
                json << "    \"allowableClass\": " << item->m_allowableClass << ",\n";
                json << "    \"allowableRace\": " << item->m_allowableRace << ",\n";
                json << "    \"itemLevel\": " << item->m_itemLevel << ",\n";
                json << "    \"requiredLevel\": " << item->m_requiredLevel << ",\n";
                json << "    \"requiredSkill\": " << item->m_requiredSkill << ",\n";
                json << "    \"requiredSkillRank\": " << item->m_requiredSkillRank << ",\n";
                json << "    \"requiredSpell\": " << item->m_requiredSpell << ",\n";
                json << "    \"requiredHonorRank\": " << item->m_requiredHonorRank << ",\n";
                json << "    \"requiredCityRank\": " << item->m_requiredCityRank << ",\n";
                json << "    \"requiredRep\": " << item->m_requiredRep << ",\n";
                json << "    \"requiredRepRank\": " << item->m_requiredRepRank << ",\n";
                json << "    \"maxCount\": " << item->m_maxCount << ",\n";
                json << "    \"stackable\": " << item->m_stackable << ",\n";
                json << "    \"containerSlots\": " << item->m_containerSlots << ",\n";

                // Arrays
                json << "    \"bonusStat\": [";
                for (int i = 0; i < 10; ++i) {
                    json << item->m_bonusStat[i];
                    if (i < 9) json << ", ";
                }
                json << "],\n";

                json << "    \"bonusAmount\": [";
                for (int i = 0; i < 10; ++i) {
                    json << item->m_bonusAmount[i];
                    if (i < 9) json << ", ";
                }
                json << "],\n";

                json << "    \"minDamage\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_minDamage[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"maxDamage\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_maxDamage[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"damageType\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_damageType[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"resistances\": [";
                for (int i = 0; i < 7; ++i) {
                    json << item->m_resistances[i];
                    if (i < 6) json << ", ";
                }
                json << "],\n";

                json << "    \"delay\": " << item->m_delay << ",\n";
                json << "    \"ammoType\": " << item->m_ammoType << ",\n";
                json << "    \"rangedModRange\": " << item->m_rangedModRange << ",\n";

                json << "    \"spellID\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellID[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellTrigger\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellTrigger[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellCharges\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellCharges[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellCooldown\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellCooldown[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellCategory\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellCategory[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"spellCategoryCooldown\": [";
                for (int i = 0; i < 5; ++i) {
                    json << item->m_spellCategoryCooldown[i];
                    if (i < 4) json << ", ";
                }
                json << "],\n";

                json << "    \"bonding\": " << item->m_bonding << ",\n";
                json << "    \"description\": " << escapeJsonString(item->m_description) << ",\n";
                json << "    \"pageText\": " << item->m_pageText << ",\n";
                json << "    \"languageID\": " << item->m_languageID << ",\n";
                json << "    \"pageMaterial\": " << item->m_pageMaterial << ",\n";
                json << "    \"startQuestID\": " << item->m_startQuestID << ",\n";
                json << "    \"lockID\": " << item->m_lockID << ",\n";
                json << "    \"material\": " << item->m_material << ",\n";
                json << "    \"sheatheType\": " << item->m_sheatheType << ",\n";
                json << "    \"randomProperty\": " << item->m_randomProperty << ",\n";
                json << "    \"block\": " << item->m_block << ",\n";
                json << "    \"itemSet\": " << item->m_itemSet << ",\n";
                json << "    \"maxDurability\": " << item->m_maxDurability << ",\n";
                json << "    \"area\": " << item->m_area << ",\n";
                json << "    \"map\": " << item->m_map << ",\n";
                json << "    \"duration\": " << item->m_duration << ",\n";
                json << "    \"bagFamily\": " << item->m_bagFamily << "\n";
                json << "  }";

                jsonFile << json.str();
            }

            jsonFile << "\n]\n";
            jsonFile.close();

            DEBUG_LOG("Successfully wrote " << itemStatsCache.size() << " items to items.json");
        } catch (const std::exception &e) {
            DEBUG_LOG("Exception writing to items.json: " << e.what());
        }
    }
}
