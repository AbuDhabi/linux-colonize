#ifndef COLONIZE_OPENING_H
#define COLONIZE_OPENING_H

#include <stdbool.h>
#include <stdint.h>

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
 *   Repeats 0 = play once and hold the last sprite; >0 = that many cycles
 *   series -1, Frame N = stop when the clock reaches N (shipped: 891)
 *
 * Camera follows the ship, clamped to [0, 640]. One key/click skips the
 * MPS logo into the sailing scene. Two edges during sailing jump to the
 * last frame (so a single accidental tap does not skip). After the last
 * frame — natural end or skip — the still holds for OPENING_HOLD_MS, then
 * the owner goes to the title menu. Same BIOS-tick pacing as CLOSING.EXE
 * (~55 ms).
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

#define OPENING_SCENE_Y 24
#define OPENING_SCENE_H 132
#define OPENING_WORLD_W 960
#define OPENING_VIEW_W 320
#define OPENING_CAMERA_MAX (OPENING_WORLD_W - OPENING_VIEW_W)
/* First row below the OPENBORD window (y=24..155). */
#define OPENING_CREDIT_TOP (OPENING_SCENE_Y + OPENING_SCENE_H)
/* Inner bevel of the bottom frame (OPENBORD y=156..166). Banners sit below it. */
#define OPENING_CREDIT_BORDER 11

#define OPENING_FRAME_MS 55
#define OPENING_HOLD_MS 1000
/* MPS logo clock (OPENING.EXE [0xd2]): name from 0x5c, end when past 0xe4. */
#define OPENING_LOGO_NAME_FRAME 0x5c
#define OPENING_LOGO_END_FRAME 0xe4

/*
 * OPENSHIP.SS is 47 px wide with 14 px of transparent padding on the right,
 * so width/2 sits 3 px left of OPENBONK's last still (FUN_6f30 dest x 141
 * vs PATH 161). Added to dest x so the moving hull and the still coincide.
 */
#define OPENING_SHIP_X_ALIGN 3

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
  int skip_presses;
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
  uint32_t accum_ms;
  uint32_t hold_ms;
  ColonizePalette palette;
  bool palette_ok;
  uint8_t scene[OPENING_WORLD_W * OPENING_SCENE_H];
  bool scene_ok;
  uint8_t border[320 * 200];
  bool border_ok;
  uint8_t canvas[320 * 200];
} OpeningCinematic;

typedef void (*ColonizeOpeningSoundFn)(int id);
void opening_set_sound_hooks(ColonizeOpeningSoundFn play_fn, ColonizeOpeningSoundFn set_bgm_fn);

bool opening_open(OpeningCinematic* o, const char* data_dir);
void opening_close(OpeningCinematic* o);

void opening_update(OpeningCinematic* o, uint32_t dt_ms);

/* True while still open. One key/click skips the MPS logo into the sailing
 * scene. Two edges during sailing jump to the last frame. */
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
