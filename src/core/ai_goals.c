#include "core/ai_goals.h"

#include "core/ai_diplo.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/map.h"
#include "core/units.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static AiNationGoals s_goals[4];
static AiWorkSlot s_work[AI_WORK_SLOTS];
static AiEuroInventory s_inv[4];
static AiNationPlanScratch s_plan[4];

static const int k_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

void ai_goals_reset(void) {
  memset(s_goals, 0, sizeof(s_goals));
  memset(s_work, 0, sizeof(s_work));
  memset(s_inv, 0, sizeof(s_inv));
  memset(s_plan, 0, sizeof(s_plan));
  for (int n = 0; n < 4; ++n) {
    for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
      s_goals[n].primary[i].code = AI_GOAL_EMPTY;
    }
    for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
      s_goals[n].secondary[i].code = AI_GOAL_EMPTY;
    }
  }
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    s_work[i].id = -1;
  }
}

AiEuroInventory* ai_goals_inventory(int nation_id) {
  if (nation_id < 0 || nation_id >= 4) {
    return NULL;
  }
  return &s_inv[nation_id];
}

void ai_goals_inventory_clear(int nation_id) {
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  if (inv) {
    memset(inv, 0, sizeof(*inv));
  }
}

void ai_goals_clear_primary_slot(int nation_id, int slot) {
  if (nation_id < 0 || nation_id >= 4 || slot < 0 || slot >= AI_PRIMARY_SLOTS) {
    return;
  }
  s_goals[nation_id].primary[slot].code = AI_GOAL_EMPTY;
  s_goals[nation_id].primary[slot].prio = 0;
}

void ai_goals_clear_secondary_slots(int nation_id) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    s_goals[nation_id].secondary[i].code = AI_GOAL_EMPTY;
    s_goals[nation_id].secondary[i].prio = 0;
  }
}

/*
 * FUN_521d_001c — invalidate_nearby_secondary_goals (decomp 86786-86816).
 * Distance callee resolved 2026-09-07: FUN_281f_037a is a far thunk to
 * FUN_124c_007c (decomp 2776-2792), which absolutizes both deltas and tail-
 * calls FUN_124c_0040 (decomp 2759-2774) = `(min >> 1) + max`. That is
 * exactly the formula already coded here, so this is now an identity, not a
 * stand-in. (FUNCTION_CATALOG's "Manhattan |dx|+|dy|" note for 037a is
 * wrong — 0040's body has the `>>1` term.)
 */
void ai_goals_invalidate_nearby_secondary(int nation_id, int code, int x, int y, int radius) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    AiGoalSlot* s = &s_goals[nation_id].secondary[i];
    if ((int)s->code != code) {
      continue;
    }
    int dx = x - (int)s->x;
    int dy = y - (int)s->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    const int d = (dy < dx) ? (dy >> 1) + dx : (dx >> 1) + dy;
    if (d <= radius) {
      s->code = AI_GOAL_EMPTY;
      s->prio = 0;
    }
  }
}

/* FUN_521d_0072 — open hole at slot by shifting [slot..62] down. */
static void primary_shift_down(int nation_id, int slot) {
  for (int i = AI_PRIMARY_SLOTS - 2; i >= slot; --i) {
    s_goals[nation_id].primary[i + 1] = s_goals[nation_id].primary[i];
  }
}

static void secondary_shift_down(int nation_id, int slot) {
  for (int i = AI_SECONDARY_SLOTS - 2; i >= slot; --i) {
    s_goals[nation_id].secondary[i + 1] = s_goals[nation_id].secondary[i];
  }
}

static void work_shift_down(int slot) {
  for (int i = AI_WORK_SLOTS - 2; i >= slot; --i) {
    s_work[i + 1] = s_work[i];
  }
}

void ai_goals_upsert_primary(int nation_id, int x, int y, int code, int prio) {
  if (nation_id < 0 || nation_id >= 4 || code < 0 || code == (int)AI_GOAL_EMPTY) {
    return;
  }
  AiNationGoals* g = &s_goals[nation_id];
  /* Reject if matching (x,y,code) already has prio ≥ new. */
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    AiGoalSlot* s = &g->primary[i];
    if (s->x == (int8_t)x && s->y == (int8_t)y && s->code == (uint8_t)code &&
        prio <= (int)s->prio) {
      return;
    }
  }
  /* Insert at first slot with lower prio or empty code (priority-ordered). */
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    AiGoalSlot* s = &g->primary[i];
    if ((int)s->prio < prio || s->code == AI_GOAL_EMPTY) {
      primary_shift_down(nation_id, i);
      s = &g->primary[i];
      s->x = (int8_t)x;
      s->y = (int8_t)y;
      s->code = (uint8_t)code;
      s->prio = (uint8_t)(prio > 255 ? 255 : prio);
      return;
    }
  }
}

void ai_goals_upsert_secondary(int nation_id, int x, int y, int code, int prio) {
  if (nation_id < 0 || nation_id >= 4 || code < 0 || code == (int)AI_GOAL_EMPTY) {
    return;
  }
  AiNationGoals* g = &s_goals[nation_id];
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    AiGoalSlot* s = &g->secondary[i];
    if (s->x == (int8_t)x && s->y == (int8_t)y && s->code == (uint8_t)code &&
        prio <= (int)s->prio) {
      return;
    }
  }
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    AiGoalSlot* s = &g->secondary[i];
    if ((int)s->prio < prio || s->code == AI_GOAL_EMPTY) {
      secondary_shift_down(nation_id, i);
      s = &g->secondary[i];
      s->x = (int8_t)x;
      s->y = (int8_t)y;
      s->code = (uint8_t)code;
      s->prio = (uint8_t)(prio > 255 ? 255 : prio);
      return;
    }
  }
}

void ai_goals_promote_secondary_to_primary(int nation_id) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  /* FUN_521d_0342: clear all 64 primaries, then upsert each live secondary. */
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    ai_goals_clear_primary_slot(nation_id, i);
  }
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    AiGoalSlot* s = &s_goals[nation_id].secondary[i];
    if ((int8_t)s->code >= 0) {
      ai_goals_upsert_primary(nation_id, s->x, s->y, (int)s->code, (int)s->prio);
    }
  }
}

/*
 * FUN_521d_031c (decomp 86996-87008). DOS writes only `id = 0xffff` for the
 * 16 slots and leaves score/loads/military stale; every reader (02be's
 * insert scan, the 4393 consumer) tests `id < 0` first, so clearing the rest
 * here is observably identical and keeps the port's own dumps clean.
 */
void ai_goals_clear_work_queue(void) {
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    s_work[i].id = -1;
    s_work[i].score = 0;
    s_work[i].loads = 0;
    s_work[i].military = 0;
  }
}

void ai_goals_upsert_work(int id, int score, uint8_t loads, uint8_t military) {
  /* FUN_521d_02be: score-ordered insert when score > occupant or id < 0. */
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    if (s_work[i].score < score || s_work[i].id < 0) {
      work_shift_down(i);
      s_work[i].id = (int16_t)id;
      s_work[i].score = (int16_t)score;
      s_work[i].loads = loads;
      s_work[i].military = military;
      return;
    }
  }
}

void ai_goals_work_consume(int slot, int free_holds) {
  if (slot < 0 || slot >= AI_WORK_SLOTS) {
    return;
  }
  AiWorkSlot* w = &s_work[slot];
  if (w->id < 0 || w->loads == 0) {
    return;
  }
  if (free_holds < 0) {
    free_holds = 0;
  }
  /*
   * DOS: iStack_9a = rec[+4] - unit_type_hold_capacity(0x5237) +
   * unit->holds_occupied(+0x3150), floored at 0 -- i.e. loads minus the
   * hauler's *free* holds. The score write-back divides by the pre-claim
   * load count, so it is proportional, and a fully served colony's slot is
   * released the same beat.
   */
  int remaining = (int)w->loads - free_holds;
  if (remaining < 0) {
    remaining = 0;
  }
  w->score = (int16_t)(((int)w->score * remaining) / (int)w->loads);
  w->loads = (uint8_t)remaining;
  if (remaining == 0) {
    w->id = -1;
  }
}

int ai_goals_max_primary_prio(int nation_id, int x, int y, int code) {
  if (nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  int best = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* s = &s_goals[nation_id].primary[i];
    if (s->x == (int8_t)x && s->y == (int8_t)y && s->code == (uint8_t)code &&
        (int)s->prio >= best) {
      best = (int)s->prio;
    }
  }
  return best;
}

const AiGoalSlot* ai_goals_primary(int nation_id, int slot) {
  if (nation_id < 0 || nation_id >= 4 || slot < 0 || slot >= AI_PRIMARY_SLOTS) {
    return NULL;
  }
  return &s_goals[nation_id].primary[slot];
}

const AiWorkSlot* ai_goals_work(int slot) {
  if (slot < 0 || slot >= AI_WORK_SLOTS) {
    return NULL;
  }
  return &s_work[slot];
}

/*
 * Nearest water tile a loaded transport can actually land settlers from.
 *
 * Scenario maps (@SCENARIO) start each Euro fleet on the Atlantic high-seas
 * rim with no landfall target of its own, and the seed-100 landfall tables the
 * Euro planner keys most of its first-colony geometry off do not resolve
 * anywhere else. Without a target the ship sits on its spawn tile with the
 * colonists aboard forever. This is the map-agnostic fallback: ring-scan out
 * from `from` for a water/high-seas tile that is free and has a land neighbour
 * a colony could be founded on. Nearest ring wins; inside a ring prefer the
 * westward tile (America lies west of every Atlantic start), then the one
 * closest to the starting latitude.
 */
int ai_goals_nearest_landing_water(
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  int from_x,
  int from_y,
  int max_radius,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y || from_x < 0 || from_y < 0) {
    return 0;
  }
  static const int k_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int k_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  if (max_radius < 1) {
    max_radius = 1;
  }
  for (int r = 1; r <= max_radius; ++r) {
    int best_x = -1;
    int best_y = -1;
    int best_key = INT_MAX;
    for (int y = from_y - r; y <= from_y + r; ++y) {
      for (int x = from_x - r; x <= from_x + r; ++x) {
        const int ax = (x > from_x) ? x - from_x : from_x - x;
        const int ay = (y > from_y) ? y - from_y : from_y - y;
        if ((ax > ay ? ax : ay) != r) {
          continue; /* ring edge only */
        }
        if (x < 0 || y < 0 || x >= (int)map->width || y >= (int)map->height) {
          continue;
        }
        if (!map_tile_is_water(map, x, y) && !map_tile_is_high_seas(map, x, y)) {
          continue;
        }
        if (units && units_id_at(units, x, y) >= 0) {
          continue; /* another ship is parked there */
        }
        int landable = 0;
        for (int d = 0; d < 8; ++d) {
          const int lx = x + k_dx[d];
          const int ly = y + k_dy[d];
          if (lx < 0 || ly < 0 || lx >= (int)map->width || ly >= (int)map->height) {
            continue;
          }
          if (!map_tile_is_land(map, lx, ly) || map_tile_is_water(map, lx, ly)) {
            continue;
          }
          if (colonies && !colonies_can_found(colonies, map, lx, ly)) {
            continue;
          }
          landable = 1;
          break;
        }
        if (!landable) {
          continue;
        }
        /* West first, then nearest latitude. */
        const int key = x * 256 + ay;
        if (key < best_key) {
          best_key = key;
          best_x = x;
          best_y = y;
        }
      }
    }
    if (best_x >= 0) {
      *out_x = best_x;
      *out_y = best_y;
      return 1;
    }
  }
  return 0;
}

int ai_goals_best_found_tile(int nation_id, int* out_x, int* out_y) {
  /* Position-free query (CHEAT colony-site overlay): no distance tiebreak. */
  return ai_goals_best_found_tile_near(NULL, nation_id, -1, -1, out_x, out_y);
}

/*
 * Priority-then-distance FOUND pick.
 *
 * The primary table is priority-ordered, so taking the first FOUND slot is
 * right on priority and blind on distance. Planning routinely fills the table
 * with a whole band of equal-priority sites (one per tribe village, section F
 * of the Euro planner), identical for all four nations, so every settler of
 * every nation marched at the table's first entry however far away it was --
 * across a strait on some maps, which meant no colony ever got founded. Among
 * the slots that tie on the top priority, take the one nearest `from`
 * (FUN_124c_0040 distance, the metric FUN_521d_0a60's own goal scan uses),
 * preferring the unit's own landmass when the map is known.
 */
int ai_goals_best_found_tile_near(
  const ColonizeWorldMap* map,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (nation_id < 0 || nation_id >= 4 || !out_x || !out_y) {
    return 0;
  }
  const int have_from = (from_x >= 0 && from_y >= 0);
  const int from_cont =
    (map && have_from) ? map_continent_id_at(map, from_x, from_y) : -1;

  int best_x = -1;
  int best_y = -1;
  int best_prio = -1;
  int best_dist = INT_MAX;
  int best_same_cont = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* s = &s_goals[nation_id].primary[i];
    if (s->code != AI_GOAL_FOUND && s->code != AI_GOAL_MIL_EXPAND) {
      continue;
    }
    if (!have_from) {
      *out_x = s->x;
      *out_y = s->y;
      return 1; /* priority order alone, as before */
    }
    int dx = (int)s->x - from_x;
    int dy = (int)s->y - from_y;
    if (dx < 0) {
      dx = -dx;
    }
    if (dy < 0) {
      dy = -dy;
    }
    const int dist = (dx < dy) ? dx / 2 + dy : dy / 2 + dx;
    const int same_cont =
      (from_cont >= 0) ? (map_continent_id_at(map, s->x, s->y) == from_cont) : 0;
    /* Reachable landmass first, then priority, then distance. */
    if (same_cont > best_same_cont ||
        (same_cont == best_same_cont &&
         ((int)s->prio > best_prio ||
          ((int)s->prio == best_prio && dist < best_dist)))) {
      best_same_cont = same_cont;
      best_prio = (int)s->prio;
      best_dist = dist;
      best_x = s->x;
      best_y = s->y;
    }
  }
  if (best_x < 0) {
    return 0;
  }
  *out_x = best_x;
  *out_y = best_y;
  return 1;
}

/*
 * FUN_521d_0492 — colony_count_balance_flags(nation, continent).
 * Decomp 87098-87136, transcribed literally:
 *
 *   target = DS:0x85c8[cont] / 12            (continent_tally_b, word)
 *   used   = DS:0x947e[cont]                 (village_counts_by_continent)
 *          + Σ_{n=0..3} DS:0x94e6[n*16+cont] (colony counts per nation)
 *   flags  = (target > used) ? 1 : (target < used) ? -1 : 0
 *   flags += 2 when DS:0x947e[cont] == used  (⇔ no Euro colony there)
 *   flags += 4 when this nation has none there
 *
 * FIXED 2026-09-07: the port summed only the four nation colony counts and
 * dropped the `DS:0x947e[cont]` village base from `used`, which made every
 * continent look emptier than DOS sees it — the `target > used` arm (the
 * "expand here" +1) fired on continents DOS already scores 0 or -1. Indian
 * villages are counted live off `col1->tribe` rather than the saved
 * `stuff.village_counts_by_continent[]`, which nothing in the port keeps
 * fresh (it is only used as the census blank-window boundary marker).
 */
int ai_goals_colony_balance_flags(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int continent_id
) {
  if (!col1 || continent_id < 0 || continent_id > 15) {
    return 0;
  }
  if (nation_id < 0 || nation_id > 3) {
    return 0;
  }

  int nation_cont[4][16];
  memset(nation_cont, 0, sizeof(nation_cont));
  if (map && colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      const int cid = map_continent_id_at(map, c->x, c->y);
      if (cid < 0 || cid > 15) {
        continue;
      }
      nation_cont[c->nation_id][cid]++;
    }
  }

  int euro_on_cont = 0;
  for (int n = 0; n < 4; ++n) {
    euro_on_cont += nation_cont[n][continent_id];
  }

  /* DS:0x947e[cont] — Indian villages on this continent (live from tribes). */
  int villages_on_cont = 0;
  if (map && col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if (t->nation_id < 4) {
        continue;
      }
      if (map_continent_id_at(map, t->x, t->y) == continent_id) {
        villages_on_cont++;
      }
    }
  }
  const int used = villages_on_cont + euro_on_cont;

  const unsigned target = (unsigned)col1->post_map.continent_tally_b[continent_id] / 12u;
  int flags = 0;
  if ((int)target > used) {
    flags = 1;
  } else if ((int)target < used) {
    flags = -1;
  }
  /* Decomp: 947e == (947e + Σ94e6) ⇔ Σ nation colonies == 0. */
  if (euro_on_cont == 0) {
    flags += 2;
  }
  if (nation_cont[nation_id][continent_id] == 0) {
    flags += 4;
  }
  return flags;
}

/* Defined with the 0906 probes below; DOS FUN_281f_0682 / 06be. */
static int ai_goals_tile_layer2_owner(const ColonizeWorldMap* map, int x, int y, unsigned bit);

/*
 * FUN_281f_074a → FUN_137f_02f8: the DS:0x168 seen plane byte. Its LOW nibble
 * is the map-gen colony-site score (0..15); the per-nation seen bits live in
 * the HIGH nibble (0x10 << nation). Mirrors ai_euro_20e6_site_nibble
 * (ai_euro.c:11084) including its generated-map fallback: Linux map_gen writes
 * no nibble at all, and an all-zero nibble field would silently zero the whole
 * extras term, so when no tile in the plane carries a low nibble the old
 * per-nation unseen→4 stand-in is used instead.
 */
static int ai_goals_site_nibble_074a(const ColonizeWorldMap* map, int x, int y, int nation) {
  if (!map || !map->seen || x < 0 || y < 0 || x >= (int)map->width || y >= (int)map->height) {
    return 0;
  }
  static const uint8_t* s_nib_plane = NULL;
  static int s_nib_count = -1;
  static int s_nib_present = 0;
  const int count = (int)map->width * (int)map->height;
  if (map->seen != s_nib_plane || count != s_nib_count) {
    s_nib_plane = map->seen;
    s_nib_count = count;
    s_nib_present = 0;
    for (int i = 0; i < count; ++i) {
      if (map->seen[i] & 0x0f) {
        s_nib_present = 1;
        break;
      }
    }
  }
  if (s_nib_present) {
    return (int)(map->seen[(size_t)y * (size_t)map->width + (size_t)x] & 0x0f);
  }
  return !map_tile_seen_by(map, x, y, nation) ? 4 : 0;
}

/*
 * FUN_521d_06ae — pick_best_adjacent_founding_tile.
 * Decomp viceroy_unpacked.c ~87237. Base score = DS:0x2f77[class]; when
 * score_extras, add 0492(candidate continent)*0x10 + (074a & 0xf) per land
 * neighbour with no unit on it. Cite: euro_goals.c; move_scoring.md.
 */
int ai_goals_pick_founding_tile_ex(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  const struct ColonizeUnitPool* units,
  int nation_id,
  int x,
  int y,
  int score_extras,
  int wagon_filter,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return 0;
  }
  int best_dir = -1;
  /*
   * DOS seeds the running best with `local_1a = 0xffff` and compares
   * `(int)local_1a < (int)local_a` (decomp 87244 / 87310) — signed, so a
   * candidate must score >= 0 to be taken at all. With `bal` reaching -1
   * and multiplying by 0x10 per empty neighbour a tile really can land
   * below that, and the port's INT_MIN seed accepted those. Matching the
   * -1 floor also means an all-negative ring falls through to DOS's own
   * default (`local_10 = 8`, i.e. stay) — here, to the `!any` return below.
   */
  int best_score = -1;
  int any = 0;
  for (int dir = 0; dir <= 8; ++dir) {
    const int nx = x + k_dir8_dx[dir];
    const int ny = y + k_dir8_dy[dir];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    /* Land, not ocean/HS (FUN_281f_0302 + !0768). */
    if (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny)) {
      continue;
    }
    /*
     * No Arctic exclusion: DOS 06ae (decomp 87282-87285) gates the candidate
     * on 0302 (inset) && !0768 (not ocean/high-seas) only. Arctic's DS:0x2f77
     * founding byte is 0, so it can only ever win when nothing else scores —
     * the explicit `terrain_index == 24 -> continue` here was Linux-only.
     */
    /*
     * Never the village tile itself (DOS: "Illegal entry into village").
     * colonies_can_found only knows the layer2 has-city bit, which a
     * new-game tribe placement never stamps, so on a fresh map this search
     * happily returned a dwelling as a colony site -- the founder then walked
     * into the village, which resolves as an attack, and died there.
     */
    if (col1 && col1->tribe) {
      int on_village = 0;
      for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
        const ColonizeCol1Tribe* t = &col1->tribe[ti];
        if ((int)t->x == nx && (int)t->y == ny && t->nation_id >= 4) {
          on_village = 1;
          break;
        }
      }
      if (on_village) {
        continue;
      }
    }
    /*
     * colonies_can_found is a PORT belt, not a DOS 06ae gate: DOS validates
     * the site only at found time (5b66 case 7) and tolerates picks its
     * found routine later refuses. The port's callers treat a pick as a
     * committed destination (walk there / unload there), so an unfoundable
     * candidate must be filtered here — for every dir INCLUDING 8. (DOS's
     * `(local_c == 8 || bVar1)` bypass at decomp 87283-87284 covers only the
     * 06d2/08bc OCCUPANT gate below, which the port likewise skips at 8;
     * 2026-09-08 briefly extended the bypass to this belt too, which let an
     * expand scan seeded on a colony tile return that colony tile itself and
     * park the founder there forever.)
     */
    if (colonies && !colonies_can_found(colonies, map, nx, ny)) {
      continue;
    }
    /*
     * DOS 06ae occupant gate (FUN_281f_06d2 tribe-or-presence, then
     * FUN_281f_08bc(unit) == 1 singleton check): a neighbour holding a
     * foreign presence is skipped; an own unit there only passes when it is
     * a lone unit whose wagon-ness differs from the unit being placed
     * (type 0x0b == wagon XOR wagon_filter). Stay (dir 8) is never gated.
     * Seed-100 TURN2→3: the Dutch Soldier lands at (48,14) because the
     * Pioneer already dropped on (49,14) blocks that tile.
     */
    if (dir != 8 && units) {
      const int oid = units_id_at(units, nx, ny);
      if (oid >= 0) {
        const ColonizeUnit* ou = units_get_const(units, oid);
        if (!ou || ou->nation_id != nation_id) {
          continue;
        }
        int on_tile = 0;
        for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
          const ColonizeUnit* tu = &units->units[ui];
          if (tu->active && tu->aboard_ship_id < 0 && tu->x == nx && tu->y == ny) {
            on_tile++;
          }
        }
        const ColonizeUnitType* ot = units_type(units, ou->type_index);
        const int ou_is_wagon = ot && strstr(ot->name, "Wagon") != NULL;
        if (on_tile != 1 || (ou_is_wagon != 0) == (wagon_filter != 0)) {
          continue;
        }
      }
    }

    /*
     * Base: terrain-class founding byte @ DS:0x2f77 (decomp 87286-87287).
     * That byte plus `bal * 0x10` plus the seen-plane nibble is DOS's *whole*
     * term set — the Linux `coastal_bonus` (+10 every colony, +40 the first)
     * and its `x - nx` west bias were removed 2026-09-08: a flat +10 on every
     * coastal candidate dominates a 0..6 terrain byte outright.
     */
    int score = map_dos_terr_found_score_byte(map_dos_terr_class_at(map, nx, ny));

    if (score_extras) {
      /* Decomp: 0492(nation, continent_of_candidate) once per empty neighbor. */
      const int cand_cid = map_continent_id_at(map, nx, ny);
      const int bal = ai_goals_colony_balance_flags(map, colonies, col1, nation_id, cand_cid);
      for (int nd = 0; nd < 8; ++nd) {
        const int hx = nx + k_dir8_dx[nd];
        const int hy = ny + k_dir8_dy[nd];
        if (!map_coords_inset(map, hx, hy)) {
          continue;
        }
        if (map_tile_is_water(map, hx, hy) || map_tile_is_high_seas(map, hx, hy)) {
          continue;
        }
        /*
         * DOS 87289: `iVar4 = FUN_281f_0682(0x281f,iVar3,iVar9); iVar4 < 0` —
         * 0682 → FUN_137f_0314 is the **unit-presence** probe (layer2 bit 0 +
         * owner nibble), so the neighbour only contributes when *no unit*
         * stands on it. The port was testing the colony pool instead, i.e.
         * asking the 06be settlement question; same 0682/06be swap already
         * fixed inside 0906 (see ai_goals_tile_layer2_owner below).
         */
        if (ai_goals_tile_layer2_owner(map, hx, hy, MAP_OCCUPANCY_HAS_UNIT) >= 0) {
          continue;
        }
        /*
         * Neighbour-relation gate (decomp viceroy_unpacked.c 87293-87298):
         *
         *   cVar2 = FUN_281f_06dc(0x281f,iVar3,iVar9);       // raw owner nibble
         *   iVar4 = (int)cVar2;
         *   if ((((iVar4 < 0) || (3 < iVar4)) ||
         *       ((*(char *)(iVar4 * 0x34 + 0x543f) != '\0' ||
         *        (uVar5 = FUN_281f_0a38(0x281f,param_1,iVar4), (uVar5 & 0x40) == 0)))) &&
         *      (param_4 != 0)) { ... add ... }
         *
         * FUN_281f_06dc → FUN_137f_0200 (decomp 5383-5395) is the *ungated*
         * layer3 high nibble (0xf → 0xff → −1 once narrowed to char), not the
         * layer2-gated 0682/06be pair. DS:0x543f+n*0x34 is the player control
         * byte (0 human / 1 AI / 2 withdrawn; turn/year_loop.c:55).
         * FUN_281f_0a38 → FUN_15b3_0004 (decomp 7752-7761) reads the raw
         * euro_relation peer byte for nation < 4; bit 0x40 is PEACE.
         *
         * So the neighbour contributes nothing when its claimed owner is a
         * Euro nation 0..3 that is human-controlled AND we are at peace with
         * it — the AI does not bank expansion score off a human neighbour's
         * claimed land while the peace holds. AI-owned or unmet/at-war land
         * still counts, as does unclaimed (nibble 0xf) land.
         *
         * Raw byte on purpose: DOS has no self-pair special case here, and
         * euro_relation[self] is never written, so an own-claimed tile reads
         * 0 and the gate never fires. ai_diplo_read's PEACE|ALLY self virtual
         * would fire it, so it is deliberately not used.
         */
        {
          const int hi = (int)((map_get_layer3(map, hx, hy) >> 4) & 0x0fu);
          const int howner = hi == 0x0f ? -1 : hi;
          if (howner >= 0 && howner <= 3 && col1 && nation_id >= 0 && nation_id < 4 &&
              col1->player[howner].control == 0 &&
              (col1->nation[nation_id].euro_relation[howner] & AI_DIPLO_PEACE) != 0) {
            continue;
          }
        }
        /*
         * DOS 87302: `uVar5 = FUN_281f_074a(0x281f,iVar3,iVar9);
         *            local_a = local_a + iVar4 * 0x10 + (uVar5 & 0xf);`
         * 074a → FUN_137f_02f8 reads the DS:0x168 seen plane; its LOW nibble
         * is the map-gen colony-site score 0..15 (the seen bits are the high
         * nibble, 0x10 << nation). The old `seen ? 1 : 0` stand-in capped the
         * per-neighbour term at 1 instead of 15 — a whole ring is worth up to
         * +120 in DOS, +8 before. DOS multiplier `* 0x10` kept verbatim.
         * Score must stay signed — bal can be −1.
         */
        const int explore = ai_goals_site_nibble_074a(map, hx, hy, nation_id);
        score += bal * 0x10 + (explore & 0xf);
      }
    }

    if (score > best_score) {
      best_score = score;
      best_dir = dir;
      any = 1;
    }
  }
  if (!any) {
    /*
     * DOS has no fallback: `local_10` is seeded 8 and 06ae returns it, i.e.
     * "stay". The Linux ring-2..4 rescan that used to sit here invented sites
     * two to four tiles away that DOS never scores, so it is gone (2026-09-08).
     * The port keeps a 0 = "no site" return rather than DOS's unconditional
     * dir 8 because ~9 call sites branch on it (ai_euro.c:16928 hands the
     * result straight to units_unload_passenger), and returning the caller's
     * own tile when that tile is open water would land a passenger at sea.
     */
    return 0;
  }
  *out_x = x + k_dir8_dx[best_dir];
  *out_y = y + k_dir8_dy[best_dir];
  return 1;
}

int ai_goals_pick_founding_tile(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int x,
  int y,
  int* out_x,
  int* out_y
) {
  return ai_goals_pick_founding_tile_ex(
    map,
    colonies,
    col1,
    /*units=*/NULL,
    nation_id,
    x,
    y,
    /*score_extras=*/1,
    /*wagon_filter=*/0,
    out_x,
    out_y
  );
}

AiNationPlanScratch* ai_goals_plan_scratch(int nation_id) {
  if (nation_id < 0 || nation_id >= 4) {
    return NULL;
  }
  return &s_plan[nation_id];
}

/*
 * FUN_521d_6d8e prelude (decomp 93109 + 93139-93141) plus the four
 * FUN_4962_0018 census bytes 03d0 reads. The census half is already
 * recomputed every turn into col1->stuff by
 * col1_stuff_census_refresh_colony_counts(); only the DS:0xa0b8
 * "colonies asking for colonists" count has no census home, and DOS
 * rebuilds that at the top of each Euro AI nation turn, which is where
 * this is called from.
 *
 * `last_colony_founded_turn` is deliberately NOT touched: its writer is
 * FUN_479b_076e (ai_goals_note_colony_founded), not the prelude.
 */
void ai_goals_plan_scratch_refresh(
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int leader_trait1
) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  AiNationPlanScratch* p = &s_plan[nation_id];
  p->leader_trait1 = (int8_t)leader_trait1;
  if (col1) {
    p->colony_count = col1->stuff.colony_counts[nation_id];
    p->census_pop = col1->stuff.census_pop_proxy[nation_id];
    p->ship_cargo_total = col1->stuff.ship_cargo_totals[nation_id];
    /* stuff.avg_colony_pop is 4 packed little-endian u16 (DS:0x944e). */
    p->avg_colony_pop = (int)col1->stuff.avg_colony_pop[nation_id * 2] |
                        ((int)col1->stuff.avg_colony_pop[nation_id * 2 + 1] << 8);
  } else {
    p->colony_count = 0;
    p->census_pop = 0;
    p->ship_cargo_total = 0;
    p->avg_colony_pop = 0;
  }
  int wanting = 0;
  int live_colonies = 0;
  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      live_colonies++;
      if (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) {
        wanting++;
      }
    }
    /*
     * The census byte is only as fresh as the last between-turns refresh;
     * a colony founded (or lost) mid-pass would leave 03d0's `colony_count
     * == 0` early-out reading stale. DOS recounts DS:0x9298 per nation in
     * the same between-turns sweep, so prefer the live pool whenever a
     * colony pool is supplied.
     */
    p->colony_count = (uint8_t)(live_colonies > 255 ? 255 : live_colonies);
  }
  p->colonies_wanting_colonists = (uint8_t)(wanting > 255 ? 255 : wanting);
}

/*
 * FUN_479b_076e's `nation*0x13c-0x77b2 = DS:0x538e` stamp (case-7 FOUND
 * COLONY handler, viceroy_unpacked.c:77006): record the current turn as
 * this nation's last colony-founding turn, feeding 052c's decay term.
 */
void ai_goals_note_colony_founded(int nation_id, int turn) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  s_plan[nation_id].last_colony_founded_turn = turn;
}

/*
 * FUN_521d_03d0 — founding_expansion_urgency. Decomp 87058-87095, literal.
 *
 * Scratch is real since 2026-09-07 (see AiNationPlanScratch's header note);
 * the `return 8` arm is DOS's own early-out for "no colonies yet" or "no
 * colony is asking for colonists", not a port stand-in, and it is still what
 * fires through the opening turns.
 *
 * Signed-shift note: the two `>> 1` adjust arms are the decompiler's
 * rendering of `urgency` walking half the remaining gap toward
 * `ship_cargo_total >> 1`; kept in the raw's exact algebraic form rather
 * than simplified so it can be diffed against 87076-87081 line for line.
 */
int ai_goals_founding_expansion_urgency(int nation_id, int total_colony_count) {
  if (nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  if (total_colony_count < 0x30) {
    const AiNationPlanScratch* p = &s_plan[nation_id];
    if (p->colony_count == 0 || p->colonies_wanting_colonists == 0) {
      return 8;
    }
    /* `4 - trait1` with trait1 in {-1,0,1}: divisor is 3..5, never 0. */
    int divisor = 4 - (int)p->leader_trait1;
    if (divisor == 0) {
      divisor = 1;
    }
    int local_10 = ((int)p->census_pop - (int)p->colony_count) / divisor;
    const int uv4 = (int)p->ship_cargo_total >> 1;
    if (uv4 < local_10) {
      local_10 = -(((local_10 - uv4 + 1) >> 1) - local_10);
    } else if (local_10 < uv4) {
      local_10 = local_10 + ((1 - (local_10 - uv4)) >> 1);
    }
    const int iv2 = (int)p->leader_trait1 * 3 - 7;
    const int iv3 = -iv2;
    const int iv1 = p->avg_colony_pop;
    if (-iv1 != iv2 && iv1 <= iv3) {
      local_10 = local_10 + (-1 - (iv3 - iv1)) * (int)p->colony_count;
    }
    if (local_10 >= 0) {
      return local_10;
    }
  }
  return 0;
}

/*
 * FUN_281f_0614 → FUN_15eb_0142 (decomp 9380-9424): nearest colony of
 * `nation` (nation < 0 = any) restricted to `continent` (continent < 0 =
 * any), by the FUN_124c_0040 metric. Returns the colony index, -1 when
 * none, and writes the winning distance out through `out_dist` (DOS parks
 * it in DS:0x8db8; 9999 on a miss). DOS's `param_4 == -2` mode (derive the
 * continent from the probe tile and require colony flag 0x40) has no caller
 * in this segment and is not modelled.
 */
static int ai_goals_nearest_colony_15eb_0142(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int x,
  int y,
  int nation,
  int continent,
  int* out_dist
) {
  int best = -1;
  int best_d = 9999;
  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active) {
        continue;
      }
      if (nation >= 0 && c->nation_id != nation) {
        continue;
      }
      if (continent >= 0) {
        if (!map || map_continent_id_at(map, c->x, c->y) != continent) {
          continue;
        }
      }
      int dx = (int)c->x - x;
      int dy = (int)c->y - y;
      if (dx < 0) dx = -dx;
      if (dy < 0) dy = -dy;
      const int d = (dy < dx) ? (dy >> 1) + dx : (dx >> 1) + dy;
      if (d <= best_d) { /* DOS uses <=, so the LAST tie wins */
        best_d = d;
        best = i;
      }
    }
  }
  if (out_dist) {
    *out_dist = best_d;
  }
  return best;
}

/*
 * FUN_521d_052c — unit_desirability_score. Decomp 87139-87193.
 *
 * Two divergences fixed 2026-09-07:
 *
 *  1. The distance term. The port took a caller-supplied `home_dist` and
 *     only spent it when the unit stood exactly on an own colony tile,
 *     scoring +2 otherwise — so a settler two tiles from home scored the
 *     same as one on another continent. DOS calls FUN_281f_0614 →
 *     FUN_15eb_0142(x, y, nation, continent): the *nearest own colony on
 *     this continent*. Miss (-1) → +2; hit → `DS:0x8db8 / 5 - 1`, and
 *     DS:0x8db8 is that same call's own output, not an earlier snapshot.
 *
 *  2. The type-0 (Colonist) profession term. FUN_281f_0c9a is a far thunk
 *     to FUN_15eb_0002 (decomp 9298-9307), which returns 0 for profession
 *     0x13 and 0x19..0x1c and 1 for everything else; 052c then adds -4 on
 *     nonzero and -2 on zero. The port hardcoded the gate to false, so it
 *     always took the -2 arm — i.e. the *rarer* one. (ai_euro.c's
 *     ai_euro_5d04_cb_profession_gate already carries this exact table.)
 *
 * Tail term identity confirmed 2026-08-19: thunk_2a1f_0494(nation) is
 * founding_expansion_urgency, called here a second time purely as a nonzero
 * gate (not a war flag) for a "(turns since last colony founded) >> 4"
 * decay bonus.
 */
int ai_goals_unit_desirability_score(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int unit_x,
  int unit_y,
  int unit_type,
  int unit_profession,
  int continent_id,
  int turn,
  int total_colony_count
) {
  if (nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  const AiNationPlanScratch* p = &s_plan[nation_id];
  int score = 0;
  if (p->colony_count != 0) {
    int dist = 9999;
    const int idx = ai_goals_nearest_colony_15eb_0142(
      map, colonies, unit_x, unit_y, nation_id, continent_id, &dist
    );
    if (idx < 0) {
      score = 2;
    } else {
      score = dist / 5 - 1;
    }
  }
  if (unit_type == 2) score += 2;
  if (unit_type == 1) score += -2;
  if (unit_type == 4) score += -3;
  if (unit_type == 0) {
    /* FUN_281f_0c9a → FUN_15eb_0002: 0 for {0x13, 0x19..0x1c}, else 1. */
    const int gate =
      (unit_profession == 0x13 || (unit_profession >= 0x19 && unit_profession <= 0x1c)) ? 0 : 1;
    score += gate ? -4 : -2;
    if (unit_profession == 0x1b) {
      score += -0x14;
    }
  }
  if (ai_goals_founding_expansion_urgency(nation_id, total_colony_count) != 0) {
    score += (turn - p->last_colony_founded_turn) >> 4;
  }
  if (score > 0) {
    score = 0;
  }
  return score;
}

/*
 * FUN_521d_0600 — composite_unit_priority. Cite: viceroy_unpacked.c ~87196.
 * Thunk identities resolved by arg-shape match against 052c/0492/03d0's own
 * signatures (all three take a leading nation id and share this call site's
 * exact arg counts).
 */
int ai_goals_composite_unit_priority(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int unit_x,
  int unit_y,
  int unit_type,
  int unit_profession,
  int turn,
  int total_colony_count
) {
  if (!map) {
    return 0;
  }
  const int continent = map_continent_id_at(map, unit_x, unit_y);
  const int desirability = ai_goals_unit_desirability_score(
    map, colonies, nation_id, unit_x, unit_y, unit_type, unit_profession,
    continent, turn, total_colony_count
  );
  const int balance = ai_goals_colony_balance_flags(map, colonies, col1, nation_id, continent);
  const int urgency = ai_goals_founding_expansion_urgency(nation_id, total_colony_count);
  int total = desirability + balance + urgency;
  if (total < 0) {
    total = 0;
  }
  return total;
}

/*
 * FUN_521d_0656 — stack_settler_pick. The canonical decompile
 * (viceroy_unpacked.c ~87219) is register-mangled and reads as a bare
 * "follow the chain to its end"; the real body, re-disassembled byte-exact
 * from OVL14_L0000:0656 (viceroy_overlays.asm) on 2026-09-06e, is a
 * max-scan gated on the @UNIT capability table:
 *
 *   best = -1;
 *   while (unit >= 0) {
 *     t = unit.type(+0x3146);
 *     if (DS:0x523d[t*0xe] & 0x40 && (best < 0 || type[best] < t)) best = unit;
 *     unit = next_stack_member(unit);          // FUN_1000_84d4
 *   }
 *
 * Bit 0x40 is set for exactly types 0 (Colonist), 2 (Pioneer) and 5 (Scout);
 * every ship type carries 0x81/0x82/0xa2, so a carrier never picks itself and
 * an empty transport returns -1. Table values are the same DS:0x523d
 * @UNIT bit-string ai_euro.c's k_20e6_type_flags carries.
 */
static const uint8_t k_goals_type_flags_523d[23] = {
  0x40, 0x1c, 0x40, 0x20, 0x3c, 0x64, 0x1c, 0x1c, 0x1c, 0x1c, 0x00, 0x18,
  0x00, 0xa2, 0x82, 0x82, 0x01, 0x81, 0x81, 0x38, 0x38, 0x38, 0x38
};

int ai_goals_stack_settler_pick(
  const ColonizeCol1Unit* units,
  int unit_count,
  int unit_index
) {
  int best = -1;
  int best_type = -1;
  while (units && unit_index >= 0 && unit_index < unit_count) {
    const int t = (int)units[unit_index].type;
    const int flags =
      (t >= 0 && t < (int)(sizeof(k_goals_type_flags_523d))) ? (int)k_goals_type_flags_523d[t] : 0;
    if ((flags & 0x40) != 0 && t > best_type) {
      best_type = t;
      best = unit_index;
    }
    unit_index = units[unit_index].transport_chain.next_unit_idx;
  }
  return best;
}

/*
 * FUN_521d_0896 — the Indian hostility gate. Cite: viceroy_unpacked.c
 * 87319-87340, raw body:
 *
 *   if (3 < param_2) {
 *     if (param_3 == 0) return -1;
 *     iVar2 = FUN_281f_030c(0x521d, param_2 + -4, param_1);
 *     bVar1 = 0x4a < iVar2;
 *     if ((-1 < param_4) &&
 *        (0x7f < *(int *)((*(char *)(param_4 * 0x1c + 0x314a) * 9 + param_1)
 *                         * 2 + 0x54f6))) bVar1 = true;
 *     if (!bVar1) return -1;
 *   }
 *   return param_2;
 *
 * Ghidra prepends the far-call segment word, so the real argument order at
 * FUN_521d_0906's two call sites (`thunk_FUN_2a1f_056c(0x281f, param_3,
 * iVar5, param_4, local_10)`) is:
 *   param_1 = acting Euro nation      -> nation_id
 *   param_2 = the adjacent tile's owner id (0..3 Euro, >=4 Indian nation)
 *                                     -> profession (DOS's overload)
 *   param_3 = 0906's own param_4 flag -> has_context
 *   param_4 = unit index on that tile -> unit_index (-1 for Euro owners
 *             and for the settlement probe)
 *
 * The two reads, both wired 2026-09-08 (previously PARKED as 0):
 *   FUN_281f_030c -> FUN_15dc_00e0 = DS:0x5b1c[indian][euro], the Indian
 *     nation's alarm toward that Euro (ai_diplo_indian_alarm, HIGH =
 *     HOSTILE). `0x4a < alarm` — strictly greater than 74.
 *   DS:0x54f6 = the grudge/tension table (ColonizeCol1Save.indian_tension,
 *     docs/indians.md "third layer"), keyed by the *tile unit's* home
 *     settlement id (DOS unit +0x06 / DS:0x314a = ColonizeUnit
 *     .home_tribe_id) x the acting Euro nation. DOS strides that table by
 *     9; the port stores it packed x4 (only euro 0..3 has a confirmed
 *     touch site — col1_save.h) and every other port site keys it the same
 *     way, so reuse that indexing here. `0x7f < tension` — over 127.
 *
 * So: an adjacent native only raises a contact claim when the nation is
 * already hostile OR that particular village carries a grudge. `has_context`
 * 0 rejects outright — that is the 20e6 explorer-flag probe, which is why no
 * tribe has ever raised a claim there.
 */
int ai_goals_filter_profession_by_distance_wealth(
  const ColonizeCol1Save* col1,
  const struct ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int has_context,
  int unit_index
) {
  if (profession > 3) {
    if (!has_context) {
      return -1;
    }
    /* FUN_281f_030c(profession - 4, nation) — accessor takes 4..11 raw. */
    const int alarm = (col1 && nation_id >= 0 && nation_id < 4)
      ? ai_diplo_indian_alarm(col1, profession, nation_id)
      : 0;
    int gate = alarm > 0x4a;
    if (!gate && unit_index >= 0 && units && col1 && col1->indian_tension &&
        nation_id >= 0 && nation_id < 4) {
      const ColonizeUnit* tu = units_get_const(units, unit_index);
      const int home = tu ? tu->home_tribe_id : -1;
      if (home >= 0 && home < (int)col1->head.tribe_count) {
        /*
         * Decomp 87333:
         *   0x7f < *(int *)((*(char *)(param_4*0x1c + 0x314a) * 9 + param_1) * 2 + 0x54f6)
         * i.e. DS base **0x54f6**, int16 cells, row stride **9** —
         * `[home_settlement * 9 + euro_nation]` (the 9-column layout in
         * docs/archive/mysteries_catalog.md). The port's table is packed to
         * stride COLONIZE_COL1_NATION_COUNT (= 4) instead, and deliberately
         * stays that way: `indian_tension` is runtime-only (col1_save.h — it
         * is not in DOS's save chunk, so no DOS memory image ever feeds it),
         * it is allocated `tribe_count * 4` in col1_save.c, and every writer
         * (ai_diplo.c:3645, ai_contact.c:7248/7705, units.c:3393/3729) uses
         * stride 4. Re-striding only this reader would read the wrong cell
         * and run past the allocation for home >= tribe_count * 4 / 9.
         */
        const int tension =
          (int)col1->indian_tension[(size_t)home * COLONIZE_COL1_NATION_COUNT + (size_t)nation_id];
        if (tension > 0x7f) {
          gate = 1;
        }
      }
    }
    if (!gate) {
      return -1;
    }
  }
  return profession;
}

/*
 * DOS layer2 occupancy accessors, resolved 2026-09-07 from the raw 137f
 * bodies rather than the catalog blurbs:
 *
 *   FUN_281f_0682 → FUN_137f_0314 (decomp 6775-6790): in-bounds, layer2 & 1
 *                   (a unit stands here), then the layer3 owner nibble.
 *   FUN_281f_06be → FUN_137f_03e4 (decomp 6840-6858): in-bounds, layer2 & 2
 *                   (a settlement stands here), then the same nibble.
 *   FUN_281f_06d2 → FUN_137f_0428: 06be, falling back to 0682.
 *
 * The port's own MAP_OCCUPANCY_HAS_UNIT/HAS_CITY are 0x01/0x02, matching.
 * FUN_137f_0200 maps an owner nibble of 0xf to -1.
 */
static int ai_goals_tile_layer2_owner(const ColonizeWorldMap* map, int x, int y, unsigned bit) {
  if (!map || !map->layer2 || !map_coords_inset(map, x, y)) {
    return -1;
  }
  if ((map->layer2[(size_t)y * (size_t)map->width + (size_t)x] & bit) == 0) {
    return -1;
  }
  const int nib = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
  return nib == 0x0f ? -1 : nib;
}

/*
 * FUN_521d_0906 — probe_adjacent_contact_claim. Decomp 87345-87405.
 *
 * REWORKED 2026-09-07. The port had the two probes conflated: the primary
 * probe (`FUN_281f_0682`) was reading the *colony* pool, and the secondary
 * probe (`FUN_281f_06be`) was hardwired to "nothing found". The raw 137f
 * bodies say the opposite — 0682 is the **unit-presence** owner (layer2
 * bit 0) and 06be is the **settlement** owner (layer2 bit 1) — so the port
 * was answering the settlement question with the unit probe and never
 * asking the unit question at all.
 *
 * Faithful shape now:
 *   medium = ocean_or_high_seas(x, y)
 *   for dir in 0..7 while claim < 0:
 *     if ocean_or_high_seas(n) != medium: skip
 *     o = 0682(n)                                   // foreign unit adjacent
 *     if (o >= 0 && o != nation):
 *        claim = 0896(nation, o, profession, unit_on(n) when o >= 4)
 *        if (claim >= 0 && medium != 0 && no armed ship in n's stack)
 *           claim = -1                              // water: armed hull only
 *     s = 06be(n)                                   // foreign settlement
 *     if (s >= 0 && s != nation):
 *        claim = 0896(nation, s, profession, -1)    // DOS assigns claim here
 *        if (claim < 4 && side < 0) side = claim    // DS:0x9ea8, first only
 *
 * 0896's tribe arm (owner >= 4) went live 2026-09-08: it now really reads
 * the FUN_281f_030c Indian↔Euro alarm word and the DS:0x54f6 tension table
 * (see that function). The 20e6 explorer-flag probe passes has_context 0, so
 * only the 0a60 tile-housekeeping probe (has_context 1) can raise a tribe
 * claim.
 */
int ai_goals_probe_adjacent_contact_claim(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const struct ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  int x,
  int y,
  int nation_id,
  int profession,
  int* out_side_claim
) {
  (void)colonies; /* settlements come off layer2 bit 1, as in DOS */
  int side = -1;
  if (!map) {
    if (out_side_claim) *out_side_claim = side;
    return -1;
  }
  const int origin_water =
    (map_tile_is_water(map, x, y) || map_tile_is_high_seas(map, x, y)) ? 1 : 0;
  int claim = -1;
  for (int dir = 0; dir < 8 && claim < 0; ++dir) {
    const int nx = x + k_dir8_dx[dir];
    const int ny = y + k_dir8_dy[dir];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    const int n_water =
      (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny)) ? 1 : 0;
    if (n_water != origin_water) {
      continue;
    }
    const int owner = ai_goals_tile_layer2_owner(map, nx, ny, MAP_OCCUPANCY_HAS_UNIT);
    if (owner >= 0 && owner != nation_id) {
      /* Tribe owners key 0896's tension lookup off the tile's top unit. */
      const int unit_filter =
        (owner >= 4 && units) ? units_id_at(units, nx, ny) : -1;
      claim = ai_goals_filter_profession_by_distance_wealth(
        col1, units, nation_id, owner, profession, unit_filter
      );
      if (claim >= 0 && origin_water != 0) {
        /*
         * Water arm: DOS walks the tile's stack and keeps the claim only if
         * some member is a ship type 0x0d..0x12 whose DS:0x5236 combat byte
         * is nonzero — i.e. an *armed* hull, the same test the FUN_4962_0018
         * census ship-pressure scan uses (colony.h ..._AI_NEARBY_ARMED_SHIP).
         */
        int armed = 0;
        if (units) {
          for (int ui = 0; ui < COLONIZE_UNITS_MAX && !armed; ++ui) {
            const ColonizeUnit* su = units_get_const(units, ui);
            if (!su || !su->active || su->aboard_ship_id >= 0 || su->x != nx || su->y != ny) {
              continue;
            }
            const ColonizeUnitType* st = units_type(units, su->type_index);
            if (st && st->domain == COLONIZE_UNIT_DOMAIN_SEA && st->attack > 0) {
              armed = 1;
            }
          }
        }
        if (!armed) {
          claim = -1;
        }
      }
    }
    const int settlement = ai_goals_tile_layer2_owner(map, nx, ny, MAP_OCCUPANCY_HAS_CITY);
    if (settlement >= 0 && settlement != nation_id) {
      claim = ai_goals_filter_profession_by_distance_wealth(
        col1, units, nation_id, settlement, profession, -1
      );
      if (claim < 4 && side < 0) {
        side = claim;
      }
    }
  }
  if (out_side_claim) {
    *out_side_claim = side;
  }
  return claim;
}
