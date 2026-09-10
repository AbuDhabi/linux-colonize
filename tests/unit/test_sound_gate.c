/*
 * FUN_12d8_000e option gate (asm 12d8:000e-0050) + its option binding
 * (FUN_2b5a_23ce, asm 2b5a:23ce-2447).
 *
 * The three @SOUNDOPTIONS checkboxes (GAME.TXT:111-118) are read back in
 * declaration order into DS:0xa2 / DS:0xa0 / DS:0xa4 and saved as Tut2 bits
 * 0x2 / 0x4 / 0x8, so:
 *   Background Music (DS:0xa2) -> BGM scheduler only, never this gate
 *   Event Music      (DS:0xa0) -> the 0x20 song class (0x20..0x3f)
 *   Sound Effects    (DS:0xa4) -> the 0x40 event class (0x40..0x5c)
 * and the CMP CX,0x10 in front of both is signed, so ids >= 0x8000 (the
 * 0x8020/0x8024 chord stings) are always forwarded.
 */

#include <stdbool.h>
#include <stdio.h>

#include "core/sound.h"

static int g_fail = 0;

static void check(bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    g_fail = 1;
  }
}

static ColonizeSoundOptions mk(bool bgm, bool ev, bool sfx) {
  ColonizeSoundOptions o;
  o.background_music = bgm;
  o.event_music = ev;
  o.sound_effects = sfx;
  return o;
}

int main(void) {
  const ColonizeSoundOptions all_on = mk(true, true, true);
  const ColonizeSoundOptions all_off = mk(false, false, false);

  /* System ids (< 0x10): unconditional, BX = 1 at 12d8:001c. */
  for (int id = 0; id < 0x10; ++id) {
    check(sound_id_gate_allows(id, all_off), "system id forwarded with every option off");
  }

  /* Song class 0x20..0x3f follows Event Music, not Background Music. */
  for (int id = 0x20; id <= 0x3f; ++id) {
    check(sound_id_gate_allows(id, all_on), "song plays with all options on");
    check(sound_id_gate_allows(id, mk(false, true, false)), "song plays on event_music alone");
    check(!sound_id_gate_allows(id, mk(true, false, true)), "song silent when event_music off");
  }

  /* Event class 0x40..0x5c follows Sound Effects, not Event Music. */
  for (int id = 0x40; id <= 0x5c; ++id) {
    check(sound_id_gate_allows(id, all_on), "event id plays with all options on");
    check(sound_id_gate_allows(id, mk(false, false, true)), "event id plays on sound_effects alone");
    check(!sound_id_gate_allows(id, mk(true, true, false)), "event id silent when sfx off");
  }

  /* Background Music never reaches this gate in either direction. */
  check(
    sound_id_gate_allows(0x21, mk(false, true, false)) ==
      sound_id_gate_allows(0x21, mk(true, true, false)),
    "background_music does not gate songs"
  );
  check(
    sound_id_gate_allows(0x45, mk(false, false, true)) ==
      sound_id_gate_allows(0x45, mk(true, false, true)),
    "background_music does not gate event ids"
  );

  /* Chord stings: negative as int16 => BX = 1 => ungated. */
  check(sound_id_gate_allows(0x8020, all_off), "0x8020 war-declaration chord ungated");
  check(sound_id_gate_allows(0x8024, all_off), "0x8024 assign-colonist chord ungated");
  check(sound_id_gate_allows(0x8026, all_off), "0x8026 chord ungated");
  check(sound_id_gate_allows(0x8000, all_off), "first negative id ungated");

  /* An id in [0x10,0x1f] carries neither bit: the OR chain drops it. */
  check(!sound_id_gate_allows(0x10, all_on), "0x10 carries neither class bit, dropped");
  check(!sound_id_gate_allows(0x1f, all_on), "0x1f carries neither class bit, dropped");

  /* The two class checks are an OR chain (12d8:002f-0045), not two vetoes:
   * an id with both bits plays when either option is on. */
  check(sound_id_gate_allows(0x60, mk(false, true, false)), "0x60 plays on event_music");
  check(sound_id_gate_allows(0x60, mk(false, false, true)), "0x60 plays on sound_effects");
  check(!sound_id_gate_allows(0x60, mk(true, false, false)), "0x60 silent with both off");

  if (g_fail) {
    return 1;
  }
  fprintf(stderr, "sound gate tests ok\n");
  return 0;
}
