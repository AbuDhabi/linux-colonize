/* settings.json divergences.modern_prng: vendored xoshiro256** / splitmix64
 * against reference values, and the dos_rng + RTST plumbing on top. */
#include <stdio.h>
#include <stdlib.h>

#include "../common/test_runner.h"
#include "core/dos_rng.h"
#include "core/runtime_ext.h"
#include "third_party/xoshiro256starstar.h"

static int g_failures;

static void check(int cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    g_failures++;
  }
}

/* State {1,2,3,4}: the authors' xoshiro256starstar.c gives these (same
 * vector as the rand_xoshiro crate's reference test). splitmix64 seed 0
 * first output = Java SplittableRandom(0).nextLong(). */
static void test_xoshiro_reference(void) {
  static const uint64_t k_want[] = {
    11520u, 0u, 1509978240u, 1215971899390074240u, 1216172134540287360u, 607988272756665600u,
  };
  uint64_t s[4] = {1, 2, 3, 4};
  for (int i = 0; i < 6; ++i) {
    check(xoshiro256ss_next(s) == k_want[i], "xoshiro256** reference output");
  }
  uint64_t x = 0;
  check(splitmix64_next(&x) == 0xe220a8397b1dcdafu, "splitmix64 seed 0 first output");
}

/* Seed 100 through splitmix64: xoshiro outputs 0afee0773a0d8a51,
 * 13b0ca759b9b1735, 5c76d220f8461395, 8852f10b70a289f7, 481bbf68e1b4b076;
 * a draw is the top 15 bits. Reseeds must not replay; RTST must carry it. */
static void test_modern_stream(void) {
  ColonizeDosRng r;
  dos_rng_seed(&r, 5);
  check(dos_rng_modern_state() == NULL, "off until seeded");
  dos_rng_modern_seed(100);
  check(dos_rng_next(&r) == 1407, "draw 0 = top 15 bits");
  check(dos_rng_next(&r) == 2520, "draw 1");
  check(dos_rng_range(&r, 0, 0x7fff) == 11835, "range on the same stream");
  dos_rng_seed(&r, 5);
  check(dos_rng_next(&r) == 17449, "reseed does not replay");

  size_t n = 0;
  uint8_t* blob = runtime_ext_serialize(NULL, NULL, &n);
  check(dos_rng_next(&r) == 9229, "draw 4");
  runtime_ext_deserialize(NULL, NULL, NULL, blob, n);
  free(blob);
  check(dos_rng_next(&r) == 9229, "RTST restores the stream");

  dos_rng_seed(&r, 1);
  check(dos_rng_range_lcg(&r, 0, 0x7fff) == 41, "lcg path stays MSC rand (seed 1 -> 41)");
}

#define CASE_(fn) \
  static int tc_##fn(void) { \
    g_failures = 0; \
    fn(); \
    return g_failures; \
  }
CASE_(test_xoshiro_reference)
CASE_(test_modern_stream)
#undef CASE_

static const TestCase k_cases[] = {
  {"test_xoshiro_reference", tc_test_xoshiro_reference},
  {"test_modern_stream", tc_test_modern_stream},
};

TEST_MAIN(k_cases)
