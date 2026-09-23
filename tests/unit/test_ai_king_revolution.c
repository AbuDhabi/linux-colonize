/* Slice of the former tests/unit/test_ai_king.c (split by feature 2026-09-23):
 * revolution end ladder: @LOSING/@WARN/@WINNING/@RETIRING2/@SCORED/@SOONRETIRING bands. */
#include "test_ai_king_common.h"

static int case_revolution_lose2_no_colonies(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1); /* REF already invading */
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1785;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("rev-lose2 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 1;
    }

    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-lose2: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 2) {
      fprintf(stderr, "unit_ai_king: rev-lose2 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("WoI + REF + no colonies should latch revolution lost");
    }
    if (!strstr(estatus, "control all colonies")) {
      fprintf(stderr, "unit_ai_king: rev-lose2 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-lose2 should set @LOSING2 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "control all colonies") &&
            strstr(pop.queue[i].body, "United Colonies") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("rev-lose2 should enqueue @LOSING2 INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution lose2 (no colonies) ok\n");
  return 0;
}

static int case_revolution_lose1_no_ports(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1); /* REF already invading */
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1785;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("rev-lose1 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 1; /* all land — inland colony is non-coastal */
    }
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-lose1: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 2) {
      fprintf(stderr, "unit_ai_king: rev-lose1 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("WoI + REF + inland-only should latch revolution lost via ports");
    }
    if (!strstr(estatus, "control all ports") && !strstr(estatus, "ports in")) {
      fprintf(stderr, "unit_ai_king: rev-lose1 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-lose1 should set @LOSING1 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "control all ports") &&
            strstr(pop.queue[i].body, "United Colonies") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("rev-lose1 should enqueue @LOSING1 INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution lose1 (no ports, inland left) ok\n");
  return 0;
}

static int case_revolution_warn_one_colony(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    ai_king_latch_set(&end, 6, 0);
    ai_king_latch_set(&end, 7, 0);
    end.head.year = 1600;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("warn1 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25; /* ocean */
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25; /* coast neighbor for (5,5) */
    emap.terrain[8 + 5 * 16] = 1;
    emap.terrain[9 + 5 * 16] = 25; /* coast neighbor for (8,5) */
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);
    /* Keep a crown unit so year<<1850 win path cannot fire. */
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1;
      u->x = 0;
      u->y = 0;
      eu.unit_count = 1;
    }
    /* DOS 3844_0442 win gate: a bare typeless crown unit doesn't count as
     * land force — keep the REF Regulars pool stocked so the King has not
     * "run out of forces" in this warn-only scenario (bugs.md #255 rework). */
    end.head.expeditionary_force[0] = 5;

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn1: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 0) {
      fprintf(stderr, "unit_ai_king: warn1 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn1 must not latch endgame");
    }
    if (ai_king_latch_get(&end, 6) != 0) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("colonies<3 outranks ports<3: market_demand_pool_raw[6] must stay clear");
    }
    if (ai_king_latch_get(&end, 7) != 1) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn2 should set market_demand_pool_raw[7] episode latch");
    }
    if (!strstr(estatus, "all but 1") || !strstr(estatus, "lose the war")) {
      fprintf(stderr, "unit_ai_king: warn2 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn2 should set @WARN2 status");
    }
    {
      int found_port = 0;
      int found_col = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind != AI_POPUP_KIND_OK) {
          continue;
        }
        if (strstr(pop.queue[i].body, "all but 1") &&
            strstr(pop.queue[i].body, "United Colonies") &&
            strstr(pop.queue[i].body, "surrender")) {
          found_port = 1;
        }
        if (strstr(pop.queue[i].body, "all but 1") &&
            strstr(pop.queue[i].body, "colonies") &&
            strstr(pop.queue[i].body, "lose the war")) {
          found_col = 1;
        }
      }
      if (!found_col) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn2 should enqueue @WARN2 INFO OK");
      }
      if (found_port) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("DOS shows one @WARN%d per turn: @WARN1 must not join @WARN2");
      }
    }

    /* Second turn with the latch set: no second @WARN2 enqueue. */
    {
      const int q0 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      if (ai_king_latch_get(&end, 7) != 1) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn latch should remain while one colony left");
      }
      int rewarn = 0;
      for (int i = q0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "all but 1")) {
          rewarn = 1;
          break;
        }
      }
      if (rewarn) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn2 must not re-enqueue while latched");
      }
    }

    /* bugs.md batch (DOS 3844_0442): warn band is <3 ports/colonies, so the
     * episode clears only at >=3 — reclaim TWO coastal colonies to clear,
     * then drop back below to re-fire. */
    {
      ColonizeColony* c2 = &cp.colonies[1];
      memset(c2, 0, sizeof(*c2));
      c2->active = true;
      c2->nation_id = 0;
      c2->x = 8;
      c2->y = 5;
      ColonizeColony* c3 = &cp.colonies[2];
      memset(c3, 0, sizeof(*c3));
      c3->active = true;
      c3->nation_id = 0;
      c3->x = 10;
      c3->y = 5;
      emap.terrain[10 + 5 * 16] = 1; /* land tile for the third port */
      cp.colony_count = 3;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      if (ai_king_latch_get(&end, 6) != 0 || ai_king_latch_get(&end, 7) != 0) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn latches should clear when ports/colonies>1");
      }
      c2->active = false;
      c3->active = false;
      cp.colony_count = 1;
      const int q1 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      if (ai_king_latch_get(&end, 7) != 1) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn should re-latch after reclaim then drop to one");
      }
      int found_port = 0;
      int found_col = 0;
      for (int i = q1; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind != AI_POPUP_KIND_OK) {
          continue;
        }
        if (strstr(pop.queue[i].body, "surrender") &&
            strstr(pop.queue[i].body, "all but 1")) {
          found_port = 1;
        }
        if (strstr(pop.queue[i].body, "lose the war") &&
            strstr(pop.queue[i].body, "colonies")) {
          found_col = 1;
        }
      }
      if (!found_col || found_port) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("@WARN2 alone should re-enqueue after the reclaim episode");
      }
    }

    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution warn selector (one colony) ok\n");
  return 0;
}

static int case_revolution_warn3_pop_share(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    ai_king_latch_set(&end, 10, 0);
    end.head.year = 1600;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("warn3 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25;
    }
    /*
     * THREE human coastal colonies (the colonies<3 test is the last write in
     * DOS's warn selector and would otherwise pick @WARN2, raw 58530) + one
     * crown colony.
     */
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25;
    emap.terrain[8 + 5 * 16] = 1;
    emap.terrain[9 + 5 * 16] = 25;
    emap.terrain[11 + 5 * 16] = 1;
    emap.terrain[12 + 5 * 16] = 25;
    emap.terrain[5 + 8 * 16] = 1;
    emap.terrain[6 + 8 * 16] = 25;
    {
      ColonizeColony* c0 = &cp.colonies[0];
      memset(c0, 0, sizeof(*c0));
      c0->active = true;
      c0->nation_id = 0;
      c0->x = 5;
      c0->y = 5;
      c0->population = 20;
      ColonizeColony* c1 = &cp.colonies[1];
      memset(c1, 0, sizeof(*c1));
      c1->active = true;
      c1->nation_id = 0;
      c1->x = 8;
      c1->y = 5;
      c1->population = 20;
      /* Third human colony keeps the count at 3 (so the colonies<3 arm of the
       * selector stays quiet) without moving the 85% share. */
      ColonizeColony* c2 = &cp.colonies[2];
      memset(c2, 0, sizeof(*c2));
      c2->active = true;
      c2->nation_id = 0;
      c2->x = 11;
      c2->y = 5;
      c2->population = 0;
      ColonizeColony* ck = &cp.colonies[3];
      memset(ck, 0, sizeof(*ck));
      ck->active = true;
      ck->nation_id = 1; /* crown */
      ck->x = 5;
      ck->y = 8;
      /* 85% share — DOS warn band is 80-89%. DOS FUN_3844_0442 computes
       * (crown+1)*100 / ((human+1)+(crown+1)) off the census mirror, so the
       * crown total that lands on 85 with 40 human pop is 232, not 227
       * (audit D7 wired the +1 offsets). */
      ck->population = 232;
      cp.colony_count = 4;
    }

    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1;
      eu.unit_count = 1;
    }

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 0) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3 must not latch endgame");
    }
    if (ai_king_latch_get(&end, 10) != 1) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3 should set market_demand_pool_raw[10] episode latch");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "85%") &&
            strstr(pop.queue[i].body, "population") &&
            strstr(pop.queue[i].body, "United Colonies")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn3 should enqueue @WARN3 INFO OK");
      }
    }
    {
      const int q0 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      int re = 0;
      for (int i = q0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "85%") &&
            strstr(pop.queue[i].body, "population")) {
          re = 1;
          break;
        }
      }
      if (re) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn3 must not re-enqueue while latched");
      }
    }
    /* Drop crown share below 80% → clear latch; raise again → re-fire. */
    cp.colonies[3].population = 10; /* crown 11/(41+11) = 21% */
    estatus[0] = '\0';
    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 10) != 0) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3 latch should clear when pop share <80%");
    }
    cp.colonies[3].population = 232; /* crown back to 85% under the DOS +1 formula */
    const int q1 = pop.queue_count;
    estatus[0] = '\0';
    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 10) != 1) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("warn3 should re-latch after share returns to 80–89%");
    }
    {
      int found = 0;
      for (int i = q1; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "population") &&
            strstr(pop.queue[i].body, "85%")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("warn3 should re-enqueue after reclaim episode");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution warn3 (pop share) ok\n");
  return 0;
}

static int case_revolution_lose3_pop_share(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1785;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("lose3 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25;
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25;
    emap.terrain[8 + 5 * 16] = 1;
    emap.terrain[9 + 5 * 16] = 25;
    {
      ColonizeColony* c0 = &cp.colonies[0];
      memset(c0, 0, sizeof(*c0));
      c0->active = true;
      c0->nation_id = 0;
      c0->x = 5;
      c0->y = 5;
      c0->population = 10;
      ColonizeColony* ck = &cp.colonies[1];
      memset(ck, 0, sizeof(*ck));
      ck->active = true;
      ck->nation_id = 1;
      ck->x = 8;
      ck->y = 5;
      /* 90%: DOS FUN_3844_0442 is (crown+1)*100/((human+1)+(crown+1)) off
       * the census mirror, so with 10 human pop the bar is crown 98+
       * (audit D7 wired the +1 offsets). */
      ck->population = 100;
      cp.colony_count = 2;
    }

    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1;
      eu.unit_count = 1;
    }

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("lose3: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 2) {
      fprintf(stderr, "unit_ai_king: lose3 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("crown pop ≥90% should latch revolution lost via @LOSING3");
    }
    if (!strstr(estatus, "over 90%") && !strstr(estatus, "90% of")) {
      fprintf(stderr, "unit_ai_king: lose3 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("lose3 should set @LOSING3 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "population") &&
            strstr(pop.queue[i].body, "United Colonies") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("lose3 should enqueue @LOSING3 INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution lose3 (pop share) ok\n");
  return 0;
}

static int case_revolution_win_1850(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1850;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain) {
      return fail("rev-win alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25; /* ocean */
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25; /* coast neighbor */
    /* Found a coastal colony for human so lose path doesn't fire. */
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);
    /* No crown units. */

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-win: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 1) {
      fprintf(stderr, "unit_ai_king: rev-win endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("WoI + 1850 + no crown units should latch revolution won");
    }
    if (!strstr(estatus, "Expeditionary Force annihilated") &&
        !strstr(estatus, "accepts surrender") &&
        !strstr(estatus, "annihilated")) {
      fprintf(stderr, "unit_ai_king: rev-win status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("rev-win should set @WINNING status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            (strstr(pop.queue[i].body, "Expeditionary Force annihilated") ||
             strstr(pop.queue[i].body, "accepts surrender")) &&
            strstr(pop.queue[i].body, "Washington") &&
            strstr(pop.queue[i].body, "United Colonies")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("rev-win should enqueue @WINNING INFO OK");
      }
    }
    /* bugs.md #258: @WINNING (KING_WAR_END payload 1) FIRST, then the
     * @KINGLOSE throne audience (KING_THRONE payload 1) — the audience
     * dismissal is what opens the retire score. */
    {
      int win_at = -1;
      int throne_at = -1;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_WAR_END && pop.queue[i].payload == 1) {
          win_at = i;
        }
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_THRONE) {
          throne_at = i;
          if (pop.queue[i].payload != 1 ||
              !strstr(pop.queue[i].body, "go your own way")) {
            assets_msg_free(&game_txt);
            free(emap.terrain);
            free(emap.layer2);
            free(emap.layer3);
            return fail("rev-win throne audience should carry @KINGLOSE, payload 1");
          }
        }
      }
      if (win_at < 0 || throne_at < 0 || throne_at < win_at) {
        fprintf(stderr, "unit_ai_king: rev-win order win_at=%d throne_at=%d\n", win_at,
                throne_at);
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("rev-win: @WINNING must precede the @KINGLOSE throne audience");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution win (1850) ok\n");
  return 0;
}

static int case_revolution_retiring2_1850_stalemate(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1850;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("retiring2 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25;
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25;
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      c->population = 8;
      snprintf(c->name, sizeof(c->name), "Jamestown");
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1; /* crown still in the field */
      eu.unit_count = 1;
    }
    /* DOS win gate (bugs.md #255): keep the REF pool stocked so the
     * exhaustion win cannot preempt the 1850 war-weariness loss. */
    end.head.expeditionary_force[0] = 5;

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("retiring2: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 2) {
      fprintf(stderr, "unit_ai_king: retiring2 endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("1850 + crown alive should latch revolution lost via @RETIRING2");
    }
    if (ai_king_latch_get(&end, 1) != 0) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("@RETIRING2 should clear REF-present");
    }
    if (!strstr(estatus, "sues for peace") && !strstr(estatus, "War-weary")) {
      fprintf(stderr, "unit_ai_king: retiring2 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("1850 stalemate should set @RETIRING2 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            (strstr(pop.queue[i].body, "sues for peace") ||
             strstr(pop.queue[i].body, "War-weary")) &&
            strstr(pop.queue[i].body, "Washington") &&
            strstr(pop.queue[i].body, "Jamestown")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("1850 stalemate should enqueue @RETIRING2 INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: revolution retiring2 (1850 stalemate) ok\n");
  return 0;
}

static int case_peacetime_scored_retiring_1800(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 0);
    end.head.game_options.woi = 0;
    ai_king_latch_set(&end, 1, 0);
    end.head.game_options.ref_present = 0;
    ai_king_latch_set(&end, 4, 0);
    end.head.year = 1800;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    end.nation[0].liberty_bells_total = 0; /* no declare */
    end.head.colony_count = 0;

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      c->population = 6;
      snprintf(c->name, sizeof(c->name), "Jamestown");
      cp.colony_count = 1;
    }
    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      return fail("scored: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 4) != 3) {
      fprintf(stderr, "unit_ai_king: scored endgame=%d status='%s'\n",
              ai_king_latch_get(&end, 4), estatus);
      assets_msg_free(&game_txt);
      return fail("year≥1800 peacetime should latch PEACE_1800");
    }
    if (!strstr(estatus, "Scoring for this game")) {
      fprintf(stderr, "unit_ai_king: scored status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      return fail("1800 end should set @SCORED status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind != AI_POPUP_KIND_CHOICE ||
            pop.queue[i].tag != AI_POPUP_TAG_KING_SCORED) {
          continue;
        }
        if (!strstr(pop.queue[i].body, "Scoring for this game")) {
          continue;
        }
        if (pop.queue[i].choice_count < 2) {
          continue;
        }
        if (!strstr(pop.queue[i].choices[0], "That's all") ||
            !strstr(pop.queue[i].choices[1], "Keep playing")) {
          continue;
        }
        found = 1;
        break;
      }
      if (!found) {
        assets_msg_free(&game_txt);
        return fail("1800 end should enqueue @SCORED CHOICE");
      }
    }
    const int q_before_apply = pop.queue_count;
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_tag = AI_POPUP_TAG_KING_SCORED;
    pop.result_choice_id = 0; /* That's all */
    pop.result_nation_a = 0;
    ai_king_apply_popup_result(&ectx, &pop);
    if (!strstr(estatus, "steps down") || !strstr(estatus, "loyal service") ||
        !strstr(estatus, "Washington") || !strstr(estatus, "Jamestown")) {
      fprintf(stderr, "unit_ai_king: scored apply status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      return fail("@SCORED That's all should set @RETIRING status");
    }
    {
      int found = 0;
      for (int i = q_before_apply; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "steps down") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        return fail("@SCORED That's all should enqueue @RETIRING INFO OK");
      }
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "unit_ai_king: peacetime @SCORED/@RETIRING (1800) ok\n");
  return 0;
}

static int case_peacetime_soonretiring0_1790(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 0);
    end.head.game_options.woi = 0;
    ai_king_latch_set(&end, 4, 0);
    ai_king_latch_set(&end, 8, 0);
    end.head.year = 1790;
    end.head.difficulty = 0;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    end.nation[0].liberty_bells_total = 0;
    end.head.colony_count = 0;

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      return fail("soon0: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    uint16_t autumn = 0; /* spring */
    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.game_autumn = &autumn;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 8) != 1) {
      assets_msg_free(&game_txt);
      return fail("1790 spring should set market_demand_pool_raw[8] @SOONRETIRING0 latch");
    }
    if (!strstr(estatus, "retire in 1800")) {
      fprintf(stderr, "unit_ai_king: soon0 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      return fail("1790 should set @SOONRETIRING0 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "retire in 1800") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        return fail("1790 should enqueue @SOONRETIRING0 INFO OK");
      }
    }
    {
      const int q0 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      int re = 0;
      for (int i = q0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "retire in 1800")) {
          re = 1;
          break;
        }
      }
      if (re) {
        assets_msg_free(&game_txt);
        return fail("@SOONRETIRING0 must not re-enqueue while latched");
      }
    }
    assets_msg_free(&game_txt);
    fprintf(stderr, "unit_ai_king: peacetime @SOONRETIRING0 (1790) ok\n");
  return 0;
}

static int case_wartime_soonretiring1_1840(void) {
    ColonizeCol1Save end;
    col1_save_init(&end);
    ai_king_latch_set(&end, 0, 1);
    end.head.game_options.woi = 1;
    ai_king_latch_set(&end, 1, 1);
    end.head.game_options.ref_present = 1;
    ai_king_latch_set(&end, 4, 0);
    ai_king_latch_set(&end, 9, 0);
    end.head.year = 1840;
    end.player[0].control = 0;
    snprintf(end.player[0].name, sizeof(end.player[0].name), "Washington");
    snprintf(end.player[0].country_name, sizeof(end.player[0].country_name),
             "United Colonies");
    for (int n = 1; n < 4; ++n) {
      end.player[n].control = 2;
    }

    ColonizeColonyPool cp;
    colonies_init(&cp);
    colonies_set_occupancy_map(NULL);
    ColonizeWorldMap emap;
    memset(&emap, 0, sizeof(emap));
    emap.width = 16;
    emap.height = 16;
    emap.tile_count = 256;
    emap.terrain = calloc(256, 1);
    emap.layer2 = calloc(256, 1);
    emap.layer3 = calloc(256, 1);
    if (!emap.terrain || !emap.layer2 || !emap.layer3) {
      return fail("soon1 alloc");
    }
    for (int i = 0; i < 256; ++i) {
      emap.terrain[i] = 25;
    }
    emap.terrain[5 + 5 * 16] = 1;
    emap.terrain[6 + 5 * 16] = 25;
    {
      ColonizeColony* c = &cp.colonies[0];
      memset(c, 0, sizeof(*c));
      c->active = true;
      c->nation_id = 0;
      c->x = 5;
      c->y = 5;
      cp.colony_count = 1;
    }

    ColonizeUnitPool eu;
    memset(&eu, 0, sizeof(eu));
    units_reset(&eu);
    units_set_occupancy_map(NULL);
    {
      ColonizeUnit* u = &eu.units[0];
      memset(u, 0, sizeof(*u));
      u->active = true;
      u->nation_id = 1;
      eu.unit_count = 1;
    }
    /* DOS win gate (bugs.md #255): stocked pool keeps the war live at 1840. */
    end.head.expeditionary_force[0] = 5;

    ColonizeMsgCatalog game_txt;
    memset(&game_txt, 0, sizeof(game_txt));
    if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("soon1: GAME.TXT load failed");
    }
    AiPopupState pop;
    ai_popup_init(&pop);

    char estatus[AI_POPUP_BODY_LEN];
    estatus[0] = '\0';
    ColonizeTurnContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.messages = test_game_txt();
    ectx.names = test_names_txt();
    ectx.col1 = &end;
    ectx.col1_ok = true;
    ectx.human_nation = 0;
    ectx.colonies = &cp;
    ectx.units = &eu;
    ectx.map = &emap;
    ectx.status = estatus;
    ectx.status_size = sizeof(estatus);
    ectx.messages = &game_txt;
    ectx.ai_popups = &pop;

    ai_king_nation_turn(&ectx);
    if (ai_king_latch_get(&end, 9) != 1) {
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("1840 WoI should set market_demand_pool_raw[9] @SOONRETIRING1 latch");
    }
    if (!strstr(estatus, "weary of this long war") && !strstr(estatus, "1850")) {
      fprintf(stderr, "unit_ai_king: soon1 status: '%s'\n", estatus);
      assets_msg_free(&game_txt);
      free(emap.terrain);
      free(emap.layer2);
      free(emap.layer3);
      return fail("1840 should set @SOONRETIRING1 status");
    }
    {
      int found = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "weary of this long war") &&
            strstr(pop.queue[i].body, "Washington")) {
          found = 1;
          break;
        }
      }
      if (!found) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("1840 should enqueue @SOONRETIRING1 INFO OK");
      }
    }
    {
      const int q0 = pop.queue_count;
      estatus[0] = '\0';
      ai_king_nation_turn(&ectx);
      int re = 0;
      for (int i = q0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "weary of this long war")) {
          re = 1;
          break;
        }
      }
      if (re) {
        assets_msg_free(&game_txt);
        free(emap.terrain);
        free(emap.layer2);
        free(emap.layer3);
        return fail("@SOONRETIRING1 must not re-enqueue while latched");
      }
    }
    assets_msg_free(&game_txt);
    free(emap.terrain);
    free(emap.layer2);
    free(emap.layer3);
    fprintf(stderr, "unit_ai_king: wartime @SOONRETIRING1 (1840) ok\n");
  return 0;
}

static const TestCase k_cases[] = {
    {"case_revolution_lose2_no_colonies", case_revolution_lose2_no_colonies},
    {"case_revolution_lose1_no_ports", case_revolution_lose1_no_ports},
    {"case_revolution_warn_one_colony", case_revolution_warn_one_colony},
    {"case_revolution_warn3_pop_share", case_revolution_warn3_pop_share},
    {"case_revolution_lose3_pop_share", case_revolution_lose3_pop_share},
    {"case_revolution_win_1850", case_revolution_win_1850},
    {"case_revolution_retiring2_1850_stalemate", case_revolution_retiring2_1850_stalemate},
    {"case_peacetime_scored_retiring_1800", case_peacetime_scored_retiring_1800},
    {"case_peacetime_soonretiring0_1790", case_peacetime_soonretiring0_1790},
    {"case_wartime_soonretiring1_1840", case_wartime_soonretiring1_1840},
};
TEST_MAIN(k_cases)
