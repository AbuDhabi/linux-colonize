#include "core/dos_rng.h"

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
  return (uint16_t)((rng->state >> 16) & 0x7fffu);
}

int dos_rng_range(ColonizeDosRng* rng, int lo, int hi) {
  if (!rng || hi <= lo) {
    return lo;
  }
  const int span = hi - lo + 1;
  const uint32_t r = dos_rng_next(rng);
  /* FUN_19ef_0032: (span * r) >> 15 + lo */
  return lo + (int)(((uint32_t)span * r) >> 15);
}
