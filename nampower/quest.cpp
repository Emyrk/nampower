//
// Quest log and dialog quest ID Lua APIs.
//

#include "quest.hpp"

#include "offsets.hpp"
#include "helper.hpp"

namespace Nampower {
    enum class QuestDialogKind : int32_t {
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

    static uint32_t *GetQuestLogEntryCountPtr() {
        return reinterpret_cast<uint32_t *>(Offsets::QuestLogEntryCount);
    }

    static QuestLogEntryRaw *GetQuestLogEntriesPtr() {
        return reinterpret_cast<QuestLogEntryRaw *>(Offsets::QuestLogEntries);
    }

    static int32_t GetQuestDialogStateCode() {
        return *reinterpret_cast<int32_t *>(Offsets::QuestDialogState);
    }

    static uint32_t GetQuestDialogQuestIdRaw() {
        auto const state = GetQuestDialogStateCode();
        if (state < static_cast<int32_t>(QuestDialogKind::DETAIL) ||
            state > static_cast<int32_t>(QuestDialogKind::COMPLETE)) {
            return 0;
        }

        auto const questId = *reinterpret_cast<uint32_t *>(Offsets::QuestDialogCurrentQuestId);
        return questId > 0 ? questId : 0;
    }

    uint32_t Script_GetQuestLogQuestIds(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        lua_newtable(luaState);

        auto const rowCount = *GetQuestLogEntryCountPtr();
        if (rowCount == 0 || rowCount > kQuestLogMaxRows) {
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
