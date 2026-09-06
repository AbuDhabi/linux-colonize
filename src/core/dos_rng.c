#include "core/dos_rng.h"

#include <stdio.h>
#include <stdlib.h>

/* DOS_RNG_TRACE=1: log every draw with a running index (T1.23 stream audits). */
static int dos_rng_trace_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("DOS_RNG_TRACE");
    cached = (e && e[0] && e[0] != '0') ? 1 : 0;
  }
  return cached;
}

static unsigned long s_dos_rng_draws;

void dos_rng_seed(ColonizeDosRng* rng, uint32_t seed) {
  if (!rng) {
    return;
  }
  /* FUN_1d1d_0df2: store seed in low word, clear high. */
  rng->state = (uint32_t)(seed & 0x7fffu);
}

uint16_t dos_rng_next(ColonizeDosRng* rng) {
  if (!rng) {
    return 0;
  }
  /* Microsoft C / Watcom rand (FUN_1d1d_0e04). */
  rng->state = rng->state * 0x343FDu + 0x269EC3u;
  const uint16_t out = (uint16_t)((rng->state >> 16) & 0x7fffu);
  if (dos_rng_trace_enabled()) {
    fprintf(stderr, "DOS_RNG[%lu]=%u\n", s_dos_rng_draws++, (unsigned)out);
  }
  return out;
}

int dos_rng_range(ColonizeDosRng* rng, int lo, int hi) {
  if (!rng) {
    return lo;
  }
  if (hi < lo) {
    return lo;
  }
  /* Inclusive span; hi==lo still advances the LCG (span 1). */
  const int span = hi - lo + 1;
  const uint32_t r = dos_rng_next(rng);
  /* FUN_19ef_0032: (span * r) >> 15 + lo */
  return lo + (int)(((uint32_t)span * r) >> 15);
}
