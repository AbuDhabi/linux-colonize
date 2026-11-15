/*
 * MOD_AI_IMPROVEMENTS nation gate (docs/ai_euro_improvement_spec.md), kept
 * apart from ai_euro_improved.c so the slim test targets, which link the
 * production code but not the AI, can link it.
 */
#include "core/ai_euro.h"
#include "core/ai_king.h"
#include "core/col1_save.h"
#include "core/mods.h"

bool ai_euro_improved_nation(const ColonizeCol1Save* col1, int human_nation, int nation) {
  if (!mods_on(MOD_AI_IMPROVEMENTS) || nation < 0 || nation > 3 || nation == human_nation) {
    return false;
  }
  if (col1) {
    if (col1->player[nation].control == 0) {
      return false;
    }
    if (col1->head.game_options.woi != 0 && nation == ai_king_crown_nation_col1(col1, human_nation)) {
      return false;
    }
  }
  return true;
}
