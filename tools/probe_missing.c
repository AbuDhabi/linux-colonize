/*
 * TURN2→3 native-unit diff probe (T4.3, docs/ai_port_plan.md).
 *
 * Simulates test-saves-ai/TURN2.SAV -> TURN3.SAV the same way
 * tests/golden/test_ai_turns.c's run_step() does, then dumps every
 * type=19 nation=6 (Brave) unit's xy before/after/golden side by side.
 * Used to isolate the "missing Brave xy=(39,20)" golden failure to a
 * single wrong quiet-pulse direction pick (W vs NW) at tile (40,20) — see
 * T4.3's 2026-08-22 update for the full finding. Not wired into
 * CMakeLists; build ad hoc:
 *   gcc -Isrc -Ibuild -o /tmp/probe_missing tools/probe_missing.c \
 *     build/libcolonize_core.a && /tmp/probe_missing
 */
#include <stdio.h>
#include <string.h>
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

static void dump_n6(const char* tag, const ColonizeCol1Save* s) {
  for (unsigned i = 0; i < s->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &s->unit[i];
    if (u->type == 19 && u->nation_id == 6) {
      printf("  %s unit[%u] xy=(%u,%u) ord=%u mv=%u tw=%u\n", tag, i, u->x, u->y, u->orders, u->moves, u->turns_worked);
    }
  }
}

int main(void) {
  char err[256];
  ColonizeCol1Save start, expect;
  col1_save_init(&start);
  col1_save_init(&expect);
  col1_save_read_file("test-saves-ai/TURN2.SAV", &start, err, sizeof err);
  col1_save_read_file("test-saves-ai/TURN3.SAV", &expect, err, sizeof err);

  printf("BEFORE simulate (TURN2.SAV raw):\n");
  dump_n6("start", &start);
  printf("GOLDEN (TURN3.SAV raw):\n");
  dump_n6("exp", &expect);

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  assets_msg_load_file(&names, "COLONIZE/NAMES.TXT");
  ColonizeUnitPool units;
  units_reset(&units);
  units_load_types(&units, &names);
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_load_buildings(&colonies, &names);
  colonies_load_names(&colonies, "COLONIZE/COLONY.TXT");
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult br;
  col1_bridge_apply(&start, &map, &units, &colonies, &europe, &br, err, sizeof(err));

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

  col1_bridge_capture(&start, &map, &units, &colonies, &europe, year, autumn, turn_number,
                       br.human_nation, br.cursor_x, br.cursor_y, units.selected_id, err, sizeof(err));

  printf("AFTER simulate (Linux result):\n");
  dump_n6("got", &start);
  return 0;
}
