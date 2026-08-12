#include "core/assets.h"
#include "core/howmuch_dialog.h"
#include "core/name_entry_dialog.h"
#include "core/options_dialog.h"
#include "core/popup_msg.h"

#include <stdio.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_popup_dialogs: %s\n", msg);
  return 1;
}

int main(void) {
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

  HowmuchDialog hm;
  if (!howmuch_open(&hm, HOWMUCH_KIND_BUY, "How much?", 100, 50, 3, 0)) {
    return fail("howmuch_open");
  }
  ColonizeInputState in;
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  howmuch_handle_input(&hm, &in);
  if (!hm.has_result || hm.result_cancelled || hm.result_amount != 50) {
    return fail("howmuch confirm");
  }

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
      !applied.combat_analysis || applied.water_color_cycling ||
      !applied.tutorial_hints) {
    return fail("options apply");
  }
  if (!options_dialog_open_game(&od, NULL, &applied) ||
      od.values[0] != 0 || od.values[1] != 1 || od.values[2] != 0 ||
      od.values[3] != 0 || od.values[4] != 1 || od.values[5] != 1 ||
      od.values[6] != 1 || od.values[7] != 1) {
    return fail("options reopen round-trip");
  }

  NameEntryDialog ne;
  if (!name_entry_open(&ne, NAME_ENTRY_KIND_FOUND, "What shall we name this colony?", "Jamestown", 1)) {
    return fail("name_entry found open");
  }
  if (strstr(ne.prompt, "Land Ho") != NULL) {
    return fail("found colony must not use Land Ho prompt");
  }
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

  /* Authenticity: wired sections must fill from GAME.TXT when present. */
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  if (assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT")) {
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
    n = popup_msg_choices(taxopt, tax_choices, 4);
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
  }

  printf("smoke_popup_dialogs ok\n");
  return 0;
}
