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

/*
 * Rough goods-value for AI STORES plunder pick (FUN_5fef_016c stand-in).
 * Horses stay on secondary military loot — not primary STORES. Cite:
 * peel layer_b_combat_raid FUN_5fef_016c; indian_raid_outcomes.md @RAIDSTORES.
 */
static int ai_contact_stores_cargo_value(int cargo) {
  switch (cargo) {
  case COLONIZE_CARGO_SILVER:
    return 8;
  case COLONIZE_CARGO_MUSKETS:
    return 7;
  case COLONIZE_CARGO_TRADE_GOODS:
    return 6;
  case COLONIZE_CARGO_TOOLS:
    return 5;
  case COLONIZE_CARGO_RUM:
  case COLONIZE_CARGO_CIGARS:
  case COLONIZE_CARGO_CLOTH:
  case COLONIZE_CARGO_COATS:
    return 4;
  case COLONIZE_CARGO_SUGAR:
  case COLONIZE_CARGO_TOBACCO:
  case COLONIZE_CARGO_COTTON:
  case COLONIZE_CARGO_FURS:
    return 3;
  case COLONIZE_CARGO_ORE:
    return 2;
  case COLONIZE_CARGO_FOOD:
  case COLONIZE_CARGO_LUMBER:
    return 1;
  default:
    return 0; /* horses / unknown — not primary STORES */
  }
}

/* True if colony warehouse has any cargo the STORES arm can actually drain. */
/* `skip_cargo` = -1 for "any raidable cargo"; the burn-preference test passes
 * COLONIZE_CARGO_LUMBER to ask the same question ignoring lumber (audit AC-32). */
static int ai_contact_colony_has_stores(const ColonizeColony* c, int skip_cargo) {
  if (!c) {
    return 0;
  }
  for (int cargo = 0; cargo < COLONIZE_CARGO_COUNT; ++cargo) {
    if (cargo == skip_cargo) {
      continue;
    }
    if (ai_contact_stores_cargo_value(cargo) > 0 && c->stock[cargo] > 0) {
      return 1;
    }
  }
  return 0;
}

/* Pick highest-value stock>0 cargo (ties → higher stock, then lower index). */
static int ai_contact_pick_stores_cargo(const ColonizeColony* c) {
  if (!c) {
    return -1;
  }
  int best = -1;
  int best_val = -1;
  int best_stock = -1;
  for (int cargo = 0; cargo < COLONIZE_CARGO_COUNT; ++cargo) {
    const int stock = c->stock[cargo];
    if (stock <= 0) {
      continue;
    }
    const int val = ai_contact_stores_cargo_value(cargo);
    if (val <= 0) {
      continue;
    }
    if (val > best_val || (val == best_val && stock > best_stock) ||
        (val == best_val && stock == best_stock && (best < 0 || cargo < best))) {
      best = cargo;
      best_val = val;
      best_stock = stock;
    }
  }
  return best;
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

/* True if WREAK can mutate food/tools/building-in-production. */
static int ai_contact_colony_has_wreak_target(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  return c->stock[COLONIZE_CARGO_FOOD] > 0 || c->stock[COLONIZE_CARGO_TOOLS] > 0 ||
         c->building_in_production >= 0;
}

/*
 * True if BURN loot arm can fire: construction clear, lumber stock, or a
 * non-Town-Hall built building (colonies_destroy_building). Cite: @RAIDBURN;
 * indian_raid_outcomes.md.
 */
static int ai_contact_colony_has_burn_target(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c
) {
  if (!c) {
    return 0;
  }
  if (c->building_in_production >= 0 || c->stock[COLONIZE_CARGO_LUMBER] > 0) {
    return 1;
  }
  if (!pool) {
    return 0;
  }
  for (int bi = 0; bi < pool->building_type_count; ++bi) {
    if (!c->has_building[bi]) {
      continue;
    }
    const ColonizeBuildingType* bt = colonies_building_type(pool, bi);
    if (!bt || colonies_building_name_row(bt->name) == COLONY_BUILDING_TOWN_HALL) {
      continue;
    }
    return 1;
  }
  return 0;
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

static AiRaidKind ai_contact_pick_raid_kind(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int target_euro,
  int max_alarm,
  ColonizeDosRng* rng,
  int forced
) {
  /*
   * Banded picker mirroring @RAID* message outcomes (not DOS bit-identity).
   * Gate kinds on colony stock / gold actually present so empty warehouses
   * do not fake STORES/WREAK/muskets loot (5fef_0f14-shaped). No Indian-nation
   * treasury fiction — GOLD drains Euro gold only when present.
   *
   * `forced` = DOS `FUN_5fef_0f14` param_4 (the 1b0e repelled-at-a-colony
   * handoff): it bypasses the walls check below. DOS has no alarm band at
   * this head at all — that gate is a Linux-only stand-in for the raid
   * pulse's own targeting, so `forced` bypasses it too.
   */
  if (max_alarm < 45 && !forced) {
    return AI_RAID_NOTHING;
  }
  /*
   * FUN_5fef_0f14 head — the colony's walls decide first (static port
   * 2026-08-28): walls = FUN_281f_0ab0(0) = owned buildings along the
   * Stockade → Fort → Fortress chain (0..3); r = rand(0,12) - 1, plus
   * difficulty-2 when the victim is human; r < walls*3 + 1 → kind 0
   * (@RAIDNOTHING "raiding party wiped out"). Bare colony: 1/13; Stockade
   * 4/13; Fort 7/13; Fortress 10/13 before the difficulty shift.
   */
  if (c && ctx && ctx->colonies && rng && !forced) {
    int walls = 0;
    const int* k_chain = colonies_building_chain_rows(COLONIES_CHAIN_FORTIFICATION);
    for (int i = 0; i < 3 && k_chain && k_chain[i] >= 0; ++i) {
      const int b = colonies_building_row(ctx->colonies, (ColonizeBuildingRow)k_chain[i]);
      if (b >= 0 && b < COLONIZE_BUILDING_TYPES_MAX && c->has_building[b]) {
        walls++;
      }
    }
    int r = dos_rng_range(rng, 0, 12) - 1;
    if (ai_contact_euro_is_human(ctx, target_euro) && ctx->col1) {
      r += (int)ctx->col1->head.difficulty - 2;
    }
    if (r < walls * 3 + 1) {
      return AI_RAID_NOTHING;
    }
  }
  /*
   * Same head, early-game grace: on Discoverer/Explorer, before turn
   * (2-difficulty)*40, DOS demotes the building (2) and unit (3) kinds to
   * nothing. Applied below to BURN / WREAK / SHIP / SCALP-by-roll.
   */
  int early_grace = 0;
  if (ctx && ctx->col1 && ctx->turn_number && ctx->col1->head.difficulty < 2u) {
    const int limit = (2 - (int)ctx->col1->head.difficulty) * 40;
    early_grace = (int)*ctx->turn_number < limit;
  }
  const int roll = rng ? dos_rng_range(rng, 0, 99) : (max_alarm % 100);
  if (max_alarm >= 85 && roll < 15 && ai_contact_colony_has_wreak_target(c)) {
    return early_grace ? AI_RAID_NOTHING : AI_RAID_WREAK;
  }
  if (max_alarm >= 70 && roll < 25 && c && c->population > 1) {
    return AI_RAID_SCALP;
  }
  /* BURN: construction, lumber, or destroyable built building. */
  if (max_alarm >= 60 && roll < 20 &&
      ai_contact_colony_has_burn_target(ctx ? ctx->colonies : NULL, c)) {
    return early_grace ? AI_RAID_NOTHING : AI_RAID_BURN;
  }
  if (max_alarm >= 55 && roll < 15 && ctx && ctx->col1_ok && ctx->col1 &&
      target_euro >= 0 && target_euro < 4 &&
      /* Same store the drain below debits (audit G3) — a stale record here
       * would pick AI_RAID_GOLD for a victim whose live purse is empty. */
      europe_nation_gold(ctx->europe, ctx->col1, target_euro) > 0) {
    return AI_RAID_GOLD;
  }
  if (max_alarm >= 50 && roll < 12 && c && ctx && ctx->map) {
    /* Harbor: prefer if water adjacent. */
    for (int d = 0; d < 8; ++d) {
      if (map_tile_is_water(ctx->map, c->x + MAP_DIR8_DX[d], c->y + MAP_DIR8_DY[d])) {
        if (roll < 10) {
          return early_grace ? AI_RAID_NOTHING : AI_RAID_SHIP;
        }
        break;
      }
    }
  }
  /*
   * STORES when lootable cargo present. Prefer BURN over lumber-as-STORES in
   * the BURN band when the burn gate (construction / lumber) is the only
   * wooden-building stock target — richer warehouses still take STORES.
   */
  if (ai_contact_colony_has_stores(c, -1)) {
    const int prefer_burn =
      max_alarm >= 60 &&
      ai_contact_colony_has_burn_target(ctx ? ctx->colonies : NULL, c) &&
      !ai_contact_colony_has_stores(c, COLONIZE_CARGO_LUMBER);
    if (!prefer_burn) {
      return AI_RAID_STORES;
    }
  }
  if (c && c->population > 1 && max_alarm >= 70) {
    return AI_RAID_SCALP;
  }
  if (ai_contact_colony_has_burn_target(ctx ? ctx->colonies : NULL, c) &&
      max_alarm >= 60) {
    return AI_RAID_BURN;
  }
  return AI_RAID_NOTHING;
}

/* Apply 5fef_0f14-shaped difficulty/year/building demote after primary pick. */
COLONIZE_INTERNAL AiRaidKind ai_contact_raid_kind_demote(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  AiRaidKind kind
) {
  if (kind == AI_RAID_NOTHING || kind == AI_RAID_STORES) {
    return kind;
  }
  /*
   * FUN_5fef_0f14 kind 3 (raw 99991-99992): no ship in the colony's port
   * (`FUN_281f_088a` fails) → `goto LAB_5fef_123a`, i.e. `local_6 = 0` — the
   * raid collapses to NOTHING outright, NOT to the goods kind, so no loot and
   * no alarm vent. Without this the port could pay 0f14's −16 vent for a
   * harbor raid that damaged nothing.
   */
  if (kind == AI_RAID_SHIP && ai_contact_raid_port_ship(ctx, c) < 0) {
    return AI_RAID_NOTHING;
  }
  int difficulty = 0;
  int year = 1492;
  if (ctx && ctx->col1_ok && ctx->col1) {
    difficulty = (int)ctx->col1->head.difficulty;
    year = (int)ctx->col1->head.year;
  }
  /*
   * Demote harsh kinds on easy / early game, or when burn/scalp target missing:
   * BURN/SCALP/GOLD/SHIP/WREAK → STORES if stock, else NOTHING.
   * Cite: indian_raid_loot.md kind demote gates.
   */
  int demote = 0;
  if (difficulty <= 0 &&
      (kind == AI_RAID_SCALP || kind == AI_RAID_WREAK || kind == AI_RAID_GOLD)) {
    demote = 1;
  }
  if (year < 1520 && kind == AI_RAID_WREAK) {
    demote = 1;
  }
  if (kind == AI_RAID_BURN &&
      !ai_contact_colony_has_burn_target(ctx ? ctx->colonies : NULL, c)) {
    demote = 1;
  }
  if (kind == AI_RAID_SCALP && (!c || c->population <= 1)) {
    demote = 1;
  }
  if (!demote) {
    return kind;
  }
  if (ai_contact_colony_has_stores(c, -1)) {
    return AI_RAID_STORES;
  }
  return AI_RAID_NOTHING;
}

/*
 * Secondary multi-loot after a successful primary @RAID* (kind != NOTHING).
 *  - Military side-steal: only if warehouse/unit actually holds muskets/horses
 *    (−5 muskets stock, else −1 horse stock, else same from target-nation unit
 *    gear on the colony tile). Empty warehouses do not fake muskets loot.
 *  - High friction (≥80): also drain tools (−1) when stock present.
 * Full 5fef_0f14 / 4528 dialog PARKED.
 */
COLONIZE_INTERNAL void ai_contact_raid_secondary_loot(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int target_euro,
  int max_alarm
) {
  if (!c) {
    return;
  }

  if (c->stock[COLONIZE_CARGO_MUSKETS] >= 5) {
    c->stock[COLONIZE_CARGO_MUSKETS] -= 5;
  } else if (c->stock[COLONIZE_CARGO_HORSES] >= 1) {
    c->stock[COLONIZE_CARGO_HORSES] -= 1;
  } else if (ctx && ctx->units && target_euro >= 0 && target_euro < 4) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != target_euro) {
        continue;
      }
      if (u->x != c->x || u->y != c->y) {
        continue;
      }
      if (u->muskets >= 5) {
        u->muskets -= 5;
        break;
      }
      if (u->horses >= 1) {
        u->horses -= 1;
        break;
      }
    }
  }
  /* else: empty warehouse + no unit gear → no fake military loot */

  if (max_alarm >= 80 && c->stock[COLONIZE_CARGO_TOOLS] > 0) {
    c->stock[COLONIZE_CARGO_TOOLS]--;
  }
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

void ai_contact_apply_raid_loot(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int target_euro,
  AiRaidKind kind,
  int max_alarm
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
     * FUN_5fef_0f14 kind1 + 016c pick: goods-value cargo, remove
     * clamp(1..10) of up to half stock (decomp ~99913–99925).
     */
    const int cargo = ai_contact_pick_stores_cargo(c);
    if (cargo >= 0 && c->stock[cargo] > 0) {
      int half = c->stock[cargo] >> 1;
      if (half > 10) {
        half = 10;
      }
      if (half < 1) {
        half = 1;
      }
      if (half > c->stock[cargo]) {
        half = c->stock[cargo];
      }
      c->stock[cargo] -= half;
      snprintf(ai_contact_s_last_stores_cargo, sizeof(ai_contact_s_last_stores_cargo), "%s", ai_contact_cargo_name(cargo));
    }
    break;
  }
  case AI_RAID_BURN:
    /* @RAIDBURN / 5fef_0f14: clear construction first. */
    if (c->building_in_production >= 0) {
      c->building_in_production = -1;
    } else if (c->stock[COLONIZE_CARGO_LUMBER] > 0) {
      c->stock[COLONIZE_CARGO_LUMBER] -= (c->stock[COLONIZE_CARGO_LUMBER] > 2) ? 2 : 1;
    } else if (ctx && ctx->colonies) {
      /*
       * Empty warehouse: damage a non-Town-Hall built building via
       * colonies_destroy_building (clears workplace colonists). Prefer
       * Stockade/Warehouse/Dock-like first built index > Town Hall.
       * Cite: @RAIDBURN building loot; colonies_destroy_building.
       */
      int burn_bt = -1;
      for (int bi = 0; bi < ctx->colonies->building_type_count; ++bi) {
        if (!c->has_building[bi]) {
          continue;
        }
        const ColonizeBuildingType* bt = colonies_building_type(ctx->colonies, bi);
        if (!bt || colonies_building_name_row(bt->name) == COLONY_BUILDING_TOWN_HALL) {
          continue;
        }
        burn_bt = bi;
        break;
      }
      if (burn_bt >= 0) {
        const ColonizeBuildingType* bbt = colonies_building_type(ctx->colonies, burn_bt);
        if (colonies_destroy_building(ctx->colonies, c->id, burn_bt) && bbt &&
            bbt->name[0]) {
          snprintf(ai_contact_s_last_burn_building, sizeof(ai_contact_s_last_burn_building), "%s", bbt->name);
        }
      }
    }
    break;
  case AI_RAID_SCALP:
    if (c->population > 1) {
      c->population--;
      if (c->colonist_count > 1) {
        c->colonist_count--;
      }
    }
    break;
  case AI_RAID_GOLD:
    if (ctx && ctx->col1_ok && ctx->col1 && target_euro >= 0 && target_euro < 4) {
      /*
       * FUN_5fef_0f14 kind4: roll gold drain vs treasury (thin: 32..min(cap,treasury)).
       * Cite: indian_raid_loot.md; decomp ~99876–99893 / 100017–100030.
       */
      const uint32_t victim_gold = europe_nation_gold(ctx->europe, ctx->col1, target_euro);
      if (victim_gold > 0) {
        unsigned drain = 32u + (unsigned)(c->population > 0 ? c->population * 8 : 8);
        if (drain < 50u) {
          drain = 50u;
        }
        if (drain > 500u) {
          drain = 500u;
        }
        if (drain > victim_gold) {
          drain = victim_gold;
        }
        europe_nation_gold_add(ctx->europe, ctx->col1, target_euro, -(long)drain);
        ai_contact_s_last_gold_drained = (int)drain;
      }
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
  case AI_RAID_WREAK:
    if (c->stock[COLONIZE_CARGO_FOOD] > 0) {
      c->stock[COLONIZE_CARGO_FOOD]--;
    }
    if (c->stock[COLONIZE_CARGO_TOOLS] > 0) {
      c->stock[COLONIZE_CARGO_TOOLS]--;
    }
    if (c->building_in_production >= 0) {
      c->building_in_production = -1;
    }
    break;
  default:
    break;
  }

  if (kind != AI_RAID_NOTHING) {
    ai_contact_raid_secondary_loot(ctx, c, target_euro, max_alarm);
  }
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
 * DOS kind space vs. the port's AiRaidKind:
 *   1 goods     -> AI_RAID_STORES (warehouse cargo drain)
 *   2 building  -> AI_RAID_BURN and AI_RAID_WREAK (a building razed /
 *                  construction wrecked — 0f14's kind 2 picks a building
 *                  index 0..0x29 and destroys it)
 *   3 unit      -> AI_RAID_SHIP and AI_RAID_SCALP. 0f14's kind-3 walk only
 *                  accepts unit types 0xd..0x12, which are the SHIPS
 *                  (docs/indians.md:449, same band 4cc6_03f8's threat ring
 *                  skips) — so SHIP is the literal match. The port's
 *                  colonist-kill band has no DOS kind of its own and is the
 *                  same "a unit at the colony dies" outcome, so it takes the
 *                  same row. (Until this pass the port had these swapped:
 *                  SCALP got the 16 and SHIP was lumped with gold's 8.)
 *   4 gold      -> AI_RAID_GOLD (treasury drain)
 */
COLONIZE_INTERNAL int ai_contact_raid_alarm_delta(AiRaidKind kind) {
  switch (kind) {
    case AI_RAID_STORES:
      return -4;
    case AI_RAID_BURN:
    case AI_RAID_WREAK:
      return -12;
    case AI_RAID_SHIP:
    case AI_RAID_SCALP:
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

  const AiRaidKind kind = ai_contact_raid_kind_demote(
    &ctx, c, ai_contact_pick_raid_kind(&ctx, c, euro_nation, max_alarm, rng, forced)
  );
  ai_contact_apply_raid_loot(&ctx, c, euro_nation, kind, max_alarm);

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
 * spelled out the same shape — optional bgm, optional token field, then
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
 * `section` is the GAME.TXT body (the only MicroProse wording involved — it is
 * read from the catalog, never typed here). The thin_* lines are the PORT's
 * own short notices for the cases DOS's dialog cannot cover (no colony name
 * to substitute, or a burn with no named building); they are deliberately
 * not phrased like the catalog text.
 */
static const AiRaidChrome k_raid_chrome[] = {
  /* @RAIDNOTHING; sound 0x5b = raid repelled (gunfight). */
  {AI_RAID_NOTHING, "RAIDNOTHING", "", NULL, "%s raid repelled.", 0x5b, 2, AI_RAID_TOK_NONE, 0},
  /* @RAIDSHIP */
  {AI_RAID_SHIP, "RAIDSHIP", "", NULL, "The %s raid your harbor.", -1, -1, AI_RAID_TOK_SHIP, 0},
  /* @RAIDSCALP; sound 0x4e = colonists killed (screaming). */
  {AI_RAID_SCALP, "RAIDSCALP", "", NULL, "The %s massacre colonists at your colony!", 0x4e, -1,
   AI_RAID_TOK_NONE, 0},
  /* @RAIDGOLD; 0x4d = loot gold. */
  {AI_RAID_GOLD, "RAIDGOLD", "", NULL, "The %s raid your treasury!", 0x4d, -1, AI_RAID_TOK_GOLD, 0},
  /* @RAIDSTORES; 0x4f = loot goods. */
  {AI_RAID_STORES, "RAIDSTORES", "", NULL, "The %s loot your stores.", 0x4f, -1,
   AI_RAID_TOK_STORES, 0},
  /* @RAIDWREAK needs a foreign-colony nation token the human path lacks: thin. */
  {AI_RAID_WREAK, NULL, NULL, "%s raiders strike %s.", "%s raiders strike.", -1, -1,
   AI_RAID_TOK_NONE, 0}
};

/* @RAIDBURN, with the destroyed building named. */
static const AiRaidChrome k_raid_chrome_burn_named = {
  AI_RAID_BURN, "RAIDBURN", "", NULL, NULL, -1, -1, AI_RAID_TOK_BURN, 1
};
/* Burn with no named building: port notice. */
static const AiRaidChrome k_raid_chrome_burn_thin = {
  AI_RAID_BURN, NULL, NULL, "%s raiders set fires in %s.", "%s raiders set fires.", -1, -1,
  AI_RAID_TOK_NONE, 0
};
/* Generic successful raid chrome when no kind-specific line applies. */
static const AiRaidChrome k_raid_chrome_generic = {
  AI_RAID_NOTHING, NULL, NULL, "The %s raid %s.", "The %s raid your colony.",
  -1, -1, AI_RAID_TOK_NONE, 0
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
      tok.string4 = (foe_type >= 0 && foe_type < 7) ? "defeat" : "defeats";
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
        ft && ft->name[0] ? ft->name : "units"
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
    if (row->bgm >= 0) {
      /* FUN_5fef_0f14 5fef:1299: a wiped-out raid on a human colony
       * hands the tune pool back to 2; any other outcome pushes the
       * 0x32 combat sting (5fef:13b2). */
      sound_set_bgm(row->bgm);
    }
    switch (row->tok) {
      case AI_RAID_TOK_SHIP:
        raid_tok.string2 = ai_contact_s_last_ship_type[0] ? ai_contact_s_last_ship_type : "A ship";
        break;
      case AI_RAID_TOK_STORES:
        raid_tok.string2 = ai_contact_s_last_stores_cargo[0] ? ai_contact_s_last_stores_cargo : "goods";
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
    if (row->section && (c->name[0] || row->popup_without_colony)) {
      if (row->sound >= 0) {
        sound_play(row->sound);
      }
      popup_msg_fill(
        ctx->messages, row->section, &raid_tok, row->popup_fallback,
        raid_line, sizeof(raid_line)
      );
    } else if (row->thin_colony && c->name[0]) {
      snprintf(raid_line, sizeof(raid_line), row->thin_colony, tribe, c->name);
    } else {
      snprintf(raid_line, sizeof(raid_line), row->thin_bare, tribe);
    }
    raid_body = raid_line;
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
        snprintf(
          pre_buf, sizeof(pre_buf), "The %s declare war! Prepare for WAR!", tribe
        );
        pre = pre_buf;
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

  const AiRaidKind kind = ai_contact_raid_kind_demote(
    ctx, c, ai_contact_pick_raid_kind(ctx, c, target_euro, max_alarm, rng, 0)
  );
  ai_contact_apply_raid_loot(ctx, c, target_euro, kind, max_alarm);
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
