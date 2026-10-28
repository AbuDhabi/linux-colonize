/*
 * Combat Analysis popup (FUN_636c_0000) against real DOS pixels.
 *
 * Each case loads a fixture save from test-saves-combat/, runs the real
 * engagement (units_resolve_land_combat_ff_w with a presenter), renders the
 * dialog with combat_analysis_render through VICEROY.PAL and compares the
 * dialog rectangle, pixel for pixel, with a crop of a DOSBox-X screenshot of
 * VICEROY.EXE showing the same fight (test-saves-combat/NAME.dos.ppm, P6,
 * frame rect only). DOS's mouse cursor is in some captures: the mask in the
 * case table is skipped. Recipe: docs/combat_analysis.md "Pixel reference".
 *
 * A layout regression (frame, title, columns, rows, icons, ink) fails here
 * with the first differing pixel and the count.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/combat_analysis.h"
#include "core/font_file.h"
#include "core/popup.h"
#include "core/ui_colors.h"
#include "core/unit_chrome.h"
#include "core/units_combat.h"
#include "core/units_internal.h"
#include "core/world.h"
#include "tools/render_common.h"

#include "../common/test_runner.h"

#define MASKS 6

typedef struct Case {
  const char* name;
  int atk_x, atk_y, def_x, def_y;
  int village; /* tribe nation when the defender is the village's temporary Brave, else 0 */
  int roll; /* cheat footer roll shown by DOS, 0 = no footer */
  int mask[MASKS][4]; /* x, y, w, h in the frame: DOS mouse cursor, badge letter (see k_cases_data) */
} Case;

typedef struct Capture {
  const CombatAnalysisDialog* dlg;
  ColonizeFont font;
  ColonizeSpriteSheet wood, icons, terrain, phys0;
  const ColonizeWorldMap* map;
  ColonizePalette pal;
  uint8_t pix[320 * 200];
  bool done;
} Capture;

static Capture g_cap;
static CombatAnalysisDialog g_dlg;

static void present(const ColonizeCombatEngagement* eng, void* user) {
  (void)user;
  const ColonizeUnitPool* pool = (const ColonizeUnitPool*)user;
  if (g_cap.done || !combat_analysis_open(&g_dlg, pool, eng)) {
    return;
  }
  ColonizeFramebuffer8 fb;
  render_fb_init(&fb, g_cap.pix);
  ColonizePopupColors cols;
  popup_colors_from_ui(&cols);
  combat_analysis_render(&g_dlg, &g_cap.font, &g_cap.wood, &g_cap.icons, &g_cap.terrain,
                         &g_cap.phys0, g_cap.map, &cols, COLONIZE_COL_BASIC, COLONIZE_COL_HILITE,
                         NULL, &fb);
  g_cap.done = true;
}

static bool read_ppm(const char* path, int* w, int* h, uint8_t** rgb) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  int maxv = 0;
  if (fscanf(f, "P6 %d %d %d", w, h, &maxv) != 3 || fgetc(f) == EOF) {
    fclose(f);
    return false;
  }
  *rgb = malloc((size_t)*w * (size_t)*h * 3);
  const bool ok = *rgb && fread(*rgb, 3, (size_t)*w * (size_t)*h, f) == (size_t)*w * (size_t)*h;
  fclose(f);
  return ok;
}

/* Loads the fixture, runs the engagement with `seed` and leaves the rendered dialog in
 * g_cap / g_dlg. Returns 0, TR_SKIP or 1; `total` = the footer's w_atk + w_def. */
static int present_case(const Case* c, uint32_t seed, int* total) {
  char path[256];
  RenderSaveBundle* b = calloc(1, sizeof(*b));
  snprintf(path, sizeof(path), "test-saves-combat/%s.SAV", c->name);
  if (!b || !render_load_save("COLONIZE", path, b)) {
    fprintf(stderr, "SKIP %s: save or COLONIZE/ assets\n", c->name);
    free(b);
    return TR_SKIP;
  }
  memset(&g_cap, 0, sizeof(g_cap));
  memset(&g_dlg, 0, sizeof(g_dlg));
  if (!render_load_font("COLONIZE", "FONTTINY.FF", &g_cap.font) ||
      !render_load_sheet("COLONIZE", "WOODTILE.SS", &g_cap.wood) ||
      !render_load_sheet("COLONIZE", "ICONS.SS", &g_cap.icons) ||
      !render_load_sheet("COLONIZE", "TERRAIN.SS", &g_cap.terrain) ||
      !render_load_sheet("COLONIZE", "PHYS0.SS", &g_cap.phys0) ||
      !assets_load_palette("COLONIZE", &g_cap.pal)) {
    free(b);
    return TR_SKIP;
  }
  g_cap.map = &b->map;
  /* Game state the engagement reads (game_loop_orders.c sets the same). Cheats + DS:0x894
   * bit 3 make DOS show foreign units' AI plan glyph in the badge; the captures were taken
   * that way. */
  units_set_ff_col1(&b->save);
  units_set_combat_human_nation(b->human);
  colonies_set_col1_context(&b->save);
  units_set_occupancy_map(&b->map);
  colonies_set_occupancy_map(&b->map);
  units_set_native_fallout_context(&b->save, &b->map);
  units_set_combat_colonies(&b->colonies);
  unit_chrome_set_plan_nations(0xEu);
  const int aid = units_id_at(&b->units, c->atk_x, c->atk_y);
  const int did = c->village
    ? units_spawn_village_temp_defender(&b->units, &b->save, c->def_x, c->def_y, c->village, aid)
    : units_best_defender_at(&b->units, &b->save, c->def_x, c->def_y, aid, -1);
  ColonizeDosRng rng;
  dos_rng_seed(&rng, seed);
  combat_analysis_set_presenter(present, &b->units);
  if (did < 0) {
    /* Undefended colony: DOS spawns the militia phantom first (FUN_5fef_1b0e). */
    (void)units_revere_defend_colony_tile(&b->units, &b->colonies, aid, c->def_x, c->def_y, &rng);
  } else {
    (void)units_resolve_land_combat_ff_w(
      &(ColonizeWorld){.units = &b->units, .colonies = &b->colonies, .map = &b->map,
                       .col1 = &b->save, .col1_ok = true, .rng = &rng,
                       .human_nation = b->human},
      aid, did);
  }
  combat_analysis_set_presenter(NULL, NULL);
  unit_chrome_set_plan_nations(0);
  units_set_combat_human_nation(-1);
  *total = g_dlg.eng.w_atk + g_dlg.eng.w_def;
  free(b);
  if (!g_cap.done) {
    fprintf(stderr, "%s: no analysis was presented (atk %d def %d)\n", c->name, aid, did);
    return 1;
  }
  return 0;
}

static int run_case(const Case* c) {
  char path[256];
  int total = 0;
  int rc = present_case(c, 1, &total);
  /* The cheat footer prints the fight's roll. DOS's draw is not reproducible (its LCG is
   * seeded from the wall clock), so search for a seed whose draw is the captured value. */
  for (uint32_t seed = 2; rc == 0 && c->roll > 0 && g_dlg.eng.cheat_roll != c->roll && seed < 5000;
       ++seed) {
    rc = present_case(c, seed, &total);
  }
  if (rc == 0 && c->roll > 0 && g_dlg.eng.cheat_roll != c->roll) {
    fprintf(stderr, "%s: no seed draws roll %d\n", c->name, c->roll);
    rc = 1;
  }
  if (rc) {
    return rc;
  }
  int w = 0, h = 0;
  uint8_t* ref = NULL;
  snprintf(path, sizeof(path), "test-saves-combat/%s.dos.ppm", c->name);
  if (!read_ppm(path, &w, &h, &ref)) {
    fprintf(stderr, "%s: cannot read %s\n", c->name, path);
    return 1;
  }
  const int fx = g_dlg.dialog_x, fy = g_dlg.dialog_y;
  int bad = 0, first = -1;
  if (w != g_dlg.dialog_w || h != g_dlg.dialog_h) {
    fprintf(stderr, "%s: frame %dx%d, DOS %dx%d\n", c->name, g_dlg.dialog_w, g_dlg.dialog_h, w, h);
    bad = 1;
  } else {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        bool masked = false;
        for (int m = 0; m < MASKS; ++m) {
          masked |= x >= c->mask[m][0] && x < c->mask[m][0] + c->mask[m][2] &&
            y >= c->mask[m][1] && y < c->mask[m][1] + c->mask[m][3];
        }
        if (masked) {
          continue;
        }
        const uint8_t* p = g_cap.pal.rgb[g_cap.pix[(fy + y) * 320 + fx + x]];
        const uint8_t* r = ref + ((size_t)y * (size_t)w + (size_t)x) * 3;
        /* Palette slots 120..127 rotate (CYCLE.DAT): DOS was caught mid-cycle. */
        bool pc = false, rc2 = false;
        for (int k = 120; k < 128; ++k) {
          const uint8_t* q = g_cap.pal.rgb[k];
          pc |= p[0] == q[0] && p[1] == q[1] && p[2] == q[2];
          rc2 |= r[0] == q[0] && r[1] == q[1] && r[2] == q[2];
        }
        if ((p[0] != r[0] || p[1] != r[1] || p[2] != r[2]) && !(pc && rc2)) {
          if (first < 0) {
            first = y * w + x;
          }
          ++bad;
        }
      }
    }
    if (bad) {
      fprintf(stderr, "%s: %d pixels differ from DOS, first at frame (%d,%d)\n", c->name, bad,
              first % w, first / w);
    }
  }
  const char* dump = getenv("COMBAT_ANALYSIS_DUMP"); /* dir: writes NAME.port.ppm (author aid) */
  if (dump && bad) {
    snprintf(path, sizeof(path), "%s/%s.port.ppm", dump, c->name);
    FILE* f = fopen(path, "wb");
    if (f) {
      fprintf(f, "P6\n%d %d\n255\n", g_dlg.dialog_w, g_dlg.dialog_h);
      for (int y = 0; y < g_dlg.dialog_h; ++y) {
        for (int x = 0; x < g_dlg.dialog_w; ++x) {
          fwrite(g_cap.pal.rgb[g_cap.pix[(fy + y) * 320 + fx + x]], 1, 3, f);
        }
      }
      fclose(f);
    }
  }
  free(ref);
  return bad ? 1 : 0;
}

/*
 * Known deltas masked here (each is outside combat_analysis_render.c; remove the mask when fixed):
 *  - cursor: the DOS mouse pointer sits in every capture, (158..178, 98..116) on screen.
 *  - plan badge: DOS draws the AI plan glyph '?' 1px right of where unit_chrome_draw.c centres
 *    the ink, and tribe flags/marks use DS:0x848[nation] as it stands at run time (nation 10 =
 *    118, the port table holds 117, unit_chrome.c k_tribe_colors).
 *  - militia header: an empty colony's DOS phantom defender (FUN_5fef_1b0e 0x200 arm) shows the
 *    stand-in colonist's PROFESSION name and icon ("Pioneer") in the header and the footer;
 *    combat_analysis.c names the type ("Colonists").
 */
#define CURSOR(fy) {105, (fy), 20, 18}
#define PHANTOM_HDR {107, 23, 80, 17}
#define PHANTOM_FOOT(fy) {107, (fy), 50, 14}
static const Case k_cases_data[] = {
  /* human Veteran Soldier (50,37) vs French Soldier (50,38) */
  {"ca_soldier_open", 50, 37, 50, 38, 0, 2, {CURSOR(61), {113, 29, 9, 9}}},
  /* Veteran Soldier (51,37) vs undefended French Quebec (50,37), stockade */
  {"ca_colony_stockade", 51, 37, 50, 37, 0, 4, {CURSOR(61), PHANTOM_HDR, PHANTOM_FOOT(63)}},
  /* Veteran Soldier (51,37) vs undefended Quebec with a Fort */
  {"ca_colony_fort", 51, 37, 50, 37, 0, 4, {CURSOR(61), PHANTOM_HDR, PHANTOM_FOOT(63)}},
  /* Artillery (51,37) vs undefended Quebec, no fortification */
  {"ca_artillery_colony", 51, 37, 50, 37, 0, 7, {CURSOR(51), PHANTOM_HDR, PHANTOM_FOOT(63)}},
  /* Veteran Soldier (49,40) vs the Sioux capital (50,40): starburst over the village icon */
  {"ca_capital", 49, 40, 50, 40, 10, 1,
   {CURSOR(61), {113, 28, 10, 12}, {110, 47, 1, 1}, {119, 47, 1, 1}}},
  /* Veteran Soldier (48,36) vs the Sioux camp (47,36), Attack Village */
  {"ca_village", 48, 36, 47, 36, 10, 1,
   {CURSOR(61), {113, 28, 10, 12}, {110, 47, 1, 1}, {119, 47, 1, 1}, {116, 49, 1, 1}}},
};

#define CASE_FN(i) static int case_##i(void) { return run_case(&k_cases_data[i]); }
CASE_FN(0) CASE_FN(1) CASE_FN(2) CASE_FN(3) CASE_FN(4) CASE_FN(5)

static const TestCase k_cases[] = {
  {"ca_soldier_open", case_0}, {"ca_colony_stockade", case_1},
  {"ca_colony_fort", case_2}, {"ca_artillery_colony", case_3}, {"ca_capital", case_4},
  {"ca_village", case_5},
};

TEST_MAIN(k_cases)
