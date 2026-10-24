/*
 * Headless interactive play driver (tools/README.md "play_driver").
 *
 *   play_driver DIR [--load SAVE] [--seed N]
 *
 * Runs the game with the input-replay platform of tests/regression (no SDL
 * window, no audio) and takes commands on stdin, one per line, so an agent or
 * script can play a whole campaign across turns. DIR holds settings.json
 * (created with debug.trace_autosaves + debug.logs on when missing),
 * saves/ (save dir: COLONY0n, trace_autosave_turn_XXX.sav) and
 * shots/ (PPM screenshots). x/y are 320x200 logical pixels.
 *
 *   key NAME [shift|ctrl|alt]   tap: a-z 0-9 - = [ ] ` Return Escape space Backspace
 *                               Tab Delete Up Down Left Right KP_1..KP_9 KP_Enter
 *                               F1..F10 Home End
 *   type TEXT                   text input, one char per frame (name fields)
 *   click X Y [shift|ctrl|alt]  left click; rclick X Y; dclick X Y
 *   drag X1 Y1 X2 Y2 [shift]    left-button drag
 *   move X Y                    pointer move
 *   wait N                      N idle frames
 *   idle [MAX]                  run until the main map has been idle 20 frames
 *                               (no popup, no turn in progress), at most MAX
 *                               frames (default 20000)
 *   shot NAME                   DIR/shots/NAME.ppm
 *   save NAME                   save into DIR/saves/NAME
 *   status | units | colonies   print state
 *   map [R]                     ASCII map of radius R (default 8) around the cursor
 *   sync TAG                    print "@@ TAG" (end-of-batch marker)
 *   quit
 *
 * Every popup / ticker text the game presents is echoed as "LOG: ..." after
 * the command that triggered it.
 */
#include <SDL2/SDL_keycode.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/ai_popup.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/game_dialogs.h"
#include "core/game_loop.h"
#include "core/game_loop_internal.h"
#include "core/map.h"
#include "core/map_panel.h"
#include "core/screen_geom.h"
#include "core/settings.h"
#include "core/units.h"
#include "core/window_log.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/* ===== Input platform ===== */

enum { E_MOVE, E_DOWN, E_UP, E_KEY_DOWN, E_KEY_UP, E_TEXT };

typedef struct {
  uint32_t frame;
  int type;
  int x, y;
  int button; /* 1 left, 3 right */
  int32_t keycode;
  unsigned mod;
  char ch;
} Ev;

struct ColonizePlatform {
  Ev* evs;
  size_t count, cap, next;
  uint32_t frame;  /* frame the next poll serves */
  uint32_t now_ms;
  int mouse_x, mouse_y;
  bool left_down, right_down;
  unsigned mod;
  bool outer_on;
  ColonizePalette outer;
  int outer_x, outer_y, outer_w, outer_h;
};

static ColonizePlatform g_p;
static uint32_t g_at; /* frame the next scheduled event lands on */

static void ev(int type, int x, int y, int button, int32_t keycode, unsigned mod, char ch) {
  if (g_p.count == g_p.cap) {
    g_p.cap = g_p.cap ? g_p.cap * 2 : 256;
    g_p.evs = realloc(g_p.evs, g_p.cap * sizeof(Ev));
  }
  g_p.evs[g_p.count++] = (Ev){g_at, type, x, y, button, keycode, mod, ch};
}

bool platform_poll_input(ColonizePlatform* p, ColonizeInputState* out) {
  out->text_input_len = 0;
  out->text_input[0] = '\0';
  const uint32_t f = p->frame++;
  p->now_ms += 16;
  for (; p->next < p->count && p->evs[p->next].frame <= f; ++p->next) {
    const Ev* e = &p->evs[p->next];
    switch (e->type) {
      case E_MOVE:
        p->mouse_x = e->x;
        p->mouse_y = e->y;
        break;
      case E_DOWN:
      case E_UP: {
        const bool down = e->type == E_DOWN;
        p->mouse_x = e->x;
        p->mouse_y = e->y;
        if (e->button == 1) {
          p->left_down = down;
          *(down ? &out->mouse_left_clicked : &out->mouse_left_released) = true;
        } else {
          p->right_down = down;
          *(down ? &out->mouse_right_clicked : &out->mouse_right_released) = true;
        }
        break;
      }
      case E_KEY_DOWN: {
        const ColonizeKey k = platform_key_from_sdl(e->keycode);
        if (k != COLONIZE_KEY_NONE) {
          out->last_key = k;
        }
        p->mod = e->mod;
        break;
      }
      case E_KEY_UP:
        p->mod = e->mod;
        break;
      case E_TEXT:
        if (out->text_input_len + 1 < COLONIZE_TEXT_INPUT_MAX) {
          out->text_input[out->text_input_len++] = e->ch;
          out->text_input[out->text_input_len] = '\0';
        }
        break;
      default:
        break;
    }
  }
  out->mouse_x = p->mouse_x;
  out->mouse_y = p->mouse_y;
  out->mouse_left_down = p->left_down;
  out->mouse_right_down = p->right_down;
  out->alt_held = (p->mod & KMOD_ALT) != 0;
  out->shift_held = (p->mod & KMOD_SHIFT) != 0;
  out->ctrl_held = (p->mod & KMOD_CTRL) != 0;
  return true;
}

bool platform_present(ColonizePlatform* p, const ColonizeFramebuffer8* fb, const ColonizePalette* pal) {
  (void)p;
  (void)fb;
  (void)pal;
  return true;
}

void platform_set_outer_palette(ColonizePlatform* p, const ColonizePalette* outer, int x, int y, int w, int h) {
  p->outer_on = outer != NULL;
  if (outer) {
    p->outer = *outer;
    p->outer_x = x;
    p->outer_y = y;
    p->outer_w = w;
    p->outer_h = h;
  }
}

/* ===== Commands ===== */

static int32_t keycode(const char* name) {
  static const struct { const char* n; int32_t k; } t[] = {
    {"Return", SDLK_RETURN}, {"Escape", SDLK_ESCAPE}, {"space", SDLK_SPACE},
    {"Backspace", SDLK_BACKSPACE}, {"Tab", SDLK_TAB}, {"Delete", SDLK_DELETE},
    {"Up", SDLK_UP}, {"Down", SDLK_DOWN}, {"Left", SDLK_LEFT}, {"Right", SDLK_RIGHT},
    {"KP_1", SDLK_KP_1}, {"KP_2", SDLK_KP_2}, {"KP_3", SDLK_KP_3}, {"KP_4", SDLK_KP_4},
    {"KP_5", SDLK_KP_5}, {"KP_6", SDLK_KP_6}, {"KP_7", SDLK_KP_7}, {"KP_8", SDLK_KP_8},
    {"KP_9", SDLK_KP_9}, {"KP_Enter", SDLK_KP_ENTER}, {"Home", SDLK_HOME}, {"End", SDLK_END},
    {"F1", SDLK_F1}, {"F2", SDLK_F2}, {"F3", SDLK_F3}, {"F4", SDLK_F4}, {"F5", SDLK_F5},
    {"F6", SDLK_F6}, {"F7", SDLK_F7}, {"F8", SDLK_F8}, {"F9", SDLK_F9}, {"F10", SDLK_F10},
  };
  for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
    if (strcmp(name, t[i].n) == 0) {
      return t[i].k;
    }
  }
  if (strlen(name) == 1 && (unsigned char)name[0] > 32 && (unsigned char)name[0] < 127) {
    return (int32_t)(name[0] >= 'A' && name[0] <= 'Z' ? name[0] - 'A' + 'a' : name[0]);
  }
  return -1;
}

static const struct { const char* n; int32_t k; unsigned bit; } k_mods[] = {
  {"shift", SDLK_LSHIFT, KMOD_LSHIFT}, {"ctrl", SDLK_LCTRL, KMOD_LCTRL}, {"alt", SDLK_LALT, KMOD_LALT},
};

static int mod_index(const char* s) {
  for (int i = 0; s && i < 3; ++i) {
    if (strcmp(s, k_mods[i].n) == 0) {
      return i;
    }
  }
  return -1;
}

static void mod_down(int m) {
  if (m >= 0) {
    ev(E_KEY_DOWN, 0, 0, 0, k_mods[m].k, k_mods[m].bit, 0);
    g_at++;
  }
}

static void mod_up(int m) {
  if (m >= 0) {
    ev(E_KEY_UP, 0, 0, 0, k_mods[m].k, 0, 0);
    g_at++;
  }
}

static void tap(int32_t code, int m, char text) {
  const unsigned bit = m >= 0 ? k_mods[m].bit : 0;
  mod_down(m);
  ev(E_KEY_DOWN, 0, 0, 0, code, bit, 0);
  if (text) {
    ev(E_TEXT, 0, 0, 0, 0, 0, text);
  }
  g_at += 3;
  ev(E_KEY_UP, 0, 0, 0, code, bit, 0);
  g_at++;
  mod_up(m);
}

static void press(int x, int y, int button, int m) {
  mod_down(m);
  ev(E_MOVE, x, y, 0, 0, 0, 0);
  g_at++;
  ev(E_DOWN, x, y, button, 0, 0, 0);
  g_at += 3;
  ev(E_UP, x, y, button, 0, 0, 0);
  g_at++;
  mod_up(m);
}

static ColonizeGameState* g_game;
static ColonizeFramebuffer8 g_fb;
static ColonizePalette g_pal;
static char g_dir[512];
static unsigned g_log_seen;

static const char* off_map_reason(const ColonizeGameState* g) {
  return g->in_menu          ? "title menu"
       : g->in_colony        ? "colony screen"
       : g->in_europe        ? "Europe screen"
       : g->in_pedia         ? "Colonizopedia"
       : g->in_report        ? "report screen"
       : g->in_debug_atlas   ? "debug atlas"
       : game_modal_open(g)  ? "modal open"
       : ai_popup_busy(&g->ai_popups) ? "popup open or queued"
       : game_turn_busy(g)   ? "turn in progress"
                             : NULL;
}

static bool run_frame(void) {
  ColonizeInputState input = {0};
  const uint32_t before = g_p.now_ms;
  platform_poll_input(&g_p, &input);
  if (!game_update(g_game, &input, g_p.now_ms - before)) {
    return false;
  }
  game_render(g_game, &g_fb, &g_pal);
  return true;
}

/* Runs frames until every scheduled event has been served, plus `extra`. */
static bool flush_frames(int extra) {
  while (g_p.next < g_p.count || g_p.frame < g_at) {
    if (!run_frame()) {
      return false;
    }
  }
  for (int i = 0; i < extra; ++i) {
    if (!run_frame()) {
      return false;
    }
  }
  g_at = g_p.frame;
  return true;
}

static void print_new_log(void) {
  const unsigned total = window_log_total();
  unsigned n = total - g_log_seen;
  if (n > (unsigned)window_log_count()) {
    printf("LOG: (%u older lines lost)\n", n - (unsigned)window_log_count());
    n = (unsigned)window_log_count();
  }
  for (int age = (int)n - 1; age >= 0; --age) {
    printf("LOG: %s\n", window_log_line(age));
  }
  g_log_seen = total;
  const AiPopupState* pop = &g_game->ai_popups;
  if (pop->open && pop->current.choice_count > 0) {
    printf("CHOICES:");
    for (int i = 0; i < pop->current.choice_count; ++i) {
      printf(" [%d] %s", i, pop->current.choices[i]);
    }
    printf("\n");
  }
}

static void write_shot(const char* name) {
  char path[700];
  snprintf(path, sizeof(path), "%s/shots/%s.ppm", g_dir, name);
  FILE* f = fopen(path, "wb");
  if (!f) {
    printf("ERR cannot write %s\n", path);
    return;
  }
  fprintf(f, "P6\n%d %d\n255\n", g_fb.width, g_fb.height);
  const ColonizePlatform* p = &g_p;
  for (int i = 0; i < g_fb.width * g_fb.height; ++i) {
    const int x = i % g_fb.width;
    const int y = i / g_fb.width;
    const bool outer = p->outer_on &&
      (x < p->outer_x || x >= p->outer_x + p->outer_w || y < p->outer_y || y >= p->outer_y + p->outer_h);
    fwrite((outer ? &p->outer : &g_pal)->rgb[g_fb.pixels[i]], 1, 3, f);
  }
  fclose(f);
  printf("SHOT %s\n", path);
}

static const char* unit_name(const ColonizeUnit* u) {
  return u->type_index >= 0 && u->type_index < COLONIZE_UNIT_TYPES_MAX ? g_game->units.types[u->type_index].name : "?";
}

static void print_status(void) {
  ColonizeGameState* g = g_game;
  int cols = 0, rows = 0, vx = 0, vy = 0;
  game_map_zoom_view_size(g->map_zoom, &cols, &rows);
  map_panel_clamp_view_origin((int)g->world_map.width, (int)g->world_map.height, g->map_view_x, g->map_view_y,
                              cols, rows, &vx, &vy);
  const char* why = off_map_reason(g);
  printf("STATUS turn %u year %u%s; %s; screen: %s (%s); popup tag %d\n", (unsigned)g->turn_number,
         (unsigned)g->game_year, g->game_autumn ? " autumn" : "", game_status_text(g), why ? why : "main map",
         g->log_screen, game_ai_popup_tag(g));
  if (!g->in_menu && g->human_nation >= 0 && g->human_nation < 4) {
    printf("STATUS gold %u; view origin (%d,%d) %dx%d tiles of %dpx, y0 %d; cursor (%d,%d)\n",
           (unsigned)europe_nation_gold(&g->europe, &g->col1, g->human_nation), vx, vy, cols, rows,
           game_map_zoom_tile_px(g->map_zoom), MAP_VIEW_ORIGIN_Y, g->map_cursor_x, g->map_cursor_y);
  }
  const ColonizeUnit* sel = g->units.selected_id >= 0 ? units_get_const(&g->units, g->units.selected_id) : NULL;
  if (sel) {
    printf("STATUS selected unit %d %s at (%d,%d) moves %d orders %d\n", sel->id, unit_name(sel), sel->x, sel->y,
           sel->moves, sel->orders);
  }
}

static void print_units(void) {
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(&g_game->units, i);
    if (!u || !u->active || u->nation_id != g_game->human_nation) {
      continue;
    }
    printf("UNIT %d %s at (%d,%d) moves %d orders %d prof %d", u->id, unit_name(u), u->x, u->y, u->moves,
           u->orders, u->profession);
    if (u->aboard_ship_id >= 0) {
      printf(" aboard %d", u->aboard_ship_id);
    }
    if (u->tools || u->muskets || u->horses) {
      printf(" tools %d muskets %d horses %d", u->tools, u->muskets, u->horses);
    }
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (u->hold_goods_amount[h] > 0) {
        printf(" hold[%d] %d x%d", h, u->hold_goods_type[h], u->hold_goods_amount[h]);
      }
    }
    printf("\n");
  }
}

static void print_colonies(void) {
  for (int i = 0; i < g_game->colonies.colony_count; ++i) {
    const ColonizeColony* c = &g_game->colonies.colonies[i];
    if (c->active) {
      printf("COLONY %d %s nation %d at (%d,%d) pop %d\n", i, c->name, c->nation_id, c->x, c->y, c->population);
    }
  }
}

/* ASCII map around the cursor, two chars a tile: terrain (lower case, upper
 * case = forested; ~ ocean, = high seas, a arctic, blank = unexplored) then
 * an overlay (@ cursor, C colony, V village/other city, u unit, ? rumour,
 * # mountain, n hill, r river, . nothing). */
static void print_map(int radius) {
  const ColonizeWorldMap* m = &g_game->world_map;
  const int cx = g_game->map_cursor_x, cy = g_game->map_cursor_y;
  printf("MAP x %d..%d (cols), y %d..%d (rows)\n", cx - radius, cx + radius, cy - radius, cy + radius);
  printf("MAP     ");
  for (int x = cx - radius; x <= cx + radius; ++x) {
    printf("%2d", ((x % 100) + 100) % 100);
  }
  printf("\n");
  for (int y = cy - radius; y <= cy + radius; ++y) {
    printf("MAP %3d ", y);
    for (int x = cx - radius; x <= cx + radius; ++x) {
      if (x < 0 || y < 0 || x >= m->width || y >= m->height ||
          !map_tile_seen_by(m, x, y, g_game->human_nation)) {
        printf("  ");
        continue;
      }
      const uint8_t t = m->terrain[(size_t)y * m->width + x];
      const uint8_t l2 = m->layer2[(size_t)y * m->width + x];
      const int base = t & 0x1f;
      char c = base == T_OCEAN ? '~' : base == T_HIGH_SEAS ? '=' : base == T_ARCTIC ? 'a'
             : "tdprgsmw"[base & 7];
      if (base < T_ARCTIC && (base & T_FOREST_BIT)) {
        c = (char)(c - 32);
      }
      bool colony = false;
      for (int i = 0; i < g_game->colonies.colony_count; ++i) {
        const ColonizeColony* col = &g_game->colonies.colonies[i];
        colony |= col->active && col->x == x && col->y == y;
      }
      const char o = (x == cx && y == cy) ? '@' : colony ? 'C' : (l2 & MAP_OCCUPANCY_HAS_CITY) ? 'V'
                   : (l2 & MAP_OCCUPANCY_HAS_UNIT) ? 'u' : map_tile_has_rumour(m, x, y) ? '?'
                   : (t & F_MOUNTAIN) ? '#' : (t & F_HILL) ? 'n' : (t & F_RIVER) ? 'r' : '.';
      printf("%c%c", c, o);
    }
    printf("\n");
  }
}

/* One command line; false = quit. */
static bool command(char* line) {
  char* argv[8] = {0};
  int argc = 0;
  char* rest = NULL;
  for (char* s = line; *s && argc < 8;) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
      ++s;
    }
    if (!*s) {
      break;
    }
    argv[argc++] = s;
    while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') {
      ++s;
    }
    if (*s) {
      *s++ = '\0';
    }
    if (strcmp(argv[0], "type") == 0) {
      rest = s; /* rest of the line is the text */
      break;
    }
  }
  if (argc == 0 || argv[0][0] == '#') {
    return true;
  }
  const char* op = argv[0];
  const int a1 = argc > 1 ? atoi(argv[1]) : 0;
  const int a2 = argc > 2 ? atoi(argv[2]) : 0;
  bool ok = true;
  if (strcmp(op, "key") == 0 && argc > 1) {
    const int32_t code = keycode(argv[1]);
    const int m = mod_index(argv[2]);
    if (code < 0) {
      printf("ERR unknown key %s\n", argv[1]);
      return true;
    }
    const bool printable = code >= 32 && code < 127 && m != 1 && m != 2;
    char ch = printable ? (char)code : 0;
    if (printable && m == 0) {
      static const char from[] = "1234567890-=[]`", to[] = "!@#$%^&*()_+{}~";
      const char* hit = strchr(from, ch);
      ch = hit ? to[hit - from] : (ch >= 'a' && ch <= 'z' ? (char)(ch - 32) : ch);
    }
    tap(code, m, ch);
    ok = flush_frames(10);
  } else if (strcmp(op, "type") == 0) {
    const char* text = rest ? rest : "";
    for (const char* c = text; *c && *c != '\n' && *c != '\r'; ++c) {
      ev(E_TEXT, 0, 0, 0, 0, 0, *c);
      g_at += 2;
    }
    ok = flush_frames(5);
  } else if ((strcmp(op, "click") == 0 || strcmp(op, "rclick") == 0) && argc > 2) {
    press(a1, a2, op[0] == 'r' ? 3 : 1, mod_index(argv[3]));
    ok = flush_frames(10);
  } else if (strcmp(op, "dclick") == 0 && argc > 2) {
    press(a1, a2, 1, -1);
    g_at += 2;
    press(a1, a2, 1, -1);
    ok = flush_frames(10);
  } else if (strcmp(op, "drag") == 0 && argc > 4) {
    const int x2 = atoi(argv[3]), y2 = atoi(argv[4]);
    const int m = mod_index(argv[5]);
    mod_down(m);
    ev(E_MOVE, a1, a2, 0, 0, 0, 0);
    g_at++;
    ev(E_DOWN, a1, a2, 1, 0, 0, 0);
    g_at += 2;
    for (int i = 1; i <= 8; ++i) {
      ev(E_MOVE, a1 + (x2 - a1) * i / 8, a2 + (y2 - a2) * i / 8, 0, 0, 0, 0);
      g_at++;
    }
    g_at += 2;
    ev(E_UP, x2, y2, 1, 0, 0, 0);
    g_at++;
    mod_up(m);
    ok = flush_frames(10);
  } else if (strcmp(op, "move") == 0 && argc > 2) {
    ev(E_MOVE, a1, a2, 0, 0, 0, 0);
    g_at++;
    ok = flush_frames(2);
  } else if (strcmp(op, "wait") == 0) {
    ok = flush_frames(a1 > 0 ? a1 : 1);
  } else if (strcmp(op, "idle") == 0) {
    const int max = a1 > 0 ? a1 : 20000;
    int settled = 0, n = 0;
    for (; n < max && settled < 20 && ok; ++n) {
      ok = run_frame();
      settled = off_map_reason(g_game) ? 0 : settled + 1;
    }
    g_at = g_p.frame;
    printf("IDLE %s after %d frames\n", settled >= 20 ? "reached" : "NOT reached", n);
  } else if (strcmp(op, "shot") == 0 && argc > 1) {
    write_shot(argv[1]);
  } else if (strcmp(op, "save") == 0 && argc > 1) {
    char err[256];
    if (game_save_col1_named(g_game, argv[1], err, sizeof(err))) {
      printf("SAVED %s\n", argv[1]);
    } else {
      printf("ERR save failed: %s\n", err);
    }
  } else if (strcmp(op, "status") == 0) {
    print_status();
  } else if (strcmp(op, "units") == 0) {
    print_units();
  } else if (strcmp(op, "map") == 0) {
    print_map(a1 > 0 ? a1 : 8);
  } else if (strcmp(op, "colonies") == 0) {
    print_colonies();
  } else if (strcmp(op, "sync") == 0) {
    printf("@@ %s\n", argc > 1 ? argv[1] : "");
  } else if (strcmp(op, "quit") == 0) {
    return false;
  } else {
    printf("ERR bad command %s\n", op);
  }
  print_new_log();
  if (!ok) {
    printf("GAME QUIT\n");
  }
  fflush(stdout);
  return ok;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: play_driver DIR [--load SAVE] [--seed N]\n");
    return 2;
  }
  snprintf(g_dir, sizeof(g_dir), "%s", argv[1]);
  const char* load = NULL;
  bool seed_set = false;
  uint32_t seed = 0;
  for (int i = 2; i + 1 < argc; i += 2) {
    if (strcmp(argv[i], "--load") == 0) {
      load = argv[i + 1];
    } else if (strcmp(argv[i], "--seed") == 0) {
      seed = (uint32_t)strtoul(argv[i + 1], NULL, 0);
      seed_set = true;
    }
  }
  char path[700], saves[600];
  snprintf(saves, sizeof(saves), "%s/saves", g_dir);
  mkdir(g_dir, 0755);
  mkdir(saves, 0755);
  snprintf(path, sizeof(path), "%s/shots", g_dir);
  mkdir(path, 0755);
  diag_init(argc, argv); /* opencol.log next to the binary (tools/play.py runs a copy in DIR/bin) */
  snprintf(path, sizeof(path), "%s/settings.json", g_dir);
  FILE* sf = fopen(path, "r");
  if (sf) {
    fclose(sf);
  } else if ((sf = fopen(path, "w")) != NULL) {
    fputs("{\"version\": 1, \"no_sound\": true, \"skip_intro\": true,\n"
          " \"debug\": {\"trace_autosaves\": true, \"logs\": true}}\n", sf);
    fclose(sf);
  }
  char err[256];
  if (!settings_init(path, err, sizeof(err))) {
    fprintf(stderr, "settings: %s\n", err);
  }
  diag_set_info_enabled(settings_get()->debug_logs);
  units_set_unlimited(settings_get()->unlimited_units);

  screen_geom_set(SCREEN_BASE_W, SCREEN_BASE_H);
  ColonizeGameConfig cfg = {
    .data_dir = "./COLONIZE", .save_dir = saves, .rng_seed = seed, .rng_seed_set = seed_set,
    .debug_logs = settings_get()->debug_logs, .debug_logs_set = true,
  };
  if (seed_set) {
    dos_rng_set_tick_word((int32_t)(seed & 0x7fffu));
  }
  g_game = game_create(&cfg);
  if (!g_game || !game_assets_ok(g_game)) {
    fprintf(stderr, "no COLONIZE/ assets (run from the repo root)\n");
    return 1;
  }
  if (load && !game_load_col1_file(g_game, load, err, sizeof(err))) {
    fprintf(stderr, "cannot load %s: %s\n", load, err);
    return 1;
  }
  game_set_platform(g_game, &g_p);
  static uint8_t pixels[SCREEN_BASE_W * SCREEN_BASE_H];
  g_fb = (ColonizeFramebuffer8){.width = SCREEN_BASE_W, .height = SCREEN_BASE_H, .pixels = pixels};
  for (int i = 0; i < 30; ++i) {
    run_frame();
  }
  g_at = g_p.frame;
  print_status();
  print_new_log();
  printf("@@ ready\n");
  fflush(stdout);
  char line[1024];
  while (fgets(line, sizeof(line), stdin) && command(line)) {
  }
  game_destroy(g_game);
  diag_shutdown();
  return 0;
}
