#ifndef COLONIZE_DOS_RNG_H
#define COLONIZE_DOS_RNG_H

#include <stdbool.h>
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
/* FUN_281f_04ca = randomize(): JMPF FUN_19ef_002c -> FUN_19ef_0008 =
 * srand(BIOS tick 0040:006C & 0x7fff), at every nation slot, the LCR roll, the
 * 465b overspend move, 0688 starvation, 0f14 raid, ... The PUSH [0x83a6] at all
 * 17 call sites is a dead argument (DS:0x83a6 = 0, written once by
 * FUN_75c2_2d46). VR_SEED.EXE patches FUN_1c0c_0012 to return 100; the
 * "83a6" word below models that fixed tick (--seed, goldens), recorded by
 * ai_nation_reseed; -1 until then (no reseed). With live ticks on (unseeded
 * main.c) every 04ca reads the clock instead, as VICEROY does (playtest C2-O1:
 * every rumour of a game rolled the same outcome). */
void dos_rng_set_83a6(int32_t seed);
int32_t dos_rng_get_83a6(void); /* -1 until a nation reseed */
void dos_rng_reseed_83a6(ColonizeDosRng* rng);
void dos_rng_set_live_ticks(bool on);
uint32_t dos_rng_04ca_word(uint32_t fixed_word); /* live ? tick now : fixed_word */
/* Port-only, play_driver --live-ticks: with a fixed tick word set, each tick
 * read returns that word mixed with a read counter (reproducible per command
 * script, but no longer VR_SEED's constant). Pair with live ticks on. */
void dos_rng_set_synth_ticks(bool on);
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
