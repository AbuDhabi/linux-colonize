#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/cheat_list_dialog.h"
#include "core/font_file.h"
#include "core/game_loop_internal.h"
#include "core/howmuch_dialog.h"
#include "core/name_entry_dialog.h"
#include "core/options_dialog.h"
#include "core/popup_msg.h"
#include "core/save_load_dialog.h"

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
  /* FUN_6f74_309c collapses "%%" with no token table too (pedia: Drake's "50%%."). */
  popup_msg_apply_tokens(out, sizeof(out), "by 50%%.", NULL);
  if (strcmp(out, "by 50%.") != 0) {
    return fail("%% escape without tokens");
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

/* port_bugs.md #1199: @checkbox rows keep the label's '~'; its FUN_6f74_0642 hotkey (the
 * char after the last '~', upper-folded) moves the selection there and toggles the row,
 * the box staying open (FUN_6f74_2580 6f74:2df6 / 2dcd). The "~[ " circle and the hotkey
 * letter draw in the hilite ink. */
static int case_checkbox_hotkeys(void) {
  static char game_lines[][COLONIZE_MSG_LINE_LEN] = {
    "@width=190", "@checkbox", "Set Game Options", "@options", "Show ~Indian Moves",
    "Fast Piece ~Slide", "Water Color C~ycling"
  };
  static char debug_lines[][COLONIZE_MSG_LINE_LEN] = {
    "@width=220", "@checkbox", "Select Debug Information Options", "~Anger & Friction Levels",
    "~Indian AI movement", "~Supply and Demand (Indians)", "~Foreign AI planning modes",
    "~Close Moves", "~Far Moves", "~All Movement"
  };
  ColonizeMsgSection sections[2];
  memset(sections, 0, sizeof(sections));
  snprintf(sections[0].name, sizeof(sections[0].name), "GAMEOPTIONS");
  sections[0].lines = game_lines;
  sections[0].line_count = 7;
  snprintf(sections[1].name, sizeof(sections[1].name), "OPTIONS");
  sections[1].lines = debug_lines;
  sections[1].line_count = 10;
  const ColonizeMsgCatalog cat = {.sections = sections, .section_count = 2};
  if (popup_hotkey_of("Water Color C~ycling") != 'Y' || popup_hotkey_of("~F~3  Zoom") != 0x13d ||
      popup_hotkey_of("Labels on {buildings}") != 0) {
    return fail("FUN_6f74_0642 hotkey");
  }
  OptionsDialog od;
  if (!options_dialog_open_game(&od, &cat, NULL) || od.option_count != 3) {
    return fail("options open from catalog");
  }
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.text_input[0] = 'y';
  in.text_input_len = 1;
  options_dialog_handle_input(&od, &in);
  if (!od.open || od.selection != 2 || od.values[2] != 1 || od.values[0] || od.values[1]) {
    return fail("options hotkey 'y' must select and toggle Water Color C~ycling, box open");
  }
  in.text_input[0] = 'S';
  options_dialog_handle_input(&od, &in);
  in.text_input[0] = 'q';
  options_dialog_handle_input(&od, &in);
  in.text_input[0] = 'i';
  in.alt_held = true; /* Alt-I is not the key 'I' */
  options_dialog_handle_input(&od, &in);
  if (!od.open || od.selection != 1 || od.values[1] != 1 || od.values[0] != 0) {
    return fail("options hotkeys: 'S' toggles row 1, 'q' and Alt-I nothing");
  }
  ColonizeFont font;
  char err[128];
  if (font_file_load("COLONIZE/FONTINTR.FF", &font, err, sizeof(err))) {
    static uint8_t px[320 * 200];
    ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = px};
    memset(px, 0, sizeof(px));
    options_dialog_render(&od, &font, NULL, NULL, 68, 149, 138, &fb);
    int n_hi = 0;
    for (size_t i = 0; i < sizeof(px); ++i) {
      n_hi += px[i] == 149;
    }
    font_file_free(&font);
    if (n_hi == 0) {
      return fail("options rows: no hilite '~' circle / hotkey letter drawn");
    }
  }

  CheatListDialog cl;
  if (!cheat_list_open_debug_flags(&cl, &cat, 0) || cl.option_count != 7 ||
      strncmp(cl.options[0], "~[ ~Anger", 9) != 0) {
    return fail("debug flags rows must be DS:0x1f91 \"~[ \" + the raw label");
  }
  memset(&in, 0, sizeof(in));
  in.text_input[0] = 's';
  in.text_input_len = 1;
  cheat_list_handle_input(&cl, &in);
  if (!cl.open || cl.selection != 2 || cl.selected_mask != 0x4 ||
      strncmp(cl.options[2], "~] ", 3) != 0) {
    return fail("debug flags hotkey 's' must select and toggle row 2, box open");
  }
  return 0;
}

/* port_bugs.md #1209: FUN_6f74_2580's list arm (raw 116905-116940) wraps Up / Down and steps
 * past disabled rows, giving up after two wraps; options and the cheat lists use it. */
static int list_key(int sel, int count, uint32_t mask, ColonizeKey key) {
  return popup_list_key_step(key, &sel, count, mask) ? sel : -99;
}

static int case_list_keys_wrap(void) {
  if (list_key(0, 4, 0x6, COLONIZE_KEY_DOWN) != 3 || list_key(3, 4, 0x6, COLONIZE_KEY_DOWN) != 0 ||
      list_key(0, 4, 0x6, COLONIZE_KEY_KP8) != 3 || list_key(3, 4, 0x6, COLONIZE_KEY_UP) != 0) {
    return fail("wrap past disabled rows");
  }
  if (list_key(1, 4, 0xf, COLONIZE_KEY_DOWN) != 0 || list_key(-1, 4, 0, COLONIZE_KEY_DOWN) != 0 ||
      list_key(-1, 4, 0, COLONIZE_KEY_UP) != 3 || list_key(0, 4, 0, COLONIZE_KEY_LEFT) != -99 ||
      list_key(0, 0, 0, COLONIZE_KEY_DOWN) != -99) {
    return fail("all disabled: stop after two wraps; no selection; other keys / no rows");
  }
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  OptionsDialog od;
  ColonizeCol1GameOptions opts;
  memset(&opts, 0, sizeof(opts));
  if (!options_dialog_open_game(&od, NULL, &opts) || od.option_count < 2) {
    return fail("options_open_game");
  }
  od.selection = 0;
  in.last_key = COLONIZE_KEY_UP;
  options_dialog_handle_input(&od, &in);
  if (od.selection != od.option_count - 1) {
    return fail("options: Up on the first row wraps to the last");
  }
  in.last_key = COLONIZE_KEY_DOWN;
  options_dialog_handle_input(&od, &in);
  if (od.selection != 0) {
    return fail("options: Down on the last row wraps to the first");
  }
  static char lines[][COLONIZE_MSG_LINE_LEN] = {"@checkbox", "Flags", "~A", "~B", "~C", "~D", "~E",
                                               "~F", "~G"};
  ColonizeMsgSection sec;
  memset(&sec, 0, sizeof(sec));
  snprintf(sec.name, sizeof(sec.name), "OPTIONS");
  sec.lines = lines;
  sec.line_count = 9;
  const ColonizeMsgCatalog cat = {.sections = &sec, .section_count = 1};
  CheatListDialog cl;
  if (!cheat_list_open_debug_flags(&cl, &cat, 0) || cl.option_count != 7) {
    return fail("debug flags open");
  }
  cl.selection = 0;
  cl.disabled_mask = 1u << 6;
  in.last_key = COLONIZE_KEY_UP;
  cheat_list_handle_input(&cl, &in);
  if (cl.selection != 5) {
    return fail("debug flags: Up on the first row wraps past the disabled last row");
  }
  in.last_key = COLONIZE_KEY_DOWN;
  cheat_list_handle_input(&cl, &in);
  if (cl.selection != 0) {
    return fail("debug flags: Down skips the disabled last row and wraps");
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
 * C12-B4: every Yes/No shares the one map_confirm slot. A second confirm raised while the
 * first is open or queued used to overwrite the slot (kind + payload) and leave a deaf popup
 * once the first answer reset it. DOS shows them synchronously, so the second is refused.
 */
static int case_map_confirm_second_refused(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("allocate game state");
  }
  assets_msg_init(&game->messages);
  ai_popup_init(&game->ai_popups);
  int rc = 0;
  game_enqueue_yes_no(game, GAME_MAP_CONFIRM_EUROPE_SAIL, 0, "SAILAWAY", "", NULL);
  game_enqueue_yes_no(game, GAME_MAP_CONFIRM_EUROPE_SAIL, 1, "SAILAWAY", "", NULL);
  if (game->ai_popups.queue_count != 1 || game->map_confirm != GAME_MAP_CONFIRM_EUROPE_SAIL ||
      game->map_confirm_payload != 0) {
    rc = fail("queued Yes/No: second enqueue must be refused");
  }
  if (rc == 0 && !ai_popup_try_present_next(&game->ai_popups)) {
    rc = fail("present the first Yes/No");
  }
  game_enqueue_yes_no(game, GAME_MAP_CONFIRM_DISBAND, 7, "SUREDISBAND", "", NULL);
  if (rc == 0 && (game->ai_popups.queue_count != 0 || game->map_confirm_payload != 0)) {
    rc = fail("open Yes/No: second enqueue must be refused");
  }
  ai_popup_cancel_current(&game->ai_popups); /* No */
  game_apply_ai_popup_result(game);
  if (rc == 0 && (ai_popup_busy(&game->ai_popups) || game->ai_popups.has_result ||
                  game->map_confirm != GAME_MAP_CONFIRM_NONE)) {
    rc = fail("answered Yes/No must leave no orphan popup");
  }
  game_enqueue_yes_no(game, GAME_MAP_CONFIRM_EUROPE_SAIL, 1, "SAILAWAY", "", NULL);
  if (rc == 0 && (game->ai_popups.queue_count != 1 || game->map_confirm_payload != 1)) {
    rc = fail("next Yes/No after the answer must queue");
  }
  assets_msg_free(&game->messages);
  free(game);
  return rc;
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
  /* Dock options (FUN_38fd_3746 sets neither DS:0x1f66 nor box[+0x22], port_bugs.md #1190):
   * 1 prose line + 6 rows = 94, no footer, centred plain (no figure): y = 100 - 47. */
  game->europe.menu = EUROPE_MENU_DOCK;
  if (!europe_menu_layout(game, 320, 200, &lay) || lay.dialog_h != 94 || lay.dialog_y != 53 ||
      lay.row_dx != 4 || lay.help_footer || lay.graphic) {
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

/*
 * port_bugs.md #1221: the DEBUG.TXT Create Unit lists are generic section boxes (181f:0998), so
 * box[+0x28] is the 0x50 default and the rows widen it, and FUN_2b5a_2866 latches DS:0x1f5e
 * first: @CREATE MSS1 (dropped: 14 rows + the soldier reach 200), @CSHIP MSS0, @FOREIGN MSS2;
 * @SETVIEW / @SETHUMAN MSS3. Frames from the DOSBox-X captures of 2026-10-23.
 */
static int case_cheat_create_unit_is_dos(void) {
  ColonizeMsgCatalog dbg;
  assets_msg_init(&dbg);
  ColonizeFont font;
  char err[128];
  if (!assets_msg_load_file(&dbg, "COLONIZE/DEBUG.TXT") ||
      !font_file_load("COLONIZE/FONTINTR.FF", &font, err, sizeof(err))) {
    return fail("DEBUG.TXT / FONTINTR");
  }
  ai_popup_set_portrait_source("COLONIZE");
  static const struct {
    const char* tag;
    int rows;
    int mss;
    int x, y, w;
  } k_lists[] = {
    {"CREATE", 14, 1, 106, 5, 108},
    {"CSHIP", 6, 0, 119, 84, 86},
    {"FOREIGN", 4, 2, 117, 94, 86},
  };
  int rc = 0;
  static uint8_t px[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = px};
  for (size_t k = 0; k < sizeof(k_lists) / sizeof(k_lists[0]); ++k) {
    char buf[15][CHEAT_LIST_LABEL_LEN];
    const char* fallback[15] = {""};
    for (int i = 0; i < 15; ++i) {
      fallback[i] = "";
    }
    cheat_list_catalog_rows(&dbg, k_lists[k].tag, 0, fallback, buf, k_lists[k].rows + 1);
    const char* labels[14];
    int ids[14];
    for (int i = 0; i < k_lists[k].rows; ++i) {
      labels[i] = buf[i + 1];
      ids[i] = i;
    }
    CheatListDialog dlg;
    memset(px, 0, sizeof(px));
    if (!cheat_list_open_create_unit(&dlg, buf[0], labels, ids, k_lists[k].rows, k_lists[k].mss)) {
      rc = fail("create unit list did not open");
      continue;
    }
    cheat_list_render(&dlg, &font, NULL, NULL, COLONIZE_COL_BASIC, 0x46, &fb);
    if (dlg.dialog_x != k_lists[k].x || dlg.dialog_y != k_lists[k].y ||
        dlg.dialog_w != k_lists[k].w) {
      fprintf(stderr, "@%s frame (%d, %d, w %d), DOS (%d, %d, w %d)\n", k_lists[k].tag,
              dlg.dialog_x, dlg.dialog_y, dlg.dialog_w, k_lists[k].x, k_lists[k].y, k_lists[k].w);
      rc = 1;
    }
  }
  CheatListDialog dlg;
  if (!cheat_list_open_setview(&dlg, &dbg) || dlg.mss != 3 ||
      !cheat_list_open_set_human(&dlg, &dbg) || dlg.mss != 3) {
    rc = fail("@SETVIEW / @SETHUMAN lost the MSS3 latch");
  }
  /* "None" -> @SETAUTO: FUN_1000_8b88 row 1 = Yes sets DS:0x826 (ND 0x23D77). */
  if (!cheat_list_open_set_auto(&dlg, &dbg) || dlg.kind != CHEAT_LIST_KIND_SET_AUTO ||
      dlg.option_count != 2 || dlg.option_ids[0] != 1 || dlg.option_ids[1] != 2 || dlg.mss != 3) {
    rc = fail("@SETAUTO list");
  }
  font_file_free(&font);
  assets_msg_free(&dbg);
  return rc;
}

/*
 * port_bugs.md #1194: the FUN_647e_01c6 pager. n <= 10 = one page of n + 1 slots, else pages of
 * 10 ((n + E + 9) / 10, dos_bugs.md DB-53); the Europe row is slot 0, colony k slot k + E; Previous
 * (0x3e5) first past page 0, More (0x3e6) last before the last page, the delete row (1000)
 * only on the last page; greyed rows skipped by Up / Down, clicks ignored, never preselected.
 */
static int dest_open(
  CheatListDialog* dlg, int n, bool europe, const uint8_t* at, int def, const char* del
) {
  static char names[CHEAT_LIST_DEST_MAX][8];
  const char* labels[CHEAT_LIST_DEST_MAX];
  int ids[CHEAT_LIST_DEST_MAX];
  int count = 0;
  if (europe) {
    labels[count] = "Europe";
    ids[count++] = 999;
  }
  for (int k = 0; k < n; ++k) {
    snprintf(names[count], sizeof(names[count]), "C%d", k);
    labels[count] = names[count];
    ids[count++] = 100 + k;
  }
  return cheat_list_open_dest_pages(
    dlg, CHEAT_LIST_KIND_GOTO_PORT, "", labels, ids, at, count, europe, def, del
  );
}

static void dest_key(CheatListDialog* dlg, ColonizeKey key) {
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = key;
  cheat_list_handle_input(dlg, &in);
}

/* Enter on row idx (a page turn keeps the box up). */
static void dest_pick(CheatListDialog* dlg, int idx) {
  dlg->selection = idx;
  dest_key(dlg, COLONIZE_KEY_ENTER);
}

static int case_dest_pager_is_dos(void) {
  static CheatListDialog dlg;
  int size = 0;
  int pages = 0;
  cheat_list_dest_page_math(10, 1, &size, &pages);
  if (size != 11 || pages != 1) {
    return fail("10 colonies: one page of 11");
  }
  cheat_list_dest_page_math(11, 1, &size, &pages);
  if (size != 10 || pages != 2) {
    return fail("11 colonies: two pages of 10");
  }
  /* Single page: Europe, 3 colonies, the delete row. */
  if (!dest_open(&dlg, 3, true, NULL, -1, "Del") || dlg.option_count != 5 ||
      dlg.option_ids[0] != 999 || dlg.option_ids[1] != 100 || dlg.option_ids[3] != 102 ||
      dlg.option_ids[4] != 1000 || dlg.selection != 0) {
    return fail("single page rows");
  }
  /* Europe + 25 colonies: 3 pages of 10 slots. */
  if (!dest_open(&dlg, 25, true, NULL, -1, "Del") || dlg.page_count != 3 ||
      dlg.option_count != 11 || dlg.option_ids[0] != 999 || dlg.option_ids[1] != 100 ||
      dlg.option_ids[9] != 108 || dlg.option_ids[10] != 0x3e6) {
    return fail("page 0: Europe + colonies 0..8 + More");
  }
  dest_pick(&dlg, 10);
  if (!dlg.open || dlg.has_result || dlg.page != 1 || dlg.option_count != 12 ||
      dlg.option_ids[0] != 0x3e5 || dlg.option_ids[1] != 109 || dlg.option_ids[10] != 118 ||
      dlg.option_ids[11] != 0x3e6 || dlg.selection != 0) {
    return fail("page 1: Previous + colonies 9..18 + More");
  }
  dest_pick(&dlg, 11);
  if (dlg.page != 2 || dlg.option_count != 8 || dlg.option_ids[1] != 119 ||
      dlg.option_ids[6] != 124 || dlg.option_ids[7] != 1000) {
    return fail("page 2: Previous + colonies 19..24 + delete");
  }
  dest_pick(&dlg, 0);
  if (dlg.page != 1 || dlg.option_ids[1] != 109) {
    return fail("Previous goes back a page");
  }
  dest_pick(&dlg, 3);
  if (dlg.open || !dlg.has_result || dlg.result_id != 111) {
    return fail("a colony row returns its id");
  }
  /* No Europe row: colony k in slot k. Europe + 20: DOS leaves slot 20 on no page (DB-53). */
  if (!dest_open(&dlg, 12, false, NULL, -1, NULL) || dlg.option_count != 11 ||
      dlg.option_ids[0] != 100 || dlg.option_ids[9] != 109 || dlg.option_ids[10] != 0x3e6) {
    return fail("no Europe row: page 0 = colonies 0..9 + More");
  }
  dest_pick(&dlg, 10);
  if (dlg.option_count != 3 || dlg.option_ids[2] != 111) {
    return fail("no Europe row, no delete: page 1 = Previous + colonies 10..11");
  }
  if (!dest_open(&dlg, 20, false, NULL, -1, NULL) || dlg.page_count != 2) {
    return fail("20 colonies, no Europe row: two pages (DOS)");
  }
  if (!dest_open(&dlg, 20, true, NULL, -1, "Del") || dlg.page_count != 3) {
    return fail("Europe + 20 colonies: three pages");
  }
  dest_pick(&dlg, dlg.option_count - 1);
  if (dlg.option_count != 12 || dlg.option_ids[10] != 118 || dlg.option_ids[11] != 0x3e6) {
    return fail("Europe + 20: page 1 = Previous + colonies 9..18 + More");
  }
  dest_pick(&dlg, dlg.option_count - 1);
  if (dlg.option_count != 3 || dlg.option_ids[1] != 119 || dlg.option_ids[2] != 1000) {
    return fail("Europe + 20: the 20th colony on page 2 (DB-53)");
  }
  /* Default: preselected on its page. */
  if (!dest_open(&dlg, 5, true, NULL, 103, NULL) || dlg.selection != 4) {
    return fail("default preselected");
  }
  /* Greyed and omitted rows. */
  uint8_t at[CHEAT_LIST_DEST_MAX] = {0};
  at[2] = CHEAT_LIST_DEST_GREYED; /* colony 1 */
  if (!dest_open(&dlg, 4, true, at, 101, NULL) || dlg.disabled_mask != (1u << 2) ||
      dlg.selection != 0) {
    return fail("greyed row: disabled, never preselected");
  }
  dlg.selection = 1;
  dest_key(&dlg, COLONIZE_KEY_DOWN);
  if (dlg.selection != 3) {
    return fail("Down skips the greyed row");
  }
  dest_key(&dlg, COLONIZE_KEY_UP);
  dest_key(&dlg, COLONIZE_KEY_UP);
  dest_key(&dlg, COLONIZE_KEY_UP);
  if (dlg.selection != 4) {
    return fail("Up skips the greyed row and wraps to the last");
  }
  dlg.dialog_x = 0;
  dlg.dialog_y = 0;
  dlg.dialog_w = 320;
  dlg.dialog_h = 200;
  dlg.list_y0 = 50;
  dlg.line_h = 12;
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.mouse_left_clicked = true;
  in.mouse_x = 100;
  in.mouse_y = 50 + 2 * 12 + 5;
  cheat_list_handle_input(&dlg, &in);
  if (!dlg.open || dlg.has_result) {
    return fail("a click on the greyed row is ignored");
  }
  at[2] = CHEAT_LIST_DEST_PLAIN;
  at[0] = CHEAT_LIST_DEST_GREYED; /* no Europe row: colony 0 greyed, first row */
  if (!dest_open(&dlg, 3, false, at, -1, NULL) || dlg.selection != 0) {
    return fail("greyed first row is the initial selection");
  }
  dest_key(&dlg, COLONIZE_KEY_ENTER);
  if (!dlg.has_result || dlg.result_id != 100) {
    return fail("Enter returns the initially selected greyed row");
  }
  at[0] = CHEAT_LIST_DEST_OMITTED;
  if (!dest_open(&dlg, 12, false, at, -1, NULL) || dlg.option_count != 10 ||
      dlg.option_ids[0] != 101 || dlg.option_ids[8] != 109) {
    return fail("omitted row keeps its slot");
  }
  if (dest_open(&dlg, 0, false, NULL, -1, NULL) || dlg.open) {
    return fail("no rows: nothing shown");
  }
  if (!dest_open(&dlg, 0, false, NULL, -1, "Del") || dlg.option_count != 1) {
    return fail("no destinations but the delete row: shown");
  }
  return 0;
}

/*
 * port_bugs.md #1197: Find Colony is the @FINDCITY text entry; ND 0x22F2B walks every colony
 * comparing names case-blind (whole string) and takes the first the map view may show.
 */
static int case_find_colony_is_dos(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("allocate game state");
  }
  assets_msg_init(&game->messages);
  ai_popup_init(&game->ai_popups);
  if (!assets_msg_load_file(&game->messages, "COLONIZE/GAME.TXT")) {
    free(game);
    fprintf(stderr, "SKIP: GAME.TXT\n");
    return TR_SKIP;
  }
  char err[64];
  int rc = 0;
  if (!map_alloc(&game->world_map, 58, 72, err, sizeof(err))) {
    assets_msg_free(&game->messages);
    free(game);
    return fail("map_alloc");
  }
  game->world_map_ok = true;
  static const struct { const char* name; int nation; int x; int y; bool seen; } k_col[] = {
    {"Jamestown", 0, 10, 10, false}, {"Quebec", 1, 20, 20, false},
    {"Quebec", 1, 30, 30, true},     {"Montreal", 1, 40, 40, true},
  };
  for (int i = 0; i < 4; ++i) {
    ColonizeColony* c = &game->colonies.colonies[i];
    c->active = true;
    c->id = i + 1;
    c->nation_id = k_col[i].nation;
    c->x = k_col[i].x;
    c->y = k_col[i].y;
    snprintf(c->name, sizeof(c->name), "%s", k_col[i].name);
    if (k_col[i].seen) {
      map_reveal_tile(&game->world_map, c->x, c->y, 1);
    }
  }
  game->col1.head.curr_nation_map_view = 1;
  game_open_find_colony(game);
  if (!game->name_entry.open || game->name_entry.kind != NAME_ENTRY_KIND_FIND_COLONY ||
      strcmp(game->name_entry.label, "Colony:") != 0 || game->name_entry.name[0] ||
      game->name_entry.name_cap != 0x17 + 1) {
    rc = fail("@FINDCITY entry: empty field, \"Colony:\", 23 chars");
  }
  snprintf(game->name_entry.name, sizeof(game->name_entry.name), "qUEBEC");
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  game_handle_modal_input(game, &in);
  if (game->name_entry.open || game->map_cursor_x != 30 || game->map_cursor_y != 30 ||
      !game->view_pieces_mode || game->in_colony || game->ai_popups.queue_count != 0) {
    rc = fail("case-blind match: first colony the view shows (the seen Quebec), View Pieces");
  }
  game_find_colony(game, "Jamestown");
  if (game->map_cursor_x != 30 || game->ai_popups.queue_count != 1 ||
      !strstr(game->ai_popups.queue[0].body, "\"Jamestown\"")) {
    rc = fail("unseen colony skipped: @NOCITY with the typed text");
  }
  game_find_colony(game, "Mont");
  if (game->ai_popups.queue_count != 2 || !strstr(game->ai_popups.queue[1].body, "\"Mont\"")) {
    rc = fail("a prefix is no match");
  }
  game->col1.head.show_entire_map = 1;
  game_find_colony(game, "quebec");
  if (game->map_cursor_x != 20 || game->ai_popups.queue_count != 2) {
    rc = fail("show_entire_map: the first Quebec");
  }
  game->col1.head.show_entire_map = 0;
  game->col1.head.curr_nation_map_view = 4;
  game_find_colony(game, "JAMESTOWN");
  if (game->map_cursor_x != 10) {
    rc = fail("view >= 4 shows every colony");
  }
  game->col1.head.curr_nation_map_view = 0xffff;
  game_find_colony(game, "Montreal");
  if (game->ai_popups.queue_count != 3) {
    rc = fail("a negative view shows nothing");
  }
  map_free(&game->world_map);
  assets_msg_free(&game->messages);
  free(game);
  return rc;
}

/*
 * port_bugs.md #1206: the trade-route editor's cargo columns are DOS's. FUN_647e_09da heads
 * @ROUTE 6 Unload at x 0x7d and @ROUTE 7 Load at 0xd0; FUN_647e_1064 sends 0x72 < mx <= 0xc5 to
 * the unload list (strip from 0x7d) and mx > 0xc5 to the load list (strip from 0xd0).
 */
static int trade_click(TradeScreen* ts, const ColonizeCol1Save* col1, int x, int y) {
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.mouse_left_clicked = true;
  in.mouse_x = x;
  in.mouse_y = y;
  ts->request = TRADE_SCREEN_REQ_NONE;
  trade_screen_handle_input(ts, col1, NULL, &in);
  return ts->request;
}

static int case_trade_cargo_columns_are_dos(void) {
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.trade_route[0].dest_count = 1;
  col1.trade_route[0].stop[0].colony_index = 999;
  col1.trade_route[0].stop[0].load_count = 1; /* load list [Food], unload list empty */
  TradeScreen ts;
  trade_screen_init(&ts, NULL);
  trade_screen_open(&ts, 0);
  if (trade_click(&ts, &col1, 127, 70) != TRADE_SCREEN_REQ_CARGO_ADD || ts.request_is_load) {
    return fail("x 127: append to the (empty) unload list");
  }
  if (trade_click(&ts, &col1, 0xc5, 70) != TRADE_SCREEN_REQ_CARGO_ADD || ts.request_is_load) {
    return fail("x 0xc5: still the unload column");
  }
  if (trade_click(&ts, &col1, 0xd2, 70) != TRADE_SCREEN_REQ_CARGO_REMOVE || !ts.request_is_load ||
      ts.request_slot != 0) {
    return fail("x 0xd2: the load list's first icon (strip from 0xd0)");
  }
  if (trade_click(&ts, &col1, 0xd0 + 12, 70) != TRADE_SCREEN_REQ_CARGO_ADD || !ts.request_is_load) {
    return fail("past the load icon: append to the load list");
  }
  ColonizeFont tiny;
  char err[128];
  if (!font_file_load("COLONIZE/FONTTINY.FF", &tiny, err, sizeof(err))) {
    fprintf(stderr, "SKIP: %s\n", err);
    return TR_SKIP;
  }
  snprintf(ts.lab_unload, sizeof(ts.lab_unload), "U");
  snprintf(ts.lab_load, sizeof(ts.lab_load), "L");
  static uint8_t a[320 * 200], b[320 * 200];
  ColonizeFramebuffer8 fa = {.width = 320, .height = 200, .pixels = a};
  ColonizeFramebuffer8 fbb = {.width = 320, .height = 200, .pixels = b};
  trade_screen_render(&ts, &col1, NULL, "", &tiny, NULL, &fbb);
  memset(a, 0x22, sizeof(a));
  const uint8_t ink[4] = {0, 0x0f, 0x0f, 0x0f};
  const int y = 0x37 - tiny.max_height;
  font_draw_text_shaded(&tiny, &fa, 0x7d, y, "U", ink);
  font_draw_text_shaded(&tiny, &fa, 0xd0, y, "L", ink);
  int rc = 0;
  for (int py = y; py < 0x37; ++py) {
    for (int px = 0x73; px < 320; ++px) {
      if (a[py * 320 + px] != b[py * 320 + px]) {
        rc = fail("headers: Unload at x 0x7d, Load at x 0xd0");
        py = 0x37;
        break;
      }
    }
  }
  font_file_free(&tiny);
  return rc;
}

/*
 * port_bugs.md #1193: FUN_7562_0052 builds the save / load picker in FONTTINY (FUN_2a1f_07c4
 * with DS:0x89e, raw 119634), so it is 12 + 6 + 8 * rows tall (save 8 rows: 82, load 10: 98).
 */
static int case_save_load_is_fonttiny(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("allocate game state");
  }
  char err[128];
  if (!font_file_load("COLONIZE/FONTINTR.FF", &game->intro_font, err, sizeof(err)) ||
      !font_file_load("COLONIZE/FONTTINY.FF", &game->colony_font, err, sizeof(err))) {
    free(game);
    fprintf(stderr, "SKIP: assets: %s\n", err);
    return TR_SKIP;
  }
  game->intro_font_ok = true;
  game->colony_font_ok = true;
  int rc = 0;
  if (save_load_font(game) != &game->colony_font) {
    rc = fail("save / load picker not in FONTTINY");
  }
  static SaveLoadDialog dlg;
  memset(&dlg, 0, sizeof(dlg));
  dlg.open = true;
  dlg.width = 190;
  snprintf(dlg.prompt, sizeof(dlg.prompt), "Select Save Slot");
  for (int rows = 8; rows <= 10; rows += 2) {
    dlg.option_count = rows;
    for (int i = 0; i < rows; ++i) {
      snprintf(dlg.options[i], sizeof(dlg.options[i]), "(EMPTY)");
    }
    static uint8_t px[320 * 200];
    ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = px};
    save_load_render(&dlg, save_load_font(game), NULL, NULL, 1, 3, &fb);
    if (dlg.dialog_h != 12 + 6 + 8 * rows || dlg.line_h != 8 || dlg.list_y0 != dlg.dialog_y + 15) {
      fprintf(stderr, "save / load %d rows: h %d line_h %d list_y0 %d (y %d)\n", rows,
              dlg.dialog_h, dlg.line_h, dlg.list_y0, dlg.dialog_y);
      rc = 1;
    }
  }
  font_file_free(&game->intro_font);
  font_file_free(&game->colony_font);
  free(game);
  return rc;
}

static const TestCase k_cases[] = {
    {"token_apply", case_token_apply},
    {"save_load_is_fonttiny", case_save_load_is_fonttiny},
    {"howmuch", case_howmuch},
    {"options", case_options},
    {"checkbox_hotkeys", case_checkbox_hotkeys},
    {"list_keys_wrap", case_list_keys_wrap},
    {"name_entry", case_name_entry},
    {"game_txt_wired_sections", case_game_txt_wired_sections},
    {"recruitchoose_width", case_recruitchoose_width},
    {"game_confirmation_catalog_choices", case_game_confirmation_catalog_choices},
    {"map_confirm_second_refused", case_map_confirm_second_refused},
    {"europe_menu_frames_are_dos", case_europe_menu_frames_are_dos},
    {"debug_test_ignores_directives", case_debug_test_ignores_directives},
    {"cheat_list_shared_parser", case_cheat_list_shared_parser},
    {"cheat_create_unit_is_dos", case_cheat_create_unit_is_dos},
    {"dest_pager_is_dos", case_dest_pager_is_dos},
    {"find_colony_is_dos", case_find_colony_is_dos},
    {"trade_cargo_columns_are_dos", case_trade_cargo_columns_are_dos},
};

TEST_MAIN(k_cases)
