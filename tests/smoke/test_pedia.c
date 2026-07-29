#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/pedia.h"
#include "platform/diagnostics.h"

static int expect_preview(
  int index,
  int terrain_sprite,
  int phys0_count,
  const int* phys0_sprites,
  char* err,
  size_t err_size
) {
  PediaTerrainPreview preview;
  pedia_terrain_preview(index, &preview);
  if (preview.terrain_sprite != terrain_sprite) {
    snprintf(
      err,
      err_size,
      "TERRAIN%d terrain sprite expected %d got %d",
      index,
      terrain_sprite,
      preview.terrain_sprite
    );
    return 1;
  }
  if (phys0_count <= 0) {
    if (preview.phys0_count != 0) {
      snprintf(err, err_size, "TERRAIN%d expected no PHYS0, got %d", index, preview.phys0_sprites[0]);
      return 1;
    }
    return 0;
  }
  if (preview.phys0_count != phys0_count) {
    snprintf(
      err,
      err_size,
      "TERRAIN%d PHYS0 count expected %d got %d",
      index,
      phys0_count,
      preview.phys0_count
    );
    return 1;
  }
  for (int i = 0; i < phys0_count; ++i) {
    if (preview.phys0_sprites[i] != phys0_sprites[i]) {
      snprintf(
        err,
        err_size,
        "TERRAIN%d PHYS0[%d] expected %d got %d",
        index,
        i,
        phys0_sprites[i],
        preview.phys0_sprites[i]
      );
      return 1;
    }
  }
  return 0;
}

int main(void) {
  diag_init(0, NULL);

  static const int coast_phys0[] = {153, 152, 151, 150};
  static const int forest8[] = {70};
  static const int forest13[] = {69};
  static const int mountain[] = {36};
  static const int hills[] = {48};

  char err[256];
  if (expect_preview(0, 0, 0, NULL, err, sizeof(err)) != 0 ||
      expect_preview(4, 4, 0, NULL, err, sizeof(err)) != 0 ||
      expect_preview(8, 0, 1, forest8, err, sizeof(err)) != 0 ||
      expect_preview(9, 8, 0, NULL, err, sizeof(err)) != 0 ||
      expect_preview(13, 5, 1, forest13, err, sizeof(err)) != 0 ||
      expect_preview(25, 10, 4, coast_phys0, err, sizeof(err)) != 0 ||
      expect_preview(27, 4, 1, mountain, err, sizeof(err)) != 0 ||
      expect_preview(28, 4, 1, hills, err, sizeof(err)) != 0) {
    fprintf(stderr, "%s\n", err);
    return 1;
  }

  ColonizeMsgCatalog catalog;
  assets_msg_init(&catalog);
  if (!assets_msg_load_file(&catalog, "COLONIZE/PEDIA.TXT")) {
    fprintf(stderr, "failed to load PEDIA.TXT\n");
    return 1;
  }

  PediaTerrainPage page;
  if (!pedia_terrain_page(&catalog, 0, &page)) {
    fprintf(stderr, "failed to build TERRAIN0 page\n");
    assets_msg_free(&catalog);
    return 1;
  }
  if (strcmp(page.title, "TUNDRA") != 0) {
    fprintf(stderr, "TERRAIN0 title expected TUNDRA got '%s'\n", page.title);
    assets_msg_free(&catalog);
    return 1;
  }
  if (page.body_line_count < 1) {
    fprintf(stderr, "TERRAIN0 expected body text\n");
    assets_msg_free(&catalog);
    return 1;
  }

  if (!pedia_terrain_page(&catalog, 27, &page) || strcmp(page.title, "MOUNTAINS") != 0) {
    fprintf(stderr, "TERRAIN27 title expected MOUNTAINS got '%s'\n", page.title);
    assets_msg_free(&catalog);
    return 1;
  }

  fprintf(stderr, "pedia tests ok\n");
  assets_msg_free(&catalog);
  diag_shutdown();
  return 0;
}
