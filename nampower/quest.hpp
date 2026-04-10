//
// Quest ID helper APIs for quest log and quest dialog state.
//

#pragma once

#include "main.hpp"

namespace Nampower {
    uint32_t Script_GetQuestLogQuestIds(uintptr_t *luaState);
    uint32_t Script_GetQuestDialogQuestId(uintptr_t *luaState);
}
