#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/ss.h"
#include "core/units.h"
#include "core/village_trade_intel.h"
#include "core/world.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

#include "../common/test_runner.h"

static int fail(const char* msg) {
  fprintf(stderr, "%s\n", msg);
  return 1;
}

/* Shared read-only fixture: LABELS.TXT catalog, WOODTILE panel and the
 * AMER2 map, loaded once and reused by every case below that needs them.
 * None of these cases mutate map/panel/labels themselves (only their own
 * local pixel buffers, tribe/col1 fixtures, etc.), so sharing is safe
 * across any case order. */
static ColonizeMsgCatalog g_labels;
static MapPanel g_panel;
static ColonizeWorldMap g_map;
static int g_shared_ready = 0; /* 0 = untried, 1 = ok, -1 = failed */

static void free_shared(void) {
  if (g_shared_ready == 1) {
    map_free(&g_map);
    map_panel_free(&g_panel);
    assets_msg_free(&g_labels);
  }
}

static int ensure_shared(void) {
  if (g_shared_ready != 0) {
    return g_shared_ready == 1 ? 0 : 1;
  }
  diag_init(0, NULL);

  assets_msg_init(&g_labels);
  if (!assets_msg_load_file(&g_labels, "COLONIZE/LABELS.TXT")) {
    fprintf(stderr, "Failed to load LABELS.TXT\n");
    g_shared_ready = -1;
    return 1;
  }
  if (!map_panel_load(&g_panel, "COLONIZE", &g_labels)) {
    assets_msg_free(&g_labels);
    fprintf(stderr, "map_panel_load failed\n");
    g_shared_ready = -1;
    return 1;
  }
  char err[256];
  if (!map_load_mp("COLONIZE/AMER2.MP", &g_map, err, sizeof(err))) {
    fprintf(stderr, "map_load_mp: %s\n", err);
    map_panel_free(&g_panel);
    assets_msg_free(&g_labels);
    g_shared_ready = -1;
    return 1;
  }
  g_shared_ready = 1;
  atexit(free_shared);
  return 0;
}

static int case_panel_geometry_constants(void) {
  if (MAP_PANEL_X != 240 || MAP_PANEL_W != 80) {
    fprintf(stderr, "panel x/w expected 240/80, got %d/%d\n", MAP_PANEL_X, MAP_PANEL_W);
    return 1;
  }
  if (MAP_VIEW_TILE_COLS != 15 || MAP_VIEW_TILE_ROWS != 12) {
    fprintf(
      stderr,
      "viewport tiles expected 15x12, got %dx%d\n",
      MAP_VIEW_TILE_COLS,
      MAP_VIEW_TILE_ROWS
    );
    return 1;
  }
  if (MAP_MENU_BAR_H != 8) {
    fprintf(stderr, "menu bar height expected 8, got %d\n", MAP_MENU_BAR_H);
    return 1;
  }
  if (MAP_VIEW_W != 240 || MAP_VIEW_H != 192 || MAP_VIEW_ORIGIN_Y != MAP_MENU_BAR_H) {
    return fail("viewport pixel geometry mismatch");
  }
  if (MAP_VIEW_ORIGIN_Y + MAP_VIEW_H != 200) {
    return fail("viewport + menu should fill 200px");
  }
  if (MAP_PANEL_MINIMAP_H != 39 || MAP_PANEL_MINIMAP_W != 56) {
    return fail("minimap window expected 56x39");
  }
  if (MAP_PANEL_TEXT_MARGIN != 2) {
    return fail("text margin expected 2");
  }
  return 0;
}

static int case_panel_load_and_labels(void) {
  if (ensure_shared() != 0) {
    return 1;
  }
  if (!g_panel.wood_ok) {
    return fail("WOODTILE.SS missing");
  }
  if (strcmp(g_panel.label_moves, "Moves:") != 0) {
    fprintf(stderr, "unexpected @INFO moves label: '%s'\n", g_panel.label_moves);
    return 1;
  }
  return 0;
}

static int case_map_load_size(void) {
  if (ensure_shared() != 0) {
    return 1;
  }
  if (g_map.width != 58 || g_map.height != 72) {
    fprintf(stderr, "AMER2 expected 58x72, got %ux%u\n", g_map.width, g_map.height);
    return 1;
  }
  return 0;
}

static int case_minimap_rect_and_origin(void) {
  if (ensure_shared() != 0) {
    return 1;
  }
  int mx, my, mw, mh, ox, oy;
  map_panel_minimap_rect(&g_map, 0, 0, 15, 12, &mx, &my, &mw, &mh, &ox, &oy);
  if (mw != 56 || mh != 39) {
    fprintf(stderr, "minimap size expected 56x39, got %dx%d\n", mw, mh);
    return 1;
  }
  /* DOS inset: window origin ≥ 1 (never includes the 1-tile rim). */
  if (ox != 1 || oy != 1) {
    fprintf(stderr, "expected origin 1,1 at NW of visible map, got %d,%d\n", ox, oy);
    return 1;
  }

  {
    int vx = -1;
    int vy = -1;
    map_panel_clamp_view_origin(58, 72, 0, 0, 15, 12, &vx, &vy);
    if (vx != 1 || vy != 1) {
      fprintf(stderr, "view origin NW expected 1,1 got %d,%d\n", vx, vy);
      return 1;
    }
    map_panel_clamp_view_origin(58, 72, 57, 71, 15, 12, &vx, &vy);
    if (vx != 42 || vy != 59) {
      fprintf(stderr, "view origin SE expected 42,59 got %d,%d\n", vx, vy);
      return 1;
    }
    /*
     * bugs.md #429: the AMER2 west-coast sea lane IS column 1 (high seas,
     * rows 33..70) and the east lane runs to column 56 — both must fall
     * inside the playable board and inside the scrolled-to-the-edge viewport.
     * DOS draws exactly columns 1..56 (FUN_6ba1_000c clamps the view origin to
     * [1, map_w - cols - 1] and FUN_6ba1_0d6c's tile loop is hard-clamped to
     * [DS:0x8328, DS:0x8804]); columns 0 and 57 are map data only.
     */
    map_panel_clamp_view_origin(58, 72, 1, 40, 15, 12, &vx, &vy);
    if (vx != 1) {
      fprintf(stderr, "west sea lane column 1 outside far-west viewport (origin %d)\n", vx);
      return 1;
    }
    if (!map_tile_is_high_seas(&g_map, 1, 40) || !map_coords_inset(&g_map, 1, 40)) {
      return fail("AMER2 (1,40) should be a playable high-seas lane tile");
    }
    map_panel_clamp_view_origin(58, 72, 56, 40, 15, 12, &vx, &vy);
    if (vx + 14 != 56) {
      fprintf(stderr, "east lane column 56 not the last drawn column (origin %d)\n", vx);
      return 1;
    }
    if (map_coords_inset(&g_map, 0, 10) || map_coords_inset(&g_map, 57, 10) ||
        !map_coords_inset(&g_map, 1, 1) || !map_coords_inset(&g_map, 56, 70)) {
      return fail("map_coords_inset rim/interior mismatch");
    }
  }
  if (my != MAP_PANEL_MINIMAP_ORIGIN_Y) {
    fprintf(stderr, "minimap y expected %d, got %d\n", MAP_PANEL_MINIMAP_ORIGIN_Y, my);
    return 1;
  }
  if (mx < MAP_PANEL_X || mx + mw > 320 || my < MAP_MENU_BAR_H) {
    fprintf(stderr, "minimap origin out of panel: %d,%d %dx%d\n", mx, my, mw, mh);
    return 1;
  }

  /* Scrolled window near south pole. */
  map_panel_minimap_rect(&g_map, 0, 50, 15, 12, &mx, &my, &mw, &mh, &ox, &oy);
  if (oy + mh > (int)g_map.height) {
    fprintf(stderr, "minimap window overflows map: oy=%d mh=%d\n", oy, mh);
    return 1;
  }

  int tx = -1;
  int ty = -1;
  if (!map_panel_minimap_click(&g_map, 0, 0, 15, 12, mx + 10, my + 20, &tx, &ty) ||
      tx != 11 || ty != 21) {
    fprintf(stderr, "minimap click map failed: %d,%d\n", tx, ty);
    return 1;
  }
  if (map_panel_minimap_click(&g_map, 0, 0, 15, 12, mx - 1, my, &tx, &ty) ||
      map_panel_minimap_click(&g_map, 0, 0, 15, 12, mx + mw, my, &tx, &ty)) {
    return fail("click outside minimap should miss");
  }
  if (!map_panel_contains_xy(MAP_PANEL_X, MAP_MENU_BAR_H) ||
      map_panel_contains_xy(MAP_PANEL_X - 1, MAP_MENU_BAR_H) ||
      map_panel_contains_xy(MAP_PANEL_X, MAP_MENU_BAR_H - 1)) {
    return fail("map_panel_contains_xy geometry wrong");
  }
  return 0;
}

static int case_render_basic(void) {
  if (ensure_shared() != 0) {
    return 1;
  }
  uint8_t* pixels = (uint8_t*)calloc(320 * 200, 1);
  if (!pixels) {
    return fail("oom");
  }
  ColonizeFramebuffer8 fb;
  fb.width = 320;
  fb.height = 200;
  fb.pixels = pixels;
  ColonizeWorld w_basic = world_make(NULL, NULL, &g_map, NULL, false, NULL, NULL);
  map_panel_render_w(
    &w_basic,
    &g_panel,
    NULL,
    NULL,
    NULL,
    NULL,
    5,
    5,
    MAP_VIEW_TILE_COLS,
    MAP_VIEW_TILE_ROWS,
    10,
    20,
    -1,
    0,
    1492,
    0,
    1000,
    0,
    "New England",
    NULL,
    false,
    false,
    &fb
  );

  /* Left edge of sidebar is the black rule. */
  if (pixels[MAP_MENU_BAR_H * 320 + MAP_PANEL_X] != 0) {
    fprintf(stderr, "expected black left edge at panel x\n");
    free(pixels);
    return 1;
  }

  /* Panel must not write into the map viewport (except the shared left rule column is panel). */
  int panel_nonzero = 0;
  for (int y = MAP_MENU_BAR_H; y < 200; ++y) {
    for (int x = 0; x < MAP_PANEL_X; ++x) {
      if (pixels[y * 320 + x] != 0) {
        fprintf(stderr, "panel render wrote left of x=%d at %d,%d = %u\n", MAP_PANEL_X, x, y,
          pixels[y * 320 + x]);
        free(pixels);
        return 1;
      }
    }
    for (int x = MAP_PANEL_X + 1; x < 320; ++x) {
      if (pixels[y * 320 + x] != 0) {
        panel_nonzero = 1;
      }
    }
  }
  if (!panel_nonzero) {
    free(pixels);
    return fail("panel region stayed blank");
  }

  int mx, my, mw, mh, ox, oy;
  map_panel_minimap_rect(&g_map, 5, 5, MAP_VIEW_TILE_COLS, MAP_VIEW_TILE_ROWS, &mx, &my, &mw, &mh,
    &ox, &oy);
  /* Dark-orange border pixel just above minimap terrain (touches menu rule above). */
  if (pixels[(my - 1) * 320 + mx] != 6) {
    fprintf(stderr, "minimap border color expected 6, got %u\n", pixels[(my - 1) * 320 + mx]);
    free(pixels);
    return 1;
  }
  /* No wood gap: section black separator sits on the row under the brown bottom. */
  if (pixels[(my + mh + 1) * 320 + MAP_PANEL_X + 2] != 0) {
    fprintf(stderr, "expected black section separator under minimap border\n");
    free(pixels);
    return 1;
  }

  /* View-rect corner should sit ON the edge tile of the view (world 5,5). */
  const int vx = mx + (5 - ox);
  const int vy = my + (5 - oy);
  if (vx < 0 || vy < 0 || vx >= 320 || vy >= 200 || pixels[vy * 320 + vx] != 15) {
    fprintf(
      stderr,
      "view rect missing at edge tile pixel %d,%d (got %u)\n",
      vx,
      vy,
      (vx >= 0 && vy >= 0 && vx < 320 && vy < 200) ? pixels[vy * 320 + vx] : 0u
    );
    free(pixels);
    return 1;
  }

  free(pixels);
  return 0;
}

static int case_selected_ship_hold_icons(void) {
  if (ensure_shared() != 0) {
    return 1;
  }
  uint8_t* pixels = (uint8_t*)calloc(320 * 200, 1);
  if (!pixels) {
    return fail("oom");
  }
  ColonizeFramebuffer8 fb;
  fb.width = 320;
  fb.height = 200;
  fb.pixels = pixels;

  /* Selected ship: With: hold icons (passengers) must paint into the sidebar. */
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    free(pixels);
    return fail("NAMES.TXT load failed for hold icon check");
  }
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  if (!units_load_types(&units, &names)) {
    assets_msg_free(&names);
    free(pixels);
    return fail("units_load_types failed");
  }
  units_new_world_start(&units, &g_map, 39, 10, 0, 0);
  ColonizeUnit* ship = units_get(&units, units.selected_id);
  if (!ship || ship->cargo_count < 2) {
    assets_msg_free(&names);
    free(pixels);
    return fail("starter ship missing passengers for With: check");
  }

  ColonizeSpriteSheet icons;
  memset(&icons, 0, sizeof(icons));
  char err2[256];
  const bool icons_ok = ss_load("COLONIZE/ICONS.SS", &icons, err2, sizeof(err2));

  memset(pixels, 0, 320 * 200);
  ColonizeWorld w_ship = world_make(&units, NULL, &g_map, NULL, false, NULL, NULL);
  map_panel_render_w(
    &w_ship,
    &g_panel,
    icons_ok ? &icons : NULL,
    NULL,
    &names,
    &g_labels,
    ship->x > 7 ? ship->x - 7 : 0,
    ship->y > 6 ? ship->y - 6 : 0,
    MAP_VIEW_TILE_COLS,
    MAP_VIEW_TILE_ROWS,
    ship->x,
    ship->y,
    ship->id,
    0,
    1492,
    0,
    1000,
    0,
    "England",
    NULL,
    false,
    false,
    &fb
  );

  int hold_pixels = 0;
  for (int y = 80; y < 160; ++y) {
    for (int x = MAP_PANEL_X + 2; x < 320; ++x) {
      if (pixels[y * 320 + x] != 0 && pixels[y * 320 + x] != 8) {
        hold_pixels++;
      }
    }
  }
  if (icons_ok) {
    ss_free(&icons);
  }
  assets_msg_free(&names);
  free(pixels);
  if (hold_pixels < 20) {
    fprintf(stderr, "expected With: hold icons in sidebar (opaque=%d)\n", hold_pixels);
    return 1;
  }
  return 0;
}

/*
 * Village chrome geometry (FUN_112b_0790, CODE_5:112b:09c4..0b8c): a threat
 * draws exclamation marks from tile_x+6, each a 3x7 surround with a 1x5 bar
 * inset (1,1) split by one surround pixel four rows down; the mission cross
 * follows as a 5x6 pad with a 1x4 vertical and a 3x1 horizontal bar. Both
 * were wrong before — the mission drew an exclamation mark rather than a
 * cross, and the marks were centred on a drifting row instead of anchored.
 * Self-contained: its own small map/colony/tribe fixtures, no dependency on
 * the shared AMER2 fixture above.
 */
static int case_village_chrome(void) {
  if (ensure_shared() != 0) { /* only for diag_init */
    return 1;
  }
  ColonizeSpriteSheet chrome_icons;
  memset(&chrome_icons, 0, sizeof(chrome_icons));
  char cerr[256];
  if (!ss_load("COLONIZE/ICONS.SS", &chrome_icons, cerr, sizeof(cerr))) {
    fprintf(stderr, "ICONS.SS for village chrome: %s\n", cerr);
    return 1;
  }
  ColonizeWorldMap cmap;
  memset(&cmap, 0, sizeof(cmap));
  if (!map_alloc(&cmap, 32, 32, cerr, sizeof(cerr))) {
    fprintf(stderr, "village chrome map_alloc: %s\n", cerr);
    ss_free(&chrome_icons);
    return 1;
  }
  for (int i = 0; i < 32 * 32; ++i) {
    cmap.terrain[i] = 2; /* plains */
  }
  map_reveal_all(&cmap, 0);

  ColonizeColonyPool ccol;
  colonies_init(&ccol);
  colonies_set_occupancy_map(NULL);
  const int ccid = colonies_found(&ccol, &cmap, 12, 10, 0, 0, 0, 0, 0, 0);
  ColonizeColony* ccolony = colonies_get_mut(&ccol, ccid);
  ccolony->colonist_count = 10;
  ccolony->population = 10;

  ColonizeCol1Tribe ctribe;
  memset(&ctribe, 0, sizeof(ctribe));
  ctribe.x = 10;
  ctribe.y = 10;
  ctribe.nation_id = 4;
  ctribe.population = 5;
  ctribe.mission = 0x00;          /* English, plain (not Jesuit) */
  ctribe.alarm[0].friction = 96;  /* >> 5 = tier 3 */
  ColonizeCol1Save ccol1;
  memset(&ccol1, 0, sizeof(ccol1));
  /*
   * 2026-09-06d: unclaimed Fathers are -1 (col1_save_init), not 0. Left at
   * 0 the head says nation 0 owns every Father, and the alarm-mark count
   * now depends on FF 16 Pocahontas (halves the threat score — the
   * de-stubbed FUN_281f_07b4(nation, 0x10) in ai_indian_village_threat).
   */
  for (size_t fi = 0; fi < sizeof(ccol1.head.founding_father); ++fi) {
    ccol1.head.founding_father[fi] = -1;
  }
  ccol1.head.tribe_count = 1;
  ccol1.head.difficulty = 2;
  ccol1.tribe = &ctribe;
  ccol1.indian[0].tech = 1;

  uint8_t cpx[32 * 24];
  memset(cpx, 0xff, sizeof(cpx));
  ColonizeFramebuffer8 cfb = {.width = 32, .height = 24, .pixels = cpx};
  ColonizeWorld w_ctribe = world_make(NULL, &ccol, &cmap, &ccol1, true, NULL, NULL);
  map_panel_render_tribes_on_map_w(&w_ctribe, &chrome_icons, &cfb,
                                    10, 10, 2, 2, 16, 16, 0, 0, 0, NULL);

  /*
   * smell_audit_2026-09-09 #100: "Complete Map" makes game_fog_nation()
   * hand this renderer −1. DOS keys the alarm marks off DS:0x5396
   * (head.curr_nation_map_view), which is never a sentinel, and draws the
   * mission cross outside that branch entirely — so the chrome must be
   * identical to the nation-0 view, not silently dropped.
   */
  ccol1.head.curr_nation_map_view = 0;
  ccol1.head.human_player = 0;
  uint8_t xpx[32 * 24];
  memset(xpx, 0xff, sizeof(xpx));
  ColonizeFramebuffer8 xfb = {.width = 32, .height = 24, .pixels = xpx};
  ColonizeWorld w_xtribe = world_make(NULL, &ccol, &cmap, &ccol1, true, NULL, NULL);
  map_panel_render_tribes_on_map_w(&w_xtribe, &chrome_icons, &xfb,
                                    10, 10, 2, 2, 16, 16, 0, 0, -1, NULL);
  const int complete_map_chrome_ok = memcmp(xpx, cpx, sizeof(cpx)) == 0;

  /*
   * bugs.md #364: the Dutch shade is DS:0x848 index 13, which is ICONS.SS-
   * native orange (255,113,0) but plain EGA magenta in every other
   * palette. Given the active palette, the cross must land on whatever
   * index actually holds that orange — here a deliberately relocated slot
   * 200 — never on the magenta 13.
   */
  ColonizePalette cpal;
  memset(&cpal, 0, sizeof(cpal));
  cpal.rgb[13][0] = 255; cpal.rgb[13][1] = 85;  cpal.rgb[13][2] = 255; /* magenta */
  cpal.rgb[5][0] = 170;  cpal.rgb[5][1] = 0;    cpal.rgb[5][2] = 170;
  cpal.rgb[200][0] = 255; cpal.rgb[200][1] = 113; cpal.rgb[200][2] = 0;  /* orange */
  cpal.rgb[201][0] = 170; cpal.rgb[201][1] = 73;  cpal.rgb[201][2] = 0;
  ctribe.mission = 0x13; /* Dutch (3) + Jesuit bit 0x10 */
  uint8_t dpx[32 * 24];
  memset(dpx, 0xff, sizeof(dpx));
  ColonizeFramebuffer8 dfb = {.width = 32, .height = 24, .pixels = dpx};
  ColonizeWorld w_dtribe = world_make(NULL, &ccol, &cmap, &ccol1, true, NULL, NULL);
  map_panel_render_tribes_on_map_w(&w_dtribe, &chrome_icons, &dfb,
                                    10, 10, 2, 2, 16, 16, 0, 0, 0, &cpal);
  /*
   * The cross's x depends on how many alarm marks precede it, so count
   * shades instead of pinning columns: the cross is 6 pixels (4-tall bar
   * + 3-wide arm sharing one), all of them the orange index, and the
   * magenta 13 must appear nowhere.
   */
  int orange_px = 0;
  int magenta_px = 0;
  for (int i = 0; i < 32 * 24; ++i) {
    if (dpx[i] == 200) {
      orange_px++;
    }
    if (dpx[i] == 13) {
      magenta_px++;
    }
  }
  const int dutch_cross_ok = orange_px == 6 && magenta_px == 0;
  ctribe.mission = 0x00;

  ss_free(&chrome_icons);

#define CHROME_AT(x, y) cpx[(y) * 32 + (x)]
  /* Alarm mark: bar at x=7 rows 5..7 and 9, surround pixel at row 8. */
  int mark_ok = CHROME_AT(6, 4) == 0 && CHROME_AT(8, 10) == 0 &&
                CHROME_AT(7, 5) == 0x0c && CHROME_AT(7, 7) == 0x0c &&
                CHROME_AT(7, 8) == 0 && CHROME_AT(7, 9) == 0x0c;
  /* Mission cross at pad x=10: vertical at x=12 rows 6..9, horizontal row 7. */
  const uint8_t cross = (uint8_t)(12u - 8u); /* English (12), plain → −8 */
  int cross_ok = CHROME_AT(10, 5) == 0 && CHROME_AT(14, 10) == 0 &&
                 CHROME_AT(12, 6) == cross && CHROME_AT(12, 9) == cross &&
                 CHROME_AT(11, 7) == cross && CHROME_AT(13, 7) == cross &&
                 CHROME_AT(11, 6) == 0 && CHROME_AT(13, 8) == 0;
#undef CHROME_AT
  map_free(&cmap);
  if (!mark_ok) {
    return fail("village alarm mark geometry wrong");
  }
  if (!cross_ok) {
    return fail("village mission cross geometry wrong (should be a cross, not a bang)");
  }
  if (!dutch_cross_ok) {
    return fail("Dutch mission cross not palette-adapted (bugs.md #364: pink, not orange)");
  }
  if (!complete_map_chrome_ok) {
    return fail("Complete Map (fog nation -1) dropped village alarm/mission chrome");
  }
  return 0;
}

/* Tribe settlement (#10 tipis for tech 0) blits on the main map viewport. */
static int case_tribe_settlement_icon(void) {
  ColonizeSpriteSheet icons;
  memset(&icons, 0, sizeof(icons));
  char err2[256];
  if (!ss_load("COLONIZE/ICONS.SS", &icons, err2, sizeof(err2))) {
    fprintf(stderr, "ICONS.SS for tribe marker: %s\n", err2);
    return 1;
  }
  if (icons.sprite_count < 14 || icons.sprites[10].width != 21) {
    fprintf(stderr, "expected ICONS #10 tribe settlement 21px wide\n");
    ss_free(&icons);
    return 1;
  }
  ColonizeCol1Tribe tribe;
  memset(&tribe, 0, sizeof(tribe));
  tribe.x = 3;
  tribe.y = 4;
  tribe.nation_id = 4;
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.head.tribe_count = 1;
  col1.tribe = &tribe;
  /* nation 4 → indian[0]; tech 0 → tipis #10 */
  col1.indian[0].tech = 0;

  uint8_t tile_px[16 * 16];
  memset(tile_px, 0, sizeof(tile_px));
  ColonizeFramebuffer8 tile_fb = {.width = 16, .height = 16, .pixels = tile_px};
  ColonizeWorld w_tile = world_make(NULL, NULL, NULL, &col1, true, NULL, NULL);
  map_panel_render_tribes_on_map_w(
    &w_tile, &icons, &tile_fb, 3, 4, 1, 1, 16, 16, 0, 0, 0, NULL);

  int opaque = 0;
  for (int i = 0; i < 16 * 16; ++i) {
    if (tile_px[i] != 0 && tile_px[i] != COLONIZE_SS_TRANSPARENT) {
      ++opaque;
    }
  }
  ss_free(&icons);
  if (opaque < 8) {
    fprintf(stderr, "expected tribe map icon opaque pixels, got %d\n", opaque);
    return 1;
  }
  return 0;
}

/*
 * Tile stack (DOS FUN_1427_04d6 order + FUN_281f_02ee chain walk): every unit
 * standing on the tile plus everyone aboard a transport standing there, the
 * transport listed first. Self-contained NAMES/unit pool fixture.
 */
static int case_tile_stack(void) {
  ColonizeMsgCatalog names;
  memset(&names, 0, sizeof(names));
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    /* Matches the original: silently skip if NAMES.TXT can't be located. */
    return 0;
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    assets_msg_free(&names);
    return fail("units_load_types failed");
  }

  /* Caravel (@UNIT 13) with a Soldier (1) and a Pioneer (2) aboard, plus a
   * loose Colonist (0) standing on the same tile. */
  const int ship = units_spawn_allow_stack(&pool, 13, 7, 9);
  const int soldier = units_spawn_allow_stack(&pool, 1, 7, 9);
  const int pioneer = units_spawn_allow_stack(&pool, 2, 7, 9);
  const int loose = units_spawn_allow_stack(&pool, 0, 7, 9);
  if (ship < 0 || soldier < 0 || pioneer < 0 || loose < 0) {
    assets_msg_free(&names);
    return fail("stack spawn failed");
  }
  units_get(&pool, soldier)->aboard_ship_id = ship;
  units_get(&pool, pioneer)->aboard_ship_id = ship;

  int ids[COLONIZE_UNITS_MAX];
  const int n = map_panel_collect_stack(&pool, 7, 9, ids, COLONIZE_UNITS_MAX);
  if (n != 4) {
    fprintf(stderr, "stack should list carried units too, got %d\n", n);
    assets_msg_free(&names);
    return 1;
  }
  if (ids[0] != ship) {
    fprintf(stderr, "transport should sort to the head of the stack\n");
    assets_msg_free(&names);
    return 1;
  }
  /* A neighbouring tile must not pick any of them up. */
  if (map_panel_collect_stack(&pool, 8, 9, ids, COLONIZE_UNITS_MAX) != 0) {
    assets_msg_free(&names);
    return fail("stack leaked onto a neighbouring tile");
  }
  assets_msg_free(&names);
  return 0;
}

/*
 * Linux-only village trade intel: "Buys:" / "Sells:" icon rows under the
 * settlement line, each only once that half has been disclosed to the
 * viewing player; reset and village removal forget it. Uses the shared
 * panel/labels fixture but its own map/pixels; explicitly resets the
 * village_trade_intel module's own statics at both ends so it neither
 * inherits nor leaks state relative to other cases.
 */
static int case_village_trade_intel(void) {
  if (ensure_shared() != 0) {
    return 1;
  }
  village_trade_intel_reset();
  int got_b[3];
  int got_s[3];
  int nb = -1;
  int ns = -1;
  const int want[3] = {12, 11, 16}; /* 16 is not a cargo id: dropped */
  const int sell[3] = {1, 2, 3};
  village_trade_intel_note_buys(0, 10, 10, want, 3);
  if (!village_trade_intel_get(0, 10, 10, got_b, &nb, got_s, &ns) || nb != 2 || ns != 0 ||
      got_b[0] != 12 || got_b[1] != 11) {
    return fail("village intel: buys half not recorded");
  }
  if (village_trade_intel_get(1, 10, 10, got_b, &nb, got_s, &ns) || nb != 0) {
    return fail("village intel leaked to another European nation");
  }
  if (village_trade_intel_get(0, 11, 10, got_b, &nb, got_s, &ns)) {
    return fail("village intel leaked to another tile");
  }
  village_trade_intel_forget_tile(10, 10);
  if (village_trade_intel_get(0, 10, 10, got_b, &nb, got_s, &ns)) {
    return fail("village intel survived forget_tile");
  }

  ColonizeSpriteSheet iicons;
  memset(&iicons, 0, sizeof(iicons));
  char ierr[256];
  if (!ss_load("COLONIZE/ICONS.SS", &iicons, ierr, sizeof(ierr))) {
    fprintf(stderr, "ICONS.SS for village intel: %s\n", ierr);
    return 1;
  }
  ColonizeWorldMap imap;
  memset(&imap, 0, sizeof(imap));
  if (!map_alloc(&imap, 32, 32, ierr, sizeof(ierr))) {
    fprintf(stderr, "village intel map_alloc: %s\n", ierr);
    ss_free(&iicons);
    return 1;
  }
  for (int i = 0; i < 32 * 32; ++i) {
    imap.terrain[i] = 2;
  }
  map_reveal_all(&imap, 0);
  ColonizeCol1Tribe itribe;
  memset(&itribe, 0, sizeof(itribe));
  itribe.x = 10;
  itribe.y = 10;
  itribe.nation_id = 4;
  itribe.population = 5;
  itribe.mission = COL1_TRIBE_MISSION_NONE;
  ColonizeCol1Save icol1;
  memset(&icol1, 0, sizeof(icol1));
  for (size_t fi = 0; fi < sizeof(icol1.head.founding_father); ++fi) {
    icol1.head.founding_father[fi] = -1;
  }
  for (int n = 1; n < 4; ++n) {
    icol1.player[n].control = 1;
  }
  icol1.head.tribe_count = 1;
  icol1.tribe = &itribe;

  uint8_t* pixels = (uint8_t*)calloc(320 * 200, 1);
  if (!pixels) {
    ss_free(&iicons);
    map_free(&imap);
    return fail("oom");
  }
  ColonizeFramebuffer8 ifb = {.width = 320, .height = 200, .pixels = pixels};
  uint8_t* shot[3] = {NULL, NULL, NULL};
  for (int pass = 0; pass < 3; ++pass) {
    village_trade_intel_reset();
    if (pass >= 1) {
      village_trade_intel_note_buys(0, 10, 10, want, 2);
    }
    if (pass >= 2) {
      village_trade_intel_note_sells(0, 10, 10, sell, 3);
    }
    memset(pixels, 0, 320 * 200);
    ColonizeWorld w_intel = world_make(NULL, NULL, &imap, &icol1, true, NULL, NULL);
    map_panel_render_w(
      &w_intel, &g_panel, &iicons, NULL, NULL, &g_labels, 3, 3,
      MAP_VIEW_TILE_COLS, MAP_VIEW_TILE_ROWS, 10, 10, -1, 0, 1492, 0, 1000, 0, "England",
      NULL, false, false, &ifb
    );
    shot[pass] = malloc(320 * 200);
    if (shot[pass]) {
      memcpy(shot[pass], pixels, 320 * 200);
    }
  }
  village_trade_intel_reset();
  ss_free(&iicons);
  map_free(&imap);
  free(pixels);
  if (!shot[0] || !shot[1] || !shot[2]) {
    for (int k = 0; k < 3; ++k) {
      free(shot[k]);
    }
    return fail("village intel: out of memory");
  }
  /* First differing row between two renders, −1 when identical. */
  int first_diff[2] = {-1, -1};
  for (int k = 0; k < 2; ++k) {
    for (int y = 0; y < 200 && first_diff[k] < 0; ++y) {
      if (memcmp(shot[k] + y * 320, shot[k + 1] + y * 320, 320) != 0) {
        first_diff[k] = y;
      }
    }
  }
  for (int k = 0; k < 3; ++k) {
    free(shot[k]);
  }
  /* Buys row appears below the village line; Sells row one row further down. */
  if (first_diff[0] < 0 || first_diff[1] < 0 || first_diff[1] <= first_diff[0]) {
    fprintf(stderr, "village intel rows missing/misordered (buys y=%d, sells y=%d)\n",
            first_diff[0], first_diff[1]);
    return 1;
  }
  return 0;
}

/*
 * bugs.md #630: FUN_49dd_0424 raw 79185-79196 prefixes the stack row's tools
 * detail with LABELS @MISC row 4 ("Expert") when the profession byte is 0x14
 * (Hardy Pioneer), and spells it without the parentheses the selected-unit
 * block (raw 78897-78918) uses.
 */
static int case_stack_detail_expert_prefix(void) {
  ColonizeMsgCatalog names;
  memset(&names, 0, sizeof(names));
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    return 0;
  }
  reports_names_load_catalogs("COLONIZE");
  const char* expert = reports_misc_display_word(4, "");
  if (!expert || !expert[0]) {
    assets_msg_free(&names);
    return fail("LABELS @MISC row 4 missing");
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    assets_msg_free(&names);
    return fail("units_load_types failed");
  }
  const int id = units_spawn_allow_stack(&pool, 2, 7, 9); /* Pioneers */
  if (id < 0) {
    assets_msg_free(&names);
    return fail("pioneer spawn failed");
  }
  ColonizeUnit* u = units_get(&pool, id);
  u->tools = 100;
  u->profession = UNITS_JOB_NONE;
  char plain[72];
  char hardy[72];
  if (!map_panel_stack_detail_text(&pool, u, &names, plain, sizeof(plain))) {
    assets_msg_free(&names);
    return fail("stack detail (plain) not produced");
  }
  u->profession = UNITS_JOB_PIONEER; /* DOS 0x14 */
  if (!map_panel_stack_detail_text(&pool, u, &names, hardy, sizeof(hardy))) {
    assets_msg_free(&names);
    return fail("stack detail (hardy) not produced");
  }
  char want[128];
  snprintf(want, sizeof(want), "%s %s", expert, plain);
  if (strcmp(hardy, want) != 0) {
    fprintf(stderr, "stack detail: want '%s' got '%s'\n", want, hardy);
    assets_msg_free(&names);
    return 1;
  }
  if (strncmp(plain, "100 ", 4) != 0 || strchr(plain, '(') || strchr(hardy, '(')) {
    fprintf(stderr, "stack detail should be '100 <Tools>' with no parens: '%s'\n", plain);
    assets_msg_free(&names);
    return 1;
  }
  assets_msg_free(&names);
  return 0;
}

/*
 * bugs.md #642 / #643: the Treasure detail word comes from LABELS @CTITLE row
 * 1 (the DOS DS:0x93a0 slot, FUN_49dd_0424 raw 78911 / 79210), and the type-2
 * tools row is gated on the @UNIT type alone (raw 78896), so a 0-tools
 * Pioneers body still prints its tools line.
 */
static int case_treasure_word_and_zero_tools(void) {
  ColonizeMsgCatalog names;
  memset(&names, 0, sizeof(names));
  char names_path[512];
  if (!dos_compat_normalize_asset_path("COLONIZE", "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    return 0;
  }
  reports_names_load_catalogs("COLONIZE");
  const char* gold = reports_ctitle_word(1);
  if (!gold || !gold[0]) {
    assets_msg_free(&names);
    return fail("LABELS @CTITLE row 1 missing");
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!units_load_types(&pool, &names)) {
    assets_msg_free(&names);
    return fail("units_load_types failed");
  }
  int rc = 0;
  const int tid = units_spawn_allow_stack(&pool, 10, 5, 5); /* Treasure */
  if (tid < 0) {
    assets_msg_free(&names);
    return fail("treasure spawn failed");
  }
  ColonizeUnit* t = units_get(&pool, tid);
  t->profession = 7; /* DOS +0x315b, x100 */
  char want[128];
  char got[72];
  snprintf(want, sizeof(want), "%s 700", gold);
  if (!map_panel_stack_detail_text(&pool, t, &names, got, sizeof(got)) ||
      strcmp(got, want) != 0) {
    fprintf(stderr, "treasure stack row: want '%s' got '%s'\n", want, got);
    rc = 1;
  }
  const int pid = units_spawn_allow_stack(&pool, 2, 7, 9); /* Pioneers */
  if (pid < 0) {
    assets_msg_free(&names);
    return fail("pioneer spawn failed");
  }
  ColonizeUnit* u = units_get(&pool, pid);
  u->tools = 0;
  u->profession = UNITS_JOB_NONE;
  if (!map_panel_stack_detail_text(&pool, u, &names, got, sizeof(got)) ||
      strncmp(got, "0 ", 2) != 0) {
    fprintf(stderr, "0-tools pioneer stack row: got '%s'\n", got);
    rc = 1;
  }
  assets_msg_free(&names);
  return rc;
}

static const TestCase k_cases[] = {
  {"panel_geometry_constants", case_panel_geometry_constants},
  {"panel_load_and_labels", case_panel_load_and_labels},
  {"map_load_size", case_map_load_size},
  {"minimap_rect_and_origin", case_minimap_rect_and_origin},
  {"render_basic", case_render_basic},
  {"selected_ship_hold_icons", case_selected_ship_hold_icons},
  {"village_chrome", case_village_chrome},
  {"tribe_settlement_icon", case_tribe_settlement_icon},
  {"tile_stack", case_tile_stack},
  {"village_trade_intel", case_village_trade_intel},
  {"stack_detail_expert_prefix", case_stack_detail_expert_prefix},
  {"treasure_word_and_zero_tools", case_treasure_word_and_zero_tools},
};

TEST_MAIN(k_cases)
