#ifndef COLONIZE_RUNTIME_EXT_H
#define COLONIZE_RUNTIME_EXT_H

/*
 * 'RTST' port-extension chunk: cross-turn state DOS keeps in DS but never
 * writes to COLONY##.SAV (so a DOS load starts it at zero while a continuous
 * session carries it), plus the port's LCG. Carrying it makes a port
 * save/reload chain play exactly like the continuous run (tests/golden
 * test_idle_campaign.c GOLDEN_IDLE_RELOAD; docs/savegame.md "Port extension
 * block"). A save without the chunk (DOS original, DOS re-save) loads the way
 * DOS does: every table zero.
 *
 * Payload: records of `uint16 id, uint16 version, uint32 len, len bytes`,
 * little-endian. Unknown ids and unexpected versions are skipped, so the
 * tables evolve independently.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/dos_rng.h"

struct ColonizeUnitPool;
struct ColonizeColonyPool;
struct ColonizeWorldMap;

/* 'RTST' */
#define COLONIZE_COL1_EXT_TAG_RUNTIME_STATE 0x54535452u

/* The LCG and the FUN_281f_04ca reseed word (game: move_rng / ai_rng_seed;
 * harness: fx->rng / fx->seed). NULL = the RNG record is neither written nor
 * applied. */
void runtime_ext_set_rng(ColonizeDosRng* rng, uint32_t* seed_word);

/* Freshly malloc'd chunk payload; never NULL unless out of memory. */
uint8_t* runtime_ext_serialize(
  const struct ColonizeUnitPool* units, const struct ColonizeColonyPool* colonies, size_t* out_size
);

/* Apply a payload on top of freshly reset modules. NULL / malformed = nothing
 * applied (DOS from-load semantics). */
void runtime_ext_deserialize(
  struct ColonizeUnitPool* units, struct ColonizeColonyPool* colonies, const struct ColonizeWorldMap* map,
  const uint8_t* data, size_t size
);

/* True when the last deserialize restored the LCG, so the loader must not
 * reseed it (game_apply_col1_save, golden_reload). */
bool runtime_ext_rng_restored(void);

#endif
