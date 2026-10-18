#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include "core/dos_rng.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

static int32_t s_dos_rng_83a6 = -1;

/* FUN_19ef_0008 tick reseed (music pick, see dos_rng.h). */
static int32_t s_dos_rng_tick = -1;
static int s_tick_pending;
static unsigned long s_draw_count;
static const unsigned long* s_sched;
static int s_sched_n;

void dos_rng_set_tick_word(int32_t word) {
  s_dos_rng_tick = word;
}

void dos_rng_tick_reseed(void) {
  s_tick_pending = 1;
}

unsigned long dos_rng_draw_count(void) {
  return s_draw_count;
}

void dos_rng_schedule_tick_reseeds(const unsigned long* draw_index, int n) {
  s_draw_count = 0;
  s_sched = draw_index;
  s_sched_n = n;
  s_tick_pending = 0;
}

void dos_rng_set_83a6(int32_t seed) {
  s_dos_rng_83a6 = seed;
}

void dos_rng_reseed_83a6(ColonizeDosRng* rng) {
  if (rng && s_dos_rng_83a6 >= 0) {
    dos_rng_seed(rng, (uint32_t)s_dos_rng_83a6);
  }
}

void dos_rng_seed(ColonizeDosRng* rng, uint32_t seed) {
  if (!rng) {
    return;
  }
  /* FUN_1d1d_0df2: store seed in low word, clear high. A pick queued before
   * this reseed is overwritten by it. */
  s_tick_pending = 0;
  rng->state = (uint32_t)(seed & 0x7fffu);
}

uint16_t dos_rng_next(ColonizeDosRng* rng) {
  if (!rng) {
    return 0;
  }
  for (int i = 0; i < s_sched_n; ++i) {
    if (s_sched[i] == s_draw_count) {
      s_tick_pending = 1;
    }
  }
  ++s_draw_count;
  if (s_tick_pending) {
    /* FUN_19ef_0008: srand(BIOS tick & 0x7fff); VR_SEED's tick is fixed. */
    const uint32_t tick = s_dos_rng_tick >= 0 ? (uint32_t)s_dos_rng_tick : (uint32_t)time(NULL);
    dos_rng_seed(rng, tick);
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
  if (getenv("RANGEDBG")) { void* bt[6]; int nb = backtrace(bt, 6); fprintf(stderr, "RANGE %d %d draw %lu state %08x\n", lo, hi, dos_rng_draw_count(), rng ? rng->state : 0u); if (getenv("RANGEBT")) backtrace_symbols_fd(bt + 1, nb - 1, 2); }
  const uint32_t r = dos_rng_next(rng);
  /* FUN_19ef_0032: (span * r) >> 15 + lo */
  return lo + (int)(((uint32_t)span * r) >> 15);
}
