#include "core/dos_rng.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "third_party/xoshiro256starstar.h"

#if defined(__GLIBC__) && !defined(_WIN32)
#include <execinfo.h>
#define DOS_RNG_HAVE_BACKTRACE 1
#endif

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

/* RANGEDBG / RANGEBT (docs/debug_env_vars.md): set = on, read once. */
static int dos_rng_env_cached(const char* name, int* cache) {
  if (*cache < 0) {
    *cache = getenv(name) ? 1 : 0;
  }
  return *cache;
}

static int32_t s_dos_rng_83a6 = -1;

/* settings.json divergences.modern_prng: one process-wide xoshiro256** stream
 * feeds every ColonizeDosRng; dos_rng_seed and the reseed points then only
 * rewrite the instance's (unused) LCG word, so they never replay a sequence. */
static bool s_xo_on;
static uint64_t s_xo[4];

void dos_rng_modern_seed(uint64_t seed) {
  for (int i = 0; i < 4; ++i) {
    s_xo[i] = splitmix64_next(&seed);
  }
  s_xo_on = true;
}

uint64_t* dos_rng_modern_state(void) {
  return s_xo_on ? s_xo : NULL;
}

int dos_rng_range_lcg(ColonizeDosRng* rng, int lo, int hi) {
  const bool on = s_xo_on;
  s_xo_on = false;
  const int r = dos_rng_range(rng, lo, hi);
  s_xo_on = on;
  return r;
}

/* FUN_19ef_0008 tick reseed (music pick, see dos_rng.h). */
static int32_t s_dos_rng_tick = -1;
static int s_tick_pending;
static unsigned long s_draw_count;
static const unsigned long* s_sched;
static int s_sched_n;

static bool s_live_ticks;

void dos_rng_set_tick_word(int32_t word) {
  s_dos_rng_tick = word;
}

/* FUN_1c0c_0012 stand-in: fixed tick word (--seed / VR_SEED), else the clock.
 * ponytail: 1 s granularity vs DOS ~20 Hz, so 04ca calls within one wall
 * second (an instant EOT's nation slots) still replay one stream; upgrade to a
 * monotonic ms clock * 20 / 1000 if that ever shows. */
static uint32_t dos_rng_tick_now(void) {
  return s_dos_rng_tick >= 0 ? (uint32_t)s_dos_rng_tick : (uint32_t)time(NULL);
}

void dos_rng_set_live_ticks(bool on) {
  s_live_ticks = on;
}

uint32_t dos_rng_04ca_word(uint32_t fixed_word) {
  return s_live_ticks ? dos_rng_tick_now() : fixed_word;
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

int32_t dos_rng_get_83a6(void) {
  return s_dos_rng_83a6;
}

void dos_rng_set_83a6(int32_t seed) {
  s_dos_rng_83a6 = seed;
}

void dos_rng_reseed_83a6(ColonizeDosRng* rng) {
  if (rng && (s_live_ticks || s_dos_rng_83a6 >= 0)) {
    dos_rng_seed(rng, dos_rng_04ca_word((uint32_t)s_dos_rng_83a6));
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
    dos_rng_seed(rng, dos_rng_tick_now());
  }
  uint16_t out;
  if (s_xo_on) {
    const uint64_t x = xoshiro256ss_next(s_xo);
    rng->state = (uint32_t)(x >> 32); /* .state readers (europe stand-in seed, traces) see it move */
    out = (uint16_t)(x >> 49);        /* top 15 bits: the LCG's output domain */
  } else {
    /* Microsoft C / Watcom rand (FUN_1d1d_0e04). */
    rng->state = rng->state * 0x343FDu + 0x269EC3u;
    out = (uint16_t)((rng->state >> 16) & 0x7fffu);
  }
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
    /* FUN_19ef_0032 draws for any span: lo + (span * rand) >> 15, signed
     * (span <= 0 here), low word returned (port_bugs.md #1143). */
    const int32_t r = (int32_t)dos_rng_next(rng);
    return (int)(int16_t)(lo + (((int32_t)(hi - lo + 1) * r) >> 15));
  }
  /* Inclusive span; hi==lo still advances the LCG (span 1). */
  const int span = hi - lo + 1;
  static int s_rangedbg = -1;
  if (dos_rng_env_cached("RANGEDBG", &s_rangedbg)) {
    fprintf(stderr, "RANGE %d %d draw %lu state %08x\n", lo, hi, dos_rng_draw_count(), rng->state);
#ifdef DOS_RNG_HAVE_BACKTRACE
    static int s_rangebt = -1;
    if (dos_rng_env_cached("RANGEBT", &s_rangebt)) {
      void* bt[6];
      const int nb = backtrace(bt, 6);
      backtrace_symbols_fd(bt + 1, nb - 1, 2);
    }
#endif
  }
  const uint32_t r = dos_rng_next(rng);
  /* FUN_19ef_0032: (span * r) >> 15 + lo */
  return lo + (int)(((uint32_t)span * r) >> 15);
}
