#include "core/closing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/cinematic.h"
#include "core/pik.h"
#include "platform/diagnostics.h"

static const char* const kClosingSheets[CLOSING_SHEET_COUNT] = {
  "CLOS-HAT.SS",
  "CLOS-LDY.SS",
  "CLOS-MAN.SS",
  "CLOS-MIL.SS",
  "CLOS-FWK.SS",
  "CLOS-ROC.SS",
  "CLOS-BEL.SS",
};

static ColonizeClosingSoundFn g_closing_play;
static ColonizeClosingSoundFn g_closing_set_bgm;
static ColonizeClosingStopSfxFn g_closing_stop_sfx;

void closing_set_sound_hooks(
  ColonizeClosingSoundFn play_fn,
  ColonizeClosingSoundFn set_bgm_fn,
  ColonizeClosingStopSfxFn stop_sfx_fn
) {
  g_closing_play = play_fn;
  g_closing_set_bgm = set_bgm_fn;
  g_closing_stop_sfx = stop_sfx_fn;
}

/* CLOSING.TXT @CLOSING row: series, frame, repeats, baseX and an optional
 * fifth Delay column (absent = 0). */
static void closing_store_row(void* ctx, int index, const int* v, int n) {
  ClosingSeries* out = (ClosingSeries*)ctx;
  out[index].series = v[0];
  out[index].frame = v[1];
  out[index].repeats = v[2];
  out[index].base_x = v[3];
  out[index].delay = (n < 5) ? 0 : v[4];
}

int closing_parse_timeline(
  const char* data_dir,
  ClosingSeries* out,
  int out_max,
  int* out_end_frame
) {
  if (out_end_frame) {
    *out_end_frame = 390;
  }
  if (!out || out_max <= 0) {
    return 0;
  }
  return cinematic_parse_timeline(
    data_dir, "CLOSING.TXT", "CLOSING", 390, out_max, closing_store_row, out, out_end_frame
  );
}

static int closing_start_tick(const ClosingSeries* s) {
  if (!s) {
    return 1;
  }
  int start = s->frame + s->delay;
  if (start < 1) {
    start = 1;
  }
  return start;
}

static void closing_compose(ClosingCinematic* c) {
  if (!c) {
    return;
  }
  memcpy(c->canvas, c->background, sizeof(c->canvas));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = c->canvas};
  for (int i = 0; i < c->series_count; ++i) {
    const ClosingSeries* s = &c->series[i];
    if (s->series < 0 || s->series >= CLOSING_SHEET_COUNT || !c->sheet_ok[s->series]) {
      continue;
    }
    const ColonizeSpriteSheet* sheet = &c->sheets[s->series];
    const int n = sheet->sprite_count;
    if (n <= 0) {
      continue;
    }
    const int start = closing_start_tick(s);
    const int elapsed = c->clock - start;
    if (elapsed < 0) {
      continue;
    }
    if (s->repeats >= 0 && elapsed >= s->repeats * n) {
      continue;
    }
    /* FUN_6f30_002e: anchor_x is a horizontal centre, anchor_y a bottom baseline. */
    ss_blit_anchored(sheet, elapsed % n, &fb, s->base_x, 0);
  }
}

static void closing_play_frame_sounds(const ClosingCinematic* c) {
  if (!c || !g_closing_play) {
    return;
  }
  for (int i = 0; i < c->series_count; ++i) {
    const ClosingSeries* s = &c->series[i];
    if (s->series < 0 || s->series >= CLOSING_SHEET_COUNT || !c->sheet_ok[s->series]) {
      continue;
    }
    const int n = c->sheets[s->series].sprite_count;
    if (n <= 0) {
      continue;
    }
    const int elapsed = c->clock - closing_start_tick(s);
    if (elapsed < 0) {
      continue;
    }
    const int frame = elapsed % n;
    /*
     * CLOSING.EXE keeps a 1-based per-series counter (record +12, table
     * 0x4b96 stride 14) and both cues are keyed on it, so `frame` — this
     * port's 0-based sprite index — is the counter minus one:
     *
     *   `_anim_loop` (image 0x20c, cue test 0x284-0x2a5) compares the
     *   counter BEFORE incrementing: series 4 (CLOS-FWK) plays 0x59 at
     *   counter 1, 27, 37, 42 (`cmp ax,0x2a` then `dec al` / `sub al,0x1a`
     *   / `sub al,0x0a`). It then increments and tail-calls `_do_anims`
     *   (0x37b -> 0x102), which draws with the *incremented* counter — so
     *   the tick that plays 0x59 for counter c is the tick that draws
     *   c + 1, i.e. port frame c. The two off-by-ones cancel and the test
     *   below is literally 1/27/37/42. `_do_anims` instead plays 0x5a
     *   inline while drawing (0x19b-0x1ae: series 0 = CLOS-HAT, counter
     *   == 1), which is the drawn frame itself, i.e. port frame 0.
     *
     * Both cues DO re-fire on every wrap, and that is DOS behaviour, not a
     * port artefact: with repeats == -1 (the shipped CLOS-FWK/CLOS-HAT
     * rows) `_anim_loop` at 0x2c4-0x2e5 leaves the negative repeat count
     * alone, resets the counter to 1 and keeps the series active, so the
     * counter cycles 1..sprite_count forever with period sprite_count —
     * exactly what `elapsed % n` reproduces. CLOS-FWK has 66 sprites and
     * CLOS-HAT 22 (section 1 of each .SS is 1056 / 352 bytes of 16-byte
     * sprite records), so over the 390-tick run the fireworks cue fires in
     * ~5.9 cycles and every one of the four counters is reachable.
     */
    if (s->series == CLOSING_SHEET_FIREWORKS &&
        (frame == 1 || frame == 27 || frame == 37 || frame == 42)) {
      g_closing_play(CLOSING_FIREWORK_SOUND_ID);
    }
    if (s->series == CLOSING_SHEET_HAT && frame == 0) {
      g_closing_play(CLOSING_CHEER_SOUND_ID);
    }
  }
}

static bool closing_step(ClosingCinematic* c, bool play_sounds) {
  if (!c || c->finished) {
    return false;
  }
  c->clock++;
  if (c->end_frame > 0 && c->clock >= c->end_frame) {
    c->clock = c->end_frame;
    closing_compose(c);
    if (play_sounds) {
      closing_play_frame_sounds(c);
    }
    c->finished = true;
    return false;
  }
  closing_compose(c);
  if (play_sounds) {
    closing_play_frame_sounds(c);
  }
  return true;
}

bool closing_open(ClosingCinematic* c, const char* data_dir) {
  if (!c || !data_dir) {
    return false;
  }
  closing_close(c);
  memset(c, 0, sizeof(*c));

  if (!cinematic_load_background(
        data_dir, "CLOS-BKG.PIK", "Closing cinematic", c->background, &c->palette,
        &c->palette_ok
      )) {
    return false;
  }

  int loaded = 0;
  for (int i = 0; i < CLOSING_SHEET_COUNT; ++i) {
    if (!cinematic_load_sheet(data_dir, kClosingSheets[i], &c->sheets[i], "Closing cinematic")) {
      continue;
    }
    c->sheet_ok[i] = true;
    loaded++;
  }
  if (loaded <= 0) {
    diag_warn("Closing cinematic: no CLOS-*.SS sheets loaded.");
    return false;
  }

  c->series_count = closing_parse_timeline(data_dir, c->series, CLOSING_SERIES_MAX, &c->end_frame);
  if (c->series_count <= 0) {
    /* Shipped CLOSING.TXT @CLOSING, used when the catalog is missing. */
    static const ClosingSeries kDefault[] = {
      {4, 1, -1, 0, 0},
      {6, 1, -1, 0, 0},
      {5, 1, -1, 0, 100},
      {0, 1, -1, 0, 16},
      {1, 1, -1, 0, 0},
      {2, 1, -1, 0, 0},
      {3, 1, -1, 0, 0},
    };
    memcpy(c->series, kDefault, sizeof(kDefault));
    c->series_count = (int)(sizeof(kDefault) / sizeof(kDefault[0]));
    c->end_frame = 390;
  }
  if (c->end_frame < 1) {
    c->end_frame = 390;
  }

  memcpy(c->canvas, c->background, sizeof(c->canvas));
  c->clock = 0;
  c->accum_ms = 0;
  c->open = true;
  c->finished = false;
  if (g_closing_set_bgm) {
    g_closing_set_bgm(0); /* CLOSING.EXE is its own process; no VICEROY pool. */
  }
  if (g_closing_play) {
    g_closing_play(CLOSING_BGM_ID);
  }
  return true;
}

void closing_close(ClosingCinematic* c) {
  if (!c) {
    return;
  }
  const bool was_open = c->open;
  for (int i = 0; i < CLOSING_SHEET_COUNT; ++i) {
    if (c->sheet_ok[i]) {
      ss_free(&c->sheets[i]);
    }
  }
  memset(c, 0, sizeof(*c));
  /* CLOSING.EXE exit tears down GSOUND; the COLDIG ring would otherwise
   * keep cheering into the score screen. BGM 0x3d is left playing. */
  if (was_open && g_closing_stop_sfx) {
    g_closing_stop_sfx();
  }
}

void closing_update(ClosingCinematic* c, uint32_t dt_ms) {
  if (!c || !c->open || c->finished) {
    return;
  }
  c->accum_ms += dt_ms;
  while (c->accum_ms >= CLOSING_FRAME_MS) {
    c->accum_ms -= CLOSING_FRAME_MS;
    if (!closing_step(c, true)) {
      c->accum_ms = 0;
      break;
    }
  }
}

bool closing_handle_input(ClosingCinematic* c, const ColonizeInputState* input) {
  if (!c || !c->open) {
    return false;
  }
  if (!input) {
    return true;
  }
  const bool pressed =
    input->last_key != COLONIZE_KEY_NONE || input->mouse_left_clicked ||
    input->mouse_right_clicked;
  if (pressed) {
    closing_close(c);
  }
  return true;
}

void closing_render(
  const ClosingCinematic* c,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* palette
) {
  if (!c || !c->open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  cinematic_blit_canvas320(c->canvas, framebuffer);
  if (palette && c->palette_ok) {
    *palette = c->palette;
  }
}
