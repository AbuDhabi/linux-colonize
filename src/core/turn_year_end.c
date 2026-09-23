#include "core/internal.h"
#include "core/popup_msg.h"
#include "core/turn.h"
#include "core/turn_internal.h"

#include "core/strutil.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "core/ai.h"
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/ai_king.h"
#include "core/col1_stuff_census.h"
#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/unit_chrome.h"
#include "core/woodcut.h"
#include "platform/diagnostics.h"


/*
 * Sections:
 *  - Year-end chrome: rival popups, anniversary, era-end, WoI, independence, defeat
 */

/* ===================== Year-end chrome: rival popups, anniversary, era-end, WoI, independence, defeat (turn_year_end_rival_rebels .. turn_run_year_end_chrome) ===================== */
COLONIZE_INTERNAL int turn_year_end_rival_rebels(const ColonizeCol1Save* col1, int rival) {
  if (!col1 || rival < 0 || rival >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  int v = (int)col1->nation[rival].rebel_sentiment *
          (int)col1->stuff.census_pop_proxy[rival] / 100;
  return v > 100 ? 100 : v;
}

/*
 * NAMES.TXT @INDEPENDENT (row = nation): the republic name DOS strcpy's over
 * the nation's country_name (DS nation*0x34+0x5426 = player[n].country_name)
 * when its King grants independence — raw 58598-58603.
 */
COLONIZE_INTERNAL const char* turn_year_end_independent_name(int nation) {
  /* Own per-nation buffer: reports_names_field hands back one shared scratch
   * string, and the caller fetches other names before it copies this one. */
  static char live[COLONIZE_COL1_NATION_COUNT][40];
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return "";
  }
  const char* field = reports_names_field("INDEPENDENT", nation, 0);
  snprintf(live[nation], sizeof(live[nation]), "%s", field ? field : "");
  return live[nation];
}

/* player[n].country_name (DS 0x5426) with NAMES.TXT @COUNTRY as fallback.
 * The private table this replaced said "Holland"; @COUNTRY row 3 is
 * "Netherlands" (audit theme D). */
COLONIZE_INTERNAL const char* turn_year_end_country_name(const ColonizeCol1Save* col1, int nation) {
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return "";
  }
  if (col1 && col1->player[nation].country_name[0]) {
    return col1->player[nation].country_name;
  }
  return reports_nation_country_name(nation);
}

/* player[n].name (DS 0x540e) with the nationality adjective as fallback. */
COLONIZE_INTERNAL const char* turn_year_end_leader_name(const ColonizeCol1Save* col1, int nation) {
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return "";
  }
  if (col1 && col1->player[nation].name[0]) {
    return col1->player[nation].name;
  }
  return reports_nation_adjective_display_name(nation);
}

/* §D popup: fill `tag` from GAME.TXT with DOS's substitutions, enqueue OK. */
COLONIZE_INTERNAL void turn_year_end_rival_popup(
  ColonizeTurnContext* ctx,
  const char* tag,
  const PopupMsgTokens* tok,
  const char* fallback
) {
  if (!ctx || !ctx->ai_popups) {
    return;
  }
  popup_chrome_ok(ctx->ai_popups, ctx->messages, tag, tok, fallback);
}

COLONIZE_INTERNAL void turn_year_end_anniversary(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, uint16_t year, int splash_done,
  int woi_latched
) {
  /* Section E anniversary (0x6fe=1790, 0x730=1840) — status only; gate 5382|0x10.
   * Spring-only: DOS wraps the block in `*(int *)0x538c == 0` (raw :58619).
   * `((year==0x6fe && !woi) || year==0x730)` is raw :58620 verbatim. */
  if (!splash_done && ((year == 0x6feu && !woi_latched) || year == 0x730u) &&
      ctx->status && ctx->status_size > 0 && !out->year_end_defeat &&
      !out->year_end_victory &&
      !(ctx->game_autumn && *ctx->game_autumn != 0)) {
    const int d =
      (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.difficulty : -1;
    /* NAMES.TXT @DIFFICULTY (audit theme D). */
    const char* dname = (d >= 0 && d <= 4) ? reports_difficulty_title(d) : NULL;
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
      popup_chrome_ok(
        ctx->ai_popups, ctx->messages, year == 0x6feu ? "WARN1" : "WARN2", NULL, ctx->status
      );
    }
  }
}

COLONIZE_INTERNAL void turn_year_end_era_end(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, uint16_t year, int splash_done,
  int woi_latched
) {
  /* Section E game-over years (0x708=1800, 0x73a=1850) — status; HoF PARKED.
   * raw :58630 `((0x538a == 0x708) && ((0x5382 & 1) == 0)) || (0x538a == 0x73a)`:
   * 1800 does NOT end a game in which independence has been declared, so the
   * era-end status and the calendar_latch below must not fire mid-war — that
   * latch is what game_loop.c's `WON && !calendar_latch` gate reads to decide
   * whether the win sequence still owes the player its closing chain. */
  if (!splash_done && ((year == 0x708u && !woi_latched) || year == 0x73au) &&
      ctx->status && ctx->status_size > 0 && !out->year_end_defeat &&
      !out->year_end_victory) {
    const int d =
      (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.difficulty : -1;
    /* NAMES.TXT @DIFFICULTY (audit theme D). */
    const char* dname = (d >= 0 && d <= 4) ? reports_difficulty_title(d) : NULL;
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
}

/* Returns true when this section already fired the WoI victory latch —
 * caller must return immediately, matching the original function's mid-body
 * `return;` at that point. */
COLONIZE_INTERNAL bool turn_year_end_woi_chrome(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, int woi_latched
) {
  const int woi = woi_latched;
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
         * Artillery) — classified by @UNIT kind (audit theme E) so
         * synthetic pools count right. units_name_kind resolves "Continental
         * Cavalry" to 0x07 before bare "Cavalry" can claim it, which is what
         * the old `!strstr(n, "Cont")` guard was for. */
        const ColonizeUnitKind k = units_type_kind(units_type(ctx->units, u->type_index));
        if (units_kind_is_royal(k) || k == UNITS_KIND_ARTILLERY) {
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
         * (bugs.md #259 — the game just carried on with a status line). */
        ctx->col1->head.turn_loop_running = 0; /* DS:0x53c2 clear */
      }
      if (ctx->status && ctx->status_size > 0) {
        snprintf(ctx->status, ctx->status_size, "Victory: independence won.");
      }
      return true;
    }

    /*
     * Section C2: WoI crown vs human SoL ratio (FUN year-end C2).
     * Was bells proxy; now pop-weighted colony SoL via ai_king_sol_percent.
     *
     * NOT gated on crown colonies. raw 58493 opens the C1 arm with
     * `(crown_colony_count == 0) || (0x5382 & 0x20)`; the C2 body (58507
     * onward — rebel-colony tally, SoL ratio, 0xf29/0xf39) sits after that
     * arm's closing brace, still inside `(0x5382&1) && !(0x5382&8)`. The port
     * used to require `crown_colonies > 0`, so a King wiped out on land but
     * still strong at sea (C1's fleet/REF gates unmet) emitted no chrome at
     * all. Still thin: DOS also raises the same two bands off the rebel-
     * colony tally (`colony flags +0x1c & 0x40` < 3 / == 0) and the human
     * colony count (< 3 / == 0), which the port does not carry.
     */
    if (ctx->status && ctx->status_size > 0 && ctx->col1_ok && ctx->col1) {
      const int human = ctx->human_nation;
      const int human_sol =
        (human >= 0 && human < 4) ? ai_king_sol_percent(ctx, human) : 0;
      const int crown_sol = ai_king_sol_percent(ctx, crown);
      /* raw 58522 verbatim: `((crown + 1) * 100) / (human + 1 + crown + 1)`.
       * Both operands carry their own +1 guard; the port dropped one of them,
       * which read a 50/50 board as 100 instead of 50. */
      const unsigned sol = ((unsigned)crown_sol + 1u) * 100u /
                           ((unsigned)human_sol + 1u + (unsigned)crown_sol + 1u);
      if (sol > 89u) {
        snprintf(ctx->status, ctx->status_size, "Crown peace offer.");
      } else if (sol > 79u) {
        snprintf(ctx->status, ctx->status_size, "Independence pressure.");
      }
    }
  }
  return false;
}

COLONIZE_INTERNAL void turn_year_end_rival_independence(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, int woi
) {
  /*
   * Section D — rival European nations winning their OWN independence
   * (viceroy_unpacked.c 58558-58617, inside thunked FUN_3844_0442).
   *
   * NOT a war declaration. The three DOS message ids resolve through the
   * VICEROY.EXE DS string table (EXE offset 121248+addr, the popup_tag_ids.md
   * method) to GAME.TXT tags: 0xf5e = @OTHERMIGHT (rising), 0xf69 =
   * @OTHERLESS (falling), and for the at/over-threshold branch 0xf51 =
   * @OTHERGRANTED — "The King of {%STRING0} grants {independence} to
   * %STRING1! %STRING2 elected first President of the new republic. %STRING3
   * makes peace with all european nations." Its four %STRING slots match the
   * four substitutions raw 58597-58603 loads there one for one, and the
   * closing line is exactly what the relation writes below do. The port used
   * to fire ai_diplo_declare_war + "Rival declares war." here: inverted.
   *
   * Loop: all four player slots, gated on DS `nation*0x34 + 0x543f` =
   * player+0x31 = `control != 0` (every non-human slot, withdrawn included) —
   * not the two head.rival_nation_slot_* cells, which are the King/WoI cache
   * (DS:0x53d4/0x53d6) and are never read or written by this section.
   *
   * Once-only latch: raw 58563 skips a nation whose nation_flags bit 0x04
   * ("has achieved independence from its King") is already up, so the branch
   * fires at most once per nation per game. Raw 58605 sets it.
   *
   * Hysteresis: below threshold, the rising band needs BOTH `v >= thresh−20`
   * (raw 58572 `local_6 + -0x14 <= iVar5`) and `v > cached` (58573); the
   * falling band needs `v < cached − 5` (58584). Both write the cache
   * (nation+0x1a = rebellion_pct_last_notified) back — without the bands a
   * one-point wobble re-fired a popup every single year.
   */
  if (!woi && ctx->col1_ok && ctx->col1 && !out->year_end_defeat &&
      !out->year_end_victory) {
    ColonizeCol1Save* dcol1 = ctx->col1;
    const int thresh = (8 - (int)dcol1->head.difficulty) * 10;
    for (int rival = 0; rival < (int)COLONIZE_COL1_NATION_COUNT; ++rival) {
      if (dcol1->player[rival].control == 0) {
        continue; /* DS 0x543f gate: human-controlled slot */
      }
      if ((dcol1->nation[rival].nation_flags & 0x04u) != 0) {
        continue; /* raw 58563 once-only latch */
      }
      const int v = turn_year_end_rival_rebels(dcol1, rival);
      const char* country = turn_year_end_country_name(dcol1, rival);
      const char* adj = reports_nation_adjective_display_name(rival);
      if (v < thresh) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = country;
        tok.string1 = adj;
        tok.number0 = v;
        tok.number1 = (int)dcol1->stuff.census_pop_proxy[rival];
        tok.number2 = thresh;
        tok.has_number0 = true;
        tok.has_number1 = true;
        tok.has_number2 = true;
        /* raw 58572: rising band — inside the top 20 points AND above cache. */
        if (v >= thresh - 20 &&
            v > (int)dcol1->nation[rival].rebellion_pct_last_notified) {
          if (ctx->status && ctx->status_size > 0) {
            snprintf(
              ctx->status,
              ctx->status_size,
              "The King of %s considers granting independence.",
              country
            );
          }
          turn_year_end_rival_popup(ctx, "OTHERMIGHT", &tok, ctx->status);
          dcol1->nation[rival].rebellion_pct_last_notified = (uint8_t)v;
        }
        /* raw 58584: falling band, re-reading the (possibly just written)
         * cache exactly as DOS does. */
        if (v < (int)dcol1->nation[rival].rebellion_pct_last_notified - 5) {
          if (ctx->status && ctx->status_size > 0) {
            snprintf(
              ctx->status,
              ctx->status_size,
              "Support for independence in %s is easing.",
              country
            );
          }
          turn_year_end_rival_popup(ctx, "OTHERLESS", &tok, ctx->status);
          dcol1->nation[rival].rebellion_pct_last_notified = (uint8_t)v;
        }
      } else {
        /* raw 58596-58611: @OTHERGRANTED. */
        char old_country[sizeof(dcol1->player[rival].country_name) + 1];
        snprintf(old_country, sizeof(old_country), "%s", country);
        const char* newname = turn_year_end_independent_name(rival);
        char leader[sizeof(dcol1->player[rival].name) + 1];
        snprintf(leader, sizeof(leader), "%s", turn_year_end_leader_name(dcol1, rival));
        /* raw 58602 strcpy: the nation is renamed to its republic name. */
        snprintf(
          dcol1->player[rival].country_name,
          sizeof(dcol1->player[rival].country_name),
          "%s",
          newname
        );
        dcol1->nation[rival].nation_flags |= 0x04u; /* raw 58605 */
        /* raw 58606-58611: peace with, and a cleared slate toward, every
         * other European nation — or-both 0x40 (PEACE), clear-both 0xbb
         * (WAR_INTENT|WAR|ALLY|AMICABLE|CROWN_ARMED|MET|TREASURE_ALERT). */
        for (int other = 0; other < (int)COLONIZE_COL1_NATION_COUNT; ++other) {
          if (other == rival) {
            continue;
          }
          ai_diplo_or_both(dcol1, rival, other, AI_DIPLO_PEACE);
          ai_diplo_clear_both(dcol1, rival, other, 0xbb);
        }
        PopupMsgTokens gtok;
        memset(&gtok, 0, sizeof(gtok));
        gtok.string0 = old_country;
        gtok.string1 = old_country;
        gtok.string2 = leader;
        gtok.string3 = newname;
        if (ctx->status && ctx->status_size > 0 && ctx->messages) {
          char status_body[AI_POPUP_BODY_LEN];
          popup_msg_fill(ctx->messages, "OTHERGRANTED", &gtok, "", status_body, sizeof(status_body));
          popup_msg_strip_markup(status_body);
          snprintf(ctx->status, ctx->status_size, "%s", status_body);
        }
        turn_year_end_rival_popup(ctx, "OTHERGRANTED", &gtok, "");
      }
    }
  }
}

COLONIZE_INTERNAL void turn_year_end_defeat_check(
  ColonizeTurnContext* ctx, ColonizeTurnResult* out, uint16_t year, int woi
) {
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
      const int d = (int)ctx->col1->head.difficulty;
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      /* NAMES.TXT @DIFFICULTY (audit theme D). */
      tok.string0 = (d >= 0 && d <= 4) ? reports_difficulty_title(d) : reports_difficulty_title(4);
      tok.string1 = (ctx->human_nation >= 0 && ctx->human_nation < 4)
        ? ctx->col1->player[ctx->human_nation].name
        : "";
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        ctx->messages, "LOSENOCOLONIES", &tok,
        "",
        body, sizeof(body)
      );
      if (ai_popup_enqueue_ok(ctx->ai_popups, AI_POPUP_TAG_INFO, NULL, body)) {
        /* DOS 3844:04e1 `PUSH 0xf09` / 3844:04e4 `CALLF 291f:0ad4` — the
         * King-flair message helper (FUN_6f74_378a latches DS:0x1f5c = 8),
         * so the dismissal audience stands KING.SS beside the scroll. */
        ai_popup_set_last_portrait(ctx->ai_popups, 8, 0);
      }
    }
  }
  if (ctx->status && ctx->status_size > 0) {
    snprintf(ctx->status, ctx->status_size, "Defeat: no colonies remain.");
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

  /* DOS 0x5382 bit0 = the WoI latch. raw :58620/:58630 gate the *early*
   * calendar pair (1790 warning / 1800 era end) on `(0x5382 & 1) == 0`: once
   * independence is declared the war extends the game to the late pair
   * (1840/1850), which carries no war gate. */
  const int woi_latched =
    ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi != 0;

  turn_year_end_anniversary(ctx, out, year, splash_done, woi_latched);
  turn_year_end_era_end(ctx, out, year, splash_done, woi_latched);
  if (turn_year_end_woi_chrome(ctx, out, woi_latched)) {
    return;
  }
  turn_year_end_rival_independence(ctx, out, woi_latched);
  turn_year_end_defeat_check(ctx, out, year, woi_latched);
}

