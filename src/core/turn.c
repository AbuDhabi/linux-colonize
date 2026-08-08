#include "core/turn.h"

#include <stdio.h>
#include <string.h>

#include "core/ai.h"
#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/founding_fathers.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"

static void turn_set_active_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (ctx && ctx->active_turn_nation) {
    *ctx->active_turn_nation = nation_id;
  }
}

uint8_t turn_nation_color(int nation_id) {
  return unit_chrome_nation_color(nation_id);
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

void turn_refresh_moves_for_nation(
  ColonizeUnitPool* pool,
  int nation_id,
  const ColonizeCol1Save* col1,
  ColonizeWorldMap* map
) {
  if (!pool) {
    return;
  }
  /* FF combat context for units_try_move (Washington / Drake / Revere). */
  units_set_ff_col1(col1);
  /* Native settlement fallout (FUN_5fef_31ea-shaped). Gold amount unknown. */
  units_set_native_fallout_context(
    col1 ? (ColonizeCol1Save*)col1 : NULL, map, -1
  );
  const bool magellan =
    col1 && founding_fathers_nation_has(col1, nation_id, FF_FERDINAND_MAGELLAN);
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    /* Fortify completes overnight → Fortified; stay asleep until woken. */
    if (u->orders == UNITS_ORDER_FORTIFY) {
      u->orders = UNITS_ORDER_FORTIFIED;
      u->moves_left = 0;
      continue;
    }
    if (units_orders_skip_turn(u)) {
      u->moves_left = 0;
      continue;
    }
    const ColonizeUnitType* type = units_type(pool, u->type_index);
    if (type) {
      /*
       * Natives: COL1 moves = DOS spent thirds; day loop clears spent to 0
       * (decomp ~6357). Brave max allotment is 3 thirds.
       * Europeans: remaining MP = @UNIT movement (+ Magellan sea +1).
       */
      if (nation_id >= 4) {
        u->moves_left = 0;
      } else {
        u->moves_left = type->movement;
        if (magellan && units_is_sea(pool, u->id)) {
          u->moves_left += 1;
        }
      }
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
  const ColonizeCol1Save* col1,
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
      int add = yld;
      /* Henry Hudson: fur trapper output +100% (fandom_col1994 / manual). */
      if (c->field_job == COLONIZE_JOB_FUR_TRAPPER && col1 &&
          founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON)) {
        add *= 2;
      }
      const int cargo = colony_yield_job_cargo(c->field_job);
      if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
        continue;
      }
      colony->stock[cargo] = turn_clamp_stock(colony->stock[cargo] + add);
      if (delta) {
        delta->goods[cargo] += add;
      }
      if (cargo == COLONIZE_CARGO_FOOD) {
        field_food += add;
      } else if (cargo == COLONIZE_CARGO_LUMBER) {
        field_lumber += add;
      } else if (cargo == COLONIZE_CARGO_ORE) {
        field_ore += add;
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

  /* Carpenter hammers: convert lumber toward current project (or bank if none). */
  {
    int lumber_use = 0;
    const int hammers_add = colony_prod_colony_hammers(pool, colony, &lumber_use);
    if (hammers_add > 0) {
      if (lumber_use > colony->stock[COLONIZE_CARGO_LUMBER]) {
        lumber_use = colony->stock[COLONIZE_CARGO_LUMBER];
      }
      /* Without a project, still bank hammers when lumber is available (TURN5→6). */
      int hammers = 0;
      if (colony->building_in_production >= 0) {
        colony->stock[COLONIZE_CARGO_LUMBER] -= lumber_use;
        if (delta) {
          delta->lumber -= lumber_use;
          delta->goods[COLONIZE_CARGO_LUMBER] -= lumber_use;
        }
        hammers = lumber_use > 0 ? lumber_use : hammers_add;
      } else if (lumber_use > 0) {
        colony->stock[COLONIZE_CARGO_LUMBER] -= lumber_use;
        if (delta) {
          delta->lumber -= lumber_use;
          delta->goods[COLONIZE_CARGO_LUMBER] -= lumber_use;
        }
        hammers = lumber_use;
      } else {
        hammers = hammers_add;
      }
      colony->hammers += hammers;
      if (delta) {
        delta->hammers_added = hammers;
      }

      if (colony->building_in_production >= 0 &&
          colonies_try_complete_building(pool, colony->id)) {
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
  const ColonizeCol1Save* col1,
  ColonizeTurnResult* out
) {
  if (!pool) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active) {
      turn_produce_one_colony(pool, &pool->colonies[i], map, col1, out, NULL);
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
  turn_produce_one_colony(pool, colony, map, NULL, out ? out : &local, out_delta);
}

static int turn_count_bells_and_crosses_for_nation(
  const ColonizeColonyPool* pool,
  int nation_id,
  const ColonizeCol1Save* col1,
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
  /* Jefferson / Paine / Penn — fandom_col1994.md Political / Religious FF table. */
  const int statesmen_pct =
    (col1 && founding_fathers_nation_has(col1, nation_id, FF_THOMAS_JEFFERSON)) ? 50 : 0;
  const int paine_tax_pct =
    (col1 && founding_fathers_nation_has(col1, nation_id, FF_THOMAS_PAINE) &&
     nation_id >= 0 && nation_id < (int)COLONIZE_COL1_NATION_COUNT)
      ? (int)col1->nation[nation_id].tax_rate
      : 0;
  const int penn_crosses_pct =
    (col1 && founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_PENN)) ? 50 : 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    bells += colony_prod_colony_bells_ff(pool, c, statesmen_pct, paine_tax_pct);
    crosses += colony_prod_colony_crosses_ff(pool, c, penn_crosses_pct);
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
  turn_count_bells_and_crosses_for_nation(
    ctx->colonies, ctx->human_nation, ctx->col1_ok ? ctx->col1 : NULL, &bells, &crosses
  );

  if (ctx->europe) {
    if (ctx->europe->needed_crosses == 0) {
      ctx->europe->needed_crosses = TURN_DEFAULT_NEEDED_CROSSES;
    }
    /* Deferred needed+1 one turn after first immigrant (TURN5→TURN6 goldens). */
    if (ctx->europe->crosses_pending_needed_bump) {
      unsigned need = (unsigned)ctx->europe->needed_crosses + 1u;
      if (need > 65535u) {
        need = 65535u;
      }
      ctx->europe->needed_crosses = (uint16_t)need;
      ctx->europe->crosses_pending_needed_bump = false;
    }
    /*
     * Base +2 crosses per turn until the first dock immigrant arrives; afterward
     * only colony church crosses accumulate (idle human TURN5–7 stay at 0).
     */
    if (!ctx->europe->crosses_immigrant_seen) {
      unsigned cur = (unsigned)ctx->europe->current_crosses + 2u;
      if (cur > 65535u) {
        cur = 65535u;
      }
      ctx->europe->current_crosses = (uint16_t)cur;
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
      /* Discard remainder; needed bumps on the following turn. */
      ctx->europe->current_crosses = 0;
      ctx->europe->crosses_immigrant_seen = true;
      ctx->europe->crosses_pending_needed_bump = true;
      turn_push_dock_immigrant(ctx->europe, out);
      /* Mirror dock immigrant as Europe-map unit for Col1 capture. */
      if (ctx->units && ctx->europe->dock_count > 0) {
        const EuropeDockImmigrant* d = &ctx->europe->dock[ctx->europe->dock_count - 1];
        const int tid = units_find_type(ctx->units, "Colonists");
        const int type_index = tid >= 0 ? tid : 0;
        const int id = units_spawn_allow_stack(ctx->units, type_index, 236, 236);
        ColonizeUnit* u = units_get(ctx->units, id);
        if (u) {
          u->nation_id = ctx->human_nation;
          u->orders = UNITS_ORDER_SENTRY;
          u->profession = d->profession;
          u->goto_x = 0;
          u->goto_y = 0;
          u->moves_left = 0;
        }
      }
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
    /* Rough FF election from liberty bells (human); full cost/effects PARKED. */
    founding_fathers_tick(ctx);
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
    turn_refresh_moves_for_nation(ctx->units, n, ctx->col1_ok ? ctx->col1 : NULL, ctx->map);
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
    turn_refresh_moves_for_nation(ctx->units, n, ctx->col1_ok ? ctx->col1 : NULL, ctx->map);
    ai_indian_nation_turn(ctx, n);
  }
}

void turn_run_king_stub(ColonizeTurnContext* ctx) {
  ai_king_nation_turn(ctx);
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
      turn_run_colony_production(
        ctx->colonies, ctx->map, ctx->col1_ok ? ctx->col1 : NULL, &proc->result
      );
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
        turn_refresh_moves_for_nation(ctx->units, n, ctx->col1_ok ? ctx->col1 : NULL, ctx->map);
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
        turn_refresh_moves_for_nation(ctx->units, n, ctx->col1_ok ? ctx->col1 : NULL, ctx->map);
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
      turn_refresh_moves_for_nation(
        ctx->units, ctx->human_nation, ctx->col1_ok ? ctx->col1 : NULL, ctx->map
      );
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
