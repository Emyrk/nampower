#include "party_member_fields.hpp"
#include "helper.hpp"
#include "lua_refs.hpp"

namespace Nampower {
    std::unordered_map<std::string, size_t> cgPartyMemberFieldMap;
    std::unordered_map<std::string, size_t> cgPartyMemberArrayFieldMap;
    std::unordered_map<std::string, size_t> cgPartyMemberPetFieldMap;
    std::unordered_map<std::string, size_t> cgPartyMemberPetArrayFieldMap;
    static std::unordered_map<std::string, int> cgPartyMemberArrayRefs;
    static std::unordered_map<std::string, int> cgPartyMemberPetArrayRefs;
    static bool cgPartyMemberFieldMapsInitialized = false;

    const FieldDescriptor cgPartyMemberFields[] = {
        {"guid",      offsetof(game::CGPartyMemberInfo, guid),      FieldType::UINT64},
        {"status",    offsetof(game::CGPartyMemberInfo, status),    FieldType::UINT8},
        {"powerType", offsetof(game::CGPartyMemberInfo, powerType), FieldType::UINT8},
        {"health",    offsetof(game::CGPartyMemberInfo, health),    FieldType::UINT16},
        {"maxHealth", offsetof(game::CGPartyMemberInfo, maxHealth), FieldType::UINT16},
        {"power",     offsetof(game::CGPartyMemberInfo, power),     FieldType::UINT16},
        {"maxPower",  offsetof(game::CGPartyMemberInfo, maxPower),  FieldType::UINT16},
        {"level",     offsetof(game::CGPartyMemberInfo, level),     FieldType::UINT16},
        {"zoneId",    offsetof(game::CGPartyMemberInfo, zoneId),    FieldType::UINT16},
        {"posX",      offsetof(game::CGPartyMemberInfo, posX),      FieldType::UINT16},
        {"posY",      offsetof(game::CGPartyMemberInfo, posY),      FieldType::UINT16},
    };
    const size_t cgPartyMemberFieldsCount = sizeof(cgPartyMemberFields) / sizeof(cgPartyMemberFields[0]);

    const ArrayFieldDescriptor cgPartyMemberArrayFields[] = {
        {"aura", offsetof(game::CGPartyMemberInfo, aura), FieldType::UINT16, 48},
    };
    const size_t cgPartyMemberArrayFieldsCount = sizeof(cgPartyMemberArrayFields) / sizeof(cgPartyMemberArrayFields[0]);

    const FieldDescriptor cgPartyMemberPetFields[] = {
        {"guid",      offsetof(game::CGPartyMemberPetInfo, guid),      FieldType::UINT64},
        {"name",      offsetof(game::CGPartyMemberPetInfo, name),      FieldType::INLINE_STRING},
        {"powerType", offsetof(game::CGPartyMemberPetInfo, powerType), FieldType::UINT8},
        {"displayId", offsetof(game::CGPartyMemberPetInfo, displayId), FieldType::UINT16},
        {"health",    offsetof(game::CGPartyMemberPetInfo, health),    FieldType::UINT16},
        {"maxHealth", offsetof(game::CGPartyMemberPetInfo, maxHealth), FieldType::UINT16},
        {"power",     offsetof(game::CGPartyMemberPetInfo, power),     FieldType::UINT16},
        {"maxPower",  offsetof(game::CGPartyMemberPetInfo, maxPower),  FieldType::UINT16},
    };
    const size_t cgPartyMemberPetFieldsCount = sizeof(cgPartyMemberPetFields) / sizeof(cgPartyMemberPetFields[0]);

    const ArrayFieldDescriptor cgPartyMemberPetArrayFields[] = {
        {"aura", offsetof(game::CGPartyMemberPetInfo, aura), FieldType::UINT16, 48},
    };
    const size_t cgPartyMemberPetArrayFieldsCount = sizeof(cgPartyMemberPetArrayFields) / sizeof(cgPartyMemberPetArrayFields[0]);

    void InitializeCGPartyMemberFieldMaps() {
        if (cgPartyMemberFieldMapsInitialized) return;

        for (size_t i = 0; i < cgPartyMemberFieldsCount; ++i)
            cgPartyMemberFieldMap[cgPartyMemberFields[i].name] = i;
        for (size_t i = 0; i < cgPartyMemberArrayFieldsCount; ++i)
            cgPartyMemberArrayFieldMap[cgPartyMemberArrayFields[i].name] = i;
        for (size_t i = 0; i < cgPartyMemberPetFieldsCount; ++i)
            cgPartyMemberPetFieldMap[cgPartyMemberPetFields[i].name] = i;
        for (size_t i = 0; i < cgPartyMemberPetArrayFieldsCount; ++i)
            cgPartyMemberPetArrayFieldMap[cgPartyMemberPetArrayFields[i].name] = i;

        cgPartyMemberFieldMapsInitialized = true;
    }

    template <typename T>
    static uint32_t LookupCGPartyMemberField(uintptr_t *luaState, const T *info,
                                             const char *fieldName, bool useCopy,
                                             const std::unordered_map<std::string, size_t> &fieldMap,
                                             const FieldDescriptor *fields,
                                             const std::unordered_map<std::string, size_t> &arrayFieldMap,
                                             const ArrayFieldDescriptor *arrayFields,
                                             std::unordered_map<std::string, int> &arrayRefs) {
        auto simpleIt = fieldMap.find(fieldName);
        if (simpleIt != fieldMap.end()) {
            const auto &field = fields[simpleIt->second];
            const char *ptr = reinterpret_cast<const char *>(info) + field.offset;
            switch (field.type) {
                case FieldType::UINT64: PushGuidString(luaState, *reinterpret_cast<const uint64_t *>(ptr)); return 1;
                case FieldType::UINT16: lua_pushnumber(luaState, *reinterpret_cast<const uint16_t *>(ptr)); return 1;
                case FieldType::UINT8:  lua_pushnumber(luaState, *reinterpret_cast<const uint8_t *>(ptr));  return 1;
                case FieldType::INLINE_STRING: lua_pushstring(luaState, const_cast<char *>(ptr));           return 1;
                default: break;
            }
        }

        auto arrayIt = arrayFieldMap.find(fieldName);
        if (arrayIt != arrayFieldMap.end()) {
            const auto &field = arrayFields[arrayIt->second];
            const char *ptr = reinterpret_cast<const char *>(info) + field.offset;

            if (useCopy)
                lua_newtable(luaState);
            else
                GetTableRef(luaState, arrayRefs[fieldName]);

            for (size_t i = 0; i < field.count; ++i) {
                lua_pushnumber(luaState, static_cast<double>(i + 1));
                lua_pushnumber(luaState, reinterpret_cast<const uint16_t *>(ptr)[i]);
                lua_settable(luaState, -3);
            }
            return 1;
        }

        return 0;
    }

    uint32_t GetCGPartyMemberField(uintptr_t *luaState, const game::CGPartyMemberInfo *info,
                                   const char *fieldName, bool useCopy) {
        InitializeCGPartyMemberFieldMaps();
        return LookupCGPartyMemberField(luaState, info, fieldName, useCopy,
                                        cgPartyMemberFieldMap, cgPartyMemberFields,
                                        cgPartyMemberArrayFieldMap, cgPartyMemberArrayFields,
                                        cgPartyMemberArrayRefs);
    }

    uint32_t GetCGPartyMemberPetField(uintptr_t *luaState, const game::CGPartyMemberPetInfo *info,
                                      const char *fieldName, bool useCopy) {
        InitializeCGPartyMemberFieldMaps();
        return LookupCGPartyMemberField(luaState, info, fieldName, useCopy,
                                        cgPartyMemberPetFieldMap, cgPartyMemberPetFields,
                                        cgPartyMemberPetArrayFieldMap, cgPartyMemberPetArrayFields,
                                        cgPartyMemberPetArrayRefs);
    }
}
