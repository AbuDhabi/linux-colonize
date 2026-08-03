#ifndef COLONIZE_DOS_TYPES_H
#define COLONIZE_DOS_TYPES_H

/*
 * Compatibility typedefs mirroring Ghidra decompilation conventions in
 * original_sources_decompiled/viceroy_unpacked.c.
 * These allow incremental extraction of decompiled functions into Linux builds.
 */

#include <stdint.h>

typedef uint8_t undefined;
typedef uint8_t undefined1;
typedef uint16_t undefined2;
typedef uint32_t undefined4;
typedef unsigned int uint;
typedef uint16_t word;
typedef uint8_t byte;
typedef uint8_t bool;

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

/* 16-bit far calling convention is ignored on flat Linux; keep the token as empty. */
#define __cdecl16far

#endif
