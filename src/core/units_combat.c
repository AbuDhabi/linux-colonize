#include "core/units.h"

/*
 * Defender selection, combat outcomes, popups, native fallout, LCR.
 *
 * Sections:
 *  - Defender selection, village temp-defenders & combat label helpers (units_foreign_scan_at .. units_type_is_royal_name)
 *  - Combat outcomes: promote/demote/capture, loss resolution, cargo holds, alarm venting (units_promote_prof_label .. units_indian_attack_alarm_vent)
 *  - Outcome popups, native fallout, LCR resolution & combat sound hooks (units_combat_outcome_popups .. units_combat_music_sting)
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

/* ===================== Defender selection, village temp-defenders & combat label helpers (units_foreign_scan_at .. units_type_is_royal_name) ===================== */


/*
 * First unit on (x,y) that is neither `mover_id` nor of `mover_nation`
 * (UN-11/UN-12). domain_filter -1 = any unit; 0/1 = land/sea hulls only, which
 * is the extra test units_domain_blocker_at needs.
 */
static int units_foreign_scan_at(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int mover_id,
  int mover_nation,
  int domain_filter
) {
  int slot = 0;
  for (const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot); u != NULL;
       u = units_next_on_tile_const(pool, x, y, &slot)) {
    if (u->id == mover_id) {
      continue;
    }
    if (mover_nation >= 0 && u->nation_id == mover_nation) {
      continue;
    }
    if (domain_filter >= 0 && (units_is_sea(pool, u->id) ? 1 : 0) != domain_filter) {
      continue; /* other domain — FUN_5fef_0000 never sees it */
    }
    return u->id;
  }
  return -1;
}

int units_foreign_unit_at(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int except_unit_id,
  int except_nation_id
) {
  return units_foreign_scan_at(pool, x, y, except_unit_id, except_nation_id, -1);
}

/*
 * "Is this tile still contested for `mover`?" under DOS's own definition.
 *
 * FUN_5fef_1b0e has no unit-at-tile test at all: the only notion of a
 * contested tile it carries is whether FUN_5fef_0000 handed it a defender
 * (`bVar28`), and that picker is domain-gated (raw 99186-99195) — a
 * candidate whose ship-ness (`0xd <= type && type <= 0x12`, raw 99187-99192)
 * differs from `local_c` is skipped outright. `local_c` is
 * `FUN_281f_0768(x, y)` = ocean_or_high_seas of the SCANNED TILE, taken from
 * `param_2`'s position (raw 99137-99148), and `param_2` is the first unit on
 * the target tile — the caller passes `uVar17 = FUN_281f_07e0(...)` at raw
 * 100353-100354, not the attacker. So a foreign hull sitting in a port is
 * invisible to a land assault at every stage: the tile is land, the hull is
 * a ship, so it never defends, it never blocks the walk-in, and the capture
 * arm (raw 100905-101034) never touches it — no sink, no seizure, no owner
 * flip. It simply stays where it is, under its old flag, inside the town
 * that just changed hands.
 *
 * The port's plain units_foreign_unit_at could not express that: an armed hull
 * (attack > 0, so units_seize_noncombat_at leaves it) held the tile
 * "contested" forever and no land force could ever take the port.
 *
 * The domain is read off the TARGET TILE, exactly as units_best_defender_at
 * does — see the tile_domain derivation there for the full asm quote and for
 * why the ATTACKER's own ship-ness is the wrong reading (they diverge for a
 * warship attacking out of a colony berth: a sea unit on a land tile).
 * `mover_id` stays for the self-skip and for the headless fallback.
 */
int units_domain_blocker_at(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int mover_id,
  int mover_nation
) {
  if (!pool) {
    return -1;
  }
  /* Same map resolution as units_combat_strength_ctx. Headless fixtures wire
   * neither map; there the old attacker-ship-ness proxy stands in. */
  const ColonizeWorldMap* map = units_occupancy_map ? units_occupancy_map : g_units_fallout_map;
  const int tile_domain =
    map ? ((map_tile_is_water(map, x, y) || map_tile_is_high_seas(map, x, y)) ? 1 : 0)
        : (units_is_sea(pool, mover_id) ? 1 : 0);
  return units_foreign_scan_at(pool, x, y, mover_id, mover_nation, tile_domain);
}

/*
 * FUN_5fef_0000: walk stack for highest engagement toughness vs attacker.
 * Artillery vs Indian attacker ×2; skip type.attack==0.
 */
int units_best_defender_at(
  const ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  int x,
  int y,
  int attacker_id,
  int except_id
) {
  if (!pool) {
    return -1;
  }
  const ColonizeUnit* atk = units_get_const(pool, attacker_id);
  const int atk_nat = atk ? atk->nation_id : -1;
  ColonizeCombatStrengthCtx sctx = units_combat_strength_ctx(col1);
  sctx.units = pool;

  /*
   * bugs.md #677 (Colonist audit): DOS FUN_5fef_0000 (OVL17 asm 0x0091-0x0166)
   * has ONE ranking, not an armed/unarmed two-tier:
   *   score = ((015e(cand, attacker) & 0xff) << 8) - 004a(cand, 0) + 0xff
   * The attack-byte skip (asm 0x0091-0x00b5) runs only when local_6 != 0 =
   * FUN_281f_0696 Euro-settlement owner >= 0 (a colony on the tile). On open
   * ground an attack-0 unit is a full-rank candidate, so a Fortified Colonist
   * outranks an unfortified Scout. Ties update on `>=` (JNC at 0x0152): the
   * last candidate in bucket order wins.
   */
  int best_id = -1;
  unsigned best_score = 0;
  const int on_colony_tile = g_units_combat_colonies &&
                             colonies_id_at(g_units_combat_colonies, x, y) >= 0;
  /*
   * DOS FUN_5fef_0000 domain gate, verbatim (viceroy_unpacked.c 99137-99147
   * + 99186-99195):
   *
   *   if (-1 < param_2) {
   *     uVar1 = unit[param_2].x; uVar6 = unit[param_2].y;
   *     local_6 = (FUN_281f_0696(uVar1,uVar6) >= 0);        // colony there?
   *     if (FUN_281f_0302(uVar1,uVar6))                     // tile in bounds
   *       { local_c = FUN_281f_0768(uVar1,uVar6); bVar2 = true; }
   *   }
   *   ...
   *   if (bVar2) { local_16 = (0xd <= cand.type && cand.type <= 0x12);
   *                if (local_c != local_16) skip; }
   *
   * `param_2` is NOT the attacker — the attacker is `param_3` (the artillery
   * arm reads its nation nibble, and FUN_281f_09dc scores each candidate
   * against it). `param_2` comes from the caller as
   * `uVar17 = FUN_281f_07e0(...); FUN_5fef_0000(uVar17, ...)` at raw
   * 100353-100354: the first unit standing on the TARGET tile. That is the
   * same tile `local_6` reads for the artillery colony bonus below, which
   * this port already resolves as `colonies_id_at(colonies, x, y)`.
   *
   * So the gate is `FUN_281f_0768(x, y)` = ocean_or_high_seas of the SCANNED
   * TILE (SYMBOL_MAP.md): a water tile is defended by ships only, a land tile
   * by land units only. A hull moored in a harbour therefore never defends
   * the town (the D1 REF wedge), and a garrison never answers a naval fight.
   *
   * The port tested the ATTACKER's own ship-ness instead (smell audit
   * 2026-09-09 #13). That agrees with DOS whenever the attacker's domain
   * matches its tile — i.e. almost always — but diverged for a warship
   * attacking out of a colony berth: it is a sea unit standing on a land
   * tile, so the port let it engage only ships while DOS engages whatever
   * matches the TARGET tile.
   *
   * When no map is wired (headless fixtures) fall back to the old
   * attacker-ship-ness proxy — DOS's `bVar2` is false only for an absent
   * `param_2`, never for an absent world.
   */
  int tile_domain = -1;
  if (atk) {
    if (sctx.map) {
      tile_domain =
        (map_tile_is_water(sctx.map, x, y) || map_tile_is_high_seas(sctx.map, x, y)) ? 1 : 0;
    } else {
      tile_domain = units_is_sea(pool, attacker_id) ? 1 : 0;
    }
  }
  int slot_1 = 0;
  for (const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot_1); u != NULL;
       u = units_next_on_tile_const(pool, x, y, &slot_1)) {
    if (u->id == except_id || u->id == attacker_id) {
      continue;
    }
    if (atk_nat >= 0 && u->nation_id == atk_nat) {
      continue;
    }
    /* Domain gate — see the tile_domain derivation above. */
    if (tile_domain >= 0 && (units_is_sea(pool, u->id) ? 1 : 0) != tile_domain) {
      continue;
    }
    /*
     * Colony-tile attack-0 skip (asm 0x009a-0x00b5): DOS reads the @UNIT
     * attack byte; the port's body/kit model spells that as
     * units_is_combat_role (type attack > 0 OR carried muskets/horses).
     */
    if (on_colony_tile && !units_is_combat_role(pool, u)) {
      continue;
    }
    const int base = combat_unit_base_x8(&sctx, u->id, 0, NULL);
    const int eng = combat_engagement_strength(&sctx, u->id, attacker_id, NULL);
    /* asm 0x00dd-0x00e5: MOV AH,AL ; SUB AL,AL ; SUB AX,SI ; ADD AX,0xff */
    int score = (int)(((unsigned)eng & 0xffu) << 8) - base + 0xff;
    const ColonizeUnitType* t = units_type(pool, u->type_index);
    /*
     * DOS FUN_5fef_0000 artillery arm (OVL17 asm 0x0f5..0x11c). The gate is
     * `FUN_1000_8886(tile) >= 0` = FUN_281f_0696, EURO-settlement-owner —
     * a colony, and deliberately NOT the layer2 settlement bit the 1b0e
     * strength clauses use, so a native village counts as open ground here:
     *
     *   type 0x0b and a Euro colony on the tile → x2, but only against a
     *   native attacker (asm reads the ATTACKER's nation nibble < 4 → skip);
     *   type 0x0b anywhere else, unless the artillery is Fortify/Fortified
     *   → score >>= 3, so a gun in the open never outranks the dragoons
     *   standing next to it.
     *
     * The port had the x2 with no colony gate and no >>3 at all (bugs.md #328
     * described both but only the pick-order rewrite landed), which made
     * open-field artillery the pick against Indians by a factor of 16.
     */
    if (t && combat_type_is_artillery(t)) {
      const int on_colony = g_units_combat_colonies &&
                            colonies_id_at(g_units_combat_colonies, x, y) >= 0;
      if (on_colony) {
        if (atk_nat > 3) {
          score *= 2;
        }
      } else if (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED) {
        score >>= 3;
      }
    }
    /* asm 0x014c-0x0155: unsigned 16-bit CMP, update on >= (last tie wins). */
    const unsigned uscore = (unsigned)score & 0xffffu;
    if (uscore >= best_score) {
      best_score = uscore;
      best_id = u->id;
    }
  }
  /* -1 on a colony tile = no armed defender: the town fights through its
   * militia / Paul Revere auto-arm (units_revere_defend_colony_tile). */
  return best_id;
}

int units_spawn_village_temp_defender(
  ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  int village_x,
  int village_y,
  int indian_nation,
  int attacker_id
) {
  if (!pool || !col1 || indian_nation < 4 || indian_nation > 11) {
    return -1;
  }
  /* Real map foe already on the settlement — fight them; no phantom. */
  if (units_best_defender_at(pool, col1, village_x, village_y, attacker_id, attacker_id) >= 0) {
    return -1;
  }

  int tribe_index = -1;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if ((int)t->x == village_x && (int)t->y == village_y &&
          (int)t->nation_id == indian_nation) {
        tribe_index = (int)ti;
        break;
      }
    }
  }
  if (tribe_index < 0) {
    return -1;
  }

  /*
   * FUN_5fef_1b0e empty-village arm: type 0x13 Brave; muskets→0x14 Armed;
   * horse_breeding > 0x18 → +2 (Mtd. Braves / Mtd. Warriors).
   */
  const ColonizeCol1Indian* ind = &col1->indian[indian_nation - 4];
  ColonizeUnitKind spawn_kind = UNITS_KIND_BRAVE;
  if (ind->muskets != 0 && ind->horse_breeding > 0x18) {
    spawn_kind = UNITS_KIND_MTD_WARRIOR;
  } else if (ind->muskets != 0) {
    spawn_kind = UNITS_KIND_ARMED_BRAVE;
  } else if (ind->horse_breeding > 0x18) {
    spawn_kind = UNITS_KIND_MTD_BRAVE;
  }
  int ti = units_kind_type_index(pool, spawn_kind);
  if (ti < 0) {
    ti = units_kind_type_index(pool, UNITS_KIND_BRAVE);
  }
  if (ti < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(pool, ti, village_x, village_y);
  ColonizeUnit* u = units_get(pool, id);
  if (!u) {
    return -1;
  }
  u->nation_id = indian_nation;
  u->home_tribe_id = tribe_index;
  /* The nation_id just written is native, so moves is the DOS SPENT byte
   * here: a raw 0 would mean "full allotment", not "cannot act". The phantom
   * must never be able to move (smell audit 2026-09-09 #7). */
  units_mp_exhaust(pool, u);
  return id;
}

void units_finish_village_temp_defender(
  ColonizeUnitPool* pool,
  ColonizeCol1Save* col1,
  ColonizeWorldMap* map,
  int temp_id,
  int attacker_won,
  int attacker_nation,
  int village_x,
  int village_y,
  ColonizeDosRng* rng
) {
  if (!pool || temp_id < 0) {
    return;
  }
  /* FUN_291f_0a06-shaped: always undo the phantom if it survived the roll. */
  ColonizeUnit* temp = units_get(pool, temp_id);
  if (temp && temp->active) {
    units_despawn(pool, temp_id);
  }
  if (!attacker_won || !col1 || !col1->tribe) {
    return;
  }
  ColonizeCol1Tribe* tribe = NULL;
  int indian_nation = -1;
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->x == village_x && (int)t->y == village_y && t->nation_id >= 4 &&
        t->nation_id <= 11) {
      tribe = t;
      indian_nation = (int)t->nation_id;
      break;
    }
  }
  if (!tribe || indian_nation < 4) {
    return;
  }
  /*
   * FUN_5fef_1b0e: if population < 2 → destroy dwelling; else population--.
   * Cite: *(tribe+4) check before DEC; FUN_291f_0248 destroy; 31ea fallout.
   */
  if (tribe->population < 2) {
    (void)units_try_native_settlement_fallout_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .map=(ColonizeWorldMap*)(map), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL), .rng=(ColonizeDosRng*)(rng)}, attacker_nation, indian_nation, village_x, village_y, -1);
  } else {
    tribe->population--;
  }
}

int units_tribe_nation_at(const ColonizeCol1Save* col1, int x, int y) {
  if (!col1 || !col1->tribe) {
    return -1;
  }
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->x == x && (int)t->y == y && t->nation_id >= 4 && t->nation_id <= 11) {
      return (int)t->nation_id;
    }
  }
  return -1;
}

int units_combat_human_involved(const ColonizeCol1Save* col1, int nat_a, int nat_b) {
  if (!col1) {
    return g_units_combat_human_nation >= 0 &&
           (nat_a == g_units_combat_human_nation || nat_b == g_units_combat_human_nation);
  }
  const int a_h =
    (nat_a >= 0 && nat_a <= 3 && col1->player[nat_a].control == 0) ||
    (g_units_combat_human_nation >= 0 && nat_a == g_units_combat_human_nation);
  const int b_h =
    (nat_b >= 0 && nat_b <= 3 && col1->player[nat_b].control == 0) ||
    (g_units_combat_human_nation >= 0 && nat_b == g_units_combat_human_nation);
  return a_h || b_h;
}

const char* units_combat_nation_label(const ColonizeCol1Save* col1, int nation_id) {
  if (nation_id >= 0 && nation_id <= 3) {
    /* bugs.md: during the WoI the crown's borrowed slot is the King's side —
     * the proper adjective is "Tory" ("Tory Cavalry"), not the peer nation
     * whose slot it wears and not "Royal". */
    if (nation_id == unit_chrome_crown_nation()) {
      /* LABELS.TXT @MISC row 70. Own static buffer (not
       * reports_misc_display_word's shared one): a Tory-vs-Rebels combat
       * fills tok.string0/string1 from two calls into this function before
       * either is read, and a shared buffer would alias them both to
       * whichever word resolved last. */
      static char tory_buf[32];
      str_copy_trunc(tory_buf, sizeof(tory_buf), reports_misc_display_word(70, ""));
      return tory_buf;
    }
    /* bugs.md #239: under the WoI the player faction is "Rebels" (LABELS.TXT
     * @MISC row 86), never "United Colonies" or the old country name. */
    if (col1 && col1->head.game_options.woi &&
        (col1->player[nation_id].control == 0 ||
         nation_id == g_units_combat_human_nation)) {
      /* LABELS.TXT @MISC row 86. Own static buffer, same aliasing reason. */
      static char rebels_buf[32];
      str_copy_trunc(rebels_buf, sizeof(rebels_buf), reports_misc_display_word(86, ""));
      return rebels_buf;
    }
    /*
     * bugs.md ("New Spain Privateer" should be "Spanish Privateer"): DOS
     * substitutes the @NATIONALITY adjective here, not the player's
     * @COLONYNAME. country_name is the new-world colony region and belongs
     * in colony/diplomacy text, never on a unit.
     */
    if (g_units_nationality[nation_id][0]) {
      return g_units_nationality[nation_id];
    }
    /* Theme D: the local {"English",...} fallback table is now the shared
     * NAMES @NATIONALITY accessor. */
    return reports_nation_adjective_display_name(nation_id);
  }
  if (nation_id >= 4 && nation_id <= 11) {
    /* @TRIBES column 1 is the singular ("Inca"), which is what a unit label
     * wants — column 0 is the plural. */
    return reports_tribe_singular_name(nation_id - 4);
  }
  return "enemy";
}

/* Colony name, else village tribe, else LABELS "Wilderness". */
static const char* units_combat_place_label(
  const ColonizeCol1Save* col1,
  int x,
  int y
) {
  if (g_units_combat_colonies) {
    const int cid = colonies_id_at(g_units_combat_colonies, x, y);
    const ColonizeColony* col = colonies_get(g_units_combat_colonies, cid);
    if (col && col->active && col->name[0]) {
      return col->name;
    }
  }
  if (col1 && col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if ((int)t->x == x && (int)t->y == y && t->nation_id >= 4 && t->nation_id <= 11) {
        return units_combat_nation_label(col1, (int)t->nation_id);
      }
    }
  }
  /* bugs.md: a fight right outside a town is "near <colony>", not "near
   * Wilderness" — pick the closest colony within 3 tiles (Chebyshev). */
  if (g_units_combat_colonies) {
    const ColonizeColony* best = NULL;
    int best_d = 4;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &g_units_combat_colonies->colonies[i];
      if (!c->active || !c->name[0]) {
        continue;
      }
      const int dx = c->x > x ? c->x - x : x - c->x;
      const int dy = c->y > y ? c->y - y : y - c->y;
      const int d = dx > dy ? dx : dy;
      if (d < best_d) {
        best_d = d;
        best = c;
      }
    }
    if (best) {
      return best->name;
    }
  }
  /* LABELS.TXT @MISC row 17. */
  return reports_misc_display_word(17, "");
}

static const char* units_combat_unit_label(
  const ColonizeUnitPool* pool,
  const ColonizeUnit* u
) {
  if (!u) {
    return "unit";
  }
  const ColonizeUnitType* t = units_type(pool, u->type_index);
  if (t && t->name[0]) {
    return t->name;
  }
  return "unit";
}

/*
 * LABELS.TXT "defeat" / "defeats". Nation subjects use "defeat"; unit subjects
 * with type_index ≥ 7 (Cont. Cav.+) use "defeats" (FUN_5fef_1b0e).
 */
static const char* units_combat_defeat_verb(int subject_is_nation_only, int unit_type_index) {
  /* LABELS.TXT @MISC rows 73 / 74. */
  if (subject_is_nation_only) {
    return reports_misc_display_word(73, "");
  }
  return (unit_type_index >= 0 && unit_type_index < 7)
    ? reports_misc_display_word(73, "")
    : reports_misc_display_word(74, "");
}

void units_combat_enqueue_tok(
  AiPopupTag tag,
  const char* section,
  int nation_a,
  int nation_b,
  int payload,
  const PopupMsgTokens* tok,
  const char* fallback
) {
  if (!g_units_combat_popups || !section) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens local;
  memset(&local, 0, sizeof(local));
  if (tok) {
    local = *tok;
  }
  if (g_units_combat_game_txt) {
    popup_msg_fill(g_units_combat_game_txt, section, &local, fallback, body, sizeof(body));
  } else {
    snprintf(body, sizeof(body), "%s", fallback ? fallback : section);
  }
  (void)ai_popup_enqueue_ok_ctx(
    g_units_combat_popups, tag, nation_a, nation_b, payload, NULL, body
  );
}

void units_combat_notify_colony_captured(
  const ColonizeCol1Save* col1,
  const ColonizeColony* colony,
  int capturer_nation,
  int plunder_gold
) {
  if (!colony || !colony->active) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = units_combat_nation_label(col1, capturer_nation);
  tok.string2 = colony->name[0] ? colony->name : "colony";
  /* Raw 101015-101029 (FUN_5fef_1b0e capture tail): DOS picks the tag by
   * whether EITHER side is human-controlled (control byte 0), then the WoI
   * bit — @CAPTURED (with plunder) at peace, @CAPTURED3 at war (plunder is
   * skipped during WoI, raw 100984); an AI-vs-AI capture gets the
   * spies-report @CAPTURED2. The old plunder>0 / AI-capturer
   * discriminators were port inventions. */
  if (!units_combat_human_involved(col1, capturer_nation, colony->nation_id)) {
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_COLONY,
      "CAPTURED2",
      capturer_nation,
      colony->nation_id,
      0,
      &tok,
      "");
  } else if (col1 && col1->head.game_options.woi != 0) {
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_COLONY,
      "CAPTURED3",
      capturer_nation,
      colony->nation_id,
      0,
      &tok,
      "");
  } else {
    tok.number0 = plunder_gold;
    tok.has_number0 = true;
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_COLONY,
      "CAPTURED",
      capturer_nation,
      colony->nation_id,
      plunder_gold,
      &tok,
      "");
  }
}

/*
 * Shared token fill + enqueue of the two colony-burned notices (UN-46): the
 * burner label, the victim's adjective and the colony name go into the same
 * three slots either way; the callers differ only in tag, section and
 * audience.
 */
static void units_combat_burn_chrome(
  const ColonizeCol1Save* col1,
  AiPopupTag tag,
  const char* section,
  int nation_a,
  int nation_b,
  const char* burner_label,
  int victim_nation,
  const char* colony_name,
  const char* fallback
) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = burner_label && burner_label[0] ? burner_label : "";
  tok.string1 = units_combat_nation_label(col1, victim_nation);
  tok.string3 = colony_name;
  units_combat_enqueue_tok(tag, section, nation_a, nation_b, 0, &tok, fallback);
}

void units_combat_notify_colony_burned(
  const ColonizeCol1Save* col1,
  const char* colony_name,
  int victim_nation,
  const char* burner_label
) {
  if (!colony_name || !colony_name[0]) {
    return;
  }
  const int human =
    (victim_nation >= 0 && victim_nation <= 3 && col1 &&
     col1->player[victim_nation].control == 0) ||
    (g_units_combat_human_nation >= 0 && victim_nation == g_units_combat_human_nation);
  if (!human) {
    return;
  }
  /* FUN_5fef_1b0e 5fef:3040-3063 gates the burn cue on the human victim. */
  units_play_event_sound(0x53);
  /*
   * DOS FUN_5fef_1b0e splits the colony-destroyed chrome by attacker class:
   * the both-Euro arm (`uVar16 < 4 && uVar15 < 4`) uses @BURNED / @BURNED2 /
   * @BURNED3 (0x1c28/0x1c2f/0x1c37), while the native arm (`3 < uVar16 &&
   * uVar15 < 4`) uses @INDIANBURNCOLONY (0x1c65, human victim, + woodcut 11
   * via FUN_281f_0524(0xb)) / @INDIANBURNCOLONY2 (0x1c76, bystander). Both
   * callers here are native burners (units.c last-colonist kill, ai_contact
   * SCALP/BURN raid abandon) — cite ai/indian_raid_outcomes.md §6.
   */
  units_combat_burn_chrome(
    col1, AI_POPUP_TAG_COMBAT_COLONY, "INDIANBURNCOLONY", victim_nation, -1, burner_label,
    victim_nation, colony_name, ""
  );
}

void units_combat_notify_colony_burned_foreign(
  const ColonizeCol1Save* col1,
  const char* colony_name,
  int victim_nation,
  const char* burner_label
) {
  if (!colony_name || !colony_name[0]) {
    return;
  }
  /* Bystander only: a human nation exists and it is not the victim. The
   * victim already gets @INDIANBURNCOLONY (units_combat_notify_colony_burned);
   * the burner here is always a native tribe (id ≥4), never a euro nation —
   * so DOS's native arm tag 0x1c76 @INDIANBURNCOLONY2, not @BURNED3. */
  if (g_units_combat_human_nation < 0 || g_units_combat_human_nation == victim_nation) {
    return;
  }
  units_combat_burn_chrome(
    col1, AI_POPUP_TAG_INFO, "INDIANBURNCOLONY2", g_units_combat_human_nation, victim_nation,
    burner_label, victim_nation, colony_name, ""
  );
}

/*
 * bugs.md: royal (REF) types never change class. "Regulars"/"Cavalry" are the
 * King's units — a Cavalry winner must not promote into the rebel "Cont. Cav."
 * type (that chain ended with defeated Regulars demoting into capturable
 * Colonists), and defeated Regulars are destroyed, not demoted.
 */
/* bugs.md #581: the class comes from the @UNIT ROW (kind_plus1), never from
 * the English name — units_name_kind has no resolver in production, so the
 * name spelling read UNKNOWN and this guard was dead. */
/* ===================== Combat outcomes: promote/demote/capture, loss resolution, cargo holds, alarm venting (units_promote_prof_label .. units_indian_attack_alarm_vent) ===================== */


/*
 * bugs.md #248 — full FUN_5fef_172c + FUN_5fef_16ea port (combat promotion).
 * Eligibility: winner is an armed colonial soldier/dragoon body (DOS types
 * 1/4; royal Regulars/Cavalry never promote). Ladder (16ea, by @JOB id):
 *   Petty Criminal → Indentured Servant → Free Colonist (job NONE) →
 *   Veteran Soldiers; Converts never promote; expert skills never promote.
 *   Veteran (WoI + mobilization bit only): type → Cont. Army / Cont. Cav.,
 *   profession stays Veteran.
 * Odds (172c): Washington (FF 0xb) always promotes; else
 *   roll(1, atk+def ± difficulty) <= loser_strength — the pool shrinks 10
 *   for Criminals, 5 for Servants (human gets +difficulty, AI −).
 * Popups (@CONTINENTAL / @VETERAN / @VALOR) for the human's own unit.
 */
/* @JOB profession label for the @VALOR ladder (DOS FUN_281f_0c40). */
static const char* units_promote_prof_label(int prof) {
  switch (prof) {
  case UNITS_JOB_CRIMINAL:
  case UNITS_JOB_SERVANT:
  case UNITS_JOB_SOLDIER:
  case UNITS_JOB_DRAGOON:
    return reports_job_display_name(prof);
  case UNITS_JOB_COLONIST:
  case UNITS_JOB_NONE:
  default:
    return reports_job_display_name(UNITS_JOB_COLONIST);
  }
}

int units_promote_on_win(
  ColonizeUnitPool* pool,
  ColonizeUnit* winner,
  const ColonizeCol1Save* col1,
  int winner_str,
  int loser_str,
  ColonizeDosRng* rng
) {
  if (!pool || !winner || !winner->active) {
    return 0;
  }
  if (winner->nation_id < 0 || winner->nation_id > 3) {
    return 0; /* DOS: promotions are for Euro nations (natives never) */
  }
  const ColonizeUnitType* ut = units_type(pool, winner->type_index);
  const ColonizeUnitKind wk = units_type_kind(ut);
  if (units_kind_is_royal(wk)) {
    return 0; /* REF units never promote into colonial types */
  }
  if (units_kind_is_continental(wk)) {
    return 0; /* already at the top of the ladder */
  }
  /* DOS type 1/4 gate: a soldier or dragoon body. The port also stores an
   * armed colonist as a Colonists-type with muskets — same thing in DOS. */
  const int is_dragoon_body =
    wk == UNITS_KIND_DRAGOON || (winner->muskets > 0 && winner->horses > 0);
  const int is_soldier_body =
    !is_dragoon_body && (wk == UNITS_KIND_SOLDIER || winner->muskets > 0);
  if (!is_soldier_body && !is_dragoon_body) {
    return 0; /* unarmed colonists never combat-promote */
  }
  const int prof = winner->profession;
  /* DOS-LITERAL FUN_5fef_172c raw 100076: the veteran test is `+0x315b ==
   * 0x15` only. 0x17 is never written to a unit by DOS (bugs.md #503). */
  const int is_veteran = (prof == UNITS_JOB_SOLDIER);
  int next_prof = -2; /* -2 = ineligible; -1 = Continental type promote */
  if (is_veteran) {
    /* 172c raw 100077-100082: veteran promotes only once independence is
     * declared (DS:0x5382 bit 1) and the mobilization flag is set. DOS reads
     * that flag from `nation[*(int*)0x5398]` — the HUMAN player's record
     * (`head.human_player`), not the winner's own nation (bugs.md #504). */
    const int mob_nation = col1 ? (int)col1->head.human_player : -1;
    if (!col1 || !col1->head.game_options.woi || mob_nation < 0 || mob_nation > 3 ||
        (col1->nation[mob_nation].nation_flags & 0x08u) == 0) {
      return 0;
    }
    next_prof = -1;
  } else if (prof == UNITS_JOB_CRIMINAL) {
    next_prof = UNITS_JOB_SERVANT;
  } else if (prof == UNITS_JOB_SERVANT) {
    next_prof = UNITS_JOB_NONE; /* Free Colonist */
  } else if (prof == UNITS_JOB_COLONIST || prof == UNITS_JOB_NONE) {
    next_prof = UNITS_JOB_SOLDIER; /* DOS 0x15 for soldier AND dragoon bodies */
  } else if (prof == UNITS_JOB_CONVERT) {
    /* DOS-LITERAL FUN_5fef_172c raw 100085-100104: the eligibility gate is
     * FUN_281f_0c9a (= FUN_15eb_0002 raw 9298-9307), which returns 0 for
     * 0x13/0x19/0x1a/0x1b/0x1c — so a Convert (0x1b) FALLS THROUGH into the
     * Washington test and, absent Washington, draws FUN_281f_04d4(1, local_6)
     * off the shared stream. Only then does the ladder FUN_5fef_16ea raw
     * 100040-100059 map 0x1b -> 0x1b, so `iVar3 != iVar2` fails and nothing
     * is promoted. Outcome is a no-op; the RNG draw is not. (bugs.md #598) */
    next_prof = UNITS_JOB_CONVERT;
  } else {
    return 0; /* experts / everything else: FUN_15eb_0002 returns 1, early out */
  }
  /* Odds. */
  const int human = (g_units_combat_human_nation >= 0 &&
                     winner->nation_id == g_units_combat_human_nation);
  const int difficulty = col1 ? (int)col1->head.difficulty : 2;
  int pool_n = winner_str + loser_str + (human ? difficulty : -difficulty);
  if (prof == UNITS_JOB_CRIMINAL) {
    pool_n -= 10;
  }
  if (prof == UNITS_JOB_SERVANT) {
    pool_n -= 5;
  }
  const int washington =
    col1 && founding_fathers_nation_has(col1, winner->nation_id, FF_GEORGE_WASHINGTON);
  if (!washington) {
    if (!rng) {
      return 0;
    }
    /* DOS-LITERAL FUN_5fef_172c raw 100104: `FUN_281f_04d4(1, pool_n)` with no
     * clamp. `04d4` → FUN_19ef_0032 computes `((hi-lo+1) * rand()) >> 15 + lo`
     * with a SIGNED span, so a non-positive span still draws off the shared
     * stream and yields <= lo. dos_rng_range() short-circuits `hi < lo` to lo
     * WITHOUT drawing, so spell the DOS formula out here rather than clamp
     * pool_n to 1 (that clamp was invented — bugs.md #504).
     * Arg order confirmed against the asm at the FUN_5fef_1b0e call site
     * (viceroy_unpacked.asm 5fef:284e-2856, cdecl last-arg-first: PUSH
     * local_92 = attacker/loser strength, PUSH local_a8 = winner strength,
     * PUSH local_c8 = the winning unit), so param_3 is the LOSER strength and
     * the roll compares against it. */
    int roll;
    if (pool_n >= 1) {
      roll = dos_rng_range(rng, 1, pool_n);
    } else {
      roll = 1 + (int)(((int32_t)pool_n * (int32_t)dos_rng_next(rng)) >> 15);
    }
    if (roll > loser_str) {
      return 0;
    }
  }
  /* 172c raw 100111: ladder(0x1b) == 0x1b, so the `iVar3 != iVar2` guard
   * fails and the Convert keeps its profession — after the draw above. */
  if (next_prof == UNITS_JOB_CONVERT) {
    return 0;
  }
  const ColonizeUnitType* old_ty = ut;
  const int old_prof = winner->profession;
  const char* popup_tag = NULL;
  if (next_prof == -1) {
    /* DOS-LITERAL FUN_5fef_172c raw 100112-100118: before the Continental type
     * swap DOS re-reads the unit's nation nibble and bails when it is > 3 or
     * the slot is not player-controlled (`(n&0xf)*0x34 + 0x543f != 0`) — REF /
     * AI-run nations keep their Veteran professions (bugs.md #504). */
    if (winner->nation_id > 3 ||
        (col1 && col1->player[winner->nation_id].control != 0)) {
      return 0;
    }
    int tgt = is_dragoon_body ? units_kind_type_index(pool, UNITS_KIND_CONT_CAV)
                              : units_kind_type_index(pool, UNITS_KIND_CONT_ARMY);
    if (tgt < 0) {
      tgt = is_dragoon_body ? units_kind_type_index(pool, UNITS_KIND_CONT_CAV)
                            : units_kind_type_index(pool, UNITS_KIND_CONT_ARMY);
    }
    if (tgt < 0) {
      return 0;
    }
    winner->type_index = tgt;
    winner->profession = UNITS_JOB_SOLDIER; /* DOS keeps 0x15 */
    popup_tag = "CONTINENTAL";
  } else {
    winner->profession = next_prof;
    popup_tag = (next_prof == UNITS_JOB_SOLDIER) ? "VETERAN" : "VALOR";
  }
  if (human) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    const char* base = old_ty && old_ty->name[0] ? old_ty->name : "";
    char fb[AI_POPUP_BODY_LEN];
    if (strcmp(popup_tag, "CONTINENTAL") == 0) {
      tok.string0 = base;
      fb[0] = '\0';
    } else if (strcmp(popup_tag, "VETERAN") == 0) {
      tok.string0 = base;
      fb[0] = '\0';
    } else {
      /* bugs.md #265: DOS 172c fills @VALOR's STRING1/STRING2 with the OLD
       * and NEW profession names (FUN_281f_0c40), not unit display names —
       * a dragoon body's display name hides the profession change, so the
       * popup read "our Dragoons ... promoted from Dragoon to Dragoon". */
      tok.string0 = base;
      tok.string1 = units_promote_prof_label(old_prof);
      tok.string2 = units_promote_prof_label(winner->profession);
      fb[0] = '\0';
    }
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_DEMOTE, popup_tag, winner->nation_id, -1, 0, &tok, fb
    );
  }
  return 1;
}

/*
 * FUN_5fef_0352 type demote table (NAMES @UNIT indices):
 * Dragoon→Soldier, Soldier→Colonist, Cont.Cav→Cont.Army, Cavalry→Regulars,
 * Cont.Army→Colonist; Jesuit profession on Colonist demote → Missionary.
 * Returns new type_index or -1 if no demote (destroy path).
 */
static int units_combat_demote_type_index(ColonizeUnitPool* pool, const ColonizeUnit* loser) {
  if (!pool || !loser) {
    return -1;
  }
  const ColonizeUnitType* lt = units_type(pool, loser->type_index);
  if (!lt || !lt->name[0]) {
    return -1;
  }
  ColonizeUnitKind dest_kind = UNITS_KIND_UNKNOWN;
  switch (units_type_kind(lt)) {
  case UNITS_KIND_CONT_CAV:
    dest_kind = UNITS_KIND_CONT_ARMY;
    break;
  case UNITS_KIND_CAVALRY:
    dest_kind = UNITS_KIND_REGULAR;
    break;
  case UNITS_KIND_CONT_ARMY:
  case UNITS_KIND_SOLDIER:
    dest_kind = (loser->profession == UNITS_JOB_MISSIONARY) ? UNITS_KIND_MISSIONARY : UNITS_KIND_COLONIST;
    break;
  case UNITS_KIND_DRAGOON:
    dest_kind = UNITS_KIND_SOLDIER;
    break;
  default:
    break;
  }
  if (dest_kind == UNITS_KIND_UNKNOWN) {
    return -1;
  }
  return units_kind_type_index(pool, dest_kind);
}

/* Align tools/muskets/horses with post-demote type (spawn-shaped). */
void units_sync_equip_after_type_change(ColonizeUnit* u, const ColonizeUnitType* t) {
  if (!u || !t) {
    return;
  }
  u->tools = 0;
  u->muskets = 0;
  u->horses = 0;
  switch (units_type_kind(t)) {
  case UNITS_KIND_PIONEER:
    u->tools = UNITS_EQUIP_TOOLS_MAX;
    break;
  case UNITS_KIND_DRAGOON:
  case UNITS_KIND_CONT_CAV:
  case UNITS_KIND_CAVALRY:
    u->muskets = UNITS_EQUIP_MUSKETS;
    u->horses = UNITS_EQUIP_HORSES;
    break;
  case UNITS_KIND_SOLDIER:
  case UNITS_KIND_REGULAR:
  case UNITS_KIND_CONT_ARMY:
    u->muskets = UNITS_EQUIP_MUSKETS;
    break;
  case UNITS_KIND_SCOUT:
    u->horses = UNITS_EQUIP_HORSES;
    break;
  default:
    break;
  }
}

/*
 * FUN_5fef_0352 land demote: change type, @DEMOTE if human-facing.
 * Returns 1 if demoted (unit survives), 0 if no demote mapping.
 */
static int units_demote_combat_type(
  ColonizeUnitPool* pool,
  ColonizeUnit* loser,
  const ColonizeCol1Save* col1,
  int human_facing
) {
  if (!pool || !loser || !loser->active) {
    return 0;
  }
  /*
   * bugs.md: an armed colonist-TYPE unit (muskets on a Colonists/expert body
   * — how a colonist armed in a colony is stored) demotes by shedding
   * equipment, keeping type AND profession: armed+mounted loses the horses
   * (Dragoon→Soldier), armed loses the muskets (Soldier→Colonist). A
   * Veteran keeps veteran status — only CAPTURE strips it. Horses-only
   * (scout kit) has no demote: destroyed, as before.
   */
  {
    const ColonizeUnitType* lt0 = units_type(pool, loser->type_index);
    const int table_target = units_combat_demote_type_index(pool, loser);
    /* bugs.md #247: the equipment-shed demote is for colonist-BODY units
     * only. A typed military unit with no table target (Regulars, Armed
     * Braves) is DESTROYED — the old unguarded branch stripped a Regular's
     * muskets and popped "Regulars routed, demoted to Regulars". */
    if (table_target < 0 && lt0 && loser->muskets > 0 &&
        !units_type_is_royal(lt0) && !units_type_is_brave_named(lt0)) {
      const char* was = units_display_name(pool, loser);
      char old_name[48];
      snprintf(old_name, sizeof(old_name), "%s", was ? was : "Soldier");
      if (loser->horses > 0) {
        loser->horses = 0;
      } else {
        loser->muskets = 0;
      }
      /* Same DOS write set as the table arm below (raw 99433-99464): type /
       * equipment only, orders and MP untouched. bugs.md #646. */
      if (human_facing) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, loser->nation_id);
        tok.string1 = old_name;
        tok.string2 = units_display_name(pool, loser);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_DEMOTE,
          "DEMOTE",
          loser->nation_id,
          -1,
          0,
          &tok,
          "");
      }
      return 1;
    }
  }
  const int tgt = units_combat_demote_type_index(pool, loser);
  if (tgt < 0 || tgt == loser->type_index) {
    return 0;
  }
  const ColonizeUnitType* old_ty = units_type(pool, loser->type_index);
  const char* old_name = old_ty && old_ty->name[0] ? old_ty->name : "unit";
  loser->type_index = tgt;
  const ColonizeUnitType* nt = units_type(pool, tgt);
  units_sync_equip_after_type_change(loser, nt);
  /*
   * DOS-LITERAL FUN_5fef_0352 raw 99433-99464: the demote arm writes the
   * TYPE byte (`+0x3146`) and nothing else — no `+0x314c` orders write, no
   * `+0x3149` MP write. The port used to clear both, so a Fortified Dragoon
   * beaten down to Soldiers lost its Fortified order. bugs.md #646.
   */
  if (human_facing) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = units_combat_nation_label(col1, loser->nation_id);
    tok.string1 = old_name;
    /* bugs.md #247: name the post-demote unit by DISPLAY name so a veteran
     * stripped to a colonist body reads "Veteran Soldiers", not "Colonists". */
    const char* new_disp = units_display_name(pool, loser);
    tok.string2 = new_disp && new_disp[0] ? new_disp
                  : (nt && nt->name[0] ? nt->name : "colonist");
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_DEMOTE,
      "DEMOTE",
      loser->nation_id,
      -1,
      0,
      &tok,
      "");
  }
  return 1;
}

/*
 * bugs.md: a captured unit must not stay mixed into the enemy stack it was
 * taken from — it changes allegiance AND moves to the captor's tile. The one
 * exception is a colony tile (colony capture keeps everyone put; the colony
 * flip handles them).
 */
static void units_capture_relocate_to_winner(
  ColonizeUnitPool* pool, ColonizeUnit* lose, const ColonizeUnit* win
) {
  if (!pool || !lose || !win || !units_is_on_map(win)) {
    return;
  }
  if (lose->x == win->x && lose->y == win->y) {
    return;
  }
  if (g_units_combat_colonies &&
      colonies_id_at(g_units_combat_colonies, lose->x, lose->y) >= 0) {
    return; /* colony tile: captured colonists stay put */
  }
  const int ox = lose->x;
  const int oy = lose->y;
  lose->x = win->x;
  lose->y = win->y;
  if (lose->aboard_ship_id < 0 && units_is_on_map(lose)) {
    units_tile_stack_arrive(pool, lose->id);
  }
  /* Now stands with the captor: share the captor tile's sight stamp. */
  lose->col1_vis_mask = win->col1_vis_mask;
  units_occupancy_refresh_tile(pool, ox, oy, -1);
  units_occupancy_refresh_tile(pool, lose->x, lose->y, -1);
}

/*
 * FUN_5fef_0352: artillery damage; Euro-only Colonist/Wagon capture; type demote;
 * else despawn. Natives never capture (winner nation must be 0..3). Pioneers /
 * Missionaries / Scouts are not capture types → destroy. Returns 1 if unit still
 * active (captured/damaged/demoted), 0 if gone.
 */
/*
 * DOS FUN_5fef_0352 capture-alive core, shared by the wagon and colonist arms
 * (UN-45): the unit changes flag, drops its orders and its remaining MP and
 * steps onto the winner's tile. The two arms differ only in their chrome.
 */
static void units_capture_to_winner(
  ColonizeUnitPool* pool, ColonizeUnit* lose, ColonizeUnit* win
) {
  units_set_nation(lose, win->nation_id);
  lose->orders = UNITS_ORDER_NONE;
  /*
   * DOS-LITERAL FUN_5fef_0352 raw 99393-99399: the capture arm writes the
   * nation nibble (0812/0894/0844 = occupancy remove / set-nation / add) and
   * `+0x314c = 0`. There is NO `+0x3149` MP write, so the port's `moves = 0`
   * is dropped (bugs.md #646). The relocation below is deliberate and is NOT
   * DOS: bugs.md #198 (CLOSED, user-observed) requires a captured unit to
   * change allegiance AND step onto the captor's tile, colony tiles excepted.
   */
  units_capture_relocate_to_winner(pool, lose, win);
}

int units_apply_land_loss_outcome(
  ColonizeUnitPool* pool,
  int loser_id,
  int winner_id,
  const ColonizeCol1Save* col1,
  int show_popups,
  ColonizeDosRng* rng
) {
  ColonizeUnit* lose = units_get(pool, loser_id);
  ColonizeUnit* win = units_get(pool, winner_id);
  if (!lose || !win || !lose->active) {
    return 0;
  }
  const ColonizeUnitType* lt = units_type(pool, lose->type_index);
  const ColonizeUnitType* wt = units_type(pool, win->type_index);
  const int human =
    show_popups && units_combat_human_involved(col1, lose->nation_id, win->nation_id);
  const int win_euro = win->nation_id >= 0 && win->nation_id <= 3;
  /*
   * Smell audit 2026-09-10 #5. DOS raw 99378-99380 gates the capture on the
   * WINNER's type-table byte (`*(char *)(local_4 * 0xe + 0x5236) == '\0' →
   * bVar12 = false`, local_4 = param_2's type byte at raw 99343, param_2 =
   * winner — its nation nibble is the `local_32 < 4` Euro gate at raw 99392).
   * That byte is the @UNIT attack column, so the DOS question is "is the
   * winner a combatant". A bare `wt->attack > 0` asks it of the port's TYPE
   * only, and DOS stores a colony-armed colonist as type 1 "Soldiers"
   * (attack 2) where this port keeps the Colonists body and hangs muskets on
   * it — so a winner DOS calls a soldier read attack 0 here and its beaten
   * Colonists/Wagon fell through to demote/despawn instead of flipping.
   * units_is_combat_role (:6111) is the body-model spelling of the same byte.
   */
  const int win_can_capture = win_euro && units_is_combat_role(pool, win);
  const int loser_euro = lose->nation_id >= 0 && lose->nation_id <= 3;

  /* ===== Native gear arms (bugs.md #645 / #653) ===== */
  g_units_native_gear_armed = 0;
  g_units_native_gear_mounted = 0;
  {
    ColonizeCol1Save* wcol1 = g_units_fallout_col1;
    const int on_colony = g_units_combat_colonies &&
                          colonies_id_at(g_units_combat_colonies, lose->x, lose->y) >= 0;
    /*
     * DOS-LITERAL FUN_5fef_0352 raw 99364-99377 (bugs.md #653): a KILLED
     * Armed / Mtd. Brave hands its gear back to its own tribe. Gate: loser
     * nation nibble > 3 and (`loser+0x3148 & 0x10` — the wander-dest latch —
     * or `FUN_281f_04d4(0,1)`, a coin flip). Record base is `nation * 0x4e +
     * 0x599e` (tech at +2 = DS:0x5ad6 for nation 4, ai.c:1390), so 0x59a5 =
     * muskets (+7) and 0x59a8 = horse_breeding (+10):
     *   type 0x14 / 0x16 -> muskets += 1
     *   type 0x15 / 0x16 -> horse_breeding += 0x19
     */
    /*
     * bugs.md #835: DOS's only gate is `3 < uVar15` plus the flag/coin term,
     * so the draw happens even with no col1 record bound. Evaluate the gate
     * first and apply the record write only when a record exists, otherwise a
     * headless caller silently skips the RNG draw and shifts the stream.
     */
    if (!loser_euro && lose->nation_id >= 4 &&
        (((lose->col1_flags15 & 0x10u) != 0) || dos_rng_range(rng, 0, 1) != 0)) {
      if (wcol1 && (unsigned)(lose->nation_id - 4) < (unsigned)COLONIZE_COL1_INDIAN_COUNT) {
        const ColonizeUnitKind lk = units_type_kind(lt);
        ColonizeCol1Indian* ind = &wcol1->indian[lose->nation_id - 4];
        if (lk == UNITS_KIND_ARMED_BRAVE || lk == UNITS_KIND_MTD_WARRIOR) {
          ind->muskets = (uint8_t)(ind->muskets + 1);
        }
        if (lk == UNITS_KIND_MTD_BRAVE || lk == UNITS_KIND_MTD_WARRIOR) {
          ind->horse_breeding = (uint16_t)(ind->horse_breeding + 0x19);
        }
      }
    }
    /*
     * DOS-LITERAL FUN_5fef_1b0e raw 100641-100646 (bugs.md #834): when the
     * native attacker wins on a tile carrying a colony (`-1 < iVar18`, the
     * 07be colony lookup) DOS sets `local_6 = 1` AND latches
     * `attacker+0x3148 |= 0x10`. That bit is the guaranteed arm of the 0352
     * gear-return gate above, so a colony-raiding brave killed later always
     * hands its gear back. DOS's extra `(colony+0x1f > 1 || !bVar28)` term is
     * always true at this entry point: `bVar28` is "no real unit defender was
     * found", and this function is only ever reached with a live loser unit.
     */
    if (win->nation_id >= 4 && on_colony) {
      win->col1_flags15 = (uint8_t)(win->col1_flags15 | 0x10u);
    }
    /*
     * DOS-LITERAL FUN_5fef_1b0e raw 100730-100744 (bugs.md #645): a native
     * winner takes the beaten Euro unit's GEAR as a type STEP, not as a
     * numeric kit transfer. Gate: attacker nation nibble > 3, defender nibble
     * < 4, a real defender existed and the fight is not a colony raid
     * (`local_6`, raw 100644 = native attacker with a colony on the target
     * tile).
     *   defender type 4 Dragoons / 5 Scouts, brave type 0x13 / 0x14
     *       -> brave type += 2 (mounted) and tribe `0x59a6` (+8 horse_herds)
     *          += 1; flags @INDIANWIN2.
     *   else defender type 1 Soldiers, brave type 0x15 / 0x13
     *       -> brave type += 1 (armed); flags @INDIANWIN1.
     * (bVar14 -> tag suffix '2', bVar13 -> '1' at raw 101088-101095.)
     */
    if (loser_euro && win->nation_id >= 4 && !on_colony) {
      const ColonizeUnitKind lk = units_type_kind(lt);
      const ColonizeUnitKind wk = units_type_kind(wt);
      int step = 0;
      if ((lk == UNITS_KIND_DRAGOON || lk == UNITS_KIND_SCOUT) &&
          (wk == UNITS_KIND_BRAVE || wk == UNITS_KIND_ARMED_BRAVE)) {
        step = 2;
        g_units_native_gear_mounted = 1;
        if (wcol1 && (unsigned)(win->nation_id - 4) < (unsigned)COLONIZE_COL1_INDIAN_COUNT) {
          ColonizeCol1Indian* ind = &wcol1->indian[win->nation_id - 4];
          ind->horse_herds = (uint8_t)(ind->horse_herds + 1);
        }
      } else if (lk == UNITS_KIND_SOLDIER &&
                 (wk == UNITS_KIND_MTD_BRAVE || wk == UNITS_KIND_BRAVE)) {
        step = 1;
        g_units_native_gear_armed = 1;
      }
      if (step != 0) {
        const int nti = units_kind_type_index(pool, (ColonizeUnitKind)(wk + step));
        if (nti >= 0) {
          win->type_index = nti;
          units_sync_equip_after_type_change(win, units_type(pool, nti));
        } else {
          g_units_native_gear_mounted = 0;
          g_units_native_gear_armed = 0;
        }
      }
    }
  }

  /*
   * DOS-LITERAL FUN_5fef_0352 raw 99352-99362: `local_2a` = FUN_281f_0768 of
   * the WINNER's tile OR'd with the LOSER's tile (ocean / high seas).
   * Hoisted above the artillery arm 2026-09-23 (bugs.md #756) — it gates both
   * that arm and the capture arm below.
   */
  const int on_water =
    units_tile_is_ocean_or_hs(col1, win->x, win->y) ||
    units_tile_is_ocean_or_hs(col1, lose->x, lose->y);
  const int loser_is_hull = lt && units_type_is_ship(lt);
  /* DOS tests param_2 (the WINNER) here: types 0xd..0x12 are hulls. */
  const int winner_is_hull = wt && units_type_is_ship(wt);

  /*
   * Artillery: first loss → damaged bit7; already damaged → destroyed.
   *
   * DOS-LITERAL FUN_5fef_0352 raw 99435-99436: the demote ladder AND this
   * artillery block sit inside
   *   `if (((winner type < 0xd) || (0x12 < winner type)) && (local_2a == 0))`.
   * When the gate fails (a hull won, or either combatant stands on ocean /
   * high seas) the gun falls straight through to the plain despawn tail at
   * raw 99711 — destroyed outright, no damage stage, no @ARTILLERY popup.
   * (bugs.md #756)
   */
  if (lt && combat_type_is_artillery(lt) && !winner_is_hull && !on_water) {
    /* bugs.md: these are GAME.TXT @ARTILLERY / @ARTILLERY2, not the ship's
     * @SHIPDAMAGE — the old reuse produced the "Artillery ... Ship returns
     * to for repairs" mashup. */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = units_combat_nation_label(col1, lose->nation_id);
    tok.string1 = lt->name;
    if ((lose->col1_flags15 & 0x80u) == 0) {
      /* DOS-LITERAL FUN_5fef_0352 raw 99475-99482: the 0x1b54 arm writes
       * ONLY bit7 and returns — no spent-byte / orders write. The invented
       * `lose->moves = 0` was removed 2026-09-23 (bugs.md #757). */
      lose->col1_flags15 |= 0x80u;
      if (human) {
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_SHIP,
          "ARTILLERY",
          lose->nation_id,
          win->nation_id,
          0,
          &tok,
          "");
      }
      return 1;
    }
    if (human) {
      units_combat_enqueue_tok(
        AI_POPUP_TAG_COMBAT_SHIP,
        "ARTILLERY2",
        lose->nation_id,
        win->nation_id,
        0,
        &tok,
        "");
    }
    units_despawn(pool, loser_id);
    return 0;
  }

  /*
   * Capture-alive (DOS loser type ∈ {0x00 Colonists, 0x0a Treasure,
   * 0x0c Wagon}). Winner must be Euro with attack>0 — natives destroy,
   * never nation-flip.
   *
   * DOS-LITERAL FUN_5fef_0352 raw 99381-99383 (bugs.md #663): the capture
   * flag is cleared again by
   * `if ((0xc < loser type && loser type < 0x13) || local_2a != 0)
   *    bVar12 = false;`
   * — a losing hull (types 0xd..0x12) never changes hands, and neither does
   * anyone when `local_2a` (FUN_281f_0768, ocean / high seas of the WINNER's
   * tile OR'd with the LOSER's tile) is set. A disqualified capture falls
   * through to the demote ladder / destroy below.
   */
  if (lt && lt->name[0] /* bugs.md #678: 0352 raw 99343-99392 tests loser TYPE only, never its nation */ && win_can_capture && !loser_is_hull && !on_water) {
    const int is_treasure = units_type_is_treasure(lt);
    const int is_wagon = units_type_is_wagon(lt);
    /*
     * bugs.md: mounted scouts are DESTROYED, never captured — by anyone.
     * DOS captures type 0 Colonists only, and an equipped colonist is a
     * different type there; a Colonists-type unit carrying horses or
     * muskets here (seizure, save import) must not slip into the capture
     * branch on its type name.
     *
     * bugs.md #637: tools too. DOS FUN_5fef_0352 raw 99344-99347 captures
     * loser types {0 Colonists, 0x0a Treasure, 0x0c Wagon}; type 2 Pioneers
     * is absent there and absent from the demote ladder (raw 99437-99447),
     * so a losing Pioneer is destroyed. Latent in-port (tools only ride the
     * Pioneers type today); guards imported/hand-edited saves.
     */
    const int is_colonist =
      units_type_is_colonist(lt) && !is_treasure && !is_wagon &&
      lose->horses <= 0 && lose->muskets <= 0 && lose->tools <= 0;
    if (is_treasure) {
      /*
       * DOS-LITERAL FUN_5fef_0352 raw 99392-99413 (bugs.md #660): a beaten
       * Treasure Train CHANGES HANDS like a Wagon — FUN_281f_0812 (unlink),
       * FUN_281f_0894(unit, winner nation) (nation flip), FUN_281f_0844
       * (re-place), `+0x314c = 0` (orders none) — and pushes tag 0x1b13
       * @LOOTCAPTURE with STRING0 = loser nation, STRING1 = winner nation,
       * NUMBER0 = `*(char *)(loser * 0x1c + 0x315b) * 100`, the train's value
       * as DISPLAY ONLY. No gold is credited anywhere in 0352; the port's
       * Accept/Refuse "ransom" CHOICE and its credit were inventions.
       * The value goes through units_treasure_value_gold so a COL1-imported
       * train (value in the profession byte, empty LE16 mirror) is not 0.
       */
      const int from_nat = lose->nation_id;
      const int loot_gold = units_treasure_value_gold(lose);
      units_capture_to_winner(pool, lose, win);
      if (human) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, from_nat);
        tok.string1 = units_combat_nation_label(col1, win->nation_id);
        tok.number0 = loot_gold;
        tok.has_number0 = true;
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_CAPTURE,
          "LOOTCAPTURE",
          win->nation_id,
          from_nat,
          loot_gold,
          &tok,
          "");
      }
      return 1;
    } else if (is_wagon) {
      /* DOS-LITERAL FUN_5fef_0352 raw 99398-99408: the wagon capture arm
       * pushes only @WAGONCAPTURE (0x1b1f). @CARGOCAPTURE (0x1b69) is raised
       * solely by the ship-vs-ship hold-transfer loop (raw 99490-99514), not
       * here; cargo stays on the captured wagon untouched. bugs.md #790. */
      const int from_nat = lose->nation_id;
      units_capture_to_winner(pool, lose, win);
      if (human) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, from_nat);
        tok.string1 = units_combat_nation_label(col1, win->nation_id);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_CAPTURE,
          "WAGONCAPTURE",
          win->nation_id,
          from_nat,
          0,
          &tok,
          "");
      }
      return 1;
    } else if (is_colonist) {
      const int from_nat = lose->nation_id;
      /* DOS: Veteran specialty (0x15) → NONE + @COLONISTCAPTURE2. */
      const int stripped_vet = (lose->profession == UNITS_JOB_SOLDIER);
      if (stripped_vet) {
        lose->profession = UNITS_JOB_NONE;
      }
      units_capture_to_winner(pool, lose, win);
      if (human) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, from_nat);
        tok.string1 = units_combat_nation_label(col1, win->nation_id);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_CAPTURE,
          stripped_vet ? "COLONISTCAPTURE2" : "COLONISTCAPTURE",
          win->nation_id,
          from_nat,
          0,
          &tok,
          "");
      }
      return 1;
    }
  }

  /*
   * Type demote (Soldier→Colonist, Dragoon→Soldier, …); keep nation.
   *
   * DOS-LITERAL FUN_5fef_0352 raw 99355-99364 + 99433: the ladder is gated.
   * `local_2a` is `FUN_281f_0768` (ocean / high seas) of the WINNER's tile
   * OR'd with the LOSER's tile, and the arm runs only when
   * `(winner type < 0xd || winner type > 0x12) && local_2a == 0` — i.e. the
   * winner is not a hull and neither tile is water. Beaten by a ship, or
   * fought on water, the loser is DESTROYED. The port called the ladder
   * unconditionally. bugs.md #647.
   */
  {
    /* `winner_is_hull` / `on_water` are the raw 99435-99436 gate hoisted above
     * (shared with the artillery arm, bugs.md #756). */
    if (!winner_is_hull && !on_water &&
        units_demote_combat_type(pool, lose, col1, human)) {
      return 1;
    }
  }

  units_despawn(pool, loser_id);
  return 0;
}

/* Defined below with the rest of the naval tail; the 0ec0 sweep needs it for
 * FUN_5fef_0352's hull arm (raw 99518-99649). */
/*
 * DOS-LITERAL FUN_5fef_0352 raw 99527-99570 (second, overlay-duplicated copy
 * at raw 113600-113620) — the full damage-vs-sink gate that sits ON TOP of
 * the guns/hull roll units_ship_damage_vs_sink makes. bugs.md #867: the port
 * had none of it.
 *
 *   uVar15 = loser nation, local_32 = winner nation, local_4 = winner type.
 *   uVar16 = loser type. `bVar11` = "damaged" (false = sunk).
 *
 *   if (loser_type*0xe + 0x5236 == 0) {                  // unarmed hull
 *     bVar10 = DS[uVar15*0x13 - 0x6da3];                 // Frigate count
 *     bVar6  = DS:0x5325;                                // = @UNIT[0x11].0x5237
 *     bVar7  = DS[uVar15 + 0x9414];                      // ship_cargo_totals
 *     bVar8  = loser_type*0xe + 0x5237;                  // loser cargo column
 *     bVar9  = DS[uVar15*0x13 - 0x6da4];                 // Privateer count
 *     bVar11 = (DS[n+0x9418] - DS[n+0x9424] < 9) && bVar11;
 *     if (loser_type == 0x0d && DS:0x538e > 0x4f) bVar11 = false;   // Caravel
 *     iVar18 = FUN_281f_035c(DS[n+0x9410] >> 2, 3, 6);   // clamp(…,3,6)
 *     if (bVar7 - bVar8 - bVar10*bVar6 - bVar9 < iVar18) bVar11 = true;
 *   } else if ((DS:0x5382 & 1) == 0) {                   // armed hull, no WoI
 *     bVar10 = DS[uVar15*0x13 + loser_type - 0x6db4];    // own count of this type
 *     if (DS[n+0x9298] < bVar10 || 8 < DS[n+0x9424]) bVar11 = false;
 *     if (loser_type == 0x11 && local_4 == 0x11 &&
 *         DS[local_32*0x13 - 0x6da3] < bVar10) bVar11 = false;
 *     if (bVar10 < 2 && DS[n+0x9298] != 0) bVar11 = true;
 *   }
 *   if ((DS:0x5382 & 1) && uVar15 == DS:0x53d2 &&
 *       loser_type == 0x12 && DS[uVar15*0x13 - 0x6da2] < 2) bVar11 = true;
 *
 * i.e. AI fleet-pool keep/lose biases for an unarmed hull, a Frigate-vs-
 * Frigate "the bigger navy keeps its hull" clause for an armed one, and — the
 * clause #867 was filed for — the King's LAST Man-O-War is unsinkable during
 * the War of Independence. DS:0x5325 folds to @UNIT[Frigate].cargo (the table
 * base 0x5230 + 0x11*0xe + 7).
 *
 * `wt` NULL = the winner is not a unit (coastal fort): only the
 * Frigate-vs-Frigate clause reads the winner's type, and a fort is never a
 * Frigate, so it simply does not fire. DOS's `param_2 < 0` force-damage arm
 * belongs to the 0f14 raid, which never reaches this helper.
 *
 * Census note: DOS also decrements DS:0x9414/0x924c/0x9424 on the sink path
 * (raw 99570-99579); the port rebuilds the whole census from the live pool
 * (col1_stuff_census.c), so that write-back is not transcribed.
 */
/* DS:0x53d2 = head.crown_nation_id, the slot the succession merger vacated
 * (same rule as ai_king_crown_nation_col1; spelled locally so the slim
 * units test targets need not link ai_king.c). */
static int units_combat_crown_nation(const ColonizeCol1Save* col1) {
  if (!col1) {
    return -1;
  }
  const int human = (int)col1->head.human_player;
  const int c = (int)col1->head.crown_nation_id;
  if (c >= 0 && c < 4 && c != human) {
    return c;
  }
  return (human == 0) ? 1 : 0;
}

int units_naval_damage_gate(
  const ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  const ColonizeUnit* lose,
  const ColonizeUnitType* wt,
  int winner_nation,
  int damaged
) {
  if (!pool || !col1 || !lose) {
    return damaged;
  }
  const int n = lose->nation_id;
  if (n < 0 || n > 3) {
    return damaged;
  }
  const ColonizeCol1Stuff* st = &col1->stuff;
  const ColonizeUnitType* lt = units_type(pool, lose->type_index);
  if (!lt) {
    return damaged;
  }
  const int lrow = lose->type_index;
  const int row_frigate = units_kind_type_index(pool, UNITS_KIND_FRIGATE);
  const int row_privateer = units_kind_type_index(pool, UNITS_KIND_PRIVATEER);
  const int row_mow = units_kind_type_index(pool, UNITS_KIND_MAN_O_WAR);
  const int woi = col1->head.game_options.woi != 0;

#define UNITS_TYPE_CNT(nat, row) \
  (((nat) >= 0 && (nat) < 4 && (row) >= 0 && (row) < 19) \
     ? (int)st->unit_type_counts[(nat)][(row)] : 0)

  if (lt->attack == 0) {
    /* Unarmed hull: the AI fleet-pool keep/lose bias. */
    const ColonizeUnitType* ft = row_frigate >= 0 ? units_type(pool, row_frigate) : NULL;
    const int frigate_cargo = ft ? ft->cargo : 0;
    damaged = ((int)st->ship_counts[n] - (int)st->armed_ship_counts[n] < 9) && damaged;
    if (units_type_kind(lt) == UNITS_KIND_CARAVEL && (int)col1->head.turn > 0x4f) {
      damaged = 0;
    }
    int floor_v = (int)st->census_pop_proxy[n] >> 2;
    if (floor_v < 3) {
      floor_v = 3;
    }
    if (floor_v > 6) {
      floor_v = 6;
    }
    const int spare = (int)st->ship_cargo_totals[n] - lt->cargo -
                      UNITS_TYPE_CNT(n, row_frigate) * frigate_cargo -
                      UNITS_TYPE_CNT(n, row_privateer);
    if (spare < floor_v) {
      damaged = 1;
    }
  } else if (!woi) {
    /* Armed hull outside the WoI. */
    const int own = UNITS_TYPE_CNT(n, lrow);
    if ((int)st->colony_counts[n] < own || (int)st->armed_ship_counts[n] > 8) {
      damaged = 0;
    }
    if (row_frigate >= 0 && lrow == row_frigate && wt &&
        units_type_kind(wt) == UNITS_KIND_FRIGATE &&
        UNITS_TYPE_CNT(winner_nation, row_frigate) < own) {
      damaged = 0;
    }
    if (own < 2 && st->colony_counts[n] != 0) {
      damaged = 1;
    }
  }

  /* raw 99562-99565 / 113608: the King's last Man-O-War cannot be sunk. */
  if (woi && row_mow >= 0 && lrow == row_mow &&
      n == units_combat_crown_nation(col1) &&
      UNITS_TYPE_CNT(n, row_mow) < 2) {
    damaged = 1;
  }
#undef UNITS_TYPE_CNT
  return damaged;
}

int units_apply_naval_loss_outcome(
  ColonizeUnitPool* pool,
  int loser_id,
  int winner_id,
  int loser_str,
  int winner_str,
  int show_popups,
  const ColonizeCol1Save* col1,
  ColonizeDosRng* rng
);

/* Thin FUN_5fef_0ec0: after combat loss, capture leftover non-combat same-nation stack. */
void units_sweep_stack_after_loss(
  ColonizeUnitPool* pool,
  int x,
  int y,
  int loser_nation,
  int winner_id,
  int primary_loser_id,
  const ColonizeCol1Save* col1
) {
  if (!pool || loser_nation < 0) {
    return;
  }
  ColonizeUnit* win = units_get(pool, winner_id);
  if (!win || !win->active) {
    return;
  }
  int slot_2 = 0;
  for (ColonizeUnit* u = units_next_on_tile(pool, x, y, &slot_2); u != NULL;
       u = units_next_on_tile(pool, x, y, &slot_2)) {
    if (u->nation_id != loser_nation || u->id == winner_id || u->id == primary_loser_id) {
      continue;
    }
    /*
     * Smell audit 2026-09-09 #4, verified against DOS and kept as-is except
     * for the hull skip:
     *
     *  - DOS FUN_5fef_0ec0 (raw 99708-99730) carries NO per-unit predicate at
     *    all: it walks the tile list and hands every entry to 0352. The
     *    `attack == 0` narrowing here is the port's own safety rail (a won
     *    attack must not capture a whole defended stack), so tightening it
     *    further to units_is_combat_role would move AWAY from DOS, not
     *    toward it — an armed colonist body is DOS's type 1 "Soldiers", and
     *    0ec0 does sweep it.
     *  - It is also not "seized without a fight": units_apply_land_loss_outcome
     *    only nation-flips a Colonists body with `muskets <= 0 && horses <= 0`
     *    (see is_colonist below), so an armed body falls to
     *    units_demote_combat_type and sheds its kit — exactly DOS's
     *    Soldiers → Colonists demote row.
     *  - Hulls must NOT be despawned: a berthed Caravel/Merchantman/Galleon is
     *    attack 0 and has no capture/demote row, so units_apply_land_loss_outcome
     *    dropped it outright. They must not be skipped either — see below.
     *
     * FUN_5fef_0352's hull arm (raw 99518-99649), ported 2026-09-09. 0352 tests
     * the LOSER's type byte first — `if ((0xc < type) && (type < 0x13))`, i.e.
     * any hull, whatever domain the fight was — long before any of its land
     * rows, and none of those rows can match a ship anyway (the capture set at
     * raw 99345 is types 0/0xa/0xc, the demote set at raw 99437-99451 is
     * 4/1/9/7/8, and the artillery row is 0xb). Inside that arm:
     *
     *   bVar11 = true;                                   // raw 99523
     *   if (winner_type*0xe + 0x523b != 0) { ...roll... } // raw 99527-99530
     *
     * 0x523b is the @UNIT guns column, and EVERY land type in NAMES.TXT carries
     * guns 0 (only Merchantman..Man-O-War are non-zero). So a land winner never
     * even reaches the damage-vs-sink roll: the hull is ALWAYS damaged — no RNG
     * draw, exactly as DOS skips the draw. It then takes the ordinary damage
     * tail (raw 99582-99649): holds and passengers lost, bit7, repair timer,
     * relocation to the nearest own Drydock colony (or the Europe lane), and
     * @SHIPDAMAGE. The one escape is 0352's shared WoI human-with-no-port sink
     * (raw 99604-99607), which units_apply_naval_loss_outcome already applies.
     *
     * The `attack == 0` rail above is deliberately NOT applied to hulls: DOS
     * keys this arm on the type range alone, so an armed Privateer/Frigate
     * berthed alongside takes the same damage as a Caravel. The rail exists so
     * a won attack cannot capture a defended LAND stack; a hull is never a land
     * defender in this port (units_best_defender_at's domain gate), so there is
     * nothing for it to guard here.
     */
    if (units_is_sea(pool, u->id)) {
      /* rng NULL is safe and DOS-exact: the guns==0 short-circuit in
       * units_ship_damage_vs_sink returns "damaged" without drawing, and a
       * land winner is the only winner this sweep can have (the naval paths
       * use units_sweep_naval_stack_after_loss). */
      (void)units_apply_naval_loss_outcome(pool, u->id, winner_id, 0, 0, 1, col1, NULL);
      continue;
    }
    const ColonizeUnitType* t = units_type(pool, u->type_index);
    if (t && t->attack == 0 && u->nation_id >= 0 && u->nation_id <= 3) {
      (void)units_apply_land_loss_outcome(pool, u->id, winner_id, col1, 0, NULL);
    }
  }
}

/* Nearest own colony WITH a Drydock (bugs.md: repair routing). NULL if none. */
const ColonizeColony* units_nearest_own_drydock_colony(
  const ColonizeColonyPool* colonies,
  int nation_id,
  int x,
  int y
) {
  if (!colonies) {
    return NULL;
  }
  /* DOS tests colony feature bit 7 (Drydock). Shipyard is the tier above it
   * and leaves that bit set, but a save whose mask only carries the top tier
   * must still count as a repair port. */
  const int drydock = colonies_building_row(colonies, COLONY_BUILDING_DRYDOCK);
  const int shipyard = colonies_building_row(colonies, COLONY_BUILDING_SHIPYARD);
  if (drydock < 0 && shipyard < 0) {
    return NULL;
  }
  const ColonizeColony* best = NULL;
  long best_d = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    const int repairs =
      (drydock >= 0 && drydock < COLONIZE_BUILDING_TYPES_MAX && c->has_building[drydock]) ||
      (shipyard >= 0 && shipyard < COLONIZE_BUILDING_TYPES_MAX && c->has_building[shipyard]);
    if (!repairs) {
      continue;
    }
    const long dx = c->x - x;
    const long dy = c->y - y;
    const long d = dx * dx + dy * dy;
    if (best_d < 0 || d < best_d) {
      best_d = d;
      best = c;
    }
  }
  return best;
}

/*
 * @SHIPDAMAGE %STRING2 when the loser's nation owns no Drydock colony: DOS
 * FUN_5fef_0352 substitutes the nation's home-port name from DS:-0x7c74 (the
 * crown slot borrowing the human's) — the ship goes to EUROPE, not to the
 * nearest ordinary colony. bugs.md: "Ships damaged don't seem to respect the
 * need for a drydock or shipyard".
 */
const char* units_home_port_name(const ColonizeCol1Save* col1, int nation_id) {
  int n = nation_id;
  const int crown = unit_chrome_crown_nation();
  if (crown >= 0 && n == crown) {
    n = col1 ? (int)col1->head.human_player : g_units_combat_human_nation;
  }
  if (n < 0 || n > 3) {
    return "Europe";
  }
  return g_units_homeport[n][0] ? g_units_homeport[n] : "Europe";
}

/*
 * DOS FUN_5fef_0352 damage tail (overlays.c 85117-85186), shared by the naval
 * loss outcome and the FUN_5fef_0f14 raid (UN-7): repair is a TIMER, not a
 * drydock flash — bit7 + col1_counter16 preset so the normal EOT ship tick
 * (+1/turn, +1 extra on a colony tile) counts up to the 0x5235-column
 * threshold. Remaining turns = winner's combat strength (already doubled by
 * the caller for a non-ship winner), clamped to the own threshold; Frigate
 * presets col1_counter16 >= 4, Man-O-War >= 8 (repair-time cap).
 *
 * `winner_nation` is the @SHIPDAMAGE popup's nation_b; the raid arm has no
 * winner unit and passes -1.
 */
/*
 * DOS's off-map Europe slot for a damaged ship with no repair port (see
 * units_ship_enter_repair): FUN_5fef_0352 places the hull at
 * (nation-0x14, nation-0x14) and leaves the repair counter (+0x315a)
 * running. The port has no off-map unit slots, so the equivalent is the
 * Europe harbor's Expected lane with the DOS repair timer as the wait —
 * the ship leaves the map NOW (that is what makes it pixelate away under
 * the combat dissolve) and comes back when the timer is out.
 * Europe is the human's screen only; an AI hull with no port keeps the old
 * stay-on-tile behaviour, which turn_route_damaged_ships still owns.
 */
static void units_ship_damaged_to_europe(ColonizeUnitPool* pool, ColonizeUnit* lose) {
  EuropeScreen* eu = g_units_combat_europe;
  const ColonizeCol1Save* col1 = g_units_fallout_col1;
  if (!eu || !lose || !pool) {
    return;
  }
  if (g_units_combat_human_nation < 0 || lose->nation_id != g_units_combat_human_nation) {
    return;
  }
  if (col1 && col1->head.game_options.woi) {
    return; /* no friendly Europe during the WoI (DOS raw 85139 sinks her) */
  }
  const ColonizeUnitType* lt = units_type(pool, lose->type_index);
  int turns = (int)lose->col1_counter16;
  if (turns < 1) {
    turns = 1;
  }
  const bool east =
    g_units_fallout_map ? (lose->x >= (int)g_units_fallout_map->width / 2) : true;
  if (europe_enqueue_expected(
        eu, lose->type_index, lt && lt->name[0] ? lt->name : "Ship", NULL, NULL, 0,
        lose->hold_goods_type, lose->hold_goods_amount, lose->x, lose->y, east, turns
      )) {
    /* The Europe wait IS the repair (same rule turn_route_damaged_ships
     * used): she docks seaworthy. */
    lose->col1_flags15 = (uint8_t)(lose->col1_flags15 & 0x7fu);
    units_despawn(pool, lose->id);
  }
}

void units_ship_enter_repair(
  ColonizeUnitPool* pool,
  ColonizeUnit* lose,
  int wstr,
  const ColonizeColony* home,
  const ColonizeCol1Save* col1,
  int human,
  int winner_nation
) {
  const ColonizeUnitType* lt = units_type(pool, lose->type_index);
  lose->col1_flags15 |= 0x80u;
  lose->moves = 0;
  lose->orders = UNITS_ORDER_NONE; /* DOS zeroes +0x314c */
  lose->repair_pending = 2; /* 2 = damaged this turn; see the repair tick */
  {
    /* DOS-LITERAL FUN_5fef_0352 raw 99622-99631: BOTH sides of the repair
     * bill read the @UNIT 0x5235 (defense) column raw — `local_6 =
     * winner_type*0xe+0x5235` (doubled for a non-ship winner) versus
     * `bVar10 = loser_type*0xe+0x5235`, with no fallback. bugs.md #878(h):
     * the `: 4` default was invented and unreachable — every @UNIT hull row
     * 0x0d..0x12 carries a non-zero defense. */
    const int thresh = lt ? lt->defense : 0;
    int worked = (wstr < thresh) ? thresh - wstr : 0;
    if (units_type_is_frigate(lt) && worked < 4) {
      worked = 4;
    }
    if (units_type_is_man_o_war(lt) && worked < 8) {
      worked = 8;
    }
    lose->col1_counter16 = (uint8_t)worked;
  }
  /*
   * bugs.md #462/#463: BOTH arms teleport, on the spot. The decompile drops
   * the register args of the FUN_281f_0812 (unlink from tile) /
   * FUN_281f_0844 (place on tile) pair at raw 99643-99644; the asm
   * (5fef:0ceb-5fef:0cf9, ndisasm-literal read) loads them from local_28 /
   * local_2c, which the colony scan at 5fef:0b4e-5fef:0bac fills with the
   * winning colony's x/y (DS 0x5d46/0x5d47) and, when the scan finds
   * nothing (local_20 == 0x3e7, 5fef:0bc0-5fef:0bcb), with
   * `loser_nation - 0x14` in BOTH coordinates — DOS's off-map Europe slot.
   * So a damaged ship with no repair port is in Europe the instant it
   * loses, on the repair timer; it does not linger on its tile waiting for
   * an end-of-turn router, and it does not sail a voyage (the old port
   * behaviour, which also left the sprite standing under the dissolve).
   */
  if (home && (home->x != lose->x || home->y != lose->y)) {
    const int old_x = lose->x;
    const int old_y = lose->y;
    lose->x = home->x;
    lose->y = home->y;
    if (lose->aboard_ship_id < 0 && units_is_on_map(lose)) {
      units_tile_stack_arrive(pool, lose->id);
    }
    units_occupancy_refresh_tile(pool, old_x, old_y, -1);
    units_occupancy_refresh_tile(pool, home->x, home->y, -1);
  }
  if (human) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = units_combat_nation_label(col1, lose->nation_id);
    tok.string1 = lt ? lt->name : "Ship";
    tok.string2 =
      (home && home->name[0]) ? home->name : units_home_port_name(col1, lose->nation_id);
    char fb[AI_POPUP_BODY_LEN];
    fb[0] = '\0';
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_SHIP, "SHIPDAMAGE", lose->nation_id, winner_nation, 0, &tok, fb
    );
  }
  if (!home) {
    units_ship_damaged_to_europe(pool, lose);
  }
}

/* Naval: damage-not-always-sink when margin close; else plunder+despawn. */
/*
 * Occupied holds = DOS unit +0x3150, which is the GOODS hold count only — the
 * length of the packed hold arrays, written solely by FUN_15eb_30b8 (add
 * goods, ++) and FUN_15eb_317c (remove hold, --) at viceroy 13301/13339.
 * Boarding never touches it: FUN_1427_10be parks passengers off-map at
 * (-2,-2) and debits only its own local budget. FUN_5bfb_312e (viceroy 98448)
 * reads that byte raw for the -4/hold evasion peel, so a troop-laden ship
 * evades exactly like an empty one — the same goods-only count
 * combat_strength.c's FUN_157e_004a peel (viceroy 8957-8959) uses.
 */
int units_unit_hold_amount(const ColonizeUnit* u, int hold) {
  if (!u || hold < 0 || hold >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  const int amt = u->hold_goods_amount[hold];
  return (amt > 0 && amt < 255) ? amt : 0;
}

int units_hold_amount(const ColonizeUnitPool* pool, int unit_id, int hold) {
  return units_unit_hold_amount(units_get_const(pool, unit_id), hold);
}

int units_holds_used(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u) {
    return 0;
  }
  int used = 0;
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (units_unit_hold_amount(u, i) > 0) {
      used++;
    }
  }
  return used;
}

/* DOS 0352 damage tail zeroes the loser's holds (unit +0x3150 = 0): goods
 * not lifted by the winner vanish and passengers go down with them — a
 * damaged ship limps home empty. */
void units_ship_lose_holds(ColonizeUnitPool* pool, int ship_id) {
  ColonizeUnit* u = units_get(pool, ship_id);
  if (!u) {
    return;
  }
  int pax[COLONIZE_UNIT_CARGO_MAX];
  const int n = u->cargo_count > COLONIZE_UNIT_CARGO_MAX ? COLONIZE_UNIT_CARGO_MAX
                                                        : u->cargo_count;
  for (int i = 0; i < n; ++i) {
    pax[i] = u->cargo_ids[i];
  }
  for (int i = 0; i < n; ++i) {
    (void)units_despawn(pool, pax[i]);
  }
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    u->hold_goods_amount[i] = 0;
    u->hold_goods_type[i] = 0;
  }
}

/*
 * Damage vs sink — DOS FUN_5fef_0352 (viceroy_unpacked.c 99518-99530):
 *
 *   bVar11 = true;                                     // 99523, ship loser
 *   if (winner_type*0xe + 0x523b != 0) {               // @UNIT guns column
 *     uVar16 = loser_type*0xe + 0x523c;                // @UNIT hull column
 *     iVar18 = FUN_281f_04d4(1, guns + uVar16);
 *     bVar11 = iVar18 <= (int)uVar16;                  // <= hull → damaged
 *   }
 *
 * i.e. a gunless victor can only drive the loser off damaged, never sink it.
 * Both call sites (naval loss, coastal-fort fire — the fort's strength stands
 * in for "guns", bugs.md/DOS 0352 doubling the repair bill for a non-ship
 * winner) go through this one helper so the port cannot break ties two ways.
 *
 * The rng == NULL path is port-only (headless tests / deterministic replays).
 * It applies the same mechanical transform this file already uses for the
 * other DOS `roll(1, X+Y) <= X` decision — `atk_wins = attack_str >= defense`
 * in units_fort_vs_ship — to `roll(1, guns+hull) <= hull`, i.e. `hull >= guns`
 * → damaged. hull == guns is an exact coin flip in DOS (@UNIT Privateer and
 * Caravel both have guns == hull), so the tie direction is a port convention,
 * not a DOS fact; the two call sites used to break it in OPPOSITE directions
 * and are now one function so they cannot diverge again.
 */
int units_ship_damage_vs_sink(
  ColonizeDosRng* rng,
  int winner_guns,
  int loser_hull
) {
  if (winner_guns <= 0) {
    return 1;
  }
  if (loser_hull < 0) {
    loser_hull = 0;
  }
  if (!rng) {
    return loser_hull >= winner_guns;
  }
  return dos_rng_range(rng, 1, winner_guns + loser_hull) <= loser_hull;
}

int units_apply_naval_loss_outcome(
  ColonizeUnitPool* pool,
  int loser_id,
  int winner_id,
  int loser_str,
  int winner_str,
  int show_popups,
  const ColonizeCol1Save* col1,
  ColonizeDosRng* rng
) {
  (void)loser_str;
  (void)winner_str;
  ColonizeUnit* lose = units_get(pool, loser_id);
  ColonizeUnit* win = units_get(pool, winner_id);
  if (!lose || !win || !lose->active) {
    return 0;
  }
  const ColonizeUnitType* lt = units_type(pool, lose->type_index);
  const ColonizeUnitType* wt = units_type(pool, win->type_index);
  const int human =
    show_popups && units_combat_human_involved(col1, lose->nation_id, win->nation_id);
  /* FUN_5fef_0352 5fef:07db-0803: both combatants are ships and the fight
   * is visible → 0x4d (COLDIG 10 cheering + fireworks) *before* the
   * damage / sink / seizure split — a naval-win beat, not a capture cue. */
  if (show_popups && units_combat_is_visible(pool, winner_id, loser_id)) {
    units_play_event_sound(0x4d);
  }

  /*
   * DOS FUN_5fef_0352 5fef:05e6: with BOTH parties ships, the winner lifts
   * the loser's cargo hold by hold while it has space (@CARGOCAPTURE per
   * item), and the loser's holds are then zeroed EITHER WAY — the
   * damage-vs-sink roll comes after, so even a survivor limps home empty
   * and its passengers are lost.
   */
  if (units_is_sea(pool, winner_id) && units_is_sea(pool, loser_id)) {
    (void)units_plunder_ship_holds(pool, winner_id, loser_id);
  }
  units_ship_lose_holds(pool, loser_id);

  /*
   * Damage vs sink (DOS 0352 5fef:09xx): roll range(1, winner.guns +
   * loser.hull) <= loser.hull → survives damaged; a gunless victor (@UNIT
   * guns column 0) can never sink, only drive off damaged. This replaces
   * the earlier "close fight" heuristic. DOS then layers the raw 99527-99570
   * fleet-pool / crown-Man-O-War gate on the result — units_naval_damage_gate
   * (bugs.md #867).
   */
  const int wguns = wt ? wt->guns : 0;
  const int lhull = lt ? lt->hull : 0;
  int damaged = units_ship_damage_vs_sink(rng, wguns, lhull);
  /* bugs.md #867: DOS raw 99527-99570 layers the fleet-pool / crown-MoW gate
   * on top of that roll. */
  damaged = units_naval_damage_gate(pool, col1, lose, wt, win->nation_id, damaged);
  /*
   * bugs.md #254: WoI human with no drydock port has no friendly Europe either
   * — the ship goes down instead of limping anywhere (DOS 85139).
   */
  const ColonizeColony* home = NULL;
  if (damaged) {
    home = units_nearest_own_drydock_colony(
      g_units_combat_colonies, lose->nation_id, lose->x, lose->y
    );
    if (!home && col1 && col1->head.game_options.woi &&
        lose->nation_id == (int)col1->head.human_player) {
      damaged = 0; /* DOS 85139: `goto` into the sunk path */
    }
  }
  if (damaged) {
    int wstr = wt ? wt->defense : 0;
    if (!units_is_sea(pool, winner_id)) {
      wstr <<= 1; /* non-ship (fort) winner doubles the repair bill */
    }
    units_ship_enter_repair(pool, lose, wstr, home, col1, human, win->nation_id);
    return 1;
  }

  if (human) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = units_combat_nation_label(col1, lose->nation_id);
    tok.string1 = lt ? lt->name : "Ship";
    tok.string2 = units_combat_nation_label(col1, win->nation_id);
    tok.string3 = wt ? wt->name : "Ship";
    char fb[AI_POPUP_BODY_LEN];
    snprintf(
      fb, sizeof(fb), "%s %s sunk by %s %s!", tok.string0, tok.string1, tok.string2, tok.string3
    );
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_SHIP, "SHIPSUNK", win->nation_id, lose->nation_id, 0, &tok, fb
    );
    units_play_event_sound(0x57); /* UNITS_SFX_SHIP_SUNK: FUN_5fef_0352 (COLDIG 16 sinking) */
  }
  /* FUN_5fef_0352 raw 99653-99690: Royal-flagged hull lost → tax cut (bugs.md #659). */
  (void)units_royal_loss_tax_cut((ColonizeCol1Save*)col1, pool, lose);
  /* FUN_5fef_0352 5fef:0d6c/0d87: a human loser goes back to the map pool
   * (281f_0498(1)), a human winner gets the Military pool (0498(4)). */
  if (col1) {
    if (lose->nation_id >= 0 && lose->nation_id <= 3 &&
        col1->player[lose->nation_id].control == 0) {
      units_set_bgm_pool(1);
    }
    if (win->nation_id >= 0 && win->nation_id <= 3 && col1->player[win->nation_id].control == 0) {
      units_set_bgm_pool(4);
    }
  }
  units_despawn(pool, loser_id);
  return 0;
}

/*
 * FUN_5fef_0f14 kind 3 (raw 99989-100004): the Indian colony raid's "unit"
 * outcome picks a SHIP lying in the colony's port — `FUN_281f_07e0`
 * (unit_index_on_tile of the colony tile), abort unless `FUN_281f_088a`
 * (stack_has_ship), then walk the stack down with `FUN_281f_02e4` until the
 * type byte is 0xd..0x12 — and hands it to the combat resolver as a loser
 * with NO winner:
 *
 *     thunk_FUN_2a1f_06e0(0x281f, unit, 0xffff, 1, unit.x, unit.y)
 *       = FUN_5fef_0352(loser, -1, visible, x, y)
 *
 * With `param_2 < 0` FUN_5fef_0352 skips its entire winner block (no plunder,
 * no capture, no demote; `local_4 = 0x10`) and raw 99567 forces `bVar11 =
 * true`, so the ship ALWAYS survives DAMAGED — exactly what GAME.TXT
 * `@RAIDSHIP` says: "{ship} damaged.  Colonists appalled!". The tail is the
 * ordinary damage tail: holds zeroed (+0x3150 — cargo AND passengers go),
 * damaged bit7 (+0x3148 | 0x80), orders/MP cleared, repair timer preset,
 * ship relocated to the nearest own repair port. The repair bill's "winner
 * strength" is the no-winner stand-in type `local_4 = 0x10` (Privateer's
 * 0x5235 column). DOS's `local_6 <<= 1` non-ship doubling reads the type byte
 * of unit −1 here (an out-of-bounds read below the unit array), so it is not
 * reproduced. The only escape from `bVar11` is 0352's shared WoI
 * human-with-no-repair-port sink (raw 99607).
 *
 * Returns 1 if the ship limped off damaged, 0 if it went down.
 */
int units_raid_damage_ship(ColonizeUnitPool* pool, int ship_id, const ColonizeCol1Save* col1) {
  ColonizeUnit* lose = units_get(pool, ship_id);
  if (!lose || !lose->active || !units_is_sea(pool, ship_id)) {
    return 0;
  }
  const ColonizeUnitType* lt = units_type(pool, lose->type_index);
  const int human = units_combat_human_involved(col1, lose->nation_id, -1);

  units_ship_lose_holds(pool, ship_id);

  int damaged = 1; /* raw 99567: param_2 < 0 → bVar11 = true */
  const ColonizeColony* home = units_nearest_own_drydock_colony(
    g_units_combat_colonies, lose->nation_id, lose->x, lose->y
  );
  if (!home && col1 && col1->head.game_options.woi &&
      lose->nation_id == (int)col1->head.human_player) {
    damaged = 0; /* DOS 85139 / raw 99607: no friendly port at all */
  }
  if (!damaged) {
    if (human && g_units_combat_popups) {
      /* GAME.TXT @SHIPSUNK names the sinker (%STRING2 %STRING3); this arm has
       * no winner unit at all, so enqueue the plain line rather than the
       * section with two empty tokens. */
      char body[AI_POPUP_BODY_LEN];
      snprintf(
        body, sizeof(body), "%s %s sunk!", units_combat_nation_label(col1, lose->nation_id),
        lt ? lt->name : "Ship"
      );
      (void)ai_popup_enqueue_ok_ctx(
        g_units_combat_popups, AI_POPUP_TAG_COMBAT_SHIP, -1, lose->nation_id, 0, NULL, body
      );
      units_play_event_sound(0x57);
    }
    units_despawn(pool, ship_id);
    return 0;
  }

  {
    /* The raid arm has no winner unit: DOS bills the repair against the
     * Privateer's defense column (the raider stand-in). */
    const int pi = units_kind_type_index(pool, UNITS_KIND_PRIVATEER);
    const ColonizeUnitType* pt = pi >= 0 ? units_type(pool, pi) : NULL;
    units_ship_enter_repair(pool, lose, pt ? pt->defense : 0, home, col1, human, -1);
  }
  return 1;
}

/*
 * DOS-LITERAL FUN_5bfb_312e raw 98433-98453 — the ONE naval evasion/slip
 * power. bugs.md #869: the port had two copies that disagreed; this is the
 * single helper both the naval-evade pre-roll and the ship-slow scan use.
 *
 *   local_4 = FUN_281f_090c(unit) & 0xff;        // MAX MP, in thirds
 *   local_4 += 3;
 *   if (type == 0x10) local_4 *= 2;              // Privateer
 *   if (type == 0x0f) local_4 += 3;              // Galleon
 *   local_4 -= 4 * unit[+0x3150];                // GOODS holds in use
 *   if (local_4 < 1) local_4 = 1;
 *
 * Two fidelity points the old copies each got half right:
 *  - 090c is the type's max MP in THIRDS (Man-O-War 18), not the tile count
 *    (`movement`, 6) — bugs.md #869.
 *  - +0x3150 is the GOODS hold count only, never passengers (`cargo_count`)
 *    — bugs.md #868; see units_unit_hold_amount's header.
 */
int units_naval_evade_power(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  const ColonizeUnitType* t = u ? units_type(pool, u->type_index) : NULL;
  if (!u || !t) {
    return 1;
  }
  int p = units_max_mp(pool, unit_id) + 3;
  /* bugs.md #581: @UNIT row class (kind_plus1), not the display spelling. */
  const ColonizeUnitKind k = units_type_kind(t);
  if (k == UNITS_KIND_PRIVATEER) {
    p *= 2;
  }
  if (k == UNITS_KIND_GALLEON) {
    p += 3;
  }
  p -= 4 * units_holds_used(pool, unit_id);
  return p < 1 ? 1 : p;
}

/*
 * DOS 0ec0 on a naval loss: walk the loser's tile stack and apply 0352 to
 * every unit — each ship rolls its own plunder + damage-vs-sink; anything
 * else standing there is destroyed (a ship winner can neither capture nor
 * demote in 0352, so land stackmates fall through to despawn). DOS naval
 * fights never occur on colony tiles, so a colony tile is left untouched
 * (a port garrison must not drown because a ship lost offshore).
 */
void units_sweep_naval_stack_after_loss(
  ColonizeUnitPool* pool,
  int x,
  int y,
  int loser_nation,
  int winner_id,
  int primary_loser_id,
  const ColonizeCol1Save* col1,
  ColonizeDosRng* rng
) {
  if (!pool || loser_nation < 0) {
    return;
  }
  if (g_units_combat_colonies && colonies_id_at(g_units_combat_colonies, x, y) >= 0) {
    return;
  }
  ColonizeUnit* win = units_get(pool, winner_id);
  if (!win || !win->active) {
    return;
  }
  int slot_3 = 0;
  for (ColonizeUnit* u = units_next_on_tile(pool, x, y, &slot_3); u != NULL;
       u = units_next_on_tile(pool, x, y, &slot_3)) {
    if (u->nation_id != loser_nation || u->id == winner_id || u->id == primary_loser_id) {
      continue;
    }
    if (units_is_sea(pool, u->id)) {
      (void)units_apply_naval_loss_outcome(pool, u->id, winner_id, 0, 0, 1, col1, rng);
    } else {
      (void)units_despawn(pool, u->id);
    }
  }
}

/*
 * DS:0x54f6 attitude writer. `(origin*9 + euro)*2 + 0x54f6` is field +10 of
 * the stride-0x12 settlement record, i.e. ColonizeCol1Tribe.alarm[euro]
 * {friction, attacks} — DOS stores 0 into the whole word, so both bytes go.
 * Repointed 2026-09-08 off the phantom `indian_tension` parallel array.
 */
void units_indian_tension_clear(
  const ColonizeCol1Save* col1, int tribe_index, int euro_nation
) {
  if (!col1 || !col1->tribe || euro_nation < 0 || euro_nation > 3) {
    return;
  }
  if (tribe_index < 0 || (uint16_t)tribe_index >= col1->head.tribe_count) {
    return;
  }
  ColonizeCol1Save* mut = (ColonizeCol1Save*)col1;
  col1_tribe_attitude_set(&mut->tribe[tribe_index], euro_nation, 0);
}

/*
 * FUN_5fef_1b0e's own DS:0x54f6 discharge, the first of its two sites
 * (viceroy_unpacked.c:101039-101041) — the same clear FUN_5fef_0f14 does on
 * the raid path (ai_contact_indian_raids, docs/indians.md).
 *
 * DOS arm: `if (3 < attacker_nation && defender_nation < 4)` — a native
 * attacker against a European defender — then
 * `if (local_10 < 0 || DS:0x8db8 != 0 || attacker_won)`. `local_10` is the
 * nearest-colony scan FUN_281f_0614(x, y, -1, -1) over the DEFENDER'S tile
 * and DS:0x8db8 is that scan's distance output, so the two leading terms
 * together read "the fight was not on a colony tile". The `else` limb is
 * DOS's colony-raid handoff to FUN_5fef_0f14, which carries its own clear.
 * Net rule: every resolved native-vs-European land fight discharges the
 * raiding tribe's tension toward that European, EXCEPT a native attack that
 * loses at a colony (which discharges it through the raid path instead).
 *
 * Index is the ATTACKER's +0x314a home tribe (home_tribe_id) against the
 * DEFENDER's nation nibble — same index space as the raid-path clear.
 */
void units_indian_attack_tension_clear(
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  int atk_nation,
  int atk_home_tribe,
  int def_nation,
  int def_x,
  int def_y,
  int atk_wins
) {
  if (atk_nation < 4 || atk_nation > 11 || def_nation < 0 || def_nation > 3) {
    return;
  }
  if (!atk_wins && colonies && colonies_id_at(colonies, def_x, def_y) >= 0) {
    return; /* DOS routes this to FUN_5fef_0f14's clear, not this one */
  }
  units_indian_tension_clear(col1, atk_home_tribe, def_nation);
}

/*
 * FUN_5fef_1b0e's `local_a6` alarm delta (raw 101043-101196), the write that
 * sits beside the tension clear above. Every value in the table is NEGATIVE —
 * a native attack VENTS the tribe's alarm toward that European, the same way
 * the discharge empties its tension row:
 *
 *   natives raze the colony (bVar28 && local_c)        −50 flat
 *   natives beat an undefended colony, colony survives  difficulty − 10
 *   natives win an ordinary field fight (!bVar28)       difficulty/2 − 5
 *   natives lose a field fight                          0 (no delta)
 *
 * The `difficulty` term is added only when the European side is
 * human-controlled (`*(char *)(nation * 0x34 + 0x543f) == '\0'`), so a HIGHER
 * difficulty gives back LESS alarm relief.
 *
 * Gate (raw 101129-101136): `FUN_281f_0a38(DS:0x8d50, euro) & 2` == 0 —
 * `FUN_15b3_0004(indian_nation, euro)` is the Indian record's relation byte
 * toward that European (`indian[].euro_diplo[]` here) and bit 1 is the WAR
 * bit, so the vent applies only while the tribe is NOT already at war with
 * that European. Delta itself goes through FUN_281f_0d6c = FUN_4cc6_00f2 =
 * ai_diplo_indian_alarm_delta.
 */
int units_indian_attack_alarm_vent_amount(
  const ColonizeCol1Save* col1, int def_nation, int base, int difficulty_shift
) {
  int amount = 0;
  if (col1 && def_nation >= 0 && def_nation <= 3 && col1->player[def_nation].control == 0) {
    amount = (int)col1->head.difficulty >> difficulty_shift;
  }
  return amount - base;
}

/*
 * DOS `bVar28`: the defender 1b0e is resolving was auto-spawned for an
 * undefended settlement, not a unit standing on the map. The port spawns the
 * same stand-in from units_revere_defend_colony_tile, which raises this while
 * its units_resolve_land_combat_ff call runs.
 */
bool g_units_colony_autodefender = false;

/*
 * One-shot latch, cleared at the top of every units_try_move. DOS runs the
 * whole of 1b0e once per attack and picks exactly ONE `local_a6` row, but the
 * port splits the same beat in two: the resolver fights the defender and then
 * units_try_move walks the winner into the colony, where the Indian arm plays
 * DOS's `bVar28` (undefended colony) rows. Without the latch a native that
 * beats a real garrison and steps in would vent both the field-fight row and
 * the undefended-colony row for one attack.
 */
bool g_units_indian_combat_vent_done = false;

void units_indian_attack_alarm_vent(
  const ColonizeCol1Save* col1, int atk_nation, int def_nation, int delta
) {
  if (!col1 || atk_nation < 4 || atk_nation > 11 || def_nation < 0 || def_nation > 3) {
    return;
  }
  if (delta == 0) {
    return; /* DOS: `if (local_a6 != 0)` */
  }
  /* FUN_15b3_0004(indian_nation, euro) & 2 — already at war, no relief. */
  if ((col1->indian[atk_nation - 4].euro_diplo[def_nation] & COL1_INDIAN_WAR_BIT) != 0) {
    return;
  }
  ai_diplo_indian_alarm_delta((ColonizeCol1Save*)col1, atk_nation, def_nation, delta);
}
/* ===================== Outcome popups, native fallout, LCR resolution & combat sound hooks (units_combat_outcome_popups .. units_combat_music_sting) ===================== */


/* win/lose are PRE-LOSS snapshots: bugs.md #240 — DOS 1b0e applies the 0352
 * loss outcome (demote/damage/capture popups) first and fills @EUROPEWIN /
 * @EUROPELOSE after, so this runs after the loser may already be despawned. */
void units_combat_outcome_popups(
  const ColonizeUnitPool* pool,
  const ColonizeUnit* win,
  const ColonizeUnit* lose,
  int atk_wins,
  int atk_nation,
  int def_nation,
  int is_naval,
  int ambush,
  const ColonizeCol1Save* col1
) {
  (void)ambush; /* @INDIANWIN1/2 seizure chrome stays with ai_contact's arm. */
  if (!units_combat_human_involved(col1, atk_nation, def_nation)) {
    return;
  }
  if (!win || !lose) {
    return;
  }

  if (!is_naval) {
    /*
     * Euro attacker (incl. vs natives): @EUROPEWIN / @EUROPELOSE.
     * Native attacker vs human: @INDIANWIN0/@INDIANLOSE below (ai_contact's
     * ambush arm suppresses it and draws its richer version itself).
     * Cite: FUN_5fef_1b0e both-euro gate; indian raid ambush fill.
     */
    if (atk_nation < 4) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      const int place_x = atk_wins ? lose->x : win->x;
      const int place_y = atk_wins ? lose->y : win->y;
      tok.string0 = units_combat_nation_label(col1, atk_nation);
      tok.string1 = units_combat_nation_label(col1, def_nation);
      {
        const ColonizeUnit* def_u = atk_wins ? lose : win;
        tok.string2 = units_combat_unit_label(pool, def_u);
      }
      tok.string3 = units_combat_place_label(col1, place_x, place_y);
      if (atk_wins) {
        tok.string4 = units_combat_defeat_verb(1, -1);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_EUROPE,
          "EUROPEWIN",
          atk_nation,
          def_nation,
          0,
          &tok,
          "");
      } else {
        const ColonizeUnit* def_u = win;
        tok.string4 = units_combat_defeat_verb(0, def_u->type_index);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_EUROPE,
          "EUROPELOSE",
          atk_nation,
          def_nation,
          0,
          &tok,
          "");
      }
    } else if (atk_nation >= 4 && atk_nation <= 11 && def_nation >= 0 && def_nation <= 3 &&
               !g_units_native_chrome_owned) {
      /*
       * Native attacker vs the human on the generic combat path (a Brave
       * stepping onto a defended tile via units_try_move, alarm marches):
       * DOS FUN_5fef_1b0e fills the @INDIANWIN/@INDIANLOSE chrome on every
       * land resolution (0x5230 @UNIT-name subst, viceroy ~101087) — the
       * port only drew it from ai_contact's ambush arm, so these attacks
       * resolved silently (user-reported). ai_contact keeps its richer
       * version (seizure lines, chief portrait) via the owned flag.
       */
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      const int place_x = atk_wins ? lose->x : win->x;
      const int place_y = atk_wins ? lose->y : win->y;
      const ColonizeUnit* def_u = atk_wins ? lose : win;
      tok.string0 = units_combat_nation_label(col1, atk_nation); /* tribe */
      tok.string1 = units_combat_nation_label(col1, def_nation);
      tok.string2 = units_combat_unit_label(pool, def_u);
      tok.string3 = units_combat_place_label(col1, place_x, place_y);
      if (atk_wins) {
        /* @INDIANWIN0: {tribe} ambush {nation unit} near {place}! */
        tok.string4 = tok.string0;
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_AMBUSH,
          "INDIANWIN0",
          atk_nation,
          def_nation,
          0,
          &tok,
          "");
      } else {
        /* @INDIANLOSE: {nation unit} {defeat} {tribe} near {place}!
         * LABELS.TXT @MISC rows 73/74 — unit subjects type_index ≥7 use "defeats". */
        tok.string4 = units_combat_defeat_verb(0, def_u->type_index);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_AMBUSH,
          "INDIANLOSE",
          atk_nation,
          def_nation,
          0,
          &tok,
          "");
      }
    }
    /* bugs.md #238: no @SEIZURELAND here — DOS shows it only from the 0512
     * entry-seizure path (FUN_43f7_0512, ported in ai_king.c), never as a
     * combat-win follow-up. A damaged/destroyed loser is not "captured". */
  }
}

/* PEDIA Francis Drake: privateer combat strengths +50% → multiply by 3/2. */
int units_drake_scale_strength(
  const ColonizeUnitPool* pool,
  const ColonizeUnit* unit,
  int strength,
  const ColonizeCol1Save* col1
) {
  if (!pool || !unit || !col1 || strength <= 0) {
    return strength;
  }
  const ColonizeUnitType* t = units_type(pool, unit->type_index);
  if (!units_type_is_privateer(t)) {
    return strength;
  }
  if (!founding_fathers_nation_has(col1, unit->nation_id, FF_FRANCIS_DRAKE)) {
    return strength;
  }
  return (strength * 3) / 2;
}

static int units_count_nation_on_tile(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id
) {
  if (!pool) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->aboard_ship_id >= 0) {
      continue;
    }
    if (u->x == x && u->y == y && u->nation_id == nation_id) {
      ++count;
    }
  }
  return count;
}

static bool units_tile_has_tribe(const ColonizeCol1Save* col1, int x, int y) {
  if (!col1 || !col1->tribe) {
    return false;
  }
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    if ((int)col1->tribe[i].x == x && (int)col1->tribe[i].y == y) {
      return true;
    }
  }
  return false;
}

static int col1_destroy_tribe_at(
  ColonizeCol1Save* col1,
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int x,
  int y
) {
  if (!col1 || !col1->tribe || col1->head.tribe_count == 0) {
    return -1;
  }
  int found = -1;
  int nation_id = -1;
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    if ((int)col1->tribe[i].x == x && (int)col1->tribe[i].y == y) {
      found = (int)i;
      nation_id = (int)col1->tribe[i].nation_id;
      break;
    }
  }
  if (found < 0 || nation_id < 4) {
    return -1;
  }

  village_trade_intel_forget_tile(x, y); /* sidebar Buys/Sells rows go with it */
  /* FUN_4d56_00e0 entry (raw 81307-81308): FUN_281f_068c(x, y, 2, 0) →
   * FUN_137f_015e AND-clears settlement bit 0x02 of the mask plane (DS:0x160)
   * and nothing else: a real road (0x08) stays, but the village's implied
   * road art (map_tile_has_road_art = road || has_city) goes. The port left
   * the bit set until the next save/load occupancy rebuild, so the razed
   * site kept drawing a road (bugs.md #550). */
  if (map) {
    map_occupancy_set_layer2(map, x, y, MAP_OCCUPANCY_HAS_CITY, false);
  }
  const uint16_t old_count = col1->head.tribe_count;
  if (found + 1 < (int)old_count) {
    memmove(
      &col1->tribe[found],
      &col1->tribe[found + 1],
      ((size_t)old_count - (size_t)found - 1u) * sizeof(ColonizeCol1Tribe)
    );
  }
  col1->head.tribe_count = (uint16_t)(old_count - 1u);

  /*
   * DS:0x54f6 attitude[euro] IS field +10 of the settlement record (stride
   * 0x12 = the "9 words" of the 0x54f6 indexing), so the memmove above
   * already carried it — no parallel array to shift since the phantom
   * `indian_tension` was retired 2026-09-08.
   */

  /* FUN_4d56_00e0 (raw 81310-81319): units bound to the destroyed village
   * are DESTROYED with it (any Indian-owned unit whose +0x314a home-village
   * byte names it), higher indexes shift down. The old port only cleared
   * the binding to -1. */
  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units->units[i];
      if (!u->active || u->home_tribe_id < 0) {
        continue;
      }
      /* DOS-LITERAL FUN_4d56_00e0 raw 81310-81319: the whole loop body sits
       * under `if (3 < (*(byte*)(i*0x1c + 0x3147) & 0xf))` — Euro units are
       * never destroyed and never renumbered here (bugs.md #839). */
      if (u->nation_id < 4) {
        continue;
      }
      if (u->home_tribe_id == found) {
        (void)units_despawn(units, u->id);
      } else if (u->home_tribe_id > found) {
        u->home_tribe_id--;
      }
    }
  }

  /* FUN_4d56_00e0 tail (raw 81332-81346): decrement the nation's village
   * count (DS:0x962a); at zero the nation goes EXTINCT (indian +3 bit 0x80,
   * GAME.TXT @EXTINCT 0x14d4); otherwise the dead village takes its share
   * of the nation's horses: field += field / (-1 - remaining) for
   * horse_herds (+8) and horse_breeding (+10). */
  {
    const int idx = nation_id - 4;
    if (idx >= 0 && idx < 8) {
      uint8_t* vc = &col1->stuff.tribe_village_counts[idx];
      if (*vc > 0) {
        (*vc)--;
      }
      if (*vc == 0) {
        col1->indian[idx].extinct = 1;
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, nation_id);
        char fb[160];
        snprintf(fb, sizeof(fb), "The %s tribe has become extinct!", tok.string0);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_COLONY, "EXTINCT", nation_id, -1, 0, &tok, fb
        );
      } else {
        const int div = -1 - (int)*vc;
        ColonizeCol1Indian* ind = &col1->indian[idx];
        ind->horse_herds = (uint8_t)((int)ind->horse_herds + (int)ind->horse_herds / div);
        ind->horse_breeding =
          (uint16_t)((int)ind->horse_breeding + (int)ind->horse_breeding / div);
      }
    }
  }

  if (map) {
    map_set_owner_nibble(map, x, y, 0x0f);
  }
  return nation_id;
}

int units_conquest_treasure_gold(
  const ColonizeCol1Save* col1,
  int attacker_nation_id,
  int tribe_nation_id,
  ColonizeDosRng* rng,
  int rich_capital
) {
  /*
   * Peel FUN_5fef_31ea amount → gold×100 (viceroy_unpacked.c ~101407–101495).
   * Locals: -6 Cortes FF10, -0xa8 Spanish (nation==2), -0xcc rich/capital
   * (callers: tribe.state.capital from fallout).
   *
   * The band switch reads `*(char *)(*(int *)0x8d4e + 2)` — the razed
   * tribe's Indian record +2 = `tech` (NAMES.TXT @TRIBES column 4: 0
   * Semi-Nomadic .. 3 Civilized), bound by `FUN_281f_0a42(nation - 4)` just
   * above. It is NOT the game difficulty (bugs.md #549: an Arawak village
   * at difficulty 2 paid 11000; tech 1 caps a Cortes Spaniard at 1200).
   */
  if (!rng || !col1 || attacker_nation_id < 0 || attacker_nation_id > 3) {
    return 0;
  }
  if (tribe_nation_id < 4 || tribe_nation_id > 11) {
    return 0;
  }
  const int cortes =
    founding_fathers_cortes_guarantees_conquest_treasure(col1, attacker_nation_id) ? 1 : 0;
  const int spanish = (attacker_nation_id == 2) ? 1 : 0;
  const int rich = rich_capital ? 1 : 0;
  /* Any tech > 3 hits no DOS case and pays 0; the port clamps nothing. */
  const int tech = (int)col1->indian[tribe_nation_id - 4].tech;

  int amount = 0; /* DOS -0xce before ×100 */
  if (tech == 0) {
    const int hi = ((spanish == 0) ? 3 : 0) + 3;
    const int r0 = dos_rng_range(rng, 0, hi);
    if (r0 == 0 || rich || cortes) {
      amount = dos_rng_range(rng, 2, 4);
    }
    if (rich) {
      amount <<= 1;
    }
    if (cortes) {
      amount += amount >> 1;
    }
  } else if (tech == 1) {
    /* -0x62 set from Spanish but roll is always 04d4(0,2). */
    const int r0 = dos_rng_range(rng, 0, 2);
    if (r0 == 0 || rich || cortes) {
      amount = dos_rng_range(rng, 3, 8);
    }
    if (rich) {
      amount <<= 1;
    }
    if (cortes) {
      amount += amount >> 1;
    }
  } else if (tech == 2) {
    const int lo = rich ? 4 : 2;
    const int hi = rich ? 10 : 6;
    const int r = dos_rng_range(rng, lo, hi);
    amount = (r + (cortes ? 6 : 0) + (spanish ? 3 : 0)) * 10;
  } else if (tech == 3) {
    /* tech 3: 16-bit wrap of (cc==0 ? 0xfff7 : 0) + 0x19 → 16 or 25. */
    amount = dos_rng_range(rng, 0, 4) + 2;
    const uint16_t mult16 =
      (uint16_t)((rich ? 0 : 0xfff7) + 0x19 + (cortes ? 10 : 0) + (spanish ? 5 : 0));
    amount *= (int)mult16;
  }
  if (amount <= 0) {
    return 0;
  }
  return amount * 100;
}

/*
 * FUN_5fef_31ea / 1b0e subjugated convert-join threshold (before rng).
 * mission 0xff → ineligible (-1). Else low-nibble must equal attacker.
 * Base 4, Jesuit bit0x10 → 8; Spanish +4; Sepulveda +4; Las Casas −4.
 * Succeed when dos_rng_range(0,12) < threshold. Cite: viceroy ~101155–101184;
 * PEDIA @FATHER23; GAME.TXT @INDIANSLAVES.
 */
static int units_subjugated_convert_join_threshold(
  const ColonizeCol1Save* col1,
  int attacker_nation_id,
  uint8_t mission
) {
  if (!col1 || attacker_nation_id < 0 || attacker_nation_id > 3) {
    return -1;
  }
  if ((int8_t)mission < 0) {
    return -1; /* COL1_TRIBE_MISSION_NONE 0xff */
  }
  if ((mission & COL1_TRIBE_MISSION_NATION_MASK) != (uint8_t)attacker_nation_id) {
    return -1;
  }
  int thr = (mission & COL1_TRIBE_MISSION_JESUIT_BIT) ? 8 : 4;
  if (attacker_nation_id == 2) {
    thr += 4; /* Spanish nation id — same as Cortes peel */
  }
  if (founding_fathers_sepulveda_convert_join_bonus(col1, attacker_nation_id)) {
    thr += 4;
  }
  if (founding_fathers_nation_has(col1, attacker_nation_id, FF_BARTOLOME_DE_LAS_CASAS)) {
    thr -= 4;
  }
  return thr;
}

/* Spawn Colonists + Convert profession (@JOB 27). Returns unit id or -1. */
static int units_spawn_subjugated_convert(
  ColonizeUnitPool* units,
  int x,
  int y,
  int nation_id
) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return -1;
  }
  int ti = units_kind_type_index(units, UNITS_KIND_COLONIST);
  if (ti < 0) {
    ti = units_kind_type_index(units, UNITS_KIND_COLONIST);
  }
  if (ti < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(units, ti, x, y);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation_id);
  u->profession = UNITS_JOB_CONVERT; /* NAMES @JOB Convert / COLONIZE_PROF_CONVERT */
  return id;
}

/*
 * FUN_5fef_1b0e dwelling-destroy arm (raw 100661-100673), right after
 * FUN_291f_0248 (= FUN_4d56_00e0) razed the settlement: when the village's
 * mission byte (local_62 = record +5, read at raw 100410) names the
 * CONQUEROR in its low nibble, DOS spawns @UNIT type 3 (Missionary) for the
 * conqueror on the village tile (FUN_281f_095c(3, nation, x, y)); with the
 * Jesuit bit 0x10 it also writes profession byte +0x315b = 0x18 (@JOB 24,
 * Jesuit Missionary). A rival nation's mission is lost with the village.
 * Returns unit id or -1.
 */
static int units_spawn_mission_return(
  ColonizeUnitPool* units,
  int x,
  int y,
  int attacker_nation_id,
  uint8_t mission
) {
  if (!units || attacker_nation_id < 0 || attacker_nation_id > 3) {
    return -1;
  }
  /* DOS-LITERAL: (local_62 & 0xf) == uVar16; 0xff (no mission) gives 0xf. */
  if ((mission & COL1_TRIBE_MISSION_NATION_MASK) != (uint8_t)attacker_nation_id) {
    return -1;
  }
  const int ti = units_kind_type_index(units, UNITS_KIND_MISSIONARY);
  if (ti < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(units, ti, x, y);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, attacker_nation_id);
  if ((mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0) {
    u->profession = UNITS_JOB_MISSIONARY; /* +0x315b = 0x18 */
  }
  return id;
}

bool units_try_native_settlement_fallout_w(
  const ColonizeWorld* w,
  int attacker_nation_id,
  int defender_nation_id,
  int tile_x,
  int tile_y,
  int gold_amount
) {
  ColonizeCol1Save* col1 = w->col1;
  ColonizeUnitPool* units = w->units;
  ColonizeWorldMap* map = w->map;
  ColonizeDosRng* rng = w->rng;

  /*
   * Post-win fallout for FUN_5fef_31ea (structural): destroy native village
   * when explicitly conquering an empty dwelling (caller: village temp Brave
   * arm when population < 2). Convert-join before destroy when mission owned
   * by attacker; Cortes treasure after. Not triggered merely by killing a map
   * Brave on the tile.
   */
  if (!col1 || !units || defender_nation_id < 4) {
    return false;
  }
  if (!units_tile_has_tribe(col1, tile_x, tile_y)) {
    return false;
  }
  if (units_count_nation_on_tile(units, tile_x, tile_y, defender_nation_id) > 0) {
    return false;
  }

  /*
   * FUN_5fef_31ea stack-local -0xcc (rich): map to ColonizeCol1TribeState.capital
   * before destroy. Cite: col1_save.h capital bit; fandom capital / Aztec treasure;
   * viceroy_unpacked.c ~101416–101466 (-0xcc doubles / boosts amount).
   * Mission byte (+5): convert-join owner + Jesuit bit0x10.
   */
  int rich_capital = 0;
  uint8_t mission = COL1_TRIBE_MISSION_NONE;
  if (col1->tribe) {
    for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
      if ((int)col1->tribe[i].x == tile_x && (int)col1->tribe[i].y == tile_y) {
        rich_capital = col1->tribe[i].state.capital ? 1 : 0;
        mission = col1->tribe[i].mission;
        break;
      }
    }
  }

  const int tribe_nation = col1_destroy_tribe_at(col1, units, map, tile_x, tile_y);
  if (tribe_nation < 0) {
    return false;
  }

  /* bugs.md #551: the conqueror's own mission comes back as a Missionary on
   * the razed site (FUN_5fef_1b0e raw 100667-100672, right after 00e0). */
  if (attacker_nation_id >= 0 && attacker_nation_id < 4) {
    (void)units_spawn_mission_return(units, tile_x, tile_y, attacker_nation_id, mission);
  }

  /*
   * Subjugated convert-join. DOS order (FUN_5fef_1b0e): destroy (00e0) →
   * mission return → convert → treasure; the convert used to run before the
   * destroy here (moved 2026-09-21, bugs.md #551). Cite: FUN_5fef_1b0e
   * ~101155–101184; @INDIANSLAVES 0x1cbf.
   */
  if (attacker_nation_id >= 0 && attacker_nation_id < 4 && rng) {
    const int thr =
      units_subjugated_convert_join_threshold(col1, attacker_nation_id, mission);
    if (thr >= 0) {
      const int roll = dos_rng_range(rng, 0, 12);
      if (roll < thr) {
        (void)units_spawn_subjugated_convert(units, tile_x, tile_y, attacker_nation_id);
        /*
         * @INDIANSLAVES (0x1cbf, viceroy_unpacked.c 101176-101181): DOS
         * announces the convert to a HUMAN conqueror, with slot 0 = the
         * tribe's name and slot 1 = the conquering nation's adjective, in
         * the same `attacker < 4 && not AI-run` gate the rest of 1b0e's
         * chrome uses. The spawn itself was already ported; only the popup
         * was missing (2026-09-16).
         */
        if (attacker_nation_id == g_units_combat_human_nation) {
          PopupMsgTokens stok;
          memset(&stok, 0, sizeof(stok));
          stok.string0 = units_combat_nation_label(col1, defender_nation_id);
          stok.string1 = units_combat_nation_label(col1, attacker_nation_id);
          units_combat_enqueue_tok(
            AI_POPUP_TAG_COMBAT_COLONY, "INDIANSLAVES", attacker_nation_id,
            defender_nation_id, 0, &stok, ""
          );
        }
      }
    }
  }


  if (attacker_nation_id >= 0 && attacker_nation_id < 4) {
    /*
     * col1_save.h ColonizeCol1Nation.villages_burned; reports.c scores
     * villages_penalty = -(difficulty+1)*villages_burned. Increment on
     * successful tribe destroy only.
     */
    if (col1->nation[attacker_nation_id].villages_burned < 255u) {
      col1->nation[attacker_nation_id].villages_burned++;
    }
    if (rich_capital) {
      /*
       * Fandom Capital destroy: hostile tribe surrenders once — hostility
       * reset + peace; no new capital (destroyed). Cite: docs/fandom_col1994.md.
       *
       * DS:0x54f6 discharge, DOS 1b0e site 2 (LAB_5fef_362a,
       * viceroy_unpacked.c:101289-101298): gated on `local_c != 0 &&
       * local_ce != 0` = dwelling destroyed AND it carried the capital bit
       * (record +3 bit 2). DOS then walks the whole DS:0x539a settlement
       * array and zeroes `(i*9 + euro)*2 + 0x54f6` for every record whose
       * `+2` type byte (= owner nation - 4, settlement_record_8d4a.md)
       * equals the bound Indian nation index at DS:0x8d52 — the NATION-WIDE
       * grudge against the conqueror is spent when its capital falls, not
       * just the razed settlement's. Translated into the port's index space
       * (the attitude word is keyed by village, docs/indians.md) that is every
       * tribe of the razed capital's nation. The alarm side of the same block
       * (FUN_281f_030c/0d6c: clamp alarm DOWN to 15 when above) is what
       * ai_diplo_indian_capital_surrender models.
       */
      ai_diplo_indian_capital_surrender(col1, tribe_nation, attacker_nation_id);
      /*
       * @INDIANBOW (0x1cd7, viceroy_unpacked.c 101300-101306): the tribe
       * bows and cedes the land it occupies. DOS shows it at the tail of
       * the same capital-fall block, to a human conqueror only, with slot 0
       * = the tribe name and slot 1 = the conqueror's adjective (2026-09-16
       * — the mechanics were ported, the popup was not).
       */
      if (attacker_nation_id == g_units_combat_human_nation) {
        PopupMsgTokens btok;
        memset(&btok, 0, sizeof(btok));
        btok.string0 = units_combat_nation_label(col1, tribe_nation);
        btok.string1 = units_combat_nation_label(col1, attacker_nation_id);
        char bfb[224];
        bfb[0] = '\0';
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_COLONY, "INDIANBOW", attacker_nation_id,
          tribe_nation, 0, &btok, bfb
        );
      }
      if (col1->tribe) {
        for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
          if ((int)col1->tribe[ti].nation_id == tribe_nation) {
            units_indian_tension_clear(col1, (int)ti, attacker_nation_id);
          }
        }
      }
    } else {
      ai_diplo_indian_relation_delta(col1, tribe_nation, attacker_nation_id, -5);
      ai_diplo_indian_hostility_sync(col1, attacker_nation_id);
    }
  }

  /*
   * bugs.md #381: treasure is NOT Cortes-gated. FUN_5fef_31ea runs the peel for
   * every conqueror (viceroy_unpacked.c ~101407-101495) — Cortes (local -6) is
   * only one of the three "or" terms that let the low-difficulty roll pay out
   * (`roll == 0 || rich || cortes`) plus a +50% / +10-per-unit bonus, and at
   * difficulty 2 and 3 the amount is unconditional for everyone. The port
   * wrapped the whole block in the Cortes test, so a player without him could
   * burn any number of villages and never see a Treasure Train.
   */
  if (attacker_nation_id >= 0 && attacker_nation_id < 4) {
    /* @LOOT/@LOOT2 slots 1/2 (FUN_5fef_31ea raw ~101470): the tribe name
     * (FUN_281f_09a4) and @LEVELS[rich ? 4 : tech]. */
    const int level_row =
      rich_capital ? 4 : (int)col1->indian[(tribe_nation - 4) & 7].tech;
    const char* loot_level = (level_row >= 0 && level_row < 5) ? g_units_levels[level_row] : "";
    int gold = gold_amount;
    if (gold <= 0) {
      gold = units_conquest_treasure_gold(
        col1, attacker_nation_id, tribe_nation, rng, rich_capital);
    }
    if (gold > 0) {
      (void)units_spawn_treasure_train(units, tile_x, tile_y, attacker_nation_id, gold);
      if (units_combat_human_involved(col1, attacker_nation_id, defender_nation_id)) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, attacker_nation_id);
        tok.string1 = units_combat_nation_label(col1, tribe_nation);
        tok.string2 = loot_level;
        tok.number0 = gold;
        tok.has_number0 = true;
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_LOOT, "LOOT", attacker_nation_id, defender_nation_id, gold, &tok,
          "");
      }
    } else if (units_combat_human_involved(col1, attacker_nation_id, defender_nation_id)) {
      /* Peel came up empty — DOS 0x1cd1 (@LOOT2), not @NOLOOT. */
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = units_combat_nation_label(col1, attacker_nation_id);
      tok.string1 = units_combat_nation_label(col1, tribe_nation);
      tok.string2 = loot_level;
      units_combat_enqueue_tok(
        AI_POPUP_TAG_COMBAT_LOOT,
        "LOOT2",
        attacker_nation_id,
        defender_nation_id,
        0,
        &tok,
        "");
    }
  }
  return true;
}


/*
 * FUN_65dd_0004 outcome kinds. The dialog tag is literally built from the
 * case number: DOS strcpy's DS:0x1dae ("LOSTCITY", VICEROY.EXE@128846) into
 * a local and appends `local_8` (FUN_281f_0182), so case N == @LOSTCITYN
 * one-to-one; DS:0x1db7 = "BURIAL" + variant and DS:0x1dbe = "SCREWED" sit
 * right after it, and the two session counters below (DS:0x1dc6/0x1dc7)
 * are the very next bytes. This settles the case-identity question the
 * P7.1 pass flagged: case 2 = Cibola (spawns unit type 10 = Treasure with
 * +0x315b = value/100), case 9 = survivors (spawns type 0 = Colonist).
 */
typedef enum ColonizeLcrOutcome {
  COLONIZE_LCR_NOTHING = 0,     /* @LOSTCITY6 */
  COLONIZE_LCR_SMALL_TREASURE,  /* @LOSTCITY3 */
  COLONIZE_LCR_CHIEFS_GIFT,     /* @LOSTCITY7 */
  COLONIZE_LCR_BURIAL_MOUNDS,   /* @LOSTCITY4 → @BURIAL1/2/3 / @SCREWED */
  COLONIZE_LCR_TRESPASS_ANGER,  /* @LOSTCITY8 */
  COLONIZE_LCR_SURVIVORS_JOIN,  /* @LOSTCITY9 */
  COLONIZE_LCR_FOUNTAIN_OF_YOUTH, /* @LOSTCITY1 */
  COLONIZE_LCR_VANISHES,        /* @LOSTCITY5 */
  COLONIZE_LCR_CIBOLA           /* @LOSTCITY2 */
} ColonizeLcrOutcome;

/*
 * DS:0x1dc6 / DS:0x1dc7 — two byte counters that live in the EXE's data
 * segment right after the "SCREWED" string, never saved: total rumours
 * explored this process (any nation) and total Cibola finds this process.
 * FUN_65dd_0004 is their only reader/writer. Case 1 (Fountain of Youth)
 * needs >= 4 rumours explored; case 2 (Cibola) is capped at 7 per session.
 *
 * There is deliberately no reset: DOS never clears them, not even across a
 * New Game in the same process, so they run for the lifetime of the process.
 * A reset helper existed here unused and was deleted 2026-09-08 — wiring one
 * would diverge from DOS.
 */
static uint8_t s_lcr_explored_total = 0;
static uint8_t s_lcr_cibola_total = 0;

/* FUN_4cc6_0356-shaped nearest-tribe scan; -1 if none. out_dist (optional)
 * receives the winning tile-distance (manhattan, matching the rest of this
 * file's distance style) — DOS leaves it in DS:0x8db8. */
static int units_lcr_nearest_tribe_dist(
  const ColonizeCol1Save* col1, int x, int y, int* out_dist
) {
  if (!col1 || !col1->tribe) {
    return -1;
  }
  int best = -1;
  int best_d = 0x7fffffff;
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* tr = &col1->tribe[i];
    const int dx = x - (int)tr->x;
    const int dy = y - (int)tr->y;
    const int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    if (d < best_d) {
      best_d = d;
      best = tr->nation_id;
    }
  }
  if (out_dist) {
    *out_dist = best_d;
  }
  return best;
}

/* Credits the one treasury for `nation` (europe_nation_gold_add picks the live
 * store: EuropeScreen.gold for the bound/human nation, the record otherwise). */
static void units_lcr_credit_gold(
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  int nation,
  int amount
) {
  if (!col1 || amount <= 0 || nation < 0 || nation >= 4) {
    return;
  }
  (void)human_nation; /* the accessor picks the store by nation (audit G3) */
  europe_nation_gold_add(europe, col1, nation, (long)amount);
}

/* Everything the FUN_65dd_0004 roll loop decides before the dispatch tail:
 * the outcome plus its magnitudes, so the apply step below is side-effect
 * only (DOS interleaves the Cibola spawn into the loop; same result). */
typedef struct ColonizeLcrRoll {
  ColonizeLcrOutcome outcome;
  int gate;              /* local_c: RNG(1,100)+skill*10, reused by burial */
  int gold;              /* local_12: flat gold (cases 3/7) */
  int treasure_hundreds; /* local_34: Cibola treasure-train value / 100 */
  int trespass_tribe;    /* case 8: tribe nation hit by the hidden alarm, -1 */
  int trespass_mag;      /* local_38 */
} ColonizeLcrRoll;

/*
 * Real FUN_65dd_0004 case-selection state machine (viceroy:103462-103618).
 *   - `skill` (local_36): 0 = not Scout-type, 1 = Scout, 2 = Seasoned Scout.
 *   - `de_soto_reroll` (bVar4): FF7 owned AND Scout-type (103454-103458);
 *     bumps `skill` once more and rerolls any "Nothing" instead of
 *     accepting it (103617, 103480/103488/103553).
 *   - `floor` ratchets 1→2→3 across reroll attempts, `raw = max(floor,
 *     RNG(1,9))` (103464-103472); `gate` = RNG(1,100)+skill*10 (103473).
 *   - case-5/8 "kicker": a fresh 5 or 8 sticks 1-in-(skill+1) (103476-91).
 *   - Terrain qualify (local_10 = FUN_281f_078c = terrain_class_at, the
 *     pedia class: 0-7 clear, 8-23 forest with type = &7, 27 mountain,
 *     28 hill): case 1 needs class < 24 with (class&7) > 3, i.e.
 *     grassland/savannah/marsh/swamp or their forests (103494-103495);
 *     case 2 needs mountain/hill or (class&7)==1 desert/scrub (103514-15).
 *   - case 1 also needs >= 4 rumours explored this session (DS:0x1dc6);
 *     otherwise (unless de Soto) gate<11→5 else 6 (103500-103507). WoI
 *     then forces 1→2 unconditionally (103508-103510).
 *   - case 2: de Soto's own kicker (RNG(0,2)==0 forces qualify, 103520-22);
 *     needs DS:0x1dc6 != 0 (byte wrap only) and < 7 Cibolas found this
 *     session (DS:0x1dc7); else gate<11→5, <25→8, else 6 (103524-103534).
 *     On success: treasure = ((skill+2)*10 + RNG(1,20)) hundred gold as a
 *     Treasure unit on the tile (103536-103548).
 *   - case 8: de Soto rerolls it away (103552); a village within 3 tiles
 *     (DS:0x8db8 distance from the nearest-village scan) takes a relation
 *     hit of RNG(1,6) + ((difficulty - skill) + 1) * 5, gated on that
 *     tribe having met the nation (FUN_281f_0a38 & 0x20). Met (103563-64
 *     `goto LAB_65dd_0320`) KEEPS case 8 — the @LOSTCITY8 trespass popup —
 *     and applies the hit; unmet clears local_32 to -1 and falls through to
 *     `local_8 = 6`, i.e. plain "Nothing" and no hit. No village within 3
 *     is also "Nothing" (103554-103568).
 *   - case 3: 3d8*10, times (skill+2)/2 when skill (103571-103578).
 *   - case 7: 4d10*2 (103580-103586).
 *   - case 5 (103597-103612): nation with < 5 census pop (DS:0x9410) and
 *     < 3 colonies (DS:0x9298) → 6; a Pioneer (type 2) with census < 9 →
 *     6 half the time; then the per-nation one-shot `lcr_case5_bonus_used`
 *     turns it into 4 (burial mounds) REGARDLESS of the two gates above
 *     (the latch check is last in the block); de Soto never accepts a bare
 *     Vanishes (103614-103616).
 * The case-8 "met" gate is live (bugs.md #497): `FUN_281f_0a38 & 0x20` is
 * `col1->indian[t-4].euro_diplo[nation] & COL1_INDIAN_MET_BIT`.
 */
static void units_lcr_roll_outcome(
  ColonizeLcrRoll* out,
  ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  ColonizeDosRng* rng,
  int nation,
  int x,
  int y,
  int skill,
  bool de_soto_reroll,
  bool woi,
  bool is_pioneer
) {
  memset(out, 0, sizeof(*out));
  out->trespass_tribe = -1;
  const int cls = map ? map_dos_terr_class_at(map, x, y) : -1;
  const int census = (col1 && nation >= 0 && nation < 4)
    ? col1->stuff.census_pop_proxy[nation] : 0;
  const int colonies = (col1 && nation >= 0 && nation < 4)
    ? col1->stuff.colony_counts[nation] : 0;
  int floor_val = 0;
  for (;;) {
    floor_val = floor_val + 1;
    if (floor_val > 3) {
      floor_val = 3;
    }
    const int r9 = dos_rng_range(rng, 1, 9);
    int raw = (floor_val > r9) ? floor_val : r9;
    const int gate = dos_rng_range(rng, 1, 100) + skill * 10;
    out->gate = gate;
    out->gold = 0;
    out->treasure_hundreds = 0;
    out->trespass_tribe = -1;
    out->trespass_mag = 0;

    if (raw == 5 && dos_rng_range(rng, 1, skill + 1) != 1) {
      if (de_soto_reroll) {
        continue;
      }
      raw = 6;
    }
    if (raw == 8 && dos_rng_range(rng, 1, skill + 1) != 1) {
      if (de_soto_reroll) {
        continue;
      }
      raw = 6;
    }

    if (raw == 1) {
      const bool qualifies = cls >= 0 && cls < 24 && (cls & 7) > 3;
      if ((!qualifies || s_lcr_explored_total < 4) && !de_soto_reroll) {
        raw = (gate < 11) ? 5 : 6;
      }
      if (woi) {
        raw = 2;
      }
    }

    if (raw == 2) {
      bool qualifies = cls == 27 || cls == 28 || (cls >= 0 && cls < 24 && (cls & 7) == 1);
      if (de_soto_reroll && dos_rng_range(rng, 0, 2) == 0) {
        qualifies = true;
      }
      if (!qualifies || s_lcr_explored_total == 0 || s_lcr_cibola_total > 6) {
        if (gate < 11) {
          raw = 5;
        } else if (gate < 25) {
          raw = 8;
        } else {
          raw = 6;
        }
      } else {
        /* 103536-103546: DOS rolls the value, then spawns the Treasure with
         * FUN_281f_095c and bails out of the whole routine when that fails
         * (`if (param_1 < 0) goto LAB_65dd_080a`); DS:0x1dc7 is bumped only on
         * the success path. The port spawns in the dispatch tail below, so the
         * counter is bumped there too (bugs.md #502). */
        out->treasure_hundreds = (skill + 2) * 10 + dos_rng_range(rng, 1, 20);
      }
    }

    if (raw == 8) {
      if (de_soto_reroll) {
        continue;
      }
      int dist = 0x7fffffff;
      const int tribe = units_lcr_nearest_tribe_dist(col1, x, y, &dist);
      raw = 6; /* 103568: the fall-through case unless the met gate jumps out */
      if (tribe >= 0 && dist < 3) {
        const int difficulty = col1 ? col1->head.difficulty : 0;
        /* 103557-103559: local_32 = the village's tribe, local_38 = the hit. */
        out->trespass_mag = dos_rng_range(rng, 1, 6) + ((difficulty - skill) + 1) * 5;
        out->trespass_tribe = tribe;
        /*
         * 103563-103565: `FUN_281f_0a38(*0x5394, *0x8d50) & 0x20` — the tribe
         * must have MET this nation. Met -> `goto LAB_65dd_0320`, which skips
         * the `local_8 = 6` below and leaves case 8 (the @LOSTCITY8 popup plus
         * the relation hit). Unmet -> local_32 = -1 and case 6 ("Nothing"),
         * so the later `(local_38 != 0) && (-1 < local_32)` hit never fires.
         * (The 09a4/0438 pair at 103560-103561 is the %STRING0 tribe-name
         * substitution for that popup; the port sets it as `tok.string0` in
         * the TRESPASS_ANGER arm below.)
         */
        const int tidx = tribe - 4;
        const bool met = col1 && tidx >= 0 && tidx < 8 && nation >= 0 && nation < 4 &&
          (col1->indian[tidx].euro_diplo[nation] & COL1_INDIAN_MET_BIT) != 0;
        if (met) {
          raw = 8;
        } else {
          out->trespass_tribe = -1;
        }
      }
    }

    if (raw == 3) {
      int g = dos_rng_range(rng, 1, 8);
      g += dos_rng_range(rng, 1, 8);
      g += dos_rng_range(rng, 1, 8);
      out->gold = g * 10;
      if (skill != 0) {
        out->gold = ((skill + 2) * out->gold) >> 1;
      }
    }
    if (raw == 7) {
      int g = dos_rng_range(rng, 1, 10);
      g += dos_rng_range(rng, 1, 10);
      g += dos_rng_range(rng, 1, 10);
      g += dos_rng_range(rng, 1, 10);
      out->gold = g * 2;
    }

    if (raw == 5) {
      if (census < 5 && colonies < 3) {
        raw = 6;
      }
      if (is_pioneer && census < 9 && dos_rng_range(rng, 1, 2) == 1) {
        raw = 6;
      }
      if (col1 && nation >= 0 && nation < 4 && !col1->player[nation].lcr_case5_bonus_used) {
        col1->player[nation].lcr_case5_bonus_used = 1;
        raw = 4;
      }
    }
    if (raw == 5 && de_soto_reroll) {
      raw = 6;
    }
    if (raw == 6 && de_soto_reroll) {
      continue;
    }

    if (out->trespass_tribe >= 0) {
      out->outcome = COLONIZE_LCR_TRESPASS_ANGER;
      return;
    }
    switch (raw) {
    case 1: out->outcome = COLONIZE_LCR_FOUNTAIN_OF_YOUTH; return;
    case 2: out->outcome = COLONIZE_LCR_CIBOLA; return;
    case 3: out->outcome = COLONIZE_LCR_SMALL_TREASURE; return;
    case 4: out->outcome = COLONIZE_LCR_BURIAL_MOUNDS; return;
    case 5: out->outcome = COLONIZE_LCR_VANISHES; return;
    case 7: out->outcome = COLONIZE_LCR_CHIEFS_GIFT; return;
    case 9: out->outcome = COLONIZE_LCR_SURVIVORS_JOIN; return;
    case 6:
    default: out->outcome = COLONIZE_LCR_NOTHING; return;
    }
  }
}

bool units_resolve_lcr_rumour_w(
  const ColonizeWorld* w,
  int unit_id,
  int human_nation
) {
  ColonizeUnitPool* pool = w->units;
  ColonizeWorldMap* map = w->map;
  ColonizeCol1Save* col1 = w->col1;
  ColonizeDosRng* rng = w->rng;
  EuropeScreen* europe = w->europe;

  /*
   * FUN_65dd_0004: any land unit standing on a procedural rumour tile
   * clears it and rolls one of the manual-documented outcomes (treasure /
   * Fountain of Youth / Cibola / survivors join / burial mounds / vanish /
   * nothing / trespass anger). Player-caught: this was previously
   * hard-gated to Scouts only, so any other unit (a lone Pioneer, a Soldier
   * escorting a wagon train, …) silently walked over an LCR tile with zero
   * effect — the ai_euro.c AI caller has the matching `is_scout &&`
   * pre-check relaxed too; every unit type still rolls, Scouts/Seasoned
   * Scouts just roll with a higher skill tier (see units_lcr_roll_outcome).
   */
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !map || !units_is_on_map(u)) {
    return false;
  }
  if (!map_tile_has_rumour(map, u->x, u->y)) {
    return false;
  }
  if (!map_clear_rumour(map, u->x, u->y)) {
    return false;
  }
  const int x = u->x;
  const int y = u->y;
  const int nation = u->nation_id;
  const bool de_soto_owned = col1 && nation >= 0 && nation < 4 &&
    founding_fathers_de_soto_lcr_always_positive(col1, nation);
  /* Sight around the explorer: the per-move FUN_13f1_02f8 radius (de Soto's
   * extended sight lives in units_sight_radius, not in the LCR tail). */
  (void)units_reveal_sight_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(map), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL)}, u);

  /*
   * Explorer skill tier (decomp local_36, viceroy:103450-103458): Scout
   * unit type (NAMES.TXT @UNIT row 5, "Scouts") = 1, + Seasoned Scout
   * expert profession = 2, + de Soto (FF7, but ONLY when already
   * Scout-type — de Soto's LCR bonus never fires for a non-Scout explorer
   * in this decomp) = +1 more. See units_lcr_roll_outcome for how `skill`
   * and `de_soto_reroll` (decomp bVar4) drive the case-selection loop.
   */
  const int scout_type = units_kind_type_index(pool, UNITS_KIND_SCOUT);
  int skill = (scout_type >= 0 && u->type_index == scout_type) ? 1 : 0;
  if (skill != 0 && u->profession == UNITS_JOB_SCOUT) {
    skill += 1;
  }
  const bool de_soto_reroll = skill != 0 && de_soto_owned;
  if (de_soto_reroll) {
    skill += 1;
  }
  const bool woi = col1 && col1->head.game_options.woi;
  const int pioneer_type = units_kind_type_index(pool, UNITS_KIND_PIONEER);
  const bool is_pioneer = pioneer_type >= 0 && u->type_index == pioneer_type;
  /* DS:0x1dc6++ happens before the roll loop (103459). */
  s_lcr_explored_total++;
  ColonizeLcrRoll roll;
  units_lcr_roll_outcome(
    &roll, col1, map, rng, nation, x, y, skill, de_soto_reroll, woi, is_pioneer
  );
  const ColonizeLcrOutcome outcome = roll.outcome;
  diag_info(
    "LCR unit %d nation=%d at (%d,%d): outcome=%d gold=%d skill=%d",
    unit_id, nation, x, y, (int)outcome, roll.gold, skill
  );

  /*
   * Dispatch tail (103620-103757). The numeric FUN_281f_048e arguments the
   * mysteries catalog once took for message ids are sound/event ids
   * (FUN_129f_02cc): 0x37 Fountain of Youth, 0x3c Cibola, 0x33 burial
   * prompt, 0x24 burial treasure with no tribe nearby, 0x32 SCREWED (the
   * same military sting units_combat_music_sting plays); FUN_281f_0498/04ac
   * (2, case) pick a BGM track for the gold cases. Sound chrome PARK.
   */
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  switch (outcome) {
  case COLONIZE_LCR_NOTHING:
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY6", nation, -1, 0, &tok, "");
    break;
  case COLONIZE_LCR_SMALL_TREASURE: {
    const int gold = roll.gold;
    units_lcr_credit_gold(col1, europe, human_nation, nation, gold);
    tok.has_number0 = true;
    tok.number0 = gold;
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY3", nation, -1, gold, &tok,
      "");
    /* FUN_65dd_0004 65dd:04ca: human + gold found (local_12) → pool 2. */
    if (nation == human_nation && gold != 0) {
      units_set_bgm_pool(2);
    }
    break;
  }
  case COLONIZE_LCR_CHIEFS_GIFT: {
    const int gold = roll.gold;
    units_lcr_credit_gold(col1, europe, human_nation, nation, gold);
    tok.has_number0 = true;
    tok.number0 = gold;
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY7", nation, -1, gold, &tok,
      "");
    if (nation == human_nation && gold != 0) {
      units_set_bgm_pool(2); /* 65dd:04ca, same arm as LOSTCITY3 */
    }
    break;
  }
  case COLONIZE_LCR_FOUNTAIN_OF_YOUTH:
    units_play_event_sound(0x37); /* FUN_65dd_0004 65dd:04a9 case 1: queued tune (281f_048e) */
    /* 65dd:04a9, right after that 0x37: FUN_281f_0524(8), human explorer only. */
    if (nation == human_nation) {
      (void)woodcut_fire(col1, WOODCUT_THE_FOUNTAIN_OF_YOUTH);
    }
    /*
     * 8 free dock immigrants (8x FUN_291f_0d2c, 103727-103731) after
     * FUN_281f_0524(8). DOS runs the loop for EVERY nation (bugs.md #496):
     * 103719 `FUN_281f_0582(nation)` binds DS:0x84fc to the explorer's own
     * nation record first, and only the 0x37 tune + the woodcut above sit
     * inside the `local_a != 0` (human) gate.
     */
    if (europe && nation == human_nation) {
      if (g_units_combat_popups) {
        /* Real 4884(1,0) ×8: the player picks each of the eight (2026-08-28). */
        units_fountain_youth_enqueue_pick(
          europe, g_units_combat_popups, g_units_combat_game_txt, human_nation, 8
        );
      } else {
        for (int i = 0; i < 8; ++i) {
          /*
           * 103728-103731 is `FUN_291f_0d2c(1,0)` ×8 = FUN_38fd_4884(1,0),
           * the free-passage pick dialog — not the 5e52 random-slot spawn.
           * With no UI to ask with, take the front slot through the same
           * 4884(1,0) door the popup path uses: free passage, no recruit
           * counter bump, and the emptied slot refilled by `46d4(0)` off the
           * shared game stream, which is in scope here (smell audit
           * 2026-09-10 G5). The old europe_immigrant_from_pool call rolled
           * the tail's refill on a private LCG and forced the expert half on
           * every fourth turn, neither of which 4884 does.
           */
          (void)europe_recruit_free_from_pool_ex(europe, 0, rng);
        }
      }
    } else if (col1 && nation >= 0 && nation < 4) {
      /*
       * AI (or any nation with no EuropeScreen bound): FUN_38fd_4884 skips its
       * list dialog whenever the bound nation's control byte is non-zero
       * (64744-64751 `*(0x543f + n*0x34) != 0` -> `local_58 = 1`), so each of
       * the eight picks takes pool slot 1 of that nation's own recruit[3]
       * (nation record +2..+4). param_1 != 0 means passage 0 (64695), the +0x2e
       * crosses word untouched (64763 is `param_1 == 0` only) and no +6 recruit
       * bump (64771). Tail order (64767-64774): create the unit, then refill the
       * emptied slot with 46d4(0) off the shared stream — and only on success.
       * bugs.md #496.
       */
      for (int i = 0; i < 8; ++i) {
        const int profession = (int)col1->nation[nation].recruit[1];
        const int id = europe_nation_harbor_spawn(w, nation, profession);
        if (id < 0) {
          break;
        }
        europe_nation_refill_pool_slot(col1, nation, 1, false, rng);
      }
    }
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY1", nation, -1, 0, &tok,
      "");
    break;
  case COLONIZE_LCR_CIBOLA: {
    units_play_event_sound(0x3c); /* FUN_65dd_0004 65dd:04b6 case 2: queued tune (281f_048e) */
    /* Case 2 (103536-103548): Treasure unit with +0x315b = hundreds. */
    const int gold = roll.treasure_hundreds * 100;
    const int treasure_id = units_spawn_treasure_train(pool, x, y, nation, gold);
    if (treasure_id < 0) {
      /* 103543 `if (param_1 < 0) goto LAB_65dd_080a`: no Treasure, no popup,
       * and DS:0x1dc7 stays put (bugs.md #502). */
      break;
    }
    s_lcr_cibola_total++; /* 103546: `*(char *)0x1dc7 += 1` after the spawn. */
    tok.has_number1 = true;
    tok.number1 = gold;
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY2", nation, -1, gold, &tok,
      "");
    break;
  }
  case COLONIZE_LCR_SURVIVORS_JOIN: {
    /* Case 9 (103718-103723): spawn unit type 0 (Colonist) on the tile,
     * nation name substituted (FUN_291f_0ac8). NAMES.TXT @COUNTRY — the
     * Crown nation ("England"), NOT the player's new-world country_name
     * ("New England"): survivors swear allegiance to the nation (bugs.md).
     *
     * raw 103591 `*(byte *)0x9cd2 = 0` (asm 65dd: `MOV byte [0x9cd2],0`):
     * DS:0x9cd2 is the base of the popup **%STRING substitution array** —
     * 64-byte slots, slot n at `0x9cd2 + (n << 6)`; the setter is
     * `FUN_6f74_03d0(slot, src)` = `strcpy(0x9cd2 + slot*64, src)`
     * (`SHL AX,6; ADD AX,0x9cd2; CALLF FUN_1d1d_117e` = strcpy), and the
     * `%STRING` token expander in FUN_6f74 (token table DS:0x1fa4 "STRING")
     * reads the same base. So the write truncates slot 0 to "" so no stale
     * string from a previous popup can leak, immediately before
     * `FUN_291f_0ac8(.,0,0,nation)` refills slot 0 with the nation name.
     * The port composes each popup from a freshly `memset` AiPopupTokens and
     * always assigns `tok.string0` here, so the clear has no port-visible
     * effect and is deliberately not transcribed. */
    const int ct = units_kind_type_index(pool, UNITS_KIND_COLONIST);
    if (ct >= 0) {
      const int nid = units_spawn_allow_stack(pool, ct, x, y);
      ColonizeUnit* nu = units_get(pool, nid);
      if (nu) {
        units_set_nation(nu, nation);
      }
    }
    tok.string0 = (nation >= 0 && nation <= 3) ? reports_nation_country_name(nation)
                                               : units_combat_nation_label(col1, nation);
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY9", nation, -1, 0, &tok,
      "");
    break;
  }
  case COLONIZE_LCR_TRESPASS_ANGER: {
    const int tribe = roll.trespass_tribe;
    if (col1 && tribe >= 0 && nation >= 0 && nation < 4) {
      ai_diplo_indian_relation_delta(col1, tribe, nation, -roll.trespass_mag);
    }
    tok.string0 = units_combat_nation_label(col1, tribe);
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY8", nation, -1, 0, &tok,
      "");
    break;
  }
  case COLONIZE_LCR_VANISHES:
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY5", nation, -1, 0, &tok,
      "");
    /* 65dd:0778 (case 5): human → tune pool 1 (map) before the unit goes. */
    if (nation == human_nation) {
      units_set_bgm_pool(1);
    }
    /* 65dd:080a tail: only the despawn arm (local_e) redraws 7×7 and presents
     * with the fizzle (FUN_281f_03ea, 8) — the piece pixelates away. */
    units_dissolve_notify(0);
    units_despawn(pool, unit_id);
    units_dissolve_notify(1);
    break;
  case COLONIZE_LCR_BURIAL_MOUNDS: {
    units_play_event_sound(0x33); /* FUN_65dd_0004 65dd:04c0 case 4: queued tune (281f_048e) */
    /*
     * @LOSTCITY4 (Search / Stay clear) auto-resolves as Search (DOS
     * local_3c==1; interactive CHOICE PARK). Post-choice block 103653-
     * 103715: the nearest village's tribe claims the mounds when
     * RNG(1, (dist+5) << skill) < 4 and it has met the nation (0a38 &
     * 0x20 — no per-tribe contact flag here, treated as met). Variant by
     * the loop's `gate`: <25 → BURIAL1 empty; <50, or <65 with no claim →
     * BURIAL2 3d8*10 gold; else BURIAL3 Treasure unit worth
     * (RNG(1,8) + (skill+5)*2)*2 hundred. A claim adds @SCREWED and +100
     * relation hit (FUN_281f_0d6c(tribe, nation, 100)) — DOS does not kill
     * the unit here; the angry tribe does that on its own turn.
     */
    int dist = 0x7fffffff;
    const int near_tribe = units_lcr_nearest_tribe_dist(col1, x, y, &dist);
    int screwed_tribe = -1;
    if (near_tribe >= 0 && dist < 0x7fffffff) {
      const int span = (dist + 5) << skill;
      if (dos_rng_range(rng, 1, span < 1 ? 1 : span) < 4) {
        screwed_tribe = near_tribe;
      }
    }
    if (roll.gate < 25) {
      units_combat_enqueue_tok(
        AI_POPUP_TAG_INFO, "BURIAL1", nation, -1, 0, &tok, "");
    } else if (roll.gate < 50 || (screwed_tribe < 0 && roll.gate < 65)) {
      int g = dos_rng_range(rng, 1, 8);
      g += dos_rng_range(rng, 1, 8);
      g += dos_rng_range(rng, 1, 8);
      const int gold = g * 10;
      units_lcr_credit_gold(col1, europe, human_nation, nation, gold);
      tok.has_number0 = true;
      tok.number0 = gold;
      units_combat_enqueue_tok(
        AI_POPUP_TAG_INFO, "BURIAL2", nation, -1, gold, &tok, "");
    } else {
      const int gold = (dos_rng_range(rng, 1, 8) + (skill + 5) * 2) * 2 * 100;
      (void)units_spawn_treasure_train(pool, x, y, nation, gold);
      /* 65dd:0654: human + no tribe claim → 0x24 treasure tune (281f_048e). */
      if (nation == human_nation && screwed_tribe < 0) {
        units_play_event_sound(0x24);
      }
      tok.has_number1 = true;
      tok.number1 = gold;
      units_combat_enqueue_tok(
        AI_POPUP_TAG_INFO, "BURIAL3", nation, -1, gold, &tok,
        "");
    }
    if (screwed_tribe >= 0) {
      /* 65dd:06e6: human → 0x32 Military sting ahead of @SCREWED. */
      if (nation == human_nation) {
        units_play_event_sound(0x32);
      }
      PopupMsgTokens stok;
      memset(&stok, 0, sizeof(stok));
      if (col1 && nation >= 0 && nation < 4) {
        /* DOS FUN_281f_0d6c(tribe, nation, 100): +100 there is an ALARM rise;
         * the port's relation accessor is sign-inverted (worse = negative),
         * same convention as the FUN_5fef_31ea conquest delta. */
        ai_diplo_indian_relation_delta(col1, screwed_tribe, nation, -100);
      }
      stok.string0 = units_combat_nation_label(col1, screwed_tribe);
      units_combat_enqueue_tok(
        AI_POPUP_TAG_INFO, "SCREWED", nation, -1, 0, &stok,
        "");
    }
    break;
  }
  }
  return true;
}


static ColonizeSoundPlayFn g_units_combat_sound_play = NULL;
static ColonizeSoundActiveIdFn g_units_combat_sound_active_id = NULL;
static ColonizeSoundPlayFn g_units_set_bgm = NULL;
void units_set_bgm_hook(ColonizeSoundPlayFn set_bgm_fn) {
  g_units_set_bgm = set_bgm_fn;
}
void units_set_bgm_pool(int pool) {
  if (g_units_set_bgm) {
    g_units_set_bgm(pool);
  }
}

static ColonizeTaxChangeFn g_units_tax_change = NULL;
static void* g_units_tax_change_user = NULL;
void units_set_tax_change_hook(ColonizeTaxChangeFn fn, void* user) {
  g_units_tax_change = fn;
  g_units_tax_change_user = user;
}

/*
 * DOS-LITERAL FUN_5fef_0352 raw 99653-99690 (asm OVL17 0xdaa-0xea8), bugs.md
 * #659. Runs in the destroy tail for ANY loser (the human test below only gates
 * the %STRING/%NUMBER fills, and no popup call follows them: KINGMERCY has no
 * DS string in VICEROY.EXE, so DOS lowers the tax silently):
 *   if (loser +0x3148 & 0x40) {                       // Royal-flagged unit
 *     k = -1; for (i = 0; k < 0 && i < 6; i++)        // DS:0x978d stride 6
 *       if (table[i].type == loser.type) k = i;       //   {type, cut, 0xff, price16, 0}
 *     if (0 < k) {                                    // JG: index 0 (Artillery) excluded
 *       cut = min(nation.tax_rate, table_byte(0x978e + i*6));   // i == k+1 after the
 *       if (cut > 0) nation.tax_rate -= cut;                    //   loop INC: NEXT entry's cut
 *     }
 *   }
 * Table bytes (three original_memory_dumps, byte-identical): types
 * 0b/0d/0e/0f/10/11 with cut bytes 2/2/4/6/3/8 and the byte after the table is
 * 0. So Caravel reads 4, Merchantman 6, Galleon 3, Privateer 8, Frigate 0
 * (no cut). Ported verbatim, off-by-one included.
 */
int units_royal_loss_tax_cut(
  ColonizeCol1Save* col1, const ColonizeUnitPool* pool, const ColonizeUnit* lose
) {
  static const int k_type[6] = {0x0b, 0x0d, 0x0e, 0x0f, 0x10, 0x11};
  static const int k_cut_byte[7] = {2, 2, 4, 6, 3, 8, 0}; /* [6] = byte after table */
  if (!col1 || !pool || !lose || (lose->col1_flags15 & 0x40u) == 0) {
    return 0;
  }
  const int nation = lose->nation_id;
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  const int dos_type =
    (lose->type_index >= 0 && lose->type_index < pool->type_count)
      ? units_type_dos_code(&pool->types[lose->type_index])
      : -1;
  int k = -1;
  int i;
  for (i = 0; k < 0 && i < 6; i++) {
    if (k_type[i] == dos_type) {
      k = i;
    }
  }
  if (k <= 0) {
    return 0;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation];
  int cut = (int)(int8_t)nat->tax_rate;
  if (k_cut_byte[i] < cut) {
    cut = k_cut_byte[i];
  }
  if (cut <= 0) {
    return 0;
  }
  nat->tax_rate = (uint8_t)((int)nat->tax_rate - cut);
  if (g_units_tax_change) {
    g_units_tax_change(g_units_tax_change_user, nation, (int)nat->tax_rate);
  }
  return cut;
}

void units_set_combat_music_hooks(
  ColonizeSoundPlayFn play_fn, ColonizeSoundActiveIdFn active_id_fn
) {
  g_units_combat_sound_play = play_fn;
  g_units_combat_sound_active_id = active_id_fn;
}

/* See units.h. Puts every process-global callback hook back to its
 * unregistered (NULL) initial value; units_reset(pool) touches pool state
 * only and does not clear these. */
void units_reset_hooks(void) {
  g_units_move_watch = NULL;
  g_units_move_watch_user = NULL;
  g_units_combat_watch = NULL;
  g_units_combat_watch_user = NULL;
  g_units_dissolve = NULL;
  g_units_dissolve_user = NULL;
  g_units_raid_repelled = NULL;
  g_units_popup_pump = NULL;
  g_units_popup_pump_user = NULL;
  g_units_set_bgm = NULL;
  g_units_tax_change = NULL;
  g_units_tax_change_user = NULL;
  g_units_combat_sound_play = NULL;
  g_units_combat_sound_active_id = NULL;
}

/*
 * DOS combat engagement (segment 5fef, reached after an attacker/defender
 * tile-adjacency check succeeds) pushes literal id 0x32 — @PICKMUSIC
 * Indian sublist, first song "Indian Victory" (docs/assets.md) — into the BGM-change path
 * (FUN_281f_048e -> FUN_129f_02cc) once a land or naval attack begins. The
 * real driver only restarts playback when the id actually changes
 * (FUN_129f_0318 "cmp [0x9c],id; jz done"); mirror that via the active-id
 * hook so a combat-heavy turn does not restart the track on every attack.
 */

void units_play_event_sound(int id);

void units_play_event_sound(int id) {
  if (g_units_combat_sound_play) {
    g_units_combat_sound_play(id);
  }
}

/*
 * FUN_5fef_1b0e only animates and plays the fire/win event sounds when its
 * `param_4` "visible" flag is set: FUN_465b_0000 (~75692) passes 1 when the
 * attacker is the viewport nation or either side is human (`0x543f == 0`),
 * while the AI move scorer at 521d:52aa (~88888) passes 0 and resolves the
 * same combat silently. The port played the cannonade for every AI-vs-AI
 * battle resolved during end-of-turn, so cannon fire landed on top of
 * unrelated popups (bugs.md: prices-fall / immigration popups).
 * No col1 context (unit tests, standalone smokes) → audible, as before.
 */
bool units_combat_is_visible(const ColonizeUnitPool* pool, int a_id, int b_id) {
  const ColonizeCol1Save* col1 = g_units_ff_col1;
  if (!col1) {
    return true;
  }
  const ColonizeUnit* a = units_get_const(pool, a_id);
  const ColonizeUnit* b = units_get_const(pool, b_id);
  const int sides[2] = {a ? a->nation_id : -1, b ? b->nation_id : -1};
  for (int i = 0; i < 2; ++i) {
    const int n = sides[i];
    if (n >= 0 && n < (int)COLONIZE_COL1_NATION_COUNT && col1->player[n].control == 0) {
      return true;
    }
  }
  return false;
}

void units_combat_music_sting(void) {
  if (!g_units_combat_sound_play) {
    return;
  }
  if (!g_units_combat_sound_active_id ||
      g_units_combat_sound_active_id() != SOUND_MILITARY_BGM_ID) {
    g_units_combat_sound_play(SOUND_MILITARY_BGM_ID);
  }
}
