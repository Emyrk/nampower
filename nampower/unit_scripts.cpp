//
// Created by pmacc on 4/7/2026.
//

#include "unit_scripts.hpp"
#include "helper.hpp"
#include "lua_refs.hpp"
#include "offsets.hpp"
#include "party_member_fields.hpp"
#include "unit_fields.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace Nampower {
    // Reusable table references to reduce memory allocations
    static int unitDataTableRef = LUA_REFNIL;

    // Maps to store separate references for each array field name (for Field functions)
    static std::unordered_map<std::string, int> unitFieldsArrayFieldRefs;

    // Maps to store separate references for nested array fields (for main table functions)
    static std::unordered_map<std::string, int> unitFieldsNestedArrayRefs;

    uint32_t Script_GetRaidTargets(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto raidTargets =
            reinterpret_cast<uint64_t *>(static_cast<uint32_t>(Offsets::CGRaidInfo_m_raidtargets));

        lua_newtable(luaState);

        for (uint32_t i = 0; i < 8; ++i) {
            lua_pushnumber(luaState, i + 1);
            PushGuidString(luaState, raidTargets[i]);
            lua_settable(luaState, -3);
        }

        return 1;
    }

    uint32_t Script_GetUnitData(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: GetUnitData(unitToken, [copy]) - unitToken can be 'player', 'target', 'pet', etc., or a GUID string");
            return 0;
        }

        const char *unitToken = lua_tostring(luaState, 1);

        bool useCopy = false;
        if (lua_isnumber(luaState, 2)) {
            useCopy = static_cast<int>(lua_tonumber(luaState, 2)) != 0;
        }

        uint64_t guid = GetUnitGuidFromString(unitToken);

        if (guid == 0) {
            lua_pushnil(luaState);
            return 1;
        }

        auto unit = game::GetObjectPtr(guid);
        if (!unit) {
            lua_pushnil(luaState);
            return 1;
        }

        auto *unitFields = *reinterpret_cast<game::UnitFields **>(unit + 68);
        if (!unitFields) {
            lua_pushnil(luaState);
            return 1;
        }

        if (useCopy) {
            lua_newtable(luaState);
        } else {
            GetTableRef(luaState, unitDataTableRef);
        }

        for (size_t i = 0; i < unitFieldsFieldsCount; ++i) {
            const auto &field = unitFieldsFields[i];
            lua_pushstring(luaState, const_cast<char *>(field.name));

            const char *fieldPtr = reinterpret_cast<const char *>(unitFields) + field.offset;
            switch (field.type) {
                case FieldType::UINT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint32_t *>(fieldPtr));
                    break;
                case FieldType::UINT8:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint8_t *>(fieldPtr));
                    break;
                case FieldType::FLOAT:
                    lua_pushnumber(luaState, *reinterpret_cast<const float *>(fieldPtr));
                    break;
                case FieldType::UINT64:
                    PushGuidString(luaState, *reinterpret_cast<const uint64_t *>(fieldPtr));
                    break;
                default:
                    lua_pushnil(luaState);
                    break;
            }

            lua_settable(luaState, -3);
        }

        if (useCopy) {
            PushArrayFieldsToLua(luaState, unitFields, unitFieldsArrayFields, unitFieldsArrayFieldsCount);
        } else {
            PushArrayFieldsToLuaWithRefs(luaState, unitFields, unitFieldsArrayFields, unitFieldsArrayFieldsCount, unitFieldsNestedArrayRefs);
        }

        return 1;
    }

    uint32_t Script_GetUnitField(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        if (!lua_isstring(luaState, 1) || !lua_isstring(luaState, 2)) {
            lua_error(luaState, "Usage: GetUnitField(unitToken, fieldName, [copy]) - unitToken can be 'player', 'target', 'pet', etc., or a GUID string");
            return 0;
        }

        InitializeUnitFieldMaps();

        const char *unitToken = lua_tostring(luaState, 1);
        const char *fieldName = lua_tostring(luaState, 2);

        bool useCopy = false;
        if (lua_isnumber(luaState, 3)) {
            useCopy = static_cast<int>(lua_tonumber(luaState, 3)) != 0;
        }

        uint64_t guid = GetUnitGuidFromString(unitToken);

        if (guid == 0) {
            lua_pushnil(luaState);
            return 1;
        }

        auto unit = game::GetObjectPtr(guid);
        if (!unit) {
            if (auto *info = game::GetPartyOrRaidMemberInfo(guid)) {
                if (!GetCGPartyMemberField(luaState, info, fieldName, useCopy)) {
                    lua_pushnil(luaState);
                }
                return 1;
            }
            if (auto *info = game::GetPartyOrRaidPetInfo(guid)) {
                if (!GetCGPartyMemberPetField(luaState, info, fieldName, useCopy)) {
                    lua_pushnil(luaState);
                }
                return 1;
            }
            lua_pushnil(luaState);
            return 1;
        }

        auto *unitFields = *reinterpret_cast<game::UnitFields **>(unit + 68);
        if (!unitFields) {
            lua_pushnil(luaState);
            return 1;
        }

        auto simpleIt = unitFieldsFieldMap.find(fieldName);
        if (simpleIt != unitFieldsFieldMap.end()) {
            size_t i = simpleIt->second;
            const char *fieldPtr = reinterpret_cast<const char *>(unitFields) + unitFieldsFields[i].offset;

            switch (unitFieldsFields[i].type) {
                case FieldType::UINT32:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint32_t *>(fieldPtr));
                    return 1;
                case FieldType::UINT8:
                    lua_pushnumber(luaState, *reinterpret_cast<const uint8_t *>(fieldPtr));
                    return 1;
                case FieldType::UINT64:
                    PushGuidString(luaState, *reinterpret_cast<const uint64_t *>(fieldPtr));
                    return 1;
                case FieldType::FLOAT:
                    lua_pushnumber(luaState, *reinterpret_cast<const float *>(fieldPtr));
                    return 1;
                default:
                    break;
            }
        }

        auto arrayIt = unitFieldsArrayFieldMap.find(fieldName);
        if (arrayIt != unitFieldsArrayFieldMap.end()) {
            size_t i = arrayIt->second;
            const auto &field = unitFieldsArrayFields[i];

            if (useCopy) {
                lua_newtable(luaState);
            } else {
                GetTableRef(luaState, unitFieldsArrayFieldRefs[fieldName]);
            }

            const char *fieldPtr = reinterpret_cast<const char *>(unitFields) + field.offset;

            for (size_t j = 0; j < field.count; ++j) {
                lua_pushnumber(luaState, j + 1);

                switch (field.type) {
                    case FieldType::UINT32:
                        lua_pushnumber(luaState, reinterpret_cast<const uint32_t *>(fieldPtr)[j]);
                        break;
                    case FieldType::UINT8:
                        lua_pushnumber(luaState, reinterpret_cast<const uint8_t *>(fieldPtr)[j]);
                        break;
                    case FieldType::FLOAT:
                        lua_pushnumber(luaState, reinterpret_cast<const float *>(fieldPtr)[j]);
                        break;
                    default:
                        lua_pushnumber(luaState, 0);
                        break;
                }
                lua_settable(luaState, -3);
            }
            return 1;
        }

        lua_error(luaState, "Unknown field name");
        return 0;
    }

    uint32_t Script_SetMouseoverUnit(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        uint64_t guid = 0;
        if (lua_isstring(luaState, 1)) {
            const char *unitToken = lua_tostring(luaState, 1);
            auto const getGUIDFromName = reinterpret_cast<GetGUIDFromNameT>(Offsets::GetGUIDFromName);
            guid = getGUIDFromName(unitToken);
        }

        auto const handleObjectTrackChange = reinterpret_cast<CGGameUI_HandleObjectTrackChangeT>(
            Offsets::CGGameUI_HandleObjectTrackChange);
        handleObjectTrackChange(guid, 0, 0);

        auto const mouseoverLow = *reinterpret_cast<uint32_t *>(Offsets::MouseoverGuidLow);
        auto const mouseoverHigh = *reinterpret_cast<uint32_t *>(Offsets::MouseoverGuidHigh);

        if (mouseoverLow != 0 || mouseoverHigh != 0) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnil(luaState);
        }

        return 1;
    }

    uint32_t Script_SetLocalRaidTargetIndex(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isstring(luaState, 1) || !lua_isnumber(luaState, 2)) {
            lua_error(luaState, "Usage: SetLocalRaidTargetIndex(unitStr, raidTargetIndex)");
            return 0;
        }

        const char *unitStr = lua_tostring(luaState, 1);
        int32_t raidTargetIndex = static_cast<int32_t>(lua_tonumber(luaState, 2)) - 1;

        uint64_t targetGuid = GetUnitGuidFromString(unitStr);
        if (targetGuid == 0) {
            return 0;
        }

        auto localPlayer = game::GetObjectPtr(game::ClntObjMgrGetActivePlayerGuid());
        auto targetUnit = game::GetObjectPtr(targetGuid);
        if (!localPlayer || !targetUnit) {
            return 0;
        }

        using CGUnit_C_CanInteractT = uint32_t (__thiscall *)(uintptr_t *thisPtr, uintptr_t *target);
        auto const canInteract =
            reinterpret_cast<CGUnit_C_CanInteractT>(static_cast<uint32_t>(Offsets::CGUnit_C_CanInteract));
        auto const *targetObject = reinterpret_cast<game::CGObject const *>(targetUnit);
        if (targetObject->m_obj &&
            targetObject->m_obj->m_type == game::TYPE_OBJECT &&
            !canInteract(localPlayer, targetUnit)) {
            auto const targetTypeMask = targetObject->m_obj->m_type;

            if (((targetTypeMask >> 4) & 1) != 0) {
                lua_error(luaState, "Cannot set raid target index: player cannot interact with object");
                return 0;
            }
        }

        if (raidTargetIndex >= 8) {
            lua_error(luaState, "SetLocalRaidTargetIndex: raidTargetIndex must be in range 0-8");
            return 0;
        }

        using CGUnit_C_NamePlateUpdateRaidTargetT = void (__thiscall *)(uintptr_t *thisPtr);
        auto const namePlateUpdateRaidTarget =
            reinterpret_cast<CGUnit_C_NamePlateUpdateRaidTargetT>(
                static_cast<uint32_t>(Offsets::CGUnit_C_NamePlateUpdateRaidTarget));

        auto raidTargets =
            reinterpret_cast<uint64_t *>(static_cast<uint32_t>(Offsets::CGRaidInfo_m_raidtargets));

        // if raidTargetIndex is valid and already has the targetGuid, do nothing
        if (raidTargetIndex >= 0 && raidTargets[raidTargetIndex] == targetGuid) {
            return 1;
        }

        // Clear out previous mark for this guid if there was one
        for (uint32_t i = 0; i < 8; ++i) {
            if (i == static_cast<uint32_t>(raidTargetIndex) || raidTargets[i] != targetGuid) {
                continue;
            }

            raidTargets[i] = 0;
        }

        auto const previousTargetGuid = raidTargets[raidTargetIndex];
        // Update the nameplate for the previous guid for the mark we are going to use
        if (previousTargetGuid != 0) {
            raidTargets[raidTargetIndex] = 0;

            auto previousTargetUnit = game::GetObjectPtr(previousTargetGuid);
            if (previousTargetUnit) {
                namePlateUpdateRaidTarget(previousTargetUnit);
            }
        }

        // Store new mark if >= 0, -1 means clear
        if (raidTargetIndex >= 0) {
            raidTargets[raidTargetIndex] = targetGuid;
        }

        // Update the nameplate for the new guid for the mark we are going to use
        namePlateUpdateRaidTarget(targetUnit);

        return 1;
    }

    uint32_t Script_SetRaidTargetHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState) {
        auto activePlayerGuid = game::ClntObjMgrGetActivePlayerGuid();
        auto const numRaidMembers =
            *reinterpret_cast<uint32_t *>(static_cast<uint32_t>(Offsets::CGRaidInfo_m_numRaidMembers));

        using CGPartyInfo_NumMembersT = int (__fastcall *)();
        auto const numPartyMembers =
            reinterpret_cast<CGPartyInfo_NumMembersT>(static_cast<uint32_t>(Offsets::CGPartyInfo_NumMembers));
        auto const partyMemberCount = numPartyMembers ? numPartyMembers() : 0;

        // if not in party or raid use local version
        if (gUserSettings.enableLocalSetRaidTarget &&
            (activePlayerGuid == 0 || (numRaidMembers == 0 && partyMemberCount == 0))) {
            return Script_SetLocalRaidTargetIndex(luaState);
        }

        auto const original = detour->GetTrampolineT<LuaScriptT>();
        return original(luaState);
    }

    uint32_t Script_GetUnitGUID(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: GetUnitGUID(unitToken)");
            return 0;
        }

        const char *unitToken = lua_tostring(luaState, 1);
        if (!unitToken || unitToken[0] == '\0') {
            lua_pushnil(luaState);
            return 1;
        }

        auto const getGUIDFromName = reinterpret_cast<GetGUIDFromNameT>(Offsets::GetGUIDFromName);
        const uint64_t guid = getGUIDFromName(unitToken);
        if (guid == 0) {
            lua_pushnil(luaState);
        } else {
            PushGuidString(luaState, guid);
        }

        return 1;
    }

    uint64_t GetGUIDFromNameHook(hadesmem::PatchDetourBase *detour, const char *nameStr) {
        auto const original = detour->GetTrampolineT<GetGUIDFromNameT>();

        if (!nameStr || nameStr[0] == '\0') {
            return 0;
        }

        const size_t tokenLen = std::strlen(nameStr);
        const bool isHexToken = (nameStr[0] == '0' && (nameStr[1] == 'x' || nameStr[1] == 'X'));
        const bool hasHexAndUnitSuffix = isHexToken && tokenLen > 18 &&
                                      (_stricmp(nameStr + 18, "owner") == 0 ||
                                       _stricmp(nameStr + 18, "target") == 0 ||
                                       _stricmp(nameStr + 18, "pet") == 0);

        if (_strnicmp(nameStr, "mark", 4) == 0) {
            const char digit = nameStr[4];
            if (digit >= '1' && digit <= '8' && nameStr[5] == '\0') {
                const uint32_t markIndex = static_cast<uint32_t>(digit - '1');
                auto const markGuids = reinterpret_cast<uint64_t *>(static_cast<uintptr_t>(Offsets::CGRaidInfo_m_raidtargets));
                return markGuids[markIndex];
            }
        }

        if (!hasHexAndUnitSuffix) {
            uint64_t result = original(nameStr);
            if (result != 0) {
                return result;
            }
        }

        if (isHexToken) {
            if (tokenLen < 18) {
                return 0;
            }

            char guidHex[17] = {};
            std::memcpy(guidHex, nameStr + 2, 16);
            if (!std::all_of(guidHex, guidHex + 16,
                             [](unsigned char c) { return std::isxdigit(c) != 0; })) {
                return 0;
            }

            const uint64_t parsedGuid = std::strtoull(guidHex, nullptr, 16);
            const char *const hexSuffix = nameStr + 18;

            if (_stricmp(hexSuffix, "owner") == 0) {
                return game::UnitGetOwnerGuidForGuid(parsedGuid);
            }

            if (_stricmp(hexSuffix, "target") == 0) {
                return game::UnitGetTargetGuidForGuid(parsedGuid);
            }

            if (_stricmp(hexSuffix, "pet") == 0) {
                return game::UnitGetPetGuidForGuid(parsedGuid);
            }

            return parsedGuid;
        }

        if (tokenLen <= 0x14) {
            auto resolveBaseGuid = [&](size_t suffixLen) -> uint64_t {
                char buf[0x15] = {};
                std::memcpy(buf, nameStr, tokenLen - suffixLen);
                auto const hooked = reinterpret_cast<GetGUIDFromNameT>(
                    static_cast<uint32_t>(Offsets::GetGUIDFromName));
                return hooked(buf);
            };

            if (tokenLen > 5 && _stricmp(nameStr + tokenLen - 5, "owner") == 0) {
                const uint64_t baseGuid = resolveBaseGuid(5);
                return game::UnitGetOwnerGuidForGuid(baseGuid);
            }

            if (tokenLen > 6 && _stricmp(nameStr + tokenLen - 6, "target") == 0) {
                const uint64_t baseGuid = resolveBaseGuid(6);
                return game::UnitGetTargetGuidForGuid(baseGuid);
            }

            if (tokenLen > 3 && _stricmp(nameStr + tokenLen - 3, "pet") == 0) {
                const uint64_t baseGuid = resolveBaseGuid(3);
                return game::UnitGetPetGuidForGuid(baseGuid);
            }
        }

        return 0;
    }

    uint32_t CSimpleFrame_GetNameHook(hadesmem::PatchDetourBase *detour, uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        static auto typeIdCounter = reinterpret_cast<uint32_t *>(0x00CEEF6C);
        static auto nameplateTypeId = reinterpret_cast<uint32_t *>(0x00CF0C3C);

        if (*nameplateTypeId == 0) {
            *typeIdCounter = *typeIdCounter + 1;
            *nameplateTypeId = *typeIdCounter;
        }
        uint32_t typeId = *nameplateTypeId;

        uintptr_t *frameObj = nullptr;
        uintptr_t *vtable = nullptr;

        int luaArgType = lua_type(luaState, 1);
        if (luaArgType == LUA_TTABLE) {
            lua_rawgeti(luaState, 1, 0);

            frameObj = reinterpret_cast<uintptr_t *>(lua_touserdata(luaState, -1));
            lua_settop(luaState, -2);

            if (frameObj == nullptr) {
                lua_error(luaState, "Attempt to find 'this' in non-frame table");
            } else if (!IsReadableMemory(frameObj, sizeof(uintptr_t))) {
                lua_pushnil(luaState);
                return 1;
            } else {
                vtable = reinterpret_cast<uintptr_t *>(*frameObj);
                if (!IsReadableMemory(vtable, 5 * sizeof(uintptr_t))) {
                    lua_pushnil(luaState);
                    return 1;
                }
                using IsTypeT = char (__thiscall *)(uintptr_t *thisPtr, uint32_t typeId);
                auto isType = reinterpret_cast<IsTypeT>(vtable[4]);
                if (isType(frameObj, typeId) == 0) {
                    lua_error(luaState, "Wrong object type for member function");
                }
            }
        } else {
            lua_error(luaState, "Attempt to find 'this' in non-table value");
        }

        if (lua_isnumber(luaState, 2) && static_cast<int>(lua_tonumber(luaState, 2)) != 0) {
            if (!IsReadableMemory(frameObj + 0x13a, 2 * sizeof(uint32_t))) {
                lua_pushnil(luaState);
            } else {
                uint32_t guidLow = frameObj[0x13a];
                uint32_t guidHigh = frameObj[0x13b];
                uint64_t guid = (static_cast<uint64_t>(guidHigh) << 32) | guidLow;
                PushGuidString(luaState, guid);
            }
        } else {
            using GetNameT = char *(__thiscall *)(uintptr_t *thisPtr);
            auto getName = reinterpret_cast<GetNameT>(vtable[1]);
            char *name = getName(frameObj);

            if (name != nullptr && name[0] != '\0') {
                lua_pushstring(luaState, name);
            } else {
                lua_pushnil(luaState);
            }
        }

        return 1;
    }
}
