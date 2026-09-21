#include "core/europe.h"
#include "core/europe_art.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "platform/diagnostics.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/ui_button.h"
#include "core/units.h"

/* Sound hook (unit tests build europe.c without sound.c — same shape as
 * units_set_combat_music_hooks). */
static void (*g_europe_sound_play)(int id) = NULL;
static void (*g_europe_set_bgm)(int pool) = NULL;
#include "platform/platform.h"

static void europe_refresh_recruit_passage(EuropeScreen* eu);
/* One treasury, both stores — see the block above europe_cargo_boycotted_ex. */
static void europe_purse_move(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, long delta
);

/*
 * Recruit-pool randomness. DOS `FUN_38fd_46d4` uses TWO sources and the
 * split matters for stream fidelity:
 *
 *  - the three tier rolls come off the shared game stream,
 *    `FUN_281f_04d4(1,15)` / `(1,10)` / `(1,8)` (viceroy_unpacked.c 64632,
 *     64636, 64640) — the same stream `europe_immigrant_from_pool`'s slot
 *     pick (`04d4(0,2)`, 68581) already takes a ColonizeDosRng* for, and the
 *     two are drawn back to back in DOS's own phase-5 spawn (68581/68583);
 *  - the expert value comes off a per-nation 5-bit LFSR
 *    (`FUN_291f_0eda` stepping nation+0x44 with poly 0x14 plus the +0x45
 *     salt, 64585-64590), which consumes NO shared-stream draw. A
 *    force-expert refill (`46d4(1)`, the (turn & 3) == 0 case) therefore
 *    advances the shared stream not at all.
 *
 * The port has the shared stream (ColonizeTurnContext.rng /
 * ColonizeGameState.move_rng, plumbed in below) but not the LFSR: the port
 * repurposed nation+0x44/+0x45 as ColonizeCol1Nation.diplo_flag[0..1]. So
 * the tier rolls now go through dos_rng_range on the real stream and the
 * expert value keeps this local LCG as the LFSR stand-in — which is also
 * what keeps the shared stream exactly where DOS leaves it.
 *
 * The local state is seeded from the bound stream's current state (a read,
 * never a draw) when there is one; the treasury-derived seed survives only
 * for callers that legitimately hold no game rng — europe_seed_pool (which
 * shares one state across the two slots it rolls) and fixture/unit-test
 * callers that pass NULL.
 */
static unsigned europe_rng_next(unsigned* state) {
  unsigned s = state ? *state : 1u;
  s = s * 1103515245u + 12345u;
  if (state) {
    *state = s;
  }
  return (s >> 16) & 0x7fffu;
}

typedef struct EuropePoolRng {
  ColonizeDosRng* dos; /* shared game stream; NULL for fixture callers */
  unsigned* local;     /* LFSR stand-in state; never NULL */
} EuropePoolRng;

/* DOS `FUN_281f_04d4(lo,hi)` — inclusive, off the shared stream. */
static int europe_pool_tier_roll(EuropePoolRng* r, int lo, int hi) {
  if (r->dos) {
    return dos_rng_range(r->dos, lo, hi);
  }
  return lo + (int)(europe_rng_next(r->local) % (unsigned)(hi - lo + 1));
}

/* DOS's per-nation LFSR stand-in: 0..hi inclusive, no shared-stream draw. */
static int europe_pool_expert_roll(EuropePoolRng* r, int hi) {
  return (int)(europe_rng_next(r->local) % (unsigned)(hi + 1));
}

static bool europe_parse_int_field(const char** cursor, int* out) {
  while (**cursor == ' ' || **cursor == '\t' || **cursor == ',') {
    ++(*cursor);
  }
  if (**cursor == '\0') {
    return false;
  }
  char* end = NULL;
  long v = strtol(*cursor, &end, 10);
  if (end == *cursor) {
    return false;
  }
  *out = (int)v;
  *cursor = end;
  return true;
}

static void europe_set_status(EuropeScreen* eu, const char* text) {
  if (!eu) {
    return;
  }
  snprintf(eu->status, sizeof(eu->status), "%s", text ? text : "");
}

static void europe_copy_ship(EuropeHarborShip* dst, const EuropeHarborShip* src) {
  if (!dst || !src) {
    return;
  }
  *dst = *src;
}

static void europe_clear_ship(EuropeHarborShip* s) {
  if (!s) {
    return;
  }
  memset(s, 0, sizeof(*s));
  s->type_index = -1;
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    s->cargo_professions[i] = -1;
  }
}

static int europe_goods_slots_used(const EuropeHarborShip* ship) {
  if (!ship) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    if (ship->hold_goods_amount[i] > 0 && ship->hold_goods_amount[i] < 255) {
      n++;
    }
  }
  return n;
}

static int europe_ship_cargo_cap(const EuropeHarborShip* ship, const ColonizeUnitPool* units) {
  if (!ship) {
    return 0;
  }
  if (units) {
    int ti = ship->type_index;
    if (ti < 0) {
      ti = units_find_type(units, ship->name);
    }
    const ColonizeUnitType* ut = units_type(units, ti);
    if (ut && ut->cargo > 0) {
      return ut->cargo > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : ut->cargo;
    }
  }
  return EUROPE_SHIP_CARGO_MAX;
}

/*
 * Free goods slots in a harbor ship, DOS FUN_15eb_3208's first term:
 * `@UNIT cargo column (type*0xe+0x5237) − holds_occupied`. Passengers ride
 * the same slot array in DOS, and the port's boarding path already spends
 * them that way (europe_board_sentry_dockers), so they count here too.
 */
static int europe_ship_free_slots(const EuropeHarborShip* ship, const ColonizeUnitPool* units) {
  if (!ship) {
    return 0;
  }
  const int free_slots =
    europe_ship_cargo_cap(ship, units) - europe_goods_slots_used(ship) - ship->cargo_count;
  return free_slots > 0 ? free_slots : 0;
}

/*
 * A docked Artillery piece carries no dos_type of its own; it is recognised by
 * carrying the catalog's @UNIT row 11 name (whatever a given NAMES.TXT calls
 * it) — never by an English word compiled in here.
 */
static bool europe_dock_name_is_artillery(const char* name) {
  const char* live = reports_names_field("UNIT", UNITS_KIND_ARTILLERY, 0);
  return name && name[0] && live && live[0] && strcmp(name, live) == 0;
}

/* Insert at dock front (index 0). Returns false if docks are full. */
static bool europe_dock_push_front(
  EuropeScreen* eu,
  const char* name,
  int profession,
  bool sentry
) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  for (int i = eu->dock_count; i > 0; --i) {
    eu->dock[i] = eu->dock[i - 1];
  }
  EuropeDockImmigrant* d = &eu->dock[0];
  memset(d, 0, sizeof(*d));
  snprintf(d->name, sizeof(d->name), "%s", name ? name : "");
  d->profession = profession;
  d->present = true;
  d->sentry = sentry;
  d->dos_type = europe_dock_type_for(d->name, profession);
  eu->dock_count++;
  return true;
}

bool europe_dock_push_load(EuropeScreen* eu, const char* name, int profession) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "");
  slot->profession = profession;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_for(slot->name, profession);
  return true;
}

bool europe_dock_slot_pos(int index, int* out_x, int* out_y) {
  /*
   * FUN_38fd_146c: tier 0 = the first EUROPE_DOCK_ROW0 slots on the upper quay,
   * tier 1 = the next EUROPE_DOCK_ROW1 on the lower one, both starting from the
   * same base x. Tier 2 (anything beyond) is computed but never blitted.
   */
  if (index < 0 || index >= EUROPE_DOCK_ROW0 + EUROPE_DOCK_ROW1) {
    return false;
  }
  int col = index;
  int y = EUROPE_DOCK_Y;
  if (index >= EUROPE_DOCK_ROW0) {
    col = index - EUROPE_DOCK_ROW0;
    y = EUROPE_DOCK_Y2;
  }
  if (out_x) {
    *out_x = EUROPE_DOCK_X + col * EUROPE_DOCK_PITCH;
  }
  if (out_y) {
    *out_y = y;
  }
  return true;
}

static int europe_type_is_treasure(const ColonizeUnitPool* units, int type_tag) {
  if (!units || type_tag < 0) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, type_tag);
  return units_type_is_treasure(ut);
}

/*
 * Treasure passengers cash in (or PARK without inventing gold) and are removed
 * before dock unload — they are not immigrants. Cite: Colonization.pdf Treasure
 * Trains; GAME.TXT @LOOTCASH / @CASHTREASURE.
 */
static void europe_cash_treasure_passengers(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  const ColonizeUnitPool* units
) {
  if (!eu || !ship || ship->cargo_count <= 0) {
    return;
  }
  int w = 0;
  for (int i = 0; i < ship->cargo_count; ++i) {
    const int tag = ship->cargo_types[i];
    const int gold = ship->cargo_treasure_gold[i];
    if (europe_type_is_treasure(units, tag)) {
      if (gold > 0) {
        (void)europe_cash_treasure(eu, gold);
      } else {
        /*
         * gold==0 here means cargo_treasure_gold was never filled for this
         * slot — game_loop.c's game_europe_fill_expected_treasure_gold (the
         * H/Return-to-Europe path) now does the real fill, so this is a
         * defensive no-op for any other boarding path, not the live gap
         * this comment used to describe. Still don't invent a rate/value.
         */
      }
      continue;
    }
    if (w != i) {
      ship->cargo_types[w] = tag;
      ship->cargo_professions[w] = ship->cargo_professions[i];
      ship->cargo_treasure_gold[w] = gold;
    }
    ++w;
  }
  for (int i = w; i < ship->cargo_count; ++i) {
    ship->cargo_types[i] = 0;
    ship->cargo_professions[i] = -1;
    ship->cargo_treasure_gold[i] = 0;
  }
  ship->cargo_count = w;
}

/* Unload passengers onto dock front (preserves on-board order); clear holds. */
static void europe_disembark_passengers_to_dock(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  const ColonizeUnitPool* units
) {
  if (!eu || !ship || ship->cargo_count <= 0) {
    return;
  }
  europe_cash_treasure_passengers(eu, ship, units);
  for (int i = ship->cargo_count - 1; i >= 0; --i) {
    char name[40];
    const int tag = ship->cargo_types[i];
    const int prof = ship->cargo_professions[i];
    if (tag == -2) {
      snprintf(name, sizeof(name), "%s", "");
    } else if (units) {
      const ColonizeUnitType* ut = units_type(units, tag);
      if (ut && ut->name[0]) {
        snprintf(name, sizeof(name), "%s", ut->name);
      } else {
        snprintf(name, sizeof(name), "%s", "");
      }
    } else {
      snprintf(name, sizeof(name), "%s", "");
    }
    /* Passengers keep sentry ("board next") — same convention as aboard ship. */
    if (!europe_dock_push_front(eu, name, prof, true)) {
      snprintf(eu->status, sizeof(eu->status), "%s", "Docks are full — some passengers remain aboard.");
      /* Leave remaining passengers (0..i) on the ship. */
      ship->cargo_count = i + 1;
      return;
    }
  }
  ship->cargo_count = 0;
  memset(ship->cargo_types, 0, sizeof(ship->cargo_types));
  memset(ship->cargo_treasure_gold, 0, sizeof(ship->cargo_treasure_gold));
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    ship->cargo_professions[i] = -1;
  }
}

/*
 * The Europe purchase list (no Man-O-War). Oracle:
 * original_screenshots/europe/purchase.png; in DOS it is the FUN_521d_5c3c
 * table at DS:0x978d, stride 6, pinned byte-identical across three
 * original_memory_dumps — 0=Artillery/500, 1=Caravel/1000,
 * 2=Merchantman/2000, 3=Galleon/3000, 4=Privateer/2000, 5=Frigate/5000.
 *
 * Audit AE-17: the same six prices were also typed out in ai_euro.c's
 * 5d04 `k_purchase` and a third time as 5d04's gold-floor switch (the
 * DS:0x9796/0x97a8/0x97ae "catch-up gold" values are literally this table's
 * Caravel / Privateer / Frigate price cells). This file owns the numbers
 * now; europe_purchase_price / europe_purchase_option_at expose them.
 */
static const EuropePurchaseOption k_purchase_opts[] = {
  {UNITS_KIND_ARTILLERY, "", 500, false},
  {UNITS_KIND_CARAVEL, "", 1000, true},
  {UNITS_KIND_MERCHANTMAN, "", 2000, true},
  {UNITS_KIND_GALLEON, "", 3000, true},
  {UNITS_KIND_PRIVATEER, "", 2000, true},
  {UNITS_KIND_FRIGATE, "", 5000, true},
};
static const int k_purchase_opt_count =
  (int)(sizeof(k_purchase_opts) / sizeof(k_purchase_opts[0]));

int europe_purchase_option_count(void) {
  return k_purchase_opt_count;
}

const EuropePurchaseOption* europe_purchase_option_at(int index) {
  if (index < 0 || index >= k_purchase_opt_count) {
    return NULL;
  }
  return &k_purchase_opts[index];
}

int europe_purchase_price(ColonizeUnitKind kind) {
  for (int i = 0; i < k_purchase_opt_count; ++i) {
    if (k_purchase_opts[i].kind == kind) {
      return k_purchase_opts[i].gold;
    }
  }
  return 0;
}

/*
 * NAMES.TXT @UNIT row per purchase slot — the display name the list draws.
 * k_purchase_opts[].kind (a ColonizeUnitKind) is the price-lookup key; name[]
 * is filled live from the catalog below and is display-only, so a renamed
 * unit in a modded NAMES.TXT shows through here the way DOS's own list does.
 * A catalog miss leaves name[] empty (no DOS text baked in).
 */
static const int k_purchase_unit_rows[] = {11, 13, 14, 15, 16, 17};

void europe_init_purchase_table(EuropeScreen* eu) {
  eu->purchase_count = 0;
  for (int i = 0; i < k_purchase_opt_count && eu->purchase_count < EUROPE_PURCHASE_MAX; ++i) {
    EuropePurchaseOption* slot = &eu->purchase[eu->purchase_count++];
    *slot = k_purchase_opts[i];
    const char* live = reports_names_field("UNIT", k_purchase_unit_rows[i], 0);
    if (live && live[0]) {
      str_copy_trunc(slot->name, sizeof(slot->name), live);
    }
  }
}

/*
 * Recruit-pool profession table. The roll below can only produce these 21
 * @JOB ids; the six the DOS remap folds away (Master Sugar/Tobacco/Cotton
 * Planters, Expert Fur Trappers, Expert Teachers, Veteran Dragoons) are
 * deliberately absent — see europe_pool_remap.
 */
/*
 * @JOB ids only — no display text lives here (audit SC-13). The name
 * column this table used to carry (Petty Criminals=26, Indentured
 * Servants=25, Free Colonists=19, Expert Farmers=0, Expert Lumberjacks=5,
 * Expert Ore Miners=6, Expert Silver Miners=7, Expert Fishermen=8, Master
 * Distiller=9, Master Tobacconists=10, Master Weavers=11, Master Fur
 * Traders=12, Master Carpenters=13, Master Blacksmiths=14, Master
 * Gunsmiths=15, Firebrand Preachers=16, Elder Statesmen=17, Hardy
 * Pioneers=20, Veteran Soldiers=21, Seasoned Scouts=22, Jesuit
 * Missionaries=24) was dead: nothing ever read it, and
 * reports_job_display_name (NAMES.TXT @JOB column 1) is the live source.
 */
static const int k_pool_cands[] = {
  26, 25, 19, 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20, 21, 22, 24,
};

/* @JOB id of Free Colonists — the pool's "nothing special here" value and
 * what the DOS label 4884 swaps 0x1c (job NONE) for. */
#define EUROPE_POOL_JOB_FREE_COLONIST 19

/* Slot in k_pool_cands, or -1 when this @JOB id cannot appear in the pool. */
static int europe_pool_cand_index(int profession) {
  for (size_t i = 0; i < sizeof(k_pool_cands) / sizeof(k_pool_cands[0]); ++i) {
    if (k_pool_cands[i] == profession) {
      return (int)i;
    }
  }
  return -1;
}

/*
 * Audit SC-13: k_pool_cands stays as the DOS profession-id FILTER (the
 * remap below makes six @JOB ids unreachable from Europe), but the name
 * column no longer answers — reports_job_display_name reads NAMES.TXT @JOB
 * column 1, the same column those literals were copied from, so a
 * translated NAMES.TXT now reaches the recruit pool too.
 */
static const char* europe_pool_job_name(int profession) {
  const int cand = europe_pool_cand_index(profession);
  return reports_job_display_name(cand >= 0 ? profession : EUROPE_POOL_JOB_FREE_COLONIST);
}

/*
 * DOS `FUN_38fd_46d4`'s remap block (viceroy_unpacked.c:64595-64609): the
 * expert tier rolls a raw @JOB in 0..0x18 and then rewrites seven of them
 * before the value is ever stored. Six professions are therefore
 * UNREACHABLE from Europe — they exist only as colony/Indian training
 * outcomes or promotions:
 *
 *   0x12 Expert Teachers    -> 0x0d Master Carpenters   (bugs.md)
 *   0x17 Veteran Dragoons   -> 0x0d Master Carpenters
 *   0x13 Free Colonists     -> 0x16 Seasoned Scouts     (the free-colonist
 *                                    result comes from the tier roll's own
 *                                    0x1c return, not from this table)
 *   0x01 Master Sugar Planters  -> 0x08 Expert Fishermen
 *   0x02 Master Tobacco Planters-> 0x05 Expert Lumberjacks
 *   0x03 Master Cotton Planters -> 0x00 Expert Farmers
 *   0x04 Expert Fur Trappers    -> 0x06 Expert Ore Miners
 *
 * which is also why reports.c's labour scan says Expert Teachers (18) and
 * Veteran Dragoons (23) "never appear". The port used to draw from a
 * hand-weighted candidate table that listed Expert Teachers outright.
 *
 * The Expert Teacher colonist TYPE was cut from the final DOS game: this
 * remap makes it unhirable, schools can't produce it (graduates take the
 * teacher's specialty), and its school level 4 bars it from teaching. Any
 * DOS code that seems to create or handle one is a dead leftover — never
 * port such paths.
 */
static int europe_pool_remap(int job) {
  switch (job) {
    case COLONIZE_PROF_TEACHER: return COLONIZE_PROF_CARPENTER;
    case UNITS_JOB_COLONIST: return UNITS_JOB_SCOUT;
    case COLONIZE_PROF_SUGAR_PLANTER: return COLONIZE_PROF_FISHERMAN;
    case COLONIZE_PROF_TOBACCO_PLANTER: return COLONIZE_PROF_LUMBERJACK;
    case COLONIZE_PROF_COTTON_PLANTER: return COLONIZE_PROF_FARMER;
    case COLONIZE_PROF_FUR_TRAPPER: return COLONIZE_PROF_ORE_MINER;
    case UNITS_JOB_DRAGOON: return COLONIZE_PROF_CARPENTER;
    default: return job;
  }
}

/* DOS `FUN_15eb_0002` (via FUN_281f_0c9a): 0 for @JOB 0x13 and 0x19..0x1c,
 * i.e. Free Colonists / Servants / Criminals / Converts / job NONE — the
 * non-expert classes. 1 for every expert. */
static bool europe_job_is_expert(int job) {
  return !(job == UNITS_JOB_COLONIST || (job >= 0x19 && job <= 0x1c));
}

/*
 * The three pool-slot bytes `FUN_38fd_46d4` reads (nation+2..+4 = DS:0x84fc+2)
 * plus the two scalars it consults (DS:0x53a6 difficulty — substituted with 1
 * for a non-human bound nation — and FF 0x14 ownership). The human Europe
 * screen and the raw `ColonizeCol1Nation.recruit[3]` of an AI nation both
 * project onto this, so the roll below is shared instead of duplicated.
 */
typedef struct EuropePoolView {
  int job[EUROPE_POOL_SIZE];
  bool filled[EUROPE_POOL_SIZE];
  int difficulty;
  bool brewster;
} EuropePoolView;

static void europe_pool_view_from_screen(EuropePoolView* v, const EuropeScreen* eu) {
  memset(v, 0, sizeof(*v));
  if (!eu) {
    return;
  }
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    v->job[i] = eu->pool[i].profession;
    v->filled[i] = eu->pool[i].filled;
  }
  v->difficulty = (int)eu->difficulty;
  v->brewster = eu->brewster_no_criminals;
}

/*
 * `FUN_38fd_46d4(force_expert)` (viceroy_unpacked.c:64554-64694) — the
 * profession a recruit-pool slot is refilled with. Two halves:
 *
 *  - Tier roll (force_expert == 0, the Recruit-click refill 64776 and the
 *    game-start seed). Threshold t = (difficulty + 3) >> 1, with the DOS
 *    difficulty byte DS:0x53a6 used only for the human nation (AI nations
 *    substitute 1). RNG(1,15) <= t -> Petty Criminals; else RNG(1,10) <= t
 *    -> Indentured Servants; else RNG(1,8) <= t -> Free Colonists; else
 *    fall through to the expert half. Brewster (FF 0x14) turns the two
 *    bottom results into Free Colonists in place, exactly as
 *    europe_apply_brewster does to slots already rolled.
 *
 *  - Expert half (force_expert != 0, the end-of-turn crosses spawn on the
 *    season quad, 68583). Guarded twice: if all three pool slots already
 *    hold experts the routine gives up and returns Free Colonists (so the
 *    pool never shows three experts at once), and a roll that duplicates a
 *    profession already in the pool is thrown away and re-rolled, up to
 *    100 attempts.
 *
 * Deviation: DOS draws the expert value from a per-nation 5-bit LFSR
 * (nation +0x44 stepped by FUN_3f3f_0006 with poly 0x14, plus the +0x45
 * salt, rejecting > 0x18) so the sequence never repeats a value inside one
 * cycle. Those two bytes are the ones the port repurposed as
 * ColonizeCol1Nation.diplo_flag[0..1], so the state is not available here;
 * a uniform draw over the same 0..0x18 range is used instead (see
 * EuropePoolRng — like the LFSR it takes no shared-stream draw). The
 * reachable set and its distribution are identical, only the ordering
 * differs. The tier rolls above are the real `04d4` stream draws.
 */
static int europe_roll_pool_profession(
  const EuropePoolView* v, int slot, bool force_expert, EuropePoolRng* st
) {
  if (!force_expert) {
    /* DS:0x53a6 for the human nation; AI nations use 1 (46d4 64627-64633).
     * The port's Europe screen caches the human's difficulty (eu->difficulty);
     * the nation-record view fills the same field. */
    const int threshold = ((v ? v->difficulty : 0) + 3) >> 1;
    if (europe_pool_tier_roll(st, 1, 15) <= threshold) {
      return (v && v->brewster) ? 0x13 : 0x1a; /* Petty Criminals */
    }
    if (europe_pool_tier_roll(st, 1, 10) <= threshold) {
      return (v && v->brewster) ? 0x13 : 0x19; /* Indentured Servants */
    }
    if (europe_pool_tier_roll(st, 1, 8) <= threshold) {
      return 0x13; /* Free Colonists (DOS 0x1c, drawn as Free Colonists) */
    }
  }

  for (int tries = 0;;) {
    bool any_non_expert = false;
    for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
      if (!v->filled[i] || !europe_job_is_expert(v->job[i])) {
        any_non_expert = true;
      }
    }
    if (!any_non_expert) {
      return 0x13; /* three experts already in the pool */
    }
    const int job = europe_pool_remap(europe_pool_expert_roll(st, 0x18));
    if (++tries > 100) {
      return job;
    }
    bool duplicate = false;
    for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
      /* DOS compares against all three slots — the one being refilled still
       * holds its outgoing profession at this point, so the class that just
       * left the pool is not immediately redrawn. */
      if (v->filled[i] && v->job[i] == job) {
        duplicate = true;
      }
    }
    (void)slot;
    if (!duplicate) {
      return job;
    }
  }
}

/* bugs.md: Brewster's ban covers the EXISTING pool too — slots rolled
 * before the flag rose still held Petty Criminals / Indentured Servants,
 * so the Recruit list and the docks disagreed with the Brewster pick
 * dialog. DOS FUN_4345_0342 case 0x14 walks the three pool slot bytes
 * (nation*0x13c - 0x77f6, viceroy_unpacked.c 73160-73168) and overwrites
 * 0x19/0x1a (servant/criminal) with 0x1c — job NONE, which FUN_38fd_4884
 * draws as Free Colonists (0x1c→0x13 label swap, 64719 / 68591). A direct
 * substitution, not a reroll (bugs.md #224 kept it idempotent).
 *
 * The 0x13 stored below is that swap applied at the store instead of at the
 * draw — the port's single pool convention, not a divergence (smell audit
 * 2026-09-10 G5 secondary, refuted):
 *   - europe_set_pool_slot folds 0x1c (and every unnamed job) to 0x13 on
 *     load, and europe_roll_pool_profession's own free tier returns 0x13
 *     where DOS returns 0x1c, so 0x13 is the port's Free Colonists byte
 *     everywhere in the pool;
 *   - the roll's duplicate check cannot tell them apart: it only ever
 *     compares against europe_pool_remap output (0..0x18), which is never
 *     0x13 nor 0x1c;
 *   - col1_bridge reads a saved 0x1c as "slot empty" (col1_bridge.c:1701)
 *     and would reroll a fully-Brewstered pool on the next load if this
 *     wrote the raw DOS byte. */
void europe_apply_brewster(EuropeScreen* eu, int owned) {
  if (!eu || !owned) {
    return;
  }
  eu->brewster_no_criminals = true;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (eu->pool[i].filled &&
        (eu->pool[i].profession == UNITS_JOB_SERVANT ||
         eu->pool[i].profession == UNITS_JOB_CRIMINAL)) {
      eu->pool[i].profession = EUROPE_POOL_JOB_FREE_COLONIST;
      snprintf(
        eu->pool[i].name, sizeof(eu->pool[i].name), "%s",
        europe_pool_job_name(EUROPE_POOL_JOB_FREE_COLONIST)
      );
    }
  }
}

static void europe_refill_pool_slot_impl(
  EuropeScreen* eu, int slot, bool force_expert, ColonizeDosRng* dos, unsigned* rng_state
) {
  if (!eu || slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return;
  }
  /* See EuropePoolRng: with a bound stream the LFSR stand-in is seeded from
   * that stream's state (a read, not a draw); the treasury seed is the
   * no-rng fallback only. */
  unsigned local = dos ? (dos->state | 1u)
                       : 1u + (unsigned)(eu->gold + eu->recruit_passage + slot * 17);
  EuropePoolRng st;
  st.dos = dos;
  st.local = rng_state ? rng_state : &local;
  EuropePoolView view;
  europe_pool_view_from_screen(&view, eu);
  const int job = europe_roll_pool_profession(&view, slot, force_expert, &st);
  EuropePoolSlot* p = &eu->pool[slot];
  snprintf(p->name, sizeof(p->name), "%s", europe_pool_job_name(job));
  p->profession = job;
  p->filled = true;
}

/* DOS 64776 (the Recruit-click tail) calls 46d4(0) — the tier roll. Audit
 * SC-24: this used to be a three-deep wrapper stack (_ex over _impl, plain
 * over _ex) with no external caller for either intermediate. */
static void europe_refill_pool_slot(EuropeScreen* eu, int slot, unsigned* rng_state) {
  europe_refill_pool_slot_impl(eu, slot, false, NULL, rng_state);
}

void europe_refill_pool_slot_rng(
  EuropeScreen* eu, int slot, bool force_expert, ColonizeDosRng* rng
) {
  europe_refill_pool_slot_impl(eu, slot, force_expert, rng, NULL);
}

/* Restore one pool slot from a saved nation+2..+4 job byte. 0x1c (job NONE,
 * what DOS stores for "nothing here") and anything unnamed fall back to
 * Free Colonists, the label 4884's 0x1c→0x13 swap gives it. */
void europe_set_pool_slot(EuropeScreen* eu, int slot, int profession) {
  if (!eu || slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return;
  }
  EuropePoolSlot* p = &eu->pool[slot];
  /* Anything outside the pool's own candidate set (including 0x1c "job
   * NONE") becomes a Free Colonist, the swap label 4884 does. The test used
   * to be a strcmp against the English name, which a translated NAMES.TXT
   * would have broken (audit SC-13). */
  if (profession < 0 || profession > 0x1a || europe_pool_cand_index(profession) < 0) {
    profession = EUROPE_POOL_JOB_FREE_COLONIST;
  }
  snprintf(p->name, sizeof(p->name), "%s", europe_pool_job_name(profession));
  p->profession = profession;
  p->filled = true;
}

void europe_pool_ensure_filled(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (!eu->pool[i].filled) {
      europe_refill_pool_slot(eu, i, NULL);
    }
  }
}

const char* europe_pool_label(const EuropeScreen* eu, int slot) {
  if (!eu || slot < 0 || slot >= EUROPE_POOL_SIZE) {
    return "Colonist";
  }
  const EuropePoolSlot* p = &eu->pool[slot];
  return (p->filled && p->name[0]) ? p->name : "Colonist";
}

/*
 * Game-start recruit pool — DOS `FUN_38fd_6024` (viceroy_unpacked.c:68707-
 * 68729). Difficulty is DS:0x53a6, 0 Discoverer … 4 Viceroy (difficulty.md).
 * Slot 0 is a fixed bottom-tier class (`0x53a6 < 4` -> Indentured Servants,
 * i.e. below Viceroy; Petty Criminals at Viceroy); slots 1 and 2 are 46d4
 * rolls, slot 1 forced to the expert half on `0x53a6 < 3` — below GOVERNOR,
 * so Discoverer/Explorer/Conquistador — and slot 2 always. The human player
 * then gets a hand-picked easy opener, gated `0x53a6 == 0` / `== 1`: at
 * Discoverer all three slots become Master Carpenters / Expert Farmers /
 * Seasoned Scouts, at EXPLORER only slots 1 and 2 do, and Conquistador and
 * above get nothing. Spain then forces slot 0 to Jesuit Missionaries on top
 * of all of that (LAB_38fd_6161).
 */
void europe_seed_pool(EuropeScreen* eu, int difficulty, bool human) {
  if (!eu) {
    return;
  }
  if (difficulty < 0) {
    difficulty = 0;
  }
  if (difficulty > 8) {
    difficulty = 8;
  }
  const uint8_t saved_difficulty = eu->difficulty;
  eu->difficulty = (uint8_t)difficulty;
  unsigned rng = 42u;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    eu->pool[i].filled = false;
    eu->pool[i].profession = -1;
    eu->pool[i].name[0] = '\0';
  }
  if (EUROPE_POOL_SIZE > 0) {
    const int job = (difficulty < 4) ? 0x19 : 0x1a;
    snprintf(eu->pool[0].name, sizeof(eu->pool[0].name), "%s", europe_pool_job_name(job));
    eu->pool[0].profession = job;
    eu->pool[0].filled = true;
  }
  if (EUROPE_POOL_SIZE > 1) {
    europe_refill_pool_slot_impl(eu, 1, difficulty < 3, NULL, &rng);
  }
  if (EUROPE_POOL_SIZE > 2) {
    europe_refill_pool_slot_impl(eu, 2, true, NULL, &rng);
  }
  if (human && difficulty <= 1) {
    static const int k_easy[3] = {
      COLONIZE_PROF_CARPENTER, COLONIZE_PROF_FARMER, UNITS_JOB_SCOUT
    };
    for (int i = (difficulty == 0) ? 0 : 1; i < EUROPE_POOL_SIZE && i < 3; ++i) {
      snprintf(eu->pool[i].name, sizeof(eu->pool[i].name), "%s", europe_pool_job_name(k_easy[i]));
      eu->pool[i].profession = k_easy[i];
      eu->pool[i].filled = true;
    }
  }
  /*
   * LAB_38fd_6161 (viceroy_unpacked.c 68731-68733): `if (DS:0x9e12 == 2)
   * *(byte*)(DS:0x84fc + 2) = 0x18;` — the bound nation being Spain forces
   * recruit slot 0 (record +2) to job 0x18, Jesuit Missionaries. It sits
   * after the human easy-opener block and is not gated on difficulty or on
   * human control, so it overwrites the Discoverer 0x0d pick too. Seed-time
   * only: 0x9e12 == 2 appears nowhere else in the image, so the pool refill
   * (FUN_38fd_4884) has no Spanish case and a re-rolled slot 0 is ordinary.
   * Smell audit #56.
   */
  if (EUROPE_POOL_SIZE > 0 && eu->bound_nation == 2) {
    const int job = UNITS_JOB_MISSIONARY; /* NAMES @JOB 24 = "Jesuit Missionaries" */
    snprintf(eu->pool[0].name, sizeof(eu->pool[0].name), "%s", europe_pool_job_name(job));
    eu->pool[0].profession = job;
    eu->pool[0].filled = true;
  }
  eu->difficulty = saved_difficulty;
  if (eu->brewster_no_criminals) {
    europe_apply_brewster(eu, 1);
  }
}

void europe_seed_campaign_prices(EuropeScreen* eu, ColonizeDosRng* rng) {
  /*
   * FUN_38fd_6024 price seed (viceroy_unpacked.c 68645-68654): new campaign
   * only — bid = FUN_281f_04d4(0, start_hi−start_lo) + start_lo per cargo,
   * inclusive both ends, broadcast to all four nations. Fixed 16 iterations
   * in cargo order; hi==lo still consumes a draw, so the LCG stream matches
   * DOS. No clamp to [low, high]. Smell audit #55.
   */
  if (!eu || !rng) {
    return;
  }
  for (int i = 0; i < EUROPE_CARGO_MAX; ++i) {
    EuropeCargoQuote* q = &eu->cargo[i];
    const int lo = q->start_lo;
    const int hi = q->start_hi >= lo ? q->start_hi : lo;
    int bid = dos_rng_range(rng, lo, hi);
    if (i >= eu->cargo_count) {
      continue; /* draw consumed (DOS loops all 16 slots), table row absent */
    }
    if (bid < 0) {
      bid = 0;
    }
    q->bid = bid;
    q->ask = q->bid + q->burden;
  }
}

static void europe_init_pool(EuropeScreen* eu) {
  /* Reset time: the real difficulty is not cached yet (europe_reset_campaign
   * zeroes it and the first EOT tick fills it in), so this is the Discoverer
   * seed; the new-game bridge calls europe_seed_pool again once the wizard's
   * difficulty is known. */
  europe_seed_pool(eu, eu->difficulty, true);
}

/*
 * DOS `FUN_38fd_41ce` (the Train dialog) collects every @JOB with a positive
 * hire cost in job order, exactly as the loop below does, and then hands the
 * cost array and the parallel job-id array to `FUN_291f_0ed0` ->
 * `FUN_1cf8_000a` before drawing a single row. That routine is a sort: it
 * walks for the first descending step, lifts that element out (shifting the
 * tail left), finds the first slot whose key is >= the lifted key, shifts
 * right and drops it in — an ascending sort by cost. So the Train list is
 * ordered cheapest-first, not by @JOB index the way this port had it
 * (bugs.md).
 *
 * Transcribed rather than replaced with a qsort because the tie order is
 * observable and is this algorithm's own: an element only moves on a strictly
 * descending step, and re-enters *before* every equal key. With stock
 * NAMES.TXT that puts Carpenters before Fishermen at 1000, Farmers before
 * Distiller at 1100, and Pioneers before Tobacconists at 1200. Costs come
 * from NAMES.TXT, so a modded table has to re-sort the same way.
 */
static void europe_sort_train_by_cost(EuropeTrainOption* a, int n) {
  if (!a || n < 2) {
    return;
  }
  int i = 0;
  while (i < n - 1) {
    if (a[i + 1].cost >= a[i].cost) {
      ++i;
      continue;
    }
    const EuropeTrainOption lifted = a[i + 1];
    for (int k = 0; k < n - 2 - i; ++k) {
      a[i + 1 + k] = a[i + 2 + k];
    }
    int pos = 0;
    while (pos < n - 1 && a[pos].cost < lifted.cost) {
      ++pos;
    }
    for (int k = n - 2; k >= pos; --k) {
      a[k + 1] = a[k];
    }
    a[pos] = lifted;
    /* DOS does not rewind `local_12` here — the scan resumes where it was. */
  }
}

/*
 * @CARGO burden column, cached from the last table load so screens that hold
 * no EuropeScreen (the Economic report's fallback row) can still compute the
 * DOS ask price `euro_price + burden` (FUN_38fd_0016). Smell audit #63.
 */
static int g_europe_cargo_burden[EUROPE_CARGO_MAX];
static int g_europe_cargo_burden_count;

int europe_cargo_burden(int cargo_type) {
  if (cargo_type < 0 || cargo_type >= g_europe_cargo_burden_count) {
    return 0;
  }
  return g_europe_cargo_burden[cargo_type];
}

bool europe_load_tables(EuropeScreen* eu, const ColonizeMsgCatalog* names) {
  eu->cargo_count = 0;
  eu->class_count = 0;
  eu->train_count = 0;

  const ColonizeMsgSection* cargo = assets_msg_find(names, "CARGO");
  if (cargo) {
    for (int i = 0; i < cargo->line_count && eu->cargo_count < EUROPE_CARGO_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", cargo->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* comma = strchr(line, ',');
      if (!comma) {
        continue;
      }
      *comma = '\0';
      str_trim(line);
      if (line[0] == '\0') {
        continue;
      }

      const char* p = comma + 1;
      int start_lo = 0;
      int start_hi = 0;
      int low = 0;
      int high = 0;
      int burden = 0;
      int rise = 0;
      int fall = 0;
      int attrition = 0;
      int volatility = 0;
      if (!europe_parse_int_field(&p, &start_lo) || !europe_parse_int_field(&p, &start_hi) ||
          !europe_parse_int_field(&p, &low) || !europe_parse_int_field(&p, &high) ||
          !europe_parse_int_field(&p, &burden) || !europe_parse_int_field(&p, &rise) ||
          !europe_parse_int_field(&p, &fall) || !europe_parse_int_field(&p, &attrition) ||
          !europe_parse_int_field(&p, &volatility)) {
        continue;
      }
      EuropeCargoQuote* q = &eu->cargo[eu->cargo_count++];
      str_copy_trunc(q->name, sizeof(q->name), line);
      q->start_lo = start_lo;
      q->start_hi = start_hi;
      /* Table load leaves bid at the band floor; a new campaign then rolls
       * bid within [start_lo, start_hi] (europe_seed_campaign_prices) and a
       * loaded save overwrites it with euro_price. */
      q->bid = start_lo;
      if (q->bid < 0) {
        q->bid = 0;
      }
      q->low = low;
      q->high = high;
      q->burden = burden;
      q->rise = rise;
      q->fall = fall;
      q->attrition = attrition;
      q->volatility = volatility;
      if (q->volatility < 0) {
        q->volatility = 0;
      }
      if (q->volatility > 15) {
        q->volatility = 15;
      }
      q->ask = q->bid + q->burden;
      g_europe_cargo_burden[eu->cargo_count - 1] = q->burden;
      if (eu->cargo_count > g_europe_cargo_burden_count) {
        g_europe_cargo_burden_count = eu->cargo_count;
      }
    }
  }
  memset(eu->trade_nr, 0, sizeof(eu->trade_nr));

  const ColonizeMsgSection* classes = assets_msg_find(names, "CLASS");
  if (classes) {
    for (int i = 0; i < classes->line_count && eu->class_count < EUROPE_CLASS_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", classes->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* comma = strchr(line, ',');
      if (!comma) {
        continue;
      }
      *comma = '\0';
      str_trim(line);
      const char* p = comma + 1;
      int cost = 0;
      if (!europe_parse_int_field(&p, &cost) || cost <= 0 || line[0] == '\0') {
        continue;
      }
      EuropeRecruitClass* c = &eu->classes[eu->class_count++];
      str_copy_trunc(c->name, sizeof(c->name), line);
      c->cost = cost;
    }
  }

  /* @JOB: name, expert_name, school_tier, europe_hire_cost */
  const ColonizeMsgSection* jobs = assets_msg_find(names, "JOB");
  if (jobs) {
    int job_index = 0;
    for (int i = 0; i < jobs->line_count && eu->train_count < EUROPE_TRAIN_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", jobs->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* c1 = strchr(line, ',');
      if (!c1) {
        continue;
      }
      *c1 = '\0';
      str_trim(line);
      char* c2 = strchr(c1 + 1, ',');
      if (!c2) {
        ++job_index;
        continue;
      }
      *c2 = '\0';
      char expert[40];
      snprintf(expert, sizeof(expert), "%s", c1 + 1);
      str_trim(expert);
      const char* p = c2 + 1;
      int tier = 0;
      int cost = 0;
      if (!europe_parse_int_field(&p, &tier) || !europe_parse_int_field(&p, &cost)) {
        ++job_index;
        continue;
      }
      (void)tier;
      (void)line;
      if (cost > 0 && expert[0] != '\0') {
        EuropeTrainOption* t = &eu->train[eu->train_count++];
        snprintf(t->expert_name, sizeof(t->expert_name), "%s", expert);
        t->job_index = job_index;
        t->cost = cost;
      }
      ++job_index;
    }
    europe_sort_train_by_cost(eu->train, eu->train_count);
  }

  const ColonizeMsgSection* home = assets_msg_find(names, "HOMEPORT");
  const ColonizeMsgSection* cname = assets_msg_find(names, "COLONYNAME");
  (void)home;
  (void)cname;

  europe_init_purchase_table(eu);
  return eu->cargo_count > 0;
}

void europe_set_nation(EuropeScreen* eu, int nation, const ColonizeMsgCatalog* names) {
  /* @HOMEPORT / @COUNTRY come from reports.c's shared NAMES.TXT accessors
   * now (audit SC-6/SC-7). The `names` catalog handed in here is read first
   * when it has the section — it is the *caller's* catalog, which may be a
   * different one from reports_load's. */
  /* NAMES.TXT @COLONYNAME through the shared sim-side parse, for callers
   * that hand in no catalog of their own. Copied at once: the accessor's
   * scratch buffer is reused by the next name lookup. */
  char shared_region[48];
  {
    const char* r = reports_names_field("COLONYNAME", nation < 0 || nation > 3 ? 0 : nation, 0);
    snprintf(shared_region, sizeof(shared_region), "%s", r ? r : "");
  }
  if (!eu) {
    return;
  }
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  if (names) {
    const ColonizeMsgSection* home = assets_msg_find(names, "HOMEPORT");
    const ColonizeMsgSection* reg = assets_msg_find(names, "COLONYNAME");
    if (home && nation >= 0 && nation < home->line_count) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", home->lines[nation]);
      str_trim(line);
      if (line[0] && line[0] != ';') {
        str_copy_trunc(eu->port_city, sizeof(eu->port_city), line);
      } else {
        str_copy_trunc(eu->port_city, sizeof(eu->port_city), reports_home_port_name(nation));
      }
    } else {
      str_copy_trunc(eu->port_city, sizeof(eu->port_city), reports_home_port_name(nation));
    }
    if (reg && nation >= 0 && nation < reg->line_count) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", reg->lines[nation]);
      str_trim(line);
      if (line[0] && line[0] != ';') {
        str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), line);
      } else {
        str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), shared_region);
      }
    } else {
      str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), shared_region);
    }
  } else {
    str_copy_trunc(eu->port_city, sizeof(eu->port_city), reports_home_port_name(nation));
    str_copy_trunc(eu->colony_region, sizeof(eu->colony_region), shared_region);
  }
  str_copy_trunc(eu->nation_name, sizeof(eu->nation_name), reports_nation_country_name(nation));
  /* FUN_38fd_0000(nation): DS:0x9e12 = nation, DS:0x84fc = its record
   * (viceroy_unpacked.c 58696-58702). The trade-volume term reads 0x9e12 for
   * the human test and the Dutch slot-3 damping. */
  eu->bound_nation = (uint8_t)nation;
}

int europe_voyage_turns_roll(ColonizeDosRng* rng, bool magellan, int ship_count) {
  if (!rng) {
    return 1;
  }
  /* 48d3:0042 RNG(1,100) always rolled; >0x59 && ship_counts>2 && !FF5 → 2. */
  const int roll = dos_rng_range(rng, 1, 100);
  if (roll > 89 && ship_count > 2 && !magellan) {
    return 2;
  }
  return 1;
}

static int europe_clamp_voyage_turns(int t) {
  if (t < 1) {
    return 1;
  }
  if (t > EUROPE_VOYAGE_TURNS_MAX) {
    return EUROPE_VOYAGE_TURNS_MAX;
  }
  return t;
}

void europe_reset_campaign(EuropeScreen* eu) {
  europe_reset_campaign_nation(eu, 0);
}

void europe_reset_campaign_nation(EuropeScreen* eu, int nation) {
  if (!eu) {
    return;
  }
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  europe_set_nation(eu, nation, NULL);
  eu->gold = 1000;
  eu->tax_percent = 0;
  eu->current_crosses = 0;
  /* Match new-game Col1 human needed seed (COLONY00); first EOT overwrites via 584a. */
  eu->needed_crosses = 9;
  eu->crosses_immigrant_seen = false;
  eu->liberty_bells_total = 0;
  eu->liberty_bells_last_turn = 0;
  eu->harbor_ships = 0;
  eu->expected_ships = 0;
  eu->bound_ships = 0;
  memset(eu->harbor, 0, sizeof(eu->harbor));
  memset(eu->expected, 0, sizeof(eu->expected));
  memset(eu->bound, 0, sizeof(eu->bound));
  eu->selected_harbor = -1;
  eu->selected_market = 0;
  eu->dock_count = 0;
  memset(eu->dock, 0, sizeof(eu->dock));
  eu->recruit_count = 0;
  eu->difficulty = 0; /* first EOT tick caches the real col1 difficulty */
  eu->bound_human = true; /* the port binds the screen to the human nation */
  europe_refresh_recruit_passage(eu);
  europe_init_pool(eu);
  europe_init_purchase_table(eu);
  eu->menu = EUROPE_MENU_NONE;
  eu->menu_selection = 0;
  eu->menu_dock_index = -1;
  eu->last_exit_valid = false;
  eu->open_on_dock = false;
  eu->price_event_count = 0;
  eu->immigration_score = 0;
  eu->immigration_pressure = 0;
  eu->boycott_bitmap = 0;
  /* DOS FUN_38fd_6024: recruit pool (+2..+4) filled; docks empty; pressure 0. */
  europe_set_status(eu, "Home port ready. Recruit / Purchase / Train / S Sail.");
}

int europe_compute_recruit_passage(
  int recruit_count, int difficulty, int current_crosses, int needed_crosses
) {
  if (difficulty < 0) {
    difficulty = 0;
  }
  if (difficulty > 8) {
    difficulty = 8;
  }
  const long base = (long)(recruit_count + difficulty + 7) * 20;
  long floor_val = base / 5;
  if (floor_val < 100) {
    floor_val = 100;
  }
  /* -(needed_crosses+1): DOS's own divide-by-zero guard (~X == -X-1). */
  const long denom = -((long)needed_crosses + 1);
  const long discount = ((base - floor_val) * (long)current_crosses) / denom;
  long passage = base + discount;
  if (passage < 10) {
    passage = 10;
  }
  return (int)passage;
}

static void europe_refresh_recruit_passage(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  eu->recruit_passage = europe_compute_recruit_passage(
    eu->recruit_count, eu->difficulty, eu->current_crosses, eu->needed_crosses
  );
}

static void europe_bump_recruit_count(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  if (eu->recruit_count < 180) {
    eu->recruit_count++;
  }
  europe_refresh_recruit_passage(eu);
}

/*
 * DOS FUN_38fd_0718 (raw 59098-59140) is the ONE harbor-spawn behind every
 * dock arrival — recruit (64432/64768), the crosses immigrant (68585) and
 * the AI hires (90665/92616) all reach it through thunk_FUN_291f_0b26 — so
 * the Soldier->Dragoon roll happens on every one of them, off the shared
 * stream. europe_dock_type_for's name lookup stays in front of it (the port
 * files purchases by @UNIT name, which DOS spawns by a different path).
 */
static int europe_dock_type_roll(
  const EuropeScreen* eu, const char* name, int profession, ColonizeDosRng* rng
) {
  if (name && name[0]) {
    for (int i = 0; i < EUROPE_DOCK_TYPE_COUNT; ++i) {
      if (strcmp(name, reports_dock_type_name(i)) == 0) {
        return i;
      }
    }
  }
  return europe_dock_unit_dos_type(
    profession, eu ? (int)eu->difficulty : 0, eu ? eu->bound_human : true, rng
  );
}

bool europe_recruit_from_pool_ex(EuropeScreen* eu, int pool_index, ColonizeDosRng* rng) {
  if (!eu || pool_index < 0 || pool_index >= EUROPE_POOL_SIZE) {
    return false;
  }
  if (!eu->pool[pool_index].filled) {
    europe_set_status(eu, "That recruit is unavailable.");
    return false;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  if (eu->gold < eu->recruit_passage) {
    snprintf(
      eu->status,
      sizeof(eu->status),
      "Need %d$ passage for %s.",
      eu->recruit_passage,
      eu->pool[pool_index].name
    );
    return false;
  }
  eu->gold -= eu->recruit_passage;
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", eu->pool[pool_index].name);
  slot->profession = eu->pool[pool_index].profession;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_roll(eu, slot->name, slot->profession, rng);
  snprintf(
    eu->status,
    sizeof(eu->status),
    "Recruited %s (-%d$).",
    slot->name,
    eu->recruit_passage
  );
  diag_info(
    "EUROPE recruited %s (job %d) for %d$ passage (gold=%d)",
    slot->name, slot->profession, eu->recruit_passage, eu->gold
  );
  /*
   * FUN_38fd_4884 tail, param_1==0 (viceroy_unpacked.c:64765): a *paid*
   * passage clears the crosses meter (nation+0x2e = 0) before the +6
   * counter bumps. Without it the discount term kept the next price pinned
   * near the 100 floor, so buying colonists never walked the price ladder
   * up (bugs.md: "recruit price isn't increased by recruiting with gold").
   */
  eu->current_crosses = 0;
  eu->immigration_pressure = 0;
  europe_bump_recruit_count(eu);
  /* 64776: the emptied slot is refilled by `46d4(0)` off the shared stream. */
  europe_refill_pool_slot_rng(eu, pool_index, false, rng);
  return true;
}

bool europe_recruit_free_from_pool_ex(
  EuropeScreen* eu, int pool_index, ColonizeDosRng* rng
) {
  if (!eu || pool_index < 0 || pool_index >= EUROPE_POOL_SIZE) {
    return false;
  }
  if (!eu->pool[pool_index].filled) {
    europe_refill_pool_slot_rng(eu, pool_index, false, rng);
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  /* FUN_38fd_4884 with param_1 != 0: passage forced to 0, the +6 recruit
   * counter and the +0x2e crosses word are left alone (64695-64697, 64778). */
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", eu->pool[pool_index].name);
  slot->profession = eu->pool[pool_index].profession;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_roll(eu, slot->name, slot->profession, rng);
  snprintf(eu->status, sizeof(eu->status), "%s joins the docks.", slot->name);
  europe_refill_pool_slot_rng(eu, pool_index, false, rng);
  return true;
}

bool europe_brewster_pick_from_pool_ex(
  EuropeScreen* eu, int pool_index, ColonizeDosRng* rng
) {
  if (!eu || !europe_recruit_free_from_pool_ex(eu, pool_index, rng)) {
    return false;
  }
  /* 4884 tail with param_1==0: +0x2e crosses zeroed after the pick; no +6
   * recruit-count bump (param_2!=0 skips it). */
  eu->current_crosses = 0;
  eu->immigration_pressure = 0;
  eu->crosses_immigrant_seen = true;
  /* bugs.md #223: do NOT clear open_on_dock here — that flag belongs to a
   * ship ARRIVAL (DS:0x14c). A Brewster pick answered after the end of turn
   * was wiping the pending auto-open of the ship that had just docked. */
  europe_refresh_recruit_passage(eu);
  snprintf(eu->status, sizeof(eu->status), "Immigrant arrives in Europe.");
  return true;
}

bool europe_immigrant_from_pool(EuropeScreen* eu, ColonizeDosRng* rng) {
  if (!eu || eu->dock_count >= EUROPE_DOCK_MAX) {
    return false;
  }
  int slot = -1;
  if (rng) {
    /* DOS `5e52` phase 5: 04d4(0,2) rolls the slot before rerolling it. */
    const int roll = dos_rng_range(rng, 0, EUROPE_POOL_SIZE - 1);
    if (eu->pool[roll].filled) {
      slot = roll;
    }
  }
  if (slot < 0) {
    for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
      if (eu->pool[i].filled) {
        slot = i;
        break;
      }
    }
  }
  if (slot < 0) {
    europe_refill_pool_slot_rng(eu, 0, false, rng);
    slot = 0;
  }
  EuropeDockImmigrant* d = &eu->dock[eu->dock_count++];
  memset(d, 0, sizeof(*d));
  snprintf(d->name, sizeof(d->name), "%s", eu->pool[slot].name);
  d->profession = eu->pool[slot].profession;
  d->present = true;
  d->sentry = true;
  d->dos_type = europe_dock_type_roll(eu, d->name, d->profession, rng);
  /* DOS 0718 harbor-spawn does NOT bump Europe+6 — only 4884's own real
   * Recruit-click tail does (see europe_compute_recruit_passage). */
  /* 68583: this refill is `46d4((turn & 3) == 0)`, not `46d4(0)` — and it
   * rolls off the same shared stream as the 04d4(0,2) slot pick above, the
   * two draws back to back (68581/68583). */
  europe_refill_pool_slot_rng(eu, slot, eu->pool_force_expert, rng);
  return true;
}

bool europe_train(EuropeScreen* eu, int train_index) {
  return europe_train_ex(eu, train_index, NULL);
}

bool europe_train_ex(EuropeScreen* eu, int train_index, ColonizeDosRng* rng) {
  if (!eu || train_index < 0 || train_index >= eu->train_count) {
    return false;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  const EuropeTrainOption* t = &eu->train[train_index];
  if (eu->gold < t->cost) {
    snprintf(eu->status, sizeof(eu->status), "Need %d$ for %s.", t->cost, t->expert_name);
    return false;
  }
  eu->gold -= t->cost;
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", t->expert_name);
  slot->profession = t->job_index;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_roll(eu, slot->name, slot->profession, rng);
  snprintf(eu->status, sizeof(eu->status), "Trained %s (-%d$).", t->expert_name, t->cost);
  diag_info("EUROPE trained %s for %d$ (gold=%d)", t->expert_name, t->cost, eu->gold);
  return true;
}

int europe_purchase_cost(const EuropeScreen* eu, int purchase_index) {
  if (!eu || purchase_index < 0 || purchase_index >= eu->purchase_count) {
    return -1;
  }
  const EuropePurchaseOption* p = &eu->purchase[purchase_index];
  int cost = p->gold;
  /* FUN_38fd_4b50: type 0xb (Artillery) costs base + nation+0x1e * 100. */
  if (p->kind == UNITS_KIND_ARTILLERY && eu->artillery_bought > 0) {
    cost += eu->artillery_bought * 100;
  }
  return cost;
}

bool europe_purchase(EuropeScreen* eu, int purchase_index) {
  return europe_purchase_ex(eu, purchase_index, NULL);
}

bool europe_purchase_ex(EuropeScreen* eu, int purchase_index, ColonizeDosRng* rng) {
  if (!eu || purchase_index < 0 || purchase_index >= eu->purchase_count) {
    return false;
  }
  const EuropePurchaseOption* p = &eu->purchase[purchase_index];
  const int cost = europe_purchase_cost(eu, purchase_index);
  const bool is_artillery = p->kind == UNITS_KIND_ARTILLERY;
  if (eu->gold < cost) {
    snprintf(eu->status, sizeof(eu->status), "Need %d$ for %s.", cost, p->name);
    return false;
  }
  if (p->is_ship) {
    if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
      europe_set_status(eu, "Harbor is full.");
      return false;
    }
    eu->gold -= cost;
    EuropeHarborShip* slot = &eu->harbor[eu->harbor_ships++];
    europe_clear_ship(slot);
    slot->type_index = -1; /* resolved by name in game_loop / caller */
    snprintf(slot->name, sizeof(slot->name), "%s", p->name);
    europe_refresh_harbor_selection(eu);
    snprintf(eu->status, sizeof(eu->status), "Purchased %s (-%d$).", p->name, cost);
    diag_info("EUROPE purchased ship %s for %d$ (gold=%d)", p->name, cost, eu->gold);
    return true;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  eu->gold -= cost;
  /* FUN_38fd_4b50 purchase arm: nation+0x1e += 1 after charging. */
  if (is_artillery) {
    eu->artillery_bought += 1;
  }
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->name, sizeof(slot->name), "%s", p->name);
  slot->profession = -1;
  slot->present = true;
  slot->sentry = true;
  slot->dos_type = europe_dock_type_roll(eu, slot->name, slot->profession, rng);
  snprintf(eu->status, sizeof(eu->status), "Purchased %s (-%d$).", p->name, cost);
  diag_info("EUROPE purchased %s for %d$ (gold=%d)", p->name, cost, eu->gold);
  return true;
}

bool europe_open_recruit_menu(EuropeScreen* eu) {
  if (!eu) {
    return false;
  }
  europe_menu_open(eu, EUROPE_MENU_RECRUIT);
  return true;
}

int europe_dock_unit_dos_type(int profession, int difficulty, bool human, ColonizeDosRng* rng) {
  int type = 0; /* Colonists */
  if (profession == UNITS_JOB_PIONEER) {
    type = 2; /* Pioneers */
  } else if (profession == UNITS_JOB_MISSIONARY) {
    type = 3; /* Missionaries */
  } else if (profession == UNITS_JOB_SCOUT) {
    type = 5; /* Scouts */
  } else if (profession == UNITS_JOB_SOLDIER) {
    type = 1; /* Soldiers */
    if (rng) {
      const int bound = human ? difficulty : 1;
      if (dos_rng_range(rng, 0, bound + 4) == 0) {
        type = 4; /* Dragoons */
      }
    }
  }
  return type;
}

int europe_dock_type_for(const char* name, int profession) {
  if (name && name[0]) {
    for (int i = 0; i < EUROPE_DOCK_TYPE_COUNT; ++i) {
      if (strcmp(name, reports_dock_type_name(i)) == 0) {
        return i;
      }
    }
  }
  return europe_dock_unit_dos_type(profession, 0, true, NULL);
}

/*
 * FUN_38fd_3694 (raw 61183-61197): the dock caption on the status line —
 * 0056(1) opens the line, 0074 appends the @NATIONALITY adjective of the
 * bound nation (DS -0x72f6) and the @UNIT plural of the immigrant's type
 * (0x5230 + type*0xe); then, only when +0x315b (profession) != 0x1c, it
 * appends " (" + the @JOB singular (-0x715e + prof*8) + ")".
 */
bool europe_dock_caption(const EuropeScreen* eu, int dock_index, char* out, size_t cap) {
  if (!eu || !out || cap == 0) {
    return false;
  }
  out[0] = 0;
  if (dock_index < 0 || dock_index >= eu->dock_count || dock_index >= EUROPE_DOCK_MAX) {
    return false;
  }
  const EuropeDockImmigrant* d = &eu->dock[dock_index];
  const char* adj = reports_nation_adjective_display_name((int)eu->bound_nation);
  const char* type_name = reports_dock_type_name(d->dos_type);
  const int prof = d->profession;
  const char* job_name = (prof >= 0 && prof != UNITS_JOB_NONE) ? reports_job_name(prof) : NULL;
  if (job_name && job_name[0]) {
    snprintf(out, cap, "%s %s (%s)", adj ? adj : "", type_name ? type_name : "", job_name);
  } else {
    snprintf(out, cap, "%s %s", adj ? adj : "", type_name ? type_name : "");
  }
  return true;
}

static int europe_dock_type_tools(int dos_type) {
  return dos_type == EUROPE_DOCK_TYPE_PIONEERS ? 100 : 0;
}

static int europe_dock_type_muskets(int dos_type) {
  return (dos_type == EUROPE_DOCK_TYPE_SOLDIERS || dos_type == EUROPE_DOCK_TYPE_DRAGOONS) ? 50 : 0;
}

static int europe_dock_type_horses(int dos_type) {
  return (dos_type == EUROPE_DOCK_TYPE_DRAGOONS || dos_type == EUROPE_DOCK_TYPE_SCOUTS) ? 50 : 0;
}

/*
 * Audit SC-20/AE-18. The six dock type names are NAMES.TXT @UNIT rows 0..5
 * (reports_dock_type_name) — they were typed out twice in this file 24
 * lines apart, and a third time in ai_euro.c's ai_euro_5d04_linux_type_for
 * with singular fallbacks bolted on.
 *
 * `with_singular_fallback` is that third spelling: when the pool has no
 * "Soldiers" row, try "Soldier"; likewise Pioneer/Missionary/Dragoon/Scout,
 * and Colonists → "Free Colonist" → "Colonist". The Europe screen itself
 * always passes false (its pool is built from the same @UNIT rows, so the
 * plural always resolves); the 5d04 AI purchase path passes true because it
 * also runs against hand-built test pools.
 */
int europe_dock_unit_type_index_ex(
  const ColonizeUnitPool* units, int dos_type, bool with_singular_fallback
) {
  /* EUROPE_DOCK_TYPE_* (0..5) shares its order with ColonizeUnitKind's
   * COLONIST/SOLDIER/PIONEER/MISSIONARY/DRAGOON/SCOUT (0..5) — the singular
   * fallback used to re-derive that identity by re-matching a hand-typed
   * English name; it now goes through the @UNIT row directly. */
  if (!units || dos_type < 0 || dos_type >= EUROPE_DOCK_TYPE_COUNT) {
    return -1;
  }
  int t = units_find_type((ColonizeUnitPool*)units, reports_dock_type_name(dos_type));
  if (t >= 0 || !with_singular_fallback) {
    return t;
  }
  return units_kind_type_index(units, (ColonizeUnitKind)dos_type);
}

int europe_dock_unit_type_index(const ColonizeUnitPool* units, int dos_type) {
  return europe_dock_unit_type_index_ex(units, dos_type, false);
}

/*
 * @UNIT display type for a dock entry — what unit_chrome uses to place the
 * orders/allegiance box (Dragoons/Scouts top-left, Artillery top-center,
 * everyone else bottom-right), matching what units_display_type_index
 * derives from a landed unit's kit. A dock entry's name is a profession
 * ("Veteran Soldiers"), never an @UNIT type, so looking the name up first
 * missed and fell back to Colonists: every armed or mounted immigrant wore
 * the plain bottom-right box, a Veteran Dragoon most visibly (bugs.md).
 * dos_type is the field the @ARMOPTIONS rows move around and is what
 * europe_dock_sprite already picks the sprite from; Artillery carries no
 * dos_type of its own and is name-flagged there, so it is here too.
 */
int europe_dock_display_type_index(
  const ColonizeUnitPool* units, const EuropeDockImmigrant* d
) {
  if (!units || !d) {
    return -1;
  }
  int ti = -1;
  if (europe_dock_name_is_artillery(d->name)) {
    ti = units_kind_type_index(units, UNITS_KIND_ARTILLERY);
  }
  if (ti < 0) {
    ti = europe_dock_unit_type_index(units, d->dos_type);
  }
  if (ti < 0) {
    ti = units_find_type(units, d->name);
  }
  if (ti < 0) {
    ti = units_kind_type_index(units, UNITS_KIND_COLONIST);
  }
  return ti;
}

void europe_apply_dock_unit_kit(ColonizeUnit* u, int dos_type) {
  if (!u) {
    return;
  }
  /*
   * DOS's harbor spawn writes only the Tools byte (+0x3159) because
   * muskets and horses ride on the @UNIT type itself. This port keeps them
   * as explicit unit fields, so set all three from the type — which is also
   * what the @ARMOPTIONS rows need when they move an immigrant between
   * Colonists / Soldiers / Pioneers / Dragoons / Scouts.
   */
  u->tools = europe_dock_type_tools(dos_type);
  u->muskets = europe_dock_type_muskets(dos_type);
  u->horses = europe_dock_type_horses(dos_type);
}

/*
 * Re-type and re-kit the (236,236) mirror unit behind a dock entry after an
 * @ARMOPTIONS row changed it. Matched on profession the same way
 * europe_remove_dock_mirror_unit matches, with the pre-change type as the
 * tie-break so two immigrants of the same profession do not swap.
 */
static void europe_retype_dock_mirror_unit(
  ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int from_dos_type,
  int to_dos_type
) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return;
  }
  const int from_ti = europe_dock_unit_type_index(units, from_dos_type);
  const int to_ti = europe_dock_unit_type_index(units, to_dos_type);
  if (to_ti < 0) {
    return;
  }
  int fallback = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->x != 236 || u->y != 236 ||
        u->aboard_ship_id >= 0 || units_is_sea(units, u->id)) {
      continue;
    }
    if (u->profession != profession) {
      continue;
    }
    if (from_ti >= 0 && u->type_index == from_ti) {
      u->type_index = to_ti;
      europe_apply_dock_unit_kit(u, to_dos_type);
      return;
    }
    if (fallback < 0) {
      fallback = i;
    }
  }
  if (fallback >= 0) {
    units->units[fallback].type_index = to_ti;
    europe_apply_dock_unit_kit(&units->units[fallback], to_dos_type);
  }
}

int europe_spawn_dock_mirror_unit(
  ColonizeUnitPool* units,
  int nation_id,
  int profession,
  int difficulty,
  bool human,
  ColonizeDosRng* rng
) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return -1;
  }
  const int dos_type = europe_dock_unit_dos_type(profession, difficulty, human, rng);
  int ti = europe_dock_unit_type_index(units, dos_type);
  if (ti < 0) {
    ti = units_kind_type_index(units, UNITS_KIND_COLONIST);
  }
  const int id = units_spawn_allow_stack(units, ti >= 0 ? ti : 0, 236, 236);
  ColonizeUnit* u = units_get(units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation_id);
  u->orders = UNITS_ORDER_SENTRY; /* DOS +0x314c = 1 */
  u->profession = profession;
  u->goto_x = 0;
  u->goto_y = 0;
  u->moves = 0;
  europe_apply_dock_unit_kit(u, dos_type);
  return id;
}

void europe_remove_dock_mirror_unit(ColonizeUnitPool* units, int nation_id, int profession) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return;
  }
  int fallback = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id || u->x != 236 || u->y != 236 ||
        u->aboard_ship_id >= 0 || units_is_sea(units, u->id)) {
      continue;
    }
    if (u->profession == profession) {
      (void)units_disband(units, u->id);
      return;
    }
    if (fallback < 0) {
      fallback = u->id;
    }
  }
  if (fallback >= 0) {
    (void)units_disband(units, fallback);
  }
}

/*
 * GAME.TXT @ARMOPTIONS — the menu behind a click on a dock immigrant.
 *
 * Prices, from DOS 38fd:3745..3830: Muskets are @CARGO 0x0f at 50 a set,
 * Tools 0x0e at 100, Horses 0x08 at 50, each priced with the buy accessor
 * (FUN_281f_0c3e) or the sell one (FUN_281f_09ea) times that quantity.
 * Which of the pair a row shows is the unit's @UNIT type: Muskets sell for
 * Soldiers or Dragoons and buy otherwise, Tools sell for Pioneers, Horses
 * sell for Dragoons or Scouts.
 */
static int europe_arm_buy_cost(const EuropeScreen* eu, int cargo, int qty) {
  return europe_buy_price(eu, cargo) * qty;
}

/*
 * Disarm/de-equip proceeds are UNTAXED — smell audit #62, REFUTED: DOS credits
 * the raw gross.
 *
 * The three sell rows of the @ARMOPTIONS switch (FUN_38fd_3746's jump table at
 * 38fd:3c5a; Ghidra leaves the case bodies as raw bytes, so read them in
 * original_sources_decompiled/viceroy_ndisasm.asm at file offsets 0x3402E.. —
 * segment base 38fd = 0x30550) each do exactly two things:
 *
 *   38fd:3b4a  Sell Muskets  add [bx+0x2a],[bp-0x5e]   ; treasury += sell·50
 *                            call 291f_0a2e(0x0f, 0x32)
 *   38fd:3ba0  Sell Tools    add [bx+0x2a],[bp-0x5c]   ; treasury += sell·100
 *                            call 291f_0a2e(0x0e, 0x64)
 *   38fd:3bfc  Sell Horses   add [bx+0x2a],[bp-0x5a]   ; treasury += sell·50
 *                            call 291f_0a2e(0x08, 0x32)
 *
 * where [bp-0x5e/0x5c/0x5a] are the prologue's sell_price(cargo)·qty (38fd:3ce4
 * ..3d0e, via the 291f_09ea accessor) and 291f_0a2e is the sell ledger
 * FUN_38fd_1dfa (viceroy_unpacked.c 60247-60299).
 *
 * The harbor cargo sale is the contrast: FUN_38fd_23c4 (viceroy 60557-60575)
 * takes 1f0c's gross, computes tax = tax_rate·gross/100, credits only
 * gross − tax to the treasury and books tax into nation +0x22 (royal_money)
 * and the net into +0x26. None of that appears at the arm rows — no tax split,
 * no royal_money, no +0x26 — so the disarm gross lands whole in the treasury.
 * The Crown's cut is still reflected in the per-cargo revenue ledger, which is
 * FUN_38fd_1dfa's own (100−tax) term, applied below by the ledger call — the
 * `europe_apply_trade_volume` in europe_apply_dock_menu_row_ex, which must be
 * handed a non-NULL col1 for that ledger to move at all.
 */
static int europe_arm_sell_gain(const EuropeScreen* eu, int cargo, int qty) {
  return europe_sell_price(eu, cargo) * qty;
}

/*
 * Per-row enable, transcribed from the switch at 38fd:388e..3a04. `price` is
 * DOS's [bp-0x6a]: non-zero only on the three buy rows, and the tail greys
 * the row when the treasury cannot cover it. A boycotted cargo disables its
 * rows outright (the FUN_38fd_68c7 test each arm row runs), and an Indian
 * Convert (@JOB 0x1b) can be neither armed, equipped nor blessed.
 */
static bool europe_arm_row_enabled(
  const EuropeScreen* eu,
  const EuropeDockImmigrant* d,
  int row,
  int* out_price
) {
  const int t = d->dos_type;
  const bool convert = (d->profession == UNITS_JOB_CONVERT);
  *out_price = 0;
  switch (row) {
    case EUROPE_ARM_ROW_NO_BOARD:
      return d->sentry;
    case EUROPE_ARM_ROW_BOARD:
      return !d->sentry;
    case EUROPE_ARM_ROW_TO_FRONT:
      /* DS:0x9e2c is this immigrant's place in the dock queue. */
      return eu->menu_dock_index > 0;
    case EUROPE_ARM_ROW_BUY_MUSKETS:
      *out_price = europe_arm_buy_cost(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_MUSKETS)) {
        return false;
      }
      return !convert &&
             (t == EUROPE_DOCK_TYPE_COLONISTS || t == EUROPE_DOCK_TYPE_SCOUTS);
    case EUROPE_ARM_ROW_SELL_MUSKETS:
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_MUSKETS)) {
        return false;
      }
      return t == EUROPE_DOCK_TYPE_SOLDIERS || t == EUROPE_DOCK_TYPE_DRAGOONS;
    case EUROPE_ARM_ROW_BUY_TOOLS:
      *out_price = europe_arm_buy_cost(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_TOOLS)) {
        return false;
      }
      return !convert && t == EUROPE_DOCK_TYPE_COLONISTS;
    case EUROPE_ARM_ROW_SELL_TOOLS:
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_TOOLS)) {
        return false;
      }
      return t == EUROPE_DOCK_TYPE_PIONEERS;
    case EUROPE_ARM_ROW_BUY_HORSES:
      *out_price = europe_arm_buy_cost(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_HORSES)) {
        return false;
      }
      return !convert &&
             (t == EUROPE_DOCK_TYPE_COLONISTS || t == EUROPE_DOCK_TYPE_SOLDIERS);
    case EUROPE_ARM_ROW_SELL_HORSES:
      if (europe_cargo_boycotted(eu, COLONIZE_CARGO_HORSES)) {
        return false;
      }
      return t == EUROPE_DOCK_TYPE_SCOUTS || t == EUROPE_DOCK_TYPE_DRAGOONS;
    case EUROPE_ARM_ROW_BLESS:
      return !convert && t == EUROPE_DOCK_TYPE_COLONISTS;
    case EUROPE_ARM_ROW_UNBLESS:
      /*
       * DOS 38fd:39ec..3a09: enabled when the @UNIT type is Missionaries
       * (0x3146 == 3) AND the profession is NOT @JOB 0x18 (38fd:39fa
       * `cmp byte [bx+0x315b],0x18; jnz enable`) — i.e. only a *blessed*
       * ordinary colonist can cancel Missionary status; a born Jesuit
       * Missionary specialist cannot. Was inverted.
       */
      return t == EUROPE_DOCK_TYPE_MISSIONARIES && d->profession != UNITS_JOB_MISSIONARY;
    case EUROPE_ARM_ROW_NO_CHANGES:
    default:
      return true;
  }
}

void europe_build_dock_menu(
  EuropeScreen* eu,
  const struct ColonizeMsgCatalog* messages,
  int dock_index
) {
  if (!eu) {
    return;
  }
  eu->dock_menu_count = 0;
  if (dock_index < 0 || dock_index >= eu->dock_count) {
    return;
  }
  const EuropeDockImmigrant* d = &eu->dock[dock_index];
  const int t = d->dos_type;
  const bool sell_muskets =
    (t == EUROPE_DOCK_TYPE_SOLDIERS || t == EUROPE_DOCK_TYPE_DRAGOONS);
  const bool sell_tools = (t == EUROPE_DOCK_TYPE_PIONEERS);
  const bool sell_horses =
    (t == EUROPE_DOCK_TYPE_DRAGOONS || t == EUROPE_DOCK_TYPE_SCOUTS);

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.has_number0 = true;
  tok.number0 = sell_muskets
                  ? europe_arm_sell_gain(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS)
                  : europe_arm_buy_cost(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
  tok.has_number1 = true;
  tok.number1 = sell_tools
                  ? europe_arm_sell_gain(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS)
                  : europe_arm_buy_cost(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
  tok.has_number2 = true;
  tok.number2 = sell_horses
                  ? europe_arm_sell_gain(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES)
                  : europe_arm_buy_cost(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);

  /* GAME.TXT @ARMOPTIONS is required at startup (assets_validate_required_files);
   * a missing catalog is already fatal, so this fallback carries no MicroProse
   * wording — a catalog miss here just shows blank rows. */
  static const char* const k_fallback[EUROPE_DOCK_MENU_MAX] = {
    "", "", "", "", "", "", "", "", "", "", "", ""
  };
  const ColonizeMsgSection* sec =
    messages ? assets_msg_find((const ColonizeMsgCatalog*)messages, "ARMOPTIONS") : NULL;

  int row = 0; /* 0-based index into the section's non-directive lines */
  const int line_count = (sec && sec->line_count > 0) ? sec->line_count : EUROPE_DOCK_MENU_MAX;
  for (int i = 0; i < line_count && row < EUROPE_DOCK_MENU_MAX; ++i) {
    const char* line = (sec && i < sec->line_count) ? sec->lines[i] : k_fallback[row];
    if (sec && (popup_msg_is_directive(line) || line[0] == '\0')) {
      continue;
    }
    const int row_id = row + 1;
    ++row;
    int price = 0;
    if (!europe_arm_row_enabled(eu, d, row_id, &price)) {
      continue; /* DOS omits a disabled row rather than greying it. */
    }
    const int slot = eu->dock_menu_count;
    popup_msg_apply_tokens(
      eu->dock_menu_label[slot], sizeof(eu->dock_menu_label[slot]), line, &tok
    );
    eu->dock_menu_row[slot] = (uint8_t)row_id;
    eu->dock_menu_greyed[slot] = (price > 0 && eu->gold < price);
    eu->dock_menu_count++;
  }
}

/* @ARMOPTIONS row id -> readable name, for the debug log only. */
static const char* europe_arm_row_name(int row) {
  switch (row) {
    case EUROPE_ARM_ROW_NO_BOARD: return "no board";
    case EUROPE_ARM_ROW_BOARD: return "board";
    case EUROPE_ARM_ROW_TO_FRONT: return "to front";
    case EUROPE_ARM_ROW_BUY_MUSKETS: return "buy muskets";
    case EUROPE_ARM_ROW_SELL_MUSKETS: return "sell muskets";
    case EUROPE_ARM_ROW_BUY_TOOLS: return "buy tools";
    case EUROPE_ARM_ROW_SELL_TOOLS: return "sell tools";
    case EUROPE_ARM_ROW_BUY_HORSES: return "buy horses";
    case EUROPE_ARM_ROW_SELL_HORSES: return "sell horses";
    case EUROPE_ARM_ROW_BLESS: return "bless";
    case EUROPE_ARM_ROW_UNBLESS: return "unbless";
    case EUROPE_ARM_ROW_NO_CHANGES: return "no changes";
    default: return "row";
  }
}

static bool europe_apply_dock_menu_row_ex(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  ColonizeCol1Save* col1,
  int nation_id,
  int dock_index,
  int row
) {
  if (!eu || dock_index < 0 || dock_index >= eu->dock_count) {
    return false;
  }
  EuropeDockImmigrant* d = &eu->dock[dock_index];
  const int from = d->dos_type;
  int to = from;
  int gold_delta = 0;
  int ledger_cargo = -1;
  int ledger_qty = 0;
  int ledger_is_buy = 0;
  int sound = -1;

  switch (row) {
    case EUROPE_ARM_ROW_NO_BOARD:
      d->sentry = false;
      /* GAME.TXT @ARMOPTIONS row 0 ("Don't get on next ship."). */
      europe_set_status(eu, assets_msg_line_or(eu->messages, "ARMOPTIONS", 0, ""));
      return true;
    case EUROPE_ARM_ROW_BOARD:
      d->sentry = true;
      /* GAME.TXT @ARMOPTIONS row 1 ("Board next ship."). */
      europe_set_status(eu, assets_msg_line_or(eu->messages, "ARMOPTIONS", 1, ""));
      return true;
    case EUROPE_ARM_ROW_TO_FRONT: {
      if (dock_index <= 0) {
        return false;
      }
      const EuropeDockImmigrant moved = *d;
      for (int i = dock_index; i > 0; --i) {
        eu->dock[i] = eu->dock[i - 1];
      }
      eu->dock[0] = moved;
      eu->menu_dock_index = 0;
      /* GAME.TXT @ARMOPTIONS row 2 ("Move to front of dock."). */
      europe_set_status(eu, assets_msg_line_or(eu->messages, "ARMOPTIONS", 2, ""));
      return true;
    }
    /* 38fd:3ade — Scouts become Dragoons, anyone else Soldiers. */
    case EUROPE_ARM_ROW_BUY_MUSKETS:
      to = (from == EUROPE_DOCK_TYPE_SCOUTS) ? EUROPE_DOCK_TYPE_DRAGOONS
                                             : EUROPE_DOCK_TYPE_SOLDIERS;
      gold_delta = -europe_arm_buy_cost(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
      ledger_cargo = COLONIZE_CARGO_MUSKETS;
      ledger_qty = EUROPE_ARM_MUSKETS;
      ledger_is_buy = 1;
      sound = 0x58;
      break;
    case EUROPE_ARM_ROW_SELL_MUSKETS:
      to = (from == EUROPE_DOCK_TYPE_DRAGOONS) ? EUROPE_DOCK_TYPE_SCOUTS
                                               : EUROPE_DOCK_TYPE_COLONISTS;
      gold_delta = europe_arm_sell_gain(eu, COLONIZE_CARGO_MUSKETS, EUROPE_ARM_MUSKETS);
      ledger_cargo = COLONIZE_CARGO_MUSKETS;
      ledger_qty = EUROPE_ARM_MUSKETS;
      break;
    case EUROPE_ARM_ROW_BUY_TOOLS:
      to = EUROPE_DOCK_TYPE_PIONEERS;
      gold_delta = -europe_arm_buy_cost(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
      ledger_cargo = COLONIZE_CARGO_TOOLS;
      ledger_qty = EUROPE_ARM_TOOLS;
      ledger_is_buy = 1;
      break;
    case EUROPE_ARM_ROW_SELL_TOOLS:
      to = EUROPE_DOCK_TYPE_COLONISTS;
      gold_delta = europe_arm_sell_gain(eu, COLONIZE_CARGO_TOOLS, EUROPE_ARM_TOOLS);
      ledger_cargo = COLONIZE_CARGO_TOOLS;
      ledger_qty = EUROPE_ARM_TOOLS;
      break;
    /* 38fd:3bbe — Soldiers become Dragoons, anyone else Scouts. */
    case EUROPE_ARM_ROW_BUY_HORSES:
      to = (from == EUROPE_DOCK_TYPE_SOLDIERS) ? EUROPE_DOCK_TYPE_DRAGOONS
                                               : EUROPE_DOCK_TYPE_SCOUTS;
      gold_delta = -europe_arm_buy_cost(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);
      ledger_cargo = COLONIZE_CARGO_HORSES;
      ledger_qty = EUROPE_ARM_HORSES;
      ledger_is_buy = 1;
      sound = 0x5c;
      break;
    case EUROPE_ARM_ROW_SELL_HORSES:
      to = (from == EUROPE_DOCK_TYPE_DRAGOONS) ? EUROPE_DOCK_TYPE_SOLDIERS
                                               : EUROPE_DOCK_TYPE_COLONISTS;
      gold_delta = europe_arm_sell_gain(eu, COLONIZE_CARGO_HORSES, EUROPE_ARM_HORSES);
      ledger_cargo = COLONIZE_CARGO_HORSES;
      ledger_qty = EUROPE_ARM_HORSES;
      break;
    case EUROPE_ARM_ROW_BLESS:
      to = EUROPE_DOCK_TYPE_MISSIONARIES;
      sound = 0x8024; /* the same church chord the colony assign plays */
      break;
    case EUROPE_ARM_ROW_UNBLESS:
      to = EUROPE_DOCK_TYPE_COLONISTS;
      break;
    case EUROPE_ARM_ROW_NO_CHANGES:
    default:
      return true;
  }

  if (gold_delta < 0 && eu->gold < -gold_delta) {
    europe_set_status(eu, "The treasury cannot afford that.");
    return false;
  }
  /* 38fd:3b4a/3ba0/3bfc `add [bx+0x2a],..` — the bound record's treasury, so
   * the col1 word moves with the purse when a save is bound (audit G3). */
  europe_purse_move(eu, col1, nation_id, gold_delta);
  d->dos_type = to;
  if (ledger_cargo >= 0) {
    /*
     * Ledger only — no rise/fall step. The arm rows call the volume routines
     * bare (291f_0c14 = FUN_38fd_1d80 on a buy, 291f_0a2e = FUN_38fd_1dfa on a
     * sell) and nothing else; the 0058 threshold pass is what the *harbor*
     * handlers add at their tail (thunk_FUN_291f_0cbc(0, cargo) —
     * viceroy_unpacked.c 60500 for the buy FUN_38fd_1fa2, 60644 for the sell
     * FUN_38fd_23c4). Arming an immigrant therefore never moves the bid, so it
     * never raises @PRICEUP/@PRICEDOWN either. europe_apply_volume_price would
     * have run 0058, so call the full form with immediate_threshold = 0.
     */
    /*
     * `col1` is passed through so the per-cargo tons/tons2/gold ledger really
     * is written, as the disarm-tax note 300 lines above promises: DOS's
     * FUN_38fd_1dfa (viceroy_unpacked.c 60272-60295) adds the amount into
     * nation+0xbc and +0xfc and the (100−tax)-scaled gross into +0x7c on every
     * call, and the three arm sell rows call it bare. NULL is still accepted
     * (tests, and any caller with no save bound) and then only the price pool
     * moves, exactly as on the harbor channels.
     */
    const int bound = (int)eu->bound_nation;
    europe_apply_trade_volume(
      eu, col1, bound, bound, ledger_cargo, ledger_qty, ledger_is_buy, 0
    );
  }
  diag_info(
    "EUROPE dock %s: %s (%d) type %d -> %d, gold %+d (gold=%d)",
    d->name[0] ? d->name : "immigrant",
    europe_arm_row_name(row), row, from, to, gold_delta, eu->gold
  );
  if (units) {
    europe_retype_dock_mirror_unit(units, nation_id, d->profession, from, to);
  }
  if (sound >= 0 && g_europe_sound_play) {
    g_europe_sound_play(sound);
  }
  return true;
}

bool europe_apply_dock_menu_row(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int nation_id,
  int dock_index,
  int row
) {
  return europe_apply_dock_menu_row_ex(eu, units, NULL, nation_id, dock_index, row);
}

static bool europe_pop_dock_immigrant_ex(
  EuropeScreen* eu,
  char* out_name,
  size_t out_name_size,
  int* out_profession
);

bool europe_pop_dock_immigrant(EuropeScreen* eu, char* out_name, size_t out_name_size) {
  return europe_pop_dock_immigrant_ex(eu, out_name, out_name_size, NULL);
}

static bool europe_pop_dock_immigrant_ex(
  EuropeScreen* eu,
  char* out_name,
  size_t out_name_size,
  int* out_profession
) {
  if (!eu || eu->dock_count <= 0) {
    return false;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", eu->dock[0].name);
  }
  if (out_profession) {
    *out_profession = eu->dock[0].profession;
  }
  for (int i = 1; i < eu->dock_count; ++i) {
    eu->dock[i - 1] = eu->dock[i];
  }
  eu->dock_count--;
  memset(&eu->dock[eu->dock_count], 0, sizeof(eu->dock[0]));
  return true;
}

bool europe_harbor_push(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
) {
  return europe_harbor_push_ex(
    eu, type_index, name, cargo_types, NULL, cargo_count, hold_goods_type, hold_goods_amount
  );
}

bool europe_harbor_push_ex(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  const int* cargo_professions,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
) {
  if (!eu) {
    return false;
  }
  if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
    europe_set_status(eu, "Harbor is full.");
    return false;
  }
  EuropeHarborShip* slot = &eu->harbor[eu->harbor_ships++];
  europe_clear_ship(slot);
  slot->type_index = type_index;
  snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "Ship");
  if (cargo_types && cargo_count > 0) {
    const int n = cargo_count > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : cargo_count;
    for (int i = 0; i < n; ++i) {
      slot->cargo_types[i] = cargo_types[i];
      /* Carry the @JOB through exactly as the Expected mirror
       * (europe_enqueue_expected) does — without it a pushed ship's
       * passengers lost their profession label
       * (reports_naval_passenger_label fell back to the bare unit name). */
      slot->cargo_professions[i] = cargo_professions ? cargo_professions[i] : -1;
    }
    slot->cargo_count = n;
  }
  if (hold_goods_type && hold_goods_amount) {
    for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
      slot->hold_goods_type[i] = hold_goods_type[i];
      slot->hold_goods_amount[i] = hold_goods_amount[i];
    }
  }
  slot->turns_left = 0;
  snprintf(eu->status, sizeof(eu->status), "%s in harbor.", slot->name);
  europe_refresh_harbor_selection(eu);
  return true;
}

bool europe_enqueue_expected(
  EuropeScreen* eu,
  int type_index,
  const char* name,
  const int* cargo_types,
  const int* cargo_professions,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount,
  int exit_x,
  int exit_y,
  bool exit_east,
  int voyage_turns
) {
  if (!eu) {
    return false;
  }
  if (eu->expected_ships >= EUROPE_HARBOR_MAX) {
    europe_set_status(eu, "Expected Soon is full.");
    return false;
  }
  EuropeHarborShip* slot = &eu->expected[eu->expected_ships++];
  europe_clear_ship(slot);
  slot->type_index = type_index;
  snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "Ship");
  if (cargo_types && cargo_count > 0) {
    const int n = cargo_count > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : cargo_count;
    for (int i = 0; i < n; ++i) {
      slot->cargo_types[i] = cargo_types[i];
      slot->cargo_professions[i] =
        cargo_professions ? cargo_professions[i] : -1;
    }
    slot->cargo_count = n;
  }
  if (hold_goods_type && hold_goods_amount) {
    for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
      slot->hold_goods_type[i] = hold_goods_type[i];
      slot->hold_goods_amount[i] = hold_goods_amount[i];
    }
  }
  slot->exit_x = exit_x;
  slot->exit_y = exit_y;
  slot->exit_east = exit_east;
  slot->turns_left = europe_clamp_voyage_turns(voyage_turns);
  slot->departed_this_turn = true;
  eu->last_exit_x = exit_x;
  eu->last_exit_y = exit_y;
  eu->last_exit_east = exit_east;
  eu->last_exit_valid = true;
  snprintf(
    eu->status,
    sizeof(eu->status),
    "%s expected in %d turn(s).",
    slot->name,
    slot->turns_left
  );
  return true;
}

static void europe_board_sentry_dockers(
  EuropeScreen* eu,
  EuropeHarborShip* ship,
  ColonizeUnitPool* units,
  int nation_id,
  int cargo_cap
) {
  if (!eu || !ship || cargo_cap <= 0) {
    return;
  }
  const int goods = europe_goods_slots_used(ship);
  while (ship->cargo_count + goods < cargo_cap) {
    int di = -1;
    for (int i = 0; i < eu->dock_count; ++i) {
      if (eu->dock[i].present && eu->dock[i].sentry) {
        di = i;
        break; /* front of queue first */
      }
    }
    if (di < 0) {
      break;
    }
    int type_tag = 0;
    const bool is_artillery = europe_dock_name_is_artillery(eu->dock[di].name);
    if (is_artillery) {
      type_tag = -2;
    } else if (units) {
      /*
       * The dock entry's name is the expert plural ("Hardy Pioneers"), which
       * matches no @UNIT type, so this used to fall back to Colonists for
       * every specialist and the passenger landed as a plain colonist with no
       * kit. The entry now carries its own DOS type — set when it arrived and
       * moved by the @ARMOPTIONS rows — so a Soldier boards as Soldiers and a
       * Hardy Pioneer as Pioneers. The name lookup still wins for purchases
       * that really are typed by name (Artillery is handled above).
       */
      int ti = units_find_type(units, eu->dock[di].name);
      if (ti < 0) {
        ti = europe_dock_unit_type_index(units, eu->dock[di].dos_type);
      }
      type_tag = ti >= 0 ? ti : 0;
    }
    ship->cargo_types[ship->cargo_count] = type_tag;
    ship->cargo_professions[ship->cargo_count] = eu->dock[di].profession;
    ship->cargo_count++;
    /* The dock immigrant's (236,236) mirror unit leaves the pool with it. */
    europe_remove_dock_mirror_unit(units, nation_id, eu->dock[di].profession);
    for (int j = di + 1; j < eu->dock_count; ++j) {
      eu->dock[j - 1] = eu->dock[j];
    }
    eu->dock_count--;
    memset(&eu->dock[eu->dock_count], 0, sizeof(eu->dock[0]));
  }
}

bool europe_set_sail_from_harbor(
  EuropeScreen* eu,
  int harbor_index,
  int voyage_turns,
  ColonizeUnitPool* units,
  int nation_id
) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return false;
  }
  if (eu->bound_ships >= EUROPE_HARBOR_MAX) {
    europe_set_status(eu, "Outbound lane is full.");
    return false;
  }
  EuropeHarborShip ship;
  europe_copy_ship(&ship, &eu->harbor[harbor_index]);
  const int cargo_cap = europe_ship_cargo_cap(&ship, units);
  europe_board_sentry_dockers(eu, &ship, units, nation_id, cargo_cap);
  bool exit_east = eu->last_exit_valid ? eu->last_exit_east : true;
  ship.exit_east = exit_east;
  if (eu->last_exit_valid) {
    ship.exit_x = eu->last_exit_x;
    ship.exit_y = eu->last_exit_y;
  }
  ship.turns_left = europe_clamp_voyage_turns(voyage_turns);
  ship.departed_this_turn = true;
  for (int i = harbor_index + 1; i < eu->harbor_ships; ++i) {
    eu->harbor[i - 1] = eu->harbor[i];
  }
  eu->harbor_ships--;
  europe_clear_ship(&eu->harbor[eu->harbor_ships]);
  if (eu->selected_harbor == harbor_index) {
    eu->selected_harbor = -1;
  } else if (eu->selected_harbor > harbor_index) {
    eu->selected_harbor--;
  }
  europe_refresh_harbor_selection(eu);
  eu->bound[eu->bound_ships++] = ship;
  snprintf(
    eu->status,
    sizeof(eu->status),
    "%s bound for %s (%d turns).",
    ship.name,
    eu->colony_region[0] ? eu->colony_region : "",
    ship.turns_left
  );
  diag_info(
    "EUROPE %s departs for the colonies: %d turns, exit (%d,%d) %s",
    ship.name, ship.turns_left, ship.exit_x, ship.exit_y,
    ship.exit_east ? "east" : "west"
  );
  return true;
}

bool europe_reverse_transit(EuropeScreen* eu, bool from_expected, int index) {
  if (!eu) {
    return false;
  }
  if (from_expected) {
    if (index < 0 || index >= eu->expected_ships || eu->bound_ships >= EUROPE_HARBOR_MAX) {
      return false;
    }
    EuropeHarborShip ship = eu->expected[index];
    for (int i = index + 1; i < eu->expected_ships; ++i) {
      eu->expected[i - 1] = eu->expected[i];
    }
    eu->expected_ships--;
    europe_clear_ship(&eu->expected[eu->expected_ships]);
    eu->bound[eu->bound_ships++] = ship;
    europe_set_status(eu, "Reversed -- now sailing for the colonies.");
    return true;
  }
  if (index < 0 || index >= eu->bound_ships || eu->expected_ships >= EUROPE_HARBOR_MAX) {
    return false;
  }
  EuropeHarborShip ship = eu->bound[index];
  for (int i = index + 1; i < eu->bound_ships; ++i) {
    eu->bound[i - 1] = eu->bound[i];
  }
  eu->bound_ships--;
  europe_clear_ship(&eu->bound[eu->bound_ships]);
  eu->expected[eu->expected_ships++] = ship;
  europe_set_status(eu, "Reversed — now expected in Europe.");
  return true;
}

bool europe_harbor_pop(
  EuropeScreen* eu,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max,
  int* out_hold_goods_type,
  int* out_hold_goods_amount,
  int hold_max
) {
  if (!eu || eu->harbor_ships <= 0) {
    return false;
  }
  if (out_type_index) {
    *out_type_index = eu->harbor[0].type_index;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", eu->harbor[0].name);
  }
  if (out_cargo_count) {
    *out_cargo_count = 0;
  }
  if (out_cargo_types && out_cargo_count && cargo_max > 0) {
    const int n =
      eu->harbor[0].cargo_count > cargo_max ? cargo_max : eu->harbor[0].cargo_count;
    for (int i = 0; i < n; ++i) {
      out_cargo_types[i] = eu->harbor[0].cargo_types[i];
    }
    *out_cargo_count = n;
  }
  if (out_hold_goods_type && out_hold_goods_amount && hold_max > 0) {
    const int n = hold_max > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : hold_max;
    for (int i = 0; i < n; ++i) {
      out_hold_goods_type[i] = eu->harbor[0].hold_goods_type[i];
      out_hold_goods_amount[i] = eu->harbor[0].hold_goods_amount[i];
    }
    for (int i = n; i < hold_max; ++i) {
      out_hold_goods_type[i] = 0;
      out_hold_goods_amount[i] = 0;
    }
  }
  for (int i = 1; i < eu->harbor_ships; ++i) {
    eu->harbor[i - 1] = eu->harbor[i];
  }
  eu->harbor_ships--;
  europe_clear_ship(&eu->harbor[eu->harbor_ships]);
  if (eu->selected_harbor == 0) {
    eu->selected_harbor = -1;
  } else if (eu->selected_harbor > 0) {
    eu->selected_harbor--;
  }
  europe_refresh_harbor_selection(eu);
  return true;
}

bool europe_bound_pop_arrived(
  EuropeScreen* eu,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max,
  int* out_hold_goods_type,
  int* out_hold_goods_amount,
  int hold_max,
  int* out_exit_x,
  int* out_exit_y,
  bool* out_exit_east
) {
  if (!eu) {
    return false;
  }
  int idx = -1;
  for (int i = 0; i < eu->bound_ships; ++i) {
    if (eu->bound[i].turns_left <= 0) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    return false;
  }
  EuropeHarborShip* ship = &eu->bound[idx];
  if (out_type_index) {
    *out_type_index = ship->type_index;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", ship->name);
  }
  if (out_cargo_count) {
    *out_cargo_count = 0;
  }
  if (out_cargo_types && out_cargo_count && cargo_max > 0) {
    const int n = ship->cargo_count > cargo_max ? cargo_max : ship->cargo_count;
    for (int i = 0; i < n; ++i) {
      out_cargo_types[i] = ship->cargo_types[i];
    }
    *out_cargo_count = n;
  }
  if (out_hold_goods_type && out_hold_goods_amount && hold_max > 0) {
    const int n = hold_max > EUROPE_SHIP_CARGO_MAX ? EUROPE_SHIP_CARGO_MAX : hold_max;
    for (int i = 0; i < n; ++i) {
      out_hold_goods_type[i] = ship->hold_goods_type[i];
      out_hold_goods_amount[i] = ship->hold_goods_amount[i];
    }
  }
  if (out_exit_x) {
    *out_exit_x = ship->exit_x;
  }
  if (out_exit_y) {
    *out_exit_y = ship->exit_y;
  }
  if (out_exit_east) {
    *out_exit_east = ship->exit_east;
  }
  for (int i = idx + 1; i < eu->bound_ships; ++i) {
    eu->bound[i - 1] = eu->bound[i];
  }
  eu->bound_ships--;
  europe_clear_ship(&eu->bound[eu->bound_ships]);
  return true;
}

void europe_refresh_harbor_selection(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  if (eu->harbor_ships <= 0) {
    eu->selected_harbor = -1;
    return;
  }
  if (eu->selected_harbor >= 0 && eu->selected_harbor < eu->harbor_ships) {
    return;
  }
  eu->selected_harbor = 0;
}

void europe_tick_voyages(EuropeScreen* eu, const ColonizeUnitPool* units) {
  if (!eu) {
    return;
  }
  eu->open_on_dock = false;
  eu->docked_with_goods = false;
  /*
   * A ship that entered its lane during the turn just ending spends this
   * tick at sea without the counter moving — see EuropeHarborShip's
   * departed_this_turn. Without it an ordinary 1-turn crossing docked on
   * the very next turn, where DOS needs two End Turns (bugs.md).
   */
  for (int i = 0; i < eu->expected_ships; ++i) {
    if (eu->expected[i].departed_this_turn) {
      eu->expected[i].departed_this_turn = false;
    } else if (eu->expected[i].turns_left > 0) {
      eu->expected[i].turns_left--;
    }
  }
  for (int i = 0; i < eu->bound_ships; ++i) {
    if (eu->bound[i].departed_this_turn) {
      eu->bound[i].departed_this_turn = false;
    } else if (eu->bound[i].turns_left > 0) {
      eu->bound[i].turns_left--;
    }
  }
  /* Move due Expected ships into harbor; passengers go to dock front. */
  int i = 0;
  while (i < eu->expected_ships) {
    if (eu->expected[i].turns_left > 0) {
      ++i;
      continue;
    }
    if (eu->harbor_ships >= EUROPE_HARBOR_MAX) {
      break;
    }
    EuropeHarborShip ship = eu->expected[i];
    for (int j = i + 1; j < eu->expected_ships; ++j) {
      eu->expected[j - 1] = eu->expected[j];
    }
    eu->expected_ships--;
    europe_clear_ship(&eu->expected[eu->expected_ships]);
    ship.turns_left = 0;
    europe_disembark_passengers_to_dock(eu, &ship, units);
    eu->harbor[eu->harbor_ships++] = ship;
    eu->open_on_dock = true;
    /* 255 is the empty-hold sentinel, not a full hold — the same guard every
     * other hold consumer carries (europe_goods_slots_used, europe_sell_hold
     * / _partial, europe_best_sell_hold, europe_buy_cargo, reports.c's cargo
     * rows). units_despawn_ship_with_cargo copies the raw hold bytes in, so
     * without it a sentinel hold fired the once-per-game "Cargo from the New
     * World" woodcut (turn.c → WOODCUT_CARGO_FROM_THE_NEW_WORLD). */
    for (int g = 0; g < EUROPE_SHIP_CARGO_MAX; ++g) {
      if (ship.hold_goods_amount[g] > 0 && ship.hold_goods_amount[g] < 255) {
        eu->docked_with_goods = true;
        break;
      }
    }
    europe_refresh_harbor_selection(eu);
    snprintf(eu->status, sizeof(eu->status), "%s has docked in %s.", ship.name, eu->port_city);
  }
}

void europe_set_sound_hook(void (*play_fn)(int id)) {
  g_europe_sound_play = play_fn;
}
void europe_set_bgm_hook(void (*set_bgm_fn)(int pool)) {
  g_europe_set_bgm = set_bgm_fn;
}
void europe_notify_immigrant_sound(EuropeScreen* eu) {
  (void)eu;
  if (g_europe_set_bgm) {
    g_europe_set_bgm(2); /* 281f_0498(2) for the human's immigrant beat */
  }
}

int europe_cash_treasure(EuropeScreen* eu, int treasure_value) {
  if (!eu || treasure_value <= 0) {
    return 0;
  }
  int tax = eu->tax_percent;
  if (tax < 0) {
    tax = 0;
  }
  if (tax > 100) {
    tax = 100;
  }
  /*
   * FUN_48d3_06ba: Crown cut = min(tax_rate, 50). GAME.TXT @LOOTCASH still
   * describes tax-rate share; DOS clamps the treasure path at 50%.
   */
  if (tax > 50) {
    tax = 50;
  }
  const int credited = (treasure_value * (100 - tax)) / 100;
  eu->gold += credited;
  /* GAME.TXT @LOOTCASH, composed live via eu->messages (europe_set_messages). */
  const char* nation = eu->nation_name[0] ? eu->nation_name : "";
  const char* port = eu->port_city[0] ? eu->port_city : "";
  eu->status[0] = '\0';
  if (eu->messages) {
    PopupMsgTokens tok = {0};
    tok.string0 = nation;
    tok.string1 = port;
    tok.has_number0 = true;
    tok.number0 = treasure_value;
    tok.has_number1 = true;
    tok.number1 = tax;
    tok.has_number2 = true;
    tok.number2 = credited;
    popup_msg_fill(eu->messages, "LOOTCASH", &tok, "", eu->status, sizeof(eu->status));
    popup_msg_strip_markup(eu->status);
  }
  diag_info(
    "EUROPE treasure cashed %d$: crown %d%% share, credited %d$ (gold=%d)",
    treasure_value, tax, credited, eu->gold
  );
  if (g_europe_sound_play) {
    g_europe_sound_play(0x24); /* FUN_48d3_06ba 48d3:0b8f: Fiddler's Dance queued on the cash-in */
  }
  return credited;
}

int europe_sell_price(const EuropeScreen* eu, int cargo_type) {
  /* FUN_38fd_0040: max(euro_price − 1, 0). */
  if (!eu || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  const int p = eu->cargo[cargo_type].bid - 1;
  return p < 0 ? 0 : p;
}

int europe_buy_price(const EuropeScreen* eu, int cargo_type) {
  /* FUN_38fd_0016: max(euro_price + burden, 0) — cached as `ask`. */
  if (!eu || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  return eu->cargo[cargo_type].ask < 0 ? 0 : eu->cargo[cargo_type].ask;
}

int europe_net_after_tax(int gross, int tax_percent) {
  if (gross <= 0) {
    return 0;
  }
  if (tax_percent < 0) {
    tax_percent = 0;
  }
  if (tax_percent > 100) {
    tax_percent = 100;
  }
  /* FUN_364b_0688: tax = (tax·gross)/100 (32-bit), net = gross − tax. */
  const long taxed = ((long)tax_percent * (long)gross) / 100L;
  return gross - (int)taxed;
}

static int europe_1d44_term(int amount, int seller_is_human, int difficulty) {
  /* FUN_38fd_1d44 (viceroy_unpacked.c 60186-60201):
   *   k = (0x9e12 < 4 && 0x543f[0x9e12*0x34] == 0) ? DS:0x53a6 − 2 : −2
   * i.e. the BOUND nation (the one whose record 0x84fc the ledger credits =
   * the trading nation) being human-controlled picks the difficulty arm; an
   * AI (or a crown/indian slot ≥ 4) always gets −2. Then
   * (k · 16 · amount) / 100, C division (truncates toward zero — the AI
   * −384/100 → −3 case is what the dutch2 pair needs). */
  const int k = seller_is_human ? (difficulty - 2) : -2;
  return (k * 16 * amount) / 100;
}

/* Clamp bid into @CARGO [low, high] (only when both were loaded), floor at 0,
 * re-derive ask = bid + burden. Shared by the player-move and EOT tickers. */
static void europe_quote_settle(EuropeCargoQuote* q) {
  if (q->high > q->low) {
    if (q->bid < q->low) {
      q->bid = q->low;
    }
    if (q->bid > q->high) {
      q->bid = q->high;
    }
  }
  if (q->bid < 0) {
    q->bid = 0;
  }
  q->ask = q->bid + q->burden;
}

void europe_apply_trade_volume(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int human_nation,
  int cargo_type,
  int amount,
  int is_buy,
  int immediate_threshold
) {
  if (!eu || amount <= 0 || cargo_type < 0 || cargo_type >= eu->cargo_count ||
      cargo_type >= EUROPE_CARGO_MAX) {
    return;
  }
  EuropeCargoQuote* q = &eu->cargo[cargo_type];
  int shift = q->volatility;
  if (shift < 0) {
    shift = 0;
  }
  if (shift > 15) {
    shift = 15;
  }
  int difficulty = eu->difficulty;
  if (col1) {
    difficulty = (int)col1->head.difficulty;
  }
  /* 1d44's `0x9e12 < 4` guard: a crown/indian slot (or an unknown seller)
   * never takes the human arm. */
  const int seller_is_human =
    (seller_nation >= 0 && seller_nation < 4 && seller_nation == human_nation);
  int term = (amount << shift) + europe_1d44_term(amount, seller_is_human, difficulty);
  /* Only the human's record is live here (col1_bridge binds eu->trade_nr to
   * head.human_player's nation.trade.nr); DOS walks all four records. The
   * Dutch record (slot 3) takes (term·2)/3 regardless of who sold — SELL
   * side only: FUN_38fd_1dfa's `if (local_a == 3) iVar5 = (iVar3*2)/3`
   * (viceroy 60263-60268) has no counterpart in the buy routine
   * FUN_38fd_1d80, which subtracts the undamped term from all four records
   * (viceroy 60216-60221). */
  if (!is_buy && human_nation == 3) {
    term = (term * 2) / 3;
  }
  int nr = (int)eu->trade_nr[cargo_type];
  if (is_buy) {
    nr -= term;
  } else {
    nr += term;
  }
  if (col1 && seller_nation >= 0 && seller_nation < (int)COLONIZE_COL1_NATION_COUNT &&
      (unsigned)cargo_type < COLONIZE_COL1_CARGO_TYPES) {
    ColonizeCol1NationTrade* t = &col1->nation[seller_nation].trade;
    const int32_t signed_amt = is_buy ? -(int32_t)amount : (int32_t)amount;
    t->tons[cargo_type] += signed_amt;
    t->tons2[cargo_type] += signed_amt;
    if (is_buy) {
      /* 1d80: gold[cargo] −= buy_price·amount. */
      t->gold[cargo_type] -= (int32_t)europe_buy_price(eu, cargo_type) * (int32_t)amount;
    } else {
      /* 1dfa: gold[cargo] += (sell_price·amount·(100−tax))/100 — note the
       * ledger rounds the other way from the treasury credit (54 lumber @1,
       * 35% → ledger +35, treasury +36). */
      const int tax = (int)col1->nation[seller_nation].tax_rate;
      const long gross = (long)europe_sell_price(eu, cargo_type) * (long)amount;
      t->gold[cargo_type] += (int32_t)((gross * (long)(100 - tax)) / 100L);
    }
  }
  if (!immediate_threshold) {
    if (nr < -32768) {
      nr = -32768;
    }
    if (nr > 32767) {
      nr = 32767;
    }
    eu->trade_nr[cargo_type] = (int16_t)nr;
    return;
  }
  /* 0058 single-cargo: temporary attrition then rise/fall thresholds. */
  const int bid_before = q->bid; /* bugs.md #225: player-move price popups */
  int attrition = q->attrition;
  nr += attrition;
  const int rise = q->rise;
  const int fall = q->fall;
  if (rise > 0 && nr <= -(rise * 100)) {
    nr += rise * 100; /* shed regardless; bid step gated (see tick) */
    if (q->bid < q->high) {
      q->bid += 1;
    }
  }
  if (fall > 0 && nr >= fall * 100) {
    nr -= fall * 100;
    if (q->bid > q->low) {
      q->bid -= 1;
    }
  }
  nr -= attrition;
  /* Only clamp when @CARGO low/high were loaded (high > low). */
  europe_quote_settle(q);
  /* bugs.md #225: a player transaction that moved the price gets the same
   * @PRICEUP/@PRICEDOWN dialog the EOT market tick shows — record the event;
   * game_loop drains it into a popup right after the sell/buy. */
  if (q->bid != bid_before && eu->price_event_count < EUROPE_CARGO_MAX) {
    eu->price_event_cargo[eu->price_event_count] = cargo_type;
    eu->price_event_dir[eu->price_event_count] = q->bid > bid_before ? 1 : -1;
    eu->price_event_count++;
  }
  if (nr < -32768) {
    nr = -32768;
  }
  if (nr > 32767) {
    nr = 32767;
  }
  eu->trade_nr[cargo_type] = (int16_t)nr;
}

void europe_apply_volume_price(EuropeScreen* eu, int cargo_type, int amount, int is_buy) {
  /* Harbor buy/sell with no col1 at hand: DOS keys both the 1d44 human test
   * and the 1dfa Dutch damping off the BOUND nation (DS:0x9e12), which every
   * port caller of europe_set_nation sets to the human — so pass it as both
   * seller and human instead of the old (−1, −1) pair, which read as "human
   * seller" by accident and could never trigger the Dutch arm (smell audit
   * #57/#58). Difficulty falls back to eu->difficulty (the 0x53a6 mirror).
   * Then FUN_38fd_0058(0, cargo). Callers holding a col1 should use
   * europe_apply_trade_volume directly. */
  const int bound = eu ? (int)eu->bound_nation : 0;
  europe_apply_trade_volume(eu, NULL, bound, bound, cargo_type, amount, is_buy, 1);
}

void europe_tick_market_prices_w(
  const ColonizeWorld* w,
  int human_nation,
  uint32_t turn
) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1;
  struct ColonizeColonyPool* colonies = w->colonies;

  /*
   * FUN_38fd_0058(0, 0xffff) — the human's 5e52 phase-3 call, with nation 0's
   * pass folded in. Validated 2026-08-28 against two real-DOS turn pairs
   * (golden_market_prices01; python replica iterated until both matched):
   *   phase 1 — ledger[g] = price_group[g] (signed) + Σ_n max(0, tons2[n][g])
   *             (nation +0xfc, NOT tons); in nation 0's pass only, and only
   *             while nation 0 is not withdrawn: price_group[g] -= ledger>>7.
   *             The colony-stock approximation that used to sit here is gone.
   *   phase 2 — cargos 9..12: sign(bid − 3·Σ/L) · (rise+fall)/2 · 100
   *   phase 3 — cargos 1..4: sign · (rise+fall)/2 (no ×100; fur year bias)
   *   phase 4 — nr += attrition (Dutch ×2 on odd post-increment turns),
   *             rise/fall ±1 bid within [low, high]; @PRICEUP/@PRICEDOWN.
   * Column roles (NAMES.TXT @CARGO): rise=c6, fall=c7, attrition=c8 — the
   * same fields europe_load_tables already fills. AI nations' own records
   * are not ticked here (their bids only feed ai_euro purchases).
   * Cite: viceroy_unpacked.c 58741–59005; turn/europe_nation_eot.md.
   */
  (void)colonies;
  if (!eu) {
    return;
  }
  eu->price_event_count = 0;
  if (col1) {
    eu->difficulty = col1->head.difficulty > 8 ? 8 : col1->head.difficulty;
    /* FUN_38fd_0718 raw 59120-59122: the 0x543f control byte of the bound
     * nation picks the Dragoon-roll bound. */
    eu->bound_human = eu->bound_nation < COLONIZE_COL1_NATION_COUNT &&
                      col1->player[eu->bound_nation].control == 0;
  }

  /* Phase 1 — pool decay + ledger. */
  long ledger[16];
  if (col1) {
    const bool nation0_active = col1->player[0].control != 2;
    for (int c = 0; c < 16; ++c) {
      long s = (long)(int16_t)col1->head.market_demand_pool[c];
      for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
        const int32_t t = col1->nation[n].trade.tons2[c];
        if (t > 0) {
          s += (long)t;
        }
      }
      if (nation0_active) {
        /* Nation 0 decays in its own pass; a later human pass sees the
         * decayed pool, nation 0 itself (as human) the pre-decay copy. */
        const long decayed = (long)(int16_t)col1->head.market_demand_pool[c] - (s >> 7);
        col1->head.market_demand_pool[c] = (uint16_t)(int16_t)decayed;
        if (human_nation != 0) {
          s -= (s >> 7);
        }
      }
      ledger[c] = s;
    }
  }

  /* Phases 2–3. */
  if (col1) {

    /* Phase 2 — Rum..Coats (9..12): pressure += sign * mid * 100. */
    {
      long sum = ledger[9] + ledger[10] + ledger[11] + ledger[12];
      if (sum <= 0) {
        sum = 1;
      }
      for (int c = 9; c <= 12; ++c) {
        if (c >= eu->cargo_count) {
          break;
        }
        long L = ledger[c];
        if (L <= 0) {
          L = 1;
        }
        const long ratio = (sum * 3) / L;
        const int bid = eu->cargo[c].bid;
        int sign = 0;
        if (bid > (int)ratio) {
          sign = 1;
        } else if (bid < (int)ratio) {
          sign = -1;
        }
        const int mid = (eu->cargo[c].rise + eu->cargo[c].fall) / 2;
        const int delta = sign * mid * 100;
        int nr = (int)eu->trade_nr[c] + delta;
        if (nr < -32768) {
          nr = -32768;
        }
        if (nr > 32767) {
          nr = 32767;
        }
        eu->trade_nr[c] = (int16_t)nr;
      }
    }

    /* Phase 3 — Sugar..Furs (1..4): pressure += mid * sign (no ×100). */
    {
      long half0 = ledger[0] / 2;
      long sum = half0 + ledger[1] + ledger[2] + ledger[3];
      if (sum <= 0) {
        sum = 1;
      }
      const int year = (int)col1->head.year;
      for (int c = 1; c <= 4; ++c) {
        if (c >= eu->cargo_count) {
          break;
        }
        long L = ledger[c];
        if (c == 4) {
          L /= 2;
        }
        if (L <= 0) {
          L = 1;
        }
        long ratio = (sum * 3) / L;
        if (c == 4) {
          if (year < 0x6a4) {
            ratio += 1; /* < 1700 */
          }
          if (year < 0x640) {
            ratio += 1; /* < 1600 */
          }
        }
        const int bid = eu->cargo[c].bid;
        int sign = 0;
        if (bid > (int)ratio) {
          sign = 1;
        } else if (bid < (int)ratio) {
          sign = -1;
        }
        const int mid = (eu->cargo[c].rise + eu->cargo[c].fall) / 2;
        const int delta = mid * sign;
        int nr = (int)eu->trade_nr[c] + delta;
        if (nr < -32768) {
          nr = -32768;
        }
        if (nr > 32767) {
          nr = 32767;
        }
        eu->trade_nr[c] = (int16_t)nr;
      }
    }
  }

  int last_rise = -1;
  int last_fall = -1;
  /* DOS: `0x9e12 == 3 && (turn & 1)` — the Netherlands' market recovers twice
   * as fast on odd turns (turn already incremented for this EOT). */
  const bool dutch_double = (human_nation == 3) && ((turn & 1u) != 0u);
  for (int c = 0; c < eu->cargo_count && c < EUROPE_CARGO_MAX; ++c) {
    EuropeCargoQuote* q = &eu->cargo[c];
    int nr = (int)eu->trade_nr[c] + (dutch_double ? q->attrition * 2 : q->attrition);
    const int rise = q->rise;
    const int fall = q->fall;
    /* DOS: the pressure word always sheds rise*100 / fall*100 at the
     * threshold; only the ±1 bid step is gated by [low, high]. Gating the
     * shed on the bid too (the old code) let a capped cargo's pressure run
     * away — golden_market_prices01 caught it on Rum at the 20 cap. */
    if (rise > 0 && nr <= -(rise * 100)) {
      nr += rise * 100;
      if (q->bid < q->high) {
        q->bid += 1;
        last_rise = c;
        if (eu->price_event_count < EUROPE_CARGO_MAX) {
          eu->price_event_cargo[eu->price_event_count] = c;
          eu->price_event_dir[eu->price_event_count] = 1;
          eu->price_event_count++;
        }
      }
    }
    if (fall > 0 && nr >= fall * 100) {
      nr -= fall * 100;
      if (q->bid > q->low) {
        q->bid -= 1;
        last_fall = c;
        if (eu->price_event_count < EUROPE_CARGO_MAX) {
          eu->price_event_cargo[eu->price_event_count] = c;
          eu->price_event_dir[eu->price_event_count] = -1;
          eu->price_event_count++;
        }
      }
    }
    europe_quote_settle(q);
    if (nr < -32768) {
      nr = -32768;
    }
    if (nr > 32767) {
      nr = 32767;
    }
    eu->trade_nr[c] = (int16_t)nr;
  }
  /*
   * Phase 4 dialog crumbs 0xfa8/0xfb0 -> price_event_cargo[]/dir[] (queued
   * in loop order above, one entry per cargo that actually crossed
   * threshold this tick — DOS calls FUN_281f_0652 inline per cargo, so two
   * different cargos changing the same turn both get their own OK dialog;
   * turn.c walks the full list). The status line below stays a single-line
   * summary (rise wins ties, matching the old behaviour) — cosmetic only.
   * Real DOS wording from GAME.TXT @PRICEUP/@PRICEDOWN (COLONIZE/GAME.TXT:1683-1689):
   *   "The price of {%STRING0} in %STRING1 has risen to {%NUMBER0$}."
   *   "The price of {%STRING0} in %STRING1 has fallen to {%NUMBER0$}."
   * STRING0 = cargo name (-0x6840 @CARGO table), STRING1 = nation home-port
   * city (-0x7c74 table == eu->port_city), NUMBER0 = new bid.
   */
  if (last_rise >= 0 && eu->messages) {
    PopupMsgTokens tok = {0};
    tok.string0 = eu->cargo[last_rise].name;
    tok.string1 = eu->port_city;
    tok.has_number0 = true;
    tok.number0 = eu->cargo[last_rise].bid;
    popup_msg_fill(eu->messages, "PRICEUP", &tok, "", eu->status, sizeof(eu->status));
    popup_msg_strip_markup(eu->status);
  } else if (last_fall >= 0 && eu->messages) {
    PopupMsgTokens tok = {0};
    tok.string0 = eu->cargo[last_fall].name;
    tok.string1 = eu->port_city;
    tok.has_number0 = true;
    tok.number0 = eu->cargo[last_fall].bid;
    popup_msg_fill(eu->messages, "PRICEDOWN", &tok, "", eu->status, sizeof(eu->status));
    popup_msg_strip_markup(eu->status);
  } else if (last_rise >= 0 || last_fall >= 0) {
    eu->status[0] = '\0';
  }
}

int europe_compute_immigration_score_w(
  const ColonizeWorld* w,
  int nation_id
) {
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeUnitPool* units = w->units;
  const ColonizeCol1Save* col1 = w->col1;

  /*
   * FUN_38fd_584a: score ≈ colony pop sum + unit count; <<1 if <4000; +8;
   * cap 4000; AI/non-human ((8-diff)*score)>>3; nation0 *2/3.
   * Cite: europe_nation_eot.md phase 4; ~68248.
   */
  if (nation_id < 0 || nation_id > 3) {
    return 0;
  }
  int pop = 0;
  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (c->active && c->nation_id == nation_id) {
        pop += c->colonist_count > 0 ? c->colonist_count : c->population;
      }
    }
  }
  int units_n = 0;
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (u->active && u->nation_id == nation_id) {
        units_n++;
      }
    }
  }
  /* DOS counts every unit record of the nation, including colonists waiting
   * on the Europe docks — those are the (236,236) mirror units in the pool
   * (seed-100 TURN5→6: needed 9→10 once the first immigrant landed). */
  int score = pop + units_n;
  if (score < 4000) {
    score <<= 1;
  }
  score += 8;
  if (score > 4000) {
    score = 4000;
  }
  if (col1) {
    const int control =
      (nation_id < 4) ? (int)col1->player[nation_id].control : 1;
    if (nation_id > 3 || control != 0) {
      int diff = (int)col1->head.difficulty;
      if (diff < 0) {
        diff = 0;
      }
      if (diff > 8) {
        diff = 8;
      }
      score = ((8 - diff) * score) >> 3;
    }
    if (nation_id == 0) {
      score = (score << 1) / 3;
    }
  }
  return score;
}


int europe_tick_immigration_pressure_w(
  const ColonizeWorld* w,
  int nation_id
) {
  EuropeScreen* eu = w->europe;
  const ColonizeColonyPool* colonies = w->colonies;
  const ColonizeUnitPool* units = w->units;
  const ColonizeCol1Save* col1 = w->col1;
  ColonizeDosRng* rng = w->rng;

  /*
   * DOS: +0x30 = 584a score (needed_crosses); +0x2e += 2 (and church crosses
   * already applied by caller); spawn when score < pressure. Cite: 5e52 ~68558.
   */
  if (!eu || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  if (col1) {
    int diff = (int)col1->head.difficulty;
    if (diff < 0) {
      diff = 0;
    }
    if (diff > 8) {
      diff = 8;
    }
    eu->difficulty = (uint8_t)diff;
    eu->bound_human = nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
                      col1->player[nation_id].control == 0;
  }
  const int score = europe_compute_immigration_score_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .colonies=(ColonizeColonyPool*)(colonies), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL)}, nation_id);
  int need = score;
  if (need < 0) {
    need = 0;
  }
  if (need > 65535) {
    need = 65535;
  }
  eu->needed_crosses = (uint16_t)need;
  eu->immigration_score = (int16_t)(need > 32767 ? 32767 : need);

  /*
   * 584a *param_2 (viceroy_unpacked.c:68258-68280): +2 a turn, but once the
   * nation's dock latch is up (nation_flags 0x40, set by the first crosses
   * immigrant = crosses_immigrant_seen) every colonist still waiting on the
   * Europe dock flips the tick negative: 2 → -2 → -4 …, i.e. -2 per waiting
   * immigrant. Empty the dock and the +2 resumes — the port used to freeze
   * the meter forever after the first immigrant.
   * Cite: test-saves-ai TURN1–4 = 2/4/6/8 (flag clear, no dock unit),
   * TURN5–7 = 0 (flag 0x40 + one dock colonist, drain clamped at 0).
   */
  int delta = 2;
  if (eu->crosses_immigrant_seen) {
    for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
      if (!eu->dock[i].present) {
        continue;
      }
      delta = (delta < 1) ? delta - 2 : -2;
    }
  }
  int cur = (int)eu->current_crosses + delta;
  if (cur < 0) {
    cur = 0; /* 5e52 clamps the sum at 0 before the compare. */
  }
  if (cur > 65535) {
    cur = 65535;
  }
  eu->current_crosses = (uint16_t)cur;
  eu->immigration_pressure = (int16_t)(cur > 32767 ? 32767 : (int)cur);
  europe_refresh_recruit_passage(eu);

  /* Phase 5: needed < current → dock immigrant; clear current. */
  if (need > 0 && (int)eu->current_crosses > need) {
    /*
     * 5e52 ~68577: FF 0x14 (Brewster) owned → FUN_38fd_4884(0,1) instead of
     * the random 04d4(0,2) pool pick: the player chooses (@RECRUITCHOOSE),
     * crosses are only zeroed once a pick lands (4884 tail, param_1==0), so
     * a cancelled dialog re-asks next turn. Caller enqueues the CHOICE and
     * europe_brewster_pick_from_pool applies it.
     */
    if ((col1 && founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_BREWSTER)) ||
        eu->brewster_no_criminals) {
      europe_apply_brewster(eu, 1); /* bugs.md #224: purge stale slots BEFORE the pick */
      return 2;
    }
    eu->current_crosses = 0;
    eu->immigration_pressure = 0;
    eu->crosses_immigrant_seen = true;
    europe_refresh_recruit_passage(eu);
    if (europe_immigrant_from_pool(eu, rng)) {
      /* bugs.md #223: keep open_on_dock — arrivals own it (see above). */
      snprintf(eu->status, sizeof(eu->status), "Immigrant arrives in Europe.");
      return 1;
    }
  }
  return 0;
}


/* ===== AI-nation immigration (DOS FUN_38fd_5e52, control != 0) ===== */

/*
 * DOS runs the whole immigration tick per nation from the nation EOT
 * FUN_3844_00f2 (viceroy_unpacked.c:58375, `FUN_291f_0a90(nation)` =
 * FUN_38fd_5e52), for every nation slot — 5e52 gates only its *chrome* on
 * `player[n].control == 0` (68563, 68590, 68603). An AI nation therefore
 * accrues crosses, crosses the same 584a threshold, empties one of its own
 * `recruit[3]` pool bytes into a real unit record parked in Europe
 * (FUN_38fd_0718, spawned at the Europe sentinel tile), refills that slot
 * with FUN_38fd_46d4 and zeroes its crosses — all silently. The port used to
 * stop at the +2 accrual, so no AI immigrant ever existed.
 */

/* View over an AI nation's own pool bytes (nation+2..+4 = recruit[3]). */
static void europe_pool_view_from_nation(
  EuropePoolView* v, const ColonizeCol1Save* col1, int nation_id
) {
  memset(v, 0, sizeof(*v));
  if (!col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  const ColonizeCol1Nation* nat = &col1->nation[nation_id];
  for (int i = 0; i < EUROPE_POOL_SIZE && i < 3; ++i) {
    v->job[i] = (int)nat->recruit[i];
    /* 0x1c = job NONE; col1_bridge reads that byte as "slot empty". */
    v->filled[i] = nat->recruit[i] != UNITS_JOB_NONE;
  }
  /* 46d4 64627-64633: DS:0x53a6 only when the bound nation is the human's;
   * every AI nation substitutes 1. */
  v->difficulty = (col1->player[nation_id].control == 0) ? (int)col1->head.difficulty : 1;
  v->brewster = founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_BREWSTER);
}

int europe_nation_refill_pool_slot(
  ColonizeCol1Save* col1, int nation_id, int slot, bool force_expert, ColonizeDosRng* rng
) {
  if (!col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return -1;
  }
  if (slot < 0 || slot >= 3) {
    return -1;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  EuropePoolView view;
  europe_pool_view_from_nation(&view, col1, nation_id);
  unsigned local = rng ? (rng->state | 1u)
                       : 1u + (unsigned)(nat->gold + nat->recruit_count + slot * 17);
  EuropePoolRng st;
  st.dos = rng;
  st.local = &local;
  const int job = europe_roll_pool_profession(&view, slot, force_expert, &st);
  nat->recruit[slot] = (uint8_t)job;
  return job;
}

/*
 * FUN_38fd_0718 (viceroy_unpacked.c:59098-59147) for an AI nation: the @UNIT
 * type comes from the @JOB byte (europe_dock_unit_dos_type is that same
 * mapping, Dragoon roll included), the record is parked at the port's Europe
 * sentinel tile (the (200,100) limbo `ai_euro_in_europe` tests for, where
 * 5d04's own 0718 spawn already puts purchased units), and the profession
 * byte is stored verbatim — 0718 writes +0x315b = param_1 with no 0x1c swap.
 * Pioneers get the 100 tools of 59134.
 */
int europe_nation_harbor_spawn(const ColonizeWorld* w, int nation_id, int profession) {
  if (!w || !w->units || !w->col1 || nation_id < 0 ||
      nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return -1;
  }
  const bool human = w->col1->player[nation_id].control == 0;
  const int dos_type = europe_dock_unit_dos_type(
    profession, (int)w->col1->head.difficulty, human, w->rng
  );
  const int lt = europe_dock_unit_type_index_ex(w->units, dos_type, true);
  if (lt < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(w->units, lt, 200, 100);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(w->units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation_id);
  u->moves = 0;
  u->profession = profession;
  if (dos_type == 2) {
    u->tools = 100; /* 59134: local_4 == 2 -> +0x3159 = 100 */
  } else if (dos_type == 1) {
    u->muskets = 50;
  } else if (dos_type == 4) {
    u->muskets = 50;
    u->horses = 50;
  }
  return id;
}

/*
 * FUN_38fd_584a's *param_2 (viceroy_unpacked.c:68256-68281) for a nation
 * record: +2 a turn, flipped negative once the nation's 0x40 latch is up by
 * every colonist-class unit of that nation still parked in Europe (DOS: unit
 * x == nation - 0x14, i.e. the Europe sentinel; port: the (200,100) limbo).
 */
static int europe_nation_crosses_delta(const ColonizeWorld* w, int nation_id) {
  int delta = 2;
  const ColonizeCol1Save* col1 = w->col1;
  if (!col1 || !w->units) {
    return delta;
  }
  if ((col1->nation[nation_id].nation_flags & 0x40u) == 0u) {
    return delta;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &w->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (u->x < 200 && u->y < 200) {
      continue;
    }
    if (!units_type_has_profession_slot(u->type_index)) {
      continue; /* FUN_281f_0b78(unit) >= 0 */
    }
    delta = (delta < 1) ? delta - 2 : -2;
  }
  return delta;
}

int europe_nation_immigration_tick_w(const ColonizeWorld* w, int nation_id) {
  if (!w || !w->col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  ColonizeCol1Save* col1 = w->col1;
  /* 68551: the whole routine is skipped once independence is declared. */
  if (col1->head.game_options.woi) {
    return 0;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  /* 68554: nation flags &= 0xdf on entry. */
  nat->nation_flags = (uint8_t)(nat->nation_flags & 0xdfu);

  /* 68557-68562: +0x30 = 584a score, +0x2e += the tick, clamped at 0. */
  int need = europe_compute_immigration_score_w(w, nation_id);
  if (need < 0) {
    need = 0;
  }
  if (need > 65535) {
    need = 65535;
  }
  nat->needed_crosses = (uint16_t)need;
  int cur = (int)nat->current_crosses + europe_nation_crosses_delta(w, nation_id);
  if (cur < 0) {
    cur = 0;
  }
  if (cur > 65535) {
    cur = 65535;
  }
  nat->current_crosses = (uint16_t)cur;
  if (need >= cur) {
    return 0; /* 68563: `if (local_6 < iVar3)` */
  }

  /*
   * 68570-68577: Brewster (FF 0x14) owned -> FUN_38fd_4884(0,1) — which for a
   * non-human bound nation takes local_58 = 1 (64744: the list pick is human
   * chrome; everyone else gets slot 1), costs nothing (param_2 != 0 zeroes the
   * passage at 64692), zeroes +0x2e (param_1 == 0, 64763), refills the emptied
   * slot with 46d4(0) — not the season-quad force-expert roll — and does NOT
   * bump recruit_count (+6, gated param_1 == 0 && param_2 == 0). No 0x40 latch
   * on this path either: 68601 is inside the non-Brewster branch only.
   */
  const bool brewster = founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_BREWSTER);
  int slot;
  bool force_expert;
  if (brewster) {
    slot = 1;
    force_expert = false;
  } else {
    /* 68575-68583: crosses zeroed, 04d4(0,2) picks the slot, the slot is
     * refilled with 46d4((turn & 3) == 0) before the unit is created. */
    nat->current_crosses = 0;
    slot = w->rng ? dos_rng_range(w->rng, 0, 2) : 0;
    force_expert = ((int)col1->head.turn & 3) == 0;
  }
  const int profession = (int)nat->recruit[slot];
  if (brewster) {
    /* 4884 tail order (64763-64775): zero +0x2e, create the unit, and only
     * then refill the emptied slot. 5e52's own branch refills first (68581
     * before 68585), so the two paths differ in draw order — DOS-literal. */
    nat->current_crosses = 0;
    const int id = europe_nation_harbor_spawn(w, nation_id, profession);
    if (id < 0) {
      return 0;
    }
    europe_nation_refill_pool_slot(col1, nation_id, slot, false, w->rng);
    return 1;
  }
  europe_nation_refill_pool_slot(col1, nation_id, slot, force_expert, w->rng);
  const int id = europe_nation_harbor_spawn(w, nation_id, profession);
  if (id < 0) {
    return 0;
  }
  nat->nation_flags = (uint8_t)(nat->nation_flags | 0x40u); /* 68601 */
  return 1;
}

/*
 * The nation `eu->gold` is the purse of: DS:0x9e12, set by FUN_38fd_0000
 * together with the record pointer DS:0x84fc (viceroy_unpacked.c 58695-58703).
 * -1 when there is no screen or the field is out of range.
 */
static int europe_purse_nation(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  const int n = (int)eu->bound_nation;
  return (n >= 0 && n < (int)COLONIZE_COL1_NATION_COUNT) ? n : -1;
}

/* Live screen for callers that hold only a save — see europe_set_live_screen. */
static EuropeScreen* g_europe_live_screen = NULL;

void europe_set_live_screen(EuropeScreen* eu) {
  g_europe_live_screen = eu;
}

uint32_t europe_nation_gold(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int nation
) {
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0u;
  }
  if (!eu) {
    eu = g_europe_live_screen;
  }
  if (eu && nation == europe_purse_nation(eu)) {
    return eu->gold > 0 ? (uint32_t)eu->gold : 0u;
  }
  return col1 ? col1->nation[nation].gold : 0u;
}

void europe_nation_gold_add(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, long delta
) {
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  if (!eu) {
    eu = g_europe_live_screen;
  }
  if (eu && nation == europe_purse_nation(eu)) {
    long v = (long)eu->gold + delta;
    if (v < 0) {
      v = 0;
    }
    if (v > (long)INT_MAX) {
      v = (long)INT_MAX;
    }
    eu->gold = (int)v;
    if (col1) {
      col1->nation[nation].gold = (uint32_t)eu->gold;
    }
    return;
  }
  if (!col1) {
    return;
  }
  long v = (long)col1->nation[nation].gold + delta;
  if (v < 0) {
    v = 0;
  }
  if (v > (long)UINT32_MAX) {
    v = (long)UINT32_MAX;
  }
  col1->nation[nation].gold = (uint32_t)v;
}

void europe_gold_stamp_record(const EuropeScreen* eu, struct ColonizeCol1Save* col1) {
  if (!eu) {
    eu = g_europe_live_screen;
  }
  const int n = europe_purse_nation(eu);
  if (!eu || !col1 || n < 0) {
    return;
  }
  col1->nation[n].gold = (uint32_t)(eu->gold < 0 ? 0 : eu->gold);
}

/*
 * Module-internal treasury move for the Europe channels: DOS credits/debits
 * the record the module is pointed at, so the delta always lands on
 * `eu->gold` — whoever is currently borrowing it (units.c / ai_euro.c park an
 * AI treasury there for one call and assign it back) — and the col1 record is
 * re-stamped only when the purse really is that nation's. That is the missing
 * half of smell audit G3 for the harbor/transport channels: they moved
 * `eu->gold` alone and left the human's record for a later push to guess at.
 *
 * NOT the same as europe_nation_gold_add, which is for callers OUTSIDE this
 * module: they never borrow the purse, so a delta for a non-bound nation must
 * go to that nation's record, whereas here it must go to the borrowed purse.
 */
static void europe_purse_move(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int nation, long delta
) {
  if (!eu) {
    return;
  }
  long v = (long)eu->gold + delta;
  if (v < 0) {
    v = 0;
  }
  if (v > (long)INT_MAX) {
    v = (long)INT_MAX;
  }
  eu->gold = (int)v;
  if (col1 && nation >= 0 && nation == europe_purse_nation(eu)) {
    col1->nation[nation].gold = (uint32_t)eu->gold;
  }
}

int europe_cargo_boycotted_ex(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int nation, int cargo_type
) {
  if (cargo_type < 0 || cargo_type >= EUROPE_CARGO_MAX) {
    return 0;
  }
  uint16_t word = 0;
  if (col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT) {
    /* nation+0x20, DOS FUN_38fd_05e8's operand — the authoritative store. */
    word = col1->nation[nation].boycott_bitmap;
    if (word == 0xFFFFu) {
      word = 0; /* removed all-cargo-embargo fingerprint; both bridge
                 * directions heal it (col1_bridge.c) — never trade-block on it */
    }
  } else if (eu) {
    word = eu->boycott_bitmap; /* render mirror: tests / chrome / no save bound */
  }
  return (word & (uint16_t)(1u << cargo_type)) != 0;
}

int europe_cargo_boycotted(const EuropeScreen* eu, int cargo_type) {
  return europe_cargo_boycotted_ex(eu, NULL, -1, cargo_type);
}

int europe_buyback_boycott_cost(
  const EuropeScreen* eu, const struct ColonizeCol1Save* col1, int human_nation, int cargo_type
) {
  if (!eu || human_nation < 0 || human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  if (!europe_cargo_boycotted_ex(eu, col1, human_nation, cargo_type)) {
    return 0;
  }
  const int price = eu->cargo[cargo_type].ask;
  if (price <= 0) {
    return 0;
  }
  return price * 500;
}

int europe_buyback_boycott(
  EuropeScreen* eu, struct ColonizeCol1Save* col1, int human_nation, int cargo_type
) {
  /*
   * FUN_38fd_2dfe (viceroy_unpacked.c:60904-60945), clean disassembly, no
   * warnings. Real DOS trigger per GAME.TXT @SOMEBOYCOTT: "Some of the
   * cargo could not be unloaded because of a parliamentary boycott. If you
   * want to ask that the boycott be lifted, click on the cargo type in
   * question." — i.e. clicking a boycotted cell on the Europe market strip
   * (wired at that exact click site: game_loop.c EUROPE_HIT_MARKET).
   *
   * Traced formula (iVar3 = thunk_FUN_291f_0c3e -> FUN_38fd_0016, the same
   * "effective ask price" this file already exposes as
   * eu->cargo[cargo_type].ask):
   *   cost = ask_price * 500         // fandom "500 tons of that good"
   *   if nation.gold < cost: GAME.TXT @KISSSORRY, no state change
   *   else: nation.gold -= cost
   *         nation.royal_money += cost   // DOS write at nation+0x22, the
   *                                      // exact byte offset col1_save.h
   *                                      // already documents as royal_money
   *                                      // (REF budget, FUN_43f7_1d42) --
   *                                      // paying back taxes funds the Crown
   *         nation.boycott_bitmap &= ~(1 << cargo_type)   // nation+0x20
   * DOS gates this to the human-controlled nation (player_control[nation]
   * check at 0x543f); callers here only ever pass the human nation for the
   * same reason (only the human clicks their own market strip).
   *
   * The dialog chrome around this is DOS's own two-step and lives at the
   * click site (game_loop.c EUROPE_HIT_MARKET): @KISSUP is a 2-row CHOICE
   * whose SECOND row is "Pay {%NUMBER0$}." (38fd:2e5e compares the return of
   * FUN_281f_0652(0x1033, 2) against 2), and only after that answer does DOS
   * test the purse and show @KISSSORRY. This function is the applier — it
   * re-checks the purse itself, so a caller that skips the dialog (tests,
   * scripted play) still cannot overdraw.
   * Returns gold paid (>0) on success, 0 on no-op/insufficient funds.
   */
  if (!eu || !col1) {
    return 0;
  }
  if (human_nation < 0 || human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  if (!europe_cargo_boycotted_ex(eu, col1, human_nation, cargo_type)) {
    return 0;
  }
  const int price = eu->cargo[cargo_type].ask;
  if (price <= 0) {
    return 0;
  }
  const int cost = price * 500;
  if (eu->gold < cost) {
    /* GAME.TXT @KISSSORRY: "Unfortunately, we only have {%NUMBER0$}
     * available." — same sentence game_dialogs.c's KISSSORRY popup shows;
     * composed here via eu->messages (europe_set_messages) since this
     * applier re-checks the purse on its own. */
    eu->status[0] = '\0';
    if (eu->messages) {
      PopupMsgTokens tok = {0};
      tok.number0 = eu->gold;
      tok.has_number0 = true;
      popup_msg_fill(eu->messages, "KISSSORRY", &tok, "", eu->status, sizeof(eu->status));
      popup_msg_strip_markup(eu->status);
    }
    return 0;
  }
  europe_purse_move(eu, col1, human_nation, -cost);
  ColonizeCol1Nation* nation = &col1->nation[human_nation];
  nation->royal_money += cost;
  nation->boycott_bitmap &= (uint16_t)~(1u << cargo_type);
  eu->boycott_bitmap = nation->boycott_bitmap;
  const char* cname = eu->cargo[cargo_type].name[0] ? eu->cargo[cargo_type].name : "That cargo";
  snprintf(
    eu->status, sizeof(eu->status), "Paid %d$ in back taxes -- boycott on %s lifted.", cost, cname
  );
  diag_info(
    "EUROPE paid %d$ back taxes on %s: boycott lifted (gold=%d)", cost, cname, eu->gold
  );
  return cost;
}

int europe_sell_proceeds(const EuropeScreen* eu, int cargo_type, int amount) {
  if (!eu || amount <= 0 || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return 0;
  }
  /* FUN_38fd_1f0c: gross = (euro_price − 1)·amount; tax taken by the caller
   * as gross − gross·tax/100. */
  const int price = europe_sell_price(eu, cargo_type);
  if (price <= 0) {
    return 0;
  }
  return europe_net_after_tax(price * amount, eu->tax_percent);
}

/*
 * Crown cut of a Europe sale.
 *
 * DOS keeps the withheld tax: every sale arm writes `nation+0x22 += tax` —
 * the same 32-bit purse col1_save.h documents as royal_money (the REF budget
 * FUN_43f7_1d42 spends). Verified arms in this file: FUN_364b_0688 Custom
 * House (europe_custom_house_autosell_ex) and FUN_38fd_2dfe boycott buy-back
 * (europe_buyback_boycott). The harbor/transport sell paths credited the
 * player's gold but dropped the Crown's share, so all Europe tax revenue
 * vanished and the REF was systematically underfunded (smell audit #51).
 *
 * `gross` is the pre-tax sale value, `net` what europe_sell_proceeds paid out;
 * the difference is exactly what was withheld (same rounding, no second
 * division). No-op without a save record or on an untaxed (net == gross) sale.
 */
static void europe_credit_sale_tax(
  struct ColonizeCol1Save* col1, int nation, int gross, int net
) {
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  const int tax_paid = gross - net;
  if (tax_paid <= 0) {
    return;
  }
  col1->nation[nation].royal_money += tax_paid;
}

void europe_set_labels(EuropeScreen* eu, const struct ColonizeMsgCatalog* labels) {
  if (eu) {
    eu->labels = labels;
  }
}

void europe_set_messages(EuropeScreen* eu, const struct ColonizeMsgCatalog* game_txt) {
  if (eu) {
    eu->messages = game_txt;
  }
}

/* LABELS.TXT line, or the built-in fallback when no catalog is bound. */
static const char* europe_label(
  const EuropeScreen* eu,
  const char* section,
  int idx,
  const char* fallback
) {
  if (eu && eu->labels && section) {
    const ColonizeMsgSection* sec = assets_msg_find(eu->labels, section);
    if (sec && idx >= 0 && idx < sec->line_count && sec->lines[idx][0]) {
      return sec->lines[idx];
    }
  }
  return fallback;
}

static void europe_push_sale_status(EuropeScreen* eu, int cargo_type, int amount, int net) {
  if (!eu || amount <= 0 || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return;
  }
  if (eu->bar_event_count >= EUROPE_BAR_EVENT_MAX) {
    return;
  }
  /*
   * FUN_38fd_23c4 tail, field for field: amount, cargo name, @CMESSAGE 1
   * "sold for", gross, DS:0xfef ".", tax rate, @CMESSAGE 0x11 "% Tax:", tax
   * paid, @CMESSAGE 0x12 ". Net:", net. The 0088 calls between fields only
   * trim the space each append leaves, so the punctuation closes up. DOS
   * writes the tax half unconditionally here (Europe trade is shut once
   * independence is declared, so the rate is never 0 in practice).
   */
  const int gross = europe_sell_price(eu, cargo_type) * amount;
  const int tax_paid = gross - net;
  const char* cname =
    eu->cargo[cargo_type].name[0] ? eu->cargo[cargo_type].name : "cargo";
  snprintf(
    eu->bar_event[eu->bar_event_count],
    EUROPE_BAR_EVENT_LEN,
    "%d %s %s %d. %d%s %d%s %d",
    amount,
    cname,
    europe_label(eu, "CMESSAGE", 1, ""),
    gross,
    eu->tax_percent,
    europe_label(eu, "CMESSAGE", 0x11, ""),
    tax_paid < 0 ? 0 : tax_paid,
    europe_label(eu, "CMESSAGE", 0x12, ""),
    net
  );
  eu->bar_event_count++;
}

/*
 * ---------------------------------------------------------------------
 * The one Europe sell pipeline (audit SC-16 / SC-17).
 *
 * europe_sell_hold, europe_sell_hold_partial and europe_sell_unit_hold each
 * wrote this sequence out in full, with three separately-maintained copies
 * of the same smell-audit comment block. The order below is DOS's and is
 * load-bearing:
 *
 *   1. Boycott gate on the SELLER's own nation+0x20 word, not the
 *      Europe-screen render mirror (smell audit G6): an EOT trade-route
 *      unload and the map/transport dump-sell both run with no screen in
 *      sight, and the AI borrow path swaps eu->gold for the AI nation.
 *   2. europe_sell_proceeds, then europe_purse_move — which credits the
 *      seller's own record when the purse is theirs (smell audit G3).
 *   3. europe_credit_sale_tax: DOS `nation+0x22 += tax` on every sale
 *      (smell audit #51), seller = the hold's owner.
 *   4. Clear the hold. Partial sales subtract and only reset the type word
 *      once the hold empties — the empty-hold sentinel is amount 0 / type 0
 *      everywhere else in the port, and 255 here used to be the odd one out
 *      (smell audit #64). A full sale is the amt == held case of the same
 *      rule, so all three callers share it.
 *   5. europe_push_sale_status BEFORE the volume move, so the printed gross
 *      is the bid the sale actually went through at (bugs.md #376).
 *   6. europe_apply_trade_volume: the 1dfa ledger (tons/tons2/gold) so the
 *      human's own trading feeds the long-run price pool (smell audit #50).
 *
 * `hold_type` / `hold_amount` point at the caller's own slot — an
 * EuropeHarborShip mirror hold or a ColonizeUnit hold, both int[]. Returns
 * the proceeds, or 0 when the boycott gate refused (nothing is mutated).
 * The caller keeps its own diag_info line: the three differ in what they
 * can name (ship name / unit id) and whether a partial shows "amt/held".
 * ---------------------------------------------------------------------
 */
static int europe_sell_commit(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int* hold_type,
  int* hold_amount,
  int amt
) {
  const int ctype = *hold_type;
  const int held = *hold_amount;
  if (europe_cargo_boycotted_ex(eu, col1, seller_nation, ctype)) {
    const char* bname =
      (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "That cargo";
    snprintf(
      eu->status, sizeof(eu->status), "%s is boycotted — cannot trade in Europe.", bname
    );
    return 0;
  }
  const int gained = europe_sell_proceeds(eu, ctype, amt);
  europe_purse_move(eu, col1, seller_nation, gained);
  europe_credit_sale_tax(col1, seller_nation, europe_sell_price(eu, ctype) * amt, gained);
  *hold_amount = held - amt;
  if (*hold_amount == 0) {
    *hold_type = 0;
  }
  europe_push_sale_status(eu, ctype, amt, gained);
  europe_apply_trade_volume(
    eu, col1, seller_nation, col1 ? (int)col1->head.human_player : seller_nation,
    ctype, amt, 0, 1
  );
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "cargo";
  snprintf(eu->status, sizeof(eu->status), "Sold %d %s for %d$.", amt, cname, gained);
  return gained;
}

int europe_sell_hold(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int harbor_index,
  int hold_index
) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= EUROPE_SHIP_CARGO_MAX) {
    return 0;
  }
  EuropeHarborShip* ship = &eu->harbor[harbor_index];
  const int amt = ship->hold_goods_amount[hold_index];
  const int ctype = ship->hold_goods_type[hold_index];
  if (amt <= 0 || amt >= 255) {
    return 0;
  }
  const int gained = europe_sell_commit(
    eu, col1, seller_nation, &ship->hold_goods_type[hold_index],
    &ship->hold_goods_amount[hold_index], amt
  );
  if (gained <= 0) {
    return 0;
  }
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "cargo";
  diag_info(
    "EUROPE sold %d %s from %s: bid=%d tax=%d%% proceeds=%d gold=%d",
    amt, cname, ship->name[0] ? ship->name : "ship",
    europe_sell_price(eu, ctype), eu->tax_percent, gained, eu->gold
  );
  return gained;
}

int europe_sell_hold_partial(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int harbor_index,
  int hold_index,
  int amount
) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= EUROPE_SHIP_CARGO_MAX) {
    return 0;
  }
  EuropeHarborShip* ship = &eu->harbor[harbor_index];
  const int held = ship->hold_goods_amount[hold_index];
  const int ctype = ship->hold_goods_type[hold_index];
  if (held <= 0 || held >= 255 || amount <= 0) {
    return 0;
  }
  const int amt = amount < held ? amount : held;
  const int gained = europe_sell_commit(
    eu, col1, seller_nation, &ship->hold_goods_type[hold_index],
    &ship->hold_goods_amount[hold_index], amt
  );
  if (gained <= 0) {
    return 0;
  }
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "cargo";
  diag_info(
    "EUROPE sold %d/%d %s from %s: bid=%d tax=%d%% proceeds=%d gold=%d",
    amt, held, cname, ship->name[0] ? ship->name : "ship",
    europe_sell_price(eu, ctype), eu->tax_percent, gained, eu->gold
  );
  return gained;
}

/*
 * FUN_364b_0636: Custom House may auto-sell this cargo type.
 * Deny Food(0), Horses(8), Tools(0xe), Muskets(0xf).
 * Ore(6) extra DOS deny path not mapped — allow Ore (no invent).
 *
 * **2026-08-27: a same-day attempt to also deny Lumber(5) here (matching a
 * `param_1 != 5` term this function's raw decompile appears to have) was
 * reverted — both `golden_colony_prod01`/`02` (real DOS `.SAV` ground
 * truth) show Lumber genuinely auto-sold down to 50, contradicting that
 * reading. The `param_1==5` deny term is real in the decompile but must
 * gate something other than plain cargo-index-5 in this calling context
 * (`local_b6` in `FUN_364b_0688`'s loop may not be a raw cargo index the
 * way the other four terms' values are), or a second, different type-gate
 * function is the real one `0688` calls — not re-investigated this pass.
 * Golden evidence overrides the static read; leaving the deny list at its
 * original 4 entries, unchanged from before this attempt.
 */
static int europe_custom_house_cargo_eligible(int cargo_type) {
  /*
   * Resolved 2026-08-28: FUN_364b_0688 picks the gate by controller —
   * human colonies use FUN_281f_0cfe → FUN_15eb_0302 (colony +0x8a bit per
   * cargo == custom_house_bits), and ONLY AI colonies use FUN_364b_0636
   * (thunk_FUN_291f_09c0, confirmed via address_mapping.csv). So the
   * Lumber deny term is real, but it never applied to the human's Custom
   * House — which is why the real-DOS saves sold lumber. This function is
   * now the AI-only 0636 gate: deny Food/Lumber/Horses/Tools/Muskets; Ore
   * has an extra deny arm (building 3 present or DS:0x8de4/0x8de6 set)
   * that is not modelled here — Ore stays allowed.
   */
  if (cargo_type == COLONIZE_CARGO_FOOD || cargo_type == COLONIZE_CARGO_LUMBER ||
      cargo_type == COLONIZE_CARGO_HORSES || cargo_type == COLONIZE_CARGO_TOOLS ||
      cargo_type == COLONIZE_CARGO_MUSKETS) {
    return 0;
  }
  return cargo_type >= 0 && cargo_type < COLONIZE_CARGO_COUNT;
}

/* FUN_364b_0636 denylist — AI Custom House + AI peace Europe export sail. */
int europe_cargo_export_eligible(int cargo_type) {
  return europe_custom_house_cargo_eligible(cargo_type);
}

static int europe_custom_house_bit_enabled(uint16_t bits, int cargo_type) {
  /*
   * bits==0 → nothing configured yet, sell nothing (per-cargo UI PARKED —
   * see fandom_col1994.md Custom House: sells a "configured" cargo type,
   * not everything by default). Player-confirmed 2026-08-16 (real DOS
   * COLONY00/01_no-transports.SAV pair, colony-prod-tests): a real save
   * with Custom House built but custom_house_bits==0 (no cargo toggled)
   * sold *nothing* that turn — the old "bits==0 → all eligible" stand-in
   * auto-sold every stock>99 cargo down to 50 and was never DOS-verified.
   */
  if (bits == 0) {
    return 0;
  }
  return (bits >> cargo_type) & 1u;
}

bool europe_custom_house_cargo_enabled(uint16_t custom_house_bits, int cargo_type) {
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return false;
  }
  return europe_custom_house_bit_enabled(custom_house_bits, cargo_type) != 0;
}

int europe_custom_house_autosell_ex_w(
  const ColonizeWorld* w,
  ColonizeColony* colony,
  int human_nation,
  EuropeCustomHouseSale* out,
  int out_max,
  int* out_count
) {
  EuropeScreen* eu = w->europe;
  ColonizeColonyPool* pool = w->colonies;
  ColonizeCol1Save* col1 = w->col1;

  if (out_count) {
    *out_count = 0;
  }
  /*
   * FUN_364b_0688 after production: Custom House + type gate + stock>99 →
   * sell stock-50. Cite: viceroy_unpacked.c FUN_364b_0688 / FUN_364b_0636;
   * docs/fandom_col1994.md Custom House (boycott bypass; WoI untaxed).
   */
  if (!eu || !pool || !colony || !colony->active) {
    return 0;
  }
  const int ch_id = colonies_building_row(pool, COLONY_BUILDING_CUSTOM_HOUSE);
  if (ch_id < 0 || ch_id >= COLONIZE_BUILDING_TYPES_MAX || !colony->has_building[ch_id]) {
    return 0;
  }
  const int nation = colony->nation_id;
  const int is_human = (nation == human_nation);
  /* FUN_364b_0688: a human colony's Custom House is shut while an enemy
   * armed ship / Man-O-War sits next to it (colony +0x1b & 3). */
  if (is_human && (colony->ai_flags & 0x03u) != 0u) {
    return 0;
  }
  const int woi = col1 && col1->head.game_options.woi != 0;
  int tax = 0;
  if (!woi) {
    if (is_human) {
      tax = eu->tax_percent;
    } else if (col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT) {
      tax = (int)col1->nation[nation].tax_rate;
    } else {
      tax = eu->tax_percent;
    }
  }
  if (tax < 0) {
    tax = 0;
  }
  if (tax > 100) {
    tax = 100;
  }
  ColonizeCol1Nation* nat =
    (col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT) ? &col1->nation[nation]
                                                                       : NULL;

  int total = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    /* Human: per-cargo toggle bits (FUN_15eb_0302). AI: FUN_364b_0636. */
    if (is_human) {
      if (!europe_custom_house_bit_enabled(colony->custom_house_bits, c)) {
        continue;
      }
    } else if (!europe_custom_house_cargo_eligible(c)) {
      continue;
    }
    /* FUN_364b_0688: if (99 < stock) sell stock - 50 (leave 50 in warehouse). */
    if (colony->stock[c] <= 99) {
      continue;
    }
    const int amount = colony->stock[c] - 50;
    if (amount <= 0) {
      continue;
    }
    if (c >= eu->cargo_count) {
      continue;
    }
    /*
     * Sells at euro_price − 1 (FUN_291f_09ea → FUN_38fd_0040, which reads
     * `*(char *)(*(int *)0x84fc + cargo + 0x4c) - 1` clamped at 0). 0x84fc is
     * the *bound* nation record — the same one the tax byte above comes from —
     * so an AI colony's Custom House prices off that AI's own market, not the
     * human's. The port's live market (eu->cargo[].bid) is bound to the human
     * nation only (col1_bridge stamps col1 → bid on load and bid → col1 on
     * capture), so it is the human's live record; every other nation's market
     * lives in col1 trade.euro_price[]. Same substitution as the dump-sell
     * below: a game that never came from a DOS save has the AI nations' byte
     * still 0, so fall back to the one Linux market. A zero price still moves
     * the goods (DOS has no price gate here).
     */
    int price = europe_sell_price(eu, c);
    if (!is_human && nat && c < (int)COLONIZE_COL1_CARGO_TYPES &&
        nat->trade.euro_price[c] != 0) {
      const int p = (int)nat->trade.euro_price[c] - 1; /* FUN_38fd_0040 */
      price = p < 0 ? 0 : p;
    }
    const int gross = price * amount;
    const int gained = europe_net_after_tax(gross, tax);
    const int tax_paid = gross - gained;
    colony->stock[c] = 50;
    total += gained;
    if (out && out_count && *out_count < out_max) {
      EuropeCustomHouseSale* rec = &out[*out_count];
      rec->cargo = c;
      rec->amount = amount;
      rec->gross = gross;
      rec->tax_percent = tax;
      rec->tax_paid = tax_paid;
      rec->net = gained;
      (*out_count)++;
    }
    if (nat) {
      /*
       * One treasury (+0x2a), both stores: this was already the only europe.c
       * writer that bumped nat->gold AND eu->gold, which is why it looked
       * right while the harbor paths did not (smell audit G3). It now goes
       * through the shared accessor — for the human that credits the live
       * purse and re-stamps the record from it, for an AI colony's Custom
       * House only that AI's record, and the `is_human` special case below
       * is gone with it.
       */
      europe_nation_gold_add(eu, col1, nation, gained);
      /* nation +0x22 (royal_money) += tax paid; +0x26 write-only cumulative
       * net trade income (unknown24_pad, int32 LE). */
      nat->royal_money += tax_paid;
      uint32_t cum = (uint32_t)nat->unknown24_pad[0] | ((uint32_t)nat->unknown24_pad[1] << 8) |
                     ((uint32_t)nat->unknown24_pad[2] << 16) |
                     ((uint32_t)nat->unknown24_pad[3] << 24);
      cum += (uint32_t)gained;
      nat->unknown24_pad[0] = (uint8_t)(cum & 0xffu);
      nat->unknown24_pad[1] = (uint8_t)((cum >> 8) & 0xffu);
      nat->unknown24_pad[2] = (uint8_t)((cum >> 16) & 0xffu);
      nat->unknown24_pad[3] = (uint8_t)((cum >> 24) & 0xffu);
    }
    diag_info(
      "EUROPE customs %s sold %d %s: bid=%d tax=%d%% paid=%d proceeds=%d (nation=%d%s)",
      colony->name[0] ? colony->name : "colony",
      amount,
      (c < eu->cargo_count && eu->cargo[c].name[0]) ? eu->cargo[c].name : "cargo",
      price,
      tax,
      tax_paid,
      gained,
      nation,
      is_human ? " human" : ""
    );
    /* FUN_291f_0a2e → FUN_38fd_1dfa; no FUN_38fd_0058 step here. */
    europe_apply_trade_volume(eu, col1, nation, human_nation, c, amount, 0, 0);
  }
  if (total > 0) {
    diag_info(
      "EUROPE customs %s total %d$ (nation=%d, gold=%d)",
      colony->name[0] ? colony->name : "colony", total, nation, eu->gold
    );
  }
  if (total > 0 && nation == human_nation) {
    if (colony->name[0]) {
      snprintf(
        eu->status,
        sizeof(eu->status),
        "Custom House in %s sold for %d$.",
        colony->name,
        total
      );
    } else {
      snprintf(eu->status, sizeof(eu->status), "Custom House sold goods for %d$.", total);
    }
  }
  return total;
}


int europe_custom_house_autosell_w(
  const ColonizeWorld* w,
  ColonizeColony* colony,
  int human_nation
) {
  EuropeScreen* eu = w->europe;
  ColonizeColonyPool* pool = w->colonies;
  ColonizeCol1Save* col1 = w->col1;

  return europe_custom_house_autosell_ex_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(pool), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL), .europe=(EuropeScreen*)(eu)}, colony, human_nation, NULL, 0, NULL);
}


int europe_ai_colony_dump_sell_w(
  const ColonizeWorld* w,
  ColonizeColony* colony,
  int human_nation
) {
  EuropeScreen* eu = w->europe;
  ColonizeColonyPool* pool = w->colonies;
  ColonizeCol1Save* col1 = w->col1;

  /*
   * FUN_364b_0688 phase O: non-human Euro (nation≤3, control≠0) sells warehouse
   * surplus for gold before spoilage. Stock is not reduced here — spoilage clamps.
   * Cite: viceroy_unpacked.c ~57806–57848; 291f_0a2e → 38fd_1dfa.
   *
   * UNTAXED. DOS (viceroy 57834-57846):
   *   local_66 = (uint)*(byte *)(cargo + nation*0x10 + -0x7b44) * amount;
   *   *(nation_rec + cargo*4 + 0x7c) += local_66;   // per-cargo ledger
   *   *(nation_rec + 0x2a)          += local_66;    // 32-bit treasury
   * No tax split: there is no FUN_1d1d_0ec6 tax call and no `+0x22`
   * royal_money write anywhere in this arm, unlike the Custom House arm 500
   * lines earlier (57277-57302) which does both.
   *
   * The price byte, though, IS the harbor sell price. `-0x7b44` and DS:0x84BC
   * are the same address (signed vs unsigned 16-bit), and that 4x16 table is
   * not the nation records' `euro_price[]` — it is a derived per-nation SELL
   * table, and every writer of it stores `euro_price − 1` clamped at 0:
   *   viceroy_unpacked.c:6316-6320 (the nation-bind rebuild, all four nations)
   *     iVar2 = *(char *)(*(int *)0x84fc + cargo + 0x4c) − 1;
   *     if (iVar2 < 0) iVar2 = 0;
   *     *(nation*0x10 + cargo + -0x7b44) = (char)iVar2;
   *   and identically at :51962-51966 and FUN_38fd_0058's tail :58996-59000,
   *   plus the two overlay copies (viceroy_overlays.c:30272, :36095).
   * `+0x4c` is exactly `offsetof(ColonizeCol1Nation, trade.euro_price)` (0x4c,
   * with sizeof == 0x13c), i.e. the record array this port keeps in
   * `nat->trade.euro_price[]` — the value DOS reads BEFORE the −1. So the
   * dump-sell price is `euro_price − 1` == europe_sell_price(), the project's
   * standing Europe price convention, and NOT the raw record byte.
   */
  if (!eu || !colony || !colony->active) {
    return 0;
  }
  const int nation = colony->nation_id;
  if (nation < 0 || nation > 3 || nation == human_nation) {
    return 0;
  }
  ColonizeCol1Nation* nat =
    (col1 && nation < (int)COLONIZE_COL1_NATION_COUNT) ? &col1->nation[nation] : NULL;

  int total = 0;
  for (int c = 1; c < COLONIZE_CARGO_COUNT; ++c) {
    const int cap = colonies_warehouse_capacity(pool, colony, c);
    if (cap <= 0) {
      continue;
    }
    const int surplus = colony->stock[c] - cap;
    if (surplus <= 0) {
      continue;
    }
    /* Horses: DOS adds surplus to Europe horses word; sell amount → 0. */
    if (c == COLONIZE_CARGO_HORSES) {
      unsigned h = (unsigned)eu->nation_horses[nation] + (unsigned)surplus;
      if (h > 65535u) {
        h = 65535u;
      }
      eu->nation_horses[nation] = (uint16_t)h;
      continue;
    }
    /*
     * Muskets: DOS while surplus>49: Europe musket counter++, amount−50; then
     * sell remainder for gold. DOS runs this batching before any price read,
     * so a zero price must not skip it.
     */
    int amount = surplus;
    if (c == COLONIZE_CARGO_MUSKETS) {
      while (amount > 49) {
        if (eu->nation_musket_batches[nation] < 65535u) {
          eu->nation_musket_batches[nation]++;
        }
        amount -= 50;
      }
      if (amount <= 0) {
        continue;
      }
    }
    if (c >= eu->cargo_count) {
      continue;
    }
    /* 291f_0a2e → 38fd_1dfa: ledgers + volume, no 0058 step. DOS runs this
     * BEFORE it reads the price byte (asm 364b:1795 vs 17d0). */
    europe_apply_trade_volume(eu, col1, nation, human_nation, c, amount, 0, 0);
    /*
     * DS:0x84BC byte for THIS colony's nation (asm 364b:17d0
     * `MOV AL,[BX+SI+0x84bc]`, SI = nation<<4, BX = cargo) — which is that
     * nation's record price minus one, see the derivation in the block above.
     * Substitution: a game that never came from a DOS save has the AI
     * nations' record byte still 0 (only the bound nation's is stamped, see
     * col1_bridge), so fall back to the one Linux market's sell price — the
     * same live-market-else-col1 pairing ai_euro_5d04_cb_sell_price uses.
     */
    int price;
    const int record_price =
      (nat && c < (int)COLONIZE_COL1_CARGO_TYPES) ? (int)nat->trade.euro_price[c] : 0;
    if (record_price > 0) {
      price = record_price - 1; /* the table's own `− 1` clamp; > 0 here, so no floor needed */
    } else {
      price = europe_sell_price(eu, c);
    }
    /* No price gate: DOS calls 291f_0a2e (volume) at 57827, before it reads
     * the price byte at 57834 — a zero price still moves the goods. */
    const int gained = price > 0 ? price * amount : 0; /* untaxed: DOS credits gross */
    total += gained;
    if (nat) {
      /*
       * Same double-book as the 20e6 delivery tail (ai_euro_20e6_delivery_
       * sell_tail): the 1dfa call just above already moved the taxed
       * proceeds into trade.gold[]/tons[]/tons2[]; DOS then adds the gross to
       * trade.gold[] and — verbatim, asm 364b:17bd `ADD [BX+0xbc],AX` with
       * AX = the loop's CARGO INDEX, not the amount — the index to
       * trade.tons[]. Both decompiles and the listing agree.
       */
      nat->trade.tons[c] += (int32_t)c;
      nat->trade.gold[c] += (int32_t)gained;
      nat->gold += (uint32_t)gained;
    }
  }
  if (total > 0) {
    diag_info(
      "EUROPE dump-sell %s total %d$ (nation=%d, untaxed)",
      colony->name[0] ? colony->name : "colony", total, nation
    );
    snprintf(eu->status, sizeof(eu->status), "AI warehouse dump-sold for %d$.", total);
  }
  return total;
}

int europe_sell_unit_hold_w(
  const ColonizeWorld* w,
  int unit_id,
  int hold_index
) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1;
  ColonizeUnitPool* units = w->units;

  /*
   * Map/transport dump-sell (no harbor chrome). Tax path = europe_sell_proceeds:
   * bid * amount * (100 - eu->tax_percent) / 100 — same Crown cut as
   * europe_sell_hold. Cite: Colonization.pdf Europe buy/sell + tax;
   * docs/manual_gap.md Europe commodity trade.
   */
  if (!eu || !units) {
    return 0;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u || !u->active || !units_is_transport(units, unit_id)) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  const int amt = u->hold_goods_amount[hold_index];
  const int ctype = u->hold_goods_type[hold_index];
  if (amt <= 0 || amt >= 255) {
    return 0;
  }
  /* Seller = the hold's owner, so an AI-owned hold credits the AI purse. */
  const int gained = europe_sell_commit(
    eu, col1, (int)u->nation_id, &u->hold_goods_type[hold_index],
    &u->hold_goods_amount[hold_index], amt
  );
  if (gained <= 0) {
    return 0;
  }
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "cargo";
  diag_info(
    "EUROPE sold %d %s from unit %d: bid=%d tax=%d%% proceeds=%d gold=%d",
    amt, cname, unit_id, europe_sell_price(eu, ctype), eu->tax_percent, gained, eu->gold
  );
  return gained;
}

int europe_buy_unit_cargo_w(
  const ColonizeWorld* w,
  int unit_id,
  int cargo_type,
  int amount
) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1;
  ColonizeUnitPool* units = w->units;

  /*
   * Map/transport buy (no harbor chrome) — trade-route load list at a Europe
   * stop (DOS FUN_479b_0bd0 → FUN_38fd_1fa2 via FUN_291f_0b42). Flat ask ×
   * bought, boycott gated, volume price applied, same as europe_buy_cargo.
   */
  if (!eu || !units || cargo_type < 0 || cargo_type >= eu->cargo_count || amount <= 0) {
    return 0;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u || !u->active || !units_is_transport(units, unit_id)) {
    return 0;
  }
  if (europe_cargo_boycotted_ex(eu, col1, (int)u->nation_id, cargo_type)) {
    return 0;
  }
  const int ask = eu->cargo[cargo_type].ask;
  if (ask <= 0) {
    return 0;
  }
  int buy = amount > 100 ? 100 : amount;
  const int can_afford = eu->gold / ask;
  if (buy > can_afford) {
    buy = can_afford;
  }
  if (buy <= 0) {
    return 0;
  }
  const int loaded = units_load_goods(units, unit_id, cargo_type, buy);
  if (loaded <= 0) {
    return 0;
  }
  europe_purse_move(eu, col1, (int)u->nation_id, -(long)loaded * ask);
  /* Smell audit #50: ledger (1d80 gold −= ask·amt) + price move. */
  europe_apply_trade_volume(
    eu, col1, (int)u->nation_id, col1 ? (int)col1->head.human_player : (int)u->nation_id,
    cargo_type, loaded, 1, 1
  );
  diag_info(
    "EUROPE bought %d %s onto unit %d: ask=%d cost=%d gold=%d",
    loaded, eu->cargo[cargo_type].name, unit_id, ask, loaded * ask, eu->gold
  );
  return loaded;
}


int europe_harbor_cargo_room(
  const EuropeScreen* eu,
  const ColonizeUnitPool* units,
  int harbor_index,
  int cargo_type
) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  const EuropeHarborShip* ship = &eu->harbor[harbor_index];
  const int free_slots = europe_ship_free_slots(ship, units);
  int room = free_slots * 100;
  /*
   * DOS FUN_15eb_3208 only tallies the part-full matching holds when there is
   * no free slot left at all — with a free slot the room is already a whole
   * hold, and FUN_38fd_1fa2 clamps the buy to 100 either way.
   */
  if (free_slots == 0) {
    for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
      const int amt = ship->hold_goods_amount[i];
      if (amt > 0 && amt < 100 && ship->hold_goods_type[i] == cargo_type) {
        room += 100 - amt;
      }
    }
  }
  return room;
}

int europe_buy_cargo_w(
  const ColonizeWorld* w,
  int buyer_nation,
  int harbor_index,
  int cargo_type,
  int amount
) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1;
  const ColonizeUnitPool* units = w->units;

  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= eu->cargo_count || amount <= 0) {
    return 0;
  }
  if (europe_cargo_boycotted_ex(eu, col1, buyer_nation, cargo_type)) {
    const char* cname = eu->cargo[cargo_type].name[0] ? eu->cargo[cargo_type].name : "That cargo";
    snprintf(
      eu->status, sizeof(eu->status), "%s is boycotted — cannot trade in Europe.", cname
    );
    return 0;
  }
  const int ask = eu->cargo[cargo_type].ask;
  if (ask <= 0) {
    europe_set_status(eu, "Cannot buy that cargo.");
    return 0;
  }
  EuropeHarborShip* ship = &eu->harbor[harbor_index];
  /*
   * Hold capacity (smell audit #83). DOS FUN_38fd_1fa2 asks FUN_281f_0b96 →
   * FUN_15eb_3208 for this ship's free room BEFORE charging anything and
   * bails to the "no room" popup when it comes back 0; the port used to walk
   * all six EUROPE_SHIP_CARGO_MAX slots unconditionally, so a Caravel (2
   * holds) loaded 600 tons. Capacity is the @UNIT cargo column, minus goods
   * slots already used, minus passengers (same slots — see
   * europe_board_sentry_dockers). A NULL pool still falls back to the
   * six-slot maximum, as europe_ship_cargo_cap always has.
   */
  const int free_slots = europe_ship_free_slots(ship, units);
  const int room_total = europe_harbor_cargo_room(eu, units, harbor_index, cargo_type);
  if (room_total <= 0) {
    europe_set_status(eu, "No empty hold.");
    return 0;
  }
  int can_afford = eu->gold / ask;
  if (can_afford <= 0) {
    europe_set_status(eu, "Need gold.");
    return 0;
  }
  int buy = amount;
  if (buy > 100) {
    buy = 100;
  }
  if (buy > room_total) {
    buy = room_total;
  }
  if (buy > can_afford) {
    buy = can_afford;
  }
  if (buy <= 0) {
    return 0;
  }

  /* Same two-pass packing as units_load_goods (top up matching partial
   * holds to 100, then append into free slots); the append pass is bounded
   * by `free_slots` because DOS FUN_15eb_30b8 only appends while
   * holds_occupied < cargo_cap. Shared body: goods_pack_into_holds. */
  const int remaining = buy - goods_pack_into_holds(
    ship->hold_goods_type, ship->hold_goods_amount, EUROPE_SHIP_CARGO_MAX, cargo_type, buy,
    free_slots
  );
  const int bought = buy - remaining;
  europe_purse_move(eu, col1, buyer_nation, -(long)bought * ask);
  if (bought > 0) {
    /* Smell audit #50: ledger (1d80 gold −= ask·amt) + price move. */
    europe_apply_trade_volume(
      eu, col1, buyer_nation, col1 ? (int)col1->head.human_player : buyer_nation,
      cargo_type, bought, 1, 1
    );
  }
  snprintf(
    eu->status,
    sizeof(eu->status),
    "Bought %d %s (-%d$).",
    bought,
    eu->cargo[cargo_type].name,
    bought * ask
  );
  diag_info(
    "EUROPE bought %d %s onto %s: ask=%d cost=%d gold=%d",
    bought, eu->cargo[cargo_type].name, ship->name[0] ? ship->name : "ship",
    ask, bought * ask, eu->gold
  );
  return bought;
}

int europe_best_sell_hold(const EuropeScreen* eu, int harbor_index) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return -1;
  }
  const EuropeHarborShip* ship = &eu->harbor[harbor_index];
  int best = -1;
  int best_v = 0;
  int best_amt = 0;
  int first_loaded = -1;
  /*
   * Ordering heuristic only — a rough "which hold is worth the most" ranking,
   * with no DOS counterpart (DOS's own whole-cargo sweep, FUN_479b_0bd0 as
   * ported in game_europe_service_trade_harbor, just walks slots 0..5 in
   * index order). Its four zeroes (Lumber 5, Horses 8, Tools 14, Muskets 15)
   * used to make this function REFUSE such a hold outright, which broke both
   * its callers: European Status "U" loops on this picker to unload the whole
   * cargo and stopped at the first zero-value hold, and "-" fell back to it
   * and answered "Nothing to sell." on a ship full of muskets. The ranking
   * still decides the ORDER; it no longer decides whether a loaded hold is
   * sellable at all (smell audit #88).
   */
  static const int k_value[COLONIZE_CARGO_COUNT] = {
    1, 5, 4, 3, 5, 0, 4, 20, 0, 8, 8, 7, 7, 2, 0, 0
  };
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    const int amt = ship->hold_goods_amount[i];
    const int ctype = ship->hold_goods_type[i];
    if (amt <= 0 || amt >= 255) {
      continue;
    }
    if (first_loaded < 0) {
      first_loaded = i;
    }
    const int v =
      (ctype >= 0 && ctype < COLONIZE_CARGO_COUNT) ? k_value[ctype] : 0;
    if (v <= 0) {
      continue;
    }
    if (v > best_v || (v == best_v && amt > best_amt)) {
      best_v = v;
      best_amt = amt;
      best = i;
    }
  }
  return best >= 0 ? best : first_loaded;
}

static bool europe_in_rect(int mx, int my, int x, int y, int w, int h) {
  return ui_rect_hit(x, y, w, h, mx, my);
}

static int europe_ship_icon_sprite(const ColonizeUnitPool* units, const EuropeHarborShip* ship) {
  if (!units || !ship) {
    return -1;
  }
  int ti = ship->type_index;
  if (ti < 0) {
    ti = units_find_type(units, ship->name);
  }
  const ColonizeUnitType* ut = units_type(units, ti);
  return ut ? ut->icon_sprite : -1;
}

int europe_pax_type_index(const ColonizeUnitPool* units, int tag) {
  if (tag == -2) {
    const int t = units_kind_type_index(units, UNITS_KIND_ARTILLERY);
    return t >= 0 ? t : 0;
  }
  if (!units || tag < 0 || tag >= units->type_count) {
    const int t = units_kind_type_index(units, UNITS_KIND_COLONIST);
    return t >= 0 ? t : 0;
  }
  return tag;
}

bool europe_icon_flow_begin(
  EuropeIconFlow* f, int box_x, int box_y, int box_w, int box_h, int line_h
) {
  if (!f) {
    return false;
  }
  line_h = line_h > 0 ? line_h : 8;
  const int header_h = EUROPE_TRANSIT_HEADER_LINES * line_h;
  f->box_x = box_x;
  f->box_y = box_y;
  f->box_w = box_w;
  f->box_h = box_h;
  f->x = box_x + 3;
  f->y = box_y + 2 + header_h + 10;
  f->row_h = 0;
  const int ship_area_h = box_y + box_h - f->y - 1;
  return ship_area_h >= 8;
}

bool europe_icon_flow_place(EuropeIconFlow* f, int w, int h) {
  if (f->x + w > f->box_x + f->box_w - 2) {
    f->x = f->box_x + 3;
    f->y += f->row_h + 1;
    f->row_h = 0;
    if (f->y + h > f->box_y + f->box_h - 1) {
      return false;
    }
  }
  if (h > f->row_h) {
    f->row_h = h;
  }
  return true;
}

void europe_icon_flow_advance(EuropeIconFlow* f, int w) {
  f->x += w + 2;
}

void europe_icon_flow_size(const ColonizeSpriteSheet* icons, int sprite, int* w, int* h) {
  const ColonizeSprite* sp = &icons->sprites[sprite];
  *w = sp->width > 0 ? sp->width : 14;
  *h = sp->height > 0 ? sp->height : 16;
}

static int europe_transit_ship_at(
  const EuropeHarborShip* ships,
  int count,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int box_x,
  int box_y,
  int box_w,
  int box_h,
  int transit_line_h,
  int mx,
  int my
) {
  if (!ships || count <= 0 || !units || !unit_icons || !europe_in_rect(mx, my, box_x, box_y, box_w, box_h)) {
    return -1;
  }
  EuropeIconFlow f;
  if (!europe_icon_flow_begin(&f, box_x, box_y, box_w, box_h, transit_line_h)) {
    return -1;
  }
  for (int i = 0; i < count; ++i) {
    const int sprite = europe_ship_icon_sprite(units, &ships[i]);
    if (sprite < 0 || sprite >= unit_icons->sprite_count) {
      continue;
    }
    int sw = 0;
    int sh = 0;
    europe_icon_flow_size(unit_icons, sprite, &sw, &sh);
    if (!europe_icon_flow_place(&f, sw, sh)) {
      break;
    }
    if (europe_in_rect(mx, my, f.x, f.y, sw, sh)) {
      return i;
    }
    europe_icon_flow_advance(&f, sw);
    /* Passengers ride along after their ship (see the renderer); a click on
     * one selects the ship — they are cargo, the ship is what the player
     * clicks. */
    for (int c = 0; c < ships[i].cargo_count && c < EUROPE_SHIP_CARGO_MAX; ++c) {
      const int pax_type = europe_pax_type_index(units, ships[i].cargo_types[c]);
      const int pax_sprite =
        europe_passenger_icon_sprite(units, pax_type, ships[i].cargo_professions[c]);
      if (pax_sprite < 0 || pax_sprite >= unit_icons->sprite_count) {
        continue;
      }
      int pw = 0;
      int ph = 0;
      europe_icon_flow_size(unit_icons, pax_sprite, &pw, &ph);
      if (!europe_icon_flow_place(&f, pw, ph)) {
        break;
      }
      if (europe_in_rect(mx, my, f.x, f.y, pw, ph)) {
        return i;
      }
      europe_icon_flow_advance(&f, pw);
    }
  }
  /* Clicked the box but not an icon — use first ship. */
  return count > 0 ? 0 : -1;
}

EuropeHitResult europe_hit_test(const EuropeScreen* eu, int mx, int my) {
  return europe_hit_test_ex(eu, mx, my, NULL, NULL, 8);
}

EuropeHitResult europe_hit_test_ex(
  const EuropeScreen* eu,
  int mx,
  int my,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int transit_line_h
) {
  EuropeHitResult hit;
  hit.kind = EUROPE_HIT_NONE;
  hit.index = -1;
  if (!eu) {
    return hit;
  }

  if (mx >= EUROPE_EXIT_X && mx < EUROPE_SCREEN_W && my >= EUROPE_EXIT_Y && my < EUROPE_SCREEN_H) {
    hit.kind = EUROPE_HIT_EXIT;
    return hit;
  }

  if (europe_in_rect(mx, my, EUROPE_BTN_X, EUROPE_BTN_Y, EUROPE_BTN_W, EUROPE_BTN_H)) {
    hit.kind = EUROPE_HIT_BTN_RECRUIT;
    return hit;
  }
  if (europe_in_rect(
        mx, my, EUROPE_BTN_X, EUROPE_BTN_Y + EUROPE_BTN_PITCH, EUROPE_BTN_W + 8, EUROPE_BTN_H
      )) {
    hit.kind = EUROPE_HIT_BTN_PURCHASE;
    return hit;
  }
  if (europe_in_rect(
        mx,
        my,
        EUROPE_BTN_X,
        EUROPE_BTN_Y + 2 * EUROPE_BTN_PITCH,
        EUROPE_BTN_W,
        EUROPE_BTN_H
      )) {
    hit.kind = EUROPE_HIT_BTN_TRAIN;
    return hit;
  }

  {
    if (eu->harbor_ships > 0 && my >= EUROPE_HOLD_Y && my < EUROPE_HOLD_Y + EUROPE_HOLD_H &&
        mx >= EUROPE_HOLD_X && mx < EUROPE_HOLD_X + EUROPE_HOLD_MAX * EUROPE_HOLD_PITCH) {
      const int idx = (mx - EUROPE_HOLD_X) / EUROPE_HOLD_PITCH;
      if (idx >= 0 && idx < EUROPE_HOLD_MAX) {
        hit.kind = EUROPE_HIT_HOLD;
        hit.index = idx;
        return hit;
      }
    }
  }

  if (eu->harbor_ships > 0 &&
      europe_in_rect(mx, my, EUROPE_LOADING_X, EUROPE_LOADING_Y, EUROPE_LOADING_W, EUROPE_LOADING_H)) {
    hit.kind = EUROPE_HIT_HARBOR_SHIP;
    if (units && unit_icons) {
      const int si = europe_transit_ship_at(
        eu->harbor,
        eu->harbor_ships,
        units,
        unit_icons,
        EUROPE_LOADING_X,
        EUROPE_LOADING_Y,
        EUROPE_LOADING_W,
        EUROPE_LOADING_H,
        transit_line_h,
        mx,
        my
      );
      hit.index = si >= 0 ? si : (eu->selected_harbor >= 0 ? eu->selected_harbor : 0);
    } else {
      hit.index = eu->selected_harbor >= 0 ? eu->selected_harbor : 0;
    }
    return hit;
  }

  /* Transit boxes stay hittable when empty so ships can be dropped into them. */
  if (europe_in_rect(mx, my, EUROPE_EXPECTED_X, EUROPE_EXPECTED_Y, EUROPE_EXPECTED_W, EUROPE_EXPECTED_H)) {
    hit.kind = EUROPE_HIT_EXPECTED;
    if (eu->expected_ships > 0 && units && unit_icons) {
      hit.index = europe_transit_ship_at(
        eu->expected,
        eu->expected_ships,
        units,
        unit_icons,
        EUROPE_EXPECTED_X,
        EUROPE_EXPECTED_Y,
        EUROPE_EXPECTED_W,
        EUROPE_EXPECTED_H,
        transit_line_h,
        mx,
        my
      );
    } else {
      hit.index = eu->expected_ships > 0 ? 0 : -1;
    }
    return hit;
  }
  if (europe_in_rect(mx, my, EUROPE_BOUND_X, EUROPE_BOUND_Y, EUROPE_BOUND_W, EUROPE_BOUND_H)) {
    hit.kind = EUROPE_HIT_BOUND;
    if (eu->bound_ships > 0 && units && unit_icons) {
      hit.index = europe_transit_ship_at(
        eu->bound,
        eu->bound_ships,
        units,
        unit_icons,
        EUROPE_BOUND_X,
        EUROPE_BOUND_Y,
        EUROPE_BOUND_W,
        EUROPE_BOUND_H,
        transit_line_h,
        mx,
        my
      );
    } else {
      hit.index = eu->bound_ships > 0 ? 0 : -1;
    }
    return hit;
  }

  for (int i = 0; i < eu->dock_count; ++i) {
    int dx = 0;
    int dy = 0;
    if (!europe_dock_slot_pos(i, &dx, &dy)) {
      break;
    }
    if (europe_in_rect(mx, my, dx, dy, EUROPE_DOCK_UNIT_W, EUROPE_DOCK_UNIT_H)) {
      hit.kind = EUROPE_HIT_DOCK;
      hit.index = i;
      return hit;
    }
  }

  if (my >= EUROPE_MARKET_Y && my < EUROPE_MARKET_Y + EUROPE_MARKET_H && mx >= EUROPE_MARKET_X) {
    const int idx = (mx - EUROPE_MARKET_X) / EUROPE_MARKET_PITCH;
    if (idx >= 0 && idx < eu->cargo_count && idx < EUROPE_CARGO_MAX &&
        mx < EUROPE_MARKET_X + idx * EUROPE_MARKET_PITCH + EUROPE_MARKET_CELL) {
      hit.kind = EUROPE_HIT_MARKET;
      hit.index = idx;
      return hit;
    }
  }

  return hit;
}

/* Europe list-dialog name, for the debug log only. */
static const char* europe_menu_name(EuropeMenu menu) {
  switch (menu) {
    case EUROPE_MENU_RECRUIT: return "RECRUIT";
    case EUROPE_MENU_TRAIN: return "TRAIN";
    case EUROPE_MENU_PURCHASE: return "PURCHASE";
    case EUROPE_MENU_DOCK: return "DOCK";
    default: return "NONE";
  }
}

void europe_menu_open(EuropeScreen* eu, EuropeMenu menu) {
  if (!eu) {
    return;
  }
  eu->menu = menu;
  eu->menu_selection = 0;
  eu->menu_answered = false;
  if (menu == EUROPE_MENU_RECRUIT) {
    europe_pool_ensure_filled(eu);
    snprintf(
      eu->status,
      sizeof(eu->status),
      "Recruit (passage %d$). Esc cancels.",
      eu->recruit_passage
    );
  } else if (menu == EUROPE_MENU_TRAIN) {
    europe_set_status(eu, "Royal University. Esc cancels.");
  } else if (menu == EUROPE_MENU_PURCHASE) {
    europe_set_status(eu, "Purchase. Esc cancels.");
  } else if (menu == EUROPE_MENU_DOCK) {
    /* FUN_38fd_3694 (raw 61180-61197): the dock-unit click builds the
     * "<nation> <unit> (<job>)" caption on the status strip. */
    char caption[96];
    if (europe_dock_caption(eu, eu->menu_dock_index, caption, sizeof caption)) {
      europe_set_status(eu, caption);
    } else {
      europe_set_status(eu, "Dock orders. Esc cancels.");
    }
  }
  if (diag_info_enabled()) {
    char rows[512];
    size_t at = 0;
    rows[0] = '\0';
    if (menu == EUROPE_MENU_RECRUIT) {
      for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
        const int n = snprintf(
          rows + at, sizeof(rows) - at, "%s[%d]%s", i ? " " : "", i + 1, eu->pool[i].name
        );
        if (n <= 0 || (size_t)n >= sizeof(rows) - at) {
          break;
        }
        at += (size_t)n;
      }
    } else if (menu == EUROPE_MENU_TRAIN) {
      for (int i = 0; i < eu->train_count; ++i) {
        const int n = snprintf(
          rows + at, sizeof(rows) - at, "%s[%d]%s %d$", i ? " " : "", i + 1,
          eu->train[i].expert_name, eu->train[i].cost
        );
        if (n <= 0 || (size_t)n >= sizeof(rows) - at) {
          break;
        }
        at += (size_t)n;
      }
    } else if (menu == EUROPE_MENU_PURCHASE) {
      for (int i = 0; i < eu->purchase_count; ++i) {
        const int n = snprintf(
          rows + at, sizeof(rows) - at, "%s[%d]%s %d$", i ? " " : "", i + 1,
          eu->purchase[i].name, europe_purchase_cost(eu, i)
        );
        if (n <= 0 || (size_t)n >= sizeof(rows) - at) {
          break;
        }
        at += (size_t)n;
      }
    } else if (menu == EUROPE_MENU_DOCK) {
      for (int i = 0; i < eu->dock_menu_count; ++i) {
        const int n = snprintf(
          rows + at, sizeof(rows) - at, "%s[%d]%s", i ? " " : "",
          (int)eu->dock_menu_row[i], europe_arm_row_name((int)eu->dock_menu_row[i])
        );
        if (n <= 0 || (size_t)n >= sizeof(rows) - at) {
          break;
        }
        at += (size_t)n;
      }
    }
    diag_info(
      "POPUP show tag=EUROPE_%s kind=choice gold=%d choices=%s",
      europe_menu_name(menu), eu->gold, rows
    );
  }
}

void europe_menu_close(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  if (eu->menu != EUROPE_MENU_NONE && !eu->menu_answered) {
    diag_info("POPUP answered tag=EUROPE_%s cancelled", europe_menu_name(eu->menu));
  }
  eu->menu_answered = false;
  eu->menu = EUROPE_MENU_NONE;
  eu->menu_selection = 0;
  eu->menu_dock_index = -1;
}

/*
 * Turn the highlighted row of the open dock menu into its @ARMOPTIONS row id
 * and apply it. Row ids are carried per-row because DOS omits the rows it
 * disabled, so the visible index is not the id.
 */
bool europe_dock_menu_apply_selection_ex_w(
  const ColonizeWorld* w,
  int nation_id
) {
  EuropeScreen* eu = w->europe;
  ColonizeUnitPool* units = w->units;
  ColonizeCol1Save* col1 = w->col1;

  if (!eu || eu->menu != EUROPE_MENU_DOCK) {
    return false;
  }
  const int sel = eu->menu_selection;
  if (sel < 0 || sel >= eu->dock_menu_count) {
    return false;
  }
  if (eu->dock_menu_greyed[sel]) {
    europe_set_status(eu, "The treasury cannot afford that.");
    return false;
  }
  eu->menu_answered = true;
  return europe_apply_dock_menu_row_ex(
    eu, units, col1, nation_id, eu->menu_dock_index, (int)eu->dock_menu_row[sel]
  );
}


static bool europe_dock_menu_apply_selection(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int nation_id
) {
  return europe_dock_menu_apply_selection_ex_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL), .europe=(EuropeScreen*)(eu)}, nation_id);
}

bool europe_menu_confirm(EuropeScreen* eu) {
  return europe_menu_confirm_ex(eu, NULL);
}

bool europe_menu_confirm_ex(EuropeScreen* eu, ColonizeDosRng* rng) {
  if (!eu || eu->menu == EUROPE_MENU_NONE) {
    return false;
  }
  const EuropeMenu m = eu->menu;
  const int sel = eu->menu_selection;
  if (m != EUROPE_MENU_DOCK) {
    diag_info(
      "POPUP answered tag=EUROPE_%s %s row=%d", europe_menu_name(m),
      sel == 0 ? "cancelled" : "picked", sel
    );
    eu->menu_answered = true;
  }
  if (m == EUROPE_MENU_DOCK) {
    /* Units-less path (no pool to keep the mirror unit in step); game_loop
     * calls europe_dock_menu_apply_selection with the pool so equipment,
     * type and dock sprite follow the row that was picked. */
    const bool ok = europe_dock_menu_apply_selection(eu, NULL, -1);
    europe_menu_close(eu);
    return ok;
  }
  if (sel == 0) {
    europe_menu_close(eu);
    europe_set_status(eu, "Cancelled.");
    return true;
  }
  if (m == EUROPE_MENU_RECRUIT) {
    const int pool_i = sel - 1;
    const bool ok = europe_recruit_from_pool_ex(eu, pool_i, rng);
    europe_menu_close(eu);
    return ok;
  }
  if (m == EUROPE_MENU_TRAIN) {
    const bool ok = europe_train_ex(eu, sel - 1, rng);
    europe_menu_close(eu);
    return ok;
  }
  if (m == EUROPE_MENU_PURCHASE) {
    const bool ok = europe_purchase_ex(eu, sel - 1, rng);
    europe_menu_close(eu);
    return ok;
  }
  europe_menu_close(eu);
  return false;
}

void europe_cheat_add_gold(EuropeScreen* eu, int amount) {
  if (!eu) {
    return;
  }
  eu->gold += amount;
  if (eu->gold < 0) {
    eu->gold = 0;
  }
  snprintf(eu->status, sizeof(eu->status), "Treasury now %d$.", eu->gold);
}

void europe_cheat_adjust_tax(EuropeScreen* eu, int delta) {
  if (!eu) {
    return;
  }
  eu->tax_percent += delta;
  if (eu->tax_percent < 0) {
    eu->tax_percent = 0;
  }
  if (eu->tax_percent > 100) {
    eu->tax_percent = 100;
  }
  snprintf(eu->status, sizeof(eu->status), "Tax rate %d%%.", eu->tax_percent);
}

int europe_dock_icon_sprite(const ColonizeUnitPool* units, const EuropeDockImmigrant* d) {
  if (!units || !d || !d->name[0]) {
    return -1;
  }
  if (europe_dock_name_is_artillery(d->name)) {
    const int ti = units_kind_type_index(units, UNITS_KIND_ARTILLERY);
    const ColonizeUnitType* ut = units_type(units, ti);
    return ut ? ut->icon_sprite : -1;
  }
  /* Armed / equipped / blessed on the dock: show what the immigrant now is,
   * not the profession portrait it arrived with (bugs.md @ARMOPTIONS).
   * bugs.md #158/177: the @UNIT icon column carries the EXPERT poses (Hardy
   * Pioneer / Veteran Soldier …) — DOS's map rule overrides those to the
   * base pose unless the unit's own profession matches; the dock follows
   * the same rule, so a Master Blacksmith with tools reads as a plain
   * Pioneer, not a Hardy one. */
  /* bugs.md #263 (units_map_sprite): BOTH veteran professions (0x15 Veteran
   * Soldiers / 0x17 Veteran Dragoons) take the veteran pose — a Veteran
   * Soldier armed with horses on the dock is a Veteran Dragoon, exactly as
   * the map draws him. */
  const bool dock_vet_prof =
    d->profession == UNITS_JOB_SOLDIER || d->profession == UNITS_JOB_DRAGOON;
  switch (d->dos_type) {
    case EUROPE_DOCK_TYPE_PIONEERS:
      return d->profession == UNITS_JOB_PIONEER ? UNITS_ICON_HARDY_PIONEER
                                                : UNITS_ICON_PIONEER;
    case EUROPE_DOCK_TYPE_SOLDIERS:
      return dock_vet_prof ? UNITS_ICON_VETERAN_SOLDIER : UNITS_ICON_SOLDIER;
    case EUROPE_DOCK_TYPE_DRAGOONS:
      return dock_vet_prof ? UNITS_ICON_VETERAN_DRAGOON : UNITS_ICON_DRAGOON;
    case EUROPE_DOCK_TYPE_SCOUTS:
      return d->profession == UNITS_JOB_SCOUT ? UNITS_ICON_SEASONED_SCOUT
                                              : UNITS_ICON_SCOUT;
    /* bugs.md #420: same split for the fifth kit — FUN_112b_0060's
     * `type == 3 && profession != 0x18 → 0x4e`. Without this case a
     * shipped-home missionary took the @UNIT icon (the Jesuit) whatever
     * his colonist was. */
    case EUROPE_DOCK_TYPE_MISSIONARIES:
      return d->profession == UNITS_JOB_MISSIONARY ? UNITS_ICON_JESUIT_MISSIONARY
                                                   : UNITS_ICON_MISSIONARY;
    default:
      break;
  }
  if (d->dos_type != EUROPE_DOCK_TYPE_COLONISTS) {
    const int eti = europe_dock_unit_type_index(units, d->dos_type);
    const ColonizeUnitType* eut = units_type(units, eti);
    if (eut && eut->icon_sprite >= 0) {
      return eut->icon_sprite;
    }
  }
  int ti = units_find_type(units, d->name);
  if (ti < 0) {
    ti = units_kind_type_index(units, UNITS_KIND_COLONIST);
  }
  if (ti < 0) {
    return -1;
  }
  const ColonizeUnitType* ut = units_type(units, ti);
  if (units_type_is_colonist(ut)) {
    return units_working_colonist_sprite(units, ti, d->profession);
  }
  return ut ? ut->icon_sprite : -1;
}

int europe_passenger_icon_sprite(const ColonizeUnitPool* units, int type_index, int profession) {
  const ColonizeUnitType* ut = units_type(units, type_index);
  if (!ut) {
    return -1;
  }
  if (units_type_is_colonist(ut)) {
    return units_working_colonist_sprite(units, type_index, profession);
  }
  /*
   * bugs.md #452: the @UNIT icon of the five kit types is the EXPERT pose
   * (Hardy Pioneer, Veteran Soldier, ...), so a Master Carpenter given tools
   * sailed out as a Hardy Pioneer. A passenger is the same unit the dock
   * showed a moment earlier: route it through the dock's expert/generic
   * split (DOS FUN_112b_0060), keyed by its own profession.
   */
  for (int k = EUROPE_DOCK_TYPE_SOLDIERS; k < EUROPE_DOCK_TYPE_COUNT; ++k) {
    if (strcmp(ut->name, reports_dock_type_name(k)) == 0) {
      EuropeDockImmigrant d;
      memset(&d, 0, sizeof(d));
      snprintf(d.name, sizeof(d.name), "%s", ut->name);
      d.profession = profession;
      d.present = true;
      d.dos_type = k;
      return europe_dock_icon_sprite(units, &d);
    }
  }
  return ut->icon_sprite;
}
