#include "core/ai_contact.h"

#include "core/ai_diplo.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_last_raid_kind = AI_RAID_NOTHING;
static char s_last_burn_building[48];

int ai_contact_last_raid_kind(void) {
  return s_last_raid_kind;
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
}

/* Meet dialog choice ids (FUN_5bfb_022e / 5bfb_102a stand-in; widgets unparked). */
enum {
  AI_CONTACT_CHOICE_TRADE = 1,
  AI_CONTACT_CHOICE_GIFT = 2,
  AI_CONTACT_CHOICE_DEMAND = 3,
  AI_CONTACT_CHOICE_TEACH = 4,
  AI_CONTACT_CHOICE_LEAVE = 5
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
  ai_contact_set_status(ctx, body);
  if (ctx->ai_popups) {
    ai_popup_enqueue_ok_ctx(
      ctx->ai_popups, tag, e, nation_b, 0, title ? title : "Natives", body
    );
  }
}

/* @TRIBES order (Inca..Tupi); matches col1_bridge encounter labels. */
static const ColonizeMsgCatalog* s_contact_names;

static void ai_contact_bind_names(const ColonizeTurnContext* ctx) {
  s_contact_names = (ctx && ctx->names) ? ctx->names : NULL;
}

static const char* ai_contact_tribe_name(int nation_id) {
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
 * Pull quoted body lines from GAME.TXT @SECTION (skip @width / Yes / No).
 * Falls back to fallback_body when catalog missing.
 */
static void ai_contact_msg_body(
  const ColonizeMsgCatalog* messages,
  const char* section,
  const char* tribe,
  const char* euro,
  const char* fallback_body,
  char* out,
  size_t out_size
) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (messages && section) {
    const ColonizeMsgSection* sec = assets_msg_find(messages, section);
    if (sec && sec->line_count > 0) {
      size_t used = 0;
      for (int i = 0; i < sec->line_count && used + 2 < out_size; ++i) {
        const char* line = sec->lines[i];
        if (!line || !line[0]) {
          continue;
        }
        if (line[0] == '@') {
          continue;
        }
        if (strcmp(line, "Yes") == 0 || strcmp(line, "No") == 0) {
          continue;
        }
        /* Strip surrounding quotes and {%…} placeholders lightly. */
        char buf[COLONIZE_MSG_LINE_LEN];
        size_t bi = 0;
        for (size_t c = 0; line[c] && bi + 1 < sizeof(buf); ++c) {
          if (line[c] == '"' || line[c] == '{' || line[c] == '}') {
            continue;
          }
          if (line[c] == '%') {
            /* Skip %STRING0 / %NUMBER0 style tokens. */
            while (line[c] && line[c] != ' ' && line[c] != '.' && line[c] != ',') {
              ++c;
            }
            if (!line[c]) {
              break;
            }
            --c;
            continue;
          }
          buf[bi++] = line[c];
        }
        buf[bi] = '\0';
        if (bi == 0) {
          continue;
        }
        if (used > 0 && used + 1 < out_size) {
          out[used++] = ' ';
        }
        const size_t n = strlen(buf);
        if (used + n >= out_size) {
          break;
        }
        memcpy(out + used, buf, n);
        used += n;
        out[used] = '\0';
      }
    }
  }
  if (out[0] == '\0' && fallback_body) {
    snprintf(out, out_size, "%s", fallback_body);
  }
  /* Prefer inserting tribe/euro names when placeholders were stripped empty. */
  (void)tribe;
  (void)euro;
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
  /* Peaceful floor so refuse-talk (relation < 40) cannot fire next tick. */
  {
    const uint8_t cur = ai_diplo_indian_relation(ctx->col1, nation_id, e);
    if (cur < 100u) {
      ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, (int)(100u - cur));
    }
  }
  /* Fresh peace: clear alarm/friction toward this Euro (fandom first contact). */
  ind->alarm_by_player[e] = 0;
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
    ctx->messages, "INDIANPEACE", tribe, euro, peace_fb, peace_body, sizeof(peace_body)
  );
  ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Peace", peace_body);

  /* DOS FUN_5bfb_0182: @INDIANCOME when relation < 0x19 before/as friendly. */
  if (rel_before < 25u) {
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
      ctx->messages, "INDIANCOME", tribe, euro, come_fb, come_body, sizeof(come_body)
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
  /* DOS +100 hostility → Linux at-war (relation < 50): force to 0. */
  {
    const uint8_t cur = ai_diplo_indian_relation(ctx->col1, nation_id, e);
    if (cur > 0) {
      ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, -(int)cur);
    }
  }
  if (ind->alarm_by_player[e] < 80u) {
    ind->alarm_by_player[e] = 80u;
  }
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
  char shun_fb[AI_POPUP_BODY_LEN];
  snprintf(
    shun_fb,
    sizeof(shun_fb),
    "Then the mighty %s shall mercilessly drive you from our shores. Prepare for WAR!",
    tribe
  );
  char shun_body[AI_POPUP_BODY_LEN];
  ai_contact_msg_body(
    ctx->messages, "INDIANSHUN", tribe, ai_contact_euro_name(e), shun_fb, shun_body, sizeof(shun_body)
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
  char fb[AI_POPUP_BODY_LEN];
  snprintf(
    fb,
    sizeof(fb),
    "The %s tribe welcomes you. To celebrate our friendship, we generously offer "
    "you the land you now occupy as a gift. Will you accept our treaty and live "
    "with us in peace as brothers?",
    tribe
  );
  char body[AI_POPUP_BODY_LEN];
  ai_contact_msg_body(
    ctx->messages, "INDIANWELCOME", tribe, ai_contact_euro_name(e), fb, body, sizeof(body)
  );
  char title[AI_POPUP_TITLE_LEN];
  snprintf(title, sizeof(title), "%s", tribe);
  static const char* labels[] = {"Yes", "No"};
  static const int ids[] = {AI_CONTACT_WELCOME_YES, AI_CONTACT_WELCOME_NO};
  ai_popup_enqueue_choice_ctx(
    ctx->ai_popups,
    AI_POPUP_TAG_CONTACT_WELCOME,
    e,
    nation_id,
    0,
    title,
    body,
    labels,
    ids,
    2
  );
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
  /* DOS OR bit 0x20 before dialog. */
  ind->euro_diplo[euro_nation] = 1;
  ai_diplo_indian_relation_delta(ctx->col1, indian_nation, euro_nation, 5);

  if (ai_contact_euro_is_human(ctx, euro_nation) && ctx->ai_popups) {
    ai_contact_enqueue_welcome(ctx, euro_nation, indian_nation);
    return 1;
  }
  /* AI Euro / no popups: auto-accept (DOS local_c = 1). */
  ai_contact_apply_welcome_accept(ctx, ind, indian_nation, euro_nation);
  return 1;
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

static void ai_contact_enqueue_village_meet(ColonizeTurnContext* ctx, int e, int nation_id) {
  if (!ctx || !ctx->ai_popups) {
    return;
  }
  const char* tribe = ai_contact_tribe_name(nation_id);
  char body[AI_POPUP_BODY_LEN];
  snprintf(
    body,
    sizeof(body),
    "You enter a %s village. How do you wish to greet the natives?",
    tribe
  );
  char title[AI_POPUP_TITLE_LEN];
  snprintf(title, sizeof(title), "%s", tribe);
  static const char* labels[] = {"Trade", "Gift", "Demand", "Teach", "Leave"};
  static const int ids[] = {
    AI_CONTACT_CHOICE_TRADE,
    AI_CONTACT_CHOICE_GIFT,
    AI_CONTACT_CHOICE_DEMAND,
    AI_CONTACT_CHOICE_TEACH,
    AI_CONTACT_CHOICE_LEAVE
  };
  ai_popup_enqueue_choice_ctx(
    ctx->ai_popups,
    AI_POPUP_TAG_CONTACT_MEET,
    e,
    nation_id,
    0,
    title,
    body,
    labels,
    ids,
    5
  );
  {
    char st[96];
    /*
     * GAME.TXT @INDIANHELLO1 / @INDIANHELLO2 thin: cool alarm → worthy welcome;
     * hot mid+ → ruthless greeting. Cite: indian_contact.md meet §1.
     */
    const ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
    const int hot = ind && ind->alarm_by_player[e] >= 40;
    if (hot) {
      snprintf(
        st,
        sizeof(st),
        "The %s tribe greets the most ruthless of the %s.",
        tribe,
        ai_contact_euro_name(e)
      );
    } else {
      snprintf(
        st,
        sizeof(st),
        "The %s tribe welcomes the most worthy of the %s.",
        tribe,
        ai_contact_euro_name(e)
      );
    }
    ai_contact_set_status(ctx, st);
  }
}

int ai_contact_try_village_meet(ColonizeTurnContext* ctx, int euro_nation, int indian_nation) {
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
  if (ai_diplo_indian_at_war(ctx->col1, euro_nation, indian_nation - 4)) {
    return 0;
  }
  if (ai_contact_meet_choice_pending(ctx->ai_popups, euro_nation, indian_nation) ||
      ai_contact_welcome_pending(ctx->ai_popups, euro_nation, indian_nation)) {
    return 0;
  }
  ai_contact_enqueue_village_meet(ctx, euro_nation, indian_nation);
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
static int ai_contact_is_teachable_learner(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const char* name = units_display_name(units, u);
  if (!name) {
    return 0;
  }
  return strstr(name, "Free Colonist") != NULL || strstr(name, "Scout") != NULL;
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
 * NAMES.TXT @TRIBES field-3 flavor trade goods (Inca..Tupi). Prefer live parse
 * from ctx->names when present; else static table matching stock COLONIZE/NAMES.TXT.
 * Used for trade chrome only (not teach skills).
 */
static const char* ai_contact_tribe_flavor_good(const ColonizeTurnContext* ctx, int nation_id) {
  static char live[48];
  static const char* k_flavor[8] = {
      "Jewelled Relics", /* Inca */
      "Gold Bars",       /* Aztec */
      "Bone Jewelry",    /* Arawak */
      "Wood Carvings",   /* Iroquois */
      "Turquoise",       /* Cherokee */
      "Beads",           /* Apache */
      "Beads",           /* Sioux */
      "Gems",            /* Tupi */
  };
  const int idx = nation_id - 4;
  if (idx < 0 || idx >= 8) {
    return "goods";
  }
  if (ctx && ctx->names) {
    const ColonizeMsgSection* tribes = assets_msg_find(ctx->names, "TRIBES");
    if (tribes) {
      int row = 0;
      for (int i = 0; i < tribes->line_count; ++i) {
        const char* line = tribes->lines[i];
        if (!line || line[0] == '\0' || line[0] == ';') {
          continue;
        }
        if (row == idx) {
          /* Name, short, good, tech, color — want third CSV field. */
          const char* p = strchr(line, ',');
          if (p) {
            p = strchr(p + 1, ',');
          }
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
  return k_flavor[idx];
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
     * Col1 one-shot: tribe.state.learned already set → skip teach and do not
     * write teach/refuse status (preserves gift/trade chrome). Cite: village
     * teaches once; fandom Teach / trade / missions.
     */
    if (t->state.learned) {
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
      if (!ai_contact_is_teachable_learner(ctx->units, other)) {
        continue;
      }
      const int e = other->nation_id;
      /*
       * Alarmed Indian diplomacy (fandom Alarm; same ≥55 refuse-talk gate):
       * high alarm/friction → refuse teach (status thinned; ai_popup Done).
       */
      if (ind->alarm_by_player[e] >= 55 || t->alarm[e].friction >= 55) {
        char refuse_fb[AI_POPUP_BODY_LEN];
        snprintf(
          refuse_fb,
          sizeof(refuse_fb),
          "The %s refuse to teach.",
          ai_contact_tribe_name(nation_id)
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
       * (<40). Same refuse chrome as ≥55 (no invented gold). Cite: fandom
       * Alarm / Teach; indian_contact.md teach-skill pulse.
       */
      if (ind->alarm_by_player[e] >= 40 || t->alarm[e].friction >= 40) {
        char refuse_fb[AI_POPUP_BODY_LEN];
        snprintf(
          refuse_fb,
          sizeof(refuse_fb),
          "The %s refuse to teach.",
          ai_contact_tribe_name(nation_id)
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
        char line[96];
        snprintf(
          line,
          sizeof(line),
          "The %s teach Seasoned Scout.",
          ai_contact_tribe_name(nation_id)
        );
        ai_contact_human_chrome(
          ctx, e, AI_POPUP_TAG_CONTACT_TEACH, nation_id, "Teach", line
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
  char title[AI_POPUP_TITLE_LEN];
  char body[AI_POPUP_BODY_LEN];
  snprintf(title, sizeof(title), "Gift");
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
           title,
           body,
           labels,
           ids,
           n
         )
           ? 1
           : 0;
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
  char title[AI_POPUP_TITLE_LEN];
  char body[AI_POPUP_BODY_LEN];
  snprintf(title, sizeof(title), "Demand");
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
           title,
           body,
           labels,
           ids,
           n
         )
           ? 1
           : 0;
}

/*
 * Gift / demand structural stand-in (5bfb_102a / 1092 via ai_popup; VGA PARKED).
 * After peaceful meet adjacency (caller already gated alarmed / very-low rel):
 *  - alarmed (≥55 refuse-talk gate) → refuse gift/demand with status; no extra
 *    gold penalty beyond existing gift costs
 *  - low friction + Euro gold < 10 → refuse refuse (cannot pay −10 gift)
 *  - low friction + Euro gold >= 20 → gift: Euro −10 gold, friction −2
 *    (auto Large; human Meet Gift may enqueue Small/Large amount CHOICE first)
 *  - mid friction (40–54) + tools/gold → demand: auto prefers −10 tools from
 *    nearest colony warehouse when stock ≥20 (else unit tools ≥20, else −15
 *    gold when treasury ≥50); friction −3. Human Meet→Demand may enqueue
 *    tools-vs-gold amount CHOICE first. Cite: gift gold≥20 band mirrored for
 *    tools; gold stand-in needs a fuller purse (≥50) when tools are short.
 *  - very high covered by alarmed refuse / raids
 * Human-facing paths set a thin status line when ctx->status is present.
 * Source: fandom Alarm — alarmed natives may refuse trade/gifts.
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

  /* Low friction gift / tribute (auto Large −10; Generous −20 when gold≥40). */
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
    if (nat->gold >= 40u) {
      ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 20u, 3);
      return;
    }
    ai_contact_apply_gift_gold(ctx, ind, nation_id, e, 10u, 2);
    return;
  }

  /*
   * Mid friction (40–54) demand / payoff; ≥55 refused above.
   * Auto prefers tools then gold; Meet→Demand may offer amount CHOICE.
   * Cannot pay either path → refuse OK (follow-up polish; no invented drain).
   * Cite: FUN_5bfb_102a / 1092; indian_contact.md mid demand.
   */
  if (ai_contact_demand_can_pay_tools(ctx, e, other, near_x, near_y)) {
    ai_contact_apply_demand_tools(ctx, ind, nation_id, e, other, near_x, near_y);
    return;
  }
  if (ai_contact_demand_can_pay_gold(ctx, e)) {
    ai_contact_apply_demand_gold(ctx, ind, nation_id, e);
    return;
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
 *    Cite: docs/manual_gap.md; fandom Missionaries denounce; WARPATH gold PARKED.
 *  - alarmed (≥55 refuse-talk gate) → refuse convert/heresy; no crosses
 *  - mid (40..54) convert: Jesuit-grade only (PEDIA @JOB24 / Brebeuf).
 * Teach/convert widgets Done structural; WARPATH gold / deep 2820 PARKED.
 */
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
  u->x = best_x;
  u->y = best_y;
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
        const int fr_before = (int)t->alarm[e].friction;
        ai_contact_bump_u8_cap100(&t->alarm[e].friction, bump);
        ai_contact_bump_u16_cap100(&ind->alarm_by_player[e], bump);
        /*
         * GAME.TXT @INDIANCOMMENT thin: first cross into mid friction →
         * overuse concern status (dialog widgets PARKED).
         */
        if (fr_before < 40 && (int)t->alarm[e].friction >= 40) {
          char comment_fb[AI_POPUP_BODY_LEN];
          snprintf(
            comment_fb,
            sizeof(comment_fb),
            "The %s are concerned about land use near their settlements.",
            ai_contact_tribe_name(nation_id)
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
          if (c->name[0]) {
            snprintf(
              comment_fb,
              sizeof(comment_fb),
              "The %s are concerned that %s encroaches on lands near their settlements.",
              ai_contact_tribe_name(nation_id),
              c->name
            );
          } else {
            snprintf(
              comment_fb,
              sizeof(comment_fb),
              "The %s are concerned about land use near their settlements.",
              ai_contact_tribe_name(nation_id)
            );
          }
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

  /* FUN_4cc6_00f2 / 4962_06b6-shaped: met → ±1 by alarm band. */
  for (int e = 0; e < 4; ++e) {
    if (ctx->col1->player[e].control == 2) {
      continue;
    }
    int delta = 0;
    if (ind->euro_diplo[e]) {
      delta = (ind->alarm_by_player[e] > 40) ? -1 : 1;
    }
    ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, delta);
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
static void ai_contact_clear_tribe_last_goods(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return;
  }
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id != nation_id) {
      continue;
    }
    t->last_bought = 0xffu;
    t->last_sold = 0xffu;
  }
}

/*
 * Outdoor cargo natives of this nation typically trade (teach last_sold map).
 * Fish (Arawak) has no cargo id → 0xff. Cite: indian_contact.md profession map.
 */
static uint8_t ai_contact_nation_primary_sold_cargo(int nation_id) {
  static const uint8_t k_sold[8] = {
      COLONIZE_CARGO_SILVER,  /* 4 Inca */
      COLONIZE_CARGO_ORE,     /* 5 Aztec */
      0xffu,                  /* 6 Arawak — fish */
      COLONIZE_CARGO_FURS,    /* 7 Iroquois */
      COLONIZE_CARGO_TOBACCO, /* 8 Cherokee */
      COLONIZE_CARGO_COTTON,  /* 9 Apache */
      COLONIZE_CARGO_FURS,    /* 10 Sioux */
      COLONIZE_CARGO_SUGAR,   /* 11 Tupi */
  };
  const int idx = nation_id - 4;
  if (idx < 0 || idx >= 8) {
    return 0xffu;
  }
  return k_sold[idx];
}

/*
 * Thin peaceful auto-trade (FUN_5bfb_022e / 2aac…311e stand-in).
 * Colony warehouse or nearby ship/wagon hold TRADE_GOODS → alarm/friction decay.
 * Sea/land trade (fandom); no invented price math. Deep 2820 buy PARKED.
 */
static int ai_contact_auto_trade(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  int near_x,
  int near_y
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind) {
    return 0;
  }
  if (!ind->euro_diplo[e] || ind->alarm_by_player[e] >= 50) {
    return 0;
  }
  /* French (1): cooperation bias — slightly longer wagon/ship reach (fandom). */
  const int max_dist = (e == 1) ? 5 : 4;
  int best_ci = -1;
  int best_colony_score = -1;
  if (ctx->colonies) {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id != e) {
        continue;
      }
      const int dist = ai_contact_dist(c->x, c->y, near_x, near_y);
      if (dist > max_dist || c->stock[COLONIZE_CARGO_TRADE_GOODS] <= 0) {
        continue;
      }
      const int score = c->stock[COLONIZE_CARGO_TRADE_GOODS] * 4 - dist;
      if (score > best_colony_score) {
        best_colony_score = score;
        best_ci = ci;
      }
    }
  }
  int best_ship = -1;
  int best_hold = -1;
  int best_ship_score = -1;
  if (ctx->units) {
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      ColonizeUnit* u = &ctx->units->units[ui];
      if (!u->active || u->nation_id != e) {
        continue;
      }
      /* Sea ship or land Wagon Train hold (fandom sea/land trade). */
      const int sea = units_is_sea(ctx->units, u->id);
      if (!sea) {
        const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
        if (!ty || !strstr(ty->name, "Wagon") || ty->cargo <= 0) {
          continue;
        }
      }
      const int dist = ai_contact_dist(u->x, u->y, near_x, near_y);
      if (dist > max_dist) {
        continue;
      }
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        if (u->hold_goods_type[h] != COLONIZE_CARGO_TRADE_GOODS ||
            u->hold_goods_amount[h] <= 0) {
          continue;
        }
        const int score = u->hold_goods_amount[h] * 4 - dist;
        if (score > best_ship_score) {
          best_ship_score = score;
          best_ship = u->id;
          best_hold = h;
        }
      }
    }
  }
  const int use_colony =
    best_ci >= 0 && (best_ship < 0 || best_colony_score >= best_ship_score);
  if (!use_colony && best_ship < 0) {
    return 0;
  }
  /*
   * Thin 2820 hard-bargain stand-in: mid alarm 45..49 still trades but skips
   * relation bump and tribe friction decay (tense haggle / 306c tension).
   * Check before alarm decay. Deep buy/price arms PARKED.
   */
  const int hard =
    (ind->alarm_by_player[e] >= 45 && ind->alarm_by_player[e] < 50);
  if (use_colony) {
    ColonizeColony* c = &ctx->colonies->colonies[best_ci];
    c->stock[COLONIZE_CARGO_TRADE_GOODS]--;
  } else {
    ColonizeUnit* ship = units_get(ctx->units, best_ship);
    if (!ship || best_hold < 0) {
      return 0;
    }
    ship->hold_goods_amount[best_hold]--;
    if (ship->hold_goods_amount[best_hold] <= 0) {
      ship->hold_goods_amount[best_hold] = 0;
      ship->hold_goods_type[best_hold] = 0;
    }
  }
  if (ind->alarm_by_player[e] > 0) {
    ind->alarm_by_player[e]--;
  }
  if (ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->nation_id == nation_id && !hard && t->alarm[e].friction > 0) {
        t->alarm[e].friction--;
      }
      /* Book-keeping: Europeans sold trade goods; natives' outdoor good offered. */
      if ((int)t->nation_id == nation_id) {
        t->last_bought = (uint8_t)COLONIZE_CARGO_TRADE_GOODS;
        {
          const uint8_t sold = ai_contact_nation_primary_sold_cargo(nation_id);
          if (sold != 0xffu) {
            t->last_sold = sold;
          }
        }
      }
    }
  }
  if (!hard) {
    ai_diplo_indian_relation_delta(ctx->col1, nation_id, e, 2);
  }
  {
    char trade_fb[AI_POPUP_BODY_LEN];
    if (hard) {
      snprintf(
        trade_fb,
        sizeof(trade_fb),
        "Trade concluded after a hard bargain. The %s offer %s.",
        ai_contact_tribe_name(nation_id),
        ai_contact_tribe_flavor_good(ctx, nation_id)
      );
    } else {
      snprintf(
        trade_fb,
        sizeof(trade_fb),
        "Trade accepted. The %s offer %s.",
        ai_contact_tribe_name(nation_id),
        ai_contact_tribe_flavor_good(ctx, nation_id)
      );
    }
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Trade", trade_fb);
  }
  return 1;
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
            if (t->mission == COL1_TRIBE_MISSION_NONE && t->alarm[e].friction < 30) {
              /* Thin meet offer: nation only (no Jesuit unit on this Brave path). */
              t->mission = (uint8_t)e;
            }
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
      ai_contact_auto_trade(ctx, ind, nation_id, e, brave->x, brave->y);

      /* 4. Gift / demand stand-in (5bfb_102a / 1092; AI silent). */
      ai_contact_gift_or_demand(ctx, ind, nation_id, e, other, brave->x, brave->y);
    }
  }

  /* 2b. Missionary adjacent to tribe → mission owner + crosses. */
  ai_contact_missionary_convert(ctx, nation_id);

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
  const int roll = rng ? dos_rng_range(rng, 0, 99) : (max_alarm % 100);
  if (max_alarm >= 85 && roll < 15 && ai_contact_colony_has_wreak_target(c)) {
    return AI_RAID_WREAK;
  }
  if (max_alarm >= 70 && roll < 25 && c && c->population > 1) {
    return AI_RAID_SCALP;
  }
  /* BURN: construction, lumber, or destroyable built building. */
  if (max_alarm >= 60 && roll < 20 &&
      ai_contact_colony_has_burn_target(ctx ? ctx->colonies : NULL, c)) {
    return AI_RAID_BURN;
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
          return AI_RAID_SHIP;
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
 * Thin Brave escort lead pick (14fe): same-nation AI_MOVE/GOTO within MD≤3.
 * When raid gate Euro is known, prefer lead whose goto (or position) is closer
 * to that Euro's nearest colony; else nearest-lead by MD. Deep alarmed escort
 * scoring still PARKED. Cite: units_follow_unit; indian_raid_outcomes.md §1.
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
  (void)ai_contact_raid_gate_target(ctx, ind, nation_id, &gate_euro, NULL);

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
    if (md <= 0 || md > 3) {
      continue;
    }
    int ax = o->x;
    int ay = o->y;
    ai_contact_unit_goto_xy(o, &ax, &ay);
    const int target_d =
      gate_euro >= 0 ? ai_contact_nearest_euro_colony_dist(ctx, gate_euro, ax, ay) : 99;
    if (gate_euro >= 0) {
      if (target_d < best_target_d || (target_d == best_target_d && md < best_md)) {
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
  scout->x = best_x;
  scout->y = best_y;
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
     * Thin Brave escort (14fe stand-in): idle Brave with no orders may
     * units_follow_unit a same-nation Brave that already has AI_MOVE/GOTO.
     * Lead pick prefers goto toward raid-gate Euro colony when known; else
     * nearest-lead. Deep alarmed escort scoring still PARKED. Cite:
     * units_follow_unit; indian_raid_outcomes.md §1.
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
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe < 0) {
        continue;
      }
      ColonizeUnit* f = units_get(ctx->units, foe);
      if (!f || f->nation_id != target_euro || units_is_sea(ctx->units, foe)) {
        continue;
      }
      /* Snapshot gear before combat despawn (GAME.TXT @INDIANWIN1/@INDIANWIN2). */
      const int foe_muskets = f->muskets;
      const int foe_horses = f->horses;
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
      /* GAME.TXT @INDIANWIN0 / WIN1 / WIN2 / @INDIANLOSE thin ambush chrome. */
      if (ai_contact_euro_is_human(ctx, target_euro)) {
        char ambush_fb[AI_POPUP_BODY_LEN];
        if (brave_won) {
          if (seized_muskets) {
            snprintf(
              ambush_fb,
              sizeof(ambush_fb),
              "The %s ambush your units! Muskets seized by braves!",
              ai_contact_tribe_name(nation_id)
            );
          } else if (seized_horses) {
            snprintf(
              ambush_fb,
              sizeof(ambush_fb),
              "The %s ambush your units! Horses seized by braves!",
              ai_contact_tribe_name(nation_id)
            );
          } else {
            snprintf(
              ambush_fb,
              sizeof(ambush_fb),
              "The %s ambush your units!",
              ai_contact_tribe_name(nation_id)
            );
          }
        } else {
          snprintf(
            ambush_fb,
            sizeof(ambush_fb),
            "Your units defeat a %s ambush.",
            ai_contact_tribe_name(nation_id)
          );
        }
        ai_contact_human_chrome(
          ctx,
          target_euro,
          AI_POPUP_TAG_CONTACT_RAID,
          nation_id,
          "Raid",
          ambush_fb
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
      for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
        ColonizeColony* c = &ctx->colonies->colonies[ci];
        if (!c->active || c->nation_id != target_euro) {
          continue;
        }
        const int d = ai_contact_dist(brave->x, brave->y, c->x, c->y);
        if (d > 6) {
          continue;
        }
        /*
         * Prefer closer; at equal distance prefer muskets/horses (military
         * secondary), else tools≥10 (high-friction secondary −1), else higher
         * silver stock (GOLD-kind / wealth approach). Cite:
         * indian_raid_outcomes.md multi-loot / colony approach; @RAIDGOLD.
         */
        const int mil = ai_contact_colony_has_military_loot(c);
        const int tools = ai_contact_colony_has_tools_loot(c);
        const int gold_w = ai_contact_colony_gold_wealth(c);
        if (d < best_d || (d == best_d && mil && !best_mil) ||
            (d == best_d && mil == best_mil && tools && !best_tools) ||
            (d == best_d && mil == best_mil && tools == best_tools &&
             gold_w > best_gold)) {
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
          const AiRaidKind kind = ai_contact_pick_raid_kind(ctx, c, target_euro, max_alarm, rng);
          ai_contact_apply_raid_loot(ctx, c, target_euro, kind, max_alarm);
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
          for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
            if ((int)t->nation_id == nation_id) {
              t->alarm[target_euro].attacks++;
            }
          }
          /*
           * Successful raid friction/alarm escalate (fandom Alarm — raids raise
           * tension): tribe friction + alarm_by_player +2 each (cap 100).
           * Pocahontas halves bump (wiki/fandom half-rate). Cite:
           * docs/fandom_col1994.md Pocahontas / Alarm; indian_raid_outcomes.md.
           * Full 4528/2820 dialog PARKED; thin widgets Done (ai_popup).
           */
          if (kind != AI_RAID_NOTHING) {
            const int fr_bump =
              ai_contact_alarm_bump_amount(ctx->col1, target_euro, 2);
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
            const int host = (max_alarm >= 80) ? -5 : -3;
            ai_diplo_indian_relation_delta(ctx->col1, nation_id, target_euro, host);
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
            char raid_line[96];
            const char* raid_body = NULL;
            const char* tribe = ai_contact_tribe_name(nation_id);
            if (abandoned && abandoned_name[0]) {
              if (kind == AI_RAID_SCALP || kind == AI_RAID_BURN) {
                /* GAME.TXT @INDIANBURNCOLONY thin. */
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s burn %s to the ground!",
                  tribe,
                  abandoned_name
                );
              } else {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "The %s overrun %s!",
                  tribe,
                  abandoned_name
                );
              }
              raid_body = raid_line;
            } else if (kind == AI_RAID_NOTHING) {
              /* GAME.TXT @RAIDNOTHING: "{tribe} raiding party wiped out in {colony}!" */
              if (c->name[0]) {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party wiped out in %s!",
                  tribe,
                  c->name
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
            } else if (!was_at_war) {
              /* GAME.TXT @INDIANSURPRISE thin — deniable raid before open war. */
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
              /* GAME.TXT @RAIDSHIP thin. */
              if (c->name[0]) {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party attacks harbor in %s!",
                  tribe,
                  c->name
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
              /* GAME.TXT @RAIDSCALP thin (WINCOLONY when abandon handled above). */
              if (c->name[0]) {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party takes scalps in %s!",
                  tribe,
                  c->name
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
              /* GAME.TXT @RAIDGOLD thin. */
              if (c->name[0]) {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party seizes strongboxes in %s!",
                  tribe,
                  c->name
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
              snprintf(
                raid_line,
                sizeof(raid_line),
                "The %s burn your %s.",
                tribe,
                s_last_burn_building
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
              /* GAME.TXT @RAIDSTORES thin. */
              if (c->name[0]) {
                snprintf(
                  raid_line,
                  sizeof(raid_line),
                  "%s raiding party attacks stores in %s!",
                  tribe,
                  c->name
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
          }
        } else if (max_alarm >= 70) {
          /*
           * Approach march only in high-friction capture band (≥70). Mid gate
           * 40..69 keeps on-tile loot/combat but must not walk Braves — seed-100
           * TURN4→5 is already at-war for some tribes; approach broke the golden.
           * Cite: indian_raid_outcomes.md; smoke_ai_turns.
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
   * Meet CHOICE chain (FUN_5bfb_022e / 5bfb_102a stand-in): Trade / Gift /
   * Demand / Teach call existing thin handlers; Leave dismisses. Follow-up
   * OK popups enqueue from those handlers' human chrome.
   */
  if (popup->result_tag != AI_POPUP_TAG_CONTACT_MEET) {
    return;
  }
  int near_x = 0;
  int near_y = 0;
  ColonizeUnit* other = ai_contact_find_adjacent_euro(ctx, nation_id, e, &near_x, &near_y);
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
  case AI_CONTACT_CHOICE_TRADE:
    /*
     * Thin auto-trade (FUN_5bfb_022e / 2aac…311e). Success → "Trade accepted."
     * Alarmed / very-low relation → haggle refuse OK "The %s refuse to trade."
     * (2aac refuse arm stand-in; fandom Alarm). No goods otherwise → haggle
     * stub OK "Trade concluded." Deep buy/hard-bargain PARKED in FUN_4d56_2820.
     */
    {
      const int refuse_trade =
        ind->alarm_by_player[e] >= 50 ||
        ai_diplo_indian_relation(ctx->col1, nation_id, e) < 40;
      if (refuse_trade) {
        /* FUN_4d56_2af6 abort: clear tribe last-goods bookkeeping. */
        ai_contact_clear_tribe_last_goods(ctx, nation_id);
        {
          char refuse_fb[AI_POPUP_BODY_LEN];
          snprintf(
            refuse_fb,
            sizeof(refuse_fb),
            "The %s refuse to trade.",
            ai_contact_tribe_name(nation_id)
          );
          ai_contact_human_chrome(
            ctx,
            e,
            AI_POPUP_TAG_CONTACT_REFUSE,
            nation_id,
            "Trade",
            refuse_fb
          );
        }
        break;
      }
      if (!ai_contact_auto_trade(ctx, ind, nation_id, e, near_x, near_y)) {
        ai_contact_human_chrome(
          ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Trade", "Trade concluded."
        );
      }
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
    /* Follow-up OK from teach_skill human chrome (FUN_5bfb_022e teach arm). */
    ai_contact_teach_skill(ctx, nation_id);
    break;
  default:
    break;
  }
}
