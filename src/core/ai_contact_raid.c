/*
 * Indian contact — loot scoring, raid execution, scout displacement & colony war tick
 *
 * Split out of ai_contact.c (2026-09-23) verbatim; shared symbols are
 * declared in ai_contact_internal.h. See ai_contact.c for the module
 * prologue and the DOS provenance notes.
 *
 * Sections:
 *   Colony stores/loot scoring, raid-kind selection & raid execution helpers
 *   Raid execution, colony-tile scout displacement, field/war census & colony war-tick
 */

#include "core/internal.h"
#include "core/ai_contact.h"
#include "core/ai_contact_internal.h"

#include "core/ai.h"
#include "core/ai_diplo.h"
#include "core/sound.h"
#include "core/woodcut.h"
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

/* ===================== Colony stores/loot scoring, raid-kind selection & raid execution helpers (ai_contact_indian_meet_trade .. ai_contact_raid_chrome_row) ===================== */


void ai_contact_indian_meet_trade(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ai_contact_bind_names(ctx);
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];

  /*
   * FUN_5bfb_022e checklist (Brave×Euro land adjacency):
   *  1) unmet → @INDIANWELCOME (human) / auto-accept (AI); then stop
   *  2) Missionary convert + teach-skill (after loop; tribe adjacency)
   *  3–4) AI-Euro only: silent auto-trade / gift-demand stand-in
   *
   * Ships never contact (DOS ocean_or_high_seas gate on move-meet; natives
   * do not hail vessels). Human gift/refuse/trade chrome is NOT fired from
   * this pulse — original player dialogs are village enter / 2820 (PARKED).
   * Cite: FUN_5bfb_022e first-contact exit; indian_contact.md §0.
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int nx = brave->x + MAP_DIR8_DX[d];
      const int ny = brave->y + MAP_DIR8_DY[d];
      const int oid = units_id_at(ctx->units, nx, ny);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id < 0 || other->nation_id > 3) {
        continue;
      }
      /* Landfall only — no ship contact. */
      if (units_is_sea(ctx->units, other->id)) {
        continue;
      }
      const int e = other->nation_id;
      const int human = ai_contact_euro_is_human(ctx, e);

      /* 1. First meet → FUN_5bfb_022e @INDIANWELCOME (not Trade/Gift menu). */
      if (ai_native_first_contact_this_turn(nation_id, e)) {
        continue; /* first contact already fired on the Brave's own step */
      }
      if (!ind->euro_diplo[e]) {
        (void)ai_contact_try_first_welcome(ctx, e, nation_id);
        if (ctx->col1->tribe) {
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id != nation_id ||
                map_chebyshev(t->x, t->y, brave->x, brave->y) > 3) {
              continue;
            }
            /* Peaceful meet: slight friction decay on tribe alarm. */
            if (ind->alarm_by_player[e] < 40 && t->alarm[e].friction > 0 &&
                t->alarm[e].friction < 40) {
              t->alarm[e].friction--;
            }
            /* bugs.md: first contact never gifts a mission — a mission exists
             * only where a Missionary was actually sent (@ACTIONS row 3). */
            break;
          }
        }
        /*
         * FUN_5bfb_022e tail (LAB_5bfb_1005): a first-contact ceremony ends
         * the Indian-side unit's turn — spent := max MP (0934 → 1427_155e;
         * natives keep the DOS spent byte in moves). This is the writer
         * behind the seed-100 TURN2→3 "spent 9/6 → 3" Brave rows: DOS runs
         * this chain from 465b's own commit tail (0984 → 0192 → 3180 → 022e)
         * when the Brave's step lands adjacent to an unmet Euro land unit.
         */
        brave->moves = units_max_mp(ctx->units, brave->id);
        continue; /* DOS first-contact arm ends; no gift/trade this pulse */
      }

      /* Pending WELCOME: do not run AI auto arms under the dialog. */
      if (human && ctx->ai_popups &&
          ai_contact_welcome_pending(ctx->ai_popups, e, nation_id)) {
        continue;
      }

      /*
       * Human already met: no spontaneous refuse/gift/trade chrome from Brave
       * adjacency (village visit / Meet CHOICE Done thin). AI euros keep silent
       * stand-ins below.
       */
      if (human) {
        continue;
      }

      if (ind->alarm_by_player[e] >= 55 ||
          ai_diplo_indian_relation(ctx->col1, nation_id, e) < 40) {
        continue; /* AI: skip auto-trade/gift when hostile / very-low */
      }

      /*
       * 3. Peaceful auto-trade. This routes into the ported FUN_4d56_2820
       * body (`ai_contact_2820_begin` / `_begin_slot`, rewritten 2026-08-29 —
       * see its banner above), with `ai_contact_auto_buy_2e92` as the
       * empty-hold AI buy arm. Nothing about 2820 is parked here any more.
       */
      ai_contact_auto_trade(ctx, ind, nation_id, e, other);

      /* 4. Gift / demand stand-in (5bfb_102a / 1092; AI silent). */
      ai_contact_gift_or_demand(ctx, ind, nation_id, e, other, brave->x, brave->y);
    }
  }

  /* 2b. (Retired 2026-09-22, bugs.md #557.) An invented AI missionary
   * adjacency pulse (establish/heresy 50-50/crosses/alarm decay) and its
   * flee sibling lived here. DOS has no Indian-turn missionary pulse: an
   * AI missionary acts only when it reaches a village, through the
   * FUN_4d56_4528 non-human unit-type switch (case 3 -> incite/mission/
   * heresy) — ai_contact_ai_missionary_village below.
   */

  /*
   * 2b2. (Retired 2026-09-09, smell #48.) The "mission pacify deepen" −2
   * pulse used to sit here. DOS pacifies through the 152e mission term only.
   */

  /*
   * bugs.md 2026-09-04: the passive teach pulse is retired from the Indian
   * move path. DOS FUN_5bfb_022e (the Brave-side contact this pulse ports)
   * contains no teach arm at all — teaching happens only through the
   * deliberate "Live Among The Natives" @ACTIONS row (thunk_FUN_1000_a618).
   * The pulse used to fire @LEARNMAD refusals and "The %s teach outdoor
   * skills." at the player whenever a Brave wandered past a Free Colonist
   * or Scout, reading as a bogus Indian-initiated lesson.
   */
}

/* True if warehouse holds military loot secondary can drain (muskets/horses). */
static int ai_contact_colony_has_military_loot(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  return c->stock[COLONIZE_CARGO_MUSKETS] >= 5 || c->stock[COLONIZE_CARGO_HORSES] >= 1;
}

/* True if warehouse holds enough tools for high-friction secondary −1 drain. */
static int ai_contact_colony_has_tools_loot(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  return c->stock[COLONIZE_CARGO_TOOLS] >= 10;
}

/*
 * Colony wealth score for GOLD-band approach tie-break: silver stock (colony
 * precious-metal cargo; nation treasury GOLD drains separately). Cite:
 * indian_raid_outcomes.md colony approach; @RAIDGOLD / FUN_5fef_0f14.
 */
static int ai_contact_colony_gold_wealth(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  return c->stock[COLONIZE_CARGO_SILVER];
}

/*
 * True if BURN loot arm can fire: construction clear, lumber stock, or a
 * non-Town-Hall built building (colonies_destroy_building). Cite: @RAIDBURN;
 * indian_raid_outcomes.md.
 */
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
 * ship types, docs/indians.md:449). DOS filters on type only — any hull in
 * the port is fair game. Returns the unit id, or −1 when the port is empty
 * (DOS's `goto LAB_5fef_123a`, which collapses the raid to kind 0).
 */
COLONIZE_INTERNAL int ai_contact_raid_port_ship(ColonizeTurnContext* ctx, const ColonizeColony* c) {
  if (!ctx || !ctx->units || !c) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
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
 * (bugs.md #827 lead L3, settled 2026-09-23.)
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
 * Returns the cargo index, or -1 for "give up" (bugs.md #832).
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
    if (ind && ind->horse_herds == 0 && i == 1 && c->stock[cargo] > 0x34 &&
        dos_rng_range(rng, 0, 1) == 0) {
      cargo = COLONIZE_CARGO_HORSES;
    }
    if (cargo == COLONIZE_CARGO_MUSKETS) {
      (void)dos_rng_range(rng, 0, 200); /* raw 99829 — result unused in DOS */
    }
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
 * as a kind of its own (bugs.md #828 / #829).
 *
 * `forced` = 0f14's param_4 (FUN_5fef_1b0e's repelled-at-a-colony handoff):
 * it bypasses only the walls VERDICT — DOS still draws the walls roll
 * (raw 99775/99786), so the LCG stays in step.
 *
 * This replaces a Linux stand-in that picked by alarm band (>=85/70/60/55/50)
 * with a rand(0,99) and then demoted on YEAR thresholds 1500/1520 that exist
 * nowhere in the decomp (bugs.md #827). The alarm scalar has no role at this
 * head in DOS at all; the raid pulse's own targeting is where friction bites.
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
    int r = dos_rng_range(rng, 0, 12) - 1;
    if (victim_human) {
      r += difficulty - 2;
    }
    if (r < walls * 3 + 1 && !forced) {
      return AI_RAID_NOTHING;
    }
  }

  AiRaidKind kind = (AiRaidKind)dos_rng_range(rng, 1, 4); /* raw 99790 */

  /*
   * raw 99791-99795: on Discoverer/Explorer, before turn (difficulty-2)*-0x28
   * (turn 40 / 80), the building and ship kinds collapse to nothing. DS:0x538e
   * is the turn counter; the 1b0e handoff builds a context without one, so the
   * grace is skipped there (as it was before this port).
   */
  if (ctx->turn_number) {
    const int t = (difficulty - 2) * -0x28;
    const int turn = (int)*ctx->turn_number;
    if (turn <= t && turn != t && difficulty < 2 &&
        (kind == AI_RAID_BURN || kind == AI_RAID_SHIP)) {
      kind = AI_RAID_NOTHING;
    }
  }
  /* raw 99796-99807 */
  if (kind == AI_RAID_BURN) {
    const int b = victim_human ? difficulty : 1;
    if (b + 2 < dos_rng_range(rng, 0, 8)) {
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
     * (bugs.md #924).
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
 * Raid gate Euro: highest friction among met candidates (uniform ≥40),
 * prefer at-war, tie-break lower relation. Cite: indian_raid_outcomes.md §1
 * gate. No per-nation term — see the band comment below (smell #76).
 */
COLONIZE_INTERNAL int ai_contact_raid_gate_target(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int* out_euro,
  int* out_alarm
) {
  int target_euro = -1;
  int max_alarm = 0;
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || nation_id < 4 || nation_id > 11) {
    if (out_euro) {
      *out_euro = -1;
    }
    if (out_alarm) {
      *out_alarm = 0;
    }
    return 0;
  }
  int best_rel = 256;
  int best_at_war = 0;
  const int indian_idx = nation_id - 4;
  for (int e = 0; e < 4; ++e) {
    int alarm = (int)ind->alarm_by_player[e];
    if (ctx->col1->tribe) {
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id != nation_id || (int)t->alarm[e].friction <= alarm) {
          continue;
        }
        /*
         * Mid friction: prefer non-mission villages for the raid gate
         * (fandom Alarm — missions slow hostility). Mission tribes only
         * raise the gate in the burn band (≥80). Cite: indian_contact.md.
         */
        if (t->mission != COL1_TRIBE_MISSION_NONE && (int)t->alarm[e].friction < 80) {
          continue;
        }
        alarm = (int)t->alarm[e].friction;
      }
    }
    /*
     * Uniform gate, no per-nation term. Smell #76 (2026-09-09): the old
     * "Spain (2) gates at 35" special was invented AND backwards. DOS's only
     * euro-nation-2 specials in the native-hostility machinery all sit on the
     * *Euro aggressor* side, never on native targeting: the Demand-Tribute
     * contest scales the Euro's own strength for Spanish (indian_actions_menu.md
     * thunk_FUN_1000_a5f4, `e == 2: x1.5`), and FUN_5952_035e's war-decision
     * block doubles Spain's own attack weights (viceroy 94172-94186:
     * `local_1b0 == 2` turns `<<1`/`<<2` into `<<2`/`<<3` and waives the
     * relation >= 0x19 check). docs/fandom_col1994.md's "Spanish pushed toward
     * conquest" is that same Euro-side bias — nothing makes natives raid Spain
     * sooner. Gate band stays 40, matching every other friction band in
     * indian_contact.md (peaceful < 40, capture >= 70, burn >= 80).
     */
    if (alarm < 40) {
      continue;
    }
    const int at_war = ai_diplo_indian_at_war(ctx->col1, e, indian_idx);
    const int rel = (int)ai_diplo_indian_relation(ctx->col1, nation_id, e);
    if (at_war > best_at_war ||
        (at_war == best_at_war &&
         (alarm > max_alarm || (alarm == max_alarm && rel < best_rel)))) {
      best_at_war = at_war;
      max_alarm = alarm;
      best_rel = rel;
      target_euro = e;
    }
  }
  if (out_euro) {
    *out_euro = target_euro;
  }
  if (out_alarm) {
    *out_alarm = max_alarm;
  }
  return target_euro >= 0;
}

/* Chebyshev distance from (x,y) to nearest active colony of Euro `e`. */
static int ai_contact_nearest_euro_colony_dist(
  ColonizeTurnContext* ctx,
  int euro,
  int x,
  int y
) {
  if (!ctx || euro < 0 || euro > 3) {
    return 99;
  }
  int best = 99;
  (void)ai_contact_nearest_colony(ctx, euro, x, y, -1, -1, 99, &best);
  return best;
}

static void ai_contact_unit_goto_xy(const ColonizeUnit* u, int* out_x, int* out_y) {
  if (!u || !out_x || !out_y) {
    return;
  }
  if (units_orders_follow_goto(u->orders)) {
    *out_x = u->goto_x;
    *out_y = u->goto_y;
  } else {
    *out_x = u->x;
    *out_y = u->y;
  }
}

/*
 * Alarmed / mid-raid Brave escort lead pick (outside quiet 14fe).
 * Same-nation AI_MOVE/GOTO within MD≤3 (≤4 when alarm≥55; ≤5 when ≥80). When
 * raid gate Euro is known, prefer lead whose goto is closer to that Euro's
 * colony; weight 2× at ≥55, 3× at ≥80. Deep dir picker is FUN_4d56_021a (the
 * routine 14fe dispatches to, 4d56:021a..14fd — Ghidra never decompiled it);
 * still PARKED, see ai.c's ai_native_nation_pulse header for the decoded map.
 * Cite: units_follow_unit; indian_raid_outcomes.md §1; Series N.
 */
static int ai_contact_escort_pick_lead(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int follower_id,
  const ColonizeUnit* follower
) {
  if (!ctx || !ctx->units || !follower) {
    return -1;
  }
  int gate_euro = -1;
  int gate_alarm = 0;
  (void)ai_contact_raid_gate_target(ctx, ind, nation_id, &gate_euro, &gate_alarm);
  /* Peace MD≤3; alarmed≥55 → MD≤4 + 2×; hot≥80 → MD≤5 + 3× (Series N). */
  const int hot = gate_alarm >= 80;
  const int alarmed = gate_alarm >= 55;
  const int md_max = hot ? 5 : (alarmed ? 4 : 3);
  const int colony_w = hot ? 3 : (alarmed ? 2 : 1);

  int lead = -1;
  int best_md = 99;
  int best_target_d = 99;
  for (int j = 0; j < COLONIZE_UNITS_MAX; ++j) {
    const ColonizeUnit* o = &ctx->units->units[j];
    if (!o->active || o->id == follower_id || o->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(ctx->units, o->id)) {
      continue;
    }
    if (!units_orders_follow_goto(o->orders)) {
      continue;
    }
    const int md = abs(o->x - follower->x) + abs(o->y - follower->y);
    if (md <= 0 || md > md_max) {
      continue;
    }
    int ax = o->x;
    int ay = o->y;
    ai_contact_unit_goto_xy(o, &ax, &ay);
    const int target_d =
      gate_euro >= 0 ? ai_contact_nearest_euro_colony_dist(ctx, gate_euro, ax, ay) : 99;
    if (gate_euro >= 0) {
      const int score_t = target_d * colony_w;
      const int best_t = best_target_d * colony_w;
      if (score_t < best_t || (score_t == best_t && md < best_md)) {
        best_target_d = target_d;
        best_md = md;
        lead = o->id;
      }
    } else if (md < best_md) {
      best_md = md;
      lead = o->id;
    }
  }
  return lead;
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
 * construction (bugs.md #829).
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
  tok.string3 =
    (ctx->col1_ok && ctx->col1 && target_euro >= 0 && target_euro <= 3)
      ? ctx->col1->player[target_euro].country_name
      : "";
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "RAIDWREAK", &tok, "", body, sizeof(body));
  if (body[0]) {
    ai_contact_human_chrome(
      ctx, human, AI_POPUP_TAG_CONTACT_RAID, indian_nation, "Raid", body
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
     * take — a 200-stock warehouse loses 10..100 (bugs.md #830):
     *   h = stock >> 1; lo = min(h, 10); amt = rand(lo, h);
     *   amt = min(amt, stock); if (amt < 1) amt = 1;
     * and the stolen horses/muskets then feed the RAIDING TRIBE's record
     * (raw 99939-99947, bugs.md #831) — the same fields 0352's gear return
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
     * deleted (bugs.md #924).
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
   * (Retired 2026-09-23, bugs.md #833.) `ai_contact_raid_secondary_loot` ran
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
 * fandom alarm drips, bugs.md #289: DOS grows Indian alarm only through the
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
 * −16 and −12 rows, were deleted 2026-09-23: bugs.md #828/#829).
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

int ai_contact_colony_raid_repelled_w(
  const ColonizeWorld* w,
  int indian_nation,
  int euro_nation,
  int colony_id,
  int home_tribe_id,
  int forced
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
   * context 0f14 itself needs is the colony, the pools and the LCG. Build the
   * same shape the raid pulse hands the loot core; `turn_number` stays NULL
   * (its only reader, the early-game demote grace, guards on it).
   */
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = units;
  ctx.colonies = colonies;
  ctx.map = map;
  ctx.col1 = col1;
  ctx.col1_ok = true;
  ctx.rng = rng;
  ctx.human_nation = -1;

  /* Same alarm scalar the pulse's own gate hands the picker, for this Euro. */
  int max_alarm = (int)col1->indian[indian_nation - 4].alarm_by_player[euro_nation];
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if ((int)t->nation_id == indian_nation && (int)t->alarm[euro_nation].friction > max_alarm) {
        max_alarm = (int)t->alarm[euro_nation].friction;
      }
    }
  }

  const AiRaidKind kind =
    ai_contact_pick_raid_kind(&ctx, c, indian_nation, euro_nation, rng, forced);
  ai_contact_apply_raid_loot(&ctx, c, indian_nation, euro_nation, kind);
  ai_contact_raid_wreak_bulletin(&ctx, c, indian_nation, euro_nation, kind);

  /*
   * FUN_5fef_0f14's alarm tail (raw 100033), wired 2026-09-08 — negative
   * per-kind delta behind the war gate, identical to the raid pulse's entry
   * into the same resolver. DOS runs it just BEFORE the DS:0x54f6 clear
   * below, so keep this ordering.
   */
  ai_contact_raid_alarm_tail(&ctx, indian_nation, euro_nation, kind);

  /*
   * FUN_5fef_0f14's tail (raw 100034) — the unconditional DS:0x54f6 discharge
   * that 1b0e's own site 1 deliberately skips on this limb (docs/indians.md).
   * Fires for every kind, "Nothing" included.
   */
  if (col1->tribe && home_tribe_id >= 0 &&
      (uint16_t)home_tribe_id < col1->head.tribe_count) {
    /* DOS zeroes the WHOLE word, i.e. both friction and attacks. */
    col1_tribe_attitude_set(&col1->tribe[home_tribe_id], euro_nation, 0);
  }
  return (int)kind;
}

/*
 * A Brave carrying its own village's refused-demand grudge (attitude word
 * > 0x7f, LAB_5bfb_0ff2) must not be diverted into the escort/follow arm —
 * it is here to answer the refusal. Same row FUN_521d_0906 reads.
 */
static int ai_contact_brave_home_grudge(
  const ColonizeTurnContext* ctx, const ColonizeUnit* brave
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || !brave) {
    return 0;
  }
  if (brave->home_tribe_id < 0 ||
      brave->home_tribe_id >= (int)ctx->col1->head.tribe_count) {
    return 0;
  }
  const ColonizeCol1Tribe* t = &ctx->col1->tribe[brave->home_tribe_id];
  for (int e = 0; e < 4; ++e) {
    if (col1_tribe_attitude(t, e) > 0x7f) {
      return 1;
    }
  }
  return 0;
}

/*
 * Per-kind raid chrome for a HUMAN victim (audit AC-27): nine arms that each
 * spelled out the same shape — optional token field, then
 * either the GAME.TXT tag (when the colony has a name) or a thin line.
 * DOS always fires the per-kind tag for a human victim (0x1b94 @RAIDSTORES /
 * 0x1b9f @RAIDBURN / 0x1ba8 @RAIDSHIP / 0x1bb1 @RAIDGOLD / 0x1bba
 * @RAIDNOTHING, raw 99909-100020); @RAIDWREAK and the generic tail have no
 * DOS tag here and stay thin.
 *
 * `thin_colony` takes (tribe, colony name), `thin_bare` takes (tribe).
 * `popup_without_colony` is the @RAIDBURN quirk: a named burned building
 * carries the line on its own, so the tag fires even for an unnamed colony.
 */


/*
 * `section` is the GAME.TXT body — the ONLY wording involved. The thin_* slots
 * used to carry typed English notices for the cases DOS's dialog cannot cover
 * (no colony name to substitute, a burn with no named building); bugs.md #836
 * retired them under CLAUDE.md's "miss = empty string, never a typed fallback"
 * rule, so an unnamed colony now simply draws no raid line.
 */
static const AiRaidChrome k_raid_chrome[] = {
  /* @RAIDNOTHING; sound 0x5b = raid repelled (gunfight). */
  {AI_RAID_NOTHING, "RAIDNOTHING", "", NULL, NULL, 0x5b, -1, AI_RAID_TOK_NONE, 0},
  /* @RAIDSHIP; DOS plays both 0x4b and 0x4d, in that order. */
  {AI_RAID_SHIP, "RAIDSHIP", "", NULL, NULL, 0x4b, 0x4d, AI_RAID_TOK_SHIP, 0},
  /* @RAIDGOLD; 0x4e = treasury gold stolen. */
  {AI_RAID_GOLD, "RAIDGOLD", "", NULL, NULL, 0x4e, -1, AI_RAID_TOK_GOLD, 0},
  /* @RAIDSTORES; 0x4f = loot goods. */
  {AI_RAID_STORES, "RAIDSTORES", "", NULL, NULL, 0x4f, -1,
   AI_RAID_TOK_STORES, 0}
  /* @RAIDBURN thin/named rows live below (they depend on the burned building). */
};

/* @RAIDBURN, with the destroyed building named. */
static const AiRaidChrome k_raid_chrome_burn_named = {
  AI_RAID_BURN, "RAIDBURN", "", NULL, NULL, 0x53, -1, AI_RAID_TOK_BURN, 1
};
/* Burn with no named building: port notice. */
static const AiRaidChrome k_raid_chrome_burn_thin = {
  AI_RAID_BURN, NULL, NULL, NULL, NULL, 0x53, -1, AI_RAID_TOK_NONE, 0
};
/* Generic successful raid chrome when no kind-specific line applies. */
static const AiRaidChrome k_raid_chrome_generic = {
  AI_RAID_NOTHING, NULL, NULL, NULL, NULL, -1, -1, AI_RAID_TOK_NONE, 0
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
/* ===================== Raid execution, colony-tile scout displacement, field/war census & colony war-tick (ai_contact_raid_ambush_chrome .. ai_contact_a618_skill) ===================== */


/*
 * @INDIANWIN0/1/2 / @INDIANLOSE ambush chrome for a human victim of the
 * adjacent-unit arm. Extracted verbatim from ai_contact_indian_raids.
 */
COLONIZE_INTERNAL void ai_contact_raid_ambush_chrome(
  ColonizeTurnContext* ctx, int nation_id, int target_euro, int brave_won,
  int seized_muskets, int seized_horses, const char* foe_unit_name,
  const char* foe_nation_label, const char* place, int foe_type
) {
  /*
   * GAME.TXT @INDIANWIN0/1/2 / @INDIANLOSE:
   * WIN:  {%STRING0} ambush {%STRING1 %STRING2} near %STRING3!
   *       (+ Muskets/Horses seized by %STRING4 braves! for WIN1/2)
   * LOSE: {%STRING1 %STRING2} %STRING4 {%STRING0} near %STRING3!
   */
  if (ai_contact_euro_is_human(ctx, target_euro)) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    const char* tribe = ai_contact_tribe_name(nation_id);
    tok.string0 = tribe;
    tok.string1 = foe_nation_label;
    tok.string2 = foe_unit_name;
    tok.string3 = place;
    tok.string4 = tribe;
    const char* sec = "INDIANLOSE";
    char fb[AI_POPUP_BODY_LEN];
    if (brave_won) {
      if (seized_muskets) {
        sec = "INDIANWIN1";
        fb[0] = '\0';
      } else if (seized_horses) {
        sec = "INDIANWIN2";
        fb[0] = '\0';
      } else {
        sec = "INDIANWIN0";
        fb[0] = '\0';
      }
    } else {
      /* LABELS defeat/defeats — unit subjects type_index ≥7 use "defeats". */
      tok.string4 = reports_misc_display_word(
        (foe_type >= 0 && foe_type < 7) ? 73 : 74, ""
      );
      snprintf(
        fb,
        sizeof(fb),
        "%s %s %s %s near %s!",
        foe_nation_label,
        foe_unit_name,
        tok.string4,
        tribe,
        place
      );
    }
    char ambush_body[AI_POPUP_BODY_LEN];
    if (ctx->messages) {
      popup_msg_fill(ctx->messages, sec, &tok, fb, ambush_body, sizeof(ambush_body));
    } else {
      snprintf(ambush_body, sizeof(ambush_body), "%s", fb);
    }
    ai_contact_human_chrome(
      ctx,
      target_euro,
      AI_POPUP_TAG_COMBAT_AMBUSH,
      nation_id,
      "",
      ambush_body
    );
  }
}

#include "core/ai_contact_internal.h" /* AiRaidStatus, struct ai_contact_raid_ctx */

/*
 * Stage 2: adjacent unit combat (FUN_4d56_4528 arm 2). Extracted verbatim
 * from ai_contact_indian_raids; sets a->attacked for the colony stage.
 */
COLONIZE_INTERNAL void ai_contact_raid_stage_combat(struct ai_contact_raid_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* brave = a->brave;
  ColonizeDosRng* const rng = a->rng;
  const int nation_id = a->nation_id;
  const int target_euro = a->target_euro;
  const int max_alarm = a->max_alarm;

  /* 2. Adjacent unit combat. */
  int attacked = 0;
  for (int d = 0; d < 8 && !attacked; ++d) {
    const int nx = brave->x + MAP_DIR8_DX[d];
    const int ny = brave->y + MAP_DIR8_DY[d];
    /*
     * bugs.md: units_id_at picked the first unit in POOL ORDER, so a raid
     * could duel an unarmed colonist while a soldier stood on the same
     * tile. Use the DOS best-defender walk (FUN_5fef_0000) like every
     * other combat entry; on a civilian-only colony tile it returns -1
     * and the raid skips (colony raids go through their own path).
     */
    const int foe = units_best_defender_at(
      ctx->units, ctx->col1, nx, ny, brave->id, brave->id
    );
    if (foe < 0) {
      continue;
    }
    ColonizeUnit* f = units_get(ctx->units, foe);
    if (!f || f->nation_id != target_euro || units_is_sea(ctx->units, foe)) {
      continue;
    }
    /*
     * bugs.md: Indians should be more chill — the ambush arm only fires
     * in the provocation band (alarm ≥ 55, the same cut the war-declare
     * escalation uses) or at open war, not at the ≥40 raid-gate band.
     * No DOS site exists for a Treasure-Train bypass of this gate, so it
     * applies uniformly regardless of defender type (#748).
     */
    if (max_alarm < 55 &&
        !ai_diplo_indian_at_war(ctx->col1, target_euro, nation_id - 4)) {
      continue;
    }
    /* Snapshot before combat despawn (the loser is gone by the chrome call). */
    const int foe_x = f->x;
    const int foe_y = f->y;
    const int foe_type = f->type_index;
    char foe_unit_name[48];
    {
      const ColonizeUnitType* ft = units_type(ctx->units, foe_type);
      snprintf(
        foe_unit_name,
        sizeof(foe_unit_name),
        "%s",
        /* #836: no typed fallback — a NAMES miss renders empty (CLAUDE.md). */
        ft && ft->name[0] ? ft->name : ""
      );
    }
    const char* foe_nation_label = "your";
    if (ctx->col1 && target_euro >= 0 && target_euro <= 3 &&
        ctx->col1->player[target_euro].country_name[0]) {
      foe_nation_label = ctx->col1->player[target_euro].country_name;
    }
    /* LABELS.TXT @MISC row 17 = "Wilderness"; reports_misc_display_word is
       the shared sim-side live lookup (reports_names.c), fallback kept. */
    const char* place = reports_misc_display_word(17, "");
    if (ctx->colonies) {
      int best_d = 99;
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        const ColonizeColony* c = &ctx->colonies->colonies[ci];
        if (!c->active || c->nation_id != target_euro || !c->name[0]) {
          continue;
        }
        const int d = map_chebyshev(foe_x, foe_y, c->x, c->y);
        if (d < best_d) {
          best_d = d;
          place = c->name;
        }
      }
    }
    /*
     * This arm draws its own @INDIANWIN0/1/2 / @INDIANLOSE below (with the
     * muskets/horses seizure lines DOS builds by appending '1'/'2' to tag
     * 0x1ca9, FUN_1d1d_07e4). Silence the generic native-attacker chrome in
     * units_combat_outcome_popups for the duration, or the same fight
     * reports twice.
     */
    units_set_native_combat_chrome_owned(1);
    units_clear_native_gear_step();
    const int brave_won =
      units_resolve_land_combat(ctx->units, brave->id, foe, rng) ? 1 : 0;
    units_set_native_combat_chrome_owned(0);
    /*
     * bugs.md #645: the gear seizure itself is DOS FUN_5fef_1b0e raw
     * 100730-100744 and now lives in the land-loss outcome path
     * (units_apply_land_loss_outcome), where it steps the brave's TYPE and
     * tallies the tribe record — the numeric muskets/horses transfer that
     * used to sit here was an invention, and it only ever ran in this AI raid
     * arm, so a human Dragoon losing to a Brave never mounted it. All that is
     * left here is the chrome pick: `bVar13` -> @INDIANWIN1 (armed step),
     * `bVar14` -> @INDIANWIN2 (mounted step), raw 101088-101095.
     */
    int seized_muskets = 0;
    int seized_horses = 0;
    units_last_native_gear_step(&seized_muskets, &seized_horses);
    if (brave_won) {
      {
        ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, rng, NULL);
        units_try_move_w(&w_, brave->id, nx, ny);
      }
    }
    /*
     * GAME.TXT @INDIANWIN0/1/2 / @INDIANLOSE:
     * WIN:  {%STRING0} ambush {%STRING1 %STRING2} near %STRING3!
     *       (+ Muskets/Horses seized by %STRING4 braves! for WIN1/2)
     * LOSE: {%STRING1 %STRING2} %STRING4 {%STRING0} near %STRING3!
     */
  ai_contact_raid_ambush_chrome(
    ctx, nation_id, target_euro, brave_won, seized_muskets, seized_horses,
    foe_unit_name, foe_nation_label, place, foe_type
  );
    /*
     * (Retired 2026-09-08, smell #65.) A +2 alarm bump (Pocahontas-halved)
     * plus attacks++ across every tribe of the nation sat here — the last
     * retired-drip-class caller. DOS's post-ambush effects live inside the
     * combat resolve itself (negative vent + attitude zero); the attacks
     * byte's only DOS writer is the 465b trespass arm.
     */
    attacked = 1;
  }
  a->attacked = attacked;
}

/*
 * Stage 3 pick: nearest raidable colony of the gated Euro. Extracted
 * verbatim from ai_contact_indian_raids.
 */
COLONIZE_INTERNAL int ai_contact_raid_pick_colony(struct ai_contact_raid_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  const ColonizeUnit* const brave = a->brave;
  const int target_euro = a->target_euro;
  const int max_alarm = a->max_alarm;

  int best_cid = -1;
  int best_d = 99;
  int best_mil = 0;
  int best_tools = 0;
  int best_gold = 0;
  /* Alarm≥80: MD≤8 + gold-before-tools at equal dist (Series Q). */
  const int md_max = (max_alarm >= 80) ? 8 : 6;
  const int hot_wealth = (max_alarm >= 80);
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id != target_euro) {
      continue;
    }
    const int d = map_chebyshev(brave->x, brave->y, c->x, c->y);
    if (d > md_max) {
      continue;
    }
    /*
     * Prefer closer; at equal distance prefer muskets/horses (military
     * secondary). Peace/mid: tools≥10 then silver wealth. Hot alarm≥80:
     * silver wealth before tools (GOLD-band). Cite:
     * indian_raid_outcomes.md multi-loot / colony approach; @RAIDGOLD;
     * Series Q.
     */
    const int mil = ai_contact_colony_has_military_loot(c);
    const int tools = ai_contact_colony_has_tools_loot(c);
    const int gold_w = ai_contact_colony_gold_wealth(c);
    int better = 0;
    if (d < best_d) {
      better = 1;
    } else if (d == best_d && mil && !best_mil) {
      better = 1;
    } else if (d == best_d && mil == best_mil) {
      if (hot_wealth) {
        if (gold_w > best_gold ||
            (gold_w == best_gold && tools && !best_tools)) {
          better = 1;
        }
      } else if ((tools && !best_tools) ||
                 (tools == best_tools && gold_w > best_gold)) {
        better = 1;
      }
    }
    if (better) {
      best_d = d;
      best_cid = c->id;
      best_mil = mil;
      best_tools = tools;
      best_gold = gold_w;
    }
  }
  return best_cid;
}

/*
 * @RAID* / @INDIANWAR / @INDIANSURPRISE chrome for a human raid victim.
 * Extracted verbatim from ai_contact_indian_raids.
 */
COLONIZE_INTERNAL void ai_contact_raid_human_chrome(
  ColonizeTurnContext* ctx, const ColonizeColony* c, int nation_id, int target_euro,
  AiRaidKind kind, int max_alarm, int had_peace, int eff_at_war
) {
  if (ai_contact_euro_is_human(ctx, target_euro)) {
    /*
     * FUN_5fef 5fef:22a9 — a native attacker (nation ≥ 4) on a human
     * Euro defender fires woodcut 13; the burn arms below fire
     * woodcut 11 (5fef:2b6c / 5fef:305b, both COLONY BURNING — id 12
     * COLONY DESTROYED has no DOS call site).
     */
    (void)woodcut_fire(ctx->col1, WOODCUT_INDIAN_RAID);
    char raid_line[AI_POPUP_BODY_LEN];
    const char* raid_body = NULL;
    const char* tribe = ai_contact_tribe_name(nation_id);
    PopupMsgTokens raid_tok;
    memset(&raid_tok, 0, sizeof(raid_tok));
    raid_tok.string0 = tribe;
    raid_tok.string1 = c->name[0] ? c->name : NULL;
    const AiRaidChrome* row =
      ai_contact_raid_chrome_row(kind, ai_contact_s_last_burn_building[0] != '\0');
    /* DOS-LITERAL FUN_5fef_0f14 raw 99901-99908: for a human victim,
     * kind 0 restores the colony tune pool through FUN_281f_0498(2);
     * every loot kind queues track 0x32 through FUN_281f_048e. */
    if (kind == AI_RAID_NOTHING) {
      sound_set_bgm(2);
    } else {
      sound_play(0x32);
    }
    switch (row->tok) {
      case AI_RAID_TOK_SHIP:
        raid_tok.string2 = ai_contact_s_last_ship_type[0] ? ai_contact_s_last_ship_type : "";
        break;
      case AI_RAID_TOK_STORES:
        raid_tok.string2 = ai_contact_s_last_stores_cargo[0] ? ai_contact_s_last_stores_cargo : "";
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
     * DOS FUN_5fef_0f14 keys these cues only to the resolved raid kind and a
     * human victim (raw asm 154640, 154763, 154801-154803, 154847, 154869).
     * Popup text availability is unrelated; in particular the ship arm is
     * deliberately a two-cue sequence, 0x4b then 0x4d.
     */
    if (row->sound >= 0) {
      sound_play(row->sound);
    }
    if (row->sound2 >= 0) {
      sound_play(row->sound2);
    }
    if (row->section && (c->name[0] || row->popup_without_colony)) {
      popup_msg_fill(
        ctx->messages, row->section, &raid_tok, row->popup_fallback,
        raid_line, sizeof(raid_line)
      );
    } else if (row->thin_colony && c->name[0]) {
      snprintf(raid_line, sizeof(raid_line), row->thin_colony, tribe, c->name);
    } else if (row->thin_bare) {
      snprintf(raid_line, sizeof(raid_line), row->thin_bare, tribe);
    } else {
      raid_line[0] = '\0'; /* #836: no typed fallback */
    }
    raid_body = raid_line[0] ? raid_line : NULL;
    /*
     * bugs.md: the @INDIANWAR / @INDIANSURPRISE lines used to sit as
     * two arms INSIDE this chain, so any raid by a tribe that was not
     * yet at war printed only "their chief denies involvement" and
     * the player never learned what had been stolen or burned — the
     * loot was applied silently. DOS FUN_5fef_0f14 has no such arm:
     * it always fires the per-kind tag for a human victim (0x1b94
     * @RAIDSTORES / 0x1b9f @RAIDBURN / 0x1ba8 @RAIDSHIP / 0x1bb1
     * @RAIDGOLD / 0x1bba @RAIDNOTHING, raw 99909-100020). The war /
     * deniability sentence is Linux chrome, so it now rides IN FRONT
     * of the DOS line instead of replacing it.
     */
    char raid_full[AI_POPUP_BODY_LEN];
    if (raid_body && kind != AI_RAID_NOTHING) {
      const char* pre = NULL;
      char pre_buf[224];
      if (had_peace && max_alarm >= 55) {
        /*
         * Linux war notice. It used to be labelled "@INDIANWAR thin",
         * but @INDIANWAR is dead GAME.TXT text: no NUL-terminated
         * "INDIANWAR" tag string exists anywhere in VICEROY.EXE's DS
         * (only "INDIANWARPATH"/"INDIANWARPATH2"/"INDIANWARFARE"), so
         * DOS can never ask the dialog engine for that section
         * (2026-09-16). The sentence stays as port chrome, no longer
         * claiming to be a GAME.TXT body.
         */
        /*
         * (Retired 2026-09-23, bugs.md #836.) "The %s declare war! Prepare
         * for WAR!" was typed English with no catalog row behind it —
         * @INDIANWAR is dead GAME.TXT text (no DS tag string in VICEROY.EXE),
         * so there is nothing to read and the line renders empty.
         */
        pre_buf[0] = '\0';
        pre = NULL;
      } else if (!eff_at_war) {
        /*
         * @INDIANSURPRISE (0x14dc) — real GAME.TXT body, filled with
         * DOS's own three slots (tribe, the colony the raid happened
         * near, tribe again) as the OVL13 brave-move site loads them
         * (viceroy_overlays.c 76958-76970). A raid while NOT at war is
         * deniable (indian_raid_outcomes.md §8).
         */
        PopupMsgTokens stok;
        memset(&stok, 0, sizeof(stok));
        stok.string0 = tribe;
        stok.string1 = c->name[0] ? c->name : "";
        stok.string2 = tribe;
        popup_msg_fill(
          ctx->messages, "INDIANSURPRISE", &stok, "", pre_buf, sizeof(pre_buf)
        );
        pre = pre_buf;
      }
      if (pre) {
        /* Explicit tail bound: `pre` (<=223) + the two spaces always fit,
         * so only a pathologically long body is clipped. */
        const int raid_pre_len = (int)strlen(pre);
        int raid_room = (int)sizeof(raid_full) - raid_pre_len - 3;
        if (raid_room < 0) {
          raid_room = 0;
        }
        snprintf(
          raid_full, sizeof(raid_full), "%s  %.*s", pre, raid_room, raid_body
        );
        raid_body = raid_full;
      }
    }
    ai_contact_human_chrome(
      ctx,
      target_euro,
      AI_POPUP_TAG_CONTACT_RAID,
      nation_id,
      "Raid",
      raid_body
    );
  }
}

/*
 * Stages 4-5: on-tile loot resolve (FUN_5fef_0f14) + its alarm tail and the
 * raider discharge. Extracted verbatim from ai_contact_indian_raids.
 */
COLONIZE_INTERNAL void ai_contact_raid_resolve_on_tile(
  struct ai_contact_raid_ctx* a, ColonizeColony* c
) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* const brave = a->brave;
  ColonizeDosRng* const rng = a->rng;
  const int nation_id = a->nation_id;
  const int target_euro = a->target_euro;
  const int max_alarm = a->max_alarm;

  const AiRaidKind kind =
    ai_contact_pick_raid_kind(ctx, c, nation_id, target_euro, rng, 0);
  ai_contact_apply_raid_loot(ctx, c, nation_id, target_euro, kind);
  ai_contact_raid_wreak_bulletin(ctx, c, nation_id, target_euro, kind);
  /* 0f14's alarm tail + DS:0x54f6 word-zero run at the resolver's
   * very END in DOS (raw 100033-100034) — after all the popup/side-art
   * chrome — so they sit below the status block here, not at this
   * spot (moving them up made the attacks snapshot read an
   * already-cleared word once the phantom array was retired). */
  /*
   * bugs.md #281: the raid pulse never takes or destroys a colony —
   * DOS FUN_5fef_0f14 only loots. Colony destruction lives on the
   * real combat path (units_try_capture_foreign_colony's Indian arm:
   * kill one colonist, burn only when the last falls), and Indians
   * NEVER capture (the old colonies_capture here flipped ownership
   * to the tribe — "Sioux march into Amsterdam"). The three
   * `abandoned`/@BURNED/@BURNED3 arms this rule left behind a
   * permanently-false flag in front of were deleted 2026-09-14
   * (audit AC-37); the live @BURNED chrome is in units.c's
   * capture/fallout path.
   */
  /* (Retired 2026-09-08.) A Linux-only per-tribe attacks++ counter
   * sat here backing the "only the FIRST attack is deniable" chrome
   * (bugs.md). DOS has no such counter on this path: the attacks
   * byte is the attitude-word high byte, bumped only by the 465b
   * trespass arm and zeroed by 0f14's own tail every raid — so it
   * can never carry "raided before" across raids. The DOS
   * discriminator is the at-war state alone (indian_raid_outcomes.md
   * §8: plain raid line when already at war, @INDIANSURPRISE when
   * not; at-war = 153e's alarm > 0x4a, or the diplo WAR bit). */
  /*
   * (Retired 2026-09-08.) A fandom-derived POSITIVE kind bump used to
   * sit here — "raids raise tension", deltas +4/+12/+16/+8 with DOS's
   * signs flipped and DOS's kind 3 mis-assigned to SCALP. DOS 0f14
   * does the exact opposite: see ai_contact_raid_alarm_tail above,
   * now called right after the loot. Same retirement rule as the
   * three fandom alarm drips (bugs.md #289).
   */
  /*
   * High-friction successful raid → escalate Indian×Euro hostility
   * (4cc6_00f2 via ai_diplo). If treaty/peace still held → clear peace
   * bit (@INDIANWAR). Full 4528/2820 dialog PARKED.
   */
  const int had_peace =
    ai_contact_indian_has_peace(ctx->col1, nation_id, target_euro);
  const int was_at_war =
    ai_diplo_indian_at_war(ctx->col1, target_euro, nation_id - 4);
  /* DOS 153e at-war band: alarm > 0x4a counts as war for the raid
   * chrome even when the WAR bit / relation view lag behind (test
   * fixtures and fresh saves often carry alarm only). */
  const int eff_at_war = was_at_war || max_alarm > 0x4a;
  if (kind != AI_RAID_NOTHING && max_alarm >= 55) {
    /* No alarm push here: DOS 0f14's only alarm write is the tail
     * above, and it is negative. The peace-bit clear / hostility sync
     * are Linux chrome for the @INDIANWAR line below and must not add
     * a second alarm store (the old −3/−5 relation push, and the
     * fandom kind bump that replaced it, both did). */
    if (had_peace) {
      ai_contact_clear_peace(ctx->col1, nation_id, target_euro);
    }
    ai_diplo_indian_hostility_sync(ctx->col1, target_euro);
  }
  /*
   * Thin raid outcome status for human target (full @RAID* dialog PARKED).
   * @RAIDNOTHING (GAME.TXT): "raiding party wiped out" — empty warehouse /
   * no lootable stock also lands here (no invented cargo). Cite:
   * COLONIZE/GAME.TXT @RAIDNOTHING; indian_raid_outcomes.md.
   * @INDIANWAR when peace broken; @INDIANSURPRISE when not yet at war.
   */
  ai_contact_raid_human_chrome(
    ctx, c, nation_id, target_euro, kind, max_alarm, had_peace, eff_at_war
  );
  /*
   * FUN_5fef_0f14's tail, in DOS order (after every chrome draw):
   * raw 100033 — NEGATIVE per-kind alarm delta behind the
   * NOT-at-war gate (`15b3_0004 & 2` set skips the call), see
   * ai_contact_raid_alarm_tail; then raw 100034 — the unconditional
   * DS:0x54f6 word-zero: `(origin*9 + euro)*2 + 0x54f6` = the home
   * settlement record's attitude[euro] word = tribe.alarm[euro],
   * BOTH bytes, for EVERY kind including "Nothing" — the act of
   * raiding itself discharges the village's accumulated grudge.
   */
  ai_contact_raid_alarm_tail(ctx, nation_id, target_euro, kind);
  if (
    ctx->col1->tribe && brave->home_tribe_id >= 0 &&
    (uint16_t)brave->home_tribe_id < ctx->col1->head.tribe_count
  ) {
    col1_tribe_attitude_set(
      &ctx->col1->tribe[brave->home_tribe_id], target_euro, 0
    );
  }
  /*
   * bugs.md: the raiding party does not survive its raid. DOS's only
   * call site for FUN_5fef_0f14 is FUN_5fef_1b0e's loser limb (raw
   * 101142): the native attacker has ALREADY been destroyed by the
   * combat resolve before the raid resolver runs — @RAIDNOTHING even
   * says so in as many words ("raiding party wiped out"). The port's
   * pulse reaches 0f14 without a combat, so it has to discharge the
   * raider itself; without this the Brave stayed parked on the colony
   * tile and re-raided it every single turn, forever, at no risk.
   */
  units_despawn(ctx->units, brave->id);
}

/*
 * Stage 3-5 driver: colony approach / loot / capture band.
 */
COLONIZE_INTERNAL AiRaidStatus ai_contact_raid_stage_colony(struct ai_contact_raid_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* brave = a->brave;
  ColonizeDosRng* const rng = a->rng;
  const int attacked = a->attacked;
  const int max_alarm = a->max_alarm;

  /* 3–5. Colony approach / loot / capture. */
  if (!attacked && ctx->colonies && brave->active) {
    const int best_cid = ai_contact_raid_pick_colony(a);
    if (best_cid >= 0) {
      ColonizeColony* c = colonies_get_mut(ctx->colonies, best_cid);
      if (!c) {
        return AI_RAID_NEXT_BRAVE;
      }
      if (brave->x == c->x && brave->y == c->y) {
        ai_contact_raid_resolve_on_tile(a, c);
        return AI_RAID_NEXT_BRAVE;
      } else if (max_alarm >= 70) {
        /*
         * Approach march only in high-friction capture band (≥70). Mid gate
         * 40..69 keeps on-tile loot/combat but must not walk Braves — seed-100
         * TURN4→5 is already at-war for some tribes; approach broke the golden.
         * Cite: indian_raid_outcomes.md; golden_ai_turns.
         */
        int sdx = (c->x > brave->x) - (c->x < brave->x);
        int sdy = (c->y > brave->y) - (c->y < brave->y);
        {
          ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, rng, NULL);
          units_try_move_w(&w_, brave->id, brave->x + sdx, brave->y + sdy);
        }
      }
    }
  }
  return AI_RAID_CONTINUE;
}

void ai_contact_indian_raids(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (nation_id < 4 || nation_id > 11 || !ctx->col1->tribe) {
    return;
  }
  ai_contact_bind_names(ctx);
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  ColonizeDosRng local;
  ai_contact_local_rng(ctx, nation_id, &local);
  ColonizeDosRng* rng = ctx->rng ? ctx->rng : &local;
  /* Prefer isolated RNG for loot picks so pulse stream stays untouched if shared. */
  rng = &local;

  /*
   * FUN_4d56_4528 / 5fef_0f14-shaped arms (thin):
   *  1 gate → 2 adjacent combat → 3 colony approach → 4 @RAID* loot →
   *  5 capture. (The old "stage 6 scout 359c displace/despawn" arm was
   *  deleted 2026-09-18, bugs.md #499: FUN_4d56_359c is not an anti-Scout
   *  sweep at all — raw 83481-83505 is the Enter-Hostile-Village wagon
   *  outcome (@KILLWAGONS / @MADATWAGONS / @GRUDGEWAGONS), already ported as
   *  ai_contact_enter_hostile_village. The arm's alarm 90/95 gates, 1/4 kill
   *  roll and display-name Scout match had no DOS source.)
   *
   * FUN_4d56_2820 (~1.4k; thunk 2a1f_044c) is the meet/raid decision matrix
   * DOS reaches before settlement enter. Its trade half is ported elsewhere
   * in this file (`ai_contact_2820_begin`, 2026-08-29 rewrite) and the AI
   * pulse reaches it through `ai_contact_auto_trade` — do NOT re-port the
   * body here; this post-pulse path keeps the thin @RAID* / combat arms. Human `4528` `@ACTIONS` arm is ported (P8.8); `4528` VGA meet
   * chrome and the alarmed act-pick mid-body remain PARKED.
   * Linux stays on thin @RAID* / combat + equal-dist mil/tools/silver
   * approach. Widgets Done structural (ai_popup); VGA PARKED. Mid-friction prefers non-mission
   * villages (below). Cite: indian_raid_outcomes.md §10; indian_contact.md
   * PORT DEBT; docs/port_plan.md FUN_4d56_2820; Marathon2 R6 PARK.
   */
  struct ai_contact_raid_ctx a;
  memset(&a, 0, sizeof(a));
  a.ctx = ctx;
  a.ind = ind;
  a.rng = rng;
  a.nation_id = nation_id;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    /* Natives keep the DOS SPENT byte in moves — gate on remaining MP
     * via the accessor, not the raw byte (audit: raw read skipped FRESH
     * braves and admitted exhausted ones). */
    if (!brave->active || brave->nation_id != nation_id ||
        units_remaining_mp(ctx->units, brave->id) <= 0) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
      continue;
    }
    /* One limb per Brave per turn — a Brave that already resolved the
     * peaceful 5bfb_022e visit (gift / convert / beg) this turn does not also
     * raid. See the ai_contact_s_visit_brave_id note at the top of this file. */
    if (ai_contact_brave_visited_this_turn(ctx, nation_id, brave->id)) {
      continue;
    }

    /*
     * 1. Gate: among Euros with friction/alarm ≥40, prefer Indian×Euro at-war
     * (ai_diplo_indian_at_war / relation <50); then highest friction; tie-break
     * lower ai_diplo_indian_relation (very-low <40 hostility).
     * Cite: indian_raid_outcomes.md gate; FUN_4d56_4528 thin; fandom Alarm.
     *
     * Alarmed unit-act escort (outside quiet 14fe): idle Brave may
     * units_follow_unit a same-nation lead already AI_MOVE/GOTO. Lead pick
     * prefers goto toward raid-gate Euro colony; when max alarm≥55 the same
     * peel is the alarmed branch (deep dir picker FUN_4d56_021a still PARKED).
     * Quiet seed-100 pulse unchanged. Cite: units_follow_unit;
     * indian_raid_outcomes.md §1.
     */
    if (brave->orders == UNITS_ORDER_NONE &&
        !ai_contact_brave_home_grudge(ctx, brave) &&
        units_remaining_mp(ctx->units, brave->id) > 0) {
      const int lead =
        ai_contact_escort_pick_lead(ctx, ind, nation_id, brave->id, brave);
      if (lead >= 0 && units_follow_unit(ctx->units, brave->id, lead)) {
        (void)units_advance_follow_one_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .rng=(ColonizeDosRng*)(ctx->rng)}, brave->id);
        continue;
      }
    }
    if (brave->orders == UNITS_ORDER_FOLLOW) {
      (void)units_advance_follow_one_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .rng=(ColonizeDosRng*)(ctx->rng)}, brave->id);
      continue;
    }
    int target_euro = -1;
    int max_alarm = 0;
    if (!ai_contact_raid_gate_target(ctx, ind, nation_id, &target_euro, &max_alarm)) {
      continue;
    }

    a.brave = brave;
    a.target_euro = target_euro;
    a.max_alarm = max_alarm;
    a.attacked = 0;
    ai_contact_raid_stage_combat(&a);
    if (ai_contact_raid_stage_colony(&a) == AI_RAID_NEXT_BRAVE) {
      continue;
    }
  }
}
