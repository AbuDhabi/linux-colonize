#include "core/ai_king.h"

#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * FUN_43f7_* King/REF/independence — partial structural port.
 * Thin map: original_sources_annotated/ai/king_ref.md
 *
 * WoI: head.unknown46[0] stand-in for DOS 0x5382 bit0 (exact Col1 bit PARKED).
 *   Set on declare when SoL≥50 (ai_king_set_independence); restless chrome must not.
 * REF-present: head.unknown46[1] stand-in for 0x5382 bit1.
 * Tax-boycott/refuse: head.unknown46[2] stand-in + thin 38fd_5be8 audience
 *   (status + ai_popup CHOICE Accept/Refuse when ctx->ai_popups; auto when NULL).
 *   Refuse apply/auto → Sugar follow-up OK (KING_TAX; lists all boycott_bitmap
 *   cargo names — Sugar + RNG second when ctx->rng). Boycott-holds status/OK
 *   also lists all bitmap cargo names (presentation; Fugger partial clear).
 *   Cargo freeze: nation.boycott_bitmap. Fugger/diplo bitmap clear → drop
 *   unknown46[2] refuse when bitmap==0 (king sync; do not touch FF).
 * Merc hired/refused this war: head.unknown46[3] + thin 2244 hire
 *   (ai_popup CHOICE Hire/Decline when ctx->ai_popups; auto when NULL) or
 *   "Cannot afford mercenaries." OK once (flag gates spam).
 *   Hire apply/auto success → follow-up OK (same status body).
 *   Decline apply → follow-up OK ("Mercenaries declined."; gate unknown46[3]).
 * 160a rename: player[human].country_name → "United Colonies"
 *   (letter cinematic PARKED — thin rename + KING_LETTER Done).
 *   unknown46[4] endgame latch: 0 none / 1 won / 2 lost.
 *   On declare + ai_popups: thin rename OK + "War of Independence begins" OK.
 * Congress confirm: head.unknown46[5] + thin 2564 (ai_popup CHOICE Confirm/Not yet
 *   when ctx->ai_popups; auto-declare when NULL; same-turn 1528 may overwrite status).
 * Revolution end: lose if 0 coastal ports; win if year≥1850 + no crown units.
 * SoL restless chrome (40..49): status + INFO OK when human sees restless.
 * backup_force: DOS 0x53e2… foreign pools — 10f0 stand-in (seeded on declare).
 * Crown nation_id: non-human Euro slot (1 if human==0 else 0).
 */

#define AI_KING_WOI_BYTE 0
#define AI_KING_REF_PRESENT_BYTE 1
#define AI_KING_BOYCOTT_BYTE 2
#define AI_KING_MERC_HIRED_BYTE 3
/* Endgame latch: 0 none, 1 revolution won, 2 revolution lost (was rename-reserved). */
#define AI_KING_ENDGAME_BYTE 4
#define AI_KING_ENDGAME_NONE 0
#define AI_KING_ENDGAME_WON 1
#define AI_KING_ENDGAME_LOST 2
#define AI_KING_ENDGAME_PEACE_1800 3
#define AI_KING_CONGRESS_BYTE 5

#define AI_KING_INDEP_COUNTRY "United Colonies"
#define AI_KING_YEAR_CAP 1850
#define AI_KING_PEACE_YEAR_CAP 1800

/* Structural refuse thresholds (exact DOS 38fd_5be8 gates still thin). */
#define AI_KING_BOYCOTT_TAX_MIN 20
#define AI_KING_BOYCOTT_SOL_MIN 30
#define AI_KING_BOYCOTT_BELLS_MIN 80
/* Sugar = cargo index 1 (COLONIZE_CARGO_SUGAR) — structural refuse freeze
 * (king_ref / 38fd_5be8 stand-in). Dump-goods “named goods” RNG is a separate
 * FUN_38fd_3dc8 pick — use ai_king_pick_dump_goods_cargo; do not invent a
 * fixed Tobacco/etc. second refuse bit here.
 * Cite: docs/fandom_col1994.md Boycott; viceroy FUN_38fd_3dc8. */
#define AI_KING_BOYCOTT_CARGO_BIT (1u << COLONIZE_CARGO_SUGAR)
/* Thin 2244 Continental merc aid (hire dialog / ai_popup CHOICE). */
#define AI_KING_MERC_COST 300
#define AI_KING_MERC_SOL_MIN 50
/*
 * FUN_43f7_2564 / fandom Independence: declare gate when total SoL already
 * past this existing threshold (no invented %). Bells gate stays separate.
 * Human + ai_popups → CHOICE; else auto-declare.
 */
#define AI_KING_DECLARE_SOL_MIN 50
#define AI_KING_DECLARE_BELLS_MIN 100
/* Restless chrome band immediately below declare (SoL 40..49 when min=50). */
#define AI_KING_RESTLESS_SOL_MIN 40
/*
 * MoW hold fill uses real ship capacity (units_ship_capacity / type->cargo,
 * capped at COLONIZE_UNIT_CARGO_MAX=6). Cite: fandom REF “man-o-war with 6
 * units”; units_board_stacked. Coastal unload dumps multiple cargo_ids per
 * war_act beat up to min(moves_left, capacity) (1 MP/pax); full unload with
 * moves left → AI_SAIL next human coast; after that sail step, if still
 * carrying and now adjacent to the next colony → unload same beat.
 * PARK: 160a letter cinematic; full embark UI chrome; dump-goods boycott modal
 * CHOICE Done (pick API + Europe bid>0 weight for auto; KING_DUMP_GOODS for
 * human; VGA PARKED).
 */
/* 10f0: dual landing base; third when difficulty ≥ 2 (REF pressure stand-in). */
#define AI_KING_INTERVENE_LANDINGS_BASE 2
#define AI_KING_INTERVENE_DIFF_THIRD 2
/* 0982: second MoW same beat when difficulty ≥ 2 and force[2] still > 0. */
#define AI_KING_SECOND_MOW_DIFF 2
/*
 * REF idle hunt capital bias: when founding-capital MD is within this slack of
 * the nearest other human colony MD, prefer the capital (fandom REF pressure
 * on main ports; deep multi-step scoring PARKED).
 */
#define AI_KING_CAPITAL_MD_SLACK 2

/* ai_popup choice_ids (FUN_43f7_38fd_5be8 / 2244 / 2564). */
#define AI_KING_CHOICE_ACCEPT 1
#define AI_KING_CHOICE_REFUSE 2
#define AI_KING_CHOICE_HIRE 1
#define AI_KING_CHOICE_DECLINE 2
#define AI_KING_CHOICE_CONFIRM 1
#define AI_KING_CHOICE_NOT_YET 2

static int ai_king_crown_nation(int human_nation) {
  return (human_nation == 0) ? 1 : 0;
}

int ai_king_pick_dump_goods_cargo(
  uint16_t boycott_bitmap,
  uint16_t candidate_mask,
  ColonizeDosRng* rng,
  const int* cargo_bid
) {
  /*
   * Eligible = candidate_mask & ~boycott_bitmap (FUN_38fd_3dc8 skips bits
   * already set in nation boycott_bitmap / local_a6). When cargo_bid non-NULL,
   * also require bid[c] > 0 (live Europe local_7a — do not dump zero-price
   * goods), then roulette by bid. When cargo_bid NULL → uniform among mask.
   * Cite: FUN_38fd_3dc8 / king_ref dump-goods.
   */
  if (!rng) {
    return -1;
  }
  const uint16_t eligible = (uint16_t)(candidate_mask & (uint16_t)~boycott_bitmap);
  if (eligible == 0) {
    return -1;
  }
  int idxs[COLONIZE_CARGO_COUNT];
  int n = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    if ((eligible & (uint16_t)(1u << c)) == 0) {
      continue;
    }
    if (cargo_bid && cargo_bid[c] <= 0) {
      continue;
    }
    idxs[n++] = c;
  }
  if (n <= 0) {
    return -1;
  }
  if (!cargo_bid) {
    const int pick = dos_rng_range(rng, 0, n - 1);
    if (pick < 0 || pick >= n) {
      return -1;
    }
    return idxs[pick];
  }
  int total = 0;
  int weights[COLONIZE_CARGO_COUNT];
  for (int i = 0; i < n; ++i) {
    const int c = idxs[i];
    const int w = cargo_bid[c];
    weights[i] = w;
    total += w;
  }
  if (total <= 0) {
    return -1;
  }
  const int roll = dos_rng_range(rng, 1, total);
  int cum = 0;
  for (int i = 0; i < n; ++i) {
    cum += weights[i];
    if (roll <= cum) {
      return idxs[i];
    }
  }
  return idxs[n - 1];
}

/* @CARGO display names (colony.h / NAMES.TXT / reports.c) for boycott chrome. */
static const char* ai_king_cargo_name(int cargo_idx) {
  static const char* const names[COLONIZE_CARGO_COUNT] = {
    "Food",        "Sugar",  "Tobacco", "Cotton", "Furs",  "Lumber",
    "Ore",         "Silver", "Horses",  "Rum",    "Cigars", "Cloth",
    "Coats",       "Trade Goods", "Tools", "Muskets"
  };
  if (cargo_idx < 0 || cargo_idx >= COLONIZE_CARGO_COUNT) {
    return "cargo";
  }
  return names[cargo_idx];
}

/*
 * Comma-separated @CARGO names set in boycott_bitmap (presentation only).
 * Returns 1 if any bit set. Cite: king_ref refuse/holds chrome; Fugger partial
 * clear may leave a subset of bits while unknown46[2] still holds.
 */
static int ai_king_format_boycott_cargos(char* buf, size_t buf_size, uint16_t bitmap) {
  if (!buf || buf_size == 0) {
    return 0;
  }
  buf[0] = '\0';
  size_t pos = 0;
  int any = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    if ((bitmap & (uint16_t)(1u << c)) == 0) {
      continue;
    }
    if (any) {
      if (pos + 2 >= buf_size) {
        break;
      }
      pos += (size_t)snprintf(buf + pos, buf_size - pos, ", ");
    }
    pos += (size_t)snprintf(buf + pos, buf_size - pos, "%s", ai_king_cargo_name(c));
    any = 1;
  }
  return any;
}

/* Human-facing map popup queue attached (game_loop); AI/auto path when NULL. */
static int ai_king_human_popups(const ColonizeTurnContext* ctx) {
  return (ctx && ctx->ai_popups) ? 1 : 0;
}

/* Active colony count for a Euro nation (10f0 intervene nation pick). */
static int ai_king_colony_count(const ColonizeColonyPool* colonies, int nation_id) {
  if (!colonies || nation_id < 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (c->active && c->nation_id == nation_id) {
      n++;
    }
  }
  return n;
}

/*
 * Crown-hostile Euro slot for 10f0 landings (not human, not crown).
 * Prefer the Euro with most colonies; tie-break by on-map land unit count
 * (structural force presence — head.backup_force is a shared pool, not per-nation).
 * Source: FUN_43f7_10f0 intervene nation pick; Foreign intervention (fandom).
 */
static int ai_king_intervention_nation(const ColonizeTurnContext* ctx, int human_nation) {
  const int crown = ai_king_crown_nation(human_nation);
  int best = -1;
  int best_colonies = -1;
  int best_force = -1;
  for (int n = 0; n < 4; ++n) {
    if (n == human_nation || n == crown) {
      continue;
    }
    const int cols = ctx && ctx->colonies ? ai_king_colony_count(ctx->colonies, n) : 0;
    int force = 0;
    if (ctx && ctx->units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != n) {
          continue;
        }
        if (units_is_sea(ctx->units, u->id)) {
          continue;
        }
        force++;
      }
    }
    if (best < 0 || cols > best_colonies ||
        (cols == best_colonies && force > best_force)) {
      best = n;
      best_colonies = cols;
      best_force = force;
    }
  }
  return best >= 0 ? best : crown;
}

/* True if unit type/display name is Artillery (or Cannon fallback). */
static int ai_king_is_artillery(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Artillery")) || (dname && strstr(dname, "Artillery"))) {
    return 1;
  }
  if ((tname && strstr(tname, "Cannon")) || (dname && strstr(dname, "Cannon"))) {
    return 1;
  }
  return 0;
}

/* Artillery type in pool, or -1 (thin siege bias gates on this). */
static int ai_king_artillery_type(const ColonizeUnitPool* units) {
  if (!units) {
    return -1;
  }
  int ty = units_find_type(units, "Artillery");
  if (ty < 0) {
    ty = units_find_type(units, "Cannon");
  }
  return ty;
}

/*
 * Continental Army / Cont. Army / Cont. Cav after 1eca — include in hunter
 * name check so Cont. types hunt like Regular/Dragoon (fandom Independence:
 * Veteran → Continental Army/Cavalry).
 */
static int ai_king_is_continental(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Continental")) || (dname && strstr(dname, "Continental"))) {
    return 1;
  }
  if ((tname && strstr(tname, "Cont. Army")) || (dname && strstr(dname, "Cont. Army"))) {
    return 1;
  }
  if ((tname && strstr(tname, "Cont. Cav")) || (dname && strstr(dname, "Cont. Cav"))) {
    return 1;
  }
  return 0;
}

/*
 * REF land hunters: Regular / Dragoon (fandom REF AI land arm).
 * Thin Artillery siege: when Artillery type exists in pool, Artillery also hunts
 * (prefer fortified colony — see ai_king_ref_hunt_target). Cont. Army / Cont. Cav
 * included in name check (1eca promote; human Cont. hunt → capital/colony below).
 * Deep multi-step siege scoring remains PARKED.
 */
static int ai_king_is_ref_land_hunter(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u || !u->active || units_is_sea(units, u->id)) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Regular")) || (dname && strstr(dname, "Regular"))) {
    return 1;
  }
  if ((tname && strstr(tname, "Dragoon")) || (dname && strstr(dname, "Dragoon"))) {
    return 1;
  }
  if (ai_king_is_continental(units, u)) {
    return 1;
  }
  if (ai_king_artillery_type(units) >= 0 && ai_king_is_artillery(units, u)) {
    return 1;
  }
  return 0;
}

/* True if unit type/display is Regular (for post-capture fortify). */
static int ai_king_is_regular(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Regular")) || (dname && strstr(dname, "Regular"))) {
    return 1;
  }
  return 0;
}

/* True if unit type/display is Dragoon (open-land hunt bias when Artillery exists). */
static int ai_king_is_dragoon(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  if ((tname && strstr(tname, "Dragoon")) || (dname && strstr(dname, "Dragoon"))) {
    return 1;
  }
  return 0;
}

/*
 * Open-land hunt role (Dragoon thin bias + Cont. Cav as cavalry promote).
 * Source: fandom REF AI land arm / Independence Cont. Cavalry; when Artillery
 * type exists, leave fortified ports to Artillery. Cont. Army stays nearest.
 */
static int ai_king_prefers_open_land(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (ai_king_is_dragoon(units, u)) {
    return 1;
  }
  if (!units || !u || !ai_king_is_continental(units, u)) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(units, u);
  /* Cont. Cav / Continental Cavalry only — not Cont. Army. */
  if ((tname && (strstr(tname, "Cav") || strstr(tname, "Cavalry"))) ||
      (dname && (strstr(dname, "Cav") || strstr(dname, "Cavalry")))) {
    return 1;
  }
  return 0;
}

/*
 * Cavalry garrison fallback when no Regular: Dragoon or Cont. Cav (not Cont. Army).
 * Cite: Colonization.pdf Defending a Colony ("fortify soldiers, dragoons…");
 * king_ref one-garrison. Reuses open-land cavalry name check.
 */
static int ai_king_is_garrison_cavalry(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  return ai_king_prefers_open_land(units, u);
}

/*
 * Count crown garrison units (Regular or Dragoon/Cont. Cav) on (x,y) already
 * FORTIFY/FORTIFIED. Cap-2 structural multi-garrison (Defending a Colony);
 * Artillery holds separately (not this stack).
 * Cite: Colonization.pdf Defending a Colony; king_ref thin multi-garrison.
 */
static int ai_king_tile_fortified_garrison_count(const ColonizeTurnContext* ctx, int crown,
                                                 int x, int y, int except_id) {
  if (!ctx || !ctx->units || crown < 0) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown || u->id == except_id) {
      continue;
    }
    if (u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_regular(ctx->units, u) && !ai_king_is_garrison_cavalry(ctx->units, u)) {
      continue;
    }
    if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
      count++;
    }
  }
  return count;
}

/* Cont. Army (not Cav) — human founding-capital garrison pool. */
static int ai_king_is_cont_army(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  return ai_king_is_continental(units, u) && !ai_king_prefers_open_land(units, u);
}

/* Cont. Cav — human founding-capital garrison pool (crown cap-2 uses Cont.Cav only). */
static int ai_king_is_cont_cav_garrison(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  return ai_king_is_continental(units, u) && ai_king_prefers_open_land(units, u);
}

static int ai_king_is_cont_garrison(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  return ai_king_is_cont_army(units, u) || ai_king_is_cont_cav_garrison(units, u);
}

/*
 * Count human Cont. Army / Cont. Cav on (x,y) already FORTIFY/FORTIFIED.
 * Cite: Colonization.pdf Defending a Colony; king_ref Cont. capital-rally fortify.
 */
static int ai_king_tile_human_cont_fortified_count(const ColonizeTurnContext* ctx, int human,
                                                   int x, int y, int except_id) {
  if (!ctx || !ctx->units || human < 0) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != human || u->id == except_id) {
      continue;
    }
    if (u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_cont_garrison(ctx->units, u)) {
      continue;
    }
    if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
      count++;
    }
  }
  return count;
}

/*
 * Fortify one idle human Cont. garrison on (x,y). Prefer Cont. Army over
 * Cont. Cav; prefer prefer_id when eligible. Returns unit id fortified, or -1.
 */
static int ai_king_fortify_one_cont_on_tile(ColonizeTurnContext* ctx, int human, int x, int y,
                                           int require_moves, int prefer_id) {
  if (!ctx || !ctx->units || human < 0) {
    return -1;
  }
  ColonizeUnitPool* pool = ctx->units;

  if (prefer_id >= 0) {
    ColonizeUnit* p = units_get(pool, prefer_id);
    if (p && p->active && p->nation_id == human && p->x == x && p->y == y &&
        ai_king_is_cont_army(pool, p) && p->orders != UNITS_ORDER_FORTIFY &&
        p->orders != UNITS_ORDER_FORTIFIED && (!require_moves || p->moves_left > 0)) {
      (void)units_order_fortify(pool, p->id);
      return p->id;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != human || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_cont_army(pool, u) || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    if (require_moves && u->moves_left <= 0) {
      continue;
    }
    (void)units_order_fortify(pool, u->id);
    return u->id;
  }
  if (prefer_id >= 0) {
    ColonizeUnit* p = units_get(pool, prefer_id);
    if (p && p->active && p->nation_id == human && p->x == x && p->y == y &&
        ai_king_is_cont_cav_garrison(pool, p) && p->orders != UNITS_ORDER_FORTIFY &&
        p->orders != UNITS_ORDER_FORTIFIED && (!require_moves || p->moves_left > 0)) {
      (void)units_order_fortify(pool, p->id);
      return p->id;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != human || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_cont_cav_garrison(pool, u) || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    if (require_moves && u->moves_left <= 0) {
      continue;
    }
    (void)units_order_fortify(pool, u->id);
    return u->id;
  }
  return -1;
}

/*
 * Idle human Cont. on founding capital: fortify up to two (Army prefer, then
 * Cav). Second slot needs moves_left > 0. Cite: Defending a Colony cap 2;
 * king_ref Cont. capital-rally hold + fortify.
 */
static void ai_king_fortify_human_cont_at(ColonizeTurnContext* ctx, ColonizeUnit* u,
                                          int human, int x, int y) {
  if (!ctx || !ctx->units || human < 0) {
    return;
  }
  if (ai_king_tile_human_cont_fortified_count(ctx, human, x, y, -1) >= 2) {
    return;
  }
  const int prefer = u ? u->id : -1;
  if (ai_king_tile_human_cont_fortified_count(ctx, human, x, y, -1) < 1) {
    (void)ai_king_fortify_one_cont_on_tile(ctx, human, x, y, 0, prefer);
  }
  if (ai_king_tile_human_cont_fortified_count(ctx, human, x, y, -1) < 2) {
    (void)ai_king_fortify_one_cont_on_tile(ctx, human, x, y, 1, -1);
  }
}

/* True if name looks like a Man-O-War (Galleon fallback used as REF ship). */
static int ai_king_is_mow(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u || !units_is_sea(units, u->id)) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(units, u->type_index);
  const char* tname = ut ? ut->name : NULL;
  if (tname && (strstr(tname, "Man-O-War") || strstr(tname, "Man-o-War") ||
                strstr(tname, "Galleon"))) {
    return 1;
  }
  return 0;
}

/*
 * Human founding-capital stand-in: lowest active colony id for the nation.
 * Euro colonies have no Col1 capital bit (unlike tribes); cite fandom REF
 * pressure on main ports — first-founded colony as capital. Returns 1 if found.
 */
static int ai_king_human_capital(const ColonizeTurnContext* ctx, int human, int* out_x,
                                 int* out_y) {
  if (!ctx || !ctx->colonies || !out_x || !out_y || human < 0) {
    return 0;
  }
  int best_id = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    if (best_id < 0 || c->id < best_id) {
      best_id = c->id;
      bx = c->x;
      by = c->y;
    }
  }
  if (best_id < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Nearest remaining human colony by Manhattan distance (no capital MD slack).
 * Used after capture: idle hunters not on the fortify-stack garrison slot prefer
 * the next nearest uncaptured colony over closer human land units.
 * Source: fandom REF AI — attack uncaptured colonies / weakest ports; capital
 * MD bias is for peacetime-of-war idle hunt, not post-garrison extras.
 * Returns 1 if a human colony exists.
 */
static int ai_king_nearest_human_colony(const ColonizeTurnContext* ctx, int human, int from_x,
                                        int from_y, int* out_x, int* out_y) {
  if (!ctx || !ctx->colonies || !out_x || !out_y || human < 0) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    const int dist = abs(c->x - from_x) + abs(c->y - from_y);
    if (best < 0 || dist < best) {
      best = dist;
      bx = c->x;
      by = c->y;
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Among colony candidates already chosen as nearest: if founding capital MD is
 * within AI_KING_CAPITAL_MD_SLACK of that nearest colony MD, retarget to capital.
 * Does not invent a closer capital when MD is far worse. Returns 1 if retargeted.
 * Source: fandom REF AI — main-port pressure; idle hunters vs distant colonies.
 */
static int ai_king_prefer_capital_if_comparable(const ColonizeTurnContext* ctx, int human,
                                                int from_x, int from_y, int* bx, int* by,
                                                int best_colony_md) {
  int cx = 0;
  int cy = 0;
  if (!bx || !by || best_colony_md < 0) {
    return 0;
  }
  if (!ai_king_human_capital(ctx, human, &cx, &cy)) {
    return 0;
  }
  if (*bx == cx && *by == cy) {
    return 0; /* already on capital */
  }
  const int cap_md = abs(cx - from_x) + abs(cy - from_y);
  if (cap_md <= best_colony_md + AI_KING_CAPITAL_MD_SLACK) {
    *bx = cx;
    *by = cy;
    return 1;
  }
  return 0;
}

/*
 * Nearest human colony or human land unit (Manhattan) for REF land hunt.
 * Source: fandom REF AI — hunt ports / land; conceptual reuse of ai_euro land hunt
 * (implemented inside ai_king only). Returns 1 if a target was found.
 * prefer_fortified: Artillery thin siege — when set, prefer a fortified human
 * colony (Stockade/Fort/Fortress) if any exist; else fall back to nearest.
 * prefer_open: Dragoon / Cont. Cav thin bias when Artillery type exists — prefer
 * human land units / unfortified colonies (leave fortified ports to Artillery);
 * else fall back to nearest (including fortified). Deep role-split scoring PARKED.
 * Capital MD bias: among colony picks, prefer founding capital when MD is
 * within AI_KING_CAPITAL_MD_SLACK of the nearest other colony (idle hunters).
 * Artillery siege: same slack when the capital itself is fortified (else keep
 * nearest fortified). Strictly closer human land units still win over capital.
 */
static int ai_king_ref_hunt_target(const ColonizeTurnContext* ctx, int human, int from_x,
                                   int from_y, int* out_x, int* out_y,
                                   int prefer_fortified, int prefer_open) {
  if (!ctx || !ctx->units || !out_x || !out_y || human < 0) {
    return 0;
  }

  if (prefer_fortified && ctx->colonies) {
    int best = -1;
    int bx = 0;
    int by = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (!colonies_has_fortification(ctx->colonies, c)) {
        continue;
      }
      const int dist = abs(c->x - from_x) + abs(c->y - from_y);
      if (best < 0 || dist < best) {
        best = dist;
        bx = c->x;
        by = c->y;
      }
    }
    if (best >= 0) {
      /* Capital bias only if capital itself is fortified (Artillery siege set). */
      int cx = 0;
      int cy = 0;
      if (ai_king_human_capital(ctx, human, &cx, &cy)) {
        const int cid = colonies_id_at(ctx->colonies, cx, cy);
        if (cid >= 0) {
          const ColonizeColony* cap = &ctx->colonies->colonies[cid];
          if (colonies_has_fortification(ctx->colonies, cap)) {
            (void)ai_king_prefer_capital_if_comparable(ctx, human, from_x, from_y, &bx, &by,
                                                       best);
          }
        }
      }
      *out_x = bx;
      *out_y = by;
      return 1;
    }
  }

  /*
   * Dragoon / Cont. Cav open-land pass: human land units + unfortified colonies
   * only. Skip fortified colonies so Artillery (prefer_fortified) owns that job.
   */
  if (prefer_open) {
    int best_unit = -1;
    int ux = 0;
    int uy = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* f = &ctx->units->units[i];
      if (!f->active || f->nation_id != human) {
        continue;
      }
      if (!units_is_on_map(f) || units_is_sea(ctx->units, f->id)) {
        continue;
      }
      const int dist = abs(f->x - from_x) + abs(f->y - from_y);
      if (best_unit < 0 || dist < best_unit) {
        best_unit = dist;
        ux = f->x;
        uy = f->y;
      }
    }
    int best_col = -1;
    int bx = 0;
    int by = 0;
    if (ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active || c->nation_id != human) {
          continue;
        }
        if (colonies_has_fortification(ctx->colonies, c)) {
          continue;
        }
        const int dist = abs(c->x - from_x) + abs(c->y - from_y);
        if (best_col < 0 || dist < best_col) {
          best_col = dist;
          bx = c->x;
          by = c->y;
        }
      }
      if (best_col >= 0) {
        int cx = 0;
        int cy = 0;
        if (ai_king_human_capital(ctx, human, &cx, &cy)) {
          const int cid = colonies_id_at(ctx->colonies, cx, cy);
          if (cid >= 0) {
            const ColonizeColony* cap = &ctx->colonies->colonies[cid];
            /* Open-land: capital bias only when capital is unfortified. */
            if (!colonies_has_fortification(ctx->colonies, cap)) {
              (void)ai_king_prefer_capital_if_comparable(ctx, human, from_x, from_y, &bx, &by,
                                                         best_col);
              best_col = abs(bx - from_x) + abs(by - from_y);
            }
          }
        }
      }
    }
    if (best_unit >= 0 || best_col >= 0) {
      /* Unit wins on equal MD (same as prior combined pass); else colony. */
      if (best_unit >= 0 && (best_col < 0 || best_unit <= best_col)) {
        *out_x = ux;
        *out_y = uy;
      } else {
        *out_x = bx;
        *out_y = by;
      }
      return 1;
    }
    /* No open target — fall through to nearest (may be fortified). */
  }

  int best_unit = -1;
  int ux = 0;
  int uy = 0;

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* f = &ctx->units->units[i];
    if (!f->active || f->nation_id != human) {
      continue;
    }
    if (!units_is_on_map(f) || units_is_sea(ctx->units, f->id)) {
      continue;
    }
    const int dist = abs(f->x - from_x) + abs(f->y - from_y);
    if (best_unit < 0 || dist < best_unit) {
      best_unit = dist;
      ux = f->x;
      uy = f->y;
    }
  }

  int best_col = -1;
  int bx = 0;
  int by = 0;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      const int dist = abs(c->x - from_x) + abs(c->y - from_y);
      if (best_col < 0 || dist < best_col) {
        best_col = dist;
        bx = c->x;
        by = c->y;
      }
    }
    if (best_col >= 0) {
      (void)ai_king_prefer_capital_if_comparable(ctx, human, from_x, from_y, &bx, &by,
                                                 best_col);
      best_col = abs(bx - from_x) + abs(by - from_y);
    }
  }

  if (best_unit < 0 && best_col < 0) {
    return 0;
  }
  /* Unit wins on equal MD; capital-biased colony otherwise. */
  if (best_unit >= 0 && (best_col < 0 || best_unit <= best_col)) {
    *out_x = ux;
    *out_y = uy;
  } else {
    *out_x = bx;
    *out_y = by;
  }
  return 1;
}

/* True if any human land unit is adjacent to (x,y). */
static int ai_king_adjacent_human_unit(const ColonizeTurnContext* ctx, int human, int x,
                                       int y) {
  if (!ctx || !ctx->units || human < 0) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int nx = x + dx[d];
    const int ny = y + dy[d];
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* f = &ctx->units->units[i];
      if (!f->active || f->nation_id != human || !units_is_on_map(f)) {
        continue;
      }
      if (units_is_sea(ctx->units, f->id)) {
        continue;
      }
      if (f->x == nx && f->y == ny) {
        return 1;
      }
    }
  }
  return 0;
}

/* True if any active human colony is adjacent to (x,y). */
static int ai_king_adjacent_human_colony(const ColonizeTurnContext* ctx, int human, int x,
                                         int y) {
  if (!ctx || !ctx->colonies || human < 0) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int cid = colonies_id_at(ctx->colonies, x + dx[d], y + dy[d]);
    if (cid < 0) {
      continue;
    }
    const ColonizeColony* c = &ctx->colonies->colonies[cid];
    if (c->active && c->nation_id == human) {
      return 1;
    }
  }
  return 0;
}

/*
 * Water tile adjacent to a human colony (nearest to from_x/from_y).
 * Source: fandom REF AI man-o-war → ports; used for wartime MoW AI_SAIL.
 * When skip_adjacent_colony != 0, prefer a coast for a human colony the ship is
 * *not* already adjacent to (post-full-unload → next human coast). Falls back to
 * any human coast if no other port has water. Returns 1 if found.
 */
static int ai_king_human_coast_water(const ColonizeTurnContext* ctx, int human, int from_x,
                                     int from_y, int* out_x, int* out_y,
                                     int skip_adjacent_colony) {
  if (!ctx || !ctx->map || !ctx->colonies || !out_x || !out_y || human < 0) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int best = -1;
  int bx = 0;
  int by = 0;
  int best_any = -1;
  int bx_any = 0;
  int by_any = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != human) {
      continue;
    }
    /* Adjacent includes 8-neigh + same tile; skip that colony when requested. */
    int adj = 0;
    if (skip_adjacent_colony) {
      if (c->x == from_x && c->y == from_y) {
        adj = 1;
      } else {
        for (int d = 0; d < 8; ++d) {
          if (from_x == c->x + dx[d] && from_y == c->y + dy[d]) {
            adj = 1;
            break;
          }
        }
      }
    }
    for (int d = 0; d < 8; ++d) {
      const int nx = c->x + dx[d];
      const int ny = c->y + dy[d];
      if (!map_tile_is_water(ctx->map, nx, ny)) {
        continue;
      }
      const int dist = abs(nx - from_x) + abs(ny - from_y);
      if (best_any < 0 || dist < best_any) {
        best_any = dist;
        bx_any = nx;
        by_any = ny;
      }
      if (adj) {
        continue; /* just-served port — look for next human coast */
      }
      if (best < 0 || dist < best) {
        best = dist;
        bx = nx;
        by = ny;
      }
    }
  }
  if (best >= 0) {
    *out_x = bx;
    *out_y = by;
    return 1;
  }
  if (best_any < 0) {
    return 0;
  }
  *out_x = bx_any;
  *out_y = by_any;
  return 1;
}

/*
 * Adjacent land for wartime MoW unload near a human colony.
 * Prefer the human colony tile itself; else foundable or coastal land that is
 * adjacent to a human colony. Source: fandom REF man-o-war → ports / seize
 * landing. Returns 1 if a dest was found.
 */
static int ai_king_mow_unload_land_dest(const ColonizeTurnContext* ctx, int human,
                                        const ColonizeUnit* ship, int* out_x, int* out_y) {
  if (!ctx || !ctx->map || !ctx->units || !ship || !out_x || !out_y || human < 0) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int pax_id = -1;
  int pax_type = -1;
  if (ship->cargo_count > 0) {
    pax_id = ship->cargo_ids[0];
    const ColonizeUnit* pax = units_get_const(ctx->units, pax_id);
    if (pax) {
      pax_type = pax->type_index;
    }
  }
  if (pax_type < 0) {
    return 0;
  }

  int best_score = -1;
  int bx = 0;
  int by = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = ship->x + dx[d];
    const int ny = ship->y + dy[d];
    if (!map_tile_is_land(ctx->map, nx, ny)) {
      continue;
    }
    if (!units_can_enter(ctx->units, pax_type, ctx->map, nx, ny, pax_id, ctx->colonies)) {
      continue;
    }
    int score = 0;
    const int cid = ctx->colonies ? colonies_id_at(ctx->colonies, nx, ny) : -1;
    if (cid >= 0) {
      const ColonizeColony* c = &ctx->colonies->colonies[cid];
      if (c->active && c->nation_id == human) {
        score = 100; /* Prefer unload onto the human colony tile. */
      } else {
        continue; /* Other colony tiles are not REF coastal landings. */
      }
    } else {
      /* Foundable or coastal land next to a human colony. */
      const int foundable =
          ctx->colonies && colonies_can_found(ctx->colonies, ctx->map, nx, ny);
      const int coastal = map_tile_is_coastal(ctx->map, nx, ny);
      if (!foundable && !coastal) {
        continue;
      }
      if (!ai_king_adjacent_human_colony(ctx, human, nx, ny)) {
        continue;
      }
      score = foundable ? 50 : 40;
    }
    if (score > best_score) {
      best_score = score;
      bx = nx;
      by = ny;
    }
  }
  if (best_score < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Pick one cargo id to unload: prefer Regular; else Dragoon; else first slot.
 * Returns passenger id or -1.
 */
static int ai_king_mow_pick_unload_pax(const ColonizeTurnContext* ctx,
                                       const ColonizeUnit* ship) {
  if (!ctx || !ctx->units || !ship || ship->cargo_count <= 0) {
    return -1;
  }
  for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[c]);
    if (p && ai_king_is_regular(ctx->units, p)) {
      return ship->cargo_ids[c];
    }
  }
  for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[c]);
    if (p && ai_king_is_dragoon(ctx->units, p)) {
      return ship->cargo_ids[c];
    }
  }
  return ship->cargo_ids[0];
}

/*
 * Wartime MoW adjacent to coast/colony land: unload passengers this beat.
 * Prefer Regular in hold; else Dragoon (REF land arm — fandom Regulars +
 * Cavalry/Dragoons). Reuses units_unload_passenger (Euro landfall path).
 * Multi-slot: dump up to min(moves_left, ship capacity, cargo_count) — real
 * fields only; do not invent passengers. Cite: fandom man-o-war ×6 / seize.
 * Spends one ship move per passenger unloaded so leftover MP can AI_SAIL to
 * the next human coast same beat after a full unload.
 * Returns count unloaded (0 if none). Writes dest via out_x/out_y when non-NULL
 * and n>0 (for same-beat capture/fortify of passengers skipped while aboard).
 */
static int ai_king_mow_try_unload(ColonizeTurnContext* ctx, ColonizeUnit* ship,
                                 int human, int* out_x, int* out_y) {
  if (!ctx || !ctx->units || !ship || ship->cargo_count <= 0) {
    return 0;
  }
  if (ship->moves_left <= 0) {
    return 0;
  }
  int dest_x = 0;
  int dest_y = 0;
  if (!ai_king_mow_unload_land_dest(ctx, human, ship, &dest_x, &dest_y)) {
    return 0;
  }
  int cap = units_ship_capacity(ctx->units, ship->id);
  if (cap <= 0) {
    cap = COLONIZE_UNIT_CARGO_MAX;
  }
  int budget = ship->moves_left;
  if (budget > cap) {
    budget = cap;
  }
  if (budget > ship->cargo_count) {
    budget = ship->cargo_count;
  }
  int n = 0;
  while (n < budget && ship->cargo_count > 0 && ship->moves_left > 0) {
    const int pax_id = ai_king_mow_pick_unload_pax(ctx, ship);
    if (pax_id < 0) {
      break;
    }
    if (!units_unload_passenger(ctx->units, ship->id, pax_id, ctx->map, dest_x, dest_y,
                                ctx->colonies)) {
      break;
    }
    ship->moves_left--;
    n++;
  }
  if (n > 0) {
    if (out_x) {
      *out_x = dest_x;
    }
    if (out_y) {
      *out_y = dest_y;
    }
  }
  return n;
}

/*
 * Fortify one idle garrison unit on (x,y). require_moves: second slot needs MP.
 * Prefer Regular over Dragoon/Cont. Cav; prefer prefer_id when eligible.
 * Returns unit id fortified, or -1.
 */
static int ai_king_fortify_one_garrison_on_tile(ColonizeTurnContext* ctx, int crown, int x, int y,
                                                int require_moves, int prefer_id) {
  if (!ctx || !ctx->units || crown < 0) {
    return -1;
  }
  ColonizeUnitPool* pool = ctx->units;

  if (prefer_id >= 0) {
    ColonizeUnit* p = units_get(pool, prefer_id);
    if (p && p->active && p->nation_id == crown && p->x == x && p->y == y &&
        ai_king_is_regular(pool, p) && p->orders != UNITS_ORDER_FORTIFY &&
        p->orders != UNITS_ORDER_FORTIFIED && (!require_moves || p->moves_left > 0)) {
      (void)units_order_fortify(pool, p->id);
      return p->id;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != crown || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_regular(pool, u) || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    if (require_moves && u->moves_left <= 0) {
      continue;
    }
    (void)units_order_fortify(pool, u->id);
    return u->id;
  }
  if (prefer_id >= 0) {
    ColonizeUnit* p = units_get(pool, prefer_id);
    if (p && p->active && p->nation_id == crown && p->x == x && p->y == y &&
        ai_king_is_garrison_cavalry(pool, p) && p->orders != UNITS_ORDER_FORTIFY &&
        p->orders != UNITS_ORDER_FORTIFIED && (!require_moves || p->moves_left > 0)) {
      (void)units_order_fortify(pool, p->id);
      return p->id;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != crown || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_garrison_cavalry(pool, u) || u->orders == UNITS_ORDER_FORTIFY ||
        u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    if (require_moves && u->moves_left <= 0) {
      continue;
    }
    (void)units_order_fortify(pool, u->id);
    return u->id;
  }
  return -1;
}

/*
 * After capture / idle on crown colony: fortify up to two garrison units
 * (UNITS_ORDER_FORTIFY). First slot: Regular prefer, else Dragoon/Cont. Cav
 * (capture may use 0 MP). Second slot: another idle garrison on tile with
 * moves_left > 0. Extras beyond two hunt (idle-garrison gate).
 * Cite: Colonization.pdf Defending a Colony ("fortify soldiers, dragoons…");
 * king_ref thin multi-garrison (cap 2). Deep multi-step siege PARKED.
 */
static void ai_king_fortify_garrison_at(ColonizeTurnContext* ctx, ColonizeUnit* capturer,
                                        int crown, int x, int y) {
  if (!ctx || !ctx->units || crown < 0) {
    return;
  }
  if (ai_king_tile_fortified_garrison_count(ctx, crown, x, y, -1) >= 2) {
    return;
  }
  const int prefer = capturer ? capturer->id : -1;
  if (ai_king_tile_fortified_garrison_count(ctx, crown, x, y, -1) < 1) {
    (void)ai_king_fortify_one_garrison_on_tile(ctx, crown, x, y, 0, prefer);
  }
  if (ai_king_tile_fortified_garrison_count(ctx, crown, x, y, -1) < 2) {
    (void)ai_king_fortify_one_garrison_on_tile(ctx, crown, x, y, 1, -1);
  }
}

/*
 * After capture / idle on own colony: fortify crown Artillery on the tile
 * (UNITS_ORDER_FORTIFY). Euro pattern — case 0x0b fortify arm; Colonization.pdf
 * fortify defense / Artillery siege; euro_unit_act Artillery fortify after siege.
 * Unlike Regular stack (one garrison), each idle Artillery on the colony holds.
 * Artillery elsewhere still hunts fortified ports (thin siege bias).
 */
static void ai_king_fortify_artillery_at(ColonizeTurnContext* ctx, ColonizeUnit* capturer,
                                         int crown, int x, int y) {
  if (!ctx || !ctx->units || crown < 0) {
    return;
  }
  if (capturer && capturer->active && capturer->nation_id == crown &&
      ai_king_is_artillery(ctx->units, capturer) && capturer->x == x &&
      capturer->y == y) {
    if (capturer->orders != UNITS_ORDER_FORTIFY &&
        capturer->orders != UNITS_ORDER_FORTIFIED) {
      (void)units_order_fortify(ctx->units, capturer->id);
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown || u->x != x || u->y != y) {
      continue;
    }
    if (!ai_king_is_artillery(ctx->units, u)) {
      continue;
    }
    if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
      continue;
    }
    (void)units_order_fortify(ctx->units, u->id);
  }
}

/* If REF stands on a human colony tile, capture (conquest — colonies_capture). */
static void ai_king_try_capture_at(ColonizeTurnContext* ctx, ColonizeUnit* u, int crown,
                                   int human) {
  if (!ctx || !u || !u->active || !ctx->colonies) {
    return;
  }
  const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
  if (cid < 0) {
    return;
  }
  ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
  if (c && c->nation_id == human) {
    /* Source: conquest / colonies_capture — Euro owner swap; no gold fiction. */
    char cname[COLONIZE_COLONY_NAME_MAX];
    snprintf(cname, sizeof(cname), "%s", c->name[0] ? c->name : "your colony");
    if (colonies_capture(ctx->colonies, cid, crown)) {
      /*
       * Thin human status on REF capture (full conquest chrome PARKED).
       * Do not clobber same-beat 2244 merc hire status; may replace 1528 arrival.
       * ai_popup OK (AI_POPUP_TAG_KING_CAPTURE) when queue attached.
       */
      if (ctx->status && ctx->status_size &&
          !(strstr(ctx->status, "Mercenaries join") ||
            strstr(ctx->status, "Continental cause") ||
            strstr(ctx->status, "Cannot afford mercenaries"))) {
        snprintf(ctx->status, ctx->status_size, "The King's forces have captured %s!",
                 cname);
      }
      if (ai_king_human_popups(ctx)) {
        char body[AI_POPUP_BODY_LEN];
        snprintf(body, sizeof(body), "The King's forces have captured %s!", cname);
        (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_CAPTURE, human,
                                      crown, cid, "Colony Captured", body);
      }
      ai_king_fortify_garrison_at(ctx, u, crown, u->x, u->y);
      /* Euro pattern: idle Artillery on newly captured colony → FORTIFY. */
      ai_king_fortify_artillery_at(ctx, u, crown, u->x, u->y);
    }
  }
}

/*
 * Same-beat seize/fortify for passengers just put ashore (unit-index order may
 * have skipped them while aboard with moves_left==0). Cite: fandom REF seize
 * landing + fortify one Regular (else Dragoon/Cont.Cav) after capture / multi-unload.
 */
static void ai_king_mow_post_unload_land(ColonizeTurnContext* ctx, int crown, int human,
                                         int dest_x, int dest_y) {
  if (!ctx || !ctx->units || crown < 0) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown || u->x != dest_x || u->y != dest_y) {
      continue;
    }
    if (units_is_sea(ctx->units, u->id) || u->aboard_ship_id >= 0) {
      continue;
    }
    ai_king_try_capture_at(ctx, u, crown, human);
  }
}

static int ai_king_force_total(const uint16_t force[4]) {
  if (!force) {
    return 0;
  }
  return (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];
}

/*
 * Spawn one land unit near (hx,hy) for nation_id — shared by 06a6 / 10f0.
 * Returns unit id or -1.
 */
static int ai_king_spawn_landing(ColonizeTurnContext* ctx, int nation_id, int hx, int hy,
                                 const char* type_name, const char* alt_name) {
  if (!ctx || !ctx->units || nation_id < 0) {
    return -1;
  }
  int ty = units_find_type(ctx->units, type_name);
  if (ty < 0 && alt_name) {
    ty = units_find_type(ctx->units, alt_name);
  }
  if (ty < 0) {
    return -1;
  }
  const int uid = units_spawn_allow_stack(ctx->units, ty, hx, hy + 1);
  if (uid < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (u) {
    units_set_nation(u, nation_id);
    u->orders = UNITS_ORDER_AI_MOVE;
    u->goto_x = hx;
    u->goto_y = hy;
  }
  return uid;
}

static void ai_king_set_ref_present(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  col1->head.unknown46[AI_KING_REF_PRESENT_BYTE] = on ? 1 : 0;
}

static int ai_king_boycott_active(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  return col1->head.unknown46[AI_KING_BOYCOTT_BYTE] != 0;
}

static void ai_king_set_boycott(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  col1->head.unknown46[AI_KING_BOYCOTT_BYTE] = on ? 1 : 0;
}

/*
 * Sync tax-refuse stand-in when cargo boycotts were cleared externally
 * (Jakob Fugger / diplo peace lift — do not touch FF here).
 * Source: fandom Jakob Fugger “all boycotts forgiven”; king_ref refuse +
 * nation.boycott_bitmap. When bitmap==0, clear unknown46[2] so tax may resume.
 */
static void ai_king_sync_boycott_refuse(ColonizeCol1Save* col1, int human) {
  if (!col1 || human < 0 || human >= 4) {
    return;
  }
  if (col1->head.unknown46[AI_KING_BOYCOTT_BYTE] == 0) {
    return;
  }
  if (col1->nation[human].boycott_bitmap == 0) {
    col1->head.unknown46[AI_KING_BOYCOTT_BYTE] = 0;
  }
}

static int ai_king_merc_hired(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  return col1->head.unknown46[AI_KING_MERC_HIRED_BYTE] != 0;
}

static void ai_king_set_merc_hired(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  col1->head.unknown46[AI_KING_MERC_HIRED_BYTE] = on ? 1 : 0;
}

/* Grow REF pools by current tax band (1d42 crumb; no tax_rate change). */
static void ai_king_grow_ref_from_tax(ColonizeCol1Save* col1, uint8_t tax_rate) {
  if (!col1) {
    return;
  }
  col1->head.expeditionary_force[0] += 1; /* regulars */
  if (tax_rate >= 10) {
    col1->head.expeditionary_force[1] += 1; /* dragoons */
  }
  if (tax_rate >= 20) {
    col1->head.expeditionary_force[2] += (tax_rate % 5 == 0) ? 1 : 0; /* MoW */
  }
  if (tax_rate >= 30 && (tax_rate % 10 == 0)) {
    col1->head.expeditionary_force[3] += 1; /* artillery */
  }
  if (col1->head.expeditionary_force[0] > 0) {
    ai_king_set_ref_present(col1, 1);
  }
}

int ai_king_sol_percent(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  /* FUN_43f7_0004: prefer Col1 rebel_dividend/divisor; else liberty bells. */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    uint64_t div_sum = 0;
    uint64_t num_sum = 0;
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != nation_id) {
        continue;
      }
      num_sum += (uint64_t)c->rebel_dividend * (uint64_t)(c->population > 0 ? c->population : 1);
      div_sum += (uint64_t)(c->rebel_divisor > 0 ? c->rebel_divisor : 1) *
                 (uint64_t)(c->population > 0 ? c->population : 1);
    }
    if (div_sum > 0) {
      return (int)((num_sum * 100ull) / div_sum);
    }
  }
  if (ctx->col1_ok && ctx->col1) {
    const ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
    const int bells = (int)nat->liberty_bells_total;
    int sol = bells / 4;
    if (sol > 100) {
      sol = 100;
    }
    return sol;
  }
  return 0;
}

/*
 * FUN_43f7_1eca colony-SoL bias (catalog: promote when colony SoL>50%).
 * Prefer Col1 rebel_dividend/divisor at the unit tile; else nation SoL (0004).
 * King promote path only — not FF Washington mass-promote / combat upgrade.
 */
static int ai_king_colony_sol_at(const ColonizeTurnContext* ctx, int nation_id, int x, int y) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  if (ctx->col1_ok && ctx->col1 && ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != nation_id) {
        continue;
      }
      if ((int)c->x != x || (int)c->y != y) {
        continue;
      }
      const uint32_t div = c->rebel_divisor > 0 ? c->rebel_divisor : 1;
      return (int)(((uint64_t)c->rebel_dividend * 100ull) / (uint64_t)div);
    }
  }
  return ai_king_sol_percent(ctx, nation_id);
}

/*
 * Linux WoI stand-in for DOS 0x5382 bit0.
 * Set only on declare (ai_king_try_declare / ai_king_set_independence) when
 * SoL≥AI_KING_DECLARE_SOL_MIN — never by restless chrome. Exact Col1 bit PARKED.
 */
static int ai_king_independence_declared(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 0;
  }
  return col1->head.unknown46[AI_KING_WOI_BYTE] != 0;
}

static void ai_king_set_independence(ColonizeCol1Save* col1, int on) {
  if (!col1) {
    return;
  }
  /* unknown46[0] = WoI flag (set once on declare; idempotent if already set). */
  col1->head.unknown46[AI_KING_WOI_BYTE] = on ? 1 : 0;
  if (on) {
    col1->head.event.colony_burning = 1; /* chrome hint */
  }
}

/*
 * FUN_43f7_1d42 / 38fd_5be8 Accept path: hike tax + sync Europe + grow REF.
 * Used by auto peacetime path and ai_king_apply_popup_result (Accept).
 */
static void ai_king_tax_accept_hike(ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  if (nat->tax_rate >= 75) {
    return;
  }
  nat->tax_rate = (uint8_t)(nat->tax_rate + 1);
  if (ctx->europe) {
    ctx->europe->tax_percent = nat->tax_rate;
  }
  ai_king_grow_ref_from_tax(ctx->col1, nat->tax_rate);
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "The King raises taxes to %u%%.", nat->tax_rate);
  }
  if (ai_king_human_popups(ctx)) {
    char body[AI_POPUP_BODY_LEN];
    snprintf(body, sizeof(body), "The King raises taxes to %u%%.", nat->tax_rate);
    (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human,
                                  ai_king_crown_nation(human), (int)nat->tax_rate,
                                  "Royal Tax", body);
  }
}

/*
 * FUN_43f7_38fd_5be8 Refuse path: boycott stand-in + Sugar freeze + REF grow,
 * tax unchanged. Used by auto refuse and ai_king_apply_popup_result (Refuse).
 * Human queue: dump-goods CHOICE when eligible cargos remain, else follow-up
 * OK after Refuse (lists boycott_bitmap cargos).
 */
static void ai_king_tax_refuse_hike(ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  ai_king_set_boycott(ctx->col1, 1);
  nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap | AI_KING_BOYCOTT_CARGO_BIT);
  /*
   * Dump-goods second cargo (FUN_38fd_3dc8): pick among cargos not already
   * boycotted. Sugar already set above. When ctx->europe present, candidate
   * mask = cargos with live bid > 0. Human popups → CHOICE modal (tag
   * KING_DUMP_GOODS); else RNG via ai_king_pick_dump_goods_cargo.
   * Cite: FUN_38fd_3dc8 / wiki Boycott “named goods” / king_ref dump-goods.
   */
  const uint16_t all_cargos = (uint16_t)((1u << COLONIZE_CARGO_COUNT) - 1u);
  uint16_t candidate_mask = all_cargos;
  const int* bids = NULL;
  int bid_buf[COLONIZE_CARGO_COUNT];
  if (ctx->europe) {
    const EuropeScreen* eu = ctx->europe;
    candidate_mask = 0;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      const int bid = (c < eu->cargo_count) ? eu->cargo[c].bid : 0;
      bid_buf[c] = bid;
      if (bid > 0) {
        candidate_mask = (uint16_t)(candidate_mask | (uint16_t)(1u << c));
      }
    }
    bids = bid_buf;
  }
  const uint16_t eligible =
    (uint16_t)(candidate_mask & (uint16_t)~nat->boycott_bitmap);
  int dump_choice_enqueued = 0;
  if (ai_king_human_popups(ctx) && eligible != 0) {
    /* Build up to AI_POPUP_CHOICE_MAX labels (prefer highest bid when known). */
    int idxs[COLONIZE_CARGO_COUNT];
    int n = 0;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      if ((eligible & (uint16_t)(1u << c)) == 0) {
        continue;
      }
      if (bids && bids[c] <= 0) {
        continue;
      }
      idxs[n++] = c;
    }
    if (bids && n > 1) {
      for (int a = 0; a < n - 1; ++a) {
        for (int b = a + 1; b < n; ++b) {
          if (bids[idxs[b]] > bids[idxs[a]]) {
            const int tmp = idxs[a];
            idxs[a] = idxs[b];
            idxs[b] = tmp;
          }
        }
      }
    }
    if (n > AI_POPUP_CHOICE_MAX) {
      n = AI_POPUP_CHOICE_MAX;
    }
    if (n > 0) {
      char labels[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
      int ids[AI_POPUP_CHOICE_MAX];
      const char* label_ptrs[AI_POPUP_CHOICE_MAX];
      for (int i = 0; i < n; ++i) {
        const char* nm = ai_king_cargo_name(idxs[i]);
        snprintf(labels[i], sizeof(labels[i]), "%s", nm ? nm : "Cargo");
        ids[i] = idxs[i];
        label_ptrs[i] = labels[i];
      }
      char body[AI_POPUP_BODY_LEN];
      snprintf(
        body,
        sizeof(body),
        "The colonies refuse the tax (stays at %u%%). Sugar is boycotted. "
        "Name another good to dump from Europe trade:",
        nat->tax_rate
      );
      dump_choice_enqueued = ai_popup_enqueue_choice_ctx(
                               ctx->ai_popups,
                               AI_POPUP_TAG_KING_DUMP_GOODS,
                               human,
                               ai_king_crown_nation(human),
                               (int)nat->tax_rate,
                               "Dump Goods",
                               body,
                               label_ptrs,
                               ids,
                               n
                             )
                               ? 1
                               : 0;
    }
  }
  if (!dump_choice_enqueued && ctx->rng) {
    const int picked =
      ai_king_pick_dump_goods_cargo(nat->boycott_bitmap, candidate_mask, ctx->rng, bids);
    if (picked >= 0 && picked < COLONIZE_CARGO_COUNT) {
      nat->boycott_bitmap =
        (uint16_t)(nat->boycott_bitmap | (uint16_t)(1u << picked));
    }
  }
  ai_king_grow_ref_from_tax(ctx->col1, nat->tax_rate);
  if (ctx->status && ctx->status_size) {
    char cargos[96];
    if (ai_king_format_boycott_cargos(cargos, sizeof(cargos), nat->boycott_bitmap)) {
      snprintf(
        ctx->status,
        ctx->status_size,
        dump_choice_enqueued
          ? "Audience: refuse tax (stays %u%%). Boycotted so far: %s. Choose dump goods."
          : "Audience: the colonies refuse the tax increase! Tax stays at %u%%. "
            "Boycotted in Europe: %s.",
        nat->tax_rate,
        cargos
      );
    } else {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Audience: the colonies refuse the tax increase! Tax stays at %u%%.",
        nat->tax_rate
      );
    }
  }
  /* Follow-up OK when no dump CHOICE pending (auto / no eligible / queue full). */
  if (!dump_choice_enqueued && ai_king_human_popups(ctx)) {
    char body[AI_POPUP_BODY_LEN];
    char cargos[96];
    if (ai_king_format_boycott_cargos(cargos, sizeof(cargos), nat->boycott_bitmap)) {
      snprintf(
        body,
        sizeof(body),
        "The colonies refuse the tax increase (stays at %u%%). "
        "Boycotted in Europe: %s.",
        nat->tax_rate,
        cargos
      );
    } else {
      snprintf(
        body,
        sizeof(body),
        "The colonies refuse the tax increase (stays at %u%%). "
        "Sugar is boycotted in Europe.",
        nat->tax_rate
      );
    }
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups,
      AI_POPUP_TAG_KING_TAX,
      human,
      ai_king_crown_nation(human),
      (int)nat->tax_rate,
      "Royal Audience",
      body
    );
  }
}

static void ai_king_apply_dump_goods_choice(ColonizeTurnContext* ctx, int human, int cargo) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (cargo < 0 || cargo >= COLONIZE_CARGO_COUNT) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  nat->boycott_bitmap = (uint16_t)(nat->boycott_bitmap | (uint16_t)(1u << cargo));
  if (ctx->status && ctx->status_size) {
    char cargos[96];
    if (ai_king_format_boycott_cargos(cargos, sizeof(cargos), nat->boycott_bitmap)) {
      snprintf(
        ctx->status,
        ctx->status_size,
        "Dump goods: boycotted in Europe: %s.",
        cargos
      );
    }
  }
  if (ai_king_human_popups(ctx)) {
    char body[AI_POPUP_BODY_LEN];
    char cargos[96];
    if (ai_king_format_boycott_cargos(cargos, sizeof(cargos), nat->boycott_bitmap)) {
      snprintf(
        body,
        sizeof(body),
        "The colonies refuse the tax increase (stays at %u%%). "
        "Boycotted in Europe: %s.",
        nat->tax_rate,
        cargos
      );
    } else {
      snprintf(
        body,
        sizeof(body),
        "The colonies refuse the tax increase (stays at %u%%).",
        nat->tax_rate
      );
    }
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups,
      AI_POPUP_TAG_KING_TAX,
      human,
      ai_king_crown_nation(human),
      (int)nat->tax_rate,
      "Royal Audience",
      body
    );
  }
}

/*
 * FUN_43f7_1d42 checklist:
 *  spring-only; first year / interval by difficulty; cap 75%;
 *  sync europe tax; grow REF pools by tax band.
 * Structural boycott/refuse + thin 38fd_5be8 audience:
 *  Human + ctx->ai_popups → CHOICE Accept/Refuse (effect in apply_popup_result).
 *  Else auto: when tax_rate >= 20 and (SoL >= 30 or liberty bells high), refuse
 *  hike once; else Accept hike. While boycott active, skip further tax hikes
 *  (hold-audience status + OK popup when queue attached).
 * Note: TURN_PROC_FINISH may overwrite ctx->status afterward.
 */
static void ai_king_tax_event(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  /* Fugger / external bitmap clear → drop refuse so tax may resume. */
  ai_king_sync_boycott_refuse(ctx->col1, human);
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  const int year = ctx->game_year ? (int)*ctx->game_year : 1492;
  const int diff = ctx->col1->head.difficulty;
  const int first = 1536 - diff;
  const int interval = 22 - diff * 2;
  if (year < first) {
    return;
  }
  if (((year - first) % (interval > 4 ? interval : 4)) != 0) {
    return;
  }
  if (ctx->game_autumn && *ctx->game_autumn != 0) {
    return; /* spring tax audiences */
  }

  /* Already refused: no further tax hikes (REF grow-without-hike was once). */
  if (ai_king_boycott_active(ctx->col1)) {
    if (ctx->status && ctx->status_size) {
      char cargos[96];
      if (ai_king_format_boycott_cargos(cargos, sizeof(cargos), nat->boycott_bitmap)) {
        snprintf(ctx->status, ctx->status_size,
                 "Audience: boycott holds — the King cannot raise taxes. "
                 "Boycotted: %s.",
                 cargos);
      } else {
        snprintf(ctx->status, ctx->status_size,
                 "Audience: boycott holds — the King cannot raise taxes.");
      }
    }
    if (ai_king_human_popups(ctx)) {
      char body[AI_POPUP_BODY_LEN];
      char cargos[96];
      if (ai_king_format_boycott_cargos(cargos, sizeof(cargos), nat->boycott_bitmap)) {
        snprintf(body, sizeof(body),
                 "Boycott holds — the King cannot raise taxes. Boycotted: %s.",
                 cargos);
      } else {
        snprintf(body, sizeof(body), "Boycott holds — the King cannot raise taxes.");
      }
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_TAX, human, ai_king_crown_nation(human),
        (int)nat->tax_rate, "Royal Audience", body
      );
    }
    return;
  }

  if (nat->tax_rate >= 75) {
    return;
  }

  /*
   * Human map popup: defer hike/refuse to ai_king_apply_popup_result
   * (FUN_43f7_38fd_5be8 audience CHOICE). payload = current tax_rate.
   */
  if (ai_king_human_popups(ctx)) {
    const char* labels[] = {"Accept", "Refuse"};
    const int ids[] = {AI_KING_CHOICE_ACCEPT, AI_KING_CHOICE_REFUSE};
    char body[AI_POPUP_BODY_LEN];
    snprintf(body, sizeof(body),
             "The King demands taxes rise from %u%% to %u%%. Accept or refuse?",
             nat->tax_rate, (unsigned)(nat->tax_rate + 1u));
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_AUDIENCE, human,
                                    ai_king_crown_nation(human), (int)nat->tax_rate,
                                    "Royal Audience", body, labels, ids, 2)) {
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size,
                 "Audience: the King demands a tax increase to %u%%.",
                 (unsigned)(nat->tax_rate + 1u));
      }
      return;
    }
    /* Queue full — fall through to auto resolve. */
  }

  const int sol = ai_king_sol_percent(ctx, human);
  const int refuse =
      (nat->tax_rate >= AI_KING_BOYCOTT_TAX_MIN) &&
      (sol >= AI_KING_BOYCOTT_SOL_MIN || nat->liberty_bells_total >= AI_KING_BOYCOTT_BELLS_MIN);
  if (refuse) {
    ai_king_tax_refuse_hike(ctx, human);
    return;
  }

  ai_king_tax_accept_hike(ctx, human);
}

/*
 * FUN_43f7_1a26 declare body (after 2564 confirm / auto).
 * Seeds REF by difficulty; thin backup_force as 10f0 foreign-pool stand-in;
 * withdraws other Euros; thin 160a rename; unknown46[5] congress.
 */
static void ai_king_do_declare(ColonizeTurnContext* ctx, int human) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  ai_king_set_independence(ctx->col1, 1); /* WoI: unknown46[0] if not already */
  /* FUN_43f7_2564 congress-confirm stand-in. */
  ctx->col1->head.unknown46[AI_KING_CONGRESS_BYTE] = 1;
  const int diff = ctx->col1->head.difficulty;
  ctx->col1->head.expeditionary_force[0] = (uint16_t)(8 + diff * 4);
  ctx->col1->head.expeditionary_force[1] = (uint16_t)(4 + diff * 2);
  ctx->col1->head.expeditionary_force[2] = (uint16_t)(2 + diff);
  ctx->col1->head.expeditionary_force[3] = (uint16_t)(2 + diff);
  /* backup_force: DOS 0x53e2… foreign-intervention pools — 10f0 stand-in. */
  ctx->col1->head.backup_force[0] = (uint16_t)(2 + diff);
  ctx->col1->head.backup_force[1] = (uint16_t)(1 + (diff > 0 ? 1 : 0));
  ctx->col1->head.backup_force[2] = (uint16_t)(diff > 1 ? 1 : 0);
  ctx->col1->head.backup_force[3] = 1;
  ai_king_set_ref_present(ctx->col1, 1);
  /* 0218-shaped: fold other Euro AI as withdrawn. */
  for (int n = 0; n < 4; ++n) {
    if (n == human) {
      continue;
    }
    ctx->col1->player[n].control = 2;
  }
  /*
   * Thin 160a independence rename stand-in (letter-animation cinematic PARKED).
   * Writable Col1 player.country_name (and europe.nation_name if present).
   * Congress status below; same-turn 0982/1528 wave may overwrite if it spawns
   * (wave only writes status when non-empty arrival — leave congress if empty).
   * Human queue: thin rename OK + WoI-begins OK (FUN_43f7_160a / 1a26 chain).
   * Letter chrome: KING_LETTER body uses DECLARAT-shaped wording (full
   * DECLARAT.PIK / FONTKING letter-anim still PARKED).
   */
  snprintf(ctx->col1->player[human].country_name,
           sizeof(ctx->col1->player[human].country_name), "%s", AI_KING_INDEP_COUNTRY);
  if (ctx->europe) {
    snprintf(ctx->europe->nation_name, sizeof(ctx->europe->nation_name), "%s",
             AI_KING_INDEP_COUNTRY);
  }
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "Congress declares independence!");
  }
  if (ai_king_human_popups(ctx)) {
    /* FUN_43f7_160a rename OK (letter-anim cinematic PARKED — KING_LETTER tag). */
    char letter[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      "INDEPENDENCE",
      NULL,
      "Continental Congress signs Declaration of Independence! "
      "Abuses and usurpations cited! The colonies are renamed the United Colonies.",
      letter,
      sizeof(letter)
    );
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups,
      AI_POPUP_TAG_KING_LETTER,
      human,
      ai_king_crown_nation(human),
      0,
      "Declaration of Independence",
      letter
    );
    /* FUN_43f7_1a26 / 2564: WoI begins OK after Confirm/auto declare. */
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_INFO, human, ai_king_crown_nation(human), 1,
      "War of Independence", "War of Independence begins!"
    );
  }
}

/*
 * FUN_43f7_2564 gate (SoL≥AI_KING_DECLARE_SOL_MIN) + 1a26 declare.
 * Human + ctx->ai_popups → CHOICE Confirm / Not yet (effect in apply_popup_result).
 * Else auto-declare when SoL past 2564/fandom threshold and bells ≥ min.
 */
static void ai_king_try_declare(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  if (ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int sol = ai_king_sol_percent(ctx, human);
  if (sol < AI_KING_DECLARE_SOL_MIN) {
    return;
  }
  const ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  if (nat->liberty_bells_total < AI_KING_DECLARE_BELLS_MIN) {
    return;
  }
  if (ai_king_human_popups(ctx)) {
    const char* labels[] = {"Confirm independence", "Not yet"};
    const int ids[] = {AI_KING_CHOICE_CONFIRM, AI_KING_CHOICE_NOT_YET};
    char body[AI_POPUP_BODY_LEN];
    snprintf(body, sizeof(body),
             "Continental Congress: Sons of Liberty at %d%%. Declare independence?", sol);
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_CONGRESS, human,
                                    ai_king_crown_nation(human), sol, "Continental Congress",
                                    body, labels, ids, 2)) {
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size,
                 "Congress debates independence (SoL %d%%).", sol);
      }
      return;
    }
    /* Queue full — fall through to auto declare. */
  }
  ai_king_do_declare(ctx, human);
}

/* FUN_43f7_060a-shaped: weakest garrison (pop × fort). */
static int ai_king_weakest_port(ColonizeTurnContext* ctx, int nation_id, int* out_x, int* out_y) {
  if (!ctx || !ctx->colonies || !out_x || !out_y) {
    return -1;
  }
  int best = -1;
  int best_score = 999999;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    int garrison = c->population;
    if (colonies_has_fortification(ctx->colonies, c)) {
      garrison *= 2;
    }
    if (garrison < best_score) {
      best_score = garrison;
      best = c->id;
      *out_x = c->x;
      *out_y = c->y;
    }
  }
  return best;
}

/*
 * Spawn one crown land unit on colony tile (0982 wave / empty-hold fallback).
 * Returns 1 on success, else 0.
 */
static int ai_king_spawn_wave_land(ColonizeTurnContext* ctx, int nation_id, int x, int y,
                                   const char* type_name, const char* alt_name) {
  if (!ctx || !ctx->units || nation_id < 0) {
    return 0;
  }
  int lty = units_find_type(ctx->units, type_name);
  if (lty < 0 && alt_name) {
    lty = units_find_type(ctx->units, alt_name);
  }
  if (lty < 0) {
    return 0;
  }
  const int uid = units_spawn_allow_stack(ctx->units, lty, x, y);
  if (uid < 0) {
    return 0;
  }
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (u) {
    units_set_nation(u, nation_id);
    u->orders = UNITS_ORDER_AI_MOVE;
    u->goto_x = x;
    u->goto_y = y;
  }
  return 1;
}

/*
 * Prefer boarding a REF land unit into MoW cargo (units_board_stacked; same-tile
 * spawn like euro Europe hire). On board failure, place on colony tile.
 * Cite: docs/fandom_col1994.md REF “man-o-war with 6 units”; units_board.
 * Returns 1 on spawn (boarded or colony fallback), else 0.
 */
static int ai_king_mow_embark_land(ColonizeTurnContext* ctx, int nation_id, int ship_id,
                                   int land_x, int land_y, const char* type_name,
                                   const char* alt_name) {
  if (!ctx || !ctx->units || nation_id < 0) {
    return 0;
  }
  ColonizeUnit* ship = (ship_id >= 0) ? units_get(ctx->units, ship_id) : NULL;
  const int cap = (ship_id >= 0) ? units_ship_capacity(ctx->units, ship_id) : 0;
  if (ship && cap > 0 && ship->cargo_count < cap) {
    int lty = units_find_type(ctx->units, type_name);
    if (lty < 0 && alt_name) {
      lty = units_find_type(ctx->units, alt_name);
    }
    if (lty >= 0) {
      const int uid = units_spawn_allow_stack(ctx->units, lty, ship->x, ship->y);
      if (uid >= 0) {
        ColonizeUnit* u = units_get(ctx->units, uid);
        if (u) {
          units_set_nation(u, nation_id);
        }
        if (units_board_stacked(ctx->units, uid, ship_id)) {
          return 1;
        }
        /* Board failed — do not leave a land unit on water. */
        if (u) {
          u->x = land_x;
          u->y = land_y;
          u->orders = UNITS_ORDER_AI_MOVE;
          u->goto_x = land_x;
          u->goto_y = land_y;
          u->aboard_ship_id = -1;
        }
        return 1;
      }
    }
  }
  return ai_king_spawn_wave_land(ctx, nation_id, land_x, land_y, type_name, alt_name);
}

/*
 * Spawn one land type from REF pools at (x,y). When target_fortified and
 * Artillery type exists with force[3]>0, prefer Artillery (thin siege spawn).
 * Returns 1 on success.
 */
static int ai_king_spawn_wave_land_from_pools(ColonizeTurnContext* ctx, int nation_id, int x,
                                              int y, uint16_t* force, int target_fortified) {
  static const char* names[4] = {"Regular", "Dragoon", "Man-O-War", "Artillery"};
  if (!force) {
    return 0;
  }
  /* Thin Artillery siege spawn bias (fandom REF includes Artillery; type gate). */
  if (target_fortified && force[3] > 0 && ai_king_artillery_type(ctx->units) >= 0) {
    if (ai_king_spawn_wave_land(ctx, nation_id, x, y, "Artillery", "Cannon")) {
      force[3]--;
      return 1;
    }
  }
  for (int k = 0; k < 4; ++k) {
    if (k == 2 || force[k] == 0) {
      continue;
    }
    const char* alt = (k == 0) ? "Soldier" : ((k == 1) ? "Scout" : ((k == 3) ? "Cannon" : NULL));
    if (!ai_king_spawn_wave_land(ctx, nation_id, x, y, names[k], alt)) {
      continue;
    }
    force[k]--;
    return 1;
  }
  return 0;
}

/*
 * FUN_43f7_0982 (pools>0) / 06a6 (empty): REF wave arms.
 * Thin 1528: status arrival line when 0982 spawns (chrome UI PARKED).
 * MoW cargo: when force[2] drained, board up to units_ship_capacity land
 * units into the MoW (Regulars force[0] first, then Dragoons force[1]) via
 * units_board_stacked / cargo_ids. Coastal multi-unload (≤moves/capacity)
 * remains in war_act. Cite: fandom REF “man-o-war with 6 units”;
 * COLONIZE_UNIT_CARGO_MAX.
 * Second MoW: when difficulty ≥ AI_KING_SECOND_MOW_DIFF and force[2] still
 * allows, spawn a second Man-O-War stand-in same beat (existing 0982 path).
 * Thin Artillery siege: when target colony is fortified and Artillery type
 * exists, prefer force[3] Artillery for the non-MoW land spawn / empty-hold
 * guarantee (deep siege scoring PARKED).
 */
static void ai_king_ref_wave(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->map) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int crown = ai_king_crown_nation(ctx->human_nation);
  uint16_t* force = ctx->col1->head.expeditionary_force;
  const int total = (int)force[0] + (int)force[1] + (int)force[2] + (int)force[3];

  if (total <= 0) {
    /* 06a6 irregulars near player colony — crown nation_id, never human. */
    int hx = 0;
    int hy = 0;
    if (ai_king_weakest_port(ctx, ctx->human_nation, &hx, &hy) < 0) {
      return;
    }
    (void)ai_king_spawn_landing(ctx, crown, hx, hy, "Regular", "Soldier");
    return;
  }

  /* 0982: Man-O-War + board REF land into ship cargo (unload at coast in war_act). */
  int tx = 0;
  int ty = 0;
  const int cid = ai_king_weakest_port(ctx, ctx->human_nation, &tx, &ty);
  if (cid < 0) {
    return;
  }
  int target_fortified = 0;
  if (ctx->colonies && cid >= 0 && cid < COLONIZE_COLONIES_MAX) {
    const ColonizeColony* c = &ctx->colonies->colonies[cid];
    target_fortified = colonies_has_fortification(ctx->colonies, c) ? 1 : 0;
  }
  int spawned = 0;
  int mow_spawned = 0;
  int mow_sid = -1;
  int ship_ty = units_find_type(ctx->units, "Man-O-War");
  if (ship_ty < 0) {
    ship_ty = units_find_type(ctx->units, "Galleon");
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int sx = tx;
  int sy = ty;
  int sx2 = tx;
  int sy2 = ty;
  int found_water = 0;
  int found_water2 = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = tx + dx[d];
    const int ny = ty + dy[d];
    if (map_tile_is_water(ctx->map, nx, ny)) {
      if (!found_water) {
        sx = nx;
        sy = ny;
        found_water = 1;
      } else if (!found_water2) {
        sx2 = nx;
        sy2 = ny;
        found_water2 = 1;
      }
    }
  }
  if (ship_ty >= 0 && force[2] > 0) {
    const int sid = units_spawn_allow_stack(ctx->units, ship_ty, sx, sy);
    if (sid >= 0) {
      ColonizeUnit* ship = units_get(ctx->units, sid);
      if (ship) {
        units_set_nation(ship, crown);
        ship->orders = UNITS_ORDER_AI_SAIL;
        ship->goto_x = sx;
        ship->goto_y = sy;
      }
      force[2]--;
      spawned = 1;
      mow_spawned = 1;
      mow_sid = sid;
    }
  }
  /*
   * Second MoW stand-in: high difficulty + naval pool remaining.
   * Same 0982 spawn path; stack on first water tile if only one adjacent.
   * Source: fandom REF Men-O-War by difficulty; deep multi-ship chrome PARKED.
   */
  if (mow_spawned && ship_ty >= 0 && force[2] > 0 &&
      ctx->col1->head.difficulty >= AI_KING_SECOND_MOW_DIFF) {
    const int wx = found_water2 ? sx2 : sx;
    const int wy = found_water2 ? sy2 : sy;
    const int sid2 = units_spawn_allow_stack(ctx->units, ship_ty, wx, wy);
    if (sid2 >= 0) {
      ColonizeUnit* ship2 = units_get(ctx->units, sid2);
      if (ship2) {
        units_set_nation(ship2, crown);
        ship2->orders = UNITS_ORDER_AI_SAIL;
        ship2->goto_x = wx;
        ship2->goto_y = wy;
      }
      force[2]--;
      spawned = 1;
    }
  }

  if (mow_spawned) {
    /*
     * Board REF land into MoW cargo_ids up to real ship capacity (MoW=6).
     * Regulars (force[0]) first, then Dragoons (force[1]). Drain force[] only
     * — never invent units beyond the pool. war_act unloads at coast/colony.
     * Cite: fandom REF “Men-O-War, Regulars, Cavalry”; “man-o-war with 6
     * units”; units_board_stacked / units_ship_capacity.
     */
    int slots = 0;
    if (mow_sid >= 0) {
      slots = units_ship_capacity(ctx->units, mow_sid);
    }
    if (slots <= 0) {
      slots = COLONIZE_UNIT_CARGO_MAX;
    }
    int landed = 0;
    while (slots > 0 && force[0] > 0) {
      if (!ai_king_mow_embark_land(ctx, crown, mow_sid, tx, ty, "Regular", "Soldier")) {
        break;
      }
      force[0]--;
      slots--;
      spawned = 1;
      landed++;
    }
    while (slots > 0 && force[1] > 0) {
      if (!ai_king_mow_embark_land(ctx, crown, mow_sid, tx, ty, "Dragoon", "Scout")) {
        break;
      }
      force[1]--;
      slots--;
      spawned = 1;
      landed++;
    }
    /* Guarantee ≥1 land same beat if Regular+Dragoon pools were empty. */
    if (landed == 0) {
      if (ai_king_spawn_wave_land_from_pools(ctx, crown, tx, ty, force, target_fortified)) {
        spawned = 1;
      }
    }
  } else {
    /* No MoW this beat: one land pool type (Artillery prefer if fortified). */
    if (ai_king_spawn_wave_land_from_pools(ctx, crown, tx, ty, force, target_fortified)) {
      spawned = 1;
    }
  }
  ai_king_set_ref_present(ctx->col1, 1);
  /* Tax residual grow while at war (1d42 crumb). */
  force[0] += 1;
  /*
   * Thin 1528 announce (arrival chrome / dialog PARKED).
   * Only overwrite when a unit actually spawned — leaves thin 2564 congress
   * status intact if the wave beat is empty.
   */
  if (spawned && ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size, "The King's Expeditionary Force has arrived!");
  }
  /* FUN_43f7_1528 arrival OK popup when human queue attached. */
  if (spawned && ai_king_human_popups(ctx)) {
    (void)ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, ctx->human_nation,
      ai_king_crown_nation(ctx->human_nation), 0, "Royal Expeditionary Force",
      "The King's Expeditionary Force has arrived!"
    );
  }
}

/*
 * Try one foreign landing from backup pool k; drain on success.
 * MoW backup pool lands a Regular stand-in (no foreign MoW ship this path).
 * Returns 1 if a unit spawned, else 0.
 */
static int ai_king_intervene_one(ColonizeTurnContext* ctx, int ally, int hx, int hy,
                                 uint16_t* backup, int k) {
  static const char* names[4] = {"Regular", "Dragoon", "Man-O-War", "Artillery"};
  if (!backup || k < 0 || k > 3 || backup[k] == 0) {
    return 0;
  }
  const char* alt = NULL;
  const char* primary = names[k];
  if (k == 0) {
    alt = "Soldier";
  } else if (k == 1) {
    alt = "Scout";
  } else if (k == 2) {
    /* Naval pool: land a Regular stand-in near port (foreign MoW ship PARKED). */
    primary = "Regular";
    alt = "Soldier";
  }
  if (ai_king_spawn_landing(ctx, ally, hx, hy, primary, alt) < 0) {
    return 0;
  }
  backup[k]--;
  return 1;
}

/*
 * FUN_43f7_10f0-shaped: foreign-intervention landing when REF empty and
 * backup_force (DOS 0x53e2… stand-in) still has pools. Up to two landings
 * per call; third when difficulty ≥ AI_KING_INTERVENE_DIFF_THIRD (REF
 * pressure). Prefer Regular + Dragoon when both pools > 0. Intervene nation:
 * Euro with most colonies (tie-break land-unit force). Thin arrival OK once
 * when landings>0 + ai_popups (1528-shaped; deep economy / merc chrome PARKED).
 */
static void ai_king_foreign_intervene(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  uint16_t* backup = ctx->col1->head.backup_force;
  if (ai_king_force_total(ctx->col1->head.expeditionary_force) > 0) {
    return;
  }
  if (ai_king_force_total(backup) <= 0) {
    return;
  }
  int hx = 0;
  int hy = 0;
  if (ai_king_weakest_port(ctx, ctx->human_nation, &hx, &hy) < 0) {
    return;
  }
  const int ally = ai_king_intervention_nation(ctx, ctx->human_nation);
  int landings = 0;
  const int diff = ctx->col1->head.difficulty;
  const int max_landings =
      (diff >= AI_KING_INTERVENE_DIFF_THIRD) ? (AI_KING_INTERVENE_LANDINGS_BASE + 1)
                                            : AI_KING_INTERVENE_LANDINGS_BASE;

  /* Prefer mixing Regular + Dragoon when both foreign pools are live. */
  if (backup[0] > 0 && backup[1] > 0) {
    landings += ai_king_intervene_one(ctx, ally, hx, hy, backup, 0);
    if (landings < max_landings) {
      landings += ai_king_intervene_one(ctx, ally, hx, hy, backup, 1);
    }
  }

  for (int k = 0; k < 4 && landings < max_landings; ++k) {
    if (backup[k] == 0) {
      continue;
    }
    landings += ai_king_intervene_one(ctx, ally, hx, hy, backup, k);
  }

  /* Thin 10f0 announce once (multi-landing beat → single OK; VGA chrome PARKED). */
  if (landings > 0) {
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "Foreign troops have landed!");
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_ARRIVAL, ctx->human_nation, ally, landings,
        "Foreign Intervention", "Foreign troops have landed!"
      );
    }
  }
}

/* Pack offer-time landing into popup payload (hx<<16 | hy). */
static int ai_king_merc_payload(int hx, int hy) {
  return ((hx & 0xffff) << 16) | (hy & 0xffff);
}

static void ai_king_merc_payload_xy(int payload, int* out_x, int* out_y) {
  if (out_x) {
    *out_x = (payload >> 16) & 0xffff;
  }
  if (out_y) {
    *out_y = payload & 0xffff;
  }
}

/*
 * FUN_43f7_2244 hire accept: spend, spawn Soldier/Dragoon near (hx,hy),
 * set unknown46[3], hire status. Returns 1 on success.
 * Landing coords come from offer-time weakest port (popup payload) so a
 * same-turn REF capture cannot void the hire after CHOICE was queued.
 * Human queue: success follow-up OK after Hire apply / auto-hire.
 */
static int ai_king_do_merc_hire_at(ColonizeTurnContext* ctx, int human, int hx, int hy) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || human < 0 || human >= 4) {
    return 0;
  }
  if (hx < 0 || hy < 0) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  if (nat->gold < AI_KING_MERC_COST) {
    return 0;
  }
  if (ai_king_spawn_landing(ctx, human, hx, hy, "Soldier", "Dragoon") < 0) {
    return 0;
  }
  nat->gold -= (uint32_t)AI_KING_MERC_COST;
  if (ctx->europe) {
    ctx->europe->gold = (int)nat->gold;
  }
  ai_king_set_merc_hired(ctx->col1, 1);
  if (ctx->status && ctx->status_size) {
    snprintf(ctx->status, ctx->status_size,
             "Mercenaries join the Continental cause (−%d gold).", AI_KING_MERC_COST);
  }
  /* FUN_43f7_2244 Hire success follow-up OK (cannot-afford already OK on offer). */
  if (ai_king_human_popups(ctx)) {
    char body[AI_POPUP_BODY_LEN];
    snprintf(body, sizeof(body),
             "Mercenaries join the Continental cause (−%d gold).", AI_KING_MERC_COST);
    (void)ai_popup_enqueue_ok_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_MERC, human,
                                  ai_king_crown_nation(human),
                                  ai_king_merc_payload(hx, hy), "Mercenaries", body);
  }
  return 1;
}

static int ai_king_do_merc_hire(ColonizeTurnContext* ctx, int human) {
  int hx = 0;
  int hy = 0;
  if (ai_king_weakest_port(ctx, human, &hx, &hy) < 0) {
    return 0;
  }
  return ai_king_do_merc_hire_at(ctx, human, hx, hy);
}

/*
 * Thin FUN_43f7_2244 stand-in: once-per-war Continental merc offer when SoL>50.
 * Human + ctx->ai_popups + gold≥300 → CHOICE Hire/Decline (apply_popup_result).
 * Else gold≥300 → auto-hire. gold insufficient → cannot-afford status + OK once.
 * Gate: unknown46[3] — set on hire, decline, or cannot-afford (no spam).
 */
static void ai_king_merc_offer(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human >= 4) {
    return;
  }
  /* Once-per-war: flag set after hire / decline / cannot-afford. */
  if (ai_king_merc_hired(ctx->col1)) {
    return;
  }
  if (ai_king_sol_percent(ctx, human) <= AI_KING_MERC_SOL_MIN) {
    return;
  }
  int hx = 0;
  int hy = 0;
  if (ai_king_weakest_port(ctx, human, &hx, &hy) < 0) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[human];
  if (nat->gold < AI_KING_MERC_COST) {
    ai_king_set_merc_hired(ctx->col1, 1);
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size, "Cannot afford mercenaries.");
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_KING_MERC, human, ai_king_crown_nation(human),
        ai_king_merc_payload(hx, hy), "Mercenaries", "Cannot afford mercenaries."
      );
    }
    return;
  }
  if (ai_king_human_popups(ctx)) {
    const char* labels[] = {"Hire", "Decline"};
    const int ids[] = {AI_KING_CHOICE_HIRE, AI_KING_CHOICE_DECLINE};
    char body[AI_POPUP_BODY_LEN];
    snprintf(body, sizeof(body),
             "European mercenaries offer to join for %d gold. Hire them?",
             AI_KING_MERC_COST);
    if (ai_popup_enqueue_choice_ctx(ctx->ai_popups, AI_POPUP_TAG_KING_MERC, human,
                                    ai_king_crown_nation(human),
                                    ai_king_merc_payload(hx, hy), "Mercenaries", body,
                                    labels, ids, 2)) {
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size,
                 "Mercenaries offer to join the Continental cause (−%d gold).",
                 AI_KING_MERC_COST);
      }
      /* Defer unknown46[3] until Hire/Decline apply (re-offer if Esc cancel). */
      return;
    }
    /* Queue full — fall through to auto hire. */
  }
  (void)ai_king_do_merc_hire_at(ctx, human, hx, hy);
}

/*
 * FUN_43f7_2022 war act + 1eca promote.
 * REF land hunt (Regular/Dragoon/Artillery/Cont.→nearest human colony/unit) + combat/capture;
 * thin Artillery siege prefer fortified (adjacent unfortified must not override);
 * thin Dragoon/Cont. Cav prefer open when Artillery type exists; capital MD bias
 * (founding capital over distant colonies when MD within slack); post-capture
 * fortify one Regular (else Dragoon/Cont.Cav; stack extras hunt) + human status; wartime MoW with cargo
 * → unload-at-coast up to min(moves,capacity) Regular-prefer else Dragoon
 * (prefer colony tile / seize; spend 1 MP/pax) else AI_SAIL→human coast; after
 * *full* unload with moves left → AI_SAIL toward *next* human coast (skip
 * just-served port); after that sail step (or already on next-coast water)
 * prefer unload if still carrying and adjacent; same-beat post-unload
 * capture/fortify for passengers skipped while aboard; idle empty MoW →
 * AI_SAIL coastal patrol (nearest human coast water; no new ships); 0982
 * boards up to ship capacity into cargo_ids;
 * 1eca colony-SoL bands (40–50 vet / >50 Continental+Regular); Cont. Army/Cav
 * after promote → capital-rally (founding capital; weakest_port fallback);
 * 10f0 intervene arm (≤3 @ difficulty≥2); thin 2244 merc auto-accept or
 * cannot-afford once/war.
 * REF idle Regular on crown colony (no adjacent foe) → fortify only if no other
 * Regular/Dragoon/Cont.Cav on tile is already FORTIFY/FORTIFIED; if no Regular,
 * fortify one Dragoon/Cont.Cav (Colonization.pdf Defending a Colony; king_ref
 * one-garrison); already-garrisoned stay put; extras hunt.
 * Idle Artillery on crown/captured colony → FORTIFY (Euro after-siege pattern;
 * Colonization.pdf fortify defense; euro_unit_act Artillery fortify).
 */
static void ai_king_war_act(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  /*
   * Rebel arm first: 10f0 while human ports still exist (crown move/capture
   * below may seize the landing pick). In addition to 06a6 in ref_wave.
   */
  ai_king_foreign_intervene(ctx);
  /* Thin 2244: once-per-war Continental merc for human (hire CHOICE / auto). */
  ai_king_merc_offer(ctx);

  const int crown = ai_king_crown_nation(ctx->human_nation);
  const int human = ctx->human_nation;

  /*
   * Rebel arm (1eca + Cont. hunt) before crown capture so Cont. Army can still
   * aim at human ports while they exist. FUN_43f7_1eca promote (catalog:
   * Continental when colony SoL>50%):
   * Per-unit SoL from Col1 rebel_dividend/divisor at the unit tile
   * (ai_king_colony_sol_at); nation 0004 aggregate only as fallback.
   *   colony SoL>50: Soldier* → Continental Army / Cont. Army / Veteran Soldier
   *           Dragoon|Cavalry* → Continental Cavalry / Cont. Cav. / Veteran Dragoon
   *           Regular* → Veteran Soldier / Continental Army (fallback)
   *   colony SoL 40..50 (incl. exactly 50): Soldier* → Veteran Soldier only
   *     (if type exists; no Continental); Regular/Dragoon unchanged
   * Skip already Veteran / Continental / Cont. Army / Cont. Cav (abbrev Cont.
   * lacks "Continental" — reuse ai_king_is_continental).
   * Note: armed Regulars often *display* as "Soldier" — classify Regular by type name.
   * King promote path only — not FF Washington mass-promote (combat upgrade PARKED).
   * Deep veteran-profession / type-id table remains PARKED.
   */
  {
    int army = units_find_type(ctx->units, "Continental Army");
    if (army < 0) {
      army = units_find_type(ctx->units, "Cont. Army");
    }
    if (army < 0) {
      army = units_find_type(ctx->units, "Veteran Soldier");
    }
    int cav = units_find_type(ctx->units, "Continental Cavalry");
    if (cav < 0) {
      cav = units_find_type(ctx->units, "Cont. Cav.");
    }
    if (cav < 0) {
      cav = units_find_type(ctx->units, "Veteran Dragoon");
    }
    int regular_tgt = units_find_type(ctx->units, "Veteran Soldier");
    if (regular_tgt < 0) {
      regular_tgt = units_find_type(ctx->units, "Continental Army");
    }
    if (regular_tgt < 0) {
      regular_tgt = units_find_type(ctx->units, "Cont. Army");
    }
    const int vet = units_find_type(ctx->units, "Veteran Soldier");
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != human) {
        continue;
      }
      const ColonizeUnitType* ut = units_type(ctx->units, u->type_index);
      const char* tname = ut ? ut->name : NULL;
      const char* name = units_display_name(ctx->units, u);
      /*
       * Already promoted: skip Veteran / Continental / Cont. Army / Cont. Cav
       * (hunter Cont. check — abbrev Cont.* lacks "Continental"/"Veteran").
       * SoL bands: >50 Continental; 40..50 (incl. exactly 50) Veteran Soldier
       * only. Catalog: promote when colony SoL>50%.
       */
      if (ai_king_is_continental(ctx->units, u)) {
        continue;
      }
      if ((name && strstr(name, "Veteran")) || (tname && strstr(tname, "Veteran"))) {
        continue;
      }
      /* 1eca: bias threshold with colony SoL at tile (Washington FF path is separate). */
      const int sol_p = ai_king_colony_sol_at(ctx, human, u->x, u->y);
      const int is_regular = (tname && strstr(tname, "Regular") != NULL);
      if (sol_p > 50) {
        if (is_regular) {
          if (regular_tgt >= 0) {
            u->type_index = regular_tgt;
          }
        } else if (army >= 0 &&
                   ((name && strstr(name, "Soldier")) ||
                    (tname && strstr(tname, "Soldier")))) {
          u->type_index = army;
        } else if (cav >= 0 &&
                   ((name && (strstr(name, "Dragoon") || strstr(name, "Cavalry"))) ||
                    (tname && (strstr(tname, "Dragoon") || strstr(tname, "Cavalry"))))) {
          u->type_index = cav;
        }
      } else if (sol_p >= 40) {
        /* Mid-band 40..50: Soldier → Veteran Soldier; Regular/Dragoon unchanged. */
        if (is_regular || vet < 0) {
          continue;
        }
        if ((name && strstr(name, "Soldier")) || (tname && strstr(tname, "Soldier"))) {
          u->type_index = vet;
        }
      }
    }
  }

  /*
   * After 1eca Continental promote: Cont. Army / Cont. Cav (hunter name check
   * includes both) AI_MOVE toward nearest human colony, then prefer founding
   * capital when MD within AI_KING_CAPITAL_MD_SLACK (same helper as REF idle
   * hunters). Fallback weakest_port when no human colony. Hold if already on a
   * human colony tile. Source: fandom Independence Cont. Army / Cont. Cavalry +
   * REF main-port MD slack; deep rebel AI PARKED.
   */
  {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != human || u->moves_left <= 0) {
        continue;
      }
      if (units_is_sea(ctx->units, u->id)) {
        continue;
      }
      if (!ai_king_is_continental(ctx->units, u)) {
        continue;
      }
      /* Already on a human colony tile — hold; founding capital may fortify
       * up to two Cont. Army / Cont. Cav (Defending a Colony cap 2). */
      if (ctx->colonies) {
        const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
        if (cid >= 0) {
          const ColonizeColony* c = &ctx->colonies->colonies[cid];
          if (c->active && c->nation_id == human) {
            int cap_x = 0;
            int cap_y = 0;
            if (ai_king_human_capital(ctx, human, &cap_x, &cap_y) && u->x == cap_x &&
                u->y == cap_y) {
              if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
                continue;
              }
              if (ai_king_tile_human_cont_fortified_count(ctx, human, u->x, u->y, u->id) < 2) {
                ai_king_fortify_human_cont_at(ctx, u, human, u->x, u->y);
                if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
                  continue;
                }
              }
            }
            continue;
          }
        }
      }
      int hx = 0;
      int hy = 0;
      if (ai_king_nearest_human_colony(ctx, human, u->x, u->y, &hx, &hy)) {
        const int nearest_md = abs(hx - u->x) + abs(hy - u->y);
        (void)ai_king_prefer_capital_if_comparable(ctx, human, u->x, u->y, &hx, &hy,
                                                   nearest_md);
      } else if (ai_king_weakest_port(ctx, human, &hx, &hy) < 0) {
        continue;
      }
      u->orders = UNITS_ORDER_AI_MOVE;
      u->goto_x = hx;
      u->goto_y = hy;
    }
  }

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != crown) {
      continue;
    }
    /* Already on a human colony → capture even with 0 moves (conquest). */
    ai_king_try_capture_at(ctx, u, crown, human);
    if (!u->active || u->moves_left <= 0) {
      continue;
    }

    /*
     * Wartime MoW (fandom REF man-o-war → ports):
     *   cargo > 0 → unload Regular-prefer (else Dragoon) when adjacent to
     *     foundable/coastal land by a human colony (units_unload_passenger),
     *     up to min(moves_left, capacity) this beat (1 MP per pax); same-beat
     *     seize/fortify passengers that were skipped while aboard.
     *     Partial unload (cargo left) → hold; leftover sails next beat.
     *     Full unload + moves left → AI_SAIL toward *next* human coast
     *     (skip the port just served). Else (not at coast) AI_SAIL→coast.
     *     After that coast/next-coast sail step: if still carrying and now
     *     adjacent to a human colony → prefer unload same beat (then, if the
     *     hold empties with moves left, retarget next human coast).
     *   cargo == 0 (idle empty) → AI_SAIL coastal patrol toward water adjacent
     *     to nearest human coastal colony. Redirects existing ships only —
     *     do not invent new MoW. Hold fill is units_ship_capacity (MoW×6);
     *     embark UI chrome PARKED; 160a letter cinematic PARKED.
     */
    if (ai_king_is_mow(ctx->units, u)) {
      int unloaded = 0;
      int land_x = 0;
      int land_y = 0;
      if (u->cargo_count > 0) {
        unloaded = ai_king_mow_try_unload(ctx, u, human, &land_x, &land_y);
        if (unloaded > 0) {
          ai_king_mow_post_unload_land(ctx, crown, human, land_x, land_y);
        }
        if (u->cargo_count > 0) {
          /* Partial unload or unload failed mid-hold — leftover next beat. */
          if (unloaded > 0) {
            continue;
          }
          /* Not at coast — fall through to AI_SAIL toward human coast. */
        } else if (u->moves_left <= 0) {
          /* Full unload spent remaining MP — no same-beat sail. */
          continue;
        }
        /* Full unload with moves left → sail to next human coast below. */
      }
      int wx = 0;
      int wy = 0;
      /* After full unload, prefer another human port's water (next coast). */
      const int skip_adj = (unloaded > 0 && u->cargo_count == 0) ? 1 : 0;
      if (ai_king_human_coast_water(ctx, human, u->x, u->y, &wx, &wy, skip_adj)) {
        u->orders = UNITS_ORDER_AI_SAIL;
        u->goto_x = wx;
        u->goto_y = wy;
      }
      /*
       * Already on next-coast water and still carrying: prefer unload at the
       * adjacent colony over burning MP on a zero-step sail (R5). Gate on
       * human colony adjacency — not soft coastal land alone — so mid-route
       * sail steps do not dump onto foundable tiles one MD early.
       */
      if (u->cargo_count > 0 && u->moves_left > 0 && u->goto_x == u->x &&
          u->goto_y == u->y &&
          ai_king_adjacent_human_colony(ctx, human, u->x, u->y)) {
        land_x = 0;
        land_y = 0;
        const int u_here = ai_king_mow_try_unload(ctx, u, human, &land_x, &land_y);
        if (u_here > 0) {
          ai_king_mow_post_unload_land(ctx, crown, human, land_x, land_y);
          unloaded += u_here;
          if (u->cargo_count == 0 && u->moves_left > 0) {
            if (ai_king_human_coast_water(ctx, human, u->x, u->y, &wx, &wy, 1)) {
              u->orders = UNITS_ORDER_AI_SAIL;
              u->goto_x = wx;
              u->goto_y = wy;
            }
          } else {
            continue;
          }
        }
      }
      if (u->moves_left <= 0) {
        continue;
      }
      int tx = u->goto_x;
      int ty = u->goto_y;
      if (tx < 0 || ty < 0 || tx >= 255 || ty >= 255) {
        continue;
      }
      const int sdx = (tx > u->x) - (tx < u->x);
      const int sdy = (ty > u->y) - (ty < u->y);
      const int nx = u->x + sdx;
      const int ny = u->y + sdy;
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe >= 0) {
        const ColonizeUnit* f = units_get_const(ctx->units, foe);
        if (f && f->nation_id == human && units_is_sea(ctx->units, foe)) {
          units_resolve_naval_combat(ctx->units, u->id, foe, ctx->rng);
        }
        continue;
      }
      if ((sdx != 0 || sdy != 0) && map_tile_is_water(ctx->map, nx, ny)) {
        units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng);
      }
      /*
       * After coast / next-coast sail step: if still carrying and now adjacent
       * to a human colony tile, prefer unload same beat (fandom man-o-war →
       * ports). Soft-coast-only adjacency must not trigger mid-route.
       */
      if (u->cargo_count > 0 && u->moves_left > 0 &&
          ai_king_adjacent_human_colony(ctx, human, u->x, u->y)) {
        land_x = 0;
        land_y = 0;
        const int u_after = ai_king_mow_try_unload(ctx, u, human, &land_x, &land_y);
        if (u_after > 0) {
          ai_king_mow_post_unload_land(ctx, crown, human, land_x, land_y);
          if (u->cargo_count == 0 && u->moves_left > 0) {
            if (ai_king_human_coast_water(ctx, human, u->x, u->y, &wx, &wy, 1)) {
              u->orders = UNITS_ORDER_AI_SAIL;
              u->goto_x = wx;
              u->goto_y = wy;
            }
          }
        }
      }
      continue;
    }

    /*
     * REF idle garrison (heal/fortify stand-in): Regular, or Dragoon/Cont. Cav
     * when no Regular, on own (crown) colony — including a captured human
     * capital — with no adjacent human foe/colony → fortify **one** when the
     * stack rule allows (cap 2). Already FORTIFY/FORTIFIED: stay (do not wake).
     * When two crown garrison units are already FORTIFY/FORTIFIED on the tile,
     * leave this unit free to hunt. Second slot needs moves_left > 0.
     * Cite: Colonization.pdf Defending a Colony ("fortify soldiers, dragoons…");
     * king_ref thin multi-garrison; units_order_fortify. Deep siege PARKED.
     */
    if ((ai_king_is_regular(ctx->units, u) ||
         ai_king_is_garrison_cavalry(ctx->units, u)) &&
        ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (cid >= 0) {
        const ColonizeColony* c = &ctx->colonies->colonies[cid];
        if (c->active && c->nation_id == crown &&
            !ai_king_adjacent_human_unit(ctx, human, u->x, u->y) &&
            !ai_king_adjacent_human_colony(ctx, human, u->x, u->y)) {
          /* Prefer stay garrisoned once stack slot is taken by this unit. */
          if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
            continue;
          }
          if (ai_king_tile_fortified_garrison_count(ctx, crown, u->x, u->y, u->id) < 2) {
            /* Prefer Regular over Dragoon/Cont.Cav on same tile (cap 2). */
            ai_king_fortify_garrison_at(ctx, u, crown, u->x, u->y);
            if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
              continue;
            }
            /* Regular claimed the slot (or none eligible) — fall through to hunt. */
          }
          /* Extra on garrisoned colony — fall through to hunt. */
        }
      }
    }

    /*
     * Artillery idle fortify (Euro after-siege pattern): Artillery on own
     * (crown) colony — including newly captured — with no adjacent human
     * foe/colony → FORTIFY and hold. Already FORTIFY/FORTIFIED stay put.
     * Cite: euro_unit_act Artillery fortify after siege; Colonization.pdf
     * fortify defense / Artillery; case 0x0b fortify arm. Off-colony Artillery
     * still hunts fortified ports (thin siege bias below).
     */
    if (ai_king_is_artillery(ctx->units, u) && ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (cid >= 0) {
        const ColonizeColony* c = &ctx->colonies->colonies[cid];
        if (c->active && c->nation_id == crown &&
            !ai_king_adjacent_human_unit(ctx, human, u->x, u->y) &&
            !ai_king_adjacent_human_colony(ctx, human, u->x, u->y)) {
          if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
            continue;
          }
          (void)units_order_fortify(ctx->units, u->id);
          continue;
        }
      }
    }

    /*
     * REF land hunt (fandom REF AI; conceptual reuse of ai_euro land hunt):
     * idle Regular/Dragoon/Artillery/Cont. with moves → AI_MOVE toward nearest
     * human colony or human land unit. Artillery prefers fortified colonies when
     * the Artillery type exists in pool. Dragoon / Cont. Cav prefer open land /
     * unfortified colonies when Artillery exists (thin role split; else nearest).
     * Capital MD bias: founding capital preferred over a nearer distant colony
     * when MD within AI_KING_CAPITAL_MD_SLACK (idle hunters). Prefer adjacent
     * uncaptured colony over marching past (Artillery: adjacent unfortified must
     * not override a fortified hunt). Deeper multi-step combat scoring PARKED.
     *
     * After-capture extras: standing on a crown colony whose two fortify slots
     * are already taken (two Regular/Dragoon/Cont.Cav FORTIFY/FORTIFIED) →
     * prefer next nearest remaining human colony (strict MD; no capital slack /
     * no closer unit bait). Source: fandom REF AI uncaptured-colony pressure;
     * Colonization.pdf Defending a Colony; king_ref thin multi-garrison (cap 2).
     */
    if (ai_king_is_ref_land_hunter(ctx->units, u)) {
      const int have_arty = (ai_king_artillery_type(ctx->units) >= 0) ? 1 : 0;
      const int prefer_fort =
          (have_arty && ai_king_is_artillery(ctx->units, u)) ? 1 : 0;
      const int prefer_open =
          (have_arty && ai_king_prefers_open_land(ctx->units, u)) ? 1 : 0;
      /*
       * Extra on garrisoned crown colony (post-capture / stack): next colony.
       * Captured founding capital is no longer human — do not re-apply capital
       * MD slack to the next-lowest colony id.
       */
      int after_capture_next = 0;
      if (ctx->colonies) {
        const int own_cid = colonies_id_at(ctx->colonies, u->x, u->y);
        if (own_cid >= 0) {
          const ColonizeColony* own = &ctx->colonies->colonies[own_cid];
          if (own->active && own->nation_id == crown &&
              ai_king_tile_fortified_garrison_count(ctx, crown, u->x, u->y, u->id) >= 2) {
            after_capture_next = 1;
          }
        }
      }
      int hx = 0;
      int hy = 0;
      int have_hunt = 0;
      if (after_capture_next) {
        /* Prefer next uncaptured colony; if none left, fall back to unit hunt. */
        have_hunt = ai_king_nearest_human_colony(ctx, human, u->x, u->y, &hx, &hy);
        if (!have_hunt) {
          have_hunt =
              ai_king_ref_hunt_target(ctx, human, u->x, u->y, &hx, &hy, prefer_fort,
                                      prefer_open);
        }
      } else {
        have_hunt =
            ai_king_ref_hunt_target(ctx, human, u->x, u->y, &hx, &hy, prefer_fort,
                                    prefer_open);
      }
      /*
       * Adjacent human colony normally wins over a farther hunt (fandom: attack
       * adjacent uncaptured colony rather than march past). Artillery siege
       * tighten: adjacent fortified wins; do not override a fortified hunt
       * target with an unfortified adjacent colony (leave open ports to
       * Dragoon/Regular). Deep multi-step siege scoring PARKED.
       */
      if (ctx->colonies) {
        static const int adx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int ady[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        int adj_fort_x = -1;
        int adj_fort_y = -1;
        int adj_any_x = -1;
        int adj_any_y = -1;
        for (int d = 0; d < 8; ++d) {
          const int cx = u->x + adx[d];
          const int cy = u->y + ady[d];
          const int cid = colonies_id_at(ctx->colonies, cx, cy);
          if (cid < 0) {
            continue;
          }
          const ColonizeColony* c = &ctx->colonies->colonies[cid];
          if (!c->active || c->nation_id != human) {
            continue;
          }
          if (adj_any_x < 0) {
            adj_any_x = cx;
            adj_any_y = cy;
          }
          if (colonies_has_fortification(ctx->colonies, c)) {
            adj_fort_x = cx;
            adj_fort_y = cy;
            break;
          }
        }
        if (prefer_fort) {
          if (adj_fort_x >= 0) {
            hx = adj_fort_x;
            hy = adj_fort_y;
            have_hunt = 1;
          } else {
            int hunt_is_fort = 0;
            if (have_hunt) {
              const int hid = colonies_id_at(ctx->colonies, hx, hy);
              if (hid >= 0) {
                const ColonizeColony* hc = &ctx->colonies->colonies[hid];
                if (hc->active && hc->nation_id == human &&
                    colonies_has_fortification(ctx->colonies, hc)) {
                  hunt_is_fort = 1;
                }
              }
            }
            /* No fortified hunt — any adjacent colony may override (fallback). */
            if (!hunt_is_fort && adj_any_x >= 0) {
              hx = adj_any_x;
              hy = adj_any_y;
              have_hunt = 1;
            }
          }
        } else if (adj_any_x >= 0) {
          hx = adj_any_x;
          hy = adj_any_y;
          have_hunt = 1;
        }
      }
      if (have_hunt) {
        u->orders = UNITS_ORDER_AI_MOVE;
        u->goto_x = hx;
        u->goto_y = hy;
      } else if (u->goto_x < 0 || u->goto_y < 0 || u->goto_x >= 255 || u->goto_y >= 255) {
        if (ai_king_weakest_port(ctx, human, &hx, &hy) >= 0) {
          u->orders = UNITS_ORDER_AI_MOVE;
          u->goto_x = hx;
          u->goto_y = hy;
        }
      }
    } else if (u->goto_x < 0 || u->goto_y < 0 || u->goto_x >= 255 || u->goto_y >= 255) {
      int tx = 0;
      int ty = 0;
      if (ai_king_weakest_port(ctx, human, &tx, &ty) < 0) {
        continue;
      }
      u->goto_x = tx;
      u->goto_y = ty;
    }

    int tx = u->goto_x;
    int ty = u->goto_y;
    if (tx < 0 || ty < 0 || tx >= 255 || ty >= 255) {
      continue;
    }
    const int sdx = (tx > u->x) - (tx < u->x);
    const int sdy = (ty > u->y) - (ty < u->y);
    const int nx = u->x + sdx;
    const int ny = u->y + sdy;
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe >= 0) {
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (f && f->nation_id == human) {
        if (units_is_sea(ctx->units, u->id)) {
          units_resolve_naval_combat(ctx->units, u->id, foe, ctx->rng);
        } else if (units_resolve_land_combat(ctx->units, u->id, foe, ctx->rng)) {
          /* Attack win → occupy tile; capture if it was a colony (conquest). */
          units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng);
          ai_king_try_capture_at(ctx, u, crown, human);
        }
        continue;
      }
    }
    units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng);
    ai_king_try_capture_at(ctx, u, crown, human);
  }
}

/*
 * Revolution end (fandom Independence / manual 1800–1850):
 *   Lose: WoI + zero coastal human ports.
 *   Win: WoI + year≥1850 + no crown REF units on map.
 * Latches unknown46[4]; score reads won/lost. Cite: docs/fandom_col1994.md.
 */
static int ai_king_human_coastal_ports(const ColonizeTurnContext* ctx, int human) {
  if (!ctx || human < 0 || human > 3) {
    return 0;
  }
  int n = 0;
  if (ctx->colonies && ctx->map) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != human) {
        continue;
      }
      if (map_tile_is_coastal(ctx->map, c->x, c->y)) {
        ++n;
      }
    }
  }
  /* Col1 colony list (smoke / bridge) when runtime pool empty. */
  if (n == 0 && ctx->col1_ok && ctx->col1 && ctx->map && ctx->col1->colony) {
    for (uint16_t i = 0; i < ctx->col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &ctx->col1->colony[i];
      if ((int)c->nation_id != human) {
        continue;
      }
      if (map_tile_is_coastal(ctx->map, (int)c->x, (int)c->y)) {
        ++n;
      }
    }
  }
  return n;
}

static int ai_king_crown_units_alive(const ColonizeTurnContext* ctx, int crown) {
  if (!ctx || !ctx->units || crown < 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (u->active && u->nation_id == crown) {
      ++n;
    }
  }
  return n;
}

static void ai_king_check_revolution_end(ColonizeTurnContext* ctx, int ref_already) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  if (ctx->col1->head.unknown46[AI_KING_ENDGAME_BYTE] != AI_KING_ENDGAME_NONE) {
    return; /* already resolved */
  }
  const int human = ctx->human_nation;
  const int crown = ai_king_crown_nation(human);
  const int ports = ai_king_human_coastal_ports(ctx, human);
  /*
   * Lose: all coastal ports gone, and REF was already invading before this
   * turn's wave (avoid clobbering same-turn 1528 declare/arrival chrome).
   * Cite: docs/fandom_col1994.md Independence — lose all ports.
   */
  if (ports <= 0 && ref_already) {
    ctx->col1->head.unknown46[AI_KING_ENDGAME_BYTE] = AI_KING_ENDGAME_LOST;
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size,
               "The Revolution has failed — all port colonies lost.");
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, 2, "Revolution Failed",
        "The Revolution has failed. All port colonies are lost."
      );
    }
    return;
  }
  const int year = (int)ctx->col1->head.year;
  if (year >= AI_KING_YEAR_CAP && ai_king_crown_units_alive(ctx, crown) <= 0) {
    ctx->col1->head.unknown46[AI_KING_ENDGAME_BYTE] = AI_KING_ENDGAME_WON;
    ctx->col1->head.unknown46[AI_KING_REF_PRESENT_BYTE] = 0;
    if (ctx->status && ctx->status_size) {
      snprintf(ctx->status, ctx->status_size,
               "Independence won! The Royal Expeditionary Force is defeated.");
    }
    if (ai_king_human_popups(ctx)) {
      (void)ai_popup_enqueue_ok_ctx(
        ctx->ai_popups, AI_POPUP_TAG_INFO, human, crown, 1, "Independence",
        "Independence is won! The Royal Expeditionary Force is no more."
      );
    }
  }
}

void ai_king_nation_turn(ColonizeTurnContext* ctx) {
  if (!ctx) {
    return;
  }
  /*
   * FUN_43f7_2424-shaped:
   *   SoL → peacetime (1d42 tax, SoL chrome, 2564/1a26 declare) | wartime (2022 wave+act)
   */
  /* External boycott clear (Fugger/diplo) → drop refuse even mid-war / off-tax years. */
  if (ctx->col1_ok && ctx->col1) {
    ai_king_sync_boycott_refuse(ctx->col1, ctx->human_nation);
  }
  const int sol = ai_king_sol_percent(ctx, ctx->human_nation);

  if (!ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
    ai_king_tax_event(ctx);
    /*
     * Peacetime calendar end (manual pp.10–12 / 1800–1850): without WoI,
     * year≥1800 latches once. Cite: docs/manual_gap.md Auto-end.
     */
    if (ctx->col1_ok && ctx->col1 &&
        ctx->col1->head.unknown46[AI_KING_ENDGAME_BYTE] == AI_KING_ENDGAME_NONE &&
        (int)ctx->col1->head.year >= AI_KING_PEACE_YEAR_CAP) {
      ctx->col1->head.unknown46[AI_KING_ENDGAME_BYTE] = AI_KING_ENDGAME_PEACE_1800;
      if (ctx->status && ctx->status_size) {
        snprintf(ctx->status, ctx->status_size,
                 "The colonial era ends (%d). Retire to see your score.",
                 AI_KING_PEACE_YEAR_CAP);
      }
      if (ai_king_human_popups(ctx)) {
        (void)ai_popup_enqueue_ok_ctx(
          ctx->ai_popups, AI_POPUP_TAG_INFO, ctx->human_nation,
          ai_king_crown_nation(ctx->human_nation), AI_KING_PEACE_YEAR_CAP,
          "Colonial Era Ends",
          "The year 1800 arrives. Retire to record your Colonization Score."
        );
      }
    }
    /*
     * Thin pre-declare SoL chrome:
     * SoL AI_KING_RESTLESS_SOL_MIN..(DECLARE_MIN-1) → restless status line
     * before the auto-declare gate. unknown46 consistency: do not set WoI[0] /
     * congress[5] here (declare only). Optional tax mention when tax_rate
     * already in the refuse band (≥20) — reads existing tax_rate; no invented
     * tax formula. Do not clobber thin 38fd_5be8 tax audience / hike status
     * from 1d42 (ai_popup CHOICE when queue attached). (2564 in try_declare.)
     */
    if (sol >= AI_KING_RESTLESS_SOL_MIN && sol < AI_KING_DECLARE_SOL_MIN && ctx->status &&
        ctx->status_size) {
      const int keep_tax_audience =
          strstr(ctx->status, "refuse") || strstr(ctx->status, "Audience") ||
          strstr(ctx->status, "raises taxes") || strstr(ctx->status, "Tax stays");
      if (!keep_tax_audience) {
        const uint8_t tax =
            (ctx->col1_ok && ctx->col1 && ctx->human_nation >= 0 && ctx->human_nation < 4)
                ? ctx->col1->nation[ctx->human_nation].tax_rate
                : 0;
        if (tax >= AI_KING_BOYCOTT_TAX_MIN) {
          snprintf(ctx->status, ctx->status_size,
                   "Sons of Liberty grow restless (%d%%). Tax is at %u%%.", sol, tax);
        } else {
          snprintf(ctx->status, ctx->status_size, "Sons of Liberty grow restless (%d%%).", sol);
        }
        /* FUN_43f7_0004 / peacetime chrome: restless OK when human queue attached. */
        if (ai_king_human_popups(ctx)) {
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups, AI_POPUP_TAG_INFO, ctx->human_nation,
            ai_king_crown_nation(ctx->human_nation), sol, "Sons of Liberty", ctx->status
          );
        }
      }
    }
    ai_king_try_declare(ctx);
  }

  if (ai_king_independence_declared(ctx->col1_ok ? ctx->col1 : NULL)) {
    const int ref_already =
      ctx->col1_ok && ctx->col1 &&
      ctx->col1->head.unknown46[AI_KING_REF_PRESENT_BYTE] != 0;
    ai_king_ref_wave(ctx);
    ai_king_war_act(ctx);
    /* Lose only after REF was already present (not same-turn declare/wave). */
    ai_king_check_revolution_end(ctx, ref_already);
  }

  if (ctx->active_turn_nation) {
    *ctx->active_turn_nation = ctx->human_nation;
  }
}

void ai_king_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup) {
  if (!ctx || !popup || !popup->has_result || popup->result_cancelled) {
    return;
  }
  const int human = (popup->result_nation_a >= 0 && popup->result_nation_a < 4)
                      ? popup->result_nation_a
                      : ctx->human_nation;
  switch (popup->result_tag) {
    case AI_POPUP_TAG_KING_AUDIENCE:
      /* FUN_43f7_38fd_5be8: Accept → hike; Refuse → boycott/refuse path. */
      if (popup->result_choice_id == AI_KING_CHOICE_ACCEPT) {
        ai_king_tax_accept_hike(ctx, human);
      } else if (popup->result_choice_id == AI_KING_CHOICE_REFUSE) {
        ai_king_tax_refuse_hike(ctx, human);
      }
      break;
    case AI_POPUP_TAG_KING_DUMP_GOODS:
      /* Dump-goods modal: choice_id is cargo index to OR into boycott_bitmap. */
      ai_king_apply_dump_goods_choice(ctx, human, popup->result_choice_id);
      break;
    case AI_POPUP_TAG_KING_MERC:
      /* FUN_43f7_2244: Hire → spend/spawn at offer-time port; Decline → gate. */
      if (popup->result_choice_id == AI_KING_CHOICE_HIRE) {
        int hx = 0;
        int hy = 0;
        ai_king_merc_payload_xy(popup->result_payload, &hx, &hy);
        if (!ai_king_do_merc_hire_at(ctx, human, hx, hy) &&
            !ai_king_do_merc_hire(ctx, human)) {
          /* Gold drained or spawn fail — still gate so offer does not loop. */
          if (ctx->col1_ok && ctx->col1) {
            ai_king_set_merc_hired(ctx->col1, 1);
          }
          if (ctx->status && ctx->status_size) {
            snprintf(ctx->status, ctx->status_size, "Cannot afford mercenaries.");
          }
        }
      } else if (popup->result_choice_id == AI_KING_CHOICE_DECLINE) {
        /* FUN_43f7_2244 Decline: gate once/war + follow-up OK (Hire already OK). */
        if (ctx->col1_ok && ctx->col1) {
          ai_king_set_merc_hired(ctx->col1, 1);
        }
        if (ctx->status && ctx->status_size) {
          snprintf(ctx->status, ctx->status_size, "Mercenaries declined.");
        }
        if (ai_king_human_popups(ctx)) {
          (void)ai_popup_enqueue_ok_ctx(
            ctx->ai_popups, AI_POPUP_TAG_KING_MERC, human, ai_king_crown_nation(human),
            popup->result_payload, "Mercenaries", "Mercenaries declined."
          );
        }
      }
      break;
    case AI_POPUP_TAG_KING_CONGRESS:
      /* FUN_43f7_2564 / 1a26: Confirm → declare; Not yet → leave peacetime. */
      if (popup->result_choice_id == AI_KING_CHOICE_CONFIRM) {
        ai_king_do_declare(ctx, human);
      }
      break;
    default:
      break;
  }
}
