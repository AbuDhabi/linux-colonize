#include "core/dos_buggy.h"

static bool s_dos_buggy_behavior;

bool dos_buggy_behavior(void) {
  return s_dos_buggy_behavior;
}

void dos_buggy_behavior_set(bool on) {
  s_dos_buggy_behavior = on;
}
