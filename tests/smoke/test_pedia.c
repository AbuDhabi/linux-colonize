#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/pedia.h"
#include "core/pik.h"
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

  static const int coast_phys0[] = {150, 151, 152, 153};
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

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "failed to load NAMES.TXT\n");
    assets_msg_free(&catalog);
    return 1;
  }

  PediaPage page;
  if (!pedia_terrain_page(&catalog, 0, &page)) {
    fprintf(stderr, "failed to build TERRAIN0 page\n");
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }
  if (strcmp(page.title, "TUNDRA") != 0) {
    fprintf(stderr, "TERRAIN0 title expected TUNDRA got '%s'\n", page.title);
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }
  if (page.body_line_count < 1) {
    fprintf(stderr, "TERRAIN0 expected body text\n");
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  if (!pedia_terrain_page(&catalog, 27, &page) || strcmp(page.title, "MOUNTAINS") != 0) {
    fprintf(stderr, "TERRAIN27 title expected MOUNTAINS got '%s'\n", page.title);
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  if (!pedia_page(&catalog, &names, PEDIA_CAT_CARGO, 0, &page) ||
      strstr(page.title, "FOOD") == NULL) {
    fprintf(stderr, "CARGO0 title expected FOOD got '%s'\n", page.title);
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }
  if (page.preview_kind != PEDIA_PREVIEW_ICON || page.icon_sprite != 22) {
    fprintf(stderr, "CARGO0 icon expected 22\n");
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  if (!pedia_page(&catalog, &names, PEDIA_CAT_JOB, 12, &page)) {
    fprintf(stderr, "JOB12 page failed (section name trim?)\n");
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }
  if (page.body_line_count < 1) {
    fprintf(stderr, "JOB12 expected body\n");
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  if (!pedia_page(&catalog, &names, PEDIA_CAT_FATHER, 0, &page) ||
      strstr(page.title, "Adam Smith") == NULL) {
    fprintf(stderr, "FATHER0 title expected Adam Smith got '%s'\n", page.title);
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }
  if (page.preview_kind != PEDIA_PREVIEW_FATHER || page.father_index != 0) {
    fprintf(stderr, "FATHER0 preview kind/index wrong\n");
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  if (!pedia_page(&catalog, &names, PEDIA_CAT_MISC, 9, &page) ||
      strcmp(page.title, "Liberty Bells") != 0) {
    fprintf(stderr, "MISC Liberty Bells failed got '%s'\n", page.title);
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  char title[PEDIA_TITLE_LEN];
  if (!pedia_entry_title(&catalog, &names, PEDIA_CAT_CARGO, 0, title, sizeof(title)) ||
      strstr(title, "FOOD") == NULL) {
    fprintf(stderr, "list title CARGO0 expected FOOD got '%s'\n", title);
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  /* Encyclopedia list: wood background, header, Exit, green entry links. */
  ColonizePikImage wood;
  memset(&wood, 0, sizeof(wood));
  char pik_err[128];
  const bool wood_ok = pik_load("COLONIZE/WOODPANL.PIK", &wood, pik_err, sizeof(pik_err));

  ColonizeFont font;
  memset(&font, 0, sizeof(font));
  char ff_err[128];
  const bool font_ok = ff_load("COLONIZE/FONTTINY.FF", &font, ff_err, sizeof(ff_err));
  const ColonizeFont* f = font_ok ? &font : NULL;

  uint8_t pixels[320 * 200];
  memset(pixels, 0, sizeof(pixels));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  pedia_list_render(
    &catalog, &names, PEDIA_CAT_TERRAIN, wood_ok ? &wood : NULL, f, -1, &fb
  );
  if (pixels[8 + 4 * 320] == 0 && pixels[20 * 320 + 8] == 0) {
    fprintf(stderr, "pedia list render produced empty pixels\n");
    if (wood_ok) {
      pik_free(&wood);
    }
    if (font_ok) {
      ff_free(&font);
    }
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  PediaListHit hit = pedia_list_hit(&catalog, &names, PEDIA_CAT_TERRAIN, f, 310, 4);
  if (hit.kind != PEDIA_LIST_HIT_EXIT) {
    fprintf(stderr, "expected Exit hit near top-right, got kind=%d\n", (int)hit.kind);
    if (wood_ok) {
      pik_free(&wood);
    }
    if (font_ok) {
      ff_free(&font);
    }
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }
  hit = pedia_list_hit(&catalog, &names, PEDIA_CAT_TERRAIN, f, 10, 22);
  if (hit.kind != PEDIA_LIST_HIT_ENTRY || hit.entry_index != 0) {
    fprintf(
      stderr,
      "expected entry 0 hit, got kind=%d index=%d\n",
      (int)hit.kind,
      hit.entry_index
    );
    if (wood_ok) {
      pik_free(&wood);
    }
    if (font_ok) {
      ff_free(&font);
    }
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  if (assets_msg_find(&catalog, "JOB12") == NULL) {
    fprintf(stderr, "JOB12 section should be trimmed of trailing space\n");
    if (wood_ok) {
      pik_free(&wood);
    }
    if (font_ok) {
      ff_free(&font);
    }
    assets_msg_free(&catalog);
    assets_msg_free(&names);
    return 1;
  }

  fprintf(
    stderr,
    "pedia tests ok (categories=%d terrain=%d)\n",
    (int)PEDIA_CAT_COUNT,
    PEDIA_TERRAIN_COUNT
  );
  if (wood_ok) {
    pik_free(&wood);
  }
  if (font_ok) {
    ff_free(&font);
  }
  assets_msg_free(&catalog);
  assets_msg_free(&names);
  diag_shutdown();
  return 0;
}
