/*
 * Report plates against real DOS pixels: saves opened in VICEROY.EXE under DOSBox-X
 * (tools/dosbox_trace.py, 2026-11-05). Each probe is a pixel the port once drew
 * differently; the RGB is read off the DOS screenshot (docs/reports.md "DOS check on
 * dutch-campaign/COLONY00.SAV" and "DOS check across 24 saves").
 */
#include <stdio.h>
#include <string.h>

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

static const TestCase k_cases[] = {
  {"dutch_campaign_00", case_dutch_campaign_00},
  {"french_campaign_00", case_french_campaign_00},
};

TEST_MAIN(k_cases)
