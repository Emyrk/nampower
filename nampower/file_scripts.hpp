//
// File read/write Lua script bindings
//

#pragma once

#include "main.hpp"

namespace Nampower {
    uint32_t Script_WriteCustomFile(uintptr_t *luaState);
    uint32_t Script_ReadCustomFile(uintptr_t *luaState);
    uint32_t Script_CustomFileExists(uintptr_t *luaState);
    uint32_t Script_ImportFile(uintptr_t *luaState);
    uint32_t Script_ExportFile(uintptr_t *luaState);
    uint32_t Script_ExecuteCustomLuaFile(uintptr_t *luaState);
    uint32_t Script_EncryptPassword(uintptr_t *luaState);
    uint32_t Script_EncryptedServerLogin(uintptr_t *luaState);
}
