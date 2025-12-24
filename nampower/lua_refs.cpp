#include "lua_refs.hpp"
#include "helper.hpp"
#include "logging.hpp"
#include <vector>

namespace Nampower {
    // Vector to store pointers to all registered table refs
    static std::vector<int*> registeredTableRefs;

    void RegisterTableRef(int* tableRefPtr) {
        // Only register if not already in the list
        for (auto ref : registeredTableRefs) {
            if (ref == tableRefPtr) {
                return;
            }
        }
        registeredTableRefs.push_back(tableRefPtr);
    }

    void GetTableRef(uintptr_t* luaState, int& tableRef) {
        // Create table if needed and push onto stack
        // Check for both LUA_REFNIL (-2) and 0 (default array initialization)
        if (tableRef == LUA_REFNIL || tableRef == 0) {
            // Register this table ref if it's a new one
            RegisterTableRef(&tableRef);

            lua_newtable(luaState);
            tableRef = luaL_ref(luaState, LUA_REGISTRYINDEX);
        }
        lua_rawgeti(luaState, LUA_REGISTRYINDEX, tableRef);
    }

    void CleanupAllLuaTableRefs() {
        DEBUG_LOG("Clearing all LUA table references");
        for (auto tableRefPtr : registeredTableRefs) {
            *tableRefPtr = LUA_REFNIL;
        }
        registeredTableRefs.clear();
    }
}
