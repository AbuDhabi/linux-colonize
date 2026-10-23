#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/fb.h"
#include "core/font.h"
#include "core/game_loop.h"
#include "core/new_game.h"
#include "core/pik.h"
#include "core/reports.h"
#include "core/ss.h"
#include "core/ui_colors.h"

#include "tests/common/test_assets.h"
#include "../common/test_runner.h"

static const char* k_data_dir = "./test-assets-newgame";
static const char* k_save_dir = "./test-saves-newgame";

static bool step_click(ColonizeGameState* game) {
  ColonizeInputState input = {0};
  input.mouse_left_clicked = true;
  input.mouse_x = 160;
  input.mouse_y = 100;
  return game_update(game, &input, 16);
}

static int setup(void) {
  if (!test_assets_create(k_data_dir, TEST_ASSETS_FULL_MENU | TEST_ASSETS_WIZARD_PAGES | TEST_ASSETS_NAMES_FULL)) {
    fprintf(stderr, "failed to create test assets\n");
    return 1;
  }
  return 0;
}

/* Unit: scenario start parsing. */
static int case_scenario_start(void) {
  if (setup() != 0) return 1;
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char path[512];
  snprintf(path, sizeof(path), "%s/NAMES.TXT", k_data_dir);
  if (!assets_msg_load_file(&names, path)) {
    fprintf(stderr, "NAMES load failed\n");
    return 1;
  }
  int x = 0, y = 0;
  /* @SCENARIO is four pairs, one per nation, in nation order. */
  new_game_scenario_start(&names, "AMER2", 0, &x, &y);
  if (x != 34 || y != 20) {
    fprintf(stderr, "scenario England start expected 34,20 got %d,%d\n", x, y);
    return 1;
  }
  new_game_scenario_start(&names, "AMER2", 1, &x, &y);
  if (x != 39 || y != 10) {
    fprintf(stderr, "scenario France start expected 39,10 got %d,%d\n", x, y);
    return 1;
  }
  new_game_scenario_start(&names, "AMER2", 2, &x, &y);
  if (x != 47 || y != 61) {
    fprintf(stderr, "scenario Spain start expected 47,61 got %d,%d\n", x, y);
    return 1;
  }
  new_game_scenario_start(&names, "AMER2", 3, &x, &y);
  if (x != 50 || y != 33) {
    fprintf(stderr, "scenario Netherlands start expected 50,33 got %d,%d\n", x, y);
    return 1;
  }
  assets_msg_free(&names);
  return 0;
}

/* Unit: CUSTOMIZE path defaults + confirm preserves axes. */
static int case_customize_defaults(void) {
  if (setup() != 0) return 1;
  NewGameWizard ng;
  new_game_init(&ng);
  if (!new_game_begin(&ng, NEW_GAME_PATH_CUSTOMIZE, k_data_dir, NULL, NULL)) {
    fprintf(stderr, "customize begin failed\n");
    return 1;
  }
  if (ng.phase != NEW_GAME_PHASE_CUSTOMIZE) {
    fprintf(stderr, "expected CUSTOMIZE phase got %d\n", (int)ng.phase);
    return 1;
  }
  if (!ng.generate_map || ng.gen_params.land_mass != 1 || ng.gen_params.land_form != 1 ||
      ng.gen_params.temperature != 1 || ng.gen_params.climate != 1 ||
      ng.gen_params.forest_extra != 1) {
    fprintf(stderr, "customize defaults expected all 1\n");
    return 1;
  }
  ColonizeInputState input = {0};
  input.last_key = COLONIZE_KEY_DOWN;
  if (!new_game_handle_input(&ng, &input)) {
    fprintf(stderr, "customize down failed\n");
    return 1;
  }
  if (ng.gen_params.land_mass != 2) {
    fprintf(stderr, "expected land_mass 2 after DOWN got %d\n", ng.gen_params.land_mass);
    return 1;
  }
  /* Click Temperature / Cool cell (col 2, row 0). */
  memset(&input, 0, sizeof(input));
  input.mouse_left_clicked = true;
  input.mouse_x = 162 + 36;
  input.mouse_y = 16 + 24;
  if (!new_game_handle_input(&ng, &input)) {
    fprintf(stderr, "customize cell click failed\n");
    return 1;
  }
  if (ng.customize_focus != 2 || ng.gen_params.temperature != 0) {
    fprintf(
      stderr,
      "expected focus 2 temp 0 got focus=%d temp=%d\n",
      ng.customize_focus,
      ng.gen_params.temperature
    );
    return 1;
  }
  memset(&input, 0, sizeof(input));
  input.last_key = COLONIZE_KEY_ENTER;
  if (!new_game_handle_input(&ng, &input)) {
    fprintf(stderr, "customize enter failed\n");
    return 1;
  }
  if (ng.phase != NEW_GAME_PHASE_DIFFICULTY) {
    fprintf(stderr, "expected DIFFICULTY after customize confirm got %d\n", (int)ng.phase);
    return 1;
  }
  if (ng.gen_params.land_mass != 2 || ng.gen_params.temperature != 0) {
    fprintf(stderr, "customize params wiped after confirm\n");
    return 1;
  }
  new_game_free(&ng);
  return 0;
}

/* Full wizard flow via the title menu: NEW WORLD -> difficulty -> nation ->
 * leader name -> lore -> sail skip, checking the resulting campaign state. */
static int case_wizard_new_world_flow(void) {
  if (setup() != 0) return 1;
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    fprintf(stderr, "game_create failed\n");
    return 1;
  }

  if (!game_in_menu(game)) {
    fprintf(stderr, "expected title menu\n");
    game_destroy(game);
    return 1;
  }

  /* Select NEW WORLD (index 0) and activate. */
  if (!test_assets_step(game, COLONIZE_KEY_ENTER)) {
    fprintf(stderr, "enter NEW WORLD failed\n");
    game_destroy(game);
    return 1;
  }
  if (!game_in_new_game(game) || game_in_menu(game)) {
    fprintf(stderr, "expected new-game wizard after NEW WORLD\n");
    game_destroy(game);
    return 1;
  }

  /* Difficulty: Discoverer(0) is default; RIGHT → Explorer(1), then confirm. */
  if (!test_assets_step(game, COLONIZE_KEY_RIGHT)) { game_destroy(game); return 1; }
  if (!test_assets_step(game, COLONIZE_KEY_ENTER)) { game_destroy(game); return 1; }

  /* Nation: England(0) default; RIGHT → France(1). */
  if (!test_assets_step(game, COLONIZE_KEY_RIGHT)) { game_destroy(game); return 1; }
  if (!test_assets_step(game, COLONIZE_KEY_ENTER)) { game_destroy(game); return 1; }

  /* Leader name: confirm default. */
  if (!test_assets_step(game, COLONIZE_KEY_ENTER)) { game_destroy(game); return 1; }

  /* Lore A → B → King → Sail */
  if (!test_assets_step(game, COLONIZE_KEY_ENTER)) { game_destroy(game); return 1; }
  if (!test_assets_step(game, COLONIZE_KEY_ENTER)) { game_destroy(game); return 1; }
  if (!test_assets_step(game, COLONIZE_KEY_ENTER)) { game_destroy(game); return 1; }

  /* Skip sail with LMB. */
  if (!step_click(game)) {
    fprintf(stderr, "sail skip failed\n");
    game_destroy(game);
    return 1;
  }

  if (game_in_new_game(game) || game_in_menu(game)) {
    fprintf(stderr, "expected map after sail skip (menu=%d new=%d)\n",
            game_in_menu(game), game_in_new_game(game));
    game_destroy(game);
    return 1;
  }
  if (game_difficulty(game) != 1) {
    fprintf(stderr, "expected difficulty 1 got %d\n", game_difficulty(game));
    game_destroy(game);
    return 1;
  }
  if (game_human_nation(game) != 1) {
    fprintf(stderr, "expected nation France(1) got %d\n", game_human_nation(game));
    game_destroy(game);
    return 1;
  }
  if (strcmp(game_leader_name(game), "Leader Two") != 0) {
    fprintf(stderr, "expected fixture @LEADERNAME row 1 got '%s'\n", game_leader_name(game));
    game_destroy(game);
    return 1;
  }

  game_destroy(game);
  return 0;
}

/* Hall of Fame must not start a game. */
static int case_hall_of_fame_from_menu(void) {
  if (setup() != 0) return 1;
  ColonizeGameConfig cfg = {.data_dir = k_data_dir, .save_dir = k_save_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game) {
    return 1;
  }
  /* Move to Hall of Fame (@BEGINMENU row 4) */
  for (int i = 0; i < 4; ++i) {
    if (!test_assets_step(game, COLONIZE_KEY_DOWN)) { game_destroy(game); return 1; }
  }
  if (!test_assets_step(game, COLONIZE_KEY_ENTER)) { game_destroy(game); return 1; }
  /* Opens the Hall of Fame screen (see unit_hall_of_fame), not a new game;
   * Esc returns to the title menu. */
  if (!game_in_hall_of_fame(game) || game_in_menu(game) || game_in_new_game(game)) {
    fprintf(stderr, "Hall of Fame should open its own screen, not start a game\n");
    game_destroy(game);
    return 1;
  }
  if (!test_assets_step(game, COLONIZE_KEY_ESCAPE)) { game_destroy(game); return 1; }
  if (game_in_hall_of_fame(game) || !game_in_menu(game)) {
    fprintf(stderr, "Esc from Hall of Fame should return to title menu\n");
    game_destroy(game);
    return 1;
  }

  game_destroy(game);
  return 0;
}

/* ===== port_bugs.md #1198: wizard text is the DOS writers, one pass, no drop shadow ===== */

/* Real-asset wizard fixture (COLONIZE/): the catalogs, fonts and frame art the game
 * hands the wizard. */
typedef struct NgFix {
  ColonizeMsgCatalog game_txt;
  ColonizeMsgCatalog names_txt;
  ColonizeMsgCatalog labels_txt;
  ColonizeFont intr;
  ColonizeFont tiny;
  ColonizePikImage woodpanl;
  ColonizeSpriteSheet woodtile;
  ColonizeReportsView view;
  NewGameWizard ng;
  ColonizePopupColors cols;
  uint8_t px[320 * 200];
  uint8_t ref[320 * 200];
  ColonizeFramebuffer8 fb;
  ColonizePalette pal;
} NgFix;

static NgFix g_fix;

static void ng_fix_free(NgFix* f) {
  new_game_free(&f->ng);
  reports_free(&f->view);
  assets_msg_free(&f->game_txt);
  assets_msg_free(&f->names_txt);
  assets_msg_free(&f->labels_txt);
  font_file_free(&f->intr);
  font_file_free(&f->tiny);
  pik_free(&f->woodpanl);
  ss_free(&f->woodtile);
}

static int ng_fix_begin(NgFix* f, NewGamePath path) {
  memset(f, 0, sizeof(*f));
  char err[256];
  assets_msg_init(&f->game_txt);
  assets_msg_init(&f->names_txt);
  assets_msg_init(&f->labels_txt);
  if (!assets_msg_load_file(&f->game_txt, "COLONIZE/GAME.TXT") ||
      !assets_msg_load_file(&f->names_txt, "COLONIZE/NAMES.TXT") ||
      !assets_msg_load_file(&f->labels_txt, "COLONIZE/LABELS.TXT") ||
      !font_file_load("COLONIZE/FONTINTR.FF", &f->intr, err, sizeof(err)) ||
      !font_file_load("COLONIZE/FONTTINY.FF", &f->tiny, err, sizeof(err)) ||
      !pik_load("COLONIZE/WOODPANL.PIK", &f->woodpanl, err, sizeof(err)) ||
      !ss_load("COLONIZE/WOODTILE.SS", &f->woodtile, err, sizeof(err)) ||
      !reports_load(&f->view, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "wizard fixture: COLONIZE assets missing (%s)\n", err);
    return 1;
  }
  new_game_init(&f->ng);
  if (!new_game_begin(&f->ng, path, "COLONIZE", &f->game_txt, &f->names_txt)) {
    return 1;
  }
  f->ng.ui_font = &f->intr;
  f->ng.tiny_font = &f->tiny;
  f->ng.lore_font = &f->intr;
  f->ng.labels_txt = &f->labels_txt;
  f->ng.woodpanl = &f->woodpanl;
  f->ng.wood_tile = &f->woodtile;
  popup_colors_from_ui(&f->cols);
  f->fb = (ColonizeFramebuffer8){.width = 320, .height = 200, .pixels = f->px};
  return 0;
}

static void ng_fix_render(NgFix* f) {
  new_game_render(
    &f->ng, &f->fb, &f->pal, &f->cols, COLONIZE_COL_BASIC, COLONIZE_COL_HILITE, COLONIZE_COL_SELECT
  );
}

static bool ng_fix_enter(NgFix* f) {
  ColonizeInputState input = {0};
  input.last_key = COLONIZE_KEY_ENTER;
  return new_game_handle_input(&f->ng, &input);
}

/* LABELS.TXT @MISC row (the wizard block starts at row 144, "Land Mass"). */
static const char* ng_misc(const NgFix* f, int offset) {
  const ColonizeMsgSection* misc = assets_msg_find(&f->labels_txt, "MISC");
  return (misc && 144 + offset < misc->line_count) ? misc->lines[144 + offset] : "";
}

/* Oracle for one FUN_104b_0318 / 0430 call (raw 1007 / 1063): centred
 * x + (w >> 1) - ((width - 1) >> 1), one blit through {skip, c1, c2, c3}. */
static void ng_ref_put(
  NgFix* f, const ColonizeFont* font, const char* s, int x, int w, int y, uint8_t c1, uint8_t c2,
  uint8_t c3
) {
  ColonizeFramebuffer8 ref = {.width = 320, .height = 200, .pixels = f->ref};
  const uint8_t map[4] = {0, c1, c2, c3};
  int cx = x + (w >> 1) - ((font_text_width_skip(font, s, FONT_SKIP_NONE) - 1) >> 1);
  font_draw_text_shaded(font, &ref, cx < 0 ? 0 : cx, y, s, map);
}

/* Selected-cell line: ink 0 at x + 1, then the ink at x (733a:061b/0638 etc.). */
static void ng_ref_cell_line(NgFix* f, const char* s, int x, int w, int y, uint8_t ink) {
  ng_ref_put(f, &f->tiny, s, x + 1, w, y, 0, 0, 0);
  ng_ref_put(f, &f->tiny, s, x, w, y, ink, ink, ink);
}

static void ng_ref_outline(NgFix* f, int x, int y, int w, int h, uint8_t c) {
  ColonizeFramebuffer8 ref = {.width = 320, .height = 200, .pixels = f->ref};
  fb_rect_outline(&ref, x, y, w, h, c, c);
}

static void ng_ref_bg(NgFix* f, const ColonizePikImage* pik, uint8_t fill) {
  ColonizeFramebuffer8 ref = {.width = 320, .height = 200, .pixels = f->ref};
  memset(f->ref, fill, sizeof(f->ref));
  if (pik) {
    pik_blit(pik, &ref, 0, 0);
  }
}

static int ng_ref_compare(const NgFix* f, const char* what) {
  int bad = 0;
  for (int i = 0; i < 320 * 200; ++i) {
    if (f->px[i] != f->ref[i] && bad++ < 4) {
      fprintf(stderr, "%s (%d,%d): got %d want %d\n", what, i % 320, i / 320, f->px[i], f->ref[i]);
    }
  }
  if (bad) {
    fprintf(stderr, "%s: %d pixels differ from the DOS writer oracle\n", what, bad);
  }
  return bad ? 1 : 0;
}

static void ng_upper_colon(char* out, size_t n, const char* s, bool upper) {
  snprintf(out, n, "%s:", s);
  for (char* p = out; upper && *p; ++p) {
    if (*p >= 'a' && *p <= 'z') {
      *p = (char)(*p - 'a' + 'A');
    }
  }
}

/* Difficulty (FUN_733a_06a4 / 0512), nation (FUN_733a_0b3e / 09c6) and customize
 * (FUN_733a_01a4 / 002c) screens equal, pixel for pixel, the DOS writer calls in DOS
 * order over the screen's own PIK: titles FONTINTR {0xfe, 0xfd, 0}, the finished line
 * FONTTINY 0xfe, the selected cell's outline and two lines each with a black (x + 1)
 * pass. A DOSBox-X capture of all three screens matched the port exactly (2026-10-23). */
static int case_wizard_picture_text_is_dos(void) {
  NgFix* f = &g_fix;
  int rc = 1;
  char fin[80];
  char top[48];
  if (ng_fix_begin(f, NEW_GAME_PATH_NEW_WORLD) != 0) {
    goto out;
  }
  snprintf(fin, sizeof(fin), "(%s)", ng_misc(f, 17));
  const int ih = f->intr.max_height;
  const int th = f->tiny.max_height;

  /* Difficulty, Discoverer selected: cell (128, 7) 0x44 x 0x5a, ink 10. */
  ng_fix_render(f);
  ng_ref_bg(f, &f->ng.difficul_pik, 0);
  ng_ref_put(f, &f->intr, ng_misc(f, 18), 0x17, 0x44, 0x14 - (ih >> 1), 0xfe, 0xfd, 0);
  ng_ref_put(f, &f->intr, ng_misc(f, 19), 0x17, 0x44, 0x14 - (ih >> 1) + ih + 4, 0xfe, 0xfd, 0);
  ng_ref_put(f, &f->tiny, fin, 0x17, 0x44, 0x51, 0xfe, 0xfe, 0xfe);
  ng_ref_outline(f, 128, 7, 0x44, 0x5a, 10);
  ng_upper_colon(top, sizeof(top), reports_difficulty_title(0), true);
  ng_ref_cell_line(f, top, 128, 0x44, 7 + 0x2c - th, 10);
  ng_ref_cell_line(f, ng_misc(f, 21), 128, 0x44, 7 + 0x2e, 10);
  if (ng_ref_compare(f, "difficulty")) {
    goto out;
  }

  /* Nation, England selected: cell (112, 13) 0x58 x 0x52, @COUNTRY ink 12. */
  if (!ng_fix_enter(f) || f->ng.phase != NEW_GAME_PHASE_NATION) {
    fprintf(stderr, "expected the nation screen\n");
    goto out;
  }
  ng_fix_render(f);
  ng_ref_bg(f, &f->ng.nations_pik, 0);
  ng_ref_put(f, &f->intr, ng_misc(f, 26), 0, 0x70, 0x28 - (ih >> 1), 0xfe, 0xfd, 0);
  ng_ref_put(f, &f->intr, ng_misc(f, 27), 0, 0x70, 0x28 - (ih >> 1) + ih + 4, 0xfe, 0xfd, 0);
  ng_ref_put(f, &f->tiny, fin, 0, 0x70, 0xb6, 0xfe, 0xfe, 0xfe);
  ng_ref_outline(f, 112, 13, 0x58, 0x52, 12);
  ng_upper_colon(top, sizeof(top), new_game_nation_name(0), true);
  ng_ref_cell_line(f, top, 112, 0x58, 13 + 2, 12);
  ng_ref_cell_line(f, ng_misc(f, 29), 112, 0x58, 13 + 0x50 - th, 12);
  if (ng_ref_compare(f, "nation")) {
    goto out;
  }
  ng_fix_free(f);

  /* Customize, every axis 1, focus column 0: cells (c * 0x4c + 10, 76) 0x48 x 0x30,
   * ink 0x0e for the focus column and 0x0a otherwise. */
  if (ng_fix_begin(f, NEW_GAME_PATH_CUSTOMIZE) != 0) {
    goto out;
  }
  ng_fix_render(f);
  ng_ref_bg(f, &f->ng.customiz_pik, 0);
  ng_ref_put(f, &f->intr, ng_misc(f, 16), 0, 0x140, 4, 0xfe, 0xfd, 0);
  ng_ref_put(f, &f->tiny, fin, 0, 0x140, 0xbe, 0xfe, 0xfe, 0xfe);
  for (int c = 0; c < 4; ++c) {
    const int x = c * 0x4c + 10;
    const uint8_t ink = c == 0 ? 0x0e : 0x0a;
    ng_ref_outline(f, x, 76, 0x48, 0x30, ink);
    ng_upper_colon(top, sizeof(top), ng_misc(f, c), false);
    ng_ref_cell_line(f, top, x, 0x48, 76 + 0x17 - th, ink);
    ng_ref_cell_line(f, ng_misc(f, 4 + c * 3 + 1), x, 0x48, 76 + 0x19, ink);
  }
  rc = ng_ref_compare(f, "customize");
out:
  ng_fix_free(f);
  return rc;
}

/* Colours the screen added over its background, as a 256-entry count. */
static void ng_added(const NgFix* f, int x0, int y0, int x1, int y1, int out[256]) {
  memset(out, 0, 256 * sizeof(int));
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      if (f->px[y * 320 + x] != f->ref[y * 320 + x]) {
        out[f->px[y * 320 + x]]++;
      }
    }
  }
}

/* Every added pixel is one of `allowed` and `must` shows up: one FUN_6f74_0538 pass
 * through the screen's style map. The old drop shadow added ink 0 and the old unbold
 * pass dropped FONTINTR's shades 2 / 3. */
static int ng_check_added(
  const NgFix* f, const char* what, const int added[256], const uint8_t* allowed, int n_allowed,
  uint8_t must
) {
  int total = 0;
  for (int c = 0; c < 256; ++c) {
    if (!added[c]) {
      continue;
    }
    total += added[c];
    bool ok = false;
    for (int k = 0; k < n_allowed; ++k) {
      ok = ok || allowed[k] == c;
    }
    if (!ok) {
      fprintf(stderr, "%s: %d pixels in colour %d, outside the DOS map\n", what, added[c], c);
      return 1;
    }
  }
  if (total == 0 || added[must] == 0) {
    fprintf(stderr, "%s: no text in colour %d (%d added pixels)\n", what, must, total);
    return 1;
  }
  (void)f;
  return 0;
}

/* @AMERICA, @LEADERNAME, @NATION0A, the king's audience and @BUILD1 draw one markup
 * pass through their DOS style: @COLORS {68 (149 in {}), 128, 47}; the king
 * FUN_75c2_20e2 override {0xf2, 0x2f, 0}; the sail FUN_1262_0152 override
 * {0x0e, 0x36, 47}. */
static int case_wizard_prose_text_is_one_pass(void) {
  NgFix* f = &g_fix;
  int rc = 1;
  int added[256];
  if (ng_fix_begin(f, NEW_GAME_PATH_AMERICA) != 0) {
    goto out;
  }

  /* @AMERICA list dialog: inside the wood (WOODTILE 128..138, bar 138) the text adds
   * 68 / 128 / 47 and never ink 0. */
  ng_fix_render(f);
  {
    int n0 = 0, n47 = 0;
    for (int y = f->ng.dialog_y + 4; y < f->ng.dialog_y + f->ng.dialog_h - 4; ++y) {
      for (int x = f->ng.dialog_x + 4; x < f->ng.dialog_x + f->ng.dialog_w - 4; ++x) {
        n0 += f->px[y * 320 + x] == 0;
        n47 += f->px[y * 320 + x] == COLONIZE_COL_SHADOW;
      }
    }
    if (n0 != 0 || n47 == 0) {
      fprintf(stderr, "@AMERICA text: %d ink-0 pixels, %d shade-3 (47) pixels\n", n0, n47);
      goto out;
    }
  }
  static const uint8_t k_colors[] = {
    COLONIZE_COL_BASIC, COLONIZE_COL_HILITE, COLONIZE_COL_ENHANCE, COLONIZE_COL_SHADOW,
    COLONIZE_COL_SELECT
  };
  /* Original Americas -> difficulty -> nation -> leader name. */
  if (!ng_fix_enter(f) || !ng_fix_enter(f) || !ng_fix_enter(f) ||
      f->ng.phase != NEW_GAME_PHASE_LEADER_NAME) {
    fprintf(stderr, "expected the leader-name prompt\n");
    goto out;
  }
  ng_fix_render(f);
  ng_ref_bg(f, &f->woodpanl, 4);
  ng_added(f, 0, 0, 320, 200, added);
  if (ng_check_added(f, "@LEADERNAME", added, k_colors, 5, COLONIZE_COL_ENHANCE)) {
    goto out;
  }
  if (!ng_fix_enter(f) || f->ng.phase != NEW_GAME_PHASE_NATION_LORE_A) {
    fprintf(stderr, "expected @NATION0A\n");
    goto out;
  }
  ng_fix_render(f);
  ng_added(f, 0, 0, 320, 200, added);
  if (ng_check_added(f, "@NATION0A", added, k_colors, 4, COLONIZE_COL_SHADOW) ||
      added[COLONIZE_COL_HILITE] == 0) {
    fprintf(stderr, "@NATION0A: {} text should be hilite 149 (%d px)\n", added[COLONIZE_COL_HILITE]);
    goto out;
  }

  /* King: art only (no @VICEROY lines) is the background. */
  if (!ng_fix_enter(f) || !ng_fix_enter(f) || f->ng.phase != NEW_GAME_PHASE_KING) {
    fprintf(stderr, "expected the king's audience\n");
    goto out;
  }
  {
    const int lines = f->ng.prompt_line_count;
    f->ng.prompt_line_count = 0;
    ng_fix_render(f);
    memcpy(f->ref, f->px, sizeof(f->ref));
    f->ng.prompt_line_count = lines;
  }
  ng_fix_render(f);
  ng_added(f, 0, 0, 320, 200, added);
  static const uint8_t k_king[] = {0xf2, COLONIZE_COL_HILITE, 0x2f, 0};
  if (ng_check_added(f, "king", added, k_king, 4, 0xf2)) {
    goto out;
  }

  /* Sail, @BUILD1 over LEVN0001. */
  if (!ng_fix_enter(f) || f->ng.phase != NEW_GAME_PHASE_SAIL) {
    fprintf(stderr, "expected the sail sequence\n");
    goto out;
  }
  f->ng.game_txt = NULL;
  ng_fix_render(f);
  memcpy(f->ref, f->px, sizeof(f->ref));
  f->ng.game_txt = &f->game_txt;
  ng_fix_render(f);
  ng_added(f, 0, 0, 320, 200, added);
  static const uint8_t k_sail[] = {0x0e, COLONIZE_COL_HILITE, 0x36, COLONIZE_COL_SHADOW};
  rc = ng_check_added(f, "@BUILD1", added, k_sail, 4, 0x36);
out:
  ng_fix_free(f);
  return rc;
}

/* Leftmost / rightmost x in rows y0..y1 whose pixel is ink a or b (-1 = none). */
static int ng_ink_x(const NgFix* f, int y0, int y1, uint8_t a, uint8_t b, bool rightmost) {
  int best = -1;
  for (int y = y0; y <= y1; ++y) {
    for (int x = 0; x < 320; ++x) {
      const uint8_t p = f->px[y * 320 + x];
      if ((p == a || p == b) && (best < 0 || (rightmost ? x > best : x < best))) {
        best = x;
      }
    }
  }
  return best;
}

static int ng_expect(const char* what, int got, int want) {
  if (got != want) {
    fprintf(stderr, "%s: got %d, DOS %d\n", what, got, want);
    return 1;
  }
  return 0;
}

/* port_bugs.md #1210 / #1212: the 6f74 wizard dialogs against the DOSBox-X captures of
 * 2026-10-23 (FUN_6f74_14c6 layout; every number below was read off a capture, which the
 * port now matches pixel for pixel outside the mouse pointer). */
static int case_wizard_dialog_layout_is_dos(void) {
  NgFix* f = &g_fix;
  int rc = 1;
  if (ng_fix_begin(f, NEW_GAME_PATH_AMERICA) != 0) {
    goto out;
  }
  const uint8_t ink = COLONIZE_COL_BASIC;
  const uint8_t hil = COLONIZE_COL_HILITE;
  /* @AMERICA: @width 160, three prose lines, two rows: frame 77,67 166 x 66, row bar
   * 81..238 from y 106, rows 12 apart. */
  ng_fix_render(f);
  if (ng_expect("@AMERICA x", f->ng.dialog_x, 77) || ng_expect("@AMERICA y", f->ng.dialog_y, 67) ||
      ng_expect("@AMERICA w", f->ng.dialog_w, 166) || ng_expect("@AMERICA h", f->ng.dialog_h, 66) ||
      ng_expect("@AMERICA list_y0", f->ng.list_y0, 106) ||
      ng_expect("@AMERICA row pitch", f->ng.line_h, 12) ||
      ng_expect("@AMERICA bar", f->px[106 * 320 + 81], COLONIZE_COL_SELECT) ||
      ng_expect("@AMERICA bar right", f->px[116 * 320 + 238], COLONIZE_COL_SELECT) ||
      ng_expect("@AMERICA prose x", ng_ink_x(f, 73, 81, ink, ink, false), 82) ||
      ng_expect("@AMERICA row x", ng_ink_x(f, 107, 115, ink, ink, false), 86)) {
    goto out;
  }
  /* FUN_6f74_2580 wraps: Up from the first row lands on the last, Down goes back. */
  {
    ColonizeInputState in = {0};
    in.last_key = COLONIZE_KEY_UP;
    new_game_handle_input(&f->ng, &in);
    if (ng_expect("@AMERICA Up from row 0", f->ng.selection, 1)) {
      goto out;
    }
    in.last_key = COLONIZE_KEY_DOWN;
    new_game_handle_input(&f->ng, &in);
    if (ng_expect("@AMERICA Down from the last row", f->ng.selection, 0)) {
      goto out;
    }
    new_game_handle_input(&f->ng, &in);
  }
  /* Map Editor -> @MAPTOLOAD: @width 220, centred "^^" prompt, frame 47,83 226 x 34. */
  if (!ng_fix_enter(f) || f->ng.phase != NEW_GAME_PHASE_MAP_PICK) {
    fprintf(stderr, "expected the map picker\n");
    goto out;
  }
  ng_fix_render(f);
  if (ng_expect("@MAPTOLOAD x", f->ng.dialog_x, 47) ||
      ng_expect("@MAPTOLOAD y", f->ng.dialog_y, 83) ||
      ng_expect("@MAPTOLOAD w", f->ng.dialog_w, 226) ||
      ng_expect("@MAPTOLOAD h", f->ng.dialog_h, 34) ||
      ng_expect("@MAPTOLOAD list_y0", f->ng.list_y0, 102) ||
      ng_expect("@MAPTOLOAD prompt x", ng_ink_x(f, 89, 95, ink, ink, false), 108)) {
    goto out;
  }
  /* -> difficulty -> nation -> @LEADERNAME: frameless, prompt centred at y 88, field box
   * 79..245 x 98..111, text at 82,101. */
  if (!ng_fix_enter(f) || !ng_fix_enter(f) || !ng_fix_enter(f) ||
      f->ng.phase != NEW_GAME_PHASE_LEADER_NAME) {
    fprintf(stderr, "expected the leader-name prompt\n");
    goto out;
  }
  ng_fix_render(f);
  if (ng_expect("@LEADERNAME prompt x", ng_ink_x(f, 88, 95, ink, ink, false), 104) ||
      ng_expect("@LEADERNAME box top-left", f->px[98 * 320 + 79], ink) ||
      ng_expect("@LEADERNAME box bottom-right", f->px[111 * 320 + 245], ink) ||
      ng_expect("@LEADERNAME text x", f->ng.leader_field_x, 82) ||
      ng_expect("@LEADERNAME text y", f->ng.leader_field_y, 101)) {
    goto out;
  }
  /* @NATION0A: frameless at x 10, "ENGLAND" from x 140; the eleventh line ends at x 290
   * (DOS breaks "...had neither" before "the": FUN_6f74_1198 charges the space). */
  if (!ng_fix_enter(f) || f->ng.phase != NEW_GAME_PHASE_NATION_LORE_A) {
    fprintf(stderr, "expected @NATION0A\n");
    goto out;
  }
  ng_fix_render(f);
  if (ng_expect("@NATION0A ENGLAND x", ng_ink_x(f, 20, 26, ink, hil, false), 140) ||
      ng_expect("@NATION0A body x", ng_ink_x(f, 40, 47, ink, hil, false), 10) ||
      ng_expect("@NATION0A line 11 end", ng_ink_x(f, 130, 137, ink, hil, true), 290)) {
    goto out;
  }
  /* King: ENGLND1 / KING1 on their header anchors, (32, 0) and (0, 12). */
  if (!ng_fix_enter(f) || !ng_fix_enter(f) || f->ng.phase != NEW_GAME_PHASE_KING) {
    fprintf(stderr, "expected the king's audience\n");
    goto out;
  }
  {
    const int lines = f->ng.prompt_line_count;
    f->ng.prompt_line_count = 0;
    ng_fix_render(f);
    f->ng.prompt_line_count = lines;
    ColonizeFramebuffer8 ref = {.width = 320, .height = 200, .pixels = f->ref};
    ng_ref_bg(f, &f->ng.kinglss_pik, 0);
    ss_blit_sprite(&f->ng.nation_art, 0, &ref, 32, 0);
    ss_blit_sprite(&f->ng.king1, 0, &ref, 0, 12);
    if (ng_ref_compare(f, "king art")) {
      goto out;
    }
    /* "1492": centred in @x 232 @width 78 as 232 + 39 - (width >> 1); DOS ink from x 261. */
    memcpy(f->ref, f->px, sizeof(f->ref));
    ng_fix_render(f);
    int left = -1;
    for (int y = 37; y <= 44; ++y) {
      for (int x = 232; x < 312; ++x) {
        if (f->px[y * 320 + x] != f->ref[y * 320 + x] && (left < 0 || x < left)) {
          left = x;
        }
      }
    }
    if (ng_expect("king 1492 x", left, 261)) {
      goto out;
    }
  }
  rc = 0;
out:
  ng_fix_free(f);
  return rc;
}

static const TestCase k_cases[] = {
    {"scenario_start", case_scenario_start},
    {"customize_defaults", case_customize_defaults},
    {"wizard_new_world_flow", case_wizard_new_world_flow},
    {"hall_of_fame_from_menu", case_hall_of_fame_from_menu},
    {"wizard_picture_text_is_dos", case_wizard_picture_text_is_dos},
    {"wizard_prose_text_is_one_pass", case_wizard_prose_text_is_one_pass},
    {"wizard_dialog_layout_is_dos", case_wizard_dialog_layout_is_dos},
};

TEST_MAIN(k_cases)
