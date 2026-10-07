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
void dos_rng_reseed_83a6(ColonizeDosRng* rng);
/* Inclusive range [lo, hi], matching FUN_19ef_0032. */
int dos_rng_range(ColonizeDosRng* rng, int lo, int hi);

#endif
