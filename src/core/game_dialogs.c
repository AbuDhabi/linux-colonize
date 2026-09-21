#include "core/game_dialogs.h"

/*
 * Sections:
 *  - Colony/trade/found-colony confirm dialogs (~line 14)
 *  - Construction & Europe boycott/buy dialogs (~line 914)
 *  - Modal input handling & AI popup result appliers (~line 1623)
 *
 * game_apply_ai_popup_result is a dispatcher over the game_apply_popup_*
 * stages directly above it (map_and_colony / voyage / contact /
 * diplo_and_scout / combat_and_gifts / village_attack), each returning true
 * when it recognised and consumed the result tag.
 *
 * Split out of game_loop.c (which still owns the screens, the map and the
 * per-frame update); the shared ColonizeGameState definition and the
 * cross-file helper prototypes are in game_dialogs.h.
 */

/* ===================== Colony/trade/found-colony confirm dialogs (game_open_pedia_article .. game_request_disband_confirm) ===================== */

void game_open_pedia_article(
  ColonizeGameState* game,
  PediaCategory category,
  int index,
  bool return_to_list
);
void game_trade_begin_at_stop(ColonizeGameState* game, int route, int stop_i);
static void game_open_found_name_entry(ColonizeGameState* game, int colony_id);
static void game_open_landho_name_entry(ColonizeGameState* game);
static void game_landho_default_region(const ColonizeGameState* game, char* out, size_t out_size);
static void game_apply_name_entry_result(ColonizeGameState* game);
void game_try_prompt_landho(ColonizeGameState* game);
bool game_handle_modal_input(ColonizeGameState* game, const ColonizeInputState* input);
static void game_apply_howmuch_result(ColonizeGameState* game);
static void game_do_buy_construction(ColonizeGameState* game, int colony_id);
void game_request_buy_construction_confirm(ColonizeGameState* game);
void game_colony_open_load_prompt(
  ColonizeGameState* game, const ColonizeColony* colony, int cargo
);


bool begin_menu_compute_layout(
  const ColonizeGameState* game,
  int fb_w,
  int fb_h,
  BeginMenuLayout* out
);

const char* key_name(ColonizeKey key) {
  switch (key) {
    case COLONIZE_KEY_ESCAPE: return "ESCAPE";
    case COLONIZE_KEY_ENTER: return "ENTER";
    case COLONIZE_KEY_SPACE: return "SPACE";
    case COLONIZE_KEY_UP: return "UP";
    case COLONIZE_KEY_DOWN: return "DOWN";
    case COLONIZE_KEY_LEFT: return "LEFT";
    case COLONIZE_KEY_RIGHT: return "RIGHT";
    case COLONIZE_KEY_S: return "S";
    case COLONIZE_KEY_F: return "F";
    case COLONIZE_KEY_L: return "L";
    case COLONIZE_KEY_M: return "M";
    case COLONIZE_KEY_Q: return "Q";
    case COLONIZE_KEY_P: return "P";
    case COLONIZE_KEY_E: return "E";
    case COLONIZE_KEY_R: return "R";
    case COLONIZE_KEY_T: return "T";
    case COLONIZE_KEY_D: return "D";
    case COLONIZE_KEY_B: return "B";
    case COLONIZE_KEY_C: return "C";
    case COLONIZE_KEY_H: return "H";
    case COLONIZE_KEY_O: return "O";
    case COLONIZE_KEY_U: return "U";
    case COLONIZE_KEY_W: return "W";
    case COLONIZE_KEY_I: return "I";
    case COLONIZE_KEY_N: return "N";
    case COLONIZE_KEY_A: return "A";
    case COLONIZE_KEY_G: return "G";
    case COLONIZE_KEY_V: return "V";
    case COLONIZE_KEY_X: return "X";
    case COLONIZE_KEY_Z: return "Z";
    case COLONIZE_KEY_LEFTBRACKET: return "[";
    case COLONIZE_KEY_RIGHTBRACKET: return "]";
    case COLONIZE_KEY_TILDE: return "TILDE";
    case COLONIZE_KEY_F1: return "F1";
    case COLONIZE_KEY_F2: return "F2";
    case COLONIZE_KEY_F3: return "F3";
    case COLONIZE_KEY_F4: return "F4";
    case COLONIZE_KEY_F5: return "F5";
    case COLONIZE_KEY_F6: return "F6";
    case COLONIZE_KEY_F7: return "F7";
    case COLONIZE_KEY_F8: return "F8";
    case COLONIZE_KEY_F9: return "F9";
    case COLONIZE_KEY_F10: return "F10";
    case COLONIZE_KEY_BACKSPACE: return "Backspace";
    default: return "NONE";
  }
}

void set_status(ColonizeGameState* game, const char* prefix, const char* detail) {
  if (!game || !prefix) {
    return;
  }
  if (!detail) {
    snprintf(game->status, sizeof(game->status), "%s", prefix);
    return;
  }
  snprintf(game->status, sizeof(game->status), "%.64s: %.60s", prefix, detail);
}

/* Manual / fandom: Europe screen closes once independence is declared. */
bool game_europe_blocked_by_woi(const ColonizeGameState* game) {
  return game && game->col1_ok && ai_king_independence_declared(&game->col1);
}

/* bugs.md #225: player sell/buy that crossed a price threshold — show the
 * @PRICEUP/@PRICEDOWN dialog immediately (same format as the EOT market
 * tick's) and clear the event list. */
void game_europe_drain_price_events(ColonizeGameState* game) {
  if (!game || !game->europe_ok) {
    return;
  }
  EuropeScreen* eu = &game->europe;
  for (int i = 0; i < eu->price_event_count; ++i) {
    const int c = eu->price_event_cargo[i];
    if (c < 0 || c >= eu->cargo_count) {
      continue;
    }
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = eu->cargo[c].name;
    tok.string1 = eu->port_city;
    tok.number0 = eu->cargo[c].bid;
    tok.has_number0 = true;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages, eu->price_event_dir[i] > 0 ? "PRICEUP" : "PRICEDOWN", &tok,
      eu->status, body, sizeof(body)
    );
    (void)ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
  }
  eu->price_event_count = 0;
}

/*
 * Europe status line (bugs.md #376). DOS composes the sale line into the same
 * DS:0x2d54 buffer the map strip uses and arms it with FUN_38fd_19d8(1, 0x78,
 * 0), then repaints the European Status screen — the line replaces the top
 * strip's normal content for its dwell. Nothing blocks: the Europe screen
 * keeps taking input while it is up, and FUN_1009_0270 retires it when the
 * armed deadline passes.
 */
void game_service_europe_bar(ColonizeGameState* game) {
  if (!game || !game->europe_ok) {
    return;
  }
  EuropeScreen* eu = &game->europe;
  for (int i = 0; i < eu->bar_event_count; ++i) {
    (void)ai_popup_enqueue_bar_message(&game->ai_popups, eu->bar_event[i]);
  }
  eu->bar_event_count = 0;
  if (ai_popup_bar_message(&game->ai_popups)) {
    (void)ai_popup_bar_service(&game->ai_popups, game->elapsed_ms, false);
  }
}

bool game_try_enter_europe(ColonizeGameState* game) {
  if (!game || !game->europe_ok) {
    return false;
  }
  if (game_europe_blocked_by_woi(game)) {
    /* GAME.TXT @EUROPENOTAVAIL: the European Status Screen is withdrawn for
     * the duration of the War of Independence — same OK-popup pattern as
     * @FOREIGNNOTAVAIL (game_open_report). */
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages,
      "EUROPENOTAVAIL",
      NULL,
      "",
      body,
      sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    return false;
  }
  game->in_europe = true;
  game->in_pedia = false;
  game->in_colony = false;
  game->in_report = false;
  sound_set_bgm(3); /* FUN_75c2_2778 75c2:27a9: Europe screen → tune pool 3 (281f_0498) */
  snprintf(
    game->europe.status,
    sizeof(game->europe.status),
    "Home port ready. Recruit / Train / S Sail / Esc."
  );
  game_track_screen(game);
  diag_info(
    "EUROPE opened: gold=%d tax=%d%% harbor=%d docks=%d",
    game->europe.gold, game->europe.tax_percent, game->europe.harbor_ships,
    game->europe.dock_count
  );
  return true;
}

/* @WAREHOUSEFULL when ship→colony unload hits capacity. */
void game_emit_warehouse_full(
  ColonizeGameState* game,
  int colony_id,
  int cargo_type,
  int deposited,
  int already_included
) {
  if (!game) {
    return;
  }
  const ColonizeColony* col = colonies_get(&game->colonies, colony_id);
  if (!col) {
    return;
  }
  const char* cargo_name = "cargo";
  if (cargo_type >= 0 && cargo_type < COLONIZE_CARGO_COUNT &&
      game->europe.cargo[cargo_type].name[0]) {
    cargo_name = game->europe.cargo[cargo_type].name;
  }
  colonies_emit_warehouse_full_chrome(
    &game->colonies, col, cargo_type, cargo_name, deposited, already_included, &game->ai_popups,
    &game->messages
  );
  /* Raised from inside the colony screen: open on the spot, not at close. */
  game_colony_present_now(game, AI_POPUP_TAG_INFO);
}

/*
 * Complete human FOUND at founder unit tile (after gates / @NOPORT confirm).
 * FUN_4cc6_07c2 Indian homeland purchase via colonies_found_with_indian_land —
 * same charge/Minuit-free gate as AI euro FOUND. Cite: Colonization.pdf Indian
 * Land / Peter Minuit (FF 2). smoke_game_flow has no tribe fixture; covered by
 * unit_founding_fathers + unit_ai_euro_expand indian-land cases.
 */
/*
 * DOS encroachment CHOICE kinds — the three GAME.TXT sections that share one
 * dialog shape ("respect" / "offer {N} gold" / "take it"): @INDIANLAND
 * (found colony, FUN_479b tile-buy site), @INDIANFOREST (pioneer clear order,
 * thunk_FUN_1000_91fc), @INDIANROAD (pioneer road order, thunk_FUN_1000_9304).
 * Cite: original_sources_annotated/ai/indian_actions_menu.md §encroachment.
 */

/* DOS FUN_1000_935a CHOICE results are 1-based: 1 respect, 2 offer gold, 3 take. */
enum {
  GAME_INDIAN_LAND_RESPECT = 1,
  GAME_INDIAN_LAND_OFFER = 2,
  GAME_INDIAN_LAND_TAKE = 3
};

/*
 * Enqueue the encroachment CHOICE when DOS would: tile is tribal land with a
 * real price (Minuit / already-purchased → 0 → no dialog), the human is at
 * PEACE (relation bit 0x40, FUN_1000_8c28) with the owning tribe, and for
 * FOREST the tile is a forest class (8..23). "Offer gold" is greyed out in
 * DOS when the treasury cannot cover it (func_0x000193a6(dlg, 2, 1)); this
 * port drops the row instead. "Take it" has no immediate consequence in any
 * of the three DOS sites — encroachment friction accrues through the
 * already-ported FUN_4d56_152e village growth pass. Returns true when a
 * dialog was queued (caller must stop and wait for the result).
 */
bool game_request_indian_land_choice(
  ColonizeGameState* game,
  GameIndianLandKind kind,
  int uid,
  int x,
  int y
) {
  if (!game || !game->col1_ok || !game->world_map_ok || uid < 0) {
    return false;
  }
  const int hn = game->human_nation;
  if (hn < 0 || hn > 3) {
    return false;
  }
  ColonizeCol1Save* col1 = &game->col1;
  if (kind == GAME_INDIAN_LAND_FOREST) {
    const int cls = map_dos_terr_class_at(&game->world_map, x, y);
    if (cls < 8 || cls > 23) {
      return false;
    }
  }
  const int tribe_i = colonies_indian_land_owner_tribe(col1, &game->world_map, x, y);
  if (tribe_i < 0 || !col1->tribe) {
    return false;
  }
  const int tribe_nation = (int)col1->tribe[tribe_i].nation_id;
  if (tribe_nation < 4 || tribe_nation > 11) {
    return false;
  }
  if (!ai_contact_indian_has_peace(col1, tribe_nation, hn)) {
    return false;
  }
  /* colonies_* land helpers take the record word by pointer, so stamp it from
   * the purse first — the one named form of that copy (audit G3). */
  europe_gold_stamp_record(&game->europe, col1);
  const int price = colonies_indian_land_purchase_gold(col1, &game->world_map, x, y, hn);
  if (price <= 0) {
    return false;
  }
  static const char* k_section[3] = {"INDIANLAND", "INDIANFOREST", "INDIANROAD"};
  ColonizeTurnContext ctx;
  game_fill_turn_context(game, &ctx);
  const char* tribe = ai_contact_tribe_name(tribe_nation);
  char fb[AI_POPUP_BODY_LEN];
  fb[0] = '\0';
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = tribe;
  tok.number1 = price;
  tok.has_number1 = true;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, k_section[kind], &tok, fb, body, sizeof(body));
  char choice_buf[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
  const ColonizeMsgSection* sec = assets_msg_find(&game->messages, k_section[kind]);
  const int nch = popup_msg_choices(sec, choice_buf, AI_POPUP_CHOICE_MAX);
  const char* offer_fb = "";
  const char* labels[3];
  int ids[3];
  int n = 0;
  labels[n] = nch >= 3 ? choice_buf[0] : "";
  ids[n++] = GAME_INDIAN_LAND_RESPECT;
  if ((uint32_t)price <= europe_nation_gold(&game->europe, col1, hn)) {
    if (nch >= 3) {
      /* Substitute {%NUMBER1$} inside the label row. */
      static char offer_lbl[AI_POPUP_CHOICE_LEN];
      popup_msg_apply_tokens(offer_lbl, sizeof(offer_lbl), choice_buf[1], &tok);
      labels[n] = offer_lbl;
    } else {
      labels[n] = offer_fb;
    }
    ids[n++] = GAME_INDIAN_LAND_OFFER;
  }
  labels[n] = nch >= 3 ? choice_buf[2] : "";
  ids[n++] = GAME_INDIAN_LAND_TAKE;
  const int payload = (x & 0xff) | ((y & 0xff) << 8);
  return ai_popup_enqueue_choice_ctx(
    &game->ai_popups, AI_POPUP_TAG_INDIAN_LAND, uid, (int)kind, payload, NULL, body, labels, ids, n
  );
}

bool game_do_found_colony_at_unit(ColonizeGameState* game, int uid, bool land_resolved) {
  if (!game || !game->world_map_ok || uid < 0) {
    return false;
  }
  /* bugs.md follow-up: DOS forbids founding new colonies once independence
   * is declared — every hand and bell goes to the war effort. */
  if (game->col1_ok && game->col1.head.game_options.woi) {
    set_status(
      game,
      assets_msg_line_or(&game->messages, "NOCOLONIESEITHER", 0, ""),
      NULL
    );
    return false;
  }
  const ColonizeUnit* founder = units_get_const(&game->units, uid);
  if (!founder || !founder->active || !units_is_on_map(founder)) {
    set_status(game, "No unit at cursor to found colony", NULL);
    return false;
  }
  if (units_is_sea(&game->units, uid)) {
    set_status(game, "Ships cannot found colonies", NULL);
    return false;
  }
  const int cx = founder->x;
  const int cy = founder->y;
  if (!colonies_can_found(&game->colonies, &game->world_map, cx, cy)) {
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }

  const int type_index = founder->type_index;
  const int profession = founder->profession;
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(&game->units, uid, &tools, &muskets, &horses);

  const int hn = game->human_nation;
  ColonizeCol1Save* col1 = game->col1_ok ? &game->col1 : NULL;
  uint32_t* gold = NULL;
  int land_cost = 0;
  /*
   * @INDIANLAND: DOS asks respect / offer gold / take before founding on
   * tribal land (FUN_479b tile-buy site). land_resolved = the CHOICE already
   * ran (paid via colonies_indian_land_pay, or "take it") — found free.
   */
  if (!land_resolved && game_request_indian_land_choice(game, GAME_INDIAN_LAND_FOUND, uid, cx, cy)) {
    return true;
  }
  /*
   * No dialog (not at peace with the owner, Minuit, already bought, AI-free
   * tile): DOS founds without charging — the tile-buy only ever runs inside
   * the @INDIANLAND "offer gold" arm. The old "need N gold" hard block and
   * silent auto-pay are gone with it.
   */
  (void)col1;

  const int cid = colonies_found_with_indian_land_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL)}, gold, cx, cy, hn, type_index, profession, tools, muskets, horses);
  if (cid < 0) {
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }

  units_despawn(&game->units, uid);
  if (gold) {
    game->europe.gold = (int)*gold;
  }
  /* bugs.md #268: DOS tracks names-consumed per nation in the SAVE (the AI
   * paths already bump player.founded_colonies); the human path didn't, so
   * a reload re-derived nothing and the session counter drifted. */
  if (game->col1_ok && hn >= 0 && hn < 4) {
    game->col1.player[hn].founded_colonies++;
  }
  colonies_reveal_founded_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .col1=(ColonizeCol1Save*)(game->col1_ok ? &game->col1 : NULL), .col1_ok=((game->col1_ok ? &game->col1 : NULL) != NULL)}, cid); /* FUN_364b_1dd6 Coronado */
  const ColonizeColony* col = colonies_get(&game->colonies, cid);
  if (land_cost > 0) {
    snprintf(
      game->status,
      sizeof(game->status),
      "Founded %s (paid %d gold)",
      col ? col->name : "colony",
      land_cost
    );
  } else {
    snprintf(
      game->status,
      sizeof(game->status),
      "Founded %s (pop %d)",
      col ? col->name : "colony",
      col ? col->population : 0
    );
  }
  game->found_open_colony_id = cid;
  game_open_found_name_entry(game, cid);
  sound_play(0x54); /* DOS FUN_479b_076e: found-colony hammering (COLDIG sample 13) */
  /* 479b:0950, immediately after that same 0x54: human-only woodcut 2. */
  if (hn == game->human_nation && game->col1_ok) {
    (void)woodcut_fire(&game->col1, WOODCUT_BUILDING_A_COLONY);
  }
  return true;
}

/* @NOPORT CHOICE when founding inland (no ocean access). */
void game_request_noport_found_confirm(ColonizeGameState* game, int uid) {
  if (!game || uid < 0) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages,
    "NOPORT",
    &tok,
    "",
    body,
    sizeof(body)
  );
  char label_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(
    &game->messages, "NOPORT", &tok, "",
    "", label_buf, labels
  );
  const int ids[] = {0, 1}; /* forgot / proceed */
  game->map_confirm = GAME_MAP_CONFIRM_FOUND_INLAND;
  game->map_confirm_payload = uid;
  if (!ai_popup_enqueue_choice(
        &game->ai_popups, AI_POPUP_TAG_MAP_CONFIRM, NULL, body, labels, ids, 2
      )) {
    game->map_confirm = GAME_MAP_CONFIRM_NONE;
    game->map_confirm_payload = -1;
    set_status(game, "Dialog queue full", NULL);
  }
}

/*
 * Human FOUND (B key / Orders → Build Colony).
 * @SEACOLONY on water; @TOOMOUNTAIN on mountains; @NOPORT CHOICE when land is not coastal.
 */
bool game_try_found_colony_at_cursor(ColonizeGameState* game) {
  if (!game || !game->world_map_ok) {
    return false;
  }
  const int cx = game->map_cursor_x;
  const int cy = game->map_cursor_y;
  if (!map_tile_is_land(&game->world_map, cx, cy)) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages,
      "SEACOLONY",
      NULL,
      "",
      body,
      sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }
  /* Pool bound (colonies_abandon leaves holes and shrinks colony_count). */
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* col = &game->colonies.colonies[i];
    if (col->active && map_tiles_adjacent(col->x, col->y, cx, cy, true)) {
      char body[AI_POPUP_BODY_LEN];
      PopupMsgTokens tok = {0};
      tok.string0 = col->name[0] ? col->name : "nearby colony";
      popup_msg_fill(
        &game->messages,
        "TOONEAR",
        &tok,
        "",
        body,
        sizeof(body)
      );
      ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      set_status(game, "Cannot found colony here", NULL);
      return false;
    }
  }
  /*
   * @TOONEARBUILD (colony.c:472): a 9-tile neighbor scan (own tile plus the
   * 8 adjacent) for another unit with a Build Colony order (UNITS_ORDER_
   * BUILD_COLONY = DOS order 7) already pending — a race between two
   * colonists founding adjacent colonies the same turn.
   */
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      const int tx = cx + dx;
      const int ty = cy + dy;
      int slot = 0;
      for (const ColonizeUnit* u = units_next_on_tile_const(&game->units, tx, ty, &slot);
           u != NULL; u = units_next_on_tile_const(&game->units, tx, ty, &slot)) {
        if (u->orders == UNITS_ORDER_BUILD_COLONY) {
          char body[AI_POPUP_BODY_LEN];
          popup_msg_fill(
            &game->messages,
            "TOONEARBUILD",
            NULL,
            "",
            body,
            sizeof(body)
          );
          ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
          set_status(game, "Cannot found colony here", NULL);
          return false;
        }
      }
    }
  }
  if (map_pedia_terrain_index_at(&game->world_map, cx, cy) == 27) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages,
      "TOOMOUNTAIN",
      NULL,
      "",
      body,
      sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }
  if (!colonies_can_found(&game->colonies, &game->world_map, cx, cy)) {
    set_status(game, "Cannot found colony here", NULL);
    return false;
  }
  const int uid = units_id_at(&game->units, cx, cy);
  if (uid < 0) {
    set_status(game, "No unit at cursor to found colony", NULL);
    return false;
  }
  if (units_is_sea(&game->units, uid)) {
    set_status(game, "Ships cannot found colonies", NULL);
    return false;
  }
  if (!map_tile_is_coastal(&game->world_map, cx, cy)) {
    game_request_noport_found_confirm(game, uid);
    return true;
  }
  return game_do_found_colony_at_unit(game, uid, false);
}

void game_enqueue_yes_no(
  ColonizeGameState* game,
  GameMapConfirm confirm,
  int payload,
  const char* section,
  const char* fallback_body,
  const PopupMsgTokens* tok
) {
  if (!game) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, section, tok, fallback_body, body, sizeof(body));
  const char* labels[] = {"Yes", "No"};
  const int ids[] = {1, 0};
  game->map_confirm = confirm;
  game->map_confirm_payload = payload;
  if (!ai_popup_enqueue_choice(
        &game->ai_popups,
        AI_POPUP_TAG_MAP_CONFIRM,
        NULL,
        body,
        labels,
        ids,
        2
      )) {
    game->map_confirm = GAME_MAP_CONFIRM_NONE;
    set_status(game, "Dialog queue full", NULL);
  }
}

static void game_do_disband(ColonizeGameState* game, int uid) {
  if (!game) {
    return;
  }
  if (uid < 0 || !units_disband(&game->units, uid)) {
    set_status(game, "", NULL);
  } else {
    set_status(game, "Unit disbanded", NULL);
    game_wait_next_unit(game);
  }
}

static void game_do_overboard(ColonizeGameState* game, int uid) {
  if (!game) {
    return;
  }
  int ctype = 0;
  int amt = 0;
  if (uid < 0 || units_dump_cargo_overboard(&game->units, uid, &ctype, &amt) <= 0) {
    set_status(game, "No cargo to dump", NULL);
  } else {
    snprintf(game->status, sizeof(game->status), "Dumped %d overboard", amt);
  }
}

/* Live route count (DOS DS:0x53a0 — routes are kept compact). */
int game_trade_route_count(const ColonizeGameState* game) {
  if (!game || !game->col1_ok) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT; ++i) {
    if (game->col1.trade_route[i].name[0] != '\0' ||
        game->col1.trade_route[i].dest_count > 0) {
      n = i + 1;
    }
  }
  return n;
}

/*
 * Delete route `slot` — DOS FUN_647e_1486 tail: units on the route lose
 * order 2 (and the route/stop bookkeeping), units on later routes are
 * renumbered, and the array is compacted.
 */
static void game_do_trade_delete_slot(ColonizeGameState* game, int slot) {
  if (!game || !game->col1_ok || slot < 0 ||
      slot >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    return;
  }
  char gone[32];
  snprintf(gone, sizeof(gone), "%s", game->col1.trade_route[slot].name);
  if (game->units_ok) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &game->units.units[i];
      if (!u->active || u->orders != UNITS_ORDER_TRADE_ROUTE) {
        continue;
      }
      if (u->follow_unit_id == slot) {
        units_clear_orders(&game->units, u->id);
      } else if (u->follow_unit_id > slot) {
        u->follow_unit_id--;
      }
    }
  }
  /*
   * Ships sitting in a Europe lane are unit records in DOS's own pool, so the
   * FUN_647e_1486 loop above renumbers them too (viceroy 103175-103189 walks
   * all *(int*)0x539c units, Europe sentinels included). The port keeps those
   * ships in EuropeScreen instead, so they need the same fixup by hand — else
   * a route deleted while a ship is mid-Atlantic leaves it pointing at a dead
   * or renumbered slot, which col1_bridge's cursor guard then has to drop.
   */
  {
    EuropeHarborShip* lanes[3] = {game->europe.harbor, game->europe.bound, game->europe.expected};
    const int counts[3] = {
      game->europe.harbor_ships, game->europe.bound_ships, game->europe.expected_ships
    };
    for (int li = 0; li < 3; ++li) {
      for (int si = 0; si < counts[li] && si < EUROPE_HARBOR_MAX; ++si) {
        EuropeHarborShip* sh = &lanes[li][si];
        const int r = sh->trade_route_plus1 - 1;
        if (r == slot) {
          sh->trade_route_plus1 = 0;
          sh->trade_stop = 0;
        } else if (r > slot) {
          sh->trade_route_plus1--;
        }
      }
    }
  }
  const int count = game_trade_route_count(game);
  for (int i = slot; i < count - 1; ++i) {
    game->col1.trade_route[i] = game->col1.trade_route[i + 1];
  }
  memset(
    &game->col1.trade_route[count - 1], 0, sizeof(game->col1.trade_route[count - 1])
  );
  game->col1.head.trade_route_count = (uint16_t)(count - 1);
  if (game->trade_last_edited >= count - 1) {
    game->trade_last_edited = 0;
  }
  snprintf(game->status, sizeof(game->status), "Deleted %s", gone[0] ? gone : "route");
}

/*
 * Begin Trade Route step 2 (DOS FUN_2b5a_1e66 tail): route picked; assign it,
 * then let the player pick the starting destination (FUN_647e_090a) when the
 * route has more than one stop.
 */
void game_trade_begin_route(ColonizeGameState* game, int route) {
  if (!game || !game->units_ok || !game->col1_ok) {
    return;
  }
  const int sid = game->units.selected_id;
  if (sid < 0 || route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    set_status(game, "Select a ship or wagon", NULL);
    return;
  }
  const ColonizeCol1TradeRoute* r = &game->col1.trade_route[route];
  if (r->dest_count <= 0) {
    set_status(game, "Invalid trade route", NULL);
    return;
  }
  if (r->dest_count > 1) {
    game->trade_begin_route_pending = route;
    /* DOS FUN_2b5a_1e66 reads the unit's current stop (FUN_281f_0858) as the
     * @default preselect when the unit is already working this route; a
     * fresh assignment preselects stop 0. */
    int preselect = 0;
    const ColonizeUnit* pu = units_get_const(&game->units, sid);
    if (pu && pu->active && pu->orders == UNITS_ORDER_TRADE_ROUTE &&
        pu->follow_unit_id == route) {
      preselect = pu->col1_counter16;
    }
    game_trade_open_stop_picker(game, route, preselect);
    return;
  }
  game_trade_begin_at_stop(game, route, 0);
}

/* Begin Trade Route final step: order 2, aim the chosen stop. */
void game_trade_begin_at_stop(ColonizeGameState* game, int route, int stop_i) {
  if (!game || !game->units_ok || !game->col1_ok) {
    return;
  }
  const int sid = game->units.selected_id;
  if (sid < 0 || !units_order_trade_route(&game->units, sid)) {
    set_status(game, "Select a ship or wagon", NULL);
    return;
  }
  ColonizeUnit* u = units_get(&game->units, sid);
  if (!u || route < 0 || route >= (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
    set_status(game, "Invalid trade route", NULL);
    return;
  }
  u->follow_unit_id = route;
  u->col1_counter16 = 0;
  if (game_trade_route_aim_stop(game, u, stop_i)) {
    snprintf(
      game->status,
      sizeof(game->status),
      "Trade route: %s (stop %d/%d)",
      game->col1.trade_route[route].name,
      stop_i + 1,
      (int)game->col1.trade_route[route].dest_count
    );
  } else {
    set_status(game, "Trade route begun (could not aim first stop)", NULL);
  }
  game_wait_next_unit(game);
}

/* Defined with the colony-screen block below; the @ABANDON popup result
 * (AI_POPUP_TAG_COLONY_ABANDON) is applied from here. */

static void game_apply_map_confirm(ColonizeGameState* game) {
  if (!game || game->ai_popups.result_tag != AI_POPUP_TAG_MAP_CONFIRM) {
    return;
  }
  const GameMapConfirm conf = game->map_confirm;
  const int payload = game->map_confirm_payload;
  const bool yes = !game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1;
  game->map_confirm = GAME_MAP_CONFIRM_NONE;
  game->map_confirm_payload = -1;
  ai_popup_consume_result(&game->ai_popups);
  if (!yes) {
    set_status(game, "Cancelled", NULL);
    return;
  }
  switch (conf) {
    case GAME_MAP_CONFIRM_DISBAND:
      game_do_disband(game, payload);
      break;
    case GAME_MAP_CONFIRM_OVERBOARD:
      game_do_overboard(game, payload);
      break;
    case GAME_MAP_CONFIRM_QUIT:
      game->elapsed_ms = UINT32_MAX;
      break;
    case GAME_MAP_CONFIRM_TITLE_EXIT:
      game->elapsed_ms = UINT32_MAX;
      break;
    case GAME_MAP_CONFIRM_RETIRE: {
      ColonizeInputState empty;
      memset(&empty, 0, sizeof(empty));
      map_menu_handle_input(&game->map_menu, &empty, NULL, true);
      game_open_retire_score(game);
      break;
    }
    case GAME_MAP_CONFIRM_TRADE_DELETE:
      game_do_trade_delete_slot(game, payload);
      break;
    case GAME_MAP_CONFIRM_BUY_CONSTRUCTION:
      game_do_buy_construction(game, payload);
      break;
    case GAME_MAP_CONFIRM_FOUND_INLAND:
      (void)game_do_found_colony_at_unit(game, payload, false);
      break;
    case GAME_MAP_CONFIRM_EUROPE_SAIL:
      game_europe_sail_harbor(game, payload);
      break;
    default:
      break;
  }
}

void game_request_disband_confirm(ColonizeGameState* game) {
  if (!game || !game->units_ok) {
    return;
  }
  const int uid = game->units.selected_id;
  if (uid < 0) {
    set_status(game, "", NULL);
    return;
  }
  const ColonizeUnit* u = units_get_const(&game->units, uid);
  const ColonizeUnitType* ut = u ? units_type(&game->units, u->type_index) : NULL;
  const char* uname = (ut && ut->name[0]) ? ut->name : "unit";

  /* @DISBANDSHIP is an error OK (no Yes/No) when a ship still carries units. */
  if (units_is_sea(&game->units, uid)) {
    int has_pax = 0;
    if (u && u->cargo_count > 0) {
      has_pax = 1;
    } else {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* p = &game->units.units[i];
        if (p->active && p->aboard_ship_id == uid) {
          has_pax = 1;
          break;
        }
      }
    }
    if (has_pax) {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        &game->messages,
        "DISBANDSHIP",
        NULL,
        "",
        body,
        sizeof(body)
      );
      ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      set_status(game, "Cannot disband ship with units aboard", NULL);
      return;
    }
  }

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = uname;
  game_enqueue_yes_no(
    game,
    GAME_MAP_CONFIRM_DISBAND,
    uid,
    "SUREDISBAND",
    "",
    &tok
  );
}
/* ===================== Construction & Europe boycott/buy dialogs (game_do_buy_construction .. game_trade_service_screen_request) ===================== */


static void game_do_buy_construction(ColonizeGameState* game, int colony_id) {
  if (!game || !game->in_colony) {
    return;
  }
  ColonizeColony* colony = colonies_get_mut(&game->colonies, colony_id);
  ColonyScreenView* csv = &game->colony_screen;
  if (!colony || colony->building_in_production < 0) {
    set_status(game, "No project", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const ColonizeBuildingType* bt =
    colonies_building_type(&game->colonies, colony->building_in_production);
  const char* uname = NULL;
  if (!bt) {
    colonies_unit_build_info(colony->building_in_production, &uname, NULL, NULL);
  }
  const char* name = bt ? bt->name : (uname ? uname : "building");
  /* Player-corrected: Buy is NOT instant — it only tops hammers/tools up to
   * the completion threshold (colonies_buy_construction, DOS's
   * FUN_2f2b_5e44 formula). Actual completion happens next turn's
   * construction processing (turn_run_colony_building_completion /
   * turn_run_colony_unit_construction), same as a colony that reached the
   * threshold through ordinary Carpenter production. */
  const int gold_cost = colonies_construction_gold_cost(&game->colonies, colony, game->europe.difficulty);
  if (game->europe.gold < gold_cost) {
    set_status(game, "Need gold", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  if (colonies_buy_construction(
        &game->colonies, colony_id, game->europe.difficulty, &game->europe.gold
      )) {
    snprintf(game->status, sizeof(game->status), "Bought materials for %s (-%d$)", name, gold_cost);
    colony_screen_close_construction(csv);
  } else {
    set_status(game, "Cannot buy", NULL);
  }
  colony_screen_set_status(csv, game->status);
}

/*
 * bugs.md: a dialog the player raised from inside the colony screen must open
 * on the spot. game_update's queue pump is suppressed while
 * colony_zoom_popup_hold is set (the EOT batch must not interrupt a colony
 * the player elected to zoom into), so a BUY / NODOCKS popup enqueued in a
 * zoomed colony simply sat in the queue — invisible — until the screen closed
 * and the hold lifted. Player-initiated popups skip the queue order the same
 * way the @ABANDON confirm already does; DOS nests them in the colony
 * screen's own loop.
 */
void game_colony_present_now(ColonizeGameState* game, AiPopupTag tag) {
  if (!game || !game->in_colony) {
    return;
  }
  (void)ai_popup_present_now(&game->ai_popups, tag);
}

/*
 * FUN_38fd_2dfe (viceroy_unpacked.c:60904) — the Europe market strip's
 * boycott buy-back asks before it pays. DOS sets %STRING0 to the cargo name
 * and %STRING1 to the player's country, quotes the back taxes in %NUMBER0,
 * then runs `FUN_281f_0652(0x1033, 2)` = the @KISSUP two-row CHOICE and acts
 * only when the answer is row 2, "Pay {%NUMBER0$}." (row 1 is the refusal,
 * "This is taxation without representation! Unfair!"). The purse test and its
 * @KISSSORRY message come AFTER that answer, not before it, so a player who
 * cannot afford the boycott still sees the price first.
 *
 * The port used to pay silently on the click (europe_buyback_boycott + a
 * status line) and swallow the insufficient-funds arm entirely.
 */
void game_europe_ask_boycott_buyback(ColonizeGameState* game, int cargo_type) {
  if (!game || !game->europe_ok || !game->col1_ok) {
    return;
  }
  EuropeScreen* eu = &game->europe;
  const int human = game->human_nation;
  if (human < 0 || human >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }
  const int cost = europe_buyback_boycott_cost(eu, &game->col1, human, cargo_type);
  if (cost <= 0) {
    return;
  }
  const char* cname =
    eu->cargo[cargo_type].name[0] ? eu->cargo[cargo_type].name : "That cargo";
  const char* country = game->col1.player[human].country_name[0]
                          ? game->col1.player[human].country_name
                          : "Europe";
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  tok.string1 = country;
  tok.number0 = cost;
  tok.has_number0 = true;
  char body[AI_POPUP_BODY_LEN];
  char fallback[AI_POPUP_BODY_LEN];
  fallback[0] = '\0';
  popup_msg_fill(&game->messages, "KISSUP", &tok, fallback, body, sizeof(body));
  char labels_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(
    &game->messages, "KISSUP", &tok,
    "", "",
    labels_buf, labels
  );
  /* DOS row order: 1 = refuse, 2 = pay. Choice id 1 is the payer here, so the
   * label array stays in GAME.TXT order and only the ids are swapped. */
  const int ids[2] = {2, 1};
  if (ai_popup_enqueue_choice_ctx(
        &game->ai_popups, AI_POPUP_TAG_EUROPE_KISSUP, human, cargo_type, cost, NULL,
        body, labels, ids, 2
      )) {
    (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_EUROPE_KISSUP);
  }
}

void game_request_buy_construction_confirm(ColonizeGameState* game) {
  if (!game || !game->in_colony) {
    return;
  }
  ColonizeColony* colony = colonies_get_mut(&game->colonies, game->colony_view_id);
  ColonyScreenView* csv = &game->colony_screen;
  if (!colony || colony->building_in_production < 0) {
    set_status(game, "No project", NULL);
    colony_screen_set_status(csv, game->status);
    return;
  }
  const ColonizeBuildingType* bt =
    colonies_building_type(&game->colonies, colony->building_in_production);
  const char* uname = NULL;
  if (!bt) {
    colonies_unit_build_info(colony->building_in_production, &uname, NULL, NULL);
  }
  const char* bname = (bt && bt->name[0]) ? bt->name : (uname ? uname : "building");
  /* bugs.md: a project the colony already owns can't be bought or completed —
   * say so (@ALREADYHAVE) instead of quoting a meaningless price and letting
   * "Complete it" do nothing. */
  if (bt && colony->building_in_production < COLONIZE_BUILDING_TYPES_MAX &&
      colony->has_building[colony->building_in_production]) {
    PopupMsgTokens atok;
    memset(&atok, 0, sizeof(atok));
    atok.string0 = colony->name[0] ? colony->name : "colony";
    atok.string1 = bname;
    char abody[AI_POPUP_BODY_LEN];
    char afb[160];
    snprintf(afb, sizeof(afb), "%s has already built a %s!", atok.string0, bname);
    popup_msg_fill(&game->messages, "ALREADYHAVE", &atok, afb, abody, sizeof(abody));
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, abody);
    game_colony_present_now(game, AI_POPUP_TAG_INFO);
    return;
  }
  /* One uniform Buy, whether or not tools are short — DOS's own
   * FUN_2f2b_5e44 formula (colonies_construction_gold_cost) already sums
   * hammers+tools into one gold figure; the popup never differs by cause,
   * just "Complete it" / "Never mind" (@BUYME1) if affordable, or the
   * informational @BUYME0 sibling if not. Buy itself only tops the
   * resources up — it does not complete the project (player-corrected). */
  const int gold_cost = colonies_construction_gold_cost(&game->colonies, colony, game->europe.difficulty);
  if (game->europe.gold < gold_cost) {
    set_status(game, "Need gold", NULL);
    colony_screen_set_status(csv, game->status);
    PopupMsgTokens gtok;
    memset(&gtok, 0, sizeof(gtok));
    gtok.string0 = bname;
    gtok.number0 = gold_cost;
    gtok.has_number0 = true;
    gtok.number1 = game->europe.gold;
    gtok.has_number1 = true;
    char gbody[AI_POPUP_BODY_LEN];
    popup_msg_fill(&game->messages, "BUYME0", &gtok, "", gbody, sizeof(gbody));
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, gbody);
    game_colony_present_now(game, AI_POPUP_TAG_INFO);
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = bname;
  tok.number0 = gold_cost;
  tok.has_number0 = true;
  tok.number1 = game->europe.gold;
  tok.has_number1 = true;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->messages, "BUYME1", &tok, "", body, sizeof(body));
  char label_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  (void)popup_msg_section_labels(
    &game->messages, "BUYME1", &tok, "", "", label_buf, labels
  );
  const int ids[] = {0, 1}; /* Never mind / Complete it */
  game->map_confirm = GAME_MAP_CONFIRM_BUY_CONSTRUCTION;
  game->map_confirm_payload = game->colony_view_id;
  if (!ai_popup_enqueue_choice(
        &game->ai_popups, AI_POPUP_TAG_MAP_CONFIRM, NULL, body, labels, ids, 2
      )) {
    game->map_confirm = GAME_MAP_CONFIRM_NONE;
    set_status(game, "Dialog queue full", NULL);
    colony_screen_set_status(csv, game->status);
  } else {
    game_colony_present_now(game, AI_POPUP_TAG_MAP_CONFIRM);
  }
}

void game_request_overboard_confirm(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  const int sid = game->units.selected_id;
  if (sid < 0 || !units_is_transport(&game->units, sid)) {
    set_status(game, "No cargo to dump", NULL);
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  game_enqueue_yes_no(
    game,
    GAME_MAP_CONFIRM_OVERBOARD,
    sid,
    "OVERBOARD",
    "",
    &tok
  );
}

void game_open_find_colony_picker(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  const char* labels[CHEAT_LIST_MAX_OPTIONS];
  int ids[CHEAT_LIST_MAX_OPTIONS];
  char name_bufs[CHEAT_LIST_MAX_OPTIONS][CHEAT_LIST_LABEL_LEN];
  int count = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX && count < CHEAT_LIST_MAX_OPTIONS; ++i) {
    const ColonizeColony* c = &game->colonies.colonies[i];
    if (!c->active || c->nation_id != game->human_nation) {
      continue;
    }
    str_copy_trunc(
      name_bufs[count],
      sizeof(name_bufs[count]),
      c->name[0] ? c->name : "Colony"
    );
    labels[count] = name_bufs[count];
    ids[count] = c->id;
    count++;
  }
  if (count <= 0) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages, "NOCITY", NULL, "", body, sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    return;
  }
  char prompt[COLONIZE_MSG_LINE_LEN];
  popup_msg_fill(
    &game->messages, "FINDCITY", NULL, "", prompt, sizeof(prompt)
  );
  if (!cheat_list_open_find_colony(&game->cheat_list, prompt, labels, ids, count)) {
    set_status(game, "Find Colony unavailable", NULL);
  }
}

/*
 * Route picker (DOS FUN_647e_0796). mode: 1 = Begin (filter to the selected
 * unit's sea/land routes — FUN_2b5a_1e66's param 2/1), 2 = Edit (@TRADESELECT,
 * all routes), 3 = Delete (@TRADEDELETE). No routes at all → @TRADENONE; no
 * routes of the unit's type → @TRADENONE2 {sea|land}.
 */
void game_open_trade_route_picker(ColonizeGameState* game, int mode) {
  if (!game || !game->col1_ok) {
    set_status(game, "No trade routes", NULL);
    return;
  }
  int filter = 0; /* 0 = all; 1 = land only; 2 = sea only */
  if (mode == 1 && game->units_ok && game->units.selected_id >= 0) {
    filter = units_is_sea(&game->units, game->units.selected_id) ? 2 : 1;
  }
  const int total = game_trade_route_count(game);
  if (total <= 0) {
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(
      &game->messages, "TRADENONE", NULL,
      "", body, sizeof(body)
    );
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    return;
  }
  const char* labels[CHEAT_LIST_MAX_OPTIONS];
  int ids[CHEAT_LIST_MAX_OPTIONS];
  char name_bufs[CHEAT_LIST_MAX_OPTIONS][CHEAT_LIST_LABEL_LEN];
  int count = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT && count < CHEAT_LIST_MAX_OPTIONS;
       ++i) {
    const ColonizeCol1TradeRoute* r = &game->col1.trade_route[i];
    if (r->name[0] == '\0' && r->dest_count == 0) {
      continue;
    }
    if (filter != 0 && ((filter == 2) != (r->sea != 0))) {
      continue;
    }
    /* DOS rows are "N. NAME" (FUN_647e_0796's number + FUN_1d1d_11b4). */
    snprintf(
      name_bufs[count], sizeof(name_bufs[count]), "%d. %s", i + 1,
      r->name[0] ? r->name : "Route"
    );
    labels[count] = name_bufs[count];
    ids[count] = i;
    count++;
  }
  if (count <= 0) {
    /* @TRADENONE2 {%STRING0} — LABELS @ROUTE "Sea" / "Land". */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 =
      (filter == 2) ? game->trade_screen.lab_sea : game->trade_screen.lab_land;
    char body[AI_POPUP_BODY_LEN];
    char fb[AI_POPUP_BODY_LEN];
    fb[0] = '\0';
    popup_msg_fill(&game->messages, "TRADENONE2", &tok, fb, body, sizeof(body));
    ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
    return;
  }
  game->trade_select_mode = mode;
  const char* sec = (mode == 3) ? "TRADEDELETE" : "TRADESELECT";
  char prompt[COLONIZE_MSG_LINE_LEN];
  popup_msg_fill(
    &game->messages,
    sec,
    NULL,
    "",
    prompt,
    sizeof(prompt)
  );
  if (!cheat_list_open_trade_select(&game->cheat_list, prompt, labels, ids, count)) {
    game->trade_select_mode = 0;
    set_status(game, "Trade select unavailable", NULL);
  }
}

static void game_open_found_name_entry(ColonizeGameState* game, int colony_id) {
  if (!game) {
    return;
  }
  const ColonizeColony* col = colonies_get(&game->colonies, colony_id);
  char prompt[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages,
    "COLONY",
    NULL,
    "",
    prompt,
    sizeof(prompt)
  );
  const char* seed = col && col->name[0] ? col->name : "Colony";
  if (!name_entry_open(
        &game->name_entry, NAME_ENTRY_KIND_FOUND, prompt, seed, colony_id
      )) {
    /* No prompt to answer means nothing will fire the FUN_281f_0608 tail;
     * open the colony now rather than leaving it armed for the next one. */
    set_status(game, "Name entry failed", NULL);
    if (game->found_open_colony_id == colony_id) {
      game->found_open_colony_id = -1;
      game_enter_colony(game, colony_id);
    }
  }
}

static void game_landho_default_region(const ColonizeGameState* game, char* out, size_t out_size) {
  static const char* k_regions[4] = {
    "", "", "", ""
  };
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!game) {
    str_copy_trunc(out, out_size, "");
    return;
  }
  /* Seed from the human nation's @COLONYNAME — do not trust a stale
   * europe.colony_region left by europe_load (always nation 0 / New England). */
  const int nation = game->human_nation;
  if (game->names_ok && nation >= 0 && nation <= 3) {
    const ColonizeMsgSection* reg = assets_msg_find(&game->names, "COLONYNAME");
    if (reg && nation < reg->line_count) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", reg->lines[nation]);
      /* Trim trailing CR/spaces lightly. */
      size_t n = strlen(line);
      while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n' || line[n - 1] == ' ')) {
        line[--n] = '\0';
      }
      if (line[0] && line[0] != ';') {
        str_copy_trunc(out, out_size, line);
        return;
      }
    }
  }
  if (nation >= 0 && nation <= 3) {
    str_copy_trunc(out, out_size, k_regions[nation]);
  } else {
    str_copy_trunc(out, out_size, k_regions[0]);
  }
}

static void game_open_landho_name_entry(ColonizeGameState* game) {
  if (!game) {
    return;
  }
  char prompt[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    &game->messages,
    "LANDHO",
    NULL,
    "",
    prompt,
    sizeof(prompt)
  );
  char seed[48];
  game_landho_default_region(game, seed, sizeof(seed));
  if (!name_entry_open(
        &game->name_entry, NAME_ENTRY_KIND_LANDHO, prompt, seed, -1
      )) {
    /* Fallback: mark discovery so DOS woodcut one-shot does not re-fire. */
    if (game->col1_ok) {
      col1_bridge_mark_new_world_discovered(&game->col1, game->human_nation);
    }
    str_copy_trunc(game->europe.colony_region, sizeof(game->europe.colony_region), seed);
    if (game->col1_ok && game->human_nation >= 0 && game->human_nation < 4) {
      str_copy_trunc(
        game->col1.player[game->human_nation].country_name,
        sizeof(game->col1.player[game->human_nation].country_name),
        seed
      );
    }
    set_status(game, "Name entry failed", NULL);
  }
}

static void game_apply_name_entry_result(ColonizeGameState* game) {
  if (!game || !game->name_entry.has_result) {
    return;
  }
  const NameEntryKind kind = game->name_entry.result_kind;
  if (kind == NAME_ENTRY_KIND_TRADE_NAME) {
    /* Create wizard stage 3 (@TRADENAME): cancel aborts the whole flow. */
    if (game->name_entry.result_cancelled || game->trade_create_stage != 3) {
      game->trade_create_stage = 0;
    } else {
      str_copy_trunc(
        game->trade_create_name, sizeof(game->trade_create_name),
        game->name_entry.result_name
      );
      game_trade_wizard_open_dest(game, 2);
    }
    game->name_entry.has_result = false;
    return;
  }
  if (kind == NAME_ENTRY_KIND_TRADE_RENAME) {
    if (!game->name_entry.result_cancelled && game->col1_ok &&
        game->name_entry.result_name[0] &&
        game->name_entry.result_colony_id >= 0 &&
        game->name_entry.result_colony_id < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT) {
      ColonizeCol1TradeRoute* r =
        &game->col1.trade_route[game->name_entry.result_colony_id];
      str_copy_trunc(r->name, sizeof(r->name), game->name_entry.result_name);
    }
    game->name_entry.has_result = false;
    return;
  }
  if (kind == NAME_ENTRY_KIND_LANDHO) {
    char fallback[48];
    game_landho_default_region(game, fallback, sizeof(fallback));
    const char* name =
      game->name_entry.result_name[0] ? game->name_entry.result_name : fallback;
    str_copy_trunc(game->europe.colony_region, sizeof(game->europe.colony_region), name);
    if (game->col1_ok && game->human_nation >= 0 && game->human_nation < 4) {
      str_copy_trunc(
        game->col1.player[game->human_nation].country_name,
        sizeof(game->col1.player[game->human_nation].country_name),
        name
      );
    }
    if (game->col1_ok) {
      col1_bridge_mark_new_world_discovered(&game->col1, game->human_nation);
    }
    snprintf(game->status, sizeof(game->status), "New land: %s", name);
  } else if (!game->name_entry.result_cancelled) {
    ColonizeColony* col =
      colonies_get_mut(&game->colonies, game->name_entry.result_colony_id);
    if (col) {
      str_copy_trunc(col->name, sizeof(col->name), game->name_entry.result_name);
      snprintf(
        game->status, sizeof(game->status), "%s: %s", reports_misc_display_word(80, ""), col->name
      );
      if (game->in_colony) {
        colony_screen_set_status(&game->colony_screen, game->status);
      }
    }
  }
  /*
   * FUN_479b_076e tail: once the human has named the new colony, DOS calls
   * FUN_281f_0608(colony) and drops the player straight into its screen.
   * Only for a founding — a rename from inside the colony screen leaves the
   * view exactly where it was.
   */
  if (kind == NAME_ENTRY_KIND_FOUND && game->found_open_colony_id >= 0) {
    const int cid = game->found_open_colony_id;
    game->found_open_colony_id = -1;
    if (!game->in_colony) {
      game_enter_colony(game, cid);
    }
  }
  game->name_entry.has_result = false;
}

void game_try_prompt_landho(ColonizeGameState* game) {
  if (!game || !game->col1_ok || !game->world_map_ok) {
    return;
  }
  if (game->name_entry.open || game->in_menu) {
    return;
  }
  if (game->human_nation < 0 || game->human_nation > 3) {
    return;
  }
  if (game->col1.player[game->human_nation].named_new_world) {
    return;
  }
  if (!col1_bridge_human_has_seen_land(&game->world_map, game->human_nation)) {
    return;
  }
  /*
   * FUN_4720_049e: the ring scan that first finds land sets the nation's
   * named_new_world bit and immediately calls FUN_281f_0f6c → FUN_2b5a_001e
   * → woodcut 1. The @LANDHO naming prompt opens behind it and takes input
   * once the woodcut is dismissed.
   */
  (void)woodcut_fire(&game->col1, WOODCUT_DISCOVERY_OF_THE_NEW_WORLD);
  game_open_landho_name_entry(game);
}

/* Persist DEBUG HUD toggles (mouse coords, building rects, logs) to settings.json. */
void game_persist_debug_hud(const ColonizeGameState* game) {
  if (!game || !settings_is_loaded()) {
    return;
  }
  ColonizeSettings prefs = *settings_get();
  prefs.show_mouse_coords = game->debug_show_mouse_coords;
  prefs.show_building_rects = game->debug_building_rects;
  prefs.debug_logs = game->debug_logs;
  settings_set(&prefs);
  char err[256];
  if (!settings_flush(err, sizeof(err))) {
    diag_warn("Could not save settings: %s", err);
  }
}

/*
 * Port-only: mirror whatever the options dialogs just changed into
 * settings.json so the choice survives the process. DOS had no such file —
 * see settings.h.
 */
static void game_persist_settings(const ColonizeGameState* game) {
  ColonizeSettings prefs = *settings_get();
  if (game && game->col1_ok) {
    settings_capture_from_head(&prefs, &game->col1.head);
  }
  settings_set(&prefs);
  char err[256];
  if (!settings_flush(err, sizeof(err))) {
    diag_warn("Could not save settings: %s", err);
  }
}

static void game_apply_options_result(ColonizeGameState* game) {
  if (!game || !game->options_dlg.has_result) {
    return;
  }
  if (!game->options_dlg.result_cancelled) {
    if (game->options_dlg.result_kind == OPTIONS_KIND_GAME && game->col1_ok) {
      options_dialog_apply_game(&game->options_dlg, &game->col1.head.game_options);
      game_persist_settings(game);
      set_status(game, "Game options updated", NULL);
    } else if (game->options_dlg.result_kind == OPTIONS_KIND_COLONY && game->col1_ok) {
      options_dialog_apply_colony(
        &game->options_dlg, &game->col1.head.colony_report_options
      );
      game_persist_settings(game);
      set_status(game, "", NULL);
    } else if (game->options_dlg.result_kind == OPTIONS_KIND_SOUND) {
      bool bg = true, ev = true, sfx = true;
      if (options_dialog_apply_sound(&game->options_dlg, &bg, &ev, &sfx)) {
        ColonizeSoundOptions so = sound_get_options();
        so.background_music = bg;
        so.event_music = ev;
        so.sound_effects = sfx;
        sound_set_options(so);
        if (game->col1_ok) {
          game->col1.head.tut2.background_music = bg ? 1 : 0;
          game->col1.head.tut2.event_music = ev ? 1 : 0;
          game->col1.head.tut2.sound_effects = sfx ? 1 : 0;
          game_persist_settings(game);
        } else {
          ColonizeSettings prefs = *settings_get();
          prefs.background_music = bg;
          prefs.event_music = ev;
          prefs.sound_effects = sfx;
          settings_set(&prefs);
          char err[256];
          if (!settings_flush(err, sizeof(err))) {
            diag_warn("Could not save settings: %s", err);
          }
        }
        set_status(game, "Sound options updated", NULL);
      }
    }
  }
  game->options_dlg.has_result = false;
}

/* Service the EDIT TRADE ROUTE screen's posted request (see trade_screen.h). */
static void game_trade_service_screen_request(ColonizeGameState* game) {
  TradeScreen* ts = &game->trade_screen;
  const TradeScreenRequest req = ts->request;
  ts->request = TRADE_SCREEN_REQ_NONE;
  if (req == TRADE_SCREEN_REQ_NONE || !game->col1_ok || ts->route < 0) {
    if (req == TRADE_SCREEN_REQ_CLOSE) {
      trade_screen_close(ts);
    }
    return;
  }
  ColonizeCol1TradeRoute* r = &game->col1.trade_route[ts->route];
  switch (req) {
    case TRADE_SCREEN_REQ_CLOSE:
      trade_screen_close(ts);
      break;
    case TRADE_SCREEN_REQ_RENAME: {
      char prompt[AI_POPUP_BODY_LEN];
      popup_msg_fill(
        &game->messages, "TRADENAME", NULL, "",
        prompt, sizeof(prompt)
      );
      (void)name_entry_open(
        &game->name_entry, NAME_ENTRY_KIND_TRADE_RENAME, prompt, r->name, ts->route
      );
      break;
    }
    case TRADE_SCREEN_REQ_DEST:
      game_trade_open_dest_picker(game, ts->request_stop);
      break;
    case TRADE_SCREEN_REQ_CARGO_ADD:
      game_trade_open_cargo_picker(game, ts->request_stop, ts->request_is_load);
      break;
    case TRADE_SCREEN_REQ_CARGO_REMOVE: {
      /* DOS FUN_647e_0f2c: clicking an icon removes that list entry. */
      const int stop_i = ts->request_stop;
      const int slot = ts->request_slot;
      if (stop_i < 0 || stop_i >= (int)r->dest_count) {
        break;
      }
      ColonizeCol1TradeStop* st = &r->stop[stop_i];
      uint8_t* nib = ts->request_is_load ? st->load_cargo_nibbles : st->unload_cargo_nibbles;
      const int count = ts->request_is_load ? (int)st->load_count : (int)st->unload_count;
      if (slot < 0 || slot >= count) {
        break;
      }
      for (int i = slot; i < count - 1; ++i) {
        col1_trade_nibble_set(nib, i, col1_trade_nibble_cargo(nib, i + 1));
      }
      col1_trade_nibble_set(nib, count - 1, 0);
      if (ts->request_is_load) {
        st->load_count--;
      } else {
        st->unload_count--;
      }
      break;
    }
    default:
      break;
  }
}
/* ===================== Modal input handling & AI popup result appliers (game_handle_modal_input .. game_apply_ai_popup_result) ===================== */


/*
 * Parent-view hotkeys must not fire while any wood modal is open (name entry,
 * howmuch, options, ai_popup, lists, stack picker).
 */
bool game_handle_modal_input(ColonizeGameState* game, const ColonizeInputState* input) {
  if (!game || !input) {
    return false;
  }
  if (game->woodcut.open) {
    return woodcut_handle_input(&game->woodcut, input);
  }
  if (game->declaration.open) {
    return declaration_handle_input(&game->declaration, input);
  }
  if (game->opening.open) {
    return opening_handle_input(&game->opening, input);
  }
  if (game->closing.open) {
    return closing_handle_input(&game->closing, input);
  }
  if (game->pick_music.open) {
    const ColonizeFont* pm_font = pick_music_font(
      &game->pick_music,
      game->colony_font_ok ? &game->colony_font : NULL,
      game->intro_font_ok ? &game->intro_font
                          : (game->menu_font_ok ? &game->menu_font : NULL)
    );
    pick_music_handle_input(
      &game->pick_music, &game->messages, input, pm_font, game->status, sizeof(game->status)
    );
    return true;
  }
  if (game->save_load.open) {
    save_load_handle_input(&game->save_load, input);
    game_apply_save_load_result(game);
    return true;
  }
  if (game->options_dlg.open) {
    options_dialog_handle_input(&game->options_dlg, input);
    game_apply_options_result(game);
    return true;
  }
  if (game->combat_analysis.open) {
    combat_analysis_handle_input(&game->combat_analysis, input);
    return true;
  }
  if (game->name_entry.open) {
    name_entry_handle_input(&game->name_entry, input);
    game_apply_name_entry_result(game);
    return true;
  }
  if (game->howmuch.open) {
    howmuch_handle_input(&game->howmuch, input);
    if (game->howmuch.has_result) {
      if (!game->howmuch.result_cancelled && game->howmuch.result_amount > 0) {
        game_apply_howmuch_result(game);
      }
      game->howmuch.has_result = false;
    }
    return true;
  }
  if (game->cheat_list.open) {
    cheat_list_handle_input(&game->cheat_list, input);
    const bool cancelled = !game->cheat_list.open && !game->cheat_list.has_result;
    game_apply_cheat_list_result(game);
    if (cancelled) {
      /* An Esc'd picker aborts whatever trade flow was waiting on it. */
      game->trade_create_stage = 0;
      game->trade_begin_route_pending = -1;
      game->trade_dest_stop = -1;
      game->trade_cargo_stop = -1;
      game->trade_select_mode = 0;
      game->goto_port_pending_unit = -1;
    }
    return true;
  }
  if (game->ai_popups.open) {
    /*
     * bugs.md: F1 on the Congress debate CHOICE opens the highlighted
     * father's Colonizopedia entry; closing that page comes straight back
     * to the popup (the persistent slate re-presents it — no reroll).
     */
    /*
     * DOS FUN_4345_06d2 arms the dialog's "explain this row" flag (DS:0x1f66
     * = 1) before FUN_291f_016a, so FUN_6f74_2580's right-button arm returns
     * the row under the cursor with DS:0x1f68 = 1; 06d2 then calls
     * FUN_2a1f_0062 (→ FUN_6cb2_1f28, the Colonizopedia founding-father
     * article) and loops back to rebuild the same dialog. Right-click is the
     * DOS gesture; F1 below is the port's keyboard equivalent.
     */
    if ((input->last_key == COLONIZE_KEY_F1 || input->mouse_right_clicked) &&
        game->ai_popups.current.tag == AI_POPUP_TAG_FF_CONGRESS &&
        game->ai_popups.current.kind == AI_POPUP_KIND_CHOICE) {
      const AiPopupRequest* cur = &game->ai_popups.current;
      const int sel = input->mouse_right_clicked
                        ? ai_popup_choice_row_at(
                            &game->ai_popups, input->mouse_x, input->mouse_y
                          )
                        : game->ai_popups.selection;
      if (sel >= 0 && sel < cur->choice_count) {
        const int ff_index = cur->choice_ids[sel];
        if (ff_index >= 0 && ff_index < PEDIA_FATHER_COUNT) {
          /* Cancel the open dialog — apply's cancelled branch re-enqueues
           * the identical slate, which re-presents once the pedia closes.
           * bugs.md #368: the re-enqueue lands at the BACK of the queue, so
           * without this the article handed the screen to whatever else was
           * waiting instead of back to the debate. Reading a candidate's
           * entry must not cost the player their place in the vote. */
          ai_popup_cancel_current(&game->ai_popups);
          game_apply_ai_popup_result(game);
          ai_popup_move_tag_to_front(&game->ai_popups, AI_POPUP_TAG_FF_CONGRESS);
          game_open_pedia_article(game, PEDIA_CAT_FATHER, ff_index, false);
          return true;
        }
      }
    }
    ai_popup_handle_input(&game->ai_popups, input);
    game_apply_ai_popup_result(game);
    return true;
  }
  if (game->trade_screen.open) {
    trade_screen_handle_input(
      &game->trade_screen, game->col1_ok ? &game->col1 : NULL,
      game->unit_icons_ok ? &game->unit_icons : NULL, input
    );
    game_trade_service_screen_request(game);
    return true;
  }
  if (game->unit_stack.open) {
    int select_id = -1;
    unit_stack_handle_input(&game->unit_stack, &game->units, input, &select_id);
    if (select_id >= 0) {
      /* Picking a row out of the tile stack is the same activation DOS
       * clears the orders byte on (2b5a:1dd0). */
      game_click_activate_unit(game, select_id);
    }
    return true;
  }
  return false;
}

/*
 * Warehouse -> ship hold transfer with its status line (audit GL-17: four
 * copies — howmuch apply, the drag drop, key '+' and key L). The caller
 * publishes game->status to the colony screen; the howmuch and drag paths
 * already did, the key paths do it once at the end of their own arm.
 */
void game_colony_load_hold(ColonizeGameState* game, int unit_id, int cargo, int amount) {
  const int moved = colonies_transfer_to_unit(
    &game->colonies, game->colony_view_id, &game->units, unit_id, cargo, amount
  );
  if (moved > 0) {
    snprintf(game->status, sizeof(game->status), "Loaded %d", moved);
  } else {
    set_status(game, "No empty hold", NULL);
  }
}

/*
 * Ship hold -> warehouse transfer of ONE hold, with the four-way status line
 * and the @WAREHOUSEFULL chrome (audit GL-18: the drag drop and key U carried
 * this twice). The hold's type/amount are peeked first because the transfer
 * empties it. empty_msg is the two paths' own wording for "nothing moved and
 * the warehouse was not full" and is the one thing that differed.
 *
 * game_colony_unload_all_cargo is NOT this function in a loop: it reports one
 * aggregate total over every hold, not a line per hold.
 */
void game_colony_unload_hold(
  ColonizeGameState* game,
  int unit_id,
  int hold,
  const char* empty_msg
) {
  bool full = false;
  int peek_type = -1;
  int peek_amt = 0;
  const ColonizeUnit* tu = units_get_const(&game->units, unit_id);
  if (tu && hold >= 0 && hold < COLONIZE_UNIT_CARGO_MAX) {
    peek_type = tu->hold_goods_type[hold];
    peek_amt = tu->hold_goods_amount[hold];
  }
  const int moved = colonies_transfer_from_unit(
    &game->colonies, game->colony_view_id, &game->units, unit_id, hold, &full
  );
  if (moved > 0 && full) {
    snprintf(game->status, sizeof(game->status), "Unloaded %d (Warehouse full)", moved);
    game_emit_warehouse_full(game, game->colony_view_id, peek_type, moved, moved);
  } else if (moved > 0) {
    snprintf(game->status, sizeof(game->status), "Unloaded %d", moved);
  } else if (full) {
    set_status(game, "Warehouse full", NULL);
    game_emit_warehouse_full(game, game->colony_view_id, peek_type, peek_amt, 0);
  } else {
    set_status(game, empty_msg, NULL);
  }
}

static void game_apply_howmuch_result(ColonizeGameState* game) {
  if (!game || game->howmuch.result_cancelled || game->howmuch.result_amount <= 0) {
    return;
  }
  const int amt = game->howmuch.result_amount;
  const int cargo = game->howmuch.result_cargo;
  const HowmuchKind kind = game->howmuch.result_kind;
  if (kind == HOWMUCH_KIND_MOVE) {
    /* @HOWMUCH3: ship-to-ship transfer of a chosen amount (bugs.md #432
     * pattern), never through the warehouse. */
    const int src = game->colony_screen.transport_unit_id;
    const int dst = game->howmuch_move_dst_unit_id;
    const int hold = game->howmuch.result_payload;
    if (src < 0 || dst < 0) {
      return;
    }
    const int moved = units_load_goods(&game->units, dst, cargo, amt);
    if (moved > 0) {
      (void)units_unload_goods_hold(&game->units, src, hold, NULL, NULL);
      if (moved < amt) {
        (void)units_load_goods(&game->units, src, cargo, amt - moved);
      }
      snprintf(game->status, sizeof(game->status), "Transferred %d", moved);
      colony_screen_set_status(&game->colony_screen, game->status);
    } else {
      set_status(game, "No empty hold", NULL);
      colony_screen_set_status(&game->colony_screen, game->status);
    }
    return;
  }
  if (kind == HOWMUCH_KIND_LOAD) {
    ColonyScreenView* csv = &game->colony_screen;
    if (!game->in_colony || csv->transport_unit_id < 0) {
      return;
    }
    game_colony_load_hold(game, csv->transport_unit_id, cargo, amt);
    colony_screen_set_status(csv, game->status);
  } else if (kind == HOWMUCH_KIND_UNLOAD) {
    ColonyScreenView* csv = &game->colony_screen;
    if (!game->in_colony || csv->transport_unit_id < 0) {
      return;
    }
    bool full = false;
    const int hold = game->howmuch.result_payload;
    int peek_type = -1;
    {
      const ColonizeUnit* tu = units_get_const(&game->units, csv->transport_unit_id);
      if (tu && hold >= 0 && hold < COLONIZE_UNIT_CARGO_MAX) {
        peek_type = tu->hold_goods_type[hold];
      }
    }
    const int moved = colonies_transfer_from_unit_amount(
      &game->colonies,
      game->colony_view_id,
      &game->units,
      csv->transport_unit_id,
      hold,
      amt,
      &full
    );
    if (moved > 0 && full) {
      snprintf(game->status, sizeof(game->status), "Unloaded %d (Warehouse full)", moved);
      game_emit_warehouse_full(game, game->colony_view_id, peek_type, moved, moved);
    } else if (moved > 0) {
      snprintf(game->status, sizeof(game->status), "Unloaded %d", moved);
    } else if (full) {
      set_status(game, "Warehouse full", NULL);
      game_emit_warehouse_full(game, game->colony_view_id, peek_type, amt, 0);
    } else {
      set_status(game, "Cannot unload", NULL);
    }
    colony_screen_set_status(csv, game->status);
  } else if (kind == HOWMUCH_KIND_BUY) {
    EuropeScreen* eu = &game->europe;
    if (!game->in_europe || eu->selected_harbor < 0) {
      return;
    }
    europe_buy_cargo_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .col1=(ColonizeCol1Save*)(&game->col1), .col1_ok=true, .europe=(EuropeScreen*)(eu)}, game->human_nation, eu->selected_harbor, cargo, amt);
        game_europe_drain_price_events(game);
  } else if (kind == HOWMUCH_KIND_SELL) {
    EuropeScreen* eu = &game->europe;
    if (!game->in_europe || eu->selected_harbor < 0) {
      return;
    }
    const int hold = game->howmuch.result_payload;
    if (hold < 0) {
      return;
    }
    europe_sell_hold_partial(
      eu, &game->col1, game->human_nation, eu->selected_harbor, hold, amt
    );
    game_europe_drain_price_events(game);
  } else if (kind == HOWMUCH_KIND_SOUND_TEST) {
    sound_play(amt);
    char line[32];
    snprintf(line, sizeof(line), "Playing sound #%d", amt);
    set_status(game, line, NULL);
  }
}

/*
 * game_apply_ai_popup_result stages. Each returns true when it recognised
 * and consumed the result tag, and the applier returns immediately; false
 * falls through to the next stage. Grouped by subject, in the DOS order the
 * original if-chain tested them.
 */
/* Map confirm, trade-route wizard type, and the colony-screen dialogs
 * (event zoom, abandon, Clear Specialty). */
static bool game_apply_popup_map_and_colony(ColonizeGameState* game) {
  if (game->ai_popups.result_tag == AI_POPUP_TAG_MAP_CONFIRM) {
    game_apply_map_confirm(game);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_TRADE_TYPE) {
    /* @TRADETYPE (create wizard stage 2): choice 1 = Sea, 0 = Land (DOS). */
    const bool cancelled = game->ai_popups.result_cancelled;
    const int choice = game->ai_popups.result_choice_id;
    ai_popup_consume_result(&game->ai_popups);
    if (cancelled || game->trade_create_stage != 2) {
      game->trade_create_stage = 0;
  return true;
    }
    game->trade_create_sea = (choice == 1) ? 1u : 0u;
    game->trade_create_stage = 3;
    game_trade_wizard_open_name(game);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_COLONY_EVENT) {
    /* Choice 2 = "Zoom to colony." (DOS FUN_364b_0000 result 2 → DS:0xa898);
     * 1 / Esc / right-click = "Continue turn.". The screen itself opens once
     * the colony's remaining messages are answered (take_colony_zoom). */
    if (!game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 2) {
      ai_popup_colony_zoom_elect(&game->ai_popups, game->ai_popups.result_payload);
    }
    ai_popup_consume_result(&game->ai_popups);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_COLONY_ABANDON) {
    /* DOS 2f2b: `DEC AX; JZ` — only choice 1 abandons; 2 / Esc keeps it. */
    const bool go = !game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1;
    const int who = game->ai_popups.result_nation_a;
    const int role = game->ai_popups.result_nation_b;
    ai_popup_consume_result(&game->ai_popups);
    if (go && game->in_colony) {
      colony_screen_close_eject(&game->colony_screen);
      game_colony_finish_eject(game, who, role);
    }
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_COLONY_CLEARSPEC) {
    /* @LOBOTOMIZE: only choice 1 clears — FUN_281f_0cae(colonist, 0x1c). */
    const bool go = !game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1;
    const int who = game->ai_popups.result_nation_a;
    ai_popup_consume_result(&game->ai_popups);
    if (go && game->in_colony) {
      ColonizeColony* col = colonies_get_mut(&game->colonies, game->colony_view_id);
      if (col && who >= 0 && who < col->colonist_count) {
        col->colonists[who].profession = COLONIZE_PROF_FREE_COLONIST;
        set_status(game, "Specialty cleared", NULL);
        colony_screen_set_status(&game->colony_screen, game->status);
      }
    }
  return true;
  }
  return false;
}

/* Ship dialogs: @LANDFALL disembark choice and @SAILHOME. */
static bool game_apply_popup_voyage(ColonizeGameState* game) {
  if (game->ai_popups.result_tag == AI_POPUP_TAG_LANDFALL) {
    if (!game->ai_popups.result_cancelled) {
      const int ship_id = game->ai_popups.result_nation_a;
      const int dest_x = game->ai_popups.result_nation_b;
      const int dest_y = game->ai_popups.result_payload;
      const int choice = game->ai_popups.result_choice_id;
      ColonizeUnit* ship = units_get(&game->units, ship_id);
      /* choice 0 = Stay With Ships; 1 = Make Landfall (one passenger ashore). */
      if (ship && units_is_sea(&game->units, ship_id) && choice == 1) {
        /* DOS 4720: prefer cargo with moves; sentry cargo still eligible. */
        const int pax_id = units_first_landfall_cargo(&game->units, ship_id);
        if (pax_id >= 0 &&
            units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, ship_id, pax_id, dest_x, dest_y)) {
          /* bugs.md: landfall costs the SHIP nothing — the passengers move,
           * not the ship. The old 1 MP "coastal order" charge on the ship
           * was invented. The passenger's landfall step, however, consumes
           * its WHOLE turn, not just the shore terrain cost. */
          {
            ColonizeUnit* pax = units_get(&game->units, pax_id);
            if (pax) {
              pax->moves = 0;
            }
          }
          game->units.selected_id = pax_id;
          snprintf(game->status, sizeof(game->status), "Landfall at (%d,%d)", dest_x, dest_y);
          game_after_unit_action(game);
        } else {
          set_status(game, "Landfall failed", NULL);
        }
      } else if (choice == 0) {
        set_status(game, "Staying with ships", NULL);
      }
    }
    ai_popup_consume_result(&game->ai_popups);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_SAILHOME) {
    const bool cancelled = game->ai_popups.result_cancelled;
    const int ship_id = game->ai_popups.result_nation_a;
    const int dest_x = game->ai_popups.result_nation_b;
    const int dest_y = game->ai_popups.result_payload;
    const int choice = game->ai_popups.result_choice_id;
    ai_popup_consume_result(&game->ai_popups);
    ColonizeUnit* ship = units_get(&game->units, ship_id);
    if (!cancelled && ship && ship->active && units_is_sea(&game->units, ship_id)) {
      if (choice == 1) {
        /* Yes, steady as she goes — sail for Europe from the lane tile.
         * Hand control on only when the ship really left (a refused
         * crossing leaves it selected on the lane tile). */
        if (units_on_high_seas(&game->world_map, ship->x, ship->y) && game->europe_ok &&
            game_ship_sail_to_europe(game, ship_id)) {
          if (game_select_next_unit_awaiting_orders(game)) {
            game->view_pieces_mode = false;
          }
        }
      } else {
        /* No, remain in these waters — the DOS reason-5 tail still commits
         * the eastward step (sea lanes are traversable). */
        if (game_commit_sea_lane_step(game, ship_id, dest_x, dest_y)) {
          game_after_unit_action(game);
        }
      }
    }
  return true;
  }
  return false;
}

/* Native contact: land demand, the village raid warn, the "whack" prompt
 * and the European war declaration. */
static bool game_apply_popup_contact(ColonizeGameState* game) {
  /*
   * @INDIANLAND / @INDIANFOREST / @INDIANROAD result (DOS 1-based): 1 respect
   * → cancel the order; 2 offer gold → colonies_indian_land_pay + @INDIANBRIBE,
   * then proceed; 3 take → proceed unpaid (no immediate DOS consequence).
   */
  if (game->ai_popups.result_tag == AI_POPUP_TAG_INDIAN_LAND) {
    const int uid = game->ai_popups.result_nation_a;
    const int kind = game->ai_popups.result_nation_b;
    const int x = game->ai_popups.result_payload & 0xff;
    const int y = (game->ai_popups.result_payload >> 8) & 0xff;
    const int choice = game->ai_popups.result_cancelled ? GAME_INDIAN_LAND_RESPECT
                                                        : game->ai_popups.result_choice_id;
    ai_popup_consume_result(&game->ai_popups);
    if (choice == GAME_INDIAN_LAND_RESPECT) {
      set_status(game, "We respect their wishes", NULL);
  return true;
    }
    const int hn = game->human_nation;
    if (choice == GAME_INDIAN_LAND_OFFER && game->col1_ok && hn >= 0 && hn < 4) {
      ColonizeCol1Save* col1 = &game->col1;
      /* Stamp → pay-by-pointer → pull: the one place an absolute copy is
       * legitimate, because the pay helper writes the record directly (G3). */
      europe_gold_stamp_record(&game->europe, col1);
      const int price = colonies_indian_land_purchase_gold(col1, &game->world_map, x, y, hn);
      if (price > 0 && col1->nation[hn].gold >= (uint32_t)price) {
        colonies_indian_land_pay(col1, &game->world_map, x, y, hn, &col1->nation[hn].gold, price);
        game->europe.gold = (int)col1->nation[hn].gold;
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages,
          "INDIANBRIBE",
          NULL,
          "",
          body,
          sizeof(body)
        );
        ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      }
    }
    switch (kind) {
      case GAME_INDIAN_LAND_FOUND:
        (void)game_do_found_colony_at_unit(game, uid, true);
        break;
      case GAME_INDIAN_LAND_FOREST:
      case GAME_INDIAN_LAND_ROAD: {
        char msg[96];
        msg[0] = '\0';
        game->units.selected_id = uid;
        const bool ok =
          kind == GAME_INDIAN_LAND_FOREST
            ? units_pioneer_plow_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, uid, msg, sizeof(msg), &game->ai_popups, &game->messages)
            : units_pioneer_road_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map)}, uid, msg, sizeof(msg), &game->ai_popups, &game->messages);
        if (!ok) {
          set_status(game, msg[0] ? msg : "Cannot work here", NULL);
        } else {
          set_status(game, msg, NULL);
          game_wait_next_unit(game);
        }
        break;
      }
      default:
        break;
    }
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_CONTACT_WHACK) {
    const int unit_id = game->ai_popups.result_nation_a;
    const int indian_nation = game->ai_popups.result_nation_b;
    const int dest_x = game->ai_popups.result_payload & 0xff;
    const int dest_y = (game->ai_popups.result_payload >> 8) & 0xff;
    if (!game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1 &&
        game->col1_ok && indian_nation >= 4 && indian_nation <= 11) {
      ColonizeUnit* u = units_get(&game->units, unit_id);
      if (u && u->nation_id >= 0 && u->nation_id <= 3) {
        /* switchD_2000:da9f::caseD_10(attacker, tribe, 4) = 15b3_0066
         * or-BOTH (decomp 75611) — asked once, latched on both rows
         * (single-side euro_diplo write before 2026-09-08). */
        ai_diplo_or_both(
          &game->col1, u->nation_id, indian_nation, COL1_INDIAN_ATTACK_CONFIRMED_BIT
        );
        game->units.selected_id = unit_id;
        ai_popup_consume_result(&game->ai_popups);
        (void)game_try_unit_move(game, dest_x, dest_y);
  return true;
      }
    } else {
      /* Smell #105: a goto-delegated confirm must not re-fire next frame —
       * cancelling the attack cancels the Go To aimed at it. */
      ColonizeUnit* u = units_get(&game->units, unit_id);
      if (u && u->active && units_orders_follow_goto(u->orders)) {
        units_clear_orders(&game->units, unit_id);
      }
      set_status(game, "Attack called off", NULL);
    }
    ai_popup_consume_result(&game->ai_popups);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_CONTACT_EURO_WAR) {
    const int unit_id = game->ai_popups.result_nation_a;
    const int target_nation = game->ai_popups.result_nation_b;
    const int dest_x = game->ai_popups.result_payload & 0xff;
    const int dest_y = (game->ai_popups.result_payload >> 8) & 0xff;
    if (!game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1 &&
        game->col1_ok && target_nation >= 0 && target_nation <= 3) {
      ColonizeUnit* u = units_get(&game->units, unit_id);
      if (u && u->nation_id >= 0 && u->nation_id <= 3) {
        /* Break Treaty: clear 0x40 both ways and open the war (FUN_281f_0a10). */
        ColonizeTurnContext ctx;
        game_fill_turn_context(game, &ctx);
        ai_diplo_declare_war_ctx(&ctx, u->nation_id, target_nation);
        game->units.selected_id = unit_id;
        ai_popup_consume_result(&game->ai_popups);
        /* bugs.md #437: the colony confirm (if any) was already answered
         * before this treaty prompt — don't ask twice on the retry. */
        game->colony_attack_ok_unit = unit_id;
        game->colony_attack_ok_payload = dest_x | (dest_y << 8);
        (void)game_try_unit_move(game, dest_x, dest_y);
        game->colony_attack_ok_unit = -1;
  return true;
      }
    } else {
      /* Smell #105: see the WHACK cancel above. */
      ColonizeUnit* u = units_get(&game->units, unit_id);
      if (u && u->active && units_orders_follow_goto(u->orders)) {
        units_clear_orders(&game->units, unit_id);
      }
      set_status(game, "Attack called off", NULL);
    }
    ai_popup_consume_result(&game->ai_popups);
  return true;
  }
  return false;
}

/* Europe kiss-up, the colony attack confirm and the @SCOUTCOLONY menu. */
static bool game_apply_popup_diplo_and_scout(ColonizeGameState* game) {
  if (game->ai_popups.result_tag == AI_POPUP_TAG_EUROPE_KISSUP) {
    const int cargo_type = game->ai_popups.result_nation_b;
    const int cost = game->ai_popups.result_payload;
    const bool pay = !game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1;
    ai_popup_consume_result(&game->ai_popups);
    if (!pay || !game->europe_ok || !game->col1_ok) {
  return true;
    }
    EuropeScreen* eu = &game->europe;
    if (eu->gold < cost) {
      /* 38fd:2e6e — the purse test only runs after "Pay": @KISSSORRY
       * "Unfortunately, we only have {%NUMBER0$} available." and no state
       * change. */
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.number0 = eu->gold;
      tok.has_number0 = true;
      char body[AI_POPUP_BODY_LEN];
      char fallback[AI_POPUP_BODY_LEN];
      fallback[0] = '\0';
      popup_msg_fill(&game->messages, "KISSSORRY", &tok, fallback, body, sizeof(body));
      ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_INFO);
  return true;
    }
    (void)europe_buyback_boycott(eu, &game->col1, game->human_nation, cargo_type);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_COLONY_ATTACK) {
    const int unit_id = game->ai_popups.result_nation_a;
    const int dest_x = game->ai_popups.result_payload & 0xff;
    const int dest_y = (game->ai_popups.result_payload >> 8) & 0xff;
    const bool go = !game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1;
    const int payload = game->ai_popups.result_payload;
    ai_popup_consume_result(&game->ai_popups);
    ColonizeUnit* u = units_get(&game->units, unit_id);
    if (go && u && u->active) {
      game->units.selected_id = unit_id;
      game->colony_attack_ok_unit = unit_id;
      game->colony_attack_ok_payload = payload;
      (void)game_try_unit_move(game, dest_x, dest_y);
      game->colony_attack_ok_unit = -1;
  return true;
    }
    if (u && u->active && units_orders_follow_goto(u->orders)) {
      units_clear_orders(&game->units, unit_id);
    }
    set_status(game, "Attack called off", NULL);
  return true;
  }
  /* FUN_5f7a_020e hold pick (@TRADEWHICH): id 99 / cancel = never mind. */
  if (game->ai_popups.result_tag == AI_POPUP_TAG_FOREIGN_TRADE_WHICH) {
    const int unit_id = game->ai_popups.result_nation_a;
    const int cid = game->ai_popups.result_nation_b;
    const int choice = game->ai_popups.result_cancelled ? 99 : game->ai_popups.result_choice_id;
    ai_popup_consume_result(&game->ai_popups);
    if (choice > 0 && choice != 99) {
      game_foreign_trade_price_hold(game, unit_id, cid, choice - 1); /* raw 98961 */
    }
    return true;
  }
  /*
   * FUN_5f7a_020e offer (@TRADEWITH): 1 = take the goods, 2 = take the gold,
   * 3 = refuse. DOS-LITERAL raw 99024: `if (r < 3) { if (r == 1) goods; else
   * gold; }` -- FUN_2a1f_0688's no-choice return (Esc) is < 3 and != 1, so an
   * Esc takes the gold, same as DOS.
   */
  if (game->ai_popups.result_tag == AI_POPUP_TAG_FOREIGN_TRADE_OFFER) {
    const int unit_id = game->ai_popups.result_nation_a;
    const int cid = game->ai_popups.result_nation_b;
    const int hold = game->ai_popups.result_payload;
    const int choice = game->ai_popups.result_cancelled ? 0 : game->ai_popups.result_choice_id;
    ai_popup_consume_result(&game->ai_popups);
    if (choice < 3 && game->foreign_trade_unit == unit_id &&
        game->foreign_trade_colony == cid && game->foreign_trade_hold == hold) {
      ColonizeWorld w = world_make(
        &game->units, &game->colonies, &game->world_map, &game->col1, game->col1_ok, NULL, NULL
      );
      (void)colonies_foreign_trade_apply(
        &w, cid, unit_id, hold, &game->foreign_trade_deal, choice == 1
      );
    }
    game->foreign_trade_unit = -1;
    set_status(game, "Trade concluded", NULL);
    return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_SCOUT_COLONY) {
    const int unit_id = game->ai_popups.result_nation_a;
    const int cid = game->ai_popups.result_nation_b;
    const int payload = game->ai_popups.result_payload;
    const int dest_x = payload & 0xff;
    const int dest_y = (payload >> 8) & 0xff;
    const int choice = game->ai_popups.result_cancelled ? 4 : game->ai_popups.result_choice_id;
    ai_popup_consume_result(&game->ai_popups);
    ColonizeUnit* u = units_get(&game->units, unit_id);
    ColonizeColony* col = colonies_get_mut(&game->colonies, cid);
    if (!u || !u->active || !col || !col->active) {
  return true;
    }
    /*
     * bugs.md #453/460: every outcome that ends the move — Meet (000e returns
     * true), Nothing (local_8 == 4, returns true) and a successful Infiltrate —
     * makes FUN_5f7a_0662 call FUN_281f_0934 (spent = full allotment), and
     * 465b stops the step. Without the spend and with the Go To still armed,
     * the pacer (or the next keypress) re-fired @SCOUTCOLONY at once.
     */
    if (choice != 3) {
      u->moves = 0;
      if (units_orders_follow_goto(u->orders)) {
        units_clear_orders(&game->units, unit_id);
      }
    }
    if (choice == 1) {
      /* Meet With Mayor — FUN_5f7a_000e local_8 == 1: WoI refuses with
       * @NOMAYORSDURINGREV, else the 5bfb_153e encounter dialog runs. */
      if (game->col1.head.game_options.woi) {
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages, "NOMAYORSDURINGREV", NULL,
          "",
          body, sizeof(body)
        );
        ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_INFO);
      } else {
        ColonizeTurnContext ctx;
        game_fill_turn_context(game, &ctx);
        (void)ai_diplo_153e_encounter_forced(&ctx, u->nation_id, col->nation_id, unit_id);
      }
      game_after_unit_action(game);
  return true;
    }
    if (choice == 2) {
      /* Infiltrate — DOS roll: threshold = (fortification-chain count + 6)*2,
       * halved for a Seasoned Scout (profession 0x16); RNG(1,0x24) must beat
       * it. Success opens a view of the colony; failure loses the scouts
       * (@LOSTOURSCOUTS) and the colony stables gain their 100 horses. */
      int fort_chain = 0;
      /* @BUILDING rows 0-2 via the NAMES.TXT accessor, not a private table
       * (audit theme D / GL row "fort names"). */
      for (int f = 0; f < 3; ++f) {
        const int bi = colonies_find_building(&game->colonies, reports_fort_tier_name(f));
        if (bi >= 0 && bi < COLONIZE_BUILDING_TYPES_MAX && col->has_building[bi]) {
          fort_chain++;
        }
      }
      int threshold = (fort_chain + 6) * 2;
      if (u->profession == UNITS_JOB_SCOUT) { /* @JOB 0x16 Seasoned Scout */
        threshold >>= 1;
      }
      const int roll = dos_rng_range(&game->move_rng, 1, 0x24);
      if (threshold < roll) {
        game_enter_colony(game, cid);
      } else {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        /* NAMES.TXT @NATIONALITY, not a hardcoded table (audit GL-31): a
         * renamed nation printed the wrong adjective in @LOSTOURSCOUTS. */
        tok.string0 = (col->nation_id >= 0 && col->nation_id < 4)
                        ? reports_nation_adjective_display_name(col->nation_id)
                        : "enemy";
        tok.string1 = col->name;
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          &game->messages, "LOSTOURSCOUTS", &tok,
          "",
          body, sizeof(body)
        );
        ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, body);
        (void)ai_popup_present_now(&game->ai_popups, AI_POPUP_TAG_INFO);
        col->stock[COLONIZE_CARGO_HORSES] += 100; /* +0xaa += 100 */
        units_despawn(&game->units, unit_id);
        game_after_unit_action(game);
      }
  return true;
    }
    if (choice == 3) {
      /* Attack Colony: rejoin the ordinary attack path (443 confirm already
       * answered here — the latch skips both menus on the retry). */
      game->units.selected_id = unit_id;
      game->colony_attack_ok_unit = unit_id;
      game->colony_attack_ok_payload = payload;
      (void)game_try_unit_move(game, dest_x, dest_y);
      game->colony_attack_ok_unit = -1;
  return true;
    }
    set_status(game, "The scouts hold their ground", NULL);
  return true;
  }
  return false;
}

/* Combat half/ransom prompts, the Brewster pick, Fountain of Youth and the
 * King's galleon offer. */
static bool game_apply_popup_combat_and_gifts(ColonizeGameState* game) {
  if (game->ai_popups.result_tag == AI_POPUP_TAG_COMBAT_HALF) {
    const int unit_id = game->ai_popups.result_nation_a;
    const int dest_x = game->ai_popups.result_payload & 0xff;
    const int dest_y = (game->ai_popups.result_payload >> 8) & 0xff;
    ColonizeUnit* u = units_get(&game->units, unit_id);
    if (!game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1 && u) {
      game->units.selected_id = unit_id;
      game->tired_ok_unit = unit_id;
      game->tired_ok_payload = game->ai_popups.result_payload;
      ai_popup_consume_result(&game->ai_popups);
      (void)game_try_unit_move(game, dest_x, dest_y);
      game->tired_ok_unit = -1;
  game->colony_attack_ok_unit = -1;
  return true;
    }
    /*
     * "Then let them rest." DOS has already added the attack's 3 thirds by
     * the time it asks (1b0e ~100342), and the unit had fewer than 3 left, so
     * declining ends its turn either way.
     */
    if (u) {
      u->moves = 0;
    }
    set_status(game, "The men rest.", NULL);
    ai_popup_consume_result(&game->ai_popups);
    game_after_unit_action(game);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_COMBAT_RANSOM) {
    if (game->col1_ok) {
      (void)units_combat_apply_ransom_popup(&game->col1, &game->ai_popups);
    }
    ai_popup_consume_result(&game->ai_popups);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_BREWSTER_PICK) {
    (void)units_brewster_apply_popup_ex_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(game->units_ok ? &game->units : NULL), .rng=(ColonizeDosRng*)(&game->move_rng), .europe=(EuropeScreen*)(game->europe_ok ? &game->europe : NULL)}, &game->ai_popups);
    ai_popup_consume_result(&game->ai_popups);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_FOUNTAIN_YOUTH) {
    (void)units_fountain_youth_apply_popup_ex(
      game->europe_ok ? &game->europe : NULL, &game->ai_popups, &game->messages,
      &game->move_rng
    );
    ai_popup_consume_result(&game->ai_popups);
  return true;
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_KING_GALLEON) {
    if (game->col1_ok && game->units_ok) {
      (void)units_king_galleon_apply_popup_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .col1=(ColonizeCol1Save*)(&game->col1), .col1_ok=true, .europe=(EuropeScreen*)(game->europe_ok ? &game->europe : NULL)}, &game->ai_popups, &game->messages);
    }
    ai_popup_consume_result(&game->ai_popups);
  return true;
  }
  return false;
}

/* Village menu "Attack Village" (@ACTIONS row 9). */
static bool game_apply_popup_village_attack(ColonizeGameState* game) {
  /*
   * Village menu "Attack Village" (@ACTIONS row 9): commit the deferred move
   * onto the adjacent village tile — same path as the old Attack/Leave warn.
   */
  if (game->ai_popups.result_tag == AI_POPUP_TAG_CONTACT_MEET &&
      !game->ai_popups.result_cancelled &&
      game->ai_popups.result_choice_id == AI_CONTACT_CHOICE_ATTACK) {
    const int unit_id = ai_contact_meet_payload_unit(game->ai_popups.result_payload);
    const int indian_nation = game->ai_popups.result_nation_b;
    ColonizeUnit* u = unit_id >= 0 ? units_get(&game->units, unit_id) : NULL;
    int dest_x = -1;
    int dest_y = -1;
    if (u && u->active && game->col1_ok && game->col1.tribe) {
      for (uint16_t ti = 0; ti < game->col1.head.tribe_count; ++ti) {
        const ColonizeCol1Tribe* t = &game->col1.tribe[ti];
        if ((int)t->nation_id == indian_nation &&
            map_tiles_adjacent((int)t->x, (int)t->y, u->x, u->y, true)) {
          dest_x = t->x;
          dest_y = t->y;
          break;
        }
      }
    }
    /*
     * bugs (village-attack freeze): consume the ACTIONS result BEFORE the
     * move — units_try_move's combat runs game_combat_popup_pump, and
     * ai_popup_busy counts a pending has_result, so the pump spun forever
     * (not open → no input handling, has_result → try_present_next refused)
     * until a window-close SDL_QUIT broke it out.
     */
    ai_popup_consume_result(&game->ai_popups);
    if (dest_x >= 0) {
      ColonizeTurnContext ctx;
      game_fill_turn_context(game, &ctx);
      ai_contact_village_open_hostilities(&ctx, indian_nation, u->nation_id);
      units_set_ff_col1(game->col1_ok ? &game->col1 : NULL);
      colonies_set_col1_context(game->col1_ok ? &game->col1 : NULL);
      units_set_combat_human_nation(game->human_nation);
      units_set_combat_popups(&game->ai_popups, &game->messages);
      units_set_combat_europe(&game->europe);
      units_set_occupancy_map(&game->world_map);
      colonies_set_occupancy_map(&game->world_map);
      units_set_combat_colonies(&game->colonies);
      units_set_native_fallout_context(game->col1_ok ? &game->col1 : NULL, &game->world_map, -1);
      game->units.selected_id = unit_id;
      if (units_try_move_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&game->units), .colonies=(ColonizeColonyPool*)(&game->colonies), .map=(ColonizeWorldMap*)(&game->world_map), .rng=(ColonizeDosRng*)(&game->move_rng)}, unit_id, dest_x, dest_y)) {
        snprintf(game->status, sizeof(game->status), "Village attacked (%d,%d)", dest_x, dest_y);
        game_after_unit_action(game);
      } else if (units_last_combat_outcome() < 0) {
        set_status(game, "Combat lost", NULL);
        game_after_unit_action(game);
      } else {
        set_status(game, "Attack failed", NULL);
      }
    }
    /* Result already consumed above (before the move). */
  return true;
  }
  return false;
}

void game_apply_ai_popup_result(ColonizeGameState* game) {
  if (!game || !game->ai_popups.has_result) {
    return;
  }
  if (game_apply_popup_map_and_colony(game)) {
    return;
  }
  if (game_apply_popup_voyage(game)) {
    return;
  }
  if (game_apply_popup_contact(game)) {
    return;
  }
  if (game_apply_popup_diplo_and_scout(game)) {
    return;
  }
  if (game_apply_popup_combat_and_gifts(game)) {
    return;
  }
  if (game_apply_popup_village_attack(game)) {
    return;
  }
  ColonizeTurnContext ctx;
  game_fill_turn_context(game, &ctx);
  ai_king_apply_popup_result(&ctx, &game->ai_popups);
  if (game->ai_popups.result_tag == AI_POPUP_TAG_KING_SCORED &&
      !game->ai_popups.result_cancelled &&
      game->ai_popups.result_choice_id == 0) {
    /* @SCORED "That's all." → open retire score (same path as Retire menu). */
    game_open_retire_score(game);
  }
  /* bugs.md: the war-end announcements. Payload 2 (@RETIRING2 and legacy
   * loss popups with no audience behind them) retires straight away; payload
   * 1 (@WINNING) and 4 (@LOSINGn) have the KING_THRONE audience queued next,
   * and the retire score waits for its dismissal (DOS 3844_0442 order:
   * announcement → throne audience → CLOSING.EXE → score chain). */
  if (game->ai_popups.result_tag == AI_POPUP_TAG_KING_WAR_END &&
      game->ai_popups.result_payload == 2) {
    game->war_end_retired = true;
    game_open_retire_score(game);
  }
  if (game->ai_popups.result_tag == AI_POPUP_TAG_KING_THRONE) {
    game->war_end_retired = true;
    game->war_end_won = (game->ai_popups.result_payload == 1);
    if (game->war_end_won) {
      game_begin_win_closing_or_score(game);
    } else {
      game_open_retire_score(game);
    }
  }
  /* Post-HoF @SCORED (WoI win): "That's all." ends at the title menu; "Keep
   * playing anyway." resumes the map (DOS main-loop 0x104 block: FUN_281f_03fe
   * choice 2 keeps DS:0x53c2 alive). Either way scoring is complete. */
  if (game->ai_popups.result_tag == AI_POPUP_TAG_WAR_SCORED) {
    if (game->col1_ok) {
      game->col1.head.game_options.calendar_latch = 1;
    }
    if (!game->ai_popups.result_cancelled && game->ai_popups.result_choice_id == 1) {
      if (game->col1_ok) {
        game->col1.head.turn_loop_running = 0;
      }
      game->in_menu = true;
      sound_stop_bgm();
      set_status(game, "Colonization Linux Port", NULL);
    } else {
      /* Keep playing (or Esc): back on the map, campaign continues. */
      if (game->col1_ok) {
        game->col1.head.turn_loop_running = 1;
      }
      sound_set_bgm(1);
      set_status(game, "Independence won — playing on", NULL);
    }
  }
  ai_contact_apply_popup_result(&ctx, &game->ai_popups);
  ai_diplo_apply_popup_result(&ctx, &game->ai_popups);
  founding_fathers_apply_popup_result(&ctx, &game->ai_popups);
  /*
   * bugs.md: a Founding Father joining Congress is a three-beat sequence —
   * the arrival announcement, then the Continental Congress report's second
   * page (the hall photo), then his Colonizopedia entry. The announce OK
   * carries the elected index in result_nation_b (founding_fathers.c);
   * the pedia hand-off happens when the report closes.
   */
  if (game->ai_popups.result_tag == AI_POPUP_TAG_FF_CONGRESS &&
      game->ai_popups.result_payload == -1 && !game->ai_popups.result_cancelled) {
    const int ff_index = game->ai_popups.result_nation_b;
    if (ff_index >= 0 && ff_index < PEDIA_FATHER_COUNT) {
      game_open_report(game, COLONIZE_REPORT_CONGRESS);
      game->congress_page2 = true;
      game->ff_pedia_after_report = ff_index;
    }
  }
  ai_popup_consume_result(&game->ai_popups);
}
