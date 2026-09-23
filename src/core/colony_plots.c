#include "core/colony.h"

/*
 * Sections:
 *  - Colony pool accessors, field/plot geometry & DOS plot-blocked mask (colonies_get .. colonies_plot_blocked_mask)
 *  - Colonist tile slots, school tiers & profession labels/teaching rules (colonies_colonist_tile .. colonies_profession_name)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/ai_euro.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/ai_diplo.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/units_cargo.h"
#include "platform/diagnostics.h"

#include "core/colony_internal.h"

/* ===================== Colony pool accessors, field/plot geometry & DOS plot-blocked mask (colonies_get .. colonies_plot_blocked_mask) ===================== */
const ColonizeColony* colonies_get(const ColonizeColonyPool* pool, int colony_id) {
  return colonies_get_mut((ColonizeColonyPool*)pool, colony_id);
}

ColonizeColony* colonies_get_mut(ColonizeColonyPool* pool, int colony_id) {
  if (!pool || colony_id < 0) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].id == colony_id) {
      return &pool->colonies[i];
    }
  }
  return NULL;
}

int colonies_id_at(const ColonizeColonyPool* pool, int x, int y) {
  if (!pool) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].x == x && pool->colonies[i].y == y) {
      return pool->colonies[i].id;
    }
  }
  return -1;
}

/*
 * Runtime plot slots. 0..7 are the port's own clockwise ring
 * N, NE, E, SE, S, SW, W, NW; 8..19 are the DOS outer plots in the DS:0xc8 /
 * DS:0xde order (slots 8..19 of those tables), so the runtime array and the
 * save's colony+0x70 array agree above 7 and col1_bridge only has to remap
 * 0..7.
 *
 * How many of these a colony actually works is per colony, not 8: DOS reads
 * `DS:0x329[FUN_15eb_0470()]` over {0,4,8,12,20} with
 * `FUN_15eb_0470 = min(FUN_15eb_039e(10),2)+2` (raw 9636-9645 / 9561-9578).
 * See colonies_work_plot_count(). bugs.md #593.
 */
static const int k_field_dx[COLONIZE_COLONY_FIELD_TILES_MAX] = {
  0, 1, 1, 1, 0, -1, -1, -1,
  0, 2, 0, -2, -1, 1, -1, 1, -2, -2, 2, 2
};
static const int k_field_dy[COLONIZE_COLONY_FIELD_TILES_MAX] = {
  -1, -1, 0, 1, 1, 1, 0, -1,
  -2, 0, 2, 0, -2, -2, 2, 2, -1, 1, -1, 1
};

bool colonies_field_tile_delta(int tile_index, int* out_dx, int* out_dy) {
  if (tile_index < 0 || tile_index >= COLONIZE_COLONY_FIELD_TILES_MAX) {
    return false;
  }
  if (out_dx) {
    *out_dx = k_field_dx[tile_index];
  }
  if (out_dy) {
    *out_dy = k_field_dy[tile_index];
  }
  return true;
}

int colonies_field_tile_index(int dx, int dy) {
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    if (k_field_dx[i] == dx && k_field_dy[i] == dy) {
      return i;
    }
  }
  return -1;
}

/*
 * DS:0xc8 / DS:0xde (VICEROY.EXE file offset 121248 + addr), 20 entries each:
 * the DOS work-plot delta tables. Index order is
 *   0..7  N, E, S, W, NW, NE, SE, SW      (the ring the port stores in tiles[])
 *   8..19 the outer ring (tiles[8..19], worked only at ring tier 3/4)
 * `FUN_15eb_05e2` (raw ~9838) searches them for a (dx,dy) pair, and
 * `FUN_15eb_28c8` (raw 12993) walks them in this order, so the earliest index
 * wins an equal-score tie. The port's own slot order (k_field_dx/dy above) is
 * the MAP_DIR8 clockwise one and is kept — col1_bridge.c maps between the two.
 * bugs.md #584.
 */
static const int8_t k_dos_plot_dx[20] = {
  0, 1, 0, -1, -1, 1, 1, -1, 0, 2, 0, -2, -1, 1, -1, 1, -2, -2, 2, 2
};
static const int8_t k_dos_plot_dy[20] = {
  -1, 0, 1, 0, -1, -1, 1, 1, -2, 0, 2, 0, -2, -2, 2, 2, -1, 1, -1, 1
};

int colonies_field_scan_order(int step) {
  if (step < 0 || step >= COLONIZE_COLONY_FIELD_TILES_MAX) {
    return -1;
  }
  return colonies_field_tile_index((int)k_dos_plot_dx[step], (int)k_dos_plot_dy[step]);
}

/* FUN_15eb_05e2 (raw ~9838): the DOS plot slot (0..19) for a delta, or -1. */
static int colonies_dos_plot_index(int dx, int dy) {
  for (int i = 0; i < 20; ++i) {
    if ((int)k_dos_plot_dx[i] == dx && (int)k_dos_plot_dy[i] == dy) {
      return i;
    }
  }
  return -1;
}

/* Is plot slot `dos_index` of `oc` worked? DOS reads colony+0x70+index. */
static bool colonies_dos_plot_worked(const ColonizeColony* oc, int dos_index) {
  if (dos_index < 0) {
    return false;
  }
  if (dos_index >= COLONIZE_COLONY_FIELD_TILES_MAX) {
    return false;
  }
  const int rti = colonies_field_tile_index(
    (int)k_dos_plot_dx[dos_index], (int)k_dos_plot_dy[dos_index]
  );
  return rti >= 0 && (int)oc->tiles[rti] >= 0;
}

/*
 * DOS-LITERAL FUN_15eb_0470 raw 9636-9645 + the DS:0x329 table (VICEROY.EXE
 * file offset 121248+0x329 = {0,4,8,12,20}).
 *
 * `0470` is `min(FUN_15eb_039e(10),2)+2`, and `039e(10)` (raw 9561-9578)
 * counts the owned rows of the @BUILDING chain that starts at row 10, walking
 * the next-row byte at `row*0xc - 0x707a`. That chain is rows 0x0a then 0x0b,
 * the two Town Hall upgrades of NAMES.TXT:177-178 — the port's Town Hall chain
 * above its first row. DOS's buildability gate FUN_15eb_3650 (raw ~13674)
 * hard-zeroes both rows (`if (local_e == 10) local_14 = 0;` and the same for
 * 0xb), so its own construction menu can never offer them and every colony
 * stock DOS produces stays at tier 2 / ring 8. A save can still carry the bits
 * (colony buildings mask bits 9-11 = @BUILDING rows 9/10/11), and then DOS
 * really does work 12 or 20 plots — hence the per-colony count.
 */
int colonies_work_plot_count(const ColonizeColonyPool* pool, const ColonizeColony* col) {
  static const int k_ring_by_tier[5] = {0, 4, 8, 12, 20};
  int owned = 0;
  if (pool && col) {
    if (colonies_has_building_row(pool, col, COLONY_BUILDING_TOWN_HALL_2)) {
      owned++;
    }
    if (colonies_has_building_row(pool, col, COLONY_BUILDING_TOWN_HALL_3)) {
      owned++;
    }
  }
  if (owned > 2) {
    owned = 2;
  }
  return k_ring_by_tier[owned + 2];
}

/*
 * DOS-LITERAL FUN_15eb_23f2 raw 12695-12800 — the work-plot "blocked" byte.
 * `FUN_15eb_268e` (raw 12806-12820) caches the whole 5x5 into DS:0x8df0; this
 * port computes one plot on demand (the cache is a DOS speed trick, not a
 * rule). Both consumers — the AI plot scan `FUN_15eb_28c8` (raw 12993) and the
 * human area-view click `FUN_2f2b_3fa6` (raw 50917) — require byte == 0.
 *
 * Bits, in DOS order:
 *   0x10  off the playable map, outside the colony's work radius, or
 *         unexplored by the colony's nation (an early `return 0x10`, so no
 *         other bit can be set with it);
 *   0x80  the tile is owned by another European nation (layer2 unit flag +
 *         layer3 owner nibble), is not water, and carries a fortified unit
 *         that is either armed (type attack > 1) or a Pioneer (DOS type 2)
 *         of a human-controlled nation. DOS also reveals that unit to the
 *         colony's nation here (`FUN_1427_0992`); the port's query is const
 *         and skips that side effect;
 *   0x02  Lost City Rumour tile (`FUN_137f_0598`);
 *   0x04  an Indian village stands on it;
 *   0x20  another colony's centre tile;
 *   0x40  the plot is already worked by another colony;
 *   0x08  the colony's own centre tile (unreachable through this entry point,
 *         which only takes the 8 ring slots).
 */
uint8_t colonies_plot_blocked_mask(
  const ColonizeWorld* w,
  const ColonizeColony* col,
  int tile_index
) {
  if (!w || !col || !col->active) {
    return 0x10u;
  }
  int dx = 0;
  int dy = 0;
  if (!colonies_field_tile_delta(tile_index, &dx, &dy)) {
    return 0x10u;
  }
  const ColonizeWorldMap* map = w->map;
  const int x = col->x + dx;
  const int y = col->y + dy;
  /* FUN_137f_000a: the playable interior. */
  if (!map || !map_coords_inset(map, x, y)) {
    return 0x10u;
  }
  /*
   * DOS-LITERAL FUN_137f_003c raw 6535-6557, called as
   * `FUN_137f_003c(|dx|,|dy|,FUN_15eb_0470())` (raw 12727): the work-radius
   * test. Tier 1 keeps |dx|+|dy| < 2, tier 2 adds the diagonals (the 3x3
   * block), tier 3 adds |dx|+|dy| < 3, tier 4 takes everything in the 5x5 but
   * the four corners. The tier is per colony (bugs.md #593), recovered here
   * from the plot count rather than assumed to be 2.
   */
  const int ring = colonies_work_plot_count(w->colonies, col);
  int tier = 2;
  if (ring <= 4) {
    tier = 1;
  } else if (ring <= 8) {
    tier = 2;
  } else if (ring <= 12) {
    tier = 3;
  } else {
    tier = 4;
  }
  const int adx = dx < 0 ? -dx : dx;
  const int ady = dy < 0 ? -dy : dy;
  bool in_radius = (adx + ady) < 2;
  if (tier != 1) {
    if (adx < 2 && ady < 2) {
      in_radius = true;
    }
    if (tier != 2) {
      in_radius = in_radius || (adx + ady) < 3;
      if (tier != 3 && (adx < 2 || ady < 2)) {
        in_radius = true;
      }
    }
  }
  if (!in_radius) {
    return 0x10u;
  }
  /* `3 < colony_nation || (FUN_137f_02f8(x,y) & (0x10 << nation))` — a native
   * settlement skips the fog test, a European colony needs the tile seen. */
  if (col->nation_id < 4 && !map_tile_seen_by(map, x, y, col->nation_id)) {
    return 0x10u;
  }
  uint8_t mask = 0;
  const ColonizeCol1Save* col1 = w->col1_ok ? w->col1 : NULL;
  /* 0x80: foreign-owned tile guarded by a fortified unit. */
  const size_t mi = (size_t)y * (size_t)map->width + (size_t)x;
  const int occ = map->layer2 ? (int)map->layer2[mi] : 0;
  const int owner =
    ((occ & (int)MAP_OCCUPANCY_HAS_UNIT) != 0 && map->layer3)
      ? (int)((map->layer3[mi] >> MAP_L3_OWNER_SHIFT) & MAP_L3_OWNER_MASK)
      : -1;
  if (owner >= 0 && owner != col->nation_id && owner < 4 &&
      !map_tile_is_water((ColonizeWorldMap*)map, x, y) && w->units) {
    const int pioneer_type = units_kind_type_index(w->units, UNITS_KIND_PIONEER);
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &w->units->units[i];
      if (!u->active || u->x != x || u->y != y) {
        continue;
      }
      if (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED) {
        continue; /* raw 12747: `+0x314c == 5 || == 6` */
      }
      const ColonizeUnitType* ut = units_type(w->units, u->type_index);
      bool blocks = (ut && ut->attack > 1); /* type table +0x5236 */
      if (!blocks && pioneer_type >= 0 && u->type_index == pioneer_type &&
          u->nation_id >= 0 && u->nation_id < 4 && col1 && col1->player &&
          col1->player[u->nation_id].control == 0) {
        blocks = true; /* raw 12745: DOS type 2, nation < 4, 0x543f == 0 */
      }
      if (blocks) {
        mask |= 0x80u;
      }
    }
  }
  /* 0x02: FUN_137f_0598 rumour tile. */
  if (map_dos_0598_rumour_tile(map, x, y)) {
    mask |= 0x02u;
  }
  /* 0x04: an Indian settlement record on the tile (DOS walks all of them). */
  if (col1 && col1_save_tribe_at(col1, x, y)) {
    mask |= 0x04u;
  }
  /* 0x20 / 0x40: every OTHER colony's centre and worked plots. */
  if (w->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* oc = &w->colonies->colonies[i];
      if (!oc->active || oc->id == col->id) {
        continue;
      }
      if (oc->x == x && oc->y == y) {
        mask |= 0x20u;
      }
      const int ddx = oc->x - x < 0 ? x - oc->x : oc->x - x;
      const int ddy = oc->y - y < 0 ? y - oc->y : oc->y - y;
      if (ddx < 3 && ddy < 3 &&
          colonies_dos_plot_worked(oc, colonies_dos_plot_index(x - oc->x, y - oc->y))) {
        mask |= 0x40u;
      }
    }
  }
  return mask;
}

/* ===================== Colonist tile slots, school tiers & profession labels/teaching rules (colonies_colonist_tile .. colonies_profession_name) ===================== */
int colonies_colonist_tile(const ColonizeColony* colony, int colonist_index) {
  if (!colony || colonist_index < 0) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    if ((int)colony->tiles[i] == colonist_index) {
      return i;
    }
  }
  return -1;
}

void colonies_clear_colonist_tile(ColonizeColony* col, int colonist_index) {
  if (!col) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    if ((int)col->tiles[i] == colonist_index) {
      col->tiles[i] = -1;
    }
  }
}

/*
 * NAMES.TXT @JOB **column 2** — the school tier (1..4) a profession needs.
 * DOS `FUN_364b_0688` (raw 57518) reads the loaded record at `-0x715a +
 * prof*8`, i.e. the third word of the stride-8 @JOB row, so the port reads
 * the same catalog column instead of compiling a copy of it (bugs.md #596,
 * data_vs_hardcoded Part D). 0 when the catalog or the row is missing.
 */
int colonies_job_school_tier(int profession) {
  if (profession < 0 || profession >= 28) {
    return 0;
  }
  const char* field = reports_names_field("JOB", profession, 2);
  if (!field) {
    return 0;
  }
  const int tier = atoi(field);
  return (tier >= 1 && tier <= 4) ? tier : 0;
}

int colonies_school_building_tier(
  const ColonizeColonyPool* pool,
  int building_type
) {
  if (!pool || building_type < 0 || building_type >= pool->building_type_count) {
    return 0;
  }
  switch (colonies_building_type_row(pool, building_type)) {
  case COLONY_BUILDING_UNIVERSITY:
    return 3;
  case COLONY_BUILDING_COLLEGE:
    return 2;
  case COLONY_BUILDING_SCHOOLHOUSE:
    return 1;
  default:
    return 0;
  }
}

/*
 * The school tier this colony actually OWNS: University 3, College 2,
 * Schoolhouse 1, none 0. DOS's work-assign validator
 * (thunk_FUN_1000_9808, overlays.c:60459-60484) never looks at the school row
 * the player clicked — it asks `FUN_1000_8bec(0xe/0xd/0xc)`, "does the colony
 * own this @BUILDING", both for the faculty cap (@UNIV3 3 / @COLLEGE2 2 /
 * @SCHOOL1 1 teachers) and for the teacher's own level requirement
 * (@NEEDUNIVERSITY / @NEEDCOLLEGE). DOS never clears the lower tiers, so a
 * colony with a University owns all three rows. bugs.md #580, #589.
 */
int colonies_school_owned_tier(const ColonizeColonyPool* pool, const ColonizeColony* col) {
  if (!pool || !col) {
    return 0;
  }
  static const int k_rows[3] = {
    COLONY_BUILDING_UNIVERSITY, COLONY_BUILDING_COLLEGE, COLONY_BUILDING_SCHOOLHOUSE
  };
  for (int i = 0; i < 3; ++i) {
    const int bi = colonies_building_row(pool, k_rows[i]);
    if (bi >= 0 && bi < pool->building_type_count && col->has_building[bi]) {
      return 3 - i;
    }
  }
  return 0;
}

/*
 * The DOS @JOB occupation a workable building employs, or -1. The mirror of
 * col1_bridge.c's building→occupation switch (the save byte DOS stores per
 * colonist): DOS has no per-building membership at all, so both the school
 * faculty cap and @MORETHANTHREE tally COLONISTS BY OCCUPATION
 * (`FUN_1000_8dfe` / `FUN_15eb_1376`), which spans a chain's tiers —
 * Church and Cathedral are one Preacher pool.
 */
int colonies_building_occupation(const ColonizeColonyPool* pool, int building_type) {
  if (!pool || building_type < 0 || building_type >= pool->building_type_count) {
    return -1;
  }
  switch (colonies_building_row_chain(colonies_building_type_row(pool, building_type))) {
    case COLONIES_CHAIN_RUM:         return 9;
    case COLONIES_CHAIN_TOBACCONIST: return 10;
    case COLONIES_CHAIN_WEAVER:      return 11;
    case COLONIES_CHAIN_FUR:         return 12;
    case COLONIES_CHAIN_CARPENTER:   return 13;
    case COLONIES_CHAIN_BLACKSMITH:  return 14;
    case COLONIES_CHAIN_ARMORY:      return 15;
    case COLONIES_CHAIN_CHURCH:      return 16;
    case COLONIES_CHAIN_TOWN_HALL:   return 17;
    case COLONIES_CHAIN_SCHOOL:      return COLONIES_JOB_TEACHER;
    default:                         return -1;
  }
}

/* Colonists working that occupation, skipping `except_index` (-1 = none). */
int colonies_occupation_worker_count(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int occupation,
  int except_index
) {
  if (!pool || !col || occupation < 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < col->colonist_count; ++i) {
    if (i == except_index) {
      continue;
    }
    const ColonizeColonist* c = &col->colonists[i];
    if (!c->active || c->building_type < 0) {
      continue;
    }
    if (colonies_building_occupation(pool, c->building_type) == occupation) {
      n++;
    }
  }
  return n;
}

int colonies_school_tier_shortfall(int profession, int building_tier) {
  if (building_tier <= 0) {
    return 0;
  }
  const int need = colonies_job_school_tier(profession);
  if (need != 2 && need != 3) {
    return 0;
  }
  if (need > building_tier) {
    return need;
  }
  return 0;
}

/*
 * DOS FUN_364b_0688 (viceroy_unpacked.c ~57519): a colonist may teach when the
 * @JOB school level of his specialty is below 4 — `local_8c = jobtable[prof].
 * level; if (local_8c < 4)`. Level 4 covers @JOB 18 "Teacher"/19 "Colonist"
 * (the two placeholder rows) plus Indentured Servant, Petty Criminal and
 * Indian Convert, so the old hand-written exclusion list was right about
 * everything except @JOB 18, which it wrongly admitted.
 */
bool colonies_profession_may_teach(int profession) {
  const int level = colonies_job_school_tier(profession);
  return level >= 1 && level <= 3;
}

/*
 * NAMES.TXT @JOB column 0, with the table below as the no-catalog fallback
 * (audit CO-33). The fallback also decides WHICH professions get a label at
 * all: 0x13 Colonist / 0x19 Ind. Servant / 0x1a Criminal / 0x1b Convert are
 * deliberately absent and answer "profession", matching DOS
 * FUN_15eb_0002's non-expert gate, so the live lookup is only consulted for
 * professions the fallback already names.
 */
/* Professions DOS labels at all (FUN_15eb_0002's non-expert gate): the field
 * jobs, the ten indoor trades and the five equipped roles. 0x13 Colonist /
 * 0x19 Ind. Servant / 0x1a Criminal / 0x1b Convert answer the port's neutral
 * "profession". The wording itself is NAMES.TXT @JOB column 0. */
static bool colonies_profession_is_labelled(int profession) {
  if (profession >= 0 && profession < COLONIZE_FIELD_JOB_COUNT) {
    return true;
  }
  switch (profession) {
  case COLONIZE_PROF_DISTILLER:
  case COLONIZE_PROF_TOBACCONIST:
  case COLONIZE_PROF_WEAVER:
  case COLONIZE_PROF_FUR_TRADER:
  case COLONIZE_PROF_CARPENTER:
  case COLONIZE_PROF_BLACKSMITH:
  case COLONIZE_PROF_GUNSMITH:
  case COLONIZE_PROF_PREACHER:
  case COLONIZE_PROF_STATESMAN:
  case COLONIZE_PROF_TEACHER:
  case UNITS_JOB_PIONEER:
  case UNITS_JOB_SOLDIER:
  case UNITS_JOB_SCOUT:
  /* bugs.md #656: 0x17 (UNITS_JOB_DRAGOON) dropped — DOS never writes it
   * to a unit (#503/#639), matching units.c's 0x15-only veteran gates. */
  case UNITS_JOB_MISSIONARY:
    return true;
  default:
    return false;
  }
}

const char* colonies_profession_name(int profession) {
  if (!colonies_profession_is_labelled(profession)) {
    return ""; /* no catalog row = empty string, never a typed fallback */
  }
  return reports_job_short_name(profession);
}

