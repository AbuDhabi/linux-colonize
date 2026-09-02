#include "core/opening.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/pik.h"
#include "platform/diagnostics.h"

static const char* const kOpeningSheets[OPENING_SHEET_COUNT] = {
  "OPENWND1.SS",
  "OPENSUN.SS",
  "OPENMON1.SS",
  "OPENWND2.SS",
  "OPENMON2.SS",
  "OPENMON3.SS",
  "OPENFISH.SS",
  "OPENGUY.SS",
  "OPENLOGO.SS",
  "OPENBONK.SS",
};

static const char* const kCreditSheets[OPENING_CREDIT_SHEETS] = {
  "OPENCRD1.SS",
  "OPENCRD2.SS",
  "OPENCRD3.SS",
};

static ColonizeOpeningSoundFn g_opening_play;

void opening_set_sound_hooks(ColonizeOpeningSoundFn play_fn, ColonizeOpeningSoundFn set_bgm_fn) {
  g_opening_play = play_fn;
  (void)set_bgm_fn;
}

static void opening_strip_comment(char* line) {
  if (!line) {
    return;
  }
  char* semi = strchr(line, ';');
  if (semi) {
    *semi = '\0';
  }
}

static int opening_parse_series_row(const char* line, OpeningSeries* out) {
  if (!line || !out) {
    return 0;
  }
  char buf[COLONIZE_MSG_LINE_LEN];
  snprintf(buf, sizeof(buf), "%s", line);
  opening_strip_comment(buf);
  int series = 0;
  int frame = 0;
  int repeats = 0;
  int base_x = 0;
  if (sscanf(buf, "%d , %d , %d , %d", &series, &frame, &repeats, &base_x) < 4) {
    return 0;
  }
  out->series = series;
  out->frame = frame;
  out->repeats = repeats;
  out->base_x = base_x;
  return 1;
}

static int opening_parse_credit_row(const char* line, OpeningCredit* out) {
  if (!line || !out) {
    return 0;
  }
  char buf[COLONIZE_MSG_LINE_LEN];
  snprintf(buf, sizeof(buf), "%s", line);
  opening_strip_comment(buf);
  int start_frame = 0;
  int end_frame = 0;
  int series = 0;
  int sprite = 0;
  if (sscanf(buf, "%d , %d , %d , %d", &start_frame, &end_frame, &series, &sprite) < 4) {
    return 0;
  }
  if (sprite > 0) {
    sprite--;
  }
  out->start_frame = start_frame;
  out->end_frame = end_frame;
  out->series = series;
  out->sprite = sprite;
  return 1;
}

int opening_parse_timeline(
  const char* data_dir,
  OpeningSeries* out,
  int out_max,
  int* out_end_frame
) {
  if (out_end_frame) {
    *out_end_frame = 891;
  }
  if (!out || out_max <= 0) {
    return 0;
  }
  ColonizeMsgCatalog cat;
  assets_msg_init(&cat);
  int count = 0;
  int end_frame = 891;
  char path[512];
  if (data_dir &&
      dos_compat_normalize_asset_path(data_dir, "OPENING.TXT", path, sizeof(path)) &&
      assets_msg_load_file(&cat, path)) {
    const ColonizeMsgSection* sec = assets_msg_find(&cat, "OPENING");
    if (sec) {
      for (int i = 0; i < sec->line_count; ++i) {
        OpeningSeries row;
        if (!opening_parse_series_row(sec->lines[i], &row)) {
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

int opening_parse_credits(const char* data_dir, OpeningCredit* out, int out_max) {
  if (!out || out_max <= 0) {
    return 0;
  }
  ColonizeMsgCatalog cat;
  assets_msg_init(&cat);
  int count = 0;
  char path[512];
  if (data_dir &&
      dos_compat_normalize_asset_path(data_dir, "OPENING.TXT", path, sizeof(path)) &&
      assets_msg_load_file(&cat, path)) {
    const ColonizeMsgSection* sec = assets_msg_find(&cat, "CREDITS");
    if (sec) {
      for (int i = 0; i < sec->line_count; ++i) {
        OpeningCredit row;
        if (!opening_parse_credit_row(sec->lines[i], &row)) {
          continue;
        }
        if (count < out_max) {
          out[count++] = row;
        }
      }
    }
  }
  assets_msg_free(&cat);
  return count;
}

int opening_parse_path(const char* data_dir, int* xs, int* ys, int out_max) {
  if (!xs || !ys || out_max <= 0 || !data_dir) {
    return 0;
  }
  char path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "PATH.DAT", path, sizeof(path))) {
    return 0;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    return 0;
  }
  int count = 0;
  char line[64];
  while (count < out_max && fgets(line, (int)sizeof(line), f)) {
    int x = 0;
    int y = 0;
    if (sscanf(line, "%d , %d", &x, &y) < 2 && sscanf(line, "%d,%d", &x, &y) < 2) {
      continue;
    }
    xs[count] = x;
    ys[count] = y;
    count++;
  }
  fclose(f);
  return count;
}

static void opening_blit_anchored(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* fb,
  int add_x
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* s = &sheet->sprites[sprite_index];
  ss_blit_sprite(
    sheet, sprite_index, fb, s->anchor_x - (s->width >> 1) + add_x, s->anchor_y - s->height + 1
  );
}

static void opening_ship_at(const OpeningCinematic* o, int* x, int* y) {
  if (!o || o->path_count <= 0) {
    if (x) {
      *x = 868;
    }
    if (y) {
      *y = 89;
    }
    return;
  }
  int idx = o->clock > 0 ? o->clock - 1 : 0;
  if (idx >= o->path_count) {
    idx = o->path_count - 1;
  }
  if (x) {
    *x = o->path_x[idx];
  }
  if (y) {
    *y = o->path_y[idx];
  }
}

static int opening_ship_end_clock(const OpeningCinematic* o) {
  int end = o && o->path_count > 0 ? o->path_count : 0;
  if (!o) {
    return end;
  }
  for (int i = 0; i < o->series_count; ++i) {
    if (o->series[i].series == OPENING_SHEET_BONK && o->series[i].frame > 0 &&
        (end <= 0 || o->series[i].frame < end)) {
      end = o->series[i].frame;
    }
  }
  return end;
}

static int opening_camera_x(const OpeningCinematic* o) {
  int ship_x = 0;
  opening_ship_at(o, &ship_x, NULL);
  int camera = ship_x - (OPENING_VIEW_W / 2);
  if (camera < 0) {
    camera = 0;
  }
  if (camera > OPENING_CAMERA_MAX) {
    camera = OPENING_CAMERA_MAX;
  }
  return camera;
}

static void opening_blit_scene(OpeningCinematic* o, ColonizeFramebuffer8* fb, int camera) {
  if (!o || !o->scene_ok || !fb || !fb->pixels) {
    return;
  }
  for (int y = 0; y < OPENING_SCENE_H; ++y) {
    int fy = OPENING_SCENE_Y + y;
    if (fy < 0 || fy >= fb->height) {
      continue;
    }
    for (int x = 0; x < OPENING_VIEW_W; ++x) {
      int sx = camera + x;
      if (sx < 0 || sx >= OPENING_WORLD_W) {
        continue;
      }
      if (x >= fb->width) {
        continue;
      }
      fb->pixels[fy * fb->width + x] = o->scene[y * OPENING_WORLD_W + sx];
    }
  }
}

static void opening_blit_border(const OpeningCinematic* o, ColonizeFramebuffer8* fb) {
  if (!o || !o->border_ok || !fb || !fb->pixels) {
    return;
  }
  const int w = fb->width < 320 ? fb->width : 320;
  const int h = fb->height < 200 ? fb->height : 200;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint8_t p = o->border[y * 320 + x];
      if (p != 0) {
        fb->pixels[y * fb->width + x] = p;
      }
    }
  }
}

static int opening_sprite_index(const OpeningSeries* s, int elapsed, int n) {
  if (!s || n <= 0 || elapsed < 0) {
    return -1;
  }
  if (s->repeats > 0) {
    if (elapsed >= s->repeats * n) {
      return -1;
    }
    return elapsed % n;
  }
  return elapsed < n ? elapsed : n - 1;
}

static void opening_compose(OpeningCinematic* o);

static void opening_begin_sailing(OpeningCinematic* o) {
  if (!o) {
    return;
  }
  o->logo_phase = false;
  o->clock = 0;
  o->accum_ms = 0;
  o->skip_presses = 0;
  opening_compose(o);
}

static void opening_advance_logo_frames(OpeningCinematic* o) {
  if (!o) {
    return;
  }
  o->logo_frame++;
  if (o->mps_logo_ok && o->logo_frame >= o->mps_logo.sprite_count) {
    o->logo_frame = o->mps_logo.sprite_count > 1 ? 1 : 0;
  }
  if (o->logo_clock >= OPENING_LOGO_NAME_FRAME && o->mps_name_ok) {
    o->logo_name_frame++;
    if (o->logo_name_frame >= o->mps_name.sprite_count) {
      o->logo_name_frame = o->mps_name.sprite_count - 1;
    }
  }
}

static void opening_compose_logo(OpeningCinematic* o) {
  if (!o) {
    return;
  }
  memset(o->canvas, 0, sizeof(o->canvas));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = o->canvas};
  if (o->mps_logo_ok && o->mps_logo.sprite_count > 0) {
    int idx = o->logo_frame;
    if (idx < 0) {
      idx = 0;
    }
    if (idx >= o->mps_logo.sprite_count) {
      idx = o->mps_logo.sprite_count - 1;
    }
    opening_blit_anchored(&o->mps_logo, idx, &fb, 0);
  }
  if (o->logo_clock >= OPENING_LOGO_NAME_FRAME && o->mps_name_ok &&
      o->mps_name.sprite_count > 0) {
    int idx = o->logo_name_frame;
    if (idx < 0) {
      idx = 0;
    }
    if (idx >= o->mps_name.sprite_count) {
      idx = o->mps_name.sprite_count - 1;
    }
    opening_blit_anchored(&o->mps_name, idx, &fb, 0);
  }
}

static bool opening_logo_step(OpeningCinematic* o) {
  if (!o || !o->logo_phase) {
    return false;
  }
  o->logo_clock++;
  if (o->logo_clock > OPENING_LOGO_END_FRAME) {
    opening_begin_sailing(o);
    return false;
  }
  opening_compose_logo(o);
  opening_advance_logo_frames(o);
  return true;
}

static void opening_compose(OpeningCinematic* o) {
  if (!o) {
    return;
  }
  if (o->logo_phase) {
    opening_compose_logo(o);
    return;
  }
  memset(o->canvas, 0, sizeof(o->canvas));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = o->canvas};
  const int camera = opening_camera_x(o);
  opening_blit_scene(o, &fb, camera);

  for (int i = 0; i < o->series_count; ++i) {
    const OpeningSeries* s = &o->series[i];
    if (s->series < 0 || s->series >= OPENING_SHEET_COUNT || !o->sheet_ok[s->series]) {
      continue;
    }
    const ColonizeSpriteSheet* sheet = &o->sheets[s->series];
    const int n = sheet->sprite_count;
    if (n <= 0) {
      continue;
    }
    const int elapsed = o->clock - s->frame;
    const int idx = opening_sprite_index(s, elapsed, n);
    if (idx < 0) {
      continue;
    }
    opening_blit_anchored(sheet, idx, &fb, s->base_x - camera);
  }

  if (o->ship_ok && o->ship.sprite_count > 0 && o->clock < opening_ship_end_clock(o)) {
    int sx = 0;
    int sy = 0;
    opening_ship_at(o, &sx, &sy);
    int spr = 0;
    if (o->clock > 0) {
      spr = (o->clock - 1) % o->ship.sprite_count;
    }
    const ColonizeSprite* sh = &o->ship.sprites[spr];
    /* PATH.DAT is the sprite centre in world/screen space, not a FUN_6f30
     * baseline. Using height-1 as a bottom anchor parked the hull ~12 px
     * above OPENBONK's still (anchor 123 / dest y 102 vs path y 114).
     * OPENING_SHIP_X_ALIGN parks the padded 47-wide sheet on the 33-wide
     * still (dest x 141 vs path x 161). OPENSHIP stops when OPENBONK
     * starts (frame 701); drawing both left two hulls for the landfall. */
    ss_blit_sprite(
      &o->ship,
      spr,
      &fb,
      sx - camera - (sh->width >> 1) + OPENING_SHIP_X_ALIGN,
      sy - (sh->height >> 1)
    );
  }

  for (int i = 0; i < o->credit_count; ++i) {
    const OpeningCredit* c = &o->credit_rows[i];
    if (o->clock < c->start_frame || o->clock > c->end_frame) {
      continue;
    }
    if (c->series < 0 || c->series >= OPENING_CREDIT_SHEETS || !o->credit_ok[c->series]) {
      continue;
    }
    opening_blit_anchored(&o->credits[c->series], c->sprite, &fb, 0);
  }

  opening_blit_border(o, &fb);
}

static bool opening_step(OpeningCinematic* o) {
  if (!o || o->finished) {
    return false;
  }
  o->clock++;
  if (o->end_frame > 0 && o->clock >= o->end_frame) {
    o->clock = o->end_frame;
    opening_compose(o);
    return false;
  }
  opening_compose(o);
  return true;
}

static bool opening_load_pik_rect(
  const char* data_dir,
  const char* name,
  uint8_t* dest,
  int dest_w,
  int dest_h,
  ColonizePalette* pal,
  bool* pal_ok
) {
  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    return false;
  }
  ColonizePikImage img;
  if (!pik_load(path, &img, err, sizeof(err))) {
    diag_warn("Opening cinematic: %s failed: %s", name, err);
    return false;
  }
  const int w = img.width < dest_w ? img.width : dest_w;
  const int h = img.height < dest_h ? img.height : dest_h;
  memset(dest, 0, (size_t)dest_w * (size_t)dest_h);
  for (int y = 0; y < h; ++y) {
    memcpy(dest + (size_t)y * (size_t)dest_w, img.pixels + (size_t)y * (size_t)img.width, (size_t)w);
  }
  if (pal && pal_ok && img.has_palette && !*pal_ok) {
    *pal = img.palette;
    *pal_ok = true;
  }
  pik_free(&img);
  return true;
}

bool opening_open(OpeningCinematic* o, const char* data_dir) {
  if (!o || !data_dir) {
    return false;
  }
  opening_close(o);
  memset(o, 0, sizeof(*o));

  if (!opening_load_pik_rect(
        data_dir, "OPENING.PIK", o->scene, OPENING_WORLD_W, OPENING_SCENE_H, &o->palette,
        &o->palette_ok
      )) {
    return false;
  }
  o->scene_ok = true;
  o->border_ok = opening_load_pik_rect(
    data_dir, "OPENBORD.PIK", o->border, 320, 200, &o->palette, &o->palette_ok
  );

  char path[512];
  char err[256];
  int loaded = 0;
  for (int i = 0; i < OPENING_SHEET_COUNT; ++i) {
    if (!dos_compat_normalize_asset_path(data_dir, kOpeningSheets[i], path, sizeof(path))) {
      continue;
    }
    if (!ss_load(path, &o->sheets[i], err, sizeof(err))) {
      diag_warn("Opening cinematic: %s failed: %s", kOpeningSheets[i], err);
      continue;
    }
    o->sheet_ok[i] = true;
    loaded++;
  }
  if (dos_compat_normalize_asset_path(data_dir, "OPENSHIP.SS", path, sizeof(path)) &&
      ss_load(path, &o->ship, err, sizeof(err))) {
    o->ship_ok = true;
    loaded++;
  }
  if (dos_compat_normalize_asset_path(data_dir, "MPSLOGO.SS", path, sizeof(path)) &&
      ss_load(path, &o->mps_logo, err, sizeof(err))) {
    o->mps_logo_ok = true;
  }
  if (dos_compat_normalize_asset_path(data_dir, "MPSNAME.SS", path, sizeof(path)) &&
      ss_load(path, &o->mps_name, err, sizeof(err))) {
    o->mps_name_ok = true;
  }
  for (int i = 0; i < OPENING_CREDIT_SHEETS; ++i) {
    if (!dos_compat_normalize_asset_path(data_dir, kCreditSheets[i], path, sizeof(path))) {
      continue;
    }
    if (ss_load(path, &o->credits[i], err, sizeof(err))) {
      o->credit_ok[i] = true;
    }
  }
  if (loaded <= 0) {
    diag_warn("Opening cinematic: no OPEN*.SS sheets loaded.");
    return false;
  }

  o->series_count = opening_parse_timeline(data_dir, o->series, OPENING_SERIES_MAX, &o->end_frame);
  if (o->series_count <= 0) {
    static const OpeningSeries kDefault[] = {
      {0, 78, 1, 640},
      {0, 97, 1, 640},
      {1, 40, 0, 640},
      {2, 200, 2, 320},
      {3, 248, 1, 320},
      {4, 255, 1, 320},
      {5, 485, 1, 320},
      {6, 502, 3, 320},
      {9, 701, 0, 0},
      {7, 720, 0, 0},
      {8, 767, 0, 0},
    };
    memcpy(o->series, kDefault, sizeof(kDefault));
    o->series_count = (int)(sizeof(kDefault) / sizeof(kDefault[0]));
    o->end_frame = 891;
  }
  if (o->end_frame < 1) {
    o->end_frame = 891;
  }
  o->credit_count = opening_parse_credits(data_dir, o->credit_rows, OPENING_CREDIT_MAX);
  o->path_count = opening_parse_path(data_dir, o->path_x, o->path_y, OPENING_PATH_MAX);

  o->clock = 0;
  o->accum_ms = 0;
  o->skip_presses = 0;
  o->open = true;
  o->finished = false;
  o->logo_phase = o->mps_logo_ok;
  opening_compose(o);
  if (g_opening_play) {
    g_opening_play(OPENING_BGM_ID);
  }
  return true;
}

void opening_close(OpeningCinematic* o) {
  if (!o) {
    return;
  }
  for (int i = 0; i < OPENING_SHEET_COUNT; ++i) {
    if (o->sheet_ok[i]) {
      ss_free(&o->sheets[i]);
    }
  }
  if (o->ship_ok) {
    ss_free(&o->ship);
  }
  if (o->mps_logo_ok) {
    ss_free(&o->mps_logo);
  }
  if (o->mps_name_ok) {
    ss_free(&o->mps_name);
  }
  for (int i = 0; i < OPENING_CREDIT_SHEETS; ++i) {
    if (o->credit_ok[i]) {
      ss_free(&o->credits[i]);
    }
  }
  memset(o, 0, sizeof(*o));
}

void opening_update(OpeningCinematic* o, uint32_t dt_ms) {
  if (!o || !o->open || o->finished) {
    return;
  }
  if (o->logo_phase) {
    o->accum_ms += dt_ms;
    while (o->accum_ms >= OPENING_FRAME_MS) {
      o->accum_ms -= OPENING_FRAME_MS;
      if (!opening_logo_step(o)) {
        break;
      }
    }
    if (o->logo_phase) {
      return;
    }
  } else if (o->end_frame > 0 && o->clock >= o->end_frame) {
    o->hold_ms += dt_ms;
    if (o->hold_ms >= OPENING_HOLD_MS) {
      o->finished = true;
    }
    return;
  } else {
    o->accum_ms += dt_ms;
  }
  while (o->accum_ms >= OPENING_FRAME_MS) {
    o->accum_ms -= OPENING_FRAME_MS;
    if (!opening_step(o)) {
      o->accum_ms = 0;
      break;
    }
  }
}

bool opening_handle_input(OpeningCinematic* o, const ColonizeInputState* input) {
  if (!o || !o->open) {
    return false;
  }
  if (!input) {
    return true;
  }
  const bool pressed =
    input->last_key != COLONIZE_KEY_NONE || input->mouse_left_clicked ||
    input->mouse_right_clicked;
  if (pressed) {
    if (o->logo_phase) {
      opening_begin_sailing(o);
      return true;
    }
    if (o->end_frame > 0 && o->clock >= o->end_frame) {
      return true;
    }
    o->skip_presses++;
    if (o->skip_presses >= 2) {
      o->clock = o->end_frame;
      opening_compose(o);
      o->accum_ms = 0;
      o->hold_ms = 0;
    }
  }
  return true;
}

void opening_render(
  const OpeningCinematic* o,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* palette
) {
  if (!o || !o->open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int w = framebuffer->width < 320 ? framebuffer->width : 320;
  const int h = framebuffer->height < 200 ? framebuffer->height : 200;
  for (int y = 0; y < h; ++y) {
    memcpy(
      framebuffer->pixels + (size_t)y * (size_t)framebuffer->width,
      o->canvas + (size_t)y * 320,
      (size_t)w
    );
  }
  if (palette) {
    if (o->logo_phase && o->mps_logo_ok && o->mps_logo.has_palette) {
      *palette = o->mps_logo.palette;
    } else if (o->palette_ok) {
      *palette = o->palette;
    }
  }
}
