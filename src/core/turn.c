#include "core/turn.h"

#include <stdio.h>
#include <string.h>

#include "core/ai.h"
#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "platform/diagnostics.h"

/* NAMES.TXT @COUNTRY / FUN_43f7_05f4 → DS:0x848..0x84b */
static const uint8_t k_european_colors[4] = {12, 9, 14, 13};

/* NAMES.TXT @TRIBES color field → DS:0x84c..0x853 (nations 4..11) */
static const uint8_t k_tribe_colors[8] = {97, 149, 54, 87, 67, 111, 118, 71};

static void turn_set_active_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (ctx && ctx->active_turn_nation) {
    *ctx->active_turn_nation = nation_id;
  }
}

uint8_t turn_nation_color(int nation_id) {
  if (nation_id >= 0 && nation_id < 4) {
    return k_european_colors[nation_id];
  }
  if (nation_id >= 4 && nation_id <= 11) {
    return k_tribe_colors[nation_id - 4];
  }
  return k_european_colors[0];
}

void turn_draw_owner_indicator(ColonizeFramebuffer8* framebuffer, int nation_id) {
  if (!framebuffer || !framebuffer->pixels || framebuffer->width <= 0 || framebuffer->height <= 0) {
    return;
  }
  const uint8_t color = turn_nation_color(nation_id);
  const int x0 = TURN_OWNER_INDICATOR_X;
  const int y0 = TURN_OWNER_INDICATOR_Y;
  for (int y = y0; y < y0 + TURN_OWNER_INDICATOR_H; ++y) {
    if (y < 0 || y >= framebuffer->height) {
      continue;
    }
    for (int x = x0; x < x0 + TURN_OWNER_INDICATOR_W; ++x) {
      if (x < 0 || x >= framebuffer->width) {
        continue;
      }
      framebuffer->pixels[y * framebuffer->width + x] = color;
    }
  }
}

void turn_advance_calendar(uint16_t* year, uint16_t* autumn, uint32_t* turn_number) {
  if (!year || !autumn || !turn_number) {
    return;
  }
  if (*year == 0) {
    *year = TURN_START_YEAR;
  }
  (*turn_number)++;

  if (*year < TURN_BIANNUAL_YEAR) {
    /* One turn per year; season stays Spring. */
    *autumn = 0;
    (*year)++;
    return;
  }

  /* From 1600: Spring → Autumn within the year, then next Spring. */
  if (*autumn == 0) {
    *autumn = 1;
  } else {
    *autumn = 0;
    (*year)++;
  }
}

void turn_format_date(uint16_t year, uint16_t autumn, char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  if (year == 0) {
    year = TURN_START_YEAR;
  }
  snprintf(out, out_size, "%s %u", autumn ? "Autumn" : "Spring", (unsigned)year);
}

void turn_refresh_moves_for_nation(ColonizeUnitPool* pool, int nation_id) {
  if (!pool) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    const ColonizeUnitType* type = units_type(pool, u->type_index);
    if (type) {
      u->moves_left = type->movement;
    }
  }
}

bool turn_human_units_exhausted(const ColonizeUnitPool* pool, int human_nation) {
  if (!pool) {
    return true;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != human_nation) {
      continue;
    }
    if (!units_is_on_map(u)) {
      continue;
    }
    if (u->moves_left > 0) {
      return false;
    }
  }
  return true;
}

bool turn_select_next_unit(ColonizeUnitPool* pool, int human_nation) {
  if (!pool) {
    return false;
  }
  const int start = pool->selected_id;
  int best_after = -1;
  int best_any = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != human_nation || u->moves_left <= 0) {
      continue;
    }
    if (!units_is_on_map(u)) {
      continue;
    }
    if (best_any < 0 || u->id < best_any) {
      best_any = u->id;
    }
    if (u->id > start && (best_after < 0 || u->id < best_after)) {
      best_after = u->id;
    }
  }
  const int pick = best_after >= 0 ? best_after : best_any;
  if (pick < 0) {
    return false;
  }
  pool->selected_id = pick;
  return true;
}

bool turn_option_end_of_turn(const ColonizeCol1Save* col1, bool col1_ok) {
  return col1_ok && col1 && col1->head.game_options.end_of_turn != 0;
}

bool turn_option_autosave(const ColonizeCol1Save* col1, bool col1_ok) {
  return col1_ok && col1 && col1->head.game_options.autosave != 0;
}

static int turn_clamp_stock(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 65535) {
    return 65535;
  }
  return v;
}

static bool turn_building_name_has(const ColonizeColonyPool* pool, const ColonizeColony* colony, const char* needle) {
  if (!pool || !colony || !needle) {
    return false;
  }
  for (int i = 0; i < pool->building_type_count && i < COLONIZE_BUILDING_TYPES_MAX; ++i) {
    if (!colony->has_building[i]) {
      continue;
    }
    if (strstr(pool->building_types[i].name, needle) != NULL) {
      return true;
    }
  }
  return false;
}

static int turn_count_field_job(const ColonizeColony* colony, int field_job) {
  if (!colony) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES; ++i) {
    const int who = (int)colony->tiles[i];
    if (who < 0 || who >= colony->colonist_count) {
      continue;
    }
    if (colony->colonists[who].active && colony->colonists[who].field_job == field_job) {
      n++;
    }
  }
  return n;
}

static void turn_produce_one_colony(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const ColonizeWorldMap* map,
  ColonizeTurnResult* out,
  ColonizeColonyProdDelta* delta
) {
  if (delta) {
    memset(delta, 0, sizeof(*delta));
  }
  if (!pool || !colony || !colony->active) {
    return;
  }
  const int pop = colony->colonist_count > 0 ? colony->colonist_count : colony->population;
  if (pop <= 0) {
    return;
  }

  int field_food = 0;
  int field_lumber = 0;
  int field_ore = 0;

  /* Town commons (center tile) + area-view field workers. */
  if (map) {
    ColonizeTownCommonsYield tc;
    colony_yield_town_commons(map, colony->x, colony->y, &tc);
    if (tc.food > 0) {
      colony->stock[COLONIZE_CARGO_FOOD] =
        turn_clamp_stock(colony->stock[COLONIZE_CARGO_FOOD] + tc.food);
      field_food += tc.food;
      if (delta) {
        delta->goods[COLONIZE_CARGO_FOOD] += tc.food;
      }
    }
    if (tc.secondary_amount > 0 && tc.secondary_cargo >= 0 &&
        tc.secondary_cargo < COLONIZE_CARGO_COUNT) {
      colony->stock[tc.secondary_cargo] =
        turn_clamp_stock(colony->stock[tc.secondary_cargo] + tc.secondary_amount);
      if (delta) {
        delta->goods[tc.secondary_cargo] += tc.secondary_amount;
      }
      if (tc.secondary_cargo == COLONIZE_CARGO_LUMBER) {
        field_lumber += tc.secondary_amount;
      } else if (tc.secondary_cargo == COLONIZE_CARGO_ORE) {
        field_ore += tc.secondary_amount;
      }
    }

    for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
      const int who = (int)colony->tiles[ti];
      if (who < 0 || who >= colony->colonist_count) {
        continue;
      }
      const ColonizeColonist* c = &colony->colonists[who];
      if (!c->active || c->field_job < 0) {
        continue;
      }
      int dx = 0;
      int dy = 0;
      if (!colonies_field_tile_delta(ti, &dx, &dy)) {
        continue;
      }
      const int yld =
        colony_yield_for_worker(map, colony->x + dx, colony->y + dy, c->field_job, c->profession);
      if (yld <= 0) {
        continue;
      }
      const int cargo = colony_yield_job_cargo(c->field_job);
      if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
        continue;
      }
      colony->stock[cargo] = turn_clamp_stock(colony->stock[cargo] + yld);
      if (delta) {
        delta->goods[cargo] += yld;
      }
      if (cargo == COLONIZE_CARGO_FOOD) {
        field_food += yld;
      } else if (cargo == COLONIZE_CARGO_LUMBER) {
        field_lumber += yld;
      } else if (cargo == COLONIZE_CARGO_ORE) {
        field_ore += yld;
      }
    }
  }

  const int consumed = pop * TURN_FOOD_PER_COLONIST;
  colony->stock[COLONIZE_CARGO_FOOD] =
    turn_clamp_stock(colony->stock[COLONIZE_CARGO_FOOD] - consumed);
  if (delta) {
    delta->goods[COLONIZE_CARGO_FOOD] -= consumed;
    delta->food_net = field_food - consumed;
    delta->lumber = field_lumber;
    delta->ore = field_ore;
  }
  if (field_food < consumed && out) {
    out->food_shortages++;
  }

  /*
   * Lumber fallback: if no lumberjacks but a carpenter building exists,
   * invent 1 lumber so Stockade demos still work without field assign.
   */
  if (turn_count_field_job(colony, COLONIZE_JOB_LUMBERJACK) == 0 &&
      turn_building_name_has(pool, colony, "Carpenter")) {
    colony->stock[COLONIZE_CARGO_LUMBER] =
      turn_clamp_stock(colony->stock[COLONIZE_CARGO_LUMBER] + 1);
    if (delta) {
      delta->lumber += 1;
      delta->goods[COLONIZE_CARGO_LUMBER] += 1;
    }
  }

  /* Settlement manufacturing (raw → goods) before hammers consume lumber. */
  colony_craft_one_colony(pool, colony, delta);
  if (delta) {
    delta->lumber = delta->goods[COLONIZE_CARGO_LUMBER];
    delta->ore = delta->goods[COLONIZE_CARGO_ORE];
    delta->food_net = delta->goods[COLONIZE_CARGO_FOOD];
  }

  /* Carpenter hammers: convert lumber toward current project. */
  if (colony->building_in_production >= 0) {
    int lumber_use = 0;
    const int hammers_add =
      colony_prod_colony_hammers(pool, colony, &lumber_use);
    if (hammers_add > 0) {
      if (lumber_use > colony->stock[COLONIZE_CARGO_LUMBER]) {
        lumber_use = colony->stock[COLONIZE_CARGO_LUMBER];
      }
      colony->stock[COLONIZE_CARGO_LUMBER] -= lumber_use;
      if (delta) {
        delta->lumber -= lumber_use;
        delta->goods[COLONIZE_CARGO_LUMBER] -= lumber_use;
      }
      const int hammers = lumber_use > 0 ? lumber_use : hammers_add;
      colony->hammers += hammers;
      if (delta) {
        delta->hammers_added = hammers;
      }

      if (colonies_try_complete_building(pool, colony->id)) {
        if (delta) {
          delta->building_completed = true;
        }
        if (out) {
          out->buildings_completed++;
        }
      }
    }
  }

  if (out) {
    out->colonies_produced++;
  }
}

void turn_run_colony_production(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  ColonizeTurnResult* out
) {
  if (!pool) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active) {
      turn_produce_one_colony(pool, &pool->colonies[i], map, out, NULL);
    }
  }
}

void turn_colony_free_production(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const ColonizeWorldMap* map,
  ColonizeTurnResult* out,
  ColonizeColonyProdDelta* out_delta
) {
  ColonizeTurnResult local;
  memset(&local, 0, sizeof(local));
  turn_produce_one_colony(pool, colony, map, out ? out : &local, out_delta);
}

static int turn_count_bells_and_crosses(
  const ColonizeColonyPool* pool,
  int* out_bells,
  int* out_crosses
) {
  int bells = 0;
  int crosses = 0;
  if (!pool) {
    if (out_bells) {
      *out_bells = 0;
    }
    if (out_crosses) {
      *out_crosses = 0;
    }
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (!c->active) {
      continue;
    }
    bells += colony_prod_colony_bells(pool, c);
    crosses += colony_prod_colony_crosses(pool, c);
  }
  if (out_bells) {
    *out_bells = bells;
  }
  if (out_crosses) {
    *out_crosses = crosses;
  }
  return bells + crosses;
}

static void turn_push_dock_immigrant(EuropeScreen* europe, ColonizeTurnResult* out) {
  if (!europe) {
    return;
  }
  if (europe_immigrant_from_pool(europe)) {
    if (out) {
      out->immigrants_arrived++;
    }
  }
}

void turn_run_nation_ticks(ColonizeTurnContext* ctx, ColonizeTurnResult* out) {
  if (!ctx) {
    return;
  }
  int bells = 0;
  int crosses = 0;
  turn_count_bells_and_crosses(ctx->colonies, &bells, &crosses);

  if (ctx->europe) {
    if (ctx->europe->needed_crosses == 0) {
      ctx->europe->needed_crosses = TURN_DEFAULT_NEEDED_CROSSES;
    }
    ctx->europe->liberty_bells_last_turn = (uint16_t)(bells > 65535 ? 65535 : bells);
    {
      unsigned total = (unsigned)ctx->europe->liberty_bells_total + (unsigned)bells;
      if (total > 65535u) {
        total = 65535u;
      }
      ctx->europe->liberty_bells_total = (uint16_t)total;
    }
    {
      unsigned cur = (unsigned)ctx->europe->current_crosses + (unsigned)crosses;
      if (cur > 65535u) {
        cur = 65535u;
      }
      ctx->europe->current_crosses = (uint16_t)cur;
    }
    while (ctx->europe->needed_crosses > 0 &&
           ctx->europe->current_crosses >= ctx->europe->needed_crosses) {
      ctx->europe->current_crosses =
        (uint16_t)(ctx->europe->current_crosses - ctx->europe->needed_crosses);
      /* Next immigrant costs more crosses (DOS ramps threshold). */
      unsigned need = (unsigned)ctx->europe->needed_crosses + 4u;
      if (need > 65535u) {
        need = 65535u;
      }
      ctx->europe->needed_crosses = (uint16_t)need;
      turn_push_dock_immigrant(ctx->europe, out);
    }
    europe_tick_voyages(ctx->europe, ctx->units);
  }

  if (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4) {
    ColonizeCol1Nation* nat = &ctx->col1->nation[ctx->human_nation];
    nat->liberty_bells_last_turn = (uint16_t)(bells > 65535 ? 65535 : bells);
    {
      unsigned total = (unsigned)nat->liberty_bells_total + (unsigned)bells;
      if (total > 65535u) {
        total = 65535u;
      }
      nat->liberty_bells_total = (uint16_t)total;
    }
    {
      unsigned cur = (unsigned)nat->current_crosses + (unsigned)crosses;
      if (cur > 65535u) {
        cur = 65535u;
      }
      nat->current_crosses = (uint16_t)cur;
    }
    if (ctx->europe) {
      nat->current_crosses = ctx->europe->current_crosses;
      nat->needed_crosses = ctx->europe->needed_crosses;
      nat->liberty_bells_total = ctx->europe->liberty_bells_total;
      nat->liberty_bells_last_turn = ctx->europe->liberty_bells_last_turn;
    }
    /* Founding-father election remains a stub until cost tables are recovered. */
    (void)nat->next_founding_father;
  }
}

void turn_run_european_ai_stubs(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units) {
    return;
  }
  for (int n = 0; n < 4; ++n) {
    if (n == ctx->human_nation) {
      continue;
    }
    uint8_t control = 1; /* default AI */
    if (ctx->col1_ok && ctx->col1) {
      control = ctx->col1->player[n].control;
    }
    if (control == 2) {
      continue; /* withdrawn */
    }
    turn_set_active_nation(ctx, n);
    turn_refresh_moves_for_nation(ctx->units, n);
    ai_euro_nation_turn(ctx, n);
  }
}

void turn_run_indian_stub(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units) {
    return;
  }
  const bool show =
    ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.show_indian_moves != 0;
  (void)show; /* animation TBD */
  for (int n = 4; n <= 11; ++n) {
    turn_set_active_nation(ctx, n);
    turn_refresh_moves_for_nation(ctx->units, n);
    ai_indian_nation_turn(ctx, n);
  }
}

void turn_run_king_stub(ColonizeTurnContext* ctx) {
  /* Tax audiences / REF / independence events — not yet recovered. */
  if (ctx) {
    turn_set_active_nation(ctx, ctx->human_nation);
  }
}

static bool turn_euro_ai_should_run(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4 || nation_id == ctx->human_nation) {
    return false;
  }
  uint8_t control = 1;
  if (ctx->col1_ok && ctx->col1) {
    control = ctx->col1->player[nation_id].control;
  }
  return control != 2;
}

static int turn_next_euro_ai(const ColonizeTurnContext* ctx, int start) {
  for (int n = start; n < 4; ++n) {
    if (turn_euro_ai_should_run(ctx, n)) {
      return n;
    }
  }
  return -1;
}

static void turn_finish_status(ColonizeTurnContext* ctx, const ColonizeTurnResult* result) {
  (void)result;
  if (!ctx || !ctx->status || ctx->status_size == 0 || !ctx->game_year || !ctx->game_autumn ||
      !ctx->turn_number) {
    return;
  }
  char date[32];
  turn_format_date(*ctx->game_year, *ctx->game_autumn, date, sizeof(date));
  snprintf(
    ctx->status,
    ctx->status_size,
    "End of Turn — %s (turn %u)",
    date,
    (unsigned)*ctx->turn_number
  );
}

void turn_processor_start(ColonizeTurnProcessor* proc) {
  if (!proc) {
    return;
  }
  memset(proc, 0, sizeof(*proc));
  proc->step = TURN_PROC_SETUP;
}

bool turn_processor_active(const ColonizeTurnProcessor* proc) {
  return proc && proc->step != TURN_PROC_IDLE;
}

bool turn_processor_show_indicator(const ColonizeTurnProcessor* proc) {
  return proc && proc->show_indicator;
}

bool turn_processor_advance(ColonizeTurnProcessor* proc, ColonizeTurnContext* ctx) {
  if (!proc || !ctx || proc->step == TURN_PROC_IDLE) {
    return false;
  }
  if (!ctx->turn_number || !ctx->game_year || !ctx->game_autumn) {
    proc->step = TURN_PROC_IDLE;
    proc->show_indicator = false;
    return false;
  }

  switch (proc->step) {
    case TURN_PROC_SETUP: {
      proc->show_indicator = false;
      proc->year_before = *ctx->game_year;
      turn_advance_calendar(ctx->game_year, ctx->game_autumn, ctx->turn_number);
      proc->result.advanced = true;
      if (ctx->col1_ok && ctx->col1) {
        ctx->col1->head.turn =
          (uint16_t)(*ctx->turn_number > 65535u ? 65535u : *ctx->turn_number);
        ctx->col1->head.year = *ctx->game_year;
        ctx->col1->head.autumn = *ctx->game_autumn;
      }
      turn_run_colony_production(ctx->colonies, ctx->map, &proc->result);
      turn_run_nation_ticks(ctx, &proc->result);
      proc->nation_cursor = 0;
      {
        const int next = turn_next_euro_ai(ctx, 0);
        if (next >= 0) {
          proc->nation_cursor = next;
          proc->step = TURN_PROC_EURO;
        } else {
          proc->nation_cursor = 4;
          proc->step = TURN_PROC_INDIAN;
        }
      }
      break;
    }
    case TURN_PROC_EURO: {
      const int n = proc->nation_cursor;
      proc->show_indicator = true;
      turn_set_active_nation(ctx, n);
      if (ctx->units) {
        turn_refresh_moves_for_nation(ctx->units, n);
      }
      ai_euro_nation_turn(ctx, n);
      {
        const int next = turn_next_euro_ai(ctx, n + 1);
        if (next >= 0) {
          proc->nation_cursor = next;
        } else {
          proc->nation_cursor = 4;
          proc->step = TURN_PROC_INDIAN;
        }
      }
      break;
    }
    case TURN_PROC_INDIAN: {
      const int n = proc->nation_cursor;
      proc->show_indicator = true;
      turn_set_active_nation(ctx, n);
      if (ctx->units) {
        turn_refresh_moves_for_nation(ctx->units, n);
      }
      ai_indian_nation_turn(ctx, n);
      if (n < 11) {
        proc->nation_cursor = n + 1;
      } else {
        proc->step = TURN_PROC_FINISH;
      }
      break;
    }
    case TURN_PROC_FINISH: {
      proc->show_indicator = false;
      turn_run_king_stub(ctx);
      turn_set_active_nation(ctx, ctx->human_nation);
      turn_refresh_moves_for_nation(ctx->units, ctx->human_nation);
      /* Go-To resumes at 10 steps/sec in game_update so the player can watch. */
      turn_select_next_unit(ctx->units, ctx->human_nation);
      if (turn_option_autosave(ctx->col1, ctx->col1_ok)) {
        proc->result.request_autosave_turn = true;
        if (*ctx->game_year != proc->year_before && (*ctx->game_year % 10u) == 0u &&
            *ctx->game_autumn == 0) {
          proc->result.request_autosave_decade = true;
        }
      }
      turn_finish_status(ctx, &proc->result);
      diag_info(
        "turn_end: turn=%u year=%u autumn=%u colonies=%d shortages=%d immigrants=%d",
        (unsigned)*ctx->turn_number,
        (unsigned)*ctx->game_year,
        (unsigned)*ctx->game_autumn,
        proc->result.colonies_produced,
        proc->result.food_shortages,
        proc->result.immigrants_arrived
      );
      proc->step = TURN_PROC_IDLE;
      return false;
    }
    case TURN_PROC_IDLE:
    default:
      proc->step = TURN_PROC_IDLE;
      proc->show_indicator = false;
      return false;
  }
  return proc->step != TURN_PROC_IDLE;
}

ColonizeTurnResult turn_end(ColonizeTurnContext* ctx) {
  ColonizeTurnResult empty;
  memset(&empty, 0, sizeof(empty));
  if (!ctx || !ctx->turn_number || !ctx->game_year || !ctx->game_autumn) {
    return empty;
  }
  ColonizeTurnProcessor proc;
  turn_processor_start(&proc);
  while (turn_processor_advance(&proc, ctx)) {
  }
  return proc.result;
}
