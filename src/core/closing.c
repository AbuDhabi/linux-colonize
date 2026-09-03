#include "core/closing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/pik.h"
#include "platform/diagnostics.h"

static const char* const kClosingSheets[CLOSING_SHEET_COUNT] = {
  "CLOS-HAT.SS",
  "CLOS-LDY.SS",
  "CLOS-MAN.SS",
  "CLOS-MIL.SS",
  "CLOS-FWK.SS",
  "CLOS-ROC.SS",
  "CLOS-BEL.SS",
};

static ColonizeClosingSoundFn g_closing_play;
static ColonizeClosingSoundFn g_closing_set_bgm;

void closing_set_sound_hooks(ColonizeClosingSoundFn play_fn, ColonizeClosingSoundFn set_bgm_fn) {
  g_closing_play = play_fn;
  g_closing_set_bgm = set_bgm_fn;
}

static void closing_strip_comment(char* line) {
  if (!line) {
    return;
  }
  char* semi = strchr(line, ';');
  if (semi) {
    *semi = '\0';
  }
}

static int closing_parse_row(const char* line, ClosingSeries* out) {
  if (!line || !out) {
    return 0;
  }
  char buf[COLONIZE_MSG_LINE_LEN];
  snprintf(buf, sizeof(buf), "%s", line);
  closing_strip_comment(buf);
  int series = 0;
  int frame = 0;
  int repeats = 0;
  int base_x = 0;
  int delay = 0;
  const int n = sscanf(buf, "%d , %d , %d , %d , %d", &series, &frame, &repeats, &base_x, &delay);
  if (n < 4) {
    return 0;
  }
  if (n < 5) {
    delay = 0;
  }
  out->series = series;
  out->frame = frame;
  out->repeats = repeats;
  out->base_x = base_x;
  out->delay = delay;
  return 1;
}

int closing_parse_timeline(
  const char* data_dir,
  ClosingSeries* out,
  int out_max,
  int* out_end_frame
) {
  if (out_end_frame) {
    *out_end_frame = 390;
  }
  if (!out || out_max <= 0) {
    return 0;
  }
  ColonizeMsgCatalog cat;
  assets_msg_init(&cat);
  int count = 0;
  int end_frame = 390;
  char path[512];
  if (data_dir &&
      dos_compat_normalize_asset_path(data_dir, "CLOSING.TXT", path, sizeof(path)) &&
      assets_msg_load_file(&cat, path)) {
    const ColonizeMsgSection* sec = assets_msg_find(&cat, "CLOSING");
    if (sec) {
      for (int i = 0; i < sec->line_count; ++i) {
        ClosingSeries row;
        if (!closing_parse_row(sec->lines[i], &row)) {
          continue;
        }
        if (row.series < 0) {
          if (row.frame > 0) {
            end_frame = row.frame;
          }
          break;
        }
        if (row.series == 0 && row.frame == 0 && row.repeats == 0) {
          break;
        }
        if (count < out_max) {
          out[count++] = row;
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

static void closing_blit_anchored(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* fb,
  int base_x
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* s = &sheet->sprites[sprite_index];
  /* FUN_6f30_002e: anchor_x is a horizontal centre, anchor_y a bottom baseline. */
  ss_blit_sprite(
    sheet,
    sprite_index,
    fb,
    s->anchor_x - (s->width >> 1) + base_x,
    s->anchor_y - s->height + 1
  );
}

static int closing_start_tick(const ClosingSeries* s) {
  if (!s) {
    return 1;
  }
  int start = s->frame + s->delay;
  if (start < 1) {
    start = 1;
  }
  return start;
}

static void closing_compose(ClosingCinematic* c) {
  if (!c) {
    return;
  }
  memcpy(c->canvas, c->background, sizeof(c->canvas));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = c->canvas};
  for (int i = 0; i < c->series_count; ++i) {
    const ClosingSeries* s = &c->series[i];
    if (s->series < 0 || s->series >= CLOSING_SHEET_COUNT || !c->sheet_ok[s->series]) {
      continue;
    }
    const ColonizeSpriteSheet* sheet = &c->sheets[s->series];
    const int n = sheet->sprite_count;
    if (n <= 0) {
      continue;
    }
    const int start = closing_start_tick(s);
    const int elapsed = c->clock - start;
    if (elapsed < 0) {
      continue;
    }
    if (s->repeats >= 0 && elapsed >= s->repeats * n) {
      continue;
    }
    closing_blit_anchored(sheet, elapsed % n, &fb, s->base_x);
  }
}

static void closing_play_frame_sounds(const ClosingCinematic* c) {
  if (!c || !g_closing_play) {
    return;
  }
  for (int i = 0; i < c->series_count; ++i) {
    const ClosingSeries* s = &c->series[i];
    if (s->series < 0 || s->series >= CLOSING_SHEET_COUNT || !c->sheet_ok[s->series]) {
      continue;
    }
    const int n = c->sheets[s->series].sprite_count;
    if (n <= 0) {
      continue;
    }
    const int elapsed = c->clock - closing_start_tick(s);
    if (elapsed < 0) {
      continue;
    }
    const int frame = elapsed % n;
    /* `_anim_loop` checks the 1-based counter *before* incrementing, so
     * port elapsed 1/27/37/42 is DOS frame 1/27/37/42. */
    if (s->series == CLOSING_SHEET_FIREWORKS &&
        (frame == 1 || frame == 27 || frame == 37 || frame == 42)) {
      g_closing_play(CLOSING_FIREWORK_SOUND_ID);
    }
    /* `_do_anims` plays 0x5a while drawing hat sprite 1 (after increment). */
    if (s->series == CLOSING_SHEET_HAT && frame == 0) {
      g_closing_play(CLOSING_CHEER_SOUND_ID);
    }
  }
}

static bool closing_step(ClosingCinematic* c, bool play_sounds) {
  if (!c || c->finished) {
    return false;
  }
  c->clock++;
  if (c->end_frame > 0 && c->clock >= c->end_frame) {
    c->clock = c->end_frame;
    closing_compose(c);
    if (play_sounds) {
      closing_play_frame_sounds(c);
    }
    c->finished = true;
    return false;
  }
  closing_compose(c);
  if (play_sounds) {
    closing_play_frame_sounds(c);
  }
  return true;
}

bool closing_open(ClosingCinematic* c, const char* data_dir) {
  if (!c || !data_dir) {
    return false;
  }
  closing_close(c);
  memset(c, 0, sizeof(*c));

  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(data_dir, "CLOS-BKG.PIK", path, sizeof(path))) {
    diag_warn("Closing cinematic: CLOS-BKG.PIK not found.");
    return false;
  }
  ColonizePikImage bg;
  if (!pik_load(path, &bg, err, sizeof(err))) {
    diag_warn("Closing cinematic: CLOS-BKG.PIK failed to load: %s", err);
    return false;
  }
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = c->background};
  memset(c->background, 0, sizeof(c->background));
  pik_blit(&bg, &fb, 0, 0);
  if (bg.has_palette) {
    c->palette = bg.palette;
    c->palette_ok = true;
  }
  pik_free(&bg);

  int loaded = 0;
  for (int i = 0; i < CLOSING_SHEET_COUNT; ++i) {
    if (!dos_compat_normalize_asset_path(data_dir, kClosingSheets[i], path, sizeof(path))) {
      diag_warn("Closing cinematic: %s not found.", kClosingSheets[i]);
      continue;
    }
    if (!ss_load(path, &c->sheets[i], err, sizeof(err))) {
      diag_warn("Closing cinematic: %s failed to load: %s", kClosingSheets[i], err);
      continue;
    }
    c->sheet_ok[i] = true;
    loaded++;
  }
  if (loaded <= 0) {
    diag_warn("Closing cinematic: no CLOS-*.SS sheets loaded.");
    return false;
  }

  c->series_count = closing_parse_timeline(data_dir, c->series, CLOSING_SERIES_MAX, &c->end_frame);
  if (c->series_count <= 0) {
    /* Shipped CLOSING.TXT @CLOSING, used when the catalog is missing. */
    static const ClosingSeries kDefault[] = {
      {4, 1, -1, 0, 0},
      {6, 1, -1, 0, 0},
      {5, 1, -1, 0, 100},
      {0, 1, -1, 0, 16},
      {1, 1, -1, 0, 0},
      {2, 1, -1, 0, 0},
      {3, 1, -1, 0, 0},
    };
    memcpy(c->series, kDefault, sizeof(kDefault));
    c->series_count = (int)(sizeof(kDefault) / sizeof(kDefault[0]));
    c->end_frame = 390;
  }
  if (c->end_frame < 1) {
    c->end_frame = 390;
  }

  memcpy(c->canvas, c->background, sizeof(c->canvas));
  c->clock = 0;
  c->accum_ms = 0;
  c->open = true;
  c->finished = false;
  if (g_closing_set_bgm) {
    g_closing_set_bgm(0); /* CLOSING.EXE is its own process; no VICEROY pool. */
  }
  if (g_closing_play) {
    g_closing_play(CLOSING_BGM_ID);
  }
  return true;
}

void closing_close(ClosingCinematic* c) {
  if (!c) {
    return;
  }
  for (int i = 0; i < CLOSING_SHEET_COUNT; ++i) {
    if (c->sheet_ok[i]) {
      ss_free(&c->sheets[i]);
    }
  }
  memset(c, 0, sizeof(*c));
}

void closing_update(ClosingCinematic* c, uint32_t dt_ms) {
  if (!c || !c->open || c->finished) {
    return;
  }
  c->accum_ms += dt_ms;
  while (c->accum_ms >= CLOSING_FRAME_MS) {
    c->accum_ms -= CLOSING_FRAME_MS;
    if (!closing_step(c, true)) {
      c->accum_ms = 0;
      break;
    }
  }
}

void closing_skip_to_end(ClosingCinematic* c) {
  if (!c || !c->open) {
    return;
  }
  while (closing_step(c, false)) {
  }
  c->finished = true;
}

bool closing_handle_input(ClosingCinematic* c, const ColonizeInputState* input) {
  if (!c || !c->open) {
    return false;
  }
  if (!input) {
    return true;
  }
  const bool pressed =
    input->last_key != COLONIZE_KEY_NONE || input->mouse_left_clicked ||
    input->mouse_right_clicked;
  if (pressed) {
    closing_close(c);
  }
  return true;
}

void closing_render(
  const ClosingCinematic* c,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* palette
) {
  if (!c || !c->open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int w = framebuffer->width < 320 ? framebuffer->width : 320;
  const int h = framebuffer->height < 200 ? framebuffer->height : 200;
  for (int y = 0; y < h; ++y) {
    memcpy(
      framebuffer->pixels + (size_t)y * (size_t)framebuffer->width,
      c->canvas + (size_t)y * 320,
      (size_t)w
    );
  }
  if (palette && c->palette_ok) {
    *palette = c->palette;
  }
}
