#include <stdio.h>

#include "core/pik.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  ColonizePikImage image;
  char err[256];
  const char* path = "COLONIZE/OPENMENU.PIK";
  if (!pik_load(path, &image, err, sizeof(err))) {
    fprintf(stderr, "pik_load failed: %s\n", err);
    diag_shutdown();
    return 1;
  }

  if (image.width <= 0 || image.height <= 0 || !image.pixels) {
    fprintf(stderr, "invalid image dimensions\n");
    pik_free(&image);
    diag_shutdown();
    return 1;
  }

  fprintf(stderr, "loaded %s as %dx%d palette=%d\n",
    path, image.width, image.height, image.has_palette ? 1 : 0);
  pik_free(&image);
  diag_shutdown();
  return 0;
}
