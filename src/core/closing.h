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
 *   Frame = first global clock tick the series is drawn
 *   Repeats -1 = loop the sheet until the end marker, 0 = play once and
 *   hold the last sprite
 *   Delay = hidden ticks between cycles (a re-armed cycle starts at sprite 2)
 *   BaseX is added to the sprite's own FUN_6f30_002e anchor
 *   series -1, Frame N = stop when the clock reaches N (shipped: 390)
 *
 * CLOSING.EXE paces frames on its own 60.877 Hz clock (CLOSING_ANIM_TIMING
 * below): the 390-frame run lasts ~38.4 s. A skip key / click goes to scoring.
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

/*
 * CLOSING.EXE timer_install (image 0x2659) programs the PIT with
 * TIMER_SET_RATE(0x7a8) = 608.77 Hz; the IRQ0 handler (0x24e8) bumps the
 * timer_read counter DS:0x4f4c on every 10th IRQ = 60.877 Hz. `_anim_loop`
 * (0x211-0x232) steps one frame once now - anim_clock >= anim_timing; DS:0x54
 * anim_timing starts at 6 ('+'/'-' in `_get_input` 0x42a/0x430 nudge it).
 * accum counts ms * 60877: one 60.877 Hz tick = 1000000 units.
 */
#define CLOSING_ANIM_TIMING 6
#define CLOSING_ACCUM_PER_MS 60877u
#define CLOSING_ACCUM_PER_TICK 1000000u
/* Whole ms that always covers one frame (98.56 ms -> 99). */
#define CLOSING_FRAME_MS \
  ((CLOSING_ANIM_TIMING * CLOSING_ACCUM_PER_TICK + CLOSING_ACCUM_PER_MS - 1u) / CLOSING_ACCUM_PER_MS)

/*
 * CLOSING.EXE `call 069B:000E` (GSOUND FUN_12d8_000e):
 *   0x3d  BGM at `_closing` start (unlisted victory cue, not pool 3)
 *   0x5a  COLDIG 15 cheer+fireworks when CLOS-HAT's 1-based frame is 1
 *         (`_do_anims` 0x19b, tested on the drawn counter)
 *   0x59  COLDIG event when CLOS-FWK's pre-increment frame is
 *         1, 27, 37 or 42 (`_anim_loop` 0x284)
 *
 * CLOS-FWK (Delay 0) wraps straight back to counter 1, so 0x59 re-fires every
 * cycle. CLOS-HAT (Delay 16) re-arms at counter 1 and draws 2 first, so 0x5a
 * fires once (closing.c closing_tick).
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
  /* `_anim_loop` runtime (record +2 / +4 / +10 / +12), set by closing_open. */
  int start;
  int repeats_left;
  bool active;
  int counter; /* 1-based drawn sprite, 0 before the first draw */
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
  int anim_timing; /* DS:0x54, '+'/'-' nudge it */
  /* Alt-Q / Alt-X cleared to_game DS:0x70 (`_get_input` 0x421): CLOSING.EXE
   * exits to DOS without `viceroy -ow`, so no score. Set after the close;
   * the owner must quit the app. */
  bool quit_to_dos;
  uint64_t accum; /* ms * CLOSING_ACCUM_PER_MS since the last frame */
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

/*
 * Enter / Esc / Space / a mouse click / Alt-Q / Alt-X end the cinematic
 * outright; other keys do nothing and '+'/'=' / '-'/'_' change the speed.
 * There is no skip-to-the-last-frame step (CLOSING.EXE image 0x398: those keys store 0 into
 * the [0x6c] loop flag, the same word _anim_loop clears at the terminator row,
 * and the main loop at 0x471 falls straight out). True while still open.
 */
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
