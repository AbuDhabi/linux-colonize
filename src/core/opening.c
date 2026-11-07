#include "core/opening.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/cinematic.h"
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
static ColonizeOpeningSoundFn g_opening_set_bgm;

void opening_set_sound_hooks(ColonizeOpeningSoundFn play_fn, ColonizeOpeningSoundFn set_bgm_fn) {
  g_opening_play = play_fn;
  g_opening_set_bgm = set_bgm_fn;
}

/* OPENING.TXT @OPENING row: series, frame, repeats, baseX. */
static void opening_store_row(void* ctx, int index, const int* v, int n) {
  (void)n;
  OpeningSeries* out = (OpeningSeries*)ctx;
  out[index].series = v[0];
  out[index].frame = v[1];
  out[index].repeats = v[2];
  out[index].base_x = v[3];
}

int opening_parse_timeline(
  const char* data_dir,
  OpeningSeries* out,
  int out_max,
  int* out_end_frame
) {
  if (out_end_frame) {
    *out_end_frame = 0; /* no end row in the TXT = 0, never a typed copy */
  }
  if (!out || out_max <= 0) {
    return 0;
  }
  return cinematic_parse_timeline(
    data_dir, "OPENING.TXT", "OPENING", 0, out_max, opening_store_row, out, out_end_frame
  );
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
        int v[CINEMATIC_ROW_FIELDS];
        if (cinematic_parse_ints(sec->lines[i], v, CINEMATIC_ROW_FIELDS) < 4) {
          continue;
        }
        if (count < out_max) {
          /* start_frame, end_frame, series, sprite — the sprite column is
           * 1-based in the file. */
          out[count].start_frame = v[0];
          out[count].end_frame = v[1];
          out[count].series = v[2];
          out[count].sprite = (v[3] > 0) ? v[3] - 1 : v[3];
          count++;
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
    if (sscanf(line, "%d , %d", &x, &y) < 2) { /* " , " also matches "," */
      continue;
    }
    xs[count] = x;
    ys[count] = y;
    count++;
  }
  fclose(f);
  return count;
}

/* MPSLOGO.SS is 155×119 with FUN_6f30 dest y = 0. Shift so the spin sits on
 * the 200-tall screen; MPSNAME keeps the same offset so the unroll stays
 * attached to the badge. */
static int opening_logo_y_shift(const OpeningCinematic* o) {
  if (!o || !o->mps_logo_ok || o->mps_logo.sprite_count <= 0) {
    return 0;
  }
  const ColonizeSprite* s = &o->mps_logo.sprites[0];
  const int dest_y = s->anchor_y - s->height + 1;
  return (200 - s->height) / 2 - dest_y;
}

/* OPENCRD*.SS packed the banners as a vertical contact sheet; the stored
 * anchor_y is the row in that sheet, not the playback slot. Every plate
 * sits in the OPENBORD bottom panel, below the inner bevel so it does not
 * overlap the map frame. X is centred on the 320-wide screen; the stored
 * anchor_x is the contact-sheet column, not a playback dest. */
static void opening_blit_credit(
  const ColonizeSpriteSheet* sheet,
  int sprite_index,
  ColonizeFramebuffer8* fb
) {
  if (!sheet || sprite_index < 0 || sprite_index >= sheet->sprite_count || !fb) {
    return;
  }
  const ColonizeSprite* s = &sheet->sprites[sprite_index];
  const int panel = OPENING_CREDIT_TOP + OPENING_CREDIT_BORDER;
  const int strip = 200 - panel;
  const int dest_x = (OPENING_VIEW_W - s->width) / 2;
  const int dest_y = panel + (strip - s->height) / 2;
  ss_blit_sprite(sheet, sprite_index, fb, dest_x, dest_y);
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

/* OPENING.EXE pan_x DS:0x4aca: `_open_loop` 0xaaf seeds 0x280, `_pan`
 * 0x56a-0x571 decrements it once per scene step while non-zero, and
 * `_jump_to_logo` 0x8be zeroes it. The scene clock is the step count, so
 * pan_x = 0x280 - clock floored at 0 (the jump lands past 0x280, where that
 * is already 0). The ship never moves it. */
int opening_camera_x(const OpeningCinematic* o) {
  return o->clock < OPENING_PAN_START ? OPENING_PAN_START - o->clock : 0;
}

/* Scene step the ship was last advanced on. `_jump_to_logo` leaves ship_wave
 * DS:0x7a alone and ship_off DS:0x7c unset, so after a skip the ship keeps the
 * sprite of the skip step (its x is the path end: ship_at = path_count - 1). */
static int opening_ship_step(const OpeningCinematic* o) {
  return o->skip_from >= 0 ? o->skip_from : o->clock;
}

static void opening_draw_ship(OpeningCinematic* o, ColonizeFramebuffer8* fb, int camera) {
  const int step = opening_ship_step(o);
  if (!o->ship_ok || o->ship.sprite_count <= 0 || step >= opening_ship_end_clock(o)) {
    return;
  }
  int sx = 0;
  int sy = 0;
  opening_ship_at(o, &sx, &sy);
  /* ship_wave starts at 1 (DS:0x7a) and `_ship_move` 0x5c6 bumps it before
   * the draw, wrapping past the sprite count to 1: 0-based step % count. */
  const int spr = step % o->ship.sprite_count;
  /* `_do_ship` 0x37e-0x3a3: dest = (x - w/4 - pan_x, y - h/2) with w/h of the
   * sheet's first sprite (+0x4a/+0x4c). PATH.DAT's last point then puts the
   * hull on OPENBONK's first sprite (dest x 150). OPENSHIP stops when OPENBONK
   * starts (frame 701, ship_off at `_anim_loop` 0x78b). */
  const ColonizeSprite* sh = &o->ship.sprites[0];
  ss_blit_sprite(&o->ship, spr, fb, sx - (sh->width >> 2) - camera, sy - (sh->height >> 1));
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

int opening_sprite_index(const OpeningSeries* s, int elapsed, int n) {
  if (!s || n <= 0 || elapsed < 0) {
    return -1;
  }
  if (s->repeats > 0 && elapsed >= s->repeats * n) {
    return -1;
  }
  if (s->series == OPENING_SHEET_SUN) {
    /* OPENING.EXE `_do_anims` 0x462-0x4c5: OPENSUN ignores its own counter;
     * the 1-based sprite steps on the absolute anim_frame (DS:0x82), and the
     * last test (>= 0xfc) drops it back to sprite 6. */
    static const int at[] = {0x87, 0x99, 0xad, 0xc3, 0xdc, 0xec};
    const int clock = s->frame + elapsed;
    int spr = 1;
    for (int i = 0; i < (int)(sizeof(at) / sizeof(at[0])); ++i) {
      if (clock >= at[i]) {
        spr = i + 2;
      }
    }
    if (clock >= 0xfc) {
      spr = 6;
    }
    return (spr <= n ? spr : n) - 1;
  }
  if (s->repeats > 0) {
    return elapsed % n;
  }
  if (elapsed < n) {
    return elapsed;
  }
  /* Repeats 0: hold the last sprite. OPENGUY is special — `_anim_loop`
   * sets the 1-based frame to count-5 so the last 6 frames idle-loop. */
  if (s->series == OPENING_SHEET_GUY && n > OPENING_GUY_LOOP_BACK) {
    const int loop = OPENING_GUY_LOOP_BACK + 1;
    return (elapsed - n) % loop + (n - loop);
  }
  return n - 1;
}

static void opening_compose(OpeningCinematic* o);

static void opening_begin_sailing(OpeningCinematic* o) {
  if (!o) {
    return;
  }
  o->logo_phase = false;
  o->clock = 0;
  o->accum = 0;
  o->skip_from = -1;
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
  const int logo_dy = opening_logo_y_shift(o);
  if (o->mps_logo_ok && o->mps_logo.sprite_count > 0) {
    int idx = o->logo_frame;
    if (idx < 0) {
      idx = 0;
    }
    if (idx >= o->mps_logo.sprite_count) {
      idx = o->mps_logo.sprite_count - 1;
    }
    ss_blit_anchored(&o->mps_logo, idx, &fb, 0, logo_dy);
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
    ss_blit_anchored(&o->mps_name, idx, &fb, 0, logo_dy);
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

  /* `_pan` 0x57e-0x595: anims then ship up to anim_frame 0x1fb, after it the
   * ship first so the series cover it. */
  const bool ship_under = o->clock > OPENING_SHIP_UNDER_FRAME;
  if (ship_under) {
    opening_draw_ship(o, &fb, camera);
  }
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
    int elapsed = o->clock - s->frame;
    if (o->skip_from >= 0) {
      /* `_jump_to_logo`: a series running at the skip keeps its own counter
       * and takes one more step; one not yet started only starts on an exact
       * anim_frame match (`_anim_loop` 0x835), i.e. only the logo row.
       * OPENSUN reads the absolute anim_frame, so it keeps clock - frame. */
      if (s->frame > o->skip_from) {
        if (s->frame != o->clock) {
          continue;
        }
      } else if (s->series != OPENING_SHEET_SUN) {
        elapsed = o->skip_from + 1 - s->frame;
      }
    }
    const int idx = opening_sprite_index(s, elapsed, n);
    if (idx < 0) {
      continue;
    }
    ss_blit_anchored(sheet, idx, &fb, s->base_x - camera, 0);
  }

  if (!ship_under) {
    opening_draw_ship(o, &fb, camera);
  }

  opening_blit_border(o, &fb);

  for (int i = 0; i < o->credit_count; ++i) {
    const OpeningCredit* c = &o->credit_rows[i];
    if (o->clock < c->start_frame || o->clock > c->end_frame) {
      continue;
    }
    if (c->series < 0 || c->series >= OPENING_CREDIT_SHEETS || !o->credit_ok[c->series]) {
      continue;
    }
    opening_blit_credit(&o->credits[c->series], c->sprite, &fb);
  }
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
    if (!ss_load_asset(data_dir, kOpeningSheets[i], &o->sheets[i], "Opening cinematic")) {
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
  /* The timeline is OPENING.TXT data (end frame = its end row); no
   * compiled-in copy. */
  if (o->series_count <= 0 || o->end_frame < 1) {
    diag_warn("Opening cinematic: no timeline in OPENING.TXT.");
    opening_close(o);
    return false;
  }
  o->credit_count = opening_parse_credits(data_dir, o->credit_rows, OPENING_CREDIT_MAX);
  o->path_count = opening_parse_path(data_dir, o->path_x, o->path_y, OPENING_PATH_MAX);

  o->clock = 0;
  o->accum = 0;
  o->skip_from = -1;
  o->pan_timing = OPENING_PAN_TIMING;
  o->logo_timing = OPENING_LOGO_TIMING;
  o->open = true;
  o->finished = false;
  o->logo_phase = o->mps_logo_ok;
  opening_compose(o);
  if (g_opening_set_bgm) {
    /* OPENING.EXE is its own process; no VICEROY pool. Clearing the category
     * first makes sound_play take the immediate branch (sound.c "No VICEROY
     * pool") instead of queueing 0x34 behind a running map tune. Mirrors
     * closing_open. */
    g_opening_set_bgm(0);
  }
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

/* The steppers fire once game_clock - clock >= timing (a signed 32-bit
 * compare, e.g. `_pan` 0x53e-0x55a), so a timing nudged to 0 or below steps
 * on every loop pass: one step per update here. 0 = that case. */
static uint64_t opening_frame_units(int timing) {
  return timing > 0 ? (uint64_t)timing * OPENING_ACCUM_PER_TICK : 0;
}

static bool opening_holding(const OpeningCinematic* o) {
  return !o->logo_phase && (o->skip_from >= 0 || (o->end_frame > 0 && o->clock >= o->end_frame));
}

void opening_update(OpeningCinematic* o, uint32_t dt_ms) {
  if (!o || !o->open || o->finished) {
    return;
  }
  if (o->logo_phase) {
    const uint64_t logo_frame = opening_frame_units(o->logo_timing);
    o->accum += (uint64_t)dt_ms * OPENING_ACCUM_PER_MS;
    while (o->accum >= logo_frame) {
      o->accum -= logo_frame;
      if (!opening_logo_step(o)) {
        break;
      }
      if (logo_frame == 0) {
        o->accum = 0;
        break;
      }
    }
    if (o->logo_phase) {
      return;
    }
  } else if (opening_holding(o)) {
    o->hold_ms += dt_ms;
    if (o->hold_ms >= OPENING_HOLD_MS) {
      o->finished = true;
    }
    return;
  } else {
    o->accum += (uint64_t)dt_ms * OPENING_ACCUM_PER_MS;
  }
  const uint64_t frame = opening_frame_units(o->pan_timing);
  while (o->accum >= frame) {
    o->accum -= frame;
    if (!opening_step(o) || frame == 0) {
      o->accum = 0;
      break;
    }
  }
}

/* `_get_input` 0x977-0x9a3: Enter, Esc, Space, a mouse button (stored as
 * 0x1b), Alt-Q (0x110) or Alt-X (0x12d). Any other key is ignored. */
static bool opening_skip_key(const ColonizeInputState* in) {
  const ColonizeKey k = in->last_key;
  return k == COLONIZE_KEY_ENTER || k == COLONIZE_KEY_ESCAPE || k == COLONIZE_KEY_SPACE ||
         in->mouse_left_clicked || in->mouse_right_clicked ||
         (in->alt_held && (k == COLONIZE_KEY_Q || k == COLONIZE_KEY_X));
}

/* `_jump_to_logo` 0x872: the last OPENLOGO row's start frame, -1 = none. */
static int opening_logo_frame(const OpeningCinematic* o) {
  int at = -1;
  for (int i = 0; i < o->series_count; ++i) {
    if (o->series[i].series == OPENING_SHEET_LOGO) {
      at = o->series[i].frame;
    }
  }
  return at;
}

bool opening_handle_input(OpeningCinematic* o, const ColonizeInputState* input) {
  if (!o || !o->open) {
    return false;
  }
  if (!input) {
    return true;
  }
  /* `_get_input` 0xa7c/0xa86: '-' / '_' slow down, '+' / '=' speed up, by one
   * tick on pan_timing and logo_timing together, with no limit. */
  for (int i = 0; i < input->text_input_len; ++i) {
    const char ch = input->text_input[i];
    const int d = (ch == '-' || ch == '_') ? 1 : (ch == '+' || ch == '=') ? -1 : 0;
    o->pan_timing += d;
    o->logo_timing += d;
  }
  if (!opening_skip_key(input)) {
    return true;
  }
  if (o->logo_phase) {
    /* 0x9aa: logo_going = 0; outside demo mode that is all. */
    opening_begin_sailing(o);
    return true;
  }
  if (opening_holding(o)) {
    o->finished = true; /* the hold is port-only: DOS has already chained */
    return true;
  }
  /* 0x9d4-0xa11: opening_going = 0 ends the loop. With to_game set (-g, and
   * Alt-Q / Alt-X clear it) `_jump_to_logo` first moves to the logo frame
   * when it is still ahead: pan_x 0, ship at the path end, one step. */
  const int logo_at = opening_logo_frame(o);
  const bool alt_quit = input->alt_held &&
                        (input->last_key == COLONIZE_KEY_Q || input->last_key == COLONIZE_KEY_X);
  if (alt_quit || logo_at <= o->clock) {
    o->finished = true;
    return true;
  }
  o->skip_from = o->clock;
  o->clock = logo_at;
  o->accum = 0;
  o->hold_ms = 0;
  opening_compose(o);
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
  cinematic_blit_canvas320(o->canvas, framebuffer);
  if (palette) {
    if (o->logo_phase && o->mps_logo_ok && o->mps_logo.has_palette) {
      *palette = o->mps_logo.palette;
    } else if (o->palette_ok) {
      *palette = o->palette;
    }
  }
}
