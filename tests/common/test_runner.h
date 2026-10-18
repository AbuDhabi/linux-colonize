/* test_runner.h — shared table-driven main() for tests/{unit,smoke,golden}.
 *
 * Usage:
 *   typedef int (*TestFn)(void);
 *   static const TestCase k_cases[] = {
 *       {"unit_mid_hire_mil", unit_mid_hire_mil},
 *       {"unit_soldier_board_empty_transport", unit_soldier_board_empty_transport},
 *   };
 *   TEST_MAIN(k_cases)
 *
 * Each case returns 0 on pass, nonzero on fail (existing convention).
 * Default execution order == declaration order, so plain `ctest` behaviour
 * is byte-for-byte unchanged. Env vars (see docs/debug_env_vars.md):
 *
 *   COLONIZE_TEST_ONLY=<name>       run exactly one named case
 *   COLONIZE_TEST_REVERSE=1         run cases in reverse declared order
 *   COLONIZE_TEST_SHUFFLE=<seed>    deterministic Fisher-Yates shuffle
 *   COLONIZE_TEST_LIST=1            print case names, one per line, exit 0
 *
 * REVERSE and SHUFFLE are mutually exclusive with each other but either
 * may combine with ONLY (ONLY simply restricts the run to one case and
 * ignores ordering).
 */
#ifndef COLONIZE_TEST_RUNNER_H
#define COLONIZE_TEST_RUNNER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A case (or a single-flow driver's main) returns TR_SKIP when it cannot run
 * here (no COLONIZE/ assets). It prints "SKIP <name>"; when every case that
 * ran skipped the binary exits 77, which CMake (colonize_add_test sets
 * SKIP_RETURN_CODE 77) reports as "Skipped" instead of a silent pass. */
#define TR_SKIP 77

typedef struct TestCase {
  const char* name;
  int (*fn)(void);
} TestCase;

/* Deterministic small LCG so we don't depend on rand()'s implementation
 * varying by libc -- shuffle must be reproducible across platforms. */
static unsigned long tr_lcg_state;

static inline unsigned long tr_lcg_next(void) {
  tr_lcg_state = tr_lcg_state * 6364136223846793005UL + 1442695040888963407UL;
  return (tr_lcg_state >> 33);
}

static inline void tr_fisher_yates(int* order, int n, unsigned long seed) {
  int i;
  tr_lcg_state = seed;
  for (i = 0; i < n; i++) {
    order[i] = i;
  }
  for (i = n - 1; i > 0; i--) {
    int j = (int)(tr_lcg_next() % (unsigned long)(i + 1));
    int tmp = order[i];
    order[i] = order[j];
    order[j] = tmp;
  }
}

static inline int tr_run_main(const TestCase* cases, int n) {
  const char* only = getenv("COLONIZE_TEST_ONLY");
  const char* reverse_env = getenv("COLONIZE_TEST_REVERSE");
  const char* shuffle_env = getenv("COLONIZE_TEST_SHUFFLE");
  const char* list_env = getenv("COLONIZE_TEST_LIST");
  int* order;
  int i;
  int failures = 0;
  int ran = 0;
  int skipped = 0;

  if (list_env != NULL && list_env[0] != '\0' && strcmp(list_env, "0") != 0) {
    for (i = 0; i < n; i++) {
      printf("%s\n", cases[i].name);
    }
    return 0;
  }

  order = (int*)malloc(sizeof(int) * (size_t)n);
  if (order == NULL) {
    fprintf(stderr, "test_runner: out of memory\n");
    return 1;
  }
  for (i = 0; i < n; i++) {
    order[i] = i;
  }

  if (shuffle_env != NULL && shuffle_env[0] != '\0') {
    unsigned long seed = strtoul(shuffle_env, NULL, 10);
    tr_fisher_yates(order, n, seed);
  } else if (reverse_env != NULL && reverse_env[0] != '\0' &&
             strcmp(reverse_env, "0") != 0) {
    for (i = 0; i < n; i++) {
      order[i] = n - 1 - i;
    }
  }

  for (i = 0; i < n; i++) {
    int idx = order[i];
    const TestCase* c = &cases[idx];
    int rc;

    if (only != NULL && only[0] != '\0' && strcmp(only, c->name) != 0) {
      continue;
    }

    ran++;
    rc = c->fn();
    if (rc == TR_SKIP) {
      printf("SKIP %s\n", c->name);
      skipped++;
    } else if (rc != 0) {
      printf("FAIL %s\n", c->name);
      failures++;
    } else {
      printf("PASS %s\n", c->name);
    }
  }

  free(order);

  if (only != NULL && only[0] != '\0' && ran == 0) {
    fprintf(stderr, "test_runner: COLONIZE_TEST_ONLY=%s matched no case\n",
            only);
    return 1;
  }

  if (failures != 0) {
    return 1;
  }
  return (ran > 0 && skipped == ran) ? TR_SKIP : 0;
}

#define TEST_MAIN(CASES) \
  int main(void) { \
    return tr_run_main((CASES), (int)(sizeof(CASES) / sizeof((CASES)[0]))); \
  }

/* Same, with one-time setup/teardown statements around the whole run
 * (e.g. diag_init(0, NULL) / diag_shutdown()). */
#define TEST_MAIN_WRAPPED(CASES, SETUP, TEARDOWN) \
  int main(void) { \
    int tr_rc_; \
    SETUP; \
    tr_rc_ = tr_run_main((CASES), (int)(sizeof(CASES) / sizeof((CASES)[0]))); \
    TEARDOWN; \
    return tr_rc_; \
  }

/* For hand-written single-flow drivers (goldens, smokes with one flow):
 * call first in main(): `int g = tr_single_flow_gate("name"); if (g >= 0) return g;`
 * The flow is the one case `name`: LIST prints it, ONLY=<other> is an error
 * (same message as the table runner), unset/empty/equal proceeds (-1). */
static inline int tr_single_flow_gate(const char* name) {
  const char* list_env = getenv("COLONIZE_TEST_LIST");
  const char* only = getenv("COLONIZE_TEST_ONLY");
  if (list_env != NULL && list_env[0] != '\0' && strcmp(list_env, "0") != 0) {
    printf("%s\n", name);
    return 0;
  }
  if (only != NULL && only[0] != '\0' && strcmp(only, name) != 0) {
    fprintf(stderr, "test_runner: COLONIZE_TEST_ONLY=%s matched no case\n", only);
    return 1;
  }
  return -1;
}

#endif /* COLONIZE_TEST_RUNNER_H */
