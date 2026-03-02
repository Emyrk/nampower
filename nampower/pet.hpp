//
// Created by pmacc on 3/1/2026.
//

#pragma once

#include "main.hpp"

namespace Nampower {
    using CGPetInfo_SetPetT = void (__fastcall *)(void *thisptr, void *dummy_edx, uint32_t guid1, uint32_t guid2);
    using TogglePetSlotAutocastT = void (__fastcall *)(uint32_t slot);
    using CGPetInfo_GetPetSpellActionT = void (__fastcall *)(uint32_t *action);

    void CGPetInfo_SetPetHook(hadesmem::PatchDetourBase *detour, void *thisptr, void *dummy_edx, uint32_t guid1, uint32_t guid2);
    void TogglePetSlotAutocastHook(hadesmem::PatchDetourBase *detour, uint32_t slot);
    void CGPetInfo_GetPetSpellActionHook(hadesmem::PatchDetourBase *detour, uint32_t *action);

    void SaveGreaterDemonAutocastPreferences();
    void SaveGreaterDemonAutocastPreferences(uint32_t spellSlot, uint32_t actionData);
    void RestoreGreaterDemonAutocastPreferences();
}
