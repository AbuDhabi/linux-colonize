#include "core/unit_stack.h"

#include <stdio.h>
#include <string.h>

#include "core/assets.h"
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
  int n = units_collect_tile_stack(pool, x, y, nation_id, dlg->ids, UNITS_TILE_STACK_MAX);
  if (n <= 1) {
    return false;
  }
  /* bugs.md 252: grid shows up to 3 columns x 8 rows. */
  if (n > 24) {
    n = 24;
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

/* bugs.md 232: expert-skill label for a row (local resolver — some test
 * binaries link unit_stack.c without map_panel.c). NULL when not an expert. */
static const char* unit_stack_profession_label(
  const ColonizeMsgCatalog* names, int type_index, int profession
) {
  if (!units_type_has_profession_slot(type_index)) {
    return NULL;
  }
  if (profession < 0 || profession == UNITS_JOB_NONE || profession == 19 ||
      profession == 25 || profession == 26 || profession == 27) {
    return NULL;
  }
  const ColonizeMsgSection* sec = names ? assets_msg_find(names, "JOB") : NULL;
  if (!sec || profession >= sec->line_count) {
    return NULL;
  }
  const char* p = strchr(sec->lines[profession], ',');
  if (!p) {
    return NULL;
  }
  ++p;
  while (*p == ' ') {
    ++p;
  }
  static char buf[40];
  size_t n = 0;
  while (p[n] && p[n] != ',' && n + 1 < sizeof(buf)) {
    buf[n] = p[n];
    ++n;
  }
  while (n > 0 && buf[n - 1] == ' ') {
    --n;
  }
  buf[n] = '\0';
  return buf[0] ? buf : NULL;
}

/* bugs.md 266: one row's full label ("Dragoon (Expert Farmers) (aboard)"). */
static void unit_stack_row_label(
  const ColonizeUnitPool* pool,
  const ColonizeMsgCatalog* names,
  const ColonizeUnit* u,
  char* out,
  size_t out_size
) {
  const char* name = units_display_name(pool, u);
  /* bugs.md 232: cross-specialized soldiers/dragoons carry their expert
   * skill in the row name — "Dragoon (Expert Farmers)". */
  const char* prof =
    (u && names) ? unit_stack_profession_label(names, u->type_index, u->profession) : NULL;
  char base[56];
  if (prof && name && strstr(name, prof) == NULL) {
    snprintf(base, sizeof(base), "%s (%s)", name, prof);
  } else {
    snprintf(base, sizeof(base), "%s", name ? name : "Unit");
  }
  if (u && u->aboard_ship_id >= 0) {
    snprintf(out, out_size, "%s (%s)", base, u->orders == 1 ? "aboard" : "ready");
  } else {
    snprintf(out, out_size, "%s", base);
  }
}

/* bugs.md 266: crop a label to max_w pixels (drop trailing chars). */
static void unit_stack_crop_label(const ColonizeFont* font, char* label, int max_w) {
  if (!font || max_w <= 0) {
    return;
  }
  size_t n = strlen(label);
  while (n > 0 && font_text_width(font, label) > max_w) {
    label[--n] = '\0';
  }
}

/* bugs.md 252: grid hit-test — column-major fill (first 8 in column 0, ...). */
static int unit_stack_row_at_y(const UnitStackPopup* dlg, int mx, int my) {
  if (!dlg || dlg->line_h <= 0 || dlg->count <= 0 || dlg->rows <= 0) {
    return -1;
  }
  if (my < dlg->list_y0 || mx < dlg->list_x0) {
    return -1;
  }
  const int row = (my - dlg->list_y0) / dlg->line_h;
  const int col = dlg->col_w > 0 ? (mx - dlg->list_x0) / dlg->col_w : 0;
  if (row < 0 || row >= dlg->rows || col < 0 || col >= dlg->cols) {
    return -1;
  }
  const int idx = col * dlg->rows + row;
  if (idx >= dlg->count) {
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
  /* bugs.md 252: left/right hop a full grid column. */
  if (colonize_key_left(input->last_key) && dlg->rows > 0 && dlg->selection - dlg->rows >= 0) {
    dlg->selection -= dlg->rows;
    return true;
  }
  if (colonize_key_right(input->last_key) && dlg->rows > 0 && dlg->selection + dlg->rows < dlg->count) {
    dlg->selection += dlg->rows;
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
    const int idx = unit_stack_row_at_y(dlg, mx, my);
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
  const ColonizeMsgCatalog* names,
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
  /* bugs.md 252: 8 rows per column, columns added as needed (max 3 / 24). */
  const int rows_per_col = 8;
  int cols = (dlg->count + rows_per_col - 1) / rows_per_col;
  if (cols < 1) {
    cols = 1;
  }
  if (cols > 3) {
    cols = 3;
  }
  const int rows = cols > 1 ? rows_per_col : dlg->count;
  /*
   * bugs.md 266: size each column to its widest row instead of a fixed
   * 148px — with the labels cropped to whatever per-column budget still
   * lets every column fit on screen.
   */
  const int col_w_cap =
    (framebuffer->width - 8 - POPUP_FRAME_INSET * 2 - pad_x * 2) / (cols > 0 ? cols : 1);
  int col_w = 60;
  if (font) {
    for (int i = 0; i < dlg->count; ++i) {
      const ColonizeUnit* u = units_get_const(pool, dlg->ids[i]);
      int icon_adv = icon_slot + 3;
      const int sprite = u ? units_map_sprite(pool, u->id) : -1;
      if (sprite >= 0 && icons && sprite < icons->sprite_count) {
        const int sw = icons->sprites[sprite].width;
        icon_adv = (sw > 0 ? sw : icon_slot) + 3;
      }
      char label[72];
      unit_stack_row_label(pool, names, u, label, sizeof(label));
      const int need = (pad_x - 1) + icon_adv + font_text_width(font, label) + 4;
      if (need > col_w) {
        col_w = need;
      }
    }
  } else {
    col_w = 148;
  }
  if (col_w > col_w_cap) {
    col_w = col_w_cap;
  }
  const int options_h = rows * line_h;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad_y + title_h + options_h + pad_y;
  if (dialog_h < 40) {
    dialog_h = 40;
  }
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }

  int dialog_w = POPUP_FRAME_INSET * 2 + pad_x + cols * col_w + pad_x;
  /* Title must still fit ("Units (nnn,nnn)"). */
  if (font) {
    const int title_need = POPUP_FRAME_INSET * 2 + pad_x * 2 + 70;
    if (dialog_w < title_need) {
      dialog_w = title_need;
    }
  }
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
  dlg->list_x0 = inner_x + 1;
  dlg->col_w = cols > 1 ? col_w : inner_w;
  dlg->cols = cols;
  dlg->rows = rows;

  for (int i = 0; i < dlg->count; ++i) {
    const int gcol = rows > 0 ? i / rows : 0;
    const int grow = rows > 0 ? i % rows : i;
    const int col_x0 = dlg->list_x0 + gcol * dlg->col_w;
    const int row_y = list_y0 + grow * line_h;
    const bool selected = (i == dlg->selection);
    if (selected) {
      const int sel_x1 = cols > 1 ? col_x0 + dlg->col_w - 1 : inner_x + inner_w - 1;
      for (int y = row_y - 1; y <= row_y + line_h - 2; ++y) {
        for (int x = col_x0; x < sel_x1; ++x) {
          if (x >= 0 && y >= 0 && x < framebuffer->width && y < framebuffer->height) {
            framebuffer->pixels[y * framebuffer->width + x] = select_color;
          }
        }
      }
    }

    const ColonizeUnit* u = units_get_const(pool, dlg->ids[i]);
    const int sprite = u ? units_map_sprite(pool, u->id) : -1;
    int text_x = col_x0 + pad_x - 1;
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

    char label[72];
    unit_stack_row_label(pool, names, u, label, sizeof(label));
    if (font) {
      /* bugs.md 266: crop instead of spilling into the next column. */
      unit_stack_crop_label(font, label, col_x0 + dlg->col_w - 2 - text_x);
      font_draw_text(font, framebuffer, text_x, row_y + 2, label, text_color);
    }
  }
}
