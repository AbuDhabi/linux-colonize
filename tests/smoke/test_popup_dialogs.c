#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/cheat_list_dialog.h"
#include "core/font_file.h"
#include "core/game_loop_internal.h"
#include "core/howmuch_dialog.h"
#include "core/name_entry_dialog.h"
#include "core/options_dialog.h"
#include "core/popup_msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/test_runner.h"

static int fail(const char* msg) {
  fprintf(stderr, "smoke_popup_dialogs: %s\n", msg);
  return 1;
}

static int case_token_apply(void) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = "Jamestown";
  tok.string1 = "Sugar";
  tok.number0 = 42;
  tok.has_number0 = true;

  char out[128];
  popup_msg_apply_tokens(out, sizeof(out), "At %STRING0, %NUMBER0 of {%STRING1}.", &tok);
  if (strstr(out, "Jamestown") == NULL || strstr(out, "42") == NULL ||
      strstr(out, "Sugar") == NULL) {
    return fail("token apply");
  }
  return 0;
}

static int case_howmuch(void) {
  HowmuchDialog hm;
  if (!howmuch_open(&hm, HOWMUCH_KIND_BUY, "How much?", NULL, 100, 50, 3, 0)) {
    return fail("howmuch_open");
  }
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  howmuch_handle_input(&hm, &in);
  if (!hm.has_result || hm.result_cancelled || hm.result_amount != 50) {
    return fail("howmuch confirm");
  }
  return 0;
}

static int case_options(void) {
  OptionsDialog od;
  ColonizeCol1GameOptions opts;
  memset(&opts, 0, sizeof(opts));
  opts.show_indian_moves = 1;
  opts.show_foreign_moves = 1;
  opts.autosave = 1;
  opts.combat_analysis = 1;
  opts.tutorial_hints = 1;
  if (!options_dialog_open_game(&od, NULL, &opts)) {
    return fail("options_open_game");
  }
  if (od.option_count < 8) {
    return fail("options count");
  }
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_SPACE;
  options_dialog_handle_input(&od, &in);
  in.last_key = COLONIZE_KEY_ENTER;
  options_dialog_handle_input(&od, &in);
  if (!od.has_result || od.result_cancelled) {
    return fail("options confirm");
  }
  ColonizeCol1GameOptions applied;
  memset(&applied, 0, sizeof(applied));
  options_dialog_apply_game(&od, &applied);
  if (applied.show_indian_moves || !applied.show_foreign_moves ||
      applied.fast_piece_slide || applied.end_of_turn || !applied.autosave ||
      !applied.combat_analysis || applied.water_color_cycling_off ||
      !applied.tutorial_hints) {
    return fail("options apply");
  }
  if (!options_dialog_open_game(&od, NULL, &applied) ||
      od.values[0] != 0 || od.values[1] != 1 || od.values[2] != 0 ||
      od.values[3] != 0 || od.values[4] != 1 || od.values[5] != 1 ||
      od.values[6] != 1 || od.values[7] != 1) {
    return fail("options reopen round-trip");
  }
  return 0;
}

static int case_name_entry(void) {
  NameEntryDialog ne;
  if (!name_entry_open(&ne, NAME_ENTRY_KIND_FOUND, "What shall we name this colony?", "Jamestown", 1)) {
    return fail("name_entry found open");
  }
  if (strstr(ne.prompt, "Land Ho") != NULL) {
    return fail("found colony must not use Land Ho prompt");
  }
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  name_entry_handle_input(&ne, &in);
  if (!ne.has_result || ne.result_cancelled || strcmp(ne.result_name, "Jamestown") != 0) {
    return fail("name_entry found confirm");
  }
  if (!name_entry_open(&ne, NAME_ENTRY_KIND_LANDHO, "Land Ho!", "New England", -1)) {
    return fail("name_entry landho open");
  }
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ESCAPE;
  name_entry_handle_input(&ne, &in);
  if (!ne.has_result || !ne.result_cancelled || strcmp(ne.result_name, "New England") != 0) {
    return fail("landho cancel should keep @COLONYNAME seed");
  }
  return 0;
}

/* Authenticity: wired sections must fill from GAME.TXT when present. */
static int case_game_txt_wired_sections(void) {
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (!assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
    return 0;
  }
  static const char* wired[] = {
    "LANDFALL",
    "KEEPSTOCKADE",
    "ABANDON",
    "DONTKNOWSHIPS",
    "MADATSHIPS",
    "INDIANCOMMENT",
    "WHICHFREEDOM",
    "FREEDOM",
    "KINGTAX",
    "MERCENARIES",
    "MERCS",
  };
  PopupMsgTokens fill_tok;
  memset(&fill_tok, 0, sizeof(fill_tok));
  fill_tok.string0 = "Sioux";
  fill_tok.string1 = "Jamestown";
  fill_tok.number0 = 5;
  fill_tok.has_number0 = true;
  for (size_t i = 0; i < sizeof(wired) / sizeof(wired[0]); i++) {
    char body[512];
    popup_msg_fill(&game_txt, wired[i], &fill_tok, "FALLBACK", body, sizeof(body));
    if (strcmp(body, "FALLBACK") == 0 || body[0] == '\0') {
      fprintf(stderr, "smoke_popup_dialogs: %s fell back\n", wired[i]);
      assets_msg_free(&game_txt);
      return fail("popup_msg_fill must use GAME.TXT section");
    }
  }
  char landfall_choices[4][POPUP_MSG_CHOICE_LEN];
  const ColonizeMsgSection* landfall = assets_msg_find(&game_txt, "LANDFALL");
  int n = popup_msg_choices(landfall, landfall_choices, 4);
  if (n < 2) {
    assets_msg_free(&game_txt);
    return fail("LANDFALL must expose Stay/Landfall choices");
  }
  char tax_choices[4][POPUP_MSG_CHOICE_LEN];
  const ColonizeMsgSection* taxopt = assets_msg_find(&game_txt, "TAXOPTIONS");
  n = popup_msg_rows(taxopt, tax_choices, 4);
  if (n < 2) {
    assets_msg_free(&game_txt);
    return fail("TAXOPTIONS must expose Kiss/Party choices");
  }
  /* @DECLARE Never… is 53 chars — must fit POPUP_MSG_CHOICE_LEN without cut. */
  char declare_choices[4][POPUP_MSG_CHOICE_LEN];
  const ColonizeMsgSection* decl = assets_msg_find(&game_txt, "DECLARE");
  n = popup_msg_choices(decl, declare_choices, 4);
  if (n < 2) {
    assets_msg_free(&game_txt);
    return fail("DECLARE must expose Never/Yes choices");
  }
  if (!strstr(declare_choices[0], "God save the King")) {
    fprintf(stderr, "smoke_popup_dialogs: DECLARE Never truncated: '%s'\n",
            declare_choices[0]);
    assets_msg_free(&game_txt);
    return fail("DECLARE Never choice must keep 'God save the King'");
  }
  assets_msg_free(&game_txt);
  return 0;
}

/* P11.3: GAME.TXT @width rides popup_msg_fill's side-channel into the next
 * ai_popup enqueue (RECRUITCHOOSE is one of the 99 @width=220 sections;
 * a fallback-only fill leaves the default). */
static int case_recruitchoose_width(void) {
  ColonizeMsgCatalog wtxt;
  assets_msg_init(&wtxt);
  if (!assets_msg_load_file(&wtxt, "COLONIZE/GAME.TXT")) {
    return 0;
  }
  const ColonizeMsgSection* rc = assets_msg_find(&wtxt, "RECRUITCHOOSE");
  if (!rc || popup_msg_section_width(rc) != 220) {
    return fail("RECRUITCHOOSE @width should parse as 220");
  }
  char body[256];
  PopupMsgTokens wtok;
  memset(&wtok, 0, sizeof(wtok));
  wtok.string0 = "Amsterdam";
  popup_msg_fill(&wtxt, "RECRUITCHOOSE", &wtok, "fb", body, sizeof(body));
  AiPopupState wst;
  ai_popup_init(&wst);
  if (!ai_popup_enqueue_ok(&wst, AI_POPUP_TAG_INFO, NULL, body) || wst.queue[0].width != 220) {
    return fail("enqueue after fill should carry @width=220");
  }
  if (!ai_popup_enqueue_ok(&wst, AI_POPUP_TAG_INFO, NULL, "plain") || wst.queue[1].width != 0) {
    return fail("second enqueue without a fill should take the default width");
  }
  popup_msg_fill(&wtxt, "NOSUCHSECTION_XYZ", &wtok, "fb", body, sizeof(body));
  if (popup_msg_take_pending_width() != 0) {
    return fail("fallback fill must clear the pending width");
  }
  assets_msg_free(&wtxt);
  return 0;
}

static int case_game_confirmation_catalog_choices(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("allocate game state");
  }
  assets_msg_init(&game->messages);
  ai_popup_init(&game->ai_popups);
  if (!assets_msg_load_file(&game->messages, "COLONIZE/GAME.TXT")) {
    free(game);
    return fail("load GAME.TXT for confirmations");
  }

  const ColonizeMsgSection* sail = assets_msg_find(&game->messages, "SAILAWAY");
  char expected[2][POPUP_MSG_CHOICE_LEN];
  if (popup_msg_choices(sail, expected, 2) != 2) {
    assets_msg_free(&game->messages);
    free(game);
    return fail("SAILAWAY choices missing from catalog");
  }
  game_enqueue_yes_no(
    game, GAME_MAP_CONFIRM_EUROPE_SAIL, 0, "SAILAWAY", "", NULL
  );
  if (game->ai_popups.queue_count != 1 ||
      strcmp(game->ai_popups.queue[0].choices[0], expected[0]) != 0 ||
      strcmp(game->ai_popups.queue[0].choices[1], expected[1]) != 0) {
    assets_msg_free(&game->messages);
    free(game);
    return fail("confirmation must preserve section-specific choice rows");
  }

  /* port_bugs.md #984: a section without choice rows gets EMPTY rows — never
   * port-authored English. @OVERBOARD is that case (its own picker builds the
   * rows from the ship's holds instead). */
  ai_popup_clear(&game->ai_popups);
  game_enqueue_yes_no(
    game, GAME_MAP_CONFIRM_EUROPE_SAIL, 0, "OVERBOARD", "", NULL
  );
  if (game->ai_popups.queue_count != 1 ||
      game->ai_popups.queue[0].choices[0][0] != '\0' ||
      game->ai_popups.queue[0].choices[1][0] != '\0') {
    assets_msg_free(&game->messages);
    free(game);
    return fail("choice-less section must leave the rows empty");
  }

  assets_msg_free(&game->messages);
  free(game);
  return 0;
}

/*
 * port_bugs.md #1187: the Europe Recruit / Purchase / Train dialogs are the generic 6f74 list
 * dialog and must land on the rects measured off the DOS reference shots
 * (original_screenshots/europe/, black rule to black rule): recruit 196x103 at (62,88),
 * purchase 196x119 at (62,80), train 196x180 at (62,10); first-row selection bar tops at
 * frame_y + 9 + prose height (137 / 109 / 37). Real GAME.TXT prose and the real fonts: the
 * line counts (4 / 2 / 3) come from wrapping the real text at content - 4.
 */
static int europe_layout(
  ColonizeGameState* game, int menu, int shot_fy, int shot_h, int bar_y, int glyph_h, int row_h,
  const char* what
) {
  game->europe.menu = menu;
  EuropeMenuLayout lay;
  if (!europe_menu_layout(game, 320, 200, &lay)) {
    fprintf(stderr, "%s: no layout\n", what);
    return 1;
  }
  int rc = 0;
  if (lay.dialog_x != 62 || lay.dialog_w != 196 || lay.dialog_y != shot_fy ||
      lay.dialog_h != shot_h || lay.list_y0 != bar_y || lay.glyph_h != glyph_h ||
      lay.row_h != row_h || !lay.help_footer) {
    fprintf(stderr,
            "%s: want frame (62,%d) 196x%d bar %d glyph %d pitch %d footer, got (%d,%d) %dx%d "
            "bar %d glyph %d pitch %d footer %d\n",
            what, shot_fy, shot_h, bar_y, glyph_h, row_h, lay.dialog_x, lay.dialog_y,
            lay.dialog_w, lay.dialog_h, lay.list_y0, lay.glyph_h, lay.row_h, lay.help_footer);
    rc = 1;
  }
  /* The click hit-test follows the same rows: bar top row 0, the 1 px gap, past the last row. */
  if (europe_menu_row_at(game, 120, lay.list_y0 + 3) != 0 ||
      europe_menu_row_at(game, 120, lay.list_y0 - 1) != -1 ||
      europe_menu_row_at(game, 120, lay.list_y0 + row_h * lay.rows - 1) != lay.rows - 1 ||
      europe_menu_row_at(game, 120, lay.list_y0 + row_h * lay.rows) != -1 ||
      europe_menu_row_at(game, lay.dialog_x + 3, lay.list_y0 + 3) != -1) {
    fprintf(stderr, "%s: row hit-test off the bar rows\n", what);
    rc = 1;
  }
  return rc;
}

static int case_europe_menu_frames_are_dos(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("allocate game state");
  }
  char err[128];
  assets_msg_init(&game->messages);
  if (!assets_msg_load_file(&game->messages, "COLONIZE/GAME.TXT") ||
      !font_file_load("COLONIZE/FONTINTR.FF", &game->intro_font, err, sizeof(err)) ||
      !font_file_load("COLONIZE/FONTTINY.FF", &game->colony_font, err, sizeof(err))) {
    assets_msg_free(&game->messages);
    free(game);
    fprintf(stderr, "SKIP: assets: %s\n", err);
    return TR_SKIP;
  }
  game->intro_font_ok = true;
  game->colony_font_ok = true;
  ai_popup_set_portrait_source("COLONIZE");
  game->europe.recruit_passage = 116; /* recruit.png: "(116 gold)" */
  game->europe.train_count = 17;
  game->europe.purchase_count = 6;
  game->europe.dock_menu_count = 6;

  int rc = 0;
  rc |= europe_layout(game, EUROPE_MENU_RECRUIT, 88, 103, 137, 9, 12, "recruit.png");
  rc |= europe_layout(game, EUROPE_MENU_PURCHASE, 80, 119, 109, 9, 12, "purchase.png");
  rc |= europe_layout(game, EUROPE_MENU_TRAIN, 10, 180, 37, 5, 8, "train.png");

  /* @REALLYBUY confirm: a plain popup (no DS:0x1f66 footer, default +0x22 = 4), 12 + 10 + 2 * 12. */
  game->europe.menu = EUROPE_MENU_PURCHASE;
  game->europe.purchase_confirming = true;
  EuropeMenuLayout lay;
  if (!europe_menu_layout(game, 320, 200, &lay) || lay.dialog_h != 46 || lay.help_footer ||
      lay.row_dx != 4 || lay.rows != 2) {
    fprintf(stderr, "confirm: h %d footer %d dx %d rows %d\n", lay.dialog_h, lay.help_footer,
            lay.row_dx, lay.rows);
    rc = 1;
  }
  game->europe.purchase_confirming = false;
  /* Dock options: 1 prose line + 6 rows + footer = 97, centred plain (no figure): y = 100 - 48. */
  game->europe.menu = EUROPE_MENU_DOCK;
  if (!europe_menu_layout(game, 320, 200, &lay) || lay.dialog_h != 97 || lay.dialog_y != 52 ||
      lay.row_dx != 8 || lay.graphic) {
    fprintf(stderr, "dock: h %d y %d dx %d\n", lay.dialog_h, lay.dialog_y, lay.row_dx);
    rc = 1;
  }
  font_file_free(&game->intro_font);
  font_file_free(&game->colony_font);
  assets_msg_free(&game->messages);
  free(game);
  return rc;
}

static int case_debug_test_ignores_directives(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("allocate debug game state");
  }
  assets_msg_init(&game->debug_txt);
  if (!assets_msg_load_file(&game->debug_txt, "COLONIZE/DEBUG.TXT")) {
    free(game);
    return fail("load DEBUG.TXT");
  }
  game->debug_txt_ok = true;
  game->units_ok = true;
  game->colonies_ok = true;
  game->units.units[0].active = true;
  game->units.units[0].id = 1;
  game->units.units[3].active = true;
  game->units.units[3].id = 2;
  game->units.next_id = 3;
  units_pool_sync(&game->units);
  game->colonies.colonies[0].active = true;

  game_cheat_test_routine(game);

  const ColonizeMsgSection* test = assets_msg_find(&game->debug_txt, "TEST");
  char rows[2][POPUP_MSG_CHOICE_LEN];
  if (popup_msg_rows(test, rows, 2) != 2) {
    assets_msg_free(&game->debug_txt);
    free(game);
    return fail("DEBUG @TEST content rows missing");
  }
  PopupMsgTokens tok0 = {.number0 = 2, .has_number0 = true};
  PopupMsgTokens tok1 = {.number1 = 1, .has_number1 = true};
  const char* text0;
  const char* text1;
  popup_msg_caret_flags(rows[0], &text0);
  popup_msg_caret_flags(rows[1], &text1);
  char row0[64];
  char row1[64];
  char expected[160];
  popup_msg_apply_tokens(row0, sizeof(row0), text0, &tok0);
  popup_msg_apply_tokens(row1, sizeof(row1), text1, &tok1);
  snprintf(expected, sizeof(expected), "%s; %s", row0, row1);
  if (strcmp(game->status, expected) != 0 || strchr(game->status, '@') != NULL) {
    fprintf(stderr, "smoke_popup_dialogs: DEBUG @TEST got '%s' want '%s'\n",
            game->status, expected);
    assets_msg_free(&game->debug_txt);
    free(game);
    return fail("DEBUG @TEST directive shifted its text rows");
  }

  assets_msg_free(&game->debug_txt);
  free(game);
  return 0;
}

/* port_bugs.md #1111: @SETVIEW and @SETHUMAN share one parser — both honour
 * @width= and give overflow rows the table's last id (-2 normal view /
 * -1 no human). */
static int case_cheat_list_shared_parser(void) {
  static char view_lines[][COLONIZE_MSG_LINE_LEN] = {
    "@width=240", "P", "A", "B", "C", "D", "E", "F", "G"
  };
  static char human_lines[][COLONIZE_MSG_LINE_LEN] = {
    "@width=250", "P", "A", "B", "C", "D", "E", "F"
  };
  ColonizeMsgSection sections[2];
  memset(sections, 0, sizeof(sections));
  snprintf(sections[0].name, sizeof(sections[0].name), "SETVIEW");
  sections[0].lines = view_lines;
  sections[0].line_count = 9;
  snprintf(sections[1].name, sizeof(sections[1].name), "SETHUMAN");
  sections[1].lines = human_lines;
  sections[1].line_count = 8;
  const ColonizeMsgCatalog cat = {.sections = sections, .section_count = 2};
  CheatListDialog dlg;
  if (!cheat_list_open_setview(&dlg, &cat) || dlg.width != 240 || dlg.option_count != 7 ||
      dlg.option_ids[4] != -1 || dlg.option_ids[5] != -2 || dlg.option_ids[6] != -2) {
    return fail("SETVIEW width / overflow id");
  }
  if (!cheat_list_open_set_human(&dlg, &cat) || dlg.width != 250 || dlg.option_count != 6 ||
      dlg.option_ids[4] != -1 || dlg.option_ids[5] != -1) {
    return fail("SETHUMAN width / overflow id");
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"token_apply", case_token_apply},
    {"howmuch", case_howmuch},
    {"options", case_options},
    {"name_entry", case_name_entry},
    {"game_txt_wired_sections", case_game_txt_wired_sections},
    {"recruitchoose_width", case_recruitchoose_width},
    {"game_confirmation_catalog_choices", case_game_confirmation_catalog_choices},
    {"europe_menu_frames_are_dos", case_europe_menu_frames_are_dos},
    {"debug_test_ignores_directives", case_debug_test_ignores_directives},
    {"cheat_list_shared_parser", case_cheat_list_shared_parser},
};

TEST_MAIN(k_cases)
