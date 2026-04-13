//
// Quest log and dialog quest ID Lua APIs.
//

#include "quest.hpp"

#include "offsets.hpp"
#include "helper.hpp"
#include "lua_refs.hpp"

namespace Nampower {
    enum class QuestDialogKind : uint32_t {
        UNKNOWN = 0,
        DETAIL = 1,
        PROGRESS = 2,
        COMPLETE = 3,
    };

    struct QuestLogEntryRaw {
        int32_t questId;
        int32_t questSortOrHeaderId;
        int32_t headerToken;
        int32_t flags;
    };

    static constexpr uint32_t kQuestLogMaxRows = 256;
    static int questLogQuestIdsTableRef = LUA_REFNIL;
    static uint32_t questLogQuestIdsCount = 0;

    static uint32_t *GetQuestLogEntryCountPtr() {
        return reinterpret_cast<uint32_t *>(Offsets::QuestLogEntryCount);
    }

    static QuestLogEntryRaw *GetQuestLogEntriesPtr() {
        return reinterpret_cast<QuestLogEntryRaw *>(Offsets::QuestLogEntries);
    }

    static QuestDialogKind GetQuestDialogStateCode() {
        auto const raw = *reinterpret_cast<uint32_t *>(Offsets::QuestDialogState);
        if (raw == 0 || raw > static_cast<uint32_t>(QuestDialogKind::COMPLETE)) {
            return QuestDialogKind::UNKNOWN;
        }

        return static_cast<QuestDialogKind>(raw);
    }

    static uint32_t GetQuestDialogQuestIdRaw() {
        auto const state = GetQuestDialogStateCode();
        if (state == QuestDialogKind::UNKNOWN) {
            return 0;
        }

        auto const questId = *reinterpret_cast<uint32_t *>(Offsets::QuestDialogCurrentQuestId);
        return questId > 0 ? questId : 0;
    }

    uint32_t Script_GetQuestLogQuestIds(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        bool useCopy = false;
        if (lua_isnumber(luaState, 1)) {
            useCopy = static_cast<int>(lua_tonumber(luaState, 1)) != 0;
        }

        if (useCopy) {
            lua_newtable(luaState);
        } else {
            GetTableRef(luaState, questLogQuestIdsTableRef);
        }

        auto const rowCount = *GetQuestLogEntryCountPtr();
        if (rowCount == 0 || rowCount > kQuestLogMaxRows) {
            if (!useCopy && questLogQuestIdsCount != 0) {
                for (uint32_t i = 1; i <= questLogQuestIdsCount; ++i) {
                    lua_pushnumber(luaState, static_cast<double>(i));
                    lua_pushnil(luaState);
                    lua_settable(luaState, -3);
                }
                questLogQuestIdsCount = 0;
            }
            return 1;
        }

        auto const *entries = GetQuestLogEntriesPtr();

        int32_t outIndex = 1;
        for (uint32_t i = 0; i < rowCount; ++i) {
            auto const &entry = entries[i];
            if (entry.headerToken != 0 || entry.questId <= 0) {
                continue;
            }

            lua_pushnumber(luaState, static_cast<double>(outIndex));
            lua_pushnumber(luaState, static_cast<double>(entry.questId));
            lua_settable(luaState, -3);
            ++outIndex;
        }

        auto const currentCount = static_cast<uint32_t>(outIndex - 1);
        if (!useCopy) {
            for (uint32_t i = currentCount + 1; i <= questLogQuestIdsCount; ++i) {
                lua_pushnumber(luaState, static_cast<double>(i));
                lua_pushnil(luaState);
                lua_settable(luaState, -3);
            }
            questLogQuestIdsCount = currentCount;
        }

        return 1;
    }

    uint32_t Script_GetQuestDialogQuestId(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto const questId = GetQuestDialogQuestIdRaw();
        if (questId == 0) {
            lua_pushnil(luaState);
        } else {
            lua_pushnumber(luaState, static_cast<double>(questId));
        }

        return 1;
    }
}
