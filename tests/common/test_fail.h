#ifndef COLONIZE_TESTS_COMMON_TEST_FAIL_H
#define COLONIZE_TESTS_COMMON_TEST_FAIL_H

/*
 * The two failure reporters the test suite used to retype per file
 * (duplication audit TT-17 / TT-18).
 *
 * fail():  define TEST_NAME to the ctest target name before including this
 *          header; fail() prints "<TEST_NAME>: FAIL <msg>" and returns 1,
 *          which is exactly what the 21 per-file copies did.
 * check(): opt in with `#define TEST_WANT_CHECK` before the include. It
 *          prints "FAIL: <what>" and bumps the file-local `test_failures`
 *          counter — the body the 6 per-file copies shared, with their
 *          differently-named globals (failures / g_failures / g_fail)
 *          unified on one name.
 */

#include <stdbool.h>
#include <stdio.h>

#ifndef TEST_NAME
#define TEST_NAME "test"
#endif

static inline int fail(const char* msg) {
  fprintf(stderr, TEST_NAME ": FAIL %s\n", msg);
  return 1;
}

#ifdef TEST_WANT_CHECK
static int test_failures = 0;

static inline void check(bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    ++test_failures;
  }
}
#endif /* TEST_WANT_CHECK */

#endif /* COLONIZE_TESTS_COMMON_TEST_FAIL_H */
