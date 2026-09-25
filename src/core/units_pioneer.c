#include "core/units.h"

/*
 * Pioneer work: plow/road/clear tick & tool wear.
 *
 * Sections:
 *  - Pioneer work: plow/road/clear tick & tool wear (units_is_pioneer .. units_pioneer_road)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/combat_analysis.h"
#include "core/combat_strength.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/sound.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/village_trade_intel.h"
#include "core/woodcut.h"
#include "platform/diagnostics.h"
#include "core/units_internal.h"

/* ===================== Pioneer work: plow/road/clear tick & tool wear (units_is_pioneer .. units_pioneer_road) ===================== */


bool units_is_pioneer(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || units_is_sea(pool, unit_id)) {
    return false;
  }
  /* Skill alone is not enough — plow/road require carried tools. */
  int tools = 0;
  units_founder_loot(pool, unit_id, &tools, NULL, NULL);
  return tools > 0;
}

#define UNITS_PIONEER_TOOL_COST 20

/*
 * DOS `FUN_281f_0754(x,y) & 0x0a` — the layer2 road-OR-settlement mask that
 * both Build Road gates use (order entry FUN_2b5a_1454 raw 42552-42555, work
 * body FUN_479b_0526 raw 76873-76876). bugs.md #618. The port's settlement
 * bit is MAP_OCCUPANCY_HAS_CITY (map_tile_has_city); the colony pool is
 * consulted as well so the gate still holds when the live occupancy map is
 * not bound (headless tests, tools).
 */
static bool units_pioneer_road_mask(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int x,
  int y
) {
  if (map_tile_has_road(map, x, y) || map_tile_has_city(map, x, y)) {
    return true;
  }
  return colonies && colonies_id_at(colonies, x, y) >= 0;
}


/*
 * DOS FUN_479b_01a6/0526: both clear/plow and road read the same DS:0x2f78
 * threshold byte (offset +2 of the terrain-class record), but only the
 * clear/plow body adds 2 (`local_2a = byte + 2`); the road body takes the
 * bare byte (`local_1c = byte`). Hardy Pioneer (profession 20) halves either.
 * Cite: viceroy_unpacked FUN_479b_01a6:76767 / FUN_479b_0526:76891; live
 * capture 2026-08-20 (was approximated with the move-cost byte, offset +0,
 * before the real +2 byte was captured). The `road` arm was `(void)road` —
 * every road cost 2 extra turns — until 2026-09-06e re-read both bodies.
 */
static int units_pioneer_work_needed(const ColonizeUnit* u, const ColonizeWorldMap* map, bool road) {
  int needed = map_dos_terr_pioneer_threshold_byte(map_dos_terr_class_at(map, u->x, u->y));
  if (!road) {
    needed += 2;
  }
  if (u->profession == UNITS_JOB_PIONEER) {
    needed >>= 1;
  }
  /* DOS-LITERAL: neither FUN_479b_01a6 (raw 76765-76770) nor FUN_479b_0526
   * (raw 76890-76893) clamps the threshold — a `needed < 1` floor was an
   * invented guard (bugs.md #606). */
  return needed;
}

/*
 * FUN_281f_0614 (= FUN_15eb_0142) nearest-colony search, the one writer of
 * DOS DS:0x8db8: closest active colony of `filter_nation` (any when < 0) by
 * map_dos_dist, first wins on ties; *out_dist gets the winning distance
 * (the 0x8db8 side effect both Pioneer bodies read back). Cite:
 * move_scoring_land.md "0x8db8 identified".
 */
static ColonizeColony* units_nearest_colony_dos(
  ColonizeColonyPool* colonies,
  int x,
  int y,
  int filter_nation,
  int* out_dist
) {
  ColonizeColony* best = NULL;
  int best_d = 9999;
  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &colonies->colonies[i];
      if (!c->active || (filter_nation >= 0 && c->nation_id != filter_nation)) {
        continue;
      }
      const int d = map_dos_dist(c->x - x, c->y - y);
      if (d < best_d) {
        best_d = d;
        best = c;
      }
    }
  }
  if (out_dist) {
    *out_dist = best_d;
  }
  return best;
}

/*
 * FUN_479b_01a6 LAB_479b_043b (clear/plow, base 5) / FUN_479b_0526
 * LAB_479b_0687 (road, base 3) — what finishing a Pioneer job on tribal land
 * costs. Gates, in DOS order:
 *   FUN_281f_0d84 nearest village on the tile's own continent, kept only when
 *     DS:0x8db8 <= FUN_281f_0a56 (the tribe's tech tier 1/2/3) — exactly
 *     colonies_indian_land_owner_tribe's homeland test;
 *   FUN_281f_0696 < 0: no colony stands on the tile;
 *   layer2 & 0x10 clear (land not already bought/gifted) and
 *     FUN_281f_07b4(nation, 2) == 0 (no Peter Minuit) — both are exactly the
 *     early-outs of colonies_indian_land_purchase_gold, so a price of 0 here
 *     means one of them fired;
 *   thunk_FUN_2a1f_01d8 = FUN_479b_00ca: an **AI** nation (0x543f != 0) that
 *     can cover price + price/2 buys the tile outright (treasury debit,
 *     indian.lands_bought++, purchased bit) and takes no anger at all.
 * Otherwise the tribe's alarm rises by `base` (+ difficulty when the acting
 * nation is the human, DS:0x53a6), doubled within DOS distance 3 and tripled
 * within 2, via FUN_281f_0d6c → FUN_4cc6_00f2 (positive delta = alarm up).
 * The 4th argument DOS passes 0d6c (2 for clear, 1 for road) is not read by
 * 00f2's body — recorded, not modelled.
 */
static void units_pioneer_native_land_tail(
  const ColonizeUnit* u,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int base_add
) {
  ColonizeCol1Save* col1 = g_units_fallout_col1;
  if (!col1 || !map || !u) {
    return;
  }
  const int nation = u->nation_id;
  if (nation < 0 || nation > 3) {
    return;
  }
  const int ti = colonies_indian_land_owner_tribe(col1, map, u->x, u->y);
  if (ti < 0 || !col1->tribe) {
    return;
  }
  if (colonies && colonies_id_at(colonies, u->x, u->y) >= 0) {
    return;
  }
  const int price = colonies_indian_land_purchase_gold(col1, map, u->x, u->y, nation);
  if (price <= 0) {
    return; /* already purchased/gifted (mask 0x10), or Peter Minuit */
  }
  const int is_ai = col1->player[nation].control != 0;
  if (is_ai) {
    uint32_t* gold = &col1->nation[nation].gold;
    const long have = (long)*gold;
    if ((long)(price >> 1) <= have - (long)price) {
      colonies_indian_land_pay(col1, map, u->x, u->y, nation, gold, price);
      return; /* FUN_479b_00ca returned 1 — bought, no alarm */
    }
  }
  int base = base_add;
  if (!is_ai) {
    base += (int)col1->head.difficulty;
  }
  const int dist = map_dos_dist(u->x - (int)col1->tribe[ti].x, u->y - (int)col1->tribe[ti].y);
  int amount = base;
  if (dist < 3) {
    amount = base * 2;
  }
  if (dist < 2) {
    amount += base;
  }
  ai_diplo_indian_alarm_delta(col1, (int)col1->tribe[ti].nation_id, nation, amount);
}

static bool units_pioneer_tile_can_clear_or_plow(
  const ColonizeWorldMap* map,
  int x,
  int y,
  bool* out_clearing
) {
  if (!map_tile_is_land(map, x, y) || map_tile_is_high_seas(map, x, y)) {
    return false;
  }
  /*
   * bugs.md #619. DOS's terrain veto for Clear/Plow lives in the ORDERS menu
   * FUN_2b5a_0b34 raw 42224-42227: it hides ids 0x312/0x313 only on classes
   * 0x1b (Mountains) / 0x1c (Hills) on the FUN_281f_078c scale that
   * map_pedia_terrain_index_at mirrors. Arctic (24) is plowable in DOS (its
   * DS:0x2f76 threshold byte is 4). The work body FUN_479b_01a6 raw
   * 76745-76755 itself aborts only on `layer2 & 0x40`, class 0x19 (Ocean)
   * and 0x1a (Sea Lane) — covered by the land / high-seas tests above.
   * The port had `24 || 27`, which denied Arctic and permitted Hills, so the
   * menu offered Plow Fields on Arctic and the order then failed.
   */
  const int pedia = map_pedia_terrain_index_at(map, x, y);
  if (pedia == 0x1b || pedia == 0x1c) {
    return false; /* Mountains / Hills */
  }
  if (pedia >= 8 && pedia <= 23) {
    if (out_clearing) {
      *out_clearing = true;
    }
    return true;
  }
  if (map_tile_is_plowed(map, x, y)) {
    return false;
  }
  if (out_clearing) {
    *out_clearing = false;
  }
  return true;
}

/* FUN_479b_0158: cargo_hold[5] / tools −20; type→Colonist when <20. */
static bool units_pioneer_wear_tools(ColonizeUnitPool* pool, ColonizeUnit* u) {
  if (u->tools >= UNITS_PIONEER_TOOL_COST) {
    u->tools -= UNITS_PIONEER_TOOL_COST;
  } else {
    u->tools = 0;
  }
  if (u->tools < UNITS_PIONEER_TOOL_COST) {
    u->tools = 0;
    /* DOS-LITERAL FUN_479b_0158 raw 76696-76718: tools-out writes only
     * tools (+0x3159), type (+0x3146) and @USEDUPTOOLS — it never touches
     * orders (+0x314c). Orders are cleared by the work bodies themselves on
     * completion (FUN_479b_01a6 raw 76779 / 0526 raw 76899). (bugs.md #597) */
    if (pool) {
      /* DOS-LITERAL FUN_479b_0158 raw 76706-76709: type := 0 (Colonists), then
       * `if (+0x315b == 0x18) type := 3` — a Jesuit Missionary that ran its
       * tools out reverts to Missionaries, not Colonists (bugs.md #509). */
      const ColonizeUnitKind want_kind = (u->profession == UNITS_JOB_MISSIONARY) ? UNITS_KIND_MISSIONARY : UNITS_KIND_COLONIST;
      const int tgt = units_kind_type_index(pool, want_kind);
      if (tgt >= 0) {
        u->type_index = tgt;
      }
    }
    return true;
  }
  return false;
}

/* @USEDUPTOOLS after demotion (human / unset-human only). */
static void units_pioneer_emit_useduptools(
  const ColonizeUnit* u,
  char* err,
  size_t err_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (err && err_size) {
    snprintf(err, err_size, "Tools used up — now a colonist");
  }
  if (!ai_popups || !u) {
    return;
  }
  if (g_units_combat_human_nation >= 0 && u->nation_id != g_units_combat_human_nation) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    messages,
    "USEDUPTOOLS",
    NULL,
    "", /* bugs.md #632: catalog miss = empty string */
    body,
    sizeof(body)
  );
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

/* Order-gate OK chrome (human / unset-human). */
static void units_pioneer_emit_order_gate(
  const ColonizeUnit* u,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages,
  const char* section,
  const char* fallback
) {
  if (!ai_popups || !section) {
    return;
  }
  if (u && g_units_combat_human_nation >= 0 && u->nation_id != g_units_combat_human_nation) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(messages, section, NULL, fallback ? fallback : section, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

bool units_pioneer_work_tick_w(
  const ColonizeWorld* w,
  int unit_id,
  char* err,
  size_t err_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  ColonizeUnitPool* pool = w->units;
  ColonizeWorldMap* map = w->map;
  ColonizeColonyPool* colonies = w->colonies;

  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !map || !u->active || !units_is_on_map(u)) {
    return false;
  }
  const bool road = (u->orders == UNITS_ORDER_BUILD_ROAD);
  const bool clear_plow = (u->orders == UNITS_ORDER_CLEAR_PLOW);
  if (!road && !clear_plow) {
    return false;
  }
  /*
   * bugs.md #628: DOS's work bodies never read tools (raw 76722-76960); the
   * tool debit FUN_479b_0158 simply underflows hold[5] and demotes. Only the
   * type test survives (FUN_2b5a_0b08 is type-only).
   * bugs.md #620: an aborted tick (raw 76753 / 76886) clears the order and
   * returns WITHOUT touching the +0x315a progress counter.
   */
  if (!units_is_pioneer(pool, unit_id)) {
    u->orders = UNITS_ORDER_NONE;
    return false;
  }

  if (road) {
    /* bugs.md #618: DOS FUN_479b_0526 raw 76873-76876 aborts on
     * `layer2 & 0x0a` = road OR settlement, not the road bit alone. */
    if (!map_tile_is_land(map, u->x, u->y) || map_tile_is_high_seas(map, u->x, u->y) ||
        units_pioneer_road_mask(map, colonies, u->x, u->y)) {
      u->orders = UNITS_ORDER_NONE;
      return false;
    }
  } else {
    bool clearing = false;
    if (!units_pioneer_tile_can_clear_or_plow(map, u->x, u->y, &clearing)) {
      u->orders = UNITS_ORDER_NONE;
      return false;
    }
    (void)clearing;
  }

  /* FUN_281f_0934 stand-in: exhaust MP for this act (spent-aware, audit A9). */
  units_mp_exhaust(pool, u);
  if (u->col1_counter16 < 255) {
    u->col1_counter16++;
  }
  const int needed = units_pioneer_work_needed(u, map, road);
  if (u->col1_counter16 < needed) {
    /* bugs.md #633: DOS's work bodies write nothing to the status channel
     * while the job is in progress; the only text they emit is @CLEARCUT
     * (raw 76800) and @USEDUPTOOLS (0158 raw 76715). The port's
     * "Clearing forest (n/m)" / "Plowing (n/m)" / "Building road (n/m)"
     * progress lines were port-only chrome and are gone. */
    return true;
  }

  u->col1_counter16 = 0;
  u->orders = UNITS_ORDER_NONE;
  if (road) {
    map_tile_set_road(map, u->x, u->y, true);
    const bool demoted = units_pioneer_wear_tools(pool, u);
    /*
     * DOS-LITERAL FUN_479b_0526 raw 76908-76921: road completion looks up the
     * nearest colony of ANY nation — `FUN_281f_0614(x, y, 0xffff, 0xffff)`,
     * filter -1 — and only then tests
     * `if ((unit.nation & 0xf) == colony[+0x1a]) colony[+0x98] += 10`.
     * So a road laid beside a nearer FOREIGN colony pays nobody; it does not
     * fall through to a more distant own colony (bugs.md #617). Distance is
     * DOS's own max+min/2 metric.
     */
    if (colonies) {
      ColonizeColony* near_road = units_nearest_colony_dos(colonies, u->x, u->y, -1, NULL);
      if (near_road && near_road->nation_id == (u->nation_id & 0xf)) {
        int next = (int)near_road->hammers_purchased + 10;
        if (next > 65535) {
          next = 65535;
        }
        near_road->hammers_purchased = (uint16_t)next;
      }
    }
    if (demoted) {
      units_pioneer_emit_useduptools(u, err, err_size, ai_popups, messages);
    }
    /* LAB_479b_0687 tail: base 3 (0d6c mode 1). */
    units_pioneer_native_land_tail(u, map, colonies, 3);
  } else {
    bool clearing = false;
    (void)units_pioneer_tile_can_clear_or_plow(map, u->x, u->y, &clearing);
    if (clearing) {
      map_tile_clear_forest(map, u->x, u->y);
      const bool demoted = units_pioneer_wear_tools(pool, u);
      /*
       * FUN_479b_01a6 clear: lumber → nearest same-nation colony + @CLEARCUT.
       * Real formula (was flat 20, PARKED, before the terrain-table byte was
       * captured 2026-08-20): scale = terrain +8 byte if colony has a Lumber
       * Mill, else 1 (a floor, not a gate) — add = scale*20, doubled for
       * Hardy Pioneer, clamped to warehouse room. Cite: viceroy_unpacked
       * FUN_479b_01a6, live capture 2026-08-20.
       *
       * 2026-09-06e, from the raw body: the grant is **radius-gated** —
       * `FUN_281f_0614(x, y, nation, 0xffff)` must return a colony AND its
       * DS:0x8db8 distance must be < 4 (the road body, 0526, has no such
       * gate). 0x8db8 is the nearest-colony search's own distance output
       * (move_scoring_land.md), not the difficulty byte. The scale also takes
       * a +1 from `FUN_281f_0754(colony.x, colony.y) & 0x0a` = layer2
       * road|city on the receiving colony's own tile; a colony tile always
       * carries the city bit, so in DOS that bump always lands (the road half
       * of the test is dead there) — kept as an unconditional +1 rather than
       * a Linux layer2 read, because Linux's layer2 bit 0x08 is
       * MAP_LAYER2_LCR_CONSUMED, not Col1's road bit.
       */
      int lumber_add = 0;
      ColonizeColony* near = NULL;
      if (colonies) {
        int near_dist = 9999;
        near = units_nearest_colony_dos(colonies, u->x, u->y, u->nation_id, &near_dist);
        if (near_dist >= 4) {
          near = NULL;
        }
        if (near) {
          const int lumber_mill = colonies_building_row(colonies, COLONY_BUILDING_LUMBER_MILL);
          const bool has_mill = lumber_mill >= 0 && near->has_building[lumber_mill];
          int scale = map_dos_terr_lumber_reward_byte(map_dos_terr_class_at(map, u->x, u->y));
          scale += 1; /* layer2(colony tile) & 0x0a — city bit, always set */
          if (!has_mill) {
            scale = 1;
          }
          int potential = scale * 20;
          if (u->profession == UNITS_JOB_PIONEER) {
            potential *= 2;
          }
          const int cap = colonies_warehouse_capacity(colonies, near, COLONIZE_CARGO_LUMBER);
          int room = cap - near->stock[COLONIZE_CARGO_LUMBER];
          if (room < 0) {
            room = 0;
          }
          lumber_add = potential;
          if (lumber_add > room) {
            lumber_add = room;
          }
          if (lumber_add > 0) {
            int next = near->stock[COLONIZE_CARGO_LUMBER] + lumber_add;
            if (next < 0) {
              next = 0;
            }
            if (next > 65535) {
              next = 65535;
            }
            near->stock[COLONIZE_CARGO_LUMBER] = next;
          }
        }
      }
      /* bugs.md #633: no "Forest cleared (…)" status line in DOS — @CLEARCUT
       * below is the only text FUN_479b_01a6 emits. */
      if (lumber_add > 0 && near && ai_popups &&
          (g_units_combat_human_nation < 0 || u->nation_id == g_units_combat_human_nation)) {
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = near->name[0] ? near->name : "";
        tok.number0 = lumber_add;
        tok.has_number0 = true;
        popup_msg_fill(
          messages,
          "CLEARCUT",
          &tok,
          "", /* bugs.md #632: catalog miss = empty string */
          body,
          sizeof(body)
        );
        ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      }
      /* bugs.md #278: no @DEFOREST popup — the tag string never appears in
       * VICEROY.EXE (unlike CLEARCUT/DEPLETION), so DOS never shows that
       * GAME.TXT section; the port fired it on every chop on top of
       * @CLEARCUT, double-notifying. */
      if (demoted) {
        units_pioneer_emit_useduptools(u, err, err_size, ai_popups, messages);
      }
      /*
       * LAB_479b_043b tail: base 5 (0d6c mode 2). DOS gates it on `!bVar2`,
       * i.e. only a finished forest CLEAR angers the tribe / triggers the AI
       * land buy — plowing an already-open tile does not reach the label.
       */
      units_pioneer_native_land_tail(u, map, colonies, 5);
    } else {
      map_tile_set_plowed(map, u->x, u->y, true);
      const bool demoted = units_pioneer_wear_tools(pool, u);
      if (demoted) {
        units_pioneer_emit_useduptools(u, err, err_size, ai_popups, messages);
      }
      /* bugs.md #633: DOS's plow completion writes no status text. */
    }
  }
  return true;
}


/*
 * Shared head of units_pioneer_plow / units_pioneer_road (UN-9): the pioneer
 * gate + @ONLYPIO refusal, the moves gate (skipped while the order is already
 * latched, because a work tick costs nothing) and the tools gate. Returns the
 * unit on success, NULL when the order was refused.
 */
static ColonizeUnit* units_pioneer_begin_order(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int order,
  char* err,
  size_t err_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  (void)ai_popups;
  (void)messages;
  (void)order;
  ColonizeUnit* u = units_get(pool, unit_id);
  /*
   * bugs.md #621: no @ONLYPIO popup. The literal ONLYPIO does not occur in
   * VICEROY.EXE at all; DOS FUN_2b5a_0b34 raw 42211-42215 simply greys menu
   * ids 0x312/0x313/0x314 via FUN_291f_0146 when FUN_2b5a_0b08 says the unit
   * is not type 2 — the row can never be picked, so there is nothing to
   * refuse. map_menu.c does the same greying.
   * bugs.md #615: no MP gate. FUN_2b5a_123e raw 42427-42513 and
   * FUN_2b5a_1454 raw 42519-42604 read moves nowhere; the first work tick
   * runs at 0 MP and FUN_281f_0934 exhausts inside the body (raw 76761),
   * which is why DOS pioneers finish work at 0 MP.
   * bugs.md #628: no tools gate either — DOS never reads tools at order time.
   */
  if (!u || !map || !units_is_pioneer(pool, unit_id)) {
    if (err && err_size) {
      snprintf(err, err_size, "Select a Pioneer");
    }
    return NULL;
  }
  return u;
}

bool units_pioneer_plow_w(
  const ColonizeWorld* w,
  int unit_id,
  char* err,
  size_t err_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  ColonizeUnitPool* pool = w->units;
  ColonizeWorldMap* map = w->map;
  ColonizeColonyPool* colonies = w->colonies;

  ColonizeUnit* u = units_pioneer_begin_order(
    pool, unit_id, map, UNITS_ORDER_CLEAR_PLOW, err, err_size, ai_popups, messages
  );
  if (!u) {
    return false;
  }
  if (map_tile_is_plowed(map, u->x, u->y)) {
    if (err && err_size) {
      snprintf(err, err_size, "Already plowed");
    }
    units_pioneer_emit_order_gate(u, ai_popups, messages, "NOPLOW", "");
    return false;
  }
  if (!units_pioneer_tile_can_clear_or_plow(map, u->x, u->y, NULL)) {
    if (err && err_size) {
      snprintf(err, err_size, "Cannot plow here");
    }
    return false;
  }
  if (u->orders != UNITS_ORDER_CLEAR_PLOW) {
    /* bugs.md #620: DOS FUN_2b5a_123e raw 42511-42512 writes only +0x314c;
     * the +0x315a progress counter survives an order change. Only completion
     * (raw 76773 / 76899), fortify (raw 42418) and entering a colony (465b
     * raw 76733) zero it. */
    u->orders = UNITS_ORDER_CLEAR_PLOW;
    if (diag_info_enabled()) {
      char who[96];
      units_log_ident(pool, unit_id, who, sizeof(who));
      bool clearing = false;
      (void)units_pioneer_tile_can_clear_or_plow(map, u->x, u->y, &clearing);
      diag_info(
        "ORDER %s: %s tile (%d,%d) tools=%d",
        who,
        clearing ? "clear forest" : "plow fields",
        u->x, u->y, u->tools
      );
    }
  }
  return units_pioneer_work_tick_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, unit_id, err, err_size, ai_popups, messages);
}

bool units_pioneer_road_w(
  const ColonizeWorld* w,
  int unit_id,
  char* err,
  size_t err_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  ColonizeUnitPool* pool = w->units;
  ColonizeWorldMap* map = w->map;
  ColonizeColonyPool* colonies = w->colonies;

  ColonizeUnit* u = units_pioneer_begin_order(
    pool, unit_id, map, UNITS_ORDER_BUILD_ROAD, err, err_size, ai_popups, messages
  );
  if (!u) {
    return false;
  }
  if (!map_tile_is_land(map, u->x, u->y) || map_tile_is_high_seas(map, u->x, u->y)) {
    if (err && err_size) {
      snprintf(err, err_size, "Cannot build road here");
    }
    return false;
  }
  /* bugs.md #618: DOS FUN_2b5a_1454 raw 42552-42555 tests `layer2 & 0x0a`
   * = road OR settlement before raising @NOROAD, so Build Road is never
   * offered on a colony/village tile. */
  if (units_pioneer_road_mask(map, colonies, u->x, u->y)) {
    if (err && err_size) {
      popup_msg_fill(messages, "NOROAD", NULL, "", err, err_size);
      popup_msg_strip_markup(err);
    }
    units_pioneer_emit_order_gate(u, ai_popups, messages, "NOROAD", "");
    return false;
  }
  if (u->orders != UNITS_ORDER_BUILD_ROAD) {
    /* bugs.md #620: FUN_2b5a_1454 raw 42602-42603 writes only +0x314c. */
    u->orders = UNITS_ORDER_BUILD_ROAD;
    if (diag_info_enabled()) {
      char who[96];
      units_log_ident(pool, unit_id, who, sizeof(who));
      diag_info("ORDER %s: build road tile (%d,%d) tools=%d", who, u->x, u->y, u->tools);
    }
  }
  return units_pioneer_work_tick_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, unit_id, err, err_size, ai_popups, messages);
}

bool units_adjacent(int ax, int ay, int bx, int by) {
  const int dx = ax - bx;
  const int dy = ay - by;
  if (dx == 0 && dy == 0) {
    return false;
  }
  return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}
