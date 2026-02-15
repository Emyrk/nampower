//
// Created by pmacc on 2/15/2026.
//

#include "player_scripts.hpp"
#include "helper.hpp"
#include "offsets.hpp"

namespace Nampower {
    static uint32_t GetPlayerMovementFlags() {
        auto playerGuid = game::ClntObjMgrGetActivePlayerGuid();
        if (playerGuid == 0) return 0;

        auto unit = game::GetObjectPtr(playerGuid);
        if (!unit) return 0;

        // Movement flags are at offset 0x9E8 from unit base (divide by 4 for uintptr_t* indexing)
        return *(unit + 0x27A);
    }

    uint32_t Script_PlayerIsMoving(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto movementFlags = GetPlayerMovementFlags();
        constexpr uint32_t MOVEFLAG_MASK_MOVING =
            game::MOVEFLAG_FORWARD | game::MOVEFLAG_BACKWARD |
            game::MOVEFLAG_STRAFE_LEFT | game::MOVEFLAG_STRAFE_RIGHT |
            game::MOVEFLAG_PITCH_UP | game::MOVEFLAG_PITCH_DOWN |
            game::MOVEFLAG_JUMPING | game::MOVEFLAG_FALLINGFAR |
            game::MOVEFLAG_SPLINE_ELEVATION;

        if (movementFlags & MOVEFLAG_MASK_MOVING) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnil(luaState);
        }
        return 1;
    }

    uint32_t Script_PlayerIsRooted(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto movementFlags = GetPlayerMovementFlags();
        if (movementFlags & game::MOVEFLAG_ROOT) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnil(luaState);
        }
        return 1;
    }

    uint32_t Script_PlayerIsSwimming(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto movementFlags = GetPlayerMovementFlags();
        if (movementFlags & game::MOVEFLAG_SWIMMING) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnil(luaState);
        }
        return 1;
    }
}
