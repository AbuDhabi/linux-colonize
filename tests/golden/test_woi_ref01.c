/*
 * port_plan P5.1: the Royal Expeditionary Force must actually prosecute the
 * War of Independence against real assets. Loads the real-DOS Dutch fixture
 * (COLONY00-dutch2-t0.SAV, 7 human colonies), forces colony SoL to 100%,
 * declares via the menu path (no popups → auto-confirm) and runs turn_end
 * with a passive human. Before 2026-08-28 this run produced zero attacks in
 * 40 turns, for four independent reasons (see king_ref.md "headless WoI
 * run"): REF land types never spawned against NAMES.TXT's plural names, the
 * crown slot (control 2) never had its moves refreshed, the ordinary Euro AI
 * spent the REF's moves before war_act ran, and any own unit on the next
 * tile "blocked" a whole REF column.
 *
 * Asserts: Regulars from the real @UNIT table land within 3 turns, a colony
 * falls within 12, every colony falls and the @LOSING endgame latch (stored
 * on the relocated king latch bytes, not the market words) is set within 40.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_king.h"
#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

static int count_colonies(const ColonizeColonyPool* pool, int nation) {
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].nation_id == nation) {
      n++;
    }
  }
  return n;
}

static int count_type(const ColonizeUnitPool* units, int nation, const char* type_name) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation) {
      continue;
    }
    const ColonizeUnitType* t = units_type(units, u->type_index);
    if (t && strcmp(t->name, type_name) == 0) {
      n++;
    }
  }
  return n;
}

int main(void) {
  const char* path = "original_saves/colony-prod-tests/COLONY00-dutch2-t0.SAV";
  char err[256];
  ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file(path, &save, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path, err);
    return 1;
  }
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeMsgCatalog msgs;
  assets_msg_init(&msgs);
  (void)assets_msg_load_file(&msgs, "COLONIZE/GAME.TXT");
  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    return 1;
  }
  /* The real @UNIT table spells these plural; the AI asks singular. */
  if (units_find_type(&units, "Regular") < 0 || units_find_type(&units, "Dragoon") < 0 ||
      units_find_type(&units, "Cont. Cav") < 0) {
    fprintf(stderr, "units_find_type must tolerate NAMES.TXT plural/abbreviated names\n");
    return 1;
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    return 1;
  }
  (void)colonies_load_names(&colonies, "COLONIZE/COLONY.TXT");
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(&save, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
    fprintf(stderr, "bridge: %s\n", err);
    return 1;
  }

  uint32_t turn_number = br.turn_number;
  uint16_t year = br.year;
  uint16_t autumn = br.autumn;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 100u);
  char status[256] = {0};
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn_number;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.human_nation = br.human_nation;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.map = &map;
  ctx.col1 = &save;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.rng_seed = 100;
  ctx.status = status;
  ctx.status_size = sizeof(status);
  ctx.messages = &msgs;
  ctx.names = &names;
  const int human = br.human_nation;
  int crown = ai_king_crown_nation(human); /* refreshed after declare — the
    succession merger (bugs.md follow-up) may vacate a different slot */

  /* Market pool words must not be the king's scratch space any more. */
  uint16_t market_before[16];
  memcpy(market_before, save.head.price_group_state, sizeof(market_before));

  for (unsigned i = 0; i < save.head.colony_count; ++i) {
    if (save.colony[i].nation_id == human) {
      save.colony[i].rebel_dividend = 100;
      save.colony[i].rebel_divisor = 100;
    }
  }
  const int colonies_start = count_colonies(&colonies, human);
  ai_king_menu_declare_independence(&ctx);
  if (!ai_king_independence_declared(&save)) {
    fprintf(stderr, "declare failed: %s\n", status);
    return 1;
  }
  crown = ai_king_crown_nation_col1(&save, human);
  /* Succession invariant: the King borrows an EMPTY slot — no inherited
   * colonies (bugs.md follow-up: Quebec must not become Tory for free). */
  if (count_colonies(&colonies, crown) != 0) {
    fprintf(stderr, "crown slot %d inherited %d colonies at declare\n", crown,
            count_colonies(&colonies, crown));
    return 1;
  }
  if (memcmp(market_before, save.head.price_group_state, sizeof(market_before)) != 0) {
    fprintf(stderr, "declare wrote into price_group_state (old unknown46 king bytes)\n");
    return 1;
  }
  if (ai_king_latch_get(&save, AI_KING_ENDGAME_BYTE) != AI_KING_ENDGAME_NONE) {
    fprintf(stderr, "endgame latch not clear on a real save after declare\n");
    return 1;
  }

  int first_regular_turn = -1;
  int first_capture_turn = -1;
  int all_lost_turn = -1;
  int endgame_turn = -1;
  for (int t = 1; t <= 40; ++t) {
    status[0] = '\0';
    turn_end(&ctx);
    if (getenv("WOI_DEBUG")) {
      fprintf(stderr, "[woi t%d] status='%s'\n", t, status);
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &colonies.colonies[i];
        if (c->active && c->nation_id == human) {
          fprintf(stderr, "  human colony %s (%d,%d) pop=%d\n", c->name, c->x, c->y, c->population);
          for (int k = 0; k < COLONIZE_UNITS_MAX; ++k) {
            const ColonizeUnit* u = &units.units[k];
            if (u->active && u->aboard_ship_id < 0 && abs(u->x - c->x) <= 2 && abs(u->y - c->y) <= 2) {
              fprintf(stderr, "    near: id=%d n=%d %s (%d,%d) ord=%d mv=%d\n", u->id, u->nation_id, units_display_name(&units, u), u->x, u->y, u->orders, u->moves_left);
            }
          }
        }
      }
    }
    if (first_regular_turn < 0 && count_type(&units, crown, "Regulars") > 0) {
      first_regular_turn = t;
    }
    const int left = count_colonies(&colonies, human);
    if (first_capture_turn < 0 && left < colonies_start) {
      first_capture_turn = t;
    }
    if (all_lost_turn < 0 && left == 0) {
      all_lost_turn = t;
    }
    if (ai_king_latch_get(&save, AI_KING_ENDGAME_BYTE) != AI_KING_ENDGAME_NONE) {
      endgame_turn = t;
      break;
    }
  }
  printf(
    "golden_woi_ref01: start=%d colonies, Regulars t%d, first capture t%d, all lost t%d, "
    "endgame=%d at t%d\n",
    colonies_start, first_regular_turn, first_capture_turn, all_lost_turn,
    ai_king_latch_get(&save, AI_KING_ENDGAME_BYTE), endgame_turn
  );
  int rc = 0;
  if (first_regular_turn < 0 || first_regular_turn > 3) {
    fprintf(stderr, "REF Regulars did not land by turn 3 (t%d)\n", first_regular_turn);
    rc = 1;
  }
  if (first_capture_turn < 0 || first_capture_turn > 12) {
    fprintf(stderr, "REF captured nothing by turn 12 (t%d)\n", first_capture_turn);
    rc = 1;
  }
  /*
   * The war may end before every colony falls: @LOSING3 (crown controls
   * >= 90% of the colony population) surrenders with towns still standing —
   * 2026-08-28 the Recife run ends that way at t8. Only an unfinished war
   * (no LOST latch) counts as a stalled REF.
   */
  if (all_lost_turn < 0 && ai_king_latch_get(&save, AI_KING_ENDGAME_BYTE) != AI_KING_ENDGAME_LOST) {
    fprintf(stderr, "passive human still holds %d colonies after 40 turns\n",
            count_colonies(&colonies, human));
    rc = 1;
  }
  if (ai_king_latch_get(&save, AI_KING_ENDGAME_BYTE) != AI_KING_ENDGAME_LOST) {
    fprintf(stderr, "endgame latch want LOST(%d) got %d\n", AI_KING_ENDGAME_LOST,
            ai_king_latch_get(&save, AI_KING_ENDGAME_BYTE));
    rc = 1;
  }
  map_free(&map);
  assets_msg_free(&msgs);
  assets_msg_free(&names);
  col1_save_free(&save);
  return rc;
}
