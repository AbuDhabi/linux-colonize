#include "core/internal.h"
#include "core/ai_contact.h"
#include "core/ai_contact_internal.h"

/*
 * Sections:
 *  - Contact bookkeeping: visit tracking, status/chrome text & tribe/euro names (~line 96)
 *  - Peace state, land-grant welcome dialogs & first-contact welcome flow (~line 300)
 *  - Encounter/meet scans, village-meet dialogs & attack/raid confirmations (~line 750)
 *  - Ship-village visits & jesuit/teachable checks (~line 1403)
 *
 * The remaining bands moved into split siblings (2026-09-23); the cross-file
 * seams are declared in ai_contact_internal.h:
 *  - ai_contact_demand.c  friction/gift-gold, demand pipeline, visit mood, beg-food,
 *                         incite, missionary/WoI defect & relation tick
 *  - ai_contact_trade.c   village gift exchange, reparations, 2820/2e92 haggle
 *  - ai_contact_raid.c    loot scoring, raid execution, scout displacement, war tick
 *  - ai_contact_actions.c DOS village action handlers & menu/popup dispatch
 */

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

int ai_contact_s_last_raid_kind = AI_RAID_NOTHING;
char ai_contact_s_last_burn_building[48];
char ai_contact_s_last_stores_cargo[48];
char ai_contact_s_last_ship_type[48];
int ai_contact_s_last_gold_drained;

/*
 * VISIT pacing (no turn cooldown — DOS has none; audited 2026-09-10).
 * DOS's FUN_5bfb_022e is a single encounter that picks exactly one of
 * gift / beg / demand off `bVar6` (viceroy_unpacked.c 96723-96731:
 * `bVar6 = local_10 == 0 && bVar5`, then the demand half at LAB_5bfb_0def is
 * guarded by `if (!bVar6)`), and the generous half stamps `contact_state = 2`
 * which is the demand half's own `!= 2` gate. Its cadence comes from four
 * gates the port carries in full:
 *  - the move-tail trigger: a Brave must WALK UP to the colony this turn
 *    (ai_contact_brave_walked_up_to, all three arms);
 *  - once per nation per move-tail pass (`aiStack_20[nation]`, viceroy
 *    98653-98676) — the port's once-per-nation-per-turn arm order in ai.c §9
 *    (gifts first; a resolved gift or beg returns before the next arm);
 *  - the `contact_state` latch (2 = generous resolved blocks demand, 1 = past
 *    demand blocks gifts), cleared EVERY TURN at the top of FUN_4d56_1b3a
 *    (viceroy 81704-81707, ai_indian_midpass_clear_tables);
 *  - the alarm ceiling (`> 0x4a` bails the whole visit) and the mood RNG.
 * An earlier port (through 2026-09-09) added an invented 8-turn per-nation
 * throttle on top; bugs.md #417's defect (demand at alarm 0 right after a
 * gift) was really the per-arm split of that throttle bypassing the mood
 * roll, and the structural gift-before-demand order above is the DOS-real
 * fix. Do not reintroduce a cooldown.
 */


/*
 * One encounter per Brave per turn (bugs.md: "attacked my colony, and then
 * gave me gifts in the same interaction").
 *
 * DOS has no separate "visit" and "raid" passes: FUN_5bfb_35xx's move tail
 * walks the stepped-onto tile ONCE (viceroy 98628-98690) and picks a single
 * limb off the war test — `FUN_281f_0768(mover, neighbour)` non-zero takes
 * the hostile limb, zero takes the encounter limb that ends in
 * thunk_FUN_2a1f_066c = FUN_5bfb_022e (gift / beg / demand), latched
 * per-nation in `aiStack_20[nation]`. The port splits those limbs across
 * ai.c §9's arm order, so the SAME Brave could be picked up by the gift arm
 * and then again by the raid pulse in one nation-turn. This latch restores
 * the one-limb rule: a Brave that resolved a peaceful visit is off the table
 * for the raid pulse this turn.
 */
int ai_contact_s_visit_brave_id[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
int ai_contact_s_visit_brave_turn[8];
/* ===================== Contact bookkeeping: visit tracking, status/chrome text & tribe/euro names (ai_contact_mark_visit_brave .. ai_contact_euro_name) ===================== */


void ai_contact_mark_visit_brave(
  const ColonizeTurnContext* ctx, int nation_id, int brave_id
) {
  if (!ctx || nation_id < 4 || nation_id > 11 || brave_id < 0) {
    return;
  }
  ai_contact_s_visit_brave_id[nation_id - 4] = brave_id;
  ai_contact_s_visit_brave_turn[nation_id - 4] = ctx->turn_number ? (int)*ctx->turn_number : 0;
}

int ai_contact_brave_visited_this_turn(
  const ColonizeTurnContext* ctx, int nation_id, int brave_id
) {
  if (!ctx || nation_id < 4 || nation_id > 11 || brave_id < 0) {
    return 0;
  }
  const int turn = ctx->turn_number ? (int)*ctx->turn_number : 0;
  return ai_contact_s_visit_brave_id[nation_id - 4] == brave_id &&
         ai_contact_s_visit_brave_turn[nation_id - 4] == turn;
}

int ai_contact_last_raid_kind(void) {
  return ai_contact_s_last_raid_kind;
}

/* @CARGO display names via the shared NAMES.TXT-backed accessor. */
const char* ai_contact_cargo_name(int cargo_idx) {
  if (cargo_idx < 0 || cargo_idx >= COLONIZE_CARGO_COUNT) {
    return "goods";
  }
  return reports_cargo_display_name(cargo_idx);
}

/* Prefer human Euro for player-facing status chrome (unpark #1 Done structural). */
int ai_contact_euro_is_human(const ColonizeTurnContext* ctx, int e) {
  if (!ctx || e < 0 || e > 3) {
    return 0;
  }
  if (ctx->human_nation >= 0 && ctx->human_nation <= 3) {
    return e == ctx->human_nation;
  }
  if (ctx->col1_ok && ctx->col1) {
    return ctx->col1->player[e].control == 0;
  }
  return 0;
}

void ai_contact_set_status(ColonizeTurnContext* ctx, const char* msg) {
  if (!ctx || !ctx->status || ctx->status_size == 0 || !msg) {
    return;
  }
  snprintf(ctx->status, ctx->status_size, "%s", msg);
  popup_msg_strip_markup(ctx->status); /* status line: no {} coloring */
}

/*
 * Village action menu choice ids. The menu itself is DOS's NAMES.TXT
 * @ACTIONS list (FUN_4d56_4528 human arm, overlay 13 0x478a..0x4bdb):
 *   1 Trade With Village      → TRADE
 *   2 Enter Hostile Village   → ENTER_HOSTILE (thunk_FUN_1000_a5e8)
 *   3 Establish Mission       → MISSION (thunk_FUN_1000_a5dc)
 *   4 Denounce Heresy of …    → HERESY (thunk_FUN_1000_a594)
 *   5 Live Among The Natives  → TEACH (thunk_FUN_1000_a618, param_5 = 0)
 *   6 Ask to Speak With Chief → CHIEF (thunk_FUN_1000_a60c)
 *   7 Incite Indians          → INCITE (FUN_4d56_417e)
 *   8 Demand Tribute          → DEMAND (thunk_FUN_1000_a5f4)
 *   9 Attack Village          → ATTACK (game_loop commits the move)
 *  10 Cancel Action           → LEAVE
 * GIFT has no DOS @ACTIONS row (a gold gift is not a village action; goods
 * gifts live inside the 2820 trade flow) — kept only for the legacy
 * gift-amount CHOICE path, never listed. Cite: indian_actions_menu.md.
 */

/* @LEARNSTAY CHOICE ids (thunk_FUN_1000_a618): 1 = "Then I shall become…". */

/* Gift amount CHOICE ids (CONTACT_GIFT; FUN_5bfb_102a amount stand-in). */

/* Demand amount CHOICE ids (CONTACT_DEMAND; tools vs gold stand-in). */

/*
 * Reparations CHOICE (CONTACT_REPARATIONS; FUN_5bfb_022e LAB_5bfb_0def).
 * DOS's `local_c` is FUN_291f_019c's 1-based row number, and the two
 * GAME.TXT sections print their rows in OPPOSITE order, so the accepting
 * row id is per-flavor — DOS reads `local_c != 2 -> refuse` at the
 * @INDIANCITY site (raw 96895) and `local_c == 1 -> accept` at the
 * @INDIANWAGONS site (raw 96962):
 *
 *   @INDIANCITY   row 1 "Man the stockade."   row 2 "Hand them over."
 *   @INDIANWAGONS row 1 "Hand them over."     row 2 "Circle the wagons."
 */

/* Which of the two demand sites is on offer (CONTACT_REPARATIONS payload). */

/*
 * Trade buy-offer CHOICE ids (CONTACT_TRADE_OFFER; FUN_4d56_2820 LAB_002e92
 * human `iStack_8 != 0` branch — Accept/Decline a locked price instead of
 * the AI's silent auto-accept). Cite: indian_trade_2820.md.
 */

/*
 * FUN_291f_019c(tag, tribe) — every native dialog in DOS goes through the
 * portrait entry point (FUN_6f74_3760 stores the tribe in DS:0x1f5c, then
 * FUN_6f74_0042 builds IND{tribe}A{tier}.SS from it, tier = FUN_281f_0a60's
 * alarm band). DS:0x1f5c is cleared again at LAB_6f74_3018 when the dialog
 * closes, so each popup re-arms it; the port's equivalent is a set on the
 * request that was just enqueued. Call right after any enqueue whose DOS
 * twin is a 019c call with *(0x8d52) — the whole FUN_4d56_2820 village
 * trade/haggle chain and FUN_5bfb_022e's reparations demands.
 */
void ai_contact_chief_flair(ColonizeTurnContext* ctx, int e, int nation_b) {
  if (!ctx || !ctx->ai_popups || !ctx->col1 || nation_b < 4 || nation_b > 11) {
    return;
  }
  const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_b, e);
  ai_popup_set_last_portrait(
    ctx->ai_popups, nation_b - 4, ai_popup_portrait_tier_from_alarm(alarm)
  );
}

/*
 * Human status chrome + optional AI popup OK (keep both). Cite: FUN_5bfb_022e /
 * FUN_4d56_4528 thin arms; unpark #1 dialog widgets.
 */
void ai_contact_human_chrome(
  ColonizeTurnContext* ctx,
  int e,
  AiPopupTag tag,
  int nation_b,
  const char* title,
  const char* body
) {
  if (!ctx || !body || !ai_contact_euro_is_human(ctx, e)) {
    return;
  }
  (void)title;
  ai_contact_set_status(ctx, body);
  if (ctx->ai_popups) {
    ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, tag, e, nation_b, 0, NULL, body
    );
    /* FUN_6f74_0042: DS:0x1f5c = the contact tribe → IND{tribe}A{tier}.SS
     * portrait beside the dialog, tier from the alarm band (P8.6). */
    ai_contact_chief_flair(ctx, e, nation_b);
  }
}

/*
 * "The <tribe> refuse <what>." — the refusal chrome twelve call sites in this
 * file spelled out byte for byte (audit AC-3). `what` is the plural noun the
 * DOS status line uses: "gifts", "demands", "conversion".
 */
void ai_contact_refuse_chrome(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  AiPopupTag tag,
  const char* title,
  const char* what
) {
  char refuse_fb[AI_POPUP_BODY_LEN];
  snprintf(
    refuse_fb, sizeof(refuse_fb), "The %s refuse %s.", ai_contact_tribe_name(nation_id), what
  );
  ai_contact_human_chrome(ctx, e, tag, nation_id, title, refuse_fb);
}

/*
 * NAMES.TXT catalog for the @VALUES lookup below; every other name table in
 * this file now goes through the reports_* accessors (which parse their own
 * NAMES.TXT copy).
 */
const ColonizeMsgCatalog* ai_contact_s_contact_names;

void ai_contact_bind_names(const ColonizeTurnContext* ctx) {
  ai_contact_s_contact_names = (ctx && ctx->names) ? ctx->names : NULL;
}

/*
 * @TRIBES field 1 (singular: Inca, Aztec, ...) — this accessor names one
 * settlement or one brave, never the people as a whole, so it is the
 * singular column. Thin wrapper: game_loop.c and ai_diplo.c call it by name.
 */
const char* ai_contact_tribe_name(int nation_id) {
  const int idx = nation_id - 4;
  if (idx < 0 || idx >= 8) {
    return "natives";
  }
  return reports_tribe_singular_name(idx);
}

/* FUN_5bfb_0182: peace/treaty bit on indian.euro_diplo[euro] (COL1_INDIAN_PEACE_BIT). */

/* FUN_5bfb_022e Yes/No (local_c). */

/* @NATIONALITY (English/French/Spanish/Dutch) via the shared accessor. */
const char* ai_contact_euro_name(int euro_nation) {
  if (euro_nation < 0 || euro_nation > 3) {
    return "Europeans";
  }
  return reports_nation_adjective_display_name(euro_nation);
}
/* ===================== Peace state, land-grant welcome dialogs & first-contact welcome flow (ai_contact_alarm_delta_00f2 .. ai_contact_try_first_welcome) ===================== */


/*
 * FUN_4cc6_00f2 with its escalation tail (raw 80903-80915) + FUN_4cc6_0000:
 * apply the alarm delta (halving/clamp/clears/tension tiers live in
 * ai_diplo_indian_alarm_delta), then — when the pair lands at alarm 100
 * while formally at PEACE — roll rng(0,10) <= cap+1 (cap = difficulty for
 * a human-controlled euro, else 1); on success expel that euro's missions
 * from every tribe of the nation (mission byte → 0xff) and, when any were
 * cleared and the euro is human, show GAME.TXT @INDIANBURN (DS tag 0x14c8,
 * %STRING0 = the Indian nation). Replaces the old Linux "burn at alarm
 * ≥80 every tick" stand-in (2026-09-07d).
 */
void ai_contact_alarm_delta_00f2(
  ColonizeTurnContext* ctx, int nation_id, int euro, int delta
) {
  if (!ctx || !ctx->col1) {
    return;
  }
  ai_diplo_indian_alarm_delta(ctx->col1, nation_id, euro, delta);
  const int idx = nation_id - 4;
  if (euro < 0 || euro > 3 || idx < 0 || idx >= 8) {
    return;
  }
  const ColonizeCol1Indian* ind = &ctx->col1->indian[idx];
  if (ind->alarm_by_player[euro] < 100 ||
      (ind->euro_diplo[euro] & COL1_INDIAN_PEACE_BIT) == 0) {
    return;
  }
  /* Audit note: this read the control byte directly and so ignored the
   * ctx->human_nation override the rest of the file honours. */
  const int human = ai_contact_euro_is_human(ctx, euro);
  const int cap = human ? (int)ctx->col1->head.difficulty : 1;
  if (dos_rng_range(ctx->rng, 0, 10) > cap + 1) {
    return;
  }
  int cleared = 0;
  for (uint16_t ti = 0; ctx->col1->tribe && ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id || t->mission == COL1_TRIBE_MISSION_NONE) {
      continue;
    }
    if ((int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK) == euro) {
      t->mission = COL1_TRIBE_MISSION_NONE;
      cleared = 1;
    }
  }
  if (cleared && human) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_tribe_name(nation_id);
    char fb[AI_POPUP_BODY_LEN];
    snprintf(fb, sizeof(fb), "The %s burn your missions!", tok.string0);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "INDIANBURN", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(
      ctx, euro, AI_POPUP_TAG_CONTACT_RAID, nation_id, "", body
    );
  }
}

int ai_contact_indian_has_peace(
  const ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation
) {
  if (!col1 || euro_nation < 0 || euro_nation > 3) {
    return 0;
  }
  const int idx = indian_nation - 4;
  if (idx < 0 || idx >= 8) {
    return 0;
  }
  return (col1->indian[idx].euro_diplo[euro_nation] & COL1_INDIAN_PEACE_BIT) != 0;
}

/*
 * 2026-09-08: both routed through the dual-mode 15b3 pair ops. DOS keeps
 * Euro↔Indian peace SYMMETRIC: the peace-break is FUN_4cc6_0092's
 * `281f_0a10(tribe+4, euro, 0x40)` = FUN_15b3_00d0 clear-both (decomp
 * 80819), and every DOS save shows nation[].relation_by_indian carrying
 * PEACE (0x60) post-contact, so the set side is symmetric too. The old
 * helpers wrote only the Indian-side euro_diplo byte.
 */
static void ai_contact_set_peace(ColonizeCol1Save* col1, int indian_nation, int euro_nation) {
  if (euro_nation < 0 || euro_nation > 3 || indian_nation < 4 || indian_nation > 11) {
    return;
  }
  ai_diplo_or_both(col1, indian_nation, euro_nation, COL1_INDIAN_PEACE_BIT);
}

void ai_contact_clear_peace(ColonizeCol1Save* col1, int indian_nation, int euro_nation) {
  if (euro_nation < 0 || euro_nation > 3 || indian_nation < 4 || indian_nation > 11) {
    return;
  }
  ai_diplo_clear_both(col1, indian_nation, euro_nation, COL1_INDIAN_PEACE_BIT);
}

/* Settlement count (villages/camps/cities), not braves — bugs.md #3. */
static int ai_contact_nation_settlement_count(const ColonizeTurnContext* ctx, int nation_id) {
  int count = 0;
  if (!ctx || !ctx->col1 || !ctx->col1->tribe) {
    return 0;
  }
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id == nation_id) {
      ++count;
    }
  }
  return count;
}

int ai_contact_welcome_pending(const AiPopupState* st, int e, int nation_id) {
  return ai_popup_pending(
    st, AI_POPUP_TAG_CONTACT_WELCOME, -1, AI_POPUP_KEY_NATION_AB, e, nation_id
  ) ? 1 : 0;
}

/*
 * Mark tile as gifted/purchased tribal land (FUN_281f_068c bit 0x10).
 * Used by WELCOME land grant and colonies_found_with_indian_land spend path.
 */
static void ai_contact_mark_tile_purchased(
  ColonizeCol1Save* col1,
  ColonizeWorldMap* map,
  int x,
  int y
) {
  if (col1 && col1->map.mask && col1->head.map_size_x > 0 && x >= 0 && y >= 0) {
    const size_t idx = (size_t)y * (size_t)col1->head.map_size_x + (size_t)x;
    if (idx < col1->map.tile_count) {
      col1->map.mask[idx] = (uint8_t)(col1->map.mask[idx] | 0x10u);
    }
  }
  if (map && map->layer2 && map_coords_inset(map, x, y)) {
    const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
    if (idx < map->tile_count) {
      map->layer2[idx] = (uint8_t)(map->layer2[idx] | MAP_LAYER2_PURCHASED);
    }
  }
}

/* Euro land unit of e adjacent to a Brave of nation_id (grant / meet apply). */
ColonizeUnit* ai_contact_find_adjacent_euro(
  ColonizeTurnContext* ctx,
  int nation_id,
  int e,
  int* near_x,
  int* near_y
);

/*
 * @INDIANWELCOME land grant: Euro land unit occupying the gifted tile.
 * Prefer Brave adjacency; else tribe adjacency (game_loop first-contact path).
 */
static ColonizeUnit* ai_contact_find_land_grant_unit(
  ColonizeTurnContext* ctx,
  int nation_id,
  int e
) {
  if (!ctx || !ctx->units || e < 0 || e > 3) {
    return NULL;
  }
  ColonizeUnit* by_brave = ai_contact_find_adjacent_euro(ctx, nation_id, e, NULL, NULL);
  if (by_brave && !units_is_sea(ctx->units, by_brave->id)) {
    return by_brave;
  }
  if (!ctx->col1 || !ctx->col1->tribe) {
    return NULL;
  }
  /* Self tile first, then MAP_DIR8 order (9-entry walk, not the shared table). */
  static const int dx9[9] = {0, 0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy9[9] = {0, -1, -1, 0, 1, 1, 1, 0, -1};
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    for (int d = 0; d < 9; ++d) {
      const int oid = units_id_at(ctx->units, (int)t->x + dx9[d], (int)t->y + dy9[d]);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id != e) {
        continue;
      }
      if (units_is_sea(ctx->units, other->id)) {
        continue;
      }
      return other;
    }
  }
  return NULL;
}

/*
 * Thin WELCOME land grant (GAME.TXT: "land you now occupy as a gift").
 * Stamp MAP_LAYER2_PURCHASED + euro owner nibble on the contacting unit tile.
 * Deep DOS grant radius / multi-tile arms remain PARKED.
 */
static void ai_contact_apply_welcome_land_grant(
  ColonizeTurnContext* ctx,
  int nation_id,
  int e
) {
  ColonizeUnit* u = ai_contact_find_land_grant_unit(ctx, nation_id, e);
  if (!u) {
    return;
  }
  ai_contact_mark_tile_purchased(ctx->col1, ctx->map, u->x, u->y);
  if (ctx->map) {
    /* FUN_137f_0228 layer3 owner nibble; shared writer in map.c. */
    map_set_owner_nibble(ctx->map, u->x, u->y, e);
  }
}

void ai_contact_apply_welcome_accept(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e
) {
  if (!ctx || !ctx->col1 || !ind) {
    return;
  }
  (void)ind;
  const uint8_t rel_before = ai_diplo_indian_relation(ctx->col1, nation_id, e);
  ai_contact_set_peace(ctx->col1, nation_id, e);
  /*
   * relation_by_indian is the DOS 0x60 MET|PEACE flag BYTE, not a scalar (every
   * DOS save: 96 once met, 0 before). set_peace above or-boths 0x40; MET 0x20 is
   * the other half, and ai_contact_try_first_welcome only ORs it into the Indian
   * side, so the Euro-side row needs it here. Until 2026-09-10 (audit #16) this
   * was a raw `= 96` assignment that bypassed the 15b3 pair ops — DOS's own
   * surrender/first-contact idiom is or_both, never a store (FUN_43f7_0108,
   * viceroy_unpacked.c:73555-73557).
   */
  ai_diplo_or_both(ctx->col1, nation_id, e, COL1_INDIAN_MET_BIT);
  /* FUN_5bfb first contact (viceroy_unpacked.c:96624): alarm clamped <= 20. */
  if (ind->alarm_by_player[e] > 20u) {
    ind->alarm_by_player[e] = 20u;
  }
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id != nation_id) {
        continue;
      }
      t->alarm[e].friction = 0;
      t->alarm[e].attacks = 0;
    }
  }
  ai_diplo_indian_hostility_sync(ctx->col1, e);

  /* Land grant on occupied tile (copy-only → thin ownership write). */
  ai_contact_apply_welcome_land_grant(ctx, nation_id, e);

  const char* tribe = ai_contact_tribe_name(nation_id);
  const char* euro = ai_contact_euro_name(e);
  PopupMsgTokens peace_tok;
  memset(&peace_tok, 0, sizeof(peace_tok));
  peace_tok.string0 = tribe;
  peace_tok.string1 = euro;
  char peace_fb[AI_POPUP_BODY_LEN];
  peace_fb[0] = '\0';
  char peace_body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    ctx->messages, "INDIANPEACE", &peace_tok, peace_fb, peace_body, sizeof(peace_body)
  );
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "", peace_body);

  /* DOS FUN_5bfb_0182: @INDIANCOME when relation < 0x19 before/as friendly. */
  if (rel_before < 25u) {
    PopupMsgTokens come_tok;
    memset(&come_tok, 0, sizeof(come_tok));
    come_tok.string0 = tribe;
    char come_fb[AI_POPUP_BODY_LEN];
    come_fb[0] = '\0';
    char come_body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages, "INDIANCOME", &come_tok, come_fb, come_body, sizeof(come_body)
    );
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "", come_body);
  }

  /*
   * DOS FUN_5bfb_022e first-contact arm ends after 0182 (peace / optional
   * COME) — goto LAB_5bfb_1005. Meet CHOICE / gift / trade is later village
   * interaction (PARKED), not chained onto Accept.
   */
}

void ai_contact_apply_welcome_reject(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e
) {
  if (!ctx || !ctx->col1 || !ind) {
    return;
  }
  ai_contact_clear_peace(ctx->col1, nation_id, e);
  /*
   * DOS +100 hostility → Linux at-war band (0 < relation < 26, i.e. alarm
   * > 0x4a; the band constant is AI_DIPLO_INDIAN_AT_WAR_REL, not the 50 this
   * comment claimed until 2026-09-09, smell #57). Write a hostile floor (1),
   * not unmet 0 — seed-100 early goldens keep r==0/sticky clear until first
   * contact. Cite: FUN_4cc6_00f2; indian_contact.md.
   */
  ai_contact_alarm_delta_00f2(ctx, nation_id, e, 100); /* DOS +100 hostility */
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id != nation_id) {
        continue;
      }
      if (t->alarm[e].friction < 80u) {
        t->alarm[e].friction = 80u;
      }
      /* (Retired 2026-09-08, smell #66.) An `attacks++` across every tribe of
       * the nation sat here. The attacks byte is the attitude word's high half
       * (col1_tribe_attitude); DOS's only writer is the per-settlement 465b
       * trespass bump (units.c ~6781), and 022e's @INDIANSHUN reject limb
       * touches nothing but the +100 hostility (FUN_4cc6_00f2). */
    }
  }
  ai_diplo_indian_hostility_sync(ctx->col1, e);

  const char* tribe = ai_contact_tribe_name(nation_id);
  PopupMsgTokens shun_tok;
  memset(&shun_tok, 0, sizeof(shun_tok));
  shun_tok.string0 = tribe;
  char shun_fb[AI_POPUP_BODY_LEN];
  shun_fb[0] = '\0';
  char shun_body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    ctx->messages, "INDIANSHUN", &shun_tok, shun_fb, shun_body, sizeof(shun_body)
  );
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, NULL, shun_body);
}

static void ai_contact_enqueue_welcome(ColonizeTurnContext* ctx, int e, int nation_id) {
  if (!ctx || !ctx->ai_popups || !ai_contact_euro_is_human(ctx, e)) {
    return;
  }
  if (ai_contact_welcome_pending(ctx->ai_popups, e, nation_id)) {
    return;
  }
  const char* tribe = ai_contact_tribe_name(nation_id);
  const int settlements = ai_contact_nation_settlement_count(ctx, nation_id);
  const int shown = settlements > 0 ? settlements : 1;
  /* FUN_5bfb_022e 5bfb:0325: from turn 20 (DS:0x538e ≥ 0x14) the meet
   * switches the tune pool — 5 Natives, Inca → 7 (Cuzco), Aztec → 6
   * (Tenochtitlan); 04ac vs 0498 only differ in option gating.
   * DOS keys both this and the woodcut below on `*(int *)0x8d52`, the tribe
   * SLOT (0 = Inca, 1 = Aztec) — the same value that indexes DS:0x962a at
   * `*0x8d52 + -0x69d6`. This read `nation_id`, which is the 4..11 Col1 id,
   * so neither branch could ever be taken (fixed with bugs.md #416). */
  const int tribe_slot = nation_id - 4;
  if (ctx->col1 && ctx->col1->head.turn >= 20) {
    sound_set_bgm(tribe_slot == 0 ? 7 : (tribe_slot == 1 ? 6 : 5));
  }
  /*
   * FUN_5bfb_022e 5bfb:038a, right before the @INDIANWELCOME dialog: the
   * tribe SLOT picks the woodcut — DOS passes FUN_281f_0524 5 for slot 0
   * (Inca), 4 for slot 1 (Aztec), 3 otherwise, which are exactly
   * WOODCUT_THE_INCA_NATION / _THE_AZTEC_EMPIRE / _MEETING_THE_NATIVES.
   * Keyed on `nation_id` (4..11) until 2026-09-09, so the Aztec and Inca
   * cinematics were unreachable.
   */
  (void)woodcut_fire(
    ctx->col1,
    tribe_slot == 0   ? WOODCUT_THE_INCA_NATION
    : tribe_slot == 1 ? WOODCUT_THE_AZTEC_EMPIRE
                      : WOODCUT_MEETING_THE_NATIVES
  );
  PopupMsgTokens welcome_tok;
  memset(&welcome_tok, 0, sizeof(welcome_tok));
  welcome_tok.string0 = tribe;
  welcome_tok.string1 = shown == 1 ? "settlement" : "settlements";
  welcome_tok.number0 = shown;
  welcome_tok.has_number0 = true;
  char fb[AI_POPUP_BODY_LEN];
  fb[0] = '\0';
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    ctx->messages, "INDIANWELCOME", &welcome_tok, fb, body, sizeof(body)
  );
  /* GAME.TXT @INDIANWELCOME choice rows: "Yes" / "No" (bugs.md #541). */
  char choice_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(
    ctx->messages, "INDIANWELCOME", &welcome_tok, "", "", choice_buf, labels
  );
  static const int ids[] = {AI_CONTACT_WELCOME_YES, AI_CONTACT_WELCOME_NO};
  ai_popup_enqueue_choice_ctx(
    ctx->ai_popups,
    AI_POPUP_TAG_CONTACT_WELCOME,
    e,
    nation_id,
    0,
    NULL,
    body,
    labels,
    ids,
    2
  );
  /* @INDIANWELCOME is a chief audience: IND{tribe}A{tier} portrait (P8.6). */
  if (nation_id >= 4 && nation_id <= 11) {
    const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
    ai_popup_set_last_portrait(
      ctx->ai_popups, nation_id - 4, ai_popup_portrait_tier_from_alarm(alarm)
    );
  }
  {
    char st[96];
    snprintf(st, sizeof(st), "The %s offer peace.", tribe);
    ai_contact_set_status(ctx, st);
  }
}

int ai_contact_try_first_welcome(ColonizeTurnContext* ctx, int euro_nation, int indian_nation) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || euro_nation < 0 || euro_nation > 3) {
    return 0;
  }
  ai_contact_bind_names(ctx);
  if (indian_nation < 4 || indian_nation > 11) {
    return 0;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[indian_nation - 4];
  if (ind->euro_diplo[euro_nation]) {
    return 0;
  }
  /* DOS OR bit 0x20 before dialog; accept ORs PEACE 0x40 → euro_diplo 0x60
   * (96 in seed-100 TURN3+ goldens). Relation set by accept (96) / reject (1).
   * raw 96619: `caseD_10(euro, indian, 0x20)` = FUN_15b3_0066 or_both, so the
   * Euro-side row gets MET here too, reject or not (2026-09-17). */
  ai_diplo_or_both(ctx->col1, indian_nation, euro_nation, COL1_INDIAN_MET_BIT);

  if (ai_contact_euro_is_human(ctx, euro_nation) && ctx->ai_popups) {
    ai_contact_enqueue_welcome(ctx, euro_nation, indian_nation);
    return 1;
  }
  /* AI Euro / no popups: auto-accept (DOS local_c = 1). */
  ai_contact_apply_welcome_accept(ctx, ind, indian_nation, euro_nation);
  return 1;
}
/* ===================== Encounter/meet scans, village-meet dialogs & attack/raid confirmations (ai_contact_encounter_scan .. ai_contact_try_tired_attack_confirm) ===================== */


int ai_contact_encounter_scan(ColonizeTurnContext* ctx, int euro_nation, int x, int y) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->map || euro_nation < 0 || euro_nation > 3) {
    return 0;
  }
  /* DOS DS:0xb4 / DS:0xbe direction tables (N, NE, E, SE, S, SW, W, NW). */
  int opened = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = x + MAP_DIR8_DX[d];
    const int ny = y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    int other = -1;
    /*
     * FUN_137f_03e4 tile_tribe_owner: owner nibble only when bit 0x02 set.
     *
     * bugs.md #416: the nibble is a STAMP, not a record — DOS FUN_1427_02ca
     * rewrites it with the mover's own nation on every step, and it survives
     * the unit leaving (FUN_1427_023a clears presence only). In DOS a unit
     * never stands on a settlement tile it does not own, so on a `& 2` tile
     * the stamp is always the settlement's owner; the port's AI walkers do
     * cross settlement tiles, and a single Brave that passed through left the
     * tile reading as ITS nation for good. Any Euro land unit that later
     * stepped beside that settlement then "met" a tribe whose villages were
     * nowhere near (the user's Aztec, playing France). Resolve the owner from
     * the settlement records instead — same value DOS reads, without the
     * stale-stamp aliasing. A Euro colony tile resolves to no tribe at all.
     */
    const int i = ny * ctx->map->width + nx;
    if (ctx->map->layer2 && ctx->map->layer3 && (ctx->map->layer2[i] & 0x02u) != 0) {
      const int hi = (ctx->map->layer3[i] >> 4) & 0x0f;
      other = hi == 0x0f ? -1 : hi;
      if (other >= 4 && other <= 11) {
        int real = -1;
        if (ctx->col1->tribe) {
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->x == nx && (int)t->y == ny) {
              real = (int)t->nation_id;
              break;
            }
          }
        }
        other = real; /* no village here → the nibble was a passing stamp */
      }
    }
    /* FUN_281f_07e0 unit_index_on_tile overrides the land owner. */
    const int oid = ctx->units ? units_id_at(ctx->units, nx, ny) : -1;
    if (oid >= 0) {
      const ColonizeUnit* ou = units_get_const(ctx->units, oid);
      if (ou) {
        other = ou->nation_id;
      }
    }
    if (other < 4 || other > 11) {
      continue;
    }
    opened += ai_contact_try_first_welcome(ctx, euro_nation, other);
  }
  return opened;
}

static int ai_contact_meet_choice_pending(const AiPopupState* st, int e, int nation_id) {
  return ai_popup_pending(
    st, AI_POPUP_TAG_CONTACT_MEET, AI_POPUP_KIND_CHOICE,
    AI_POPUP_KEY_NATION_AB, e, nation_id
  ) ? 1 : 0;
}

/*
 * Acting-unit classification for the DOS @ACTIONS gating. DOS reads the
 * unit-type byte (0x3146: 5 = Scouts, 3 = Missionaries, 0xc = Wagon Train,
 * 0xd..0x12 = ships), the type's attack column (0x5236) and the profession
 * byte (0x315b: 0x1b = Indian Convert). Linux keys the same facts off the
 * @UNIT type name / domain.
 */

static void ai_contact_classify_unit(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u,
  AiContactUnitClass* out
) {
  memset(out, 0, sizeof(*out));
  if (!units || !u) {
    return;
  }
  const ColonizeUnitType* t = units_type(units, u->type_index);
  out->is_ship = units_is_sea(units, u->id) ? 1 : 0;
  out->is_wagon = units_type_is_wagon(t) ? 1 : 0;
  out->is_scout = combat_type_is_scout(t);
  /*
   * @ACTIONS row gating (Mission / Heresy / Live Among / Incite) is by @UNIT
   * KIND — any blessed missionary gets those rows. Not to be confused with
   * the incite -1500 discount, which FUN_4d56_417e raw 83579-83580 keys on
   * the PROFESSION byte (+0x315b == 0x18, Jesuit) — that flag is passed in
   * by the caller (game_loop's `profession == UNITS_JOB_MISSIONARY`) and
   * travels in the CHOICE payload bit0. bugs.md #587.
   */
  out->is_missionary = units_type_is_missionary(t) ? 1 : 0;
  out->attack = t ? t->attack : 0;
  const int colonist_class =
    !out->is_ship && !out->is_wagon && !out->is_scout && !out->is_missionary &&
    !units_type_is_treasure(t) && !units_type_is_artillery(t);
  /*
   * FUN_1000_8d68 → FUN_15eb_0902: DS:0x30e default profession by unit type
   * = {19,21,20,24,23,22,-1,23,-1,21,-1…} — ≥ 0 only for Colonists, Soldiers,
   * Pioneers, Missionaries, Dragoons, Scouts, Cont. Cavalry, Cont. Army. With
   * the attack < 2 / not-Scout / not-Missionary gates that is exactly the
   * colonist-class name test above (Regulars/Cavalry fall to attack ≥ 2).
   *
   * bugs.md #726: the DOS row-5 gate (asm OVL13:0x49e0-0x4a3f) ends with a
   * fourth test, `FUN_1000_8d68(unit) != 0x1b`, which the port used to spell
   * as "profession != Convert". 8d68 reads the TYPE-default table DS:0x30e,
   * never the unit's +0x315b profession byte, and that table holds no 0x1b —
   * so the DOS test can never fire and a Convert DOES get Live Among Natives
   * (thunk_FUN_1000_a618 then answers @TEACHCONVERT). Gate removed.
   */
  out->can_live_among = colonist_class && out->attack < 2;
}

/* Meet payload: bit0 is_missionary, bit1 is_capital, bits 2.. = unit id + 1. */
static int ai_contact_meet_payload(int is_missionary, int is_capital, int unit_id) {
  return (is_missionary ? 1 : 0) | (is_capital ? 2 : 0) | ((unit_id + 1) << 2);
}

int ai_contact_meet_payload_unit(int payload) {
  return (payload >> 2) - 1;
}

/* NAMES.TXT @LEVELS column 1 by tribe tech (DS:0x9634 + tech*6): Camp/Village/City. */
const char* ai_contact_level_noun(const ColonizeTurnContext* ctx, int tech) {
  static const char* k_fallback[4] = {"", "", "", ""};
  static char live[32];
  if (tech < 0) {
    tech = 0;
  }
  if (tech > 3) {
    tech = 3;
  }
  if (ctx && assets_msg_row_field(ctx->names, "LEVELS", tech, 1, live, sizeof(live))) {
    return live;
  }
  return k_fallback[tech];
}

/* NAMES.TXT @ACTIONS row (0-based). %F = the rival nation adjective. */
static const char* ai_contact_action_label(
  const ColonizeTurnContext* ctx,
  int row,
  const char* rival_adj,
  char* out,
  size_t out_size
) {
  /* NAMES.TXT @ACTIONS is the only source; a missing row reads empty. */
  const char* src = "";
  if (ctx) {
    src = assets_msg_line_or(ctx->names, "ACTIONS", row, src);
  }
  size_t n = 0;
  for (const char* c = src; *c && n + 1 < out_size; ++c) {
    if (c[0] == '%' && c[1] == 'F') {
      const char* a = rival_adj ? rival_adj : "foreign";
      while (*a && n + 1 < out_size) {
        out[n++] = *a++;
      }
      c++;
      continue;
    }
    out[n++] = *c;
  }
  out[n] = '\0';
  return out;
}

/*
 * FUN_4d56_4528 human arm (overlay 13 LAB_478a..0x4bdb): the village action
 * menu. Body = GAME.TXT "VILLAGE" + {WAR ≥75 | BAD ≥50 | MEDIUM ≥25 or a
 * tribe.alarm[e] word ≥ 0x80 | SAVAGE (Arawak, slot 2) | HAPPY} with
 * %STRING0 = @LEVELS noun, %STRING1 = tribe. Rows are enabled exactly as
 * the DOS gating does (see ai_contact_classify_unit); no unit known (legacy
 * callers) → Trade / Live Among / Incite / Cancel.
 */
void ai_contact_enqueue_village_meet(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  int is_missionary,
  int is_capital,
  int unit_id,
  int tribe_index
) {
  if (!ctx || !ctx->ai_popups || !ctx->col1) {
    return;
  }
  const char* tribe = ai_contact_tribe_name(nation_id);
  const ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
  const ColonizeUnit* u = (ctx->units && unit_id >= 0) ? units_get_const(ctx->units, unit_id) : NULL;
  if (u && !u->active) {
    u = NULL;
  }
  /*
   * Village record: the one being entered. Callers that already know it (the
   * move handler matched a settlement at the destination tile) pass its index
   * — the fallback scan below can only look for "a village of this tribe next
   * to the unit", and when the unit happens to stand beside two of them, or
   * beside none, it can land on a different village whose mission state then
   * decides the Establish Mission / Denounce Heresy rows for the wrong place.
   */
  const ColonizeCol1Tribe* village = NULL;
  if (ctx->col1->tribe && tribe_index >= 0 && tribe_index < (int)ctx->col1->head.tribe_count) {
    village = &ctx->col1->tribe[tribe_index];
  } else if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id != nation_id) {
        continue;
      }
      if (!village) {
        village = t;
      }
      if (u && map_chebyshev(t->x, t->y, u->x, u->y) <= 1) {
        village = t;
        break;
      }
    }
  }
  const char* section = "VILLAGEHAPPY";
  if (alarm >= 0x4b) {
    section = "VILLAGEWAR";
  } else if (alarm >= 0x32) {
    section = "VILLAGEBAD";
  } else {
    int word = 0;
    if (village) {
      word = (int)village->alarm[e].friction | ((int)village->alarm[e].attacks << 8);
    }
    if (alarm >= 0x19 || word >= 0x80) {
      section = "VILLAGEMEDIUM";
    } else if (nation_id == 6) {
      section = "VILLAGESAVAGE";
    }
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_level_noun(ctx, (int)ind->tech);
  tok.string1 = tribe;
  char fb[AI_POPUP_BODY_LEN];
  fb[0] = '\0';
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, section, &tok, fb, body, sizeof(body));

  const char* labels[AI_POPUP_CHOICE_MAX];
  int ids[AI_POPUP_CHOICE_MAX];
  static char lbl[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  int n = 0;
  const int met = ind->euro_diplo[e] != 0;
  /*
   * DOS row 3/4 gate reads the settlement's mission byte as *signed*:
   * `+5 < 0` (0x80..0xff) means "no mission", anything else is an owner in the
   * low nibble. 0xff is only the usual spelling of that, so test the sign —
   * a mission byte that is neither 0xff nor a real owner must not be allowed
   * to swallow the Establish Mission row.
   */
  const int foreign_owner =
    (village && (int)(int8_t)village->mission >= 0)
      ? (int)(village->mission & COL1_TRIBE_MISSION_NATION_MASK)
      : -1;
  const char* rival_adj = (foreign_owner >= 0 && foreign_owner <= 3) ? ai_contact_euro_name(foreign_owner) : NULL;
#define AI_CONTACT_MENU_ADD(row, id)                                                    \
  do {                                                                                  \
    if (n < AI_POPUP_CHOICE_MAX) {                                                      \
      labels[n] = ai_contact_action_label(ctx, (row), rival_adj, lbl[n], sizeof(lbl[n])); \
      ids[n] = (id);                                                                    \
      n++;                                                                              \
    }                                                                                   \
  } while (0)
  if (u) {
    AiContactUnitClass cls;
    ai_contact_classify_unit(ctx->units, u, &cls);
    /*
     * DOS-LITERAL FUN_4d56_4528 overlay 13 (asm 0x486b..0x4ad0): the @ACTIONS
     * CHOICE is built by walking the ten fixed rows in order and enabling each
     * (FUN_1000_8212(row) + LAB_1000_9365), so the visible order is always
     * 1 Trade, 2 Enter Hostile, 3 Mission, 4 Heresy, 5 Live Among, 6 Chief,
     * 7 Incite, 8 Demand, 9 Attack, 10 Cancel — never the order the port used
     * to emit them in (bugs.md #501; indian_actions_menu.md "Row enabling").
     * `ai_contact_action_label` takes the 0-based row, so rows are 0..9 here.
     */
    if ((cls.is_wagon || cls.is_ship) && alarm < 0x4b) {
      AI_CONTACT_MENU_ADD(0, AI_CONTACT_CHOICE_TRADE); /* row 1 */
    }
    if ((cls.is_wagon || cls.is_ship) && alarm >= 0x4b) {
      AI_CONTACT_MENU_ADD(1, AI_CONTACT_CHOICE_ENTER_HOSTILE); /* row 2 */
    }
    if (met && cls.is_missionary && foreign_owner < 0) {
      AI_CONTACT_MENU_ADD(2, AI_CONTACT_CHOICE_MISSION); /* row 3 */
    }
    if (met && cls.is_missionary && foreign_owner >= 0 && foreign_owner != e) {
      AI_CONTACT_MENU_ADD(3, AI_CONTACT_CHOICE_HERESY); /* row 4 */
    }
    if (met && !cls.is_missionary && cls.can_live_among) {
      AI_CONTACT_MENU_ADD(4, AI_CONTACT_CHOICE_TEACH); /* row 5 */
    }
    if (cls.is_scout) {
      AI_CONTACT_MENU_ADD(5, AI_CONTACT_CHOICE_CHIEF); /* row 6 */
    }
    if (met && cls.is_missionary) {
      AI_CONTACT_MENU_ADD(6, AI_CONTACT_CHOICE_INCITE); /* row 7 */
    }
    if (met && !cls.is_missionary && cls.attack != 0 && !cls.is_ship) {
      AI_CONTACT_MENU_ADD(7, AI_CONTACT_CHOICE_DEMAND); /* row 8 */
    }
    /* row 9: land unit with attack != 0. DOS adds it early for attack > 1
     * and again at OVL13::004a85 for attack != 0 — that label is also the
     * unmet jump target (OVL13::00493f), so the second add is not met-gated
     * and the two collapse to attack != 0. */
    if (!cls.is_ship && cls.attack != 0) {
      AI_CONTACT_MENU_ADD(8, AI_CONTACT_CHOICE_ATTACK_VILLAGE);
    }
  } else {
    AI_CONTACT_MENU_ADD(0, AI_CONTACT_CHOICE_TRADE);
    AI_CONTACT_MENU_ADD(4, AI_CONTACT_CHOICE_TEACH);
    AI_CONTACT_MENU_ADD(6, AI_CONTACT_CHOICE_INCITE);
  }
  AI_CONTACT_MENU_ADD(9, AI_CONTACT_CHOICE_LEAVE);
#undef AI_CONTACT_MENU_ADD
  ai_popup_enqueue_choice_ctx(
    ctx->ai_popups,
    AI_POPUP_TAG_CONTACT_MEET,
    e,
    nation_id,
    ai_contact_meet_payload(is_missionary, is_capital, unit_id),
    NULL,
    body,
    labels,
    ids,
    n
  );
  ai_contact_set_status(ctx, body);
}

int ai_contact_meet_pending_for_unit(const AiPopupState* st, int unit_id) {
  if (unit_id < 0) {
    return 0;
  }
  return ai_popup_pending_payload(
    st, AI_POPUP_TAG_CONTACT_MEET, AI_POPUP_KIND_CHOICE,
    ai_contact_meet_payload_unit, unit_id
  ) ? 1 : 0;
}

int ai_contact_try_village_meet_unit_at(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int indian_nation,
  int is_missionary,
  int is_capital,
  int unit_id,
  int tribe_index
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || euro_nation < 0 || euro_nation > 3) {
    return 0;
  }
  ai_contact_bind_names(ctx);
  if (indian_nation < 4 || indian_nation > 11) {
    return 0;
  }
  if (!ai_contact_euro_is_human(ctx, euro_nation) || !ctx->ai_popups) {
    return 0;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[indian_nation - 4];
  /*
   * FUN_4d56_4528 human land arm (OVL13::004932..004ad0): the @ACTIONS menu
   * is built for every land unit, met or not — the met test (TEST AL,0x40 at
   * OVL13::00493b) only gates rows 3-8, and the only met-bit abort in the
   * function is the ship head's @DONTKNOWSHIPS. It is also built at any
   * alarm (@VILLAGEWAR body + "Enter Hostile Village" row). Only the legacy
   * no-unit callers keep the old unmet / at-war refusals. (bugs.md #542: the
   * port's Attack/Leave "raid warn" fallback for unmet tribes had no DOS
   * counterpart and was deleted.)
   */
  if (unit_id < 0 && (!ind->euro_diplo[euro_nation] ||
                      ai_diplo_indian_at_war(ctx->col1, euro_nation, indian_nation - 4))) {
    return 0;
  }
  if (ai_contact_meet_choice_pending(ctx->ai_popups, euro_nation, indian_nation) ||
      ai_contact_welcome_pending(ctx->ai_popups, euro_nation, indian_nation)) {
    return 0;
  }
  ai_contact_enqueue_village_meet(
    ctx, euro_nation, indian_nation, is_missionary, is_capital, unit_id, tribe_index);
  return 1;
}

int ai_contact_try_village_meet_unit(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int indian_nation,
  int is_missionary,
  int is_capital,
  int unit_id
) {
  return ai_contact_try_village_meet_unit_at(
    ctx, euro_nation, indian_nation, is_missionary, is_capital, unit_id, -1);
}

int ai_contact_try_village_meet(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int indian_nation,
  int is_missionary,
  int is_capital
) {
  return ai_contact_try_village_meet_unit(ctx, euro_nation, indian_nation, is_missionary, is_capital, -1);
}

void ai_contact_village_open_hostilities(
  ColonizeTurnContext* ctx,
  int indian_nation,
  int euro_nation
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || indian_nation < 4 || indian_nation > 11 ||
      euro_nation < 0 || euro_nation > 3) {
    return;
  }
  /*
   * Same at-war floor as welcome reject (FUN_4cc6_00f2 thin) — the tribe
   * FRICTION floor below, which is what that sibling actually writes.
   *
   * 2026-09-09 (smell #50): a second floor sat here, forcing
   * alarm_by_player >= 80 right after the +100 delta. FUN_4cc6_00f2 halves a
   * positive delta for France (euro 1) and again for Pocahontas (raw
   * 80844-80850, ported in ai_diplo_indian_alarm_delta), so +100 lands as
   * +50 / +25 for those players — and the floor stomped exactly that,
   * handing France and a Pocahontas owner the same alarm as everyone else.
   * The sibling this line claimed to copy floors t->alarm[].friction only.
   */
  ai_contact_clear_peace(ctx->col1, indian_nation, euro_nation);
  ai_contact_alarm_delta_00f2(ctx, indian_nation, euro_nation, 100); /* DOS +100 hostility */
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id != indian_nation) {
        continue;
      }
      if (t->alarm[euro_nation].friction < 80u) {
        t->alarm[euro_nation].friction = 80u;
      }
      /* (Retired 2026-09-08, smell #66.) Same all-tribes `attacks++` as the
       * @INDIANSHUN reject limb above — the attacks byte belongs to the 465b
       * per-settlement trespass bump alone (units.c ~6781). */
    }
  }
  ai_diplo_indian_hostility_sync(ctx->col1, euro_nation);
}

/*
 * FUN_4d56_4528 human warn CHOICE before combatish village enter.
 * Relation-banded body (0x1710…0x172e stand-in). Cite: indian_settlement_4528.md.
 */
static int ai_contact_whack_pending(const AiPopupState* st, int unit_id) {
  return ai_popup_pending(
    st, AI_POPUP_TAG_CONTACT_WHACK, -1, AI_POPUP_KEY_NATION_A, unit_id, 0
  ) ? 1 : 0;
}

int ai_contact_try_whack_confirm(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int indian_nation,
  int unit_id,
  int dest_x,
  int dest_y
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->ai_popups || euro_nation < 0 ||
      euro_nation > 3 || indian_nation < 4 || indian_nation > 11) {
    return 0;
  }
  if (ctx->col1->player[euro_nation].control != 0) {
    return 0; /* DOS: 0x543f[nation] == 0 — human only */
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[indian_nation - 4];
  if (ai_diplo_indian_alarm(ctx->col1, indian_nation, euro_nation) >= 0x4b) {
    return 0; /* already hostile: no question */
  }
  if (ind->euro_diplo[euro_nation] & COL1_INDIAN_ATTACK_CONFIRMED_BIT) {
    return 0;
  }
  if (ai_contact_whack_pending(ctx->ai_popups, unit_id)) {
    return 1;
  }
  const char* tribe = ai_contact_tribe_name(indian_nation);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = tribe;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "WHACKINDIANS", &tok, "", body, sizeof(body));
  /* GAME.TXT @WHACKINDIANS choice rows: "Yes" / "No" (bugs.md #541). */
  char choice_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(ctx->messages, "WHACKINDIANS", &tok, "", "", choice_buf, labels);
  static const int ids[] = {1, 0};
  const int payload = dest_x | (dest_y << 8);
  if (!ai_popup_enqueue_choice_ctx(
        ctx->ai_popups, AI_POPUP_TAG_CONTACT_WHACK, unit_id, indian_nation, payload, NULL, body,
        labels, ids, 2
      )) {
    return 0;
  }
  return 1;
}

/*
 * FUN_465b_0000 Euro-vs-Euro attack gate (viceroy_unpacked.c:75540..75560):
 * DOS never refuses an attack on a Euro peer at peace. With a signed peace
 * treaty (relation & 0x40) it asks @HAVETREATY ("We have signed a peace
 * treaty with the {%STRING0}" — Cancel Action / Break Treaty; result != 2
 * aborts the move); without one the attack simply proceeds and war is
 * declared. Returns 1 when a popup now gates the move (caller must stop),
 * 0 when the move may continue (war was declared here if it had to be).
 */
static int ai_contact_euro_war_pending(const AiPopupState* st, int unit_id) {
  return ai_popup_pending(
    st, AI_POPUP_TAG_CONTACT_EURO_WAR, -1, AI_POPUP_KEY_NATION_A, unit_id, 0
  ) ? 1 : 0;
}

int ai_contact_try_euro_attack_confirm(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int target_nation,
  int unit_id,
  int dest_x,
  int dest_y
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->ai_popups || euro_nation < 0 ||
      euro_nation > 3 || target_nation < 0 || target_nation > 3 ||
      euro_nation == target_nation) {
    return 0;
  }
  if (!ai_contact_euro_is_human(ctx, euro_nation)) {
    return 0;
  }
  if (ai_diplo_at_war(ctx->col1, euro_nation, target_nation)) {
    return 0;
  }
  if (ai_contact_euro_war_pending(ctx->ai_popups, unit_id)) {
    return 1;
  }
  /*
   * bugs.md #382: ONE byte, ONE direction — DOS tests
   * `FUN_281f_0a38(attacker, target) & 0x40` (viceroy_unpacked.c:75545), the
   * attacker's own relation byte, which is exactly what the Foreign Affairs
   * report prints (reports.c reports_foreign_at_war reads
   * nation[viewer].euro_relation[peer]). The port OR'd both directions, so a
   * peer whose byte carried the peace bit while the human's did not made the
   * report say "War" and this gate say "we have signed a peace treaty".
   * (DOS does use the both-direction OR further down, but only to decide
   * whether a war DECLARATION is announced, not whether to prompt.)
   */
  const uint8_t rel = ai_diplo_read(ctx->col1, euro_nation, target_nation);
  if ((rel & AI_DIPLO_PEACE) == 0) {
    /* No treaty: DOS attacks without a prompt — open hostilities and go. */
    ai_diplo_declare_war_ctx(ctx, euro_nation, target_nation);
    return 0;
  }
  const char* name = ai_contact_euro_name(target_nation);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = name;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "HAVETREATY", &tok, "", body, sizeof(body));
  /* GAME.TXT @HAVETREATY choice rows: "Cancel Action." / "Break Treaty." */
  char choice_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(
    ctx->messages, "HAVETREATY", &tok, "", "", choice_buf, labels
  );
  static const int ids[] = {0, 1};
  const int payload = dest_x | (dest_y << 8);
  if (!ai_popup_enqueue_choice_ctx(
        ctx->ai_popups, AI_POPUP_TAG_CONTACT_EURO_WAR, unit_id, target_nation, payload, NULL,
        body, labels, ids, 2
      )) {
    return 0;
  }
  return 1;
}

static int ai_contact_tired_pending(const AiPopupState* st, int unit_id) {
  return ai_popup_pending(
    st, AI_POPUP_TAG_COMBAT_HALF, -1, AI_POPUP_KEY_NATION_A, unit_id, 0
  ) ? 1 : 0;
}

int ai_contact_try_tired_attack_confirm(
  ColonizeTurnContext* ctx,
  int unit_id,
  int dest_x,
  int dest_y
) {
  if (!ctx || !ctx->units || !ctx->ai_popups || unit_id < 0) {
    return 0;
  }
  if (dest_x < 0 || dest_y < 0 || dest_x > 255 || dest_y > 255) {
    return 0;
  }
  const ColonizeUnit* u = units_get_const(ctx->units, unit_id);
  if (!u || !u->active) {
    return 0;
  }
  /* DOS asks only for a human-controlled European attacker (nation < 4 with
   * control 0); everyone else eats the penalty without a dialog. */
  if (u->nation_id < 0 || u->nation_id > 3 || !ai_contact_euro_is_human(ctx, u->nation_id)) {
    return 0;
  }
  const int rem = units_remaining_mp(ctx->units, unit_id);
  if (rem <= 0 || rem >= UNITS_MP_PER_TILE) {
    return 0; /* rested — DOS's `uVar15 < 3` gate */
  }
  if (ai_contact_tired_pending(ctx->ai_popups, unit_id)) {
    return 1;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.number0 = rem;
  tok.has_number0 = true;
  char fb[AI_POPUP_BODY_LEN];
  fb[0] = '\0';
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "HALF", &tok, fb, body, sizeof(body));
  /* @HALF carries its own two rows after the blank line, same as the
   * diplomacy sections — take them when GAME.TXT is loaded. */
  char choice_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  popup_msg_section_labels(
    ctx->messages, "HALF", &tok, "", "", choice_buf, labels
  );
  static const int ids[] = {1, 0};
  const int payload = dest_x | (dest_y << 8);
  if (!ai_popup_enqueue_choice_ctx(
        ctx->ai_popups, AI_POPUP_TAG_COMBAT_HALF, unit_id, rem, payload, NULL, body, labels,
        ids, 2
      )) {
    return 0;
  }
  return 1;
}

/* ===================== Ship-village visits & jesuit/teachable checks (ai_contact_try_ship_village .. ai_contact_is_criminal_learner) ===================== */


/*
 * FUN_4d56_4528 ship head (ASM): unmet met-bit 0x20 → @DONTKNOWSHIPS abort;
 * relation≥0x4b or friction≥0x40 → @MADATSHIPS abort; else fall through to
 * village meet. Ship never enters the tile. Cite: indian_settlement_4528.md.
 */
int ai_contact_try_ship_village(ColonizeTurnContext* ctx, int euro_nation, int x, int y) {
  return ai_contact_try_ship_village_unit(ctx, euro_nation, x, y, -1);
}

/*
 * Ship-contact advisor chrome (audit AC-30): status/popup, then clear the
 * chief portrait — this is the King's advisor speaking, not a native
 * audience — and mirror the line to the status bar for an AI nation.
 */
static void ai_contact_ship_advisor_chrome(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int indian_nation,
  const char* body
) {
  ai_contact_human_chrome(ctx, euro_nation, AI_POPUP_TAG_INFO, indian_nation, "", body);
  if (ctx->ai_popups) {
    ai_popup_set_last_portrait(ctx->ai_popups, -1, 0);
  }
  if (!ai_contact_euro_is_human(ctx, euro_nation)) {
    ai_contact_set_status(ctx, body);
  }
}

int ai_contact_try_ship_village_unit(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int x,
  int y,
  int unit_id
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || euro_nation < 0 ||
      euro_nation > 3) {
    return 0;
  }
  ai_contact_bind_names(ctx);

  const ColonizeCol1Tribe* tribe = col1_save_village_at(ctx->col1, x, y);
  if (!tribe) {
    return 0;
  }
  const int tribe_index = (int)(tribe - ctx->col1->tribe);

  const int indian_nation = (int)tribe->nation_id;
  ColonizeCol1Indian* ind = &ctx->col1->indian[indian_nation - 4];
  const uint8_t diplo = ind->euro_diplo[euro_nation];

  /* Unmet (DOS met bit 0x20 clear) → must contact on land first. */
  if ((diplo & 0x20u) == 0) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      ctx->messages,
      "DONTKNOWSHIPS",
      NULL,
      "",
      body,
      sizeof(body)
    );
    ai_contact_ship_advisor_chrome(ctx, euro_nation, indian_nation, body);
    return 1;
  }

  const int alarm = ai_diplo_indian_alarm(ctx->col1, indian_nation, euro_nation);
  const int friction = (int)tribe->alarm[euro_nation].friction;
  /* ASM: FUN_1000_84fc (alarm) >= 0x4b OR friction >= 0x40 → MADAT. */
  if (alarm >= 0x4b || friction >= 0x40 || ai_diplo_indian_at_war(ctx->col1, euro_nation, indian_nation - 4)) {
    char body[AI_POPUP_BODY_LEN];
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_tribe_name(indian_nation);
    popup_msg_fill(
      ctx->messages,
      "MADATSHIPS",
      &tok,
      "",
      body,
      sizeof(body)
    );
    ai_contact_ship_advisor_chrome(ctx, euro_nation, indian_nation, body);
    return 1;
  }

  /* Narrow mid-relation window: thin Meet CHOICE (land path stand-in). */
  /* Mid band ≥0x32..<0x4b: cooler ship voice, still fall through (Series T). */
  int mid_wary = 0;
  if (alarm > 25 && alarm <= 50 && friction < 0x40) {
    char wary[AI_POPUP_BODY_LEN];
    snprintf(
      wary,
      sizeof(wary),
      "The %s are wary of ships.",
      ai_contact_tribe_name(indian_nation)
    );
    ai_contact_human_chrome(ctx, euro_nation, AI_POPUP_TAG_INFO, indian_nation, "", wary);
    if (!ai_contact_euro_is_human(ctx, euro_nation)) {
      ai_contact_set_status(ctx, wary);
    }
    mid_wary = 1;
  }

  /* Ship contact never carries a Missionary; capital status is real
   * (the specific village record was already resolved above). */
  if (ai_contact_try_village_meet_unit_at(
        ctx, euro_nation, indian_nation, 0, tribe->state.capital, unit_id, tribe_index)) {
    return 1;
  }
  if (!mid_wary) {
    ai_contact_set_status(ctx, "The village will not receive our ships.");
  }
  return 1;
}

/* Isolated from quiet-pulse LCG (seed-100 TURN goldens). */
void ai_contact_local_rng(ColonizeTurnContext* ctx, int nation_id, ColonizeDosRng* out) {
  uint32_t seed = 0xC07Au ^ (uint32_t)(nation_id * 97);
  if (ctx && ctx->turn_number) {
    seed ^= (uint32_t)(*ctx->turn_number) * 0x9E3779B9u;
  }
  if (ctx && ctx->rng_seed) {
    seed ^= ctx->rng_seed * 0x85ebca6bu;
  }
  dos_rng_seed(out, seed ? seed : 1u);
}

void ai_contact_clamp_alarms(ColonizeCol1Indian* ind) {
  if (!ind) {
    return;
  }
  for (int e = 0; e < 4; ++e) {
    /*
     * Linux-only guard: keep the uint16 alarm mirror in band. NOT DOS 1816
     * §4 — that clamp is on `muskets`, see below (mis-mapped until
     * 2026-09-06d).
     *
     * 2026-09-09 (smell #53): the band was 200, while FUN_4cc6_00f2 clamps
     * 0..100 on every write (ai_diplo_indian_alarm_delta) and
     * ai_diplo_indian_alarm clamps 0..100 on every read. That left a 101..200
     * window in which the raw readers in this file — ai_contact_pair_friction
     * and the raid-target gate — saw a different number than every accessor
     * path, so two band tests in the same file disagreed about one pair.
     * 100 is the only value consistent with both.
     */
    if (ind->alarm_by_player[e] > 100) {
      ind->alarm_by_player[e] = 100;
    }
  }
  /*
   * FUN_4d56_1816 §4 (raw viceroy_unpacked.c:81606-81610), ported
   * 2026-09-06d: `cVar3 = *(char *)(indian + 7); if (cVar3 < 0) cVar3 = 0;`
   * — `+7` is `muskets`, and DOS handles it as a **signed** byte throughout
   * (152e's spend test is `'\0' < muskets` too). The WoI defect arm's
   * `muskets = min(muskets, villages) << 2` is what can push it past 0x7f,
   * so this is the guard that catches that overflow the next turn.
   */
  if ((int8_t)ind->muskets < 0) {
    ind->muskets = 0;
  }
}

/*
 * (ai_contact_alarm_bump_amount removed 2026-09-09, smell #72: its last two
 * callers were the ambush raid pulse (retired, smell #65) and the prelude
 * flag-body escalate (retired, smell #72). The Pocahontas/French halving it
 * applied pre-call is DOS-real but belongs INSIDE the alarm delta —
 * FUN_4cc6_00f2, viceroy 80844-80850 — where ai_diplo_indian_alarm_delta
 * already does it; the helper only ever double-counted it.
 * ai_contact_bump_u8_cap100 removed with it — same sole caller.)
 */

/* (ai_contact_bump_u16_cap100 removed 2026-09-08: its last caller was the
 * raid pulse's fandom positive kind bump, retired for DOS 0f14's negative
 * alarm tail — see ai_contact_raid_alarm_tail.) */

/*
 * Peaceful teach-skill stub (5bfb / meet checklist): Free Colonist or Scout
 * adjacent to tribe, low alarm/friction → set Col1 tribe.state.learned and
 * optionally grant a native-teachable profession on the unit.
 * Teach dialog widgets Done structural (ai_popup); VGA chrome PARKED; status thinned.
 * @TRIBES flavor goods are trade chrome (ai_contact_tribe_flavor_good); teach uses
 * cargo / nation_id outdoor maps below.
 */
/* @LEARNCRIMINAL: "we doubt that you will ever be more than a common
 * criminal. The %s will teach you nothing." Petty Criminals are refused
 * outright, distinct from the alarm-based @LEARNMAD refusal. */
int ai_contact_is_petty_criminal(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  /* bugs.md: profession byte, not the display name — a Petty Criminal is a
   * Colonists-type unit whose display name reads "Free Colonist", so the
   * old name test never fired (and Indentured Servants fell into the
   * profession-set "master" refusal instead of learning). */
  (void)units;
  return u->profession == UNITS_JOB_CRIMINAL;
}

/* @LEARNMASTER %STRING1: skill name of an already-expert learner (field job
 * noun when set; else equipment-based display name for Scout/Pioneer/…). */
const char* ai_contact_learner_skill_name(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
) {
  if (!u) {
    return "";
  }
  if (u->profession >= 0 && u->profession < COLONIZE_FIELD_JOB_COUNT) {
    return colony_yield_job_name(u->profession);
  }
  /* Factory / other skills by their @JOB row too (bugs.md #547). */
  const char* job = reports_job_short_name(u->profession);
  if (job && job[0]) {
    return job;
  }
  return units_display_name(units, u);
}

/*
 * bugs.md #573 — RETIRED: the legacy "village adjacency teach pulse"
 * (ai_contact_teach_skill) and its hand-written tribe→skill table
 * (ai_contact_taught_profession, with ai_contact_profession_from_cargo /
 * _from_nation) had no DOS counterpart at all. DOS teaches a skill in
 * exactly one place, `thunk_FUN_1000_a618` (overlays.c 77658-77835) — the
 * "Live Among The Natives" @ACTIONS row on a named acting unit and a named
 * village — which is ported as ai_contact_live_among_natives /
 * ai_contact_a618_skill. The pulse's own rules were invented too: it admitted
 * @JOB 28/19 but excluded Indentured Servants where DOS's learner test is
 * exactly `0x1c || 0x19` (overlays.c 77796), it consumed the village one-shot
 * on refusals, and it ignored the capital exemption. Its only caller was the
 * AI_CONTACT_CHOICE_TEACH fallback for an unbound menu_unit/menu_village,
 * which cannot happen for a real @ACTIONS pick (ai_contact_menu_village falls
 * back to the tribe's first village whenever the payload carries a unit), so
 * that arm is now a no-op.
 */
