#pragma once

#include "main.hpp"

namespace Nampower {
    void CGUnit_C_AddChatBubbleHook(hadesmem::PatchDetourBase *detour,
                                    uintptr_t *unitPtr,
                                    void *dummy_edx,
                                    game::CHAT_COMMAND_ID chatType,
                                    char *msg);

    void ApplyOnChatMessagePatch();
}

