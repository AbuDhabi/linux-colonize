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
 *   choose N                    answer the open choice popup with row N (arrows + Return)
 *   type TEXT                   text input, one char per frame (name fields)
 *   click X Y [shift|ctrl|alt]  left click; rclick X Y; dclick X Y
 *   drag X1 Y1 X2 Y2 [shift]    left-button drag
 *   mdown X Y | mup X Y         bare left press / release (hand-paced drag)
 *   move X Y                    pointer move
 *   wait N                      N idle frames
 *   idle [MAX]                  run until the main map has been idle 20 frames
 *                               (no popup, no turn in progress), at most MAX
 *                               frames (default 20000)
 *   auto [MAX|zoom]             play on, answering message popups, woodcuts and colony
 *                               "Continue turn" messages (unless zoom) with Return,
 *                               closes the Congress report and pedia page of a new
 *                               founding father,
 *                               until a choice popup, another screen or modal, or
 *                               the idle main map (default 5000 frames)
 *   shot NAME                   DIR/shots/NAME.ppm
 *   save NAME                   save into DIR/saves/NAME
 *   status | units | colonies   print state
 *   map [R [X Y]]               ASCII map of radius R (default 8) around the cursor (or X,Y)
 *   res [R [X Y]]               seen special resources (fish, ore, ...) within R of the cursor (or X,Y)
 *   near X Y [R]                every unit (any nation) within R (default 2) of X,Y
 *   goto ID X Y                 Go To order for unit ID to tile X,Y
 *   colony N                    open colony N's screen (index from `colonies`)
 *   field N CI TILE JOB         colonist CI of colony N onto ring tile TILE (0 N .. 7 NW), field job JOB
 *   work N CI B                 colonist CI of colony N into @BUILDING row B (as `hit` prints)
 *   build N BID [queue]         colony N builds BID (@BUILDING row, or unit code 42 artillery..)
 *   equip UID ROLE              unit UID on a colony tile takes role ROLE (COLONIZE_EJECT_*, 4 dragoon)
 *   admit UID                   unit UID on a colony tile joins that colony (prints colonist index)
 *   eject N CI ROLE             colonist CI of colony N leaves as ROLE (COLONIZE_EJECT_*, 0 colonist)
 *   endturn                     confirm the End of Turn prompt (error while a unit waits)
 *   world                       whole map, one char a tile
 *   rumours                     list seen rumour tiles with terrain (h hill, m mountain)
 *   activate ID                 wake and select unit ID (the stack-list pick by id;
 *                               in game, A on a stack opens the picker)
 *   sync TAG                    print "@@ TAG" (end-of-batch marker)
 *   quit
 *
 * Every popup / ticker text the game presents is echoed as "LOG: ..." after
 * the command that triggered it. A nested modal loop inside one game_update
 * (Combat Analysis) gets a Return after 300 idle polls ("NESTED:" line).
 */
#include <SDL2/SDL_keycode.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/ai_popup.h"
#include "core/colony_preview.h"
#include "core/colony_screen.h"
#include "core/colony_production.h"
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

static int g_update_polls; /* polls inside the current game_update */
static void tap(int32_t code, int m, char text);

bool platform_poll_input(ColonizePlatform* p, ColonizeInputState* out) {
  /* A nested modal loop inside one game_update (Combat Analysis, combat
   * popup pump) cannot see later stdin commands: after 300 idle polls it
   * gets a Return, as a player would press. */
  if (++g_update_polls > 300 && p->next >= p->count) {
    g_update_polls = 0;
    g_at = p->frame;
    tap(SDLK_RETURN, -1, 0);
    printf("NESTED: Return pressed in a modal loop\n");
  }
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

/* A human unit still walking a goto this turn (the map looks idle between
 * its steps). */
static bool goto_pending(void) {
  const ColonizeUnit* sel =
    g_game->units.selected_id >= 0 ? units_get_const(&g_game->units, g_game->units.selected_id) : NULL;
  if (sel && sel->orders != 3) {
    return false; /* the selected unit waits for orders first */
  }
  for (int i = 0; i < g_game->units.slot_end; ++i) {
    const ColonizeUnit* u = &g_game->units.units[i];
    if (u->active && u->nation_id == g_game->human_nation && u->orders == 3 && u->moves > 0 &&
        u->aboard_ship_id < 0) {
      return true;
    }
  }
  return false;
}

static bool run_frame(void) {
  ColonizeInputState input = {0};
  const uint32_t before = g_p.now_ms;
  g_update_polls = 0;
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
  /* Choice rows once per popup opening, not after every command. */
  static char shown[AI_POPUP_BODY_LEN + 8];
  const AiPopupState* pop = &g_game->ai_popups;
  if (!pop->open) {
    shown[0] = '\0';
  } else if (pop->current.choice_count > 0 && strcmp(shown, pop->current.body) != 0) {
    snprintf(shown, sizeof(shown), "%s", pop->current.body);
    printf("CHOICES:");
    for (int i = 0; i < pop->current.choice_count; ++i) {
      printf(" [%d] %s", i, pop->current.choices[i]);
    }
    printf(" (sel %d)\n", pop->selection);
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
  for (int i = 0; i < g_game->units.slot_end; ++i) {
    const ColonizeUnit* u = &g_game->units.units[i];
    if (u->nation_id != g_game->human_nation || (!u->active && u->type_index == 0 && u->x == 0)) {
      continue;
    }
    if (!u->active) {
      printf("(inactive) ");
    }
    printf("UNIT %d %s at (%d,%d) moves %d orders %d prof %d", u->id, unit_name(u), u->x, u->y, u->moves,
           u->orders, u->profession);
    if (u->aboard_ship_id >= 0) {
      printf(" aboard %d", u->aboard_ship_id);
    }
    if (u->col1_flags15 & 0x80u) {
      printf(" damaged");
    }
    /* units_founder_loot drops the +0x3159 residue byte a non-Pioneer keeps
     * (playtest C3-B2: a colonist reloaded as "tools 100"). */
    int tools = 0, muskets = 0, horses = 0;
    units_founder_loot(&g_game->units, u->id, &tools, &muskets, &horses);
    if (tools || muskets || horses) {
      printf(" tools %d muskets %d horses %d", tools, muskets, horses);
    }
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (u->hold_goods_amount[h] > 0) {
        printf(" hold[%d] %d x%d", h, u->hold_goods_type[h], u->hold_goods_amount[h]);
      }
    }
    printf("\n");
  }
}

/* Every unit (any nation) within `r` tiles of (cx,cy). */
static void print_near(int cx, int cy, int r) {
  for (int i = 0; i < g_game->units.slot_end; ++i) {
    const ColonizeUnit* u = &g_game->units.units[i];
    if (!u->active || u->x < cx - r || u->x > cx + r || u->y < cy - r || u->y > cy + r) {
      continue;
    }
    printf("NEAR %d %s nation %d at (%d,%d) moves %d orders %d%s\n", u->id, unit_name(u), u->nation_id, u->x,
           u->y, u->moves, u->orders, u->aboard_ship_id >= 0 ? " aboard" : "");
  }
}

static void print_europe_ships(const char* what, const EuropeHarborShip* ships, int n) {
  for (int i = 0; i < n; ++i) {
    const EuropeHarborShip* sh = &ships[i];
    printf("EUROPE %s %s turns_left %d passengers", what, sh->name, sh->turns_left);
    for (int c = 0; c < sh->cargo_count; ++c) {
      printf(" prof%d", sh->cargo_professions[c]);
    }
    for (int h = 0; h < EUROPE_SHIP_CARGO_MAX; ++h) {
      if (sh->hold_goods_amount[h] > 0) {
        printf(" hold %d x%d", sh->hold_goods_type[h], sh->hold_goods_amount[h]);
      }
    }
    printf("\n");
  }
}

static void print_europe(void) {
  const EuropeScreen* eu = &g_game->europe;
  print_europe_ships("harbor", eu->harbor, eu->harbor_ships);
  print_europe_ships("to-new-world", eu->bound, eu->bound_ships);
  print_europe_ships("to-europe", eu->expected, eu->expected_ships);
  for (int i = 0; i < eu->dock_count; ++i) {
    if (eu->dock[i].present) {
      printf("EUROPE dock %s prof %d%s\n", eu->dock[i].name, eu->dock[i].profession,
             eu->dock[i].sentry ? " (boards next ship)" : "");
    }
  }
}

static void print_colonies(void) {
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &g_game->colonies.colonies[i];
    if (!c->active) {
      continue;
    }
    printf("COLONY %d %s nation %d at (%d,%d) pop %d\n", i, c->name, c->nation_id, c->x, c->y, c->population);
    if (c->nation_id != g_game->human_nation) {
      continue;
    }
    /* Human colonies: build, stock, this turn's production preview, roster. */
    const ColonizeWorld w = game_world(g_game);
    ColonizeColonyPreview pv;
    colony_preview_compute_w(&w, c, &pv);
    const int b = c->building_in_production;
    printf("  build %s hammers %d | SoL %d%% | food %+d bells %d crosses %d hammers +%d\n",
           b >= 0 && b < g_game->colonies.building_type_count ? g_game->colonies.building_types[b].name
           : b >= 0 ? "unit" : "-",
           c->hammers, colony_prod_sol_percent(&g_game->col1, c), pv.food_net, pv.bells, pv.crosses, pv.hammers);
    static const char* const k_cargo[COLONIZE_CARGO_COUNT] = {
      "food", "sugar", "tobacco", "cotton", "furs", "lumber", "ore", "silver",
      "horses", "rum", "cigars", "cloth", "coats", "tradegoods", "tools", "muskets"};
    printf("  stock");
    for (int k = 0; k < COLONIZE_CARGO_COUNT; ++k) {
      if (c->stock[k] || pv.goods[k]) {
        printf(" %s %d(%+d)", k_cargo[k], c->stock[k], pv.goods[k]);
      }
    }
    printf("\n");
    for (int k = 0; k < c->colonist_count; ++k) {
      const ColonizeColonist* p = &c->colonists[k];
      int slot = -1;
      for (int s = 0; s < COLONIZE_COLONY_FIELD_TILES_MAX; ++s) {
        slot = c->tiles[s] == k ? s : slot;
      }
      printf("  colonist %d prof %d %s%s field_job %d slot %d\n", k, p->profession,
             p->building_type >= 0 ? "in " : "",
             p->building_type >= 0 && p->building_type < g_game->colonies.building_type_count
               ? g_game->colonies.building_types[p->building_type].name : "",
             p->field_job, slot);
    }
  }
}

/* ASCII map around the cursor, two chars a tile: terrain (lower case, upper
 * case = forested; ~ ocean, = high seas, a arctic, blank = unexplored) then
 * an overlay (@ cursor, C colony, V village/other city, u unit, ? rumour,
 * # mountain, n hill, r river, . nothing). */
static void print_map(int radius, int cx, int cy) {
  const ColonizeWorldMap* m = &g_game->world_map;
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
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
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
    /* Escape on the bare main map asks @DOSYES (default Yes, C2-B1); the
     * driver's next Return would quit the program. Refuse it unless forced
     * with `key Escape force`. */
    if (code == SDLK_ESCAPE && !off_map_reason(g_game) && !g_game->ai_popups.open &&
        !(argc > 2 && strcmp(argv[2], "force") == 0)) {
      printf("ERR Escape on the main map refused (asks Exit to DOS); use 'key Escape force'\n");
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
  } else if (strcmp(op, "choose") == 0 && argc > 1) {
    /* Answer the open choice popup with row N: arrow keys to the row, then
     * Return (a bare Return takes whatever row is highlighted). */
    const AiPopupState* pop = &g_game->ai_popups;
    ok = flush_frames(10);
    for (int tries = 0; ok && tries < 5 && strcmp(g_game->log_screen, "woodcut") == 0; ++tries) {
      tap(SDLK_RETURN, -1, 0); /* a woodcut in front of the popup */
      ok = flush_frames(10);
    }
    for (int tries = 0; ok && pop->open && pop->selection != a1 && tries < 40; ++tries) {
      tap(pop->selection < a1 ? SDLK_DOWN : SDLK_UP, -1, 0);
      ok = flush_frames(2);
    }
    if (!pop->open || pop->selection != a1) {
      printf("ERR choose %d: popup %s, selection %d\n", a1, pop->open ? "open" : "closed", pop->selection);
    } else {
      /* Some popups (portraits, a woodcut in front) ignore keys for a while:
       * press again while the same popup is still up. */
      char body[AI_POPUP_BODY_LEN];
      snprintf(body, sizeof(body), "%s", pop->current.body);
      for (int tries = 0; ok && tries < 6; ++tries) {
        tap(SDLK_RETURN, -1, 0);
        ok = flush_frames(10);
        if (!pop->open || strcmp(body, pop->current.body) != 0) {
          break;
        }
      }
    }
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
  } else if ((strcmp(op, "mdown") == 0 || strcmp(op, "mup") == 0) && argc > 2) {
    /* Bare left press / release (a hand-paced drag: mdown, move..., mup). */
    ev(E_MOVE, a1, a2, 0, 0, 0, 0);
    g_at++;
    ev(op[1] == 'd' ? E_DOWN : E_UP, a1, a2, 1, 0, 0, 0);
    g_at++;
    ok = flush_frames(5);
  } else if (strcmp(op, "endturn") == 0) {
    /* Confirm the End of Turn prompt (what Space / the sidebar click do);
     * refuses while a unit still waits for orders. */
    if (game_end_turn_prompt_active(g_game)) {
      game_do_end_turn(g_game);
      ok = flush_frames(5);
    } else {
      printf("ERR no End of Turn prompt (selected unit %d)\n", g_game->units.selected_id);
    }
  } else if (strcmp(op, "hit") == 0 && argc > 2) {
    /* Colony-screen hit test at X,Y (ColonyScreenHit kind + index). */
    const ColonizeColony* c = colonies_get(&g_game->colonies, g_game->colony_view_id);
    if (c) {
      const ColonyScreenHitResult h = colony_screen_hit_test(
        &g_game->colony_screen, &g_game->colonies, c, &g_game->units, a1, a2);
      const ColonyScreenView* csv = &g_game->colony_screen;
      const int outside_id = h.kind == COLONY_HIT_OUTSIDE_UNIT && h.index >= 0 &&
                                 h.index < csv->outside_unit_count
                               ? csv->outside_unit_ids[h.index] : -1;
      printf("HIT kind %d index %d %s", (int)h.kind, h.index,
             h.kind == COLONY_HIT_BUILDING && h.index >= 0 && h.index < g_game->colonies.building_type_count
               ? g_game->colonies.building_types[h.index].name : "");
      if (outside_id >= 0) {
        printf("unit %d", outside_id);
      }
      printf("\n");
    }
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
      settled = off_map_reason(g_game) || goto_pending() ? 0 : settled + 1;
    }
    g_at = g_p.frame;
    printf("IDLE %s after %d frames\n", settled >= 20 ? "reached" : "NOT reached", n);
  } else if (strcmp(op, "auto") == 0) {
    /* Play on until a decision is needed: message popups and woodcuts are
     * answered with Return; stops at a popup with 2+ choices, any other
     * screen or modal, or the main map idle for 20 frames. */
    const int max = a1 > 0 ? a1 : 5000;
    int settled = 0, n = 0;
    const char* why = "frame limit";
    for (; n < max && ok; ++n) {
      ok = run_frame();
      const AiPopupState* pop = &g_game->ai_popups;
      const bool busy = g_p.next < g_p.count;
      /* Colony EOT messages ("Continue turn." / "Zoom to colony.") are
       * answered with the default Continue unless `auto zoom`. */
      if (pop->open && pop->current.choice_count >= 2 &&
          (pop->current.tag != AI_POPUP_TAG_COLONY_EVENT || (argc > 1 && strcmp(argv[1], "zoom") == 0))) {
        why = "choice popup";
        break;
      }
      if ((pop->open || strcmp(g_game->log_screen, "woodcut") == 0) && !busy) {
        g_at = g_p.frame;
        /* Some colony messages default to "Zoom to colony.": step up to Continue. */
        tap(pop->open && pop->current.tag == AI_POPUP_TAG_COLONY_EVENT && pop->selection > 0 ? SDLK_UP : SDLK_RETURN,
            -1, 0);
        print_new_log();
        continue;
      }
      /* A founding father's arrival: the Congress report, then his pedia page. */
      if (!pop->open && !busy && (strncmp(g_game->log_screen, "report:CONTINENTAL", 18) == 0 ||
                                  strcmp(g_game->log_screen, "pedia:article") == 0)) {
        g_at = g_p.frame;
        tap(SDLK_ESCAPE, -1, 0);
        continue;
      }
      if (!pop->open && (game_screen_owns_display(g_game) || game_modal_open(g_game)) && !busy) {
        why = g_game->log_screen;
        break;
      }
      settled = off_map_reason(g_game) || goto_pending() ? 0 : settled + 1;
      if (settled >= 20) {
        why = "map idle";
        break;
      }
    }
    g_at = g_p.frame;
    printf("AUTO stopped (%s) after %d frames\n", why, n);
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
    print_europe();
  } else if (strcmp(op, "activate") == 0 && argc > 1) {
    /* What picking a unit from the tile's stack list does, by unit id. */
    game_click_activate_unit(g_game, a1);
    ok = flush_frames(5);
  } else if (strcmp(op, "goto") == 0 && argc > 3) {
    /* Go To order for unit ID to tile X,Y (what a map goto drag sets). */
    const ColonizeWorld w = game_world(g_game);
    if (!units_set_goto_w(&w, a1, a2, atoi(argv[3]))) {
      printf("ERR goto refused\n");
    }
    ok = flush_frames(5);
  } else if (strcmp(op, "colony") == 0 && argc > 1) {
    /* Open colony N's screen (index as printed by `colonies`). */
    if (a1 < 0 || a1 >= COLONIZE_COLONIES_MAX || !g_game->colonies.colonies[a1].active) {
      printf("ERR no colony %d\n", a1);
      return true;
    }
    game_enter_colony(g_game, g_game->colonies.colonies[a1].id); /* takes the id, not the index */
    ok = flush_frames(10);
  } else if ((strcmp(op, "field") == 0 && argc > 4) || (strcmp(op, "work") == 0 && argc > 3)) {
    /* field N CI TILE JOB: colonist CI of colony N onto ring tile TILE (0 N .. 7 NW) as
     * field @JOB JOB (0 farmer .. 8 fisherman); work N CI B: into @BUILDING row B (`hit`
     * prints it). The same shared writers the colony screen's drops end in. */
    if (a1 < 0 || a1 >= COLONIZE_COLONIES_MAX || !g_game->colonies.colonies[a1].active) {
      printf("ERR no colony %d\n", a1);
      return true;
    }
    const int id = g_game->colonies.colonies[a1].id;
    const ColonizeWorld w = game_world(g_game);
    const bool done = op[0] == 'f' ? colonies_assign_field_w(&w, id, a2, atoi(argv[3]), atoi(argv[4]))
                                   : colonies_assign_workplace(&g_game->colonies, id, a2, atoi(argv[3]));
    if (!done) {
      printf("ERR %s refused\n", op);
    }
    ok = flush_frames(2);
  } else if (strcmp(op, "admit") == 0 && argc > 1) {
    /* admit UID: unit UID standing on one of its colonies joins it (the colony
     * screen's fence-to-colony drop; the fence row only draws the first few units).
     * Prints the new colonist index for `work` / `field`. */
    const ColonizeUnit* u = units_get(&g_game->units, a1);
    const int cid = u ? colonies_id_at(&g_game->colonies, u->x, u->y) : -1;
    const ColonizeWorld w = game_world(g_game);
    const int ci = cid >= 0 ? colonies_admit_unit_w(&w, cid, a1) : -1;
    if (ci < 0) {
      printf("ERR admit refused\n");
    } else {
      printf("ADMIT colonist %d\n", ci);
    }
    ok = flush_frames(2);
  } else if (strcmp(op, "eject") == 0 && argc > 3) {
    /* eject N CI ROLE: colonist CI of colony N leaves as ROLE (COLONIZE_EJECT_*, 0 colonist),
     * the colony screen's colonist-to-fence drop (unreachable when the fence row is full). */
    if (a1 < 0 || a1 >= COLONIZE_COLONIES_MAX || !g_game->colonies.colonies[a1].active) {
      printf("ERR no colony %d\n", a1);
      return true;
    }
    const int uid = colonies_eject_colonist(&g_game->colonies, g_game->colonies.colonies[a1].id, a2,
                                            &g_game->units, atoi(argv[3]));
    if (uid < 0) {
      printf("ERR eject refused\n");
    } else {
      printf("EJECT unit %d\n", uid);
    }
    ok = flush_frames(2);
  } else if (strcmp(op, "equip") == 0 && argc > 2) {
    /* equip UID ROLE: the colony screen's outside-unit role menu (COLONIZE_EJECT_*: 4 dragoon),
     * for a unit standing on one of its colonies; the screen row only shows the first few. */
    const ColonizeUnit* u = units_get(&g_game->units, a1);
    const int cid = u ? colonies_id_at(&g_game->colonies, u->x, u->y) : -1;
    ColonizeColony* c = cid >= 0 ? colonies_get_mut(&g_game->colonies, cid) : NULL;
    if (!c || !game_colony_apply_outside_role(&g_game->colonies, c, &g_game->units, a1, a2)) {
      printf("ERR equip refused\n");
    }
    ok = flush_frames(2);
  } else if (strcmp(op, "build") == 0 && argc > 2) {
    /* build N BID [queue]: colony N's construction pick, as a click (or a Shift-click with
     * `queue`) on that build-menu row does; BID = @BUILDING row or unit code (42 artillery,
     * 43 wagon train ...). Opens the colony screen; `key Escape` closes it. */
    if (a1 < 0 || a1 >= COLONIZE_COLONIES_MAX || !g_game->colonies.colonies[a1].active) {
      printf("ERR no colony %d\n", a1);
      return true;
    }
    game_enter_colony(g_game, g_game->colonies.colonies[a1].id);
    flush_frames(10);
    if (argc > 3 && strcmp(argv[3], "queue") == 0) {
      game_colony_queue_construction(g_game, a2);
    } else {
      game_colony_commit_construction(g_game, a2);
    }
    printf("BUILD %s now %d\n", g_game->colonies.colonies[a1].name,
           g_game->colonies.colonies[a1].building_in_production);
    ok = flush_frames(2);
  } else if (strcmp(op, "world") == 0) {
    /* Whole map, one char a tile: ~ water, C/c own/foreign colony, V village,
     * ? rumour, n/# hill/mountain, else the terrain letter as in `map`. */
    const ColonizeWorldMap* m = &g_game->world_map;
    for (int y = 0; y < m->height; ++y) {
      char row[256];
      int n = 0;
      for (int x = 0; x < m->width && n < 250; ++x) {
        char c = ' ';
        if (map_tile_seen_by(m, x, y, g_game->human_nation)) {
          const uint8_t t = m->terrain[(size_t)y * m->width + x];
          const int base = t & 0x1f;
          c = (base == T_OCEAN || base == T_HIGH_SEAS) ? '~' : base == T_ARCTIC ? 'a' : "tdprgsmw"[base & 7];
          if (base < T_ARCTIC && (base & T_FOREST_BIT)) {
            c = (char)(c - 32);
          }
          if (t & F_MOUNTAIN) {
            c = '#';
          } else if (t & F_HILL) {
            c = 'n';
          }
          if (map_tile_has_city(m, x, y)) {
            c = 'V';
          }
          for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
            const ColonizeColony* col = &g_game->colonies.colonies[i];
            if (col->active && col->x == x && col->y == y) {
              c = col->nation_id == g_game->human_nation ? 'C' : 'c';
            }
          }
          if (map_tile_has_rumour(m, x, y)) {
            c = '?';
          }
        }
        row[n++] = c;
      }
      row[n] = '\0';
      printf("W%3d %s\n", y, row);
    }
  } else if (strcmp(op, "rumours") == 0) {
    /* Every seen rumour tile: terrain letter as in `map`, h = hill, m = mountain. */
    const ColonizeWorldMap* m = &g_game->world_map;
    for (int y = 0; y < m->height; ++y) {
      for (int x = 0; x < m->width; ++x) {
        if (map_tile_has_rumour(m, x, y) && map_tile_seen_by(m, x, y, g_game->human_nation)) {
          const uint8_t t = m->terrain[(size_t)y * m->width + x];
          const int base = t & 0x1f;
          char c = "tdprgsmw"[base & 7];
          if (base < T_ARCTIC && (base & T_FOREST_BIT)) {
            c = (char)(c - 32);
          }
          printf("RUMOUR (%d,%d) %c%s\n", x, y, c, (t & F_MOUNTAIN) ? " m" : (t & F_HILL) ? " h" : "");
        }
      }
    }
  } else if (strcmp(op, "res") == 0) {
    /* Seen special resources within R (default 8) of the cursor or X,Y. */
    static const char* const k_res[14] = {"depleted", "oasis", "wheat", "cotton", "tobacco", "sugar",
                                          "minerals", "fish", "beaver", "game", "timber", "?11",
                                          "silver", "ore"};
    const ColonizeWorldMap* m = &g_game->world_map;
    const int r = a1 > 0 ? a1 : 8;
    const int cx = argc > 3 ? a2 : g_game->map_cursor_x, cy = argc > 3 ? atoi(argv[3]) : g_game->map_cursor_y;
    for (int y = cy - r; y <= cy + r; ++y) {
      for (int x = cx - r; x <= cx + r; ++x) {
        if (x < 0 || y < 0 || x >= m->width || y >= m->height || !map_tile_seen_by(m, x, y, g_game->human_nation)) {
          continue;
        }
        const int t = map_resource_type_for_yield(m, x, y);
        if (t >= 0 && t < 14) {
          printf("RES (%d,%d) %s\n", x, y, k_res[t]);
        }
      }
    }
  } else if (strcmp(op, "near") == 0 && argc > 2) {
    print_near(a1, a2, argc > 3 ? atoi(argv[3]) : 2);
  } else if (strcmp(op, "map") == 0) {
    print_map(a1 > 0 ? a1 : 8, argc > 3 ? a2 : g_game->map_cursor_x,
              argc > 3 ? atoi(argv[3]) : g_game->map_cursor_y);
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
    .show_building_rects = settings_get()->show_building_rects, .show_building_rects_set = true,
  };
  if (seed_set) {
    dos_rng_set_tick_word((int32_t)(seed & 0x7fffu));
  }
  dos_rng_set_live_ticks(!seed_set); /* same as main.c */
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
