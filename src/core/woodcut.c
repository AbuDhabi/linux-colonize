#include "core/woodcut.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/font.h"
#include "platform/diagnostics.h"

/*
 * `event` (bits 1-16) and `unknown05` (bits 17-32) are the one 32-bit array
 * DOS addresses as DS:0x540a; the byte-level accessors below only work if
 * they really are adjacent in the packed header.
 */
_Static_assert(
  offsetof(ColonizeCol1Head, unknown05) == offsetof(ColonizeCol1Head, event) + 2,
  "woodcut once-only array must be event[2] + unknown05[2]"
);

/*
 * FUN_281f_01f0(0xffff, 0x5c, 0x5e, 0x5d) fills the four-entry glyph shade
 * table at DS:0x269e. Entry 0 is the background (0xff = leave the plate
 * alone); FF glyph shades 1-3 ink the brown ramp of the nameplate palette.
 */
static const uint8_t WOODCUT_TEXT_SHADES[4] = {0x00, 0x5c, 0x5e, 0x5d};

static ColonizeWoodcutSoundFn g_woodcut_play;
static ColonizeWoodcutSoundFn g_woodcut_set_bgm;

void woodcut_set_sound_hooks(ColonizeWoodcutSoundFn play_fn, ColonizeWoodcutSoundFn set_bgm_fn) {
  g_woodcut_play = play_fn;
  g_woodcut_set_bgm = set_bgm_fn;
}

/* ---------------------------------------------------------------- queue -- */

static int g_woodcut_queue[WOODCUT_QUEUE_MAX];
static int g_woodcut_queue_len;

void woodcut_request(int id) {
  if (id < 0 || id >= WOODCUT_ID_MAX) {
    return;
  }
  if (g_woodcut_queue_len >= WOODCUT_QUEUE_MAX) {
    diag_warn("woodcut: queue full, dropping id %d", id);
    return;
  }
  for (int i = 0; i < g_woodcut_queue_len; ++i) {
    if (g_woodcut_queue[i] == id) {
      return; /* already armed this turn */
    }
  }
  g_woodcut_queue[g_woodcut_queue_len++] = id;
}

bool woodcut_has_pending(void) {
  return g_woodcut_queue_len > 0;
}

int woodcut_take_pending(void) {
  if (g_woodcut_queue_len <= 0) {
    return -1;
  }
  const int id = g_woodcut_queue[0];
  for (int i = 1; i < g_woodcut_queue_len; ++i) {
    g_woodcut_queue[i - 1] = g_woodcut_queue[i];
  }
  g_woodcut_queue_len--;
  return id;
}

void woodcut_clear_pending(void) {
  g_woodcut_queue_len = 0;
}

/* ------------------------------------------------- FUN_12fd_006c gate --- */

/* FUN_12fd_0048 / FUN_12fd_000e: 1-based bit `id` of the DS:0x540a array. */
static uint8_t* woodcut_bit_byte(ColonizeCol1Save* col1, int id) {
  uint8_t* base = (uint8_t*)&col1->head.event;
  return base + ((id - 1) >> 3);
}

static uint8_t woodcut_bit_mask(int id) {
  return (uint8_t)(1u << ((unsigned)(id - 1) & 7u));
}

/*
 * The FUN_12fd_006c jump table: every case falls through to the presenter,
 * the only per-id work is which tune is queued first. Cases 0/1/9 hit
 * FUN_129f_0318 and case 2 FUN_129f_034c — both are "switch to tune pool 2,
 * restart if it changed" (sound_set_bgm). Cases 3-6 push an explicit event id
 * through FUN_129f_02cc (sound_play). Cases 7, 8, 10 and every id above 10
 * have no table entry and play nothing.
 */
static void woodcut_play_event(int id) {
  if (g_woodcut_play) {
    g_woodcut_play(id);
  }
}

static void woodcut_play_tune(int id) {
  switch (id) {
    case WOODCUT_A_NEW_WORLD:
    case WOODCUT_DISCOVERY_OF_THE_NEW_WORLD:
    case WOODCUT_BUILDING_A_COLONY:
    case WOODCUT_CARGO_FROM_THE_NEW_WORLD:
      if (g_woodcut_set_bgm) {
        g_woodcut_set_bgm(2);
      }
      break;
    case WOODCUT_MEETING_THE_NATIVES: woodcut_play_event(0x33); break;
    case WOODCUT_THE_AZTEC_EMPIRE: woodcut_play_event(0x35); break;
    case WOODCUT_THE_INCA_NATION: woodcut_play_event(0x36); break;
    case WOODCUT_DISCOVERY_OF_THE_PACIFIC_OCEAN: woodcut_play_event(0x39); break;
    default: break;
  }
}

bool woodcut_fire(ColonizeCol1Save* col1, int id) {
  if (id <= 0 || id >= WOODCUT_ID_MAX) {
    return false;
  }
  if (col1) {
    uint8_t* p = woodcut_bit_byte(col1, id);
    const uint8_t mask = woodcut_bit_mask(id);
    if ((*p & mask) != 0) {
      return false;
    }
    *p = (uint8_t)(*p | mask);
  }
  woodcut_play_tune(id);
  woodcut_request(id);
  return true;
}

/* ------------------------------------------------------------- present -- */

static bool woodcut_load_sheet(
  const char* data_dir,
  const char* name,
  ColonizeSpriteSheet* out
) {
  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(data_dir, name, path, sizeof(path))) {
    diag_warn("Woodcut: %s not found.", name);
    return false;
  }
  if (!ss_load(path, out, err, sizeof(err))) {
    diag_warn("Woodcut: %s failed to load: %s", name, err);
    return false;
  }
  return true;
}

/* FUN_6f30_002e: place a sheet's first sprite by its own anchor. */
static void woodcut_blit_centred(
  const ColonizeSpriteSheet* sheet,
  ColonizeFramebuffer8* fb
) {
  if (!sheet || sheet->sprite_count < 1) {
    return;
  }
  const ColonizeSprite* s = &sheet->sprites[0];
  ss_blit_sprite(sheet, 0, fb, s->anchor_x - (s->width >> 1), s->anchor_y - s->height + 1);
}

/* WOODCUT.TXT @WOODCUT line `id` (line 0 is id 0's "A NEW WORLD"). */
static void woodcut_load_caption(const char* data_dir, int id, char* out, size_t out_size) {
  out[0] = '\0';
  char path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "WOODCUT.TXT", path, sizeof(path))) {
    diag_warn("Woodcut: WOODCUT.TXT not found.");
    return;
  }
  ColonizeMsgCatalog cat;
  assets_msg_init(&cat);
  if (assets_msg_load_file(&cat, path)) {
    const ColonizeMsgSection* sec = assets_msg_find(&cat, "WOODCUT");
    if (sec && id >= 0 && id < sec->line_count) {
      snprintf(out, out_size, "%s", sec->lines[id]);
    }
  }
  assets_msg_free(&cat);
}

/*
 * The nameplate run: DOS rounds the caption width up to a whole number of
 * middle tiles, centres cap+tiles+cap on that rounded width, then draws the
 * caption centred on its own true width.
 */
static void woodcut_draw_plate(
  const ColonizeSpriteSheet* plate,
  const ColonizeFont* font,
  const char* caption,
  ColonizeFramebuffer8* fb
) {
  if (!plate || plate->sprite_count < 3) {
    return;
  }
  const int left_w = plate->sprites[0].width;
  const int mid_w = plate->sprites[1].width;
  const int right_w = plate->sprites[2].width;
  if (mid_w <= 0) {
    return;
  }
  const int text_w = font ? font_text_width(font, caption) : 0;

  int rounded = 0;
  int tiles = 0;
  while (rounded < text_w) {
    rounded += mid_w;
    tiles++;
  }

  int x = WOODCUT_CENTRE_X - ((rounded + right_w + left_w) >> 1);
  ss_blit_sprite(plate, 0, fb, x, WOODCUT_PLATE_Y);
  x += left_w;
  for (int i = 0; i < tiles; ++i) {
    ss_blit_sprite(plate, 1, fb, x, WOODCUT_PLATE_Y);
    x += mid_w;
  }
  ss_blit_sprite(plate, 2, fb, x, WOODCUT_PLATE_Y);

  if (font && caption[0]) {
    font_draw_text_shaded(
      font, fb, WOODCUT_CENTRE_X - (text_w >> 1), WOODCUT_CAPTION_Y, caption,
      WOODCUT_TEXT_SHADES
    );
  }
}

bool woodcut_open(ColonizeWoodcutScreen* w, const char* data_dir, int id) {
  if (!w || id < 0 || id >= WOODCUT_ID_MAX) {
    return false;
  }
  memset(w, 0, sizeof(*w));

  char art_name[16];
  snprintf(art_name, sizeof(art_name), "WDCUT%02d.SS", id);

  ColonizeSpriteSheet art;
  ColonizeSpriteSheet frame;
  ColonizeSpriteSheet plate;
  const bool art_ok = woodcut_load_sheet(data_dir, art_name, &art);
  if (!art_ok) {
    /* FUN_6f30_0062 bails the same way when WDCUTnn.SS is absent. */
    return false;
  }
  const bool frame_ok = woodcut_load_sheet(data_dir, "WOODFRAM.SS", &frame);
  const bool plate_ok = woodcut_load_sheet(data_dir, "NAMEPLAT.SS", &plate);
  if (!frame_ok) {
    ss_free(&art);
    if (plate_ok) {
      ss_free(&plate);
    }
    return false;
  }

  ColonizeFont font;
  bool font_ok = false;
  {
    char path[512];
    char err[256];
    if (dos_compat_normalize_asset_path(data_dir, "FONT-NP.FF", path, sizeof(path)) &&
        ff_load(path, &font, err, sizeof(err))) {
      font_ok = true;
    } else {
      diag_warn("Woodcut: FONT-NP.FF missing");
    }
  }

  woodcut_load_caption(data_dir, id, w->caption, sizeof(w->caption));

  /*
   * WOODFRAM / NAMEPLAT only ink indices below 96 and their palettes agree
   * with the art's there; the art owns 96-250, so its palette is the screen's.
   */
  if (art.has_palette) {
    w->palette = art.palette;
    w->palette_ok = true;
  } else if (frame.has_palette) {
    w->palette = frame.palette;
    w->palette_ok = true;
  }

  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = w->canvas};
  memset(w->canvas, 0, sizeof(w->canvas));
  woodcut_blit_centred(&frame, &fb);
  if (plate_ok) {
    woodcut_draw_plate(&plate, font_ok ? &font : NULL, w->caption, &fb);
  }
  /* DOS draws the picture last, after the plate and the palette fade-in. */
  woodcut_blit_centred(&art, &fb);

  ss_free(&art);
  ss_free(&frame);
  if (plate_ok) {
    ss_free(&plate);
  }
  if (font_ok) {
    ff_free(&font);
  }

  w->id = id;
  w->open = true;
  return true;
}

void woodcut_close(ColonizeWoodcutScreen* w) {
  if (!w) {
    return;
  }
  w->open = false;
}

bool woodcut_handle_input(ColonizeWoodcutScreen* w, const ColonizeInputState* input) {
  if (!w || !w->open) {
    return false;
  }
  if (!input) {
    return true;
  }
  if (input->last_key != COLONIZE_KEY_NONE || input->mouse_left_clicked ||
      input->mouse_right_clicked) {
    woodcut_close(w);
  }
  return true;
}

void woodcut_render(
  const ColonizeWoodcutScreen* w,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* palette
) {
  if (!w || !w->open || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const int cw = framebuffer->width < 320 ? framebuffer->width : 320;
  const int ch = framebuffer->height < 200 ? framebuffer->height : 200;
  for (int y = 0; y < ch; ++y) {
    memcpy(
      framebuffer->pixels + (size_t)y * (size_t)framebuffer->width,
      w->canvas + (size_t)y * 320,
      (size_t)cw
    );
  }
  if (palette && w->palette_ok) {
    *palette = w->palette;
  }
}
