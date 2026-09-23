#include <stdio.h>
#include <string.h>

#include "../common/test_catalogs.h"
#include "core/assets.h"
#include "core/ff.h"
#include "core/font.h"
#include "core/pedia.h"
#include "core/pik.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

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

static int case_terrain_preview_sprites(void) {
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
  return 0;
}

/*
 * PEDIA.TXT / NAMES.TXT are read-only message catalogs — every remaining case
 * only reads them, so a single process-lifetime load (never mutated, never
 * freed until exit) is order-independent by construction. This mirrors the
 * original main()'s single load-then-use-many-times structure without a
 * per-case reset hook, since there is nothing case-run can mutate.
 */
static ColonizeMsgCatalog s_catalog;
static ColonizeMsgCatalog s_names;
static int s_loaded = 0;
static int s_load_ok = 0;

static int fixture_ensure(void) {
  if (s_loaded) {
    return s_load_ok ? 0 : 1;
  }
  s_loaded = 1;
  assets_msg_init(&s_catalog);
  if (!assets_msg_load_file(&s_catalog, "COLONIZE/PEDIA.TXT")) {
    fprintf(stderr, "failed to load PEDIA.TXT\n");
    s_load_ok = 0;
    return 1;
  }
  assets_msg_init(&s_names);
  if (!assets_msg_load_file(&s_names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "failed to load NAMES.TXT\n");
    assets_msg_free(&s_catalog);
    s_load_ok = 0;
    return 1;
  }
  s_load_ok = 1;
  return 0;
}

static int case_load_catalogs(void) {
  return fixture_ensure();
}

static int case_terrain_pages(void) {
  if (fixture_ensure() != 0) {
    return 1;
  }
  PediaPage page;
  if (!pedia_terrain_page(&s_catalog, 0, &page)) {
    fprintf(stderr, "failed to build TERRAIN0 page\n");
    return 1;
  }
  if (strcmp(page.title, "TUNDRA") != 0) {
    fprintf(stderr, "TERRAIN0 title expected TUNDRA got '%s'\n", page.title);
    return 1;
  }
  if (page.body_line_count < 1) {
    fprintf(stderr, "TERRAIN0 expected body text\n");
    return 1;
  }
  if (!pedia_terrain_page(&s_catalog, 27, &page) || strcmp(page.title, "MOUNTAINS") != 0) {
    fprintf(stderr, "TERRAIN27 title expected MOUNTAINS got '%s'\n", page.title);
    return 1;
  }
  return 0;
}

static int case_cargo_page(void) {
  if (fixture_ensure() != 0) {
    return 1;
  }
  PediaPage page;
  if (!pedia_page(&s_catalog, &s_names, PEDIA_CAT_CARGO, 0, &page) ||
      strstr(page.title, "FOOD") == NULL) {
    fprintf(stderr, "CARGO0 title expected FOOD got '%s'\n", page.title);
    return 1;
  }
  if (page.preview_kind != PEDIA_PREVIEW_ICON || page.icon_sprite != 22) {
    fprintf(stderr, "CARGO0 icon expected 22\n");
    return 1;
  }
  return 0;
}

static int case_job_page(void) {
  if (fixture_ensure() != 0) {
    return 1;
  }
  PediaPage page;
  if (!pedia_page(&s_catalog, &s_names, PEDIA_CAT_JOB, 12, &page)) {
    fprintf(stderr, "JOB12 page failed (section name trim?)\n");
    return 1;
  }
  if (page.body_line_count < 1) {
    fprintf(stderr, "JOB12 expected body\n");
    return 1;
  }
  return 0;
}

/*
 * Skill-page portrait = DOS FUN_6cb2_1820 raw 110943-110945 (asm
 * CODE_144:6cb2:1928-193a): sprite = 0x52 + job, with a single escape
 * 0x1b -> 0x43, minus 1 for the port's 0-based blit indices. Both Pedia
 * paths (the PediaPage preview field and the article renderer) must agree.
 */
static int case_job_icon_formula(void) {
  if (fixture_ensure() != 0) {
    return 1;
  }
  const struct { int job; int sprite; } want[] = {
    {0, 81},   /* Expert Farmer */
    {18, 99},  /* cut Expert Teacher, still linear */
    {19, 100}, /* Free Colonist */
    {20, 101}, /* Hardy Pioneer - equipped pose, NOT the map/colony 58 */
    {24, 105}, /* Jesuit Missionary - equipped pose, NOT 61 */
    {26, 107}, /* Petty Criminal */
    {27, 66},  /* Indian Convert - the one DOS escape */
  };
  for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); ++i) {
    PediaPage page;
    if (!pedia_page(&s_catalog, &s_names, PEDIA_CAT_JOB, want[i].job, &page)) {
      fprintf(stderr, "JOB%d page failed\n", want[i].job);
      return 1;
    }
    if (page.preview_kind != PEDIA_PREVIEW_ICON || page.icon_sprite != want[i].sprite) {
      fprintf(
        stderr,
        "JOB%d icon expected %d got %d\n",
        want[i].job,
        want[i].sprite,
        page.icon_sprite
      );
      return 1;
    }
  }
  return 0;
}

static int case_father_page(void) {
  if (fixture_ensure() != 0) {
    return 1;
  }
  PediaPage page;
  if (!pedia_page(&s_catalog, &s_names, PEDIA_CAT_FATHER, 0, &page) ||
      strstr(page.title, "Adam Smith") == NULL) {
    fprintf(stderr, "FATHER0 title expected Adam Smith got '%s'\n", page.title);
    return 1;
  }
  if (page.preview_kind != PEDIA_PREVIEW_FATHER || page.father_index != 0) {
    fprintf(stderr, "FATHER0 preview kind/index wrong\n");
    return 1;
  }
  return 0;
}

static int case_misc_page(void) {
  if (fixture_ensure() != 0) {
    return 1;
  }
  PediaPage page;
  if (!pedia_page(&s_catalog, &s_names, PEDIA_CAT_MISC, 9, &page) ||
      strcmp(page.title, "Liberty Bells") != 0) {
    fprintf(stderr, "MISC Liberty Bells failed got '%s'\n", page.title);
    return 1;
  }
  return 0;
}

static int case_caret_flags(void) {
  if (fixture_ensure() != 0) {
    return 1;
  }
  /*
   * Caret prefixes follow DOS FUN_6f74_0c32: "^^" = flag 1 (centred own line),
   * a single "^" = flag 2 (own line, LEFT-aligned) — the brace that follows a
   * heading caret says nothing about alignment.
   */
  const char* rest = NULL;
  struct {
    const char* line;
    int flags;
    const char* rest;
  } cases[] = {
    {"^{Adam Smith (1723-1790)}", PEDIA_CARET_OWN_LINE, "{Adam Smith (1723-1790)}"},
    {"^", PEDIA_CARET_OWN_LINE, ""},
    {"^^Centred", PEDIA_CARET_CENTER, "Centred"},
    {"^^{Centred heading}", PEDIA_CARET_CENTER, "{Centred heading}"},
    {"Ordinary prose.", 0, "Ordinary prose."},
    {"{brace} but no caret", 0, "{brace} but no caret"},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    const int got = pedia_caret_flags(cases[i].line, &rest);
    if (got != cases[i].flags || strcmp(rest, cases[i].rest) != 0) {
      fprintf(
        stderr,
        "caret '%s' expected flags=%d rest='%s' got flags=%d rest='%s'\n",
        cases[i].line,
        cases[i].flags,
        cases[i].rest,
        got,
        rest
      );
      return 1;
    }
  }
  /* Shipped PEDIA.TXT carries only single carets, so no heading centres. */
  int caret_rows = 0;
  int centred_rows = 0;
  for (int s = 0; s < s_catalog.section_count; ++s) {
    const ColonizeMsgSection* sec = &s_catalog.sections[s];
    for (int i = 0; i < sec->line_count; ++i) {
      const int f = pedia_caret_flags(sec->lines[i], NULL);
      if (f != 0) {
        caret_rows++;
      }
      if (f == PEDIA_CARET_CENTER) {
        centred_rows++;
      }
    }
  }
  if (caret_rows == 0 || centred_rows != 0) {
    fprintf(
      stderr,
      "PEDIA.TXT caret rows=%d centred=%d (expected many, none centred)\n",
      caret_rows,
      centred_rows
    );
    return 1;
  }
  return 0;
}

static int case_entry_title(void) {
  if (fixture_ensure() != 0) {
    return 1;
  }
  char title[PEDIA_TITLE_LEN];
  if (!pedia_entry_title(&s_catalog, &s_names, PEDIA_CAT_CARGO, 0, title, sizeof(title)) ||
      strstr(title, "FOOD") == NULL) {
    fprintf(stderr, "list title CARGO0 expected FOOD got '%s'\n", title);
    return 1;
  }
  return 0;
}

static int case_list_render_and_hits(void) {
  /* The list chrome ("(Exit)", header, category names) is catalog text. */
  pedia_set_chrome_catalogs(test_labels_txt(), test_menu_txt());
  if (fixture_ensure() != 0) {
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

  int rc = 0;
  uint8_t pixels[320 * 200];
  memset(pixels, 0, sizeof(pixels));
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  pedia_list_render(
    &s_catalog, &s_names, PEDIA_CAT_TERRAIN, wood_ok ? &wood : NULL, f, -1, &fb
  );
  if (pixels[8 + 4 * 320] == 0 && pixels[20 * 320 + 8] == 0) {
    fprintf(stderr, "pedia list render produced empty pixels\n");
    rc = 1;
    goto done;
  }

  {
    PediaListHit hit = pedia_list_hit(&s_catalog, &s_names, PEDIA_CAT_TERRAIN, f, 310, 4);
    if (hit.kind != PEDIA_LIST_HIT_EXIT) {
      fprintf(stderr, "expected Exit hit near top-right, got kind=%d\n", (int)hit.kind);
      rc = 1;
      goto done;
    }
    hit = pedia_list_hit(&s_catalog, &s_names, PEDIA_CAT_TERRAIN, f, 10, 22);
    if (hit.kind != PEDIA_LIST_HIT_ENTRY || hit.entry_index != 0) {
      fprintf(
        stderr,
        "expected entry 0 hit, got kind=%d index=%d\n",
        (int)hit.kind,
        hit.entry_index
      );
      rc = 1;
      goto done;
    }
  }

done:
  if (wood_ok) {
    pik_free(&wood);
  }
  if (font_ok) {
    ff_free(&font);
  }
  return rc;
}

static int case_job12_section_trimmed(void) {
  if (fixture_ensure() != 0) {
    return 1;
  }
  if (assets_msg_find(&s_catalog, "JOB12") == NULL) {
    fprintf(stderr, "JOB12 section should be trimmed of trailing space\n");
    return 1;
  }
  fprintf(
    stderr,
    "pedia tests ok (categories=%d terrain=%d)\n",
    (int)PEDIA_CAT_COUNT,
    PEDIA_TERRAIN_COUNT
  );
  return 0;
}

static const TestCase k_cases[] = {
    {"case_terrain_preview_sprites", case_terrain_preview_sprites},
    {"case_load_catalogs", case_load_catalogs},
    {"case_terrain_pages", case_terrain_pages},
    {"case_cargo_page", case_cargo_page},
    {"case_job_page", case_job_page},
    {"case_job_icon_formula", case_job_icon_formula},
    {"case_father_page", case_father_page},
    {"case_misc_page", case_misc_page},
    {"case_caret_flags", case_caret_flags},
    {"case_entry_title", case_entry_title},
    {"case_list_render_and_hits", case_list_render_and_hits},
    {"case_job12_section_trimmed", case_job12_section_trimmed},
};

int main(void) {
  diag_init(0, NULL);
  int rc = tr_run_main(k_cases, (int)(sizeof(k_cases) / sizeof(k_cases[0])));
  diag_shutdown();
  return rc;
}
