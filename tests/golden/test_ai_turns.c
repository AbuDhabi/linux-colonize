/*
 * PARKED 2026-08-19 (docs/port_plan.md R0, docs/port_plan.md): DISABLED
 * in CMakeLists.txt. This chases turn-for-turn DOS parity against an AI
 * planner that is still only structurally/T0-T1 ported, not T3 1:1 — every
 * remaining unported/stubbed callee (FUN_41f2_0294 etc.) is a guaranteed
 * future diff here, so a red run means "porting incomplete", not "new
 * regression". Do not chase individual TURN-step diffs to green until the
 * underlying AI transcription is actually done; re-enable
 * (set_tests_properties ... DISABLED FALSE) only then.
 *
 * 2026-08-28: TURN1→2, 2→3, 4→5, 5→6, 6→7 pass; TURN3→4 fails on two Braves
 * (docs/port_plan.md T1.23). Diagnostics: AI_TURNS_ALL=1 runs every step
 * instead of stopping at the first failure, AI_TURNS_ONLY=t runs one step.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai.h"
#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/platform.h"

#define AI_TURNS_VR_SEED 100u

static void print_unit(const char* tag, unsigned i, const ColonizeCol1Unit* u) {
  fprintf(
    stderr,
    "  %s[%u] t=%u n=%u xy=(%u,%u) ord=%u g=(%u,%u) mv=%u tw=%u\n",
    tag,
    i,
    (unsigned)u->type,
    (unsigned)u->nation_id,
    (unsigned)u->x,
    (unsigned)u->y,
    (unsigned)u->orders,
    (unsigned)u->goto_x,
    (unsigned)u->goto_y,
    (unsigned)u->moves,
    (unsigned)u->turns_worked
  );
}

static int find_unit_match(
  const ColonizeCol1Save* got,
  const ColonizeCol1Unit* want,
  bool* used
) {
  for (unsigned i = 0; i < got->head.unit_count; ++i) {
    if (used[i]) {
      continue;
    }
    const ColonizeCol1Unit* u = &got->unit[i];
    if (u->type == want->type && u->nation_id == want->nation_id && u->x == want->x &&
        u->y == want->y) {
      return (int)i;
    }
  }
  return -1;
}

static bool compare_ai_state(
  const ColonizeCol1Save* got,
  const ColonizeCol1Save* exp,
  const char* step_label
) {
  bool ok = true;
  if (got->head.year != exp->head.year || got->head.autumn != exp->head.autumn ||
      got->head.turn != exp->head.turn) {
    fprintf(
      stderr,
      "%s calendar got year=%u autumn=%u turn=%u expected %u/%u/%u\n",
      step_label,
      got->head.year,
      got->head.autumn,
      got->head.turn,
      exp->head.year,
      exp->head.autumn,
      exp->head.turn
    );
    ok = false;
  }

  for (int n = 0; n < 4; ++n) {
    if (got->nation[n].current_crosses != exp->nation[n].current_crosses ||
        got->nation[n].needed_crosses != exp->nation[n].needed_crosses) {
      fprintf(
        stderr,
        "%s nation[%d] crosses got %u/%u expected %u/%u\n",
        step_label,
        n,
        got->nation[n].current_crosses,
        got->nation[n].needed_crosses,
        exp->nation[n].current_crosses,
        exp->nation[n].needed_crosses
      );
      ok = false;
    }
    if (got->player[n].founded_colonies != exp->player[n].founded_colonies) {
      fprintf(
        stderr,
        "%s nation[%d] founded_colonies got %u expected %u\n",
        step_label,
        n,
        got->player[n].founded_colonies,
        exp->player[n].founded_colonies
      );
      ok = false;
    }
  }

  if (got->head.colony_count != exp->head.colony_count) {
    fprintf(
      stderr,
      "%s colony_count got %u expected %u\n",
      step_label,
      got->head.colony_count,
      exp->head.colony_count
    );
    ok = false;
  } else {
    for (unsigned i = 0; i < exp->head.colony_count; ++i) {
      const ColonizeCol1Colony* g = &got->colony[i];
      const ColonizeCol1Colony* e = &exp->colony[i];
      if (g->x != e->x || g->y != e->y || g->nation_id != e->nation_id ||
          g->population != e->population || g->building_in_production != e->building_in_production ||
          g->hammers != e->hammers || strncmp(g->name, e->name, sizeof(g->name)) != 0) {
        fprintf(
          stderr,
          "%s colony[%u] got '%.*s' n=%u xy=(%u,%u) pop=%u bip=%u hammers=%u\n"
          "             expected '%.*s' n=%u xy=(%u,%u) pop=%u bip=%u hammers=%u\n",
          step_label,
          i,
          (int)sizeof(g->name),
          g->name,
          g->nation_id,
          g->x,
          g->y,
          g->population,
          g->building_in_production,
          g->hammers,
          (int)sizeof(e->name),
          e->name,
          e->nation_id,
          e->x,
          e->y,
          e->population,
          e->building_in_production,
          e->hammers
        );
        ok = false;
        break;
      }
    }
  }

  /* Euro units (nation < 4) and Braves: match by type/nation/xy then check orders/goto. */
  {
    bool* used = calloc(got->head.unit_count ? got->head.unit_count : 1, sizeof(bool));
    if (!used) {
      fprintf(stderr, "%s oom\n", step_label);
      return false;
    }
    int mismatches = 0;
    for (unsigned i = 0; i < exp->head.unit_count; ++i) {
      const ColonizeCol1Unit* e = &exp->unit[i];
      const int gi = find_unit_match(got, e, used);
      if (gi < 0) {
        static int dumped;
        if (!dumped) {
          dumped = 1;
          fprintf(stderr, "%s got euro units:\n", step_label);
          for (unsigned j = 0; j < got->head.unit_count; ++j) {
            if (got->unit[j].nation_id < 4) print_unit("got", j, &got->unit[j]);
          }
        }
        fprintf(stderr, "%s missing unit:\n", step_label);
        print_unit("exp", i, e);
        ok = false;
        if (++mismatches >= 40) {
          break;
        }
        continue;
      }
      used[gi] = true;
      const ColonizeCol1Unit* g = &got->unit[gi];
      if (g->orders != e->orders || g->goto_x != e->goto_x || g->goto_y != e->goto_y ||
          (e->type == 19 && (g->moves != e->moves || g->turns_worked != e->turns_worked))) {
        fprintf(stderr, "%s unit field mismatch:\n", step_label);
        print_unit("got", (unsigned)gi, g);
        print_unit("exp", i, e);
        ok = false;
        if (++mismatches >= 40) {
          break;
        }
      }
    }
    if (got->head.unit_count != exp->head.unit_count) {
      fprintf(
        stderr,
        "%s unit_count got %u expected %u\n",
        step_label,
        got->head.unit_count,
        exp->head.unit_count
      );
      ok = false;
    }
    free(used);
  }

  /* Tribe growth accumulators / pop. */
  for (unsigned i = 0; i < exp->head.tribe_count && i < got->head.tribe_count; ++i) {
    if (got->tribe[i].population != exp->tribe[i].population ||
        got->tribe[i].growth_accum != exp->tribe[i].growth_accum) {
      fprintf(
        stderr,
        "%s tribe[%u] pop/acc got %u/%u expected %u/%u\n",
        step_label,
        i,
        got->tribe[i].population,
        got->tribe[i].growth_accum,
        exp->tribe[i].population,
        exp->tribe[i].growth_accum
      );
      ok = false;
      break;
    }
  }
  if (got->head.tribe_count != exp->head.tribe_count) {
    fprintf(
      stderr,
      "%s tribe_count got %u expected %u\n",
      step_label,
      got->head.tribe_count,
      exp->head.tribe_count
    );
    ok = false;
  }

  /*
   * Joint Euro↔Indian diplo fields (T3 roadmap Phase 0).
   * euro_relation + sticky + relation_by_indian — false WAR already bit Privateer.
   */
  for (int n = 0; n < 4; ++n) {
    for (int p = 0; p < 4; ++p) {
      if (got->nation[n].euro_relation[p] != exp->nation[n].euro_relation[p]) {
        fprintf(
          stderr,
          "%s nation[%d].euro_relation[%d] got 0x%02x expected 0x%02x\n",
          step_label,
          n,
          p,
          got->nation[n].euro_relation[p],
          exp->nation[n].euro_relation[p]
        );
        ok = false;
      }
    }
    if (got->nation[n].indian_hostility_sticky != exp->nation[n].indian_hostility_sticky) {
      fprintf(
        stderr,
        "%s nation[%d].indian_hostility_sticky got %u expected %u\n",
        step_label,
        n,
        (unsigned)got->nation[n].indian_hostility_sticky,
        (unsigned)exp->nation[n].indian_hostility_sticky
      );
      ok = false;
    }
    for (int idx = 0; idx < 8; ++idx) {
      if (got->nation[n].relation_by_indian[idx] != exp->nation[n].relation_by_indian[idx]) {
        fprintf(
          stderr,
          "%s nation[%d].relation_by_indian[%d] got %u expected %u\n",
          step_label,
          n,
          idx,
          (unsigned)got->nation[n].relation_by_indian[idx],
          (unsigned)exp->nation[n].relation_by_indian[idx]
        );
        ok = false;
      }
    }
  }
  return ok;
}

static int run_step(int from_turn) {
  char path_in[64];
  char path_exp[64];
  char err[256];
  snprintf(path_in, sizeof(path_in), "test-saves-ai/TURN%d.SAV", from_turn);
  snprintf(path_exp, sizeof(path_exp), "test-saves-ai/TURN%d.SAV", from_turn + 1);

  ColonizeCol1Save start;
  ColonizeCol1Save expect;
  col1_save_init(&start);
  col1_save_init(&expect);
  if (!col1_save_read_file(path_in, &start, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_in, err);
    return 1;
  }
  if (!col1_save_read_file(path_exp, &expect, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_exp, err);
    col1_save_free(&start);
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
  dos_rng_seed(&rng, AI_TURNS_VR_SEED);

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
  ctx.rng_seed = AI_TURNS_VR_SEED;

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

  char label[32];
  snprintf(label, sizeof(label), "TURN%d→%d", from_turn, from_turn + 1);
  const bool ok = compare_ai_state(&start, &expect, label);

  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&start);
  col1_save_free(&expect);
  if (!ok) {
    fprintf(stderr, "%s FAILED\n", label);
    return 1;
  }
  printf("%s ok\n", label);
  return 0;
}

int main(void) {
  /* AI_TURNS_ALL=1: keep going past a failing step (diagnostic overview). */
  const int run_all = getenv("AI_TURNS_ALL") != NULL;
  /* AI_TURNS_ONLY=t: run just TURNt→t+1 (diagnostic). */
  const int only = getenv("AI_TURNS_ONLY") ? atoi(getenv("AI_TURNS_ONLY")) : 0;
  int failed = 0;
  for (int t = 1; t <= 6; ++t) {
    if (only && t != only) {
      continue;
    }
    if (run_step(t) != 0) {
      failed = 1;
      if (!run_all) {
        return 1;
      }
    }
  }
  if (failed) {
    return 1;
  }
  printf("golden_ai_turns: all TURN1→TURN7 steps ok\n");
  return 0;
}
