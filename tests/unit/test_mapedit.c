/* MAPEDIT.EXE port (src/core/mapedit.c, docs/mapedit.md). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/mapedit.h"
#include "tests/common/test_runner.h"

static MapEditor g_ed;

/* A 20x16 ocean map, view centred, no assets: enough for the paint and palette rules. */
static MapEditor* blank_editor(void) {
  memset(&g_ed, 0, sizeof g_ed);
  if (!map_alloc(&g_ed.map, 20, 16, NULL, 0)) {
    return NULL;
  }
  memset(g_ed.map.terrain, 0x19, g_ed.map.tile_count);
  g_ed.map_ok = true;
  g_ed.view_cx = 10;
  g_ed.view_cy = 8;
  g_ed.brush_type = 0x19;
  g_ed.brush_and = 0x40;
  g_ed.palette_idx = 1;
  return &g_ed;
}

#define AT(x, y) g_ed.map.terrain[(y) * g_ed.map.width + (x)]

/* FUN_1000_1be0: paint, pick up, features, Coast Protect. */
static int case_paint_rules(void) {
  MapEditor* ed = blank_editor();
  if (!ed) return 1;
  mapedit_palette_pick(ed, 3); /* Plains (row 0, col 2) */
  mapedit_paint_tile(ed, 5, 5, false);
  if (AT(5, 5) != 2) { fprintf(stderr, "plains %02x\n", AT(5, 5)); return 1; }
  ed->coast_protect = true;
  mapedit_paint_tile(ed, 6, 5, false); /* ocean is protected */
  if (AT(6, 5) != 0x19) { fprintf(stderr, "coast protect %02x\n", AT(6, 5)); return 1; }
  mapedit_palette_pick(ed, 23); /* hills: OR 0x20, AND 0x5f */
  mapedit_paint_tile(ed, 5, 5, false);
  if (AT(5, 5) != 0x22) { fprintf(stderr, "hills %02x\n", AT(5, 5)); return 1; }
  mapedit_palette_pick(ed, 22); /* mountains: AND 0x1f OR 0xa0 */
  mapedit_paint_tile(ed, 5, 5, false);
  if (AT(5, 5) != 0xa2) { fprintf(stderr, "mountains %02x\n", AT(5, 5)); return 1; }
  mapedit_palette_pick(ed, 21); /* minor river removal (rmb): AND 0x3f -> mountain becomes hill */
  mapedit_paint_tile(ed, 5, 5, true);
  if (AT(5, 5) != 0x22) { fprintf(stderr, "remove minor river %02x\n", AT(5, 5)); return 1; }
  ed->coast_protect = false;
  mapedit_palette_pick(ed, 23); /* hills never land on water */
  mapedit_paint_tile(ed, 7, 5, false);
  if (AT(7, 5) != 0x19) { fprintf(stderr, "hills on ocean %02x\n", AT(7, 5)); return 1; }
  mapedit_palette_pick(ed, 21); /* rivers do: the estuary marker */
  mapedit_paint_tile(ed, 7, 5, false);
  if (AT(7, 5) != 0x59) { fprintf(stderr, "river on ocean %02x\n", AT(7, 5)); return 1; }
  mapedit_palette_pick(ed, 1); /* terrain mode: rmb picks the tile up */
  mapedit_paint_tile(ed, 7, 5, true);
  if (ed->brush_type != 0x19 || ed->brush_and != 0x40 || ed->brush_or != 0 || AT(7, 5) != 0x59) {
    return 1;
  }
  mapedit_paint_tile(ed, 0, 5, false); /* the border is never edited */
  if (AT(0, 5) != 0x19) return 1;
  map_free(&ed->map);
  return 0;
}

/* FUN_1000_1226 index walk. */
static int case_palette_walk(void) {
  MapEditor* ed = blank_editor();
  if (!ed) return 1;
  mapedit_palette_pick(ed, 1);
  mapedit_palette_key(ed, COLONIZE_KEY_LEFT); /* 1 - 1 - 1 = -1 -> +8 = 7 -> 8 */
  if (ed->palette_idx != 8) { fprintf(stderr, "left %d\n", ed->palette_idx); return 1; }
  mapedit_palette_key(ed, COLONIZE_KEY_UP); /* index 8 goes down a row */
  if (ed->palette_idx != 16) { fprintf(stderr, "up from 8 %d\n", ed->palette_idx); return 1; }
  mapedit_palette_key(ed, COLONIZE_KEY_DOWN); /* index 16 goes up a row */
  if (ed->palette_idx != 8) return 1;
  mapedit_palette_pick(ed, 23);
  mapedit_palette_key(ed, COLONIZE_KEY_RIGHT); /* 23 = 0x17 -> 0 -> 1 */
  if (ed->palette_idx != 1 || ed->brush_feature) return 1;
  map_free(&ed->map);
  return 0;
}

/* FUN_19c4_0002: two islands and two separate seas. */
static int case_find_continents(void) {
  ColonizeWorldMap m;
  memset(&m, 0, sizeof m);
  if (!map_alloc(&m, 12, 8, NULL, 0)) return 1;
  memset(m.terrain, 0x19, m.tile_count);
  for (int y = 2; y < 6; ++y) {
    m.terrain[y * 12 + 2] = 2;
    m.terrain[y * 12 + 3] = 2;
    m.terrain[y * 12 + 8] = 4;
  }
  const int flags = mapedit_find_continents(&m);
  const uint8_t a = m.layer3[3 * 12 + 2];
  const uint8_t b = m.layer3[3 * 12 + 8];
  const uint8_t sea = m.layer3[1 * 12 + 5];
  const int ok = flags == 0 && a != 0 && b != 0 && a != b && sea != 0 && m.layer3[0] == 0;
  if (!ok) fprintf(stderr, "flags %d a %u b %u sea %u\n", flags, a, b, sea);
  map_free(&m);
  return ok ? 0 : 1;
}

/* .MP round trip through map_save_mp (FUN_19f9_02b0 layout). */
static int case_mp_round_trip(void) {
  ColonizeWorldMap m;
  memset(&m, 0, sizeof m);
  if (!map_alloc(&m, 10, 9, NULL, 0)) return 1;
  for (size_t i = 0; i < m.tile_count; ++i) {
    m.terrain[i] = (uint8_t)i;
    m.layer2[i] = (uint8_t)(i * 3);
    m.layer3[i] = (uint8_t)(i * 7);
  }
  const char* path = "mapedit_round_trip.mp";
  char err[128];
  int rc = 1;
  if (map_save_mp(path, &m, err, sizeof err)) {
    FILE* f = fopen(path, "rb");
    unsigned char hdr[6] = {0};
    if (f) {
      (void)fread(hdr, 1, 6, f);
      fclose(f);
    }
    ColonizeWorldMap r;
    memset(&r, 0, sizeof r);
    if (hdr[0] == 10 && hdr[1] == 0 && hdr[2] == 9 && hdr[3] == 0 && hdr[4] == 4 && hdr[5] == 0 &&
        map_load_mp(path, &r, err, sizeof err) && r.tile_count == m.tile_count &&
        memcmp(r.terrain, m.terrain, m.tile_count) == 0 && memcmp(r.layer2, m.layer2, m.tile_count) == 0 &&
        memcmp(r.layer3, m.layer3, m.tile_count) == 0) {
      rc = 0;
    }
    map_free(&r);
  }
  remove(path);
  map_free(&m);
  return rc;
}

/* The real AMER2.MP with the shipped art: loads, draws a map, a panel and a menu bar.
 * MAPEDIT_SHOT=path writes the frame as a PPM. */
static int case_amer2_renders(void) {
  struct stat st;
  if (stat("COLONIZE/AMER2.MP", &st) != 0 || stat("COLONIZE/PHYS0.SS", &st) != 0) {
    printf("SKIP amer2_renders\n");
    return TR_SKIP;
  }
  MapEditOptions opt = {.data_dir = "COLONIZE", .force_new = false, .map_file = "AMER2.MP"};
  if (!mapedit_init(&g_ed, &opt) || !g_ed.map_ok || g_ed.map.width != 0x3a || g_ed.map.height != 0x48) {
    mapedit_free(&g_ed);
    return 1;
  }
  static uint8_t px[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = px};
  mapedit_render(&g_ed, &fb);
  int view_ink = 0;
  for (int i = 8 * 320; i < 200 * 320; ++i) {
    if ((i % 320) < 240 && px[i] != 0) {
      ++view_ink;
    }
  }
  const char* shot = getenv("MAPEDIT_SHOT");
  for (int pass = 0; shot && pass < 2; ++pass) {
    char path[512];
    snprintf(path, sizeof path, "%s%s", shot, pass ? ".palette.ppm" : "");
    if (pass) {
      g_ed.palette_open = true;
      mapedit_palette_pick(&g_ed, 12);
      mapedit_render(&g_ed, &fb);
    }
    FILE* f = fopen(path, "wb");
    if (f) {
      fprintf(f, "P6\n320 200\n255\n");
      for (int i = 0; i < 320 * 200; ++i) {
        fputc(g_ed.palette.rgb[px[i]][0], f);
        fputc(g_ed.palette.rgb[px[i]][1], f);
        fputc(g_ed.palette.rgb[px[i]][2], f);
      }
      fclose(f);
    }
  }
  const bool specials_off = g_ed.map.specials_off && map_resource_type_at(&g_ed.map, 30, 30) < 0;
  mapedit_free(&g_ed);
  return (view_ink > 240 * 150 && specials_off) ? 0 : 1;
}

static bool step(ColonizeKey key, char ch) {
  ColonizeInputState in;
  memset(&in, 0, sizeof in);
  in.last_key = key;
  if (ch) {
    in.text_input[0] = ch;
    in.text_input_len = 1;
  }
  in.mouse_x = 300;
  in.mouse_y = 5;
  return mapedit_update(&g_ed, NULL, &in, 16);
}

/* Startup picker -> AMER2, paint, hotkeys, and the dirty exit dialog (@EXIT default 3). */
static int case_flow(void) {
  struct stat st;
  if (stat("COLONIZE/AMER2.MP", &st) != 0 || stat("COLONIZE/MAPEDIT.TXT", &st) != 0) {
    printf("SKIP flow\n");
    return TR_SKIP;
  }
  MapEditOptions opt = {.data_dir = "COLONIZE", .force_new = false, .map_file = NULL};
  int rc = 1;
  if (!mapedit_init(&g_ed, &opt)) {
    goto out;
  }
  for (int i = 0; i < 3 && !g_ed.map_ok; ++i) {
    (void)step(COLONIZE_KEY_NONE, 0); /* present the picker */
    (void)step(COLONIZE_KEY_ENTER, 0);
    (void)step(COLONIZE_KEY_NONE, 0);
  }
  if (!g_ed.map_ok || g_ed.dirty) {
    fprintf(stderr, "picker load: ok %d dirty %d\n", g_ed.map_ok, g_ed.dirty);
    goto out;
  }
  (void)step(COLONIZE_KEY_P, 'p'); /* Toggle Coastline ~Protect */
  (void)step(COLONIZE_KEY_F, 'f'); /* ~Fill Mode Change */
  if (!g_ed.coast_protect || g_ed.fill_radius != 1) {
    fprintf(stderr, "hotkeys: coast %d fill %d\n", g_ed.coast_protect, g_ed.fill_radius);
    goto out;
  }
  const int x0 = g_ed.cur_x;
  (void)step(COLONIZE_KEY_RIGHT, 0);
  if (g_ed.cur_x != x0 + 1) goto out;
  mapedit_palette_pick(&g_ed, 3);
  (void)step(COLONIZE_KEY_SPACE, ' ');
  if (!g_ed.dirty) {
    fprintf(stderr, "paint did not dirty\n");
    goto out;
  }
  (void)step(COLONIZE_KEY_ESCAPE, 0); /* @EXIT, default row 3 = Cancel */
  for (int i = 0; i < 3; ++i) {
    (void)step(COLONIZE_KEY_NONE, 0);
  }
  (void)step(COLONIZE_KEY_ENTER, 0);
  (void)step(COLONIZE_KEY_NONE, 0);
  if (g_ed.quit) {
    fprintf(stderr, "Cancel quit\n");
    goto out;
  }
  (void)step(COLONIZE_KEY_ESCAPE, 0);
  (void)step(COLONIZE_KEY_NONE, 0);
  (void)step(COLONIZE_KEY_UP, 0);
  (void)step(COLONIZE_KEY_UP, 0);
  (void)step(COLONIZE_KEY_ENTER, 0); /* "Exit without saving." */
  (void)step(COLONIZE_KEY_NONE, 0);
  rc = g_ed.quit ? 0 : 1;
  if (rc) fprintf(stderr, "exit without saving did not quit\n");
out:
  mapedit_free(&g_ed);
  return rc;
}

/* port_bugs.md #1303 / #1304 / #1307 / #1309: a window close waits for an open dialog, ESC on
 * a pull-down only closes it, the name box keeps its "Name:" label, an unmapped key closes the
 * palette. Nothing is saved (every dialog is cancelled). */
static int case_dialog_guards(void) {
  struct stat st;
  if (stat("COLONIZE/AMER2.MP", &st) != 0 || stat("COLONIZE/MAPEDIT.TXT", &st) != 0) {
    printf("SKIP dialog_guards\n");
    return TR_SKIP;
  }
  MapEditOptions opt = {.data_dir = "COLONIZE", .force_new = false, .map_file = "AMER2.MP"};
  int rc = 1;
  if (!mapedit_init(&g_ed, &opt)) {
    goto out;
  }
  /* #1304: clean map, menu open, ESC: the menu closes, the editor stays. */
  if (!map_menu_open_alt_hotkey(&g_ed.menu, 'E')) {
    fprintf(stderr, "Alt-E did not open the Editor menu\n");
    goto out;
  }
  (void)step(COLONIZE_KEY_ESCAPE, 0);
  if (g_ed.quit || g_ed.menu.open_index >= 0 || ai_popup_busy(&g_ed.popups)) {
    fprintf(stderr, "menu esc: quit %d menu %d\n", g_ed.quit, g_ed.menu.open_index);
    goto out;
  }
  /* #1303: dirty, @SAVE open, window close: @SAVE keeps its answer slot. */
  g_ed.dirty = true;
  (void)step(COLONIZE_KEY_S, 's');
  ColonizeInputState in;
  memset(&in, 0, sizeof in);
  in.quit_requested = true;
  (void)mapedit_update(&g_ed, NULL, &in, 16);
  if (g_ed.pending != MAPEDIT_PENDING_SAVE || g_ed.quit) {
    fprintf(stderr, "close over @SAVE: pending %d\n", g_ed.pending);
    goto out;
  }
  (void)step(COLONIZE_KEY_ESCAPE, 0);
  (void)step(COLONIZE_KEY_NONE, 0);
  /* #1307. */
  (void)step(COLONIZE_KEY_A, 'a');
  if (!g_ed.name_entry.open || strcmp(g_ed.name_entry.label, "Name:") != 0) {
    fprintf(stderr, "saveas label [%s]\n", g_ed.name_entry.label);
    goto out;
  }
  (void)step(COLONIZE_KEY_ESCAPE, 0);
  /* #1309: '7' has no ColonizeKey, only text. */
  g_ed.palette_open = true;
  (void)step(COLONIZE_KEY_NONE, '7');
  rc = g_ed.palette_open || g_ed.quit ? 1 : 0;
  if (rc) fprintf(stderr, "palette stayed open\n");
out:
  mapedit_free(&g_ed);
  return rc;
}

static const TestCase k_cases[] = {
    {"flow", case_flow},
    {"dialog_guards", case_dialog_guards},
    {"paint_rules", case_paint_rules},
    {"palette_walk", case_palette_walk},
    {"find_continents", case_find_continents},
    {"mp_round_trip", case_mp_round_trip},
    {"amer2_renders", case_amer2_renders},
};

TEST_MAIN(k_cases)
