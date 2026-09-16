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
#include "core/map_menu.h"
#include "core/popup.h"
#include "core/popup_msg.h"
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
    if (colonize_key_up(input->last_key) && st->selection > 0) {
      st->selection--;
      return true;
    }
    if (colonize_key_down(input->last_key) &&
        st->selection + 1 < st->current.choice_count) {
      st->selection++;
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
     * bugs.md: on a CHOICE dialog only a click on one of the option rows is a
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
 * what wrecked the King's tax-audience flair (bugs.md). The sheets now blit
 * raw and ai_popup_art_palette_merge lends the host palette their block.
 */
static char g_portrait_dir[512];
static ColonizeSpriteSheet g_portrait_sheets[8][4];
static uint8_t g_portrait_state[8][4]; /* 0 untried, 1 loaded, 2 failed */
/* bugs.md: the King's tax-audience flair — DS:0x1f5c = 8 loads "KING" and
 * arms a one-shot frame animation (FUN_6f74_0042: first step after 240
 * ticks of the 60.877 Hz clock ≈ 3.9 s, then one frame per 10 ticks ≈
 * 164 ms until the sheet runs out). KING2.SS carries the 8 frames
 * (79x161, same box as KING.SS's static pose). */
static ColonizeSpriteSheet g_king_sheet;
static uint8_t g_king_state; /* 0 untried, 1 loaded, 2 failed */
/*
 * bugs.md ("Tax hike popup only renders the king's right arm"): KING2.SS's
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
   * MYR*, SCORE* — showed that grey instead of its own colour (bugs.md #415:
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

void ai_popup_render(
  AiPopupState* st,
  const ColonizeFont* font,
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

  const AiPopupRequest* req = &st->current;
  /*
   * DOS compositor (FUN_6f74_14c6 rects, FUN_6f74_1198 wrap, defaults from
   * FUN_6f74_06d0): the dialog record's content width is @WIDTH (default 80),
   * text wraps inside width − 2·2 (margin +0x48 = 2), the wood frame adds
   * 3 px per side (+0x46/+0x2a = 3), and the outer height is text + 12.
   * Centre = (160 − w/2, 100 − h/2), clamped to 320×200 — box +0x14 is the
   * width and +0x16 the height, pinned by FUN_6f74_14c6 @ OVL24 0x16cb–0x16f6
   * (auto-place does 160 − box[0x14]/2 and 100 − box[0x16]/2, then clamps
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
  int glyph_h = font ? font->max_height : 6;
  if (glyph_h == 6) {
    glyph_h = 5;
  }
  const int line_h = glyph_h + 1;
  /* FUN_6f74_14c6: option rows advance by glyph_h + box[+0x46] (= 3). */
  const int option_h = glyph_h + 3;
  const int pad_x = 2;
  const int title_gap = req->title[0] ? 2 : 0;

  int content_w = req->width > 0 ? req->width : AI_POPUP_DEFAULT_WIDTH;
  if (content_w + 6 > framebuffer->width) {
    content_w = framebuffer->width - 6;
  }
  const int text_max_w = content_w - 2 * pad_x;

  char wrapped[AI_POPUP_WRAP_MAX][AI_POPUP_BODY_LEN];
  bool wrapped_center[AI_POPUP_WRAP_MAX];
  memset(wrapped_center, 0, sizeof(wrapped_center));
  int wrapped_count = 0;
  if (req->body[0]) {
    wrapped_count = ai_popup_wrap_body(
      font, req->body, wrapped, wrapped_center, AI_POPUP_WRAP_MAX, text_max_w
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
  if (font) {
    for (int i = 0; i < req->choice_count; ++i) {
      const int w = popup_markup_text_width(font, req->choices[i]) + 2 * pad_x;
      if (w > content_w) {
        content_w = w;
      }
    }
    if (content_w + 6 > framebuffer->width) {
      content_w = framebuffer->width - 6;
    }
  }
  int dialog_w = content_w + 6;

  const int title_h = req->title[0] ? line_h + title_gap : 0;
  const int body_h = wrapped_count * line_h;
  const int options_h = req->choice_count * option_h;
  int dialog_h = 12 + title_h + body_h + options_h;
  if (dialog_h > framebuffer->height) {
    dialog_h = framebuffer->height;
  }

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
  const ColonizeSpriteSheet* portrait =
    req->portrait_tribe == 8 ? ai_popup_king_sheet()
                             : ai_popup_portrait_sheet(req->portrait_tribe, req->portrait_tier);
  /* KING2 frames are arm overlays; KING.SS is the figure they sit on. */
  const ColonizeSpriteSheet* portrait_base =
    (portrait && req->portrait_tribe == 8) ? ai_popup_king_base_sheet() : NULL;
  if (portrait_base &&
      (portrait_base->sprite_count <= 0 || !portrait_base->sprites[0].pixels)) {
    portrait_base = NULL;
  }
  int portrait_frame = 0;
  if (portrait && req->portrait_tribe == 8) {
    /* FUN_6f74_0042 one-shot flair animation: ~3.9 s pause on frame 0,
     * then ~164 ms per frame to the sheet's last (bugs.md). */
    if (st->king_anim_next_ms == 0) {
      st->king_anim_frame = 0;
      st->king_anim_next_ms = g_popup_now_ms + 3900u;
    } else if (g_popup_now_ms >= st->king_anim_next_ms &&
               st->king_anim_frame + 1 < portrait->sprite_count) {
      st->king_anim_frame++;
      st->king_anim_next_ms = g_popup_now_ms + 164u;
    }
    if (st->king_anim_frame < portrait->sprite_count) {
      portrait_frame = st->king_anim_frame;
    }
  }
  int portrait_w = 0;
  int portrait_h = 0;
  if (portrait && portrait->sprite_count > portrait_frame &&
      portrait->sprites[portrait_frame].pixels) {
    portrait_w = portrait->sprites[portrait_frame].width;
    portrait_h = portrait->sprites[portrait_frame].height;
  } else {
    portrait = NULL;
    portrait_base = NULL;
  }
  /* Both sheets share the 79x161 box; take the larger so neither is clipped. */
  if (portrait_base) {
    if (portrait_base->sprites[0].width > portrait_w) {
      portrait_w = portrait_base->sprites[0].width;
    }
    if (portrait_base->sprites[0].height > portrait_h) {
      portrait_h = portrait_base->sprites[0].height;
    }
  }
  const bool portrait_left =
    portrait && (req->portrait_tribe == 0 || req->portrait_tribe == 3 || req->portrait_tribe == 5 ||
                 req->portrait_tribe == 7 || req->portrait_tribe == 8);

  /*
   * MSS/MYR decoration (FUN_6f74_14c6 @ DS:0x1f5e/0x1f60 ≥ 0): the sprite
   * stands ABOVE the dialog, its bottom overlapping the dialog top by the
   * sheet's place_offset_y; place_mode 0 = at the dialog's left edge
   * (horizontal overlap place_offset_x), 1 = centred over it, 2 = at the
   * right edge. The pair is vertically centred as a unit; DOS drops the
   * sprite when the combined height reaches 200.
   */
  const ColonizeSpriteSheet* graphic =
    portrait ? NULL : ai_popup_graphic_sheet(req->graphic_mss, req->graphic_myr);
  int graphic_w = 0;
  int graphic_h = 0;
  if (graphic && graphic->sprite_count > 0 && graphic->sprites[0].pixels) {
    graphic_w = graphic->sprites[0].width;
    graphic_h = graphic->sprites[0].height;
  } else {
    graphic = NULL;
  }
  if (graphic) {
    const int ov_y = graphic->place_offset_y > graphic_h ? graphic_h : graphic->place_offset_y;
    if (graphic_h - ov_y + dialog_h >= framebuffer->height) {
      graphic = NULL; /* DOS: too tall together → no decoration (flag |0x40). */
    }
  }

  int dialog_y = (framebuffer->height - dialog_h) / 2;
  if (dialog_y + dialog_h > framebuffer->height) {
    dialog_y = framebuffer->height - dialog_h;
  }
  if (dialog_y < MAP_MENU_BAR_H + 2) {
    dialog_y = MAP_MENU_BAR_H + 2;
  }

  /* Frame = the dialog only (DOS +0x10..+0x16). The portrait is a free-
   * floating decorator beside it: the combined span (dialog + sprite + 6) is
   * centred on 160 (DOS +0x18/+0x1c = the save/clip rect, not the frame),
   * the sprite sits at the span edge and the dialog is pushed past it by
   * sprite_w + 6 (left arm), or the sprite sits dialog_w + 3 past the span
   * start (right arm). The frame never widens to take the sprite in; which
   * of the two wins where they touch is the draw order below (DOS
   * FUN_6f74_248e: portrait under the wood, MSS/MYR over it). */
  const int frame_h = dialog_h;
  const int frame_w = dialog_w;
  int frame_y; /* = dialog_y, after the MSS/MYR block may move it */
  int frame_x;
  int dialog_x;
  int portrait_x = 0;
  int portrait_y = 0;
  if (portrait) {
    /*
     * DOS [bp-0x18] starts at −3 (OVL24 0x17bc `mov word [bp-0x18],0xfffd`)
     * and only becomes span_w − 323 when the span is clamped to 0x140
     * (0x17d8 `sub ax,0x143`). Both arms subtract it, so the unclamped case
     * adds a 3px gutter — it is not a plain overflow term.
     */
    int shift = -3;
    int span_w = dialog_w + portrait_w + 6;
    if (span_w > framebuffer->width) {
      shift = span_w - (framebuffer->width + 3);
      span_w = framebuffer->width;
    }
    /* 0x1823/0x1847: `sar ax,1` / `sub ax,0xa0` / `neg ax` = 160 − span_w/2,
     * an arithmetic halve of the span — not a halve of the leftover margin. */
    const int span_x = framebuffer->width / 2 - (span_w >> 1);
    if (portrait_left) {
      portrait_x = span_x;
      dialog_x = span_x - shift + (portrait_w + 3);
    } else {
      dialog_x = span_x;
      portrait_x = span_x + dialog_w - shift;
    }
    /* 0x1867: 100 − (sprite_h + 3)/2. */
    portrait_y = framebuffer->height / 2 - ((portrait_h + 3) >> 1);
    if (portrait_y < MAP_MENU_BAR_H) {
      portrait_y = MAP_MENU_BAR_H;
    }
    if (portrait_y + portrait_h > framebuffer->height) {
      portrait_y = framebuffer->height - portrait_h;
    }
  } else {
    dialog_x = (framebuffer->width - dialog_w) / 2;
  }

  int graphic_x = 0;
  int graphic_y = 0;
  if (graphic) {
    const int ov_y = graphic->place_offset_y > graphic_h ? graphic_h : graphic->place_offset_y;
    const int total_h = graphic_h - ov_y + dialog_h;
    int top = (framebuffer->height - total_h) / 2;
    /* Keep the whole assembly below the menu bar. */
    if (top < MAP_MENU_BAR_H) {
      top = MAP_MENU_BAR_H;
    }
    if (top + total_h > framebuffer->height) {
      top = framebuffer->height - total_h;
    }
    graphic_y = top;
    dialog_y = top + graphic_h - ov_y;
    if (graphic->place_mode == 1) {
      /* Centred over the (already centred) dialog. */
      graphic_x = (framebuffer->width - graphic_w) / 2;
    } else {
      int ov_x = graphic->place_offset_x;
      int total_w = dialog_w + graphic_w - ov_x;
      if (total_w > framebuffer->width) {
        ov_x += total_w - framebuffer->width; /* DOS widens the overlap. */
        total_w = framebuffer->width;
      }
      const int x0 = (framebuffer->width - total_w) / 2;
      if (graphic->place_mode == 2) {
        dialog_x = x0;
        graphic_x = x0 + dialog_w - ov_x;
      } else {
        graphic_x = x0;
        dialog_x = x0 + graphic_w - ov_x;
      }
    }
  }
  frame_x = dialog_x;
  frame_y = dialog_y;

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
  if (portrait_base) {
    ss_blit_sprite(portrait_base, 0, framebuffer, portrait_x, portrait_y);
  }
  if (portrait) {
    ss_blit_sprite(portrait, portrait_frame, framebuffer, portrait_x, portrait_y);
  }

  int inner_x = 0;
  int inner_y = 0;
  int inner_w = 0;
  int inner_h = 0;
  popup_draw(
    framebuffer,
    frame_x,
    frame_y,
    frame_w,
    frame_h,
    wood_tile,
    colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );
  /* Content box = the dialog part of the frame (text/rows anchor here). */
  inner_x = dialog_x + POPUP_FRAME_INSET;
  inner_y = dialog_y + POPUP_FRAME_INSET;
  inner_w = dialog_w - POPUP_FRAME_INSET * 2;
  inner_h = dialog_h - POPUP_FRAME_INSET * 2;

  st->dialog_x = dialog_x;
  st->dialog_y = dialog_y;
  st->dialog_w = dialog_w;
  st->dialog_h = dialog_h;
  /* Hit-testing walks the OPTION rows, so this is their pitch, not the body's. */
  st->line_h = option_h;

  if (graphic) {
    /* After the frame so the figure's head/shoulders overlap the wood top
     * (DOS 248e's second thunk_2a1f_0ab6 arm, DS:0x1f5c < 0). */
    ss_blit_sprite(graphic, 0, framebuffer, graphic_x, graphic_y);
  }

  /* DOS DS:0x1f62 starts 0 per popup and carries across every line the
   * writer emits — title, body, then the option rows (FUN_6f74_0538). */
  bool emph = false;
  int text_y = inner_y + 3; /* DOS +0x2c = 3 + 3 */
  if (req->title[0] && font) {
    popup_draw_text_markup(
      font, framebuffer, inner_x + pad_x, text_y, req->title, text_color,
      hilite_color, true, true, &emph
    );
    text_y += title_h;
  }

  for (int i = 0; i < wrapped_count; ++i) {
    if (text_y + line_h > inner_y + inner_h - options_h) {
      break;
    }
    /* GAME.TXT '^^' rows centre inside the text column (FUN_6f74_1198). */
    int line_x = inner_x + pad_x;
    if (wrapped_center[i]) {
      const int w = popup_markup_text_width(font, wrapped[i]);
      if (w < text_max_w) {
        line_x += (text_max_w - w) / 2;
      }
    }
    popup_draw_text_markup(
      font, framebuffer, line_x, text_y, wrapped[i], text_color,
      hilite_color, true, true, &emph
    );
    text_y += line_h;
  }
  st->list_y0 = text_y;
  for (int i = 0; i < req->choice_count; ++i) {
    const int row_y = text_y + i * option_h;
    if (i == st->selection) {
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
        framebuffer,
        inner_x + pad_x - 1,
        row_y - 1,
        inner_x + pad_x - 1 + (inner_w - 2) - 1,
        row_y - 1 + (glyph_h + 2) - 1,
        select_color
      );
    }
    if (font) {
      popup_draw_text_markup(
        font, framebuffer, inner_x + pad_x, row_y, req->choices[i], text_color,
        hilite_color, true, true, &emph
      );
    }
  }
}

