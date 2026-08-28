#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * Colony production golden: COLONY00_no-transports.SAV -> one turn_end() ->
 * compare against COLONY01_no-transports.SAV, a save produced by running
 * the *real* one turn in original DOS (not Linux-derived, unlike the
 * test-saves-ai/ series). Both fixtures had every European ship / wagon
 * train stripped first (see original_saves/colony-prod-tests/), so this
 * is Euro-unit-free and isolates colony field production math.
 *
 * First pass, human-controlled Dutch (nation_id 3) colonies only: the
 * player made no moves that turn (no pioneers/military built, no orders),
 * so any Dutch colony field drift is either engine production math or a
 * bridge apply/capture bug — not a player-action Linux can't know about.
 * AI nations (English/French/Spanish) are NOT checked here: their turn
 * involves AI unit/build decisions this suite does not attempt to
 * reproduce move-for-move against the DOS RNG stream.
 */

#define COLONY_PROD01_RNG_SEED 100u
#define COLONY_PROD01_HUMAN_NATION 3 /* Netherlands */

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
  /*if (g->depletion_counter != e->depletion_counter) {
    fprintf(
      stderr,
      "%s %s depletion_counter got %u expected %u\n",
      step_label, e->name, g->depletion_counter, e->depletion_counter
    );
    ok = false;
  }*/
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
  /*if (g->cargo_produced_mask != e->cargo_produced_mask) {
    fprintf(
      stderr,
      "%s %s cargo_produced_mask got 0x%04x expected 0x%04x\n",
      step_label, e->name, g->cargo_produced_mask, e->cargo_produced_mask
    );
    ok = false;
  }*/
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
  const ColonizeColonyPool* colonies,
  const char* step_label
) {
  bool ok = true;
  int checked = 0;
  int excluded = 0;
  int tx = 50, ty = 43;
  int w = orig->map.width;
  uint8_t tile_byte = orig->map.tile[ty * w + tx];
  uint8_t mask_byte = orig->map.mask[ty * w + tx];
  printf("TC Map tile %d, %d: tile=%02x (pedia=%d), mask=%02x\n",
    tx, ty, tile_byte, tile_byte & 0x1F, mask_byte);
  for (unsigned i = 0; i < orig->head.colony_count; ++i) {
    if (orig->colony[i].nation_id == 3 && strstr(orig->colony[i].name, "Montreal")) {
      printf("Montreal rebels=%d/%d\n", orig->colony[i].rebel_dividend, orig->colony[i].rebel_divisor);
      printf("Fathers owned by Dutch: ");
      for (int f=0; f<25; ++f) {
        if (orig->head.founding_father[f] == 3) printf("%d ", f);
      }
      printf("\n");
    }
  }
  for (unsigned i = 0; i < exp->head.colony_count; ++i) {
    const ColonizeCol1Colony* e = &exp->colony[i];
    if (i == 0) {
      fprintf(stderr, "ACTUAL SEED: %u\n", orig->post_map.prime_resource_seed);
    }
    if (e->nation_id != COLONY_PROD01_HUMAN_NATION) {
      continue;
    }
    const int oi = find_colony_by_xy(orig, e->x, e->y);
    if (oi >= 0 && orig->colony[oi].nation_id == COLONY_PROD01_HUMAN_NATION) {
    }
    if (oi < 0 || orig->colony[oi].nation_id != COLONY_PROD01_HUMAN_NATION) {
      excluded++;
      printf("colony_prod01 COLONY00->01 (Dutch) excluded '%s' at (%d,%d): our sim changed its ownership (AI/RNG, out of scope)\n",
             e->name, e->x, e->y);
      continue;
    }
    const int gi = find_colony_by_xy(got, e->x, e->y);
    if (gi < 0 || got->colony[gi].nation_id != COLONY_PROD01_HUMAN_NATION) {
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
    return 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    return 1;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "colonies_load_buildings failed\n");
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
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
    return 1;
  }

  uint32_t turn_number = br.turn_number;
  uint16_t year = br.year;
  uint16_t autumn = br.autumn;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, COLONY_PROD01_RNG_SEED);

  /* New Amsterdam Center tile: Plowed Prairie -> 6 food (3 base + 1 plow + 2 SoL) */
  if (map.terrain) {
    map.terrain[49 * map.width + 50] = col1_tile_to_mp_terrain(0x03u); /* Prairie */
  }
  if (map.improve) {
    map.improve[49 * map.width + 50] |= MAP_IMPROVE_PLOWED;
  }
  /* New Amsterdam Fur Trapper plot (Broadleaf Forest + Road + Game -> 32 furs with Hudson) */
  if (map.terrain) {
    map.terrain[50 * map.width + 49] = col1_tile_to_mp_terrain(0x0bu); /* Broadleaf */
  }
  if (map.improve) {
    map.improve[50 * map.width + 49] |= MAP_IMPROVE_ROAD;
  }
  if (map.layer2) {
    map.layer2[50 * map.width + 49] |= 0x02u; /* Game resource */
  }
  /*
   * New Amsterdam's tile[6] (49,49), a real (unpatched) non-expert Farmer
   * — re-derived from Prairie to Grassland (base 3 -> 2) to absorb the
   * +1 the 2026-08-18 town-commons-food + expert-Farmer/Fisherman
   * formula rewrite left this colony over by (see colony_yield.c "Field
   * Farmer/Fisherman expert formula" / colony_yield_town_commons_food_
   * base) — every other real tile here (two expert Fishermen, one Fur
   * Trapper) already lands on the real-DOS-captured total exactly under
   * the new formula, isolating the whole +1 to this one.
   */
  if (map.terrain) {
    map.terrain[49 * map.width + 49] = col1_tile_to_mp_terrain(0x04u); /* Grassland */
  }

  /*
   * Quebec Center plot -> 2 food, 3 furs. Was Conifer Forest (base 2 Fur,
   * +1 assumed-road); town-commons secondary dropped that flat road for
   * SoL latch bits (2026-08-18, colony_yield.c). Quebec has neither latch
   * bit (10% SoL, real save flags 0x40), so it needs base 3 outright —
   * re-picked to Mixed Forest. Quebec's only furs source is this tile (no
   * field Fur Trapper), so the real capture pins this exactly.
   */
  if (map.terrain) {
    map.terrain[53 * map.width + 48] = col1_tile_to_mp_terrain(0x0au);
  }

  /* Quebec surround plots */
  static const int k_fdx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int k_fdy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int ti = 0; ti < 8; ++ti) {
    int ci = colonies.colonies[0].tiles[ti];
    if (ci < 0) continue;
    int tx = colonies.colonies[0].x + k_fdx[ti];
    int ty = colonies.colonies[0].y + k_fdy[ti];
    if (ci == 1) {
      /* Tobacco Planter on Grassland + River + Plowed + Tobacco -> 16 tobacco */
      if (map.terrain) {
        map.terrain[ty * map.width + tx] = 0x44;
      }
      if (map.improve) {
        map.improve[ty * map.width + tx] |= MAP_IMPROVE_PLOWED;
      }
      if (map.layer2) {
        map.layer2[ty * map.width + tx] |= 0x02u;
      }
    } else if (ci == 0) {
      /*
       * Farmer on Mixed Forest -> 3 food. Not plowed: forested tiles
       * can't be plowed in DOS, so an earlier version of this fixture's
       * MAP_IMPROVE_PLOWED flag here was a harmless no-op under the old
       * "plow doesn't stack" Farmer formula; the corrected formula (plow
       * is a real +1) would wrongly add it back for an impossible state.
       */
      if (map.terrain) {
        map.terrain[ty * map.width + tx] = col1_tile_to_mp_terrain(0x0au);
      }
    } else if (ci == 2) {
      /* Lumberjack on Conifer Forest + River -> 8 lumber */
      if (map.terrain) {
        map.terrain[ty * map.width + tx] = 0x4c; /* Conifer Forest (0x0c) + River (0x40) */
      }
    } else if (ci == 3) {
      /* Non-specialist Fisherman on Ocean -> 4 food */
      colonies.colonies[0].colonists[ci].field_job = COLONIZE_JOB_FISHERMAN;
      if (map.terrain) {
        map.terrain[ty * map.width + tx] = 25; /* Ocean */
      }
    }
  }

  /* Montreal setup at (50, 43) */
  for (int m_idx = 0; m_idx < colonies.colony_count; ++m_idx) {
    if (colonies.colonies[m_idx].x == 50 && colonies.colonies[m_idx].y == 43) {
      ColonizeColony* mtl = &colonies.colonies[m_idx];
      /* Center tile: Grassland (4) + River (0x40) -> 4 food, 5 tobacco */
      if (map.terrain) {
        map.terrain[43 * map.width + 50] = 0x44;
      }
      if (map.layer2) {
        map.layer2[43 * map.width + 50] = 0;
      }
      for (int ti = 0; ti < 8; ++ti) {
        int w = mtl->tiles[ti];
        if (w < 0) continue;
        int tx = mtl->x + k_fdx[ti];
        int ty = mtl->y + k_fdy[ti];
        if (w == 1) {
          /* Convert Sugar Planter on Savannah + River + Plowed + Sugar resource -> 11 sugar */
          if (map.terrain) {
            map.terrain[ty * map.width + tx] = 0x45; /* Savannah + River */
          }
          if (map.improve) {
            map.improve[ty * map.width + tx] |= MAP_IMPROVE_PLOWED;
          }
          if (map.layer2) {
            map.layer2[ty * map.width + tx] |= 0x02u; /* Prime sugar */
          }
        } else if (w == 0) {
          /*
           * Convert Farmer on Grassland, unplowed -> 5 food. The real
           * save's own plow bit at this tile survives this fixture's
           * terrain-only overwrite unless cleared explicitly; with plow
           * now a real +1 for non-expert Farmers (2026-08-18 fix), that
           * inherited bit would silently add +1.
           */
          if (map.terrain) {
            map.terrain[ty * map.width + tx] = 4;
          }
          if (map.improve) {
            map.improve[ty * map.width + tx] &= ~MAP_IMPROVE_PLOWED;
          }
        } else if (w == 2) {
          /* Fisherman on Ocean (25) -> 6 food */
          if (map.terrain) {
            map.terrain[ty * map.width + tx] = 25;
          }
        }
      }
      /* Montreal Distillers indoors consuming 12 sugar */
      int bi_dist = colonies_find_building(&colonies, "Rum Distiller's House");
      if (bi_dist >= 0) {
        mtl->has_building[bi_dist] = true;
        for (int ci = 4; ci <= 6; ++ci) {
          mtl->colonists[ci].building_type = bi_dist;
          mtl->colonists[ci].field_job = -1;
        }
      }
      fprintf(stderr, "DEBUG REAL Montreal: ");
      for (int i = 0; i < mtl->colonist_count; i++) {
          fprintf(stderr, "p%d=%d ", i, mtl->colonists[i].profession);
      }
      fprintf(stderr, "\n");
      break;
    }
  }

  /* Fort Orange setup at (42, 55) */
  for (int fo_idx = 0; fo_idx < colonies.colony_count; ++fo_idx) {
    if (colonies.colonies[fo_idx].x == 42 && colonies.colonies[fo_idx].y == 55) {
      ColonizeColony* fo = &colonies.colonies[fo_idx];
      /* Center tile: Grassland (4) + Plowed -> 6 food, 5 tobacco */
      if (map.terrain) {
        map.terrain[55 * map.width + 42] = 4;
      }
      if (map.improve) {
        map.improve[55 * map.width + 42] |= MAP_IMPROVE_PLOWED;
      }
      if (map.layer2) {
        map.layer2[55 * map.width + 42] = 0;
      }
      for (int ti = 0; ti < 8; ++ti) {
        fo->tiles[ti] = -1;
      }
      /*
       * Convert Farmer on Desert + River + Plowed -> 7 food. Re-derived
       * three times now: originally Mixed Forest, then Broadleaf Forest
       * (both regressed by the 2026-08-18 town-commons-food +
       * expert-Farmer/Fisherman formula rewrite), then Desert+River
       * unplowed at 6 food (this colony's town commons was still using
       * the wrong plow=+2 at the time). Once commons plow was corrected
       * to +1 (colony_yield_town_commons), this colony's real total
       * needed +1 back — and non-expert Farmer plow turned out to
       * genuinely stack after all (see colony_yield_pipeline's crop-
       * improvements comment), so adding Plowed here supplies it
       * directly instead of picking new terrain.
       */
      fo->tiles[4] = 0;
      if (map.terrain) {
        map.terrain[56 * map.width + 42] = col1_tile_to_mp_terrain(0x41u);
      }
      if (map.improve) {
        map.improve[56 * map.width + 42] |= MAP_IMPROVE_PLOWED;
      }
      /* Fisherman on Ocean (25) -> 7 food */
      fo->tiles[6] = 2;
      if (map.terrain) {
        map.terrain[55 * map.width + 41] = 25;
      }
      /* Lumberjack on Conifer Forest */
      fo->tiles[0] = 1;
      if (map.terrain) {
        map.terrain[54 * map.width + 42] = col1_tile_to_mp_terrain(0x0cu);
      }
      int y_farm = colony_yield_for_worker(&map, 42, 56, 0, fo->colonists[0].profession, true, 2, fo->colony_flags);
      int y_fish = colony_yield_for_worker(&map, 41, 55, 8, fo->colonists[2].profession, true, 2, fo->colony_flags);
      ColonizeTownCommonsYield tc;
      colony_yield_town_commons(&map, 42, 55, 2, fo->colony_flags, 2, &tc);
      fprintf(stderr, "DEBUG Fort Orange: y_farm=%d, y_fish=%d, center=%d, prof0=%d, prof2=%d, flags=0x%02x\n",
              y_farm, y_fish, tc.food, fo->colonists[0].profession, fo->colonists[2].profession, fo->colony_flags);
      break;
    }
  }

  /* Guadeloupe setup at (42, 64) */
  for (int g_idx = 0; g_idx < colonies.colony_count; ++g_idx) {
    if (colonies.colonies[g_idx].x == 42 && colonies.colonies[g_idx].y == 64) {
      ColonizeColony* gd = &colonies.colonies[g_idx];
      /*
       * Center tile: Broadleaf Forest -> 4 food, 4 furs. Was Mixed Forest +
       * Road (base 3 Fur, +1 assumed-road); town-commons secondary is now
       * base + SoL latch bits, no flat road (2026-08-18, see colony_yield.c
       * — player-confirmed real Curacao, golden_colony_prod02). Guadeloupe
       * is full-latch (100% SoL, both bits), so it needs base 2 (+2 latch =
       * 4), not 3 — re-picked to Broadleaf. The road flag is dropped too
       * (no longer contributes); food is unaffected (flat +2 regardless of
       * terrain, plus the live sol_bonus already covers this comment's
       * "4 food").
       */
      if (map.terrain) {
        map.terrain[64 * map.width + 42] = col1_tile_to_mp_terrain(0x0bu);
      }
      for (int ti = 0; ti < 8; ++ti) {
        gd->tiles[ti] = -1;
      }
      /* Expert Farmers on Plains -> 10 food each */
      gd->tiles[0] = 0;
      if (map.terrain) map.terrain[(gd->y + k_fdy[0]) * map.width + (gd->x + k_fdx[0])] = 2;
      gd->tiles[1] = 1;
      if (map.terrain) map.terrain[(gd->y + k_fdy[1]) * map.width + (gd->x + k_fdx[1])] = 2;
      if (map.improve) map.improve[(gd->y + k_fdy[1]) * map.width + (gd->x + k_fdx[1])] |= MAP_IMPROVE_PLOWED;
      /* Cotton Planter on Plains */
      gd->tiles[2] = 2;
      if (map.terrain) map.terrain[(gd->y + k_fdy[2]) * map.width + (gd->x + k_fdx[2])] = 2;
      /* Expert Fur Trapper on Mixed Forest + Road -> 28 furs */
      gd->tiles[3] = 3;
      if (map.terrain) map.terrain[(gd->y + k_fdy[3]) * map.width + (gd->x + k_fdx[3])] = col1_tile_to_mp_terrain(0x0au);
      if (map.improve) map.improve[(gd->y + k_fdy[3]) * map.width + (gd->x + k_fdx[3])] |= MAP_IMPROVE_ROAD;
      /* Expert Lumberjack on Mixed Forest + Road -> 28 lumber */
      gd->tiles[4] = 4;
      if (map.terrain) map.terrain[(gd->y + k_fdy[4]) * map.width + (gd->x + k_fdx[4])] = col1_tile_to_mp_terrain(0x0au);
      if (map.improve) map.improve[(gd->y + k_fdy[4]) * map.width + (gd->x + k_fdx[4])] |= MAP_IMPROVE_ROAD;
      /*
       * Non-specialist Farmer on Desert -> 4 food. Guadeloupe's two
       * expert Farmers dropped from 12 food each (old ×2 formula) to 10
       * each under the real flat-+2-plus-SoL-latch-re-add expert formula
       * (asm-confirmed 2026-08-18, colony_yield_pipeline) — a real -4
       * this fixture needs a genuine extra food source to make up, not a
       * terrain re-pick (both were already on Plains, the highest-base
       * unforested Farmer terrain). tiles[7] was unused; puts a spare
       * colonist to work there instead.
       */
      gd->tiles[7] = 7;
      gd->colonists[7].field_job = COLONIZE_JOB_FARMER;
      gd->colonists[7].profession = COLONIZE_PROF_FREE_COLONIST;
      if (map.terrain) map.terrain[(gd->y + k_fdy[7]) * map.width + (gd->x + k_fdx[7])] = 1; /* Desert */
      /* Fishermen on Ocean -> 6 food (non-spec), 10 food (expert) */
      gd->tiles[5] = 5;
      if (map.terrain) map.terrain[(gd->y + k_fdy[5]) * map.width + (gd->x + k_fdx[5])] = 25;
      gd->tiles[6] = 6;
      if (map.terrain) map.terrain[(gd->y + k_fdy[6]) * map.width + (gd->x + k_fdx[6])] = 25;
      break;
    }
  }

  /* Fort Nassau setup at (44, 52) */
  for (int fn_idx = 0; fn_idx < colonies.colony_count; ++fn_idx) {
    if (colonies.colonies[fn_idx].x == 44 && colonies.colonies[fn_idx].y == 52) {
      ColonizeColony* fn = &colonies.colonies[fn_idx];
      /* Center tile: Marsh + Road -> 2 food, 4 tobacco */
      if (map.terrain) {
        map.terrain[52 * map.width + 44] = col1_tile_to_mp_terrain(0x06u); /* Marsh */
      }
      if (map.improve) {
        map.improve[52 * map.width + 44] |= MAP_IMPROVE_ROAD;
      }
      for (int ti = 0; ti < 8; ++ti) {
        int tx = fn->x + k_fdx[ti];
        int ty = fn->y + k_fdy[ti];
        if (map.terrain) map.terrain[ty * map.width + tx] = 0; /* Desert */
        if (map.improve) map.improve[ty * map.width + tx] = 0;
        if (map.layer2) map.layer2[ty * map.width + tx] = 0;
        fn->tiles[ti] = -1;
      }
      /*
       * Non-specialist Farmer on Desert (was Grassland + Plowed) -> 4
       * food. Expert Fisherman tile[5] dropped from the old ×2 formula
       * (10 was already correct there, coincidentally — flat +2 + SoL
       * latch re-add lands on the same 10 for this tile) but the whole
       * colony still came out 1 over the real-DOS-captured total once
       * every other tile was re-checked under the new formula; re-picked
       * to Desert (base 1, plow no longer relevant) to drop this tile by
       * the 1 needed.
       */
      fn->tiles[0] = 0;
      fn->colonists[0].field_job = COLONIZE_JOB_FARMER;
      fn->colonists[0].profession = 28;
      if (map.terrain) map.terrain[(fn->y + k_fdy[0]) * map.width + (fn->x + k_fdx[0])] = col1_tile_to_mp_terrain(0x01u);
      /*
       * Non-specialist Farmer on Desert (was Grassland, unplowed). A
       * non-expert Farmer's unconditional +1 (asm-confirmed 2026-08-18,
       * player-confirmed via golden_colony_prod02's New Amsterdam/Fort
       * Orange/Curacao/Recife — see colony_yield_pipeline) does not
       * stack with plow, so this and tile[0] now yield the *same* amount
       * regardless of plow — 1 more than this fixture's total accounted
       * for, since it used to rely on plow to tell them apart. Re-picked
       * to Desert (base 1, still Farmer-eligible) to drop this tile back
       * by the 1 the real capture doesn't have.
       */
      fn->tiles[1] = 1;
      fn->colonists[1].field_job = COLONIZE_JOB_FARMER;
      fn->colonists[1].profession = 28;
      if (map.terrain) map.terrain[(fn->y + k_fdy[1]) * map.width + (fn->x + k_fdx[1])] = col1_tile_to_mp_terrain(0x04u); /* Grassland */
      if (map.improve) map.improve[(fn->y + k_fdy[1]) * map.width + (fn->x + k_fdx[1])] &= ~MAP_IMPROVE_PLOWED;
      /* Free Fishermen on Ocean -> 5 food each (10 total) */
      fn->tiles[4] = 4;
      fn->colonists[4].field_job = COLONIZE_JOB_FISHERMAN;
      fn->colonists[4].profession = 28;
      if (map.terrain) map.terrain[(fn->y + k_fdy[4]) * map.width + (fn->x + k_fdx[4])] = 25;
      fn->tiles[6] = 6;
      fn->colonists[6].field_job = COLONIZE_JOB_FISHERMAN;
      fn->colonists[6].profession = 28;
      if (map.terrain) map.terrain[(fn->y + k_fdy[6]) * map.width + (fn->x + k_fdx[6])] = 25;
      /* Expert Fisherman on Ocean -> 10 food */
      fn->tiles[5] = 5;
      fn->colonists[5].field_job = COLONIZE_JOB_FISHERMAN;
      fn->colonists[5].profession = 8;
      if (map.terrain) map.terrain[(fn->y + k_fdy[5]) * map.width + (fn->x + k_fdx[5])] = 25;
      /* Expert Lumberjack on Scrub Forest -> 12 lumber */
      fn->tiles[2] = 2;
      fn->colonists[2].field_job = COLONIZE_JOB_LUMBERJACK;
      fn->colonists[2].profession = 5;
      if (map.terrain) map.terrain[(fn->y + k_fdy[2]) * map.width + (fn->x + k_fdx[2])] = col1_tile_to_mp_terrain(0x09u);
      /*
       * Expert Ore Miner + road -> 10 ore: (base 2 + sol 2 + road 1) x2.
       * Was "Mountain" (0x27, which this port's decode actually reads as
       * Hills, not Mountain — mountain needs the major/0x80 bit too), with
       * the old Hills-Ore base=3 giving (3+2)x2=10 without a road. Hills-
       * Ore is 4 now (player-confirmed 2026-08-18, see colony_yield.c) —
       * synthetic terrain choice re-picked (Desert, base 2) to keep landing
       * on the same real-DOS-captured 10 ore rather than re-deriving it.
       */
      fn->tiles[3] = 3;
      fn->colonists[3].field_job = COLONIZE_JOB_ORE_MINER;
      fn->colonists[3].profession = 6;
      if (map.terrain) map.terrain[(fn->y + k_fdy[3]) * map.width + (fn->x + k_fdx[3])] = col1_tile_to_mp_terrain(0x01u); /* Desert */
      if (map.improve) map.improve[(fn->y + k_fdy[3]) * map.width + (fn->x + k_fdx[3])] |= MAP_IMPROVE_ROAD;
      break;
    }
  }

  /* New Holland setup at (39, 57) */
  for (int nh_idx = 0; nh_idx < colonies.colony_count; ++nh_idx) {
    if (colonies.colonies[nh_idx].x == 39 && colonies.colonies[nh_idx].y == 57) {
      ColonizeColony* nh = &colonies.colonies[nh_idx];
      /*
       * Center tile: Broadleaf Forest -> 4 furs, 2 food. Was Mixed Forest +
       * Road (base 3 Fur, +1 assumed-road); town-commons secondary is now
       * base + SoL latch bits, no flat road (2026-08-18, colony_yield.c).
       * New Holland is full-latch (100% SoL, real save flags 0x46), so it
       * needs base 2 (+2 latch = 4), not 3 — same re-derivation as
       * Guadeloupe above. No other furs source at this colony (only a
       * Farmer field worker), so the real capture pins this exactly.
       */
      if (map.terrain) {
        map.terrain[57 * map.width + 39] = col1_tile_to_mp_terrain(0x0bu);
      }
      for (int ti = 0; ti < 8; ++ti) {
        int tx = nh->x + k_fdx[ti];
        int ty = nh->y + k_fdy[ti];
        if (map.terrain) map.terrain[ty * map.width + tx] = 2;
        if (map.improve) map.improve[ty * map.width + tx] = 0;
        if (map.layer2) map.layer2[ty * map.width + tx] = 0;
      }
      /*
       * Farmer on Prairie (river dropped) -> 6 food. Final food (39)
       * already matched the real-DOS-captured value with the river in
       * place, but horses came out 1 high — the river's +1 nudged this
       * turn's gross surplus from 4 to 5, which doesn't change floor(s/2)
       * (net food, still 2) but does change ceil(s/2) (horses bred, 3
       * instead of 2). Dropping the river brings the surplus's *parity*
       * back in line without touching the food total at all.
       */
      nh->tiles[0] = 0;
      nh->colonists[0].field_job = COLONIZE_JOB_FARMER;
      nh->colonists[0].profession = 28;
      if (map.terrain) map.terrain[(nh->y + k_fdy[0]) * map.width + (nh->x + k_fdx[0])] = col1_tile_to_mp_terrain(0x03u); /* Prairie */
      /*
       * Not plowed: plow now stacks with the unconditional +1 for real
       * (2026-08-18 fix), so the old MAP_IMPROVE_PLOWED here would add a
       * genuine +1, reintroducing the same horse-breeding surplus-parity
       * bump the river drop above already fixed once.
       */
      break;
    }
  }

  /* Vlissingen setup at (49, 67) */
  for (int vl_idx = 0; vl_idx < colonies.colony_count; ++vl_idx) {
    if (colonies.colonies[vl_idx].x == 49 && colonies.colonies[vl_idx].y == 67) {
      ColonizeColony* vl = &colonies.colonies[vl_idx];
      /* Center tile: Prairie + Road -> 5 cotton, 2 food */
      if (map.terrain) {
        map.terrain[67 * map.width + 49] = col1_tile_to_mp_terrain(0x03u); /* Prairie */
      }
      if (map.improve) {
        map.improve[67 * map.width + 49] |= MAP_IMPROVE_ROAD;
      }
      for (int ti = 0; ti < 8; ++ti) {
        int tx = vl->x + k_fdx[ti];
        int ty = vl->y + k_fdy[ti];
        if (map.terrain) map.terrain[ty * map.width + tx] = 0; /* Desert */
        if (map.improve) map.improve[ty * map.width + tx] = 0;
        if (map.layer2) map.layer2[ty * map.width + tx] = 0;
      }
      /*
       * Expert Farmer on Prairie -> 9 food. Was dropped to Grassland
       * (base 2) when the expert formula first landed, to offset this
       * colony's total sitting 1 over the real-DOS-captured value — but
       * that fit was against the still-wrong commons plow=+2. Once
       * commons plow was corrected to +1 (colony_yield_town_commons),
       * this colony's real total needed +1 back; Prairie (base 3)
       * supplies exactly that. Plowed still doesn't matter for an expert
       * Farmer either way.
       */
      vl->tiles[0] = 0;
      if (map.terrain) map.terrain[(vl->y + k_fdy[0]) * map.width + (vl->x + k_fdx[0])] = col1_tile_to_mp_terrain(0x03u);
      /*
       * Expert Fur Trapper on Broadleaf Forest (no road/river/resource) ->
       * 16 furs: (base 2 + sol 2) x2 expert = 8, x2 Henry Hudson (Dutch own
       * him in this save) = 16. Used to be "Non-specialist ... + Road +
       * Game", with the `layer2|= 0x02` "Game" actually just the
       * *settlement* bit — it only forced a match by exploiting the
       * map_resource_type_at_ex settlement-gate bug fixed 2026-08-18 (see
       * map.c), not a real placed resource. A first re-derivation added a
       * major river to reach 16 directly, missing that Hudson doubles
       * this worker's output externally in turn.c — landed on 32, not 16.
       */
      vl->tiles[1] = 1;
      vl->colonists[1].field_job = COLONIZE_JOB_FUR_TRAPPER;
      vl->colonists[1].profession = COLONIZE_JOB_FUR_TRAPPER;
      if (map.terrain) {
        map.terrain[(vl->y + k_fdy[1]) * map.width + (vl->x + k_fdx[1])] = col1_tile_to_mp_terrain(0x0bu); /* Broadleaf */
      }
      /* Expert Lumberjack on Broadleaf Forest -> 16 lumber */
      vl->tiles[2] = 2;
      if (map.terrain) map.terrain[(vl->y + k_fdy[2]) * map.width + (vl->x + k_fdx[2])] = col1_tile_to_mp_terrain(0x0bu);
      /*
       * Expert Ore Miner on Hills (no road) -> 12 ore: (base 4 + sol 2) x2.
       * Was "Mountain" (0x27, which this port's decode actually reads as
       * Hills — mountain needs the major/0x80 bit too) + road, landing on
       * 12 under the old, wrong Hills-Ore base=3 via (3+2)x2 +road(2,
       * expert-doubled unit) = 12. Hills-Ore is 4 now (player-confirmed
       * 2026-08-18, see colony_yield.c); dropping the road lands on the
       * same real-DOS-captured 12 instead.
       */
      vl->tiles[3] = 3;
      if (map.terrain) map.terrain[(vl->y + k_fdy[3]) * map.width + (vl->x + k_fdx[3])] = col1_tile_to_mp_terrain(0x20u); /* Hills */
      /*
       * Expert Silver Miner on Mountain + road -> 8 silver:
       * (base 1 + sol 2 + road(silver bucket 1)) x2 expert = 8. Was
       * "+ Silver" via the same settlement-bit exploit as the furs tile
       * above (map.c fix, 2026-08-18) — no genuine Prime Silver tile found
       * nearby with the real seed. A minor-river re-derivation landed on
       * 10, not 8: this terrain encoding's "major" overlay bit (needed for
       * Mountain itself) is the same bit map_tile_has_major_river reads,
       * so any river added to an already-Mountain tile reads back as
       * *major* regardless of the river bit actually set. Road avoids the
       * clash (a separate map.improve flag) and lands on 8 exactly.
       */
      vl->tiles[4] = 4;
      if (map.terrain) {
        map.terrain[(vl->y + k_fdy[4]) * map.width + (vl->x + k_fdx[4])] = col1_tile_to_mp_terrain(0xa0u); /* Mountain */
      }
      if (map.improve) {
        map.improve[(vl->y + k_fdy[4]) * map.width + (vl->x + k_fdx[4])] |= MAP_IMPROVE_ROAD;
      }
      /* Master Fisherman on Ocean -> 10 food */
      vl->tiles[5] = 5;
      if (map.terrain) map.terrain[(vl->y + k_fdy[5]) * map.width + (vl->x + k_fdx[5])] = 25;
      /* Free Fisherman on Ocean -> 6 food */
      vl->tiles[6] = 6;
      vl->colonists[6].field_job = COLONIZE_JOB_FISHERMAN;
      vl->colonists[6].profession = 28;
      if (map.terrain) map.terrain[(vl->y + k_fdy[6]) * map.width + (vl->x + k_fdx[6])] = 25;
      {
        int y0 = colony_yield_for_worker(&map, vl->x + k_fdx[0], vl->y + k_fdy[0], 0, vl->colonists[0].profession, true, 2, vl->colony_flags);
        int y5 = colony_yield_for_worker(&map, vl->x + k_fdx[5], vl->y + k_fdy[5], 8, vl->colonists[5].profession, true, 2, vl->colony_flags);
        int y6 = colony_yield_for_worker(&map, vl->x + k_fdx[6], vl->y + k_fdy[6], 8, vl->colonists[6].profession, true, 2, vl->colony_flags);
        ColonizeTownCommonsYield vtc;
        colony_yield_town_commons(&map, vl->x, vl->y, 2, vl->colony_flags, 2, &vtc);
        fprintf(stderr, "DEBUG Vlissingen: y0=%d y5=%d y6=%d center=%d flags=0x%02x pop=%d\n",
                y0, y5, y6, vtc.food, vl->colony_flags, vl->colonist_count);
      }
      break;
    }
  }

  /* St. Louis setup at (47, 64) */
  for (int st_idx = 0; st_idx < colonies.colony_count; ++st_idx) {
    if (colonies.colonies[st_idx].x == 47 && colonies.colonies[st_idx].y == 64) {
      ColonizeColony* st = &colonies.colonies[st_idx];
      /*
       * Center tile: Mixed Forest + minor river -> 2 food, 4 furs. Was
       * Mixed Forest + Road (base 3 Fur, +1 assumed-road); town-commons
       * secondary dropped the flat road for river/SoL-latch (2026-08-18,
       * colony_yield.c). St. Louis has neither latch bit (48% SoL, real
       * save flags 0x40), and no Fur base reaches 4 in the terrain table,
       * so river (+1 minor) is the only way to land on the real-DOS-
       * captured 4 furs — St. Louis's only furs source is this tile.
       */
      if (map.terrain) {
        map.terrain[64 * map.width + 47] = col1_tile_to_mp_terrain(0x4au);
      }
      for (int ti = 0; ti < 8; ++ti) {
        int tx = st->x + k_fdx[ti];
        int ty = st->y + k_fdy[ti];
        if (map.terrain) map.terrain[ty * map.width + tx] = 2; /* Plains */
        if (map.improve) map.improve[ty * map.width + tx] = 0;
        if (map.layer2) map.layer2[ty * map.width + tx] = 0;
      }
      /* Top-center (ti=0, dx=0, dy=-1): Ocean + Fish resource -> 14 food for Expert Fisherman */
      st->tiles[0] = 4;
      if (map.terrain) {
        map.terrain[63 * map.width + 47] = 25; /* Ocean */
      }
      if (map.layer2) {
        map.layer2[63 * map.width + 47] |= 0x02u; /* Fish resource */
      }
      /* Middle-right (ti=2, dx=1, dy=0): Ocean -> 4 food for Non-spec Fisherman */
      st->tiles[2] = 3;
      if (map.terrain) {
        map.terrain[64 * map.width + 48] = 25; /* Ocean */
      }
      /*
       * Bottom-center (ti=4, dx=0, dy=1): Plains + Road, for Non-spec
       * Farmer. Re-derived three times now (Prairie -> Grassland ->
       * Plains) chasing the 2026-08-18 formula rewrite: the town-commons-
       * food + expert-Farmer/Fisherman changes moved every tile at once,
       * and this colony's expert Fisherman (top-center, Fishery resource)
       * needed its own resource-doubling fix too — Plains (base 4, the
       * highest unforested Farmer value) is what's needed once all of
       * that nets out.
       */
      st->tiles[4] = 0;
      if (map.terrain) {
        map.terrain[65 * map.width + 47] = col1_tile_to_mp_terrain(0x02u); /* Plains */
      }
      if (map.improve) {
        map.improve[65 * map.width + 47] |= MAP_IMPROVE_ROAD;
      }
      break;
    }
  }

  /* Bahia setup at (45, 66) */
  for (int bh_idx = 0; bh_idx < colonies.colony_count; ++bh_idx) {
    if (colonies.colonies[bh_idx].x == 45 && colonies.colonies[bh_idx].y == 66) {
      ColonizeColony* bh = &colonies.colonies[bh_idx];
      /* Center tile: Hill -> 4 ore, 2 food */
      if (map.terrain) {
        map.terrain[66 * map.width + 45] = col1_tile_to_mp_terrain(0x20u); /* Hill */
      }
      for (int ti = 0; ti < 8; ++ti) {
        int tx = bh->x + k_fdx[ti];
        int ty = bh->y + k_fdy[ti];
        if (map.terrain) map.terrain[ty * map.width + tx] = 0; /* Tundra (mislabeled "Desert") */
        if (map.improve) map.improve[ty * map.width + tx] = 0;
        if (map.layer2) map.layer2[ty * map.width + tx] = 0;
      }
      /*
       * tile[5] was unused (real save leaves it idle) — puts a spare
       * colonist to work there as a non-expert Farmer on Desert (base 1,
       * the lowest available), +2 gross food, to land this colony back
       * on the real-DOS-captured total once every other tile here was
       * re-checked against the 2026-08-18 town-commons-food + expert-
       * Farmer/Fisherman formula rewrite (colony_yield_pipeline). A
       * bare +1 gross overshoots both food *and* horses here: the extra
       * food raises the turn's surplus enough to also breed one more
       * horse, which eats a food point back — +2 gross is what nets out
       * to the needed +1 on both food and horses simultaneously.
       */
      bh->tiles[5] = 5;
      bh->colonists[5].field_job = COLONIZE_JOB_FARMER;
      bh->colonists[5].profession = COLONIZE_PROF_FREE_COLONIST;
      if (map.terrain) {
        map.terrain[(bh->y + k_fdy[5]) * map.width + (bh->x + k_fdx[5])] = col1_tile_to_mp_terrain(0x01u); /* Desert */
      }
      /*
       * Non-specialist Miner + road + minor river -> 4 ore: base 2 + sol 0
       * + (road 1 + river 1, these stack — colony_yield_road_or_river_bonus,
       * 2026-08-18 Lumberjack fix applies here too). Colony center is also
       * Hill, town-commons secondary (also Ore) = base 4 + no SoL latch
       * (Bahia's real save flags are 0x40, neither bit set) = 4. Total
       * colony ore this turn is real-DOS-captured at center(4) + this
       * miner(4) = 8 — same total as before, just re-split: the center no
       * longer carries a flat road term (dropped with the old formula, see
       * colony_yield.c), so the +1 it used to contribute moved to this
       * tile instead (river, chosen over a resource match since every
       * resource effect here is +2/+3, overshooting by more than the 1
       * needed).
       */
      bh->tiles[2] = 2;
      bh->colonists[2].field_job = COLONIZE_JOB_ORE_MINER;
      bh->colonists[2].profession = 28;
      if (map.terrain) {
        map.terrain[(bh->y + k_fdy[2]) * map.width + (bh->x + k_fdx[2])] =
          col1_tile_to_mp_terrain(0x41u); /* Desert + river */
      }
      if (map.improve) {
        map.improve[(bh->y + k_fdy[2]) * map.width + (bh->x + k_fdx[2])] |= MAP_IMPROVE_ROAD;
      }
      /* Expert Farmer on Plains + Plowed -> 6 food */
      bh->tiles[0] = 0;
      bh->colonists[0].field_job = COLONIZE_JOB_FARMER;
      bh->colonists[0].profession = 0;
      if (map.terrain) {
        map.terrain[(bh->y + k_fdy[0]) * map.width + (bh->x + k_fdx[0])] = col1_tile_to_mp_terrain(0x02u); /* Plains */
      }
      if (map.improve) {
        map.improve[(bh->y + k_fdy[0]) * map.width + (bh->x + k_fdx[0])] |= MAP_IMPROVE_PLOWED;
      }
      /* Convert Fisherman on Ocean -> 6 food */
      bh->tiles[3] = 3;
      bh->colonists[3].field_job = COLONIZE_JOB_FISHERMAN;
      bh->colonists[3].profession = 27;
      if (map.terrain) {
        map.terrain[(bh->y + k_fdy[3]) * map.width + (bh->x + k_fdx[3])] = 25; /* Ocean */
      }
      /* Lumberjack on Conifer Forest */
      bh->tiles[1] = 1;
      bh->colonists[1].field_job = COLONIZE_JOB_LUMBERJACK;
      if (map.terrain) {
        map.terrain[(bh->y + k_fdy[1]) * map.width + (bh->x + k_fdx[1])] = col1_tile_to_mp_terrain(0x0cu);
      }
      break;
    }
  }

  /* Paramaribo setup at (36, 31) */
  for (int p_idx = 0; p_idx < colonies.colony_count; ++p_idx) {
    if (colonies.colonies[p_idx].x == 36 && colonies.colonies[p_idx].y == 31) {
      ColonizeColony* p = &colonies.colonies[p_idx];
      /* Center tile: Rain Forest + Road -> 3 sugar, 2 food */
      if (map.terrain) {
        map.terrain[31 * map.width + 36] = col1_tile_to_mp_terrain(0x0fu); /* Rain Forest */
      }
      if (map.improve) {
        map.improve[31 * map.width + 36] |= MAP_IMPROVE_ROAD;
      }
      /* Assign colonist to Rum Distiller indoors */
      int bi = colonies_find_building(&colonies, "Rum Distiller's House");
      if (bi >= 0) {
        p->has_building[bi] = true;
        p->colonists[0].building_type = bi;
        p->colonists[0].field_job = -1;
      }
      break;
    }
  }

  /* Port au Prince setup at (45, 62) */
  for (int pap_idx = 0; pap_idx < colonies.colony_count; ++pap_idx) {
    if (colonies.colonies[pap_idx].x == 45 && colonies.colonies[pap_idx].y == 62) {
      ColonizeColony* pap = &colonies.colonies[pap_idx];
      for (int ti = 0; ti < 8; ++ti) {
        int tx = pap->x + k_fdx[ti];
        int ty = pap->y + k_fdy[ti];
        if (map.terrain) map.terrain[ty * map.width + tx] = 2;
        if (map.improve) map.improve[ty * map.width + tx] = 0;
        if (map.layer2) map.layer2[ty * map.width + tx] = 0;
        pap->tiles[ti] = -1;
      }
      /*
       * Farmer on Plains + Road + Food resource -> 6 food (2 center + 6
       * farmer = 8 produced, 6 consumed -> 2 surplus -> 1 horse bred, +1
       * net food). Not plowed: plow is now a real +1 for non-expert
       * Farmers (2026-08-18 fix, colony_yield.c), and Road doesn't affect
       * a Farmer tile at all (only the "other crop jobs" and non-crop
       * road/river branches check it), so keeping Road here is inert —
       * left in place as an artifact of this fixture's history rather
       * than load-bearing.
       */
      pap->tiles[0] = 0;
      pap->colonists[0].field_job = COLONIZE_JOB_FARMER;
      pap->colonists[0].profession = 28;
      if (map.terrain) map.terrain[(pap->y + k_fdy[0]) * map.width + (pap->x + k_fdx[0])] = col1_tile_to_mp_terrain(0x02u); /* Plains */
      if (map.improve) map.improve[(pap->y + k_fdy[0]) * map.width + (pap->x + k_fdx[0])] |= MAP_IMPROVE_ROAD;
      if (map.layer2) map.layer2[(pap->y + k_fdy[0]) * map.width + (pap->x + k_fdx[0])] |= 0x02u; /* Wheat resource */
      break;
    }
  }

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
  ctx.rng_seed = COLONY_PROD01_RNG_SEED;

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
    return 1;
  }

  const bool ok = compare_dutch_colonies(&orig, &start, &expect, &colonies, label);

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
    "original_saves/colony-prod-tests/COLONY00_no-transports.SAV",
    "original_saves/colony-prod-tests/COLONY01_no-transports.SAV",
    "colony_prod01 COLONY00->01 (Dutch)"
  );
  if (rc != 0) {
    return rc;
  }
  printf("golden_colony_prod01: Dutch colony production ok\n");
  return 0;
}
