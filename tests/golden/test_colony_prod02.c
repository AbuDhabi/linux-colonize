#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * Colony production golden #2: COLONY00-dutch2-t0.SAV -> one turn_end() ->
 * compare against COLONY01-dutch2-t1.SAV, a save produced by running the
 * *real* one turn in original DOS (not Linux-derived). A later, larger
 * capture than colony_prod01 (turn 169, 17 colonies, established Dutch
 * economy with indoor manufacturing specialists), same idea: the player
 * made no moves that turn, so any Dutch colony field/building drift is
 * either engine production math or a bridge apply/capture bug.
 *
 * Unlike colony_prod01, this pair has no hand-reconstructed map-tile
 * patches: col1_bridge_apply builds the map straight from this save's own
 * embedded tile data, and that was enough here (see colony_prod01's
 * comment for *why* it needed patches — a different, small fixture whose
 * terrain round-trip needed hand correction; this larger capture didn't
 * hit that gap).
 *
 * Only human-controlled Dutch (nation_id 3) colonies are checked; colonies
 * that change hands either side of the turn are excluded, not failed (see
 * compare_dutch_colonies). AI nations are not checked here.
 *
 * Status 2026-08-18: 2/7 Dutch colonies (Guadeloupe, Fort Orange) match
 * exactly; the other 5 have real remaining drift.
 *
 * 2026-08-18 fix #1: New Amsterdam and Fort Orange were wrongly decoded as
 * owning Iron Works (factory-tier Blacksmith) — neither Dutch nor any
 * other nation in this save owns Adam Smith, so that was impossible from
 * the start. Root cause: `col1_apply_building_level`/`col1_encode_
 * building_level` treated a chain's stored level as the tier count N
 * directly, but DOS actually stores `(1 << N) - 1` (player-confirmed:
 * every 2-or-3-tier chain field across both colony_prod01 and
 * colony_prod02's ~30 colonies reads only 0/1/3[/7], the value 2 never
 * once appears — a plain tier-count encoding would produce 2 constantly).
 * Fixed in col1_bridge.c; both colonies now correctly read Blacksmith's
 * Shop, and their tools/muskets numbers now match the real save exactly.
 *
 * 2026-08-18 fix #2: Hills-Ore base is 4, not 3 — player-confirmed via
 * this same Fort Orange colonist (single expert Ore Miner, Hills, no
 * road/river/resource, sentiment +2, no other ore consumer/producer in
 * the colony -> 12 ore = (4+2)x2), paired with its non-specialist
 * Blacksmith's Shop worker (8 tools from 8 ore, confirming the shop-tier
 * manufacturing side unchanged by fix #1). Fort Orange now matches
 * exactly. base=3 had come from golden_colony_prod01's Bahia fixture,
 * whose whole map is hand-synthesized (real terrain was never loaded for
 * it, unlike this save) and carried an unconfirmed "+road" guess that
 * only "worked" by cancelling the wrong base's error; fixed by re-picking
 * that fixture's synthetic terrain instead (see test_colony_prod01.c).
 * New Amsterdam's ore now matches too; its remaining food/horses drift is
 * unrelated (see below).
 *
 * - New Holland: sugar way under (real +11 vs ours +0ish), lumber and
 *   horses also off — not investigated.
 * - New Amsterdam, Vlissingen, Curacao, Recife: smaller food/horses/furs/
 *   sugar drift —
 *   not investigated; likely field-yield or SoL-fold edge cases specific
 *   to this save's tiles, same category as colony_prod01's original
 *   findings but not yet chased down.
 *
 * One real, unrelated bug *was* found and fixed via this save:
 * `colonies_try_complete_building` used to clear `building_in_production`
 * on completion; DOS never does (Vlissingen's Lumber Mill completing this
 * turn proved it) — fixed in colony.c, regression-tested in test_turn.c /
 * test_colony_screen.c.
 */

#define COLONY_PROD02_RNG_SEED 100u
#define COLONY_PROD02_HUMAN_NATION 3 /* Netherlands */

static const char* k_cargo_names[COLONIZE_CARGO_COUNT] = {
  "food",   "sugar",  "tobacco", "cotton", "furs",   "lumber", "ore",   "silver",
  "horses", "rum",    "cigars",  "cloth",  "coats",  "trade",  "tools", "muskets"
};

static int find_colony_by_xy(
  const ColonizeCol1Save* save,
  uint8_t x,
  uint8_t y
) {
  for (unsigned i = 0; i < save->head.colony_count; ++i) {
    if (save->colony[i].x == x && save->colony[i].y == y) {
      return (int)i;
    }
  }
  return -1;
}

static bool compare_colony_production(
  const ColonizeCol1Colony* g,
  const ColonizeCol1Colony* e,
  const char* step_label
) {
  bool ok = true;
  if (g->population != e->population) {
    fprintf(
      stderr,
      "%s %s population got %u expected %u\n",
      step_label, e->name, g->population, e->population
    );
    ok = false;
  }
  if (g->building_in_production != e->building_in_production) {
    fprintf(
      stderr,
      "%s %s building_in_production got %u expected %u\n",
      step_label, e->name, g->building_in_production, e->building_in_production
    );
    ok = false;
  }
  if (g->hammers != e->hammers) {
    fprintf(
      stderr,
      "%s %s hammers got %u expected %u\n",
      step_label, e->name, g->hammers, e->hammers
    );
    ok = false;
  }
  if (g->hammers_purchased != e->hammers_purchased) {
    fprintf(
      stderr,
      "%s %s hammers_purchased got %u expected %u\n",
      step_label, e->name, g->hammers_purchased, e->hammers_purchased
    );
    ok = false;
  }
  if (g->warehouse_level != e->warehouse_level) {
    fprintf(
      stderr,
      "%s %s warehouse_level got %u expected %u\n",
      step_label, e->name, g->warehouse_level, e->warehouse_level
    );
    ok = false;
  }
  if (g->capitol_level != e->capitol_level) {
    fprintf(
      stderr,
      "%s %s capitol_level got %u expected %u\n",
      step_label, e->name, g->capitol_level, e->capitol_level
    );
    ok = false;
  }
  if (g->specialty_cargo != e->specialty_cargo) {
    fprintf(
      stderr,
      "%s %s specialty_cargo got %u expected %u\n",
      step_label, e->name, g->specialty_cargo, e->specialty_cargo
    );
    ok = false;
  }
  if (g->labor_shortage != e->labor_shortage) {
    fprintf(
      stderr,
      "%s %s labor_shortage got %u expected %u\n",
      step_label, e->name, g->labor_shortage, e->labor_shortage
    );
    ok = false;
  }
  if (g->cargo_idle_turns != e->cargo_idle_turns) {
    fprintf(
      stderr,
      "%s %s cargo_idle_turns got %u expected %u\n",
      step_label, e->name, g->cargo_idle_turns, e->cargo_idle_turns
    );
    ok = false;
  }
  if (g->improve_timer != e->improve_timer) {
    fprintf(
      stderr,
      "%s %s improve_timer got %u expected %u\n",
      step_label, e->name, g->improve_timer, e->improve_timer
    );
  }
  for (unsigned c = 0; c < COLONIZE_COL1_CARGO_TYPES; ++c) {
    if (g->stock[c] != e->stock[c]) {
      fprintf(
        stderr,
        "%s %s stock[%s] got %u expected %u\n",
        step_label, e->name, k_cargo_names[c], g->stock[c], e->stock[c]
      );
      ok = false;
    }
  }
  return ok;
}

/*
 * orig = pre-turn save (ground truth start), untouched by turn_end/capture.
 * got = post-turn save (our simulated end state); exp = real-DOS post-turn.
 *
 * A colony that changes hands (either side of the real DOS turn, or only in
 * our own simulation) had combat/AI decide its fate this turn — that's
 * explicitly out of scope here (AI behavior + RNG stream aren't checked by
 * this suite). Only colonies Dutch in orig, Dutch in exp, AND still Dutch in
 * got get a production comparison; everything else is reported as excluded,
 * not failed.
 */
static bool compare_dutch_colonies(
  const ColonizeCol1Save* orig,
  const ColonizeCol1Save* got,
  const ColonizeCol1Save* exp,
  const char* step_label
) {
  bool ok = true;
  int checked = 0;
  int excluded = 0;
  for (unsigned i = 0; i < exp->head.colony_count; ++i) {
    const ColonizeCol1Colony* e = &exp->colony[i];
    if (e->nation_id != COLONY_PROD02_HUMAN_NATION) {
      continue;
    }
    const int oi = find_colony_by_xy(orig, e->x, e->y);
    if (oi < 0 || orig->colony[oi].nation_id != COLONY_PROD02_HUMAN_NATION) {
      ++excluded;
      fprintf(
        stderr,
        "%s excluded '%s' at (%u,%u): our sim changed its ownership (AI/RNG, out of scope)\n",
        step_label, e->name, e->x, e->y
      );
      continue;
    }
    const int gi = find_colony_by_xy(got, e->x, e->y);
    if (gi < 0 || got->colony[gi].nation_id != COLONY_PROD02_HUMAN_NATION) {
      fprintf(
        stderr,
        "%s excluded '%s' at (%u,%u): our sim changed its ownership (AI/RNG, out of scope)\n",
        step_label, e->name, e->x, e->y
      );
      ++excluded;
      continue;
    }
    const ColonizeCol1Colony* g = &got->colony[gi];
    if (strncmp(g->name, e->name, sizeof(g->name)) != 0) {
      fprintf(
        stderr,
        "%s colony at (%u,%u) got name '%s' expected '%s'\n",
        step_label, e->x, e->y, g->name, e->name
      );
      ok = false;
      continue;
    }
    ++checked;
    if (!compare_colony_production(g, e, step_label)) {
      ok = false;
    }
  }
  fprintf(
    stderr, "%s checked %d Dutch colonies (%d excluded, ownership changed)\n",
    step_label, checked, excluded
  );
  return ok;
}

static int run_pair(const char* path_in, const char* path_exp, const char* label) {
  char err[256];

  ColonizeCol1Save start;
  ColonizeCol1Save expect;
  ColonizeCol1Save orig; /* untouched pre-turn snapshot, for ownership-stability checks */
  col1_save_init(&start);
  col1_save_init(&expect);
  col1_save_init(&orig);
  if (!col1_save_read_file(path_in, &start, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_in, err);
    return 1;
  }
  if (!col1_save_read_file(path_exp, &expect, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_exp, err);
    col1_save_free(&start);
    return 1;
  }
  if (!col1_save_read_file(path_in, &orig, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_in, err);
    col1_save_free(&start);
    col1_save_free(&expect);
    return 1;
  }

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "colonies_load_buildings failed\n");
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }
  (void)colonies_load_names(&colonies, "COLONIZE/COLONY.TXT");

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(&start, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
    fprintf(stderr, "bridge apply %s: %s\n", path_in, err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }

  uint32_t turn_number = br.turn_number;
  uint16_t year = br.year;
  uint16_t autumn = br.autumn;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, COLONY_PROD02_RNG_SEED);

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn_number;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.human_nation = br.human_nation;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.map = &map;
  ctx.col1 = &start;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.rng_seed = COLONY_PROD02_RNG_SEED;

  turn_end(&ctx);

  if (!col1_bridge_capture(
        &start,
        &map,
        &units,
        &colonies,
        &europe,
        year,
        autumn,
        turn_number,
        br.human_nation,
        br.cursor_x,
        br.cursor_y,
        units.selected_id,
        err,
        sizeof(err)
      )) {
    fprintf(stderr, "bridge capture: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }

  const bool ok = compare_dutch_colonies(&orig, &start, &expect, label);

  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&start);
  col1_save_free(&expect);
  col1_save_free(&orig);
  if (!ok) {
    fprintf(stderr, "%s FAILED\n", label);
    return 1;
  }
  printf("%s ok\n", label);
  return 0;
}

int main(void) {
  const int rc = run_pair(
    "original_saves/colony-prod-tests/COLONY00-dutch2-t0.SAV",
    "original_saves/colony-prod-tests/COLONY01-dutch2-t1.SAV",
    "colony_prod02 COLONY00->01 (Dutch)"
  );
  if (rc != 0) {
    return rc;
  }
  printf("golden_colony_prod02: Dutch colony production ok\n");
  return 0;
}
