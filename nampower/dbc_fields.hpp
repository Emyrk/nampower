//
// Created by pmacc on 1/8/2025.
//

#pragma once

#include "main.hpp"
#include "lua_refs.hpp"
#include <unordered_map>
#include <string>
#include <cstddef>

namespace Nampower {
    // Field type enumeration
    enum class FieldType {
        INT32,
        UINT32,
        FLOAT,
        STRING,
        UINT64,
        UINT8,
        UINT16,
        INLINE_STRING  // inline char array (not a pointer)
    };

    // Field descriptor for simple fields
    struct FieldDescriptor {
        const char* name;
        size_t offset;
        FieldType type;
    };

    // Field descriptor for array fields
    struct ArrayFieldDescriptor {
        const char* name;
        size_t offset;
        FieldType type;
        size_t count;
    };

    // ItemStats_C field descriptors
    extern const FieldDescriptor itemStatsFields[];
    extern const size_t itemStatsFieldsCount;
    extern const ArrayFieldDescriptor itemStatsArrayFields[];
    extern const size_t itemStatsArrayFieldsCount;

    // SpellRec field descriptors
    extern const FieldDescriptor spellRecFields[];
    extern const size_t spellRecFieldsCount;
    extern const ArrayFieldDescriptor spellRecArrayFields[];
    extern const size_t spellRecArrayFieldsCount;

    // Hash tables for O(1) field lookups
    extern std::unordered_map<std::string, size_t> itemStatsFieldMap;
    extern std::unordered_map<std::string, size_t> itemStatsArrayFieldMap;
    extern std::unordered_map<std::string, size_t> spellRecFieldMap;
    extern std::unordered_map<std::string, size_t> spellRecArrayFieldMap;

    // Initialize field maps (call once before using field lookups)
    void InitializeFieldMaps();

    // Forward declarations for Lua functions (defined in offsets.hpp/misc_scripts.cpp/spell_scripts.cpp)
    using lua_pushnumberT = void (__fastcall *)(uintptr_t *, double);
    using lua_pushstringT = void (__fastcall *)(uintptr_t *, char *);
    using lua_newtableT = void (__fastcall *)(uintptr_t *);
    using lua_settableT = void (__fastcall *)(uintptr_t *, int);

    extern lua_pushnumberT lua_pushnumber;
    extern lua_pushstringT lua_pushstring;
    extern lua_newtableT lua_newtable;
    extern lua_settableT lua_settable;

    // Helper template functions to push fields to Lua (inline for performance)
    template<typename T>
    inline void PushFieldsToLua(uintptr_t* luaState, const T* obj, const FieldDescriptor* fields, size_t fieldCount) {
        for (size_t i = 0; i < fieldCount; ++i) {
            const auto& field = fields[i];
            lua_pushstring(luaState, const_cast<char*>(field.name));

            const char* fieldPtr = reinterpret_cast<const char*>(obj) + field.offset;

            switch (field.type) {
                case FieldType::INT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const int32_t*>(fieldPtr));
                    break;
                case FieldType::UINT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint32_t*>(fieldPtr));
                    break;
                case FieldType::FLOAT:
                    lua_pushnumber(luaState, *reinterpret_cast<const float*>(fieldPtr));
                    break;
                case FieldType::UINT64:
                    lua_pushnumber(luaState, static_cast<double>(*reinterpret_cast<const uint64_t*>(fieldPtr)));
                    break;
                case FieldType::UINT8:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint8_t*>(fieldPtr));
                    break;
                case FieldType::UINT16:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint16_t*>(fieldPtr));
                    break;
                case FieldType::STRING: {
                    const char* str = *reinterpret_cast<const char* const*>(fieldPtr);
                    lua_pushstring(luaState, str ? const_cast<char*>(str) : const_cast<char*>(""));
                    break;
                }
                case FieldType::INLINE_STRING:
                    lua_pushstring(luaState, const_cast<char*>(fieldPtr));
                    break;
            }
            lua_settable(luaState, -3);
        }
    }

    template<typename T>
    inline void PushArrayFieldsToLua(uintptr_t* luaState, const T* obj, const ArrayFieldDescriptor* fields, size_t fieldCount) {
        for (size_t i = 0; i < fieldCount; ++i) {
            const auto& field = fields[i];
            lua_pushstring(luaState, const_cast<char*>(field.name));
            lua_newtable(luaState);

            const char* fieldPtr = reinterpret_cast<const char*>(obj) + field.offset;

            for (size_t j = 0; j < field.count; ++j) {
                lua_pushnumber(luaState, j + 1); // Lua is 1-indexed

                switch (field.type) {
                    case FieldType::INT32:
                        lua_pushnumber(luaState, reinterpret_cast<const int32_t*>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT32:
                        lua_pushnumber(luaState, reinterpret_cast<const uint32_t*>(fieldPtr)[j]);
                        break;
                    case FieldType::FLOAT:
                        lua_pushnumber(luaState, reinterpret_cast<const float*>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT64:
                        lua_pushnumber(luaState, static_cast<double>(reinterpret_cast<const uint64_t*>(fieldPtr)[j]));
                        break;
                    case FieldType::UINT8:
                        lua_pushnumber(luaState, reinterpret_cast<const uint8_t*>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT16:
                        lua_pushnumber(luaState, reinterpret_cast<const uint16_t*>(fieldPtr)[j]);
                        break;
                    case FieldType::STRING: {
                        const char* str = reinterpret_cast<const char* const*>(fieldPtr)[j];
                        lua_pushstring(luaState, str ? const_cast<char*>(str) : const_cast<char*>(""));
                        break;
                    }
                    default: break;
                }
                lua_settable(luaState, -3);
            }
            lua_settable(luaState, -3);
        }
    }

    // Version that uses reusable table references for each array field
    template<typename T>
    inline void PushArrayFieldsToLuaWithRefs(uintptr_t* luaState, const T* obj, const ArrayFieldDescriptor* fields,
                                              size_t fieldCount, std::unordered_map<std::string, int>& refMap) {
        for (size_t i = 0; i < fieldCount; ++i) {
            const auto& field = fields[i];
            lua_pushstring(luaState, const_cast<char*>(field.name));

            // Get or create reusable table for this specific field name
            GetTableRef(luaState, refMap[field.name]);

            const char* fieldPtr = reinterpret_cast<const char*>(obj) + field.offset;

            for (size_t j = 0; j < field.count; ++j) {
                lua_pushnumber(luaState, j + 1); // Lua is 1-indexed

                switch (field.type) {
                    case FieldType::INT32:
                        lua_pushnumber(luaState, reinterpret_cast<const int32_t*>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT32:
                        lua_pushnumber(luaState, reinterpret_cast<const uint32_t*>(fieldPtr)[j]);
                        break;
                    case FieldType::FLOAT:
                        lua_pushnumber(luaState, reinterpret_cast<const float*>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT64:
                        lua_pushnumber(luaState, static_cast<double>(reinterpret_cast<const uint64_t*>(fieldPtr)[j]));
                        break;
                    case FieldType::UINT8:
                        lua_pushnumber(luaState, reinterpret_cast<const uint8_t*>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT16:
                        lua_pushnumber(luaState, reinterpret_cast<const uint16_t*>(fieldPtr)[j]);
                        break;
                    case FieldType::STRING: {
                        const char* str = reinterpret_cast<const char* const*>(fieldPtr)[j];
                        lua_pushstring(luaState, str ? const_cast<char*>(str) : const_cast<char*>(""));
                        break;
                    }
                    default: break;
                }
                lua_settable(luaState, -3);
            }
            lua_settable(luaState, -3);
        }
    }
}
