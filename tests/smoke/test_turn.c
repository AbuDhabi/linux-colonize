#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/diagnostics.h"

static int expect_date(uint16_t year, uint16_t autumn, const char* want) {
  char got[32];
  turn_format_date(year, autumn, got, sizeof(got));
  if (strcmp(got, want) != 0) {
    fprintf(stderr, "date expected '%s' got '%s'\n", want, got);
    return 1;
  }
  return 0;
}

static int expect_cal(
  uint16_t year,
  uint16_t autumn,
  uint32_t turn,
  uint16_t ey,
  uint16_t ea,
  uint32_t et
) {
  turn_advance_calendar(&year, &autumn, &turn);
  if (year != ey || autumn != ea || turn != et) {
    fprintf(
      stderr,
      "calendar got year=%u autumn=%u turn=%u expected %u/%u/%u\n",
      year,
      autumn,
      turn,
      ey,
      ea,
      et
    );
    return 1;
  }
  return 0;
}

int main(void) {
  diag_init(0, NULL);

  if (expect_date(1492, 0, "Spring 1492") != 0 || expect_date(1600, 1, "Autumn 1600") != 0) {
    return 1;
  }

  /* Pre-1600: one year per turn. */
  if (expect_cal(1492, 0, 0, 1493, 0, 1) != 0 || expect_cal(1493, 0, 1, 1494, 0, 2) != 0 ||
      expect_cal(1599, 0, 107, 1600, 0, 108) != 0) {
    return 1;
  }

  /* From 1600: Spring → Autumn → next Spring. */
  if (expect_cal(1600, 0, 108, 1600, 1, 109) != 0 ||
      expect_cal(1600, 1, 109, 1601, 0, 110) != 0 ||
      expect_cal(1601, 0, 110, 1601, 1, 111) != 0) {
    return 1;
  }

  /* Production without fields: consume 2 food / colonist. */
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  memset(c, 0, sizeof(*c));
  c->active = true;
  c->id = 1;
  c->building_in_production = -1;
  c->stock[COLONIZE_CARGO_FOOD] = 10;
  c->colonists[0].active = true;
  c->colonists[0].unit_type_index = 0;
  c->colonists[0].building_type = -1;
  c->colonists[0].field_job = -1;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
    c->tiles[t] = -1;
  }
  c->colonist_count = 1;
  c->population = 1;
  colonies.colony_count = 1;

  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_colony_free_production(&colonies, c, NULL, &prod, NULL);
  if (c->stock[COLONIZE_CARGO_FOOD] != 8) { /* 10 - 2 */
    fprintf(stderr, "food expected 8 got %d\n", c->stock[COLONIZE_CARGO_FOOD]);
    return 1;
  }
  if (prod.colonies_produced != 1) {
    fprintf(stderr, "expected 1 colony produced\n");
    return 1;
  }

  /* Yield chart: plains farmer / ocean fisherman. */
  if (colony_yield_job_cargo(COLONIZE_JOB_LUMBERJACK) != COLONIZE_CARGO_LUMBER) {
    fprintf(stderr, "lumberjack cargo mapping wrong\n");
    return 1;
  }

  /* Full turn_end advances calendar and refreshes human MP. */
  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 1;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Scout");
  units.types[0].movement = 4;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  const int uid = units_spawn(&units, 0, 5, 5);
  if (uid < 0) {
    fprintf(stderr, "spawn failed\n");
    return 1;
  }
  ColonizeUnit* u = units_get(&units, uid);
  u->nation_id = 0;
  u->moves_left = 0;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.needed_crosses = TURN_DEFAULT_NEEDED_CROSSES;

  uint32_t turn_number = 2;
  uint16_t year = 1494;
  uint16_t autumn = 0;
  char status[128];
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn_number;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.human_nation = 0;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.status = status;
  ctx.status_size = sizeof(status);

  ColonizeTurnResult end = turn_end(&ctx);
  if (!end.advanced || year != 1495 || autumn != 0 || turn_number != 3) {
    fprintf(
      stderr,
      "turn_end calendar fail year=%u autumn=%u turn=%u\n",
      year,
      autumn,
      turn_number
    );
    return 1;
  }
  if (u->moves_left != 4) {
    fprintf(stderr, "human MP not refreshed got %d\n", u->moves_left);
    return 1;
  }
  if (strstr(status, "1495") == NULL) {
    fprintf(stderr, "status missing date: %s\n", status);
    return 1;
  }

  /* Next-unit selection wraps to units with moves. */
  const int uid2 = units_spawn_allow_stack(&units, 0, 6, 6);
  ColonizeUnit* u2 = units_get(&units, uid2);
  u2->nation_id = 0;
  u2->moves_left = 2;
  u->moves_left = 0;
  units.selected_id = uid;
  if (!turn_select_next_unit(&units, 0) || units.selected_id != uid2) {
    fprintf(stderr, "wait-next failed selected=%d\n", units.selected_id);
    return 1;
  }

  /* Turn-owner colors: NAMES.TXT @COUNTRY / FUN_43f7_05f4. */
  if (turn_nation_color(0) != 12 || turn_nation_color(1) != 9 || turn_nation_color(2) != 14 ||
      turn_nation_color(3) != 13) {
    fprintf(stderr, "european turn colors mismatch\n");
    return 1;
  }
  if (turn_nation_color(4) != 97 || turn_nation_color(11) != 71) {
    fprintf(stderr, "tribe turn colors mismatch\n");
    return 1;
  }

  {
    uint8_t pixels[320 * 200];
    ColonizeFramebuffer8 fb;
    fb.width = 320;
    fb.height = 200;
    fb.pixels = pixels;
    memset(pixels, 0, sizeof(pixels));
    turn_draw_owner_indicator(&fb, 2); /* Spain = 14 */
    const int x0 = TURN_OWNER_INDICATOR_X;
    const int y0 = TURN_OWNER_INDICATOR_Y;
    if (pixels[y0 * 320 + x0] != 14 ||
        pixels[(y0 + TURN_OWNER_INDICATOR_H - 1) * 320 + (x0 + TURN_OWNER_INDICATOR_W - 1)] != 14) {
      fprintf(stderr, "owner indicator pixels not filled\n");
      return 1;
    }
    if (pixels[y0 * 320 + (x0 - 1)] != 0) {
      fprintf(stderr, "owner indicator spilled left\n");
      return 1;
    }
  }

  /* Indicator is only armed during EURO/INDIAN processor steps. */
  {
    ColonizeTurnProcessor proc;
    turn_processor_start(&proc);
    if (turn_processor_show_indicator(&proc)) {
      fprintf(stderr, "indicator should be off at start\n");
      return 1;
    }
    uint32_t turn = 1;
    uint16_t year = 1492;
    uint16_t autumn = 0;
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.turn_number = &turn;
    ctx.game_year = &year;
    ctx.game_autumn = &autumn;
    ctx.human_nation = 0;
    int active = 0;
    ctx.active_turn_nation = &active;
    if (!turn_processor_advance(&proc, &ctx)) {
      fprintf(stderr, "setup should leave processor active\n");
      return 1;
    }
    if (turn_processor_show_indicator(&proc)) {
      fprintf(stderr, "indicator should be off after setup\n");
      return 1;
    }
    if (!turn_processor_advance(&proc, &ctx) || !turn_processor_show_indicator(&proc)) {
      fprintf(stderr, "indicator should show during euro AI step\n");
      return 1;
    }
    if (active != 1) {
      fprintf(stderr, "expected france active got %d\n", active);
      return 1;
    }
  }

  /* Carpenter workplace + Stockade project completes via free production ticks. */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names)) {
      fprintf(stderr, "failed to load buildings for hammer test\n");
      assets_msg_free(&names);
      return 1;
    }
    const int carpenter = colonies_find_building(&pool, "Carpenter's Shop");
    const int stockade = colonies_find_building(&pool, "Stockade");
    if (carpenter < 0 || stockade < 0) {
      fprintf(stderr, "missing Carpenter/Stockade building types\n");
      assets_msg_free(&names);
      return 1;
    }
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->has_building[carpenter] = true;
    col->building_in_production = stockade;
    col->stock[COLONIZE_CARGO_FOOD] = 50;
    col->colonists[0].active = true;
    col->colonists[0].unit_type_index = 0;
    col->colonists[0].building_type = carpenter;
    col->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    const ColonizeBuildingType* bt = colonies_building_type(&pool, stockade);
    const int need = bt ? bt->hammers : 64;
    ColonizeColonyProdDelta delta;
    bool completed = false;
    for (int t = 0; t < need + 8; ++t) {
      ColonizeTurnResult prod;
      memset(&prod, 0, sizeof(prod));
      turn_colony_free_production(&pool, col, NULL, &prod, &delta);
      if (delta.building_completed || col->has_building[stockade]) {
        completed = true;
        break;
      }
    }
    if (!completed || !col->has_building[stockade]) {
      fprintf(
        stderr,
        "Stockade not completed after ticks (hammers=%d need=%d)\n",
        col->hammers,
        need
      );
      assets_msg_free(&names);
      return 1;
    }
    if (col->building_in_production >= 0) {
      fprintf(stderr, "expected cleared building_in_production after complete\n");
      assets_msg_free(&names);
      return 1;
    }
    /* No carpenter assigned → no hammers even if Carpenter's Shop exists. */
    col->colonists[0].building_type = -1;
    col->hammers = 0;
    col->building_in_production = stockade;
    col->has_building[stockade] = false;
    {
      ColonizeTurnResult prod;
      ColonizeColonyProdDelta delta;
      memset(&prod, 0, sizeof(prod));
      memset(&delta, 0, sizeof(delta));
      turn_colony_free_production(&pool, col, NULL, &prod, &delta);
      if (delta.hammers_added != 0 || col->hammers != 0) {
        fprintf(
          stderr,
          "expected no hammers without carpenter got delta=%d stock=%d\n",
          delta.hammers_added,
          col->hammers
        );
        assets_msg_free(&names);
        return 1;
      }
    }
    assets_msg_free(&names);
  }
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names)) {
      fprintf(stderr, "craft test: load buildings failed\n");
      assets_msg_free(&names);
      return 1;
    }
    const int distiller = colonies_find_building(&pool, "Rum Distiller's House");
    const int weaver = colonies_find_building(&pool, "Weaver's House");
    const int smith = colonies_find_building(&pool, "Blacksmith's House");
    const int armory = colonies_find_building(&pool, "Armory");
    const int fur = colonies_find_building(&pool, "Fur Trader's House");
    if (distiller < 0 || weaver < 0 || smith < 0 || armory < 0 || fur < 0) {
      fprintf(stderr, "craft test: missing building types\n");
      assets_msg_free(&names);
      return 1;
    }

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->building_in_production = -1;
    col->has_building[distiller] = true;
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->stock[COLONIZE_CARGO_SUGAR] = 10;
    col->colonists[0].active = true;
    col->colonists[0].building_type = distiller;
    col->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    ColonizeTurnResult prod;
    ColonizeColonyProdDelta delta;
    memset(&prod, 0, sizeof(prod));
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->stock[COLONIZE_CARGO_RUM] != 3 || col->stock[COLONIZE_CARGO_SUGAR] != 7 ||
        delta.goods[COLONIZE_CARGO_RUM] != 3) {
      fprintf(
        stderr,
        "rum craft failed sugar=%d rum=%d dRum=%d\n",
        col->stock[COLONIZE_CARGO_SUGAR],
        col->stock[COLONIZE_CARGO_RUM],
        delta.goods[COLONIZE_CARGO_RUM]
      );
      assets_msg_free(&names);
      return 1;
    }

    /* No furs → no coats. */
    col->has_building[fur] = true;
    col->colonists[0].building_type = fur;
    col->stock[COLONIZE_CARGO_FURS] = 0;
    const int coats_before = col->stock[COLONIZE_CARGO_COATS];
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->stock[COLONIZE_CARGO_COATS] != coats_before || delta.goods[COLONIZE_CARGO_COATS] != 0) {
      fprintf(stderr, "expected no coats without furs\n");
      assets_msg_free(&names);
      return 1;
    }

    col->has_building[weaver] = true;
    col->colonists[0].building_type = weaver;
    col->stock[COLONIZE_CARGO_COTTON] = 5;
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->stock[COLONIZE_CARGO_CLOTH] != 3 || col->stock[COLONIZE_CARGO_COTTON] != 2) {
      fprintf(
        stderr,
        "cloth craft failed cotton=%d cloth=%d\n",
        col->stock[COLONIZE_CARGO_COTTON],
        col->stock[COLONIZE_CARGO_CLOTH]
      );
      assets_msg_free(&names);
      return 1;
    }

    col->has_building[smith] = true;
    col->has_building[armory] = true;
    col->colonists[0].building_type = smith;
    col->colonists[0].active = true;
    col->stock[COLONIZE_CARGO_ORE] = 10;
    col->stock[COLONIZE_CARGO_TOOLS] = 0;
    col->stock[COLONIZE_CARGO_MUSKETS] = 0;
    /* Two workers: smith + gunsmith. */
    col->colonists[1].active = true;
    col->colonists[1].building_type = armory;
    col->colonists[1].field_job = -1;
    col->colonist_count = 2;
    col->population = 2;
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    /* Smith makes 3 tools from ore; gunsmith converts 3 tools → 3 muskets same tick. */
    if (col->stock[COLONIZE_CARGO_ORE] != 7 || col->stock[COLONIZE_CARGO_TOOLS] != 0 ||
        col->stock[COLONIZE_CARGO_MUSKETS] != 3) {
      fprintf(
        stderr,
        "tools/muskets craft failed ore=%d tools=%d guns=%d\n",
        col->stock[COLONIZE_CARGO_ORE],
        col->stock[COLONIZE_CARGO_TOOLS],
        col->stock[COLONIZE_CARGO_MUSKETS]
      );
      assets_msg_free(&names);
      return 1;
    }
    assets_msg_free(&names);
  }

  /* Production rules: convert +1 on tiles; convert/criminal floor in buildings; wrong expert → free rate. */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[256];
    if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
      fprintf(stderr, "production rules: map load: %s\n", err);
      return 1;
    }
    int fx = -1, fy = -1;
    for (int y = 1; y < (int)map.height - 1 && fx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && fx < 0; ++x) {
        if (colony_yield_for_tile(&map, x, y, COLONIZE_JOB_LUMBERJACK) == 2) {
          fx = x;
          fy = y;
        }
      }
    }
    if (fx < 0) {
      fprintf(stderr, "production rules: no tile with lumberjack yield 2\n");
      map_free(&map);
      return 1;
    }
    const int base = colony_yield_for_tile(&map, fx, fy, COLONIZE_JOB_LUMBERJACK);
    const int convert_yld =
      colony_yield_for_worker(&map, fx, fy, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_CONVERT);
    if (convert_yld != base + 1) {
      fprintf(
        stderr,
        "convert tile bonus failed base=%d convert=%d\n",
        base,
        convert_yld
      );
      map_free(&map);
      return 1;
    }
    const int wrong_expert =
      colony_yield_for_worker(&map, fx, fy, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_FREE_COLONIST);
    if (wrong_expert != base) {
      fprintf(
        stderr,
        "wrong field expert should match free yield base=%d got=%d\n",
        base,
        wrong_expert
      );
      map_free(&map);
      return 1;
    }
    map_free(&map);

    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names)) {
      fprintf(stderr, "production rules: load buildings failed\n");
      assets_msg_free(&names);
      return 1;
    }
    const int distiller = colonies_find_building(&pool, "Rum Distiller's House");
    if (distiller < 0) {
      fprintf(stderr, "production rules: missing distillery\n");
      assets_msg_free(&names);
      return 1;
    }
    const char* dname = pool.building_types[distiller].name;
    if (colony_prod_manufacturing_output(dname, COLONIZE_PROF_CONVERT, COLONIZE_PROF_DISTILLER) != 1 ||
        colony_prod_manufacturing_output(dname, COLONIZE_PROF_BLACKSMITH, COLONIZE_PROF_DISTILLER) != 3 ||
        colony_prod_manufacturing_output(dname, COLONIZE_PROF_DISTILLER, COLONIZE_PROF_DISTILLER) != 6) {
      fprintf(stderr, "manufacturing class/skill rules failed\n");
      assets_msg_free(&names);
      return 1;
    }

    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->building_in_production = -1;
    col->has_building[distiller] = true;
    col->stock[COLONIZE_CARGO_FOOD] = 20;
    col->stock[COLONIZE_CARGO_SUGAR] = 10;
    col->colonists[0].active = true;
    col->colonists[0].building_type = distiller;
    col->colonists[0].profession = COLONIZE_PROF_CONVERT;
    col->colonists[0].field_job = -1;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      col->tiles[t] = -1;
    }
    col->colonist_count = 1;
    col->population = 1;
    pool.colony_count = 1;

    ColonizeTurnResult prod;
    ColonizeColonyProdDelta delta;
    memset(&prod, 0, sizeof(prod));
    turn_colony_free_production(&pool, col, NULL, &prod, &delta);
    if (col->stock[COLONIZE_CARGO_RUM] != 1 || col->stock[COLONIZE_CARGO_SUGAR] != 9) {
      fprintf(
        stderr,
        "convert rum craft failed sugar=%d rum=%d\n",
        col->stock[COLONIZE_CARGO_SUGAR],
        col->stock[COLONIZE_CARGO_RUM]
      );
      assets_msg_free(&names);
      return 1;
    }
    assets_msg_free(&names);
  }

  /* Field lumberjack harvests from forest surround tile. */
  {
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[256];
    if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
      fprintf(stderr, "map load for field test: %s\n", err);
      return 1;
    }
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !colonies_load_buildings(&pool, &names) || !colonies_load_names(&pool, "COLONIZE/COLONY.TXT")) {
      fprintf(stderr, "names/buildings for field test failed\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    int fx = -1, fy = -1, ftile = -1, cx = -1, cy = -1;
    for (int y = 1; y < (int)map.height - 1 && fx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && fx < 0; ++x) {
        if (!map_tile_is_land(&map, x, y) || !colonies_can_found(&pool, &map, x, y)) {
          continue;
        }
        for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
          int dx = 0, dy = 0;
          colonies_field_tile_delta(ti, &dx, &dy);
          const int yld =
            colony_yield_for_tile(&map, x + dx, y + dy, COLONIZE_JOB_LUMBERJACK);
          if (yld > 0) {
            cx = x;
            cy = y;
            fx = x + dx;
            fy = y + dy;
            ftile = ti;
            break;
          }
        }
      }
    }
    if (ftile < 0) {
      fprintf(stderr, "no colony site with lumberjack yield nearby\n");
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    const int cid = colonies_found(&pool, &map, cx, cy, 0, UNITS_JOB_NONE, 0, 0, 0);
    ColonizeColony* col = colonies_get_mut(&pool, cid);
    if (!col || !colonies_assign_field(&pool, cid, 0, ftile, COLONIZE_JOB_LUMBERJACK)) {
      fprintf(stderr, "assign lumberjack failed at (%d,%d) tile %d\n", fx, fy, ftile);
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    /* Isolate field harvest from carpenter hammers on default Stockade project. */
    col->building_in_production = -1;
    const int before = col->stock[COLONIZE_CARGO_LUMBER];
    const int expect =
      colony_yield_for_worker(&map, fx, fy, COLONIZE_JOB_LUMBERJACK, col->colonists[0].profession);
    ColonizeTurnResult prod;
    ColonizeColonyProdDelta delta;
    memset(&prod, 0, sizeof(prod));
    turn_colony_free_production(&pool, col, &map, &prod, &delta);
    /* No carpenter assigned → hammers stay 0 (shop alone does not produce). */
    if (delta.lumber < expect) {
      fprintf(
        stderr,
        "field lumber delta too low got %d expect %d (stock %d->%d)\n",
        delta.lumber,
        expect,
        before,
        col->stock[COLONIZE_CARGO_LUMBER]
      );
      assets_msg_free(&names);
      map_free(&map);
      return 1;
    }
    assets_msg_free(&names);
    map_free(&map);
  }

  fprintf(stderr, "turn tests ok\n");
  diag_shutdown();
  return 0;
}
