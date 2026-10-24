#ifndef COLONIZE_DOS_RNG_H
#define COLONIZE_DOS_RNG_H

#include <stdint.h>

/*
 * Exact DOS libc rand used by VICEROY (FUN_1d1d_0e04 / FUN_19ef_0032):
 *   state = state * 0x343FD + 0x269EC3;
 *   return (state >> 16) & 0x7FFF;
 * Seed via FUN_1d1d_0df2: low = seed & 0x7FFF, high = 0.
 */
typedef struct ColonizeDosRng {
  uint32_t state;
} ColonizeDosRng;

void dos_rng_seed(ColonizeDosRng* rng, uint32_t seed);
uint16_t dos_rng_next(ColonizeDosRng* rng);
/* FUN_281f_04ca(DS:0x83a6): DOS reseeds from one session word at every nation
 * slot and at a few in-turn points (0688 starvation, 0f14 raid). The word is
 * recorded by ai_nation_reseed; -1 until then (no reseed). */
void dos_rng_set_83a6(int32_t seed);
int32_t dos_rng_get_83a6(void); /* -1 until a nation reseed */
void dos_rng_reseed_83a6(ColonizeDosRng* rng);
/* The music pick (FUN_129f_00f6, asm 129f:0138 / 129f:0250) brackets its
 * draws with FUN_19ef_002c = srand(BIOS tick & 0x7fff) on the one game LCG,
 * so each pick leaves the gameplay stream at srand(tick) at a wall-clock
 * moment. dos_rng_tick_reseed queues that reseed for the next draw (an
 * explicit dos_rng_seed first cancels it). Tick word -1 = wall clock; a fixed
 * campaign seed fixes it (VR_SEED reads DS:0x83a8 = 100). */
void dos_rng_set_tick_word(int32_t word);
void dos_rng_tick_reseed(void);
/* Golden harness: replay recorded picks. Resets the draw counter; a pick lands
 * just before draw draw_index[i] (0-based from this call). The array must
 * outlive the run; n = 0 clears. */
void dos_rng_schedule_tick_reseeds(const unsigned long* draw_index, int n);
unsigned long dos_rng_draw_count(void);
/* Inclusive range [lo, hi], matching FUN_19ef_0032. */
int dos_rng_range(ColonizeDosRng* rng, int lo, int hi);

/* settings.json divergences.modern_prng (main.c only): from this call every
 * dos_rng_next on any instance draws the top 15 bits of one process-wide
 * xoshiro256** (third_party/xoshiro256starstar.h), seeded by splitmix64(seed);
 * reseeds no longer touch the stream. Never called = DOS LCG, bit-identical. */
void dos_rng_modern_seed(uint64_t seed);
/* The 4-word xoshiro state (RTST save carry); NULL while modern is off. */
uint64_t* dos_rng_modern_state(void);
/* dos_rng_range that always steps the LCG: deterministic per-seed layouts that
 * are recomputed every frame (colony screen building slots). */
int dos_rng_range_lcg(ColonizeDosRng* rng, int lo, int hi);

#endif
