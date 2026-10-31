/*
 * Indian contact — meet trade and the FUN_5fef_0f14 colony raid resolver
 *
 * Split out of ai_contact.c (2026-09-23) verbatim; shared symbols are
 * declared in ai_contact_internal.h. See ai_contact.c for the module
 * prologue and the DOS provenance notes.
 *
 * Sections:
 *   Raid-kind selection, loot, alarm tail, 1b0e handoff & human-victim chrome
 */

#include "core/internal.h"
#include "core/ai_contact.h"
#include "core/ai_contact_internal.h"

#include "core/ai.h"
#include "core/ai_diplo.h"
#include "core/sound.h"
#include "core/ai_king.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/strutil.h"
#include "core/units.h"
#include "core/village_trade_intel.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * FUN_5fef_0f14 picks the kind AND resolves that kind's target/amount in one
 * head (raw 99777-99893) — a cargo it cannot find and a gold roll the victim
 * cannot pay both collapse the raid to kind 0 — so the picker stashes what it
 * rolled here for the loot arms below, exactly as DOS keeps `local_20` /
 * `local_12` alive across its own arms.
 */
int ai_contact_s_raid_cargo = -1;  /* DOS local_20 */
static int ai_contact_s_raid_burn_row = -1;  /* DOS local_a (@BUILDING row 0..0x29) */
static long ai_contact_s_raid_gold = 0;   /* DOS local_12/local_10 */

/* ===================== Raid-kind selection, loot, alarm tail, 1b0e handoff & human-victim chrome (ai_contact_raid_0a88 .. ai_contact_raid_human_chrome) ===================== */


/*
 * FUN_281f_0a88 → FUN_15eb_14aa (raw 11444-11455): walk a @BUILDING row's
 * parent column up to its chain root. The port models the parent column as
 * the chain tables in colony_build.c, so the root is tier 0 of the row's
 * chain (a row in no chain is its own root).
 */
static int ai_contact_raid_0a88(int row) {
  const int chain = colonies_building_row_chain(row);
  const int* rows = (chain >= 0) ? colonies_building_chain_rows(chain) : NULL;
  return (rows && rows[0] >= 0) ? rows[0] : row;
}

/*
 * The predecessor byte `DS:(row*0xc - 0x707a)` that FUN_5fef_0f14's kind-2
 * tail walks (raw 99859): the tier below `row` in its chain, or -1.
 */
static int ai_contact_raid_pred_row(int row) {
  const int chain = colonies_building_row_chain(row);
  const int* rows = (chain >= 0) ? colonies_building_chain_rows(chain) : NULL;
  if (!rows) {
    return -1;
  }
  for (int t = 1; t < 4 && rows[t] >= 0; ++t) {
    if (rows[t] == row) {
      return rows[t - 1];
    }
  }
  return -1;
}

/* Non-lumber lootable warehouse cargo (STORES still preferred over BURN). */
/*
 * FUN_5fef_0f14 kind 3's victim pick (raw 99989-99997): `FUN_281f_07e0`
 * (unit_index_on_tile) on the raided colony's own tile, abort unless
 * `FUN_281f_088a` (stack_has_ship) says a ship is in that stack, then walk
 * down with `FUN_281f_02e4` until the unit's type byte is 0xd..0x12 (the six
 * ship types, docs/indian_alarm.md (ship → village)). DOS filters on type only — any hull in
 * the port is fair game. Returns the unit id, or −1 when the port is empty
 * (DOS's `goto LAB_5fef_123a`, which collapses the raid to kind 0).
 */
COLONIZE_INTERNAL int ai_contact_raid_port_ship(ColonizeTurnContext* ctx, const ColonizeColony* c) {
  if (!ctx || !ctx->units || !c) {
    return -1;
  }
  for (int i = 0; i < units_slot_end(ctx->units); ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!units_is_on_map(u) || u->x != c->x || u->y != c->y) {
      continue;
    }
    if (!units_is_sea(ctx->units, u->id)) {
      continue;
    }
    return u->id;
  }
  return -1;
}

/*
 * FUN_281f_09fc(n) — far thunk → FUN_15eb_038e, "test the building bit of the
 * active colony" (FUNCTION_CATALOG.md row 281f_09fc; tools/address_mapping.csv
 * 281f:09fc → FUN_1000_8bec). Its argument is a BUILDING ROW, not a cargo —
 * 0f14's kind-2 arm passes rows up to 0x29 to the same call. Rows 0/1/2 are
 * Stockade / Fort / Fortress, so 0f14's demote chain is fortification-gated.
 * (port_bugs.md #827 lead L3, settled 2026-09-23.)
 */
static int ai_contact_raid_09fc(
  const ColonizeTurnContext* ctx, const ColonizeColony* c, ColonizeBuildingRow row
) {
  if (!ctx || !ctx->colonies || !c) {
    return 0;
  }
  const int b = colonies_building_row(ctx->colonies, row);
  if (b < 0 || b >= COLONIZE_BUILDING_TYPES_MAX) {
    return 0;
  }
  return c->has_building[b] ? 1 : 0;
}

/*
 * DOS-LITERAL FUN_5fef_0f14 raw 99819-99833 — the kind-1 (goods) cargo pick.
 * A retry loop, NOT a value sort: roll a cargo 0..0xf, retry while the colony
 * holds less than 10 of it, give up after 100 tries (→ kind 0). Two literal
 * oddities are kept: on the FIRST try a horseless tribe steals horses instead
 * of a >0x34 pile on a coin flip, and a muskets roll burns a dummy rand(0,200)
 * that nothing reads (it only shifts the LCG for every later draw this turn).
 * Returns the cargo index, or -1 for "give up" (port_bugs.md #832).
 */
static int ai_contact_raid_roll_stores_cargo(
  ColonizeTurnContext* ctx, const ColonizeColony* c, int indian_nation, ColonizeDosRng* rng
) {
  const ColonizeCol1Indian* ind =
    (ctx && ctx->col1_ok && ctx->col1 && indian_nation >= 4 && indian_nation <= 11)
      ? &ctx->col1->indian[indian_nation - 4]
      : NULL;
  int i = 0;
  int cargo = 0;
  do {
    i = i + 1;
    cargo = dos_rng_range(rng, 0, 0xf);
    /* DOS tests the rolled cargo's pile, not the horses (dos_bugs.md DB-49). */
    if (ind && ind->horse_herds == 0 && i == 1 && c->stock[COLONIZE_CARGO_HORSES] > 0x34 &&
        dos_rng_range(rng, 0, 1) == 0) {
      cargo = COLONIZE_CARGO_HORSES;
    }
    /* DOS bug not reproduced (docs/dos_bugs.md DB-29): DOS burns a dummy
     * rand(0,200) here for Muskets (raw 99826-99829; asm 5fef:1119-1135
     * computes rand - difficulty*i into CX and never reads it). */
  } while (i < 100 && c->stock[cargo] < 10);
  if (99 < i) {
    return -1; /* raw 99833 `goto LAB_5fef_123a` */
  }
  return cargo;
}

/*
 * DOS-LITERAL FUN_5fef_0f14 head, raw 99777-99893: the walls check, the
 * `rand(1,4)` kind roll, the five-line demote chain, and each kind's own
 * target validation (which can still collapse the raid to kind 0).
 *
 * Kind space is DOS's `local_6`: 0 nothing, 1 goods, 2 building, 3 ship,
 * 4 gold — AiRaidKind carries exactly those values. There is no fifth kind:
 * DOS never kills a colonist here and never wrecks food/tools/construction
 * as a kind of its own (port_bugs.md #828 / #829).
 *
 * `forced` = 0f14's param_4 (FUN_5fef_1b0e's repelled-at-a-colony handoff):
 * it bypasses only the walls VERDICT — DOS still draws the walls roll
 * (raw 99775/99786), so the LCG stays in step.
 *
 * This replaces an OpenCol stand-in that picked by alarm band (>=85/70/60/55/50)
 * with a rand(0,99) and then demoted on YEAR thresholds 1500/1520 that exist
 * nowhere in the decomp (port_bugs.md #827). The alarm scalar has no role at this
 * head in DOS at all.
 */
COLONIZE_INTERNAL AiRaidKind ai_contact_pick_raid_kind(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int indian_nation,
  int target_euro,
  ColonizeDosRng* rng,
  int forced
) {
  ai_contact_s_raid_cargo = -1;
  ai_contact_s_raid_burn_row = -1;
  ai_contact_s_raid_gold = 0;
  if (!ctx || !c || !rng) {
    return AI_RAID_NOTHING;
  }
  const int difficulty =
    (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.difficulty : 0;
  /* raw 99776/99793: `uVar5 < 4 && control[uVar5] == 0` = victim is the human. */
  const int victim_human = ai_contact_euro_is_human(ctx, target_euro) ? 1 : 0;

  /*
   * Walls head (raw 99777-99783): walls = FUN_281f_0ab0(0) = owned buildings
   * along the Stockade → Fort → Fortress chain (0..3); r = rand(0,12) - 1,
   * plus difficulty-2 when the victim is human; r < walls*3 + 1 → kind 0
   * (@RAIDNOTHING "raiding party wiped out"). Bare colony 1/13; Stockade
   * 4/13; Fort 7/13; Fortress 10/13 before the difficulty shift.
   */
  {
    int walls = 0;
    const int* k_chain = colonies_building_chain_rows(COLONIES_CHAIN_FORTIFICATION);
    for (int i = 0; i < 3 && k_chain && k_chain[i] >= 0; ++i) {
      if (ai_contact_raid_09fc(ctx, c, (ColonizeBuildingRow)k_chain[i])) {
        walls++;
      }
    }
    dos_rng_reseed_83a6(rng); /* raw 99773: FUN_281f_04ca(DS:0x83a6) */
    int defense_roll = dos_rng_range(rng, 0, 12) - 1;
    if (victim_human) {
      defense_roll += difficulty - 2;
    }
    if (defense_roll < walls * 3 + 1 && !forced) {
      return AI_RAID_NOTHING;
    }
  }

  AiRaidKind kind = (AiRaidKind)dos_rng_range(rng, 1, 4); /* raw 99790 */

  /*
   * raw 99791-99795: on Discoverer/Explorer, before turn (difficulty-2)*-0x28
   * (turn 40 / 80), the building and ship kinds collapse to nothing. DS:0x538e
   * is the turn counter (the 1b0e handoff passes the save's turn).
   */
  if (ctx->turn_number) {
    const int early_raid_cutoff_turn = (difficulty - 2) * -0x28;
    const int turn = (int)*ctx->turn_number;
    if (turn <= early_raid_cutoff_turn && turn != early_raid_cutoff_turn && difficulty < 2 &&
        (kind == AI_RAID_BURN || kind == AI_RAID_SHIP)) {
      kind = AI_RAID_NOTHING;
    }
  }
  /* raw 99796-99807 */
  if (kind == AI_RAID_BURN) {
    const int burn_difficulty_shift = victim_human ? difficulty : 1;
    if (burn_difficulty_shift + 2 < dos_rng_range(rng, 0, 8)) {
      kind = AI_RAID_STORES;
    }
    if (ai_contact_raid_09fc(ctx, c, COLONY_BUILDING_FORT)) {
      kind = AI_RAID_STORES;
    }
  }
  /* raw 99808-99810 */
  if (kind == AI_RAID_GOLD && ai_contact_raid_09fc(ctx, c, COLONY_BUILDING_STOCKADE)) {
    kind = AI_RAID_STORES;
  }
  /* raw 99811-99813 */
  if (kind == AI_RAID_SHIP && ai_contact_raid_09fc(ctx, c, COLONY_BUILDING_FORTRESS)) {
    kind = AI_RAID_NOTHING;
  }
  /* raw 99814-99818 (the rand is drawn only when the Stockade test passes) */
  if (kind == AI_RAID_STORES && ai_contact_raid_09fc(ctx, c, COLONY_BUILDING_STOCKADE) &&
      difficulty < dos_rng_range(rng, 0, 8)) {
    kind = AI_RAID_NOTHING;
  }

  if (kind == AI_RAID_STORES) {
    ai_contact_s_raid_cargo =
      ai_contact_raid_roll_stores_cargo(ctx, c, indian_nation, rng);
    if (ai_contact_s_raid_cargo < 0) {
      kind = AI_RAID_NOTHING;
    }
  } else if (kind == AI_RAID_BURN) {
    /*
     * DOS-LITERAL FUN_5fef_0f14 kind 2, raw 99832-99863 — the burn target is
     * rolled HERE (it costs RNG draws), not in the apply arm:
     *   proj_is_building = FUN_281f_0cc2(colony+0x94, 0) == 1   (raw 99834)
     *   do { tries++;
     *        row = rand(0, 0x29);
     *        ok = FUN_281f_0a88(row) != 9 && row != 0x23;   (no Town Hall chain,
     *                                                        no Carpenter's Shop)
     *        if (proj_is_building && 0a88(row) == 0a88(project)) ok = false;
     *        if (row in {0x27,0x15,0x18,0x1b,0,1,2,0x20}) ok = false;
     *   } while (tries < 100 && (!FUN_281f_09fc(row) || !ok));
     *   if (tries > 99 || !ok) -> kind 0
     * then walk the predecessor column down to the lowest tier the colony
     * still owns (raw 99856-99863). Church 0x25 / Cathedral 0x26 are legal
     * targets and a Cathedral walks down to the Church it sits on, which is
     * what the port's "first built non-Town-Hall building" stand-in got wrong
     * (port_bugs.md #924).
     */
    const int proj = c->building_in_production;
    const int proj_is_building = (proj >= 0 && proj < 0x2a) ? 1 : 0;
    int tries = 0;
    int row = -1;
    bool ok = false;
    do {
      tries++;
      row = dos_rng_range(rng, 0, 0x29);
      ok = (ai_contact_raid_0a88(row) != COLONY_BUILDING_TOWN_HALL &&
            row != COLONY_BUILDING_CARPENTERS_SHOP);
      if (proj_is_building && ai_contact_raid_0a88(row) == ai_contact_raid_0a88(proj)) {
        ok = false;
      }
      if (row == COLONY_BUILDING_BLACKSMITHS_HOUSE || row == COLONY_BUILDING_WEAVERS_HOUSE ||
          row == COLONY_BUILDING_TOBACCONISTS_HOUSE ||
          row == COLONY_BUILDING_RUM_DISTILLERS_HOUSE || row == COLONY_BUILDING_STOCKADE ||
          row == COLONY_BUILDING_FORT || row == COLONY_BUILDING_FORTRESS ||
          row == COLONY_BUILDING_FUR_TRADERS_HOUSE) {
        ok = false;
      }
    } while (tries < 100 &&
             (!ai_contact_raid_09fc(ctx, c, (ColonizeBuildingRow)row) || !ok));
    if (tries > 99 || !ok) {
      kind = AI_RAID_NOTHING;
    } else {
      /* raw 99856-99863: down to the lowest owned tier of this chain. */
      for (;;) {
        const int pred = ai_contact_raid_pred_row(row);
        if (pred >= 0 && ai_contact_raid_09fc(ctx, c, (ColonizeBuildingRow)pred)) {
          row = pred;
          continue;
        }
        break;
      }
      /* dos_bugs.md DB-50: the walk down burns a Church under its Cathedral.
       * The port burns the top owned tier instead. Warehouse and Capitol keep
       * the base row: their apply arm takes one level off the counter. */
      const int chain = colonies_building_row_chain(row);
      const int* rows = colonies_building_chain_rows(chain);
      if (rows && chain != COLONIES_CHAIN_WAREHOUSE && chain != COLONIES_CHAIN_CAPITOL) {
        for (int t = 0; t + 1 < 4 && rows[t + 1] >= 0; ++t) {
          if (rows[t] == row && ai_contact_raid_09fc(ctx, c, (ColonizeBuildingRow)rows[t + 1])) {
            row = rows[t + 1];
          }
        }
      }
      ai_contact_s_raid_burn_row = row;
    }
  } else if (kind == AI_RAID_SHIP) {
    /* raw 99876-99877: FUN_281f_088a (stack_has_ship) fails → kind 0. */
    if (ai_contact_raid_port_ship(ctx, c) < 0) {
      kind = AI_RAID_NOTHING;
    }
  } else if (kind == AI_RAID_GOLD) {
    /*
     * DOS-LITERAL raw 99876-99893 — the plunder amount is rolled HERE, and a
     * roll the victim cannot pay (or below the 0x32 floor) collapses the raid
     * to kind 0 with no vent:
     *   cap = (gold * colony_population) / (census_pop_proxy[euro] + 1) + 10,
     *         saturated at 0x7fff                (FUN_1d1d_0f60 / _0ec6)
     *   amt = rand(0x32, cap)
     *   if (gold < amt || amt < 0x32) -> kind 0
     * DS:0x9410 = stuff.census_pop_proxy (resolved in col1_save_layout.h),
     * colony +0x1f = population.
     */
    long amt = 0;
    long gold = 0;
    if (ctx->col1_ok && ctx->col1 && target_euro >= 0 && target_euro < 4) {
      gold = (long)europe_nation_gold(ctx->europe, ctx->col1, target_euro);
      const int div = (int)ctx->col1->stuff.census_pop_proxy[target_euro] + 1;
      long cap = gold * (long)c->population;
      cap = cap / (div > 0 ? div : 1) + 10;
      if (cap > 0x7fff) {
        cap = 0x7fff;
      }
      amt = dos_rng_range(rng, 0x32, (int)cap);
    }
    if (gold < amt || amt < 0x32) {
      kind = AI_RAID_NOTHING;
    } else {
      ai_contact_s_raid_gold = amt;
    }
  }
  return kind;
}

/*
 * DOS-LITERAL FUN_5fef_0f14 raw 99895-99899 — the third-party bulletin:
 *
 *   uVar6 = FUN_281f_09a4(uVar5); FUN_281f_0438(3, uVar6);   // %STRING3 = victim nation
 *   if (((3 < uVar5) || (control[uVar5] != 0)) && local_6 != 0)
 *     FUN_281f_0652(0x1b8a, 3);                              // @RAIDWREAK
 *
 * i.e. @RAIDWREAK "Spies report: {%STRING0} raiding party wreaks havoc in the
 * {%STRING3} colony of {%STRING1}." fires once per SUCCESSFUL raid (any kind
 * 1..4) whose victim colony is NOT human-controlled — it is the human's news
 * bulletin about someone else's misfortune, and it loots nothing. The port
 * used to model 0x1b8a as a loot kind that drained food/tools and cancelled
 * construction (port_bugs.md #829).
 */
static void ai_contact_raid_wreak_bulletin(
  ColonizeTurnContext* ctx, const ColonizeColony* c, int indian_nation,
  int target_euro, AiRaidKind kind
) {
  if (!ctx || !c || kind == AI_RAID_NOTHING) {
    return;
  }
  if (target_euro >= 0 && target_euro <= 3 && ai_contact_euro_is_human(ctx, target_euro)) {
    return; /* the human's own colony takes the per-kind @RAID* tag instead */
  }
  int human = -1;
  for (int e = 0; e < 4; ++e) {
    if (ai_contact_euro_is_human(ctx, e)) {
      human = e;
      break;
    }
  }
  if (human < 0) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_tribe_name(indian_nation);
  tok.string1 = c->name[0] ? c->name : "";
  /* FUN_5fef_0f14 raw 99896: %STRING3 = FUN_281f_09a4(colony owner), the
   * @NATIONALITY adjective ("Tory" under the WoI), not the country name. */
  tok.string3 =
    (target_euro >= 0 && target_euro <= 3) ? reports_nation_adjective_woi(ctx->col1, target_euro) : "";
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "RAIDWREAK", &tok, "", body, sizeof(body));
  if (body[0]) {
    ai_contact_human_chrome(
      ctx, human, AI_POPUP_TAG_CONTACT_RAID, indian_nation, body
    );
  }
}

void ai_contact_apply_raid_loot(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int indian_nation,
  int target_euro,
  AiRaidKind kind
) {
  if (!c) {
    return;
  }
  ai_contact_s_last_raid_kind = (int)kind;
  ai_contact_s_last_burn_building[0] = '\0';
  ai_contact_s_last_stores_cargo[0] = '\0';
  ai_contact_s_last_ship_type[0] = '\0';
  ai_contact_s_last_gold_drained = 0;

  switch (kind) {
  case AI_RAID_NOTHING:
    break;
  case AI_RAID_STORES: {
    /*
     * DOS-LITERAL FUN_5fef_0f14 raw 99913-99947. The cargo is the one the
     * picker's retry loop rolled (DOS `local_20`); the amount is a ROLL over
     * the upper half of the pile, not the flat min(half,10) the port used to
     * take — a 200-stock warehouse loses 10..100 (port_bugs.md #830):
     *   h = stock >> 1; lo = min(h, 10); amt = rand(lo, h);
     *   amt = min(amt, stock); if (amt < 1) amt = 1;
     * and the stolen horses/muskets then feed the RAIDING TRIBE's record
     * (raw 99939-99947, port_bugs.md #831) — the same fields 0352's gear return
     * writes.
     */
    const int cargo = ai_contact_s_raid_cargo;
    if (cargo >= 0 && cargo < COLONIZE_CARGO_COUNT && c->stock[cargo] > 0) {
      const int h = c->stock[cargo] >> 1;
      const int lo = (h > 10) ? 10 : h;
      int amt = dos_rng_range(ctx ? ctx->rng : NULL, lo, h);
      if (amt > c->stock[cargo]) {
        amt = c->stock[cargo];
      }
      if (amt < 1) {
        amt = 1;
      }
      c->stock[cargo] -= amt;
      snprintf(ai_contact_s_last_stores_cargo, sizeof(ai_contact_s_last_stores_cargo), "%s", ai_contact_cargo_name(cargo));
      ColonizeCol1Indian* ind =
        (ctx && ctx->col1_ok && ctx->col1 && indian_nation >= 4 && indian_nation <= 11)
          ? &ctx->col1->indian[indian_nation - 4]
          : NULL;
      if (getenv("AI_RAID_TRACE")) {
        fprintf(stderr, "[raid-stores] colony %d by nation %d cargo %d amt %d bound %d\n",
                c->id, indian_nation, cargo, amt, g_colonies_bound_tribe);
      }
      if (ind && cargo == COLONIZE_CARGO_HORSES) {
        ind->horse_herds = (uint8_t)(ind->horse_herds + 1);
        ind->horse_breeding = (uint16_t)(ind->horse_breeding + 0x19);
      }
      if (ind && cargo == COLONIZE_CARGO_MUSKETS) {
        ind->muskets = (uint8_t)(ind->muskets + 1);
        if (amt > 0x31) {
          ind->muskets = (uint8_t)(ind->muskets + 1);
        }
      }
    }
    break;
  }
  case AI_RAID_BURN: {
    /*
     * DOS-LITERAL FUN_5fef_0f14 kind 2 tail, raw 99951-99985. The target row
     * was rolled by the picker (`local_a`); this arm only applies it. DOS has
     * NO "clear the current project" and NO "eat lumber" step here (that is
     * the kind-1 goods arm, raw 99908-99947) — both were invented and are
     * deleted (port_bugs.md #924).
     *   row 0x0f (Warehouse): colony+0x95 (warehouse_level) DEC; the building
     *     bit only clears when the counter reaches 0, otherwise the popup
     *     names the Warehouse Expansion (DS:0x9042).
     *   row 0x1e (Capitol): colony+0x96 (capitol_level) DEC, bit cleared when
     *     it reaches 0; the popup always names the Capitol Expansion
     *     (DS:0x90f6) — literal DOS oddity.
     *   everything else: FUN_281f_0bbe(row, 0) clears the bit, and the
     *     workplace colonists are evicted (colonies_destroy_building already
     *     does the FUN_281f_0c36(..., 0xd) reassignment).
     */
    const int row = ai_contact_s_raid_burn_row;
    if (!ctx || !ctx->colonies || row < 0) {
      break;
    }
    int destroy_row = -1;
    int name_row = row;
    if (row == COLONY_BUILDING_WAREHOUSE) {
      if (c->warehouse_level > 0) {
        c->warehouse_level = (uint8_t)(c->warehouse_level - 1);
      }
      if (c->warehouse_level == 0) {
        destroy_row = COLONY_BUILDING_WAREHOUSE;
      } else {
        name_row = COLONY_BUILDING_WAREHOUSE_EXPANSION;
      }
    } else if (row == COLONY_BUILDING_CAPITOL) {
      if (c->capitol_level > 0) {
        c->capitol_level = (uint8_t)(c->capitol_level - 1);
      }
      if (c->capitol_level == 0) {
        destroy_row = COLONY_BUILDING_CAPITOL;
      }
      name_row = COLONY_BUILDING_CAPITOL_EXPANSION;
    } else {
      destroy_row = row;
    }
    if (destroy_row >= 0) {
      const int bt_index = colonies_building_row(ctx->colonies, (ColonizeBuildingRow)destroy_row);
      if (bt_index >= 0) {
        (void)colonies_destroy_building(ctx->colonies, c->id, bt_index);
      }
    }
    {
      const char* nm = colonies_building_row_name(name_row);
      if (nm && nm[0]) {
        snprintf(
          ai_contact_s_last_burn_building, sizeof(ai_contact_s_last_burn_building), "%s", nm
        );
      }
    }
    break;
  }
  case AI_RAID_GOLD:
    if (ctx && ctx->col1_ok && ctx->col1 && target_euro >= 0 && target_euro < 4 &&
        ai_contact_s_raid_gold > 0) {
      /*
       * DOS-LITERAL FUN_5fef_0f14 raw 100020-100024: the amount was rolled and
       * affordability-checked in the head (see the picker); this arm only
       * subtracts it from the victim's 32-bit purse and stashes it at
       * DS:0x9cb0 for the @RAIDGOLD %NUMBER0 slot.
       */
      europe_nation_gold_add(
        ctx->europe, ctx->col1, target_euro, -(long)ai_contact_s_raid_gold
      );
      ai_contact_s_last_gold_drained = (int)ai_contact_s_raid_gold;
    }
    break;
  case AI_RAID_SHIP: {
    /*
     * FUN_5fef_0f14 kind 3 (raw 99989-100004): the victim is a ship standing
     * ON the colony tile — `FUN_281f_07e0` (unit_index_on_tile of the colony)
     * then the `FUN_281f_02e4` stack walk to the first type byte in 0xd..0x12
     * (the six ship types); no nation filter, so a visiting foreign hull can
     * take it. It is handed to FUN_5fef_0352 with `param_2 = 0xffff` (no
     * winner), which always damages it: holds and passengers lost, damaged
     * bit7, repair timer, relocation to the nearest own repair port
     * (`units_raid_damage_ship`). The port used to only zero the ship's MP
     * and dump one cargo ton for a ship anywhere within 2 tiles — nearly no
     * damage for 0f14's largest (−16) alarm vent.
     */
    const int ship_id = ai_contact_raid_port_ship(ctx, c);
    if (ship_id >= 0) {
      ColonizeUnit* u = units_get(ctx->units, ship_id);
      if (u) {
        snprintf(
          ai_contact_s_last_ship_type, sizeof(ai_contact_s_last_ship_type), "%s", units_display_name(ctx->units, u)
        );
      }
      (void)units_raid_damage_ship(ctx->units, ship_id, ctx->col1_ok ? ctx->col1 : NULL);
    }
    break;
  }
  default:
    break;
  }

  /*
   * (Retired 2026-09-23, port_bugs.md #833.) `ai_contact_raid_secondary_loot` ran
   * after every non-NOTHING kind and drained −5 muskets / −1 horse from the
   * warehouse or from a unit's gear on the tile, plus −1 tools at alarm >= 80.
   * FUN_5fef_0f14 mutates exactly ONE thing per raid — one cargo, one
   * building, one ship or the treasury — and never touches a unit's gear.
   */
}

/*
 * FUN_5fef_0f14's alarm tail (raw viceroy_unpacked.c:99944-100033) — ported
 * 2026-09-08, replacing the fandom-derived POSITIVE "raids raise tension"
 * bump the raid pulse used to apply here (same retirement as the three
 * fandom alarm drips, port_bugs.md #289: DOS grows Indian alarm only through the
 * FUN_4d56_152e accumulator).
 *
 * Each of 0f14's four loot arms ends with the SAME two lines — a war gate
 * and one `uVar6` constant — and then falls into the shared call at 100033:
 *
 *     uVar10 = FUN_281f_0a38(0x281f, *(undefined2 *)0x8d50, uVar5);
 *     if ((uVar10 & 2) != 0) goto LAB_5fef_16d2;
 *     uVar6 = 0xfffc;                                  <- per-kind constant
 *   ...
 *   FUN_281f_0d6c(0x281f, param_1 + -4, uVar5, uVar6, 0);
 *   LAB_5fef_16d2:
 *     *(undefined2 *)((param_3 * 9 + uVar5) * 2 + 0x54f6) = 0;
 *
 * so the real (non-segment) argument list is `0d6c(indian_nation, euro,
 * delta, 0)`, and every delta is NEGATIVE:
 *
 *   local_6 == 1  goods stolen      0xfffc = −4    (raw 99961)
 *   local_6 == 2  building razed    0xfff4 = −12   (raw 99992)
 *   local_6 == 3  unit killed       0xfff0 = −16   (raw 100006)
 *   local_6 == 4  gold plundered    0xfff8 = −8    (raw 100031)
 *   local_6 == 0  nothing looted    no call at all — the kind-0 arm jumps
 *                                   straight to LAB_5fef_16d2 (raw 100013)
 *
 * A successful raid therefore DISCHARGES the tribe's alarm toward that
 * European, exactly like the DS:0x54f6 tension word the next line zeroes
 * unconditionally, and like 1b0e's `local_a6` vent
 * (`units_indian_attack_alarm_vent`, units.c) which uses the same writer
 * and the same gate.
 *
 * Gate: `FUN_281f_0a38` = `FUN_15b3_0004(indian_nation, euro)` = the Indian
 * record's relation byte toward that European (`indian[].euro_diplo[euro]`),
 * whose bit 1 is the WAR bit — and the branch SKIPS the 0d6c call, so the
 * discharge only happens while the tribe is NOT already at war. No relief
 * once war is declared.
 *
 * AiRaidKind now carries DOS's own `local_6` values, so this is a straight
 * 1:1 table (the port's invented SCALP/WREAK kinds, which used to share the
 * −16 and −12 rows, were deleted 2026-09-23: port_bugs.md #828/#829).
 */
COLONIZE_INTERNAL int ai_contact_raid_alarm_delta(AiRaidKind kind) {
  switch (kind) {
    case AI_RAID_STORES:
      return -4;
    case AI_RAID_BURN:
      return -12;
    case AI_RAID_SHIP:
      return -16;
    case AI_RAID_GOLD:
      return -8;
    default:
      return 0; /* AI_RAID_NOTHING: DOS makes no 0d6c call at all. */
  }
}

COLONIZE_INTERNAL void ai_contact_raid_alarm_tail(
  ColonizeTurnContext* ctx, int indian_nation, int euro, AiRaidKind kind
) {
  if (!ctx || !ctx->col1 || indian_nation < 4 || indian_nation > 11 || euro < 0 || euro > 3) {
    return;
  }
  const int delta = ai_contact_raid_alarm_delta(kind);
  if (delta == 0) {
    return;
  }
  /* FUN_281f_0a38(DS:0x8d50, euro) & 2 — already at war, no discharge. */
  if ((ctx->col1->indian[indian_nation - 4].euro_diplo[euro] & COL1_INDIAN_WAR_BIT) != 0) {
    return;
  }
  /* FUN_281f_0d6c = FUN_4cc6_00f2 = ai_diplo_indian_alarm_delta (+ escalation
   * tail, which a negative delta can never reach). Halving (France /
   * Pocahontas) is positive-delta-only and lives inside that writer. */
  ai_contact_alarm_delta_00f2(ctx, indian_nation, euro, delta);
}

/*
 * Self-register the 1b0e handoff with units.c at load time (units.h
 * ColonizeUnitsRaidRepelledFn). Keeps units.c free of a link-time
 * dependency on this module: binaries that don't link ai_contact.c fall
 * back to the plain pre-port death.
 */
__attribute__((constructor)) static void ai_contact_register_raid_repelled(void) {
  units_set_colony_raid_repelled(ai_contact_colony_raid_repelled_w);
}

static void ai_contact_raid_human_chrome(
  ColonizeTurnContext* ctx, const ColonizeColony* c, int nation_id, int target_euro,
  AiRaidKind kind
);

int ai_contact_colony_raid_repelled_w(
  const ColonizeWorld* w,
  int indian_nation,
  int euro_nation,
  int colony_id,
  int home_tribe_index,
  int forced,
  int human_nation
) {
  ColonizeCol1Save* col1 = w->col1;
  ColonizeColonyPool* colonies = w->colonies;
  ColonizeUnitPool* units = w->units;
  ColonizeWorldMap* map = w->map;
  ColonizeDosRng* rng = w->rng;

  if (!col1 || !colonies || indian_nation < 4 || indian_nation > 11 || euro_nation < 0 ||
      euro_nation > 3) {
    return AI_RAID_NOTHING;
  }
  ColonizeColony* c = colonies_get_mut(colonies, colony_id);
  if (!c || !c->active || c->nation_id != euro_nation) {
    return AI_RAID_NOTHING;
  }
  /*
   * DOS reaches 0f14 here through the shared globals the combat resolver
   * already bound (FUN_281f_0a42 nation / FUN_281f_09e6 colony), so the only
   * context 0f14 itself needs is the colony, the pools, the LCG and the turn
   * word, plus the popup queue / catalog / human nation for its human-victim
   * chrome and @RAIDWREAK bulletin.
   */
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = units;
  ctx.colonies = colonies;
  ctx.map = map;
  ctx.col1 = col1;
  ctx.col1_ok = true;
  ctx.rng = rng;
  ctx.europe = w->europe;
  ctx.ai_popups = w->ai_popups;
  ctx.messages = w->messages;
  ctx.human_nation = human_nation;
  /* 0f14's early-game grace reads DS:0x538e on this path too (raw 99791). */
  uint32_t turn = col1->head.turn;
  ctx.turn_number = &turn;

  /* The village-friction maximum this used to build here was dead on arrival:
   * ai_contact_pick_raid_kind takes no alarm argument and derives its own.
   * Deleted with the rest of the cross-village coupling (port_bugs.md #1003). */
  const AiRaidKind kind =
    ai_contact_pick_raid_kind(&ctx, c, indian_nation, euro_nation, rng, forced);
  ai_contact_apply_raid_loot(&ctx, c, indian_nation, euro_nation, kind);
  ai_contact_raid_wreak_bulletin(&ctx, c, indian_nation, euro_nation, kind);
  ai_contact_raid_human_chrome(&ctx, c, indian_nation, euro_nation, kind);

  /*
   * FUN_5fef_0f14's alarm tail (raw 100033), wired 2026-09-08 — negative
   * per-kind delta behind the war gate. DOS runs it just BEFORE the
   * DS:0x54f6 clear below, so keep this ordering.
   */
  ai_contact_raid_alarm_tail(&ctx, indian_nation, euro_nation, kind);

  /*
   * FUN_5fef_0f14's tail (raw 100034) — the unconditional DS:0x54f6 discharge
   * that 1b0e's own site 1 deliberately skips on this limb (docs/indians.md).
   * Fires for every kind, "Nothing" included.
   */
  if (col1->tribe && home_tribe_index >= 0 &&
      (uint16_t)home_tribe_index < col1->head.tribe_count) {
    /* DOS zeroes the WHOLE word, i.e. both friction and attacks. */
    col1_tribe_attitude_set(&col1->tribe[home_tribe_index], euro_nation, 0);
  }
  return (int)kind;
}

/*
 * Per-kind raid chrome for a HUMAN victim (audit AC-27). DOS fires the
 * per-kind tag (0x1b94 @RAIDSTORES / 0x1b9f @RAIDBURN / 0x1ba8 @RAIDSHIP /
 * 0x1bb1 @RAIDGOLD / 0x1bba @RAIDNOTHING, raw 99909-100020). `section` is the
 * only wording; a miss draws nothing (port_bugs.md #836).
 */
static const AiRaidChrome k_raid_chrome[] = {
  /* @RAIDNOTHING; sound 0x5b = raid repelled (gunfight). */
  {AI_RAID_NOTHING, "RAIDNOTHING", "", 0x5b, -1, AI_RAID_TOK_NONE},
  /* @RAIDSHIP; DOS plays both 0x4b and 0x4d, in that order. */
  {AI_RAID_SHIP, "RAIDSHIP", "", 0x4b, 0x4d, AI_RAID_TOK_SHIP},
  /* @RAIDGOLD; 0x4e = treasury gold stolen. */
  {AI_RAID_GOLD, "RAIDGOLD", "", 0x4e, -1, AI_RAID_TOK_GOLD},
  /* @RAIDSTORES; 0x4f = loot goods. */
  {AI_RAID_STORES, "RAIDSTORES", "", 0x4f, -1,
   AI_RAID_TOK_STORES}
  /* @RAIDBURN thin/named rows live below (they depend on the burned building). */
};

/* @RAIDBURN, with the destroyed building named. */
static const AiRaidChrome k_raid_chrome_burn_named = {
  AI_RAID_BURN, "RAIDBURN", "", 0x53, -1, AI_RAID_TOK_BURN
};
/* Burn with no named building: cues only. */
static const AiRaidChrome k_raid_chrome_burn_thin = {
  AI_RAID_BURN, NULL, NULL, 0x53, -1, AI_RAID_TOK_NONE
};
/* Generic successful raid chrome when no kind-specific line applies. */
static const AiRaidChrome k_raid_chrome_generic = {
  AI_RAID_NOTHING, NULL, NULL, -1, -1, AI_RAID_TOK_NONE
};

COLONIZE_INTERNAL const AiRaidChrome* ai_contact_raid_chrome_row(AiRaidKind kind, int have_burn_building) {
  if (kind == AI_RAID_BURN) {
    return have_burn_building ? &k_raid_chrome_burn_named : &k_raid_chrome_burn_thin;
  }
  for (size_t i = 0; i < sizeof(k_raid_chrome) / sizeof(k_raid_chrome[0]); ++i) {
    if (k_raid_chrome[i].kind == kind) {
      return &k_raid_chrome[i];
    }
  }
  return &k_raid_chrome_generic;
}

/*
 * DOS-LITERAL FUN_5fef_0f14 human-victim chrome (raw 99901-100020): the
 * colony-tune / 0x32 cue, then the per-kind cue(s) and the per-kind @RAID*
 * dialog, all gated on `uVar5 < 4 && control[uVar5] == 0`. Reached from the
 * 1b0e loser limb (ai_contact_colony_raid_repelled_w). The woodcut is 1b0e's
 * own (5fef:22a9, units_combat_resolve.c), not 0f14's.
 *
 * No @INDIANSURPRISE prefix: its DOS site is the 021a human-colony warning
 * (OVL13 thunk_FUN_1000_a5c4, overlays.c 76958-76970), listed under
 * "Not ported" in ai_native_021a.c.
 */
static void ai_contact_raid_human_chrome(
  ColonizeTurnContext* ctx, const ColonizeColony* c, int nation_id, int target_euro,
  AiRaidKind kind
) {
  if (!ai_contact_euro_is_human(ctx, target_euro)) {
    return;
  }
  char raid_line[AI_POPUP_BODY_LEN];
  PopupMsgTokens raid_tok;
  memset(&raid_tok, 0, sizeof(raid_tok));
  raid_tok.string0 = ai_contact_tribe_name(nation_id);
  raid_tok.string1 = c->name[0] ? c->name : NULL;
  const AiRaidChrome* row =
    ai_contact_raid_chrome_row(kind, ai_contact_s_last_burn_building[0] != '\0');
  /* raw 99901-99908: kind 0 restores the colony tune pool through
   * FUN_281f_0498(2); every loot kind queues track 0x32 through FUN_281f_048e. */
  if (kind == AI_RAID_NOTHING) {
    sound_set_bgm(2);
  } else {
    sound_play(0x32);
  }
  switch (row->tok) {
    case AI_RAID_TOK_SHIP:
      raid_tok.string2 = ai_contact_s_last_ship_type;
      break;
    case AI_RAID_TOK_STORES:
      raid_tok.string2 = ai_contact_s_last_stores_cargo;
      break;
    case AI_RAID_TOK_BURN:
      raid_tok.string2 = ai_contact_s_last_burn_building;
      break;
    case AI_RAID_TOK_GOLD:
      raid_tok.number0 = ai_contact_s_last_gold_drained;
      raid_tok.has_number0 = true;
      break;
    case AI_RAID_TOK_NONE:
    default:
      break;
  }
  /*
   * DOS keys these cues only to the resolved raid kind and a human victim
   * (raw asm 154640, 154763, 154801-154803, 154847, 154869). Popup text
   * availability is unrelated; the ship arm is a two-cue sequence, 0x4b
   * then 0x4d.
   */
  if (row->sound >= 0) {
    sound_play(row->sound);
  }
  if (row->sound2 >= 0) {
    sound_play(row->sound2);
  }
  raid_line[0] = '\0'; /* #836: no typed fallback */
  if (row->section) {
    popup_msg_fill(
      ctx->messages, row->section, &raid_tok, row->popup_fallback,
      raid_line, sizeof(raid_line)
    );
  }
  ai_contact_human_chrome(
    ctx, target_euro, AI_POPUP_TAG_CONTACT_RAID, nation_id,
    raid_line[0] ? raid_line : NULL
  );
}
