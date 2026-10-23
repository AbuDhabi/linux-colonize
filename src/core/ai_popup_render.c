/*
 * AI popup presentation half — popup geometry, the lazily loaded portrait /
 * KING / MSS / MYR sheets, and ai_popup_render, split out of ai_popup.c
 * 2026-09-16 so the queue side (enqueue / pending / appliers), which the
 * simulation calls, stops referencing popup_ and ss_. Pure move: bodies are
 * byte-identical to their ai_popup.c originals.
 *
 * This file is UI. ai_popup.c is the sim->UI message channel and is SHARED.
 */
#include "core/ai_popup.h"
#include "core/ai_popup_render.h"

#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/ss.h"
#include "core/ui_button.h"
#include "core/ui_colors.h"
#include "platform/platform.h"
#include "platform/diagnostics.h"

int ai_popup_option_at_y(const AiPopupState* st, int mouse_y) {
  if (!st) {
    return -1;
  }
  return popup_row_at_y(st->list_y0, st->line_h, st->current.choice_count, mouse_y);
}

int ai_popup_choice_row_at(const AiPopupState* st, int mouse_x, int mouse_y) {
  if (!st || !st->open || st->current.choice_count <= 0) {
    return -1;
  }
  if (!ui_rect_hit(
        st->dialog_x, st->dialog_y, st->dialog_w, st->dialog_h, mouse_x, mouse_y
      )) {
    return -1;
  }
  return ai_popup_option_at_y(st, mouse_y);
}

bool ai_popup_handle_input(AiPopupState* st, const ColonizeInputState* input) {
  if (!st || !st->open || !input) {
    return false;
  }

  const int is_info = (st->current.kind == AI_POPUP_KIND_OK || st->current.choice_count <= 0);

  if (input->last_key == COLONIZE_KEY_ESCAPE) {
    /* Info: dismiss acknowledged (no invent OK button). Choice: cancel. */
    ai_popup_finish(st, !is_info, is_info ? 0 : -1);
    return true;
  }
  if (!is_info) {
    /* GAME.TXT choice boxes show through FUN_6f74_2580 (FUN_6f74_36ca): Up / Down wrap. */
    if (popup_list_key_step(input->last_key, &st->selection, st->current.choice_count, 0)) {
      return true;
    }
  }
  if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
    if (is_info) {
      ai_popup_finish(st, false, 0);
      return true;
    }
    if (st->selection >= 0 && st->selection < st->current.choice_count) {
      ai_popup_finish(st, false, st->current.choice_ids[st->selection]);
    }
    return true;
  }

  if (input->mouse_left_clicked) {
    if (is_info) {
      /* Any click dismisses body-only dialogs (DOS message-box style). */
      ai_popup_finish(st, false, 0);
      return true;
    }
    /*
     * port_bugs.md: on a CHOICE dialog only a click on one of the option rows is a
     * decision. A click anywhere else — body text, dialog chrome, or outside
     * the dialog entirely — is swallowed and changes nothing (no implicit
     * cancel, no "pick the highlighted row"). Keyboard ESC still cancels.
     */
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    const bool inside_dialog =
      ui_rect_hit(st->dialog_x, st->dialog_y, st->dialog_w, st->dialog_h, mx, my);
    if (inside_dialog) {
      const int idx = ai_popup_option_at_y(st, my);
      if (idx >= 0) {
        st->selection = idx;
        ai_popup_finish(st, false, st->current.choice_ids[idx]);
      }
    }
    return true;
  }

  if (input->mouse_right_clicked) {
    ai_popup_finish(st, !is_info, is_info ? 0 : -1);
    return true;
  }

  return true;
}

/* GAME.TXT common dialog @width (e.g. @LANDFALL); matches cheat_list / new_game. */
#define AI_POPUP_DEFAULT_WIDTH 190
#define AI_POPUP_WRAP_MAX 16

static void ai_popup_fill_row(
  ColonizeFramebuffer8* fb, int x0, int y0, int x1, int y1, uint8_t color
) {
  if (!fb || !fb->pixels || x1 < x0 || y1 < y0) {
    return;
  }
  for (int y = y0; y <= y1; ++y) {
    if (y < 0 || y >= fb->height) {
      continue;
    }
    for (int x = x0; x <= x1; ++x) {
      if (x >= 0 && x < fb->width) {
        fb->pixels[y * fb->width + x] = color;
      }
    }
  }
}

/*
 * Flow-wrap body to pixel max_w (DOS FUN_6f74_1198). The engine lives in
 * popup.c as popup_wrap_text now (audit GL-8 / SC-35 / IN-7) so new_game.c,
 * game_loop.c's europe prose and the name-entry/howmuch prompts can share it;
 * this is only the AI_POPUP_BODY_LEN-strided view of it.
 *
 * out_center, when given, receives one flag per emitted line: true for a
 * GAME.TXT '^^' row, which DOS centres in the dialog (FUN_6f74_1198's flag-1
 * arm). Rows from '^' or '^^' are drawn verbatim — the wrap loop never breaks
 * them, exactly as DOS skips straight past a caret line's text.
 */
static int ai_popup_wrap_body(
  const ColonizeFont* font,
  const char* body,
  char out[][AI_POPUP_BODY_LEN],
  bool* out_center,
  int max_out,
  int max_w
) {
  return popup_wrap_text(
    font, body, out ? out[0] : NULL, AI_POPUP_BODY_LEN, out_center, max_out, max_w
  );
}

/*
 * ---- Chief portraits (P8.6): IND{tribe}A{tier}.SS, lazy ----
 *
 * These sheets (and KING/KING2, MSSn, MYRn) are NOT remapped onto the host
 * screen's palette. Every screen palette DOS shows them over reserves a block
 * of DAC slots as black — TERRAIN.SS leaves 152..251 empty, EUROPE.PIK
 * 120..251 — and each popup sheet ships its own entries for exactly that
 * block, which DOS loads alongside the art. Nearest-colour remapping instead
 * collapsed ~100 private colours onto the ~150 the map already uses, which is
 * what wrecked the King's tax-audience flair (port_bugs.md). The sheets now blit
 * raw and ai_popup_art_palette_merge lends the host palette their block.
 */
static char g_portrait_dir[512];
static ColonizeSpriteSheet g_portrait_sheets[8][4];
static uint8_t g_portrait_state[8][4]; /* 0 untried, 1 loaded, 2 failed */
/* port_bugs.md: the King's tax-audience flair — DS:0x1f5c = 8 loads "KING" and
 * arms a one-shot frame animation (FUN_6f74_0042: first step after 240
 * ticks of the 60.877 Hz clock ≈ 3.9 s, then one frame per 10 ticks ≈
 * 164 ms until the sheet runs out). KING2.SS carries the 8 frames
 * (79x161, same box as KING.SS's static pose). */
static ColonizeSpriteSheet g_king_sheet;
static uint8_t g_king_state; /* 0 untried, 1 loaded, 2 failed */
/*
 * port_bugs.md ("Tax hike popup only renders the king's right arm"): KING2.SS's
 * eight 79x161 frames are ARM OVERLAYS — each is ~5% opaque, all of it inside
 * a (0,9)-(52,82) box — not standalone poses. The full figure lives in
 * KING.SS (one 79x161 sprite, arm at rest); DOS draws that base and composites
 * the animation frame over it in the same box. Drawing KING2 alone left just
 * the forearm floating beside the dialog.
 */
static ColonizeSpriteSheet g_king_base_sheet;
static uint8_t g_king_base_state;
static uint32_t g_popup_now_ms;

void ai_popup_set_now_ms(uint32_t now_ms) {
  g_popup_now_ms = now_ms;
}
/* MSS0..5 / MYR0..3 popup decorations, same lazy load + remap. */
static ColonizeSpriteSheet g_mss_sheets[6];
static uint8_t g_mss_state[6];
static ColonizeSpriteSheet g_myr_sheets[4];
static uint8_t g_myr_state[4];

void ai_popup_set_portrait_source(const char* data_dir) {
  for (int t = 0; t < 8; ++t) {
    for (int a = 0; a < 4; ++a) {
      if (g_portrait_state[t][a] == 1) {
        ss_free(&g_portrait_sheets[t][a]);
      }
      g_portrait_state[t][a] = 0;
      (void)0;
    }
  }
  for (int i = 0; i < 6; ++i) {
    if (g_mss_state[i] == 1) {
      ss_free(&g_mss_sheets[i]);
    }
    g_mss_state[i] = 0;
  }
  for (int i = 0; i < 4; ++i) {
    if (g_myr_state[i] == 1) {
      ss_free(&g_myr_sheets[i]);
    }
    g_myr_state[i] = 0;
  }
  /* The King pair reloads with the rest when the source directory changes. */
  if (g_king_state == 1) {
    ss_free(&g_king_sheet);
  }
  g_king_state = 0;
  if (g_king_base_state == 1) {
    ss_free(&g_king_base_sheet);
  }
  g_king_base_state = 0;
  g_portrait_dir[0] = '\0';
  if (!data_dir) {
    return;
  }
  snprintf(g_portrait_dir, sizeof(g_portrait_dir), "%s", data_dir);
}

/*
 * One lazy load per popup sheet: try once, remember the verdict in *state
 * (0 untried, 1 loaded, 2 failed), hand back the sheet only on success.
 */
static const ColonizeSpriteSheet* ai_popup_lazy_sheet(
  const char* name, ColonizeSpriteSheet* sheet, uint8_t* state
) {
  if (!name || !sheet || !state || !g_portrait_dir[0]) {
    return NULL;
  }
  if (*state == 0) {
    char path[600];
    char err[128];
    *state = 2;
    if (dos_compat_normalize_asset_path(g_portrait_dir, name, path, sizeof(path)) &&
        ss_load(path, sheet, err, sizeof(err))) {
      *state = 1;
    }
  }
  return *state == 1 ? sheet : NULL;
}

static const ColonizeSpriteSheet* ai_popup_portrait_sheet(int tribe, int tier) {
  if (tribe < 0 || tribe > 7 || tier < 0 || tier > 3) {
    return NULL;
  }
  char name[16];
  snprintf(name, sizeof(name), "IND%dA%d.SS", tribe, tier);
  return ai_popup_lazy_sheet(
    name, &g_portrait_sheets[tribe][tier], &g_portrait_state[tribe][tier]
  );
}

static const ColonizeSpriteSheet* ai_popup_king_sheet(void) {
  return ai_popup_lazy_sheet("KING2.SS", &g_king_sheet, &g_king_state);
}

/* The static full-figure King the KING2 frames overlay. */
static const ColonizeSpriteSheet* ai_popup_king_base_sheet(void) {
  return ai_popup_lazy_sheet("KING.SS", &g_king_base_sheet, &g_king_base_state);
}

static const ColonizeSpriteSheet* ai_popup_graphic_sheet(int mss, int myr) {
  /* MYR wins when both latches are set (DOS loads it after MSS). */
  ColonizeSpriteSheet* sheet = NULL;
  uint8_t* state = NULL;
  char name[16];
  if (myr >= 0 && myr <= 3) {
    sheet = &g_myr_sheets[myr];
    state = &g_myr_state[myr];
    snprintf(name, sizeof(name), "MYR%d.SS", myr);
  } else if (mss >= 0 && mss <= 5) {
    sheet = &g_mss_sheets[mss];
    state = &g_mss_state[mss];
    snprintf(name, sizeof(name), "MSS%d.SS", mss);
  } else {
    return NULL;
  }
  return ai_popup_lazy_sheet(name, sheet, state);
}

/*
 * The sheet whose private palette block the open popup needs — the portrait
 * when one is set (KING.SS and KING2.SS share a palette, so either serves),
 * else the MSS/MYR decoration. Same precedence as ai_popup_render's own pick.
 */
static const ColonizeSpriteSheet* ai_popup_art_sheet(const AiPopupRequest* req) {
  if (!req) {
    return NULL;
  }
  const ColonizeSpriteSheet* portrait =
    req->portrait_tribe == 8 ? ai_popup_king_base_sheet()
                             : ai_popup_portrait_sheet(req->portrait_tribe, req->portrait_tier);
  if (!portrait && req->portrait_tribe == 8) {
    portrait = ai_popup_king_sheet();
  }
  if (portrait) {
    return portrait;
  }
  /* Portrait asked for but absent: ai_popup_render falls back to the MSS/MYR
   * decoration in exactly the same way. */
  return ai_popup_graphic_sheet(req->graphic_mss, req->graphic_myr);
}

const ColonizeSpriteSheet* ai_popup_decoration_sheet(int mss, int myr) {
  return ai_popup_graphic_sheet(mss, myr);
}

void ai_popup_sheet_palette_merge(const ColonizeSpriteSheet* art, ColonizePalette* dst) {
  if (!art || !art->has_palette || !dst) {
    return;
  }
  /*
   * DOS installs a loaded picture's palette into a fixed reserved DAC block:
   * every caller of the partial-DAC writer FUN_1c2e_0022(buf, start=AX,
   * count=DX) for picture art passes AX=0x98, DX=0x64 — slots 152..251
   * inclusive (FUN_3f41_0000 report plate at 3f41:0072, OVL06 0x0464, the
   * OVL28 splash loader). The block is written unconditionally, whatever the
   * host screen had there.
   *
   * So slots 152..251 are copied outright, and the rest of the palette is only
   * lent the slots the host leaves black (EUROPE.PIK leaves 120..251 black and
   * the MSS courtiers fill them) — the map's own colours, the animated water
   * ramp at 120..127 included, are never disturbed.
   *
   * The unconditional block matters: TERRAIN.SS's palette carries one stray
   * non-black entry inside it, index 209 = grey (113,113,113), which nothing
   * on the map uses. Under the old black-only rule every popup art sheet that
   * paints with index 209 — all 32 IND{t}A{a} chief portraits, KING*, MSS*,
   * MYR*, SCORE* — showed that grey instead of its own colour (port_bugs.md #415:
   * the Iroquois chief, IND3A*, uses 209 as a dark brown (44,20,16) for ~130
   * px of hair/shadow, so it read as flat grey patches).
   */
  for (int i = 152; i <= 251; ++i) {
    dst->rgb[i][0] = art->palette.rgb[i][0];
    dst->rgb[i][1] = art->palette.rgb[i][1];
    dst->rgb[i][2] = art->palette.rgb[i][2];
  }
  for (int i = 1; i < 256; ++i) {
    if (i >= 152 && i <= 251) {
      continue;
    }
    if (dst->rgb[i][0] || dst->rgb[i][1] || dst->rgb[i][2]) {
      continue;
    }
    dst->rgb[i][0] = art->palette.rgb[i][0];
    dst->rgb[i][1] = art->palette.rgb[i][1];
    dst->rgb[i][2] = art->palette.rgb[i][2];
  }
}

void ai_popup_art_palette_merge(AiPopupState* st, ColonizePalette* dst) {
  if (!st || !st->open || !dst) {
    return;
  }
  ai_popup_sheet_palette_merge(ai_popup_art_sheet(&st->current), dst);
}

/*
 * ai_popup_render, split by concern (port_bugs.md #1107, big-function rule): the
 * stages below run in the original body's order over this ctx. Each field is
 * written by exactly one stage and only read after it.
 */
typedef struct AiPopupRenderCtx {
  AiPopupState* st;
  const AiPopupRequest* req;
  const ColonizeFont* font;
  const ColonizeFont* tiny_font; /* FONTTINY: the "(F1 for Help)" footer face */
  ColonizeFramebuffer8* fb;
  /* measure */
  int glyph_h;
  int line_h;
  int option_h;
  int pad_x;
  int text_max_w;
  int title_h;
  int prose_h; /* title + body lines: FUN_6f74_14c6 prose height */
  int options_h;
  int row_dx; /* box[+0x22]: option text indent past the margin */
  bool help_footer; /* DS:0x1f66 set by the builder */
  char wrapped[AI_POPUP_WRAP_MAX][AI_POPUP_BODY_LEN];
  bool wrapped_center[AI_POPUP_WRAP_MAX];
  int wrapped_count;
  int dialog_w;
  int dialog_h;
  /* pick_portrait */
  const ColonizeSpriteSheet* portrait;
  const ColonizeSpriteSheet* portrait_base;
  int portrait_frame;
  int portrait_w;
  int portrait_h;
  bool portrait_left;
  /* pick_graphic */
  const ColonizeSpriteSheet* graphic;
  /* place */
  int dialog_x;
  int dialog_y;
  int portrait_x;
  int portrait_y;
  int graphic_x;
  int graphic_y;
  /* frame */
  int inner_x;
  int inner_y;
  int inner_w;
  int inner_h;
} AiPopupRenderCtx;

/* Stage 1: row pitches, body wrap, dialog size. */
static void ai_popup_render_measure(AiPopupRenderCtx* c) {
  /*
   * DOS compositor (FUN_6f74_14c6 rects, FUN_6f74_1198 wrap, defaults from
   * FUN_6f74_06d0): the dialog record's content width is @WIDTH (default 80),
   * text wraps inside width − 2·2 (margin +0x48 = 2), the wood frame adds
   * 3 px per side (+0x46/+0x2a = 3), and the outer height is text + 12.
   * Centre = (160 − (w >> 1), 100 − (h >> 1)), clamped to 320×200 — box +0x14 is the
   * width and +0x16 the height, pinned by FUN_6f74_14c6 @ OVL24 0x16cb–0x16f6
   * (auto-place does 160 − (box[0x14] >> 1) and 100 − (box[0x16] >> 1), then clamps
   * against 0x140 and 0xc8).
   *
   * DOS runs TWO row pitches off the same glyph height (FUN_6f74_0f16 =
   * font[0], with the 6-px font counting as 5 unless DS:0x1f8a is set):
   *   - prompt / body lines: glyph_h + 1 — FUN_6f74_1198 @ 0x1234 and 0x124c
   *     advance by `call 0xf16` + `inc ax`.
   *   - option rows (the +0x54 list, which is what FUN_291f_0176 →
   *     FUN_6f74_0a00 appends to): glyph_h + box[+0x46] — FUN_6f74_14c6 @
   *     0x1611–0x1628 (`call 0xf16`, `add ax,[es:bx+0x46]`, `imul box[+0x2]`).
   * box[+0x46] is 3 for every framed dialog: FUN_6f74_06d0 @ 0x078a–0x0799
   * sets it to `(flags & 0x10) ? 0 : 3` — the same flag 0x10 that drops the
   * frame entirely (the King-audience case). The port previously used
   * glyph_h + 1 for both, so every option row sat 2 px tight.
   *
   * The third pitch, glyph_h + box[+0x46] + 5 (= glyph_h + 8, FUN_6f74_14c6 @
   * 0x1649–0x1653), belongs to the +0x60 list — text-entry fields appended by
   * FUN_6f74_0d44 and painted by FUN_6f74_1e14, only ever built when the
   * script parser sees DS:0x2008 set. No ai_popup dialog has such a row.
   */
  c->glyph_h = c->font ? c->font->max_height : 6;
  if (c->glyph_h == 6) {
    c->glyph_h = 5;
  }
  c->line_h = c->glyph_h + 1;
  /* FUN_6f74_14c6: option rows advance by glyph_h + box[+0x46] (= 3). */
  c->option_h = c->glyph_h + 3;
  c->pad_x = 2;
  /*
   * box[+0x22]: FUN_6f74_06d0 default 4; the builders behind these three dialogs set 8
   * right after FUN_291f_0182 (FUN_38fd_4884 raw 64722: Fountain of Youth / Brewster
   * picks; FUN_4345_06d2 raw 73243: Congress). FUN_6f74_0a00 widens the box by it and
   * FUN_6f74_1b7c indents the row text by it (port_bugs.md #1189). The same two
   * builders set DS:0x1f66 before FUN_291f_016a (raw 64747, 73263), so FUN_6f74_14c6
   * adds the footer height and FUN_6f74_1a78 draws it (port_bugs.md #1192).
   */
  switch (c->req->tag) {
    case AI_POPUP_TAG_FOUNTAIN_YOUTH:
    case AI_POPUP_TAG_BREWSTER_PICK:
    case AI_POPUP_TAG_FF_CONGRESS:
      c->row_dx = 8;
      c->help_footer = true;
      break;
    default:
      c->row_dx = 4;
      c->help_footer = false;
      break;
  }
  const int title_gap = c->req->title[0] ? 2 : 0;

  int content_w = c->req->width > 0 ? c->req->width : AI_POPUP_DEFAULT_WIDTH;
  if (content_w + 6 > c->fb->width) {
    content_w = c->fb->width - 6;
  }
  c->text_max_w = content_w - 2 * c->pad_x;

  memset(c->wrapped_center, 0, sizeof(c->wrapped_center));
  c->wrapped_count = 0;
  if (c->req->body[0]) {
    c->wrapped_count = ai_popup_wrap_body(
      c->font, c->req->body, c->wrapped, c->wrapped_center, AI_POPUP_WRAP_MAX,
      c->text_max_w
    );
  }

  /*
   * FUN_6f74_14c6: the box grows to the widest emitted option row (same rule
   * save_load_dialog.c already carries). The prompt is flow-wrapped to the
   * declared @width first (FUN_6f74_1198), so only the single-line option
   * rows widen the frame — a no-op for every dialog whose rows already fit,
   * which is why no existing golden moves. The Congress debate needs it: its
   * DOS rows are "<father> (<category> Adviser)", wider than @WHICHFREEDOM's
   * own @width=190.
   */
  if (c->font) {
    for (int i = 0; i < c->req->choice_count; ++i) {
      const int w =
        popup_markup_text_width(c->font, c->req->choices[i]) + 2 * c->pad_x + c->row_dx;
      if (w > content_w) {
        content_w = w;
      }
    }
    if (content_w + 6 > c->fb->width) {
      content_w = c->fb->width - 6;
    }
  }
  c->dialog_w = content_w + 6;

  c->title_h = c->req->title[0] ? c->line_h + title_gap : 0;
  c->prose_h = c->title_h + c->wrapped_count * c->line_h;
  c->options_h = c->req->choice_count * c->option_h;
  c->dialog_h = popup_dos_list_height(
    c->glyph_h, c->prose_h, c->req->choice_count, c->help_footer, c->glyph_h == 5
  );
  if (c->dialog_h > c->fb->height) {
    c->dialog_h = c->fb->height;
  }
}

/* Stage 2: chief / King portrait sheet, flair frame, size, side. */
static void ai_popup_render_pick_portrait(AiPopupRenderCtx* c) {
  /*
   * Chief portrait (FUN_6f74_14c6 @ DS:0x1f5c ≥ 0, OVL24 0x17a0..0x189c):
   * the sprite stands at the span edge — LEFT for tribes 0/3/5/7 (Inca,
   * Iroquois, Apache, Tupi) and the King (8), RIGHT for the others — the
   * combined span is dialog_w + sprite_w + 6, clamped to 320 and centred on
   * 160. Sprite top = 100 − (sprite_h + 3)/2. The sheet name itself comes
   * from FUN_6f74_0042: DS:0x1f77 holds the literal "IND0A0" and DOS adds
   * the tribe to byte 3 and the alarm quartile (FUN_281f_0a60 →
   * FUN_15dc_00a2 over FUN_281f_030c = DS:0x5b1c alarm vs DS:0x5398) to
   * byte 5; tribe ≥ 8 takes DS:0x1f72 = "KING" instead.
   */
  c->portrait = c->req->portrait_tribe == 8
                 ? ai_popup_king_sheet()
                 : ai_popup_portrait_sheet(c->req->portrait_tribe, c->req->portrait_tier);
  /* KING2 frames are arm overlays; KING.SS is the figure they sit on. */
  c->portrait_base =
    (c->portrait && c->req->portrait_tribe == 8) ? ai_popup_king_base_sheet() : NULL;
  if (c->portrait_base &&
      (c->portrait_base->sprite_count <= 0 || !c->portrait_base->sprites[0].pixels)) {
    c->portrait_base = NULL;
  }
  c->portrait_frame = 0;
  if (c->portrait && c->req->portrait_tribe == 8) {
    /* FUN_6f74_0042 one-shot flair animation: ~3.9 s pause on frame 0,
     * then ~164 ms per frame to the sheet's last (port_bugs.md). */
    if (c->st->king_anim_next_ms == 0) {
      c->st->king_anim_frame = 0;
      c->st->king_anim_next_ms = g_popup_now_ms + 3900u;
    } else if (g_popup_now_ms >= c->st->king_anim_next_ms &&
               c->st->king_anim_frame + 1 < c->portrait->sprite_count) {
      c->st->king_anim_frame++;
      c->st->king_anim_next_ms = g_popup_now_ms + 164u;
    }
    if (c->st->king_anim_frame < c->portrait->sprite_count) {
      c->portrait_frame = c->st->king_anim_frame;
    }
  }
  c->portrait_w = 0;
  c->portrait_h = 0;
  if (c->portrait && c->portrait->sprite_count > c->portrait_frame &&
      c->portrait->sprites[c->portrait_frame].pixels) {
    c->portrait_w = c->portrait->sprites[c->portrait_frame].width;
    c->portrait_h = c->portrait->sprites[c->portrait_frame].height;
  } else {
    c->portrait = NULL;
    c->portrait_base = NULL;
  }
  /* Both sheets share the 79x161 box; take the larger so neither is clipped. */
  if (c->portrait_base) {
    if (c->portrait_base->sprites[0].width > c->portrait_w) {
      c->portrait_w = c->portrait_base->sprites[0].width;
    }
    if (c->portrait_base->sprites[0].height > c->portrait_h) {
      c->portrait_h = c->portrait_base->sprites[0].height;
    }
  }
  const int tribe = c->req->portrait_tribe;
  c->portrait_left =
    c->portrait && (tribe == 0 || tribe == 3 || tribe == 5 || tribe == 7 || tribe == 8);
}

/* Stage 3: MSS/MYR decoration sheet (only without a portrait). */
static void ai_popup_render_pick_graphic(AiPopupRenderCtx* c) {
  /*
   * MSS/MYR decoration (FUN_6f74_14c6 @ DS:0x1f5e/0x1f60 ≥ 0): the sprite
   * stands ABOVE the dialog, its bottom overlapping the dialog top by the
   * sheet's place_offset_y; place_mode 0 = at the dialog's left edge
   * (horizontal overlap place_offset_x), 1 = centred over it, 2 = at the
   * right edge. The pair is vertically centred as a unit; DOS drops the
   * sprite when the combined height reaches 200.
   */
  c->graphic =
    c->portrait ? NULL : ai_popup_graphic_sheet(c->req->graphic_mss, c->req->graphic_myr);
  if (c->graphic && (c->graphic->sprite_count <= 0 || !c->graphic->sprites[0].pixels)) {
    c->graphic = NULL;
  }
  /* Too tall together → no decoration (flag |0x40): decided in ai_popup_render_place. */
}

/* Stage 4: dialog, portrait and decoration positions. */
static void ai_popup_render_place(AiPopupRenderCtx* c) {
  /*
   * FUN_6f74_14c6 plain and MSS/MYR arms (popup_place_dos): 160 − (w >> 1) /
   * 100 − (h >> 1) arithmetic shifts, DOS edge clamps only. The port's old
   * halve-the-margin form sat 1 px up/left of DOS on odd sizes, and its menu-bar
   * floors have no counterpart in 14c6 (port_bugs.md #1186).
   */
  const PopupDosPlace pl =
    popup_place_dos(c->fb->width, c->fb->height, c->dialog_w, c->dialog_h, c->graphic);
  c->dialog_x = pl.x;
  c->dialog_y = pl.y;

  /* Frame = the dialog only (DOS +0x10..+0x16). The portrait is a free-
   * floating decorator beside it: the combined span (dialog + sprite + 6) is
   * centred on 160 (DOS +0x18/+0x1c = the save/clip rect, not the frame),
   * the sprite sits at the span edge and the dialog is pushed past it by
   * sprite_w + 6 (left arm), or the sprite sits dialog_w + 3 past the span
   * start (right arm). The frame never widens to take the sprite in; which
   * of the two wins where they touch is the draw order below (DOS
   * FUN_6f74_248e: portrait under the wood, MSS/MYR over it). */
  c->portrait_x = 0;
  c->portrait_y = 0;
  if (c->portrait) {
    /*
     * DOS [bp-0x18] starts at −3 (OVL24 0x17bc `mov word [bp-0x18],0xfffd`)
     * and only becomes span_w − 323 when the span is clamped to 0x140
     * (0x17d8 `sub ax,0x143`). Both arms subtract it, so the unclamped case
     * adds a 3px gutter — it is not a plain overflow term.
     */
    int shift = -3;
    int span_w = c->dialog_w + c->portrait_w + 6;
    if (span_w > c->fb->width) {
      shift = span_w - (c->fb->width + 3);
      span_w = c->fb->width;
    }
    /* 0x1823/0x1847: `sar ax,1` / `sub ax,0xa0` / `neg ax` = 160 − (span_w >> 1). */
    const int span_x = c->fb->width / 2 - (span_w >> 1);
    if (c->portrait_left) {
      c->portrait_x = span_x;
      c->dialog_x = span_x - shift + (c->portrait_w + 3);
    } else {
      c->dialog_x = span_x;
      c->portrait_x = span_x + c->dialog_w - shift;
    }
    /* 0x1867: 100 − ((sprite_h + 3) >> 1), stored as-is (no menu-bar or edge clamp). */
    c->portrait_y = c->fb->height / 2 - ((c->portrait_h + 3) >> 1);
  }

  c->graphic_x = 0;
  c->graphic_y = 0;
  if (c->graphic && !pl.figure) {
    c->graphic = NULL; /* DOS: too tall together → no decoration (flag |0x40). */
  }
  if (c->graphic) {
    c->graphic_x = pl.fig_x;
    c->graphic_y = pl.fig_y;
  }
}

/* Stage 5: portrait (under), wood frame, content box, decoration (over). */
static void ai_popup_render_frame(
  AiPopupRenderCtx* c, const ColonizeSpriteSheet* wood_tile, const ColonizePopupColors* colors
) {
  ColonizePopupColors local_colors;
  if (!colors) {
    popup_colors_from_ui(&local_colors);
    colors = &local_colors;
  }

  /*
   * Draw order (DOS FUN_6f74_248e, viceroy_unpacked.c:116519):
   *
   *   if (DS:0x1f5c >= 0) thunk_2a1f_0ab6(box);   // FUN_6f74_1ae8 sprite blit
   *   thunk_2a1f_0710(box, +0x10,+0x12,+0x14,+0x16); // FUN_6f74_2278 frame
   *   ... rows / body / options ...
   *   if (DS:0x1f5c <  0) thunk_2a1f_0ab6(box);   // same blit, but LAST
   *
   * So a chief/King portrait (DS:0x1f5c ≥ 0) goes UNDER the wood frame, and
   * an MSS/MYR decoration (0x1f5c < 0, 0x1f5e/0x1f60 set) goes OVER it. The
   * port used to blit both after popup_draw; whenever the span clamp pulls
   * the dialog back into the sprite (`shift` above) that painted the
   * portrait on top of wood DOS would have drawn over it.
   */
  if (c->portrait_base) {
    ss_blit_sprite(c->portrait_base, 0, c->fb, c->portrait_x, c->portrait_y);
  }
  if (c->portrait) {
    ss_blit_sprite(c->portrait, c->portrait_frame, c->fb, c->portrait_x, c->portrait_y);
  }

  c->inner_x = 0;
  c->inner_y = 0;
  c->inner_w = 0;
  c->inner_h = 0;
  popup_draw(
    c->fb,
    c->dialog_x,
    c->dialog_y,
    c->dialog_w,
    c->dialog_h,
    wood_tile,
    colors,
    &c->inner_x,
    &c->inner_y,
    &c->inner_w,
    &c->inner_h
  );
  /* Content box = the dialog part of the frame (text/rows anchor here). */
  c->inner_x = c->dialog_x + POPUP_FRAME_INSET;
  c->inner_y = c->dialog_y + POPUP_FRAME_INSET;
  c->inner_w = c->dialog_w - POPUP_FRAME_INSET * 2;
  c->inner_h = c->dialog_h - POPUP_FRAME_INSET * 2;

  c->st->dialog_x = c->dialog_x;
  c->st->dialog_y = c->dialog_y;
  c->st->dialog_w = c->dialog_w;
  c->st->dialog_h = c->dialog_h;
  /* Hit-testing walks the OPTION rows, so this is their pitch, not the body's. */
  c->st->line_h = c->option_h;

  if (c->graphic) {
    /* After the frame so the figure's head/shoulders overlap the wood top
     * (DOS 248e's second thunk_2a1f_0ab6 arm, DS:0x1f5c < 0). */
    ss_blit_sprite(c->graphic, 0, c->fb, c->graphic_x, c->graphic_y);
  }
}

/* Stage 6: title, body lines, option rows + selection bar. */
static void ai_popup_render_text(
  AiPopupRenderCtx* c, const ColonizePopupColors* colors, uint8_t text_color,
  uint8_t hilite_color, uint8_t select_color
) {
  /* DOS DS:0x1f62 starts 0 per popup and carries across every line the
   * writer emits — title, body, then the option rows (FUN_6f74_0538). */
  bool emph = false;
  int text_y = c->inner_y + 3; /* DOS +0x2c = 3 + 3 */
  if (c->req->title[0] && c->font) {
    popup_draw_text_markup(
      c->font, c->fb, c->inner_x + c->pad_x, text_y, c->req->title, text_color,
      hilite_color, colors, &emph
    );
    text_y += c->title_h;
  }

  for (int i = 0; i < c->wrapped_count; ++i) {
    if (text_y + c->line_h > c->inner_y + c->inner_h - c->options_h) {
      break;
    }
    /* GAME.TXT '^^' rows centre inside the text column (FUN_6f74_1198). */
    int line_x = c->inner_x + c->pad_x;
    if (c->wrapped_center[i]) {
      const int w = popup_markup_text_width(c->font, c->wrapped[i]);
      if (w < c->text_max_w) {
        line_x += (c->text_max_w - w) / 2;
      }
    }
    popup_draw_text_markup(
      c->font, c->fb, line_x, text_y, c->wrapped[i], text_color,
      hilite_color, colors, &emph
    );
    text_y += c->line_h;
  }
  /* FUN_6f74_1b7c: row k's bar tops at list_y0 + k * option_h, its text one px lower
   * (port_bugs.md #1189; the hit test reads the same list_y0 / line_h). */
  c->st->list_y0 = popup_dos_list_y0(c->dialog_y, c->prose_h);
  for (int i = 0; i < c->req->choice_count; ++i) {
    const int row_y = c->st->list_y0 + i * c->option_h + 1;
    if (i == c->st->selection) {
      /*
       * FUN_6f74_1b7c selection bar, OVL24 0x1c5b–0x1c9b. Its x is
       * `[bp-0x8] − box[+0x22] − 1` where [bp-0x8] was built at 0x1b86 as
       * `box[+0x24] + box[+0x48] + box[+0x22]` — the two +0x22 terms cancel
       * exactly, so x = box[+0x24] + box[+0x48] − 1 whatever +0x22 holds.
       * (An earlier reading of "box+0x24 − 5" dropped that cancellation and
       * so appeared to escape the frame; it does not, and it needs no +0x5c
       * header list.) Width = box[+0x20] + 2·(1 − box[+0x48]) = content − 2,
       * height = glyph_h + 2 (`call 0xf16` then `inc ax` twice at 0x1c61).
       * box[+0x24] is the content origin (= inner_x), box[+0x48] = 2, and
       * box[+0x20] is the content width (= inner_w, which is dialog_w − 6).
       */
      ai_popup_fill_row(
        c->fb,
        c->inner_x + c->pad_x - 1,
        row_y - 1,
        c->inner_x + c->pad_x - 1 + (c->inner_w - 2) - 1,
        row_y - 1 + (c->glyph_h + 2) - 1,
        select_color
      );
    }
    if (c->font) {
      popup_draw_text_markup(
        c->font, c->fb, c->inner_x + c->pad_x + c->row_dx, row_y, c->req->choices[i], text_color,
        hilite_color, colors, &emph
      );
    }
  }
  if (c->help_footer) {
    popup_dos_draw_help_footer(
      c->fb, c->tiny_font, c->dialog_x, c->dialog_y, c->dialog_w, c->dialog_h, c->glyph_h == 5,
      reports_misc_display_word(189, ""), hilite_color
    );
  }
}

void ai_popup_render(
  AiPopupState* st,
  const ColonizeFont* font,
  const ColonizeFont* tiny_font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
) {
  if (!st || !st->open || !framebuffer || !framebuffer->pixels) {
    return;
  }

  AiPopupRenderCtx c;
  memset(&c, 0, sizeof(c));
  c.st = st;
  c.req = &st->current;
  c.font = font;
  c.tiny_font = tiny_font;
  c.fb = framebuffer;
  ai_popup_render_measure(&c);
  ai_popup_render_pick_portrait(&c);
  ai_popup_render_pick_graphic(&c);
  ai_popup_render_place(&c);
  ai_popup_render_frame(&c, wood_tile, colors);
  ai_popup_render_text(&c, colors, text_color, hilite_color, select_color);
}

