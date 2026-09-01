#include "core/unit_stack.h"

#include <stdio.h>
#include <string.h>

#include "core/font.h"
#include "core/map_menu.h"
#include "core/ui_colors.h"
#include "core/unit_chrome.h"

void unit_stack_close(UnitStackPopup* dlg) {
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
  const int n = units_collect_tile_stack(pool, x, y, nation_id, dlg->ids, UNITS_TILE_STACK_MAX);
  if (n <= 1) {
    return false;
  }
  dlg->open = true;
  dlg->tile_x = x;
  dlg->tile_y = y;
  dlg->count = n;
  /* DOS opens the picker with the currently active unit's row (the ship you
   * clicked) highlighted, not row 0 (bugs.md loaded-unit activation flow). */
  dlg->selection = 0;
  for (int i = 0; i < n; ++i) {
    if (dlg->ids[i] == pool->selected_id) {
      dlg->selection = i;
      break;
    }
  }
  return true;
}

static int unit_stack_row_at_y(const UnitStackPopup* dlg, int my) {
  if (!dlg || dlg->line_h <= 0 || dlg->count <= 0) {
    return -1;
  }
  if (my < dlg->list_y0) {
    return -1;
  }
  const int idx = (my - dlg->list_y0) / dlg->line_h;
  if (idx < 0 || idx >= dlg->count) {
    return -1;
  }
  return idx;
}

/*
 * bugs.md (two-step, final form): a pick on a row with standing orders only
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
  if (u->orders != 0) {
    /*
     * First pick cancels orders only. Wake through units_wake, not a bare
     * orders=0: boarding parks a passenger at moves_left 0 as a "skip this
     * one" flag while DOS's own spent byte is still zero, and standing
     * orders parked on a previous turn refund the allotment.
     */
    (void)units_wake(pool, uid);
    return;
  }
  if (u->moves_left <= 0) {
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
    if (mx < dlg->dialog_x || my < dlg->dialog_y || mx >= dlg->dialog_x + dlg->dialog_w ||
        my >= dlg->dialog_y + dlg->dialog_h) {
      unit_stack_close(dlg);
      return true;
    }
    const int idx = unit_stack_row_at_y(dlg, my);
    if (idx >= 0) {
      /* bugs.md: same rule for every row, preselected or not — ordered rows
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
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
) {
  if (!dlg || !dlg->open || !framebuffer || !framebuffer->pixels || !pool) {
    return;
  }

  const int icon_slot = 18;
  const int line_h = font ? (font->max_height > icon_slot ? font->max_height + 2 : icon_slot + 2) : icon_slot + 2;
  const int pad_x = 6;
  const int pad_y = 4;
  const int title_h = font ? font->max_height + 2 : 10;
  const int options_h = dlg->count * line_h;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad_y + title_h + options_h + pad_y;
  if (dialog_h < 40) {
    dialog_h = 40;
  }
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }

  int dialog_w = 160;
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  int dialog_x = (framebuffer->width - dialog_w) / 2;
  int dialog_y = (framebuffer->height - dialog_h) / 2;
  if (dialog_y < MAP_MENU_BAR_H + 2) {
    dialog_y = MAP_MENU_BAR_H + 2;
  }

  ColonizePopupColors local_colors;
  if (!colors) {
    popup_colors_from_ui(&local_colors);
    colors = &local_colors;
  }

  int inner_x = 0;
  int inner_y = 0;
  int inner_w = 0;
  int inner_h = 0;
  popup_draw(
    framebuffer, dialog_x, dialog_y, dialog_w, dialog_h, wood_tile, colors, &inner_x, &inner_y, &inner_w, &inner_h
  );

  dlg->dialog_x = dialog_x;
  dlg->dialog_y = dialog_y;
  dlg->dialog_w = dialog_w;
  dlg->dialog_h = dialog_h;
  dlg->line_h = line_h;

  if (inner_w <= 0 || inner_h <= 0) {
    return;
  }

  char title[48];
  snprintf(title, sizeof(title), "Units (%d,%d)", dlg->tile_x, dlg->tile_y);
  if (font) {
    font_draw_text(font, framebuffer, inner_x + pad_x, inner_y + pad_y, title, text_color);
  }

  const int list_y0 = inner_y + pad_y + title_h;
  dlg->list_y0 = list_y0;

  for (int i = 0; i < dlg->count; ++i) {
    const int row_y = list_y0 + i * line_h;
    const bool selected = (i == dlg->selection);
    if (selected) {
      for (int y = row_y - 1; y <= row_y + line_h - 2; ++y) {
        for (int x = inner_x + 1; x < inner_x + inner_w - 1; ++x) {
          if (x >= 0 && y >= 0 && x < framebuffer->width && y < framebuffer->height) {
            framebuffer->pixels[y * framebuffer->width + x] = select_color;
          }
        }
      }
    }

    const ColonizeUnit* u = units_get_const(pool, dlg->ids[i]);
    const int sprite = u ? units_map_sprite(pool, u->id) : -1;
    int text_x = inner_x + pad_x;
    if (sprite >= 0 && icons && sprite < icons->sprite_count && u) {
      /* bugs.md: rows carry the Orders-Allegiance chrome, so cancelling a
       * unit's orders on the first click is visible immediately. */
      unit_chrome_blit_unit_for_palette(
        framebuffer,
        font,
        icons,
        sprite,
        text_x,
        row_y,
        units_display_type_index(pool, u->id),
        u->nation_id,
        u->orders,
        false,
        u->aboard_ship_id >= 0,
        NULL
      );
      const ColonizeSprite* sp = &icons->sprites[sprite];
      text_x += (sp->width > 0 ? sp->width : icon_slot) + 3;
    }

    const char* name = units_display_name(pool, u);
    char label[64];
    if (u && u->aboard_ship_id >= 0) {
      if (u->orders == 1) {
        snprintf(label, sizeof(label), "%s (aboard)", name ? name : "Unit");
      } else {
        snprintf(label, sizeof(label), "%s (ready)", name ? name : "Unit");
      }
    } else {
      snprintf(label, sizeof(label), "%s", name ? name : "Unit");
    }
    if (font) {
      font_draw_text(font, framebuffer, text_x, row_y + 2, label, text_color);
    }
  }
}
