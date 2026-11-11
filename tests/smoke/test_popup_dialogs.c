#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/cheat_list_dialog.h"
#include "core/font_file.h"
#include "core/game_loop_internal.h"
#include "core/howmuch_dialog.h"
#include "core/mods.h"
#include "core/name_entry_dialog.h"
#include "core/options_dialog.h"
#include "core/popup_msg.h"
#include "core/save_load_dialog.h"
#include "core/settings.h"
#include "core/units_internal.h"

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

/* #1724: FUN_6f74_2580 bars the row under the pointer and toggles it on the release;
 * a release off the box (bar cleared) leaves. */
static int case_options_release_toggles_barred_row(void) {
  OptionsDialog od;
  ColonizeCol1GameOptions opts;
  memset(&opts, 0, sizeof(opts));
  if (!options_dialog_open_game(&od, NULL, &opts)) {
    return fail("options_open_game");
  }
  od.dialog_x = 50;
  od.dialog_y = 50;
  od.dialog_w = 200;
  od.dialog_h = 120;
  od.list_y0 = 60;
  od.line_h = 10;
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.mouse_x = 100;
  in.mouse_y = 61; /* row 0 */
  in.mouse_left_clicked = true;
  in.mouse_left_down = true;
  options_dialog_handle_input(&od, &in);
  in.mouse_left_clicked = false;
  in.mouse_y = 71; /* drag to row 1 */
  options_dialog_handle_input(&od, &in);
  if (od.values[0] || od.values[1] || od.selection != 1) {
    return fail("options: press / hold only move the bar");
  }
  in.mouse_left_down = false;
  in.mouse_left_released = true;
  options_dialog_handle_input(&od, &in);
  if (!od.open || od.values[0] || !od.values[1]) {
    return fail("options: the release toggles the barred row and stays open");
  }
  /* DS:0x7f6 stays set on the release poll: a release on the box off every row (the
   * title) sets bVar4 and does nothing. */
  memset(&in, 0, sizeof(in));
  in.mouse_x = 100;
  in.mouse_y = 55;
  in.mouse_left_clicked = true;
  in.mouse_left_down = true;
  options_dialog_handle_input(&od, &in);
  in.mouse_left_clicked = false;
  in.mouse_left_down = false;
  in.mouse_left_released = true;
  options_dialog_handle_input(&od, &in);
  if (!od.open || od.values[0] || !od.values[1]) {
    return fail("options: a release on the title toggles nothing");
  }
  memset(&in, 0, sizeof(in));
  in.mouse_x = 100;
  in.mouse_y = 61;
  in.mouse_left_clicked = true;
  in.mouse_left_down = true;
  options_dialog_handle_input(&od, &in);
  in.mouse_left_clicked = false;
  in.mouse_x = 10; /* off the box: the bar clears */
  options_dialog_handle_input(&od, &in);
  in.mouse_left_down = false;
  in.mouse_left_released = true;
  options_dialog_handle_input(&od, &in);
  if (od.open || !od.has_result || od.result_values[0] || !od.result_values[1]) {
    return fail("options: a release with no bar commits and leaves, row 0 untouched");
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
  /* port_bugs.md #1474: Esc keeps the toggles (FUN_6f74_2580 LAB_6f74_3018). */
  in.last_key = COLONIZE_KEY_SPACE;
  options_dialog_handle_input(&od, &in);
  in.last_key = COLONIZE_KEY_ESCAPE;
  options_dialog_handle_input(&od, &in);
  options_dialog_apply_game(&od, &applied);
  if (!od.has_result || od.result_cancelled || !applied.show_indian_moves) {
    return fail("options Esc keeps toggles");
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
  /* FUN_2b5a_268c reads the rows back whatever the box returned: Esc keeps the toggle. */
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ESCAPE;
  cheat_list_handle_input(&cl, &in);
  if (cl.open || !cl.has_result || cl.result_kind != CHEAT_LIST_KIND_DEBUG_FLAGS ||
      cl.result_mask != 0x4) {
    return fail("debug flags: Esc must commit the toggled rows (DS:0x894 = 0x4)");
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

/* FUN_6f74_2580 takes a release of either button alike (raw 117036-117060); only Esc cancels.
 * Off the box, only a flag-bit-0 box leaves (raw 117052-117054): @checkbox, FUN_647e_*. */
static int case_right_click_is_left_click(void) {
  static ColonizeFont font;
  char err[128];
  if (!font_file_load("COLONIZE/FONTINTR.FF", &font, err, sizeof(err))) {
    fprintf(stderr, "SKIP: assets: %s\n", err);
    return TR_SKIP;
  }
  static uint8_t px[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = px};
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.mouse_right_clicked = true;
  int rc = 0;

  static AiPopupState st;
  ai_popup_init(&st);
  const char* labels[] = {"Aaaa", "Bbbb", "Cccc"};
  const int ids[] = {1, 2, 3};
  for (int pass = 0; pass < 2; ++pass) {
    ai_popup_enqueue_choice_ctx(&st, AI_POPUP_TAG_KING_AUDIENCE, 0, 1, 0, NULL, "Body", labels,
                                ids, 3);
  }
  ai_popup_enqueue_ok(&st, AI_POPUP_TAG_INFO, NULL, "Body");
  ai_popup_try_present_next(&st);
  ai_popup_render(&st, &font, NULL, NULL, NULL, 1, 2, 3, &fb);
  in.mouse_x = st.dialog_x + 20;
  in.mouse_y = st.list_y0 + 2 * st.line_h + 1;
  ai_popup_handle_input(&st, &in);
  if (!st.has_result || st.result_cancelled || st.result_choice_id != 3) {
    rc = fail("ai_popup: a right click on a row picks it");
  }
  ai_popup_consume_result(&st);
  ai_popup_try_present_next(&st);
  ai_popup_render(&st, &font, NULL, NULL, NULL, 1, 2, 3, &fb);
  in.mouse_x = st.dialog_x > 4 ? 1 : 319;
  ai_popup_handle_input(&st, &in);
  if (st.has_result || !st.open || st.selection != -1) {
    rc = fail("ai_popup: a right click off a choice box drops the bar, box open (#1672)");
  }
  ai_popup_cancel_current(&st);
  ai_popup_consume_result(&st);
  ai_popup_try_present_next(&st);
  ai_popup_handle_input(&st, &in);
  if (!st.has_result || st.result_cancelled || st.result_choice_id != 0) {
    rc = fail("ai_popup: a right click dismisses a message box");
  }
  ai_popup_consume_result(&st);
  ai_popup_enqueue_choice_ctx(&st, AI_POPUP_TAG_KING_AUDIENCE, 0, 1, 0, NULL, "Body", labels, ids,
                              3);
  ai_popup_set_last_outside_leaves(&st); /* e.g. FUN_38fd_2bfe's box, flags |= 3 */
  ai_popup_try_present_next(&st);
  ai_popup_render(&st, &font, NULL, NULL, NULL, 1, 2, 3, &fb);
  in.mouse_x = st.dialog_x > 4 ? 1 : 319;
  ai_popup_handle_input(&st, &in);
  if (st.open || !st.has_result || !st.result_cancelled) {
    rc = fail("ai_popup: off a flag-bit-0 box a right click leaves with no row");
  }

  OptionsDialog od;
  ColonizeCol1GameOptions opts;
  memset(&opts, 0, sizeof(opts));
  options_dialog_open_game(&od, NULL, &opts);
  options_dialog_render(&od, &font, NULL, NULL, 68, 149, 138, &fb);
  in.mouse_x = od.dialog_x + 20;
  in.mouse_y = od.list_y0 + 1;
  {
    /* #1724: FUN_6f74_2580 acts on the release (press frame, then release frame). */
    ColonizeInputState rel = in;
    options_dialog_handle_input(&od, &in);
    rel.mouse_right_clicked = false;
    rel.mouse_right_released = true;
    options_dialog_handle_input(&od, &rel);
    if (!od.open || od.values[0] != 1) {
      rc = fail("options: a right click toggles the row, box open");
    }
    in.mouse_x = rel.mouse_x = od.dialog_x > 4 ? 1 : 319;
    options_dialog_handle_input(&od, &in);
    options_dialog_handle_input(&od, &rel);
  }
  if (!od.has_result || od.result_cancelled || od.result_values[0] != 1) {
    rc = fail("options: a right click off the @checkbox box leaves with the toggles");
  }

  CheatListDialog cl;
  const char* rows[] = {"One", "Two", "Three"};
  const int row_ids[] = {10, 20, 30};
  cheat_list_open_trade_select(&cl, "Route", rows, row_ids, 3);
  cheat_list_render(&cl, &font, NULL, NULL, 1, 3, &fb);
  in.mouse_x = cl.dialog_x + 20;
  in.mouse_y = cl.list_y0 + cl.line_h + 1;
  cheat_list_handle_input(&cl, &in);
  if (!cl.has_result || cl.result_id != 20) {
    rc = fail("cheat list: a right click on a row picks it");
  }
  cheat_list_open_trade_select(&cl, "Route", rows, row_ids, 3);
  cheat_list_render(&cl, &font, NULL, NULL, 1, 3, &fb);
  in.mouse_x = cl.dialog_x > 4 ? 1 : 319;
  cheat_list_handle_input(&cl, &in);
  if (cl.open || cl.has_result) {
    rc = fail("cheat list: off a FUN_647e_0796 box (flag bit 0) a click leaves with no pick");
  }
  cheat_list_open_create_unit(&cl, "Unit", rows, row_ids, 3, -1);
  cheat_list_render(&cl, &font, NULL, NULL, 1, 3, &fb);
  in.mouse_x = cl.dialog_x > 4 ? 1 : 319;
  for (int b = 0; b < 2; ++b) {
    in.mouse_left_clicked = b == 0;
    in.mouse_right_clicked = b == 1;
    cheat_list_handle_input(&cl, &in);
  }
  if (!cl.open || cl.selection != -1) {
    rc = fail("cheat list: off a DEBUG.TXT list box a click stays up with no selection");
  }
  in.mouse_left_clicked = in.mouse_right_clicked = false;
  in.last_key = COLONIZE_KEY_ENTER;
  cheat_list_handle_input(&cl, &in);
  if (!cl.open || cl.has_result) {
    rc = fail("cheat list: Enter after an off-box click confirms nothing (raw 116717)");
  }
  in.last_key = COLONIZE_KEY_NONE;
  font_file_free(&font);
  return rc;
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

/* OS window close asks the @DOSYES confirm first: No keeps playing, Yes quits, a
 * second close while it is up quits. A report is left first; over a player popup
 * the close is remembered and asks once that popup is answered. */
static int case_os_quit_asks_first(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("allocate game state");
  }
  assets_msg_init(&game->messages);
  ai_popup_init(&game->ai_popups);
  int rc = 0;
  ColonizeInputState enter;
  memset(&enter, 0, sizeof(enter));
  enter.last_key = COLONIZE_KEY_ENTER;
  if (!assets_msg_load_file(&game->messages, "COLONIZE/GAME.TXT")) {
    rc = fail("load GAME.TXT for the quit confirm");
  } else if (game_request_quit(game) || game->map_confirm != GAME_MAP_CONFIRM_QUIT ||
             game->ai_popups.queue_count != 1) {
    rc = fail("first OS quit must queue the Exit to DOS confirm, not quit");
  } else if (!game_request_quit(game) || game->ai_popups.queue_count != 1) {
    rc = fail("second OS quit while the confirm is queued must quit, not stack");
  } else if (!ai_popup_try_present_next(&game->ai_popups)) {
    rc = fail("present the quit confirm");
  } else {
    ai_popup_cancel_current(&game->ai_popups); /* No */
    game_apply_ai_popup_result(game);
    if (game->elapsed_ms == UINT32_MAX || game->map_confirm != GAME_MAP_CONFIRM_NONE ||
        ai_popup_busy(&game->ai_popups)) {
      rc = fail("No must keep the game running with no popup left");
    } else if (game_request_quit(game) || !ai_popup_try_present_next(&game->ai_popups)) {
      rc = fail("OS quit after a No must ask again");
    } else {
      ai_popup_handle_input(&game->ai_popups, &enter); /* default row: Yes */
      game_apply_ai_popup_result(game);
      if (game->elapsed_ms != UINT32_MAX) {
        rc = fail("Yes must quit");
      }
    }
  }
  if (rc == 0) {
    game->elapsed_ms = 0;
    game->in_menu = true;
    if (game_request_quit(game) || game->map_confirm != GAME_MAP_CONFIRM_TITLE_EXIT) {
      rc = fail("OS quit on the title menu must ask too");
    }
    ai_popup_clear(&game->ai_popups);
    game->map_confirm = GAME_MAP_CONFIRM_NONE;
    game->in_menu = false;
  }
  if (rc == 0) {
    /* A report draws no popup: the close leaves it, then asks on the map. */
    game->in_report = true;
    game->ff_pedia_after_report = -1;
    if (game_request_quit(game) || game->in_report ||
        game->map_confirm != GAME_MAP_CONFIRM_QUIT || game->ai_popups.queue_count != 1) {
      rc = fail("OS quit on a report must leave it and queue the confirm");
    }
    ai_popup_clear(&game->ai_popups);
    game->map_confirm = GAME_MAP_CONFIRM_NONE;
  }
  if (rc == 0) {
    /* The player's own popup stays up; the confirm follows its answer. */
    if (!ai_popup_enqueue_ok(&game->ai_popups, AI_POPUP_TAG_INFO, NULL, "x") ||
        !ai_popup_try_present_next(&game->ai_popups)) {
      rc = fail("present a player popup");
    } else if (game_request_quit(game) || !game->ai_popups.open ||
               game->ai_popups.current.tag != AI_POPUP_TAG_INFO ||
               game->map_confirm != GAME_MAP_CONFIRM_NONE || !game->quit_ask_pending) {
      rc = fail("OS quit over a popup must keep it open and remember the close");
    } else if (!game_request_quit(game) || !game->ai_popups.open) {
      rc = fail("second OS quit while a close is remembered must quit");
    } else {
      ai_popup_handle_input(&game->ai_popups, &enter);
      game_apply_ai_popup_result(game);
      game_quit_ask_service(game);
      if (game->map_confirm != GAME_MAP_CONFIRM_QUIT || game->ai_popups.queue_count != 1 ||
          game->quit_ask_pending) {
        rc = fail("the remembered close must ask once the popup is answered");
      }
    }
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
  /* port_bugs.md #1604 (7): the font follows the section's @smallfont (FUN_6f74_32a4), not
   * the menu id, so a mod dropping it from @KINGRECRUIT puts Train on FONTINTR. */
  ColonizeMsgSection* kr = (ColonizeMsgSection*)assets_msg_find(&game->messages, "KINGRECRUIT");
  for (int i = 0; kr && i < kr->line_count; ++i) {
    if (strcmp(kr->lines[i], "@smallfont") == 0) {
      kr->lines[i][1] = 'X';
    }
  }
  game->europe.menu = EUROPE_MENU_TRAIN;
  if (!europe_menu_layout(game, 320, 200, &lay) || lay.glyph_h != 9) {
    fprintf(stderr, "train without @smallfont: glyph %d, want 9\n", lay.glyph_h);
    rc = 1;
  }
  font_file_free(&game->intro_font);
  font_file_free(&game->colony_font);
  assets_msg_free(&game->messages);
  free(game);
  return rc;
}

/*
 * port_bugs.md #1595: under a report DOS keeps rotating the master palette (FUN_1a0a_007a)
 * and reprograms only the cycle span over the plate, so the map ramp advances and the
 * plate's span mirrors it, also with cycling off (frozen master phase).
 */
static int case_water_cycle_under_report(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("allocate game state");
  }
  game->map_palette_ok = true;
  for (int i = 0; i < 8; ++i) {
    game->map_palette.rgb[120 + i][0] = (uint8_t)(i + 1);
  }
  game->water_cycle_count = 1;
  game->water_cycle[0].start = 120;
  game->water_cycle[0].length = 8;
  game->water_cycle[0].step_ms = 10;
  game->in_report = true;
  game->reports_ok = true;
  game->report_id = COLONIZE_REPORT_LABOR;
  game->reports.background_ok[COLONIZE_REPORT_LABOR] = true;
  ColonizePikImage* plate = &game->reports.backgrounds[COLONIZE_REPORT_LABOR];
  plate->has_palette = true;
  memset(plate->palette.rgb, 0x3f, sizeof(plate->palette.rgb));
  int rc = 0;
  game->elapsed_ms = 10;
  game_water_cycle_tick(game);
  if (game->map_palette.rgb[120][0] != 8 || game->map_palette.rgb[121][0] != 1 ||
      memcmp(plate->palette.rgb[120], game->map_palette.rgb[120], 8 * 3) != 0 ||
      plate->palette.rgb[119][0] != 0x3f || plate->palette.rgb[128][0] != 0x3f) {
    fprintf(stderr, "report: map[120] %d plate[120] %d plate[128] %d\n",
            game->map_palette.rgb[120][0], plate->palette.rgb[120][0], plate->palette.rgb[128][0]);
    rc = 1;
  }
  memset(plate->palette.rgb[120], 0x3f, 8 * 3);
  game->col1.head.game_options.water_color_cycling_off = 1;
  game->elapsed_ms = 20;
  game_water_cycle_tick(game);
  if (game->map_palette.rgb[120][0] != 8 ||
      memcmp(plate->palette.rgb[120], game->map_palette.rgb[120], 8 * 3) != 0) {
    fprintf(stderr, "cycling off: map[120] %d plate[120] %d\n", game->map_palette.rgb[120][0],
            plate->palette.rgb[120][0]);
    rc = 1;
  }
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
  ai_popup_init(&game->ai_popups);
  game->col1_ok = true;
  game->col1.head.game_options.woi = 1;
  game->col1.head.game_options.intervention_announced = 1;
  game->col1.head.game_options.woi_crosses_event = 1;
  game->col1.head.game_options.independence_chrome = 1;

  /* Audit N1: FUN_2b5a_08cc (asm 0x217AC) clears DS:0x5382 &= 0xf4 and shows
   * DEBUG @TEST as a popup (FUN_281f_0998), not a status line. */
  game_cheat_test_routine(game);

  PopupMsgTokens tok = {.number0 = 2, .has_number0 = true, .number1 = 1, .has_number1 = true};
  char expected[AI_POPUP_BODY_LEN];
  popup_msg_fill(&game->debug_txt, "TEST", &tok, "", expected, sizeof(expected));
  const char* body = game->ai_popups.current.body;
  int rc = 0;
  if (!game->ai_popups.open || strcmp(body, expected) != 0 || strchr(body, '@') != NULL ||
      !strstr(body, "= 2") || !strstr(body, "= 1")) {
    fprintf(stderr, "smoke_popup_dialogs: DEBUG @TEST got '%s' want '%s'\n", body, expected);
    rc = fail("DEBUG @TEST popup body");
  } else if (game->col1.head.game_options.woi || game->col1.head.game_options.intervention_announced ||
             game->col1.head.game_options.independence_chrome ||
             !game->col1.head.game_options.woi_crosses_event) {
    rc = fail("Test Routine must clear DS:0x5382 bits 0x0b only");
  }

  assets_msg_free(&game->debug_txt);
  free(game);
  return rc;
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
 * 10 ((n + E + 9) / 10, dos_bugs_ui.md DB-53); the Europe row is slot 0, colony k slot k + E; Previous
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
 * Audit N3 / N4: FUN_2b5a_2866 Create Unit. Treasure = `mov byte [bx+0x315b],1` (asm
 * 0x23A7D, 100 gold); Wagon Train home +0x314a = FUN_281f_0614(x, y, owner, -1), the
 * owner's nearest colony (asm 0x23A42-0x23A5B).
 */
static int case_cheat_create_treasure_wagon_dos(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!game || !assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
      !units_load_types(&game->units, &names)) {
    assets_msg_free(&names);
    free(game);
    fprintf(stderr, "SKIP: NAMES.TXT\n");
    return TR_SKIP;
  }
  ai_popup_init(&game->ai_popups);
  game->units_ok = true;
  game->colonies_ok = true;
  game->human_nation = 0;
  game->cheat_create_pending_nation = -1;
  game->map_cursor_x = 5;
  game->map_cursor_y = 5;
  static const struct { int id, nation, x, y; } k_cols[] = {
    {3, 0, 12, 5}, {4, 1, 6, 5}, {7, 0, 9, 5},
  };
  for (int i = 0; i < 3; ++i) {
    ColonizeColony* c = &game->colonies.colonies[i];
    c->active = true;
    c->id = k_cols[i].id;
    c->nation_id = k_cols[i].nation;
    c->x = k_cols[i].x;
    c->y = k_cols[i].y;
  }
  int rc = 0;
  for (int row = 6; row <= 7 && rc == 0; ++row) {
    const int before = units_slot_end(&game->units);
    game->cheat_list.has_result = true;
    game->cheat_list.result_kind = CHEAT_LIST_KIND_CREATE_UNIT;
    game->cheat_list.result_id = row;
    game_apply_cheat_list_result(game);
    const ColonizeUnit* u =
      units_slot_end(&game->units) > before ? &game->units.units[before] : NULL;
    if (!u || !u->active) {
      rc = fail("cheat Create Unit spawned nothing");
    } else if (row == 6 && (units_type_kind(units_type(&game->units, u->type_index)) !=
                                UNITS_KIND_WAGON ||
                            u->col1_origin != 7)) {
      fprintf(stderr, "wagon origin %d, want 7\n", u->col1_origin);
      rc = fail("cheat Wagon Train home = owner's nearest colony");
    } else if (row == 7 && units_treasure_value_gold(u) != 100) {
      fprintf(stderr, "treasure %d gold, want 100\n", units_treasure_value_gold(u));
      rc = fail("cheat Treasure is worth 100");
    }
  }
  assets_msg_free(&names);
  free(game);
  return rc;
}

/*
 * Audit N8 / N11 / N10 / N-U. Sound Test: FUN_291f_0436(DEBUG, SOUND, dx 1) shows the
 * @SOUND prose seeded with 1 (asm 0x23D86). Default route name with no @TRADENAMES:
 * the bare colony name (asm 0x61157-0x61169). Text entry: any click accepts (FUN_6f74_2580
 * LAB_6f74_2e53, only Esc writes -1), and Land Ho stores even an emptied field
 * (FUN_2b5a_001e strcpy, asm 0x20F26).
 */
static int case_cheat_prompts_and_text_entry_dos(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("allocate game");
  }
  ai_popup_init(&game->ai_popups);
  assets_msg_init(&game->debug_txt);
  assets_msg_init(&game->messages);
  int rc = 0;
  if (assets_msg_load_file(&game->debug_txt, "COLONIZE/DEBUG.TXT")) {
    game->debug_txt_ok = true;
    game_open_cheat_sound_test(game);
    if (!game->howmuch.open || !strstr(game->howmuch.prompt, "sound") ||
        game->howmuch.amount != 1) {
      fprintf(stderr, "sound prompt '%s' amount %d\n", game->howmuch.prompt,
              game->howmuch.amount);
      rc = fail("Sound Test prompt / seed");
    }
  }
  game->colonies_ok = true;
  game->colonies.colonies[0].active = true;
  game->colonies.colonies[0].id = 2;
  snprintf(game->colonies.colonies[0].name, sizeof(game->colonies.colonies[0].name), "Plymouth");
  char def_name[NAME_ENTRY_NAME_LEN];
  game_trade_default_name(game, 2, def_name, sizeof(def_name));
  if (rc == 0 && strcmp(def_name, "Plymouth") != 0) {
    fprintf(stderr, "default route name '%s'\n", def_name);
    rc = fail("no @TRADENAMES: bare colony name");
  }
  ColonizeInputState in;
  for (int pass = 0; pass < 2 && rc == 0; ++pass) {
    NameEntryDialog ne;
    name_entry_init(&ne);
    (void)name_entry_open(&ne, NAME_ENTRY_KIND_FOUND, "", "Jamestown", 1);
    ne.dialog_x = 100;
    ne.dialog_y = 80;
    ne.dialog_w = 120;
    ne.dialog_h = 40;
    memset(&in, 0, sizeof(in));
    in.mouse_x = 10;
    in.mouse_y = 10;
    in.mouse_left_clicked = pass == 0;
    in.mouse_right_clicked = pass == 1;
    in.mouse_left_down = pass == 0;
    in.mouse_right_down = pass == 1;
    name_entry_handle_input(&ne, &in);
    /* #1724: FUN_6f74_2580 acts on the release (DS:0x7f4), not the press. */
    if (ne.has_result) {
      rc = fail("outside press must not accept before the release");
    }
    memset(&in, 0, sizeof(in));
    in.mouse_x = 10;
    in.mouse_y = 10;
    in.mouse_left_released = pass == 0;
    in.mouse_right_released = pass == 1;
    name_entry_handle_input(&ne, &in);
    if (!ne.has_result || ne.result_cancelled || strcmp(ne.result_name, "Jamestown") != 0) {
      rc = fail(pass == 0 ? "outside left click must accept" : "right click must accept");
    }
  }
  if (rc == 0) {
    game->col1_ok = true;
    game->human_nation = 0;
    snprintf(game->col1.player[0].country_name, sizeof(game->col1.player[0].country_name),
             "New England");
    (void)name_entry_open(&game->name_entry, NAME_ENTRY_KIND_LANDHO, "", "", -1);
    memset(&in, 0, sizeof(in));
    in.last_key = COLONIZE_KEY_ENTER;
    (void)game_handle_modal_input(game, &in);
    if (game->name_entry.open || game->col1.player[0].country_name[0] != '\0') {
      fprintf(stderr, "country '%s'\n", game->col1.player[0].country_name);
      rc = fail("Land Ho must store the emptied field");
    }
  }
  assets_msg_free(&game->debug_txt);
  assets_msg_free(&game->messages);
  free(game);
  return rc;
}

/*
 * @HOWMUCH3 ship-to-ship with an amount below the hold. FUN_2f2b_4fec raw 51687-51695 empties
 * the slot and puts held - moved back on the source ship.
 */
static int case_howmuch_move_keeps_rest(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!game || !assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
      !units_load_types(&game->units, &names)) {
    assets_msg_free(&names);
    free(game);
    fprintf(stderr, "SKIP: NAMES.TXT\n");
    return TR_SKIP;
  }
  ai_popup_init(&game->ai_popups);
  int rc = 0;
  const int caravel = units_kind_type_index(&game->units, UNITS_KIND_CARAVEL);
  const int src = units_spawn(&game->units, caravel, 5, 5);
  const int dst = units_spawn_allow_stack(&game->units, caravel, 5, 5);
  if (caravel < 0 || src < 0 || dst < 0 || units_load_goods(&game->units, src, 3, 100) != 100) {
    rc = fail("two caravels, 100 goods in the source hold");
  } else {
    game->colony_screen.transport_unit_id = src;
    game->howmuch_move_dst_unit_id = dst;
    units_get(&game->units, dst)->orders = UNITS_ORDER_SENTRY;
    (void)howmuch_open(&game->howmuch, HOWMUCH_KIND_MOVE, "How much?", NULL, 100, 30, 3, 0);
    ColonizeInputState in;
    memset(&in, 0, sizeof(in));
    in.last_key = COLONIZE_KEY_ENTER;
    game_handle_modal_input(game, &in);
    const int dst_held = units_unit_hold_amount(units_get(&game->units, dst), 0);
    const int src_held = units_unit_hold_amount(units_get(&game->units, src), 0);
    if (dst_held != 30 || src_held != 70) {
      fprintf(stderr, "  src=%d dst=%d\n", src_held, dst_held);
      rc = fail("30 moved, 70 stay on the source ship");
    }
    if (units_get(&game->units, dst)->orders != UNITS_ORDER_NONE) {
      rc = fail("#1730: a sentried receiver loses its orders (raw 51696-51697)");
    }
    units_get(&game->units, dst)->orders = UNITS_ORDER_TRADE_ROUTE;
    /* Destination takes only part: fill it to 1 free ton of the same cargo. */
    const int dst_room = units_goods_hold_count(&game->units, dst) * 100 - 31;
    if (units_load_goods(&game->units, dst, 3, dst_room) != dst_room) {
      rc = fail("fill destination to 1 free ton");
    } else {
      (void)howmuch_open(&game->howmuch, HOWMUCH_KIND_MOVE, "How much?", NULL, 70, 50, 3, 0);
      game_handle_modal_input(game, &in);
      const int src_left = units_unit_hold_amount(units_get(&game->units, src), 0);
      if (src_left != 69) {
        fprintf(stderr, "  src=%d\n", src_left);
        rc = fail("partial accept: 1 moved, 69 stay on the source ship");
      }
      if (units_get(&game->units, dst)->orders != UNITS_ORDER_TRADE_ROUTE) {
        rc = fail("#1730: a trade-route receiver keeps orders 2");
      }
    }
  }
  assets_msg_free(&names);
  free(game);
  return rc;
}

/*
 * port_bugs.md #1732: FUN_38fd_2edc -> FUN_38fd_285c, a hold dragged onto another harbor ship.
 * min(room, 100, held) moves; no room moves nothing; Shift asks @HOWMUCH3 and moves the answer.
 */
static int case_europe_ship_transfer(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!game || !assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
      !units_load_types(&game->units, &names)) {
    assets_msg_free(&names);
    free(game);
    fprintf(stderr, "SKIP: NAMES.TXT\n");
    return TR_SKIP;
  }
  ai_popup_init(&game->ai_popups);
  int rc = 0;
  const int caravel = units_kind_type_index(&game->units, UNITS_KIND_CARAVEL);
  EuropeScreen* eu = &game->europe;
  game->europe_ok = true;
  game->units_ok = true;
  eu->harbor_ships = 2;
  eu->harbor[0].type_index = caravel;
  eu->harbor[1].type_index = caravel;
  eu->harbor[0].hold_goods_type[0] = 3;
  eu->harbor[0].hold_goods_amount[0] = 80;
  game_europe_ship_transfer(game, 0, 1, 0, false);
  if (eu->harbor[0].hold_goods_amount[0] != 0 || eu->harbor[1].hold_goods_amount[0] != 80 ||
      eu->harbor[1].hold_goods_type[0] != 3) {
    rc = fail("#1732: the whole 80 moves to the other harbor ship");
  }
  /* Destination full of other cargo (a caravel has two holds): nothing moves. */
  eu->harbor[0].hold_goods_type[0] = 3;
  eu->harbor[0].hold_goods_amount[0] = 50;
  eu->harbor[1].hold_goods_type[1] = 4;
  eu->harbor[1].hold_goods_amount[1] = 100;
  eu->harbor[1].hold_goods_type[0] = 4;
  eu->harbor[1].hold_goods_amount[0] = 100;
  game_europe_ship_transfer(game, 0, 1, 0, false);
  if (eu->harbor[0].hold_goods_amount[0] != 50) {
    rc = fail("#1732: no room on the destination moves nothing");
  }
  /* Shift: @HOWMUCH3 capped at min(room, 100, held), the answer moves, the rest stays. */
  eu->harbor[1].hold_goods_amount[0] = 0;
  eu->harbor[1].hold_goods_amount[1] = 0;
  game->in_europe = true;
  eu->selected_harbor = 0;
  game_europe_ship_transfer(game, 0, 1, 0, true);
  if (!game->howmuch.open || game->howmuch.max_amount != 50) {
    rc = fail("#1732: Shift opens @HOWMUCH3 capped at the 50 held");
  } else {
    (void)howmuch_open(&game->howmuch, HOWMUCH_KIND_MOVE, "How much?", NULL, 50, 20, 3, 0);
    ColonizeInputState in;
    memset(&in, 0, sizeof(in));
    in.last_key = COLONIZE_KEY_ENTER;
    game_handle_modal_input(game, &in);
    if (eu->harbor[0].hold_goods_amount[0] != 30 || eu->harbor[1].hold_goods_amount[0] != 20) {
      fprintf(stderr, "  src=%d dst=%d\n", eu->harbor[0].hold_goods_amount[0],
              eu->harbor[1].hold_goods_amount[0]);
      rc = fail("#1732: 20 typed moves, 30 stay");
    }
  }
  assets_msg_free(&names);
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
  /* FUN_647e_115c runs FUN_647e_10d2 on any button's release (raw 103143-103146). */
  ColonizeInputState rin;
  memset(&rin, 0, sizeof(rin));
  rin.mouse_right_clicked = true;
  rin.mouse_x = 127;
  rin.mouse_y = 70;
  ts.request = TRADE_SCREEN_REQ_NONE;
  trade_screen_handle_input(&ts, &col1, NULL, &rin);
  if (ts.request != TRADE_SCREEN_REQ_CARGO_ADD) {
    return fail("a right click works the route table like a left one");
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

/* Title menu Mods for New Games: the toggles are grouped by kind of change over
 * OPTIONS_MODS_PAGE_COUNT dialogs (data/mods @MODOPTIONS, @MODOPTIONS2,
 * @MODOPTIONS3; all of them would not fit 200 px in FONTINTR anyway). Every
 * ModId has exactly one row, every page fits the screen, every row has its own
 * hotkey within its page, and a page only writes its own bits. */
static int case_mods_dialog_pages(void) {
  ColonizeFont font;
  char err[128];
  if (!font_file_load("COLONIZE/FONTINTR.FF", &font, err, sizeof(err))) {
    return TR_SKIP;
  }
  ColonizeMsgCatalog cat;
  assets_msg_init(&cat);
  assets_set_mod_data_dir("./data/mods");
  const bool loaded = assets_msg_load_file(&cat, "COLONIZE/GAME.TXT");
  assets_set_mod_data_dir("");
  if (!loaded) {
    font_file_free(&font);
    return TR_SKIP;
  }
  int rc = 0;
  uint32_t seen = 0;
  const uint32_t mask = 1u << MOD_WAGON_LOOT | 1u << MOD_HARD_KING | 1u << MOD_GOTO_NATIVE_WAIT;
  static uint8_t px[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = px};
  for (int page = 0; page < OPTIONS_MODS_PAGE_COUNT; ++page) {
    OptionsDialog od;
    if (!options_dialog_open_mods(&od, &cat, mask, page) || od.option_count <= 0 ||
        od.ok_label[0] == '\0') {
      fprintf(stderr, "mods page %d did not open\n", page);
      rc = 1;
      continue;
    }
    int rows = 0;
    while (options_dialog_mods_page_id(page, rows) >= 0) {
      const int id = options_dialog_mods_page_id(page, rows);
      if (id >= MOD_COUNT || (seen >> id & 1u)) {
        fprintf(stderr, "mods page %d row %d: ModId %d out of range or on two rows\n", page,
                rows, id);
        rc = 1;
      } else {
        seen |= 1u << id;
      }
      ++rows;
    }
    if (od.mods_page != page || od.option_count != rows) {
      fprintf(stderr, "mods page %d: dialog page %d, %d rows, want %d\n", page, od.mods_page,
              od.option_count, rows);
      rc = 1;
    }
    for (int i = 0; i < od.option_count; ++i) {
      if (od.mods_ids[i] != options_dialog_mods_page_id(page, i) ||
          od.values[i] != (mask >> od.mods_ids[i] & 1u)) {
        fprintf(stderr, "mods page %d row %d: ModId %d shows %d\n", page, i, od.mods_ids[i],
                od.values[i]);
        rc = 1;
      }
      const int key = popup_hotkey_of(od.labels[i]);
      for (int j = -1; j < i; ++j) {
        if (key == 0 || (j >= 0 && key == popup_hotkey_of(od.labels[j]))) {
          fprintf(stderr, "mods page %d rows %d and %d: hotkey %d not unique\n", page, j, i, key);
          rc = 1;
        }
      }
    }
    memset(px, 0, sizeof(px));
    options_dialog_render(&od, &font, NULL, NULL, 68, 149, 138, &fb);
    const int rows_end = od.list_y0 + od.option_count * od.line_h;
    if (od.dialog_y < 0 || od.dialog_y + od.dialog_h > 200 || rows_end > od.dialog_y + od.dialog_h) {
      fprintf(stderr, "mods page %d: frame y %d h %d, rows end at %d (screen 200)\n", page,
              od.dialog_y, od.dialog_h, rows_end);
      rc = 1;
    }
    /* Next on the pages before the last, Done on the last. */
    const bool last = page == OPTIONS_MODS_PAGE_COUNT - 1;
    if (strcmp(od.ok_label, last ? "Done" : "Next") != 0) {
      fprintf(stderr, "mods page %d: button \"%s\"\n", page, od.ok_label);
      rc = 1;
    }
    /* The button sits under the rows, inside the frame. */
    if (od.ok_y < rows_end || od.ok_x < od.dialog_x || od.ok_y + od.ok_h > od.dialog_y + od.dialog_h ||
        od.ok_x + od.ok_w > od.dialog_x + od.dialog_w) {
      fprintf(stderr, "mods page %d: OK at %d,%d %dx%d outside frame or over rows\n", page, od.ok_x,
              od.ok_y, od.ok_w, od.ok_h);
      rc = 1;
    }
  }
  if (seen != (1u << MOD_COUNT) - 1u) {
    fprintf(stderr, "mods pages cover ModIds %#x, want every one of %d\n", (unsigned)seen,
            MOD_COUNT);
    rc = 1;
  }
  /* Row 1 toggled and confirmed on each page (Enter, then a click on Done): only
   * that row's bit moves. */
  for (int k = 0; k < 2 * OPTIONS_MODS_PAGE_COUNT; ++k) {
    const int page = k / 2;
    OptionsDialog od;
    if (!options_dialog_open_mods(&od, &cat, mask, page)) {
      continue;
    }
    options_dialog_render(&od, &font, NULL, NULL, 68, 149, 138, &fb);
    const int row = od.option_count > 1 ? 1 : 0;
    ColonizeInputState in;
    memset(&in, 0, sizeof(in));
    od.selection = row;
    in.last_key = COLONIZE_KEY_SPACE;
    options_dialog_handle_input(&od, &in);
    memset(&in, 0, sizeof(in));
    if (k % 2 == 0) {
      in.last_key = COLONIZE_KEY_ENTER;
    } else {
      in.mouse_left_clicked = true;
      in.mouse_x = od.ok_x + od.ok_w / 2;
      in.mouse_y = od.ok_y + od.ok_h / 2;
    }
    options_dialog_handle_input(&od, &in);
    if (k % 2 != 0) { /* #1724: the release acts */
      in.mouse_left_clicked = false;
      in.mouse_left_released = true;
      options_dialog_handle_input(&od, &in);
    }
    uint32_t out = mask;
    const uint32_t want = mask ^ 1u << options_dialog_mods_page_id(page, row);
    if (!options_dialog_apply_mods(&od, &out) || out != want) {
      fprintf(stderr, "mods page %d apply: mask %#x, want %#x\n", page, (unsigned)out,
              (unsigned)want);
      rc = 1;
    }
  }
  OptionsDialog od;
  if (options_dialog_open_mods(&od, &cat, mask, OPTIONS_MODS_PAGE_COUNT)) {
    fprintf(stderr, "a mods page past the last opened\n");
    rc = 1;
  }
  assets_msg_free(&cat);
  font_file_free(&font);
  return rc;
}

/* Load `path` into game->messages (with or without the data/mods overlay) and
 * build the title menu from it. */
static bool title_menu_from(ColonizeGameState* game, const char* path, bool overlay) {
  assets_msg_free(&game->messages);
  assets_msg_init(&game->messages);
  assets_set_mod_data_dir(overlay ? "./data/mods" : "");
  const bool ok = assets_msg_load_file(&game->messages, path);
  assets_set_mod_data_dir("");
  load_begin_menu(game);
  return ok;
}

/* Feed one key through the modal gate, as game_update does on the title. */
static void title_modal_key(ColonizeGameState* game, ColonizeKey key) {
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = key;
  (void)game_handle_modal_input(game, &in);
}

/* data/mods @MODBEGINMENU: one title row after every stock row, opening the
 * mods checklist; OK on a page chains the next, each page commits its own bits to
 * the settings.json defaults, and the campaign mask is never touched. */
static int case_title_menu_mods_row(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("calloc game");
  }
  assets_msg_init(&game->messages);
  int rc = 0;
  if (!title_menu_from(game, "COLONIZE/GAME.TXT", false)) {
    assets_msg_free(&game->messages);
    free(game);
    return TR_SKIP;
  }
  const int stock = game->menu_option_count;
  if (stock != 5 || game->menu_mods_row != -1) {
    fprintf(stderr, "no overlay: %d rows, mods row %d\n", stock, game->menu_mods_row);
    rc = 1;
  }
  (void)title_menu_from(game, "COLONIZE/GAME.TXT", true);
  if (game->menu_option_count != stock + 1 || game->menu_mods_row != stock ||
      game->menu_options[stock][0] == '\0') {
    fprintf(stderr, "overlay: %d rows, mods row %d\n", game->menu_option_count,
            game->menu_mods_row);
    rc = 1;
  }

  /* A data dir with a sixth (quit) row keeps it at 5; Mods moves to 6. */
  const char* six = "build/test_title_mods_GAME.TXT";
  FILE* f = fopen(six, "wb");
  if (f) {
    fputs("@BEGINMENU\n@width=160\n@y=91\n@options\nA\nB\nC\nD\nE\nQUIT\n\n"
          "@MODBEGINMENU\n@options\nMODS\n\n", f);
    fclose(f);
    if (!title_menu_from(game, six, false) || game->menu_option_count != 7 ||
        game->menu_mods_row != 6 || strcmp(game->menu_options[5], "QUIT") != 0) {
      fprintf(stderr, "six rows: %d rows, mods row %d\n", game->menu_option_count,
              game->menu_mods_row);
      rc = 1;
    }
    game->menu_selection = 6;
    activate_menu_selection(game);
    if (!game->options_dlg.open || game->options_dlg.kind != OPTIONS_KIND_MODS ||
        game->options_dlg.mods_page != 0) {
      rc = fail("six rows: row 6 did not open mods page 1");
    }
    options_dialog_close(&game->options_dlg);
    remove(six);
  } else {
    rc = fail("write six-row GAME.TXT");
  }

  /* The chained dialog on the title, against a scratch settings file. */
  const char* settings_file = "build/test_title_mods_settings.json";
  remove(settings_file);
  char err[256];
  if (!settings_init(settings_file, err, sizeof(err))) {
    fprintf(stderr, "settings_init: %s\n", err);
    rc = 1;
  }
  (void)title_menu_from(game, "COLONIZE/GAME.TXT", true);
  game->in_menu = true;
  mods_campaign_set(0);
  const uint32_t base = settings_mods_mask(settings_get());
  /* Space toggles row 0 of each page. */
  uint32_t page_bit[OPTIONS_MODS_PAGE_COUNT];
  uint32_t all_bits = 0;
  for (int page = 0; page < OPTIONS_MODS_PAGE_COUNT; ++page) {
    page_bit[page] = 1u << options_dialog_mods_page_id(page, 0);
    all_bits |= page_bit[page];
  }
  ColonizeSettings prefs = *settings_get();
  settings_mods_set_mask(&prefs, base & ~all_bits);
  settings_set(&prefs);
  const uint32_t start = settings_mods_mask(settings_get());

  /* Esc on page 1: nothing saved, no page 2. */
  game->menu_selection = game->menu_mods_row;
  activate_menu_selection(game);
  title_modal_key(game, COLONIZE_KEY_SPACE);
  title_modal_key(game, COLONIZE_KEY_ESCAPE);
  if (game->options_dlg.open || settings_mods_mask(settings_get()) != start) {
    rc = fail("Esc on title mods page 1 saved or chained");
  }

  /* OK on every page: each commits its own bit and chains the next; the last closes. */
  activate_menu_selection(game);
  uint32_t want = start;
  for (int page = 0; page < OPTIONS_MODS_PAGE_COUNT; ++page) {
    if (!game->options_dlg.open || game->options_dlg.mods_page != page) {
      fprintf(stderr, "title mods: page %d not open (open %d page %d)\n", page,
              game->options_dlg.open, game->options_dlg.mods_page);
      rc = 1;
      break;
    }
    title_modal_key(game, COLONIZE_KEY_SPACE);
    title_modal_key(game, COLONIZE_KEY_ENTER);
    want |= page_bit[page];
    if (settings_mods_mask(settings_get()) != want) {
      fprintf(stderr, "title mods page %d OK: mask %#x, want %#x\n", page,
              (unsigned)settings_mods_mask(settings_get()), (unsigned)want);
      rc = 1;
    }
  }
  if (game->options_dlg.open) {
    rc = fail("title mods dialog still open after OK on the last page");
  }

  /* OK, OK, Esc: pages 1 and 2 stay saved, page 3 discarded. */
  activate_menu_selection(game);
  for (int page = 0; page < OPTIONS_MODS_PAGE_COUNT; ++page) {
    title_modal_key(game, COLONIZE_KEY_SPACE);
    title_modal_key(
      game, page == OPTIONS_MODS_PAGE_COUNT - 1 ? COLONIZE_KEY_ESCAPE : COLONIZE_KEY_ENTER
    );
  }
  want = start | page_bit[OPTIONS_MODS_PAGE_COUNT - 1];
  if (game->options_dlg.open || settings_mods_mask(settings_get()) != want) {
    fprintf(stderr, "title mods Esc on last page: open %d mask %#x, want %#x\n",
            game->options_dlg.open, (unsigned)settings_mods_mask(settings_get()),
            (unsigned)want);
    rc = 1;
  }
  if (mods_campaign_mask() != 0) {
    rc = fail("title mods dialog touched the campaign mask");
  }

  settings_mods_set_mask(&prefs, base);
  settings_set(&prefs);
  remove(settings_file);
  assets_msg_free(&game->messages);
  free(game);
  return rc;
}

/* mods.current_vision: the veil follows the campaign mask, nothing else. */
static int case_current_vision_follows_mods(void) {
  ColonizeGameState* game = calloc(1, sizeof(*game));
  if (!game) {
    return fail("calloc game");
  }
  game->world_map_ok = true;
  game->map_palette_ok = true;
  game->human_nation = 0;
  mods_campaign_set(0);
  const bool off = game_current_vision_on(game);
  mods_campaign_set(~(1u << MOD_CURRENT_VISION) & ((1u << MOD_COUNT) - 1u));
  const bool others = game_current_vision_on(game);
  mods_campaign_set(1u << MOD_CURRENT_VISION);
  const bool on = game_current_vision_on(game);
  mods_campaign_set(0);
  free(game);
  if (off || others || !on) {
    fprintf(stderr, "current_vision: off %d, other mods %d, on %d\n", off, others, on);
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"mods_dialog_pages", case_mods_dialog_pages},
    {"title_menu_mods_row", case_title_menu_mods_row},
    {"current_vision_follows_mods", case_current_vision_follows_mods},
    {"token_apply", case_token_apply},
    {"save_load_is_fonttiny", case_save_load_is_fonttiny},
    {"howmuch", case_howmuch},
    {"howmuch_move_keeps_rest", case_howmuch_move_keeps_rest},
    {"europe_ship_transfer", case_europe_ship_transfer},
    {"options", case_options},
    {"options_release_toggles_barred_row", case_options_release_toggles_barred_row},
    {"checkbox_hotkeys", case_checkbox_hotkeys},
    {"list_keys_wrap", case_list_keys_wrap},
    {"right_click_is_left_click", case_right_click_is_left_click},
    {"name_entry", case_name_entry},
    {"game_txt_wired_sections", case_game_txt_wired_sections},
    {"recruitchoose_width", case_recruitchoose_width},
    {"game_confirmation_catalog_choices", case_game_confirmation_catalog_choices},
    {"os_quit_asks_first", case_os_quit_asks_first},
    {"map_confirm_second_refused", case_map_confirm_second_refused},
    {"europe_menu_frames_are_dos", case_europe_menu_frames_are_dos},
    {"water_cycle_under_report", case_water_cycle_under_report},
    {"debug_test_ignores_directives", case_debug_test_ignores_directives},
    {"cheat_list_shared_parser", case_cheat_list_shared_parser},
    {"cheat_create_unit_is_dos", case_cheat_create_unit_is_dos},
    {"cheat_create_treasure_wagon_dos", case_cheat_create_treasure_wagon_dos},
    {"cheat_prompts_and_text_entry_dos", case_cheat_prompts_and_text_entry_dos},
    {"dest_pager_is_dos", case_dest_pager_is_dos},
    {"find_colony_is_dos", case_find_colony_is_dos},
    {"trade_cargo_columns_are_dos", case_trade_cargo_columns_are_dos},
};

TEST_MAIN(k_cases)
