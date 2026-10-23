#include "core/unit_stack.h"

#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/fb.h"
#include "core/font.h"
#include "core/map_menu.h"
#include "core/popup.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/ui_button.h"
#include "core/ui_colors.h"
#include "core/unit_chrome.h"

static void unit_stack_close(UnitStackPopup* dlg) {
  if (!dlg) {
    return;
  }
  dlg->open = false;
  dlg->count = 0;
  dlg->selection = 0;
}

bool unit_stack_try_open(
  UnitStackPopup* dlg,
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id
) {
  if (!dlg || !pool) {
    return false;
  }
  int n = units_collect_tile_stack(pool, x, y, nation_id, dlg->ids, UNITS_TILE_STACK_MAX);
  if (n <= 1) {
    return false;
  }
  /* port_bugs.md #654: FUN_2b5a_1b5a raw 42760 stops adding rows at 10 (`local_54 < 10`):
   * units past the tenth on the tile's chain are simply not listed, and the pick walks
   * the chain to the chosen row (raw 42765-42767), so they cannot be picked here. */
  if (n > 10) {
    n = 10;
  }
  dlg->open = true;
  dlg->tile_x = x;
  dlg->tile_y = y;
  dlg->count = n;
  /* DOS opens the picker with the currently active unit's row (the ship you
   * clicked) highlighted, not row 0 (port_bugs.md loaded-unit activation flow). */
  dlg->selection = 0;
  for (int i = 0; i < n; ++i) {
    if (dlg->ids[i] == pool->selected_id) {
      dlg->selection = i;
      break;
    }
  }
  return true;
}

/*
 * port_bugs.md #654: FUN_2b5a_1b5a raw 42738-42755 — the parenthetical word after
 * a stack row's name. Gate: DS:0x30e[type] >= 0 (units_type_has_profession_slot).
 * Then profession == DS:0x30e[type] (the type's own default role, e.g. Soldier
 * for a Dragoon/Cont.Army) -> @MISC[4] "Expert"; profession == 0x1c (NONE)
 * -> @MISC[3] "None"; else @JOB column 0 singular for the actual profession
 * (a cross-specialized body, "Farmer").
 */
static const char* unit_stack_row_paren_word(
  const ColonizeMsgCatalog* names, int type_index, int profession
) {
  if (!units_type_has_profession_slot(type_index)) {
    return NULL;
  }
  if (profession == units_type_default_job(type_index)) {
    return reports_misc_display_word(4, "");
  }
  if (profession == UNITS_JOB_NONE) {
    return reports_misc_display_word(3, "");
  }
  /* port_bugs.md #654: @JOB column 0 (singular — "Soldier", not "Soldiers"), the
   * table FUN_2b5a_1b5a raw 42749 reads (-0x715e + profession*8); split by
   * assets_msg_row_field (the local copy went with port_bugs.md #1109). */
  static char buf[40];
  if (!assets_msg_row_field(names, "JOB", profession, 0, buf, sizeof(buf)) || !buf[0]) {
    return NULL;
  }
  return buf;
}

/* port_bugs.md #654: one row's full label ("Dutch Dragoons (Soldier)") — DOS
 * FUN_2b5a_1b5a raw 42685-42760. Port used to skip the @NATIONALITY
 * adjective, read the plural @JOB column instead of the singular one, print
 * nothing for an unskilled body instead of "(None)", and append a port-only
 * "(aboard)"/"(ready)" suffix no DOS row has (port_bugs.md #260 asked for that
 * suffix explicitly, so it stays). DOS reads the raw DS:0x8d0a table here
 * (no WoI swap); the port deliberately uses the WoI words ("Rebel"/"Tory")
 * to match the sidebar and combat labels (user request 2026-10-06). */
static void unit_stack_row_label(
  const ColonizeUnitPool* pool,
  const ColonizeMsgCatalog* names,
  const ColonizeCol1Save* col1,
  const ColonizeUnit* u,
  char* out,
  size_t out_size
) {
  const char* name = units_display_name(pool, u);
  const char* nation_adj = u ? reports_nation_adjective_woi(col1, u->nation_id) : NULL;
  const char* word =
    u ? unit_stack_row_paren_word(names, u->type_index, u->profession) : NULL;
  char base[64];
  if (nation_adj && nation_adj[0] && name) {
    snprintf(base, sizeof(base), "%s %s", nation_adj, name);
  } else {
    snprintf(base, sizeof(base), "%s", name ? name : "");
  }
  char with_word[72];
  if (word) {
    if (snprintf(with_word, sizeof(with_word), "%s (%s)", base, word) >= (int)sizeof(with_word)) {
      snprintf(with_word, sizeof(with_word), "%s", base); /* too long: drop the job word */
    }
  } else {
    snprintf(with_word, sizeof(with_word), "%s", base);
  }
  if (u && u->aboard_ship_id >= 0) {
    if (snprintf(
          out, out_size, "%s (%s)", with_word,
          u->orders == UNITS_ORDER_SENTRY ? "aboard" : "ready"
        ) >= (int)out_size) {
      snprintf(out, out_size, "%s", with_word); /* too long: drop the suffix */
    }
  } else {
    snprintf(out, out_size, "%s", with_word);
  }
}

/* FUN_2b5a_1b5a row hit: row k spans its icon row, frame_y + 5 + 16k .. + 16
 * (unit_stack_render). */
static int unit_stack_row_at_y(const UnitStackPopup* dlg, int my) {
  return dlg ? popup_row_at_y(dlg->list_y0, dlg->line_h, dlg->count, my) : -1;
}

/*
 * port_bugs.md (two-step, final form): a pick on a row with standing orders only
 * cancels them (units_wake — overnight-park refund rules apply; the row's
 * orders chrome updates in place) and keeps the dialog open. A pick on an
 * order-less row activates that unit — if it has moves — and closes. Opening
 * the popup itself never touches anyone's orders.
 */
static void unit_stack_activate_row(UnitStackPopup* dlg, ColonizeUnitPool* pool, int idx, int* out_select_id) {
  if (!dlg || !pool || idx < 0 || idx >= dlg->count || !out_select_id) {
    return;
  }
  *out_select_id = -1;
  const int uid = dlg->ids[idx];
  ColonizeUnit* u = units_get(pool, uid);
  if (!u || !u->active) {
    return;
  }
  dlg->selection = idx;
  if (u->orders != UNITS_ORDER_NONE) {
    /*
     * First pick cancels orders only. Wake through units_wake, not a bare
     * orders=0: boarding parks a passenger at moves 0 as a "skip this
     * one" flag while DOS's own spent byte is still zero, and standing
     * orders parked on a previous turn refund the allotment.
     */
    (void)units_wake(pool, uid);
    return;
  }
  /* Spent-aware: moves holds REMAINING for Euro units but the DOS SPENT
   * byte for natives, so a raw `<= 0` reads a fresh Brave as exhausted
   * (smell audit 2026-09-09 #6). */
  if (units_remaining_mp(pool, uid) <= 0) {
    return; /* canceled but spent — nothing to activate this turn */
  }
  *out_select_id = uid;
  unit_stack_close(dlg);
}

bool unit_stack_handle_input(
  UnitStackPopup* dlg,
  ColonizeUnitPool* pool,
  const ColonizeInputState* input,
  int* out_select_id
) {
  if (!dlg || !dlg->open || !input) {
    return false;
  }
  if (out_select_id) {
    *out_select_id = -1;
  }

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    unit_stack_close(dlg);
    return true;
  }
  if (colonize_key_up(input->last_key) && dlg->selection > 0) {
    dlg->selection--;
    return true;
  }
  if (colonize_key_down(input->last_key) && dlg->selection + 1 < dlg->count) {
    dlg->selection++;
    return true;
  }
  if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
    int sel = -1;
    unit_stack_activate_row(dlg, pool, dlg->selection, &sel);
    if (out_select_id) {
      *out_select_id = sel;
    }
    return true;
  }

  if (input->mouse_left_clicked) {
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    if (!ui_rect_hit(dlg->dialog_x, dlg->dialog_y, dlg->dialog_w, dlg->dialog_h, mx, my)) {
      unit_stack_close(dlg);
      return true;
    }
    const int idx = unit_stack_row_at_y(dlg, my);
    if (idx >= 0) {
      /* port_bugs.md: same rule for every row, preselected or not — ordered rows
       * get their orders canceled first, order-less rows activate. */
      int sel = -1;
      unit_stack_activate_row(dlg, pool, idx, &sel);
      if (out_select_id) {
        *out_select_id = sel;
      }
    }
    return true;
  }

  if (input->mouse_right_clicked) {
    unit_stack_close(dlg);
    return true;
  }

  return true; /* consume while open */
}

void unit_stack_render(
  UnitStackPopup* dlg,
  const ColonizeUnitPool* pool,
  const ColonizeSpriteSheet* icons,
  const ColonizeMsgCatalog* names,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  const ColonizePalette* active_palette,
  const ColonizeCol1Save* col1,
  ColonizeFramebuffer8* framebuffer
) {
  if (!dlg || !dlg->open || !framebuffer || !framebuffer->pixels || !pool) {
    return;
  }

  /*
   * DOS FUN_2b5a_1b5a: FUN_291f_023c(0x800, FONTINTR) box with box[+0x46] = 0,
   * box[+0x4a] = 2 and flags |= 0x82 (unit-icon rows, selection frame); one
   * FUN_291f_0230 (FUN_6f74_0f3c) icon row per unit, no prose and no option rows, and
   * FUN_291f_0224 (FUN_6f74_112a) arms the frame (box[+0x64]). From 0f3c / 14c6 / 201e:
   * row k spans e = 2 + 16k .. e + 16 (max(16, glyph_h) + box[+0x46]); h = 2 * 2 + 0 + 6 +
   * 2 + 16n; w = widest text + 32 (box[+0x48] 2 + icon 16 + text + box[+0x32] 4 + 4 + 6 of
   * frame; 0f3c zeroes box[+0x28], so there is no @width floor); icon at (x + 5, y + 3 + e),
   * text at (x + 25, y + 3 + e + 8 - (glyph_h >> 1)); the selected row gets a colour-15
   * rectangle (x + 4, y + 2 + e) .. (x + 21, y + 19 + e) and no bar.
   */
  const int glyph_h = popup_dos_glyph_h(font);
  int text_w = 0;
  if (font) {
    for (int i = 0; i < dlg->count; ++i) {
      char label[72];
      unit_stack_row_label(pool, names, col1, units_get_const(pool, dlg->ids[i]), label,
                           sizeof(label));
      const int w = font_text_width(font, label);
      if (w > text_w) {
        text_w = w;
      }
    }
  }
  int dialog_w = text_w + 32;
  if (dialog_w > framebuffer->width) {
    dialog_w = framebuffer->width;
  }
  int dialog_h = 12 + 16 * dlg->count;
  if (dialog_h > framebuffer->height) {
    dialog_h = framebuffer->height;
  }
  PopupFrameGeom fr;
  popup_center_frame(framebuffer, dialog_w, dialog_h, wood_tile, colors, &fr);
  dlg->dialog_x = fr.x;
  dlg->dialog_y = fr.y;
  dlg->dialog_w = fr.w;
  dlg->dialog_h = fr.h;
  dlg->list_y0 = fr.y + 5;
  dlg->line_h = 16;

  for (int i = 0; i < dlg->count; ++i) {
    const int row_y = fr.y + 3 + 2 + 16 * i; /* y + 3 + e */
    if (i == dlg->selection) {
      fb_rect_outline(framebuffer, fr.x + 4, row_y - 1, 18, 18, 15, 15);
    }
    const ColonizeUnit* u = units_get_const(pool, dlg->ids[i]);
    const int sprite = u ? units_map_sprite(pool, u->id) : -1;
    if (sprite >= 0 && icons && sprite < icons->sprite_count && u) {
      /* port_bugs.md: rows carry the Orders-Allegiance chrome (FUN_112b_01ba via
       * FUN_281f_02bc), so cancelling a unit's orders on the first click is visible. */
      unit_chrome_blit_unit_for_palette(
        framebuffer,
        font,
        icons,
        sprite,
        fr.x + 5,
        row_y,
        units_display_type_index(pool, u->id),
        u->nation_id,
        unit_chrome_plan_badge_index(u->nation_id, u->col1_ai_plan, u->orders, u->orders),
        false,
        /* Chrome's 4th arm = damaged Artillery (+0x3148 bit7), not aboard. */
        (u->col1_flags15 & 0x80u) != 0,
        /* port_bugs.md: raw nation fill index goes pink for the Dutch under the
         * game palette — remap through the frame's real palette. */
        active_palette
      );
    }
    char label[72];
    unit_stack_row_label(pool, names, col1, u, label, sizeof(label));
    if (font) {
      /* FUN_6f74_201e writes the row text through FUN_6f74_0538. */
      popup_draw_text_markup(
        font, framebuffer, fr.x + 25, row_y + 8 - (glyph_h >> 1), label, text_color, text_color,
        colors, NULL
      );
    }
  }
}
