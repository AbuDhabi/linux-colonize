/*
 * Golden AI turn steps (TURN1..7 pair compares, live in ctest since 2026-08-28).
 *
 * All six steps pass (T1.23 closed 2026-09-05). Diagnostics: AI_TURNS_ALL=1 runs every step
 * instead of stopping at the first failure, AI_TURNS_ONLY=t runs one step.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai.h"
#include "core/col1_save.h"

#include "tests/common/golden_fixture.h"
#include "../common/test_runner.h"

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
    (unsigned)u->col1_counter16
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
          (e->type == 19 && (g->moves != e->moves || g->col1_counter16 != e->col1_counter16))) {
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
  snprintf(path_in, sizeof(path_in), "test-saves-ai/TURN%d.SAV", from_turn);
  snprintf(path_exp, sizeof(path_exp), "test-saves-ai/TURN%d.SAV", from_turn + 1);

  GoldenFixture fx;
  if (!golden_open(path_in, path_exp, AI_TURNS_VR_SEED, &fx)) {
    golden_close(&fx);
    return 1;
  }
  if (!golden_turn(&fx)) {
    golden_close(&fx);
    return 1;
  }

  char label[32];
  snprintf(label, sizeof(label), "TURN%d→%d", from_turn, from_turn + 1);
  const bool ok = compare_ai_state(&fx.start, &fx.expect, label);

  golden_close(&fx);
  if (!ok) {
    fprintf(stderr, "%s FAILED\n", label);
    return 1;
  }
  printf("%s ok\n", label);
  return 0;
}

/*
 * Each step loads its own fresh TURNt.SAV/TURN(t+1).SAV pair via
 * golden_open/golden_close (see golden_fixture.h: "safe to call at any
 * stage"), so the six steps carry no state between them and convert
 * mechanically to one case per turn transition. The old AI_TURNS_ALL /
 * AI_TURNS_ONLY diagnostic env vars are superseded by the shared runner's
 * COLONIZE_TEST_ONLY (run one step) — the runner already always runs every
 * case and reports which failed, i.e. AI_TURNS_ALL's behaviour is now the
 * only behaviour.
 */
static int case_turn1(void) { return run_step(1); }
static int case_turn2(void) { return run_step(2); }
static int case_turn3(void) { return run_step(3); }
static int case_turn4(void) { return run_step(4); }
static int case_turn5(void) { return run_step(5); }
static int case_turn6(void) { return run_step(6); }

static const TestCase k_cases[] = {
    {"TURN1_to_2", case_turn1},
    {"TURN2_to_3", case_turn2},
    {"TURN3_to_4", case_turn3},
    {"TURN4_to_5", case_turn4},
    {"TURN5_to_6", case_turn5},
    {"TURN6_to_7", case_turn6},
};

TEST_MAIN(k_cases)
