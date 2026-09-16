#ifndef COLONIZE_COL1_SAVE_H
#define COLONIZE_COL1_SAVE_H

/*
 * Original Sid Meier's Colonization (DOS 3.0) COLONY##.SAV layout.
 *
 * Layout derived from the community reverse-engineering in
 * hegemogy/viceroy (savegame.h), hegemogy/Colonization-SAV-files
 * (Format.md), and pavelbel/smcol_saves_utility — cross-checked
 * against section sizes in Format.md.
 *
 * File order:
 *   head (158) + player[4] (208) + other (24)           = 390 prefix
 *   colony[colony_count]   @ 202 bytes each
 *   unit[unit_count]       @  28 bytes each
 *   nation[4]              @ 316 bytes each (= 1264)
 *   tribe[tribe_count]     @  18 bytes each   (Indian villages)
 *   indian[8]              @  78 bytes each   (= 624)
 *   stuff                  @ 727 bytes (= 33 discrete DS writes in FUN_75c2_0288)
 *   map.tile/mask/path/seen @ map_w * map_h each (standard 58x72)
 *   post_map               @ 614 (= sea/land connectivity 2×270 + tallies + tail)
 *   trade_route[12]        @  74 bytes each (= 888)
 *
 * Post-map is NOT “28×18 mystery records”: FUN_67f4_0088 builds two 15×18
 * (pitch 18) neighbor-bitmask planes at DS:0x86f6 (sea) and DS:0x85e8 (land);
 * FUN_75c2_0288 writes them then continent tallies + a 10-byte tail.
 *
 * Multi-byte integers are little-endian. Bitfields assume GCC/Clang
 * LSB-first packing on little-endian hosts (matches DOS saves).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/col1_save_layout.h"


typedef struct ColonizeCol1Map {
  uint16_t width;
  uint16_t height;
  size_t tile_count;
  uint8_t* tile; /* ColonizeCol1Tile bits as bytes */
  uint8_t* mask;
  uint8_t* path;
  uint8_t* seen;
} ColonizeCol1Map;

/*
 * Full original savegame state. Variable-length sections are heap-owned
 * when loaded via col1_save_read_* / col1_save_alloc_empty.
 */
typedef struct ColonizeCol1Save {
  ColonizeCol1Head head;
  ColonizeCol1Player player[COLONIZE_COL1_NATION_COUNT];
  /* DS:0x948e × 0x18 — save R/W only in unpacked VICEROY.
   * Smcol: unknown[18] + click_before_open_colony x,y + unknown[2]. */
  uint8_t other[COLONIZE_COL1_OTHER_SIZE];
  ColonizeCol1Colony* colony;
  ColonizeCol1Unit* unit;
  ColonizeCol1Nation nation[COLONIZE_COL1_NATION_COUNT];
  ColonizeCol1Tribe* tribe;
  /*
   * The old `int16_t* indian_tension` parallel array lived here until
   * 2026-09-08. DS:0x54f6 is field +10 of the settlement record itself
   * (`ColonizeCol1Tribe.alarm[euro]`, see col1_tribe_attitude above), which
   * IS part of the saved tribe blob — the separate array was a misreading,
   * was never serialized and always read back as zeros.
   */
  ColonizeCol1Indian indian[COLONIZE_COL1_INDIAN_COUNT];
  ColonizeCol1Stuff stuff;
  ColonizeCol1Map map;
  ColonizeCol1PostMap post_map;
  ColonizeCol1TradeRoute trade_route[COLONIZE_COL1_TRADE_ROUTE_COUNT];
  bool owned; /* true if colony/unit/tribe/map buffers owned by this struct */
} ColonizeCol1Save;

void col1_save_init(ColonizeCol1Save* save);
void col1_save_free(ColonizeCol1Save* save);

/*
 * ---------------------------------------------------------------------
 * Settlement-at-tile accessors (2026-09-14 duplication audit IN-25 / AC-35 /
 * TT-7). The tribe[] walk had eight private copies (map_panel_tribe_at,
 * col1_bridge_tribe_at, col1_stuff_census_settlement_at, combat_tribe_at,
 * two inline in ai_contact.c, ai.c, two in ai_euro.c) and the colony[] walk
 * four more, three of them in the golden tests.
 *
 * TWO tribe spellings, because the eight copies were not all the same:
 *
 *   col1_save_tribe_at        — any record on the tile, no owner filter.
 *     This is the DOS FUN_281f_06be / FUN_137f_03e4 reading: the tile owner
 *     is a single nibble that already holds 0..3 for a Euro colony and
 *     4..11 for a village, so the settlement probe returns whatever is
 *     stored. Use it for the tile-owner / settlement-bit ports.
 *   col1_save_village_at      — same walk plus `nation_id` in 4..11.
 *     The encounter and village-target paths (ai_contact's 1736 probe,
 *     ai_euro_village_nation_at) added this as a record-validity guard so a
 *     malformed record cannot be mistaken for a village. Use it wherever
 *     the answer feeds `col1->indian[nation_id - 4]`.
 *
 * All four return NULL / -1 when there is nothing there; none of them
 * touches `save->head.tribe_count` beyond bounding the walk.
 * ---------------------------------------------------------------------
 */
static inline const ColonizeCol1Tribe* col1_save_tribe_at(
  const ColonizeCol1Save* save, int x, int y
) {
  if (!save || !save->tribe) {
    return NULL;
  }
  for (uint16_t i = 0; i < save->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &save->tribe[i];
    if ((int)t->x == x && (int)t->y == y) {
      return t;
    }
  }
  return NULL;
}

static inline const ColonizeCol1Tribe* col1_save_village_at(
  const ColonizeCol1Save* save, int x, int y
) {
  if (!save || !save->tribe) {
    return NULL;
  }
  for (uint16_t i = 0; i < save->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &save->tribe[i];
    if ((int)t->x == x && (int)t->y == y && t->nation_id >= 4 && t->nation_id <= 11) {
      return t;
    }
  }
  return NULL;
}

/* Index into save->tribe[] of the tile's settlement record, or -1 — the
 * shape col1_bridge.c's copy returned. */
static inline int col1_save_tribe_index_at(const ColonizeCol1Save* save, int x, int y) {
  if (!save || !save->tribe) {
    return -1;
  }
  for (uint16_t i = 0; i < save->head.tribe_count; ++i) {
    if ((int)save->tribe[i].x == x && (int)save->tribe[i].y == y) {
      return (int)i;
    }
  }
  return -1;
}

/* First settlement record belonging to `nation` (absolute Col1 id 4..11),
 * or NULL. The tribe capital lookup several callers hand-rolled. */
static inline const ColonizeCol1Tribe* col1_tribe_first_of(
  const ColonizeCol1Save* save, int nation
) {
  if (!save || !save->tribe) {
    return NULL;
  }
  for (uint16_t i = 0; i < save->head.tribe_count; ++i) {
    if ((int)save->tribe[i].nation_id == nation) {
      return &save->tribe[i];
    }
  }
  return NULL;
}

/*
 * Colony record on (x,y), or NULL. `nation` < 0 accepts any owner (the
 * golden tests' find_colony_by_xy); 0..3 filters to that nation's own
 * colonies (reports.c's own-colony tile test).
 */
static inline const ColonizeCol1Colony* col1_colony_at_xy(
  const ColonizeCol1Save* save, int nation, int x, int y
) {
  if (!save) {
    return NULL;
  }
  for (uint16_t i = 0; i < save->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &save->colony[i];
    if ((int)c->x != x || (int)c->y != y) {
      continue;
    }
    if (nation >= 0 && (int)c->nation_id != nation) {
      continue;
    }
    return c;
  }
  return NULL;
}

/* Index form of col1_colony_at_xy, for callers that need the array slot
 * (the three golden colony-production tests). -1 when absent. */
static inline int col1_save_colony_at(
  const ColonizeCol1Save* save, int nation, int x, int y
) {
  if (!save) {
    return -1;
  }
  for (uint16_t i = 0; i < save->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &save->colony[i];
    if ((int)c->x != x || (int)c->y != y) {
      continue;
    }
    if (nation >= 0 && (int)c->nation_id != nation) {
      continue;
    }
    return (int)i;
  }
  return -1;
}

/* Compile-time layout checks (also run at runtime in smoke tests). */
bool col1_save_check_layout(char* err, size_t err_size);

/*
 * FUN_75c2_0840 header probe: COLONIZE sig + 0x1A + version + optional map size.
 * expect_map_w/h < 0 → skip LOADSIZE check (title / no live map).
 * Error text mirrors @LOADNOT / @LOADOLD / @LOADSIZE.
 */
bool col1_save_validate_head(
  const ColonizeCol1Head* head,
  int expect_map_w,
  int expect_map_h,
  char* err,
  size_t err_size
);

/* Ensure DOS signature / EOF / version fields before write. */
void col1_save_stamp_head(ColonizeCol1Head* head);

/* crown_nation_id / rival_nation_slot_1 / _2 back to -1 ("none"), as
 * FUN_75c2_235c does at new game. Zero means "England" to every reader. */
void col1_save_reset_nation_slots(ColonizeCol1Head* head);

/* Human nation = the control==0 player slot (authoritative). Falls back to
 * head.human_player (DS:0x5398) — older port saves left that field stale 0.
 * Always returns 0..COLONIZE_COL1_NATION_COUNT-1: every caller indexes
 * nation[] / player[] with it. */
int col1_save_human_nation(const ColonizeCol1Save* save);

/* Same probe over a raw head + player table, for the slot probe that reads
 * only the file prefix (savegame_probe_col1_slot). One implementation so the
 * two cannot drift on the failure path (smell audit #76). */
int col1_save_human_nation_from(
  const ColonizeCol1Head* head,
  const ColonizeCol1Player* players
);

size_t col1_save_expected_size(const ColonizeCol1Save* save);
size_t col1_save_expected_size_counts(
  uint16_t map_w,
  uint16_t map_h,
  uint16_t colony_count,
  uint16_t unit_count,
  uint16_t tribe_count
);

bool col1_save_read_file(
  const char* path,
  ColonizeCol1Save* out,
  char* err,
  size_t err_size
);
bool col1_save_write_file(
  const char* path,
  const ColonizeCol1Save* save,
  char* err,
  size_t err_size
);

bool col1_save_read_memory(
  const uint8_t* data,
  size_t size,
  ColonizeCol1Save* out,
  char* err,
  size_t err_size
);
bool col1_save_write_memory(
  const ColonizeCol1Save* save,
  uint8_t** out_data,
  size_t* out_size,
  char* err,
  size_t err_size
);

/* Allocate empty standard-size map buffers; counts must already be set in head. */
bool col1_save_alloc_sections(ColonizeCol1Save* save, char* err, size_t err_size);

#endif
