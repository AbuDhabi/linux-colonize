#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/europe.h"
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

  /* Production: food +3/−2 per colonist. */
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
  c->colonist_count = 1;
  c->population = 1;
  colonies.colony_count = 1;

  ColonizeTurnResult prod;
  memset(&prod, 0, sizeof(prod));
  turn_colony_free_production(&colonies, c, &prod, NULL);
  if (c->stock[COLONIZE_CARGO_FOOD] != 11) { /* 10 + 3 - 2 */
    fprintf(stderr, "food expected 11 got %d\n", c->stock[COLONIZE_CARGO_FOOD]);
    return 1;
  }
  if (prod.colonies_produced != 1) {
    fprintf(stderr, "expected 1 colony produced\n");
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
      turn_colony_free_production(&pool, col, &prod, &delta);
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
    assets_msg_free(&names);
  }

  fprintf(stderr, "turn tests ok\n");
  diag_shutdown();
  return 0;
}
