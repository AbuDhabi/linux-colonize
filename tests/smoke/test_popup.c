#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/popup.h"
#include "core/ui_colors.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

static int fail(const char* msg) {
  fprintf(stderr, "%s\n", msg);
  return 1;
}

static uint8_t at(const ColonizeFramebuffer8* fb, int x, int y) {
  return fb->pixels[y * fb->width + x];
}

static int case_colors_from_ui(void) {
  diag_init(0, NULL);
  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);
  diag_shutdown();
  if (colors.outer != 0 || colors.mid != COLONIZE_COL_BORDER0 ||
      colors.light != COLONIZE_COL_BORDER1 || colors.dark != COLONIZE_COL_BORDER2) {
    return fail("popup_colors_from_ui mismatch vs @COLORS borders");
  }
  return 0;
}

static int case_draw(void) {
  diag_init(0, NULL);

  ColonizePopupColors colors;
  popup_colors_from_ui(&colors);

  uint8_t pixels[320 * 200];
  memset(pixels, 0xCC, sizeof(pixels));
  ColonizeFramebuffer8 fb = {320, 200, pixels};

  const int x = 40;
  const int y = 50;
  const int w = 80;
  const int h = 40;
  int ix = -1;
  int iy = -1;
  int iw = -1;
  int ih = -1;
  popup_draw(&fb, x, y, w, h, NULL, &colors, &ix, &iy, &iw, &ih);

  if (ix != x + POPUP_FRAME_INSET || iy != y + POPUP_FRAME_INSET ||
      iw != w - POPUP_FRAME_INSET * 2 || ih != h - POPUP_FRAME_INSET * 2) {
    fprintf(stderr, "inner rect expected %d,%d %dx%d got %d,%d %dx%d\n",
            x + POPUP_FRAME_INSET, y + POPUP_FRAME_INSET,
            w - POPUP_FRAME_INSET * 2, h - POPUP_FRAME_INSET * 2, ix, iy, iw, ih);
    diag_shutdown();
    return 1;
  }

  /* Outer black. */
  if (at(&fb, x, y) != colors.outer || at(&fb, x + w - 1, y + h - 1) != colors.outer ||
      at(&fb, x + w / 2, y) != colors.outer || at(&fb, x, y + h / 2) != colors.outer) {
    diag_shutdown();
    return fail("outer black edge missing");
  }

  /* Mid brown inset 1. */
  if (at(&fb, x + 1, y + 1) != colors.mid || at(&fb, x + w - 2, y + h - 2) != colors.mid ||
      at(&fb, x + w / 2, y + 1) != colors.mid || at(&fb, x + 1, y + h / 2) != colors.mid) {
    diag_shutdown();
    return fail("mid brown edge missing");
  }

  /* Bevel: light top/right, dark bottom/left. */
  if (at(&fb, x + w / 2, y + 2) != colors.light) {
    diag_shutdown();
    return fail("bevel light top missing");
  }
  if (at(&fb, x + w - 3, y + h / 2) != colors.light) {
    diag_shutdown();
    return fail("bevel light right missing");
  }
  if (at(&fb, x + w / 2, y + h - 3) != colors.dark) {
    diag_shutdown();
    return fail("bevel dark bottom missing");
  }
  if (at(&fb, x + 2, y + h / 2) != colors.dark) {
    diag_shutdown();
    return fail("bevel dark left missing");
  }

  /* Interior fill fallback. */
  if (at(&fb, ix + iw / 2, iy + ih / 2) != POPUP_FALLBACK_FILL) {
    diag_shutdown();
    return fail("expected solid fallback fill in content area");
  }

  /* Outside untouched. */
  if (at(&fb, x - 1, y) != 0xCC || at(&fb, x + w, y + h - 1) != 0xCC) {
    diag_shutdown();
    return fail("popup drew outside its rect");
  }

  diag_shutdown();
  return 0;
}

/* Remap: synthetic palettes force mid->7. Independent of case_draw: builds
 * its own src/dst palettes and colors from scratch. */
static int case_remap(void) {
  diag_init(0, NULL);

  ColonizePalette src;
  ColonizePalette dst;
  memset(&src, 0, sizeof(src));
  memset(&dst, 0, sizeof(dst));
  src.rgb[COLONIZE_COL_BORDER0][0] = 10;
  src.rgb[COLONIZE_COL_BORDER0][1] = 20;
  src.rgb[COLONIZE_COL_BORDER0][2] = 30;
  src.rgb[COLONIZE_COL_BORDER1][0] = 40;
  src.rgb[COLONIZE_COL_BORDER1][1] = 50;
  src.rgb[COLONIZE_COL_BORDER1][2] = 60;
  src.rgb[COLONIZE_COL_BORDER2][0] = 70;
  src.rgb[COLONIZE_COL_BORDER2][1] = 80;
  src.rgb[COLONIZE_COL_BORDER2][2] = 90;
  dst.rgb[7][0] = 10;
  dst.rgb[7][1] = 20;
  dst.rgb[7][2] = 30;
  dst.rgb[8][0] = 40;
  dst.rgb[8][1] = 50;
  dst.rgb[8][2] = 60;
  dst.rgb[9][0] = 70;
  dst.rgb[9][1] = 80;
  dst.rgb[9][2] = 90;
  ColonizePopupColors remapped;
  popup_colors_from_ui(&remapped);
  popup_colors_remap(&remapped, &src, &dst);
  diag_shutdown();
  if (remapped.outer != 0 || remapped.mid != 7 || remapped.light != 8 || remapped.dark != 9) {
    fprintf(
      stderr,
      "remap expected mid/light/dark 7/8/9 got %u/%u/%u\n",
      remapped.mid,
      remapped.light,
      remapped.dark
    );
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"case_colors_from_ui", case_colors_from_ui},
    {"case_draw", case_draw},
    {"case_remap", case_remap},
};

TEST_MAIN(k_cases)
