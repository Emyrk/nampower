//
// Created by pmacc on 9/21/2024.
//

#pragma once

#include "game.hpp"
#include <Windows.h>
#include "main.hpp"

namespace Nampower {
    void CastQueuedNonGcdSpell();

    void CastQueuedNormalSpell();

    void CastQueuedSpells();

    void TriggerSpellQueuedEvent(QueueEvents queueEventCode, uint32_t spellId);

    void CGSpellBook_CastSpellHook(hadesmem::PatchDetourBase *detour, uint32_t spellSlot, int bookType,
                                   uint64_t target);

    void CGActionBar_UseActionHook(hadesmem::PatchDetourBase *detour, uint32_t actionSlot, int param_2, int param_3);

    bool Spell_C_CastSpellHook(hadesmem::PatchDetourBase *detour, uint32_t *casterUnit, uint32_t spellId,
                               game::CGItem_C *item,
                               std::uint64_t guid);

    void
    CancelSpellHook(hadesmem::PatchDetourBase *detour, bool failed, bool notifyServer, game::SpellCastResult reason);

    void SpellGoHook(hadesmem::PatchDetourBase *detour, uint64_t *itemGUID, uint64_t *casterGUID, uint32_t spellId,
                     CDataStore *packet);

    bool Spell_C_TargetSpellHook(hadesmem::PatchDetourBase *detour,
                                 uint32_t *player,
                                 uint32_t *spellId,
                                 uint32_t unk3,
                                 float unk4);

    uint32_t Spell_C_HandleTerrainClickHook(hadesmem::PatchDetourBase *detour, game::CTerrainClickEvent *event);

    void CGWorldFrame_OnLayerTrackTerrainHook(hadesmem::PatchDetourBase *detour, void *thisptr, int dummy_edx,
                                              int param_1);

    float Spell_C_GetSpellRadiusHook(hadesmem::PatchDetourBase *detour);

    void SendCastHook(hadesmem::PatchDetourBase *detour, game::SpellCast *cast, char unk);

    void TriggerQuickcast();
}
