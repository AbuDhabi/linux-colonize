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
    *out_end_frame = 0; /* no end row in the TXT = 0, never a typed copy */
  }
  if (!out || out_max <= 0) {
    return 0;
  }
  return cinematic_parse_timeline(
    data_dir, "CLOSING.TXT", "CLOSING", 0, out_max, closing_store_row, out, out_end_frame
  );
}

/*
 * One CLOSING.EXE frame: `_anim_loop` (image 0x20c-0x37e) then `_do_anims`
 * (0x102-0x20b). Each series is the 14-byte record at 0x4b96 (+2 start tick,
 * +4 repeats, +8 delay, +10 active, +12 1-based counter, 0 before the first
 * draw). A series activates when its start tick equals the clock (Frame
 * alone, 0x35a-0x36d); it tests the CLOS-FWK cue on the counter, increments
 * it, and on passing the sprite count: repeats 0 holds the last sprite
 * (0x320), otherwise repeats > 0 counts down, the counter goes back to 1 and
 * the series retires at repeats 0, or with a non-zero Delay goes inactive and
 * re-arms at clock + Delay (0x2f6-0x31e). The re-armed cycle therefore draws
 * from counter 2, and the CLOS-HAT cheer (drawn counter 1, 0x19b) fires on
 * the first cycle only. _do_anims repaints the background (0x92) and draws
 * the active series only, so the Delay is a hidden gap (#1671).
 */
static void closing_tick(ClosingCinematic* c, bool play_sounds) {
  for (int i = 0; i < c->series_count; ++i) {
    ClosingSeries* s = &c->series[i];
    if (!s->active && s->start == c->clock && s->series >= 0) {
      s->active = true;
    }
    if (!s->active || s->series < 0 || s->series >= CLOSING_SHEET_COUNT ||
        !c->sheet_ok[s->series]) {
      continue;
    }
    const int n = c->sheets[s->series].sprite_count;
    if (n <= 0) {
      continue;
    }
    /* 0x284-0x2a5: CLOS-FWK counter 1, 27, 37, 42 before the increment. */
    if (play_sounds && g_closing_play && s->series == CLOSING_SHEET_FIREWORKS &&
        (s->counter == 1 || s->counter == 27 || s->counter == 37 || s->counter == 42)) {
      g_closing_play(CLOSING_FIREWORK_SOUND_ID);
    }
    if (++s->counter <= n) {
      continue;
    }
    if (s->repeats_left == 0) {
      s->counter = n;
      continue;
    }
    if (s->repeats_left > 0) {
      s->repeats_left--;
    }
    s->counter = 1;
    if (s->repeats_left == 0) {
      s->active = false;
    } else if (s->delay != 0) {
      s->active = false;
      s->start = s->delay + c->clock;
    }
  }

  memcpy(c->canvas, c->background, sizeof(c->canvas));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = c->canvas};
  for (int i = 0; i < c->series_count; ++i) {
    const ClosingSeries* s = &c->series[i];
    if (!s->active || s->series < 0 || s->series >= CLOSING_SHEET_COUNT ||
        !c->sheet_ok[s->series] || s->counter <= 0 ||
        s->counter > c->sheets[s->series].sprite_count) {
      continue;
    }
    /* FUN_6f30_002e: anchor_x is a horizontal centre, anchor_y a bottom baseline. */
    ss_blit_anchored(&c->sheets[s->series], s->counter - 1, &fb, s->base_x, 0);
    if (play_sounds && g_closing_play && s->series == CLOSING_SHEET_HAT && s->counter == 1) {
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
    closing_tick(c, play_sounds);
    c->finished = true;
    return false;
  }
  closing_tick(c, play_sounds);
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
    if (!ss_load_asset(data_dir, kClosingSheets[i], &c->sheets[i], "Closing cinematic")) {
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
  /* The timeline is CLOSING.TXT @CLOSING data (end frame = its -1 row);
   * no compiled-in copy. */
  if (c->series_count <= 0 || c->end_frame < 1) {
    diag_warn("Closing cinematic: no @CLOSING timeline in CLOSING.TXT.");
    closing_close(c);
    return false;
  }

  for (int i = 0; i < c->series_count; ++i) {
    c->series[i].start = c->series[i].frame;
    c->series[i].repeats_left = c->series[i].repeats;
  }
  memcpy(c->canvas, c->background, sizeof(c->canvas));
  c->clock = 0;
  c->accum = 0;
  c->anim_timing = CLOSING_ANIM_TIMING;
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
  /* `_anim_loop` 0x211-0x232 is a signed 32-bit now - anim_clock >=
   * anim_timing test, so a timing nudged to 0 or below steps on every loop
   * pass: one step per update here. */
  const uint64_t frame =
    c->anim_timing > 0 ? (uint64_t)c->anim_timing * CLOSING_ACCUM_PER_TICK : 0;
  c->accum += (uint64_t)dt_ms * CLOSING_ACCUM_PER_MS;
  while (c->accum >= frame) {
    c->accum -= frame;
    if (!closing_step(c, true) || frame == 0) {
      c->accum = 0;
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
  /* `_get_input` 0x42a/0x430: '-' / '_' slow down, '+' / '=' speed up, one
   * tick on anim_timing DS:0x54, with no limit. */
  for (int i = 0; i < input->text_input_len; ++i) {
    const char ch = input->text_input[i];
    c->anim_timing += (ch == '-' || ch == '_') ? 1 : (ch == '+' || ch == '=') ? -1 : 0;
  }
  /* 0x3ed-0x448: Enter, Esc, Space, a mouse button (stored as 0x1b), Alt-Q
   * (0x110) or Alt-X (0x12d) end it; any other key is ignored. */
  const ColonizeKey k = input->last_key;
  const bool alt_quit = input->alt_held && (k == COLONIZE_KEY_Q || k == COLONIZE_KEY_X);
  if (k == COLONIZE_KEY_ENTER || k == COLONIZE_KEY_ESCAPE || k == COLONIZE_KEY_SPACE ||
      alt_quit || input->mouse_left_clicked || input->mouse_right_clicked) {
    closing_close(c);
    c->quit_to_dos = alt_quit;
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
