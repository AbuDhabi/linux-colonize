#ifndef COLONIZE_CLOSING_H
#define COLONIZE_CLOSING_H

#include <stdbool.h>
#include <stdint.h>

#include "core/ss.h"
#include "platform/platform.h"

/*
 * Rebel-victory cinematic — DOS CLOSING.EXE, not VICEROY.
 *
 * After @WINNING / the @KINGLOSE audience, VICEROY execs `closing -gok`
 * (string at VICEROY.EXE 0x1dabb). CLOSING.EXE loads CLOS-BKG.PIK plus the
 * CLOS-*.SS sheets listed below, plays the @CLOSING timeline from
 * CLOSING.TXT, then chains back to VICEROY for the score / HoF / @SCORED
 * sequence (main-loop DS:0x104 block).
 *
 * CLOSING.TXT @CLOSING rows: Series, Frame, Repeats, BaseX, Delay
 *   series 0..6 → CLOS-HAT / LDY / MAN / MIL / FWK / ROC / BEL
 *   Frame + Delay = first global clock tick the series is drawn
 *   Repeats -1 = loop the sheet until the end marker
 *   BaseX is added to the sprite's own FUN_6f30_002e anchor
 *   series -1, Frame N = stop when the clock reaches N (shipped: 390)
 *
 * CLOSING.EXE uses its own TIMER_SET_RATE; with the DOS default 18.2 Hz
 * PIT the 390-tick run lasts ~21 s. Any key / click skips to scoring.
 */

#define CLOSING_SHEET_COUNT 7
#define CLOSING_SERIES_MAX 8

#define CLOSING_SHEET_HAT 0
#define CLOSING_SHEET_LADY 1
#define CLOSING_SHEET_MAN 2
#define CLOSING_SHEET_MIL 3
#define CLOSING_SHEET_FIREWORKS 4
#define CLOSING_SHEET_ROCK 5
#define CLOSING_SHEET_BELL 6

/* BIOS timer tick. 390 frames × 55 ms ≈ 21 s. */
#define CLOSING_FRAME_MS 55

/*
 * CLOSING.EXE `call 069B:000E` (GSOUND FUN_12d8_000e):
 *   0x3d  BGM at `_closing` start (unlisted victory cue, not pool 3)
 *   0x5a  COLDIG 15 cheer+fireworks when CLOS-HAT's 1-based frame is 1
 *   0x59  COLDIG event when CLOS-FWK's pre-increment frame is
 *         1, 27, 37 or 42 (`_anim_loop` 0x284)
 */
#define CLOSING_BGM_ID 0x3d
#define CLOSING_CHEER_SOUND_ID 0x5a
#define CLOSING_FIREWORK_SOUND_ID 0x59

typedef struct ClosingSeries {
  int series; /* sheet index, or -1 for the end marker */
  int frame;  /* start tick (1-based, as in the TXT) */
  int repeats;
  int base_x;
  int delay;
} ClosingSeries;

typedef struct ClosingCinematic {
  bool open;
  bool finished;
  ColonizeSpriteSheet sheets[CLOSING_SHEET_COUNT];
  bool sheet_ok[CLOSING_SHEET_COUNT];
  ClosingSeries series[CLOSING_SERIES_MAX];
  int series_count;
  int end_frame;
  int clock;
  uint32_t accum_ms;
  ColonizePalette palette;
  bool palette_ok;
  uint8_t background[320 * 200];
  uint8_t canvas[320 * 200];
} ClosingCinematic;

typedef void (*ColonizeClosingSoundFn)(int id);
typedef void (*ColonizeClosingStopSfxFn)(void);
void closing_set_sound_hooks(
  ColonizeClosingSoundFn play_fn,
  ColonizeClosingSoundFn set_bgm_fn,
  ColonizeClosingStopSfxFn stop_sfx_fn
);

/*
 * Load CLOS-BKG.PIK, the seven CLOS-*.SS sheets, and CLOSING.TXT @CLOSING.
 * False (and open == false) when the background is missing.
 */
bool closing_open(ClosingCinematic* c, const char* data_dir);
void closing_close(ClosingCinematic* c);

void closing_update(ClosingCinematic* c, uint32_t dt_ms);
void closing_skip_to_end(ClosingCinematic* c);

/* Any key or click skips the rest and closes. True while still open. */
bool closing_handle_input(ClosingCinematic* c, const ColonizeInputState* input);

void closing_render(
  const ClosingCinematic* c,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* palette
);

/* TXT parser, exposed for tests. series_count / end_frame filled. */
int closing_parse_timeline(
  const char* data_dir,
  ClosingSeries* out,
  int out_max,
  int* out_end_frame
);

#endif
