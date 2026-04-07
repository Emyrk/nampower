//
// Created by pmacc on 1/8/2025.
//

#include "misc_scripts.hpp"
#include "logging.hpp"
#include "offsets.hpp"
#include "party_member_fields.hpp"
#include "items.hpp"
#include "dbc_fields.hpp"
#include "unit_fields.hpp"
#include "helper.hpp"
#include "lua_refs.hpp"
#include "auras.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace Nampower {
    bool gScriptQueued;
    int gScriptPriority = 1;
    char *queuedScript;

    uint32_t Script_GetNampowerVersion(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        lua_pushnumber(luaState, MAJOR_VERSION);
        lua_pushnumber(luaState, MINOR_VERSION);
        lua_pushnumber(luaState, PATCH_VERSION);

        return 3;
    }

    uint32_t Script_QueueScript(uintptr_t *luaState) {
        luaState = GetLuaStatePtr(); // pcall leads to corrupted lua state pointer on added scripts, not sure why

        DEBUG_LOG("Trying to queue script");

        auto const currentTime = GetTime();
        auto effectiveCastEndMs = EffectiveCastEndMs();
        auto remainingEffectiveCastTime = (effectiveCastEndMs > currentTime) ? effectiveCastEndMs - currentTime : 0;
        auto remainingGcd = (gCastData.gcdEndMs > currentTime) ? gCastData.gcdEndMs - currentTime : 0;
        auto inSpellQueueWindow = InSpellQueueWindow(remainingEffectiveCastTime, remainingGcd, false);

        if (inSpellQueueWindow) {
            // check if valid string
            if (lua_isstring(luaState, 1)) {
                auto script = lua_tostring(luaState, 1);

                if (script != nullptr && strlen(script) > 0) {
                    // save the script to be run later
                    queuedScript = script;
                    gScriptQueued = true;

                    // check if priority is set
                    if (lua_isnumber(luaState, 2)) {
                        gScriptPriority = (int) lua_tonumber(luaState, 2);

                        DEBUG_LOG("Queuing script priority " << gScriptPriority << ": " << script);
                    } else {
                        DEBUG_LOG("Queuing script: " << script);
                    }
                }
            } else {
                DEBUG_LOG("Invalid script");
                lua_error(luaState, "Usage: QueueScript(\"script\", (optional)priority)");
            }
        } else {
            // just call regular runscript
            auto const runScript = reinterpret_cast<LuaScriptT >(Offsets::Script_RunScript);
            return runScript(luaState);
        }

        return 0;
    }

    bool RunQueuedScript(int priority) {
        if (gScriptQueued && gScriptPriority == priority) {
            auto currentTime = GetTime();

            auto effectiveCastEndMs = EffectiveCastEndMs();
            // get max of cooldown and gcd
            auto delay = effectiveCastEndMs > gCastData.gcdEndMs ? effectiveCastEndMs : gCastData.gcdEndMs;

            if (delay <= currentTime) {
                DEBUG_LOG("Running queued script priority " << gScriptPriority << ": " << queuedScript);
                LuaCall(queuedScript);
                gScriptQueued = false;
                gScriptPriority = 1;
                return true;
            }
        }

        return false;
    }

    uint32_t Script_IsAuraHidden(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isnumber(luaState, 1)) {
            lua_error(luaState, "Usage: IsAuraHidden(spellId)");
            return 0;
        }

        auto const spellId = static_cast<uint32_t>(lua_tonumber(luaState, 1));
        if (IsAuraHiddenForLua(spellId)) {
            lua_pushnumber(luaState, 1);
        } else {
            lua_pushnil(luaState);
        }
        return 1;
    }

    uint32_t Script_CombatLogFlush(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        auto const slogFlush = reinterpret_cast<void(__stdcall *)(uint32_t)>(Offsets::SLogFlush);
        auto const combatLogFileHandle = *reinterpret_cast<uint32_t *>(Offsets::CombatLogFileHandle);
        slogFlush(combatLogFileHandle);

        return 0;
    }

    void EmitKeyEvent(uint32_t eventCode, EVENT_DATA_KEY *keyData) {
        // don't trigger events until player exists as event won't be registered
        if (keyData == nullptr || game::ClntObjMgrGetActivePlayerGuid() == 0) {
            return;
        }

        static char format[] = "%d%d%d%d";
        reinterpret_cast<int (__cdecl *)(int, char *, uint32_t, uint32_t, uint32_t, uint32_t)>(Offsets::SignalEventParam)(
            eventCode,
            format,
            keyData->key,
            keyData->metaKeyState,
            keyData->repeat,
            keyData->time);
    }

    bool CSimpleTop_OnKeyDownHook(hadesmem::PatchDetourBase *detour, EVENT_DATA_KEY *keyData, int param_2) {
        auto const onKeyDown = detour->GetTrampolineT<CSimpleTop_OnKeyDownT>();
        auto result = onKeyDown(keyData, param_2);

        EmitKeyEvent(game::KEY_DOWN, keyData);

        return result;
    }

    bool CSimpleTop_OnKeyUpHook(hadesmem::PatchDetourBase *detour, EVENT_DATA_KEY *keyData, int param_2) {
        auto const onKeyUp = detour->GetTrampolineT<CSimpleTop_OnKeyUpT>();
        auto result = onKeyUp(keyData, param_2);

        EmitKeyEvent(game::KEY_UP, keyData);

        return result;
    }

}
