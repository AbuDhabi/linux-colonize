#ifndef COLONIZE_CORE_AI_POPUP_RENDER_H
#define COLONIZE_CORE_AI_POPUP_RENDER_H

/*
 * Internal seam between ai_popup.c (SHARED: the sim->UI popup queue) and
 * ai_popup_render.c (UI: geometry, portrait sheets, painter), created by the
 * 2026-09-16 sim/UI split. Public prototypes stay in ai_popup.h.
 */

#include "core/ai_popup.h"

#include <stdbool.h>

int ai_popup_option_at_y(const AiPopupState* st, int mouse_y);
void ai_popup_finish(AiPopupState* st, bool cancelled, int choice_id);

#endif /* COLONIZE_CORE_AI_POPUP_RENDER_H */
