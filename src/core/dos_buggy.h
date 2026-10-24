#ifndef COLONIZE_DOS_BUGGY_H
#define COLONIZE_DOS_BUGGY_H

#include <stdbool.h>

/*
 * Process-wide "reproduce tolerated DOS bugs" switch (docs/tolerated_dos_bugs.md).
 * Off in opencol; every test binary turns it on before main()
 * (tests/common/test_name_kinds.c), so goldens keep the DOS bugs they depend on.
 * A fix is gated `if (dos_buggy_behavior()) { DOS bug } else { fix }`.
 * Not a setting: nothing in the shipped binary calls the setter.
 */
bool dos_buggy_behavior(void);
void dos_buggy_behavior_set(bool on);

#endif /* COLONIZE_DOS_BUGGY_H */
