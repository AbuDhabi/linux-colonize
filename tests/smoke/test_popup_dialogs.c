#include "core/howmuch_dialog.h"
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
  opts.autosave = 1;
  opts.end_of_turn = 1;
  if (!options_dialog_open_game(&od, NULL, &opts)) {
    return fail("options_open_game");
  }
  if (od.option_count < 8) {
    return fail("options count");
  }
  memset(&in, 0, sizeof(in));
  in.last_key = COLONIZE_KEY_ENTER;
  options_dialog_handle_input(&od, &in);
  if (!od.has_result || od.result_cancelled) {
    return fail("options confirm");
  }
  ColonizeCol1GameOptions applied;
  memset(&applied, 0, sizeof(applied));
  options_dialog_apply_game(&od, &applied);
  if (!applied.autosave || !applied.end_of_turn) {
    return fail("options apply");
  }

  printf("smoke_popup_dialogs ok\n");
  return 0;
}
