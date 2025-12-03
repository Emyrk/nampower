//
// Created by pmacc on 1/8/2025.
//

#pragma once

#include "main.hpp"
#include "dbc_fields.hpp"
#include <unordered_map>
#include <string>
#include <cstddef>

namespace Nampower {
    // UnitFields field descriptors
    extern const FieldDescriptor unitFieldsFields[];
    extern const size_t unitFieldsFieldsCount;
    extern const ArrayFieldDescriptor unitFieldsArrayFields[];
    extern const size_t unitFieldsArrayFieldsCount;

    // Hash tables for O(1) field lookups
    extern std::unordered_map<std::string, size_t> unitFieldsFieldMap;
    extern std::unordered_map<std::string, size_t> unitFieldsArrayFieldMap;

    // Initialize unit fields maps (call once before using field lookups)
    void InitializeUnitFieldMaps();
}
