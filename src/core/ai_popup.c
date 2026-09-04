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
  req->graphic_myr = -1;
  req->kind = kind;
  req->tag = tag;
  /* P11.3: the @width of whatever popup_msg_fill last resolved (0 = default). */
  req->width = popup_msg_take_pending_width();
  /* MSS decoration of that same section (DOS DS:0x1f5e latch; -1 = none). */
  req->graphic_mss = popup_msg_take_pending_graphic();
  /* @default=N pre-highlighted row (0 = first). */
  req->default_choice = popup_msg_take_pending_default();
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

/* LABELS.TXT @MISC 34/35 (DS:0x2dfe/0x2e00); literals until the file loads. */
static char g_colony_event_continue[AI_POPUP_CHOICE_LEN] = "Continue turn.";
static char g_colony_event_zoom[AI_POPUP_CHOICE_LEN] = "Zoom to colony.";

void ai_popup_set_colony_event_labels(const char* continue_label, const char* zoom_label) {
  if (continue_label && continue_label[0]) {
    snprintf(g_colony_event_continue, sizeof(g_colony_event_continue), "%s", continue_label);
  }
  if (zoom_label && zoom_label[0]) {
    snprintf(g_colony_event_zoom, sizeof(g_colony_event_zoom), "%s", zoom_label);
  }
}

bool ai_popup_enqueue_colony_event(AiPopupState* st, int colony_id, const char* body) {
  /* DOS FUN_364b_0000 appends the two rows with ids 1/2 (FUN_281f_0022). */
  const char* labels[2] = {g_colony_event_continue, g_colony_event_zoom};
  const int ids[2] = {1, 2};
  return ai_popup_enqueue_choice_ctx(
    st, AI_POPUP_TAG_COLONY_EVENT, -1, -1, colony_id, NULL, body, labels, ids, 2
  );
}

void ai_popup_colony_zoom_elect(AiPopupState* st, int colony_id) {
  if (!st || colony_id < 0 || colony_id >= 64) {
    return;
  }
  st->colony_zoom_elected |= (uint64_t)1u << colony_id;
}

static bool ai_popup_colony_has_pending(const AiPopupState* st, int colony_id) {
  if (st->open && st->current.tag == AI_POPUP_TAG_COLONY_EVENT &&
      st->current.payload == colony_id) {
    return true;
  }
  if (st->has_result && st->result_tag == AI_POPUP_TAG_COLONY_EVENT &&
      st->result_payload == colony_id) {
    return true;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_COLONY_EVENT && st->queue[i].payload == colony_id) {
      return true;
    }
  }
  return false;
}

int ai_popup_take_colony_zoom(AiPopupState* st) {
  if (!st || st->colony_zoom_elected == 0) {
    return -1;
  }
  for (int c = 0; c < 64; ++c) {
    const uint64_t bit = (uint64_t)1u << c;
    if ((st->colony_zoom_elected & bit) != 0 && !ai_popup_colony_has_pending(st, c)) {
      st->colony_zoom_elected &= ~bit;
      return c;
    }
  }
  return -1;
}

void ai_popup_promote_tag_before(AiPopupState* st, AiPopupTag promote, AiPopupTag before) {
  if (!st || st->queue_count <= 1) {
    return;
  }
  int insert = -1;
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == before) {
      insert = i;
      break;
    }
  }
  if (insert < 0) {
    return;
  }
  for (int i = insert + 1; i < st->queue_count; ++i) {
    if (st->queue[i].tag != promote) {
      continue;
    }
    const AiPopupRequest tmp = st->queue[i];
    for (int j = i; j > insert; --j) {
      st->queue[j] = st->queue[j - 1];
    }
    st->queue[insert] = tmp;
    insert++;
  }
}

bool ai_popup_present_now(AiPopupState* st, AiPopupTag tag) {
  if (!st || st->open || st->has_result || st->queue_count <= 0) {
    return false;
  }
  int at = -1;
  for (int i = st->queue_count - 1; i >= 0; --i) {
    if (st->queue[i].tag == tag) {
      at = i;
      break;
    }
  }
  if (at < 0) {
    return false;
  }
  const AiPopupRequest req = st->queue[at];
  for (int j = at; j > 0; --j) {
    st->queue[j] = st->queue[j - 1];
  }
  st->queue[0] = req;
  const uint64_t saved_zoom = st->colony_zoom_elected;
  st->colony_zoom_elected = 0; /* player-initiated: never held by a zoom batch */
  const bool ok = ai_popup_try_present_next(st);
  st->colony_zoom_elected = saved_zoom;
  return ok;
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
  int pick = 0;
  /*
   * Zoom elected: DOS FUN_364b_0688 is per-colony BLOCKING — the elected
   * colony's remaining messages and its colony screen finish before anything
   * else (other colonies' chrome, king dialogs, …) can appear. Present only
   * the elected colony's own batch; everything else holds until
   * ai_popup_take_colony_zoom hands the colony to game_loop (which keeps the
   * hold up while the zoomed colony screen is open).
   */
  if (st->colony_zoom_elected != 0) {
    pick = -1;
    for (int i = 0; i < st->queue_count; ++i) {
      const AiPopupRequest* q = &st->queue[i];
      if (q->tag == AI_POPUP_TAG_COLONY_EVENT && q->payload >= 0 && q->payload < 64 &&
          (st->colony_zoom_elected & ((uint64_t)1u << q->payload)) != 0) {
        pick = i;
        break;
      }
    }
    if (pick < 0) {
      return false;
    }
  }
  st->current = st->queue[pick];
  for (int i = pick + 1; i < st->queue_count; ++i) {
    st->queue[i - 1] = st->queue[i];
  }
  st->queue_count--;
  /* DOS FUN_364b_0000: choices only while DS:0xa898 is clear — once zoom is
   * elected, the rest of that colony's batch presents as plain messages. */
  if (st->current.tag == AI_POPUP_TAG_COLONY_EVENT && st->current.payload >= 0 &&
      st->current.payload < 64 &&
      (st->colony_zoom_elected & ((uint64_t)1u << st->current.payload)) != 0) {
    st->current.choice_count = 0;
    st->current.kind = AI_POPUP_KIND_OK;
  }
  st->open = true;
  /* GAME.TXT @default=N (1-based) pre-highlights that row — DOS 6f74 stores
   * the matching option in the box's default slot as it parses. */
  st->selection = 0;
  if (st->current.default_choice > 0 && st->current.default_choice <= st->current.choice_count) {
    st->selection = st->current.default_choice - 1;
  }
  st->has_result = false;
  st->result_cancelled = false;
  st->king_anim_frame = 0;
  st->king_anim_next_ms = 0;
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

void ai_popup_cancel_current(AiPopupState* st) {
  if (!st || !st->open) {
    return;
  }
  const int is_info = (st->current.kind == AI_POPUP_KIND_OK || st->current.choice_count <= 0);
  ai_popup_finish(st, !is_info, is_info ? 0 : -1);
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
    /*
     * bugs.md: on a CHOICE dialog only a click on one of the option rows is a
     * decision. A click anywhere else — body text, dialog chrome, or outside
     * the dialog entirely — is swallowed and changes nothing (no implicit
     * cancel, no "pick the highlighted row"). Keyboard ESC still cancels.
     */
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    const bool inside_dialog =
      mx >= st->dialog_x && my >= st->dialog_y && mx < st->dialog_x + st->dialog_w &&
      my < st->dialog_y + st->dialog_h;
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

    const int word_w = popup_markup_text_width(font, word);
    if (accum[0]) {
      const int space_w = font_text_width(font, " ");
      if (popup_markup_text_width(font, accum) + space_w + word_w > max_w) {
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
  /* The King pair is remapped against the popup palette like the rest, so it
   * has to be dropped when the source/palette changes. */
  if (g_king_state == 1) {
    ss_free(&g_king_sheet);
  }
  g_king_state = 0;
  if (g_king_base_state == 1) {
    ss_free(&g_king_base_sheet);
  }
  g_king_base_state = 0;
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
  /* tribe 8 = the King (DS:0x1f5c = 8 → KING.SS/KING2.SS animated flair);
   * the old > 7 guard silently dropped the tax-audience King portrait. */
  if (!st || st->queue_count <= 0 || tribe > 8) {
    return;
  }
  AiPopupRequest* req = &st->queue[st->queue_count - 1];
  req->portrait_tribe = tribe < 0 ? -1 : tribe;
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

static const ColonizeSpriteSheet* ai_popup_king_sheet(void) {
  if (!g_portrait_dir[0] || !g_portrait_palette_ok) {
    return NULL;
  }
  if (g_king_state == 0) {
    char path[600];
    char err[128];
    g_king_state = 2;
    if (dos_compat_normalize_asset_path(g_portrait_dir, "KING2.SS", path, sizeof(path)) &&
        ss_load(path, &g_king_sheet, err, sizeof(err))) {
      ai_popup_remap_sheet(&g_king_sheet, &g_portrait_palette);
      g_king_state = 1;
    }
  }
  return g_king_state == 1 ? &g_king_sheet : NULL;
}

/* The static full-figure King the KING2 frames overlay. */
static const ColonizeSpriteSheet* ai_popup_king_base_sheet(void) {
  if (!g_portrait_dir[0] || !g_portrait_palette_ok) {
    return NULL;
  }
  if (g_king_base_state == 0) {
    char path[600];
    char err[128];
    g_king_base_state = 2;
    if (dos_compat_normalize_asset_path(g_portrait_dir, "KING.SS", path, sizeof(path)) &&
        ss_load(path, &g_king_base_sheet, err, sizeof(err))) {
      ai_popup_remap_sheet(&g_king_base_sheet, &g_portrait_palette);
      g_king_base_state = 1;
    }
  }
  return g_king_base_state == 1 ? &g_king_base_sheet : NULL;
}

void ai_popup_set_last_graphic_mss(AiPopupState* st, int mss) {
  if (!st || st->queue_count <= 0 || mss > 5) {
    return;
  }
  st->queue[st->queue_count - 1].graphic_mss = mss < 0 ? -1 : mss;
}

void ai_popup_set_last_graphic_myr(AiPopupState* st, int nation) {
  if (!st || st->queue_count <= 0 || nation > 3) {
    return;
  }
  st->queue[st->queue_count - 1].graphic_myr = nation < 0 ? -1 : nation;
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
  if (!g_portrait_dir[0] || !g_portrait_palette_ok) {
    return NULL;
  }
  if (*state == 0) {
    char path[600];
    char err[128];
    *state = 2;
    if (dos_compat_normalize_asset_path(g_portrait_dir, name, path, sizeof(path)) &&
        ss_load(path, sheet, err, sizeof(err))) {
      ai_popup_remap_sheet(sheet, &g_portrait_palette);
      *state = 1;
    }
  }
  return *state == 1 ? sheet : NULL;
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
   * 3 px per side (+0x46/+0x2a = 3), line pitch is glyph height + 1 where the
   * 6-px font counts as 5 unless @SMALLFONT is off (FUN_6f74_0f16), and the
   * outer height is text + 12. Centre = (160 − w/2, 100 − h/2), clamped to
   * 320×200.
   */
  int glyph_h = font ? font->max_height : 6;
  if (glyph_h == 6) {
    glyph_h = 5;
  }
  const int line_h = glyph_h + 1;
  const int pad_x = 2;
  const int title_gap = req->title[0] ? 2 : 0;

  int content_w = req->width > 0 ? req->width : AI_POPUP_DEFAULT_WIDTH;
  if (content_w + 6 > framebuffer->width) {
    content_w = framebuffer->width - 6;
  }
  int dialog_w = content_w + 6;
  const int text_max_w = content_w - 2 * pad_x;

  char wrapped[AI_POPUP_WRAP_MAX][AI_POPUP_BODY_LEN];
  int wrapped_count = 0;
  if (req->body[0]) {
    wrapped_count =
      ai_popup_wrap_body(font, req->body, wrapped, AI_POPUP_WRAP_MAX, text_max_w);
  }

  const int title_h = req->title[0] ? line_h + title_gap : 0;
  const int body_h = wrapped_count * line_h;
  const int options_h = req->choice_count * line_h;
  int dialog_h = 12 + title_h + body_h + options_h;
  if (dialog_h > framebuffer->height) {
    dialog_h = framebuffer->height;
  }

  /*
   * Chief portrait (FUN_6f74_14c6 @ DS:0x1f5c ≥ 0): the sprite stands at the
   * frame edge — LEFT for tribes 0/3/5/7 (Inca, Iroquois, Apache, Tupi) and
   * the King (8), RIGHT for the others — the frame widens by sprite_w + 6 and
   * spans both; the dialog content shifts past the sprite by sprite_w + 3.
   * Sprite top = 100 − (sprite_h + 3)/2; the frame grows to enclose it.
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
   * sprite_w + 3. Sprite is blitted after the frame so it overlaps the wood
   * border rather than widening it. */
  const int frame_h = dialog_h;
  const int frame_w = dialog_w;
  int frame_y; /* = dialog_y, after the MSS/MYR block may move it */
  int frame_x;
  int dialog_x;
  int portrait_x = 0;
  int portrait_y = 0;
  if (portrait) {
    int overflow = 0;
    int span_w = dialog_w + portrait_w + 6;
    if (span_w > framebuffer->width) {
      overflow = span_w - framebuffer->width;
      span_w = framebuffer->width;
    }
    const int span_x = (framebuffer->width - span_w) / 2;
    if (portrait_left) {
      portrait_x = span_x;
      dialog_x = span_x + portrait_w + 3 - overflow;
    } else {
      dialog_x = span_x;
      portrait_x = span_x + dialog_w - overflow;
    }
    portrait_y = (framebuffer->height - (portrait_h + 3)) / 2;
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
  st->line_h = line_h;

  if (portrait_base) {
    ss_blit_sprite(portrait_base, 0, framebuffer, portrait_x, portrait_y);
  }
  if (portrait) {
    ss_blit_sprite(portrait, portrait_frame, framebuffer, portrait_x, portrait_y);
  }
  if (graphic) {
    /* After the frame so the figure's head/shoulders overlap the wood top. */
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
    popup_draw_text_markup(
      font, framebuffer, inner_x + pad_x, text_y, wrapped[i], text_color,
      hilite_color, true, true, &emph
    );
    text_y += line_h;
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
      popup_draw_text_markup(
        font, framebuffer, inner_x + pad_x, row_y, req->choices[i], text_color,
        hilite_color, true, true, &emph
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
