#ifndef COLONIZE_CINEMATIC_H
#define COLONIZE_CINEMATIC_H

/*
 * Shared engine for the four standalone cinematics — closing.c, opening.c,
 * declaration.c and woodcut.c. Each of those was a separate DOS executable
 * (CLOSING.EXE, the VICEROY intro, the DECOIND sequence, the woodcut pages)
 * and the port grew four copies of the same four mechanisms: the
 * "<n>,<n>,<n>,<n>[,<n>]" timeline row parser, the FUN_6f30_002e anchored
 * blit, the "load a PIK into a 320x200 canvas and adopt its palette" open
 * step, and the render-time canvas-to-framebuffer copy (audit SC-27..SC-33).
 *
 * Header-only on purpose: the four unit-test targets each link a different
 * slim source list, so a new .c would have to be added to all of them.
 * ss_blit_anchored (ss.c) and str_strip_comment (strutil.c) are in every one
 * of those lists already.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/pik.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

#define CINEMATIC_ROW_FIELDS 5

/*
 * One timeline row: up to five comma-separated integers after the ';' comment
 * is cut. Returns the number of fields sscanf actually matched (0 when the
 * line holds no row). Byte-identical to the per-module parsers it replaced —
 * closing.c read five fields, opening.c four, and a five-field scan yields the
 * same first four either way.
 */
static inline int cinematic_parse_ints(const char* line, int* out, int max_out) {
  if (!line || !out || max_out <= 0) {
    return 0;
  }
  char buf[COLONIZE_MSG_LINE_LEN];
  snprintf(buf, sizeof(buf), "%s", line);
  str_strip_comment(buf);
  int v[CINEMATIC_ROW_FIELDS] = {0, 0, 0, 0, 0};
  const int n = sscanf(buf, "%d , %d , %d , %d , %d", &v[0], &v[1], &v[2], &v[3], &v[4]);
  if (n <= 0) {
    return 0;
  }
  for (int i = 0; i < max_out && i < CINEMATIC_ROW_FIELDS; ++i) {
    out[i] = v[i];
  }
  return n;
}

/*
 * Walk `section` of `file` and hand every animation row to `store`. The two
 * terminators are the DOS ones: a negative series ends the list and, when its
 * second field is positive, sets the end frame; an all-zero series/frame/
 * repeats row also ends it. `default_end` is what the caller gets when the
 * file, the section or the terminator is missing. Returns the row count.
 */
static inline int cinematic_parse_timeline(
  const char* data_dir,
  const char* file,
  const char* section,
  int default_end,
  int out_max,
  void (*store)(void* ctx, int index, const int* v, int n),
  void* ctx,
  int* out_end_frame
) {
  if (out_end_frame) {
    *out_end_frame = default_end;
  }
  if (out_max <= 0 || !store) {
    return 0;
  }
  ColonizeMsgCatalog cat;
  assets_msg_init(&cat);
  int count = 0;
  int end_frame = default_end;
  char path[512];
  if (data_dir && dos_compat_normalize_asset_path(data_dir, file, path, sizeof(path)) &&
      assets_msg_load_file(&cat, path)) {
    const ColonizeMsgSection* sec = assets_msg_find(&cat, section);
    if (sec) {
      for (int i = 0; i < sec->line_count; ++i) {
        int v[CINEMATIC_ROW_FIELDS];
        const int n = cinematic_parse_ints(sec->lines[i], v, CINEMATIC_ROW_FIELDS);
        if (n < 4) {
          continue;
        }
        if (v[0] < 0) {
          if (v[1] > 0) {
            end_frame = v[1];
          }
          break;
        }
        if (v[0] == 0 && v[1] == 0 && v[2] == 0) {
          break;
        }
        if (count < out_max) {
          store(ctx, count, v, n);
          count++;
        }
      }
    }
  }
  assets_msg_free(&cat);
  if (out_end_frame) {
    *out_end_frame = end_frame;
  }
  return count;
}

/* Publish a 320x200 cinematic canvas into the framebuffer, clipping to it. */
static inline void cinematic_blit_canvas320(
  const uint8_t* canvas, ColonizeFramebuffer8* fb
) {
  if (!canvas || !fb || !fb->pixels) {
    return;
  }
  const int w = fb->width < 320 ? fb->width : 320;
  const int h = fb->height < 200 ? fb->height : 200;
  for (int y = 0; y < h; ++y) {
    memcpy(
      fb->pixels + (size_t)y * (size_t)fb->width, canvas + (size_t)y * 320, (size_t)w
    );
  }
}

/*
 * Load `name` into a 320x200 canvas (cleared first) and capture its palette
 * when it carries one. `tag` prefixes the two diagnostics, e.g. "Closing
 * cinematic".
 */
static inline bool cinematic_load_background(
  const char* data_dir,
  const char* name,
  const char* tag,
  uint8_t* canvas320x200,
  ColonizePalette* out_palette,
  bool* out_palette_ok
) {
  if (!canvas320x200) {
    return false;
  }
  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    diag_warn("%s: %s not found.", tag, name);
    return false;
  }
  ColonizePikImage bg;
  if (!pik_load(path, &bg, err, sizeof(err))) {
    diag_warn("%s: %s failed to load: %s", tag, name, err);
    return false;
  }
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = canvas320x200};
  memset(canvas320x200, 0, (size_t)320 * 200);
  pik_blit(&bg, &fb, 0, 0);
  if (bg.has_palette && out_palette) {
    *out_palette = bg.palette;
    if (out_palette_ok) {
      *out_palette_ok = true;
    }
  }
  pik_free(&bg);
  return true;
}

/* ss_load with the two cinematic diagnostics; `tag` prefixes both. */
static inline bool cinematic_load_sheet(
  const char* data_dir, const char* name, ColonizeSpriteSheet* out, const char* tag
) {
  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    diag_warn("%s: %s not found.", tag, name);
    return false;
  }
  if (!ss_load(path, out, err, sizeof(err))) {
    diag_warn("%s: %s failed to load: %s", tag, name, err);
    return false;
  }
  return true;
}

#endif
