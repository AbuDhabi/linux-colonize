#include "core/popup_msg.h"
#include "core/turn.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ai.h"
#include "core/ai_diplo.h"
#include "core/ai_king.h"
#include "core/col1_stuff_census.h"
#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"

static void turn_set_active_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (ctx && ctx->active_turn_nation) {
    *ctx->active_turn_nation = nation_id;
  }
}

/*
 * FUN_3844_00f2 per-unit fog reveal (281f_07a0 → 13f1_02f8). Radius 1 thin
 * (deep LOS PARKED). Also reveal own colonies radius 2 (Finish human chrome).
 * Cite: nation_eot.c unit walk; game_loop human fog.
 */
static void turn_reveal_fog_for_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || nation_id < 0) {
    return;
  }
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id || !units_is_on_map(u)) {
        continue;
      }
      map_reveal_radius(ctx->map, u->x, u->y, nation_id, 1);
    }
  }
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      map_reveal_radius(ctx->map, c->x, c->y, nation_id, 2);
    }
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
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!pool) {
    return;
  }
  /* FF combat context for units_try_move (Washington / Drake / Revere). */
  units_set_ff_col1(col1);
  units_set_occupancy_map(map);
  if (col1) {
    int human = -1;
    for (int i = 0; i < 4; ++i) {
      if (col1->player[i].control == 0) {
        human = i;
        break;
      }
    }
    units_set_combat_human_nation(human);
  }
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
    /* Pioneer clear/plow/road: overnight work-tick (FUN_479b_01a6 / 0526). */
    if (map &&
        (u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD)) {
      (void)units_pioneer_work_tick(
        pool, u->id, map, NULL, 0, colonies, ai_popups, messages
      );
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

/*
 * DOS 0x5384 colony-report bits: msgs fire when the matching bit is clear
 * (defaults 0 in COL saves). Cite: viceroy ~57558/57696; colony_eot_production.md.
 */
static int turn_report_ok_trained(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_when_colonists_trained;
}
static int turn_report_ok_raw(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_raw_materials_shortages;
}
static int turn_report_ok_food(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_food_shortages;
}
static int turn_report_ok_new_cargo(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_new_cargos_available;
}
/* DOS 0x5385 bit1 clear → show rebel majority / unanimous / tory chrome. */
static int turn_report_ok_rebel_maj(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_rebel_majorities;
}
/* DOS 0x5385 bit0 clear → show @SONSUP / @SONSDOWN decade chrome. */
static int turn_report_ok_sons(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_sons_of_liberty_membership;
}
/* DOS 0x5384 bit3 clear → show @INEFFICIENT / @EFFICIENT. */
static int turn_report_ok_inefficient(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_inefficient_government;
}

/*
 * FUN_364b_0688 Phase D Tory pressure: inefficient-gov chrome (0xdd1 / 0xddd).
 * Port latch is colony->inefficient_gov (Col1 +0x1c bit3 = starvation).
 */
static void turn_emit_inefficient_gov_chrome(
  ColonizeColony* colony,
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  int sol_after,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!colony || colony->nation_id != human_nation || !europe || !col1) {
    return;
  }
  int pop = colony->population > 0 ? colony->population : colony->colonist_count;
  if (pop < 0) {
    pop = 0;
  }
  int sol = sol_after;
  if (sol < 0) {
    sol = 0;
  }
  if (sol > 100) {
    sol = 100;
  }
  /* Decomp local_82: trunc tories (not half-up used in colony_prod_sol_bonus). */
  const int tories = (pop * (100 - sol)) / 100;
  int thresh = 10;
  if (colony->nation_id >= 0 && colony->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
      col1->player[colony->nation_id].control == 0) {
    int diff = (int)col1->head.difficulty;
    if (diff < 0) {
      diff = 0;
    }
    if (diff > 4) {
      diff = 4;
    }
    thresh = 10 - diff;
  }
  if (thresh < 1) {
    thresh = 1;
  }

  const char* cname = colony->name[0] ? colony->name : "colony";
  const char* section = NULL;
  char status_buf[sizeof(europe->status)];
  status_buf[0] = '\0';

  if (tories < thresh) {
    if (colony->inefficient_gov != 0) {
      colony->inefficient_gov = 0;
      if (turn_report_ok_inefficient(col1)) {
        section = "EFFICIENT";
        snprintf(
          status_buf,
          sizeof(status_buf),
          "%s government efficiency improved.",
          cname
        );
      }
    }
  } else {
    if (colony->inefficient_gov == 0) {
      colony->inefficient_gov = 1;
      if (turn_report_ok_inefficient(col1)) {
        section = "INEFFICIENT";
        snprintf(
          status_buf,
          sizeof(status_buf),
          "%s has inefficient government (%d+ tories).",
          cname,
          thresh
        );
      }
    }
  }

  if (!status_buf[0]) {
    return;
  }
  snprintf(europe->status, sizeof(europe->status), "%s", status_buf);
  if (!ai_popups || !section) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  tok.number0 = thresh;
  tok.has_number0 = true;
  popup_msg_fill(messages, section, &tok, status_buf, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

/*
 * FUN_364b_0688 Phase D: one SoL latch or decade chrome popup for the human.
 * Match decomp nest: latch transitions before decade. Cite: colony_eot_production.md.
 */
static void turn_emit_sol_phase_d_chrome(
  ColonizeColony* colony,
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  int sol_before,
  uint8_t flags_before,
  int sol_after,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!colony || colony->nation_id != human_nation || !europe) {
    return;
  }
  const char* section = NULL;
  const char* fallback = NULL;
  char status_buf[sizeof(europe->status)];
  status_buf[0] = '\0';
  const char* cname = colony->name[0] ? colony->name : "colony";
  const int had50 = (flags_before & COLONIZE_COLONY_FLAG_SOL_50) != 0;
  const int had100 = (flags_before & COLONIZE_COLONY_FLAG_SOL_100) != 0;

  if ((sol_after < 50) || had50) {
    if ((sol_after < 100) || had100) {
      if (sol_after < 95 && had100) {
        if (turn_report_ok_rebel_maj(col1)) {
          section = "TORYMINORITY";
          snprintf(
            status_buf,
            sizeof(status_buf),
            "SoL in %s down from 100%% to %d%%.",
            cname,
            sol_after
          );
          fallback = status_buf;
        }
      } else if (sol_after < 50 && had50) {
        if (turn_report_ok_rebel_maj(col1)) {
          section = "TORYMAJORITY";
          snprintf(
            status_buf,
            sizeof(status_buf),
            "SoL in %s down to %d%%.",
            cname,
            sol_after
          );
          fallback = status_buf;
        }
      } else if (sol_before / 10 < sol_after / 10) {
        if (turn_report_ok_sons(col1)) {
          section = "SONSUP";
          snprintf(
            status_buf, sizeof(status_buf), "SoL in %s up to %d%%.", cname, sol_after
          );
          fallback = status_buf;
        }
      } else if ((sol_after + 4) / 10 < sol_before / 10) {
        if (turn_report_ok_sons(col1)) {
          section = "SONSDOWN";
          snprintf(
            status_buf, sizeof(status_buf), "SoL in %s down to %d%%.", cname, sol_after
          );
          fallback = status_buf;
        }
      }
    } else if (turn_report_ok_rebel_maj(col1)) {
      section = "REBELUNANIMOUS";
      snprintf(status_buf, sizeof(status_buf), "SoL in %s up to 100%%.", cname);
      fallback = status_buf;
    }
  } else if (turn_report_ok_rebel_maj(col1)) {
    section = "REBELMAJORITY";
    snprintf(
      status_buf, sizeof(status_buf), "SoL in %s up to %d%%.", cname, sol_after
    );
    fallback = status_buf;
  }

  if (!fallback || !fallback[0]) {
    return;
  }
  snprintf(europe->status, sizeof(europe->status), "%s", fallback);
  if (!ai_popups || !section) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  if (col1 && colony->nation_id >= 0 &&
      colony->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
      col1->player[colony->nation_id].country_name[0]) {
    tok.string1 = col1->player[colony->nation_id].country_name;
  } else if (europe->nation_name[0]) {
    tok.string1 = europe->nation_name;
  } else {
    tok.string1 = "Europe";
  }
  tok.number0 = sol_after;
  tok.has_number0 = true;
  popup_msg_fill(messages, section, &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

static void turn_produce_one_colony(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  ColonizeTurnResult* out,
  ColonizeColonyProdDelta* delta,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages,
  ColonizeDosRng* rng
) {
  if (delta) {
    memset(delta, 0, sizeof(*delta));
  }
  if (!pool || !colony || !colony->active) {
    return;
  }
  /* FUN_364b_0688: clear cargo_produced_mask (+0x90) at production start. */
  colony->cargo_produced_mask = 0;
  int stock_before[COLONIZE_CARGO_COUNT];
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    stock_before[c] = colony->stock[c];
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
    const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);
    ColonizeTownCommonsYield tc;
    colony_yield_town_commons(map, colony->x, colony->y, sol_b_field, colony->colony_flags, &tc);
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

    /* Coastal colonies can fish water surrounds; inland colonies require Docks */
    bool has_docks = (colony->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) != 0 ||
                     map_tile_is_coastal(map, colony->x, colony->y);
    if (!has_docks) {
      for (int bi = 0; bi < pool->building_type_count && bi < COLONIZE_BUILDING_TYPES_MAX; ++bi) {
        if (!colony->has_building[bi]) {
          continue;
        }
        const char* bn = pool->building_types[bi].name;
        if (bn && (strstr(bn, "Docks") != NULL || strstr(bn, "Drydock") != NULL ||
                   strstr(bn, "Shipyard") != NULL)) {
          has_docks = true;
          break;
        }
      }
    }
    int mine_depleted = 0;
    bool worked_colonist[32];
    memset(worked_colonist, 0, sizeof(worked_colonist));
    for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
      const int who = (int)colony->tiles[ti];
      if (who < 0 || who >= colony->colonist_count || (who < 32 && worked_colonist[who])) {
        continue;
      }
      if (who < 32) {
        worked_colonist[who] = true;
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
      /* DOS net SoL/Tory mod (sons_of_liberty.md). Field-specific variant:
       * zeroed outright for AI colonies, unlike manufacturing/bells/crosses/
       * hammers — see colony_prod_sol_bonus_field. Folded into
       * colony_yield_for_worker directly now (2026-08-15): DOS applies a
       * *positive* mod before expert doubling (so it gets swept up by an
       * expert's ×2/road-river-unit-doubling, not added flat after —
       * player-confirmed, see colony_yield.c's colony_yield_pipeline
       * comment); a negative mod (Tory penalty) still lands at the very
       * end, same net position as this function's old external add. */
      const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);
      const int yld = colony_yield_for_worker(
        map,
        colony->x + dx,
        colony->y + dy,
        c->field_job,
        c->profession,
        has_docks,
        sol_b_field,
        colony->colony_flags
      );
      if (yld <= 0) {
        continue;
      }
      int add = yld;
      /* Henry Hudson: fur trapper output +100% (fandom_col1994 / manual).
       * Still applied post-hoc here, same as before the SoL-fold change —
       * DOS applies Hudson inside FUN_15eb_18ec too, but exactly where
       * relative to the SoL mod isn't pinned down (see terrain_yields.md
       * point 10); this now doubles an already-sol-adjusted yield rather
       * than adding sol after doubling, a narrow behavior change only for
       * Fur Trapper+Hudson+nonzero sol_bonus, not independently verified
       * either way. */
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
      /*
       * Col1 +0x97: INC per ore/silver field yield *from a special-resource
       * deposit tile* — wrap at 50 → MAP_LAYER2_SUPPRESS on that tile
       * (FUN_364b_033a feature 4). Chrome: GAME.TXT @DEPLETION (DOS 0xd75).
       * A depletable deposit is what's being "used up"; an ordinary
       * hills/mountain tile with no bonus resource isn't. Player-confirmed
       * 2026-08-16 (colony-prod-tests real DOS save): every Dutch colony
       * mining plain ore/silver tiles ended the turn at depletion_counter
       * 0, not incremented — the old code counted any ore/silver yield.
       */
      if (add > 0 &&
          (cargo == COLONIZE_CARGO_ORE || cargo == COLONIZE_CARGO_SILVER) &&
          map_resource_type_for_yield(map, colony->x + dx, colony->y + dy) >= 0) {
        colony->depletion_counter =
          (uint8_t)(colony->depletion_counter + 1u);
        if (colony->depletion_counter > 0x31u) {
          colony->depletion_counter =
            (uint8_t)(colony->depletion_counter - 0x32u);
          if (map) {
            /* Production API takes const map; deplete mutates layer2. */
            map_occupancy_set_layer2(
              (ColonizeWorldMap*)(uintptr_t)map,
              colony->x + dx,
              colony->y + dy,
              MAP_LAYER2_SUPPRESS,
              true
            );
          }
          mine_depleted = 1;
        }
      }
    }
    if (mine_depleted && europe && colony->nation_id == human_nation) {
      const char* cname = colony->name[0] ? colony->name : "colony";
      snprintf(europe->status, sizeof(europe->status), "Mine depleted near %s.", cname);
      if (ai_popups) {
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = cname;
        popup_msg_fill(messages, "DEPLETION", &tok, europe->status, body, sizeof(body));
        ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      }
    }
  }

  /*
   * FUN_364b_0688 Phase B: AI Euro food += difficulty>>1 (DS 0x53a6).
   * Cite: colony_eot_production.md; difficulty.md.
   */
  if (col1 && colony->nation_id >= 0 &&
      colony->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
      col1->player[colony->nation_id].control != 0) {
    int diff = (int)col1->head.difficulty;
    if (diff < 0) {
      diff = 0;
    }
    if (diff > 4) {
      diff = 4;
    }
    const int ai_food = diff >> 1;
    if (ai_food > 0) {
      colony->stock[COLONIZE_CARGO_FOOD] =
        turn_clamp_stock(colony->stock[COLONIZE_CARGO_FOOD] + ai_food);
      field_food += ai_food;
      if (delta) {
        delta->goods[COLONIZE_CARGO_FOOD] += ai_food;
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
    if (europe && colony->nation_id == human_nation && turn_report_ok_food(col1)) {
      if (colony->name[0]) {
        snprintf(europe->status, sizeof(europe->status), "Food shortage in %s.", colony->name);
      } else {
        snprintf(europe->status, sizeof(europe->status), "Food shortage.");
      }
    }
  }
  const int food_surplus_turn = field_food - consumed;
  /*
   * FUN_364b_0688: starvation latch (+0x1c bit3) from food vs pop need.
   * Phase J kills when still short after this turn *and* food was already 0
   * at turn start (local_6c==0 / local_12e); pop==kills → @VANISH + abandon.
   * Easy-difficulty no-kill mercy ported below. Cite: ~57623–57694.
   */
  {
    const int need = pop * TURN_FOOD_PER_COLONIST;
    const int food_at_start = stock_before[COLONIZE_CARGO_FOOD];
    const int was_starving =
      (colony->colony_flags & COLONIZE_COLONY_FLAG_STARVATION) != 0;
    int starved_this_tick = 0;
    if (colony->stock[COLONIZE_CARGO_FOOD] < need) {
      colony->colony_flags |= COLONIZE_COLONY_FLAG_STARVATION;
    } else {
      colony->colony_flags =
        (uint8_t)(colony->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_STARVATION);
    }

    /*
     * FUN_364b_0688 phase I — birth: food ≥ 200 → Free Colonist in colony;
     * subtract 200 food (docs/building_production.md; decomp ~57615–57622).
     */
    if (colony->stock[COLONIZE_CARGO_FOOD] >= 200 &&
        colony->colonist_count < COLONIZE_COLONY_POP_MAX) {
      colony->stock[COLONIZE_CARGO_FOOD] =
        turn_clamp_stock(colony->stock[COLONIZE_CARGO_FOOD] - 200);
      if (delta) {
        delta->goods[COLONIZE_CARGO_FOOD] -= 200;
        delta->food_net -= 200;
      }
      ColonizeColonist* newborn = &colony->colonists[colony->colonist_count];
      memset(newborn, 0, sizeof(*newborn));
      newborn->active = true;
      newborn->unit_type_index = 0;
      newborn->profession = UNITS_JOB_COLONIST; /* Free Colonists */
      newborn->building_type = -1;
      newborn->field_job = -1;
      colony->colonist_count++;
      colony->population = colony->colonist_count;
      if (europe && colony->nation_id == human_nation) {
        /* DOS 0xe2f @NEWCOLONIST. Cite: colony_eot_production.md Phase I. */
        if (colony->name[0]) {
          snprintf(europe->status, sizeof(europe->status), "Birth in %s.", colony->name);
        } else {
          snprintf(europe->status, sizeof(europe->status), "Colony birth.");
        }
        if (ai_popups) {
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = colony->name[0] ? colony->name : "colony";
          popup_msg_fill(
            messages, "NEWCOLONIST", &tok, europe->status, body, sizeof(body)
          );
          ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        }
      }
    }

    /*
     * Easy-difficulty no-kill mercy (FUN_364b_0688, decomp ~57641-57647):
     * on Discoverer/Explorer (difficulty < 2), the kill below never fires
     * before year 1520; from 1520 on it's a `dos_rng_range(0, 2-difficulty)`
     * roll, nonzero cancels (2/3 odds at Discoverer, 1/2 at Explorer). NULL
     * col1/rng safely fall through to the plain kill (old behavior).
     */
    int starve_mercy = 0;
    if (col1 && col1->head.difficulty < 2) {
      if (col1->head.year < 1520) {
        starve_mercy = 1;
      } else if (dos_rng_range(rng, 0, 2 - col1->head.difficulty) != 0) {
        starve_mercy = 1;
      }
    }

    /*
     * Phase J — starve-kill when still short and started the turn at 0 food.
     * Last colonist → @VANISH + colonies_abandon (DOS 0xe47 / thunk 0254).
     */
    if ((colony->colony_flags & COLONIZE_COLONY_FLAG_STARVATION) != 0 &&
        food_at_start == 0 && colony->colonist_count > 0 && !starve_mercy) {
      const int colony_id = colony->id;
      char vanish_name[COLONIZE_COLONY_NAME_MAX];
      snprintf(
        vanish_name,
        sizeof(vanish_name),
        "%s",
        colony->name[0] ? colony->name : "colony"
      );
      const int kill_i = colony->colonist_count - 1;
      for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
        if ((int)colony->tiles[ti] == kill_i) {
          colony->tiles[ti] = (int8_t)-1;
        } else if ((int)colony->tiles[ti] > kill_i) {
          colony->tiles[ti] = (int8_t)((int)colony->tiles[ti] - 1);
        }
      }
      for (int i = kill_i; i < colony->colonist_count - 1; ++i) {
        colony->colonists[i] = colony->colonists[i + 1];
      }
      memset(
        &colony->colonists[colony->colonist_count - 1], 0, sizeof(colony->colonists[0])
      );
      colony->colonist_count--;
      colony->population = colony->colonist_count;
      starved_this_tick = 1;
      if (colony->colonist_count <= 0) {
        if (europe && colony->nation_id == human_nation) {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "Colony %s vanished.",
            vanish_name
          );
          if (ai_popups) {
            char body[AI_POPUP_BODY_LEN];
            PopupMsgTokens tok;
            memset(&tok, 0, sizeof(tok));
            tok.string0 = vanish_name;
            popup_msg_fill(messages, "VANISH", &tok, europe->status, body, sizeof(body));
            ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
          }
        }
        (void)colonies_abandon(pool, colony_id);
        return;
      }
      if (europe && colony->nation_id == human_nation) {
        if (colony->name[0]) {
          snprintf(europe->status, sizeof(europe->status), "Starvation in %s.", colony->name);
        } else {
          snprintf(europe->status, sizeof(europe->status), "Colonist starved.");
        }
        if (ai_popups) {
          const char* sec = (col1 && col1->head.autumn) ? "STARVE2" : "STARVE1";
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = colony->name[0] ? colony->name : "colony";
          popup_msg_fill(messages, sec, &tok, europe->status, body, sizeof(body));
          ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        }
      }
    }

    /*
     * First starvation latch (stock < need, not yet killing): @FOOD1 / @FOOD2.
     * Else DOS 0xe5e @FOODLOW when eating into stores:
     *   8e5a==0 (stock covers this turn) and 8e32!=0 (production < consumption)
     *   and post-eat stock < 8e32×4. Cite: FUN_15eb_0b52; ~57626–57636.
     * Surplus production (8e32==0) never warns — even if stock is modest.
     */
    if (!starved_this_tick && need > 0 && europe &&
        colony->nation_id == human_nation && turn_report_ok_food(col1)) {
      const int stock = colony->stock[COLONIZE_CARGO_FOOD];
      const int food_shortfall = consumed - field_food; /* DOS 8e32 when >0 */
      if (stock < need && !was_starving) {
        if (colony->name[0]) {
          snprintf(
            europe->status, sizeof(europe->status), "Food depleted in %s.", colony->name
          );
        } else {
          snprintf(europe->status, sizeof(europe->status), "Food stores depleted.");
        }
        if (ai_popups) {
          const char* sec = (col1 && col1->head.autumn) ? "FOOD2" : "FOOD1";
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = colony->name[0] ? colony->name : "colony";
          popup_msg_fill(messages, sec, &tok, europe->status, body, sizeof(body));
          ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        }
      } else if (
        stock >= need && food_shortfall > 0 && stock < food_shortfall * 4
      ) {
        if (colony->name[0]) {
          snprintf(europe->status, sizeof(europe->status), "Food low in %s.", colony->name);
        } else {
          snprintf(europe->status, sizeof(europe->status), "Food stores low.");
        }
        if (ai_popups) {
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = colony->name[0] ? colony->name : "colony";
          tok.number0 = stock;
          tok.has_number0 = true;
          popup_msg_fill(messages, "FOODLOW", &tok, europe->status, body, sizeof(body));
          ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        }
      }
    }
  }

  {
    const int sol_before = colony_prod_sol_percent(col1, colony);
    const uint8_t flags_before = colony->colony_flags;
    colony_prod_tick_rebel_accumulators(pool, colony, col1);
    const int sol_after = colony_prod_sol_percent(col1, colony);
    colony_prod_refresh_sol_flags(colony, col1);
    turn_emit_sol_phase_d_chrome(
      colony,
      col1,
      europe,
      human_nation,
      sol_before,
      flags_before,
      sol_after,
      ai_popups,
      messages
    );
    turn_emit_inefficient_gov_chrome(
      colony, col1, europe, human_nation, sol_after, ai_popups, messages
    );
  }

  /*
   * Horse breeding (manual / fandom): ≥2 horses + food surplus this turn →
   * breed floor(surplus/2) horses, each costing 1 food. Cap without Stable
   * allows up to surplus/2 horses (cap 8 with stable, 6 without).
   */
  if (colony->stock[COLONIZE_CARGO_HORSES] >= 2 && food_surplus_turn > 0) {
    const int has_stable = turn_building_name_has(pool, colony, "Stable");
    const int cap = has_stable ? 8 : 6;
    int breed = (food_surplus_turn + 1) / 2;
    if (breed > cap) {
      breed = cap;
    }
    if (breed > colony->stock[COLONIZE_CARGO_FOOD]) {
      breed = colony->stock[COLONIZE_CARGO_FOOD];
    }
    const int max_horses = turn_building_name_has(pool, colony, "Warehouse Expansion") ? 300 :
                           (turn_building_name_has(pool, colony, "Warehouse") ? 200 : 100);
    if (colony->stock[COLONIZE_CARGO_HORSES] + breed > max_horses) {
      breed = max_horses - colony->stock[COLONIZE_CARGO_HORSES];
      if (breed < 0) {
        breed = 0;
      }
    }
      if (breed > 0) {
        colony->stock[COLONIZE_CARGO_FOOD] =
          turn_clamp_stock(colony->stock[COLONIZE_CARGO_FOOD] - breed);
        colony->stock[COLONIZE_CARGO_HORSES] =
          turn_clamp_stock(colony->stock[COLONIZE_CARGO_HORSES] + breed);
        if (delta) {
          delta->goods[COLONIZE_CARGO_FOOD] -= breed;
          delta->food_net -= breed;
          delta->goods[COLONIZE_CARGO_HORSES] += breed;
        }
        if (europe && colony->nation_id == human_nation) {
          snprintf(
            europe->status,
            sizeof(europe->status),
            has_stable ? "Stable bred %d horses." : "Horses bred: %d.",
            breed
          );
        }
      }
  }

  /*
   * FUN_364b_0688 phases F–G thin: Schoolhouse/College/University education.
   * Teachers (profession Teacher) in school buildings; students = Free/
   * Indentured/Criminal/Convert in school. turns_in_job++ each tick; when
   * teacher ≥ 4/6/8 (by tier), graduate one student → Farmer (Schoolhouse),
   * Carpenter (College+). Deep specialty table / msgs PARKED.
   * Cite: ~57502–57589; docs/building_production.md; colony_eot_production.md.
   */
  if (pool) {
    int school_tier = 0; /* 1 Schoolhouse, 2 College, 3 University */
    for (int bi = 0; bi < pool->building_type_count && bi < COLONIZE_BUILDING_TYPES_MAX; ++bi) {
      if (!colony->has_building[bi]) {
        continue;
      }
      const char* bn = pool->building_types[bi].name;
      if (!bn) {
        continue;
      }
      if (strstr(bn, "University") != NULL) {
        school_tier = 3;
      } else if (strstr(bn, "College") != NULL && school_tier < 2) {
        school_tier = 2;
      } else if (strstr(bn, "Schoolhouse") != NULL && school_tier < 1) {
        school_tier = 1;
      }
    }
    if (school_tier > 0) {
      const int need = (school_tier == 1) ? 4 : (school_tier == 2) ? 6 : 8;
      int teachers[8];
      int students[32];
      int n_teach = 0;
      int n_stud = 0;
      for (int ci = 0; ci < colony->colonist_count; ++ci) {
        ColonizeColonist* c = &colony->colonists[ci];
        if (!c->active) {
          continue;
        }
        if (c->turns_in_job < 255) {
          c->turns_in_job++;
        }
        if (c->building_type < 0 || c->building_type >= pool->building_type_count) {
          continue;
        }
        const char* bn = pool->building_types[c->building_type].name;
        if (!bn ||
            (strstr(bn, "Schoolhouse") == NULL && strstr(bn, "College") == NULL &&
             strstr(bn, "University") == NULL)) {
          continue;
        }
        if (c->profession == COLONIZE_PROF_TEACHER) {
          if (n_teach < 8) {
            teachers[n_teach++] = ci;
          }
        } else if (
          c->profession == COLONIZE_PROF_FREE_COLONIST ||
          c->profession == COLONIZE_PROF_INDENTURED ||
          c->profession == COLONIZE_PROF_CRIMINAL ||
          c->profession == COLONIZE_PROF_CONVERT ||
          c->profession < 0
        ) {
          if (n_stud < 32) {
            students[n_stud++] = ci;
          }
        }
      }
      const int students_at_start = n_stud;
      for (int t = 0; t < n_teach && n_stud > 0; ++t) {
        ColonizeColonist* teacher = &colony->colonists[teachers[t]];
        if ((int)teacher->turns_in_job < need) {
          continue;
        }
        ColonizeColonist* student = &colony->colonists[students[n_stud - 1]];
        const int prev_prof = student->profession;
        enum { GRAD_SPECIALTY = 0, GRAD_CRIMINAL, GRAD_INDENTURED } grad = GRAD_SPECIALTY;
        const char* chrome_sec = "TRAINPROFESSION";
        const char* skill_name = "profession";
        if (prev_prof == COLONIZE_PROF_CRIMINAL) {
          /* DOS 0x1a → 0x19 / @TRAINCRIMINAL. */
          student->profession = COLONIZE_PROF_INDENTURED;
          chrome_sec = "TRAINCRIMINAL";
          grad = GRAD_CRIMINAL;
        } else if (prev_prof == COLONIZE_PROF_INDENTURED) {
          /* DOS 0x19 → Free / @TRAININDENTURED. */
          student->profession = COLONIZE_PROF_FREE_COLONIST;
          chrome_sec = "TRAININDENTURED";
          grad = GRAD_INDENTURED;
        } else {
          /* Specialty: teacher field_job 0..8 if set; else Farmer / Carpenter. */
          int skill = (school_tier >= 2) ? COLONIZE_PROF_CARPENTER : COLONIZE_JOB_FARMER;
          if (teacher->field_job >= 0 && teacher->field_job < COLONIZE_FIELD_JOB_COUNT) {
            skill = teacher->field_job;
          }
          student->profession = skill;
          if (skill >= 0 && skill < COLONIZE_FIELD_JOB_COUNT) {
            skill_name = colony_yield_job_name(skill);
          } else if (skill == COLONIZE_PROF_CARPENTER) {
            skill_name = "Carpenter";
          }
        }
        student->turns_in_job = 0;
        teacher->turns_in_job = 0;
        n_stud--;
        if (europe && colony->nation_id == human_nation && turn_report_ok_trained(col1)) {
          const char* tier_name =
            (school_tier >= 3) ? "University" : (school_tier == 2) ? "College" : "Schoolhouse";
          if (grad == GRAD_CRIMINAL) {
            snprintf(europe->status, sizeof(europe->status), "Criminal educated.");
          } else if (grad == GRAD_INDENTURED) {
            snprintf(europe->status, sizeof(europe->status), "Indentured educated.");
          } else {
            snprintf(europe->status, sizeof(europe->status), "%s graduate.", tier_name);
          }
          if (ai_popups) {
            const char* cname = colony->name[0] ? colony->name : "colony";
            char body[AI_POPUP_BODY_LEN];
            char fallback[160];
            if (grad == GRAD_CRIMINAL) {
              snprintf(
                fallback, sizeof(fallback), "Criminal in %s became indentured.", cname
              );
            } else if (grad == GRAD_INDENTURED) {
              snprintf(
                fallback, sizeof(fallback), "Indentured in %s became free colonist.", cname
              );
            } else {
              snprintf(fallback, sizeof(fallback), "%s learned %s.", cname, skill_name);
            }
            PopupMsgTokens tok;
            memset(&tok, 0, sizeof(tok));
            tok.string0 = cname;
            tok.string1 = skill_name;
            popup_msg_fill(messages, chrome_sec, &tok, fallback, body, sizeof(body));
            ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
          }
        }
      }
      /* Teacher ready but no students at start → DOS 0xde7 / @TRAINFAIL. */
      if (students_at_start == 0 && europe && colony->nation_id == human_nation &&
          turn_report_ok_trained(col1)) {
        for (int t = 0; t < n_teach; ++t) {
          ColonizeColonist* teacher = &colony->colonists[teachers[t]];
          if ((int)teacher->turns_in_job >= need) {
            snprintf(europe->status, sizeof(europe->status), "No students to teach.");
            if (ai_popups) {
              const char* cname = colony->name[0] ? colony->name : "colony";
              char body[AI_POPUP_BODY_LEN];
              char fallback[160];
              snprintf(fallback, sizeof(fallback), "No students to teach in %s.", cname);
              PopupMsgTokens tok;
              memset(&tok, 0, sizeof(tok));
              tok.string0 = cname;
              popup_msg_fill(messages, "TRAINFAIL", &tok, fallback, body, sizeof(body));
              ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
            }
            break;
          }
        }
      }
    }

    /*
     * Phase H — random field skill discover (DOS FUN_364b_0688 ~57590-57614,
     * see colony_eot_production.md Deep H). Raw bytes: skip profession 0x1b
     * (Convert); else roll dos_rng_range(0, N) where N is 99 (base — Free
     * Colonist / unset), 199 (0x19 Indentured Servant), or 299 (0x1a Petty
     * Criminal, the two ifs are sequential, not else-if, so Criminal takes
     * base+200); success on 0 → gain the field profession. Previously this
     * only fired for Free Colonist and used an ad hoc non-DOS PRNG instead
     * of the shared `rng` this function already threads through for the
     * starve-mercy roll a few lines up — fixed to use dos_rng_range(rng, ...)
     * like DOS's own FUN_281f_04d4 call, and to extend the class-scaled odds
     * to Indentured/Criminal (Convert stays excluded — not Free/Indentured/
     * Criminal). The DOS field-job lower bound reads as `1..4` in the raw
     * bytes (specialty via 0c0e), not `0..4`; whether that's the same value
     * as `field_job` here is still unresolved (see colony_eot_production.md
     * Deep H / Deep F "0c0e -> specialty" vs "0c54 -> current job" split) —
     * left at the port's existing 0..4 scan rather than guess. Nation
     * skill-flags / deep school-job tables still PARKED.
     */
    for (int ci = 0; ci < colony->colonist_count; ++ci) {
      ColonizeColonist* c = &colony->colonists[ci];
      if (!c->active) {
        continue;
      }
      /* No RNG supplied (e.g. a caller that wants deterministic production
       * with no stochastic side effects) -> skip rather than let
       * dos_rng_range(NULL, 0, N)'s "always returns lo" convention read as
       * a guaranteed hit. */
      if (!rng) {
        continue;
      }
      int discover_denom = 99;
      if (c->profession == COLONIZE_PROF_INDENTURED) {
        discover_denom = 199;
      } else if (c->profession == COLONIZE_PROF_CRIMINAL) {
        discover_denom = 299;
      } else if (c->profession != COLONIZE_PROF_FREE_COLONIST && c->profession >= 0) {
        continue;
      }
      if (c->field_job < 0 || c->field_job > COLONIZE_JOB_FUR_TRAPPER) {
        continue;
      }
      if (dos_rng_range(rng, 0, discover_denom) == 0) {
        c->profession = c->field_job;
        c->turns_in_job = 0;
        if (europe && colony->nation_id == human_nation && turn_report_ok_trained(col1)) {
          snprintf(europe->status, sizeof(europe->status), "Colonist learned a skill.");
          if (ai_popups) {
            const char* cname = colony->name[0] ? colony->name : "colony";
            const char* skill_name = colony_yield_job_name(c->field_job);
            if (!skill_name || !skill_name[0]) {
              skill_name = "profession";
            }
            char body[AI_POPUP_BODY_LEN];
            char fallback[160];
            snprintf(fallback, sizeof(fallback), "%s learned %s.", cname, skill_name);
            PopupMsgTokens tok;
            memset(&tok, 0, sizeof(tok));
            tok.string0 = cname;
            tok.string1 = skill_name;
            popup_msg_fill(messages, "TRAINPROFESSION", &tok, fallback, body, sizeof(body));
            ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
          }
        }
      }
    }
  }

  /* Settlement manufacturing (raw → goods) before hammers consume lumber. */
  colony_craft_one_colony(pool, colony, delta, colony_prod_sol_bonus(col1, colony));
  if (delta) {
    delta->lumber = delta->goods[COLONIZE_CARGO_LUMBER];
    delta->ore = delta->goods[COLONIZE_CARGO_ORE];
    delta->food_net = delta->goods[COLONIZE_CARGO_FOOD];
  }

  /*
   * FUN_364b_0688 Phase B: cargo_produced_mask (+0x90) sets bits for cargos
   * whose net production is positive this tick.
   */
  colony->cargo_produced_mask = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    const int net = delta ? delta->goods[c] : (colony->stock[c] - stock_before[c]);
    if (net > 0) {
      colony->cargo_produced_mask |= (uint16_t)(1u << c);
    }
  }

  /*
   * Carpenter hammers: convert lumber toward current project (or bank if
   * none). sol_b folds into each Carpenter worker individually, inside
   * colony_prod_colony_hammers (matches FUN_15eb_1d4c's Carpenter body —
   * see manufacturing_worker_calc_1d4c.md).
   *
   * Spring-only (post-1600 biannual calendar, col1_save.h head.autumn):
   * player-confirmed 2026-08-16 against a real DOS save (colony-prod-tests,
   * a Spring→Autumn turn) — every one of 13 Dutch colonies, spanning wildly
   * different populations/buildings/queued projects/worker setups
   * (including multiple with skilled Carpenters and ample lumber), ended
   * that Autumn turn with `hammers` byte-for-byte unchanged from Spring.
   * Regular field/craft goods production is NOT seasonal (stock changed
   * normally on the same turn) — only hammers freezes on Autumn.
   */
  if (!col1 || col1->head.autumn == 0) {
    const int sol_b = colony_prod_sol_bonus(col1, colony);
    int hammers_add = colony_prod_colony_hammers(pool, colony, sol_b, NULL);
    if (hammers_add > 0) {
      /*
       * Hammers cost lumber 1:1, capped by lumber actually on hand (this
       * turn's field-yield lumber counts — TURN5→6: a lone Lumberjack+
       * Carpenter pair goes 0 lumber/0 hammers to lumber=3/hammers=+3 in
       * one same turn, so same-turn production *is* spendable). No project
       * queued still banks hammers (TURN5→6). The old code let hammers
       * through *for free* (no lumber debit at all) whenever the clipped
       * amount hit 0 instead of stopping production — that's what's fixed
       * here, not same-turn timing. (The colony-prod-tests Autumn-turn
       * counter-example that once looked like a same-turn-lumber rule is
       * fully explained by the Spring-only gate above — no separate timing
       * restriction needed.)
       */
      int hammers = hammers_add;
      if (hammers > colony->stock[COLONIZE_CARGO_LUMBER]) {
        hammers = colony->stock[COLONIZE_CARGO_LUMBER];
      }
      if (hammers > 0) {
        colony->stock[COLONIZE_CARGO_LUMBER] -= hammers;
        if (delta) {
          delta->lumber -= hammers;
          delta->goods[COLONIZE_CARGO_LUMBER] -= hammers;
        }
      }
      colony->hammers += hammers;
      if (delta) {
        delta->hammers_added = hammers;
      }

      if (colony->building_in_production >= 0) {
        const int bip = colony->building_in_production;
        const char* bname = NULL;
        if (bip >= 0 && bip < pool->building_type_count) {
          bname = pool->building_types[bip].name;
        }
        if (colonies_try_complete_building(pool, colony->id)) {
          if (delta) {
            delta->building_completed = true;
          }
          if (out) {
            out->buildings_completed++;
          }
          if (europe && colony->nation_id == human_nation) {
            if (bname && bname[0]) {
              snprintf(europe->status, sizeof(europe->status), "%s completed.", bname);
            } else {
              snprintf(europe->status, sizeof(europe->status), "Building completed.");
            }
            /* DOS @BUILT — "%STRING0 colony produces {%STRING1}." */
            if (ai_popups) {
              char body[AI_POPUP_BODY_LEN];
              PopupMsgTokens tok;
              memset(&tok, 0, sizeof(tok));
              tok.string0 = colony->name[0] ? colony->name : "colony";
              tok.string1 = (bname && bname[0]) ? bname : "building";
              popup_msg_fill(messages, "BUILT", &tok, europe->status, body, sizeof(body));
              ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
            }
          }
        } else if (
          colony->nation_id == human_nation && europe &&
          (!col1 || !col1->head.colony_report_options.report_tools_needed_for_production)
        ) {
          /* K tools crumb (0x8e76 / 0xe8f); gate !(5384&0x10).
           * Hammers ready but tools short: @NEEDTOOLS0 (0) / @NEEDTOOLS (some). */
          const ColonizeBuildingType* bt =
            (bip >= 0 && bip < pool->building_type_count) ? &pool->building_types[bip] : NULL;
          const int tools_have = colony->stock[COLONIZE_CARGO_TOOLS];
          const int tools_cost = bt ? bt->tools_cost : 0;
          const bool hammers_ready =
            bt && bt->hammers > 0 && colony->hammers >= bt->hammers;
          if (hammers_ready && tools_cost > 0 && tools_have < tools_cost) {
            snprintf(europe->status, sizeof(europe->status), "Need tools.");
            if (ai_popups) {
              char body[AI_POPUP_BODY_LEN];
              PopupMsgTokens tok;
              memset(&tok, 0, sizeof(tok));
              tok.string0 = colony->name[0] ? colony->name : "colony";
              tok.string1 = (bname && bname[0]) ? bname : "building";
              tok.number0 = tools_cost;
              tok.has_number0 = true;
              const char* section = "NEEDTOOLS0";
              if (tools_have > 0) {
                section = "NEEDTOOLS";
                tok.number1 = tools_have;
                tok.has_number1 = true;
              }
              popup_msg_fill(messages, section, &tok, europe->status, body, sizeof(body));
              ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
            }
          }
        }
      }
    } else if (
      colony->building_in_production >= 0 && colony->nation_id == human_nation && europe &&
      turn_report_ok_raw(col1)
    ) {
      /*
       * Phase K thin: construction queued but zero hammers this tick.
       * Gate !(5384&0x20). Cite: colony_eot_production.md Deep K.
       */
      snprintf(
        europe->status,
        sizeof(europe->status),
        "No hammers for construction."
      );
    }
  }

  /* Phase K demand crumbs (hammers/tools already above): raw / craft empty. */
  if (colony->nation_id == human_nation && europe && europe->status[0] == '\0' &&
      turn_report_ok_raw(col1)) {
    /*
     * DOS gates each "ran out of X" msg on a demand scratch word (set by
     * FUN_15eb_0bd4/0b96 from the SAME turn's tier-scaled worker output,
     * see colony_eot_production.md Deep K), not on "does the building
     * exist" — a staffed-vs-unstaffed distinction the port used to miss
     * (a colony with e.g. an unstaffed starter Blacksmith's House and 0
     * ore would nag "Need ore." every turn even though nobody was trying
     * to make tools). The DOS probe's "net output of the finished good ==
     * 0" half always reduces to "stock[in_cargo] == 0" for this game's
     * recipe ratios (output tier is always >= input tier, so any nonzero
     * input yields >=1 output) — proven, not assumed — so that half of
     * the port's existing check was already right; only the gate needed
     * fixing. 2026-08-24 fix: replaced building-name-substring gates with
     * colony_craft_demand_mask (same recipe pass colony_craft_one_colony
     * already ran this tick, sol_bonus-consistent). Lumber/hammers and
     * food keep their existing gates — hammers has its own Autumn-freeze
     * wrinkle (see hammers block above) not yet disentangled from the
     * message trigger, and food isn't a craft recipe.
     */
    bool craft_demand[COLONIZE_CARGO_COUNT];
    colony_craft_demand_mask(pool, colony, colony_prod_sol_bonus(col1, colony), craft_demand);

    const char* k_sec = NULL;
    if (colony->stock[COLONIZE_CARGO_LUMBER] == 0 &&
        turn_building_name_has(pool, colony, "Carpenter")) {
      snprintf(europe->status, sizeof(europe->status), "Need lumber.");
      k_sec = "LUMBER";
    } else if (colony->stock[COLONIZE_CARGO_ORE] == 0 && craft_demand[COLONIZE_CARGO_ORE]) {
      snprintf(europe->status, sizeof(europe->status), "Need ore.");
      k_sec = "ORE";
    } else if (
      colony->stock[COLONIZE_CARGO_FOOD] == 0 && colony->colonist_count > 0
    ) {
      snprintf(europe->status, sizeof(europe->status), "Need food.");
    } else if (
      colony->stock[COLONIZE_CARGO_SUGAR] == 0 && craft_demand[COLONIZE_CARGO_SUGAR]
    ) {
      snprintf(europe->status, sizeof(europe->status), "Need sugar.");
      k_sec = "CANESUGAR";
    } else if (
      colony->stock[COLONIZE_CARGO_TOBACCO] == 0 && craft_demand[COLONIZE_CARGO_TOBACCO]
    ) {
      snprintf(europe->status, sizeof(europe->status), "Need tobacco.");
      k_sec = "TOBACCO";
    } else if (
      colony->stock[COLONIZE_CARGO_COTTON] == 0 && craft_demand[COLONIZE_CARGO_COTTON]
    ) {
      snprintf(europe->status, sizeof(europe->status), "Need cotton.");
      k_sec = "COTTON";
    } else if (
      colony->stock[COLONIZE_CARGO_FURS] == 0 && craft_demand[COLONIZE_CARGO_FURS]
    ) {
      snprintf(europe->status, sizeof(europe->status), "Need furs.");
      k_sec = "FURS";
    } else if (
      colony->stock[COLONIZE_CARGO_MUSKETS] == 0 && colony->stock[COLONIZE_CARGO_TOOLS] == 0 &&
      craft_demand[COLONIZE_CARGO_TOOLS]
    ) {
      /* 0x8e66 paired tools+muskets empty. */
      snprintf(europe->status, sizeof(europe->status), "Need tools for muskets.");
      k_sec = "TOOLS";
    } else if (
      colony->stock[COLONIZE_CARGO_MUSKETS] == 0 && craft_demand[COLONIZE_CARGO_TOOLS]
    ) {
      snprintf(europe->status, sizeof(europe->status), "Need muskets.");
    }
    if (k_sec && ai_popups) {
      char body[AI_POPUP_BODY_LEN];
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = colony->name[0] ? colony->name : "colony";
      popup_msg_fill(messages, k_sec, &tok, europe->status, body, sizeof(body));
      ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    }
  }

  if (out) {
    out->colonies_produced++;
  }

  /*
   * Custom House auto-sell after production (FUN_364b_0688). Needs europe
   * bids; col1 optional (WoI tax skip + nation gold).
   */
  if (europe) {
    (void)europe_custom_house_autosell(europe, pool, colony, col1, human_nation);
    /* Phase O: AI dump-sell surplus for gold before spoilage clamp. */
    (void)europe_ai_colony_dump_sell(europe, pool, colony, col1, human_nation);
  }
  /* Spoilage after Custom House / AI dump-sell (wiki Custom House before spoilage). */
  {
    int first_spoil = -1;
    int spoil_types = 0;
    const int spoiled =
      colonies_apply_warehouse_spoilage(pool, colony, &first_spoil, &spoil_types);
    /* Phase P thin: human spoilage status; multi-type → goods phrasing. */
    if (spoiled > 0 && europe && colony->nation_id == human_nation) {
      const char* wh =
        (colony->warehouse_level > 1u) ? "Expanded warehouse" : "Warehouse";
      const char* where = (colony->name[0]) ? colony->name : NULL;
      const char* cargo_name = NULL;
      if (first_spoil >= 0 && first_spoil < europe->cargo_count) {
        cargo_name = europe->cargo[first_spoil].name;
      }
      if (spoil_types > 1) {
        if (where) {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "%s in %s spoiled %d goods.",
            wh,
            where,
            spoiled
          );
        } else {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "%s spoiled %d goods.",
            wh,
            spoiled
          );
        }
      } else if (cargo_name && cargo_name[0]) {
        if (where) {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "%s in %s spoiled %d %s.",
            wh,
            where,
            spoiled,
            cargo_name
          );
        } else {
          snprintf(
            europe->status,
            sizeof(europe->status),
            "%s spoiled %d %s.",
            wh,
            spoiled,
            cargo_name
          );
        }
      } else {
        snprintf(
          europe->status,
          sizeof(europe->status),
          "%s spoiled %d goods.",
          wh,
          spoiled
        );
      }
      if (ai_popups) {
        /* Phase P: SPOIL1/2 tip warehouse; SPOIL3/4 if expanded. Multi → 2/4. */
        const int expanded = colony->warehouse_level > 1u;
        const char* spoil_sec =
          (spoil_types > 1) ? (expanded ? "SPOIL4" : "SPOIL2")
                           : (expanded ? "SPOIL3" : "SPOIL1");
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = where ? where : "colony";
        tok.string1 = cargo_name && cargo_name[0] ? cargo_name : "goods";
        tok.number0 = spoiled;
        tok.has_number0 = true;
        popup_msg_fill(
          messages,
          spoil_sec,
          &tok,
          europe->status,
          body,
          sizeof(body)
        );
        ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      }
    } else if (europe && colony->nation_id == human_nation) {
      /*
       * Phase P century tip: stock crosses a 100s boundary upward → @CARGOREADY*.
       * At exact warehouse cap → CARGOREADY1 (tip) / CARGOREADY2 (expanded).
       * Once-per-campaign latch DS:0x5387 bit1 → head.tut3.nr6.
       * Cite: colony_eot_production.md Deep P; viceroy ~57900–57930.
       */
      const int latched = col1 && col1->head.tut3.nr6;
      if (!latched && turn_report_ok_new_cargo(col1)) {
        for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
          const int before = stock_before[c];
          const int after = colony->stock[c];
          if (after >= 100 && before / 100 < after / 100) {
            snprintf(
              europe->status,
              sizeof(europe->status),
              "Stockpile crossed %d00.",
              after / 100
            );
            if (col1) {
              col1->head.tut3.nr6 = 1;
            }
            if (ai_popups) {
              const int cap = colonies_warehouse_capacity(pool, colony, c);
              const char* cargo_name = NULL;
              if (c >= 0 && c < europe->cargo_count && europe->cargo[c].name[0]) {
                cargo_name = europe->cargo[c].name;
              }
              const char* sec = "CARGOREADY0";
              if (cap > 0 && after == cap) {
                sec = (colony->warehouse_level > 1u) ? "CARGOREADY2" : "CARGOREADY1";
              }
              char body[AI_POPUP_BODY_LEN];
              PopupMsgTokens tok;
              memset(&tok, 0, sizeof(tok));
              tok.string0 = colony->name[0] ? colony->name : "colony";
              tok.string1 = cargo_name ? cargo_name : "cargo";
              tok.number0 = cap > 0 ? cap : after;
              tok.has_number0 = true;
              popup_msg_fill(messages, sec, &tok, europe->status, body, sizeof(body));
              ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
            }
            break;
          }
        }
      }
    }
  }
}

void turn_run_colony_production(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  ColonizeTurnResult* out,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages,
  ColonizeDosRng* rng
) {
  if (!pool) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active) {
      turn_produce_one_colony(
        pool,
        &pool->colonies[i],
        map,
        col1,
        europe,
        human_nation,
        out,
        NULL,
        ai_popups,
        messages,
        rng
      );
    }
  }
}

int turn_run_coastal_fort_fire(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units || !ctx->colonies || !ctx->map) {
    return 0;
  }
  return units_coastal_fort_fire_pulse(
    ctx->units,
    ctx->colonies,
    ctx->map,
    ctx->col1_ok ? ctx->col1 : NULL,
    ctx->rng,
    ctx->human_nation,
    ctx->status,
    ctx->status_size
  );
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
  turn_produce_one_colony(
    pool, colony, map, NULL, NULL, -1, out ? out : &local, out_delta, NULL, NULL, NULL
  );
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
  const bool nation_has_penn =
    col1 && founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_PENN);
  const bool nation_is_ai =
    col1 && nation_id >= 0 && nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
    col1->player[nation_id].control != 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    const int sol_b = colony_prod_sol_bonus(col1, c);
    /* Bells/crosses: sol_b folds into each Statesman/Preacher worker
     * individually, inside colony_prod_colony_bells_ff/_crosses_ff (matches
     * FUN_15eb_1d4c's Statesman/Preacher bodies — see
     * manufacturing_worker_calc_1d4c.md). */
    int b = colony_prod_colony_bells_ff(pool, c, statesmen_pct, paine_tax_pct, nation_is_ai, sol_b);
    int x = colony_prod_colony_crosses_ff(pool, c, nation_has_penn, sol_b);
    bells += b;
    crosses += x;
  }
  if (out_bells) {
    *out_bells = bells;
  }
  if (out_crosses) {
    *out_crosses = crosses;
  }
  return bells + crosses;
}

static void turn_notify_dock_immigrant(
  ColonizeTurnContext* ctx,
  ColonizeTurnResult* out,
  const char* immigrant_name
) {
  if (!ctx || !ctx->europe) {
    return;
  }
  if (out) {
    out->immigrants_arrived++;
    /* DOS: @UNREST dialog — do not auto-dump into Europe screen (DS:0x14c is optional). */
  }
  if (ctx->status && ctx->status_size > 0) {
    snprintf(
      ctx->status,
      ctx->status_size,
      "Immigrant arrives in Europe: %s",
      immigrant_name && immigrant_name[0] ? immigrant_name : "Colonist"
    );
  }
  if (!ctx->ai_popups) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.country = ctx->europe->nation_name[0] ? ctx->europe->nation_name : "Europe";
  tok.string0 = "Europe";
  tok.string1 = immigrant_name && immigrant_name[0] ? immigrant_name : "Colonists";
  char body[AI_POPUP_BODY_LEN];
  const char* fb =
    "Religious unrest causes increased emigration. Colonists now available in Europe.";
  if (ctx->messages) {
    popup_msg_fill(ctx->messages, "UNREST", &tok, fb, body, sizeof(body));
  } else {
    snprintf(body, sizeof(body), "%s", fb);
  }
  ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, "Immigration", body);
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
    /*
     * Church / colony crosses accrue into current_crosses, then 584a sets
     * needed and adds idle +2 until the first dock immigrant.
     * Cite: Phase M + 5e52; TURN1–7 goldens.
     */
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
    if (europe_tick_immigration_pressure(
          ctx->europe, ctx->colonies, ctx->units, ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation,
          ctx->rng
        )) {
      const char* name = "";
      if (ctx->europe->dock_count > 0) {
        name = ctx->europe->dock[ctx->europe->dock_count - 1].name;
      }
      turn_notify_dock_immigrant(ctx, out, name);
      /* Mirror dock immigrant as Europe-map unit for Col1 capture. */
      if (ctx->units && ctx->europe->dock_count > 0) {
        const EuropeDockImmigrant* d = &ctx->europe->dock[ctx->europe->dock_count - 1];
        const int tid = units_find_type(ctx->units, "Colonists");
        const int type_index = tid >= 0 ? tid : 0;
        const int id = units_spawn_allow_stack(ctx->units, type_index, 236, 236);
        ColonizeUnit* u = units_get(ctx->units, id);
        if (u) {
          units_set_nation(u, ctx->human_nation);
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

  /*
   * FUN_4345_0a22 / nation EOT: accrue bells+crosses for every active Euro
   * nation (human + AI). Dock immigrant / Europe chrome stays human-only above.
   * Cite: turn/nation_ticks_bells_ff.md; DOS per-nation 00f2.
   */
  if (ctx->col1_ok && ctx->col1) {
    for (int n = 0; n < 4; ++n) {
      uint8_t control = 1;
      if (n < (int)COLONIZE_COL1_NATION_COUNT) {
        control = ctx->col1->player[n].control;
      }
      if (control == 2) {
        continue; /* withdrawn */
      }
      int nb = 0;
      int nc = 0;
      turn_count_bells_and_crosses_for_nation(
        ctx->colonies, n, ctx->col1, &nb, &nc
      );
      ColonizeCol1Nation* nat = &ctx->col1->nation[n];
      nat->liberty_bells_last_turn = (uint16_t)(nb > 65535 ? 65535 : nb);
      {
        unsigned total = (unsigned)nat->liberty_bells_total + (unsigned)nb;
        if (total > 65535u) {
          total = 65535u;
        }
        nat->liberty_bells_total = (uint16_t)total;
      }
      if (nb > 0) {
        founding_fathers_accrue_bells(n, (unsigned)nb);
      }
      {
        unsigned cur = (unsigned)nat->current_crosses + (unsigned)nc;
        if (cur > 65535u) {
          cur = 65535u;
        }
        nat->current_crosses = (uint16_t)cur;
      }
      /*
       * AI Euro: same 584a needed +2 as human (Free Colonist spawn PARKED).
       * Cite: nation_ticks_bells_ff.md; TURN1–7 goldens (needed≈14 early).
       */
      if (n != ctx->human_nation) {
        const int score = europe_compute_immigration_score(
          ctx->colonies, ctx->units, ctx->col1, n
        );
        int need = score > 0 ? score : TURN_AI_DEFAULT_NEEDED_CROSSES;
        if (need > 65535) {
          need = 65535;
        }
        nat->needed_crosses = (uint16_t)need;
        {
          unsigned cur = (unsigned)nat->current_crosses + 2u;
          if (cur > 65535u) {
            cur = 65535u;
          }
          nat->current_crosses = (uint16_t)cur;
        }
      }
      /* Human Europe screen is authoritative for human nation counters. */
      if (n == ctx->human_nation && ctx->europe) {
        nat->current_crosses = ctx->europe->current_crosses;
        nat->needed_crosses = ctx->europe->needed_crosses;
        nat->liberty_bells_total = ctx->europe->liberty_bells_total;
        nat->liberty_bells_last_turn = ctx->europe->liberty_bells_last_turn;
      }
    }
    founding_fathers_tick(ctx);

    /* FUN_4345_0a22 wartime branch: bell pool → intervention / REF, not FF elect. */
    if (ctx->col1->head.game_options.woi) {
      for (int n = 0; n < 4; ++n) {
        if (ctx->col1->player[n].control == 2) {
          continue;
        }
        const unsigned pool = founding_fathers_bells_since_last_elect(n);
        const unsigned needed = founding_fathers_bells_needed(ctx->col1, n);
        founding_fathers_woi_intervention_chrome(ctx, n, pool, needed);
        if (pool < needed) {
          continue;
        }
        if (ai_king_spend_woi_bell_pool(ctx, n)) {
          founding_fathers_consume_woi_bell_pool(n);
          if (ctx->status && ctx->status_size > 0 && n == ctx->human_nation) {
            snprintf(
              ctx->status,
              ctx->status_size,
              "Foreign intervention force arrives!"
            );
          }
        }
      }
    }
  }

  /*
   * FUN_3844_00f2 §C rare immigrant Merchantman (every 8th turn, peacetime).
   * Year≥1600 keeps golden_ai_turns early T2 clean. Census/dialog PARKED.
   * Cite: nation_eot_ship_spawn.md §C.
   */
  if (ctx->units && ctx->game_year && ctx->turn_number && *ctx->game_year >= 1600u &&
      (*ctx->turn_number & 7u) == 0u) {
    const int woi = ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi != 0;
    if (!woi) {
      int ti = units_find_type(ctx->units, "Merchantman");
      if (ti < 0) {
        ti = 0x11;
      }
      int x = 236;
      int y = 236;
      if (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 &&
          ctx->human_nation < (int)COLONIZE_COL1_NATION_COUNT) {
        x = (int)ctx->col1->nation[ctx->human_nation].return_from_europe_x;
        y = (int)ctx->col1->nation[ctx->human_nation].return_from_europe_y;
        if (x == 0 && y == 0) {
          x = 236;
          y = 236;
        }
      }
      const int id = units_spawn_allow_stack(ctx->units, ti, x, y);
      ColonizeUnit* u = units_get(ctx->units, id);
      if (u) {
        units_set_nation(u, ctx->human_nation);
        u->orders = UNITS_ORDER_AI_SAIL;
        u->col1_unknown15 = (uint8_t)(u->col1_unknown15 | 0x40u);
        u->goto_x = x;
        u->goto_y = y;
        /* FUN_48d3_0002 landfall duration: 1; 2 if Magellan owned + RNG>89 + docks>2. */
        int dur = 1;
        const int docks = ctx->europe ? ctx->europe->dock_count + ctx->europe->harbor_ships : 0;
        const bool magellan = ctx->col1_ok && ctx->col1 &&
          founding_fathers_nation_has(ctx->col1, ctx->human_nation, FF_FERDINAND_MAGELLAN);
        if (ctx->rng && magellan && docks > 2 &&
            dos_rng_range(ctx->rng, 1, 100) > 89) {
          dur = 2;
        }
        u->turns_worked = (uint8_t)dur;
        if (ctx->status && ctx->status_size > 0) {
          snprintf(ctx->status, ctx->status_size, "Immigrant ship arrives.");
        }
      }
    }
  }
}

int turn_rank_euro_nations(
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  uint8_t out_rank[4]
) {
  /*
   * FUN_5bfb_00f8: score = gold/100 + 2*colony_count + pop_proxy + land_combat.
   * Sort descending; inverse rank[nation] = place (0 = strongest).
   * Cite: viceroy_unpacked.c ~96506–96531; turn/mid_pass_indian_rank.md.
   */
  if (!out_rank) {
    return -1;
  }
  int score[4];
  int perm[4];
  for (int n = 0; n < 4; ++n) {
    perm[n] = n;
    int colonies_n = 0;
    int pop = 0;
    if (colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &colonies->colonies[i];
        if (!c->active || c->nation_id != n) {
          continue;
        }
        colonies_n++;
        pop += c->population > 0 ? c->population : c->colonist_count;
      }
    }
    int gold100 = 0;
    int land = 0;
    if (col1 && n < (int)COLONIZE_COL1_NATION_COUNT) {
      gold100 = (int)(col1->nation[n].gold / 100u);
      /* Land combat from stuff census when present; colony/pop always live. */
      land = (int)col1->stuff.land_combat_strength[n];
    }
    score[n] = gold100 + colonies_n * 2 + pop + land;
  }
  /* Stable insertion sort by score descending; perm tracks nation ids. */
  for (int i = 1; i < 4; ++i) {
    const int s = score[i];
    const int p = perm[i];
    int j = i;
    while (j > 0 && score[j - 1] < s) {
      score[j] = score[j - 1];
      perm[j] = perm[j - 1];
      j--;
    }
    score[j] = s;
    perm[j] = p;
  }
  for (int place = 0; place < 4; ++place) {
    out_rank[perm[place]] = (uint8_t)place;
  }
  return 0;
}

void turn_tally_professions(
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  int nation_id,
  uint8_t out_hist[32]
) {
  /*
   * FUN_4962_0606: zero hist[0x1d]; count unit specialties + colony jobs.
   * Linux: profession / field_job stand-ins; 32 slots (covers @JOB range).
   */
  if (!out_hist) {
    return;
  }
  memset(out_hist, 0, 32);
  if (nation_id < 0 || nation_id > 3) {
    return;
  }
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      const int p = u->profession;
      if (p >= 0 && p < 32 && out_hist[p] < 255u) {
        out_hist[p]++;
      }
    }
  }
  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      for (int ci = 0; ci < c->colonist_count; ++ci) {
        const ColonizeColonist* col = &c->colonists[ci];
        if (!col->active) {
          continue;
        }
        int p = col->profession;
        if (p < 0 && col->field_job >= 0) {
          p = col->field_job;
        }
        if (p >= 0 && p < 32 && out_hist[p] < 255u) {
          out_hist[p]++;
        }
      }
    }
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
    turn_refresh_moves_for_nation(
      ctx->units,
      n,
      ctx->col1_ok ? ctx->col1 : NULL,
      ctx->map,
      ctx->colonies,
      ctx->ai_popups,
      ctx->messages
    );
    (void)units_tick_treasure_outside_colony(
      ctx->units, ctx->colonies, n, ctx->status, ctx->status_size
    );
    (void)units_tick_ship_build_ready(
      ctx->units, ctx->colonies, n, ctx->human_nation, ctx->status, ctx->status_size, NULL
    );
    (void)units_tick_drydock_repair(
      ctx->units,
      ctx->colonies,
      n,
      ctx->human_nation,
      ctx->status,
      ctx->status_size,
      ctx->ai_popups,
      ctx->messages
    );
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
    turn_refresh_moves_for_nation(
      ctx->units,
      n,
      ctx->col1_ok ? ctx->col1 : NULL,
      ctx->map,
      ctx->colonies,
      ctx->ai_popups,
      ctx->messages
    );
    ai_indian_nation_turn(ctx, n);
  }
}

void turn_run_king_stub(ColonizeTurnContext* ctx) {
  ai_king_nation_turn(ctx);
}

static bool turn_year_end_valid_rival(const ColonizeCol1Save* col1, int human, int n) {
  if (!col1 || n < 0 || n >= 4 || n == human) {
    return false;
  }
  return col1->player[n].control != 2;
}

/*
 * FUN_3844_0442 §D: iVar5 = europe[nation][0x19] * table[nation − 0x6bf0] / 100.
 * nation+0x19 = rebel_sentiment; continent-weight table at −0x6bf0 is a live
 * DOS segment table (not a single Col1 field). Use rebel_sentiment when set,
 * else colony SoL stand-in via ai_king_sol_percent.
 */
static int turn_year_end_rival_sol_percent(const ColonizeTurnContext* ctx, int rival) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || rival < 0 || rival >= 4) {
    return 0;
  }
  const uint8_t rs = ctx->col1->nation[rival].rebel_sentiment;
  if (rs > 0) {
    return rs > 100 ? 100 : (int)rs;
  }
  return ai_king_sol_percent(ctx, rival);
}

static void turn_year_end_ensure_rival_slots(ColonizeCol1Save* col1, int human) {
  if (!col1 || human < 0 || human >= 4) {
    return;
  }
  if (turn_year_end_valid_rival(col1, human, (int)col1->head.rival_nation_slot_1) &&
      turn_year_end_valid_rival(col1, human, (int)col1->head.rival_nation_slot_2)) {
    return;
  }
  const int crown = ai_king_crown_nation(human);
  col1->head.rival_nation_slot_1 = -1;
  col1->head.rival_nation_slot_2 = -1;
  int w = 0;
  for (int n = 0; n < 4 && w < 2; ++n) {
    if (n == human || n == crown || col1->player[n].control == 2) {
      continue;
    }
    if (w == 0) {
      col1->head.rival_nation_slot_1 = (int16_t)n;
    } else {
      col1->head.rival_nation_slot_2 = (int16_t)n;
    }
    w++;
  }
}

void turn_run_year_end_chrome(ColonizeTurnContext* ctx, ColonizeTurnResult* out) {
  /*
   * FUN_3844_0442 thin peels:
   *   B — peacetime, year≥1600, zero human colonies → defeat latch
   *   C1 — WoI + zero crown colonies → victory (fleet + REF pool thin)
   *   E — anniversary years 1790/1840 status (dialogs PARKED)
   * Cite: viceroy_unpacked.c ~58430+; turn/year_end_chrome.md.
   */
  if (!ctx || !out) {
    return;
  }
  const uint16_t year =
    (ctx->game_year) ? *ctx->game_year
                     : (ctx->col1_ok && ctx->col1) ? ctx->col1->head.year : 0;

  const int splash_done =
    ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.calendar_latch;

  /* Section E anniversary (0x6fe=1790, 0x730=1840) — status only; gate 5382|0x10. */
  if (!splash_done && (year == 0x6feu || year == 0x730u) && ctx->status &&
      ctx->status_size > 0 && !out->year_end_defeat && !out->year_end_victory) {
    static const char* k_diff[] = {
      "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
    };
    const int d =
      (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.difficulty : -1;
    const char* dname = (d >= 0 && d <= 4) ? k_diff[d] : NULL;
    if (dname) {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Anniversary year %u (%s).",
        (unsigned)year,
        dname
      );
    } else {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Anniversary year %u.",
        (unsigned)year
      );
    }
    if (ctx->ai_popups) {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        ctx->messages,
        year == 0x6feu ? "WARN1" : "WARN2",
        NULL,
        ctx->status,
        body,
        sizeof(body)
      );
      ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    }
  }
  /* Section E game-over years (0x708=1800, 0x73a=1850) — status; HoF PARKED. */
  if (!splash_done && (year == 0x708u || year == 0x73au) && ctx->status &&
      ctx->status_size > 0 && !out->year_end_defeat && !out->year_end_victory) {
    static const char* k_diff[] = {
      "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
    };
    const int d =
      (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.difficulty : -1;
    const char* dname = (d >= 0 && d <= 4) ? k_diff[d] : NULL;
    const char* richest = NULL;
    int best_pop = -1;
    if (ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active || c->nation_id != ctx->human_nation) {
          continue;
        }
        if (c->colonist_count > best_pop && c->name[0] != '\0') {
          best_pop = c->colonist_count;
          richest = c->name;
        }
      }
    }
    if (richest && dname) {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Game era ends %u (%s, %s).",
        (unsigned)year,
        dname,
        richest
      );
    } else if (richest) {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Game era ends %u (%s).",
        (unsigned)year,
        richest
      );
    } else if (dname) {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Game era ends %u (%s).",
        (unsigned)year,
        dname
      );
    } else {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Game era ends %u.",
        (unsigned)year
      );
    }
    /* DOS clears 0x53c2 then LAB_0b4a ORs bit4 when stopped. */
    if (ctx->col1_ok && ctx->col1) {
      ctx->col1->head.game_options.calendar_latch = 1;
      ctx->col1->head.turn_loop_running = 0;
    }
  }

  const int woi = ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi != 0;
  /* unknown46[4]==1 = independence achieved (reports); also skip re-fire. */
  const int already_won =
    ctx->col1_ok && ctx->col1 && ctx->col1->head.unknown46[4] == 1;

  /*
   * Section C1 thin: WoI + (no crown colonies | force) + fleet thin + REF pool
   * thin. Force (0x5382 bit5) bypasses fleet/REF gates. Cite: year_end_chrome.md.
   */
  if (woi && !already_won && !out->year_end_defeat) {
    const int crown = (ctx->human_nation == 0) ? 1 : 0;
    int crown_colonies = 0;
    if (ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (c->active && c->nation_id == crown) {
          crown_colonies++;
        }
      }
    }
    const int force =
      ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.independence_force;
    int warships = 0;
    if (ctx->units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != crown) {
          continue;
        }
        /* DOS crown warship types 0x06 / 0x08 / 0x0b. */
        if (u->type_index == 0x06 || u->type_index == 0x08 || u->type_index == 0x0b) {
          warships++;
        }
      }
    }
    const int fleet_cap =
      (ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.ref_unit_threshold) ? 1 : 8;
    const int colony_gate = (crown_colonies == 0) || force;
    const int fleet_thin = warships < fleet_cap;
    /* DOS: (2-(53dc==0)-(53e0==0)+53da) < 4 — expeditionary_force[0/1/3]. */
    int ref_score = 2;
    if (ctx->col1_ok && ctx->col1) {
      const uint16_t* ef = ctx->col1->head.expeditionary_force;
      if (ef[1] == 0) {
        ref_score--;
      }
      if (ef[3] == 0) {
        ref_score--;
      }
      ref_score += (int)ef[0];
    }
    const int ref_thin = ref_score < 4;
    if (colony_gate && (fleet_thin || force) && (ref_thin || force)) {
      out->year_end_victory = true;
      if (ctx->col1_ok && ctx->col1) {
        ctx->col1->head.unknown46[4] = 1;
        ctx->col1->head.game_options.independence_chrome = 1; /* 0x5382|8 */
        ctx->col1->head.show_entire_map = 1; /* LAB_0b4a → DS:0x53a2 */
        ctx->col1->head.game_options.calendar_latch = 1; /* LAB_0b4a when stopped */
        ctx->col1->head.turn_loop_running = 0; /* DS:0x53c2 clear */
      }
      if (ctx->status && ctx->status_size > 0) {
        snprintf(ctx->status, ctx->status_size, "Victory: independence won.");
      }
      return;
    }

    /*
     * Section C2: WoI crown vs human SoL ratio (FUN year-end C2).
     * Was bells proxy; now pop-weighted colony SoL via ai_king_sol_percent.
     */
    if (crown_colonies > 0 && ctx->status && ctx->status_size > 0 && ctx->col1_ok &&
        ctx->col1) {
      const int human = ctx->human_nation;
      const int human_sol =
        (human >= 0 && human < 4) ? ai_king_sol_percent(ctx, human) : 0;
      const int crown_sol = ai_king_sol_percent(ctx, crown);
      const unsigned sol = ((unsigned)crown_sol + 1u) * 100u /
                           ((unsigned)human_sol + (unsigned)crown_sol + 1u);
      if (sol > 89u) {
        snprintf(ctx->status, ctx->status_size, "Crown peace offer.");
      } else if (sol > 79u) {
        snprintf(ctx->status, ctx->status_size, "Independence pressure.");
      }
    }
  }

  /*
   * Section D: peacetime rival SoL pressure (year_end_chrome D).
   * Threshold (8−difficulty)×10; auto-declare when rival SoL ≥ threshold.
   * Rising/falling dedup via rebellion_pct_last_notified (+0x1a).
   * Rival pick: head.rival_nation_slot_1/_2 (lazy-filled). SoL via
   * rebel_sentiment when non-zero; else ai_king_sol_percent. Continent-weight
   * table at DOS −0x6bf0 remains PARK (see turn_year_end_rival_sol_percent).
   */
  if (!woi && ctx->col1_ok && ctx->col1 && ctx->status && ctx->status_size > 0 &&
      !out->year_end_defeat && !out->year_end_victory) {
    const int human = ctx->human_nation;
    const int thresh = (8 - (int)ctx->col1->head.difficulty) * 10;
    turn_year_end_ensure_rival_slots(ctx->col1, human);
    for (int pi = 0; pi < 2; ++pi) {
      const int rival = pi == 0 ? (int)ctx->col1->head.rival_nation_slot_1
                                : (int)ctx->col1->head.rival_nation_slot_2;
      if (!turn_year_end_valid_rival(ctx->col1, human, rival)) {
        continue;
      }
      const int rival_sol = turn_year_end_rival_sol_percent(ctx, rival);
      if (thresh > 0 && rival_sol >= thresh) {
        ai_diplo_declare_war(ctx->col1, rival, human);
        snprintf(ctx->status, ctx->status_size, "Rival declares war.");
        break;
      }
      const uint8_t last = ctx->col1->nation[rival].rebellion_pct_last_notified;
      if (rival_sol > last) {
        snprintf(ctx->status, ctx->status_size, "Rival SoL rising.");
        ctx->col1->nation[rival].rebellion_pct_last_notified = (uint8_t)rival_sol;
        break;
      }
      if (rival_sol < last) {
        snprintf(ctx->status, ctx->status_size, "Rival SoL easing.");
        ctx->col1->nation[rival].rebellion_pct_last_notified = (uint8_t)rival_sol;
        break;
      }
    }
  }

  if (year < 1600) {
    return;
  }
  /* Section B: peacetime only. */
  if (woi) {
    return;
  }
  int human_colonies = 0;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == ctx->human_nation) {
        human_colonies++;
      }
    }
  }
  if (human_colonies != 0) {
    return;
  }
  out->year_end_defeat = true;
  if (ctx->col1_ok && ctx->col1) {
    ctx->col1->head.game_options.calendar_latch = 1; /* LAB_0b4a when stopped */
    ctx->col1->head.turn_loop_running = 0; /* DS:0x53c2 clear */
  }
  if (ctx->status && ctx->status_size > 0) {
    snprintf(ctx->status, ctx->status_size, "Defeat: no colonies remain.");
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
  if (!ctx || !ctx->status || ctx->status_size == 0 || !ctx->game_year || !ctx->game_autumn ||
      !ctx->turn_number) {
    return;
  }
  if (result && result->year_end_defeat) {
    snprintf(ctx->status, ctx->status_size, "Defeat: no colonies remain.");
    return;
  }
  if (result && result->year_end_victory) {
    snprintf(ctx->status, ctx->status_size, "Victory: independence won.");
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

  /* AI combat involving the human can enqueue outcome modals. */
  units_set_combat_popups(ctx->ai_popups, ctx->messages);
  units_set_combat_human_nation(ctx->human_nation);

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
        ctx->colonies,
        ctx->map,
        ctx->col1_ok ? ctx->col1 : NULL,
        ctx->europe,
        ctx->human_nation,
        &proc->result,
        ctx->ai_popups,
        ctx->messages,
        ctx->rng
      );
      /* FUN_364b_03f6 coastal Fort/Fortress fire after production. */
      (void)turn_run_coastal_fort_fire(ctx);
      turn_run_nation_ticks(ctx, &proc->result);
      /* Mid-pass Euro rank (FUN_5bfb_00f8) — DOS before nation loop; Linux SETUP. */
      ctx->euro_power_rank_ok =
        turn_rank_euro_nations(
          ctx->col1_ok ? ctx->col1 : NULL, ctx->colonies, ctx->euro_power_rank
        ) == 0;
      /* FUN_4962_0606 profession tally for each Euro nation (DOS per-nation 00f2). */
      for (int n = 0; n < 4; ++n) {
        turn_tally_professions(ctx->colonies, ctx->units, n, ctx->profession_tally[n]);
      }
      ctx->profession_tally_ok = true;
      /* Live census peel: colony + unit/combat tallies (FUN_4962_0018). */
      if (ctx->col1_ok && ctx->col1) {
        col1_stuff_census_refresh_colony_counts(
          &ctx->col1->stuff,
          ctx->colonies,
          ctx->units
        );
      }
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
      /*
       * AI fog reveal (00f2 / 281f_07a0) PARKED for golden_ai_turns T2 —
       * revealing rivals early changes found-tile / unit paths. Human FINISH
       * still reveals. Cite: nation_eot.c; turn_between_players.md.
       */
      if (ctx->units) {
        turn_refresh_moves_for_nation(
          ctx->units,
          n,
          ctx->col1_ok ? ctx->col1 : NULL,
          ctx->map,
          ctx->colonies,
          ctx->ai_popups,
          ctx->messages
        );
        if (n >= 0 && n < 4) {
          (void)units_tick_treasure_outside_colony(
            ctx->units,
            ctx->colonies,
            n,
            ctx->status,
            ctx->status_size
          );
          int want_eu = 0;
          (void)units_tick_ship_build_ready(
            ctx->units,
            ctx->colonies,
            n,
            ctx->human_nation,
            ctx->status,
            ctx->status_size,
            &want_eu
          );
          (void)units_tick_drydock_repair(
            ctx->units,
            ctx->colonies,
            n,
            ctx->human_nation,
            ctx->status,
            ctx->status_size,
            ctx->ai_popups,
            ctx->messages
          );
          if (want_eu && n == ctx->human_nation) {
            proc->result.request_europe_open = true;
          }
        }
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
        turn_refresh_moves_for_nation(
          ctx->units,
          n,
          ctx->col1_ok ? ctx->col1 : NULL,
          ctx->map,
          ctx->colonies,
          ctx->ai_popups,
          ctx->messages
        );
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
      turn_run_year_end_chrome(ctx, &proc->result);
      /* FUN_38fd_0058 EOT market attrition / rise-fall for Europe screen. */
      if (ctx->europe) {
        europe_tick_market_prices(
          ctx->europe, ctx->col1_ok ? ctx->col1 : NULL, ctx->colonies
        );
      }
      turn_set_active_nation(ctx, ctx->human_nation);
      turn_reveal_fog_for_nation(ctx, ctx->human_nation);
      turn_refresh_moves_for_nation(
        ctx->units,
        ctx->human_nation,
        ctx->col1_ok ? ctx->col1 : NULL,
        ctx->map,
        ctx->colonies,
        ctx->ai_popups,
        ctx->messages
      );
      if (ctx->human_nation >= 0 && ctx->human_nation < 4) {
        (void)units_tick_treasure_outside_colony(
          ctx->units,
          ctx->colonies,
          ctx->human_nation,
          ctx->status,
          ctx->status_size
        );
        int want_eu = 0;
        const int ships_ready = units_tick_ship_build_ready(
          ctx->units,
          ctx->colonies,
          ctx->human_nation,
          ctx->human_nation,
          ctx->status,
          ctx->status_size,
          &want_eu
        );
        (void)units_tick_drydock_repair(
          ctx->units,
          ctx->colonies,
          ctx->human_nation,
          ctx->human_nation,
          ctx->status,
          ctx->status_size,
          ctx->ai_popups,
          ctx->messages
        );
        if (want_eu) {
          proc->result.request_europe_open = true;
        }
        if (ships_ready > 0 && ctx->ai_popups && ctx->status && ctx->status[0]) {
          char body[AI_POPUP_BODY_LEN];
          popup_msg_fill(
            ctx->messages, "CARGOREADY0", NULL, ctx->status, body, sizeof(body)
          );
          ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        }
        if (ctx->europe && ctx->col1_ok && ctx->col1) {
          (void)units_cortes_cash_coastal_treasures(
            ctx->units,
            ctx->colonies,
            ctx->map,
            ctx->europe,
            ctx->col1,
            ctx->human_nation
          );
        }
      }
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
