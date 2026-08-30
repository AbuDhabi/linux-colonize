#ifndef COLONIZE_WOODCUT_H
#define COLONIZE_WOODCUT_H

#include <stdbool.h>
#include <stdint.h>

#include "core/col1_save.h"
#include "core/ff.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Milestone woodcut screens — DOS FUN_12fd_006c (dispatch + once-only gate)
 * and FUN_6f30_0062 (presenter, reached through the thunk FUN_281f_052e).
 *
 * Trigger sites call FUN_281f_0524(id) = FUN_12fd_006c(id), which:
 *   1. tests bit `id` of the 32-bit once-only array at DS:0x540a
 *      (FUN_12fd_0048); if already set it returns immediately,
 *   2. otherwise sets the bit (FUN_12fd_000e) and runs a small jump table
 *      that picks the tune for this id,
 *   3. then falls through to FUN_6f30_0062(id), which draws the screen.
 *
 * The array at DS:0x540a is `ColonizeCol1Head.event` (bits 1-16) followed by
 * `unknown05[2]` (bits 17-32), so the "already shown" state round-trips
 * through the save exactly as DOS stores it. New game clears all four bytes.
 *
 * Screen layout, all from FUN_6f30_0062:
 *   WOODFRAM.SS  frame, centred by FUN_6f30_002e (sprite anchor)
 *   WDCUT%02d.SS the woodcut art for this id, centred the same way
 *   NAMEPLAT.SS  three-piece nameplate (left cap / repeated middle / right
 *                cap) laid out at y=0xa2, wide enough for the caption
 *   FONT-NP.FF   caption text at y=0xa5, shades {0x5c, 0x5e, 0x5d}
 * The caption is line `id` of WOODCUT.TXT's @WOODCUT section (line 0,
 * "A NEW WORLD", belongs to id 0 — the demo-autoplay intro).
 *
 * DOS builds "<year>: <caption>" into the draw buffer and then immediately
 * strcpy()s the bare caption over it (6f30:0252), so the year prefix never
 * reaches the screen; the port draws what DOS draws.
 */

/*
 * Woodcut ids. 1-13 are the ones normal play can reach; 14-16 exist as
 * event-flag bits with placeholder WOODCUT.TXT captions and no art, and ids
 * 17-25 (unknown05) are only reachable from the DOS demo-autoplay loop.
 * Id 12 (COLONY DESTROYED) has art and a caption but no call site anywhere in
 * the shipped executable — both DOS colony-loss sites push 11.
 */
typedef enum ColonizeWoodcutId {
  WOODCUT_A_NEW_WORLD = 0,
  WOODCUT_DISCOVERY_OF_THE_NEW_WORLD = 1,
  WOODCUT_BUILDING_A_COLONY = 2,
  WOODCUT_MEETING_THE_NATIVES = 3,
  WOODCUT_THE_AZTEC_EMPIRE = 4,
  WOODCUT_THE_INCA_NATION = 5,
  WOODCUT_DISCOVERY_OF_THE_PACIFIC_OCEAN = 6,
  WOODCUT_ENTERING_INDIAN_VILLAGE = 7,
  WOODCUT_THE_FOUNTAIN_OF_YOUTH = 8,
  WOODCUT_CARGO_FROM_THE_NEW_WORLD = 9,
  WOODCUT_MEETING_FELLOW_EUROPEANS = 10,
  WOODCUT_COLONY_BURNING = 11,
  WOODCUT_COLONY_DESTROYED = 12,
  WOODCUT_INDIAN_RAID = 13
} ColonizeWoodcutId;

#define WOODCUT_ID_MAX 32
#define WOODCUT_QUEUE_MAX 8
#define WOODCUT_CAPTION_LEN 96

/* DOS literals from FUN_6f30_0062. */
#define WOODCUT_PLATE_Y 0xa2
#define WOODCUT_CAPTION_Y 0xa5
#define WOODCUT_CENTRE_X 0xa0

typedef struct ColonizeWoodcutScreen {
  bool open;
  int id;
  ColonizePalette palette;
  bool palette_ok;
  char caption[WOODCUT_CAPTION_LEN];
  uint8_t canvas[320 * 200];
} ColonizeWoodcutScreen;

/*
 * Tune hooks (FUN_129f_02cc / FUN_129f_0318), installed by the game loop.
 * Kept as function pointers so leaf modules can arm a woodcut without
 * dragging sound.c into the standalone unit_* test binaries — same shape as
 * units_set_combat_music_hooks.
 */
typedef void (*ColonizeWoodcutSoundFn)(int id);
void woodcut_set_sound_hooks(ColonizeWoodcutSoundFn play_fn, ColonizeWoodcutSoundFn set_bgm_fn);

/*
 * FUN_12fd_006c: once-only gate on the DS:0x540a bit array plus the tune
 * pick, then queue the screen for the game loop to present. Returns true when
 * this call actually armed a woodcut (bit was clear). `col1` may be NULL, in
 * which case the gate is skipped and the screen always fires.
 */
bool woodcut_fire(ColonizeCol1Save* col1, int id);

/* Queue a woodcut with no once-only gate (DOS negative-id form). */
void woodcut_request(int id);

bool woodcut_has_pending(void);
/* Pop the oldest queued id, or -1 when the queue is empty. */
int woodcut_take_pending(void);
void woodcut_clear_pending(void);

/*
 * FUN_6f30_0062: build the screen for `id`. False (and open == false) when
 * the art or the frame is missing — the caller then just skips the woodcut.
 */
bool woodcut_open(ColonizeWoodcutScreen* w, const char* data_dir, int id);
void woodcut_close(ColonizeWoodcutScreen* w);

/* Any key or click dismisses (FUN_281f_03ea wait). True while still open. */
bool woodcut_handle_input(ColonizeWoodcutScreen* w, const ColonizeInputState* input);

void woodcut_render(
  const ColonizeWoodcutScreen* w,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* palette
);

#endif
