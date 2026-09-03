#include "core/trade_screen.h"

#include <stdio.h>
#include <string.h>

#include "core/strutil.h"

/* ICONS.SS cargo strip (same base as colony warehouse / map sidebar). */
#define TRADE_CARGO_ICON_BASE 22

/* DOS FUN_647e_09da / _1064 / _10d2 pixel geometry. */
#define TRADE_ROW_Y0 61      /* 0x3d */
#define TRADE_ROW_H 20       /* 0x14 */
#define TRADE_ROW_COUNT 4
#define TRADE_NAME_Y0 22     /* rename band top (~0x19) */
#define TRADE_NAME_Y1 45     /* rename band bottom (0x1d + 2 lines) */
#define TRADE_EXIT_Y 169     /* 0xa9 — clicks below leave the editor */
#define TRADE_COL_DEST_X1 114   /* 0x72 */
#define TRADE_COL_UNLOAD_X1 197 /* 0xc5 */
#define TRADE_UNLOAD_X 125   /* 0x7d — unload strip / header x */
#define TRADE_LOAD_X 208     /* 0xd0 — load strip / header x */
#define TRADE_ICON_GAP 2

static void trade_label(
  char* dst,
  size_t dst_size,
  const ColonizeMsgSection* sec,
  int line,
  const char* fallback
) {
  if (sec && line >= 0 && line < sec->line_count && sec->lines[line][0] != '\0') {
    str_copy_trunc(dst, dst_size, sec->lines[line]);
  } else {
    str_copy_trunc(dst, dst_size, fallback);
  }
}

void trade_screen_init(TradeScreen* ts, const ColonizeMsgCatalog* labels) {
  if (!ts) {
    return;
  }
  memset(ts, 0, sizeof(*ts));
  ts->route = -1;
  const ColonizeMsgSection* sec = labels ? assets_msg_find(labels, "ROUTE") : NULL;
  trade_label(ts->lab_title, sizeof(ts->lab_title), sec, 0, "EDIT TRADE ROUTE");
  trade_label(ts->lab_name, sizeof(ts->lab_name), sec, 1, "Route Name:");
  trade_label(ts->lab_type, sizeof(ts->lab_type), sec, 2, "Route Type:");
  trade_label(ts->lab_sea, sizeof(ts->lab_sea), sec, 3, "Sea");
  trade_label(ts->lab_land, sizeof(ts->lab_land), sec, 4, "Land");
  trade_label(ts->lab_dest, sizeof(ts->lab_dest), sec, 5, "Destination");
  trade_label(ts->lab_unload, sizeof(ts->lab_unload), sec, 6, "Unload Cargo");
  trade_label(ts->lab_load, sizeof(ts->lab_load), sec, 7, "Load Cargo");
  trade_label(ts->lab_delete, sizeof(ts->lab_delete), sec, 8, "(Delete Destination)");
  const ColonizeMsgSection* misc = labels ? assets_msg_find(labels, "MISC") : NULL;
  trade_label(ts->lab_ok, sizeof(ts->lab_ok), misc, 46, "OK");
}

void trade_screen_open(TradeScreen* ts, int route) {
  if (!ts) {
    return;
  }
  ts->open = true;
  ts->route = route;
  ts->request = TRADE_SCREEN_REQ_NONE;
  ts->request_stop = -1;
  ts->request_slot = -1;
  ts->request_is_load = false;
}

void trade_screen_close(TradeScreen* ts) {
  if (!ts) {
    return;
  }
  ts->open = false;
  ts->route = -1;
  ts->request = TRADE_SCREEN_REQ_NONE;
}

const char* trade_screen_stop_label(
  const ColonizeColonyPool* colonies,
  uint16_t colony_index,
  const char* europe_label
) {
  if (colony_index == 999) {
    return (europe_label && europe_label[0]) ? europe_label : "Europe";
  }
  const ColonizeColony* c = colonies ? colonies_get(colonies, (int)colony_index) : NULL;
  if (c && c->active && c->name[0]) {
    return c->name;
  }
  return "(colony)";
}

static int trade_icon_width(const ColonizeSpriteSheet* icons, int cargo) {
  const int sprite = TRADE_CARGO_ICON_BASE + cargo;
  if (icons && sprite >= 0 && sprite < icons->sprite_count) {
    return icons->sprites[sprite].width;
  }
  return 10;
}

/*
 * Which icon slot a cargo-column click lands on (FUN_647e_0f2c): walk the
 * list summing icon widths (+2 gap) from the column x. Returns the slot hit,
 * or -1 for "past the end" (append).
 */
static int trade_cargo_hit_slot(
  const ColonizeCol1TradeStop* st,
  bool is_load,
  const ColonizeSpriteSheet* icons,
  int mx
) {
  const uint8_t* nib = is_load ? st->load_cargo_nibbles : st->unload_cargo_nibbles;
  const int n = is_load ? (int)st->load_count : (int)st->unload_count;
  int x = is_load ? TRADE_LOAD_X : TRADE_UNLOAD_X;
  for (int i = 0; i < n && i < 6; ++i) {
    const int ct = col1_trade_nibble_cargo(nib, i);
    x += trade_icon_width(icons, ct) + TRADE_ICON_GAP;
    if (mx < x) {
      return i;
    }
  }
  return -1;
}

bool trade_screen_handle_input(
  TradeScreen* ts,
  const ColonizeCol1Save* col1,
  const ColonizeSpriteSheet* icons,
  const ColonizeInputState* input
) {
  if (!ts || !ts->open || !input) {
    return false;
  }
  if (!col1 || ts->route < 0 || ts->route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    ts->request = TRADE_SCREEN_REQ_CLOSE;
    return true;
  }
  const ColonizeCol1TradeRoute* r = &col1->trade_route[ts->route];

  if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_ESCAPE) {
    ts->request = TRADE_SCREEN_REQ_CLOSE;
    return true;
  }
  if (!input->mouse_left_clicked && !input->mouse_right_clicked) {
    return true;
  }
  if (input->mouse_right_clicked) {
    ts->request = TRADE_SCREEN_REQ_CLOSE;
    return true;
  }
  const int mx = input->mouse_x;
  const int my = input->mouse_y;

  /* Title band → rename (DOS FUN_647e_10d2 middle branch). */
  if (my >= TRADE_NAME_Y0 && my < TRADE_NAME_Y1) {
    ts->request = TRADE_SCREEN_REQ_RENAME;
    return true;
  }
  /* Below the table → done (DOS: y > 0xa9 clears the modal latch). */
  if (my >= TRADE_EXIT_Y) {
    ts->request = TRADE_SCREEN_REQ_CLOSE;
    return true;
  }
  /* Stop-row band (DOS FUN_647e_1064). */
  if (my >= TRADE_ROW_Y0 && my < TRADE_ROW_Y0 + TRADE_ROW_H * TRADE_ROW_COUNT) {
    int row = (my - TRADE_ROW_Y0) / TRADE_ROW_H;
    if (row > TRADE_ROW_COUNT - 1) {
      row = TRADE_ROW_COUNT - 1;
    }
    if (mx <= TRADE_COL_DEST_X1) {
      /* Destination column: edit existing stop, or append on the first
       * empty row (row > dest_count is dead space, as in DOS 0dd4). */
      if (row <= (int)r->dest_count) {
        ts->request = TRADE_SCREEN_REQ_DEST;
        ts->request_stop = row;
      }
      return true;
    }
    if (row >= (int)r->dest_count) {
      return true; /* cargo columns need an existing stop */
    }
    const bool is_load = mx > TRADE_COL_UNLOAD_X1;
    const ColonizeCol1TradeStop* st = &r->stop[row];
    const int slot = trade_cargo_hit_slot(st, is_load, icons, mx);
    const int count = is_load ? (int)st->load_count : (int)st->unload_count;
    if (slot >= 0) {
      ts->request = TRADE_SCREEN_REQ_CARGO_REMOVE;
      ts->request_stop = row;
      ts->request_slot = slot;
      ts->request_is_load = is_load;
    } else if (count < 6) {
      ts->request = TRADE_SCREEN_REQ_CARGO_ADD;
      ts->request_stop = row;
      ts->request_is_load = is_load;
    }
    return true;
  }
  return true;
}

static void trade_hline(ColonizeFramebuffer8* fb, int x0, int x1, int y, uint8_t color) {
  if (!fb || y < 0 || y >= fb->height) {
    return;
  }
  if (x0 < 0) {
    x0 = 0;
  }
  if (x1 > fb->width - 1) {
    x1 = fb->width - 1;
  }
  for (int x = x0; x <= x1; ++x) {
    fb->pixels[y * fb->width + x] = color;
  }
}

static void trade_vline(ColonizeFramebuffer8* fb, int x, int y0, int y1, uint8_t color) {
  if (!fb || x < 0 || x >= fb->width) {
    return;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (y1 > fb->height - 1) {
    y1 = fb->height - 1;
  }
  for (int y = y0; y <= y1; ++y) {
    fb->pixels[y * fb->width + x] = color;
  }
}

static void trade_draw_cargo_strip(
  const ColonizeCol1TradeStop* st,
  bool is_load,
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* fb,
  int y
) {
  const uint8_t* nib = is_load ? st->load_cargo_nibbles : st->unload_cargo_nibbles;
  const int n = is_load ? (int)st->load_count : (int)st->unload_count;
  int x = is_load ? TRADE_LOAD_X : TRADE_UNLOAD_X;
  for (int i = 0; i < n && i < 6; ++i) {
    const int ct = col1_trade_nibble_cargo(nib, i);
    const int sprite = TRADE_CARGO_ICON_BASE + ct;
    if (icons && sprite >= 0 && sprite < icons->sprite_count) {
      ss_blit_sprite(icons, sprite, fb, x, y);
      x += icons->sprites[sprite].width + TRADE_ICON_GAP;
    } else {
      x += 10 + TRADE_ICON_GAP;
    }
  }
}

void trade_screen_render(
  const TradeScreen* ts,
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  const char* europe_label,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizeSpriteSheet* icons,
  const ColonizePopupColors* colors,
  ColonizeFramebuffer8* framebuffer
) {
  if (!ts || !ts->open || !framebuffer || !col1 || ts->route < 0 ||
      ts->route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  const ColonizeCol1TradeRoute* r = &col1->trade_route[ts->route];
  ColonizePopupColors cols;
  if (colors) {
    cols = *colors;
  } else {
    popup_colors_from_ui(&cols);
  }
  popup_draw(
    framebuffer, 0, 0, framebuffer->width, framebuffer->height, wood_tile, &cols,
    NULL, NULL, NULL, NULL
  );
  if (!font) {
    return;
  }
  char line[96];

  /* Title, centered. */
  {
    const int w = font_text_width(font, ts->lab_title);
    popup_draw_text_shadowed(
      font, framebuffer, (framebuffer->width - w) / 2, 8, ts->lab_title, 15
    );
  }
  /* Route Name / Route Type (DOS prints the 1-based route number too). */
  snprintf(line, sizeof(line), "%s %s (#%d)", ts->lab_name, r->name, ts->route + 1);
  popup_draw_text_shadowed(font, framebuffer, 10, 25, line, 15);
  snprintf(
    line, sizeof(line), "%s %s", ts->lab_type, r->sea ? ts->lab_sea : ts->lab_land
  );
  popup_draw_text_shadowed(font, framebuffer, 10, 36, line, 15);

  /* Column headers (DOS x: width("0.  ")+10 / 0x7d / 0xd0). */
  popup_draw_text_shadowed(font, framebuffer, 14, 50, ts->lab_dest, 15);
  popup_draw_text_shadowed(font, framebuffer, TRADE_UNLOAD_X, 50, ts->lab_unload, 15);
  popup_draw_text_shadowed(font, framebuffer, TRADE_LOAD_X, 50, ts->lab_load, 15);

  /* Grid: 5 horizontal separators + 2 column rules (FUN_647e_09da). */
  const uint8_t rule = cols.dark;
  for (int i = 0; i <= TRADE_ROW_COUNT; ++i) {
    trade_hline(framebuffer, 6, framebuffer->width - 7, TRADE_ROW_Y0 - 2 + i * TRADE_ROW_H, rule);
  }
  trade_vline(
    framebuffer, TRADE_COL_DEST_X1 + 5, TRADE_ROW_Y0 - 2,
    TRADE_ROW_Y0 - 2 + TRADE_ROW_COUNT * TRADE_ROW_H, rule
  );
  trade_vline(
    framebuffer, TRADE_COL_UNLOAD_X1 + 5, TRADE_ROW_Y0 - 2,
    TRADE_ROW_Y0 - 2 + TRADE_ROW_COUNT * TRADE_ROW_H, rule
  );

  /* Stop rows. */
  for (int i = 0; i < (int)r->dest_count && i < TRADE_ROW_COUNT; ++i) {
    const ColonizeCol1TradeStop* st = &r->stop[i];
    const int row_y = TRADE_ROW_Y0 + i * TRADE_ROW_H;
    snprintf(
      line, sizeof(line), "%d. %s", i + 1,
      trade_screen_stop_label(colonies, st->colony_index, europe_label)
    );
    popup_draw_text_shadowed(font, framebuffer, 6, row_y + 2, line, 15);
    trade_draw_cargo_strip(st, false, icons, framebuffer, row_y + 5);
    trade_draw_cargo_strip(st, true, icons, framebuffer, row_y + 5);
  }
  /* Next free row hint (click to append). */
  if (r->dest_count < TRADE_ROW_COUNT) {
    const int row_y = TRADE_ROW_Y0 + (int)r->dest_count * TRADE_ROW_H;
    snprintf(line, sizeof(line), "%d.", (int)r->dest_count + 1);
    popup_draw_text_shadowed(font, framebuffer, 6, row_y + 2, line, 7);
  }

  /* OK, bottom right (LABELS @MISC 46; DOS draws it at x 0x118). */
  popup_draw_text_shadowed(font, framebuffer, 280, 176, ts->lab_ok, 15);
  trade_hline(framebuffer, 6, framebuffer->width - 7, 189, rule);
}
