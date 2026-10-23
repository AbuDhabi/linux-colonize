#include "core/ai_popup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/font_file.h"
#include "core/ss.h"
#include "platform/platform.h"

#include "../common/test_runner.h"

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_popup: %s\n", msg);
  return 1;
}

static int case_fresh_idle(void) {
  AiPopupState st;
  ai_popup_init(&st);
  if (ai_popup_busy(&st)) {
    return fail("fresh state should be idle");
  }
  return 0;
}

static int case_enqueue_and_present_info(void) {
  AiPopupState st;
  ai_popup_init(&st);
  if (!ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, "Title", "Hello body")) {
    return fail("enqueue_ok failed");
  }
  if (!ai_popup_queue_pending(&st) || st.queue_count != 1) {
    return fail("queue should have 1");
  }
  if (!ai_popup_try_present_next(&st) || !st.open) {
    return fail("present_next failed");
  }
  if (st.queue_count != 0) {
    return fail("queue should drain on present");
  }
  if (strcmp(st.current.body, "Hello body") != 0) {
    return fail("body mismatch");
  }
  if (st.current.choice_count != 0) {
    return fail("info dialog must not invent OK choice rows");
  }
  return 0;
}

/* Body-only dismiss: Enter (any click / Esc / Space same path). */
static int case_dismiss_result(void) {
  AiPopupState st;
  ai_popup_init(&st);
  if (!ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, "Title", "Hello body")) {
    return fail("enqueue_ok failed");
  }
  if (!ai_popup_try_present_next(&st) || !st.open) {
    return fail("present_next failed");
  }

  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  if (!ai_popup_handle_input(&st, &in)) {
    return fail("handle_input should consume");
  }
  if (st.open || !st.has_result || st.result_cancelled || st.result_choice_id != 0) {
    return fail("info dismiss result expected");
  }
  if (st.result_tag != AI_POPUP_TAG_INFO) {
    return fail("tag mismatch");
  }
  ai_popup_consume_result(&st);
  return 0;
}

static int case_choice_flow(void) {
  AiPopupState st;
  ai_popup_init(&st);

  const char* labels[] = {"Accept", "Refuse"};
  const int ids[] = {1, 2};
  if (!ai_popup_enqueue_choice_ctx(
        &st,
        AI_POPUP_TAG_KING_AUDIENCE,
        0,
        1,
        42,
        "Audience",
        "The King demands higher taxes.",
        labels,
        ids,
        2
      )) {
    return fail("enqueue_choice failed");
  }
  ai_popup_try_present_next(&st);
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_DOWN;
  ai_popup_handle_input(&st, &in);
  in.last_key = COLONIZE_KEY_ENTER;
  ai_popup_handle_input(&st, &in);
  if (!st.has_result || st.result_choice_id != 2 || st.result_payload != 42) {
    return fail("Refuse choice result expected");
  }
  ai_popup_consume_result(&st);
  return 0;
}

/* Queue overflow safety: fill then one more fails. */
static int case_queue_overflow(void) {
  AiPopupState st;
  ai_popup_init(&st);
  ai_popup_clear(&st);
  for (int i = 0; i < AI_POPUP_QUEUE_MAX; ++i) {
    if (!ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, NULL, "x")) {
      return fail("fill queue");
    }
  }
  if (ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, NULL, "overflow")) {
    return fail("overflow should fail");
  }
  return 0;
}

/* port_bugs.md #1117: a dropped enqueue must not let set_last_* patch the
 * previous (unrelated) request. */
static int case_set_last_after_dropped_enqueue(void) {
  static AiPopupState st;
  ai_popup_init(&st);
  for (int i = 0; i < AI_POPUP_QUEUE_MAX; ++i) {
    if (!ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, NULL, "x")) {
      return fail("fill queue");
    }
  }
  AiPopupRequest before = st.queue[AI_POPUP_QUEUE_MAX - 1];
  if (ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, NULL, "overflow")) {
    return fail("overflow should fail");
  }
  ai_popup_set_last_portrait(&st, 3, 2);
  ai_popup_set_last_graphic_mss(&st, 1);
  ai_popup_set_last_graphic_myr(&st, 1);
  const AiPopupRequest* last = &st.queue[AI_POPUP_QUEUE_MAX - 1];
  if (last->portrait_tribe != before.portrait_tribe ||
      last->portrait_tier != before.portrait_tier ||
      last->graphic_mss != before.graphic_mss || last->graphic_myr != before.graphic_myr) {
    return fail("set_last_* patched the last real entry after a dropped enqueue");
  }
  return 0;
}

/* Presentation tags: FA / King letter / Congress FF (chrome only). */
static int case_chrome_tags(void) {
  AiPopupState st;
  ai_popup_init(&st);
  ai_popup_clear(&st);
  if (!ai_popup_enqueue_ok_ctx(
        &st, AI_POPUP_TAG_DIPLO_FA, 0, 1, 0, "Foreign Affairs", "Alliance holds.")) {
    return fail("enqueue DIPLO_FA");
  }
  if (!ai_popup_enqueue_ok_ctx(
        &st, AI_POPUP_TAG_KING_LETTER, 0, 1, 0, "United Colonies", "Renamed.")) {
    return fail("enqueue KING_LETTER");
  }
  if (!ai_popup_enqueue_ok_ctx(
        &st, AI_POPUP_TAG_FF_CONGRESS, 0, -1, 3, "Continental Congress", "FF joins.")) {
    return fail("enqueue FF_CONGRESS");
  }
  if (st.queue_count != 3) {
    return fail("chrome tags queue count");
  }
  if (st.queue[0].tag != AI_POPUP_TAG_DIPLO_FA ||
      st.queue[1].tag != AI_POPUP_TAG_KING_LETTER ||
      st.queue[2].tag != AI_POPUP_TAG_FF_CONGRESS) {
    return fail("chrome tag values");
  }
  return 0;
}

/*
 * port_bugs.md #1107: ai_popup_render over a spread of layouts (plain, widened by
 * a long row, left/right chief portrait, span clamp, King flair frames,
 * MSS/MYR modes, overlong body, no font). Asserts something was drawn; with
 * AI_POPUP_DUMP_DIR set it also writes one PGM (indices + st geometry in the
 * header comment) per layout, so a refactor can be cmp'd against a baseline.
 */
static int render_one(
  const char* tag, const AiPopupRequest* req, uint32_t now_ms, int selection,
  const ColonizeFont* font, const ColonizeSpriteSheet* wood, AiPopupState* st
) {
  static uint8_t pixels[320 * 200];
  memset(pixels, 0x11, sizeof(pixels));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  st->current = *req;
  st->open = true;
  st->selection = selection;
  ai_popup_set_now_ms(now_ms);
  ai_popup_render(st, font, wood, NULL, 1, 2, 3, &fb);
  int drawn = 0;
  for (size_t i = 0; i < sizeof(pixels); ++i) {
    drawn += pixels[i] != 0x11;
  }
  if (!drawn) {
    return fail(tag);
  }
  const char* dir = getenv("AI_POPUP_DUMP_DIR");
  if (dir && dir[0]) {
    char path[600];
    snprintf(path, sizeof(path), "%s/%s.pgm", dir, tag);
    FILE* f = fopen(path, "wb");
    if (!f) {
      return fail("dump open");
    }
    fprintf(f, "P5\n# %d %d %d %d %d %d %d %d\n320 200\n255\n", st->dialog_x,
            st->dialog_y, st->dialog_w, st->dialog_h, st->line_h, st->list_y0,
            st->king_anim_frame, (int)st->king_anim_next_ms);
    fwrite(pixels, 1, sizeof(pixels), f);
    fclose(f);
  }
  return 0;
}

static int case_render_layouts(void) {
  static AiPopupState st;
  static ColonizeFont font;
  static ColonizeSpriteSheet wood;
  char err[128];
  const bool font_ok = font_file_load("COLONIZE/FONTTINY.FF", &font, err, sizeof(err));
  const bool wood_ok = ss_load("COLONIZE/WOODTILE.SS", &wood, err, sizeof(err));
  ai_popup_set_portrait_source("COLONIZE");
  const ColonizeFont* f = font_ok ? &font : NULL;
  const ColonizeSpriteSheet* w = wood_ok ? &wood : NULL;

  AiPopupRequest base;
  memset(&base, 0, sizeof(base));
  base.portrait_tribe = -1;
  base.graphic_mss = -1;
  base.graphic_myr = -1;
  snprintf(base.title, sizeof(base.title), "Foreign Affairs");
  snprintf(base.body, sizeof(base.body),
           "The quick brown fox jumps over the lazy dog many times over.\n"
           "^^Centred line\nLast line.");
  base.choice_count = 3;
  snprintf(base.choices[0], sizeof(base.choices[0]), "Accept");
  snprintf(base.choices[1], sizeof(base.choices[1]), "Refuse");
  snprintf(base.choices[2], sizeof(base.choices[2]), "Ask later");

  int rc = 0;
  ai_popup_init(&st);
  rc |= render_one("plain", &base, 0, 1, f, w, &st);

  AiPopupRequest r = base;
  r.width = 120;
  snprintf(r.choices[2], sizeof(r.choices[2]),
           "A very long option row that widens the frame past width");
  ai_popup_init(&st);
  rc |= render_one("widened", &r, 0, 2, f, w, &st);

  r = base;
  r.title[0] = '\0';
  r.choice_count = 0;
  ai_popup_init(&st);
  rc |= render_one("info", &r, 0, 0, f, w, &st);

  const int tribes[] = {0, 1, 3, 4, 7};
  for (int i = 0; i < 5; ++i) {
    char tag[32];
    r = base;
    r.portrait_tribe = tribes[i];
    r.portrait_tier = i % 4;
    snprintf(tag, sizeof(tag), "chief%d", tribes[i]);
    ai_popup_init(&st);
    rc |= render_one(tag, &r, 0, 0, f, w, &st);
  }
  r = base;
  r.portrait_tribe = 2;
  r.portrait_tier = 3;
  r.width = 290;
  ai_popup_init(&st);
  rc |= render_one("chief_clamp", &r, 0, 1, f, w, &st);

  r = base;
  r.portrait_tribe = 8;
  ai_popup_init(&st);
  const uint32_t times[] = {1000, 4000, 4950, 5200, 5400, 9000};
  for (int i = 0; i < 6; ++i) {
    char tag[32];
    snprintf(tag, sizeof(tag), "king%d", i);
    rc |= render_one(tag, &r, times[i], 0, f, w, &st);
  }

  for (int m = 0; m < 6; ++m) {
    char tag[32];
    r = base;
    r.graphic_mss = m;
    snprintf(tag, sizeof(tag), "mss%d", m);
    ai_popup_init(&st);
    rc |= render_one(tag, &r, 0, 0, f, w, &st);
  }
  for (int m = 0; m < 4; ++m) {
    char tag[32];
    r = base;
    r.graphic_myr = m;
    r.width = m == 3 ? 300 : 0;
    snprintf(tag, sizeof(tag), "myr%d", m);
    ai_popup_init(&st);
    rc |= render_one(tag, &r, 0, 0, f, w, &st);
  }

  r = base;
  r.body[0] = '\0';
  for (int i = 0; i < 30; ++i) {
    strncat(r.body, "Line of filler text here.\n", sizeof(r.body) - strlen(r.body) - 1);
  }
  ai_popup_init(&st);
  rc |= render_one("overlong", &r, 0, 2, f, w, &st);
  r.graphic_mss = 0;
  ai_popup_init(&st);
  rc |= render_one("overlong_mss", &r, 0, 2, f, w, &st);

  ai_popup_init(&st);
  rc |= render_one("nofont", &base, 0, 1, NULL, NULL, &st);

  ai_popup_set_portrait_source(NULL);
  if (wood_ok) {
    ss_free(&wood);
  }
  if (font_ok) {
    font_file_free(&font);
  }
  return rc;
}

/*
 * port_bugs.md #1186: ai_popup_render places the dialog like FUN_6f74_14c6 —
 * 160 - (w >> 1) / 100 - (h >> 1) as arithmetic shifts (an odd size lands 1 px
 * down/right of the old halve-the-margin form), and under an MSS/MYR figure the
 * frame sits at fig_y - oy + sprite_h with the figure at 100 - (t >> 1). The
 * expectation is worked out here by hand from the dialog size the render reports.
 * A copy of FONTTINY with max_height 8 gives odd row pitches (9 / 11), so odd
 * and even dialog heights both occur; the guard fails if the fixture ever stops
 * producing odd ones.
 */
static int case_placement_is_dos(void) {
  static AiPopupState st;
  static ColonizeFont font;
  static ColonizeSpriteSheet wood;
  char err[128];
  if (!font_file_load("COLONIZE/FONTTINY.FF", &font, err, sizeof(err)) ||
      !ss_load("COLONIZE/WOODTILE.SS", &wood, err, sizeof(err))) {
    fprintf(stderr, "SKIP: assets: %s\n", err);
    return TR_SKIP;
  }
  font.max_height = 8;
  ai_popup_set_portrait_source("COLONIZE");

  AiPopupRequest r;
  memset(&r, 0, sizeof(r));
  r.portrait_tribe = -1;
  snprintf(r.title, sizeof(r.title), "Title");
  snprintf(r.choices[0], sizeof(r.choices[0]), "One");
  snprintf(r.choices[1], sizeof(r.choices[1]), "Two");
  snprintf(r.choices[2], sizeof(r.choices[2]), "Three");

  int rc = 0;
  int odd_w = 0;
  int odd_h = 0;
  int odd_total = 0;
  for (int mss = -1; mss < 6; ++mss) {
    for (int myr = -1; myr < (mss < 0 ? 4 : 0); ++myr) {
      for (int width = 0; width <= 151; width += 151) {
        for (int lines = 1; lines <= 3; ++lines) {
          for (int choices = 0; choices <= 3; ++choices) {
            r.graphic_mss = mss;
            r.graphic_myr = myr;
            r.width = width;
            r.choice_count = choices;
            r.body[0] = '\0';
            for (int l = 0; l < lines; ++l) {
              strcat(r.body, l ? "\nAnother short line." : "A short line.");
            }
            ai_popup_init(&st);
            if (render_one("place", &r, 0, 0, &font, &wood, &st)) {
              rc = 1;
              continue;
            }
            const int w = st.dialog_w;
            const int h = st.dialog_h;
            int ex = 160 - (w >> 1);
            int ey = 100 - (h >> 1);
            const ColonizeSpriteSheet* fig = ai_popup_decoration_sheet(mss, myr);
            if (fig && fig->sprite_count > 0 && fig->sprites[0].pixels) {
              const int sw = fig->sprites[0].width;
              const int sh = fig->sprites[0].height;
              const int oy = fig->place_offset_y;
              const int ox = fig->place_offset_x;
              const int t = sh - (oy < sh ? oy : sh) + h;
              odd_total += t & 1;
              if (t < 200) {
                const int fy = 100 - (t >> 1);
                ey = fy - oy + sh;
                if (fig->place_mode != 1) {
                  int o = ox;
                  int tw = w - ox + sw;
                  if (tw > 320) {
                    o += tw - 320;
                    tw = 320;
                  }
                  const int gx = 160 - (tw >> 1);
                  ex = fig->place_mode == 2 ? gx : gx - o + sw;
                }
              }
            }
            odd_w += w & 1;
            odd_h += h & 1;
            if (st.dialog_x != ex || st.dialog_y != ey) {
              fprintf(stderr,
                      "mss %d myr %d width %d lines %d choices %d: size %dx%d want (%d,%d) got (%d,%d)\n",
                      mss, myr, width, lines, choices, w, h, ex, ey, st.dialog_x, st.dialog_y);
              rc = 1;
            }
          }
        }
      }
    }
  }
  if (!odd_w || !odd_h || !odd_total) {
    fprintf(stderr, "fixture never produced odd w / h / assembly height (%d %d %d)\n", odd_w,
            odd_h, odd_total);
    rc = 1;
  }
  ai_popup_set_portrait_source(NULL);
  ss_free(&wood);
  font_file_free(&font);
  return rc;
}

static const TestCase k_cases[] = {
    {"case_fresh_idle", case_fresh_idle},
    {"case_enqueue_and_present_info", case_enqueue_and_present_info},
    {"case_dismiss_result", case_dismiss_result},
    {"case_choice_flow", case_choice_flow},
    {"case_queue_overflow", case_queue_overflow},
    {"case_chrome_tags", case_chrome_tags},
    {"case_set_last_after_dropped_enqueue", case_set_last_after_dropped_enqueue},
    {"case_render_layouts", case_render_layouts},
    {"case_placement_is_dos", case_placement_is_dos},
};

TEST_MAIN(k_cases)
