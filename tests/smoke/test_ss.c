#include <stdio.h>

#include "core/ss.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

static int case_terrain_ss(void) {
  diag_init(0, NULL);
  ColonizeSpriteSheet terrain;
  char err[256];
  if (!ss_load("COLONIZE/TERRAIN.SS", &terrain, err, sizeof(err))) {
    fprintf(stderr, "terrain load failed: %s\n", err);
    return 1;
  }

  for (int i = 0; i < terrain.sprite_count; ++i) {
    fprintf(stderr, "terrain[%d]=%dx%d\n", i, terrain.sprites[i].width, terrain.sprites[i].height);
  }

  ss_free(&terrain);
  return 0;
}

static int case_cursor_ss(void) {
  diag_init(0, NULL);
  ColonizeSpriteSheet cursor;
  char err[256];
  if (!ss_load("COLONIZE/CURSOR.SS", &cursor, err, sizeof(err))) {
    fprintf(stderr, "cursor load failed: %s\n", err);
    return 1;
  }
  for (int i = 0; i < cursor.sprite_count; ++i) {
    fprintf(stderr, "cursor[%d]=%dx%d\n", i, cursor.sprites[i].width, cursor.sprites[i].height);
  }
  ss_free(&cursor);
  return 0;
}

static int case_phys0_ss(void) {
  diag_init(0, NULL);
  ColonizeSpriteSheet phys0;
  char err[256];
  if (!ss_load("COLONIZE/PHYS0.SS", &phys0, err, sizeof(err))) {
    fprintf(stderr, "phys0 load failed: %s\n", err);
    return 1;
  }
  if (phys0.sprite_count < 107) {
    fprintf(stderr, "phys0 expected >=107 sprites, got %d\n", phys0.sprite_count);
    ss_free(&phys0);
    return 1;
  }
  fprintf(stderr, "phys0 sprites=%d overlay101=%dx%d\n",
    phys0.sprite_count,
    phys0.sprites[101].width,
    phys0.sprites[101].height);

  ss_free(&phys0);
  diag_shutdown();
  return 0;
}

/* 2x1 sprite: index 0xFD transparent, index 7 opaque. Side 2 scales by 128 to
 * 256; vertical offset (2-1)/2 = 0 puts the art on rows 0..127, the rest is margin. */
static int case_icon_argb(void) {
  uint8_t px[2] = {COLONIZE_SS_TRANSPARENT, 7};
  ColonizeSprite sp = {.width = 2, .height = 1, .pixels = px};
  ColonizePalette pal = {0};
  pal.rgb[7][0] = 0x12;
  pal.rgb[7][1] = 0x34;
  pal.rgb[7][2] = 0x56;
  pal.rgb[COLONIZE_SS_TRANSPARENT][0] = 0xff;
  const int n = ss_icon_side(&sp);
  if (n != 256) {
    fprintf(stderr, "icon side %d, want 256\n", n);
    return 1;
  }
  static uint32_t out[256 * 256];
  ss_sprite_to_icon_argb(&sp, &pal, out);
  const uint32_t margin = out[255 * n + 200];
  const uint32_t clear = out[0 * n + 10];
  const uint32_t opaque = out[0 * n + 200];
  const uint32_t opaque_last = out[127 * n + 255];
  if (margin != 0 || clear != 0 || opaque != 0xff123456u || opaque_last != 0xff123456u) {
    fprintf(stderr, "icon argb margin=%08x clear=%08x opaque=%08x last=%08x\n",
      margin, clear, opaque, opaque_last);
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"case_terrain_ss", case_terrain_ss},
    {"case_cursor_ss", case_cursor_ss},
    {"case_phys0_ss", case_phys0_ss},
    {"case_icon_argb", case_icon_argb},
};
TEST_MAIN(k_cases)
