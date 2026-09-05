#include "core/popup_msg.h"
#include "core/turn.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
#include "core/woodcut.h"
#include "platform/diagnostics.h"

static void turn_set_active_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (ctx && ctx->active_turn_nation) {
    *ctx->active_turn_nation = nation_id;
  }
}

/*
 * FUN_3844_00f2 per-unit fog reveal (281f_07a0 → 13f1_02f8) for every unit of
 * the nation, human and AI alike. No colony reveal here: DOS colonies only
 * reveal once, ±5 at founding (colonies_reveal_founded).
 * Cite: nation_eot.c unit walk; game_loop human fog.
 */
static void turn_reveal_fog_for_nation(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units || nation_id < 0 || nation_id > 3) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || !units_is_on_map(u)) {
      continue;
    }
    (void)units_reveal_sight(ctx->map, ctx->units, ctx->colonies, u, ctx->col1_ok ? ctx->col1 : NULL);
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
  colonies_set_col1_context((ColonizeCol1Save*)col1);
  units_set_occupancy_map(map);
  colonies_set_occupancy_map(map);
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
      /* bugs.md: count the nights parked — a unit fortified/sentried on a
       * PREVIOUS turn wakes with its full allotment (units_wake checks
       * turns_worked > 0); one dug in this turn does not get its spent
       * moves back. */
      if (u->turns_worked < 255) {
        u->turns_worked++;
      }
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
        u->moves_left = units_type_max_mp(type);
        if (magellan && units_is_sea(pool, u->id)) {
          u->moves_left += UNITS_MP_PER_TILE;
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
/*
 * bugs.md: a birth puts the new Free Colonist ON the colony tile as a map
 * unit awaiting orders — not into the colony as a worker. The production
 * pass has no unit-pool parameter (deep call chain), so the game loop hands
 * it in here before ticking; NULL (tests, headless callers) falls back to
 * the old join-the-colony behaviour.
 */
static ColonizeUnitPool* s_turn_birth_units = NULL;
void turn_set_birth_units_pool(ColonizeUnitPool* units) {
  s_turn_birth_units = units;
}

static int turn_report_ok_sons(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_sons_of_liberty_membership;
}
/* DOS 0x5384 bit3 clear → show @INEFFICIENT / @EFFICIENT. */
static int turn_report_ok_inefficient(const ColonizeCol1Save* col1) {
  return !col1 || !col1->head.colony_report_options.report_inefficient_government;
}

/*
 * FUN_364b_0688 Phase D Tory pressure: inefficient-gov chrome (0xdd1 / 0xddd).
 * Latch is Col1 +0x1c bit3 (COLONIZE_COLONY_FLAG_INEFFICIENT_GOV), exactly
 * where DOS keeps it, so a crossing announced before a save is not announced
 * again after the reload (bugs.md). DOS sets and clears the bit whether or
 * not the report option lets the message through, so the latch update stays
 * outside turn_report_ok_inefficient.
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
    if ((colony->colony_flags & COLONIZE_COLONY_FLAG_INEFFICIENT_GOV) != 0) {
      colony->colony_flags =
        (uint8_t)(colony->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_INEFFICIENT_GOV);
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
    if ((colony->colony_flags & COLONIZE_COLONY_FLAG_INEFFICIENT_GOV) == 0) {
      colony->colony_flags |= COLONIZE_COLONY_FLAG_INEFFICIENT_GOV;
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
  ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
  ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
}

/*
 * LABELS.TXT for status-line wording, set by turn_processor_advance for the
 * duration of a slice. NULL outside the processor (unit tests) — every reader
 * passes a literal fallback.
 */
static const ColonizeMsgCatalog* s_turn_labels = NULL;

static const char* turn_label(const char* section, int idx, const char* fallback) {
  if (s_turn_labels && section) {
    const ColonizeMsgSection* sec = assets_msg_find(s_turn_labels, section);
    if (sec && idx >= 0 && idx < sec->line_count && sec->lines[idx][0]) {
      return sec->lines[idx];
    }
  }
  return fallback;
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
  /* DOS 0xa896: ore/silver deposit "depletion units" tallied in FUN_15eb_18ec,
   * rolled down in the FUN_364b_0688 epilogue (see bottom of this function). */
  int depl_tx[2 * COLONIZE_COLONY_FIELD_TILES];
  int depl_ty[2 * COLONIZE_COLONY_FIELD_TILES];
  int depl_n = 0;
  int field_lumber = 0;
  int field_ore = 0;

  /* Town commons (center tile) + area-view field workers. */
  if (map) {
    const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);
    ColonizeTownCommonsYield tc;
    colony_yield_town_commons(
      map, colony->x, colony->y, sol_b_field, colony->colony_flags,
      col1 ? (int)col1->head.difficulty : 4, &tc
    );
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
       * Col1 +0x97 depletion units — FUN_15eb_18ec ~11913-11923 (DS 0xa896):
       *   Minerals deposit (res 6)  + Ore Miner    → +1
       *   Minerals deposit (res 6)  + Silver Miner → +2
       *   Silver deposit   (res 12) + Silver Miner → +1
       * Nothing else counts (an ordinary hills/mountain tile, or res 13 ore
       * under an Ore Miner, never depletes — player-confirmed 2026-08-16:
       * plain ore/silver tiles ended a real DOS turn at counter 0). The roll
       * and wrap live in the epilogue below (DOS ~57932), not here.
       */
      {
        const int res = map_resource_type_for_yield(map, colony->x + dx, colony->y + dy);
        int units = 0;
        if (res == 6 && cargo == COLONIZE_CARGO_ORE) {
          units = 1;
        } else if (res == 6 && cargo == COLONIZE_CARGO_SILVER) {
          units = 2;
        } else if (res == 12 && cargo == COLONIZE_CARGO_SILVER) {
          units = 1;
        }
        while (units-- > 0 && depl_n < (int)(2 * COLONIZE_COLONY_FIELD_TILES)) {
          depl_tx[depl_n] = colony->x + dx;
          depl_ty[depl_n] = colony->y + dy;
          depl_n++;
        }
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
  /*
   * FUN_364b_0688: starvation latch (+0x1c bit3) from food vs pop need.
   * Phase J kills when still short after this turn *and* food was already 0
   * at turn start (local_6c==0 / local_12e); pop==kills → @VANISH + abandon.
   * Easy-difficulty no-kill mercy ported below. Cite: ~57623–57694.
   */
  {
    const int need = pop * TURN_FOOD_PER_COLONIST;
    const int food_at_start = stock_before[COLONIZE_CARGO_FOOD];
    const int was_starving = colony->food_shortfall_latch != 0;
    int starved_this_tick = 0;
    /*
     * bugs.md (port_orange_starves.SAV): DOS's starve trigger is DS:0x8e5a =
     * max(0, consumption − stock-at-start − production) — the colony must
     * actually have gone NEGATIVE this turn. The old `stock_after < need`
     * latch killed a colony producing exactly what it eats at 0 stores
     * (commons 2 food vs pop 1 eating 2 — net zero, DOS-fine forever).
     */
    colony->food_shortfall_latch = (need - food_at_start - field_food > 0) ? 1u : 0u;

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
      bool born_on_tile = false;
      if (s_turn_birth_units) {
        /* bugs.md: the newborn stands on the colony tile awaiting orders. */
        const int ct = units_find_type(s_turn_birth_units, "Colonists");
        if (ct >= 0) {
          const int nid =
            units_spawn_allow_stack(s_turn_birth_units, ct, colony->x, colony->y);
          ColonizeUnit* nu = units_get(s_turn_birth_units, nid);
          if (nu) {
            units_set_nation(nu, colony->nation_id);
            nu->orders = UNITS_ORDER_NONE;
            born_on_tile = true;
          }
        }
      }
      if (!born_on_tile) {
        ColonizeColonist* newborn = &colony->colonists[colony->colonist_count];
        memset(newborn, 0, sizeof(*newborn));
        newborn->active = true;
        newborn->unit_type_index = 0;
        newborn->profession = UNITS_JOB_COLONIST; /* Free Colonists */
        newborn->building_type = -1;
        newborn->field_job = -1;
        colony->colonist_count++;
        colony->population = colony->colonist_count;
      }
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
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
    if (colony->food_shortfall_latch != 0 && food_at_start == 0 &&
        colony->colonist_count > 0 && !starve_mercy) {
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
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
      /*
       * bugs.md item 5: DOS's literal FOOD1/FOOD2 latch fires on stock<need
       * alone, even at food_shortfall<=0 (production covers or beats
       * consumption) — a colony merely flatlining at 0 net-zero food would
       * re-trigger "depleted" every turn. Require actively losing food
       * (shortfall>0) to match FOODLOW's own "surplus never warns" rule
       * below and stop the false-positive nag.
       *
       * bugs.md item 292: "depleted" additionally requires the stores to be
       * EXACTLY 0 — merely dipping below next turn's need reads as "low",
       * handled by the FOODLOW branch below.
       */
      if (stock == 0 && food_shortfall > 0 && !was_starving) {
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
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
        }
      } else if (
        stock > 0 && food_shortfall > 0 && stock < food_shortfall * 4
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
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
   * Horse breeding — FUN_15eb_1f72 tail, DOS-confirmed 2026-08-26 against
   * real DOS ground truth (golden_colony_prod01/02, 13/13 Dutch colonies
   * exact) — see colony_prod_horse_breed's header comment in
   * colony_production.h for the full derivation. Replaces the old
   * "manual/fandom" approximation (flat food-surplus/2 capped at 6-or-8)
   * with the DOS-confirmed herd-size-based potential (ceil(horses/divisor)*2,
   * divisor 25 with a Stable else 50), capped by this turn's food surplus
   * and warehouse headroom.
   */
  {
    const bool horse_has_stable = turn_building_name_has(pool, colony, "Stable");
    const int horse_warehouse_cap =
      colonies_warehouse_capacity(pool, colony, COLONIZE_CARGO_HORSES);
    const ColonyProdHorseBreed breed = colony_prod_horse_breed(
      colony->stock[COLONIZE_CARGO_HORSES],
      pop,
      field_food,
      horse_warehouse_cap,
      horse_has_stable
    );
    if (breed.bred > 0) {
      colony->stock[COLONIZE_CARGO_FOOD] =
        turn_clamp_stock(colony->stock[COLONIZE_CARGO_FOOD] - breed.bred);
      colony->stock[COLONIZE_CARGO_HORSES] =
        turn_clamp_stock(colony->stock[COLONIZE_CARGO_HORSES] + breed.bred);
      if (delta) {
        delta->goods[COLONIZE_CARGO_FOOD] -= breed.bred;
        delta->food_net -= breed.bred;
        delta->goods[COLONIZE_CARGO_HORSES] += breed.bred;
      }
      if (europe && colony->nation_id == human_nation) {
        snprintf(
          europe->status,
          sizeof(europe->status),
          horse_has_stable ? "Stable bred %d horses." : "Horses bred: %d.",
          breed.bred
        );
      }
    }
  }

  /*
   * FUN_364b_0688 phases F–G: Schoolhouse/College/University education.
   * Cite: viceroy_unpacked.c ~57502-57589; GAME.TXT @TRAINFAIL /
   * @TRAINCRIMINAL / @TRAININDENTURED / @TRAINPROFESSION;
   * docs/building_production.md, colony_eot_production.md.
   *
   * bugs.md 386 ("has education even been implemented?" — it had not). Every
   * axis of the previous port was wrong, and nothing could ever graduate:
   *   - A TEACHER is a colonist whose WORK SLOT is the school (DOS occupation
   *     0x12, i.e. the @JOB 18 "Teacher" slot, `local_c2 == 0x12`) and whose
   *     SPECIALTY has an @JOB school level of 1..3. The port instead required
   *     `profession == COLONIZE_PROF_TEACHER` (18), which nothing in the game
   *     ever sets, so the teacher list was always empty.
   *   - Required turns come from the TEACHER'S profession level (1 -> 4,
   *     2 -> 6, 3 -> 8), not from the colony's best school building; the
   *     building tier is enforced when the teacher is assigned
   *     (@NEEDCOLLEGE / @NEEDUNIVERSITY, colonies_assign_workplace).
   *   - STUDENTS are colonists working anywhere in the colony, not only ones
   *     placed inside the school — @TRAINFAIL says so in as many words. DOS's
   *     list is profession 0x13 / 0x1c (free colonist), 0x19 (indentured),
   *     0x1a (criminal); Indian Converts (0x1b) are NOT students.
   *   - The graduate takes the TEACHER'S OWN profession — 0cae's third,
   *     register-passed argument, which Ghidra drops, is `aiStack_7e[t]`, the
   *     stored teacher specialty. The port handed out Farmer/Carpenter off
   *     `teacher->field_job`, which is always -1 for a colonist working in a
   *     building, so even a live teacher could not have taught Veteran
   *     Soldiers.
   *   - The student is picked at RANDOM (FUN_281f_04d4(0, n-1)), and at most
   *     3 teachers graduate per colony per tick (`local_6e < 3`).
   * The 0x1a -> 0x19 and 0x19 -> 0x1c ladders take priority over the specialty
   * and consume the teacher's turn, exactly as in DOS.
   */
  if (pool) {
    enum { EDU_MAX_TEACHERS = 3 };
    int teach_prof[EDU_MAX_TEACHERS];
    int students[COLONIZE_COLONY_POP_MAX];
    int n_teach = 0;
    int n_stud = 0;
    for (int ci = 0; ci < colony->colonist_count; ++ci) {
      ColonizeColonist* c = &colony->colonists[ci];
      if (!c->active) {
        continue;
      }
      /* DOS 0d1c/0a7e: the turn counter ticks for every colonist. */
      if (c->turns_in_job < 255) {
        c->turns_in_job++;
      }
      const int prof = c->profession;
      if (prof < 0 || prof == COLONIZE_PROF_FREE_COLONIST || prof == UNITS_JOB_COLONIST ||
          prof == COLONIZE_PROF_INDENTURED || prof == COLONIZE_PROF_CRIMINAL) {
        if (n_stud < (int)(sizeof(students) / sizeof(students[0]))) {
          students[n_stud++] = ci;
        }
        continue; /* level-4 professions never teach; lists are disjoint */
      }
      if (n_teach >= EDU_MAX_TEACHERS || c->building_type < 0 ||
          c->building_type >= pool->building_type_count ||
          colonies_school_building_tier(pool, c->building_type) <= 0) {
        continue;
      }
      const int level = colonies_job_school_tier(prof);
      if (level < 1 || level > 3) {
        continue;
      }
      const int need = (level == 1) ? 4 : (level == 2) ? 6 : 8;
      if ((int)c->turns_in_job < need) {
        continue;
      }
      teach_prof[n_teach++] = prof;
      /* DOS zeroes the counter the moment the teacher qualifies, whether or
       * not a student is available for him this turn. */
      c->turns_in_job = 0;
    }
    for (int t = 0; t < n_teach; ++t) {
      const bool tell =
        europe && colony->nation_id == human_nation && turn_report_ok_trained(col1);
      const char* cname = colony->name[0] ? colony->name : "colony";
      char body[AI_POPUP_BODY_LEN];
      char fallback[224];
      PopupMsgTokens tok;
      if (n_stud == 0) {
        /* DOS 0xde7 @TRAINFAIL, then out of the graduation loop entirely. */
        if (tell) {
          snprintf(europe->status, sizeof(europe->status), "No students to teach.");
          if (ai_popups) {
            snprintf(
              fallback,
              sizeof(fallback),
              "We have a teacher in %s, but all the colonists there already have "
              "specialty professions.",
              cname
            );
            memset(&tok, 0, sizeof(tok));
            tok.string0 = cname;
            popup_msg_fill(messages, "TRAINFAIL", &tok, fallback, body, sizeof(body));
            ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
          }
        }
        break;
      }
      const int pick = rng ? dos_rng_range(rng, 0, n_stud - 1) : n_stud - 1;
      ColonizeColonist* student = &colony->colonists[students[pick]];
      const int prev_prof = student->profession;
      const char* skill_name = colonies_profession_name(teach_prof[t]);
      const char* chrome_sec;
      enum { GRAD_SPECIALTY = 0, GRAD_CRIMINAL, GRAD_INDENTURED } grad = GRAD_SPECIALTY;
      if (prev_prof == COLONIZE_PROF_CRIMINAL) {
        /* DOS 0x1a -> 0x19, popup 0xdf1. */
        student->profession = COLONIZE_PROF_INDENTURED;
        chrome_sec = "TRAINCRIMINAL";
        grad = GRAD_CRIMINAL;
        snprintf(
          fallback,
          sizeof(fallback),
          "A criminal in %s has become an indentured servant through education.",
          cname
        );
      } else if (prev_prof == COLONIZE_PROF_INDENTURED) {
        /* DOS 0x19 -> 0x1c, popup 0xdff. */
        student->profession = COLONIZE_PROF_FREE_COLONIST;
        chrome_sec = "TRAININDENTURED";
        grad = GRAD_INDENTURED;
        snprintf(
          fallback,
          sizeof(fallback),
          "An indentured servant in %s has become a free colonist through education.",
          cname
        );
      } else {
        /* DOS 0cae(student, aiStack_7e[t]) + 0438(1, jobtable[prof].name),
         * popup 0xe0f. */
        student->profession = teach_prof[t];
        chrome_sec = "TRAINPROFESSION";
        snprintf(
          fallback,
          sizeof(fallback),
          "A colonist in %s has learned the specialty profession %s.",
          cname,
          skill_name
        );
      }
      student->turns_in_job = 0;
      for (int k = pick; k < n_stud - 1; ++k) {
        students[k] = students[k + 1];
      }
      n_stud--;
      if (tell) {
        if (grad == GRAD_CRIMINAL) {
          snprintf(europe->status, sizeof(europe->status), "Criminal educated.");
        } else if (grad == GRAD_INDENTURED) {
          snprintf(europe->status, sizeof(europe->status), "Indentured educated.");
        } else {
          snprintf(europe->status, sizeof(europe->status), "%s trained.", skill_name);
        }
        if (ai_popups) {
          memset(&tok, 0, sizeof(tok));
          tok.string0 = cname;
          tok.string1 = skill_name;
          popup_msg_fill(messages, chrome_sec, &tok, fallback, body, sizeof(body));
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
      } else if (c->profession != COLONIZE_PROF_FREE_COLONIST &&
                 c->profession != UNITS_JOB_COLONIST /* @JOB 19 free alias */ &&
                 c->profession >= 0) {
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
            ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
        /*
         * bugs.md (printing_press.SAV/carpentry.SAV): the selection stays on
         * a completed project (DOS never clears it), so hammers piled up
         * forever with no word to the player — GAME.TXT @ALREADYHAVE is the
         * DOS notification for exactly this ("{colony} is set to produce a
         * {X}, but it has already built one!").
         */
        if (bip < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[bip] &&
            colony->nation_id == human_nation && europe && ai_popups) {
          char body[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = colony->name[0] ? colony->name : "colony";
          tok.string1 = (bname && bname[0]) ? bname : "building";
          char afb[120];
          snprintf(
            afb, sizeof(afb), "%s already built.",
            (bname && bname[0]) ? bname : "Building"
          );
          /* Popup only — the status line stays free for the Phase K
           * production crumbs ("Need lumber." etc.). */
          popup_msg_fill(messages, "ALREADYHAVE", &tok, afb, body, sizeof(body));
          ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
              ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
              ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
     * already ran this tick, sol_bonus-consistent). Food keeps its existing
     * gate (not a craft recipe).
     *
     * 2026-08-24 fix (lumber): same false-positive existed for lumber —
     * an unstaffed Carpenter's Shop/Lumber Mill with 0 lumber nagged "Need
     * lumber." every turn even though nobody was banking hammers. Lumber
     * isn't in colony_craft.c's recipe table (hammers are a separate
     * pipeline, colony_prod_colony_hammers), so it can't use
     * colony_craft_demand_mask directly, but the same "real staffed demand,
     * not building-exists" principle applies: colony_prod_colony_hammers's
     * out_lumber_use is this tick's actual tier-scaled lumber requirement
     * from staffed Carpenter/Lumber Mill workers (sol_bonus-independent —
     * lumber consumption doesn't scale with SoL, see that function). This
     * intentionally does NOT touch the hammers block's Autumn-freeze gate
     * above (col1->head.autumn) — whether DOS also silences this specific
     * message on Autumn ticks is unresolved (the DOS demand word this
     * mirrors, DS:0x8de8, has no located write site in either decompile
     * export to confirm one way or the other; see colony_eot_production.md
     * Deep K) and left as a separate, still-open question. This fix only
     * replaces the always-wrong "building exists" gate with a strictly more
     * accurate "someone is actually staffed to consume lumber" gate, same
     * as the other five goods above.
     */
    bool craft_demand[COLONIZE_CARGO_COUNT];
    colony_craft_demand_mask(pool, colony, colony_prod_sol_bonus(col1, colony), craft_demand);
    int lumber_demand = 0;
    (void)colony_prod_colony_hammers(pool, colony, 0, &lumber_demand);

    const char* k_sec = NULL;
    /* bugs.md: @LUMBER only when lumber INPUT production is zero and the
     * carpenters still tried to work — producing some lumber (merely not
     * enough for full demand) is not "run out". */
    if (colony->stock[COLONIZE_CARGO_LUMBER] == 0 && lumber_demand > 0 &&
        field_lumber <= 0) {
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
      ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
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
    EuropeCustomHouseSale ch_sales[COLONIZE_CARGO_COUNT];
    int ch_sale_count = 0;
    const int ch_total = europe_custom_house_autosell_ex(
      europe, pool, colony, col1, human_nation, ch_sales, COLONIZE_CARGO_COUNT, &ch_sale_count
    );
    /*
     * FUN_364b_0688 assembles ONE line PER CARGO into DS:0x2d54 and arms it
     * with FUN_1009_0092 — that is the map's top-strip STATUS LINE, not a
     * dialog (bugs.md 375). Each line replaces the strip's normal content for
     * its dwell and the next one follows; nothing has to be clicked away.
     *
     * Wording is DOS's own: colony name, LABELS @MISC 47 "sells", amount,
     * cargo name, @MISC 48 "for", gross, DS:0xd88 ".", then (peacetime only)
     * tax rate, @CMESSAGE 0x11 "% Tax:", tax paid, @CMESSAGE 0x12 ". Net:",
     * net. The 0088 calls between fields just trim the trailing space every
     * append leaves, so the punctuation closes up.
     */
    if (ch_total > 0 && colony->nation_id == human_nation && ai_popups) {
      for (int si = 0; si < ch_sale_count; ++si) {
        const EuropeCustomHouseSale* sale = &ch_sales[si];
        const char* cargo_name =
          (sale->cargo >= 0 && sale->cargo < europe->cargo_count)
            ? europe->cargo[sale->cargo].name
            : "goods";
        char line[AI_POPUP_BAR_MSG_LEN];
        int n = snprintf(
          line,
          sizeof(line),
          "%s %s %d %s %s %d.",
          colony->name[0] ? colony->name : "Colony",
          turn_label("MISC", 47, "sells"),
          sale->amount,
          cargo_name,
          turn_label("MISC", 48, "for"),
          sale->gross
        );
        if (n > 0 && n < (int)sizeof(line) && sale->tax_percent > 0) {
          snprintf(
            line + n,
            sizeof(line) - (size_t)n,
            " %d%s %d%s %d",
            sale->tax_percent,
            turn_label("CMESSAGE", 0x11, "% Tax:"),
            sale->tax_paid,
            turn_label("CMESSAGE", 0x12, ". Net:"),
            sale->net
          );
        }
        ai_popup_enqueue_bar_message(ai_popups, line);
      }
    }
    /* Phase O: AI dump-sell surplus for gold before spoilage clamp. */
    (void)europe_ai_colony_dump_sell(europe, pool, colony, col1, human_nation);
  }
  /* Spoilage after Custom House / AI dump-sell (wiki Custom House before spoilage). */
  {
    int first_spoil = -1;
    int spoil_types = 0;
    const int spoiled =
      colonies_apply_warehouse_spoilage(pool, colony, stock_before, &first_spoil, &spoil_types);
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
        ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
      }
    }

    /*
     * Phase P century tip: stock crosses a 100s boundary upward → @CARGOREADY*.
     * At exact warehouse cap → CARGOREADY1 (tip) / CARGOREADY2 (expanded).
     *
     * DOS runs this as its own `if` after the spoil block, once per crossing
     * cargo, for as long as the "report new cargos available" option is on —
     * it is not an alternative to spoilage, and it does not latch. The
     * once-per-campaign latch DS:0x5387 bit1 (head.tut3.nr6) belongs to the
     * separate @TUTORIAL6 "move a ship in and sell it" hint, emitted below.
     * Food (cargo 0) never triggers it. Cite: viceroy ~57893–57930.
     */
    if (europe && colony->nation_id == human_nation && turn_report_ok_new_cargo(col1)) {
      for (int c = 1; c < COLONIZE_CARGO_COUNT; ++c) {
        const int before = stock_before[c];
        const int after = colony->stock[c];
        if (after < 100 || before / 100 >= after / 100) {
          continue;
        }
        const int cap = colonies_warehouse_capacity(pool, colony, c);
        const char* cargo_name = NULL;
        if (c < europe->cargo_count && europe->cargo[c].name[0]) {
          cargo_name = europe->cargo[c].name;
        }
        snprintf(
          europe->status,
          sizeof(europe->status),
          "New cargo of %s ready at %s.",
          cargo_name ? cargo_name : "goods",
          colony->name[0] ? colony->name : "colony"
        );
        if (!ai_popups) {
          continue;
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
        ai_popup_enqueue_colony_event(ai_popups, colony->id, body);

        /* @TUTORIAL6, once per campaign (DS:0x5387 bit1). */
        if (col1 && !col1->head.tut3.nr6) {
          col1->head.tut3.nr6 = 1;
          PopupMsgTokens ttok;
          memset(&ttok, 0, sizeof(ttok));
          ttok.number0 = after;
          ttok.has_number0 = true;
          ttok.string0 = cargo_name ? cargo_name : "cargo";
          ttok.string1 = colony->name[0] ? colony->name : "colony";
          ttok.string2 = europe->nation_name[0] ? europe->nation_name : "Europe";
          char tbody[AI_POPUP_BODY_LEN];
          popup_msg_fill(messages, "TUTORIAL6", &ttok, europe->status, tbody, sizeof(tbody));
          ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, tbody);
        }
      }
    }
  }

  /*
   * FUN_364b_0688 ~57932 (Deep Q): per depletion unit, `rng(0, diff+1) != 0`
   * bumps Col1 +0x97; wrap at 50 → MAP_LAYER2_SUPPRESS (FUN_364b_033a
   * feature 4) + @DEPLETION. Discoverer thus depletes at 1/2 rate, Viceroy
   * at 5/6. Rolled here, after the cargo-ready chrome, to keep the DOS rng
   * order. No rng / col1 (unit tests) → deterministic bump.
   */
  if (depl_n > 0) {
    int diff = col1 ? (int)col1->head.difficulty : 4;
    if (diff < 0) {
      diff = 0;
    }
    if (diff > 4) {
      diff = 4;
    }
    int mine_depleted = 0;
    for (int u = 0; u < depl_n; ++u) {
      if (rng && dos_rng_range(rng, 0, diff + 1) == 0) {
        continue;
      }
      colony->depletion_counter = (uint8_t)(colony->depletion_counter + 1u);
      if (colony->depletion_counter > 0x31u) {
        colony->depletion_counter = (uint8_t)(colony->depletion_counter - 0x32u);
        if (map) {
          /* Production API takes const map; deplete mutates layer2. */
          map_occupancy_set_layer2(
            (ColonizeWorldMap*)(uintptr_t)map,
            depl_tx[u],
            depl_ty[u],
            MAP_LAYER2_SUPPRESS,
            true
          );
        }
        mine_depleted = 1;
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
        ai_popup_enqueue_colony_event(ai_popups, colony->id, body);
      }
    }
  }
}

/*
 * debug.logs: one summary line per colony per turn — net stock change per
 * cargo, hammers, and what is under construction. Field names come from
 * @CARGO when a Europe screen is loaded (headless callers get the fallback).
 */
static const char* turn_log_cargo_name(const EuropeScreen* europe, int cargo) {
  static const char* fallback[COLONIZE_CARGO_COUNT] = {
    "Food", "Sugar", "Tobacco", "Cotton", "Furs", "Lumber", "Ore", "Silver",
    "Horses", "Rum", "Cigars", "Cloth", "Coats", "Trade Goods", "Tools", "Muskets"
  };
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return "?";
  }
  if (europe && cargo < europe->cargo_count && europe->cargo[cargo].name[0]) {
    return europe->cargo[cargo].name;
  }
  return fallback[cargo];
}

static void turn_log_colony_production(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeCol1Save* col1,
  const EuropeScreen* europe,
  const int* stock_before,
  int hammers_before,
  int project_before
) {
  if (!diag_info_enabled() || !colony || !stock_before) {
    return;
  }
  char goods[512];
  goods[0] = '\0';
  size_t at = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    const int net = colony->stock[c] - stock_before[c];
    if (net == 0) {
      continue;
    }
    const int n = snprintf(
      goods + at, sizeof(goods) - at, "%s%s %+d (%d)",
      at ? ", " : "", turn_log_cargo_name(europe, c), net, colony->stock[c]
    );
    if (n <= 0 || (size_t)n >= sizeof(goods) - at) {
      break;
    }
    at += (size_t)n;
  }
  if (at == 0) {
    snprintf(goods, sizeof(goods), "nothing");
  }
  const char* building = "-";
  if (pool && colony->building_in_production >= 0 &&
      colony->building_in_production < pool->building_type_count) {
    building = pool->building_types[colony->building_in_production].name;
  } else {
    /* Artillery / Wagon Train sit above the @BUILDING range. */
    const char* unit_name = NULL;
    if (colonies_unit_build_info(colony->building_in_production, &unit_name, NULL, NULL) &&
        unit_name) {
      building = unit_name;
    }
  }
  diag_info(
    "PROD %s (id=%d nation=%d pop=%d SoL=%d%%): food %+d, hammers %+d (%d) building=%s%s",
    colony->name[0] ? colony->name : "colony",
    colony->id,
    colony->nation_id,
    colony->colonist_count > 0 ? colony->colonist_count : colony->population,
    colony_prod_sol_percent(col1, colony),
    colony->stock[COLONIZE_CARGO_FOOD] - stock_before[COLONIZE_CARGO_FOOD],
    colony->hammers - hammers_before,
    colony->hammers,
    building,
    project_before != colony->building_in_production ? " (project changed)" : ""
  );
  diag_info("PROD   %s: %s", colony->name[0] ? colony->name : "colony", goods);
}

/*
 * Nation scope for the three colony-EOT passes below.
 *
 * DOS runs FUN_364b_0688 inside each nation's own FUN_3844_00f2, and 00f2
 * runs immediately BEFORE that nation acts (year_loop.c: `nation_eot` then
 * `Move Pieces` for the human slot). So the human's colonies produce — and
 * their construction projects finish, with the @BUILT popup — at the START of
 * the human's turn, not when End Turn is pressed. This port's pipeline is
 * post-human, so it runs every AI nation's colonies in TURN_PROC_SETUP and
 * the human's in TURN_PROC_FINISH, right before control returns.
 *
 * Threaded as slice-scoped statics for the same reason as s_turn_labels: the
 * public entry points' signatures are pinned by the test call sites. Both -1
 * (the default, and what those entry points restore) means "every colony".
 */
static int s_prod_only_nation = -1;
static int s_prod_skip_nation = -1;

static bool turn_prod_nation_in_scope(int nation_id) {
  if (s_prod_only_nation >= 0 && nation_id != s_prod_only_nation) {
    return false;
  }
  if (s_prod_skip_nation >= 0 && nation_id == s_prod_skip_nation) {
    return false;
  }
  return true;
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
    if (pool->colonies[i].active && turn_prod_nation_in_scope(pool->colonies[i].nation_id)) {
      /* bugs.md 262: DOS never carries an idle colonist — sweep any
       * job-less colonist (stale saves, non-UI admit paths) into work
       * before producing, so the head count always matches the workers. */
      colonies_auto_assign_idle(pool, i);
      /* Snapshot for the debug-log summary — production itself keeps the
       * NULL delta it has always had (a non-NULL one changes which branch
       * fills cargo_produced_mask). */
      int stock_before[COLONIZE_CARGO_COUNT];
      int hammers_before = pool->colonies[i].hammers;
      int project_before = pool->colonies[i].building_in_production;
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        stock_before[c] = pool->colonies[i].stock[c];
      }
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
      turn_log_colony_production(
        pool, &pool->colonies[i], col1, europe, stock_before, hammers_before, project_before
      );
    }
  }
}

void turn_run_colony_unit_construction(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->colonies || !ctx->units) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* col = &ctx->colonies->colonies[i];
    if (!col->active || col->building_in_production < 0 ||
        !turn_prod_nation_in_scope(col->nation_id)) {
      continue;
    }
    const char* name = NULL;
    if (!colonies_unit_build_info(col->building_in_production, &name, NULL, NULL)) {
      continue;
    }
    const int uid = colonies_try_complete_unit_construction(ctx->colonies, col->id, ctx->units);
    if (uid < 0) {
      continue;
    }
    if (ctx->europe && col->nation_id == ctx->human_nation) {
      snprintf(ctx->europe->status, sizeof(ctx->europe->status), "%s completed.", name);
      /* DOS @BUILT — same "%STRING0 colony produces {%STRING1}." wording
       * turn_produce_one_colony's real-building completion path uses. */
      if (ctx->ai_popups) {
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = col->name[0] ? col->name : "colony";
        tok.string1 = name;
        popup_msg_fill(ctx->messages, "BUILT", &tok, ctx->europe->status, body, sizeof(body));
        ai_popup_enqueue_colony_event(ctx->ai_popups, col->id, body);
      }
    }
  }
}

/*
 * Player-requested: BUY only tops hammers/tools up to the completion
 * threshold — it must NOT complete the project itself (colonies_buy_construction
 * no longer calls colonies_try_complete_building). Completion happens here,
 * once per turn, unconditionally (no Spring/Autumn gate, no "did hammers
 * change this tick" gate — turn_produce_one_colony's own inline complete
 * check only fires when the colony *produces* new hammers that tick, which
 * misses a colony already sitting at/above threshold from a BUY with an
 * idle Carpenter or on a frozen Autumn tick). Sibling to
 * turn_run_colony_unit_construction above — same one-pass-per-turn shape,
 * real buildings instead of units. colonies_try_complete_building's own
 * has_building[] guard makes this safe to also run on a turn where the
 * inline per-colony-production path in turn_produce_one_colony already
 * completed the same project (second call just returns false).
 */
void turn_run_colony_building_completion(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->colonies) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* col = &ctx->colonies->colonies[i];
    if (!col->active || col->building_in_production < 0 ||
        !turn_prod_nation_in_scope(col->nation_id)) {
      continue;
    }
    const ColonizeBuildingType* bt = colonies_building_type(ctx->colonies, col->building_in_production);
    if (!bt || bt->hammers <= 0 || col->hammers < bt->hammers) {
      continue;
    }
    if (!colonies_try_complete_building(ctx->colonies, col->id)) {
      continue;
    }
    if (ctx->europe && col->nation_id == ctx->human_nation) {
      snprintf(ctx->europe->status, sizeof(ctx->europe->status), "%s completed.", bt->name);
      if (ctx->ai_popups) {
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = col->name[0] ? col->name : "colony";
        tok.string1 = bt->name;
        popup_msg_fill(ctx->messages, "BUILT", &tok, ctx->europe->status, body, sizeof(body));
        ai_popup_enqueue_colony_event(ctx->ai_popups, col->id, body);
      }
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
  ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
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
    /* Keep the col1 nation copy live — the Religious report (F1) reads
     * nation[human].current/needed_crosses, which only the save/load
     * bridge used to refresh, so a live campaign showed no crosses at all
     * (bugs.md). */
    if (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4) {
      ctx->col1->nation[ctx->human_nation].current_crosses = ctx->europe->current_crosses;
      ctx->col1->nation[ctx->human_nation].needed_crosses = ctx->europe->needed_crosses;
    }
    /* bugs.md: no immigration during the War of Independence — Europe is
     * closed to the rebels (user-observed DOS; the dock is unreachable
     * anyway once the WoI blocks the European Status). */
    const int woi_now =
      ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi != 0;
    /* DOS 68583 refills the emptied pool slot with `46d4((DS:0x538e & 3)==0)`
     * — one turn in four skips the criminal/servant/free tier roll. */
    if (ctx->europe) {
      const uint32_t turn = ctx->turn_number ? *ctx->turn_number : 0u;
      ctx->europe->pool_force_expert = ((turn & 3u) == 0u);
    }
    const int imm = woi_now ? 0
                            : europe_tick_immigration_pressure(
                                ctx->europe, ctx->colonies, ctx->units,
                                ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation, ctx->rng
                              );
    if (imm == 2) {
      /* Brewster: player picks from the pool (@RECRUITCHOOSE); applied via
       * units_brewster_apply_popup in game_loop, crosses kept until then. */
      units_brewster_enqueue_pick(ctx->europe, ctx->ai_popups, ctx->messages, ctx->human_nation);
      if (ctx->status && ctx->status_size > 0) {
        snprintf(ctx->status, ctx->status_size, "Religious unrest: choose an immigrant.");
      }
    } else if (imm == 1) {
      if (ctx->europe) {
        europe_notify_immigrant_sound(ctx->europe); /* FUN_38fd_5e52 38fd:5ecb: pool 2 */
      }
      const char* name = "";
      if (ctx->europe->dock_count > 0) {
        name = ctx->europe->dock[ctx->europe->dock_count - 1].name;
      }
      turn_notify_dock_immigrant(ctx, out, name);
      /* Mirror dock immigrant as Europe-map unit for Col1 capture. */
      if (ctx->units && ctx->europe->dock_count > 0) {
        const EuropeDockImmigrant* d = &ctx->europe->dock[ctx->europe->dock_count - 1];
        (void)europe_spawn_dock_mirror_unit(
          ctx->units, ctx->human_nation, d->profession, (int)ctx->europe->difficulty, true,
          ctx->rng
        );
      }
    }
    europe_tick_voyages(ctx->europe, ctx->units);
    /*
     * FUN_48d3_08bf: the human's Europe arrival pass ends with woodcut 9 when
     * one of the ships that just docked was carrying goods, then sets the
     * auto-open flag (DS:0x14c = europe.open_on_dock).
     */
    if (ctx->europe->docked_with_goods) {
      (void)woodcut_fire(ctx->col1, WOODCUT_CARGO_FROM_THE_NEW_WORLD);
    }
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
      /*
       * bugs.md 237 / DOS FUN_4345_0a22 (viceroy ~73346): once per war,
       * while the REF has not yet landed (0x5382 bit1 clear) and the bell
       * pool has started accruing, show @AMBUSHHINT then @CONSIDER —
       * "%STRING0 is considering intervention ... generate %NUMBER0 liberty
       * bells". Latch = 0x5382 bit 0x04 (game_options.woi_crosses_event,
       * confirmed live 2026-08-18 as exactly this dialog's one-shot).
       */
      if (!ctx->col1->head.game_options.ref_present &&
          !ctx->col1->head.game_options.woi_crosses_event &&
          founding_fathers_bells_since_last_elect(ctx->human_nation) > 0u &&
          ctx->ai_popups) {
        const int ally = (int)ctx->col1->head.rival_nation_slot_1;
        const char* ally_name = "A European power";
        /* bugs.md 258 rule: PARENT country ("France"), never the new-world
         * colony name player[ally].country_name ("New France"). */
        static const char* k_euro[4] = {"England", "France", "Spain", "Netherlands"};
        if (ally >= 0 && ally < 4) {
          ally_name = k_euro[ally];
        }
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          ctx->messages, "AMBUSHHINT", NULL,
          "Attacking the King's troops while neither unit is in a colony square "
          "gains an ambush bonus equal to the terrain's defensive value!",
          body, sizeof(body)
        );
        (void)ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = ally_name;
        tok.has_number0 = true;
        tok.number0 = (int)founding_fathers_bells_needed(ctx->col1, ctx->human_nation);
        char body2[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          ctx->messages, "CONSIDER", &tok,
          "%STRING0 is considering intervention on our behalf against the King! "
          "If we can generate %NUMBER0 liberty bells, they will join us.",
          body2, sizeof(body2)
        );
        (void)ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body2);
        ctx->col1->head.game_options.woi_crosses_event = 1;
      }
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
   * FUN_3844_00f2 §C tail (viceroy 58393-58423) is NOT ported here: it is
   * the @KINGFRIGATE event — Crown offers a FRIGATE (+10% tax) when the
   * nation's colonies are threatened by warships — and lives in ai_king.c
   * (ai_king_frigate_*). An earlier duplicate here misread type 0x11 as a
   * "Merchantman" and spawned a free ship every 8th turn (bugs.md
   * free_merchanman.SAV; player-corrected: a Merchantman is the victim,
   * not the help).
   */
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

/*
 * bugs.md: damaged ships have a homing system — the nearest OWN colony with
 * a Drydock; a nation with no such colony sends them to Europe when Europe
 * is friendly: the pre-WoI human's ships sail home as an Expected-Soon
 * voyage (that voyage IS the repair timeout — they arrive repaired), and a
 * damaged Tory Man-O-War sails back to the King's dockyards (despawn; the
 * next wave draws on the fleet pool). AI peers with no drydock keep the old
 * stay-put behavior. Runs each nation phase, after units_tick_drydock_repair.
 */
/* Drydock (DOS colony feature bit 7) or its Shipyard upgrade. */
static int turn_colony_repairs_ships(const ColonizeColony* c, int drydock, int shipyard) {
  if (!c) {
    return 0;
  }
  if (drydock >= 0 && drydock < COLONIZE_BUILDING_TYPES_MAX && c->has_building[drydock]) {
    return 1;
  }
  return shipyard >= 0 && shipyard < COLONIZE_BUILDING_TYPES_MAX && c->has_building[shipyard];
}

static void turn_route_damaged_ships(ColonizeTurnContext* ctx, int nation) {
  if (!ctx || !ctx->units || !ctx->colonies || nation < 0 || nation > 3) {
    return;
  }
  const int woi =
    ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi != 0;
  const int crown =
    woi ? ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation) : -1;
  const int drydock = colonies_find_building(ctx->colonies, "Drydock");
  const int shipyard = colonies_find_building(ctx->colonies, "Shipyard");
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(ctx->units, u->id) || (u->col1_unknown15 & 0x80u) == 0) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
    /*
     * bit7 is shared by "under construction" and "combat damaged"; only the
     * latter sets repair_pending, so that — not the turns_worked/threshold
     * comparison — is what separates them. The old threshold test never let
     * a damaged ship through: units_tick_drydock_repair, which runs first,
     * clears bit7 the moment the timer completes, so the Europe fallback
     * below was unreachable and every damaged ship stayed at whatever colony
     * combat had parked it on (bugs.md).
     */
    if (!u->repair_pending) {
      continue; /* construction — build tick owns bit7 */
    }
    /* Already sitting on an own Drydock colony: the repair tick handles it. */
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    const ColonizeColony* here = colonies_get(ctx->colonies, cid);
    if (here && here->active && here->nation_id == nation &&
        turn_colony_repairs_ships(here, drydock, shipyard)) {
      continue;
    }
    /* Nearest own Drydock colony (recomputed here: the one picked at combat
     * time may since have been captured). */
    const ColonizeColony* best = NULL;
    long best_d = -1;
    if (drydock >= 0 || shipyard >= 0) {
      for (int k = 0; k < COLONIZE_COLONIES_MAX; ++k) {
        const ColonizeColony* c = &ctx->colonies->colonies[k];
        if (!c->active || c->nation_id != nation ||
            !turn_colony_repairs_ships(c, drydock, shipyard)) {
          continue;
        }
        const long dx = c->x - u->x;
        const long dy = c->y - u->y;
        const long d = dx * dx + dy * dy;
        if (best_d < 0 || d < best_d) {
          best_d = d;
          best = c;
        }
      }
    }
    if (best) {
      const int ox = u->x;
      const int oy = u->y;
      u->x = best->x;
      u->y = best->y;
      units_occupancy_notify_moved(ctx->units, ox, oy, best->x, best->y);
      continue;
    }
    if (nation == crown) {
      /*
       * A damaged Tory Man-O-War still limps home to the King, but it leaves
       * the way an emptied one does — through ai_king_ref_wave's own "4d56
       * ship act" at the next wave tick — instead of being deleted here on
       * the spot.
       *
       * bugs.md: deleting it here made the player's coastal guns HELP the
       * invasion. DOS FUN_43f7_0982 opens with "MoW pool empty and the crown
       * owns no Man-O-War -> put one back, land nothing this turn", so the
       * fleet cadence is driven by when the last hull leaves the map. An
       * emptied hull needs one wave tick to raise turns_worked before the
       * ship act takes it home, so the normal cycle is land / hold / refill /
       * land. Deleting a damaged hull mid-turn skipped that tick, and the
       * refill (and the next landing) arrived a full turn EARLIER than if the
       * player had never fired: 7 human colonies fell in 14 turns under
       * continuous bombardment versus 15 turns untouched. Leaving it to the
       * ship act restores parity — the hull is gone either way, just never
       * sooner for having been shot at.
       */
      continue;
    }
    if (nation == ctx->human_nation && !woi && ctx->europe && u->cargo_count == 0) {
      const int turns = europe_voyage_turns_roll(ctx->rng, false, 1);
      /* Same edge rule as the manual sail-to-Europe path so the ship comes
       * back on the side it left from. */
      const bool east = ctx->map ? (u->x >= (int)ctx->map->width / 2) : true;
      if (europe_enqueue_expected(
            ctx->europe, u->type_index, ty ? ty->name : "Ship", NULL, NULL, 0,
            u->hold_goods_type, u->hold_goods_amount, u->x, u->y, east, turns
          )) {
        if (ctx->status && ctx->status_size > 0) {
          snprintf(
            ctx->status, ctx->status_size, "%s sails to Europe for repairs.",
            ty && ty->name[0] ? ty->name : "Damaged ship"
          );
        }
        u->col1_unknown15 = (uint8_t)(u->col1_unknown15 & 0x7fu); /* repaired abroad */
        units_despawn(ctx->units, u->id);
      }
    }
  }
}

/*
 * FUN_43f7_2424 war dispatch: once independence is declared the crown slot
 * is the REF, driven by ai_king_nation_turn's 2022 branch (wave + war_act),
 * not the ordinary Euro unit AI. Running ai_euro_nation_turn on it first
 * spent every landed Regular's moves before war_act ever saw them (found
 * 2026-08-28 by a headless WoI run: 40 turns, zero attacks).
 */
static bool turn_euro_nation_is_ref(const ColonizeTurnContext* ctx, int n) {
  return ctx && ctx->col1_ok && ctx->col1 && ai_king_independence_declared(ctx->col1) &&
         n == ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation);
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
    if (control == 2 && !turn_euro_nation_is_ref(ctx, n)) {
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
    turn_route_damaged_ships(ctx, n);
    if (turn_euro_nation_is_ref(ctx, n)) {
      continue; /* REF: ai_king_nation_turn (war_act) moves these units. */
    }
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
  const int crown = ai_king_crown_nation_col1(col1, human);
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

  /* Anniversary chrome stops once scoring completed OR the war already
   * resolved (the WON latch no longer implies calendar_latch — that bit is
   * now set by the retire-score chain, as DOS's 0x5382|0x10 is). */
  const int splash_done =
    ctx->col1_ok && ctx->col1 &&
    (ctx->col1->head.game_options.calendar_latch ||
     ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) != AI_KING_ENDGAME_NONE);

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
  /* Endgame latch WON = independence achieved (reports); also skip re-fire. */
  const int already_won =
    ctx->col1_ok && ctx->col1 &&
    ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) == AI_KING_ENDGAME_WON;

  /*
   * Section C1 thin: WoI + (no crown colonies | force) + fleet thin + REF pool
   * thin. Force (0x5382 bit5) bypasses fleet/REF gates. Cite: year_end_chrome.md.
   */
  if (woi && !already_won && !out->year_end_defeat) {
    const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, ctx->human_nation);
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
        /* DOS crown land-force types 0x06/0x08/0x0b (Regulars/Cavalry/
         * Artillery) — matched by name so synthetic pools count right. */
        const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
        const char* n = t ? t->name : NULL;
        if (n && (strstr(n, "Regular") ||
                  (strstr(n, "Cavalry") && !strstr(n, "Cont")) ||
                  strstr(n, "Artillery") || strstr(n, "Cannon"))) {
          warships++;
        }
      }
    }
    /* DOS 3844_0442: bit 0x40 (crown captured a colony this war) LOOSENS the
     * give-up bar to <8 land units; without it the King fights to the last
     * (<1). Was inverted. */
    const int fleet_cap =
      (ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.ref_unit_threshold) ? 8 : 1;
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
        ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_WON);
        ctx->col1->head.game_options.independence_chrome = 1; /* 0x5382|8 */
        ctx->col1->head.show_entire_map = 1; /* LAB_0b4a → DS:0x53a2 */
        /* calendar_latch (0x5382|0x10 "scoring complete") is NOT set here:
         * DOS sets it only after the score chain runs (main-loop 0x104
         * block); setting it at the latch suppressed the win sequence
         * (bugs.md 265 — the game just carried on with a status line). */
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

  int human_colonies = 0;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == ctx->human_nation) {
        human_colonies++;
      }
    }
  }
  /*
   * bugs.md: the zero-colonies defeat was peacetime-only — the King
   * capturing every town during the War of Independence ended nothing.
   * During WoI the same condition is the revolution being crushed, and it
   * does not wait for the year-1600 grace either.
   */
  if (woi) {
    if (human_colonies == 0) {
      out->year_end_defeat = true;
      if (ctx->col1_ok && ctx->col1) {
        ctx->col1->head.game_options.calendar_latch = 1;
        ctx->col1->head.turn_loop_running = 0;
        /* bugs.md: keep the endgame latch in step so the @LOSING game-over
         * chain (retire score) recognizes the war as over too. */
        if (ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) == AI_KING_ENDGAME_NONE) {
          ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
        }
      }
      if (ctx->status && ctx->status_size > 0) {
        snprintf(ctx->status, ctx->status_size, "The revolution is crushed.");
      }
    }
    return;
  }
  if (year < 1600) {
    return;
  }
  /* Section B: peacetime only. */
  if (human_colonies != 0) {
    return;
  }
  out->year_end_defeat = true;
  if (ctx->col1_ok && ctx->col1) {
    ctx->col1->head.game_options.calendar_latch = 1; /* LAB_0b4a when stopped */
    ctx->col1->head.turn_loop_running = 0; /* DS:0x53c2 clear */
    /*
     * DOS Section B tail (LAB_3844_04ec): the @LOSENOCOLONIES dialog (string
     * 0xf09; 0438 slot 0 = difficulty title, 0416 slot 1 = leader name) and
     * then the Hall of Fame. Latch ENDGAME_LOST so game_loop's drained-dialog
     * gate runs the retire-score chain — same machinery as the WoI crush
     * above; without it this branch only wrote a status line and the game
     * never actually ended.
     */
    const bool first_time =
      ai_king_latch_get(ctx->col1, AI_KING_ENDGAME_BYTE) == AI_KING_ENDGAME_NONE;
    if (first_time) {
      ai_king_latch_set(ctx->col1, AI_KING_ENDGAME_BYTE, AI_KING_ENDGAME_LOST);
    }
    /* Once only — the chrome runs again every year while the board is empty. */
    if (first_time && ctx->ai_popups) {
      static const char* k_diff[5] = {
        "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
      };
      const int d = (int)ctx->col1->head.difficulty;
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = (d >= 0 && d <= 4) ? k_diff[d] : "Viceroy";
      tok.string1 = (ctx->human_nation >= 0 && ctx->human_nation < 4)
        ? ctx->col1->player[ctx->human_nation].name
        : "";
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        ctx->messages, "LOSENOCOLONIES", &tok,
        "Our efforts in the New World have proven fruitless and we have "
        "decided to remove you as Viceroy. You may, as always, kiss our "
        "royal pinky ring.",
        body, sizeof(body)
      );
      (void)ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    }
  }
  if (ctx->status && ctx->status_size > 0) {
    snprintf(ctx->status, ctx->status_size, "Defeat: no colonies remain.");
  }
}

static bool turn_euro_ai_should_run(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4 || nation_id == ctx->human_nation) {
    return false;
  }
  if (turn_euro_nation_is_ref(ctx, nation_id)) {
    return true; /* crown slot is withdrawn (control 2) but its REF units still need moves */
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

/*
 * DOS FUN_130d_0290 runs, per year tick: mid-pass Indian turns (4d56_1b3a →
 * 1816 per slot), then the Euro 0..3 loop with the human's Move Pieces inside
 * it. Linux's pipeline starts after the human ends their turn, so relative
 * to that point the DOS order is: Euro slots above the human → Indians → Euro
 * slots below the human. Seed-100 golden (human = slot 0): the Dutch
 * TURN2→3 landing meets an Aztec Brave that only moves away in the Indian
 * pass that follows. No human slot (headless sims) keeps Indians first.
 */
static int turn_human_slot(const ColonizeTurnContext* ctx) {
  return (ctx && ctx->human_nation >= 0 && ctx->human_nation < 4) ? ctx->human_nation : -1;
}

/* Next Euro AI slot strictly above the human (pre-Indian pass), or -1. */
static int turn_next_euro_ai_above_human(const ColonizeTurnContext* ctx, int start) {
  const int h = turn_human_slot(ctx);
  if (h < 0) {
    return -1;
  }
  return turn_next_euro_ai(ctx, start > h + 1 ? start : h + 1);
}

/* Next Euro AI slot below the human (post-Indian pass), or -1. */
static int turn_next_euro_ai_below_human(const ColonizeTurnContext* ctx, int start) {
  const int h = turn_human_slot(ctx);
  const int next = turn_next_euro_ai(ctx, start);
  if (next < 0 || (h >= 0 && next > h)) {
    return -1;
  }
  return next;
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
  /* LABELS.TXT wording for the status lines composed deep inside production;
   * threaded as a slice-scoped static because turn_run_colony_production's
   * signature is pinned by ~60 test call sites. */
  s_turn_labels = ctx->labels;

  switch (proc->step) {
    case TURN_PROC_SETUP: {
      diag_info(
        "TURN setup: turn=%u year=%u autumn=%u human=%d",
        (unsigned)*ctx->turn_number, (unsigned)*ctx->game_year,
        (unsigned)*ctx->game_autumn, ctx->human_nation
      );
      /* DOS FUN_3844_00f2 opens every nation's EOT with turn_owner_chrome
       * (281f_0590 → 1984_00aa); the human's own colony EOT now runs in
       * TURN_PROC_FINISH, which re-arms the same chrome there. */
      turn_set_active_nation(ctx, ctx->human_nation);
      proc->show_indicator = true;
      proc->year_before = *ctx->game_year;
      turn_advance_calendar(ctx->game_year, ctx->game_autumn, ctx->turn_number);
      proc->result.advanced = true;
      if (ctx->col1_ok && ctx->col1) {
        ctx->col1->head.turn =
          (uint16_t)(*ctx->turn_number > 65535u ? 65535u : *ctx->turn_number);
        ctx->col1->head.year = *ctx->game_year;
        ctx->col1->head.autumn = *ctx->game_autumn;
      }
      turn_set_birth_units_pool(ctx->units);
      /* AI nations only — the human's colonies run their EOT at the top of
       * TURN_PROC_FINISH instead, which is where DOS puts it (see the
       * s_prod_only_nation comment). */
      s_prod_skip_nation = ctx->human_nation;
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
      turn_set_birth_units_pool(NULL);
      /* Artillery construction completion — turn_run_colony_production has
       * no ColonizeUnitPool access (colonies_try_complete_building never
       * needed one; spawning a unit does), so this is its own pass. */
      turn_run_colony_unit_construction(ctx);
      /* BUY-topped-up (or otherwise carpenter-idle/Autumn-frozen) real
       * buildings sitting at/above threshold — turn_produce_one_colony's
       * inline complete check only fires on a tick that adds new hammers. */
      turn_run_colony_building_completion(ctx);
      s_prod_skip_nation = -1;
      /* FUN_364b_03f6 coastal Fort/Fortress fire after production. */
      (void)turn_run_coastal_fort_fire(ctx);
      turn_run_nation_ticks(ctx, &proc->result);
      /*
       * DOS FUN_364b_0688 Phase A: bells + Congress (291f_09f8 → 4345_0a22)
       * run in the colony-EOT PROLOGUE, so an FF nomination/election dialog
       * presents BEFORE the colony production messages. The port computes
       * production first (RNG draw order pinned by goldens); reorder the
       * presentation queue instead. Now that the human's own colony EOT sits
       * in TURN_PROC_FINISH this is normally a no-op (only human colonies
       * enqueue colony events, and none exist yet), kept for AI-owned queues
       * and for the synchronous turn_end path. Cite:
       * turn/colony_eot_production.md Phase A; turn/nation_ticks_bells_ff.md.
       */
      if (ctx->ai_popups) {
        ai_popup_promote_tag_before(
          ctx->ai_popups, AI_POPUP_TAG_FF_CONGRESS, AI_POPUP_TAG_COLONY_EVENT
        );
      }
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
      /*
       * DOS order relative to the human's end-of-turn (see
       * turn_human_slot): Euro slots above the human first, then the
       * 4d56_1b3a mid-pass Indian turns, then the slots below the human.
       * See turn/mid_pass_indian_rank.md, turn/year_loop.c.
       */
      {
        const int next = turn_next_euro_ai_above_human(ctx, 0);
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
      diag_info("TURN european nation %d%s", n, n == ctx->human_nation ? " (human)" : "");
      proc->show_indicator = true;
      turn_set_active_nation(ctx, n);
      if (ctx->units) {
        units_occupancy_rebuild(ctx->units); /* presence bits exact before this nation reads them */
      }
      /* 00f2 unit walk: 281f_07a0 reveal precedes the per-unit ticks. */
      if (n != ctx->human_nation) {
        turn_reveal_fog_for_nation(ctx, n);
      }
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
          turn_route_damaged_ships(ctx, n);
          if (want_eu && n == ctx->human_nation) {
            proc->result.request_europe_open = true;
          }
        }
      }
      if (!turn_euro_nation_is_ref(ctx, n)) {
        ai_euro_nation_turn(ctx, n);
      }
      {
        const int h = turn_human_slot(ctx);
        if (h >= 0 && n > h) {
          /* Pre-Indian pass (slots above the human). */
          const int next = turn_next_euro_ai_above_human(ctx, n + 1);
          if (next >= 0) {
            proc->nation_cursor = next;
          } else {
            proc->nation_cursor = 4;
            proc->step = TURN_PROC_INDIAN;
          }
        } else {
          /* Post-Indian pass (slots below the human, or every slot without one). */
          const int next = turn_next_euro_ai_below_human(ctx, n + 1);
          if (next >= 0) {
            proc->nation_cursor = next;
          } else {
            proc->step = TURN_PROC_FINISH;
          }
        }
      }
      break;
    }
    case TURN_PROC_INDIAN: {
      const int n = proc->nation_cursor;
      diag_info("TURN native nation %d", n);
      proc->show_indicator = true;
      turn_set_active_nation(ctx, n);
      if (ctx->units) {
        units_occupancy_rebuild(ctx->units);
      }
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
        const int next = turn_next_euro_ai_below_human(ctx, 0);
        if (next >= 0) {
          proc->nation_cursor = next;
          proc->step = TURN_PROC_EURO;
        } else {
          proc->step = TURN_PROC_FINISH;
        }
      }
      break;
    }
    case TURN_PROC_FINISH: {
      proc->show_indicator = false;
      /*
       * The human nation's own colony EOT — DOS FUN_3844_00f2 runs it right
       * before that nation's Move Pieces, so a construction project finishes
       * (and announces itself) at the start of the player's turn, not the
       * instant End Turn is pressed. Ahead of the king / census / market work
       * below, matching 00f2's own production-then-census-then-king order.
       */
      turn_set_active_nation(ctx, ctx->human_nation);
      proc->show_indicator = true;
      s_prod_only_nation = ctx->human_nation;
      turn_set_birth_units_pool(ctx->units);
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
      turn_set_birth_units_pool(NULL);
      turn_run_colony_unit_construction(ctx);
      turn_run_colony_building_completion(ctx);
      s_prod_only_nation = -1;
      proc->show_indicator = false;
      turn_run_king_stub(ctx);
      turn_run_year_end_chrome(ctx, &proc->result);
      /* FUN_38fd_0058 EOT market attrition / rise-fall for Europe screen. */
      if (ctx->europe) {
        europe_tick_market_prices(
          ctx->europe, ctx->col1_ok ? ctx->col1 : NULL, ctx->colonies, ctx->human_nation,
          ctx->turn_number ? *ctx->turn_number : 0u
        );
        /* FUN_38fd_0058 phase 4: 0xfa8 @PRICEUP / 0xfb0 @PRICEDOWN OK dialog
         * (FUN_281f_0652(tag, 2)) for the human nation only. DOS calls this
         * inline once per cargo that crosses threshold — a turn where two
         * different goods both move gets two separate dialogs, not one;
         * queue every entry (previously only the last event survived). */
        if (ctx->ai_popups) {
          for (int i = 0; i < ctx->europe->price_event_count; ++i) {
            const int c = ctx->europe->price_event_cargo[i];
            PopupMsgTokens tok;
            memset(&tok, 0, sizeof(tok));
            tok.string0 = ctx->europe->cargo[c].name;
            tok.string1 = ctx->europe->port_city;
            tok.number0 = ctx->europe->cargo[c].bid;
            tok.has_number0 = true;
            char body[AI_POPUP_BODY_LEN];
            popup_msg_fill(
              ctx->messages, ctx->europe->price_event_dir[i] > 0 ? "PRICEUP" : "PRICEDOWN", &tok,
              ctx->europe->status, body, sizeof(body)
            );
            (void)ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
          }
        }
        /* Shown — clear, or the next player sell/buy replays these stale
         * events via game_europe_drain_price_events and an unrelated cargo
         * (e.g. Coats) looks like it moved in response to that sale. */
        ctx->europe->price_event_count = 0;
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
        turn_route_damaged_ships(ctx, ctx->human_nation);
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
        if (ctx->col1_ok && ctx->col1) {
          /* FUN_465b_0000 → FUN_5fef_1908 King's Galleon offer (human only). */
          (void)units_king_galleon_offer_coastal_treasures(
            ctx->units,
            ctx->colonies,
            ctx->map,
            ctx->europe,
            ctx->col1,
            ctx->human_nation,
            ctx->ai_popups,
            ctx->messages
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
