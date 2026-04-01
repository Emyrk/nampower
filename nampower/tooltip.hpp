#pragma once

#include "main.hpp"

namespace Nampower {
    extern uint32_t gActiveTooltipItemId;

    using SpellParserParseTextT = bool (__fastcall *)(game::SpellRec *spellRec,
                                                      char *buffer,
                                                      int bufferSize,
                                                      float param_4,
                                                      float param_5,
                                                      float param_6,
                                                      float param_7,
                                                      float param_8);

    bool SpellParserParseTextHook(hadesmem::PatchDetourBase *detour,
                                  game::SpellRec *spellRec,
                                  char *buffer,
                                  int bufferSize,
                                  float param_4,
                                  float param_5,
                                  float param_6,
                                  float param_7,
                                  float param_8);

    using CGTooltip_SetItemT = int (__fastcall *)(void *thisptr,
                                                  void *dummy_edx,
                                                  uint32_t itemId,
                                                  int **param_2,
                                                  int **param_3,
                                                  int param_4,
                                                  int param_5,
                                                  int param_6,
                                                  int param_7,
                                                  uint32_t param_8,
                                                  int ****param_9);

    int CGTooltip_SetItemHook(hadesmem::PatchDetourBase *detour,
                              void *thisptr,
                              void *dummy_edx,
                              uint32_t itemId,
                              int **param_2,
                              int **param_3,
                              int param_4,
                              int param_5,
                              int param_6,
                              int param_7,
                              uint32_t param_8,
                              int ****param_9);
}
