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
 * Camera follows the ship, clamped to [0, 640]. Any key/click twice skips
 * (two edges so a single accidental tap does not dump the player into the
 * title menu). Same BIOS-tick pacing as CLOSING.EXE (~55 ms).
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

#define OPENING_FRAME_MS 55

/* Same id the title menu already uses (SOUND_TITLE_ID / "Natives"). */
#define OPENING_BGM_ID 0x33

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

/* True while still open. Two key/click edges set finished. */
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
