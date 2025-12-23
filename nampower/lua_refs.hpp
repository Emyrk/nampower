//
// Created for Lua table reference management
//

#pragma once

#include <cstdint>

namespace Nampower {
    // Register a table ref to be cleaned up on shutdown
    void RegisterTableRef(int* tableRefPtr);

    // Get or create a table reference (renamed from EnsureValidTableRef)
    void GetTableRef(uintptr_t* luaState, int& tableRef);

    // Cleanup all registered table refs
    void CleanupAllLuaTableRefs();
}
