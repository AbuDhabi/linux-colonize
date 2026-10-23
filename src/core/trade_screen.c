#include "core/trade_screen.h"

#include <stdio.h>
#include <string.h>

#include "core/fb.h"
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
/* DOS column order (port_bugs.md #1206): FUN_647e_1064 sends 0x72 < mx <= 0xc5 to
 * FUN_647e_0f2c(0) = the unload list (stop slots 0..5, @CARGOUNLOAD DS:0x1d47, strip from 0x7d)
 * and mx > 0xc5 to 0f2c(1) = the load list (slots 6..11, @CARGOLOAD DS:0x1d3d, strip from
 * 0xd0); FUN_647e_09da heads them @ROUTE 6 (DS:0x93ea) at 0x7d and @ROUTE 7 (DS:0x93ec) at 0xd0. */
#define TRADE_COL_MID_X1 197 /* 0xc5 — rule between the two cargo columns */
#define TRADE_UNLOAD_X 125   /* 0x7d — left cargo strip / header x */
#define TRADE_LOAD_X 208     /* 0xd0 — right cargo strip / header x */
#define TRADE_ICON_GAP 2

/* LABELS.TXT line, else the literal fallback (audit SC-14 — the same
 * "section line idx or fallback" lookup five modules used to each own). */
static void trade_label(
  char* dst,
  size_t dst_size,
  const ColonizeMsgCatalog* labels,
  const char* section,
  int line,
  const char* fallback
) {
  str_copy_trunc(dst, dst_size, assets_msg_line_or(labels, section, line, fallback));
}

void trade_screen_init(TradeScreen* ts, const ColonizeMsgCatalog* labels) {
  if (!ts) {
    return;
  }
  memset(ts, 0, sizeof(*ts));
  ts->route = -1;
  trade_label(ts->lab_title, sizeof(ts->lab_title), labels, "ROUTE", 0, "");
  trade_label(ts->lab_name, sizeof(ts->lab_name), labels, "ROUTE", 1, "");
  trade_label(ts->lab_type, sizeof(ts->lab_type), labels, "ROUTE", 2, "");
  trade_label(ts->lab_sea, sizeof(ts->lab_sea), labels, "ROUTE", 3, "");
  trade_label(ts->lab_land, sizeof(ts->lab_land), labels, "ROUTE", 4, "");
  trade_label(ts->lab_dest, sizeof(ts->lab_dest), labels, "ROUTE", 5, "");
  trade_label(ts->lab_unload, sizeof(ts->lab_unload), labels, "ROUTE", 6, "");
  trade_label(ts->lab_load, sizeof(ts->lab_load), labels, "ROUTE", 7, "");
  trade_label(
    ts->lab_delete, sizeof(ts->lab_delete), labels, "ROUTE", 8, ""
  );
  trade_label(ts->lab_ok, sizeof(ts->lab_ok), labels, "MISC", 46, "");
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
    return (europe_label && europe_label[0]) ? europe_label : "";
  }
  /* Stops hold DOS colony indices (colonies_abandon keeps them in step). */
  const ColonizeColony* c =
    colonies ? colonies_get(colonies, colonies_id_at_dos_index(colonies, (int)colony_index)) : NULL;
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
 *
 * Smell audit #92 (REFUTED, 2026-09-09): the strip x is inset from the column
 * boundary — the left column is mx 115..197 (FUN_647e_1064: `0x72 < mx`,
 * `0xc5 < mx`) while its icons start at 125, and the right column is mx >= 198
 * with icons from 208 — so a click in that 10px gap left of the first icon
 * reports slot 0. That is DOS's own behaviour, not a port defect:
 * FUN_647e_0f2c (viceroy_unpacked.c 102995-103003) seeds the running edge
 * with exactly those strip x values (`(-(param_1 == 0) & 0xffad) + 0xd0` =
 * 0x7d / 0xd0) and then only ever tests `mouse_x < edge`, with no
 * lower-bound guard of its own. Do not "fix" it.
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
      /* Destination column: edit an existing stop; any empty row appends
       * (DOS FUN_647e_0dd4 else-arm, raw 102922: row = stop count). */
      ts->request = TRADE_SCREEN_REQ_DEST;
      ts->request_stop = row < (int)r->dest_count ? row : (int)r->dest_count;
      return true;
    }
    if (row >= (int)r->dest_count) {
      return true; /* cargo columns need an existing stop */
    }
    const bool is_load = mx > TRADE_COL_MID_X1;
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

/* FUN_647e_09da writes every string through FUN_281f_013c / 0100 -> FUN_104b_0288 / 0318
 * (raw 977 / 1007) with ink literal 0x0f: one FONTTINY blit through {skip, c, c, c}, no
 * shadow pass. Returns the end x, like FUN_1c11_000c. */
#define TRADE_INK 0x0f
static int trade_text(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, int x, int y, const char* s
) {
  font_draw_text_shaded(font, fb, x, y, s, (const uint8_t[4]){0, TRADE_INK, TRADE_INK, TRADE_INK});
  return x + font_text_width(font, s);
}

/* FUN_104b_0318: x + (w >> 1) - (FUN_104b_0216 (width - 1) >> 1), clamped >= 0. */
static void trade_text_centred(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, int x, int w, int y, const char* s
) {
  x += (w >> 1) - ((font_text_width(font, s) - 1) >> 1);
  trade_text(font, fb, x < 0 ? 0 : x, y, s);
}

void trade_screen_render(
  const TradeScreen* ts,
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  const char* europe_label,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* framebuffer
) {
  if (!ts || !ts->open || !framebuffer || !col1 || ts->route < 0 ||
      ts->route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  const ColonizeCol1TradeRoute* r = &col1->trade_route[ts->route];
  /* 647e:09f1 FUN_281f_0484: the whole surface in colour 0x22. */
  fb_fill_rect(framebuffer, 0, 0, framebuffer->width, framebuffer->height, 0x22);
  if (!font) {
    return;
  }
  const int h = font->max_height; /* *(byte*)*(DS:0x89e), FONTTINY's first byte */
  char line[96];

  /* 647e:0a46: @ROUTE 0 + " " + route number, centred over x 0, w 0x140, y 5. */
  snprintf(line, sizeof(line), "%s %d", ts->lab_title, ts->route + 1);
  trade_text_centred(font, framebuffer, 0, 0x140, 5, line);
  /* 647e:0a7e / 0a96: @ROUTE 1 + " " at (0x0a, 0x19), the route name at its end x. */
  snprintf(line, sizeof(line), "%s ", ts->lab_name);
  const int value_x = trade_text(font, framebuffer, 0x0a, 0x19, line);
  trade_text(font, framebuffer, value_x, 0x19, r->name);
  /* 647e:0ad9 / 0b15: @ROUTE 2 + " " at (0x0a, h + 0x1b); Sea / Land at the route name's x. */
  snprintf(line, sizeof(line), "%s ", ts->lab_type);
  trade_text(font, framebuffer, 0x0a, h + 0x1b, line);
  trade_text(font, framebuffer, value_x, h + 0x1b, r->sea ? ts->lab_sea : ts->lab_land);

  /* Column headers at y 0x37 - h (647e:0b54 / 0b71 / 0b8f): x = FUN_104b_0216("0.  "
   * DS:0x1d38) + 0x0a, 0x7d, 0xd0. */
  trade_text(font, framebuffer, font_text_width(font, "0.  ") - 1 + 0x0a, 0x37 - h, ts->lab_dest);
  trade_text(font, framebuffer, TRADE_LOAD_X, 0x37 - h, ts->lab_load);
  trade_text(font, framebuffer, TRADE_UNLOAD_X, 0x37 - h, ts->lab_unload);

  /* Grid in colour 0: 5 rows x 0..0x13f at y 0x3d + 0x14 i (647e:0bc2 FUN_291f_08bc), column
   * rules at x 0x73 / 0xc6 over y 0x3d..0x8d (647e:0bef / 0c0e FUN_291f_08b2). */
  for (int i = 0; i <= TRADE_ROW_COUNT; ++i) {
    fb_hline(framebuffer, TRADE_ROW_Y0 + i * TRADE_ROW_H, 0, 0x13f, 0);
  }
  fb_vline(framebuffer, 0x73, TRADE_ROW_Y0, TRADE_ROW_Y0 + 0x50, 0);
  fb_vline(framebuffer, 0xc6, TRADE_ROW_Y0, TRADE_ROW_Y0 + 0x50, 0);

  /* Stop rows (647e:0d44): stop number + ".  " (DS:0x58) + stop name at (0x0a, row + 8);
   * the cargo icons at row + 5. */
  for (int i = 0; i < (int)r->dest_count && i < TRADE_ROW_COUNT; ++i) {
    const ColonizeCol1TradeStop* st = &r->stop[i];
    const int row_y = TRADE_ROW_Y0 + i * TRADE_ROW_H;
    snprintf(
      line, sizeof(line), "%d.  %s", i + 1,
      trade_screen_stop_label(colonies, st->colony_index, europe_label)
    );
    trade_text(font, framebuffer, 0x0a, row_y + 8, line);
    trade_draw_cargo_strip(st, false, icons, framebuffer, row_y + 5);
    trade_draw_cargo_strip(st, true, icons, framebuffer, row_y + 5);
  }

  /* OK button (647e:0d84 FUN_281f_00ce): outline x 0x118..0x135, y 0xaa..0xbd in colour 0;
   * LABELS @MISC 46 (DS:0x2e16) centred over x 0x118 w 0x1e at y 0xb4 - (h >> 1). */
  fb_rect_outline(framebuffer, 0x118, 0xaa, 0x135 - 0x118 + 1, 0xbd - 0xaa + 1, 0, 0);
  trade_text_centred(font, framebuffer, 0x118, 0x1e, 0xb4 - (h >> 1), ts->lab_ok);
}
