#ifndef COLONIZE_DECLARATION_H
#define COLONIZE_DECLARATION_H

#include <stdbool.h>
#include <stdint.h>

#include "core/pik.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * Declaration-of-Independence signing cinematic — DOS FUN_43f7_160a
 * (resident stub thunk_FUN_2a1f_009a, called from FUN_43f7_1a26 right after
 * the declare-year latch and FUN_281f_04ac(3)).
 *
 * DOS shows DECOIND.PIK (the unsigned parchment + quill) and writes the
 * human player's country_name (DS:0x53f6 + slot*0x34 + 0x18, i.e. the
 * "United Colonies" rename 1a26 has just done) across the signature line as
 * an animated quill stroke: one DEC-UPP{A..Z}.SS / DEC-LOW{a..z}.SS sprite
 * sheet per letter, played frame by frame, then DEC-SQIG.SS as the closing
 * flourish. Filenames come from DS:0x12f0 / 0x12f9 / 0x1302 (8-char bases
 * whose last character is overwritten with the letter); the background name
 * is DS:0x12e8 = "DECOIND".
 *
 * DECLARAT.PIK (the *signed* parchment) is referenced by no executable in
 * the shipped game — it is an unused leftover, not this cinematic's art.
 */

#define DECLARATION_SHEET_COUNT 53 /* 26 upper + 26 lower + DEC-SQIG */
#define DECLARATION_SQUIGGLE_SHEET 52
#define DECLARATION_MAX_GLYPHS 48
#define DECLARATION_NAME_MAX 32

/* DOS: x starts at 0x7e, y at 0x94, and the run stops once x reaches 0xdc. */
#define DECLARATION_START_X 0x7e
#define DECLARATION_START_Y 0x94
#define DECLARATION_END_X 0xdc

/*
 * DOS waits 5 ticks of the DS:0x8338/0x833a counter between frames. The
 * game programs PIT channel 0 with divisor 0x7a8 (FUN_0000_a443) and its
 * INT 8 handler (0000:a294) adds 1 per tick, so a tick is
 * 1193182/1960 = 608.77 Hz and 5 ticks = 8.213 ms.
 */
#define DECLARATION_FRAME_US 8213

typedef struct DeclarationGlyph {
  int sheet;  /* index into sheets[]; -1 = blank advance (space / punctuation) */
  int x;
  int y;
  int frames; /* sprites played, starting at sprite 1 (sprite 0 is the empty measure frame) */
} DeclarationGlyph;

typedef struct DeclarationCinematic {
  bool open;
  bool finished; /* every glyph drawn; waiting for a key to dismiss */
  ColonizeSpriteSheet sheets[DECLARATION_SHEET_COUNT];
  bool sheet_ok[DECLARATION_SHEET_COUNT];
  DeclarationGlyph glyphs[DECLARATION_MAX_GLYPHS];
  int glyph_count;
  int glyph_index;
  int frame_index;
  uint32_t accum_us;
  ColonizePalette palette;
  bool palette_ok;
  char name[DECLARATION_NAME_MAX]; /* title-cased signature text actually drawn */
  uint8_t canvas[320 * 200];
} DeclarationCinematic;

/*
 * Load DECOIND.PIK plus the sheets country_name needs, build the glyph run
 * and arm the animation. False (and open == false) when the background or
 * every glyph sheet is missing; the caller then just skips the cinematic.
 */
bool declaration_open(
  DeclarationCinematic* d,
  const char* data_dir,
  const char* country_name
);
void declaration_close(DeclarationCinematic* d);

/* Advance the quill by elapsed wall time; no-op once finished. */
void declaration_update(DeclarationCinematic* d, uint32_t dt_ms);

/* Draw every remaining frame at once (keypress fast-forward / headless). */
void declaration_skip_to_end(DeclarationCinematic* d);

/*
 * Any key or click fast-forwards; once finished it closes. Returns true
 * while the cinematic is open (input consumed).
 */
bool declaration_handle_input(DeclarationCinematic* d, const ColonizeInputState* input);

void declaration_render(
  const DeclarationCinematic* d,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* palette
);

/* Title-case helper (DOS strlwr + word-initial upcase); exposed for tests. */
void declaration_title_case(char* s);

#endif
