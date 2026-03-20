#pragma once

#include "main.hpp"
#include "dbc_fields.hpp"
#include "game.hpp"
#include <unordered_map>
#include <string>
#include <cstddef>

namespace Nampower {
    // CGPartyMemberInfo member field descriptors (Lua names, offsets into CGPartyMemberInfo)
    extern const FieldDescriptor      cgPartyMemberFields[];
    extern const size_t               cgPartyMemberFieldsCount;
    extern const ArrayFieldDescriptor cgPartyMemberArrayFields[];
    extern const size_t               cgPartyMemberArrayFieldsCount;

    // CGPartyMemberInfo pet field descriptors (Lua names map to pet sub-fields)
    extern const FieldDescriptor      cgPartyMemberPetFields[];
    extern const size_t               cgPartyMemberPetFieldsCount;
    extern const ArrayFieldDescriptor cgPartyMemberPetArrayFields[];
    extern const size_t               cgPartyMemberPetArrayFieldsCount;

    // Hash tables for O(1) lookups
    extern std::unordered_map<std::string, size_t> cgPartyMemberFieldMap;
    extern std::unordered_map<std::string, size_t> cgPartyMemberArrayFieldMap;
    extern std::unordered_map<std::string, size_t> cgPartyMemberPetFieldMap;
    extern std::unordered_map<std::string, size_t> cgPartyMemberPetArrayFieldMap;

    void InitializeCGPartyMemberFieldMaps();
    uint32_t GetCGPartyMemberField(uintptr_t *luaState, const game::CGPartyMemberInfo *info,
                                   const char *fieldName, bool useCopy);
    uint32_t GetCGPartyMemberPetField(uintptr_t *luaState, const game::CGPartyMemberPetInfo *info,
                                      const char *fieldName, bool useCopy);
}
