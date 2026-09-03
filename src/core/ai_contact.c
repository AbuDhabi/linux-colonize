#include "core/ai_contact.h"

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
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/units.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_last_raid_kind = AI_RAID_NOTHING;
static char s_last_burn_building[48];
static char s_last_stores_cargo[48];
static char s_last_ship_type[48];
static int s_last_gold_drained;

int ai_contact_last_raid_kind(void) {
  return s_last_raid_kind;
}

/* @CARGO display names (colony.h / NAMES.TXT) — same table as ai_king.c's
 * ai_king_cargo_name, duplicated locally to avoid a cross-module dependency
 * for this one status-line lookup. */
static const char* ai_contact_cargo_name(int cargo_idx) {
  static const char* const names[COLONIZE_CARGO_COUNT] = {
    "Food",        "Sugar",  "Tobacco", "Cotton", "Furs",  "Lumber",
    "Ore",         "Silver", "Horses",  "Rum",    "Cigars", "Cloth",
    "Coats",       "Trade Goods", "Tools", "Muskets"
  };
  if (cargo_idx < 0 || cargo_idx >= COLONIZE_CARGO_COUNT) {
    return "goods";
  }
  return names[cargo_idx];
}

static int ai_contact_dist(int x0, int y0, int x1, int y1) {
  const int dx = abs(x0 - x1);
  const int dy = abs(y0 - y1);
  return dx > dy ? dx : dy;
}

/* Prefer human Euro for player-facing status chrome (unpark #1 Done structural). */
static int ai_contact_euro_is_human(const ColonizeTurnContext* ctx, int e) {
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

static void ai_contact_set_status(ColonizeTurnContext* ctx, const char* msg) {
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
enum {
  AI_CONTACT_CHOICE_TRADE = 1,
  AI_CONTACT_CHOICE_GIFT = 2,
  AI_CONTACT_CHOICE_DEMAND = 3,
  AI_CONTACT_CHOICE_TEACH = 4,
  AI_CONTACT_CHOICE_LEAVE = 5,
  AI_CONTACT_CHOICE_INCITE = 6,
  AI_CONTACT_CHOICE_MISSION = 7,
  AI_CONTACT_CHOICE_HERESY = 8,
  AI_CONTACT_CHOICE_CHIEF = 9,
  AI_CONTACT_CHOICE_ENTER_HOSTILE = 10,
  AI_CONTACT_CHOICE_ATTACK_VILLAGE = AI_CONTACT_CHOICE_ATTACK
};

/* @LEARNSTAY CHOICE ids (thunk_FUN_1000_a618): 1 = "Then I shall become…". */
enum {
  AI_CONTACT_LEARNSTAY_YES = 1,
  AI_CONTACT_LEARNSTAY_NO = 2
};

/* Village raid warn CHOICE ids (FUN_4d56_4528; Attack Village ACTIONS). */
enum {
  AI_CONTACT_VILLAGE_LEAVE = 0,
  AI_CONTACT_VILLAGE_ATTACK = 1
};

/* Gift amount CHOICE ids (CONTACT_GIFT; FUN_5bfb_102a amount stand-in). */
enum {
  AI_CONTACT_GIFT_SMALL = 1,    /* −5 gold, friction −1 */
  AI_CONTACT_GIFT_LARGE = 2,    /* −10 gold, friction −2 */
  AI_CONTACT_GIFT_GENEROUS = 3  /* −20 gold, friction −3 (deep amount arm thin) */
};

/* Demand amount CHOICE ids (CONTACT_DEMAND; tools vs gold stand-in). */
enum {
  AI_CONTACT_DEMAND_TOOLS = 1, /* −10 tools (stock/unit ≥20), friction −3 */
  AI_CONTACT_DEMAND_GOLD = 2   /* −15 gold (treasury ≥50), friction −3 */
};

/*
 * Trade buy-offer CHOICE ids (CONTACT_TRADE_OFFER; FUN_4d56_2820 LAB_002e92
 * human `iStack_8 != 0` branch — Accept/Decline a locked price instead of
 * the AI's silent auto-accept). Cite: indian_trade_2820.md.
 */
enum {
  AI_CONTACT_TRADE_OFFER_ACCEPT = 1,
  AI_CONTACT_TRADE_OFFER_DECLINE = 2,

  AI_CONTACT_TRADE_OFFER_HAGGLE = 3, /* @TRADE0 fairer-price arm (LAB_002bbc iStack_5e == 2) */

  AI_CONTACT_TRADE_OFFER_GIFT = 4 /* @TRADE0 gift arm (iStack_5e == 3, round 0) */
};

/*
 * Human status chrome + optional AI popup OK (keep both). Cite: FUN_5bfb_022e /
 * FUN_4d56_4528 thin arms; unpark #1 dialog widgets.
 */
static void ai_contact_human_chrome(
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
    if (nation_b >= 4 && nation_b <= 11 && ctx->col1) {
      const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_b, e);
      ai_popup_set_last_portrait(
        ctx->ai_popups, nation_b - 4, ai_popup_portrait_tier_from_alarm(alarm)
      );
    }
  }
}

/* @TRIBES order (Inca..Tupi); matches col1_bridge encounter labels. */
static const ColonizeMsgCatalog* s_contact_names;

static void ai_contact_bind_names(const ColonizeTurnContext* ctx) {
  s_contact_names = (ctx && ctx->names) ? ctx->names : NULL;
}

const char* ai_contact_tribe_name(int nation_id) {
  static char live[32];
  static const char* k_names[8] = {
      "Inca", "Aztec", "Arawak", "Iroquois", "Cherokee", "Apache", "Sioux", "Tupi"
  };
  const int idx = nation_id - 4;
  if (idx < 0 || idx >= 8) {
    return "natives";
  }
  if (s_contact_names) {
    const ColonizeMsgSection* tribes = assets_msg_find(s_contact_names, "TRIBES");
    if (tribes) {
      int row = 0;
      for (int i = 0; i < tribes->line_count; ++i) {
        const char* line = tribes->lines[i];
        if (!line || line[0] == '\0' || line[0] == ';') {
          continue;
        }
        if (row == idx) {
          /* Field 2 = short name. */
          const char* p = strchr(line, ',');
          if (p) {
            ++p;
            while (*p == ' ' || *p == '\t') {
              ++p;
            }
            size_t n = 0;
            while (*p && *p != ',' && n + 1 < sizeof(live)) {
              live[n++] = *p++;
            }
            while (n > 0 && (live[n - 1] == ' ' || live[n - 1] == '\t')) {
              --n;
            }
            live[n] = '\0';
            if (live[0]) {
              return live;
            }
          }
          break;
        }
        row++;
      }
    }
  }
  return k_names[idx];
}

/* FUN_5bfb_0182: peace/treaty bit on indian.euro_diplo[euro] (COL1_INDIAN_PEACE_BIT). */

/* FUN_5bfb_022e Yes/No (local_c). */
enum {
  AI_CONTACT_WELCOME_YES = 1,
  AI_CONTACT_WELCOME_NO = 2
};

static const char* ai_contact_euro_name(int euro_nation) {
  static const char* k_euro[4] = {"English", "French", "Spanish", "Dutch"};
  if (euro_nation < 0 || euro_nation > 3) {
    return "Europeans";
  }
  return k_euro[euro_nation];
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

static void ai_contact_set_peace(ColonizeCol1Save* col1, int indian_nation, int euro_nation) {
  if (!col1 || euro_nation < 0 || euro_nation > 3) {
    return;
  }
  const int idx = indian_nation - 4;
  if (idx < 0 || idx >= 8) {
    return;
  }
  col1->indian[idx].euro_diplo[euro_nation] =
    (uint8_t)(col1->indian[idx].euro_diplo[euro_nation] | COL1_INDIAN_PEACE_BIT);
}

static void ai_contact_clear_peace(ColonizeCol1Save* col1, int indian_nation, int euro_nation) {
  if (!col1 || euro_nation < 0 || euro_nation > 3) {
    return;
  }
  const int idx = indian_nation - 4;
  if (idx < 0 || idx >= 8) {
    return;
  }
  col1->indian[idx].euro_diplo[euro_nation] =
    (uint8_t)(col1->indian[idx].euro_diplo[euro_nation] & (uint8_t)~COL1_INDIAN_PEACE_BIT);
}

void ai_contact_indian_capital_surrender(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation
) {
  /* Thin wrapper — body lives in ai_diplo (units fallout link). */
  ai_diplo_indian_capital_surrender(col1, indian_nation, euro_nation);
}

/*
 * Pull GAME.TXT @SECTION body via popup_msg_fill (tokens expanded).
 * Falls back to fallback_body when catalog missing.
 */
static void ai_contact_msg_body(
  const ColonizeMsgCatalog* messages,
  const char* section,
  const PopupMsgTokens* tok,
  const char* fallback_body,
  char* out,
  size_t out_size
) {
  popup_msg_fill(messages, section, tok, fallback_body, out, out_size);
}

/* Settlement count (villages/camps/cities), not braves — bugs.md item 3. */
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

static int ai_contact_welcome_pending(const AiPopupState* st, int e, int nation_id) {
  if (!st) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_CONTACT_WELCOME && st->queue[i].nation_a == e &&
        st->queue[i].nation_b == nation_id) {
      return 1;
    }
  }
  if (st->open && st->current.tag == AI_POPUP_TAG_CONTACT_WELCOME && st->current.nation_a == e &&
      st->current.nation_b == nation_id) {
    return 1;
  }
  return 0;
}

/*
 * FUN_137f_0228 stand-in: stamp layer3 owner high nibble (0..14; 0xf unowned).
 * Cite: ai_set_owner_nibble / units_map_set_owner_nibble.
 */
static void ai_contact_set_owner_nibble(ColonizeWorldMap* map, int x, int y, int nation_or_ff) {
  if (!map || !map->layer3 || !map_coords_inset(map, x, y)) {
    return;
  }
  const size_t i = (size_t)y * (size_t)map->width + (size_t)x;
  if (i >= map->tile_count) {
    return;
  }
  const uint8_t low = (uint8_t)(map->layer3[i] & 0x0fu);
  const uint8_t hi = (uint8_t)(((unsigned)nation_or_ff & 0x0fu) << 4);
  map->layer3[i] = (uint8_t)(low | hi);
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
static ColonizeUnit* ai_contact_find_adjacent_euro(
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
  static const int dx[9] = {0, 0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[9] = {0, -1, -1, 0, 1, 1, 1, 0, -1};
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    for (int d = 0; d < 9; ++d) {
      const int oid = units_id_at(ctx->units, (int)t->x + dx[d], (int)t->y + dy[d]);
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
    ai_contact_set_owner_nibble(ctx->map, u->x, u->y, e);
  }
}

static void ai_contact_apply_welcome_accept(
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
  /* relation_by_indian = DOS 0x60 MET|PEACE flag byte (every DOS save: 96 once met). */
  ctx->col1->nation[e].relation_by_indian[nation_id - 4] = 96u;
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
  snprintf(
    peace_fb,
    sizeof(peace_fb),
    "The %s welcome peace with our brothers the %s. Let us smoke a peace pipe "
    "to celebrate our perpetual friendship.",
    tribe,
    euro
  );
  char peace_body[AI_POPUP_BODY_LEN];
  ai_contact_msg_body(
    ctx->messages, "INDIANPEACE", &peace_tok, peace_fb, peace_body, sizeof(peace_body)
  );
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Peace", peace_body);

  /* DOS FUN_5bfb_0182: @INDIANCOME when relation < 0x19 before/as friendly. */
  if (rel_before < 25u) {
    PopupMsgTokens come_tok;
    memset(&come_tok, 0, sizeof(come_tok));
    come_tok.string0 = tribe;
    char come_fb[AI_POPUP_BODY_LEN];
    snprintf(
      come_fb,
      sizeof(come_fb),
      "We hope you will soon visit %s villages to share knowledge with us, and "
      "that you will send your wagon trains to trade with us.",
      tribe
    );
    char come_body[AI_POPUP_BODY_LEN];
    ai_contact_msg_body(
      ctx->messages, "INDIANCOME", &come_tok, come_fb, come_body, sizeof(come_body)
    );
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Peace", come_body);
  }

  /*
   * DOS FUN_5bfb_022e first-contact arm ends after 0182 (peace / optional
   * COME) — goto LAB_5bfb_1005. Meet CHOICE / gift / trade is later village
   * interaction (PARKED), not chained onto Accept.
   */
}

static void ai_contact_apply_welcome_reject(
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
   * DOS +100 hostility → Linux at-war band (0 < relation < 50). Write a
   * hostile floor (1), not unmet 0 — seed-100 early goldens keep r==0/sticky
   * clear until first contact. Cite: FUN_4cc6_00f2; indian_contact.md.
   */
  ai_diplo_indian_alarm_delta(ctx->col1, nation_id, e, 100); /* DOS +100 hostility */
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id != nation_id) {
        continue;
      }
      if (t->alarm[e].friction < 80u) {
        t->alarm[e].friction = 80u;
      }
      t->alarm[e].attacks++;
    }
  }
  ai_diplo_indian_hostility_sync(ctx->col1, e);

  const char* tribe = ai_contact_tribe_name(nation_id);
  PopupMsgTokens shun_tok;
  memset(&shun_tok, 0, sizeof(shun_tok));
  shun_tok.string0 = tribe;
  char shun_fb[AI_POPUP_BODY_LEN];
  snprintf(
    shun_fb,
    sizeof(shun_fb),
    "Then the mighty %s shall mercilessly drive you from our shores. Prepare for WAR!",
    tribe
  );
  char shun_body[AI_POPUP_BODY_LEN];
  ai_contact_msg_body(
    ctx->messages, "INDIANSHUN", &shun_tok, shun_fb, shun_body, sizeof(shun_body)
  );
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "War", shun_body);
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
   * (Tenochtitlan); 04ac vs 0498 only differ in option gating. */
  if (ctx->col1 && ctx->col1->head.turn >= 20) {
    sound_set_bgm(nation_id == 0 ? 7 : (nation_id == 1 ? 6 : 5));
  }
  /*
   * FUN_5bfb_022e 5bfb:038a, right before the @INDIANWELCOME dialog: the
   * tribe's own tech class picks the woodcut — Inca (DS:0x8d52 == 0) gets
   * THE INCA NATION, Aztec (== 1) THE AZTEC EMPIRE, everyone else MEETING
   * THE NATIVES.
   */
  (void)woodcut_fire(
    ctx->col1,
    nation_id == 0   ? WOODCUT_THE_INCA_NATION
    : nation_id == 1 ? WOODCUT_THE_AZTEC_EMPIRE
                     : WOODCUT_MEETING_THE_NATIVES
  );
  PopupMsgTokens welcome_tok;
  memset(&welcome_tok, 0, sizeof(welcome_tok));
  welcome_tok.string0 = tribe;
  welcome_tok.string1 = shown == 1 ? "village" : "villages";
  welcome_tok.number0 = shown;
  welcome_tok.has_number0 = true;
  char fb[AI_POPUP_BODY_LEN];
  snprintf(
    fb,
    sizeof(fb),
    "The %s tribe welcomes you. We are a glorious nation of %d %s. "
    "To celebrate our friendship, we generously offer you the land you now "
    "occupy as a gift. Will you accept our treaty and live with us in peace "
    "as brothers?",
    tribe,
    welcome_tok.number0,
    welcome_tok.string1
  );
  char body[AI_POPUP_BODY_LEN];
  ai_contact_msg_body(
    ctx->messages, "INDIANWELCOME", &welcome_tok, fb, body, sizeof(body)
  );
  static const char* labels[] = {"Yes", "No"};
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
   * (96 in seed-100 TURN3+ goldens). Relation set by accept (96) / reject (1). */
  ind->euro_diplo[euro_nation] = (uint8_t)(ind->euro_diplo[euro_nation] | 0x20u);

  if (ai_contact_euro_is_human(ctx, euro_nation) && ctx->ai_popups) {
    ai_contact_enqueue_welcome(ctx, euro_nation, indian_nation);
    return 1;
  }
  /* AI Euro / no popups: auto-accept (DOS local_c = 1). */
  ai_contact_apply_welcome_accept(ctx, ind, indian_nation, euro_nation);
  return 1;
}

int ai_contact_encounter_scan(ColonizeTurnContext* ctx, int euro_nation, int x, int y) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->map || euro_nation < 0 || euro_nation > 3) {
    return 0;
  }
  /* DOS DS:0xb4 / DS:0xbe direction tables (N, NE, E, SE, S, SW, W, NW). */
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int opened = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = x + dx[d];
    const int ny = y + dy[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    int other = -1;
    /* FUN_137f_03e4 tile_tribe_owner: owner nibble only when bit 0x02 set. */
    const int i = ny * ctx->map->width + nx;
    if (ctx->map->layer2 && ctx->map->layer3 && (ctx->map->layer2[i] & 0x02u) != 0) {
      const int hi = (ctx->map->layer3[i] >> 4) & 0x0f;
      other = hi == 0x0f ? -1 : hi;
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
  if (!st) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_CONTACT_MEET &&
        st->queue[i].kind == AI_POPUP_KIND_CHOICE && st->queue[i].nation_a == e &&
        st->queue[i].nation_b == nation_id) {
      return 1;
    }
  }
  if (st->open && st->current.tag == AI_POPUP_TAG_CONTACT_MEET &&
      st->current.kind == AI_POPUP_KIND_CHOICE && st->current.nation_a == e &&
      st->current.nation_b == nation_id) {
    return 1;
  }
  return 0;
}

/*
 * Acting-unit classification for the DOS @ACTIONS gating. DOS reads the
 * unit-type byte (0x3146: 5 = Scouts, 3 = Missionaries, 0xc = Wagon Train,
 * 0xd..0x12 = ships), the type's attack column (0x5236) and the profession
 * byte (0x315b: 0x1b = Indian Convert). Linux keys the same facts off the
 * @UNIT type name / domain.
 */
typedef struct AiContactUnitClass {
  int is_ship;
  int is_wagon;
  int is_scout;
  int is_missionary;
  int attack;
  int can_live_among; /* FUN_1000_8d68 ≥ 0 && attack < 2 && !scout && profession != Convert */
} AiContactUnitClass;

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
  const char* tname = t ? t->name : "";
  out->is_ship = units_is_sea(units, u->id) ? 1 : 0;
  out->is_wagon = strstr(tname, "Wagon") != NULL;
  out->is_scout = combat_type_is_scout_name(tname);
  out->is_missionary = strstr(tname, "Mission") != NULL;
  out->attack = t ? t->attack : 0;
  const char* dname = units_display_name(units, u);
  const int is_convert =
    u->profession == COLONIZE_PROF_CONVERT || (dname && strstr(dname, "Convert") != NULL);
  const int colonist_class =
    !out->is_ship && !out->is_wagon && !out->is_scout && !out->is_missionary &&
    strstr(tname, "Treasure") == NULL && strstr(tname, "Artillery") == NULL;
  /*
   * FUN_1000_8d68 → FUN_15eb_0902: DS:0x30e default profession by unit type
   * = {19,21,20,24,23,22,-1,23,-1,21,-1…} — ≥ 0 only for Colonists, Soldiers,
   * Pioneers, Missionaries, Dragoons, Scouts, Cont. Cavalry, Cont. Army. With
   * the attack < 2 / not-Scout / not-Missionary gates that is exactly the
   * colonist-class name test above (Regulars/Cavalry fall to attack ≥ 2).
   */
  out->can_live_among = colonist_class && out->attack < 2 && !is_convert;
}

/* Meet payload: bit0 is_missionary, bit1 is_capital, bits 2.. = unit id + 1. */
static int ai_contact_meet_payload(int is_missionary, int is_capital, int unit_id) {
  return (is_missionary ? 1 : 0) | (is_capital ? 2 : 0) | ((unit_id + 1) << 2);
}

int ai_contact_meet_payload_unit(int payload) {
  return (payload >> 2) - 1;
}

/* NAMES.TXT @LEVELS column 1 by tribe tech (DS:0x9634 + tech*6): Camp/Village/City. */
static const char* ai_contact_level_noun(const ColonizeTurnContext* ctx, int tech) {
  static const char* k_fallback[4] = {"camp", "village", "city", "city"};
  static char live[32];
  if (tech < 0) {
    tech = 0;
  }
  if (tech > 3) {
    tech = 3;
  }
  if (ctx && ctx->names) {
    const ColonizeMsgSection* sec = assets_msg_find(ctx->names, "LEVELS");
    if (sec) {
      int row = 0;
      for (int i = 0; i < sec->line_count; ++i) {
        const char* line = sec->lines[i];
        if (!line || !line[0] || line[0] == ';' || line[0] == '@') {
          continue;
        }
        if (row == tech) {
          const char* c1 = strchr(line, ',');
          if (c1) {
            c1++;
            while (*c1 == ' ') {
              c1++;
            }
            size_t n = 0;
            while (c1[n] && c1[n] != ',' && n + 1 < sizeof(live)) {
              live[n] = c1[n];
              n++;
            }
            while (n > 0 && live[n - 1] == ' ') {
              n--;
            }
            live[n] = '\0';
            if (n > 0) {
              return live;
            }
          }
          break;
        }
        row++;
      }
    }
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
  static const char* k_fallback[10] = {
    "Trade With Village",   "Enter Hostile Village",  "Establish Mission",
    "Denounce Heresy of %Fs Mission", "Live Among The Natives", "Ask to Speak With Chief",
    "Incite Indians",       "Demand Tribute",         "Attack Village",
    "Cancel Action"
  };
  const char* src = (row >= 0 && row < 10) ? k_fallback[row] : "";
  if (ctx && ctx->names) {
    const ColonizeMsgSection* sec = assets_msg_find(ctx->names, "ACTIONS");
    if (sec) {
      int r = 0;
      for (int i = 0; i < sec->line_count; ++i) {
        const char* line = sec->lines[i];
        if (!line || !line[0] || line[0] == ';' || line[0] == '@') {
          continue;
        }
        if (r == row) {
          src = line;
          break;
        }
        r++;
      }
    }
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
static void ai_contact_enqueue_village_meet(
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
      if (u && ai_contact_dist(t->x, t->y, u->x, u->y) <= 1) {
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
  snprintf(fb, sizeof(fb), "Your expedition has reached a %s of %s.", tok.string0, tribe);
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
    if (cls.is_wagon || cls.is_ship) {
      AI_CONTACT_MENU_ADD(alarm < 0x4b ? 0 : 1, alarm < 0x4b ? AI_CONTACT_CHOICE_TRADE : AI_CONTACT_CHOICE_ENTER_HOSTILE);
    }
    if (cls.is_scout) {
      AI_CONTACT_MENU_ADD(5, AI_CONTACT_CHOICE_CHIEF);
    }
    int attack_listed = 0;
    if (!cls.is_ship && cls.attack > 1) {
      AI_CONTACT_MENU_ADD(8, AI_CONTACT_CHOICE_ATTACK_VILLAGE);
      attack_listed = 1;
    }
    if (met) {
      if (cls.is_missionary) {
        if (foreign_owner < 0) {
          AI_CONTACT_MENU_ADD(2, AI_CONTACT_CHOICE_MISSION);
        } else if (foreign_owner != e) {
          AI_CONTACT_MENU_ADD(3, AI_CONTACT_CHOICE_HERESY);
        }
        AI_CONTACT_MENU_ADD(6, AI_CONTACT_CHOICE_INCITE);
      } else {
        if (cls.can_live_among) {
          AI_CONTACT_MENU_ADD(4, AI_CONTACT_CHOICE_TEACH);
        }
        if (cls.attack != 0 && !cls.is_ship) {
          AI_CONTACT_MENU_ADD(7, AI_CONTACT_CHOICE_DEMAND);
        }
      }
      if (!attack_listed && cls.attack != 0 && !cls.is_ship) {
        AI_CONTACT_MENU_ADD(8, AI_CONTACT_CHOICE_ATTACK_VILLAGE);
      }
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
  if (!st || unit_id < 0) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_CONTACT_MEET && st->queue[i].kind == AI_POPUP_KIND_CHOICE &&
        ai_contact_meet_payload_unit(st->queue[i].payload) == unit_id) {
      return 1;
    }
  }
  if (st->open && st->current.tag == AI_POPUP_TAG_CONTACT_MEET &&
      st->current.kind == AI_POPUP_KIND_CHOICE &&
      ai_contact_meet_payload_unit(st->current.payload) == unit_id) {
    return 1;
  }
  return 0;
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
  /* Already met only — unmet uses WELCOME. */
  if (!ind->euro_diplo[euro_nation]) {
    return 0;
  }
  /*
   * DOS shows the menu at any alarm (the @VILLAGEWAR body + "Enter Hostile
   * Village" row exist for exactly that); only the legacy no-unit callers
   * keep the old at-war refusal so their raid-warn fallback still runs.
   */
  if (unit_id < 0 && ai_diplo_indian_at_war(ctx->col1, euro_nation, indian_nation - 4)) {
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

static int ai_contact_village_warn_pending(const AiPopupState* st, int unit_id) {
  if (!st || unit_id < 0) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_CONTACT_VILLAGE_WARN &&
        st->queue[i].kind == AI_POPUP_KIND_CHOICE && st->queue[i].nation_a == unit_id) {
      return 1;
    }
  }
  if (st->open && st->current.tag == AI_POPUP_TAG_CONTACT_VILLAGE_WARN &&
      st->current.kind == AI_POPUP_KIND_CHOICE && st->current.nation_a == unit_id) {
    return 1;
  }
  return 0;
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
  ColonizeCol1Indian* ind = &ctx->col1->indian[indian_nation - 4];
  /* Same at-war floor as welcome reject (FUN_4cc6_00f2 thin). */
  ai_contact_clear_peace(ctx->col1, indian_nation, euro_nation);
  ai_diplo_indian_alarm_delta(ctx->col1, indian_nation, euro_nation, 100); /* DOS +100 hostility */
  if (ind->alarm_by_player[euro_nation] < 80u) {
    ind->alarm_by_player[euro_nation] = 80u;
  }
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id != indian_nation) {
        continue;
      }
      if (t->alarm[euro_nation].friction < 80u) {
        t->alarm[euro_nation].friction = 80u;
      }
      t->alarm[euro_nation].attacks++;
    }
  }
  ai_diplo_indian_hostility_sync(ctx->col1, euro_nation);
}

/*
 * FUN_4d56_4528 human warn CHOICE before combatish village enter.
 * Relation-banded body (0x1710…0x172e stand-in). Cite: indian_settlement_4528.md.
 */
int ai_contact_whack_pending(const AiPopupState* st, int unit_id) {
  if (!st) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_CONTACT_WHACK && st->queue[i].nation_a == unit_id) {
      return 1;
    }
  }
  return st->open && st->current.tag == AI_POPUP_TAG_CONTACT_WHACK &&
         st->current.nation_a == unit_id;
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
  char fb[AI_POPUP_BODY_LEN];
  snprintf(fb, sizeof(fb), "Shall we attack the %s, Your Excellency?", tribe);
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "WHACKINDIANS", &tok, fb, body, sizeof(body));
  static const char* labels[] = {"Yes", "No"};
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
  if (!st) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_CONTACT_EURO_WAR && st->queue[i].nation_a == unit_id) {
      return 1;
    }
  }
  return st->open && st->current.tag == AI_POPUP_TAG_CONTACT_EURO_WAR &&
         st->current.nation_a == unit_id;
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
  const uint8_t rel = (uint8_t)(ai_diplo_read(ctx->col1, euro_nation, target_nation) |
                                ai_diplo_read(ctx->col1, target_nation, euro_nation));
  if ((rel & AI_DIPLO_PEACE) == 0) {
    /* No treaty: DOS attacks without a prompt — open hostilities and go. */
    ai_diplo_declare_war_ctx(ctx, euro_nation, target_nation);
    return 0;
  }
  const char* name = ai_contact_euro_name(target_nation);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = name;
  char fb[AI_POPUP_BODY_LEN];
  snprintf(
    fb, sizeof(fb),
    "\"We have signed a peace treaty with the %s, Your Excellency.\"", name
  );
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "HAVETREATY", &tok, fb, body, sizeof(body));
  static const char* labels[] = {"Cancel Action.", "Break Treaty."};
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

int ai_contact_try_village_raid_warn(
  ColonizeTurnContext* ctx,
  int euro_nation,
  int indian_nation,
  int unit_id,
  int dest_x,
  int dest_y
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->ai_popups) {
    return 0;
  }
  if (euro_nation < 0 || euro_nation > 3 || indian_nation < 4 || indian_nation > 11) {
    return 0;
  }
  if (unit_id < 0 || dest_x < 0 || dest_y < 0 || dest_x > 255 || dest_y > 255) {
    return 0;
  }
  if (!ai_contact_euro_is_human(ctx, euro_nation)) {
    return 0;
  }
  ai_contact_bind_names(ctx);
  if (ai_contact_village_warn_pending(ctx->ai_popups, unit_id) ||
      ai_contact_welcome_pending(ctx->ai_popups, euro_nation, indian_nation)) {
    return 0;
  }
  const char* tribe = ai_contact_tribe_name(indian_nation);
  const int alarm = ai_diplo_indian_alarm(ctx->col1, indian_nation, euro_nation);
  char body[AI_POPUP_BODY_LEN];
  if (alarm <= 25) { /* relation >= 75 */
    snprintf(
      body,
      sizeof(body),
      "The %s welcome visitors, but armed entry insults their hospitality. "
      "Attack the village, or leave in peace?",
      tribe
    );
  } else if (alarm <= 50) { /* relation >= 50 */
    snprintf(
      body,
      sizeof(body),
      "The %s eye your weapons with suspicion. Attack their village, or withdraw?",
      tribe
    );
  } else if (alarm <= 75) { /* relation >= 25 */
    snprintf(
      body,
      sizeof(body),
      "The %s shout warnings from the edge of camp. Attack, or leave before blood is shed?",
      tribe
    );
  } else {
    snprintf(
      body,
      sizeof(body),
      "Hostile %s braves bar the path. Attack the village, or fall back?",
      tribe
    );
  }
  static const char* labels[] = {"Leave", "Attack"};
  static const int ids[] = {AI_CONTACT_VILLAGE_LEAVE, AI_CONTACT_VILLAGE_ATTACK};
  const int payload = dest_x | (dest_y << 8);
  if (!ai_popup_enqueue_choice_ctx(
        ctx->ai_popups,
        AI_POPUP_TAG_CONTACT_VILLAGE_WARN,
        unit_id,
        indian_nation,
        payload,
        NULL,
        body,
        labels,
        ids,
        2
      )) {
    return 0;
  }
  {
    char st[96];
    snprintf(st, sizeof(st), "Approaching %s village…", tribe);
    ai_contact_set_status(ctx, st);
  }
  return 1;
}

/*
 * FUN_4d56_4528 ship head (ASM): unmet met-bit 0x20 → @DONTKNOWSHIPS abort;
 * relation≥0x4b or friction≥0x40 → @MADATSHIPS abort; else fall through to
 * village meet. Ship never enters the tile. Cite: indian_settlement_4528.md.
 */
int ai_contact_try_ship_village(ColonizeTurnContext* ctx, int euro_nation, int x, int y) {
  return ai_contact_try_ship_village_unit(ctx, euro_nation, x, y, -1);
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

  const ColonizeCol1Tribe* tribe = NULL;
  int tribe_index = -1;
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->x == x && (int)t->y == y && t->nation_id >= 4 && t->nation_id <= 11) {
      tribe = t;
      tribe_index = (int)ti;
      break;
    }
  }
  if (!tribe) {
    return 0;
  }

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
      "We must contact the Indians on land first, Excellency.",
      body,
      sizeof(body)
    );
    ai_contact_human_chrome(ctx, euro_nation, AI_POPUP_TAG_INFO, indian_nation, "Ships", body);
    /* No chief portrait: this is the King's advisor, not a native audience. */
    if (ctx->ai_popups) {
      ai_popup_set_last_portrait(ctx->ai_popups, -1, 0);
    }
    if (!ai_contact_euro_is_human(ctx, euro_nation)) {
      ai_contact_set_status(ctx, body);
    }
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
      "The people do not trust the men in your ships.",
      body,
      sizeof(body)
    );
    ai_contact_human_chrome(ctx, euro_nation, AI_POPUP_TAG_INFO, indian_nation, "Ships", body);
    /* No chief portrait: this is the King's advisor, not a native audience. */
    if (ctx->ai_popups) {
      ai_popup_set_last_portrait(ctx->ai_popups, -1, 0);
    }
    if (!ai_contact_euro_is_human(ctx, euro_nation)) {
      ai_contact_set_status(ctx, body);
    }
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
    ai_contact_human_chrome(ctx, euro_nation, AI_POPUP_TAG_INFO, indian_nation, "Ships", wary);
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
static void ai_contact_local_rng(ColonizeTurnContext* ctx, int nation_id, ColonizeDosRng* out) {
  uint32_t seed = 0xC07Au ^ (uint32_t)(nation_id * 97);
  if (ctx && ctx->turn_number) {
    seed ^= (uint32_t)(*ctx->turn_number) * 0x9E3779B9u;
  }
  if (ctx && ctx->rng_seed) {
    seed ^= ctx->rng_seed * 0x85ebca6bu;
  }
  dos_rng_seed(out, seed ? seed : 1u);
}

static void ai_contact_clamp_alarms(ColonizeCol1Indian* ind) {
  if (!ind) {
    return;
  }
  for (int e = 0; e < 4; ++e) {
    /* Signed alarm byte clamp stand-in (1816 §4): keep uint16 in band. */
    if (ind->alarm_by_player[e] > 200) {
      ind->alarm_by_player[e] = 200;
    }
  }
}

/* Missionary / Jesuit Missionary / similar — name substring stand-in. */
static int ai_contact_is_missionary(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const char* name = units_display_name(units, u);
  if (!name) {
    return 0;
  }
  /* Match ai_euro: "Missionary" or "Jesuit" (NAMES.TXT Missionary / Jesuit Missionaries). */
  return strstr(name, "Mission") != NULL || strstr(name, "Jesuit") != NULL;
}

/*
 * Jesuit-grade missionary (expert).
 * PEDIA @JOB24: Jesuits are more effective than ordinary blessed missionaries.
 * Detect by display name "Jesuit", NAMES @JOB profession 24, or Brebeuf FF
 * ownership (fandom: all missionaries function as experts). Cite:
 * COLONIZE/PEDIA.TXT @JOB24; NAMES.TXT Missionary/Jesuit Missionaries;
 * docs/fandom_col1994.md Father Jean de Brebeuf.
 * Las Casas Convert→Free Colonist assimilate: founding_fathers elect +
 * ownership tick (PEDIA @FATHER24) — not this convert-pulse path.
 * Sepulveda convert-join (PEDIA @FATHER23 / FUN_5fef_31ea): wired in
 * units_try_native_settlement_fallout when conquering a mission-owned tribe;
 * this missionary convert pulse is a different path (no invent join % here).
 */
static int ai_contact_is_jesuit_grade(
  const ColonizeCol1Save* col1,
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
) {
  if (!u) {
    return 0;
  }
  const char* name = units_display_name(units, u);
  if (name && strstr(name, "Jesuit") != NULL) {
    return 1;
  }
  if (u->profession == 24) { /* NAMES @JOB Jesuit Missionaries */
    return 1;
  }
  if (col1 && founding_fathers_brebeuf_missionaries_are_experts(col1, u->nation_id) &&
      ai_contact_is_missionary(units, u)) {
    return 1;
  }
  return 0;
}

/* Soldier / Scout / Pioneer / Dragoon / Artillery — encroachment types. */
static int ai_contact_is_encroacher(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const char* name = units_display_name(units, u);
  if (!name) {
    return 0;
  }
  /* "Soldier" also matches Veteran Soldier; Dragoon/Artillery = military presence. */
  return strstr(name, "Soldier") != NULL || strstr(name, "Scout") != NULL ||
         strstr(name, "Pioneer") != NULL || strstr(name, "Dragoon") != NULL ||
         strstr(name, "Artillery") != NULL;
}

/*
 * Alarm growth dampers (wiki/fandom Alarm):
 * - Pocahontas: Indian alarm generated half as fast for that Euro.
 * - French (nation 1): national bonus slows hostility (same half-rate).
 * Stack when both apply (quarter). Floor; +1 bumps may become 0.
 * Cite: docs/fandom_col1994.md Indians / Pocahontas; Colonization.pdf FF table.
 */
static int ai_contact_alarm_bump_amount(
  const ColonizeCol1Save* col1,
  int euro,
  int amount
) {
  if (amount <= 0) {
    return 0;
  }
  int n = amount;
  if (col1 && euro >= 0 && euro <= 3 &&
      founding_fathers_nation_has(col1, euro, FF_POCAHONTAS)) {
    n /= 2;
  }
  if (euro == 1) { /* France */
    n /= 2;
  }
  return n;
}

/* Bump uint8 friction toward cap 100. */
static void ai_contact_bump_u8_cap100(uint8_t* v, int amount) {
  if (!v || amount <= 0) {
    return;
  }
  int n = (int)(*v) + amount;
  if (n > 100) {
    n = 100;
  }
  *v = (uint8_t)n;
}

/* Bump uint16 alarm toward cap 100. */
static void ai_contact_bump_u16_cap100(uint16_t* v, int amount) {
  if (!v || amount <= 0) {
    return;
  }
  int n = (int)(*v) + amount;
  if (n > 100) {
    n = 100;
  }
  *v = (uint16_t)n;
}

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
static int ai_contact_is_petty_criminal(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  /* bugs.md: profession byte, not the display name — a Petty Criminal is a
   * Colonists-type unit whose display name reads "Free Colonist", so the
   * old name test never fired (and Indentured Servants fell into the
   * profession-set "master" refusal instead of learning). */
  if (u->profession == UNITS_JOB_CRIMINAL) {
    return 1;
  }
  const char* name = units_display_name(units, u);
  return name && strstr(name, "Criminal") != NULL;
}

static int ai_contact_is_teachable_learner(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const char* name = units_display_name(units, u);
  if (!name) {
    return 0;
  }
  return strstr(name, "Free Colonist") != NULL || strstr(name, "Scout") != NULL;
}

/* @LEARNMASTER %STRING1: skill name of an already-expert learner (field job
 * noun when set; else equipment-based display name for Scout/Pioneer/…). */
static const char* ai_contact_learner_skill_name(
  const ColonizeUnitPool* units,
  const ColonizeUnit* u
) {
  if (!u) {
    return "colonist";
  }
  if (u->profession >= 0 && u->profession < COLONIZE_FIELD_JOB_COUNT) {
    return colony_yield_job_name(u->profession);
  }
  return units_display_name(units, u);
}

/* Warehouse cargo → outdoor @JOB (indices align food..silver). -1 unmapped. */
static int ai_contact_profession_from_cargo(int cargo) {
  switch (cargo) {
    case COLONIZE_CARGO_FOOD:
      return COLONIZE_JOB_FARMER;
    case COLONIZE_CARGO_SUGAR:
      return COLONIZE_JOB_SUGAR_PLANTER;
    case COLONIZE_CARGO_TOBACCO:
      return COLONIZE_JOB_TOBACCO_PLANTER;
    case COLONIZE_CARGO_COTTON:
      return COLONIZE_JOB_COTTON_PLANTER;
    case COLONIZE_CARGO_FURS:
      return COLONIZE_JOB_FUR_TRAPPER;
    case COLONIZE_CARGO_LUMBER:
      return COLONIZE_JOB_LUMBERJACK;
    case COLONIZE_CARGO_ORE:
      return COLONIZE_JOB_ORE_MINER;
    case COLONIZE_CARGO_SILVER:
      return COLONIZE_JOB_SILVER_MINER;
    default:
      return -1;
  }
}

/*
 * Rough Col1 nation_id (4..11) → primary taught skill. Order matches
 * NAMES.TXT @TRIBES (Inca..Tupi). Fish has no cargo id — nation only.
 * Returns -1 if out of band (caller falls back to Farmer).
 * @TRIBES field-3 flavor goods (Jewelled Relics, …) are trade chrome, not
 * teach skills — see ai_contact_tribe_flavor_good.
 */
static int ai_contact_profession_from_nation(int nation_id) {
  static const int k_by_indian[8] = {
      COLONIZE_JOB_SILVER_MINER,    /* 4 Inca */
      COLONIZE_JOB_ORE_MINER,       /* 5 Aztec */
      COLONIZE_JOB_FISHERMAN,       /* 6 Arawak */
      COLONIZE_JOB_FUR_TRAPPER,     /* 7 Iroquois */
      COLONIZE_JOB_TOBACCO_PLANTER, /* 8 Cherokee */
      COLONIZE_JOB_COTTON_PLANTER,  /* 9 Apache */
      COLONIZE_JOB_FUR_TRAPPER,     /* 10 Sioux */
      COLONIZE_JOB_SUGAR_PLANTER,   /* 11 Tupi */
  };
  const int idx = nation_id - 4;
  if (idx < 0 || idx >= 8) {
    return -1;
  }
  return k_by_indian[idx];
}

/*
 * Resolve taught profession for an unskilled Free Colonist.
 * Prefer tribe.last_sold when it is a raw cargo 1..7 (sugar..silver) — food(0)
 * is left alone so zeroed Col1 tribes still take the nation map. Else nation
 * table; else Expert Farmer.
 */
static int ai_contact_taught_profession(const ColonizeCol1Tribe* t) {
  if (!t) {
    return COLONIZE_JOB_FARMER;
  }
  if (t->last_sold >= COLONIZE_CARGO_SUGAR && t->last_sold <= COLONIZE_CARGO_SILVER) {
    const int from_cargo = ai_contact_profession_from_cargo((int)t->last_sold);
    if (from_cargo >= 0) {
      return from_cargo;
    }
  }
  const int from_nation = ai_contact_profession_from_nation((int)t->nation_id);
  if (from_nation >= 0) {
    return from_nation;
  }
  return COLONIZE_JOB_FARMER;
}

static void ai_contact_teach_skill(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    /*
     * Col1 one-shot: tribe.state.learned already set -> skip teach and do
     * not write teach/refuse status (preserves gift/trade chrome). Cite:
     * non-capital village teaches one skill to one colonist total, shared
     * across all Euro nations; the tribe's capital is exempt and teaches
     * unlimited colonists (manual Indian Land / Colonization rules, per
     * user correction 2026-08-13 -- teaching itself is always free, no
     * gold changes hands either way).
     */
    if (t->state.learned && !t->state.capital) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int oid = units_id_at(ctx->units, t->x + dx[d], t->y + dy[d]);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id < 0 || other->nation_id > 3) {
        continue;
      }
      /*
       * bugs.md: the adjacency pulse never LECTURES — a mounted
       * criminal-scout got the @LEARNCRIMINAL refusal out of nowhere when
       * a Brave wandered by, as though it had asked to live among the
       * natives. Refusal dialogs (@LEARNCRIMINAL/@LEARNMASTER/…) belong to
       * the deliberate "Live Among The Natives" flow; here an ineligible
       * unit is simply skipped in silence.
       */
      if (ai_contact_is_petty_criminal(ctx->units, other)) {
        continue;
      }
      if (!ai_contact_is_teachable_learner(ctx->units, other)) {
        continue;
      }
      const int e = other->nation_id;
      /*
       * @LEARNMASTER: "We can only teach new skills to colonists who do not
       * yet have one" — a colonist (or already-Seasoned Scout) that already
       * carries an expert skill cannot be re-taught. Refuse without consuming
       * the village's one-shot (state.learned stays clear for a future
       * unskilled colonist). Previously this fell through and silently
       * burned the teach + showed a misleading "taught outdoor skills" line.
       */
      if (other->profession != UNITS_JOB_NONE) {
        /* Skilled already — silent skip in the pulse; the @LEARNMASTER
         * refusal dialog belongs to the deliberate Live-Among flow
         * (bugs.md — no unprompted lecture popups). */
        continue;
      }
      /*
       * Alarmed Indian diplomacy (fandom Alarm; same ≥55 refuse-talk gate):
       * high alarm/friction → refuse teach (@LEARNMAD; ai_popup Done).
       */
      if (ind->alarm_by_player[e] >= 55 || t->alarm[e].friction >= 55) {
        char refuse_fb[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          ctx->messages,
          "LEARNMAD",
          NULL,
          "Your ill manners infuriate us. We doubt you will ever learn anything from us.",
          refuse_fb,
          sizeof(refuse_fb)
        );
        ai_contact_human_chrome(
          ctx,
          e,
          AI_POPUP_TAG_CONTACT_TEACH,
          nation_id,
          "Teach",
          refuse_fb
        );
        break; /* one refuse pulse per tribe per call */
      }
      /*
       * Mid-alarm refuse polish (40..54): teach is peaceful-band only
       * (<40). Same @LEARNMAD refuse chrome as ≥55 (no invented gold).
       * Cite: fandom Alarm / Teach; indian_contact.md teach-skill pulse.
       */
      if (ind->alarm_by_player[e] >= 40 || t->alarm[e].friction >= 40) {
        char refuse_fb[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          ctx->messages,
          "LEARNMAD",
          NULL,
          "Your ill manners infuriate us. We doubt you will ever learn anything from us.",
          refuse_fb,
          sizeof(refuse_fb)
        );
        ai_contact_human_chrome(
          ctx,
          e,
          AI_POPUP_TAG_CONTACT_TEACH,
          nation_id,
          "Teach",
          refuse_fb
        );
        break; /* one mid-refuse pulse per tribe per call */
      }
      t->state.learned = 1;
      /*
       * Optional expertise: unskilled Free Colonist → tribe-appropriate
       * outdoor skill (cargo / nation map); Plain Scout → Seasoned Scout.
       * Already-skilled units keep profession.
       */
      int taught_scout = 0;
      if (other->profession == UNITS_JOB_NONE) {
        const char* name = units_display_name(ctx->units, other);
        if (name && strstr(name, "Scout") != NULL) {
          other->profession = UNITS_JOB_SCOUT;
          taught_scout = 1;
        } else {
          other->profession = ai_contact_taught_profession(t);
        }
      }
      if (taught_scout) {
        char body[AI_POPUP_BODY_LEN];
        const char* fb = "Our Scouts have improved to Seasoned status.";
        popup_msg_fill(ctx->messages, "WELLSEASONED", NULL, fb, body, sizeof(body));
        ai_contact_human_chrome(
          ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", body
        );
      } else {
        char line[96];
        snprintf(
          line,
          sizeof(line),
          "The %s teach outdoor skills.",
          ai_contact_tribe_name(nation_id)
        );
        ai_contact_human_chrome(
          ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", line
        );
      }
      break; /* one teach pulse per tribe per call */
    }
  }
}

/* Decay alarm_by_player + this nation's tribe frictions by `amount` (floor 0). */
static void ai_contact_friction_decay(
  ColonizeCol1Indian* ind,
  ColonizeCol1Save* col1,
  int nation_id,
  int e,
  int amount
) {
  if (!ind || amount <= 0 || e < 0 || e > 3) {
    return;
  }
  if ((int)ind->alarm_by_player[e] > amount) {
    ind->alarm_by_player[e] = (uint16_t)(ind->alarm_by_player[e] - (uint16_t)amount);
  } else {
    ind->alarm_by_player[e] = 0;
  }
  if (!col1 || !col1->tribe) {
    return;
  }
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    if ((int)t->alarm[e].friction > amount) {
      t->alarm[e].friction = (uint8_t)(t->alarm[e].friction - (uint8_t)amount);
    } else {
      t->alarm[e].friction = 0;
    }
  }
}

/* Max of alarm_by_player and tribe frictions for this Indian×Euro pair. */
static int ai_contact_pair_friction(
  const ColonizeCol1Indian* ind,
  const ColonizeCol1Save* col1,
  int nation_id,
  int e
) {
  int friction = ind ? (int)ind->alarm_by_player[e] : 0;
  if (!col1 || !col1->tribe) {
    return friction;
  }
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->nation_id == nation_id && (int)t->alarm[e].friction > friction) {
      friction = (int)t->alarm[e].friction;
    }
  }
  return friction;
}

/*
 * Apply a gift-band gold drain (CONTACT_GIFT amount CHOICE or auto Large).
 * Small −5 / friction −1; Large −10 / friction −2. Cite: FUN_5bfb_102a;
 * indian_contact.md gift stand-in (no invented crosses).
 * Pocahontas: gift decays full friction (half-rate applies only to positive
 * alarm/friction bumps — prelude/encroachment/raid; wiki/fandom).
 */
static void ai_contact_apply_gift_gold(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  unsigned gold_cost,
  int friction_decay
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || e < 0 || e > 3) {
    return;
  }
  if (!ind->euro_diplo[e]) {
    return;
  }
  const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
  if (friction >= 55 || ind->alarm_by_player[e] >= 55 || friction >= 40) {
    char refuse_fb[AI_POPUP_BODY_LEN];
    snprintf(
      refuse_fb,
      sizeof(refuse_fb),
      "The %s refuse gifts.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_GIFT, nation_id, "Gift", refuse_fb
    );
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[e];
  if (nat->gold < gold_cost) {
    char refuse_fb[AI_POPUP_BODY_LEN];
    snprintf(
      refuse_fb,
      sizeof(refuse_fb),
      "The %s refuse gifts.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_GIFT, nation_id, "Gift", refuse_fb
    );
    return;
  }
  nat->gold -= gold_cost;
  ai_contact_friction_decay(ind, ctx->col1, nation_id, e, friction_decay);
  {
    char gift_fb[AI_POPUP_BODY_LEN];
    snprintf(
      gift_fb,
      sizeof(gift_fb),
      "Gift of gold eases tensions with the %s.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_GIFT, nation_id, "Gift", gift_fb
    );
  }
}

/*
 * Human Gift amount CHOICE (Small −5 / Large −10 / Generous −20). Returns 1 if
 * enqueued. Cite: FUN_5bfb_102a amount stand-in; indian_contact.md.
 */
static int ai_contact_enqueue_gift_amount_choice(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id
) {
  if (!ctx || !ctx->ai_popups || !ctx->col1_ok || !ctx->col1 || e < 0 || e > 3) {
    return 0;
  }
  if (!ai_contact_euro_is_human(ctx, e)) {
    return 0;
  }
  const unsigned gold = ctx->col1->nation[e].gold;
  if (gold < 5u) {
    return 0; /* cannot pay Small — caller refuses */
  }
  const char* labels[3];
  int ids[3];
  int n = 0;
  labels[n] = "Small gift (5 gold)";
  ids[n] = AI_CONTACT_GIFT_SMALL;
  n++;
  if (gold >= 10u) {
    labels[n] = "Large gift (10 gold)";
    ids[n] = AI_CONTACT_GIFT_LARGE;
    n++;
  }
  if (gold >= 20u) {
    labels[n] = "Generous gift (20 gold)";
    ids[n] = AI_CONTACT_GIFT_GENEROUS;
    n++;
  }
  char body[AI_POPUP_BODY_LEN];
  snprintf(
    body,
    sizeof(body),
    "Offer gold to the %s?",
    ai_contact_tribe_name(nation_id)
  );
  return ai_popup_enqueue_choice_ctx(
           ctx->ai_popups,
           AI_POPUP_TAG_CONTACT_GIFT,
           e,
           nation_id,
           0,
           NULL,
           body,
           labels,
           ids,
           n
         )
           ? 1
           : 0;
}

/*
 * FUN_4d56_417e price (Incite Indians / WARPATH — identified 2026-08-13,
 * see original_sources_annotated/ai/indian_incite_417e.md for the full
 * disassembly trail and live-capture confirmation). DOS formula:
 *   base = table[-0x69d6]*8 + (table[-0x6e7c]>>2&0xfe - 2*table[-0x69d6])
 *        + INDIAN_STATE.signed_byte[7]*2 + INDIAN_STATE.signed_byte[8]*2
 *   price = base / (relation_score + 0x4b); floor 500
 *
 * 2026-08-14: both previously-unnamed tables identified while tracing the
 * deep Euro G-table (euro_g_table_0a60.md / FUN_4962_06b6, same DS
 * neighborhood) — neither is a static lookup constant, both are live
 * per-turn recomputed sums over this tribe *type* (nation_id-4):
 *   table[-0x69d6][type] = count of villages of that tribe type
 *   table[-0x6e7c][type] = Σ combat_unit_base_x8(brave, mode=1) [attack-
 *                          mode value, matching the traced FUN_281f_09c8
 *                          call] over every Brave of that tribe type,
 *                          byte-clamped (DOS's saturating FUN_4962_0006)
 * INDIAN_STATE.signed_byte[7]/[8] were already-named fields all along —
 * `ind->muskets` / `ind->horse_herds` (col1_save.h) — just never wired
 * into this formula; the `ind->tech`/`alarm_by_player` stand-in below was
 * approximating the wrong quantities entirely, not just missing two
 * unconfirmed numbers.
 */
static uint32_t ai_contact_incite_price(
  ColonizeTurnContext* ctx,
  const ColonizeCol1Indian* ind,
  int nation_id,
  int inciter,
  int target,
  int is_missionary,
  int is_capital
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || nation_id < 4 || nation_id > 11) {
    return 500u;
  }
  const ColonizeCol1Save* col1 = ctx->col1;

  /* `tribe.nation_id` is the full 4-11 nation id, not a 0-7 "type" —
   * confirmed independently via colony.c/units.c's own `-4` indexing
   * into col1->indian[8] — so this compares directly against `nation_id`,
   * not `nation_id - 4`. (Was comparing against a `tribe_type` local that
   * made this loop count zero villages always; see the discount-loop
   * comment below for the matching fix + how this was found.) */
  int village_count = 0;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      if ((int)col1->tribe[ti].nation_id == nation_id) {
        ++village_count;
      }
    }
  }
  if (village_count > 0xff) {
    village_count = 0xff;
  }

  int brave_value_sum = 0;
  if (ctx->units) {
    ColonizeCombatStrengthCtx sctx;
    sctx.units = ctx->units;
    sctx.map = ctx->map;
    sctx.colonies = ctx->colonies;
    sctx.col1 = col1;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = units_get_const(ctx->units, i);
      if (!u || !u->active || u->nation_id != nation_id) {
        continue;
      }
      if (units_is_sea(ctx->units, i)) {
        continue;
      }
      const int val = combat_unit_base_x8(&sctx, i, 1, NULL);
      brave_value_sum += val;
      if (brave_value_sum > 0xff) {
        brave_value_sum = 0xff;
        break;
      }
    }
  }

  const int base =
    village_count * 8 + (((brave_value_sum >> 2) & 0xfe) - 2 * village_count) +
    (int)ind->muskets * 2 + (int)ind->horse_herds * 2;

  /* DOS relation_score is ~0-100 (0x4b=75 is a real DOS "hostile-ish"
   * threshold on this exact scale, cross-confirmed via euro_g_table_0a60's
   * independent use of the same FUN_281f_030c/relation<0x4b check);
   * ai_diplo_indian_relation is 0-255. */
  const int relation = (int)ai_diplo_indian_relation(col1, nation_id, inciter); /* 0..100 */
  (void)target; /* alarm_by_player no longer used — real formula has no target term here */
  /* Base-combine op resolved byte-exact, 2026-08-14: read the actual
   * decompiled bodies of FUN_1d1d_0f60/FUN_1d1d_0ec6 (viceroy_unpacked.c
   * lines 20742-20829, both "exact"-kind mapped) instead of trusting
   * FUNCTION_CATALOG.md's inferred labels secondhand — confirms the
   * catalog was right both times: 0f60 (called here) really is a plain
   * 32-bit multiply (`return (ulong)param_1*(ulong)param_3;` for the
   * common small-operand case), 0ec6 (called by the French branch below)
   * really is the signed-division helper. So this line is `base *
   * (relation+75)`, NOT a division — a previous pass's `* 100 /` here
   * was backwards in both operation and shape. See indian_incite_417e.md
   * "Base-combine op resolved" for the byte-exact walkthrough. */
  int price = base * (relation + 75);
  /* French get a real DOS price break here (nation_A==1, matches this
   * project's own English/French/Spanish/Dutch=0/1/2/3 ordering) — genuine
   * Colonization lore (French have the best native relations) and now
   * byte-exact: raw disassembly's FUN_1d1d_0ec6(price<<1, ..., 3, 0) is a
   * real division, i.e. price = price*2/3 (~33% off). */
  if (inciter == 1) {
    price = (int)(((long)price * 2) / 3);
  }
  /* Discount loop — now byte-exact (2026-08-14, replaces the
   * `ai_diplo_indian_relation>128`/flat-100 stand-in). Raw disassembly:
   * for every tribe record, if tribe.nation_id(+2) == this village's own
   * nation_id AND tribe.mission(+5)&0xf == inciter (i.e. that OTHER
   * village of the same tribe already has a mission from the inciting
   * Euro power — both fields already named/real in ColonizeCol1Tribe,
   * no invented globals needed): discount 250, or 1000 if it's a
   * Jesuit-grade mission (mission&0x10), doubled again if that other
   * village is the tribe capital (state.capital, DOS state+3&4).
   * Floor is applied ONCE at the very end in DOS (not before the loop
   * too) — the previous premature clamp before this loop is removed to
   * match.
   * Also fixes a real regression from earlier the same session: a prior
   * "bonus fix" here compared `t->nation_id` against `tribe_type`
   * (nation_id-4, 0-7), assuming the field was a raw 0-7 type — but
   * `colony.c`/`units.c` both independently confirm (via their own
   * `tribe.nation_id - 4` indexing into `col1->indian[8]`) the field is
   * really 4-11, same range as this function's own `nation_id` param, so
   * the ORIGINAL direct comparison was correct all along and the "fix"
   * broke it. Reverted to comparing against `nation_id` directly. */
  int discount = 0;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if ((int)t->nation_id != nation_id) {
        continue;
      }
      if ((t->mission & COL1_TRIBE_MISSION_NATION_MASK) != (uint8_t)inciter) {
        continue;
      }
      int amt = (t->mission & COL1_TRIBE_MISSION_JESUIT_BIT) ? 1000 : 250;
      if (t->state.capital) {
        amt *= 2;
      }
      discount += amt;
    }
  }
  price -= discount;
  /*
   * Two more flat DOS discounts, wired 2026-08-14 (captured at popup-offer
   * time as booleans, not re-derived at apply time — same discipline as
   * ai_king_merc's offer-time landing capture, avoids needing a live unit
   * id or specific village index to survive the async CHOICE round-trip):
   * -1500 if the unit performing Incite is a Missionary
   * (UNITS_JOB_MISSIONARY, DOS unit-state byte 0x18 — a real, already-named
   * Linux constant); -500 if the specific village visited is the tribe
   * capital (DOS CUR_TRIBE_PTR state+3&4, ColonizeCol1Tribe.state.capital).
   */
  if (is_missionary) {
    price -= 1500;
  }
  if (is_capital) {
    price -= 500;
  }
  if (price < 500) {
    price = 500;
  }
  return (uint32_t)price;
}

/*
 * Human Incite Indians target CHOICE (FUN_4d56_417e Mode 1 — the menu step
 * of @INDIANWARPATH "Whom would you like us to attack?"). Lists the other
 * Euro nations the inciter can afford to incite this tribe against.
 * Returns 1 if enqueued.
 */
static int ai_contact_enqueue_incite_target_choice(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  int is_missionary,
  int is_capital
) {
  if (!ctx || !ctx->ai_popups || !ctx->col1_ok || !ctx->col1 || e < 0 || e > 3) {
    return 0;
  }
  if (nation_id < 4 || nation_id > 11) {
    return 0;
  }
  const ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  const uint32_t gold = ctx->col1->nation[e].gold;

  const char* labels[3];
  char label_buf[3][48];
  int ids[3];
  int n = 0;
  for (int target = 0; target < 4 && n < 3; ++target) {
    if (target == e) {
      continue;
    }
    const uint32_t price =
      ai_contact_incite_price(ctx, ind, nation_id, e, target, is_missionary, is_capital);
    if (gold < price) {
      continue;
    }
    snprintf(
      label_buf[n],
      sizeof(label_buf[n]),
      "Incite against the %s (%u gold)",
      ai_contact_euro_name(target),
      (unsigned)price
    );
    labels[n] = label_buf[n];
    ids[n] = target;
    n++;
  }
  if (n == 0) {
    return 0;
  }
  char body[AI_POPUP_BODY_LEN];
  snprintf(
    body,
    sizeof(body),
    "The %s tribe is ready to go on the warpath. Whom would you like us to attack?",
    ai_contact_tribe_name(nation_id)
  );
  /* Carry the offer-time-captured discount flags through to apply time
   * (bit0=is_missionary, bit1=is_capital) — see ai_contact_incite_price. */
  const int payload = (is_missionary ? 1 : 0) | (is_capital ? 2 : 0);
  return ai_popup_enqueue_choice_ctx(
           ctx->ai_popups,
           AI_POPUP_TAG_CONTACT_INCITE,
           e,
           nation_id,
           payload,
           NULL,
           body,
           labels,
           ids,
           n
         )
           ? 1
           : 0;
}

/*
 * Apply Incite Indians (FUN_4d56_417e tail — @INDIANWARPATH2 "We will
 * gladly drive the %s from our ancestral lands in exchange for %d.").
 * Re-checks affordability (gold may have changed since the menu was
 * built), debits the inciter's treasury, and pushes the target nation's
 * alarm with this tribe (the DOS `apply(..., nation_B, 100, 0)` call —
 * approximated as a flat alarm bump, exact magnitude/semantics of the
 * DOS `100` argument unconfirmed).
 */
static void ai_contact_apply_incite(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  int target,
  int is_missionary,
  int is_capital
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || e < 0 || e > 3 ||
      target < 0 || target > 3 || target == e) {
    return;
  }
  const uint32_t price =
    ai_contact_incite_price(ctx, ind, nation_id, e, target, is_missionary, is_capital);
  ColonizeCol1Nation* nat = &ctx->col1->nation[e];
  if (nat->gold < price) {
    char refuse_fb[AI_POPUP_BODY_LEN];
    snprintf(
      refuse_fb,
      sizeof(refuse_fb),
      "The %s tribe demands more gold than you have.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_INCITE, nation_id, "Incite", refuse_fb
    );
    return;
  }
  nat->gold -= price;
  if (ind->alarm_by_player[target] < 245) {
    ind->alarm_by_player[target] = (uint16_t)(ind->alarm_by_player[target] + 10);
  }
  {
    char body[AI_POPUP_BODY_LEN];
    snprintf(
      body,
      sizeof(body),
      "\"We will gladly drive the %s from our ancestral lands in exchange for %u.\"",
      ai_contact_euro_name(target),
      (unsigned)price
    );
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_INCITE, nation_id, "Incite", body);
  }
}

/* Nearest Euro colony with warehouse tools ≥20 (mid demand tools arm). */
static ColonizeColony* ai_contact_nearest_tools_colony(
  ColonizeTurnContext* ctx,
  int e,
  int near_x,
  int near_y
) {
  if (!ctx || !ctx->colonies || e < 0 || e > 3) {
    return NULL;
  }
  int best_ci = -1;
  int best_d = 99;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id != e) {
      continue;
    }
    if (c->stock[COLONIZE_CARGO_TOOLS] < 20) {
      continue;
    }
    const int dist = ai_contact_dist(c->x, c->y, near_x, near_y);
    if (dist < best_d) {
      best_d = dist;
      best_ci = ci;
    }
  }
  return best_ci >= 0 ? &ctx->colonies->colonies[best_ci] : NULL;
}

/*
 * Nearest Euro Wagon Train with TOOLS hold ≥20 (GAME.TXT @INDIANWAGONS
 * reparations stand-in). Reach matches auto-trade wagon band (French 5 / else 4).
 */
static ColonizeUnit* ai_contact_nearest_tools_wagon(
  ColonizeTurnContext* ctx,
  int e,
  int near_x,
  int near_y,
  int* out_hold
) {
  if (out_hold) {
    *out_hold = -1;
  }
  if (!ctx || !ctx->units || e < 0 || e > 3) {
    return NULL;
  }
  const int max_dist = (e == 1) ? 5 : 4;
  int best_id = -1;
  int best_hold = -1;
  int best_d = 99;
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    ColonizeUnit* u = &ctx->units->units[ui];
    if (!u->active || u->nation_id != e) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
    if (!ty || !strstr(ty->name, "Wagon") || ty->cargo <= 0) {
      continue;
    }
    const int dist = ai_contact_dist(u->x, u->y, near_x, near_y);
    if (dist > max_dist || dist >= best_d) {
      continue;
    }
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (u->hold_goods_type[h] == COLONIZE_CARGO_TOOLS &&
          u->hold_goods_amount[h] >= 20) {
        best_d = dist;
        best_id = u->id;
        best_hold = h;
        break;
      }
    }
  }
  if (best_id < 0) {
    return NULL;
  }
  if (out_hold) {
    *out_hold = best_hold;
  }
  return units_get(ctx->units, best_id);
}

static int ai_contact_demand_can_pay_tools(
  ColonizeTurnContext* ctx,
  int e,
  ColonizeUnit* other,
  int near_x,
  int near_y
) {
  if (ai_contact_nearest_tools_colony(ctx, e, near_x, near_y)) {
    return 1;
  }
  if (ai_contact_nearest_tools_wagon(ctx, e, near_x, near_y, NULL)) {
    return 1;
  }
  return other && other->tools >= 20;
}

static int ai_contact_demand_can_pay_gold(const ColonizeTurnContext* ctx, int e) {
  return ctx && ctx->col1_ok && ctx->col1 && e >= 0 && e <= 3 &&
         ctx->col1->nation[e].gold >= 50u;
}

/*
 * Mid-band demand tools drain (−10 stock / wagon hold / unit tools) + friction −3.
 * Cite: FUN_5bfb_102a / 1092; GAME.TXT @INDIANWAGONS; indian_contact.md mid demand.
 */
static int ai_contact_apply_demand_tools(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  ColonizeUnit* other,
  int near_x,
  int near_y
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || e < 0 || e > 3) {
    return 0;
  }
  if (!ind->euro_diplo[e]) {
    return 0;
  }
  const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
  if (friction >= 55 || ind->alarm_by_player[e] >= 55 || friction < 40) {
    char refuse_fb[AI_POPUP_BODY_LEN];
    snprintf(
      refuse_fb,
      sizeof(refuse_fb),
      "The %s refuse demands.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Demand", refuse_fb
    );
    return 0;
  }
  ColonizeColony* c = ai_contact_nearest_tools_colony(ctx, e, near_x, near_y);
  int wag_hold = -1;
  ColonizeUnit* wag =
    c ? NULL : ai_contact_nearest_tools_wagon(ctx, e, near_x, near_y, &wag_hold);
  if (c) {
    c->stock[COLONIZE_CARGO_TOOLS] -= 10;
  } else if (wag && wag_hold >= 0) {
    wag->hold_goods_amount[wag_hold] -= 10;
    if (wag->hold_goods_amount[wag_hold] <= 0) {
      wag->hold_goods_amount[wag_hold] = 0;
      wag->hold_goods_type[wag_hold] = 0;
    }
  } else if (other && other->tools >= 20) {
    other->tools -= 10;
  } else {
    char refuse_fb[AI_POPUP_BODY_LEN];
    snprintf(
      refuse_fb,
      sizeof(refuse_fb),
      "The %s refuse demands.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Demand", refuse_fb
    );
    return 0;
  }
  ai_contact_friction_decay(ind, ctx->col1, nation_id, e, 3);
  {
    char trib_fb[AI_POPUP_BODY_LEN];
    snprintf(
      trib_fb,
      sizeof(trib_fb),
      "Tribute paid; tensions ease with the %s.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Demand", trib_fb
    );
  }
  return 1;
}

/*
 * Mid-band demand gold drain (−15 when treasury ≥50) + friction −3.
 * Cite: indian_contact.md mid demand gold stand-in (tools short / player pick).
 */
static int ai_contact_apply_demand_gold(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || e < 0 || e > 3) {
    return 0;
  }
  if (!ind->euro_diplo[e]) {
    return 0;
  }
  const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
  if (friction >= 55 || ind->alarm_by_player[e] >= 55 || friction < 40) {
    char refuse_fb[AI_POPUP_BODY_LEN];
    snprintf(
      refuse_fb,
      sizeof(refuse_fb),
      "The %s refuse demands.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Demand", refuse_fb
    );
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[e];
  if (nat->gold < 50u) {
    char refuse_fb[AI_POPUP_BODY_LEN];
    snprintf(
      refuse_fb,
      sizeof(refuse_fb),
      "The %s refuse demands.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Demand", refuse_fb
    );
    return 0;
  }
  nat->gold -= 15u;
  ai_contact_friction_decay(ind, ctx->col1, nation_id, e, 3);
  {
    char trib_fb[AI_POPUP_BODY_LEN];
    snprintf(
      trib_fb,
      sizeof(trib_fb),
      "Tribute paid; tensions ease with the %s.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Demand", trib_fb
    );
  }
  return 1;
}

/*
 * Human Demand amount CHOICE (tools vs gold) when mid-band and purse allows.
 * Returns 1 if enqueued. Cite: FUN_5bfb_102a / 1092; gift amount CHOICE mirror.
 */
static int ai_contact_enqueue_demand_amount_choice(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* other,
  int near_x,
  int near_y
) {
  if (!ctx || !ctx->ai_popups || !ctx->col1_ok || !ctx->col1 || e < 0 || e > 3) {
    return 0;
  }
  if (!ai_contact_euro_is_human(ctx, e)) {
    return 0;
  }
  const int can_tools = ai_contact_demand_can_pay_tools(ctx, e, other, near_x, near_y);
  const int can_gold = ai_contact_demand_can_pay_gold(ctx, e);
  if (!can_tools && !can_gold) {
    return 0;
  }
  const char* labels[2];
  int ids[2];
  int n = 0;
  if (can_tools) {
    labels[n] = "Pay tools (10)";
    ids[n] = AI_CONTACT_DEMAND_TOOLS;
    n++;
  }
  if (can_gold) {
    labels[n] = "Pay gold (15)";
    ids[n] = AI_CONTACT_DEMAND_GOLD;
    n++;
  }
  char body[AI_POPUP_BODY_LEN];
  snprintf(
    body,
    sizeof(body),
    "The %s demand tribute. How do you pay?",
    ai_contact_tribe_name(nation_id)
  );
  return ai_popup_enqueue_choice_ctx(
           ctx->ai_popups,
           AI_POPUP_TAG_CONTACT_DEMAND,
           e,
           nation_id,
           0,
           NULL,
           body,
           labels,
           ids,
           n
         )
           ? 1
           : 0;
}

/*
 * FUN_4d56_2154 meet economics: dual 16-word tables ask[] (DS:0x9e58) /
 * bid[] (DS:0x9e78). Phases 1–5 from viceroy_unpacked.c 81743–82057.
 * Cover: tribe-local 25-cell mask (full relative ring; 281f_0ce0 OPEN).
 * Terrain: map_dos_terr_class_at (281f_078c). Divisor *(0x8d52/−0x69d6) →
 * head.difficulty (0..4). Cite: indian_meet_scoring_2154.md.
 */
typedef struct AiContactMeetEcon2154 {
  int16_t ask[16];
  int16_t bid[16];
} AiContactMeetEcon2154;

static int ai_contact_2154_signed_quarter_double(int x) {
  /* Decomp: abs via xor, >>2, restore sign, *2. */
  const unsigned u = (unsigned)x;
  const unsigned sign = (unsigned)(x >> 15);
  const unsigned absv = (u ^ sign) - sign;
  const unsigned q = absv >> 2;
  return (int)(((q ^ sign) - sign) * 2u);
}

static int ai_contact_2154_clamp0_50(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 0x32) {
    return 0x32;
  }
  return v;
}

static int ai_contact_meet_economics_2154(
  ColonizeTurnContext* ctx,
  int indian_nation,
  const ColonizeCol1Tribe* tribe,
  AiContactMeetEcon2154* out
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->map || !tribe || !out ||
      indian_nation < 4 || indian_nation > 11) {
    return 0;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[indian_nation - 4];
  memset(out, 0, sizeof(*out));

  /* Phase 1: tribe-local 25-cell cover from colony relative overlap. */
  uint8_t cover[25];
  memset(cover, 0, sizeof(cover));
  const int tx = (int)tribe->x;
  const int ty = (int)tribe->y;
  if (ctx->colonies) {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      const ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      for (int ly = 0; ly < 5; ++ly) {
        for (int lx = 0; lx < 5; ++lx) {
          const int rel_x = tx - c->x + lx;
          const int rel_y = ty - c->y + ly;
          const int abs_x = rel_x - 2;
          const int abs_y = rel_y - 2;
          if (abs_x < 0 || abs_y < 0 || abs_x >= ctx->map->width ||
              abs_y >= ctx->map->height) {
            continue;
          }
          if (rel_x < 0 || rel_x > 4 || rel_y < 0 || rel_y > 4) {
            continue;
          }
          /*
           * FUN_281f_0ce0 -> FUN_15eb_06a6 -> FUN_15eb_05e2: ring-slot lookup
           * then colony+0x70 worker byte read; DOS compares the returned byte
           * as *signed* (0xff == "no worker" == not-covered). Decomp special-
           * cases the colony's own tile (lx==2 && ly==2, i.e. dx=dy=0) as
           * always covered; every other covered cell must be one of the 8
           * immediate work-ring tiles (N,E,S,W,NW,NE,SE,SW — colony.h order)
           * *and* currently worked. Col1 colony tiles[8..19] (the raw-save
           * 20-slot ring beyond the 8 immediate directions) never carry a
           * worker in either DOS or col1_bridge.c, so those cells are simply
           * never covered — an unworked adjacent tile stays "free" land for
           * the tribe scorer, matching the DOS byte-for-byte. Cite:
           * indian_meet_scoring_2154.md Phase 1 (281f_0ce0 work-slot OPEN).
           */
          const int dx = lx - 2;
          const int dy = ly - 2;
          if (dx == 0 && dy == 0) {
            cover[ly * 5 + lx] = 1;
            continue;
          }
          static const int k_dir_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
          static const int k_dir_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
          for (int dir = 0; dir < 8; ++dir) {
            if (k_dir_dx[dir] == dx && k_dir_dy[dir] == dy && c->tiles[dir] >= 0) {
              cover[ly * 5 + lx] = 1;
              break;
            }
          }
        }
      }
    }
  }

  /* Phase 2–3: tribe ±2 terr_class buckets (skip covered local cells). */
  int local_a0 = 0;
  int local_9e = 0;
  int local_9c = 0;
  int local_5e = 0;
  int local_74 = 0;
  int local_5a = 0;
  int local_6a = 0;
  int local_6e = 0;
  int local_58 = 0;
  int local_7a = 0;
  int local_68 = 0;
  int local_7e = 0;
  int local_66 = 0;

  for (int y = ty - 2; y <= ty + 2; ++y) {
    for (int x = tx - 2; x <= tx + 2; ++x) {
      if (x < 0 || y < 0 || x >= ctx->map->width || y >= ctx->map->height) {
        continue;
      }
      const int lx = x - tx + 2;
      const int ly = y - ty + 2;
      if (lx < 0 || lx > 4 || ly < 0 || ly > 4) {
        continue;
      }
      if (cover[ly * 5 + lx]) {
        continue;
      }
      const int terr = map_dos_terr_class_at(ctx->map, x, y) & 31;
      if (terr == 0x1b) {
        local_a0++;
      }
      if (terr == 0x1c) {
        local_9c++;
      }
      if (terr == 0x18) {
        local_5a += 4;
      }
      if (!(((terr < 8) || (terr > 15)) && ((terr < 16) || (terr > 23)))) {
        /* Forest bands 8..15 / 16..23. */
        local_6e++;
        if (terr >= 8 && terr <= 15) {
          local_9e = terr - 8;
        }
        if (terr >= 16 && terr <= 23) {
          local_9e = terr - 16;
        }
        if (local_9e < 3) {
          local_5e++;
          local_5a += 2;
        } else {
          local_74++;
          local_6a++;
          if (local_9e == 5) {
            local_58 += 2;
          }
          if (local_9e == 4) {
            local_7e += 2;
          }
          if (local_9e == 3) {
            local_68 += 2;
          }
        }
      } else if (terr == 0x19 || terr == 0x1a) {
        local_7a += (int)ind->tech + 1;
        while (local_7a > 2) {
          local_7a -= 3;
          local_6e += 2;
        }
      } else if (terr < 8) {
        if (terr == 5) {
          local_58 += 4;
        }
        if (terr == 7) {
          local_58 += 2;
        }
        if (terr == 4) {
          local_7e += 4;
        }
        if (terr == 6) {
          local_7e += 2;
        }
        if (terr == 3) {
          local_68 += 4;
        }
        if (terr == 0) {
          local_66 += 2;
        }
        if (terr == 2) {
          local_68 += 1;
          local_6e += 2;
        }
        if (terr < 2) {
          if (terr == 1) {
            local_6a += 4;
          } else if (terr == 0) {
            local_5a += 3;
          }
        } else if (terr < 6) {
          local_6e += 3;
          if ((terr & 4) != 0) {
            local_6a += 2;
          } else {
            local_5a += 2;
          }
        } else {
          local_66 += 1;
          local_6e += 2;
        }
      }
    }
  }

  /* Phase 4: write ask/bid words from tech, pop, buckets, indian fields. */
  const int pop = (int)tribe->population;
  const int pop1 = pop + 1;
  int tech = (int)ind->tech;
  if (tech > 6) {
    tech = 6; /* avoid (7-tech) zero / negative */
  }
  const int diff = (int)ctx->col1->head.difficulty; /* DS 0x8d52/−0x69d6 stand-in */
  int diff_div = diff;
  if (diff_div < 1) {
    diff_div = 1;
  }

  out->bid[0] = (int16_t)(((tech + pop1) * local_6e) / (7 - tech));
  out->ask[0] = (int16_t)((pop1 * pop1 * 4) >> (1 < tech ? 1 : 0));
  if (tech != 0) {
    if (tech > 1) {
      out->bid[7] = (int16_t)((int)ind->hill_silver_bid_bonus / diff_div);
      int a0_shift = local_a0 << 2;
      if (tech > 2) {
        a0_shift = local_a0 << 3;
      }
      out->bid[7] = (int16_t)((int)out->bid[7] + a0_shift);
    }
    if (ind->tech != 0) {
      out->bid[6] =
        (int16_t)((int)out->bid[6] + local_9c * 2 + local_a0 + local_66);
    }
  }
  out->bid[4] =
    (int16_t)((int)out->bid[4] + (local_5e * 2 + (local_74 >> 1)) / (tech + 1));
  out->bid[12] =
    (int16_t)ai_contact_2154_signed_quarter_double(((int)out->bid[4] + tech) * 2);
  out->bid[2] = (int16_t)((int)out->bid[2] + local_7e);
  out->bid[1] = (int16_t)((int)out->bid[1] + local_58);
  out->ask[11] =
    (int16_t)((tech + pop1) * pop1 + (local_6a >> 1) + local_5a);
  out->bid[3] = (int16_t)((int)out->bid[3] + local_68);
  {
    const int b11 = ai_contact_2154_signed_quarter_double((tech + (int)out->bid[3]) * 2);
    out->bid[11] = (int16_t)b11;
    out->ask[3] = (int16_t)(b11 + local_6a);
  }
  out->ask[2] = (int16_t)((6 - tech) * pop1 + local_5a * 2 + 5);
  out->ask[10] = (int16_t)(((pop1 * 2 - tech) + 7) * 2);
  out->ask[12] = (int16_t)(local_5a * 8 + (int)out->bid[4]);
  out->ask[9] = (int16_t)(((tech * 2 + pop1) * 2 + local_6a) * 2);
  out->ask[13] = (int16_t)((tech + 2) * (pop + 4) + 8);
  {
    const int sh = ((local_5a >> 1) + 1) & 31;
    out->ask[14] = (int16_t)((tech * pop1) << sh);
  }
  out->ask[15] = (int16_t)((-tech - ((int)ind->muskets - 7)) * 4);
  out->bid[8] =
    (int16_t)((int)ind->horse_breeding / ((diff >> 1) + 1));
  out->ask[8] = (int16_t)((-tech - ((int)ind->horse_herds - 9)) * 4);
  out->bid[15] = 0;

  /* Phase 5: clamp ask 0..0x32; capital mix; tons mix; half-cross. */
  for (int i = 0; i < 16; ++i) {
    out->ask[i] = (int16_t)ai_contact_2154_clamp0_50((int)out->ask[i]);
  }
  if (tribe->state.capital) {
    for (int i = 0; i < 8; ++i) {
      out->ask[i] = (int16_t)((int)out->ask[i] << 1);
    }
    for (int i = 13; i < 16; ++i) {
      out->ask[i] = (int16_t)((int)out->ask[i] + ((int)out->ask[i] >> 1));
    }
    for (int i = 7; i < 16; ++i) {
      out->bid[i] = (int16_t)((int)out->bid[i] << 1);
    }
  }
  for (int i = 0; i < 16; ++i) {
    const int bid0 = (int)out->bid[i];
    const int ask0 = (int)out->ask[i];
    const int tons = (int)ind->tons[i];
    if (tons < 1) {
      if (tons < 0) {
        out->bid[i] = (int16_t)(bid0 + ((tons + 0x32) / 100) * 2);
      }
    } else {
      out->ask[i] = (int16_t)(((-0x32 - tons) / 100) * 2 + ask0);
    }
    {
      const int bid1 = (int)out->bid[i];
      out->bid[i] = (int16_t)(bid1 - ((int)out->ask[i] >> 1));
      {
        const int floor_b = (bid0 > 0) ? 1 : 0;
        if ((int)out->bid[i] <= floor_b) {
          out->bid[i] = (int16_t)floor_b;
        }
      }
      out->ask[i] = (int16_t)((int)out->ask[i] - (bid1 >> 1));
      {
        const int floor_a = (ask0 > 0) ? 1 : 0;
        if ((int)out->ask[i] < floor_a) {
          out->ask[i] = (int16_t)floor_a;
        }
      }
    }
  }
  return 1;
}

/*
 * Gift / demand (5bfb_102a / 1092 via ai_popup; VGA PARKED).
 * 2154 tables: gift Generous when ask[0]-bid[0]≥1, gold≥0x4b, thin RNG;
 * else Large when gold≥20. Demand: ask[0]<bid[0] → gold-first else tools-first.
 * Cite: indian_meet_scoring_2154.md; FUN_5bfb after 2a1f_0434.
 */
static void ai_contact_gift_or_demand(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  ColonizeUnit* other,
  int near_x,
  int near_y
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || !other || e < 0 || e > 3) {
    return;
  }
  if (!ind->euro_diplo[e]) {
    return;
  }
  const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
  const int human = ai_contact_euro_is_human(ctx, e);
  /*
   * Same ≥55 gate as refuse-talk/teach: alarmed → no gift and no demand
   * payoff (no invented gold penalties). Cite: fandom Alarm — refuse trade.
   * Message band uses tribe friction (not alarm_by_player): gift-band (<40) →
   * "refuse gifts"; demand-band (≥40) → "refuse demands". Pair friction alone
   * would always be ≥55 when alarm≥55, making gift-band unreachable.
   */
  if (friction >= 55 || ind->alarm_by_player[e] >= 55) {
    if (human) {
      int tribe_fr = 0;
      if (ctx->col1->tribe) {
        for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
          const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
          if ((int)t->nation_id == nation_id && (int)t->alarm[e].friction > tribe_fr) {
            tribe_fr = (int)t->alarm[e].friction;
          }
        }
      }
      const int demand_band = tribe_fr >= 40;
      {
        char refuse_fb[AI_POPUP_BODY_LEN];
        snprintf(
          refuse_fb,
          sizeof(refuse_fb),
          demand_band ? "The %s refuse demands." : "The %s refuse gifts.",
          ai_contact_tribe_name(nation_id)
        );
        ai_contact_human_chrome(
          ctx,
          e,
          demand_band ? AI_POPUP_TAG_CONTACT_DEMAND : AI_POPUP_TAG_CONTACT_GIFT,
          nation_id,
          demand_band ? "Demand" : "Gift",
          refuse_fb
        );
      }
    }
    return; /* alarmed / very high — raids handle hostility; no invented gold penalty */
  }

  ColonizeCol1Nation* nat = &ctx->col1->nation[e];

  AiContactMeetEcon2154 econ;
  memset(&econ, 0, sizeof(econ));
  int have_econ = 0;
  const ColonizeCol1Tribe* sample = NULL;
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id == nation_id) {
        sample = t;
        break;
      }
    }
  }
  if (sample) {
    have_econ = ai_contact_meet_economics_2154(ctx, nation_id, sample, &econ);
  }
  const int ask0 = have_econ ? (int)econ.ask[0] : 0;
  const int bid0 = have_econ ? (int)econ.bid[0] : 0;
  const int delta = ask0 - bid0;

  /* Low friction gift / tribute (2154 ask−bid + gold≥0x4b → Generous). */
  if (friction < 40) {
    /* Cannot pay −10 gift drain → refuse with status (widgets unparked). */
    if (nat->gold < 10u) {
      char refuse_fb[AI_POPUP_BODY_LEN];
      snprintf(
        refuse_fb,
        sizeof(refuse_fb),
        "The %s refuse gifts.",
        ai_contact_tribe_name(nation_id)
      );
      ai_contact_human_chrome(
        ctx, e, AI_POPUP_TAG_CONTACT_GIFT, nation_id, "Gift", refuse_fb
      );
      return;
    }
    if (nat->gold < 20u) {
      return; /* mid purse: skip silent (needs ≥20 band to auto-gift Large) */
    }
    /*
     * DOS 5bfb after 2154: Generous when delta≥1, gold≥0x4b, and delta≥RNG
     * (281f_04d4 stand-in). Else Large. Cite: indian_meet_scoring_2154.md.
     */
    int generous = 0;
    if (delta >= 1 && nat->gold >= 0x4bu) {
      ColonizeDosRng local;
      ai_contact_local_rng(ctx, nation_id, &local);
      ColonizeDosRng* rng = ctx->rng ? ctx->rng : &local;
      const int roll = dos_rng_range(rng, 1, 100);
      if (delta >= roll) {
        generous = 1;
      }
    }
    if (generous) {
      ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 20u, 3);
      return;
    }
    ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 10u, 2);
    return;
  }

  /*
   * Mid friction (40–54) demand / payoff; ≥55 refused above.
   * ask[0] < bid[0] → gold-first; else tools-first (DOS LAB_5bfb_096c shape).
   */
  {
    const int gold_first = have_econ && ask0 < bid0;
    if (gold_first) {
      if (ai_contact_demand_can_pay_gold(ctx, e)) {
        ai_contact_apply_demand_gold(ctx, ind, nation_id, e);
        return;
      }
      if (ai_contact_demand_can_pay_tools(ctx, e, other, near_x, near_y)) {
        ai_contact_apply_demand_tools(ctx, ind, nation_id, e, other, near_x, near_y);
        return;
      }
    } else {
      if (ai_contact_demand_can_pay_tools(ctx, e, other, near_x, near_y)) {
        ai_contact_apply_demand_tools(ctx, ind, nation_id, e, other, near_x, near_y);
        return;
      }
      if (ai_contact_demand_can_pay_gold(ctx, e)) {
        ai_contact_apply_demand_gold(ctx, ind, nation_id, e);
        return;
      }
    }
  }
  {
    char refuse_fb[AI_POPUP_BODY_LEN];
    snprintf(
      refuse_fb,
      sizeof(refuse_fb),
      "The %s refuse demands.",
      ai_contact_tribe_name(nation_id)
    );
    ai_contact_human_chrome(
      ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Demand", refuse_fb
    );
  }
}

/*
 * FUN_5bfb_022e already-met Brave/Euro adjacency accept/refuse
 * (viceroy_unpacked.c:96751-96827) — the mechanic settlement_record_8d4a.md
 * flagged as blocked on an accept/decline sign ambiguity. Resolved
 * 2026-08-14 via live user gameplay testimony captured during this
 * session's DOSBox-X live-capture pass (see that doc's "SIGN CONVENTION
 * RESOLVED" section): giving food improves tribe relations, refusing
 * worsens them. GAME.TXT `@INDIANBEGFOOD` ("...will our brothers of
 * {colony} share the bounty...") is the matching text — confirmed zero
 * references anywhere in this project before this pass, a genuine unwired
 * gap, not an approximation being replaced.
 *
 * Real DOS trigger: tribe's `2154` scorer `ask[0]>bid[0]`, a candidate
 * Euro colony's relevant stock `> 0x4a` (74) — matches the user's own
 * independently-reported trigger condition ("only ever seen with
 * substantial food stores") — then `dos_rng_range(1,100) <= (ask-bid)`.
 * Which branch is which (re-read off the decompile 2026-08-31, bugs.md):
 * the choice returns `local_c`, and it is `local_c == 2` — the second
 * GAME.TXT row, "We offer you {%NUMBER0} of our {%NUMBER1 food}" — that
 * subtracts `colony->food >> 1`. So the *half is the voluntary gift*, and
 * refusing costs no food at all; the earlier reading had the two branches'
 * effects swapped, so accepting handed over a quarter and angered them
 * while refusing seized half and calmed them. Corrected here.
 *
 * On accept (`local_c == 2`): colony food −= half, the begging
 * settlement's alarm word toward this European is zeroed, and the nation
 * alarm is walked down 5 at a time from −5 (−10 off a capital) until it
 * sits below 0x47 — the floor-loop that was previously "not fully
 * reconciled" is exactly that `while (alarm + d >= 0x47) d -= 5;`.
 * On refuse (`local_c == 1`): no food moves, the settlement's alarm word
 * goes ×1.5 (`w += w >> 1`), and the nation alarm rises by
 * `((difficulty + 1) >> 1) + 1`, doubled off a capital.
 *
 * Still approximated: DOS halves that refuse bump when its upstream
 * "calm enough" roll passed (`bVar5`), part of the demand/threat selector
 * this port drives differently; and the DOS AI-vs-human branch-selection
 * nuance (a `param_1==2` hardcode in the human-controlled arm wasn't
 * reconciled with the user's own experience of a real Give/Refuse choice
 * regardless of nation played — implemented as a real CHOICE for any
 * human nation instead).
 */
/*
 * @INDIANBEGFOOD gift: exactly half the colony's store, DOS's own
 * `iVar15 = colony->food >> 1` in FUN_5bfb_022e (colony pointer DS:0x8542,
 * food at +0x9a). The same half is what NUMBER0 names in the accept row
 * ("We offer you {%NUMBER0} of our {%NUMBER1 food}") and what accepting
 * actually hands over, so label and effect read it from here rather than
 * computing it twice. Was a quarter, which under-asked (bugs.md).
 */
static int ai_contact_beg_food_gift(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  const int have = c->stock[COLONIZE_CARGO_FOOD];
  int gift = have >> 1;
  if (gift < 0) {
    gift = 0;
  }
  if (gift > have) {
    gift = have;
  }
  return gift;
}

/* Village alarm toward `e` as DOS's int16 at tribe+10+e*2 (0x54f6 family). */
static int ai_contact_tribe_alarm_word(const ColonizeCol1Tribe* t, int e) {
  return (int)t->alarm[e].friction | ((int)t->alarm[e].attacks << 8);
}

static void ai_contact_tribe_alarm_word_set(ColonizeCol1Tribe* t, int e, int w) {
  if (w < 0) {
    w = 0;
  }
  if (w > 0xffff) {
    w = 0xffff;
  }
  t->alarm[e].friction = (uint8_t)(w & 0xff);
  t->alarm[e].attacks = (uint8_t)((w >> 8) & 0xff);
}

static void ai_contact_apply_beg_food(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  int colony_id,
  int accept
) {
  (void)ind;
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->colonies || e < 0 || e > 3) {
    return;
  }
  if (colony_id < 0 || colony_id >= COLONIZE_COLONIES_MAX) {
    return;
  }
  ColonizeColony* c = &ctx->colonies->colonies[colony_id];
  if (!c->active || c->nation_id != e) {
    return;
  }
  int capital = 0;
  ColonizeCol1Tribe* target_tribe = NULL;
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id == nation_id) {
        target_tribe = t;
        capital = t->state.capital != 0;
        break;
      }
    }
  }
  ai_contact_bind_names(ctx);
  if (accept) {
    /*
     * DOS FUN_5bfb_022e, `local_c == 2` (the "We offer you ..." row): the
     * colony loses the half it just offered, the begging settlement's own
     * alarm word toward this European is zeroed outright, and the nation
     * alarm is walked down in steps of 5 (base -5, -10 from a capital)
     * until it lands under 0x47 --- `while (alarm + d >= 0x47) d -= 5;`.
     * Giving cannot make them angrier; the earlier port had the accept and
     * refuse effects the wrong way round (bugs.md).
     */
    const int gift = ai_contact_beg_food_gift(c);
    c->stock[COLONIZE_CARGO_FOOD] -= gift;
    if (c->stock[COLONIZE_CARGO_FOOD] < 0) {
      c->stock[COLONIZE_CARGO_FOOD] = 0;
    }
    if (target_tribe) {
      ai_contact_tribe_alarm_word_set(target_tribe, e, 0);
    }
    int d = capital ? -10 : -5;
    const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
    /* Bounded: d only ever falls, and alarm is a 0..100 byte. */
    for (int guard = 0; guard < 32 && alarm + d >= 0x47; ++guard) {
      d -= 5;
    }
    ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, -d);
    if (ctx->status && ctx->status_size) {
      snprintf(
        ctx->status, ctx->status_size, "We share %d food with the %s.", gift,
        ai_contact_tribe_name(nation_id)
      );
    }
  } else {
    /*
     * DOS `local_c == 1` (the "we gave at the office" row): refusing costs
     * no food at all --- the tribe begs, it does not raid. What it does is
     * multiply the settlement's alarm word by 1.5 (`w += w >> 1`) and add
     * `((difficulty + 1) >> 1) + 1` to the nation alarm, doubled off a
     * capital. (DOS also halves that add when the upstream "calm enough"
     * roll passed; that roll is part of the demand/threat selector this
     * port drives differently, so it is not modelled here.)
     */
    if (target_tribe) {
      const int w = ai_contact_tribe_alarm_word(target_tribe, e);
      ai_contact_tribe_alarm_word_set(target_tribe, e, w + (w >> 1));
    }
    int d = (((int)ctx->col1->head.difficulty + 1) >> 1) + 1;
    if (capital) {
      d *= 2;
    }
    ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, -d);
    if (ctx->status && ctx->status_size) {
      snprintf(
        ctx->status, ctx->status_size, "We turn the %s away empty-handed.",
        ai_contact_tribe_name(nation_id)
      );
    }
  }
}

static int ai_contact_beg_food_pending(const AiPopupState* st) {
  if (!st) {
    return 0;
  }
  for (int i = 0; i < st->queue_count; ++i) {
    if (st->queue[i].tag == AI_POPUP_TAG_CONTACT_BEGFOOD) {
      return 1;
    }
  }
  return st->open && st->current.tag == AI_POPUP_TAG_CONTACT_BEGFOOD;
}

/*
 * Trigger side of the above — once per Indian nation's §9 (post-pulse,
 * not inside the seed-100-sensitive quiet 14fe pulse), pick the first
 * already-met Euro colony with food stock > 74 and enough tribe/Euro
 * economic pressure (2154 ask>bid, RNG-gated) to beg. Human gets a real
 * Give/Refuse CHOICE (`@INDIANBEGFOOD`); AI Euro nations auto-accept
 * when it can spare the food (matches this file's established
 * AI-defaults-generous convention for gift-shaped decisions elsewhere).
 */
void ai_contact_try_village_beg_food(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->colonies || !ctx->col1->tribe || !ctx->rng) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  if (ctx->ai_popups && ai_contact_beg_food_pending(ctx->ai_popups)) {
    return;
  }
  const ColonizeCol1Tribe* sample = NULL;
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id == nation_id) {
      sample = t;
      break;
    }
  }
  if (!sample) {
    return;
  }
  AiContactMeetEcon2154 econ;
  memset(&econ, 0, sizeof(econ));
  if (!ai_contact_meet_economics_2154(ctx, nation_id, sample, &econ)) {
    return;
  }
  const int delta = (int)econ.ask[0] - (int)econ.bid[0];
  if (delta <= 0) {
    return;
  }
  /*
   * bugs.md: DOS begs at the colony a Brave actually walked next to — the
   * tribe cannot ask from across the map, and it must not re-ask every
   * turn. A colony qualifies only with one of this tribe's units standing
   * adjacent, and each tribe asks at most once per 8 turns.
   */
  static uint16_t s_beg_cooldown_until[8];
  const uint16_t now_turn = ctx->col1->head.turn;
  if (now_turn && s_beg_cooldown_until[nation_id - 4] > now_turn) {
    return;
  }
  for (int e = 0; e < 4; ++e) {
    if (!ind->euro_diplo[e] || ind->alarm_by_player[e] >= 55) {
      continue;
    }
    int best_ci = -1;
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id != e || c->stock[COLONIZE_CARGO_FOOD] <= 0x4a) {
        continue;
      }
      bool brave_adjacent = false;
      for (int ui = 0; ui < COLONIZE_UNITS_MAX && !brave_adjacent; ++ui) {
        const ColonizeUnit* bu = units_get_const(ctx->units, ui);
        if (bu && bu->active && bu->nation_id == nation_id && units_is_on_map(bu) &&
            abs(bu->x - c->x) <= 1 && abs(bu->y - c->y) <= 1) {
          brave_adjacent = true;
        }
      }
      if (!brave_adjacent) {
        continue;
      }
      best_ci = ci;
      break;
    }
    if (best_ci < 0) {
      continue;
    }
    const int roll = dos_rng_range(ctx->rng, 1, 100);
    if (roll > delta) {
      continue;
    }
    s_beg_cooldown_until[nation_id - 4] = (uint16_t)(now_turn + 8);
    ai_contact_bind_names(ctx);
    if (ai_contact_euro_is_human(ctx, e)) {
      const ColonizeColony* beg_colony = &ctx->colonies->colonies[best_ci];
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = ai_contact_tribe_name(nation_id);
      tok.string1 = beg_colony->name;
      /*
       * bugs.md: the accept row is "We offer you {%NUMBER0} of our
       * {%NUMBER1 food}" — without these it offered 0. NUMBER0 is exactly what
       * accepting hands over, NUMBER1 the colony's whole store.
       */
      tok.number0 = ai_contact_beg_food_gift(beg_colony);
      tok.has_number0 = true;
      tok.number1 = beg_colony->stock[COLONIZE_CARGO_FOOD];
      tok.has_number1 = true;
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        ctx->messages, "INDIANBEGFOOD", &tok,
        "\"The tribe has fallen upon hard times and does not have enough food this "
        "season.  Will you share your bounty with them?\"",
        body, sizeof(body)
      );
      char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
      const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "INDIANBEGFOOD");
      int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
      const char* labels[2];
      char label_buf[2][AI_POPUP_CHOICE_LEN];
      if (nch >= 2) {
        /* The rows carry %NUMBER tokens of their own — fill them. */
        for (int li = 0; li < 2; ++li) {
          popup_msg_apply_tokens(label_buf[li], sizeof(label_buf[li]), choice_buf[li], &tok);
          labels[li] = label_buf[li];
        }
      } else {
        snprintf(label_buf[0], sizeof(label_buf[0]), "I'm sorry, we gave at the office.");
        snprintf(
          label_buf[1], sizeof(label_buf[1]),
          "We offer you %d of our %d food as a sign of friendship.", tok.number0, tok.number1);
        labels[0] = label_buf[0];
        labels[1] = label_buf[1];
      }
      const int ids[2] = {1, 2}; /* 1=decline (label[0]), 2=accept (label[1]) */
      if (ai_popup_enqueue_choice_ctx(
            ctx->ai_popups, AI_POPUP_TAG_CONTACT_BEGFOOD, e, nation_id, best_ci, NULL, body,
            labels, ids, 2
          )) {
        if (ctx->status && ctx->status_size) {
          snprintf(
            ctx->status, ctx->status_size, "The %s beg for food at %s.",
            ai_contact_tribe_name(nation_id), ctx->colonies->colonies[best_ci].name
          );
        }
      }
    } else {
      /* AI Euro: auto-accept, handing over the same half a human would. */
      ai_contact_apply_beg_food(ctx, ind, nation_id, e, best_ci, 1);
    }
    return; /* one beg-for-food event per Indian nation per turn */
  }
}

/*
 * Missionary adjacent to tribe → convert / heresy pulse (5bfb / fandom
 * Missionaries / wiki heresy denounce):
 *  - mission unset (0xff) → set mission owner, alarm/friction decay (−1
 *    peaceful / −2 Jesuit mid-range 40..54), +1 nation crosses; human status
 *    "The %s accept conversion." / "… at %s."
 *  - own mission already set → skip (one-shot; no re-crosses).
 *  - foreign mission → denounce heresy 50/50 (wiki/HandWiki equal chance):
 *      success → replace with denouncer nation (regular cross — GameFAQs:
 *      heresy install is not Jesuit-bright), +1 crosses;
 *      fail → despawn denouncer (burned at the stake).
 *    Cite: docs/manual_gap.md; fandom Missionaries denounce; WARPATH gold
 *    Done thin (mechanic FUN_4d56_417e, structure mapped in
 *    original_sources_annotated/ai/indian_incite_417e.md; wired as a 6th
 *    village-meet CHOICE — ai_contact_apply_incite / AI_POPUP_TAG_CONTACT_INCITE
 *    below — price formula byte-faithful since 2026-08-14: both formerly
 *    unnamed tables identified as live per-tribe-type sums (village count,
 *    Σ combat_unit_base_x8 over Braves) and wired for real in
 *    ai_contact_incite_price above; only the human-driven Mode-1 path is
 *    wired. Mode-2 (AI Missionary auto-incites the village against the
 *    human) is confirmed real since 2026-08-27 — caller is FUN_4d56_4528
 *    tail-switch case 7, gate documented in indian_incite_417e.md
 *    "Caller: FOUND" — but not ported; see docs/ai_port_plan.md T4.5).
 *  - alarmed (≥55 refuse-talk gate) → refuse convert/heresy; no crosses
 *  - mid (40..54) convert: Jesuit-grade only (PEDIA @JOB24 / Brebeuf).
 * Teach/convert widgets Done structural; deep 2820 PARKED.
 */
/*
 * FUN_4d56_4528 non-human branch, unit type 3 (Missionary) → case 7 →
 * FUN_4d56_417e Mode 2 (static port 2026-08-27, T4.5). Gate, all of:
 * alarm(tribe → human) < 0x4b; human has MET the tribe; wealth rank of the
 * AI nation < the human's on the FUN_5bfb_00f8 table (compared literally on
 * ctx->euro_power_rank); AI gold >= 1500; RNG(0,4) != 0 or the village has
 * no mission. Then 417e Mode 2: target = human, no menu/confirm, diplo gate
 * + alarm(tribe → human) < 0x4b again + affordability, pay, push the tribe
 * against the human (+10 alarm — same effect as the human Mode 1 apply).
 * Returns 1 when the incite fired (the village keeps its mission state).
 */
int ai_contact_ai_incite_human(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  ColonizeCol1Tribe* t,
  int nation_id,
  int e,
  int is_missionary
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->rng || !ind || !t || e < 0 || e > 3 ||
      nation_id < 4 || nation_id > 11) {
    return 0;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human > 3 || human == e || ai_contact_euro_is_human(ctx, e)) {
    return 0;
  }
  if (ai_diplo_indian_alarm(ctx->col1, nation_id, human) >= 0x4b) {
    return 0;
  }
  if ((ind->euro_diplo[human] & COL1_INDIAN_MET_BIT) == 0) {
    return 0;
  }
  if (!ctx->euro_power_rank_ok || ctx->euro_power_rank[e] >= ctx->euro_power_rank[human]) {
    return 0; /* wealth_rank[ai] < wealth_rank[human] (0x917c table) */
  }
  if (ctx->col1->nation[e].gold < 1500u) {
    return 0;
  }
  if (dos_rng_range(ctx->rng, 0, 4) == 0 && t->mission != COL1_TRIBE_MISSION_NONE) {
    return 0;
  }
  /* 417e Mode 2 body: diplo gate (human at peace with the AI? DOS 8c28 & 0x20 = met)
   * then relation gate again, then pay. */
  if ((ctx->col1->nation[e].euro_relation[human] & AI_DIPLO_MET) == 0) {
    return 0;
  }
  const uint32_t price = ai_contact_incite_price(
    ctx, ind, nation_id, e, human, is_missionary, t->state.capital ? 1 : 0
  );
  ColonizeCol1Nation* nat = &ctx->col1->nation[e];
  if (nat->gold < price) {
    return 0;
  }
  nat->gold -= price;
  ai_diplo_indian_alarm_delta(ctx->col1, nation_id, human, 10);
  return 1;
}

/*
 * FUN_5bfb_022e's @INDIANSCONVERT arm (viceroy_unpacked.c:96989-97012), the
 * real way a player gets Indian Converts: a Brave from a settlement that holds
 * *your* mission walks up to one of your colonies, and
 *
 *     chance = indian.tech + 2      (doubled when the mission is Jesuit-grade)
 *     fires when rng(0,15) < chance
 *
 * On success DOS clears that settlement's alarm word for the nation, shows
 * GAME.TXT @INDIANSCONVERT with %STRING0 = the colony name, and spawns a unit
 * of type 0 (Colonists) at the colony owned by that nation with profession
 * 0x1b (Indian Convert). Establishing the mission in the first place is the
 * @ACTIONS menu's own action, not this.
 */
static void ai_contact_mission_convert_visit(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->colonies || !ctx->col1_ok || !ctx->col1 ||
      !ctx->col1->tribe) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  ColonizeDosRng local;
  ai_contact_local_rng(ctx, nation_id, &local);
  ColonizeDosRng* rng = ctx->rng ? ctx->rng : &local;
  const int convert_type = units_find_type(ctx->units, "Colonists");
  if (convert_type < 0) {
    return;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id || !units_is_on_map(brave)) {
      continue;
    }
    if (brave->home_tribe_id < 0 ||
        (uint16_t)brave->home_tribe_id >= ctx->col1->head.tribe_count) {
      continue;
    }
    ColonizeCol1Tribe* home = &ctx->col1->tribe[brave->home_tribe_id];
    if ((int)(int8_t)home->mission < 0) {
      continue; /* no mission in the Brave's own settlement */
    }
    const int e = (int)(home->mission & COL1_TRIBE_MISSION_NATION_MASK);
    if (e < 0 || e > 3) {
      continue;
    }
    if (ind->alarm_by_player[e] > 0x4a) {
      continue; /* DOS reaches this arm only below the war band */
    }
    /* The colony the Brave has walked up to. */
    ColonizeColony* target = NULL;
    for (int d = 0; d < 8 && !target; ++d) {
      const int cid = colonies_id_at(ctx->colonies, brave->x + dx[d], brave->y + dy[d]);
      ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
      if (c && c->active && c->nation_id == e) {
        target = c;
      }
    }
    if (!target) {
      continue;
    }
    int chance = (int)ind->tech + 2;
    if ((home->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0) {
      chance *= 2;
    }
    /*
     * DOS rolls this inside the visit itself — FUN_5bfb_022e runs when a Brave
     * *moves* next to the colony, so a Brave already parked there does not
     * re-roll. This pulse only sees standing positions, so cap it at one roll
     * per Indian nation per turn (return either way) rather than one roll per
     * adjacent Brave per turn, which turned a single loitering Brave into a
     * convert factory.
     */
    if (dos_rng_range(rng, 0, 15) >= chance) {
      return;
    }

    const int cid = units_spawn_allow_stack(ctx->units, convert_type, target->x, target->y);
    ColonizeUnit* convert = cid >= 0 ? units_get(ctx->units, cid) : NULL;
    if (!convert) {
      return;
    }
    convert->nation_id = (uint8_t)e;
    convert->profession = COLONIZE_PROF_CONVERT;
    home->alarm[e].friction = 0;
    home->alarm[e].attacks = 0;

    if (ai_contact_euro_is_human(ctx, e)) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = target->name[0] ? target->name : "our colony";
      char fb[AI_POPUP_BODY_LEN];
      snprintf(
        fb,
        sizeof(fb),
        "\"The wisdom of your missionaries has convinced some of us to join "
        "your colony at %s and live among you as converts.\"",
        tok.string0
      );
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "INDIANSCONVERT", &tok, fb, body, sizeof(body));
      ai_contact_human_chrome(
        ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "Mission", body
      );
    }
    return; /* one convert per tribe per pulse */
  }
}

static void ai_contact_missionary_convert(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  ColonizeDosRng local;
  ai_contact_local_rng(ctx, nation_id, &local);
  ColonizeDosRng* rng = ctx->rng ? ctx->rng : &local;
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int oid = units_id_at(ctx->units, t->x + dx[d], t->y + dy[d]);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id < 0 || other->nation_id > 3) {
        continue;
      }
      if (!ai_contact_is_missionary(ctx->units, other)) {
        continue;
      }
      const int e = other->nation_id;
      /*
       * bugs.md: this adjacency pulse is the AI's stand-in for working a
       * missionary, and only that. A human's missionary establishes a mission
       * or denounces heresy through the @ACTIONS village menu (rows 3 and 4,
       * AI_CONTACT_CHOICE_MISSION / _HERESY) — never by merely standing next
       * to a village, and never with a popup announcing it. Converts reaching
       * the player's colonies are a separate event
       * (ai_contact_mission_convert_visit, FUN_5bfb_022e's @INDIANSCONVERT arm).
       */
      if (ai_contact_euro_is_human(ctx, e)) {
        continue;
      }
      /*
       * Alarmed Indian diplomacy (fandom Alarm; same ≥55 refuse-talk gate):
       * refuse convert / heresy / crosses (status thinned; ai_popup Done).
       */
      if (ind->alarm_by_player[e] >= 55 || t->alarm[e].friction >= 55) {
        char refuse_fb[AI_POPUP_BODY_LEN];
        snprintf(
          refuse_fb,
          sizeof(refuse_fb),
          "The %s refuse conversion.",
          ai_contact_tribe_name(nation_id)
        );
        ai_contact_human_chrome(
          ctx,
          e,
          AI_POPUP_TAG_CONTACT_CONVERT,
          nation_id,
          "Mission",
          refuse_fb
        );
        break; /* one refuse pulse per tribe per call */
      }

      /* 4528 non-human Missionary: case 7 auto-incite against the human first. */
      if (!ai_contact_euro_is_human(ctx, e) &&
          ai_contact_ai_incite_human(ctx, ind, t, nation_id, e, 1)) {
        break;
      }

      /* Own mission keep — convert once (no re-crosses). */
      if (t->mission != COL1_TRIBE_MISSION_NONE &&
          (t->mission & COL1_TRIBE_MISSION_NATION_MASK) == (uint8_t)e) {
        break;
      }

      /*
       * Foreign mission → heresy denounce (50/50). Cite: Wikipedia /
       * HandWiki Colonization; fandom Missionaries denounce; GameFAQs
       * heresy install is regular (no Jesuit bit).
       */
      if (t->mission != COL1_TRIBE_MISSION_NONE) {
        const int foreign =
          (int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK);
        const int roll = dos_rng_range(rng, 0, 99);
        if (roll < 50) {
          t->mission = (uint8_t)e; /* regular mission; no Jesuit-bright bit */
          ColonizeCol1Nation* nat = &ctx->col1->nation[e];
          if (nat->current_crosses < 0xffffu) {
            nat->current_crosses++;
          }
          {
            char heresy_fb[AI_POPUP_BODY_LEN];
            snprintf(
              heresy_fb,
              sizeof(heresy_fb),
              "Heresy denounced; the %s burn the foreign mission.",
              ai_contact_tribe_name(nation_id)
            );
            ai_contact_human_chrome(
              ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "Mission", heresy_fb
            );
          }
          /* Thin: previous mission owner learns their mission burned. */
          if (foreign >= 0 && foreign <= 3 && foreign != e &&
              ai_contact_euro_is_human(ctx, foreign)) {
            char lose_fb[AI_POPUP_BODY_LEN];
            snprintf(
              lose_fb,
              sizeof(lose_fb),
              "The %s burn your mission!",
              ai_contact_tribe_name(nation_id)
            );
            ai_contact_human_chrome(
              ctx, foreign, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "Mission", lose_fb
            );
          }
        } else {
          units_despawn(ctx->units, oid);
          {
            char heresy_fb[AI_POPUP_BODY_LEN];
            snprintf(
              heresy_fb,
              sizeof(heresy_fb),
              "The %s burn your missionary at the stake.",
              ai_contact_tribe_name(nation_id)
            );
            ai_contact_human_chrome(
              ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "Mission", heresy_fb
            );
          }
        }
        break; /* one heresy pulse per tribe per call */
      }

      /*
       * Mid-alarm (40..54): Jesuit-grade only. Plain Missionary refuses
       * unless nation owns Brebeuf (fandom experts). Cite: COLONIZE/PEDIA.TXT
       * @JOB24; docs/fandom_col1994.md Brebeuf; indian_contact.md convert.
       */
      {
        const int mid =
          (ind->alarm_by_player[e] >= 40 && ind->alarm_by_player[e] < 55) ||
          (t->alarm[e].friction >= 40 && t->alarm[e].friction < 55);
        if (mid && !ai_contact_is_jesuit_grade(ctx->col1, ctx->units, other)) {
          char refuse_fb[AI_POPUP_BODY_LEN];
          snprintf(
            refuse_fb,
            sizeof(refuse_fb),
            "The %s refuse conversion.",
            ai_contact_tribe_name(nation_id)
          );
          ai_contact_human_chrome(
            ctx,
            e,
            AI_POPUP_TAG_CONTACT_CONVERT,
            nation_id,
            "Mission",
            refuse_fb
          );
          break; /* one mid-refuse pulse per tribe per call */
        }
      }
      /* Nation in low nibble; Jesuit-grade sets bit0x10 (FUN_5bfb / 5fef_31ea). */
      t->mission = (uint8_t)e;
      if (ai_contact_is_jesuit_grade(ctx->col1, ctx->units, other)) {
        t->mission = (uint8_t)(t->mission | COL1_TRIBE_MISSION_JESUIT_BIT);
      }
      /*
       * Mid-range Jesuit convert friction polish (40..54): stronger −2 decay
       * on establish (matches meet-pulse mission pacify mid band). Peaceful
       * (<40) keeps −1 for any missionary. Cite: fandom Alarm — missions
       * slow hostility; PEDIA Jesuit effectiveness; Brebeuf experts;
       * indian_contact.md. Convert +1 crosses only (no elect fiction).
       */
      {
        const int mid =
          (ind->alarm_by_player[e] >= 40 && ind->alarm_by_player[e] < 55) ||
          (t->alarm[e].friction >= 40 && t->alarm[e].friction < 55);
        const int decay = mid ? 2 : 1;
        if ((int)ind->alarm_by_player[e] > decay) {
          ind->alarm_by_player[e] =
            (uint16_t)(ind->alarm_by_player[e] - (uint16_t)decay);
        } else {
          ind->alarm_by_player[e] = 0;
        }
        if ((int)t->alarm[e].friction > decay) {
          t->alarm[e].friction = (uint8_t)(t->alarm[e].friction - (uint8_t)decay);
        } else {
          t->alarm[e].friction = 0;
        }
      }
      ColonizeCol1Nation* nat = &ctx->col1->nation[e];
      if (nat->current_crosses < 0xffffu) {
        nat->current_crosses++;
      }
      {
        /* GAME.TXT @INDIANSCONVERT: name nearest Euro colony when known. */
        char convert_fb[AI_POPUP_BODY_LEN];
        const char* col_name = NULL;
        int best_d = 99;
        if (ctx->colonies) {
          for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
            const ColonizeColony* c = &ctx->colonies->colonies[ci];
            if (!c->active || c->nation_id != e || !c->name[0]) {
              continue;
            }
            const int dist = ai_contact_dist(c->x, c->y, t->x, t->y);
            if (dist < best_d) {
              best_d = dist;
              col_name = c->name;
            }
          }
        }
        if (col_name && best_d <= 8) {
          snprintf(
            convert_fb,
            sizeof(convert_fb),
            "The %s accept conversion at %s.",
            ai_contact_tribe_name(nation_id),
            col_name
          );
        } else {
          snprintf(
            convert_fb,
            sizeof(convert_fb),
            "The %s accept conversion.",
            ai_contact_tribe_name(nation_id)
          );
        }
        ai_contact_human_chrome(
          ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "Mission", convert_fb
        );
      }
      break; /* one convert pulse per tribe per call */
    }
  }
}

/*
 * Missionary flee (structural): adjacent to alarmed tribe (≥55 refuse-talk
 * band) and not converting → nudge 1 free land tile away + AI_MOVE goto.
 * Cite: fandom Alarm — alarmed natives may refuse / attack missionaries.
 * Full 2820/4528 flee dialog PARKED; thin widgets Done (ai_popup).
 */
static int ai_contact_flee_one_tile(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int away_x,
  int away_y
) {
  if (!ctx || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  const int ox = u->x;
  const int oy = u->y;
  const int dist0 = ai_contact_dist(ox, oy, away_x, away_y);
  int best_x = -1;
  int best_y = -1;
  int best_d = -1;
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int nx = ox + dx[d];
    const int ny = oy + dy[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    if (!map_tile_is_land(ctx->map, nx, ny)) {
      continue;
    }
    if (units_id_at(ctx->units, nx, ny) >= 0) {
      continue;
    }
    if (!units_can_enter(ctx->units, u->type_index, ctx->map, nx, ny, u->id, ctx->colonies)) {
      continue;
    }
    const int dist = ai_contact_dist(nx, ny, away_x, away_y);
    if (dist < dist0) {
      continue; /* must increase Chebyshev distance from tribe */
    }
    if (dist > best_d) {
      best_d = dist;
      best_x = nx;
      best_y = ny;
    }
  }
  if (best_x < 0) {
    return 0;
  }
  {
    const int mv_ox = u->x;
    const int mv_oy = u->y;
    u->x = best_x;
    u->y = best_y;
    units_occupancy_notify_moved(ctx->units, mv_ox, mv_oy, best_x, best_y);
  }
  u->orders = UNITS_ORDER_AI_MOVE;
  u->goto_x = best_x;
  u->goto_y = best_y;
  return 1;
}

static void ai_contact_missionary_flee(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int oid = units_id_at(ctx->units, t->x + dx[d], t->y + dy[d]);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id < 0 || other->nation_id > 3) {
        continue;
      }
      if (!ai_contact_is_missionary(ctx->units, other)) {
        continue;
      }
      const int e = other->nation_id;
      /* Only flee when convert is blocked by alarm (not converting). */
      if (ind->alarm_by_player[e] < 55 && t->alarm[e].friction < 55) {
        continue;
      }
      if (ai_contact_flee_one_tile(ctx, other, t->x, t->y)) {
        /*
         * When mission unset, convert refuse chrome already wrote this pulse —
         * keep that status. Flee status when an established mission can't hold
         * amid alarm (missionary withdraws).
         */
        if (t->mission != COL1_TRIBE_MISSION_NONE) {
          char flee_fb[AI_POPUP_BODY_LEN];
          snprintf(
            flee_fb,
            sizeof(flee_fb),
            "Your missionary flees the %s village.",
            ai_contact_tribe_name(nation_id)
          );
          ai_contact_human_chrome(
            ctx,
            e,
            AI_POPUP_TAG_CONTACT_MEET,
            nation_id,
            "Mission",
            flee_fb
          );
        }
        break; /* one flee pulse per tribe per call */
      }
    }
  }
}

/*
 * Meet-pulse mission pacify deepen: mission owner present and mid-range
 * friction/alarm (40..80, below FUN_4cc6_0000 clear) → −2 tribe friction and
 * matching alarm_by_player (floor 0). Once per tribe per call.
 * Magnitude stays near prelude low-band −1; no free crosses.
 * Source: fandom Alarm — missions slow hostility / pacify.
 */
static void ai_contact_mission_pacify_meet(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id || t->mission == COL1_TRIBE_MISSION_NONE) {
      continue;
    }
    const int euro = (int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK);
    if (euro < 0 || euro > 3) {
      continue;
    }
    const int fr = (int)t->alarm[euro].friction;
    const int al = (int)ind->alarm_by_player[euro];
    /* Mid-range only; low-band stays prelude −1; ≥80 → mission burn/clear. */
    if ((fr < 40 || fr >= 80) && (al < 40 || al >= 80)) {
      continue;
    }
    if (fr >= 40 && fr < 80) {
      t->alarm[euro].friction = (uint8_t)(fr >= 2 ? fr - 2 : 0);
    }
    if (al >= 40 && al < 80) {
      ind->alarm_by_player[euro] = (uint16_t)(al >= 2 ? al - 2 : 0);
    }
  }
}

/*
 * FUN_4d56_1816 item 2 (War of Independence tribe defection) — thin port.
 * See indian_woi_defect_1816.md for the full raw-decomp derivation. Once
 * per Indian nation per turn while WoI is declared, a not-yet-resolved
 * tribe may defect to the rebel (human) side: relation vs. the human jumps
 * by +100, relation vs. the crown drops by -100 (DOS's own literal deltas
 * on ai_diplo_indian_relation_delta-shaped storage, not a hard "set to
 * max/min"), plus a one-time musket/horse windfall, then the tribe is
 * latched (woi_defect_resolved) so it isn't re-rolled every turn.
 *
 * Approximated: DOS derives the musket/horse windfall's tech cap from a
 * `DS:0x8d52`-selected "tribe" tech-lookup table whose value at this call
 * site isn't independently confirmed; substituted the same nation's own
 * already-mapped `tech` field (same conceptual quantity, most likely the
 * same table). Exact DOS status-message wording is not reproduced (see
 * doc). `woi_defect_forced` (DOS bit 0x40) has no known setter this pass —
 * the eligibility gate below never gets forced on for now. DOS's
 * `FUN_2a1f_0398` "mission clear" side-effect IS wired (below, after the
 * windfall) — byte-exact once its target (FUN_4cc6_0000) was fully read.
 */
void ai_contact_indian_woi_defect(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->rng || nation_id < 4 || nation_id > 11) {
    return;
  }
  if (!ai_king_independence_declared(ctx->col1)) {
    return;
  }
  const int human = ctx->human_nation;
  if (human < 0 || human > 3) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  if (ind->woi_defect_resolved) {
    return;
  }

  int eligible = ind->woi_defect_forced != 0;
  if (!eligible) {
    /* FUN_281f_030c = alarm toward the rebel nation: >= 25 and RNG(1,400) >= alarm. */
    const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, human);
    if (alarm >= 25) {
      const int roll = dos_rng_range(ctx->rng, 1, 400);
      eligible = roll >= alarm;
    }
  }
  if (!eligible) {
    return;
  }

  const int difficulty = ctx->col1->head.difficulty;
  const int span = (5 - difficulty) * 2; /* (difficulty-5)*-2: 10/8/6/4/2 */
  if (dos_rng_range(ctx->rng, 0, span) != 0) {
    return;
  }

  ind->woi_defect_resolved = 1;
  const int crown = ai_king_crown_nation_col1(ctx->col1_ok ? ctx->col1 : NULL, human);
  /*
   * FUN_4cc6_00f2(tribe, declaring, +100) / (tribe, crown, -100) are ALARM
   * deltas: the tribe turns fully hostile to the rebels and content with the
   * Crown (Tory natives). The earlier port had this inverted via the
   * relation_by_indian mis-mapping (2026-08-27).
   */
  ai_diplo_indian_alarm_delta(ctx->col1, nation_id, human, 100);
  ai_diplo_indian_alarm_delta(ctx->col1, nation_id, crown, -100);

  const int tech = ind->tech;
  int muskets = ind->muskets;
  if (muskets > tech) {
    muskets = tech;
  }
  ind->muskets = (uint8_t)(muskets * 4); /* DOS <<2; byte truncation matches */
  int horses = ind->horse_herds;
  if (horses > tech) {
    horses = tech;
  }
  ind->horse_herds = (uint8_t)horses;
  ind->horse_breeding = (uint16_t)(horses * 25);

  /*
   * FUN_2a1f_0398 "mission clear" side-effect, resolved 2026-08-14 (thunk
   * to FUN_4cc6_0000, viceroy_unpacked.c:80774-80802 — a clean, uncorrupted
   * canonical copy, no Ghidra needed): scans col1->tribe[] (same 18-byte
   * stride / +2 nation_id / +5 mission fields the Incite discount loop
   * already uses) for records with nation_id == this tribe's own
   * nation_id and mission's low nibble == the declaring (human) nation,
   * clearing them to "none". I.e. every village of this SAME Indian
   * nation that currently hosts a mission from the rebel side loses it
   * when one of its villages defects — DOS's own literal condition
   * (type == param_1+4 where param_1 is the tribe *type*, 0-7) collapses
   * to exactly `nation_id` here since this call always passes the
   * defecting tribe's own type. DOS then shows the human player an
   * informational popup (string id 0x14c8, exact text unrecoverable
   * without a live capture) only when anything was actually cleared —
   * folded into the existing status line instead.
   */
  int missions_cleared = 0;
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id != nation_id) {
        continue;
      }
      if ((t->mission & COL1_TRIBE_MISSION_NATION_MASK) != (uint8_t)human) {
        continue;
      }
      t->mission = COL1_TRIBE_MISSION_NONE;
      missions_cleared = 1;
    }
  }

  if (ctx->status && ctx->status_size > 0) {
    ai_contact_bind_names(ctx);
    if (missions_cleared) {
      snprintf(
        ctx->status,
        ctx->status_size,
        "The %s tribe declares for the rebel cause! Our missions among them are cleared.",
        ai_contact_tribe_name(nation_id)
      );
    } else {
      snprintf(
        ctx->status,
        ctx->status_size,
        "The %s tribe declares for the rebel cause!",
        ai_contact_tribe_name(nation_id)
      );
    }
  }
}

void ai_contact_indian_prelude(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ai_contact_bind_names(ctx);
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  ai_contact_clamp_alarms(ind);

  /*
   * Alarm prelude escalate (PARKED dialog chrome).
   * DOS: when state+3 bit 0x20 clear, difficulty-scaled RNG may set war/alarm.
   * Linux: unknown31_flags bit 0x20 = prelude-fired; isolated RNG only.
   * Pocahontas still halves the escalate bump (wiki/fandom half-rate).
   */
  uint8_t* flag = &ind->unknown31_flags;
  if ((*flag & 0x20) == 0) {
    ColonizeDosRng local;
    ai_contact_local_rng(ctx, nation_id, &local);
    const int diff = ctx->col1->head.difficulty;
    /* Harder → more often escalate. */
    const int chance = 2 + (4 - diff);
    if (dos_rng_range(&local, 1, 8) <= chance) {
      for (int e = 0; e < 4; ++e) {
        if (ctx->col1->player[e].control == 2) {
          continue;
        }
        if (ind->euro_diplo[e] && ind->alarm_by_player[e] < 30) {
          /* Pocahontas/French: half-rate alarm growth (wiki/fandom). */
          const int bump = ai_contact_alarm_bump_amount(
            ctx->col1, e, 5 + (4 - diff)
          );
          if (bump > 0) {
            ind->alarm_by_player[e] =
              (uint16_t)(ind->alarm_by_player[e] + (uint16_t)bump);
            if (ctx->col1->tribe) {
              for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
                ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
                if ((int)t->nation_id == nation_id) {
                  ai_contact_bump_u8_cap100(&t->alarm[e].friction, bump);
                }
              }
            }
          }
        }
      }
      /* War-ish sticky: mark bit so prelude does not re-roll forever. */
      *flag = (uint8_t)(*flag | 0x20);
    }
  }

  if (!ctx->col1->tribe) {
    return;
  }

  /*
   * Encroachment deepen (dialog chrome PARKED): Soldier/Scout/Pioneer/Dragoon within
   * Chebyshev ≤2 of a tribe with no mission → +2 tribe friction + alarm_by_player
   * toward that Euro (cap 100). Pocahontas halves bump (wiki/fandom half-rate).
   */
  if (ctx->units) {
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      ColonizeUnit* u = &ctx->units->units[ui];
      if (!u->active || u->nation_id < 0 || u->nation_id > 3) {
        continue;
      }
      if (units_is_sea(ctx->units, u->id)) {
        continue;
      }
      if (!ai_contact_is_encroacher(ctx->units, u)) {
        continue;
      }
      const int e = u->nation_id;
      const int bump = ai_contact_alarm_bump_amount(ctx->col1, e, 2);
      if (bump <= 0) {
        continue;
      }
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id != nation_id) {
          continue;
        }
        if (t->mission != COL1_TRIBE_MISSION_NONE) {
          continue; /* mission present → no encroachment bump */
        }
        if (ai_contact_dist(u->x, u->y, t->x, t->y) > 2) {
          continue;
        }
        ai_contact_bump_u8_cap100(&t->alarm[e].friction, bump);
        ai_contact_bump_u16_cap100(&ind->alarm_by_player[e], bump);
        /* @INDIANCOMMENT chrome only when colonies exist (see colony block). */
      }
    }
  }

  /*
   * Colony encroachment (fandom Alarm; GAME.TXT @INDIANFOREST2 colony wording):
   * Euro colony within Chebyshev ≤2 of unmissioned tribe → same +2 bump as
   * unit encroachers (Pocahontas/French half). Reuses @INDIANCOMMENT mid-cross.
   * Road/forest bribe CHOICE PARKED (no invented gold).
   */
  if (ctx->colonies) {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      const int e = c->nation_id;
      const int bump = ai_contact_alarm_bump_amount(ctx->col1, e, 2);
      if (bump <= 0) {
        continue;
      }
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id != nation_id) {
          continue;
        }
        if (t->mission != COL1_TRIBE_MISSION_NONE) {
          continue;
        }
        if (ai_contact_dist(c->x, c->y, t->x, t->y) > 2) {
          continue;
        }
        const int fr_before = (int)t->alarm[e].friction;
        ai_contact_bump_u8_cap100(&t->alarm[e].friction, bump);
        ai_contact_bump_u16_cap100(&ind->alarm_by_player[e], bump);
        if (fr_before < 40 && (int)t->alarm[e].friction >= 40) {
          char comment_fb[AI_POPUP_BODY_LEN];
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = ai_contact_tribe_name(nation_id);
          tok.string1 = c->name[0] ? c->name : "our colonies";
          popup_msg_fill(
            ctx->messages,
            "INDIANCOMMENT",
            &tok,
            "Natives are concerned that your colonies are beginning to overuse the lands near their settlements.",
            comment_fb,
            sizeof(comment_fb)
          );
          ai_contact_human_chrome(
            ctx,
            e,
            AI_POPUP_TAG_CONTACT_MEET,
            nation_id,
            "Natives",
            comment_fb
          );
        }
      }
    }
  }

  /*
   * Mission pacifies: tribe with mission + low friction toward mission Euro →
   * extra −1 friction (and matching alarm_by_player if also low). Dialog PARKED.
   */
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    if (t->mission == COL1_TRIBE_MISSION_NONE) {
      continue;
    }
    const int euro = (int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK);
    if (euro < 0 || euro > 3) {
      continue;
    }
    if (t->alarm[euro].friction < 40 && t->alarm[euro].friction > 0) {
      t->alarm[euro].friction--;
    }
    if (ind->alarm_by_player[euro] < 40 && ind->alarm_by_player[euro] > 0) {
      ind->alarm_by_player[euro]--;
    }
  }

  /*
   * Mission destroy / burn on high alarm (FUN_4cc6_0000; tribe.mission field).
   * Cite: manual/wiki — alarmed natives may burn missions. Alarm/friction ≥80
   * + mission present → clear stand-in (0xff). Status thinned; ai_popup Done.
   */
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    if (t->mission == COL1_TRIBE_MISSION_NONE) {
      continue;
    }
    const int euro = (int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK);
    if (euro < 0 || euro > 3) {
      continue;
    }
    if (t->alarm[euro].friction >= 80 || ind->alarm_by_player[euro] >= 80) {
      t->mission = 0xff;
      {
        char burn_fb[AI_POPUP_BODY_LEN];
        snprintf(
          burn_fb,
          sizeof(burn_fb),
          "The %s burn your missions!",
          ai_contact_tribe_name(nation_id)
        );
        ai_contact_human_chrome(
          ctx,
          euro,
          AI_POPUP_TAG_CONTACT_RAID,
          nation_id,
          "Mission",
          burn_fb
        );
      }
    }
  }
}

void ai_contact_indian_relation_tick(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 4 || nation_id > 11) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];

  /*
   * The former "relation ±1 by alarm band" arm is gone (2026-08-27): it moved
   * a Linux-only scalar; DOS alarm has no per-turn drift (seed-100 TURN3-7
   * saves) — the real ±1 is the 152e accumulator in ai.c. Friction part kept.
   */
  for (int e = 0; e < 4; ++e) {
    if (ctx->col1->player[e].control == 2) {
      continue;
    }
    /*
     * Goods/relation tick deepen (fandom Alarm cools / rises with band):
     *  - met + alarm cool (<40) → tribe friction −1 (floor 0; <40 band)
     *  - met + alarm hot (>40) → tribe friction +1 (cap 100)
     * Same bands as relation ±1. Cite: indian_contact.md relation tick;
     * deep 4962 census PARKED.
     */
    if (ind->euro_diplo[e] && ctx->col1->tribe) {
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id != nation_id) {
          continue;
        }
        if (ind->alarm_by_player[e] < 40) {
          if (t->alarm[e].friction > 0 && t->alarm[e].friction < 40) {
            t->alarm[e].friction--;
          }
        } else {
          /* alarm ≥40 mid/hot band — slight friction rise */
          ai_contact_bump_u8_cap100(&t->alarm[e].friction, 1);
        }
      }
    }
  }
}

/*
 * FUN_4d56_2af6 abort-trade close (catalog): clear tribe last_bought /
 * last_sold bookkeeping before refuse chrome. Deep demand-table wipe at
 * DOS −25000 stays PARKED (no Linux table). Cite: FUNCTION_CATALOG 2af6.
 */
/*
 * ===========================================================================
 * FUN_4d56_2820 — village trade (structural port; rewritten 2026-08-29
 * against the clean 595-line recovery in indian_trade_2820.md).
 *
 * One function in DOS, three phases here, tied together by a per-Euro
 * session (`s_2820[e]`) that survives the popup round trips:
 *
 *   shell   — tables (2154 ask/bid + the shell's own zeroing), hold pick
 *             (iStack_7e/iStack_c8; human: menu of holds, AI: RNG), then
 *   sell    — LAB_002bbc: the unit sells the picked hold to the tribe
 *             (@TRADE0/@TRADE1 loop, gift arm, @BADHAGGLE0), gold CREDITED,
 *             the whole hold slot removed (FUN_1000_8cdc), qty = hold amount
 *             (DS:0x8dc4 is that amount, stashed by the slot remover); then
 *   buy     — LAB_002e92: @BRING, then the tribe sells its own goods
 *             (@BUYWHICH → @BUY0/@BUY1 loop, @NOTENOUGH, @BADHAGGLE2).
 *             Only reached after a completed sale (iStack_c6 != 0); an
 *             empty-handed unit gets @BRING and nothing else.
 *
 * Human-with-cargo gates before the sell loop: last_bought/last_sold ==
 * cargo or ask[cargo] == 0 → @BADCARGO; sticky_trade_good == cargo →
 * @BADHAGGLE1. AI (iStack_8 == 0): iStack_5e = alarm > 0x31 ? 3 (gift) : 1.
 *
 * Not ported: BGM cue (FUN_1000_8688 5/6/7), the VGA chief portrait frame
 * (ai_popup portrait side-channel covers the picture).
 * ===========================================================================
 */
typedef struct AiContact2820 {
  int active;
  int nation_id;
  int unit_id;
  int cargo; /* iStack_c8; -1 = empty-handed */
  int slot; /* iStack_7e */
  int qty; /* iStack_6a = hold amount; DS:0x8dc4 after the slot removal */
  int16_t ask[16]; /* DS:0x9e58 (-25000) after the shell's zeroing */
  int16_t bid[16]; /* DS:0x9e78 (-0x6188) */
  int cand[16]; /* acStack_98: cargo ids sorted ascending by bid */
  int price; /* uStack_62 */
  int fair; /* iStack_ce */
  int c4; /* iStack_c4 */
  int tier2; /* iStack_80 */
  int value_idx; /* uStack_ca → NAMES @VALUES */
  int round; /* iStack_88 */
  int sold_ok; /* iStack_c6 */
  int buy_cargo; /* LAB_002e92 pick */
  int buy_qty;
  ColonizeDosRng rng;
} AiContact2820;
static AiContact2820 s_2820[4];

/* NAMES.TXT @VALUES (DS:-0x6cc0 table): "low quality" / "good" / "fine" / "excellent". */
static const char* ai_contact_values_name(int idx) {
  static const char* const k_values[4] = {"low quality", "good", "fine", "excellent"};
  static char live[32];
  if (idx < 0) {
    idx = 0;
  }
  if (idx > 3) {
    idx = 3;
  }
  if (s_contact_names) {
    const ColonizeMsgSection* sec = assets_msg_find(s_contact_names, "VALUES");
    if (sec) {
      int row = 0;
      for (int i = 0; i < sec->line_count; ++i) {
        const char* line = sec->lines[i];
        if (!line || line[0] == '\0' || line[0] == ';') {
          continue;
        }
        if (row == idx) {
          snprintf(live, sizeof(live), "%s", line);
          return live;
        }
        row++;
      }
    }
  }
  return k_values[idx];
}

/* FUN_1000_8c50 → FUN_15dc_00a2: quartile bucket of the 0..100 alarm. */
static int ai_contact_2820_quartile(int alarm) {
  return alarm < 25 ? 0 : alarm < 50 ? 1 : alarm < 75 ? 2 : 3;
}

static int ai_contact_2820_holds_used(const ColonizeUnit* unit) {
  int used = 0;
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (unit->hold_goods_amount[i] > 0) {
      used++;
    }
  }
  return used;
}

/* DOS: max_holds[type] - unit+0x3150 >= 1. */
static int ai_contact_2e92_unit_can_take(const ColonizeTurnContext* ctx, const ColonizeUnit* unit) {
  if (!ctx || !ctx->units || !unit) {
    return 0;
  }
  const ColonizeUnitType* ty = units_type(ctx->units, unit->type_index);
  if (!ty || ty->cargo <= 0) {
    return 0;
  }
  const int cap = ty->cargo > COLONIZE_UNIT_CARGO_MAX ? COLONIZE_UNIT_CARGO_MAX : ty->cargo;
  return (cap - ai_contact_2820_holds_used(unit)) >= 1;
}

/* FUN_1000_8cdc → FUN_15eb_317c: drop hold `slot`, compact the rest. Returns qty. */
static int ai_contact_2820_remove_slot(ColonizeUnit* unit, int slot) {
  if (!unit || slot < 0 || slot >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  const int qty = unit->hold_goods_amount[slot];
  for (int i = slot; i + 1 < COLONIZE_UNIT_CARGO_MAX; ++i) {
    unit->hold_goods_type[i] = unit->hold_goods_type[i + 1];
    unit->hold_goods_amount[i] = unit->hold_goods_amount[i + 1];
  }
  unit->hold_goods_type[COLONIZE_UNIT_CARGO_MAX - 1] = 0;
  unit->hold_goods_amount[COLONIZE_UNIT_CARGO_MAX - 1] = 0;
  return qty;
}

/* FUN_1000_8f48 → FUN_0000_8f68: merge into a matching slot or open a new one. */
static void ai_contact_2e92_give_goods(ColonizeUnit* unit, int cargo, int qty) {
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (unit->hold_goods_amount[i] > 0 && unit->hold_goods_type[i] == cargo) {
      unit->hold_goods_amount[i] += qty;
      return;
    }
  }
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (unit->hold_goods_amount[i] <= 0) {
      unit->hold_goods_type[i] = cargo;
      unit->hold_goods_amount[i] = qty;
      return;
    }
  }
}

static ColonizeCol1Tribe* ai_contact_2e92_tribe(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx->col1->tribe) {
    return NULL;
  }
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    if ((int)ctx->col1->tribe[ti].nation_id == nation_id) {
      return &ctx->col1->tribe[ti];
    }
  }
  return NULL;
}

/* tribe+10+e*2 (int16: friction | attacks<<8) -= sub, floor 0; qty == 100 → 0. */
static void ai_contact_2820_friction_sub(ColonizeCol1Tribe* t, int e, int sub, int qty) {
  if (!t || e < 0 || e > 3) {
    return;
  }
  int w = (int)t->alarm[e].friction | ((int)t->alarm[e].attacks << 8);
  w -= sub;
  if (w < 0) {
    w = 0;
  }
  if (qty == 100) {
    w = 0;
  }
  t->alarm[e].friction = (uint8_t)(w & 0xff);
  t->alarm[e].attacks = (uint8_t)((w >> 8) & 0xff);
}

/* FUN_1000_a0c0 → FUN_1cf8_000a: stable insertion sort of ids ascending by key. */
static void ai_contact_2820_sort(const int16_t* key, int* order) {
  for (int i = 0; i < 16; ++i) {
    order[i] = i;
  }
  for (int i = 1; i < 16; ++i) {
    const int v = order[i];
    int j = i - 1;
    while (j >= 0 && key[order[j]] > key[v]) {
      order[j + 1] = order[j];
      j--;
    }
    order[j + 1] = v;
  }
}

/* -0x7b44 + nation*0x10 + good wraps to the fixed DS:0x84BC row (captured 2026-08-22). */
static const uint8_t k_2820_throttle[16] = {0x00, 0x05, 0x02, 0x03, 0x04, 0x01, 0x04, 0x13,
                                            0x02, 0x0a, 0x0a, 0x0e, 0x09, 0x02, 0x01, 0x02};

/*
 * Shell tables: 2154 ask/bid, then
 *   bid0 = bid[0]; bid[0] = 0; if (bid[13] < bid0) ask[0] = 0;
 *   sort by bid; top three: ask[c] = 0 (a food slot becomes cloth, 0xc).
 */
static int ai_contact_2820_prepare(
  ColonizeTurnContext* ctx, int nation_id, AiContact2820* s
) {
  const ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id);
  if (!t) {
    return 0;
  }
  AiContactMeetEcon2154 econ;
  memset(&econ, 0, sizeof(econ));
  if (!ai_contact_meet_economics_2154(ctx, nation_id, t, &econ)) {
    return 0;
  }
  memcpy(s->ask, econ.ask, sizeof(s->ask));
  memcpy(s->bid, econ.bid, sizeof(s->bid));
  const int bid0 = s->bid[0];
  s->bid[0] = 0;
  if (s->bid[13] < bid0) {
    s->ask[0] = 0;
  }
  ai_contact_2820_sort(s->bid, s->cand);
  for (int i = 1; i < 4; ++i) {
    const int c = s->cand[16 - i];
    s->ask[c] = 0;
    if (c == 0) {
      s->cand[16 - i] = 12;
    }
  }
  return 1;
}

/*
 * LAB_002bbc price (iStack_c8 = cargo, iStack_6a = qty, aiStack_d6[0] = alarm):
 *   r = RNG(1,5); base = cargo > 8 ? 7 : 6;
 *   13: base -= RNG(0,7); 15: base -= muskets-12; 8: base -= horse_herds-10; 14: base += 1
 *   tier2 = quartile(alarm)<<1 (0 for 15/8; >>1 when ask > 19)
 *   raw = max(0, ((base-diff) - tier2 + r + 4) * 2 * ask)
 *   price = max(1, ((r*5 + raw) * qty / 100) / 2)
 *   iVar8 = ask - tier2 + 4; value_idx = min(3, iVar8/10); c4 = RNG(0,1) + (iVar8>>2)
 *   fair = (ask+1)*4 + price
 */
static int ai_contact_2820_sell_price(
  const ColonizeCol1Indian* ind, int cargo, int ask, int qty, int difficulty, int alarm,
  AiContact2820* s
) {
  ColonizeDosRng* rng = &s->rng;
  const int r = dos_rng_range(rng, 1, 5);
  int base = cargo > 8 ? 7 : 6;
  if (cargo == COLONIZE_CARGO_TRADE_GOODS) {
    base -= dos_rng_range(rng, 0, 7);
  }
  if (cargo == COLONIZE_CARGO_MUSKETS) {
    base -= (int)ind->muskets - 12;
  }
  if (cargo == COLONIZE_CARGO_HORSES) {
    base -= (int)ind->horse_herds - 10;
  }
  if (cargo == COLONIZE_CARGO_TOOLS) {
    base += 1;
  }
  int tier2 = ai_contact_2820_quartile(alarm) << 1;
  if (cargo == COLONIZE_CARGO_MUSKETS || cargo == COLONIZE_CARGO_HORSES) {
    tier2 = 0;
  }
  if (ask > 19) {
    tier2 >>= 1;
  }
  int raw = ((base - difficulty) - tier2 + r + 4) * 2 * ask;
  if (raw < 0) {
    raw = 0;
  }
  int price = (int)(((long)(r * 5 + raw) * (long)qty) / 100) / 2;
  if (price < 1) {
    price = 1;
  }
  const int iVar8 = (ask - tier2) + 4;
  int value_idx = iVar8 / 10;
  if (value_idx > 3) {
    value_idx = 3;
  }
  s->c4 = dos_rng_range(rng, 0, 1) + (iVar8 >> 2);
  s->fair = (ask + 1) * 4 + price;
  s->tier2 = tier2;
  s->value_idx = value_idx;
  s->price = price;
  return price;
}

/* @TRADE0 "fairer price" arm. Returns 1 = tribe raises its offer (*io_price, *io_fair
 * updated, c4 decremented), 0 = patience exhausted. Exposed for tests. */
int ai_contact_2820_sell_haggle(
  int difficulty, int ask, int qty, ColonizeDosRng* rng, int* io_c4, int* io_price, int* io_fair
) {
  if (*io_c4 > 0 && dos_rng_range(rng, 1, *io_c4 << 3) > difficulty) {
    (*io_c4)--;
    int inc = dos_rng_range(rng, (ask >> 1) + 1, ask * 2 + 1) * qty / 100;
    if (inc < 1) {
      inc = 1;
    }
    *io_price += inc;
    if (*io_fair <= *io_price) {
      *io_fair = *io_price + 10;
    }
    return 1;
  }
  return 0;
}

/* LAB_002bbc accept (iStack_5e == 1): slot out, gold in, tribe bookkeeping. */
static void ai_contact_2820_sell_settle(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, ColonizeCol1Tribe* t, int nation_id, int e,
  ColonizeUnit* unit, AiContact2820* s
) {
  const int cargo = s->cargo;
  const int qty = ai_contact_2820_remove_slot(unit, s->slot);
  s->qty = qty; /* DS:0x8dc4 */
  ctx->col1->nation[e].gold += (uint32_t)s->price;
  ind->tons[cargo & 15] = (int16_t)(ind->tons[cargo & 15] + qty);
  if (t) {
    t->sticky_trade_good = 0xff;
  }
  if (s->c4 > 0) {
    ai_diplo_indian_alarm_delta(ctx->col1, nation_id, e, -2 * s->c4);
    ai_contact_2820_friction_sub(t, e, qty, qty);
  }
  if (t) {
    t->last_bought = (cargo == COLONIZE_CARGO_MUSKETS || cargo == COLONIZE_CARGO_HORSES)
                       ? 0xffu
                       : (uint8_t)cargo;
  }
  if (cargo == COLONIZE_CARGO_MUSKETS) {
    if (qty > 0x18) {
      ind->muskets++;
    }
    if (qty > 0x31) {
      ind->muskets++;
    }
  }
  if (cargo == COLONIZE_CARGO_HORSES) {
    ind->horse_breeding = (uint16_t)(ind->horse_breeding + (qty >> 2));
    if (qty > 0x18) {
      ind->horse_herds++;
    }
    if (qty > 0x31) {
      ind->horse_herds++;
    }
  }
}

/* LAB_002bbc gift arm (iStack_5e == 3 && iStack_88 == 0). */
static void ai_contact_2820_gift_settle(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, ColonizeCol1Tribe* t, int nation_id, int e,
  ColonizeUnit* unit, AiContact2820* s
) {
  const int cargo = s->cargo;
  const int qty = ai_contact_2820_remove_slot(unit, s->slot);
  s->qty = qty;
  if (t) {
    t->sticky_trade_good = 0xff;
    t->last_bought = (cargo == COLONIZE_CARGO_MUSKETS || cargo == COLONIZE_CARGO_HORSES)
                       ? 0xffu
                       : (uint8_t)cargo;
  }
  if (s->c4 >= 0) {
    s->c4++;
    ai_diplo_indian_alarm_delta(ctx->col1, nation_id, e, -4 * s->c4);
    ai_contact_2820_friction_sub(t, e, 2 * qty, qty);
  }
  if (cargo == COLONIZE_CARGO_MUSKETS) {
    ind->muskets++;
  }
  if (cargo == COLONIZE_CARGO_HORSES) {
    ind->horse_breeding = (uint16_t)(ind->horse_breeding + (qty >> 2));
    ind->horse_herds++;
  }
}

/* The three highest-ask goods (acStack_7c after zeroing last_bought/last_sold). */
static void ai_contact_2820_wanted(const AiContact2820* s, const ColonizeCol1Tribe* t, int out[3]) {
  int16_t ask[16];
  memcpy(ask, s->ask, sizeof(ask));
  if (t && t->last_bought < 16) {
    ask[t->last_bought] = 0;
  }
  if (t && t->last_sold < 16) {
    ask[t->last_sold] = 0;
  }
  int order[16];
  ai_contact_2820_sort(ask, order);
  out[0] = order[15];
  out[1] = order[14];
  out[2] = order[13];
}

/* LAB_002e92 candidates: top of `cand` skipping muskets/food/tools/trade goods. */
static int ai_contact_2e92_candidates(const AiContact2820* s, int goods[3]) {
  int n = 0;
  for (int k = 15; k >= 0 && n < 3; --k) {
    const int c = s->cand[k];
    if (c == 15 || c == 0 || c == 14 || c == 13) {
      continue;
    }
    goods[n++] = c;
  }
  return n;
}

/* LAB_002e92 price (uStack_62). qty = DS:0x8dc4 (ships: >> 2). */
static int ai_contact_2e92_price(
  ColonizeTurnContext* ctx, const ColonizeCol1Indian* ind, int nation_id, int e, int cargo,
  int bid, int qty, ColonizeDosRng* rng
) {
  const int diff = (int)ctx->col1->head.difficulty;
  int price = 200;
  if (cargo > 7) {
    price = ((int)ind->tech - 8) * -0x32;
  }
  if (cargo > 6) {
    price += (int)k_2820_throttle[cargo & 15] * (diff * 2 + 15);
  }
  price += dos_rng_range(rng, 0, price);
  price += bid * -4;
  price += ai_diplo_indian_alarm(ctx->col1, nation_id, e) * 4;
  price = (int)(((long)qty * (long)price) / 100);
  price += (diff + dos_rng_range(rng, 0, 2)) * 10;
  if (price < 0x32) {
    price = 0x32;
  }
  return price;
}

/* LAB_002e92 accept (iStack_5e == 1): returns 1 on purchase, 0 when unaffordable. */
static int ai_contact_2e92_settle(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, ColonizeCol1Tribe* t, int nation_id, int e,
  ColonizeUnit* unit, int cargo, int price, int qty
) {
  ColonizeCol1Nation* nat = &ctx->col1->nation[e];
  if (nat->gold < (uint32_t)price) {
    return 0;
  }
  nat->gold -= (uint32_t)price;
  if (t) {
    t->last_sold = (uint8_t)(cargo == 9 ? 0xff : cargo); /* literal `if (cargo == 9) +9 = 0xff` */
  }
  ind->tons[cargo & 15] = (int16_t)(ind->tons[cargo & 15] - qty);
  ai_contact_2e92_give_goods(unit, cargo, qty);
  ai_diplo_indian_alarm_delta(ctx->col1, nation_id, e, price / 0x19 + 1);
  return 1;
}

static int ai_contact_2820_buy_qty(const ColonizeTurnContext* ctx, const ColonizeUnit* unit, int sold_qty) {
  int qty = sold_qty > 0 ? sold_qty : 100;
  if (unit && ctx && ctx->units && units_is_sea(ctx->units, unit->id)) {
    qty >>= 2; /* `if (0xc < type < 0x13) *0x8dc4 >>= 2` */
  }
  return qty;
}

static const char* ai_contact_2820_vehicle_name(const ColonizeTurnContext* ctx, const ColonizeUnit* unit) {
  /* DS:0x2e0c / 0x2e0e: runtime name pointers (land / ship). */
  if (unit && ctx && ctx->units && units_is_sea(ctx->units, unit->id)) {
    return "ship";
  }
  return "wagon train";
}

/*
 * @BUY0 Haggle arm (iStack_5e == 2): roll RNG(0, bid/25+8); if price < 11 or
 * roll <= difficulty+1 the tribe loses patience (returns 0: alarm +2,
 * sticky_trade_good = 0xfe, @BADHAGGLE2); else price -= price>>2 (floor 10),
 * a 1-in-(8-difficulty) roll adds alarm +1, and the offer is re-asked with
 * the @BUY1 "grow tired" text (returns 1, *io_price updated).
 */
int ai_contact_2e92_haggle(int difficulty, int bid, ColonizeDosRng* rng, int* io_price, int* out_alarm_delta) {
  int price = *io_price;
  const int roll = dos_rng_range(rng, 0, bid / 0x19 + 8);
  *out_alarm_delta = 0;
  if (price < 0xb || roll <= difficulty + 1) {
    *out_alarm_delta = 2;
    return 0;
  }
  int quarter = price >> 2;
  if (quarter < 1) {
    quarter = 1;
  }
  price -= quarter;
  if (price < 10) {
    price = 10;
  }
  if (dos_rng_range(rng, 1, 8 - difficulty) == 1) {
    *out_alarm_delta = 1;
  }
  *io_price = price;
  return 1;
}

static void ai_contact_enqueue_buy0(
  ColonizeTurnContext* ctx, int nation_id, int e, ColonizeUnit* unit, int cargo, int price, int qty,
  int round
) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_cargo_name(cargo);
  tok.string1 = ai_contact_2820_vehicle_name(ctx, unit);
  tok.number0 = price;
  tok.has_number0 = true;
  int fair = price >> 1;
  if (fair < 10) {
    fair = 10;
  }
  tok.number1 = fair;
  tok.has_number1 = true;
  tok.number2 = qty;
  tok.has_number2 = true;
  char tag[8];
  snprintf(tag, sizeof(tag), "BUY%d", round > 0 ? 1 : 0); /* FUN_0000_d9b4 + '0'+iStack_88 */
  char fb[AI_POPUP_BODY_LEN];
  snprintf(fb, sizeof(fb), "\"%sWe shall fill up your %s with %d %s in exchange for %d$. Is this acceptable?\"",
           round > 0 ? "We grow tired of your constant haggling. " : "", tok.string1, qty, tok.string0,
           price);
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, tag, &tok, fb, body, sizeof(body));
  char accept[AI_POPUP_CHOICE_LEN];
  char haggle[AI_POPUP_CHOICE_LEN];
  snprintf(accept, sizeof(accept), "We will gladly pay %d$ (of %u$)", price,
           (unsigned)ctx->col1->nation[e].gold);
  snprintf(haggle, sizeof(haggle), "A fairer price would be %d$", fair);
  const char* labels[3] = {accept, haggle, "Never mind"};
  const int ids[3] = {1, 2, 0};
  (void)ai_popup_enqueue_choice_ctx(
    ctx->ai_popups, AI_POPUP_TAG_CONTACT_BUY0, e, nation_id,
    (unit->id & 0xffff) | (cargo << 16) | ((price > 0x7ff ? 0x7ff : price) << 20) | (round > 0 ? (1 << 31) : 0),
    NULL, body, labels, ids, 3
  );
}

/* Human: @BUYWHICH (up to 3 goods). Returns 1 when a CHOICE was queued. */
static int ai_contact_enqueue_buywhich(
  ColonizeTurnContext* ctx, int nation_id, int e, ColonizeUnit* unit, const AiContact2820* s
) {
  if (!ctx || !ctx->ai_popups || !unit) {
    return 0;
  }
  int goods[3];
  const int n = ai_contact_2e92_candidates(s, goods);
  if (n <= 0) {
    return 0;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_cargo_name(goods[0]);
  tok.string1 = n > 1 ? ai_contact_cargo_name(goods[1]) : "";
  tok.string2 = n > 2 ? ai_contact_cargo_name(goods[2]) : "";
  char fb[AI_POPUP_BODY_LEN];
  snprintf(fb, sizeof(fb), "\"We have %s, %s, and %s available to trade with you. Which would you like to buy?\"",
           tok.string0, tok.string1, tok.string2);
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "BUYWHICH", &tok, fb, body, sizeof(body));
  const char* labels[3];
  int ids[3];
  for (int k = 0; k < n; ++k) {
    labels[k] = ai_contact_cargo_name(goods[k]);
    ids[k] = goods[k] + 1; /* 1..16; 0 = cancel */
  }
  return ai_popup_enqueue_choice_ctx(
    ctx->ai_popups, AI_POPUP_TAG_CONTACT_BUYWHICH, e, nation_id, unit->id, NULL, body, labels,
    ids, n
  );
}

/*
 * LAB_002e92 entry, after the sell loop (or straight from the shell when the
 * unit carries nothing). `human` = iStack_8.
 */
static void ai_contact_2820_buy_phase(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit,
  AiContact2820* s, int human
) {
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id);
  if (!unit || !ai_contact_2e92_unit_can_take(ctx, unit)) {
    s->sold_ok = 0;
  }
  if (!s->sold_ok) {
    s->active = 0;
    return;
  }
  int wanted[3];
  ai_contact_2820_wanted(s, t, wanted);
  if (wanted[0] != s->cargo && wanted[1] != s->cargo && human) {
    /* @BRING 0x1587 */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_cargo_name(wanted[0]);
    tok.string1 = ai_contact_cargo_name(wanted[1]);
    tok.string2 = ai_contact_cargo_name(wanted[2]);
    char fb[AI_POPUP_BODY_LEN];
    snprintf(fb, sizeof(fb),
             "\"We are in need of %s and %s. Perhaps you will bring some next time you come to trade with us. Even %s would be of some value.\"",
             tok.string0, tok.string1, tok.string2);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "BRING", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Trade", body);
  }
  if (human && t && t->sticky_trade_good == 0xfe) {
    s->active = 0;
    return; /* `tribe+7 == -2`: the buy loop is skipped silently */
  }
  if (s->cargo < 0) {
    s->active = 0;
    return; /* empty-handed: @BRING only, no purchase */
  }
  int goods[3];
  const int n = ai_contact_2e92_candidates(s, goods);
  if (n <= 0) {
    s->active = 0;
    return;
  }
  s->buy_qty = ai_contact_2820_buy_qty(ctx, unit, s->qty);
  if (human) {
    if (!ai_contact_enqueue_buywhich(ctx, nation_id, e, unit, s)) {
      s->active = 0;
    }
    return;
  }
  /* AI pick: max throttle byte among the three (first wins ties, DOS `<`). */
  int best = 0;
  int best_w = -1;
  for (int k = 0; k < n; ++k) {
    const int w = (int)k_2820_throttle[goods[k] & 15];
    if (w > best_w) {
      best_w = w;
      best = k;
    }
  }
  const int price = ai_contact_2e92_price(
    ctx, ind, nation_id, e, goods[best], (int)s->bid[goods[best]], s->buy_qty, &s->rng
  );
  if (!ai_contact_2e92_settle(ctx, ind, t, nation_id, e, unit, goods[best], price, s->buy_qty)) {
    ai_diplo_indian_alarm_delta(ctx->col1, nation_id, e, 1); /* @NOTENOUGH arm */
  }
  s->active = 0;
}

/*
 * AI-controlled empty-handed unit buys the tribe's own goods. DOS only reaches
 * LAB_002e92 after a completed sale; this entry (kept for unit tests / the
 * AI meet pulse) runs the same pick + price with qty 100 (ships 25).
 */
int ai_contact_auto_buy_2e92(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || !unit || e < 0 || e > 3) {
    return 0;
  }
  if (!ai_contact_2e92_unit_can_take(ctx, unit)) {
    return 0;
  }
  AiContact2820 s;
  memset(&s, 0, sizeof(s));
  if (!ai_contact_2820_prepare(ctx, nation_id, &s)) {
    return 0;
  }
  ai_contact_local_rng(ctx, nation_id, &s.rng);
  int goods[3];
  const int n = ai_contact_2e92_candidates(&s, goods);
  if (n <= 0) {
    return 0;
  }
  int best = 0;
  int best_w = -1;
  for (int k = 0; k < n; ++k) {
    const int w = (int)k_2820_throttle[goods[k] & 15];
    if (w > best_w) {
      best_w = w;
      best = k;
    }
  }
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id);
  const int qty = ai_contact_2820_buy_qty(ctx, unit, 0);
  const int price = ai_contact_2e92_price(ctx, ind, nation_id, e, goods[best], (int)s.bid[goods[best]], qty, &s.rng);
  return ai_contact_2e92_settle(ctx, ind, t, nation_id, e, unit, goods[best], price, qty);
}

static void ai_contact_apply_buywhich(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int unit_id, int cargo
) {
  if (!ctx || !ctx->ai_popups || !ctx->units || cargo < 0 || cargo > 15 || e < 0 || e > 3) {
    return;
  }
  ColonizeUnit* unit = units_get(ctx->units, unit_id);
  if (!unit) {
    return;
  }
  AiContact2820* s = &s_2820[e];
  if (!s->active || s->nation_id != nation_id) {
    /* Session lost (save/load mid-dialog): rebuild tables, qty 100 / 25. */
    memset(s, 0, sizeof(*s));
    if (!ai_contact_2820_prepare(ctx, nation_id, s)) {
      return;
    }
    ai_contact_local_rng(ctx, nation_id, &s->rng);
    s->active = 1;
    s->nation_id = nation_id;
    s->unit_id = unit_id;
    s->buy_qty = ai_contact_2820_buy_qty(ctx, unit, 0);
  }
  s->buy_cargo = cargo;
  s->round = 0;
  const int price = ai_contact_2e92_price(ctx, ind, nation_id, e, cargo, (int)s->bid[cargo], s->buy_qty, &s->rng);
  s->price = price;
  ai_contact_enqueue_buy0(ctx, nation_id, e, unit, cargo, price, s->buy_qty, 0);
}

static void ai_contact_apply_buy0(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int payload, int choice
) {
  if (!ctx || !ctx->units || e < 0 || e > 3) {
    return;
  }
  const int unit_id = payload & 0xffff;
  const int cargo = (payload >> 16) & 0xf;
  int price = (payload >> 20) & 0x7ff; /* 11 bits: 0..2047 */
  const int round = (payload >> 31) & 1;
  AiContact2820* s = &s_2820[e];
  const int have_state = s->active && s->nation_id == nation_id && s->buy_cargo == cargo;
  if (have_state) {
    price = s->price;
  }
  if (choice != 1 && choice != 2) {
    s->active = 0;
    return; /* Never mind */
  }
  ColonizeUnit* unit = units_get(ctx->units, unit_id);
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id);
  if (!unit) {
    s->active = 0;
    return;
  }
  const int qty = have_state ? s->buy_qty : ai_contact_2820_buy_qty(ctx, unit, 0);
  ColonizeDosRng local;
  ColonizeDosRng* rng = have_state ? &s->rng : &local;
  if (!have_state) {
    ai_contact_local_rng(ctx, nation_id, &local);
  }
  if (choice == 2) {
    const int bid = have_state ? (int)s->bid[cargo] : 0;
    int alarm_delta = 0;
    const int again = ai_contact_2e92_haggle((int)ctx->col1->head.difficulty, bid, rng, &price, &alarm_delta);
    if (alarm_delta) {
      ai_diplo_indian_alarm_delta(ctx->col1, nation_id, e, alarm_delta);
    }
    if (!again) {
      if (t) {
        t->sticky_trade_good = 0xfe; /* tribe+7 = 0xfe: refused outright */
      }
      s->active = 0;
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = ai_contact_cargo_name(cargo);
      char fb[AI_POPUP_BODY_LEN];
      snprintf(fb, sizeof(fb), "\"Our patience with your haggling is exhausted. We will sell you nothing further until you bring us something of value.\"");
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "BADHAGGLE2", &tok, fb, body, sizeof(body));
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "Trade", body);
      return;
    }
    s->price = price;
    s->round = round + 1;
    if (ctx->ai_popups) {
      ai_contact_enqueue_buy0(ctx, nation_id, e, unit, cargo, price, qty, round + 1);
    }
    return;
  }
  if (!ai_contact_2e92_settle(ctx, ind, t, nation_id, e, unit, cargo, price, qty)) {
    /* @NOTENOUGH 0x15ae; FUN_1000_8f5c(…, 1, 0). */
    ai_diplo_indian_alarm_delta(ctx->col1, nation_id, e, 1);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.number0 = (int)ctx->col1->nation[e].gold;
    tok.has_number0 = true;
    char fb[AI_POPUP_BODY_LEN];
    snprintf(fb, sizeof(fb), "\"Sadly, your treasury (%d$) is not large enough to back your promise.\"",
             tok.number0);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "NOTENOUGH", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "Trade", body);
  }
  s->active = 0;
}

/* @TRADE0 (round 0: accept / fairer / gift / never mind) or @TRADE1 (no gift). */
static int ai_contact_enqueue_trade_offer_round(
  ColonizeTurnContext* ctx, int nation_id, int e, const AiContact2820* s
) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_values_name(s->value_idx);
  tok.string1 = ai_contact_cargo_name(s->cargo);
  tok.number0 = s->price;
  tok.has_number0 = true;
  tok.number1 = s->fair;
  tok.has_number1 = true;
  char fb[AI_POPUP_BODY_LEN];
  if (s->round == 0) {
    snprintf(fb, sizeof(fb), "\"We see that you have brought some %s %s to trade with us. We offer you %d$ in exchange.\"",
             tok.string0, tok.string1, s->price);
  } else {
    snprintf(fb, sizeof(fb), "\"Your haggling is trying our patience, but we shall raise our offer to %d$ for your %s.\"",
             s->price, tok.string1);
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, s->round == 0 ? "TRADE0" : "TRADE1", &tok, fb, body, sizeof(body));
  char accept[AI_POPUP_CHOICE_LEN];
  char haggle[AI_POPUP_CHOICE_LEN];
  char gift[AI_POPUP_CHOICE_LEN];
  snprintf(accept, sizeof(accept), "We gratefully accept %d$", s->price);
  snprintf(haggle, sizeof(haggle), "A fairer price would be %d$", s->fair);
  snprintf(gift, sizeof(gift), "No, let the %s be our gift to you", tok.string1);
  const char* labels[4] = {accept, haggle, gift, "Never mind"};
  const int ids4[4] = {AI_CONTACT_TRADE_OFFER_ACCEPT, AI_CONTACT_TRADE_OFFER_HAGGLE,
                       AI_CONTACT_TRADE_OFFER_GIFT, AI_CONTACT_TRADE_OFFER_DECLINE};
  const char* labels3[3] = {accept, haggle, "Never mind"};
  const int ids3[3] = {AI_CONTACT_TRADE_OFFER_ACCEPT, AI_CONTACT_TRADE_OFFER_HAGGLE,
                       AI_CONTACT_TRADE_OFFER_DECLINE};
  const int four = s->round == 0;
  return ai_popup_enqueue_choice_ctx(
           ctx->ai_popups, AI_POPUP_TAG_CONTACT_TRADE_OFFER, e, nation_id, s->price, NULL,
           body, four ? labels : labels3, four ? ids4 : ids3, four ? 4 : 3
         )
           ? 1
           : 0;
}

/*
 * Post-pick dispatch (shell lines ~256-283 / 450-452 / 656-675):
 *   cargo < 0            → LAB_002e92 (@BRING for humans, nothing else)
 *   AI                   → LAB_002bbc: alarm > 0x31 ? gift : accept; then buy
 *   human, refused good  → @BADCARGO / @BADHAGGLE1
 *   human                → @TRADE0 CHOICE
 */
static void ai_contact_2820_dispatch(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit,
  AiContact2820* s
) {
  const int human = ai_contact_euro_is_human(ctx, e);
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id);
  const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e); /* aiStack_d6[0] */
  if (s->cargo < 0) {
    ai_contact_2820_buy_phase(ctx, ind, nation_id, e, unit, s, human);
    return;
  }
  if (!human) {
    ai_contact_2820_sell_price(
      ind, s->cargo, (int)s->ask[s->cargo], s->qty, (int)ctx->col1->head.difficulty, alarm, s
    );
    if (alarm > 0x31) {
      ai_contact_2820_gift_settle(ctx, ind, t, nation_id, e, unit, s);
    } else {
      ai_contact_2820_sell_settle(ctx, ind, t, nation_id, e, unit, s);
    }
    ai_contact_2820_buy_phase(ctx, ind, nation_id, e, unit, s, 0);
    return;
  }
  const int lb = t ? (int)t->last_bought : 0xff;
  const int ls = t ? (int)t->last_sold : 0xff;
  if (lb == s->cargo || ls == s->cargo || s->ask[s->cargo] == 0) {
    /* @BADCARGO 0x1561: STRING0 = cargo, STRING1..3 = the three highest asks. */
    int wanted[3];
    ai_contact_2820_wanted(s, t, wanted);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_cargo_name(s->cargo);
    tok.string1 = ai_contact_cargo_name(wanted[0]);
    tok.string2 = ai_contact_cargo_name(wanted[1]);
    tok.string3 = ai_contact_cargo_name(wanted[2]);
    char fb[AI_POPUP_BODY_LEN];
    snprintf(fb, sizeof(fb),
             "\"We have enough %s and don't need any more right now. Come back when you have something else to offer. We are in need of %s and %s. Even %s would be of some use.\"",
             tok.string0, tok.string1, tok.string2, tok.string3);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "BADCARGO", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "Trade", body);
    s->active = 0;
    return;
  }
  if (t && (int)t->sticky_trade_good == s->cargo) {
    /* @BADHAGGLE1 0x156a */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_cargo_name(s->cargo);
    char fb[AI_POPUP_BODY_LEN];
    snprintf(fb, sizeof(fb), "\"We have already told you that we no longer want your %s. Come back when you have something else.\"",
             tok.string0);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "BADHAGGLE1", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "Trade", body);
    s->active = 0;
    return;
  }
  ai_contact_2820_sell_price(
    ind, s->cargo, (int)s->ask[s->cargo], s->qty, (int)ctx->col1->head.difficulty, alarm, s
  );
  s->round = 0;
  if (!ctx->ai_popups || !ai_contact_enqueue_trade_offer_round(ctx, nation_id, e, s)) {
    s->active = 0;
  }
}

/*
 * Shell entry: tables + hold pick. Returns 1 when the trade ran or a CHOICE
 * was queued, 0 when nothing could be priced (no tribe / econ).
 */
static int ai_contact_2820_begin_slot(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit,
  int pick_slot
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || !unit || e < 0 || e > 3 ||
      unit->nation_id != e) {
    return 0;
  }
  AiContact2820* s = &s_2820[e];
  memset(s, 0, sizeof(*s));
  if (!ai_contact_2820_prepare(ctx, nation_id, s)) {
    return 0;
  }
  ai_contact_local_rng(ctx, nation_id, &s->rng);
  s->active = 1;
  s->nation_id = nation_id;
  s->unit_id = unit->id;
  s->cargo = -1;
  s->slot = 0;
  s->sold_ok = 1;
  const int holds = ai_contact_2820_holds_used(unit);
  const int human = ai_contact_euro_is_human(ctx, e);
  if (pick_slot >= 0 && pick_slot < COLONIZE_UNIT_CARGO_MAX && unit->hold_goods_amount[pick_slot] > 0) {
    s->slot = pick_slot;
    s->cargo = unit->hold_goods_type[pick_slot];
    s->qty = unit->hold_goods_amount[pick_slot];
    ai_contact_2820_dispatch(ctx, ind, nation_id, e, unit, s);
    return 1;
  }
  if (holds > 1) {
    if (!human) {
      s->slot = dos_rng_range(&s->rng, 0, holds - 1);
    } else if (ctx->ai_popups) {
      /* DOS: one menu row per hold ("<qty> <name>"), 99 = cancel. */
      const char* labels[AI_POPUP_CHOICE_MAX];
      char text[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
      int ids[AI_POPUP_CHOICE_MAX];
      int n = 0;
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX && n < AI_POPUP_CHOICE_MAX - 1; ++h) {
        if (unit->hold_goods_amount[h] <= 0) {
          continue;
        }
        snprintf(text[n], sizeof(text[n]), "%d %s", unit->hold_goods_amount[h],
                 ai_contact_cargo_name(unit->hold_goods_type[h]));
        labels[n] = text[n];
        ids[n] = h + 1;
        n++;
      }
      labels[n] = "Cancel";
      ids[n] = 99;
      n++;
      char body[AI_POPUP_BODY_LEN];
      snprintf(body, sizeof(body), "Which cargo will you offer the %s?", ai_contact_tribe_name(nation_id));
      if (ai_popup_enqueue_choice_ctx(
            ctx->ai_popups, AI_POPUP_TAG_CONTACT_TRADE_PICK, e, nation_id, unit->id, NULL, body,
            labels, ids, n
          )) {
        return 1;
      }
    }
  }
  if (holds > 0) {
    /* Single hold (or AI pick): iStack_7e = first used slot / RNG slot. */
    int seen = 0;
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (unit->hold_goods_amount[h] <= 0) {
        continue;
      }
      if (seen == s->slot) {
        s->slot = h;
        s->cargo = unit->hold_goods_type[h];
        s->qty = unit->hold_goods_amount[h];
        break;
      }
      seen++;
    }
  }
  ai_contact_2820_dispatch(ctx, ind, nation_id, e, unit, s);
  return 1;
}

static int ai_contact_2820_begin(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit
) {
  return ai_contact_2820_begin_slot(ctx, ind, nation_id, e, unit, -1);
}

/* Hold-pick CHOICE result (human, multi-hold unit). */
static void ai_contact_apply_trade_pick(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int unit_id, int choice
) {
  if (!ctx || !ctx->units || e < 0 || e > 3) {
    return;
  }
  AiContact2820* s = &s_2820[e];
  if (!s->active || s->nation_id != nation_id || s->unit_id != unit_id) {
    return;
  }
  ColonizeUnit* unit = units_get(ctx->units, unit_id);
  if (!unit || choice < 1 || choice > COLONIZE_UNIT_CARGO_MAX) {
    s->active = 0; /* 99 / cancel → LAB_003582 */
    return;
  }
  const int h = choice - 1;
  if (unit->hold_goods_amount[h] <= 0) {
    s->active = 0;
    return;
  }
  s->slot = h;
  s->cargo = unit->hold_goods_type[h];
  s->qty = unit->hold_goods_amount[h];
  ai_contact_2820_dispatch(ctx, ind, nation_id, e, unit, s);
}

/*
 * AI-silent path (Brave-adjacency meet pulse — a Linux stand-in; DOS's own
 * 2820 callers are the village-enter arms). Scope kept from the earlier
 * stand-in so the pulse does not strip every AI cargo unit each turn: sells
 * only a TRADE_GOODS hold (full LAB_002bbc mechanics, whole hold, then the
 * LAB_002e92 buy), or buys when empty-handed. `forced_price` is ignored.
 * Returns 1 when a sale, gift or purchase happened.
 */
static int ai_contact_auto_trade(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit,
  int forced_price
) {
  (void)forced_price;
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || !unit || unit->nation_id != e) {
    return 0;
  }
  if (!ind->euro_diplo[e]) {
    return 0;
  }
  if (ai_contact_euro_is_human(ctx, e)) {
    return ai_contact_2820_begin(ctx, ind, nation_id, e, unit);
  }
  int slot = -1;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (unit->hold_goods_type[h] == COLONIZE_CARGO_TRADE_GOODS && unit->hold_goods_amount[h] > 0) {
      slot = h;
      break;
    }
  }
  if (slot < 0) {
    if (ai_contact_2820_holds_used(unit) > 0) {
      return 0;
    }
    return ai_contact_auto_buy_2e92(ctx, ind, nation_id, e, unit);
  }
  return ai_contact_2820_begin_slot(ctx, ind, nation_id, e, unit, slot);
}

/*
 * @TRADE0/@TRADE1 CHOICE result (LAB_002bbc human loop, iStack_5e):
 *   1 accept → sell_settle, then LAB_002e92
 *   2 fairer → haggle; raise → @TRADE1 again; exhausted → @BADHAGGLE0, no buy
 *   3 gift (round 0) → gift_settle, then LAB_002e92
 *   else → iStack_c6 = 0 (nothing)
 */
static void ai_contact_apply_trade_offer(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int price, int choice
) {
  (void)price;
  if (!ctx || !ind || e < 0 || e > 3 || !ctx->units) {
    return;
  }
  AiContact2820* s = &s_2820[e];
  if (!s->active || s->nation_id != nation_id || s->cargo < 0) {
    return;
  }
  ColonizeUnit* unit = units_get(ctx->units, s->unit_id);
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id);
  if (!unit || unit->hold_goods_amount[s->slot] <= 0 || unit->hold_goods_type[s->slot] != s->cargo) {
    s->active = 0;
    return;
  }
  if (choice == AI_CONTACT_TRADE_OFFER_HAGGLE) {
    if (ai_contact_2820_sell_haggle(
          (int)ctx->col1->head.difficulty, (int)s->ask[s->cargo], s->qty, &s->rng, &s->c4,
          &s->price, &s->fair
        )) {
      s->round++;
      (void)ai_contact_enqueue_trade_offer_round(ctx, nation_id, e, s);
      return;
    }
    /* Patience gone: tribe+7 = cargo, alarm += tier2/2 + 1, @BADHAGGLE0. */
    if (t) {
      t->sticky_trade_good = (uint8_t)s->cargo;
    }
    ai_diplo_indian_alarm_delta(ctx->col1, nation_id, e, (s->tier2 >> 1) + 1);
    s->active = 0;
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string1 = ai_contact_cargo_name(s->cargo);
    char fb[AI_POPUP_BODY_LEN];
    snprintf(fb, sizeof(fb), "\"Our patience with your haggling is exhausted. We no longer want your worthless %s. Come back when you have something else to offer.\"",
             tok.string1);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "BADHAGGLE0", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "Trade", body);
    return;
  }
  if (choice == AI_CONTACT_TRADE_OFFER_GIFT && s->round == 0) {
    ai_contact_2820_gift_settle(ctx, ind, t, nation_id, e, unit, s);
    ai_contact_2820_buy_phase(ctx, ind, nation_id, e, unit, s, 1);
    return;
  }
  if (choice != AI_CONTACT_TRADE_OFFER_ACCEPT) {
    s->active = 0;
    return;
  }
  ai_contact_2820_sell_settle(ctx, ind, t, nation_id, e, unit, s);
  ai_contact_2820_buy_phase(ctx, ind, nation_id, e, unit, s, 1);
}

/* Find Euro unit of nation e adjacent to a Brave of nation_id (meet/gift apply). */
static ColonizeUnit* ai_contact_find_adjacent_euro(
  ColonizeTurnContext* ctx,
  int nation_id,
  int e,
  int* near_x,
  int* near_y
) {
  if (!ctx || !ctx->units || e < 0 || e > 3) {
    return NULL;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int oid = units_id_at(ctx->units, brave->x + dx[d], brave->y + dy[d]);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id != e) {
        continue;
      }
      if (near_x) {
        *near_x = brave->x;
      }
      if (near_y) {
        *near_y = brave->y;
      }
      return other;
    }
  }
  return NULL;
}

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
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int d = 0; d < 8; ++d) {
      const int nx = brave->x + dx[d];
      const int ny = brave->y + dy[d];
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
      if (!ind->euro_diplo[e]) {
        (void)ai_contact_try_first_welcome(ctx, e, nation_id);
        if (ctx->col1->tribe) {
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id != nation_id ||
                ai_contact_dist(t->x, t->y, brave->x, brave->y) > 3) {
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
       * 3. Peaceful auto-trade (nested 2bbc AI buy stand-in).
       * PARK deep FUN_4d56_2820 (~1.4k; thunk 2a1f_044c) — thin path only.
       */
      ai_contact_auto_trade(ctx, ind, nation_id, e, other, -1);

      /* 4. Gift / demand stand-in (5bfb_102a / 1092; AI silent). */
      ai_contact_gift_or_demand(ctx, ind, nation_id, e, other, brave->x, brave->y);
    }
  }

  /* 2b. AI missionary adjacent to tribe → mission owner + crosses. */
  ai_contact_missionary_convert(ctx, nation_id);

  /* 2c. Brave from a mission settlement visits that nation's colony →
   * @INDIANSCONVERT + an Indian Convert in the colony (FUN_5bfb_022e). */
  ai_contact_mission_convert_visit(ctx, nation_id);

  /* 2b1. Alarmed tribe + Missionary not converting → flee 1 tile (AI_MOVE). */
  ai_contact_missionary_flee(ctx, nation_id);

  /*
   * 2b2. Mission pacify deepen (meet pulse): mid-range alarm/friction toward
   * mission Euro → −2 once (prelude keeps low-band −1). Cite: fandom Alarm —
   * missions slow hostility. No free crosses. Burn/clear at ≥80 stays in prelude.
   */
  ai_contact_mission_pacify_meet(ctx, nation_id);

  /* 2c. Peaceful Free Colonist/Scout at tribe → state.learned + optional skill. */
  ai_contact_teach_skill(ctx, nation_id);
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
static int ai_contact_colony_has_stores(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  for (int cargo = 0; cargo < COLONIZE_CARGO_COUNT; ++cargo) {
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
    if (!bt || strcmp(bt->name, "Town Hall") == 0) {
      continue;
    }
    return 1;
  }
  return 0;
}

/* Non-lumber lootable warehouse cargo (STORES still preferred over BURN). */
static int ai_contact_colony_has_non_lumber_stores(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  for (int cargo = 0; cargo < COLONIZE_CARGO_COUNT; ++cargo) {
    if (cargo == COLONIZE_CARGO_LUMBER) {
      continue;
    }
    if (ai_contact_stores_cargo_value(cargo) > 0 && c->stock[cargo] > 0) {
      return 1;
    }
  }
  return 0;
}

static AiRaidKind ai_contact_pick_raid_kind(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int target_euro,
  int max_alarm,
  ColonizeDosRng* rng
) {
  /*
   * Banded picker mirroring @RAID* message outcomes (not DOS bit-identity).
   * Gate kinds on colony stock / gold actually present so empty warehouses
   * do not fake STORES/WREAK/muskets loot (5fef_0f14-shaped). No Indian-nation
   * treasury fiction — GOLD drains Euro gold only when present.
   */
  if (max_alarm < 45) {
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
  if (c && ctx && ctx->colonies && rng) {
    int walls = 0;
    static const char* k_chain[3] = {"Stockade", "Fort", "Fortress"};
    for (int i = 0; i < 3; ++i) {
      const int b = colonies_find_building(ctx->colonies, k_chain[i]);
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
      ctx->col1->nation[target_euro].gold > 0) {
    return AI_RAID_GOLD;
  }
  if (max_alarm >= 50 && roll < 12 && c && ctx && ctx->map) {
    /* Harbor: prefer if water adjacent. */
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int d = 0; d < 8; ++d) {
      if (map_tile_is_water(ctx->map, c->x + dx[d], c->y + dy[d])) {
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
  if (ai_contact_colony_has_stores(c)) {
    const int prefer_burn =
      max_alarm >= 60 &&
      ai_contact_colony_has_burn_target(ctx ? ctx->colonies : NULL, c) &&
      !ai_contact_colony_has_non_lumber_stores(c);
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
static AiRaidKind ai_contact_raid_kind_demote(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  AiRaidKind kind
) {
  if (kind == AI_RAID_NOTHING || kind == AI_RAID_STORES) {
    return kind;
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
  if (ai_contact_colony_has_stores(c)) {
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
static void ai_contact_raid_secondary_loot(
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
 * Raid gate Euro: highest friction among met candidates (≥40; Spain ≥35
 * conquest bias), prefer at-war, tie-break lower relation. Cite:
 * indian_raid_outcomes.md §1 gate; docs/fandom_col1994.md nation bias.
 */
static int ai_contact_raid_gate_target(
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
    if (alarm < 40) {
      /* Spain (2): fandom conquest bias — slightly earlier raid gate (35). */
      if (!(e == 2 && alarm >= 35)) {
        continue;
      }
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
  if (!ctx || !ctx->colonies || euro < 0 || euro > 3) {
    return 99;
  }
  int best = 99;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id != euro) {
      continue;
    }
    const int d = ai_contact_dist(x, y, c->x, c->y);
    if (d < best) {
      best = d;
    }
  }
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
 * colony; weight 2× at ≥55, 3× at ≥80. Deep dir picker inside 14fe still PARKED.
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

static void ai_contact_apply_raid_loot(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int target_euro,
  AiRaidKind kind,
  int max_alarm
) {
  if (!c) {
    return;
  }
  s_last_raid_kind = (int)kind;
  s_last_burn_building[0] = '\0';
  s_last_stores_cargo[0] = '\0';
  s_last_ship_type[0] = '\0';
  s_last_gold_drained = 0;

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
      snprintf(s_last_stores_cargo, sizeof(s_last_stores_cargo), "%s", ai_contact_cargo_name(cargo));
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
        if (!bt || strcmp(bt->name, "Town Hall") == 0) {
          continue;
        }
        burn_bt = bi;
        break;
      }
      if (burn_bt >= 0) {
        const ColonizeBuildingType* bbt = colonies_building_type(ctx->colonies, burn_bt);
        if (colonies_destroy_building(ctx->colonies, c->id, burn_bt) && bbt &&
            bbt->name[0]) {
          snprintf(s_last_burn_building, sizeof(s_last_burn_building), "%s", bbt->name);
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
      ColonizeCol1Nation* nat = &ctx->col1->nation[target_euro];
      if (nat->gold > 0) {
        unsigned drain = 32u + (unsigned)(c->population > 0 ? c->population * 8 : 8);
        if (drain < 50u) {
          drain = 50u;
        }
        if (drain > 500u) {
          drain = 500u;
        }
        if (drain > nat->gold) {
          drain = nat->gold;
        }
        nat->gold -= (uint16_t)drain;
        s_last_gold_drained = (int)drain;
      }
    }
    break;
  case AI_RAID_SHIP:
    if (ctx && ctx->units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != target_euro) {
          continue;
        }
        if (!units_is_sea(ctx->units, u->id)) {
          continue;
        }
        if (ai_contact_dist(u->x, u->y, c->x, c->y) > 2) {
          continue;
        }
        snprintf(s_last_ship_type, sizeof(s_last_ship_type), "%s", units_display_name(ctx->units, u));
        if (u->moves_left > 0) {
          u->moves_left = 0;
        }
        /* Thin harbor damage: dump one hold cargo ton if present. */
        for (int h = 0; h < 6; ++h) {
          if (u->hold_goods_amount[h] > 0) {
            u->hold_goods_amount[h]--;
            if (u->hold_goods_amount[h] == 0) {
              u->hold_goods_type[h] = 0;
            }
            break;
          }
        }
        break;
      }
    }
    break;
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
 * FUN_4d56_359c thin displace: nudge Scout onto free land 1–2 tiles from
 * current tile, preferring greater Chebyshev distance from (away_x,away_y)
 * (Brave / tribe contact). Sets AI_MOVE goto at the flee tile. Returns 1 if
 * moved, 0 if no free land tile (caller may despawn).
 */
static int ai_contact_displace_scout(
  ColonizeTurnContext* ctx,
  ColonizeUnit* scout,
  int away_x,
  int away_y
) {
  if (!ctx || !ctx->units || !ctx->map || !scout || !scout->active) {
    return 0;
  }
  const int ox = scout->x;
  const int oy = scout->y;
  const int dist0 = ai_contact_dist(ox, oy, away_x, away_y);
  int best_x = -1;
  int best_y = -1;
  int best_score = -1;
  int fallback_x = -1;
  int fallback_y = -1;
  int fallback_score = -1;

  for (int dy = -2; dy <= 2; ++dy) {
    for (int dx = -2; dx <= 2; ++dx) {
      const int adx = dx < 0 ? -dx : dx;
      const int ady = dy < 0 ? -dy : dy;
      const int cheb = adx > ady ? adx : ady;
      if (cheb < 1 || cheb > 2) {
        continue;
      }
      const int nx = ox + dx;
      const int ny = oy + dy;
      if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
        continue;
      }
      if (!map_tile_is_land(ctx->map, nx, ny)) {
        continue;
      }
      if (units_id_at(ctx->units, nx, ny) >= 0) {
        continue;
      }
      if (!units_can_enter(
            ctx->units, scout->type_index, ctx->map, nx, ny, scout->id, ctx->colonies
          )) {
        continue;
      }
      const int d = ai_contact_dist(nx, ny, away_x, away_y);
      const int score = d * 10 + cheb;
      if (score > fallback_score) {
        fallback_score = score;
        fallback_x = nx;
        fallback_y = ny;
      }
      if (d < dist0) {
        continue;
      }
      if (score > best_score) {
        best_score = score;
        best_x = nx;
        best_y = ny;
      }
    }
  }

  if (best_x < 0) {
    best_x = fallback_x;
    best_y = fallback_y;
  }
  if (best_x < 0) {
    return 0;
  }
  {
    const int mv_ox = scout->x;
    const int mv_oy = scout->y;
    scout->x = best_x;
    scout->y = best_y;
    units_occupancy_notify_moved(ctx->units, mv_ox, mv_oy, best_x, best_y);
  }
  scout->orders = UNITS_ORDER_AI_MOVE;
  scout->goto_x = best_x;
  scout->goto_y = best_y;
  return 1;
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
   *  5 capture → 6 scout 359c displace/despawn.
   *
   * PARK deep FUN_4d56_2820 (~1.4k; thunk 2a1f_044c): meet/raid decision
   * matrix that DOS reaches before settlement enter — nested 2aac…311e trade
   * helpers (dispatch / AI buy / hard-bargain / demand) and alarmed act pick
   * live there, not in this post-pulse path. Do not port 2820 body here.
   * Full FUN_4d56_4528 (~3k) settlement body also PARKED (comment only).
   * Linux stays on thin @RAID* / combat / 359c + equal-dist mil/tools/silver
   * approach. Widgets Done structural (ai_popup); VGA PARKED. Mid-friction prefers non-mission
   * villages (below). Cite: indian_raid_outcomes.md §10; indian_contact.md
   * PORT DEBT; docs/ai_transcription.md FUN_4d56_2820; Marathon2 R6 PARK.
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id || brave->moves_left <= 0) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
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
     * peel is the alarmed branch (deep dir picker inside 14fe still PARKED).
     * Quiet seed-100 pulse unchanged. Cite: units_follow_unit;
     * indian_raid_outcomes.md §1.
     */
    if (brave->orders == UNITS_ORDER_NONE && brave->moves_left > 0) {
      const int lead =
        ai_contact_escort_pick_lead(ctx, ind, nation_id, brave->id, brave);
      if (lead >= 0 && units_follow_unit(ctx->units, brave->id, lead)) {
        (void)units_advance_follow_one_step(
          ctx->units, brave->id, ctx->map, ctx->colonies, ctx->rng
        );
        continue;
      }
    }
    if (brave->orders == UNITS_ORDER_FOLLOW) {
      (void)units_advance_follow_one_step(
        ctx->units, brave->id, ctx->map, ctx->colonies, ctx->rng
      );
      continue;
    }
    int target_euro = -1;
    int max_alarm = 0;
    if (!ai_contact_raid_gate_target(ctx, ind, nation_id, &target_euro, &max_alarm)) {
      continue;
    }

    /* 2. Adjacent unit combat. */
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    int attacked = 0;
    for (int d = 0; d < 8 && !attacked; ++d) {
      const int nx = brave->x + dx[d];
      const int ny = brave->y + dy[d];
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
       * escalation uses) or at open war, not at the ≥40 raid-gate band. A
       * Treasure Train is the exception: hard to resist at any alarm.
       */
      {
        const ColonizeUnitType* ft2 = units_type(ctx->units, f->type_index);
        const int is_treasure2 =
          ft2 && ft2->name[0] && strstr(ft2->name, "Treasure") != NULL;
        if (!is_treasure2 && max_alarm < 55 &&
            !ai_diplo_indian_at_war(ctx->col1, target_euro, nation_id - 4)) {
          continue;
        }
      }
      /* Snapshot before combat despawn (GAME.TXT @INDIANWIN1/@INDIANWIN2). */
      const int foe_muskets = f->muskets;
      const int foe_horses = f->horses;
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
      const char* place = "Wilderness";
      if (ctx->colonies) {
        int best_d = 99;
        for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
          const ColonizeColony* c = &ctx->colonies->colonies[ci];
          if (!c->active || c->nation_id != target_euro || !c->name[0]) {
            continue;
          }
          const int d = ai_contact_dist(foe_x, foe_y, c->x, c->y);
          if (d < best_d) {
            best_d = d;
            place = c->name;
          }
        }
      }
      const int brave_won =
        units_resolve_land_combat(ctx->units, brave->id, foe, rng) ? 1 : 0;
      int seized_muskets = 0;
      int seized_horses = 0;
      if (brave_won) {
        ColonizeUnit* br = units_get(ctx->units, brave->id);
        if (br && br->active) {
          if (foe_muskets > 0) {
            br->muskets += foe_muskets;
            seized_muskets = 1;
          } else if (foe_horses > 0) {
            br->horses += foe_horses;
            seized_horses = 1;
          }
        }
        units_try_move(ctx->units, brave->id, ctx->map, nx, ny, ctx->colonies, rng);
      }
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
            snprintf(
              fb,
              sizeof(fb),
              "%s ambush %s %s near %s! Muskets seized by %s braves!",
              tribe,
              foe_nation_label,
              foe_unit_name,
              place,
              tribe
            );
          } else if (seized_horses) {
            sec = "INDIANWIN2";
            snprintf(
              fb,
              sizeof(fb),
              "%s ambush %s %s near %s! Horses seized by %s braves!",
              tribe,
              foe_nation_label,
              foe_unit_name,
              place,
              tribe
            );
          } else {
            sec = "INDIANWIN0";
            snprintf(
              fb,
              sizeof(fb),
              "%s ambush %s %s near %s!",
              tribe,
              foe_nation_label,
              foe_unit_name,
              place
            );
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
          "Ambush",
          ambush_body
        );
      }
      {
        /* Pocahontas: half-rate alarm growth (wiki/fandom). */
        const int bump = ai_contact_alarm_bump_amount(ctx->col1, target_euro, 2);
        if (bump > 0) {
          ind->alarm_by_player[target_euro] =
            (uint16_t)(ind->alarm_by_player[target_euro] + (uint16_t)bump);
        }
      }
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id == nation_id) {
          t->alarm[target_euro].attacks++;
        }
      }
      attacked = 1;
    }

    /* 3–5. Colony approach / loot / capture. */
    if (!attacked && ctx->colonies && brave->active) {
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
        const int d = ai_contact_dist(brave->x, brave->y, c->x, c->y);
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
      if (best_cid >= 0) {
        ColonizeColony* c = colonies_get_mut(ctx->colonies, best_cid);
        if (!c) {
          continue;
        }
        if (brave->x == c->x && brave->y == c->y) {
          const AiRaidKind kind = ai_contact_raid_kind_demote(
            ctx, c, ai_contact_pick_raid_kind(ctx, c, target_euro, max_alarm, rng)
          );
          ai_contact_apply_raid_loot(ctx, c, target_euro, kind, max_alarm);
          /*
           * DS:0x54f6 grudge/tension discharge: FUN_5fef_0f14's tail
           * (viceroy_unpacked.c:100034) unconditionally clears
           * `(origin*9 + euro)*2 + 0x54f6` to 0 right before returning, for
           * EVERY kind including "Nothing" (raiding party wiped out) — the
           * act of raiding itself discharges accumulated tension, win or
           * lose. `origin` is the raiding unit's home-tribe id (unit+6,
           * `home_tribe_id` here — same index space `indian_tension` is
           * already keyed by, see col1_save.h). Read side (FUN_521d_0896
           * hostility gate) stays out of domain; this only wires the write.
           */
          if (
            ctx->col1->indian_tension && brave->home_tribe_id >= 0 &&
            (uint16_t)brave->home_tribe_id < ctx->col1->head.tribe_count
          ) {
            ctx->col1->indian_tension
              [(size_t)brave->home_tribe_id * COLONIZE_COL1_NATION_COUNT + (size_t)target_euro] =
              0;
          }
          int abandoned = 0;
          char abandoned_name[40];
          abandoned_name[0] = '\0';
          if (c->population <= 1 && max_alarm >= 70) {
            if (c->name[0]) {
              snprintf(abandoned_name, sizeof(abandoned_name), "%s", c->name);
            }
            colonies_capture(ctx->colonies, best_cid, nation_id);
            abandoned = 1;
          }
          /* bugs.md: only the FIRST attack against this Euro is "deniable" —
           * snapshot before the counters bump. */
          int prior_attacks = 0;
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id == nation_id) {
              prior_attacks += (int)t->alarm[target_euro].attacks;
              t->alarm[target_euro].attacks++;
            }
          }
          /*
           * Successful raid friction/alarm escalate (fandom Alarm — raids raise
           * tension). Thin 5fef_0f14 / 0d6c-shaped kind deltas (stores −4 → +4,
           * burn −12 → +12, scalp −16 → +16, gold −8 → +8). Pocahontas / France
           * half via alarm_bump_amount. Cite: indian_raid_loot.md; Series J.
           * Full 4528/2820 dialog PARKED; thin widgets Done (ai_popup).
           */
          if (kind != AI_RAID_NOTHING) {
            int kind_delta = 4; /* STORES / default goods */
            if (kind == AI_RAID_BURN || kind == AI_RAID_WREAK) {
              kind_delta = 12;
            } else if (kind == AI_RAID_SCALP) {
              kind_delta = 16;
            } else if (kind == AI_RAID_GOLD || kind == AI_RAID_SHIP) {
              kind_delta = 8;
            } else if (kind == AI_RAID_STORES) {
              kind_delta = 4;
            }
            const int fr_bump =
              ai_contact_alarm_bump_amount(ctx->col1, target_euro, kind_delta);
            if (fr_bump > 0) {
              ai_contact_bump_u16_cap100(&ind->alarm_by_player[target_euro], fr_bump);
              for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
                ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
                if ((int)t->nation_id == nation_id) {
                  ai_contact_bump_u8_cap100(&t->alarm[target_euro].friction, fr_bump);
                }
              }
            }
          }
          /*
           * High-friction successful raid → escalate Indian×Euro hostility
           * (4cc6_00f2 via ai_diplo). If treaty/peace still held → clear peace
           * bit (@INDIANWAR). Full 4528/2820 dialog PARKED.
           */
          const int had_peace =
            ai_contact_indian_has_peace(ctx->col1, nation_id, target_euro);
          const int was_at_war =
            ai_diplo_indian_at_war(ctx->col1, target_euro, nation_id - 4);
          if (kind != AI_RAID_NOTHING && max_alarm >= 55) {
            /* Hostility already lands on alarm_by_player via the kind bump above
             * (single store since 2026-08-27); the former extra −3/−5 relation
             * push would double-count. */
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
            if (abandoned && abandoned_name[0]) {
              if (kind == AI_RAID_SCALP || kind == AI_RAID_BURN) {
                (void)woodcut_fire(ctx->col1, WOODCUT_COLONY_BURNING);
                units_combat_notify_colony_burned(
                  ctx->col1, abandoned_name, target_euro, tribe
                );
                raid_body = NULL; /* @BURNED covers human chrome */
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s overrun %s!",
                  tribe,
                  abandoned_name
                );
                raid_body = raid_line;
              }
            } else if (kind == AI_RAID_NOTHING) {
              /* FUN_5fef_0f14 5fef:1299: a wiped-out raid on a human colony
               * hands the tune pool back to 2; any other outcome pushes the
               * 0x32 combat sting (5fef:13b2). */
              sound_set_bgm(2);
              /* GAME.TXT @RAIDNOTHING: "{tribe} raiding party wiped out in {colony}! Colonists jubilant!" */
              if (c->name[0]) {
                sound_play(0x5b); /* FUN_5fef_0f14 raid repelled (gunfight) */
                popup_msg_fill(
                  ctx->messages, "RAIDNOTHING", &raid_tok,
                  "%STRING0 raiding party wiped out in %STRING1!",
                  raid_line, sizeof(raid_line)
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party wiped out!",
                  tribe
                );
              }
              raid_body = raid_line;
            } else if (had_peace && max_alarm >= 55) {
              /* GAME.TXT @INDIANWAR thin — provocations break the treaty. */
              snprintf(
                raid_line,
                sizeof(raid_line),
                "The %s declare war! Prepare for WAR!",
                tribe
              );
              raid_body = raid_line;
            } else if (!was_at_war && prior_attacks == 0) {
              /* GAME.TXT @INDIANSURPRISE thin — only the tribe's FIRST attack
               * on this Euro is deniable; later raids use the plain @RAID*
               * chrome below (bugs.md). */
              if (c->name[0]) {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s make a surprise raid near %s! Their chief denies involvement.",
                  tribe,
                  c->name
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s make a surprise raid! Their chief denies involvement.",
                  tribe
                );
              }
              raid_body = raid_line;
            } else if (kind == AI_RAID_SHIP) {
              /* GAME.TXT @RAIDSHIP: "{tribe}... in {colony}! {ship} damaged. Colonists appalled!" */
              if (c->name[0]) {
                raid_tok.string2 = s_last_ship_type[0] ? s_last_ship_type : "A ship";
                popup_msg_fill(
                  ctx->messages, "RAIDSHIP", &raid_tok,
                  "%STRING0 raiding party attacks harbor in %STRING1!",
                  raid_line, sizeof(raid_line)
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s raid your harbor.",
                  tribe
                );
              }
              raid_body = raid_line;
            } else if (kind == AI_RAID_SCALP) {
              /* GAME.TXT @RAIDSCALP (WINCOLONY when abandon handled above). */
              if (c->name[0]) {
                sound_play(0x4e); /* FUN_5fef_0f14 colonists killed (screaming) */
                popup_msg_fill(
                  ctx->messages, "RAIDSCALP", &raid_tok,
                  "%STRING0 raiding party takes scalps in %STRING1!",
                  raid_line, sizeof(raid_line)
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s massacre colonists at your colony!",
                  tribe
                );
              }
              raid_body = raid_line;
            } else if (kind == AI_RAID_GOLD) {
              /* GAME.TXT @RAIDGOLD: "{tribe}... in {colony}! Merchants report {N}$ plundered." */
              if (c->name[0]) {
                raid_tok.number0 = s_last_gold_drained;
                raid_tok.has_number0 = true;
                sound_play(0x4d); /* FUN_5fef_0f14 loot gold (cheering + fireworks) */
                popup_msg_fill(
                  ctx->messages, "RAIDGOLD", &raid_tok,
                  "%STRING0 raiding party seizes strongboxes in %STRING1!",
                  raid_line, sizeof(raid_line)
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s raid your treasury!",
                  tribe
                );
              }
              raid_body = raid_line;
            } else if (kind == AI_RAID_BURN && s_last_burn_building[0]) {
              /* GAME.TXT @RAIDBURN: "{tribe}... in {colony}! {building} destroyed..." */
              raid_tok.string2 = s_last_burn_building;
              popup_msg_fill(
                ctx->messages, "RAIDBURN", &raid_tok,
                "The %STRING0 burn your %STRING2.",
                raid_line, sizeof(raid_line)
              );
              raid_body = raid_line;
            } else if (kind == AI_RAID_BURN) {
              /* GAME.TXT @RAIDBURN thin (no named building). */
              if (c->name[0]) {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party burns buildings in %s!",
                  tribe,
                  c->name
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party burns buildings!",
                  tribe
                );
              }
              raid_body = raid_line;
            } else if (kind == AI_RAID_STORES) {
              /* GAME.TXT @RAIDSTORES: "{tribe}... in {colony}! Large quantities of {cargo} stolen." */
              if (c->name[0]) {
                raid_tok.string2 = s_last_stores_cargo[0] ? s_last_stores_cargo : "goods";
                sound_play(0x4f); /* FUN_5fef_0f14 loot goods (screaming + shooting) */
                popup_msg_fill(
                  ctx->messages, "RAIDSTORES", &raid_tok,
                  "%STRING0 raiding party attacks stores in %STRING1!",
                  raid_line, sizeof(raid_line)
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party attacks your stores!",
                  tribe
                );
              }
              raid_body = raid_line;
            } else if (kind == AI_RAID_WREAK) {
              /* GAME.TXT @RAIDWREAK thin. */
              if (c->name[0]) {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party wreaks havoc in %s!",
                  tribe,
                  c->name
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party wreaks havoc!",
                  tribe
                );
              }
              raid_body = raid_line;
            } else {
              /* Generic successful raid chrome when kind-specific line unused. */
              if (c->name[0]) {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s raid %s.",
                  tribe,
                  c->name
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s raid your colony.",
                  tribe
                );
              }
              raid_body = raid_line;
            }
            ai_contact_human_chrome(
              ctx,
              target_euro,
              AI_POPUP_TAG_CONTACT_RAID,
              nation_id,
              "Raid",
              raid_body
            );
          } else if (abandoned && abandoned_name[0] &&
                     (kind == AI_RAID_SCALP || kind == AI_RAID_BURN)) {
            /*
             * Human bystander (colony's own nation is AI-controlled): @BURNED3
             * "Spies report: …" — the victim already got @BURNED above when
             * they are human; this covers the human watching a rival's colony
             * fall. Cite: GAME.TXT @BURNED3.
             */
            units_combat_notify_colony_burned_foreign(
              ctx->col1, abandoned_name, target_euro, ai_contact_tribe_name(nation_id)
            );
          }
        } else if (max_alarm >= 70) {
          /*
           * Approach march only in high-friction capture band (≥70). Mid gate
           * 40..69 keeps on-tile loot/combat but must not walk Braves — seed-100
           * TURN4→5 is already at-war for some tribes; approach broke the golden.
           * Cite: indian_raid_outcomes.md; golden_ai_turns.
           */
          int sdx = (c->x > brave->x) - (c->x < brave->x);
          int sdy = (c->y > brave->y) - (c->y < brave->y);
          units_try_move(
            ctx->units, brave->id, ctx->map, brave->x + sdx, brave->y + sdy, ctx->colonies, rng
          );
        }
      }
    }
  }

  /* 6. FUN_4d56_359c: high alarm vs Scouts → prefer displace; despawn if blocked. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id) {
      continue;
    }
    for (int e = 0; e < 4; ++e) {
      if (ind->alarm_by_player[e] < 90) {
        continue;
      }
      static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      for (int d = 0; d < 8; ++d) {
        const int foe = units_id_at(ctx->units, brave->x + dx[d], brave->y + dy[d]);
        if (foe < 0) {
          continue;
        }
        ColonizeUnit* f = units_get(ctx->units, foe);
        if (!f || f->nation_id != e) {
          continue;
        }
        const char* name = units_display_name(ctx->units, f);
        if (!name || !strstr(name, "Scout")) {
          continue;
        }
        /*
         * FUN_4d56_359c: prefer displace 1–2 tiles away from the Brave.
         * When displaced (not despawned) and status buffer present → human
         * warn line. Dialog warn widgets Done structural (ai_popup); VGA PARKED.
         *
         * Thin RNG kill-with-flee (unpark): at very-high alarm (≥95), ~1/4
         * chance kill even when a flee tile exists (DOS 359c kill/warn/displace
         * stand-in). Alarm 90..94 keeps prefer-displace (smoke). Blocked-path
         * despawn remains. Cite: indian_raid_outcomes.md §9.
         */
        {
          int killed = 0;
          char scout_fb[AI_POPUP_BODY_LEN];
          snprintf(
            scout_fb,
            sizeof(scout_fb),
            "The %s kill your Scout.",
            ai_contact_tribe_name(nation_id)
          );
          if (ind->alarm_by_player[e] >= 95) {
            const int roll = dos_rng_range(rng, 0, 99);
            if (roll < 25) {
              units_despawn(ctx->units, foe);
              ai_contact_human_chrome(
                ctx,
                e,
                AI_POPUP_TAG_CONTACT_RAID,
                nation_id,
                "Scout",
                scout_fb
              );
              killed = 1;
            }
          }
          if (!killed) {
            if (ai_contact_displace_scout(ctx, f, brave->x, brave->y)) {
              char warn_fb[AI_POPUP_BODY_LEN];
              snprintf(
                warn_fb,
                sizeof(warn_fb),
                "The %s warn your Scout away from their village.",
                ai_contact_tribe_name(nation_id)
              );
              ai_contact_human_chrome(
                ctx,
                e,
                AI_POPUP_TAG_CONTACT_RAID,
                nation_id,
                "Scout",
                warn_fb
              );
            } else {
              units_despawn(ctx->units, foe);
              ai_contact_human_chrome(
                ctx,
                e,
                AI_POPUP_TAG_CONTACT_RAID,
                nation_id,
                "Scout",
                scout_fb
              );
            }
          }
        }
      }
    }
  }
}


/* ======================================================================
 * DOS village action handlers (overlay 13 thunks behind FUN_4d56_4528's
 * @ACTIONS switch). Static port 2026-08-28 from viceroy_overlays.c /
 * viceroy_overlays.asm; see original_sources_annotated/ai/indian_actions_menu.md.
 * ====================================================================== */

/* FUN_1000_8c50 → FUN_15dc_00a2: alarm quartile 0..3 (<25 / <50 / <75 / else). */
static int ai_contact_alarm_quartile(int alarm) {
  if (alarm < 25) {
    return 0;
  }
  if (alarm < 50) {
    return 1;
  }
  if (alarm < 75) {
    return 2;
  }
  return 3;
}

static ColonizeDosRng* ai_contact_action_rng(ColonizeTurnContext* ctx, int nation_id, ColonizeDosRng* local) {
  if (ctx && ctx->rng) {
    return ctx->rng;
  }
  ai_contact_local_rng(ctx, nation_id, local);
  return local;
}

/* The acting unit of a village menu result (payload bits 2.. = id + 1). */
static ColonizeUnit* ai_contact_menu_unit(ColonizeTurnContext* ctx, const AiPopupState* popup, int e) {
  if (!ctx || !ctx->units || !popup) {
    return NULL;
  }
  const int uid = ai_contact_meet_payload_unit(popup->result_payload);
  if (uid < 0) {
    return NULL;
  }
  ColonizeUnit* u = units_get(ctx->units, uid);
  if (!u || !u->active || u->nation_id != e) {
    return NULL;
  }
  return u;
}

/* The village the unit is acting on: adjacent (or same tile) village of the tribe. */
static ColonizeCol1Tribe* ai_contact_menu_village(ColonizeTurnContext* ctx, int nation_id, const ColonizeUnit* u) {
  if (!ctx || !ctx->col1 || !ctx->col1->tribe) {
    return NULL;
  }
  ColonizeCol1Tribe* first = NULL;
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    if (!first) {
      first = t;
    }
    if (u && ai_contact_dist(t->x, t->y, u->x, u->y) <= 1) {
      return t;
    }
  }
  return first;
}

/* NAMES.TXT @JOB column 1 (DS:0x8ea4 + job*8): "Expert Farmers" … */
static const char* ai_contact_job_expert_name(const ColonizeTurnContext* ctx, int job) {
  static char live[40];
  if (ctx && ctx->names && job >= 0) {
    const ColonizeMsgSection* sec = assets_msg_find(ctx->names, "JOB");
    if (sec) {
      int row = 0;
      for (int i = 0; i < sec->line_count; ++i) {
        const char* line = sec->lines[i];
        if (!line || !line[0] || line[0] == ';' || line[0] == '@') {
          continue;
        }
        if (row == job) {
          const char* c1 = strchr(line, ',');
          if (c1) {
            c1++;
            while (*c1 == ' ') {
              c1++;
            }
            size_t n = 0;
            while (c1[n] && c1[n] != ',' && n + 1 < sizeof(live)) {
              live[n] = c1[n];
              n++;
            }
            while (n > 0 && live[n - 1] == ' ') {
              n--;
            }
            live[n] = '\0';
            if (n > 0) {
              return live;
            }
          }
          break;
        }
        row++;
      }
    }
  }
  if (job >= 0 && job < COLONIZE_FIELD_JOB_COUNT) {
    return colony_yield_job_name(job);
  }
  if (job == UNITS_JOB_SCOUT) {
    return "Seasoned Scouts";
  }
  return "colonists";
}

/* NAMES.TXT @JOB column 0 (DS:0x8ea2 + job*8): "Farmer" … / "Scout". */
static const char* ai_contact_job_name(int job) {
  if (job >= 0 && job < COLONIZE_FIELD_JOB_COUNT) {
    return colony_yield_job_name(job);
  }
  if (job == UNITS_JOB_SCOUT) {
    return "Scout";
  }
  return "colonist";
}

/* DS:0x8394 difficulty titles (%STRING0 of the @EXTORT* bodies). */
static const char* ai_contact_difficulty_title(const ColonizeCol1Save* col1) {
  static const char* k_titles[5] = {"Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"};
  unsigned d = col1 ? (unsigned)col1->head.difficulty : 0u;
  if (d > 4u) {
    d = 4u;
  }
  return k_titles[d];
}

/* FUN_1000_8804 → FUN_15eb_0142: nearest colony of `e` (continent -1 = any). */
static int ai_contact_nearest_own_colony(
  const ColonizeTurnContext* ctx,
  int e,
  int x,
  int y,
  int continent
) {
  if (!ctx || !ctx->colonies) {
    return -1;
  }
  int best = -1;
  int best_d = 9999;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* c = &ctx->colonies->colonies[ci];
    if (!c->active || c->nation_id != e) {
      continue;
    }
    if (continent >= 0 && ctx->map && map_continent_id_at(ctx->map, c->x, c->y) != continent) {
      continue;
    }
    const int d = ai_contact_dist(c->x, c->y, x, y);
    if (d < best_d) {
      best_d = d;
      best = ci;
    }
  }
  return best;
}

/*
 * Live recompute of the FUN_4962_0018 / FUN_4962_06b6 census bytes the
 * tribute roll reads: -0x6a4e (Euro exposed land combat on a continent),
 * -0x6be4 (Euro land combat total), -0x6e34 (Brave combat on a continent),
 * -0x6e7c (Brave combat total). Byte tables in DOS — capped at 255; the
 * nation total is a word. combat value = FUN_157e_004a(unit, mode 1).
 */
static int ai_contact_land_combat_sum(
  const ColonizeTurnContext* ctx,
  int nation,
  int continent,
  int exposed_only,
  int cap
) {
  if (!ctx || !ctx->units) {
    return 0;
  }
  ColonizeCombatStrengthCtx sctx;
  sctx.units = ctx->units;
  sctx.map = ctx->map;
  sctx.colonies = ctx->colonies;
  sctx.col1 = ctx->col1;
  int sum = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active || u->nation_id != nation || u->aboard_ship_id >= 0 ||
        units_is_sea(ctx->units, i)) {
      continue;
    }
    if (exposed_only) {
      if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
        continue;
      }
      if (ctx->colonies && colonies_id_at(ctx->colonies, u->x, u->y) >= 0) {
        continue;
      }
    }
    if (continent >= 0 && ctx->map && map_continent_id_at(ctx->map, u->x, u->y) != continent) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, i, 1, NULL);
    if (sum >= cap) {
      return cap;
    }
  }
  return sum;
}

/* DS:0xc8 / DS:0xde — the 20-tile colony work ring (5x5 minus centre and corners). */
static const int8_t k_ring20_dx[20] = {0, 1, 0, -1, -1, 1, 1, -1, 0, 2, 0, -2, -1, 1, -1, 1, -2, -2, 2, 2};
static const int8_t k_ring20_dy[20] = {-1, 0, 1, 0, -1, -1, 1, 1, -2, 0, 2, 0, -2, -2, 2, 2, -1, 1, -1, 1};

/*
 * thunk_FUN_1000_a618 skill pick (the "what does this village teach" half,
 * param_5 = 1 preview). Weights = the FUN_4d56_2154 bid[] table (DS:0x9e78,
 * ai_contact_meet_economics_2154) with fisherman zeroed, then tech trims:
 * tech<1 zero FurTrader/OreMiner, Farmer/2; tech<2 zero Weaver/Tobacconist/
 * SilverMiner, Farmer -= Farmer/4; tech<3 zero Distiller; tech==3 Silver
 * += Silver/2. Weighted draw rand(1,Σ). FurTrapper(4) with (x+y)%3==0 →
 * Seasoned Scout (0x16). Farmer(0) → Fisherman(8) when rand(1,20) < ocean
 * tiles in the 20-ring. DOS reseeds the RNG from the village position
 * (FUN_1000_86ba(x*256+y + DS:0x8d80)) so the answer is stable per village;
 * the DS:0x8d80 base is not named — this port seeds from the position alone.
 */
static int ai_contact_a618_skill(ColonizeTurnContext* ctx, int nation_id, const ColonizeCol1Tribe* t) {
  if (!ctx || !ctx->col1 || !t) {
    return COLONIZE_JOB_FARMER;
  }
  AiContactMeetEcon2154 econ;
  int w[16];
  if (ai_contact_meet_economics_2154(ctx, nation_id, t, &econ)) {
    for (int i = 0; i < 16; ++i) {
      w[i] = (int)econ.bid[i];
    }
  } else {
    memset(w, 0, sizeof(w));
    const int fb = ai_contact_taught_profession(t);
    if (fb >= 0 && fb < 16) {
      w[fb] = 1;
    } else {
      w[0] = 1;
    }
  }
  const unsigned tech = (unsigned)ctx->col1->indian[nation_id - 4].tech;
  w[8] = 0;
  if (tech < 1u) {
    w[12] = 0;
    w[6] = 0;
    w[0] >>= 1;
  }
  if (tech < 2u) {
    w[11] = 0;
    w[10] = 0;
    w[7] = 0;
    w[0] -= w[0] >> 2;
  }
  if (tech < 3u) {
    w[9] = 0;
  }
  if (tech == 3u) {
    w[7] += w[7] >> 1;
  }
  ColonizeDosRng rng;
  dos_rng_seed(&rng, (uint32_t)((int)t->y * 256 + (int)t->x));
  int sum = 0;
  for (int i = 0; i < 16; ++i) {
    sum += w[i];
  }
  int skill = 0;
  if (sum > 0) {
    int r = dos_rng_range(&rng, 1, sum);
    skill = -1;
    do {
      skill++;
      r -= w[skill];
    } while (r > 0 && skill < 15);
  }
  if (skill == 4 && (((int)t->x + (int)t->y) % 3) == 0) {
    skill = UNITS_JOB_SCOUT; /* 0x16 */
  }
  if (skill == 0 && ctx->map) {
    int ocean = 0;
    for (int k = 0; k < 20; ++k) {
      const int ox = (int)t->x + k_ring20_dx[k];
      const int oy = (int)t->y + k_ring20_dy[k];
      if (map_coords_inset(ctx->map, ox, oy) && map_tile_is_water(ctx->map, ox, oy)) {
        ocean++;
      }
    }
    if (dos_rng_range(&rng, 1, 20) < ocean) {
      skill = 8;
    }
  }
  return skill;
}

/* thunk_FUN_1000_a618 LAB_398c "DONE": profession = skill, village learned bit, @LEARNDONE. */
static void ai_contact_learnstay_apply(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t,
  int skill
) {
  if (!ctx || !u || !t) {
    return;
  }
  u->profession = skill;
  t->state.learned = 1;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_tribe_name(nation_id);
  tok.string1 = ai_contact_job_name(skill);
  char fb[AI_POPUP_BODY_LEN];
  snprintf(fb, sizeof(fb), "\"Congratulations, Young One. You have learned the ways of the %s and become a master %s.\"", tok.string0, tok.string1);
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "LEARNDONE", &tok, fb, body, sizeof(body));
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", body);
}

/*
 * thunk_FUN_1000_a618 (param_5 = 0) — "Live Among The Natives". Alarm
 * quartile ≥ 2 → @LEARNMAD (+3 alarm; silent when met-but-no-peace, rel &
 * 0x60 == 0x20). Petty Criminal → @LEARNCRIMINAL. Indian Convert →
 * @TEACHCONVERT. Any skilled profession → @LEARNMASTER. Free Colonist /
 * Indentured Servant: village already taught && !capital → @LEARNALREADY;
 * quartile 1 && rand(1,1000) < 200*difficulty+100 → @LEARNSLOW; human →
 * @LEARNSTAY CHOICE (Yes → DONE, No → @LEARNLATER); AI → DONE at once.
 */
static void ai_contact_live_among_natives(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t) {
    return;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  const int human = ai_contact_euro_is_human(ctx, e);
  const int skill = ai_contact_a618_skill(ctx, nation_id, t);
  const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
  const int band = ai_contact_alarm_quartile(alarm);
  const char* tribe = ai_contact_tribe_name(nation_id);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = tribe;
  tok.string1 = ai_contact_job_name(skill);
  const char* section = NULL;
  const char* fb = NULL;

  if (band > 1) {
    ai_diplo_indian_alarm_delta(ctx->col1, nation_id, e, 3);
    const uint8_t rel = ctx->col1->indian[nation_id - 4].euro_diplo[e];
    if ((rel & 0x60u) == 0x20u) {
      return; /* met, no peace → DOS returns before the popup */
    }
    section = "LEARNMAD";
    fb = "\"Your ill manners infuriate us, Young One. You fail to understand our ways, so we doubt you will ever learn anything from us.\"";
  } else if (ai_contact_is_petty_criminal(ctx->units, u)) {
    section = "LEARNCRIMINAL";
    fb = "\"Your ill manners offend us, Young One, and we doubt that you will ever be more than a common criminal. The %s will teach you nothing.\"";
  } else {
    const char* dname = units_display_name(ctx->units, u);
    const int is_convert = u->profession == COLONIZE_PROF_CONVERT || (dname && strstr(dname, "Convert") != NULL);
    /* Profession byte, not the display name (see is_petty_criminal). */
    const int is_indentured = u->profession == UNITS_JOB_SERVANT ||
      (dname && strstr(dname, "Indentured") != NULL);
    if (is_convert) {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "TEACHCONVERT", NULL, "Indian converts already know the Indian ways.", body, sizeof(body));
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", body);
      return;
    }
    if (u->profession != UNITS_JOB_NONE && !is_indentured) {
      tok.string1 = ai_contact_learner_skill_name(ctx->units, u);
      section = "LEARNMASTER";
      fb = "\"We are glad to have a master %s living among us, Old One. However, we can only teach new skills to colonists who do not yet have one.\"";
    } else if (t->state.learned && !t->state.capital) {
      section = "LEARNALREADY";
      fb = "\"The %s of this village have already shared their skills with young Europeans. Go and learn from them if you will, for we have nothing else to teach.\"";
    } else {
      int slow = 0;
      if (band > 0) {
        const int roll = dos_rng_range(rng, 1, 1000);
        if (roll < 200 * (int)ctx->col1->head.difficulty + 100) {
          slow = 1;
        }
      }
      if (slow) {
        section = "LEARNSLOW";
        fb = "\"You are unskilled and uncouth, Young One, and have difficulty understanding our ways. We see scarce hope for you, although you are welcome to remain in our village.\"";
      } else if (human && ctx->ai_popups) {
        char body[AI_POPUP_BODY_LEN];
        char fbs[AI_POPUP_BODY_LEN];
        snprintf(fbs, sizeof(fbs), "\"You are unskilled, Young One, and your ways are strange. If you wish, however, we %s will show you how to become a master %s.\"", tribe, tok.string1);
        popup_msg_fill(ctx->messages, "LEARNSTAY", &tok, fbs, body, sizeof(body));
        char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
        const ColonizeMsgSection* sec = assets_msg_find(ctx->messages, "LEARNSTAY");
        const int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
        static char yes_lbl[AI_POPUP_CHOICE_LEN];
        if (nch >= 2) {
          popup_msg_apply_tokens(yes_lbl, sizeof(yes_lbl), choice_buf[0], &tok);
        } else {
          snprintf(yes_lbl, sizeof(yes_lbl), "Then I shall become a master %s.", tok.string1);
        }
        const char* labels[2] = {yes_lbl, nch >= 2 ? choice_buf[1] : "Not right now, thanks."};
        const int ids[2] = {AI_CONTACT_LEARNSTAY_YES, AI_CONTACT_LEARNSTAY_NO};
        const int payload = (u->id & 0xffff) | (skill << 16);
        if (ai_popup_enqueue_choice_ctx(
              ctx->ai_popups, AI_POPUP_TAG_CONTACT_LEARNSTAY, e, nation_id, payload, NULL, body, labels, ids, 2
            )) {
          ai_contact_set_status(ctx, body);
          return;
        }
        ai_contact_learnstay_apply(ctx, e, nation_id, u, t, skill);
        return;
      } else {
        ai_contact_learnstay_apply(ctx, e, nation_id, u, t, skill);
        return;
      }
    }
  }
  char fbs[AI_POPUP_BODY_LEN];
  if (strstr(fb, "%s")) {
    snprintf(fbs, sizeof(fbs), fb, strcmp(section, "LEARNMASTER") == 0 ? tok.string1 : tribe);
  } else {
    snprintf(fbs, sizeof(fbs), "%s", fb);
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, section, &tok, fbs, body, sizeof(body));
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", body);
}

/*
 * thunk_FUN_1000_a60c — "Ask to Speak With Chief" (Scouts only). seasoned =
 * profession 0x16. alarm < 75: thr = rand(0, seasoned ? 140 : 100); good
 * branch when alarm < 25 || alarm/4 < thr: Arawak (slot 2) rand(0,
 * (8-difficulty) << seasoned) == 0 → kill; @CHIEFHOWDY (village skill +
 * three most-wanted goods); then alarm < thr && !scouted → scouted, rand(1,3):
 * 1 plain Scout → Seasoned + @CHIEFGUIDES/@WELLSEASONED (seasoned → tales);
 * 2 → @CHIEFAREA + fog reveal radius 6; 3 → @CHIEFGIFT gold = (tech+1) *
 * rand(1,6) * (Σ3 rand(1,10-difficulty)) * 4. Else @CHIEFBORED. Kill path
 * (alarm ≥ 75 / bad roll): FF 6 (Coronado) owned → bored instead; else
 * @CHIEFKILL + unit destroyed.
 */
static void ai_contact_speak_with_chief(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t) {
    return;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  ColonizeCol1Save* col1 = ctx->col1;
  /* FUN_4d56_2820 4d56:2855: a human visitor rolls 04d4(0,3); on 0 the
   * tribe's tune pool takes over — 5 (Natives), Inca → 7 (Pizarro at
   * Cuzco), Aztec → 6 (Tenochtitlan). */
  if (ai_contact_euro_is_human(ctx, e) && dos_rng_range(rng, 0, 2) == 0) {
    sound_set_bgm(nation_id == 0 ? 7 : (nation_id == 1 ? 6 : 5));
  }
  ColonizeCol1Indian* ind = &col1->indian[nation_id - 4];
  const int human = ai_contact_euro_is_human(ctx, e);
  const int seasoned = u->profession == UNITS_JOB_SCOUT;
  const int alarm = ai_diplo_indian_alarm(col1, nation_id, e);
  const int diff = (int)col1->head.difficulty;
  const char* tribe = ai_contact_tribe_name(nation_id);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = tribe;
  char body[AI_POPUP_BODY_LEN];
  char fb[AI_POPUP_BODY_LEN];
  int kill = 1;

  if (alarm < 0x4b) {
    const int thr = dos_rng_range(rng, 0, seasoned ? 140 : 100);
    if (alarm < 0x19 || (alarm >> 2) < thr) {
      kill = 0;
      if (nation_id == 6) {
        const int hi = (8 - diff) << (seasoned ? 1 : 0);
        if (dos_rng_range(rng, 0, hi) == 0) {
          kill = 1;
        }
      }
      if (!kill) {
        if (human) {
          /* @CHIEFHOWDY: village skill + the three most-wanted goods (ask[] top). */
          const int skill = ai_contact_a618_skill(ctx, nation_id, t);
          AiContactMeetEcon2154 econ;
          int want[3] = {-1, -1, -1};
          if (ai_contact_meet_economics_2154(ctx, nation_id, t, &econ)) {
            int ask[16];
            int order[16];
            for (int i = 0; i < 16; ++i) {
              ask[i] = (int)econ.ask[i];
              order[i] = i;
            }
            if (t->last_bought < 16) {
              ask[t->last_bought] = 0;
            }
            if (t->last_sold < 16) {
              ask[t->last_sold] = 0;
            }
            for (int i = 1; i < 16; ++i) {
              const int v = order[i];
              int j = i - 1;
              while (j >= 0 && ask[order[j]] > ask[v]) {
                order[j + 1] = order[j];
                j--;
              }
              order[j + 1] = v;
            }
            want[0] = order[15];
            want[1] = order[14];
            want[2] = order[13];
          }
          PopupMsgTokens ht;
          memset(&ht, 0, sizeof(ht));
          ht.string0 = ai_contact_job_expert_name(ctx, skill);
          ht.string1 = want[0] >= 0 ? ai_contact_cargo_name(want[0]) : "trade goods";
          ht.string2 = want[1] >= 0 ? ai_contact_cargo_name(want[1]) : "tools";
          ht.string3 = want[2] >= 0 ? ai_contact_cargo_name(want[2]) : "muskets";
          snprintf(fb, sizeof(fb), "\"Greetings, travelers. We are a peaceful village known for our %s. We would gladly trade with you if you bring us some badly needed %s. We would also pay well for %s or %s.\"", ht.string0, ht.string1, ht.string2, ht.string3);
          popup_msg_fill(ctx->messages, "CHIEFHOWDY", &ht, fb, body, sizeof(body));
          ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
        }
        if (alarm < thr && !t->state.scouted) {
          t->state.scouted = 1;
          int r = dos_rng_range(rng, 1, 3);
          if (r == 1 && !seasoned) {
            tok.string1 = ai_contact_level_noun(ctx, (int)ind->tech);
            u->profession = UNITS_JOB_SCOUT;
            if (human) {
              snprintf(fb, sizeof(fb), "\"We gladly welcome you to our %s. In honor of the strange tales you have shared with us, the %s shall provide you with guides to aid your passage through our lands.\"", tok.string1, tribe);
              popup_msg_fill(ctx->messages, "CHIEFGUIDES", &tok, fb, body, sizeof(body));
              ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
              popup_msg_fill(ctx->messages, "WELLSEASONED", NULL, "Our Scouts have improved to Seasoned status.", body, sizeof(body));
              ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
            }
            return;
          }
          if (r == 3) {
            const int r1 = dos_rng_range(rng, 1, 10 - diff);
            const int r2 = dos_rng_range(rng, 1, 10 - diff);
            const int r3 = dos_rng_range(rng, 1, 10 - diff);
            const int r6 = dos_rng_range(rng, 1, 6);
            const int gold = ((int)ind->tech + 1) * r6 * (r1 + r2 + r3) * 4;
            tok.string1 = ai_contact_euro_name(e);
            tok.number0 = gold;
            tok.has_number0 = true;
            if (human) {
              snprintf(fb, sizeof(fb), "\"The %s welcome the emissaries of the %s tribe. Please take these valuable beads (worth %d gold) back to your chieftain as a peace offering.\"", tribe, tok.string1, gold);
              popup_msg_fill(ctx->messages, "CHIEFGIFT", &tok, fb, body, sizeof(body));
              ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
            }
            col1->nation[e].gold += (uint32_t)gold;
            /* bugs.md: the human's live treasury is europe->gold — crediting
             * only the col1 mirror made the beads worth nothing. */
            if (ctx->europe && e == ctx->human_nation) {
              ctx->europe->gold += gold;
            }
            return;
          }
          /* r == 2, or a seasoned scout rolling 1: tales of nearby lands. */
          if (human) {
            popup_msg_fill(ctx->messages, "CHIEFAREA", &tok, "\"The %s are pleased to welcome travelers from afar. Come sit by the fire and we shall tell you tales of nearby lands.\"", body, sizeof(body));
            ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
          }
          if (ctx->map) {
            /* FUN_1000_8986 → FUN_13f1_02b4(unit, DX = 6): 13x13 reveal. */
            for (int dy = -6; dy <= 6; ++dy) {
              for (int dx = -6; dx <= 6; ++dx) {
                const int rx = u->x + dx;
                const int ry = u->y + dy;
                if (map_coords_inset(ctx->map, rx, ry)) {
                  map_reveal_tile(ctx->map, rx, ry, e);
                }
              }
            }
          }
          return;
        }
        /* bored */
        tok.string1 = ai_contact_euro_name(e);
        snprintf(fb, sizeof(fb), "\"The %s are always pleased to welcome %s travelers.\"", tribe, tok.string1);
        popup_msg_fill(ctx->messages, "CHIEFBORED", &tok, fb, body, sizeof(body));
        ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
        return;
      }
    }
  }
  /* LAB_3a22: kill unless FF 6 (Coronado) is owned. */
  if (!founding_fathers_nation_has(col1, e, FF_FRANCISCO_CORONADO)) {
    snprintf(fb, sizeof(fb), "\"You have broken sacred taboos of the %s tribe! We shall tie you up for target practice.\"", tribe);
    popup_msg_fill(ctx->messages, "CHIEFKILL", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
    units_despawn(ctx->units, u->id);
    return;
  }
  tok.string1 = ai_contact_euro_name(e);
  snprintf(fb, sizeof(fb), "\"The %s are always pleased to welcome %s travelers.\"", tribe, tok.string1);
  popup_msg_fill(ctx->messages, "CHIEFBORED", &tok, fb, body, sizeof(body));
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Chief", body);
}

/*
 * thunk_FUN_1000_a5f4 — "Demand Tribute". euro = exposed land combat on the
 * unit's continent + (land combat total >> 1); Spanish ×1.5; Cortes (FF 10)
 * ×1.5. indian = (Brave combat on continent + (Brave total >> 1)) * 2 +
 * alarm/2. win = rand(0,indian) < rand(0,euro) && an own colony exists on
 * the continent. alarm bump = human ? difficulty+1 : 1. (win || indian <
 * euro) && alarm < 75: (win || alarm < 50): village not yet extorted && win →
 * bit 0x10, bump ×2, @EXTORTSTUFF: 10 of the village's top bid[] good into
 * the nearest colony; else @EXTORTPOOR, bump 0. alarm 50..74 → @EXTORTNO.
 * Otherwise @EXTORTLAUGH. Ends with FUN_4cc6_00f2(+bump).
 */
static void ai_contact_demand_tribute(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t) {
    return;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  ColonizeCol1Save* col1 = ctx->col1;
  const int human = ai_contact_euro_is_human(ctx, e);
  const int continent = ctx->map ? map_continent_id_at(ctx->map, u->x, u->y) : -1;
  const int alarm = ai_diplo_indian_alarm(col1, nation_id, e);
  const char* tribe = ai_contact_tribe_name(nation_id);

  int euro = ai_contact_land_combat_sum(ctx, e, continent, 1, 255) +
             (ai_contact_land_combat_sum(ctx, e, -1, 0, 0xffff) >> 1);
  if (e == 2) {
    euro += euro >> 1;
  }
  if (founding_fathers_nation_has(col1, e, FF_HERNAN_CORTES)) {
    euro += euro >> 1;
  }
  const int indian =
    (ai_contact_land_combat_sum(ctx, nation_id, continent, 0, 255) +
     (ai_contact_land_combat_sum(ctx, nation_id, -1, 0, 255) >> 1)) * 2 + (alarm >> 1);
  int bump = human ? (int)col1->head.difficulty + 1 : 1;
  const int r_e = dos_rng_range(rng, 0, euro);
  const int r_i = dos_rng_range(rng, 0, indian);
  const int cid = ai_contact_nearest_own_colony(ctx, e, u->x, u->y, continent);
  const int win = cid >= 0 && r_i < r_e;

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  char body[AI_POPUP_BODY_LEN];
  char fb[AI_POPUP_BODY_LEN];
  const char* title = ai_contact_difficulty_title(col1);
  if ((win || indian < euro) && alarm < 0x4b) {
    if (win || alarm < 0x32) {
      if (!t->state.tribute_paid && win) {
        bump <<= 1;
        t->state.tribute_paid = 1;
        int good = 13;
        int top_bid = 0;
        AiContactMeetEcon2154 econ;
        if (ai_contact_meet_economics_2154(ctx, nation_id, t, &econ)) {
          int order[16];
          for (int i = 0; i < 16; ++i) {
            order[i] = i;
          }
          for (int i = 1; i < 16; ++i) {
            const int v = order[i];
            int j = i - 1;
            while (j >= 0 && econ.bid[order[j]] > econ.bid[v]) {
              order[j + 1] = order[j];
              j--;
            }
            order[j + 1] = v;
          }
          good = order[15];
          top_bid = (int)econ.bid[15]; /* DS:0x9e96 literal — the muskets slot, always 0 after 2154 */
        }
        ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
        int qty = 10;
        if (c) {
          const int cap = colonies_warehouse_capacity(ctx->colonies, c, good);
          int room = cap - c->stock[good];
          int lim = top_bid * 3 + 10;
          if (lim > 100) {
            lim = 100;
          }
          if (lim < room) {
            room = lim;
          }
          if (room < 10) {
            room = 10;
          }
          qty = room;
          c->stock[good] += qty;
        }
        tok.string0 = title;
        tok.string1 = tribe;
        tok.number0 = qty;
        tok.has_number0 = true;
        tok.string2 = ai_contact_cargo_name(good);
        tok.string3 = c ? c->name : "your colony";
        snprintf(fb, sizeof(fb), "\"Great %s, we bow before the might of your strange weapons. The humble and peaceloving %s shall gladly deliver %d %s to %s.\"", title, tribe, qty, tok.string2, tok.string3);
        popup_msg_fill(ctx->messages, "EXTORTSTUFF", &tok, fb, body, sizeof(body));
        ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Tribute", body);
      } else {
        tok.string0 = title;
        tok.string1 = tribe;
        snprintf(fb, sizeof(fb), "\"Mighty %s, we tremble before you. Alas, the humble and peaceloving %s have no gifts worthy of your magnificence.\"", title, tribe);
        popup_msg_fill(ctx->messages, "EXTORTPOOR", &tok, fb, body, sizeof(body));
        ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Tribute", body);
        bump = 0;
      }
    } else {
      tok.string0 = title;
      tok.string1 = col1->player[e].name;
      tok.string2 = tribe;
      snprintf(fb, sizeof(fb), "\"You must think us very foolish indeed, %s %s. The %s will not be taken in by your tricks and treachery.\"", title, tok.string1, tribe);
      popup_msg_fill(ctx->messages, "EXTORTNO", &tok, fb, body, sizeof(body));
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Tribute", body);
    }
  } else {
    tok.string0 = tribe;
    snprintf(fb, sizeof(fb), "\"We laugh at your puny threats. Do not try our patience, for %s warriors are known for their ferocity in times of war.\"", tribe);
    popup_msg_fill(ctx->messages, "EXTORTLAUGH", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Tribute", body);
  }
  if (bump != 0) {
    ai_diplo_indian_alarm_delta(col1, nation_id, e, bump);
  }
}

/*
 * FUN_4cc6_03f8 — strongest nearby Euro presence for a village: returns the
 * owning nation of the best-scoring colony within DOS distance < 7 (or -1)
 * and its score. Ring pass: for the 20 work-ring tiles, sum the attack
 * column of that tile's land units with attack > 1 into threat[owner];
 * halved when the tile holds a Euro settlement, halved again on the outer
 * ring (|dx| ≥ 2 or |dy| ≥ 2). Colony pass: human owner → (building weight,
 * divisor) by difficulty {1/2, 3/4, 1/1, 3/2, 2/1}, AI → (1,1);
 * built = Σ built-building bits × weight / divisor; pop6 = min(6, pop);
 * score = ((2*max(0,pop-6) + min(tech, pop>>1) + pop6 + difficulty +
 * ((built-8)>>2)) * 2 - dist - 1) / (dist + 4); halved if the colony sits
 * on another continent; += threat[owner]; halved for the French; halved
 * when the owner has Pocahontas. Tail: with a mission in the village, the
 * best score is scaled by who owns it: same nation → Jesuit ×1/2, plain
 * ×3/4; other nation → Jesuit ×2, plain ×3/2.
 */
static int ai_contact_4cc6_03f8(
  ColonizeTurnContext* ctx,
  int nation_id,
  const ColonizeCol1Tribe* v,
  int* out_score
) {
  *out_score = 0;
  if (!ctx || !ctx->col1 || !v) {
    return -1;
  }
  const ColonizeCol1Save* col1 = ctx->col1;
  const int vx = (int)v->x;
  const int vy = (int)v->y;
  const int vcont = ctx->map ? map_continent_id_at(ctx->map, vx, vy) : -1;
  const int tech = (int)col1->indian[nation_id - 4].tech;
  int threat[4] = {0, 0, 0, 0};
  if (ctx->units && ctx->map) {
    for (int k = 0; k < 20; ++k) {
      const int tx = vx + k_ring20_dx[k];
      const int ty = vy + k_ring20_dy[k];
      if (!map_coords_inset(ctx->map, tx, ty) || map_tile_is_water(ctx->map, tx, ty)) {
        continue;
      }
      int owner = -1;
      int sum = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = units_get_const(ctx->units, i);
        if (!u || !u->active || u->x != tx || u->y != ty || u->aboard_ship_id >= 0) {
          continue;
        }
        if (owner < 0) {
          owner = u->nation_id;
        }
        if (u->nation_id != owner || units_is_sea(ctx->units, i)) {
          continue;
        }
        const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
        if (t && t->attack > 1) {
          sum += t->attack;
        }
      }
      if (owner < 0 || owner > 3) {
        continue;
      }
      if (ctx->colonies && colonies_id_at(ctx->colonies, tx, ty) >= 0) {
        sum >>= 1;
      }
      if (abs((int)k_ring20_dx[k]) >= 2 || abs((int)k_ring20_dy[k]) >= 2) {
        sum >>= 1;
      }
      threat[owner] += sum;
    }
  }
  int best = -1;
  int best_score = 0;
  if (ctx->colonies) {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      const ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      const int d = ai_contact_dist(vx, vy, c->x, c->y);
      if (d >= 7) {
        continue;
      }
      const int owner = c->nation_id;
      int weight = 1;
      int divisor = 1;
      int diff = 0;
      if (ai_contact_euro_is_human(ctx, owner)) {
        diff = (int)col1->head.difficulty;
        static const int k_w[5] = {1, 3, 1, 3, 2};
        static const int k_d[5] = {2, 4, 1, 2, 1};
        if (diff >= 0 && diff <= 4) {
          weight = k_w[diff];
          divisor = k_d[diff];
        }
      }
      int built = 0;
      for (int b = 0; b < COLONIZE_BUILDING_TYPES_MAX && b < 48; ++b) {
        if (c->has_building[b]) {
          built += weight;
        }
      }
      built /= divisor;
      const int pop = c->population;
      const int pop6 = pop < 6 ? pop : 6;
      int half = pop >> 1;
      if (tech < half) {
        half = tech;
      }
      int score = (((pop6 - pop) * -2 + half + pop6 + diff + ((built - 8) >> 2)) * 2 - d - 1) / (d + 4);
      if (ctx->map && map_continent_id_at(ctx->map, c->x, c->y) != vcont) {
        score >>= 1;
      }
      score += threat[owner];
      if (owner == 1) {
        score >>= 1;
      }
      if (founding_fathers_nation_has(col1, owner, FF_POCAHONTAS)) {
        score >>= 1;
      }
      if (best_score < score) {
        best_score = score;
        best = owner;
      }
    }
  }
  if (best_score > 0 && best >= 0 && v->mission != COL1_TRIBE_MISSION_NONE) {
    const int owner = (int)(v->mission & COL1_TRIBE_MISSION_NATION_MASK);
    const int jesuit = (v->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0;
    if (owner == best) {
      best_score = jesuit ? (best_score >> 1) : (best_score - (best_score >> 2));
    } else {
      best_score = jesuit ? (best_score << 1) : (best_score + (best_score >> 1));
    }
  }
  *out_score = best_score;
  return best;
}

/*
 * thunk_FUN_1000_a594 — "Denounce Heresy of {rival}'s Mission". Per village
 * of the tribe: (n, s) = FUN_4cc6_03f8 (nearby Euro presence): n == rival →
 * pro_me += s; n == me → mine += s; else s seeds that village's mission
 * weight; mission weight += population, ×2 Jesuit, ×2 capital, credited to
 * the mission owner's side (mine → mine, else pro_me). Then pro_me += alarm[foreign] << (capital ? 4 : 0),
 * mine += alarm[me] >> (capital ? 29 : 1); capital: both += rand(1,20) and
 * both deltas ×2; my Jesuit: pro_me ×2, delta ×2; rival Jesuit: mine ×2,
 * delta ×2. rand(1, mine+pro_me) > pro_me → @HERESY1 (missionary burned,
 * rival's alarm −delta, mine +delta); else @HERESY0 (mission flips to me,
 * mine −delta, rival's +delta). The missionary unit is consumed either way.
 */
static void ai_contact_denounce_heresy(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t || t->mission == COL1_TRIBE_MISSION_NONE) {
    return;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  ColonizeCol1Save* col1 = ctx->col1;
  const int foreign = (int)(t->mission & COL1_TRIBE_MISSION_NATION_MASK);
  if (foreign < 0 || foreign > 3 || foreign == e) {
    return;
  }
  int mine = 0;
  int pro_me = 0;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* v = &col1->tribe[ti];
      if ((int)v->nation_id != nation_id) {
        continue;
      }
      int c = 0;
      int sc = 0;
      const int n = ai_contact_4cc6_03f8(ctx, nation_id, v, &sc);
      if (n == foreign) {
        pro_me += sc;
      } else if (n == e) {
        mine += sc;
      } else {
        c = sc;
      }
      if (v->mission == COL1_TRIBE_MISSION_NONE) {
        continue;
      }
      c += (int)v->population;
      if (v->mission & COL1_TRIBE_MISSION_JESUIT_BIT) {
        c *= 2;
      }
      if (v->state.capital) {
        c <<= 1;
      }
      if ((int)(v->mission & COL1_TRIBE_MISSION_NATION_MASK) == e) {
        mine += c;
      } else {
        pro_me += c;
      }
    }
  }
  const int jesuit_me = ai_contact_is_jesuit_grade(col1, ctx->units, u);
  const int rival_jesuit = (t->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0;
  const unsigned cap_shift = t->state.capital ? 4u : 0u;
  pro_me += ai_diplo_indian_alarm(col1, nation_id, foreign) << cap_shift;
  mine += ai_diplo_indian_alarm(col1, nation_id, e) >> ((1u - cap_shift) & 0x1fu);
  int d_me = ai_contact_alarm_quartile(mine) + 1;
  int d_them = ai_contact_alarm_quartile(pro_me) + 1;
  if (t->state.capital) {
    pro_me += dos_rng_range(rng, 1, 20);
    mine += dos_rng_range(rng, 1, 20);
    d_them *= 2;
    d_me *= 2;
  }
  if (jesuit_me) {
    pro_me <<= 1;
    d_them <<= 1;
  }
  if (rival_jesuit) {
    mine <<= 1;
    d_me <<= 1;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_euro_name(e);
  tok.string1 = ai_contact_euro_name(foreign);
  tok.string2 = ai_contact_tribe_name(nation_id);
  char body[AI_POPUP_BODY_LEN];
  char fb[AI_POPUP_BODY_LEN];
  const int roll = dos_rng_range(rng, 1, mine + pro_me > 0 ? mine + pro_me : 1);
  if (pro_me < roll) {
    snprintf(fb, sizeof(fb), "%s missionaries denounce heresy of %s. Loyal %s worshipers burn the %s at the stake!", tok.string0, tok.string1, tok.string2, tok.string0);
    popup_msg_fill(ctx->messages, "HERESY1", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "Mission", body);
    d_them = -d_them;
  } else {
    snprintf(fb, sizeof(fb), "%s missionaries denounce heresy of %s. %s converts burn %s mission and erect a new, %s one!", tok.string0, tok.string1, tok.string2, tok.string1, tok.string0);
    popup_msg_fill(ctx->messages, "HERESY0", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "Mission", body);
    t->mission = (uint8_t)(jesuit_me ? ((unsigned)e | COL1_TRIBE_MISSION_JESUIT_BIT) : (unsigned)e);
    d_me = -d_me;
  }
  units_despawn(ctx->units, u->id);
  ai_diplo_indian_alarm_delta(col1, nation_id, foreign, d_them);
  ai_diplo_indian_alarm_delta(col1, nation_id, e, d_me);
}

/*
 * thunk_FUN_1000_a5dc — "Establish Mission". count = own missions with the
 * tribe; Sepulveda (FF 23) ×2; Las Casas (FF 24) >>1; Pocahontas (FF 16)
 * >>1; French >>1. base = count*8 − {25,15,10,5}[alarm quartile]; capital:
 * base += sign(base)*8. Text = "MISSION" + n, n = quartile raised to 1 when
 * base > −6, 2 when base > 0, 3 when base > 9. Mission owner = me; Jesuit
 * bit when profession 0x18 (Jesuit) or Brebeuf (FF 22). Unit consumed;
 * FUN_4cc6_00f2(+base).
 */
static void ai_contact_establish_mission(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u,
  ColonizeCol1Tribe* t
) {
  if (!ctx || !ctx->col1 || !u || !t || t->mission != COL1_TRIBE_MISSION_NONE) {
    return;
  }
  ColonizeCol1Save* col1 = ctx->col1;
  int count = 0;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* v = &col1->tribe[ti];
      if ((int)v->nation_id == nation_id && v->mission != COL1_TRIBE_MISSION_NONE &&
          (int)(v->mission & COL1_TRIBE_MISSION_NATION_MASK) == e) {
        count++;
      }
    }
  }
  if (founding_fathers_nation_has(col1, e, FF_JUAN_DE_SEPULVEDA)) {
    count <<= 1;
  }
  if (founding_fathers_nation_has(col1, e, FF_BARTOLOME_DE_LAS_CASAS)) {
    count >>= 1;
  }
  if (founding_fathers_nation_has(col1, e, FF_POCAHONTAS)) {
    count >>= 1;
  }
  if (e == 1) {
    count >>= 1;
  }
  const int alarm = ai_diplo_indian_alarm(col1, nation_id, e);
  int band = ai_contact_alarm_quartile(alarm);
  static const int k_sub[4] = {0x19, 0xf, 10, 5};
  int base = count * 8 - k_sub[band];
  if (t->state.capital) {
    base += (base > 0 ? 1 : (base < 0 ? -1 : 0)) * 8;
  }
  if (base > -6 && band < 1) {
    band = 1;
  }
  if (base > 0 && band < 2) {
    band = 2;
  }
  if (base > 9) {
    band = 3;
  }
  char section[16];
  snprintf(section, sizeof(section), "MISSION%d", band);
  const int cid = ai_contact_nearest_own_colony(ctx, e, t->x, t->y, -1);
  const ColonizeColony* c = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_euro_name(e);
  tok.string1 = c ? c->name : col1->player[e].country_name;
  tok.string2 = (ctx->game_autumn && *ctx->game_autumn != 0) ? "Autumn" : "Spring";
  tok.number0 = ctx->game_year ? (int)*ctx->game_year : 0;
  tok.has_number0 = true;
  tok.string3 = ai_contact_tribe_name(nation_id);
  char fb[AI_POPUP_BODY_LEN];
  snprintf(fb, sizeof(fb), "%s %s mission founded in %s, %d.", tok.string0, tok.string1, tok.string2, tok.number0);
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, section, &tok, fb, body, sizeof(body));
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "Mission", body);
  t->mission = (uint8_t)e;
  if (u->profession == UNITS_JOB_MISSIONARY || founding_fathers_nation_has(col1, e, FF_JEAN_DE_BREBEUF)) {
    t->mission = (uint8_t)(t->mission | COL1_TRIBE_MISSION_JESUIT_BIT);
  }
  units_despawn(ctx->units, u->id);
  ai_diplo_indian_alarm_delta(col1, nation_id, e, base);
}

/*
 * thunk_FUN_1000_a5e8 — "Enter Hostile Village" (wagon / ship, alarm ≥ 75).
 * r = rand(0,500): r ≤ alarm → @KILLWAGONS + unit destroyed; r ≤ 2·alarm →
 * @MADATWAGONS; else @GRUDGEWAGONS and the trade runs. Returns 1 when the
 * caller should continue into the Trade arm.
 */
static int ai_contact_enter_hostile_village(
  ColonizeTurnContext* ctx,
  int e,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->col1 || !u) {
    return 0;
  }
  ColonizeDosRng local;
  ColonizeDosRng* rng = ai_contact_action_rng(ctx, nation_id, &local);
  const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
  const int r = dos_rng_range(rng, 0, 500);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_tribe_name(nation_id);
  char body[AI_POPUP_BODY_LEN];
  char fb[AI_POPUP_BODY_LEN];
  if (r <= alarm) {
    snprintf(fb, sizeof(fb), "The %s seize your goods and kill your traders!", tok.string0);
    popup_msg_fill(ctx->messages, "KILLWAGONS", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "Trade", body);
    units_despawn(ctx->units, u->id);
    return 0;
  }
  if (r <= alarm * 2) {
    snprintf(fb, sizeof(fb), "The %s refuse to deal with you.", tok.string0);
    popup_msg_fill(ctx->messages, "MADATWAGONS", &tok, fb, body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "Trade", body);
    return 0;
  }
  snprintf(fb, sizeof(fb), "The %s grudgingly agree to trade.", tok.string0);
  popup_msg_fill(ctx->messages, "GRUDGEWAGONS", &tok, fb, body, sizeof(body));
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Trade", body);
  return 1;
}

void ai_contact_apply_popup_result(ColonizeTurnContext* ctx, const AiPopupState* popup) {
  if (!ctx || !popup || !popup->has_result) {
    return;
  }
  ai_contact_bind_names(ctx);
  const int e = popup->result_nation_a;
  const int nation_id = popup->result_nation_b;
  if (e < 0 || e > 3 || nation_id < 4 || nation_id > 11) {
    return;
  }
  if (!ctx->col1_ok || !ctx->col1) {
    return;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];

  /*
   * FUN_5bfb_022e @INDIANWELCOME: Yes → FUN_5bfb_0182 peace; No/cancel →
   * FUN_4cc6_00f2 hostility + @INDIANSHUN. Cite: GAME.TXT; indian_contact.md.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_WELCOME) {
    if (popup->result_cancelled || popup->result_choice_id == AI_CONTACT_WELCOME_NO) {
      ai_contact_apply_welcome_reject(ctx, ind, nation_id, e);
    } else if (popup->result_choice_id == AI_CONTACT_WELCOME_YES) {
      ai_contact_apply_welcome_accept(ctx, ind, nation_id, e);
    }
    return;
  }

  if (popup->result_cancelled) {
    return;
  }

  /*
   * Gift amount CHOICE (FUN_5bfb_102a stand-in): Small −5 / Large −10.
   * Cite: indian_contact.md gift amount widget.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_GIFT) {
    if (popup->result_choice_id == AI_CONTACT_GIFT_SMALL) {
      ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 5u, 1);
    } else if (popup->result_choice_id == AI_CONTACT_GIFT_LARGE) {
      ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 10u, 2);
    } else if (popup->result_choice_id == AI_CONTACT_GIFT_GENEROUS) {
      ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 20u, 3);
    }
    return;
  }

  /*
   * Trade buy-offer CHOICE (FUN_4d56_2820 LAB_002e92 human branch): Accept
   * applies the locked price via ai_contact_apply_trade_offer; Decline (or
   * cancel) is a no-op pass. Cite: indian_trade_2820.md.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_TRADE_PICK) {
    ai_contact_apply_trade_pick(ctx, ind, nation_id, e, popup->result_payload, popup->result_choice_id);
    return;
  }
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_BUYWHICH) {
    if (popup->result_choice_id > 0) {
      ai_contact_apply_buywhich(ctx, ind, nation_id, e, popup->result_payload, popup->result_choice_id - 1);
    }
    return;
  }
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_BUY0) {
    ai_contact_apply_buy0(ctx, ind, nation_id, e, popup->result_payload, popup->result_choice_id);
    return;
  }
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_TRADE_OFFER) {
    ai_contact_apply_trade_offer(ctx, ind, nation_id, e, popup->result_payload, popup->result_choice_id);
    return;
  }

  /*
   * Demand amount CHOICE (FUN_5bfb_102a / 1092 stand-in): tools vs gold.
   * Cite: indian_contact.md mid demand amount widget.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_DEMAND) {
    int near_x = 0;
    int near_y = 0;
    ColonizeUnit* other =
      ai_contact_find_adjacent_euro(ctx, nation_id, e, &near_x, &near_y);
    if (!other && ctx->col1->tribe) {
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
        if ((int)t->nation_id == nation_id) {
          near_x = t->x;
          near_y = t->y;
          break;
        }
      }
    }
    if (popup->result_choice_id == AI_CONTACT_DEMAND_TOOLS) {
      ai_contact_apply_demand_tools(ctx, ind, nation_id, e, other, near_x, near_y);
    } else if (popup->result_choice_id == AI_CONTACT_DEMAND_GOLD) {
      ai_contact_apply_demand_gold(ctx, ind, nation_id, e);
    }
    return;
  }

  /*
   * @INDIANBEGFOOD Give/Refuse (FUN_5bfb_022e already-met adjacency —
   * see ai_contact_try_village_beg_food's own header comment). Payload
   * carries the offer-time colony id (captured at offer time, same
   * discipline as ai_king_merc's landing tile — the colony could
   * theoretically change hands between offer and apply). choice_id 2 =
   * accept/give (label[1]), 1 = decline/refuse (label[0]).
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_BEGFOOD) {
    ai_contact_apply_beg_food(
      ctx, ind, nation_id, e, popup->result_payload, popup->result_choice_id == 2
    );
    return;
  }

  /*
   * Incite Indians target picked (FUN_4d56_417e tail): result_choice_id is
   * the target Euro nation (0-3), set by ai_contact_enqueue_incite_target_choice.
   */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_INCITE) {
    /* Unpack the offer-time discount flags packed by
     * ai_contact_enqueue_incite_target_choice (bit0=is_missionary,
     * bit1=is_capital). */
    const int is_missionary = popup->result_payload & 1;
    const int is_capital = (popup->result_payload >> 1) & 1;
    ai_contact_apply_incite(
      ctx, ind, nation_id, e, popup->result_choice_id, is_missionary, is_capital
    );
    return;
  }

  /* @LEARNSTAY (thunk_FUN_1000_a618): Yes → DONE; No → @LEARNLATER. */
  if (popup->result_tag == AI_POPUP_TAG_CONTACT_LEARNSTAY) {
    const int uid = popup->result_payload & 0xffff;
    const int skill = popup->result_payload >> 16;
    ColonizeUnit* lu = ctx->units ? units_get(ctx->units, uid) : NULL;
    ColonizeCol1Tribe* lt = ai_contact_menu_village(ctx, nation_id, lu);
    if (popup->result_choice_id == AI_CONTACT_LEARNSTAY_YES && lu && lu->active && lt) {
      ai_contact_learnstay_apply(ctx, e, nation_id, lu, lt, skill);
    } else {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "LEARNLATER", NULL, "\"Very well. Perhaps another time.\"", body, sizeof(body));
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", body);
    }
    return;
  }

  /*
   * Village action menu result (NAMES.TXT @ACTIONS, FUN_4d56_4528 human arm).
   * Trade keeps the 2820 port; the unit-scoped DOS actions dispatch on the
   * acting unit carried in the payload. Attack Village is committed by
   * game_loop (it moves the unit); Leave dismisses.
   */
  if (popup->result_tag != AI_POPUP_TAG_CONTACT_MEET) {
    return;
  }
  ColonizeUnit* menu_unit = ai_contact_menu_unit(ctx, popup, e);
  ColonizeCol1Tribe* menu_village = ai_contact_menu_village(ctx, nation_id, menu_unit);
  int near_x = 0;
  int near_y = 0;
  ColonizeUnit* other = ai_contact_find_adjacent_euro(ctx, nation_id, e, &near_x, &near_y);
  if (menu_unit) {
    other = menu_unit;
    near_x = menu_unit->x;
    near_y = menu_unit->y;
  }
  if (!other && ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id == nation_id) {
        near_x = t->x;
        near_y = t->y;
        break;
      }
    }
  }

  switch (popup->result_choice_id) {
  case AI_CONTACT_CHOICE_LEAVE:
    /*
     * Thin dismiss OK (FUN_5bfb_022e Leave). No trade/gift/teach side effects.
     * Deep 2820 leave/dialog matrix PARKED. Cite: indian_contact.md Meet CHOICE.
     */
    {
      char leave_fb[AI_POPUP_BODY_LEN];
      snprintf(
        leave_fb,
        sizeof(leave_fb),
        "Farewell to the %s.",
        ai_contact_tribe_name(nation_id)
      );
      ai_contact_human_chrome(
        ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Contact", leave_fb
      );
    }
    break;
  case AI_CONTACT_CHOICE_ENTER_HOSTILE:
    /* thunk_FUN_1000_a5e8: survive the roll → the Trade arm (a63c) runs. */
    if (!menu_unit || !ai_contact_enter_hostile_village(ctx, e, nation_id, menu_unit)) {
      break;
    }
    /* FALLTHROUGH */
  case AI_CONTACT_CHOICE_TRADE:
    /* FUN_4d56_2820 shell (ai_contact_2820_begin): tables, hold pick, sell loop, buy loop. */
    if (!other || !ai_contact_2820_begin(ctx, ind, nation_id, e, other)) {
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Trade", "Trade concluded.");
    }
    break;
  case AI_CONTACT_CHOICE_GIFT: {
    /*
     * Gift-band + human popups → Small/Large amount CHOICE (FUN_5bfb_102a).
     * Else thin gift_or_demand (auto Large / demand / refuse). Cite:
     * indian_contact.md gift amount widget.
     */
    const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
    const int gift_band =
      friction < 40 && friction < 55 && ind->alarm_by_player[e] < 55;
    if (gift_band && ctx->ai_popups && ai_contact_euro_is_human(ctx, e)) {
      if (ai_contact_enqueue_gift_amount_choice(ctx, e, nation_id)) {
        break;
      }
    }
    if (other) {
      ai_contact_gift_or_demand(ctx, ind, nation_id, e, other, near_x, near_y);
    }
    break;
  }
  case AI_CONTACT_CHOICE_DEMAND: {
    /* "Demand Tribute" — thunk_FUN_1000_a5f4 on the acting unit. */
    if (menu_unit && menu_village) {
      ai_contact_demand_tribute(ctx, e, nation_id, menu_unit, menu_village);
      break;
    }
    /*
     * Mid-band + human popups → tools/gold amount CHOICE (FUN_5bfb_102a).
     * Alarmed (≥55) → gift_or_demand refuse OK "The %s refuse demands."
     * (CONTACT_DEMAND; no amount CHOICE / no drain). Cite:
     * indian_contact.md demand amount widget / alarmed refuse.
     */
    const int friction = ai_contact_pair_friction(ind, ctx->col1, nation_id, e);
    const int demand_band =
      friction >= 40 && friction < 55 && ind->alarm_by_player[e] < 55;
    if (demand_band && ctx->ai_popups && ai_contact_euro_is_human(ctx, e)) {
      if (ai_contact_enqueue_demand_amount_choice(
            ctx, e, nation_id, other, near_x, near_y
          )) {
        break;
      }
    }
    if (other) {
      ai_contact_gift_or_demand(ctx, ind, nation_id, e, other, near_x, near_y);
    } else if (ind->alarm_by_player[e] >= 55 || friction >= 55) {
      /* No adjacent Euro unit — still show alarmed refuse chrome. */
      char refuse_fb[AI_POPUP_BODY_LEN];
      snprintf(
        refuse_fb,
        sizeof(refuse_fb),
        "The %s refuse demands.",
        ai_contact_tribe_name(nation_id)
      );
      ai_contact_human_chrome(
        ctx, e, AI_POPUP_TAG_CONTACT_DEMAND, nation_id, "Demand", refuse_fb
      );
    }
    break;
  }
  case AI_CONTACT_CHOICE_TEACH:
    /* "Live Among The Natives" — thunk_FUN_1000_a618 on the acting unit. */
    if (menu_unit && menu_village) {
      ai_contact_live_among_natives(ctx, e, nation_id, menu_unit, menu_village);
    } else {
      ai_contact_teach_skill(ctx, nation_id);
    }
    break;
  case AI_CONTACT_CHOICE_CHIEF:
    if (menu_unit && menu_village) {
      ai_contact_speak_with_chief(ctx, e, nation_id, menu_unit, menu_village);
    }
    break;
  case AI_CONTACT_CHOICE_HERESY:
    if (menu_unit && menu_village) {
      ai_contact_denounce_heresy(ctx, e, nation_id, menu_unit, menu_village);
    }
    break;
  case AI_CONTACT_CHOICE_MISSION:
    if (menu_unit && menu_village) {
      ai_contact_establish_mission(ctx, e, nation_id, menu_unit, menu_village);
    }
    break;
  case AI_CONTACT_CHOICE_ATTACK_VILLAGE:
    /* Committed by game_loop (needs the move engine); nothing to do here. */
    break;

  case AI_CONTACT_CHOICE_INCITE: {
    /*
     * FUN_4d56_417e Mode 1: show the "whom would you like us to attack"
     * target-nation CHOICE. No affordable/eligible target → refuse OK.
     * Re-unpack the same is_missionary/is_capital bits the Meet CHOICE
     * itself was enqueued with (ai_contact_enqueue_village_meet) and carry
     * them into the target-choice's own payload.
     */
    const int is_missionary = popup->result_payload & 1;
    const int is_capital = (popup->result_payload >> 1) & 1;
    if (!ai_contact_enqueue_incite_target_choice(ctx, e, nation_id, is_missionary, is_capital)) {
      char refuse_fb[AI_POPUP_BODY_LEN];
      snprintf(
        refuse_fb,
        sizeof(refuse_fb),
        "The %s have no reason to go on the warpath.",
        ai_contact_tribe_name(nation_id)
      );
      ai_contact_human_chrome(
        ctx, e, AI_POPUP_TAG_CONTACT_INCITE, nation_id, "Incite", refuse_fb
      );
    }
    break;
  }
  default:
    break;
  }
}
