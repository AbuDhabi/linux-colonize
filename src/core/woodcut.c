#include "core/woodcut.h"
#include "core/woodcut_present.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/cinematic.h"
#include "core/font.h"
#include "platform/diagnostics.h"

/*
 * `event` (bits 1-16) and `unknown05` (bits 17-32) are the one 32-bit array
 * DOS addresses as DS:0x540a; the byte-level accessors below only work if
 * they really are adjacent in the packed header.
 */
_Static_assert(
  offsetof(ColonizeCol1Head, unknown05) == offsetof(ColonizeCol1Head, event) + 2,
  "woodcut once-only array must be event[2] + unknown05[2]"
);
ColonizeWoodcutSoundFn g_woodcut_play;
ColonizeWoodcutSoundFn g_woodcut_set_bgm;

void woodcut_set_sound_hooks(ColonizeWoodcutSoundFn play_fn, ColonizeWoodcutSoundFn set_bgm_fn) {
  g_woodcut_play = play_fn;
  g_woodcut_set_bgm = set_bgm_fn;
}

/* ---------------------------------------------------------------- queue -- */

static int g_woodcut_queue[WOODCUT_QUEUE_MAX];
static int g_woodcut_queue_len;

static void woodcut_request(int id) {
  if (id < 0 || id >= WOODCUT_ID_MAX) {
    return;
  }
  if (g_woodcut_queue_len >= WOODCUT_QUEUE_MAX) {
    diag_warn("woodcut: queue full, dropping id %d", id);
    return;
  }
  for (int i = 0; i < g_woodcut_queue_len; ++i) {
    if (g_woodcut_queue[i] == id) {
      return; /* already armed this turn */
    }
  }
  g_woodcut_queue[g_woodcut_queue_len++] = id;
}

bool woodcut_has_pending(void) {
  return g_woodcut_queue_len > 0;
}

int woodcut_take_pending(void) {
  if (g_woodcut_queue_len <= 0) {
    return -1;
  }
  const int id = g_woodcut_queue[0];
  for (int i = 1; i < g_woodcut_queue_len; ++i) {
    g_woodcut_queue[i - 1] = g_woodcut_queue[i];
  }
  g_woodcut_queue_len--;
  return id;
}

void woodcut_clear_pending(void) {
  g_woodcut_queue_len = 0;
}

/* ------------------------------------------------- FUN_12fd_006c gate --- */

/* FUN_12fd_0048 / FUN_12fd_000e: 1-based bit `id` of the DS:0x540a array. */
static uint8_t* woodcut_bit_byte(ColonizeCol1Save* col1, int id) {
  uint8_t* base = (uint8_t*)&col1->head.event;
  return base + ((id - 1) >> 3);
}

static uint8_t woodcut_bit_mask(int id) {
  return (uint8_t)(1u << ((unsigned)(id - 1) & 7u));
}

/*
 * The FUN_12fd_006c jump table: every case falls through to the presenter,
 * the only per-id work is which tune is queued first. Cases 0/1/9 hit
 * FUN_129f_0318 and case 2 FUN_129f_034c — both are "switch to tune pool 2,
 * restart if it changed" (sound_set_bgm). Cases 3-6 push an explicit event id
 * through FUN_129f_02cc (sound_play). Cases 7, 8, 10 and every id above 10
 * have no table entry and play nothing.
 */
static void woodcut_play_event(int id) {
  if (g_woodcut_play) {
    g_woodcut_play(id);
  }
}

static void woodcut_play_tune(int id) {
  switch (id) {
    /* WOODCUT_A_NEW_WORLD (0) never reaches here: woodcut_fire rejects
     * id <= 0 (the once-only bitfield is 1-based), so DOS case 0 is
     * unreachable in the port and deliberately not listed. */
    case WOODCUT_DISCOVERY_OF_THE_NEW_WORLD:
    case WOODCUT_BUILDING_A_COLONY:
    case WOODCUT_CARGO_FROM_THE_NEW_WORLD:
      if (g_woodcut_set_bgm) {
        g_woodcut_set_bgm(2);
      }
      break;
    case WOODCUT_MEETING_THE_NATIVES: woodcut_play_event(0x33); break;
    case WOODCUT_THE_AZTEC_EMPIRE: woodcut_play_event(0x35); break;
    case WOODCUT_THE_INCA_NATION: woodcut_play_event(0x36); break;
    case WOODCUT_DISCOVERY_OF_THE_PACIFIC_OCEAN: woodcut_play_event(0x39); break;
    default: break;
  }
}

bool woodcut_fire(ColonizeCol1Save* col1, int id) {
  if (id <= 0 || id >= WOODCUT_ID_MAX) {
    return false;
  }
  if (col1) {
    uint8_t* p = woodcut_bit_byte(col1, id);
    const uint8_t mask = woodcut_bit_mask(id);
    if ((*p & mask) != 0) {
      return false;
    }
    *p = (uint8_t)(*p | mask);
  }
  woodcut_play_tune(id);
  woodcut_request(id);
  return true;
}
