/*
 * Report plates against real DOS pixels: saves opened in VICEROY.EXE under DOSBox-X
 * (tools/dosbox_trace.py, 2026-11-05). Each probe is a pixel the port once drew
 * differently; the RGB is read off the DOS screenshot (docs/reports.md "DOS check on
 * dutch-campaign/COLONY00.SAV" and "DOS check across 24 saves").
 */
#include <stdio.h>
#include <string.h>

#include "core/font_file.h"
#include "core/reports.h"
#include "core/world.h"
#include "tools/render_common.h"

#include "../common/test_runner.h"

typedef struct Probe {
  int x, y;
  uint8_t r, g, b;
  const char* what;
} Probe;

typedef struct Page {
  ColonizeReportId id;
  int economic_page, colony_page;
  Probe probes[3];
} Page;

static const Page k_dutch_00[] = {
  {COLONIZE_REPORT_ECONOMIC, 1, 0, {{213, 30, 134, 0, 0, "column rule over the Horses icon"}}},
  {COLONIZE_REPORT_COLONY, 0, 0, {
    {118, 39, 0, 0, 0, "New Amsterdam badge = that unit's own orders"},
    {135, 96, 255, 113, 0, "Guadeloupe garrison in tile-chain order"},
    {118, 141, 170, 73, 0, "Vlissingen garrison in tile-chain order"},
  }},
  {COLONIZE_REPORT_INDIAN, 0, 0, {
    {129, 37, 243, 134, 85, "\"1 Mission\" (singular, no trailing s)"},
    {38, 156, 255, 0, 0, "Sioux in DAC 0xc"},
  }},
};

/* No gold and no villages: DOS still spends score row 1, so Rebel Sentiment sits on
 * row 2 (y 157) and Total Score on y 164 (FUN_41f2_0092). */
static const Page k_french_00[] = {
  {COLONIZE_REPORT_SCORE, 0, 0, {
    {16, 157, 85, 150, 52, "Rebel Sentiment on score row 2"},
    {17, 165, 199, 162, 32, "Total Score below an empty row 1"},
  }},
};

static int check_pages(const char* save, const Page* pages, size_t n_pages) {
  RenderSaveBundle rs;
  if (!render_load_save("COLONIZE", save, &rs)) {
    return 1;
  }
  ColonizeReportsView view;
  reports_init(&view);
  char err[256];
  if (!reports_load(&view, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "reports_load failed: %s\n", err);
    render_free_save(&rs);
    return 1;
  }
  int bad = 0;
  const ColonizeWorld w = world_make(&rs.units, &rs.colonies, &rs.map, &rs.save, true, NULL, &rs.europe);
  for (size_t i = 0; i < n_pages; ++i) {
    const Page* pg = &pages[i];
    uint8_t pixels[320 * 200];
    ColonizeFramebuffer8 fb;
    render_fb_init(&fb, pixels);
    reports_render_w(
      &w, &view, pg->id, false, -1, pg->economic_page, pg->colony_page, 0, rs.human, 0, 0, 0, NULL, &fb
    );
    const ColonizePalette* pal = &view.backgrounds[pg->id].palette;
    for (int k = 0; k < 3 && pg->probes[k].what; ++k) {
      const Probe* p = &pg->probes[k];
      const uint8_t* rgb = pal->rgb[pixels[p->y * 320 + p->x]];
      if (rgb[0] != p->r || rgb[1] != p->g || rgb[2] != p->b) {
        fprintf(
          stderr, "report %d (%d,%d) %s: got %u,%u,%u want %u,%u,%u\n", pg->id, p->x, p->y, p->what,
          rgb[0], rgb[1], rgb[2], p->r, p->g, p->b
        );
        bad++;
      }
    }
  }
  reports_free(&view);
  render_free_save(&rs);
  return bad != 0;
}

static int case_dutch_campaign_00(void) {
  return check_pages("original_saves/dutch-campaign/COLONY00.SAV", k_dutch_00, sizeof(k_dutch_00) / sizeof(k_dutch_00[0]));
}

static int case_french_campaign_00(void) {
  return check_pages("original_saves/french-campaign/COLONY00.SAV", k_french_00, sizeof(k_french_00) / sizeof(k_french_00[0]));
}

/*
 * Whole-report sweep: every page of every report (Congress p1+p2, the Labor grid
 * and all 28 detail views, every Economic / Colony / Naval page) rendered from a
 * real save with the live game's FONTSMAL, one FNV-1a hash per report chained over
 * its pages. Pins CURRENT output (not DOS-checked pixel by pixel): an intended
 * renderer change re-blesses the table from the "got" lines this prints.
 */
typedef struct SweepSave {
  const char* path;
  bool cheats; /* set head.game_options.cheats_enabled first (Religious "(%d of %d)" line) */
  uint32_t hash[COLONIZE_REPORT_COUNT];
} SweepSave;

static const SweepSave k_sweep[] = {
  {"original_saves/report-screen-goldens/dutch-reports.SAV", false, {
    0x959c9cae, 0xf8d689ca, 0x796bd5a9, 0x7bcf5f7c, 0x811eef28,
    0x1a9c1331, 0x34fb4349, 0x15705228, 0x5a9aafcc,
  }},
  {"original_saves/dutch-campaign/COLONY10.SAV", false, {
    0x85739b98, 0x70fb959d, 0x7eeccb93, 0xc060f711, 0xa1df30b5,
    0x268d384e, 0x38c165c5, 0x15203880, 0x7b3fb099,
  }},
  {"original_saves/valid-lategame-saves/COLONY05.SAV", false, {
    0x427b134e, 0xccaa8293, 0x3d3cc8a8, 0x36b703ae, 0x71769dad,
    0x1ef1b120, 0xdc185083, 0x58557b60, 0xf6d3e7d3,
  }},
  {"original_saves/french-campaign/COLONY00.SAV", true, {
    0xdccb8f32, 0x6d12f83c, 0x939398ab, 0xd1c026af, 0x033a4b8b,
    0x938a9a48, 0x815760ba, 0xa539c28e, 0xe83f71ef,
  }},
  /* Declared after loading 000001 (WoI headers, Foreign Affairs gate); 000021 = 20 turns into the war. */
  {"original_saves/independence-seed-100/000002.SAV", false, {
    0xaca36fc6, 0x5f57f80c, 0xf1cab39d, 0x44638ca1, 0xad3b6e6d,
    0x713107ea, 0x38c165c5, 0xe27a5040, 0x77f18b87,
  }},
  {"original_saves/independence-seed-100/000021.SAV", false, {
    0x4d975e98, 0xd72dec09, 0x19774a51, 0xd74f454f, 0xd82d9a05,
    0x59563187, 0x38c165c5, 0x6a203ba4, 0x4cfb5e4e,
  }},
};

static uint32_t fnv1a(uint32_t h, const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    h = (h ^ p[i]) * 16777619u;
  }
  return h;
}

static uint32_t sweep_render(
  const ColonizeWorld* w, const ColonizeReportsView* view, ColonizeReportId id, bool p2, int labor_job,
  int page, int human, const ColonizeFont* font, uint32_t h
) {
  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb;
  render_fb_init(&fb, pixels);
  reports_render_w(w, view, id, p2, labor_job, page, page, page, human, 0, 0, 0, font, &fb);
  return fnv1a(h, pixels, sizeof(pixels));
}

static int sweep_save(const SweepSave* s, const ColonizeReportsView* view, const ColonizeFont* font) {
  RenderSaveBundle rs;
  if (!render_load_save("COLONIZE", s->path, &rs)) {
    return 1;
  }
  rs.save.head.game_options.cheats_enabled = s->cheats;
  const ColonizeWorld w = world_make(&rs.units, &rs.colonies, &rs.map, &rs.save, true, NULL, &rs.europe);
  const int human = rs.human;
  int bad = 0;
  for (int id = 0; id < COLONIZE_REPORT_COUNT; ++id) {
    uint32_t h = 2166136261u;
    int pages = 1;
    if (id == COLONIZE_REPORT_ECONOMIC) {
      pages = reports_economic_page_count(&rs.save, human);
    } else if (id == COLONIZE_REPORT_COLONY) {
      pages = reports_colony_page_count(&rs.save, human);
    } else if (id == COLONIZE_REPORT_NAVAL) {
      pages = reports_naval_page_count_w(&w, human);
    }
    for (int pg = 0; pg < pages; ++pg) {
      h = sweep_render(&w, view, (ColonizeReportId)id, false, -1, pg, human, font, h);
    }
    if (id == COLONIZE_REPORT_CONGRESS) {
      h = sweep_render(&w, view, (ColonizeReportId)id, true, -1, 0, human, font, h);
    }
    if (id == COLONIZE_REPORT_LABOR) {
      for (int job = 0; job < 28; ++job) {
        h = sweep_render(&w, view, (ColonizeReportId)id, false, job, 0, human, font, h);
      }
    }
    if (h != s->hash[id]) {
      fprintf(stderr, "%s report %d (%d pages): got 0x%08x want 0x%08x\n", s->path, id, pages, h, s->hash[id]);
      bad++;
    }
  }
  render_free_save(&rs);
  return bad;
}

static int case_report_sweep(void) {
  ColonizeReportsView view;
  reports_init(&view);
  char err[256];
  if (!reports_load(&view, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "reports_load failed: %s\n", err);
    return 1;
  }
  ColonizeFont font;
  const bool font_ok = render_load_font("COLONIZE", "FONTSMAL.FF", &font);
  int bad = 0;
  for (size_t i = 0; i < sizeof(k_sweep) / sizeof(k_sweep[0]); ++i) {
    bad += sweep_save(&k_sweep[i], &view, font_ok ? &font : NULL);
  }
  if (font_ok) {
    font_file_free(&font);
  }
  reports_free(&view);
  return bad != 0;
}

/*
 * Hall of Fame (FUN_41f2_0f56 presenter) and Retire exploits (FUN_41f2_0b70) frames,
 * hash-pinned like the sweep: free / declared / achieved title lines, the inserted
 * slot hilite (slot 2, and slot 5 with the retired entry on line 5, DB-57), and the
 * exploits plate with SCORE03.SS remapped onto WOODPAN2.PIK.
 */
static int case_hall_of_fame_and_exploits(void) {
  ColonizeReportsView view;
  reports_init(&view);
  char err[256];
  if (!reports_load(&view, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "reports_load failed: %s\n", err);
    return 1;
  }
  ColonizeFont intr;
  if (!render_load_font("COLONIZE", "FONTINTR.FF", &intr)) {
    reports_free(&view);
    return 1;
  }
  ColonizeHofRow rows[6];
  memset(rows, 0, sizeof(rows));
  for (int k = 0; k < 6; ++k) {
    snprintf(rows[k].leader, sizeof(rows[k].leader), "Leader %d", k);
    snprintf(rows[k].nation, sizeof(rows[k].nation), "%s", reports_nation_adjective_display_name(k % 4));
    snprintf(rows[k].independent_name, sizeof(rows[k].independent_name), "Republic %d", k);
    rows[k].score = 900 - k * 100;
    rows[k].year = 1700 + k * 20;
    rows[k].difficulty = k % 5;
    rows[k].rating = 60 - k * 7;
    rows[k].declared = k % 3 != 0;
    rows[k].achieved = k % 3 == 2;
  }
  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb;
  render_fb_init(&fb, pixels);
  uint32_t h = 2166136261u;
  reports_render_hall_of_fame(&view, rows, 6, -1, NULL, &intr, &fb);
  h = fnv1a(h, pixels, sizeof(pixels));
  reports_render_hall_of_fame(&view, rows, 6, 2, NULL, &intr, &fb);
  h = fnv1a(h, pixels, sizeof(pixels));
  reports_render_hall_of_fame(&view, rows, 6, 5, &rows[5], &intr, &fb);
  h = fnv1a(h, pixels, sizeof(pixels));
  const uint32_t want_hof = 0xfb5a3c45;

  ColonizeSpriteSheet sheet;
  memset(&sheet, 0, sizeof(sheet));
  const bool sheet_ok = render_load_sheet("COLONIZE", "SCORE03.SS", &sheet);
  if (sheet_ok) {
    reports_remap_exploits_sheet(&view, &sheet);
  }
  ColonizeExploitsView ex;
  memset(&ex, 0, sizeof(ex));
  ex.header_count = 2;
  snprintf(ex.header[0], sizeof(ex.header[0]), "Header one");
  snprintf(ex.header[1], sizeof(ex.header[1]), "Header two");
  ex.category_count = 3;
  for (int i = 0; i < 3; ++i) {
    snprintf(ex.categories[i], sizeof(ex.categories[i]), "Category %d", i);
  }
  snprintf(ex.named, sizeof(ex.named), "The named line");
  ex.sheet = sheet_ok ? &sheet : NULL;
  reports_render_exploits(&view, &ex, &intr, &fb);
  const uint32_t ex_h = fnv1a(2166136261u, pixels, sizeof(pixels));
  const uint32_t want_ex = 0x98fbdf92;

  int bad = 0;
  if (h != want_hof || ex_h != want_ex || !sheet_ok) {
    fprintf(stderr, "hall of fame got 0x%08x want 0x%08x; exploits got 0x%08x want 0x%08x (sheet %d)\n",
            h, want_hof, ex_h, want_ex, sheet_ok);
    bad = 1;
  }
  if (sheet_ok) {
    ss_free(&sheet);
  }
  font_file_free(&intr);
  reports_free(&view);
  return bad;
}

static const TestCase k_cases[] = {
  {"dutch_campaign_00", case_dutch_campaign_00},
  {"french_campaign_00", case_french_campaign_00},
  {"report_sweep", case_report_sweep},
  {"hall_of_fame_and_exploits", case_hall_of_fame_and_exploits},
};

TEST_MAIN(k_cases)
