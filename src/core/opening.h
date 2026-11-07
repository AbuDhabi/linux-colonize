#ifndef COLONIZE_OPENING_H
#define COLONIZE_OPENING_H

#include <stdbool.h>
#include <stdint.h>

#include "core/font_file.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Title intro cinematic — DOS OPENING.EXE, not VICEROY.
 *
 * COLONIZE.BAT is `opening -g`, which plays this then execs `viceroy`.
 * OPENING.EXE starts with the spinning MPS Labs logo (MPSLOGO.SS +
 * MPSNAME.SS, 228 ticks) before the sailing cinematic.
 * Art: OPENING.PIK (960×132 panorama) + OPENBORD.PIK (320×200 frame with a
 * colour-0 window at y=24..155), OPENSHIP.SS + PATH.DAT (701 world x,y
 * points), CLOS-style series sheets from OPENING.TXT @OPENING, and credit
 * plates OPENCRD1/2/3 from @CREDITS.
 *
 * @OPENING rows: Series, Frame, Repeats, BaseX
 *   series 0..9 → WND1 / SUN / MON1 / WND2 / MON2 / MON3 / FISH / GUY /
 *                 LOGO / BONK
 *   BaseX is a world-x origin on the 960-wide panorama (640 / 320 / 0)
 *   Repeats 0 = play once then hold the last sprite, except OPENGUY
 *   which idle-loops the last (OPENING_GUY_LOOP_BACK+1) sprites
 *   (OPENING.EXE `_anim_loop` 0x7f6: frame = count-5).
 *   Repeats >0 = that many cycles then the series ends.
 *   series -1, Frame N = stop when the clock reaches N (shipped: 891)
 *
 * Camera: pan_x (DS:0x4aca) starts at 640 and drops one px per scene step
 * to 0; it never follows the ship. Keys (`_get_input` 0x922): Enter / Esc /
 * Space / a click skip the MPS logo into the sailing scene; during sailing
 * one of them jumps to the OPENLOGO frame (`_jump_to_logo` 0x872) and ends
 * the intro under the @MESSAGES "loading" line; Alt-Q / Alt-X end it
 * with quit_to_dos, and in the demo a keyboard skip (logo or sailing) does
 * too. '+'/'=' and '-'/'_' make it faster / slower. Other keys do
 * nothing. After the last frame or the jump the still holds for
 * OPENING_HOLD_MS, then the owner goes to the title menu. Paced on the
 * 60.877 Hz clock (see OPENING_PAN_TIMING).
 */

#define OPENING_SHEET_COUNT 10
#define OPENING_CREDIT_SHEETS 3
#define OPENING_SERIES_MAX 16
#define OPENING_CREDIT_MAX 32
#define OPENING_PATH_MAX 768

#define OPENING_SHEET_WIND1 0
#define OPENING_SHEET_SUN 1
#define OPENING_SHEET_MON1 2
#define OPENING_SHEET_WIND2 3
#define OPENING_SHEET_MON2 4
#define OPENING_SHEET_MON3 5
#define OPENING_SHEET_FISH 6
#define OPENING_SHEET_GUY 7
#define OPENING_SHEET_LOGO 8
#define OPENING_SHEET_BONK 9
/* OPENGUY Repeats 0: wrap 1-based frame to count-5 (last 6 sprites). */
#define OPENING_GUY_LOOP_BACK 5

#define OPENING_SCENE_Y 24
#define OPENING_SCENE_H 132
#define OPENING_WORLD_W 960
#define OPENING_VIEW_W 320
/* `_open_loop` 0xaaf: pan_x DS:0x4aca = 0x280 before the first step. */
#define OPENING_PAN_START 0x280
/* `_pan` 0x57e: past anim_frame 0x1fb the series draw over the ship. */
#define OPENING_SHIP_UNDER_FRAME 0x1fb
/* First row below the OPENBORD window (y=24..155). */
#define OPENING_CREDIT_TOP (OPENING_SCENE_Y + OPENING_SCENE_H)
/* Inner bevel of the bottom frame (OPENBORD y=156..166). Banners sit below it. */
#define OPENING_CREDIT_BORDER 11

/*
 * OPENING.EXE timer_install (image 0x336d) programs TIMER_SET_RATE(0x7a8) =
 * 608.77 Hz and the IRQ0 handler bumps the timer_read counter DS:0x5cb6 every
 * 10th IRQ = 60.877 Hz (game_clock DS:0x4ade). The sailing steppers `_pan`
 * 0x53e, `_ship_move` 0x5a4, `_credit_loop` 0x5fa and `_anim_loop` 0x729 all
 * test DS:0x48 pan_timing (8; ship/credit/anim_timing are also 8 but unread
 * there), so the scene clock advances once per 8 ticks. `_logo_loop` 0xd16
 * tests DS:0x50 logo_timing = 6. accum counts ms * 60877: one tick = 1000000.
 */
#define OPENING_PAN_TIMING 8
#define OPENING_LOGO_TIMING 6
#define OPENING_ACCUM_PER_MS 60877u
#define OPENING_ACCUM_PER_TICK 1000000u
/* Whole ms that always cover one scene / logo frame (131.4 -> 132, 98.56 -> 99). */
#define OPENING_FRAME_MS \
  ((OPENING_PAN_TIMING * OPENING_ACCUM_PER_TICK + OPENING_ACCUM_PER_MS - 1u) / OPENING_ACCUM_PER_MS)
#define OPENING_LOGO_FRAME_MS \
  ((OPENING_LOGO_TIMING * OPENING_ACCUM_PER_TICK + OPENING_ACCUM_PER_MS - 1u) / OPENING_ACCUM_PER_MS)
#define OPENING_HOLD_MS 1000
/* MPS logo clock (OPENING.EXE [0xd2]): name from 0x5c, end when past 0xe4. */
#define OPENING_LOGO_NAME_FRAME 0x5c
#define OPENING_LOGO_END_FRAME 0xe4

/* OPENING.EXE: `mov ax,0x34` then far-call the GSOUND dispatcher (image
 * +0xfd8). 0x34 is the looping intro theme, not Pick Music / title 0x33. */
#define OPENING_BGM_ID 0x34

typedef struct OpeningSeries {
  int series;
  int frame;
  int repeats;
  int base_x;
} OpeningSeries;

typedef struct OpeningCredit {
  int start_frame;
  int end_frame;
  int series;
  int sprite; /* 0-based after parsing */
} OpeningCredit;

typedef struct OpeningCinematic {
  bool open;
  bool finished;
  int skip_from; /* scene clock at the `_jump_to_logo` skip, -1 = none */
  int pan_timing;  /* DS:0x48, '+'/'-' nudge it (`_get_input` 0xa7c/0xa86) */
  int logo_timing; /* DS:0x50, nudged with pan_timing */
  /* In: owner sets after opening_open when VICEROY runs the demo (argv -d,
   * DS:0x88 demo). Out: DOS cleared to_game DS:0x90 (Alt-Q / Alt-X, or a
   * demo keyboard skip), so OPENING.EXE exits to DOS instead of chaining
   * VICEROY; finished is set too and the owner must quit the app. */
  bool demo;
  bool quit_to_dos;
  /* `_get_input` 0xa15-0xa63: OPENING.TXT @MESSAGES row 0 in FONTINTR after
   * the skip jump. */
  char loading_text[64];
  ColonizeFont font;
  bool font_ok;
  ColonizeSpriteSheet sheets[OPENING_SHEET_COUNT];
  bool sheet_ok[OPENING_SHEET_COUNT];
  ColonizeSpriteSheet ship;
  bool ship_ok;
  ColonizeSpriteSheet mps_logo;
  bool mps_logo_ok;
  ColonizeSpriteSheet mps_name;
  bool mps_name_ok;
  bool logo_phase;
  int logo_clock;
  int logo_frame;
  int logo_name_frame;
  ColonizeSpriteSheet credits[OPENING_CREDIT_SHEETS];
  bool credit_ok[OPENING_CREDIT_SHEETS];
  OpeningSeries series[OPENING_SERIES_MAX];
  int series_count;
  OpeningCredit credit_rows[OPENING_CREDIT_MAX];
  int credit_count;
  int path_x[OPENING_PATH_MAX];
  int path_y[OPENING_PATH_MAX];
  int path_count;
  int end_frame;
  int clock;
  uint64_t accum; /* ms * OPENING_ACCUM_PER_MS since the last frame */
  uint32_t hold_ms;
  ColonizePalette palette;
  bool palette_ok;
  uint8_t scene[OPENING_WORLD_W * OPENING_SCENE_H];
  bool scene_ok;
  uint8_t border[320 * 200];
  bool border_ok;
  uint8_t canvas[320 * 200];
} OpeningCinematic;

/* 0-based sprite of series `s` at `elapsed` frames past its start (sheet of
 * `n` sprites), -1 = not drawn. Exposed for the unit test. */
int opening_sprite_index(const OpeningSeries* s, int elapsed, int n);
/* Panorama x at the left screen edge (pan_x DS:0x4aca). Exposed for the test. */
int opening_camera_x(const OpeningCinematic* o);

typedef void (*ColonizeOpeningSoundFn)(int id);
void opening_set_sound_hooks(ColonizeOpeningSoundFn play_fn, ColonizeOpeningSoundFn set_bgm_fn);

bool opening_open(OpeningCinematic* o, const char* data_dir);
void opening_close(OpeningCinematic* o);

void opening_update(OpeningCinematic* o, uint32_t dt_ms);

/* True while still open. See the key list at the top of this file; a skip
 * key during the end hold finishes. */
bool opening_handle_input(OpeningCinematic* o, const ColonizeInputState* input);

void opening_render(
  const OpeningCinematic* o,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* palette
);

int opening_parse_timeline(
  const char* data_dir,
  OpeningSeries* out,
  int out_max,
  int* out_end_frame
);

int opening_parse_credits(
  const char* data_dir,
  OpeningCredit* out,
  int out_max
);

int opening_parse_path(const char* data_dir, int* xs, int* ys, int out_max);

#endif
