/*
 * Woodcut presentation half — sheet/caption load, plate draw, open/close,
 * input and render, split out of woodcut.c 2026-09-16 so the simulation side
 * keeps only the once-only bit array and the pending queue (woodcut_fire).
 * Pure move: bodies are byte-identical.
 *
 * This file is UI.
 */
#include "core/woodcut.h"
#include "core/woodcut_present.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/cinematic.h"
#include "core/font.h"
#include "core/ss.h"
#include "platform/diagnostics.h"


/*
 * FUN_281f_01f0(0xffff, 0x5c, 0x5e, 0x5d) fills the four-entry glyph shade
 * table at DS:0x269e. Entry 0 is the background (0xff = leave the plate
 * alone); FF glyph shades 1-3 ink the brown ramp of the nameplate palette.
 */
static const uint8_t WOODCUT_TEXT_SHADES[4] = {0x00, 0x5c, 0x5e, 0x5d};


/* ------------------------------------------------------------- present -- */

static bool woodcut_load_sheet(
  const char* data_dir,
  const char* name,
  ColonizeSpriteSheet* out
) {
  return cinematic_load_sheet(data_dir, name, out, "Woodcut");
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
  ss_blit_anchored(&frame, 0, &fb, 0, 0); /* FUN_6f30_002e */
  if (plate_ok) {
    woodcut_draw_plate(&plate, font_ok ? &font : NULL, w->caption, &fb);
  }
  /* DOS draws the picture last, after the plate and the palette fade-in. */
  ss_blit_anchored(&art, 0, &fb, 0, 0); /* FUN_6f30_002e */

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
  cinematic_blit_canvas320(w->canvas, framebuffer);
  if (palette && w->palette_ok) {
    *palette = w->palette;
  }
}
