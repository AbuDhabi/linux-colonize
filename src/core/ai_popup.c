#include "core/ai_popup.h"

#include <stdio.h>
#include <string.h>

#include "core/map_menu.h"
#include "core/popup_msg.h"
#include "platform/platform.h"

void ai_popup_init(AiPopupState* st) {
  if (!st) {
    return;
  }
  memset(st, 0, sizeof(*st));
}

void ai_popup_clear(AiPopupState* st) {
  ai_popup_init(st);
}

bool ai_popup_enqueue(AiPopupState* st, const AiPopupRequest* req) {
  if (!st || !req || st->queue_count >= AI_POPUP_QUEUE_MAX) {
    return false;
  }
  st->queue[st->queue_count++] = *req;
  return true;
}

static void ai_popup_fill_base(
  AiPopupRequest* req,
  AiPopupKind kind,
  AiPopupTag tag,
  int nation_a,
  int nation_b,
  int payload,
  const char* title,
  const char* body
) {
  memset(req, 0, sizeof(*req));
  req->portrait_tribe = -1;
  req->portrait_tier = -1;
  req->kind = kind;
  req->tag = tag;
  /* P11.3: the @width of whatever popup_msg_fill last resolved (0 = default). */
  req->width = popup_msg_take_pending_width();
  req->nation_a = nation_a;
  req->nation_b = nation_b;
  req->payload = payload;
  if (title) {
    snprintf(req->title, sizeof(req->title), "%s", title);
  }
  if (body) {
    snprintf(req->body, sizeof(req->body), "%s", body);
  }
}

bool ai_popup_enqueue_ok(
  AiPopupState* st,
  AiPopupTag tag,
  const char* title,
  const char* body
) {
  return ai_popup_enqueue_ok_ctx(st, tag, -1, -1, 0, title, body);
}

bool ai_popup_enqueue_ok_ctx(
  AiPopupState* st,
  AiPopupTag tag,
  int nation_a,
  int nation_b,
  int payload,
  const char* title,
  const char* body
) {
  AiPopupRequest req;
  ai_popup_fill_base(&req, AI_POPUP_KIND_OK, tag, nation_a, nation_b, payload, title, body);
  /* DOS info wood: no choice rows in GAME.TXT — click/key dismisses. */
  req.choice_count = 0;
  return ai_popup_enqueue(st, &req);
}

bool ai_popup_enqueue_choice(
  AiPopupState* st,
  AiPopupTag tag,
  const char* title,
  const char* body,
  const char* const* choice_labels,
  const int* choice_ids,
  int choice_count
) {
  return ai_popup_enqueue_choice_ctx(
    st, tag, -1, -1, 0, title, body, choice_labels, choice_ids, choice_count
  );
}

bool ai_popup_enqueue_choice_ctx(
  AiPopupState* st,
  AiPopupTag tag,
  int nation_a,
  int nation_b,
  int payload,
  const char* title,
  const char* body,
  const char* const* choice_labels,
  const int* choice_ids,
  int choice_count
) {
  AiPopupRequest req;
  if (!choice_labels || choice_count <= 0 || choice_count > AI_POPUP_CHOICE_MAX) {
    return false;
  }
  ai_popup_fill_base(
    &req, AI_POPUP_KIND_CHOICE, tag, nation_a, nation_b, payload, title, body
  );
  req.choice_count = choice_count;
  for (int i = 0; i < choice_count; ++i) {
    snprintf(
      req.choices[i],
      sizeof(req.choices[i]),
      "%s",
      choice_labels[i] ? choice_labels[i] : ""
    );
    req.choice_ids[i] = choice_ids ? choice_ids[i] : i;
  }
  return ai_popup_enqueue(st, &req);
}

bool ai_popup_queue_pending(const AiPopupState* st) {
  return st && st->queue_count > 0;
}

bool ai_popup_busy(const AiPopupState* st) {
  return st && (st->open || st->queue_count > 0 || st->has_result);
}

bool ai_popup_try_present_next(AiPopupState* st) {
  if (!st || st->open || st->has_result || st->queue_count <= 0) {
    return false;
  }
  st->current = st->queue[0];
  for (int i = 1; i < st->queue_count; ++i) {
    st->queue[i - 1] = st->queue[i];
  }
  st->queue_count--;
  st->open = true;
  st->selection = 0;
  st->has_result = false;
  st->result_cancelled = false;
  return true;
}

static void ai_popup_finish(AiPopupState* st, bool cancelled, int choice_id) {
  if (!st) {
    return;
  }
  st->has_result = true;
  st->result_cancelled = cancelled;
  st->result_choice_id = choice_id;
  st->result_tag = st->current.tag;
  st->result_nation_a = st->current.nation_a;
  st->result_nation_b = st->current.nation_b;
  st->result_payload = st->current.payload;
  st->open = false;
}

static int ai_popup_option_at_y(const AiPopupState* st, int mouse_y) {
  if (!st || st->line_h <= 0) {
    return -1;
  }
  const int rel = mouse_y - st->list_y0;
  if (rel < 0) {
    return -1;
  }
  const int idx = rel / st->line_h;
  if (idx < 0 || idx >= st->current.choice_count) {
    return -1;
  }
  return idx;
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
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    if (mx < st->dialog_x || my < st->dialog_y || mx >= st->dialog_x + st->dialog_w ||
        my >= st->dialog_y + st->dialog_h) {
      ai_popup_finish(st, true, -1);
      return true;
    }
    const int idx = ai_popup_option_at_y(st, my);
    if (idx >= 0) {
      st->selection = idx;
      ai_popup_finish(st, false, st->current.choice_ids[idx]);
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

/* Nation-wizard style: unbold FONTINTR + black (0) drop-shadow. */
static void ai_popup_draw_shadowed(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  popup_draw_text_shadowed(font, fb, x, y, text, color);
}

/*
 * Flow-wrap body to pixel max_w (DOS FUN_6f74_1198 / new_game_wrap_prompt_flow).
 * Honors embedded '\n'. Returns number of output lines.
 */
static int ai_popup_wrap_body(
  const ColonizeFont* font,
  const char* body,
  char out[][AI_POPUP_BODY_LEN],
  int max_out,
  int max_w
) {
  int count = 0;
  if (!body || !body[0] || max_out <= 0) {
    return 0;
  }
  char accum[AI_POPUP_BODY_LEN];
  accum[0] = '\0';

  const char* p = body;
  while (*p && count < max_out) {
    while (*p == ' ') {
      p++;
    }
    if (!*p) {
      break;
    }
    if (*p == '\n') {
      if (accum[0]) {
        snprintf(out[count], AI_POPUP_BODY_LEN, "%s", accum);
        count++;
        accum[0] = '\0';
        if (count >= max_out) {
          return count;
        }
      } else {
        out[count][0] = '\0';
        count++;
      }
      p++;
      continue;
    }

    const char* start = p;
    while (*p && *p != ' ' && *p != '\n') {
      p++;
    }
    char word[AI_POPUP_BODY_LEN];
    size_t n = (size_t)(p - start);
    if (n >= sizeof(word)) {
      n = sizeof(word) - 1;
    }
    memcpy(word, start, n);
    word[n] = '\0';

    const int word_w = font_text_width(font, word);
    if (accum[0]) {
      const int space_w = font_text_width(font, " ");
      if (font_text_width(font, accum) + space_w + word_w > max_w) {
        snprintf(out[count], AI_POPUP_BODY_LEN, "%s", accum);
        count++;
        accum[0] = '\0';
        if (count >= max_out) {
          return count;
        }
      }
    }
    size_t len = strlen(accum);
    if (accum[0] && len + 1 < sizeof(accum)) {
      accum[len++] = ' ';
      accum[len] = '\0';
    }
    for (const char* w = word; *w && len + 1 < sizeof(accum); ++w) {
      accum[len++] = *w;
    }
    accum[len] = '\0';
  }
  if (accum[0] && count < max_out) {
    snprintf(out[count], AI_POPUP_BODY_LEN, "%s", accum);
    count++;
  }
  return count;
}

/* ---- Chief portraits (P8.6): IND{tribe}A{tier}.SS, lazy, palette-remapped ---- */
static char g_portrait_dir[512];
static ColonizePalette g_portrait_palette;
static bool g_portrait_palette_ok;
static ColonizeSpriteSheet g_portrait_sheets[8][4];
static uint8_t g_portrait_state[8][4]; /* 0 untried, 1 loaded, 2 failed */

int ai_popup_portrait_tier_from_alarm(int alarm) {
  /* FUN_15dc_00a2 */
  if (alarm < 0x19) {
    return 0;
  }
  if (alarm < 0x32) {
    return 1;
  }
  if (alarm < 0x4b) {
    return 2;
  }
  return 3;
}

void ai_popup_set_portrait_source(const char* data_dir, const ColonizePalette* palette) {
  for (int t = 0; t < 8; ++t) {
    for (int a = 0; a < 4; ++a) {
      if (g_portrait_state[t][a] == 1) {
        ss_free(&g_portrait_sheets[t][a]);
      }
      g_portrait_state[t][a] = 0;
    }
  }
  g_portrait_dir[0] = '\0';
  g_portrait_palette_ok = false;
  if (!data_dir || !palette) {
    return;
  }
  snprintf(g_portrait_dir, sizeof(g_portrait_dir), "%s", data_dir);
  g_portrait_palette = *palette;
  g_portrait_palette_ok = true;
}

void ai_popup_set_last_portrait(AiPopupState* st, int tribe, int tier) {
  if (!st || st->queue_count <= 0 || tribe < 0 || tribe > 7) {
    return;
  }
  AiPopupRequest* req = &st->queue[st->queue_count - 1];
  req->portrait_tribe = tribe;
  req->portrait_tier = tier < 0 ? 0 : (tier > 3 ? 3 : tier);
}

/* Nearest-colour remap of the sheet's own palette onto the popup palette
 * (same per-file pattern as reports.c / colony_screen.c / europe.c). */
static void ai_popup_remap_sheet(ColonizeSpriteSheet* sheet, const ColonizePalette* dst) {
  if (!sheet || !dst || !sheet->has_palette) {
    return;
  }
  uint8_t lut[256];
  for (int i = 0; i < 256; ++i) {
    if (i == COLONIZE_SS_TRANSPARENT) {
      lut[i] = (uint8_t)COLONIZE_SS_TRANSPARENT;
      continue;
    }
    const int sr = sheet->palette.rgb[i][0];
    const int sg = sheet->palette.rgb[i][1];
    const int sb = sheet->palette.rgb[i][2];
    int best = 0;
    int best_d = 1 << 30;
    for (int j = 0; j < 256; ++j) {
      if (j == COLONIZE_SS_TRANSPARENT) {
        continue;
      }
      const int dr = sr - dst->rgb[j][0];
      const int dg = sg - dst->rgb[j][1];
      const int db = sb - dst->rgb[j][2];
      const int d = dr * dr + dg * dg + db * db;
      if (d < best_d) {
        best_d = d;
        best = j;
      }
    }
    lut[i] = (uint8_t)best;
  }
  for (int k = 0; k < sheet->sprite_count; ++k) {
    ColonizeSprite* spr = &sheet->sprites[k];
    if (!spr->pixels) {
      continue;
    }
    const int n = spr->width * spr->height;
    for (int q = 0; q < n; ++q) {
      spr->pixels[q] = lut[spr->pixels[q]];
    }
  }
}

static const ColonizeSpriteSheet* ai_popup_portrait_sheet(int tribe, int tier) {
  if (tribe < 0 || tribe > 7 || tier < 0 || tier > 3 || !g_portrait_dir[0] ||
      !g_portrait_palette_ok) {
    return NULL;
  }
  if (g_portrait_state[tribe][tier] == 0) {
    char name[16];
    char path[600];
    char err[128];
    snprintf(name, sizeof(name), "IND%dA%d.SS", tribe, tier);
    g_portrait_state[tribe][tier] = 2;
    if (dos_compat_normalize_asset_path(g_portrait_dir, name, path, sizeof(path)) &&
        ss_load(path, &g_portrait_sheets[tribe][tier], err, sizeof(err))) {
      ai_popup_remap_sheet(&g_portrait_sheets[tribe][tier], &g_portrait_palette);
      g_portrait_state[tribe][tier] = 1;
    }
  }
  return g_portrait_state[tribe][tier] == 1 ? &g_portrait_sheets[tribe][tier] : NULL;
}

void ai_popup_render(
  AiPopupState* st,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t select_color,
  ColonizeFramebuffer8* framebuffer
) {
  if (!st || !st->open || !framebuffer || !framebuffer->pixels) {
    return;
  }

  const AiPopupRequest* req = &st->current;
  const int line_h = font ? (font->max_height + 2) : 8;
  /* Match new_game_render_list_dialog / cheat_list_render padding. */
  const int pad_x = 6;
  const int pad_y = 4;
  const int title_gap = req->title[0] ? 2 : 0;

  int dialog_w = req->width > 0 ? req->width : AI_POPUP_DEFAULT_WIDTH;
  if (dialog_w > framebuffer->width - 8) {
    dialog_w = framebuffer->width - 8;
  }
  const int text_max_w = dialog_w - POPUP_FRAME_INSET * 2 - pad_x * 2;

  char wrapped[AI_POPUP_WRAP_MAX][AI_POPUP_BODY_LEN];
  int wrapped_count = 0;
  if (req->body[0]) {
    wrapped_count =
      ai_popup_wrap_body(font, req->body, wrapped, AI_POPUP_WRAP_MAX, text_max_w);
  }

  const int title_h = req->title[0] ? line_h + title_gap : 0;
  const int body_h = wrapped_count > 0 ? wrapped_count * line_h + 2 : 0;
  const int options_h = req->choice_count * line_h;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad_y + title_h + body_h + options_h + pad_y;
  if (dialog_h < 40) {
    dialog_h = 40;
  }
  if (dialog_h > framebuffer->height - 8) {
    dialog_h = framebuffer->height - 8;
  }

  /* Chief portrait: full-height figure standing left of the dialog; the pair
   * is centred together (DOS meet chrome, FUN_6f74_0042 → compositor). */
  const ColonizeSpriteSheet* portrait =
    ai_popup_portrait_sheet(req->portrait_tribe, req->portrait_tier);
  int portrait_w = 0;
  int portrait_h = 0;
  if (portrait && portrait->sprite_count > 0 && portrait->sprites[0].pixels) {
    portrait_w = portrait->sprites[0].width;
    portrait_h = portrait->sprites[0].height;
    if (portrait_w + 4 + dialog_w > framebuffer->width - 4) {
      portrait = NULL;
      portrait_w = 0;
      portrait_h = 0;
    }
  }
  const int portrait_gap = portrait ? 4 : 0;
  int dialog_x = (framebuffer->width - (dialog_w + portrait_w + portrait_gap)) / 2 + portrait_w +
                 portrait_gap;
  int dialog_y = (framebuffer->height - dialog_h) / 2;
  if (dialog_y < MAP_MENU_BAR_H + 2) {
    dialog_y = MAP_MENU_BAR_H + 2;
  }
  if (dialog_y + dialog_h > framebuffer->height) {
    dialog_y = framebuffer->height - dialog_h;
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
    framebuffer,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    wood_tile,
    colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );

  st->dialog_x = dialog_x;
  st->dialog_y = dialog_y;
  st->dialog_w = dialog_w;
  st->dialog_h = dialog_h;
  st->line_h = line_h;

  if (portrait) {
    int py = (framebuffer->height - portrait_h) / 2;
    if (py < MAP_MENU_BAR_H) {
      py = MAP_MENU_BAR_H;
    }
    if (py + portrait_h > framebuffer->height) {
      py = framebuffer->height - portrait_h;
    }
    ss_blit_sprite(portrait, 0, framebuffer, dialog_x - portrait_gap - portrait_w, py);
  }

  int text_y = inner_y + pad_y;
  if (req->title[0] && font) {
    ai_popup_draw_shadowed(
      font, framebuffer, inner_x + pad_x, text_y, req->title, text_color
    );
    text_y += title_h;
  }

  for (int i = 0; i < wrapped_count; ++i) {
    if (text_y + line_h > inner_y + inner_h - options_h - pad_y) {
      break;
    }
    ai_popup_draw_shadowed(
      font, framebuffer, inner_x + pad_x, text_y, wrapped[i], text_color
    );
    text_y += line_h;
  }
  if (wrapped_count > 0) {
    text_y += 2;
  }

  st->list_y0 = text_y;
  for (int i = 0; i < req->choice_count; ++i) {
    const int row_y = text_y + i * line_h;
    if (i == st->selection) {
      ai_popup_fill_row(
        framebuffer,
        inner_x + 2,
        row_y - 1,
        inner_x + inner_w - 3,
        row_y + line_h - 2,
        select_color
      );
    }
    if (font) {
      ai_popup_draw_shadowed(
        font, framebuffer, inner_x + pad_x, row_y, req->choices[i], text_color
      );
    }
  }
}

void ai_popup_consume_result(AiPopupState* st) {
  if (!st) {
    return;
  }
  st->has_result = false;
  st->result_cancelled = false;
  st->result_choice_id = -1;
}
