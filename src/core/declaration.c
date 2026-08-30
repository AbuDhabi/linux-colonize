#include "core/declaration.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "platform/diagnostics.h"

/*
 * Per-glyph frame counts are DOS literals (FUN_43f7_160a local_520), not a
 * function of the sheet: 10 for an upper-case letter or the flourish, 7 for
 * a lower-case one. They line up with the shipped art — DEC-UPP*.SS and
 * DEC-SQIG.SS hold 11 sprites, DEC-LOW*.SS 8, and sprite 0 of every sheet
 * is empty (it exists only to carry the advance width, read at +0x4a of the
 * loaded sheet = record 1's width in DOS's in-memory layout).
 */
#define DECLARATION_FRAMES_UPPER 10
#define DECLARATION_FRAMES_LOWER 7

/* DOS local_5a: the signature slants up as it is written. */
#define DECLARATION_DY_UPPER (-3)
#define DECLARATION_DY_LOWER (-2)
#define DECLARATION_DY_GAP (-1)
#define DECLARATION_DY_SQUIGGLE (-4)

/* DOS local_6 for a space / punctuation cell (no sprite drawn). */
#define DECLARATION_GAP_ADVANCE 3

void declaration_title_case(char* s) {
  if (!s) {
    return;
  }
  /* FUN_1d1d_0d46 = strlwr. */
  for (char* p = s; *p; ++p) {
    if (*p >= 'A' && *p <= 'Z') {
      *p = (char)(*p + 0x20);
    }
  }
  /* Then upper-case every word-initial letter (DOS bVar2 word-boundary run). */
  bool at_word_start = true;
  for (char* p = s; *p; ++p) {
    if (!isalpha((unsigned char)*p)) {
      at_word_start = true;
      continue;
    }
    if (at_word_start && islower((unsigned char)*p)) {
      *p = (char)(*p - 0x20);
    }
    at_word_start = false;
  }
}

static bool declaration_load_sheet(
  DeclarationCinematic* d,
  const char* data_dir,
  int slot
) {
  if (slot < 0 || slot >= DECLARATION_SHEET_COUNT) {
    return false;
  }
  if (d->sheet_ok[slot]) {
    return true;
  }
  /*
   * DS:0x12f0 "DEC-UPP0" / DS:0x12f9 "DEC-LOW0" / DS:0x1302 "DEC-SQIG" —
   * the upper/lower bases have their 8th character overwritten with the
   * letter ('A'+i and 'a'+i respectively) before the load.
   */
  char name[16];
  if (slot == DECLARATION_SQUIGGLE_SHEET) {
    snprintf(name, sizeof(name), "DEC-SQIG.SS");
  } else if (slot < 26) {
    snprintf(name, sizeof(name), "DEC-UPP%c.SS", 'A' + slot);
  } else {
    snprintf(name, sizeof(name), "DEC-LOW%c.SS", 'A' + (slot - 26));
  }
  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    diag_warn("Declaration cinematic: %s not found.", name);
    return false;
  }
  if (!ss_load(path, &d->sheets[slot], err, sizeof(err))) {
    diag_warn("Declaration cinematic: %s failed to load: %s", name, err);
    return false;
  }
  d->sheet_ok[slot] = true;
  return true;
}

static void declaration_push_glyph(
  DeclarationCinematic* d,
  int sheet,
  int x,
  int y,
  int frames
) {
  if (d->glyph_count >= DECLARATION_MAX_GLYPHS) {
    return;
  }
  DeclarationGlyph* g = &d->glyphs[d->glyph_count++];
  g->sheet = sheet;
  g->x = x;
  g->y = y;
  g->frames = frames;
}

static int declaration_sheet_advance(const DeclarationCinematic* d, int sheet) {
  if (sheet < 0 || !d->sheet_ok[sheet] || d->sheets[sheet].sprite_count <= 0) {
    return 0;
  }
  /* DOS reads the width off record 1; every sprite in these sheets is the
   * same size, so sprite 0's width is the same number. */
  return d->sheets[sheet].sprites[0].width;
}

static int declaration_clamp_frames(const DeclarationCinematic* d, int sheet, int frames) {
  if (sheet < 0 || !d->sheet_ok[sheet]) {
    return 0;
  }
  const int available = d->sheets[sheet].sprite_count - 1;
  return frames < available ? frames : (available > 0 ? available : 0);
}

static void declaration_build_run(DeclarationCinematic* d, const char* data_dir) {
  int x = DECLARATION_START_X;
  int y = DECLARATION_START_Y;
  d->glyph_count = 0;
  for (const char* p = d->name; *p != '\0'; ++p) {
    const unsigned char c = (unsigned char)*p;
    int sheet = -1;
    int frames = 0;
    int dy;
    int advance = 0;
    bool stop = false;

    if (x >= DECLARATION_END_X) {
      /* Ran out of signature line: close with the flourish and stop. */
      sheet = DECLARATION_SQUIGGLE_SHEET;
      frames = DECLARATION_FRAMES_UPPER;
      dy = DECLARATION_DY_SQUIGGLE;
      stop = true;
    } else if (isspace(c) || ispunct(c)) {
      advance = DECLARATION_GAP_ADVANCE;
      dy = DECLARATION_DY_GAP;
    } else if (isupper(c)) {
      sheet = c - 'A';
      frames = DECLARATION_FRAMES_UPPER;
      dy = DECLARATION_DY_UPPER;
    } else if (islower(c)) {
      sheet = 26 + (c - 'a');
      frames = DECLARATION_FRAMES_LOWER;
      dy = DECLARATION_DY_LOWER;
    } else {
      /* Digits and controls: DOS falls straight through to the flourish. */
      sheet = DECLARATION_SQUIGGLE_SHEET;
      frames = DECLARATION_FRAMES_UPPER;
      dy = DECLARATION_DY_SQUIGGLE;
      stop = true;
    }

    if (sheet >= 0) {
      if (!declaration_load_sheet(d, data_dir, sheet)) {
        /* DOS aborts the whole animation on a failed load; skip the glyph. */
        continue;
      }
      frames = declaration_clamp_frames(d, sheet, frames);
      advance = declaration_sheet_advance(d, sheet);
    }
    declaration_push_glyph(d, sheet, x, y, frames);
    x += advance;
    y += dy;
    if (stop) {
      break;
    }
  }
}

bool declaration_open(
  DeclarationCinematic* d,
  const char* data_dir,
  const char* country_name
) {
  if (!d || !data_dir) {
    return false;
  }
  declaration_close(d); /* free sheets if a previous run is still resident */
  memset(d, 0, sizeof(*d));

  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(data_dir, "DECOIND.PIK", path, sizeof(path))) {
    diag_warn("Declaration cinematic: DECOIND.PIK not found.");
    return false;
  }
  ColonizePikImage bg;
  if (!pik_load(path, &bg, err, sizeof(err))) {
    diag_warn("Declaration cinematic: DECOIND.PIK failed to load: %s", err);
    return false;
  }
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = d->canvas};
  memset(d->canvas, 0, sizeof(d->canvas));
  pik_blit(&bg, &fb, 0, 0);
  if (bg.has_palette) {
    d->palette = bg.palette;
    d->palette_ok = true;
  }
  pik_free(&bg);

  snprintf(d->name, sizeof(d->name), "%s", country_name ? country_name : "");
  declaration_title_case(d->name);
  declaration_build_run(d, data_dir);

  d->open = true;
  d->finished = (d->glyph_count == 0);
  d->glyph_index = 0;
  d->frame_index = 0;
  d->accum_us = 0;
  return true;
}

void declaration_close(DeclarationCinematic* d) {
  if (!d) {
    return;
  }
  for (int i = 0; i < DECLARATION_SHEET_COUNT; ++i) {
    if (d->sheet_ok[i]) {
      ss_free(&d->sheets[i]);
      d->sheet_ok[i] = false;
    }
  }
  d->open = false;
  d->finished = false;
  d->glyph_count = 0;
  d->glyph_index = 0;
  d->frame_index = 0;
}

/* Draw one animation frame of the current glyph; returns false at the end. */
static bool declaration_step(DeclarationCinematic* d) {
  while (d->glyph_index < d->glyph_count) {
    const DeclarationGlyph* g = &d->glyphs[d->glyph_index];
    if (g->sheet < 0 || d->frame_index >= g->frames) {
      d->glyph_index++;
      d->frame_index = 0;
      continue;
    }
    ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = d->canvas};
    /* DOS passes local_11c + 2 as the in-memory record index; that array is
     * offset one ahead of the on-disk sprite list, so the drawn sprites are
     * 1 .. frames and sprite 0 (empty) is never blitted. */
    ss_blit_sprite(&d->sheets[g->sheet], d->frame_index + 1, &fb, g->x, g->y);
    d->frame_index++;
    if (d->frame_index >= g->frames) {
      d->glyph_index++;
      d->frame_index = 0;
    }
    return true;
  }
  d->finished = true;
  return false;
}

void declaration_update(DeclarationCinematic* d, uint32_t dt_ms) {
  if (!d || !d->open || d->finished) {
    return;
  }
  d->accum_us += dt_ms * 1000u;
  while (d->accum_us >= DECLARATION_FRAME_US) {
    d->accum_us -= DECLARATION_FRAME_US;
    if (!declaration_step(d)) {
      d->accum_us = 0;
      break;
    }
  }
}

void declaration_skip_to_end(DeclarationCinematic* d) {
  if (!d || !d->open) {
    return;
  }
  /* Frames are cumulative strokes, but replay them all so the result is
   * byte-identical to letting the animation run out. */
  while (declaration_step(d)) {
  }
  d->finished = true;
}

bool declaration_handle_input(DeclarationCinematic* d, const ColonizeInputState* input) {
  if (!d || !d->open) {
    return false;
  }
  if (!input) {
    return true;
  }
  const bool pressed =
    input->last_key != COLONIZE_KEY_NONE || input->mouse_left_clicked ||
    input->mouse_right_clicked;
  if (!pressed) {
    return true;
  }
  if (!d->finished) {
    declaration_skip_to_end(d);
  } else {
    declaration_close(d);
  }
  return true;
}

void declaration_render(
  const DeclarationCinematic* d,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* palette
) {
  if (!d || !d->open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int w = framebuffer->width < 320 ? framebuffer->width : 320;
  const int h = framebuffer->height < 200 ? framebuffer->height : 200;
  for (int y = 0; y < h; ++y) {
    memcpy(
      framebuffer->pixels + (size_t)y * (size_t)framebuffer->width,
      d->canvas + (size_t)y * 320,
      (size_t)w
    );
  }
  if (palette && d->palette_ok) {
    *palette = d->palette;
  }
}
