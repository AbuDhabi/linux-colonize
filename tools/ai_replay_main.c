/*
 * ai_replay — headless Euro-AI replay driver.
 *
 * Loads a real COLONY##.SAV, applies it through col1_bridge, then loops
 * `turn_end` and prints one line per AI nation per turn: colonies, total
 * colony population, live units, hulls, units parked in the off-map Europe
 * park, hulls in that park, and treasury. That is enough to see the Euro AI
 * either growing or standing still over a few dozen turns without a UI.
 *
 * This is the "headless driver on a real save" method from
 * docs/conventions.md §4, promoted out of the scratch drivers the ship-wiggle
 * and Europe-freeze investigations kept rewriting (bugs.md #953).
 *
 * Usage:
 *   ai_replay <save.SAV> [turns] [--units] [--seed N]
 *
 *   turns    number of turn_end calls (default 20)
 *   --units  also dump every AI unit (id / nation / type / xy / orders / MP /
 *            goto / carrier / cargo) before the first turn and after the last
 *   --seed   DOS RNG seed (default 100, the value the AI goldens use)
 *
 * Run from the repo root: NAMES.TXT and COLONY.TXT are read from COLONIZE/.
 *
 * Trap worth keeping: without `ctx.ai_popups` the King auto-declares
 * independence at SoL >= 50 (ai_king_try_declare only suppresses itself when a
 * popup queue exists, because a headless run has nobody to answer the CHOICE)
 * and the War of Independence despawns every rival Euro unit on turn 1. The
 * queue below is what keeps a replay of a late-game save meaningful.
 */
#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"
#include "core/units_move.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AiReplayCounts {
  int colonies;
  int population;
  int units;
  int ships;
  int parked;
  int parked_ships;
  long gold;
} AiReplayCounts;

static void ai_replay_count(
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* save,
  int nation_id,
  AiReplayCounts* out
) {
  memset(out, 0, sizeof(*out));
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (c->active && c->nation_id == nation_id) {
      out->colonies++;
      out->population += c->population;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    const int sea = units_is_sea(units, u->id) ? 1 : 0;
    const int parked = units_coords_in_europe_park(u->x, u->y) ? 1 : 0;
    out->units++;
    out->ships += sea;
    out->parked += parked;
    out->parked_ships += (sea && parked) ? 1 : 0;
  }
  if (nation_id >= 0 && nation_id < 4) {
    out->gold = (long)save->nation[nation_id].gold;
  }
}

static void ai_replay_dump_units(
  const ColonizeUnitPool* units, int human_nation, const char* tag
) {
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id < 0 || u->nation_id > 3 || u->nation_id == human_nation) {
      continue;
    }
    printf(
      "%s u%-3d n%d type%-2d (%3d,%3d) orders %-2d mp %-3d goto (%3d,%3d) aboard %-3d cargo %d\n",
      tag, u->id, u->nation_id, u->type_index, u->x, u->y, u->orders, u->moves, u->goto_x,
      u->goto_y, u->aboard_ship_id, u->cargo_count
    );
  }
}

int main(int argc, char** argv) {
  const char* path = NULL;
  int turns = 20;
  int dump_units = 0;
  unsigned seed = 100u;
  for (int a = 1; a < argc; ++a) {
    if (strcmp(argv[a], "--units") == 0) {
      dump_units = 1;
    } else if (strcmp(argv[a], "--seed") == 0 && a + 1 < argc) {
      seed = (unsigned)strtoul(argv[++a], NULL, 10);
    } else if (!path) {
      path = argv[a];
    } else {
      turns = atoi(argv[a]);
    }
  }
  if (!path) {
    fprintf(stderr, "usage: ai_replay <save.SAV> [turns] [--units] [--seed N]\n");
    return 2;
  }
  if (turns < 0) {
    turns = 0;
  }

  char err[256];
  ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file(path, &save, err, sizeof(err))) {
    fprintf(stderr, "ai_replay: read %s: %s\n", path, err);
    return 1;
  }

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "ai_replay: COLONIZE/NAMES.TXT (run from the repo root)\n");
    col1_save_free(&save);
    return 1;
  }

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "ai_replay: units_load_types\n");
    assets_msg_free(&names);
    col1_save_free(&save);
    return 1;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "ai_replay: colonies_load_buildings\n");
    assets_msg_free(&names);
    col1_save_free(&save);
    return 1;
  }
  (void)colonies_load_names(&colonies, "COLONIZE/COLONY.TXT");

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;

  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply_w(
        &(ColonizeWorld){
          .units = &units,
          .colonies = &colonies,
          .map = &map,
          .col1 = &save,
          .col1_ok = true,
          .europe = &europe
        },
        &br, err, sizeof(err)
      )) {
    fprintf(stderr, "ai_replay: bridge apply: %s\n", err);
    assets_msg_free(&names);
    col1_save_free(&save);
    return 1;
  }

  uint32_t turn_number = br.turn_number;
  uint16_t year = br.year;
  uint16_t autumn = br.autumn;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, seed);
  AiPopupState popups;
  memset(&popups, 0, sizeof(popups));

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
  ctx.col1 = &save;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.rng_seed = seed;
  ctx.ai_popups = &popups;

  printf(
    "ai_replay %s: turn %u year %u, human nation %d, %d turns, seed %u\n", path, turn_number,
    (unsigned)year, (int)br.human_nation, turns, seed
  );
  if (dump_units) {
    ai_replay_dump_units(&units, (int)br.human_nation, "pre ");
  }

  for (int t = 0; t < turns; ++t) {
    turn_end(&ctx);
    /* Drain the queue: nothing answers popups here, and a full queue would
     * change what later turns are allowed to enqueue. */
    memset(&popups, 0, sizeof(popups));
    for (int n = 0; n < 4; ++n) {
      if (n == (int)br.human_nation) {
        continue;
      }
      AiReplayCounts c;
      ai_replay_count(&units, &colonies, &save, n, &c);
      printf(
        "turn %4u nation %d  colonies %2d pop %3d units %3d hulls %2d  inEurope %2d (hulls %d)  "
        "gold %ld\n",
        turn_number, n, c.colonies, c.population, c.units, c.ships, c.parked, c.parked_ships,
        c.gold
      );
    }
    fflush(stdout);
  }

  if (dump_units) {
    ai_replay_dump_units(&units, (int)br.human_nation, "post");
  }

  assets_msg_free(&names);
  col1_save_free(&save);
  return 0;
}
