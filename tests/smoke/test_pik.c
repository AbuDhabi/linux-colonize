#include <stdio.h>

#include "core/pik.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

static const char* const kPath = "COLONIZE/OPENMENU.PIK";

static int case_pik_load_succeeds(void) {
  diag_init(0, NULL);

  ColonizePikImage image;
  char err[256];
  if (!pik_load(kPath, &image, err, sizeof(err))) {
    fprintf(stderr, "pik_load failed: %s\n", err);
    diag_shutdown();
    return 1;
  }
  pik_free(&image);
  diag_shutdown();
  return 0;
}

static int case_pik_dimensions(void) {
  diag_init(0, NULL);

  ColonizePikImage image;
  char err[256];
  if (!pik_load(kPath, &image, err, sizeof(err))) {
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
    kPath, image.width, image.height, image.has_palette ? 1 : 0);
  pik_free(&image);
  diag_shutdown();
  return 0;
}

static const TestCase k_cases[] = {
    {"case_pik_load_succeeds", case_pik_load_succeeds},
    {"case_pik_dimensions", case_pik_dimensions},
};
TEST_MAIN(k_cases)
