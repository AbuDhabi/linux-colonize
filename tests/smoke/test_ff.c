#include <stdio.h>
#include <string.h>

#include "core/ff.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  ColonizeFont font;
  char err[256];
  if (!ff_load("COLONIZE/FONTSMAL.FF", &font, err, sizeof(err))) {
    fprintf(stderr, "ff load failed: %s\n", err);
    return 1;
  }

  fprintf(stderr, "font %ux%u section=%zu\n", font.max_width, font.max_height, font.section_size);
  fprintf(stderr, "A width=%u offset=%u space width=%u\n",
    font.char_widths['A'],
    font.char_offsets['A'],
    font.char_widths[' ']);

  if (font.char_widths['A'] == 0 || font.char_offsets['A'] >= font.section_size) {
    fprintf(stderr, "invalid glyph A metadata\n");
    ff_free(&font);
    return 1;
  }

  ff_free(&font);
  diag_shutdown();
  return 0;
}
