#include <stdio.h>

#include "core/ss.h"
#include "platform/diagnostics.h"

static void dump_sprite(const ColonizeSpriteSheet* sheet, int idx) {
  if (idx < 0 || idx >= sheet->sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &sheet->sprites[idx];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    printf("sprite %d empty\n", idx);
    return;
  }
  printf("sprite %d (%dx%d)\n", idx, sp->width, sp->height);
  for (int y = 0; y < sp->height; ++y) {
    for (int x = 0; x < sp->width; ++x) {
      const uint8_t c = sp->pixels[y * sp->width + x];
      char ch = '.';
      if (c == 0xFD) {
        ch = '.';
      } else if (c == 0) {
        ch = '#';
      } else if (c >= 120) {
        ch = '@';
      } else if (c >= 90) {
        ch = 's';
      } else if (c >= 70) {
        ch = 'h';
      } else if (c >= 47) {
        ch = 'r';
      } else {
        ch = '+';
      }
      putchar(ch);
    }
    putchar('\n');
  }
}

int main(void) {
  diag_init(0, NULL);
  ColonizeSpriteSheet sheet;
  char err[256];
  if (!ss_load("COLONIZE/PHYS0.SS", &sheet, err, sizeof(err))) {
    fprintf(stderr, "load failed: %s\n", err);
    return 1;
  }

  static const int indices[] = {
    1, 2, 3, 4, 5, 6, 7,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    64, 65, 66, 67, 68, 69, 70,
    89, 90, 91, 92, 96, 97,
    140, 141, 148, 149, 150, 151,
    -1
  };
  for (int i = 0; indices[i] >= 0; ++i) {
    dump_sprite(&sheet, indices[i]);
  }

  ss_free(&sheet);
  return 0;
}
