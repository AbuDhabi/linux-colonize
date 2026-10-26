/*
 * Input-replay regressions (tests/README.md "Input-replay regressions").
 *
 * Every subdirectory of tests-automated-regression/ is one test:
 *   input.sav    save the test starts from (taken on the main map)
 *   input.jsonl  settings.json debug.input_logger log of the test span
 *   output.sav   save taken on the main map after the last input
 * The harness loads input.sav, feeds the logged events back frame by frame
 * through a replay platform, lets the game settle on the main map, saves,
 * and requires the save to match output.sav byte for byte.
 *
 * The replay platform stands in for sdl_runtime.c: platform_poll_input serves
 * one logged frame per call (the logger's `frame` = poll count), so the
 * nested polls inside game_update (combat analysis, combat popup pump) eat
 * the same frames they ate live. The game clock follows the logged event
 * times (main.c's dt), interpolated over frames that carried no event.
 *
 * Each test runs in a forked child (module statics do not leak between
 * tests). COLONIZE_TEST_ONLY=<dir name> runs one, COLONIZE_TEST_LIST=1 lists.
 * Failure artefacts land in <binary dir>/regression_out/<name>/.
 */
#include <SDL2/SDL_keycode.h>

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "col1_json.h"
#include "core/ai_popup.h"
#include "core/game_dialogs.h"
#include "core/game_loop.h"
#include "core/game_loop_internal.h"
#include "core/map_panel.h"
#include "core/units.h"
#include "core/json_min.h"
#include "core/screen_geom.h"
#include "core/settings.h"
#include "platform/platform.h"
#include "tests/common/test_runner.h"

#define REGRESSION_ROOT "tests-automated-regression"
/* Frames on the main map, after the last input, before the save is taken. */
#define SETTLE_FRAMES 60
/* main.c sleeps 16 ms a frame; idle frames outside the log advance this much. */
#define IDLE_FRAME_MS 16u
/* ponytail: floor so a one-keypress log (duration ~0) can still load the
 * frames it needs; the spec's 2x duration is the limit above it. */
#define MIN_TIME_LIMIT_MS 1000

enum {
  R_MOVE, R_DOWN, R_UP, R_KEY_DOWN, R_KEY_UP, R_TEXT, R_FOCUS_LOST, R_RESIZE, R_QUIT, R_IGNORED
};

typedef struct {
  int type;
  uint32_t frame;
  uint32_t t_ms;
  int lx, ly;
  int button; /* 1 left, 3 right, 0 other */
  int32_t keycode;
  unsigned mod;
  char text[32];
} Rec;

struct ColonizePlatform {
  Rec* recs;
  size_t count;
  size_t next;
  bool replaying;  /* false: idle frames (before the log / after it ran out) */
  uint32_t frame;  /* recorded frame the next poll serves */
  uint32_t now_ms; /* clock of the frame last served */
  uint32_t anchor_frame, anchor_ms; /* last frame that carried a record */
  int mouse_x, mouse_y;
  bool left_down, right_down;
  unsigned mod;
  /* platform_set_outer_palette (a screen over the map), for the screenshots. */
  bool outer_on;
  ColonizePalette outer;
  int outer_x, outer_y, outer_w, outer_h;
};

/* ===== Log parsing ===== */

static char* read_text(const char* path, size_t* size_out) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  const long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* buf = size >= 0 ? malloc((size_t)size + 1) : NULL;
  if (buf && fread(buf, 1, (size_t)size, f) != (size_t)size) {
    free(buf);
    buf = NULL;
  }
  fclose(f);
  if (buf) {
    buf[size] = '\0';
    if (size_out) {
      *size_out = (size_t)size;
    }
  }
  return buf;
}

static int rec_type(const char* t) {
  static const struct { const char* name; int type; } k[] = {
    {"mouse_move", R_MOVE}, {"mouse_down", R_DOWN}, {"mouse_up", R_UP},
    {"key_down", R_KEY_DOWN}, {"key_up", R_KEY_UP}, {"text", R_TEXT},
    {"focus_lost", R_FOCUS_LOST}, {"resize", R_RESIZE}, {"quit", R_QUIT},
  };
  for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
    if (strcmp(t, k[i].name) == 0) {
      return k[i].type;
    }
  }
  return R_IGNORED; /* mouse_wheel, focus_gained: platform_poll_input ignores them */
}

static int64_t get_i64(const JsonValue* o, const char* key) {
  int64_t v = 0;
  (void)json_get_i64(o, key, &v);
  return v;
}

/* Fills the platform's record list and the session screen size. */
static bool parse_log(
  const char* path, ColonizePlatform* p, int* screen_w, int* screen_h, bool* audio
) {
  char* text = read_text(path, NULL);
  if (!text) {
    fprintf(stderr, "  cannot read %s\n", path);
    return false;
  }
  bool ok = true;
  bool have_session = false;
  size_t cap = 0;
  int line_no = 0;
  for (char* line = text; line && *line && ok; ) {
    char* nl = strchr(line, '\n');
    if (nl) {
      *nl = '\0';
    }
    ++line_no;
    if (line[0] && line[0] != '\r') {
      char err[128];
      JsonValue* v = json_parse(line, err, sizeof(err));
      const char* type = v ? json_get_str(v, "type") : NULL;
      if (!type) {
        fprintf(stderr, "  %s:%d: not a log record (%s)\n", path, line_no, v ? "no type" : err);
        ok = false;
      } else if (strcmp(type, "session") == 0) {
        have_session = true;
        *screen_w = (int)get_i64(v, "screen_w");
        *screen_h = (int)get_i64(v, "screen_h");
        (void)json_get_bool(v, "audio", audio);
      } else {
        if (p->count == cap) {
          cap = cap ? cap * 2 : 256;
          p->recs = realloc(p->recs, cap * sizeof(Rec));
        }
        Rec* r = &p->recs[p->count++];
        memset(r, 0, sizeof(*r));
        r->type = rec_type(type);
        r->frame = (uint32_t)get_i64(v, "frame");
        r->t_ms = (uint32_t)get_i64(v, "t_ms");
        r->lx = (int)get_i64(v, "lx");
        r->ly = (int)get_i64(v, "ly");
        const char* button = json_get_str(v, "button");
        r->button = !button ? 0 : strcmp(button, "left") == 0 ? 1 : strcmp(button, "right") == 0 ? 3 : 0;
        r->keycode = (int32_t)get_i64(v, "keycode");
        r->mod = (unsigned)get_i64(v, "mod");
        const char* t = json_get_str(v, "text");
        snprintf(r->text, sizeof(r->text), "%s", t ? t : "");
        if (p->count > 1 && r->frame < p->recs[p->count - 2].frame) {
          fprintf(stderr, "  %s:%d: frame goes backwards\n", path, line_no);
          ok = false;
        } else if (r->type == R_RESIZE) {
          /* ponytail: the replay keeps the session's screen size; a resize
           * would need the log strip height the log does not carry. */
          fprintf(stderr, "  %s:%d: window resized during the test (unsupported)\n", path, line_no);
          ok = false;
        }
      }
      json_free(v);
    }
    line = nl ? nl + 1 : NULL;
  }
  free(text);
  if (ok && !have_session) {
    fprintf(stderr, "  %s: no \"session\" line (keep the log's first line when trimming)\n", path);
    ok = false;
  }
  if (ok && p->count == 0) {
    fprintf(stderr, "  %s: no input records\n", path);
    ok = false;
  }
  if (ok && (*screen_w < SCREEN_BASE_W || *screen_h < SCREEN_BASE_H ||
             *screen_w > SCREEN_MAX_W || *screen_h > SCREEN_MAX_H)) {
    fprintf(stderr, "  %s: bad session screen size %dx%d\n", path, *screen_w, *screen_h);
    ok = false;
  }
  return ok;
}

/* ===== Replay platform ===== */

/* Folds one record the way platform_poll_input (sdl_runtime.c) folds the SDL
 * event it was logged from; keep the two in step. */
static void fold(ColonizePlatform* p, const Rec* r, ColonizeInputState* out) {
  switch (r->type) {
    case R_MOVE:
      p->mouse_x = r->lx;
      p->mouse_y = r->ly;
      break;
    case R_DOWN:
    case R_UP: {
      const bool down = r->type == R_DOWN;
      p->mouse_x = r->lx;
      p->mouse_y = r->ly;
      if (r->button == 1) {
        p->left_down = down;
        *(down ? &out->mouse_left_clicked : &out->mouse_left_released) = true;
      } else if (r->button == 3) {
        p->right_down = down;
        *(down ? &out->mouse_right_clicked : &out->mouse_right_released) = true;
      }
      break;
    }
    case R_KEY_DOWN: {
      const ColonizeKey k = platform_key_from_sdl(r->keycode);
      if (k != COLONIZE_KEY_NONE) {
        out->last_key = k;
      }
      p->mod = r->mod; /* SDL updates its mod state before stamping the event */
      break;
    }
    case R_KEY_UP:
      p->mod = r->mod;
      break;
    case R_TEXT:
      if (r->text[0] && (unsigned char)r->text[0] < 0x80 &&
          out->text_input_len + 1 < COLONIZE_TEXT_INPUT_MAX) {
        out->text_input[out->text_input_len++] = r->text[0];
        out->text_input[out->text_input_len] = '\0';
      }
      break;
    case R_FOCUS_LOST:
      if (p->left_down) {
        out->mouse_left_released = true;
      }
      if (p->right_down) {
        out->mouse_right_released = true;
      }
      p->left_down = false;
      p->right_down = false;
      break;
    case R_QUIT:
      out->quit_requested = true;
      break;
    default:
      break;
  }
}

bool platform_poll_input(ColonizePlatform* p, ColonizeInputState* out) {
  out->text_input_len = 0;
  out->text_input[0] = '\0';
  if (!p->replaying || p->next >= p->count) {
    p->now_ms += IDLE_FRAME_MS;
  } else {
    const uint32_t f = p->frame++;
    const Rec* r = &p->recs[p->next];
    if (r->frame == f) {
      /* SDL stamps a frame's events while this poll pumps them. */
      p->now_ms = p->anchor_ms = r->t_ms;
      p->anchor_frame = f;
      while (p->next < p->count && p->recs[p->next].frame == f) {
        fold(p, &p->recs[p->next++], out);
      }
    } else {
      p->now_ms = p->anchor_ms + (uint32_t)(
        (uint64_t)(r->t_ms - p->anchor_ms) * (f - p->anchor_frame) / (r->frame - p->anchor_frame)
      );
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

bool platform_present(
  ColonizePlatform* platform, const ColonizeFramebuffer8* framebuffer, const ColonizePalette* palette
) {
  (void)platform;
  (void)framebuffer;
  (void)palette;
  return true; /* false would close the nested loops (combat analysis) */
}

void platform_set_outer_palette(
  ColonizePlatform* platform, const ColonizePalette* outer, int x, int y, int w, int h
) {
  platform->outer_on = outer != NULL;
  if (outer) {
    platform->outer = *outer;
    platform->outer_x = x;
    platform->outer_y = y;
    platform->outer_w = w;
    platform->outer_h = h;
  }
}

static const char* off_map_reason(const ColonizeGameState* g);

/* ===== Screenshots ===== */

/* REGRESSION_SHOTS=1 (authoring aid, tools/regression_script.py): binary PPM
 * of the frame the player saw before each mouse/key press, plus the start and
 * end screens, in the test's failure-artefact directory. */
static void write_shot(const char* out_dir, const char* tag, const ColonizeFramebuffer8* fb,
                       const ColonizePalette* pal, const ColonizeGameState* g) {
  /* Map tile (x,y) is drawn at screen (16*(x-vx), 8+16*(y-vy)) at zoom 0. */
  int cols = 0, rows = 0, vx = 0, vy = 0;
  game_map_zoom_view_size(g->map_zoom, &cols, &rows);
  map_panel_clamp_view_origin((int)g->world_map.width, (int)g->world_map.height, g->map_view_x,
                              g->map_view_y, cols, rows, &vx, &vy);
  const ColonizeUnit* sel = g->units.selected_id >= 0 ? units_get_const(&g->units, g->units.selected_id) : NULL;
  printf("  shot %s: view origin (%d,%d) tile %dpx y0 %d; cursor (%d,%d); selected unit %d",
         tag, vx, vy, game_map_zoom_tile_px(g->map_zoom), MAP_VIEW_ORIGIN_Y, g->map_cursor_x,
         g->map_cursor_y, sel ? sel->id : -1);
  if (sel) {
    printf(" at (%d,%d) moves %d", sel->x, sel->y, sel->moves);
  }
  printf("; %s\n", off_map_reason(g) ? off_map_reason(g) : "main map");
  char path[700];
  snprintf(path, sizeof(path), "%s/shot_%s.ppm", out_dir, tag);
  FILE* f = fopen(path, "wb");
  if (!f) {
    return;
  }
  fprintf(f, "P6\n%d %d\n255\n", fb->width, fb->height);
  const ColonizePlatform* p = g->platform;
  for (int i = 0; i < fb->width * fb->height; ++i) {
    const int x = i % fb->width;
    const int y = i / fb->width;
    const bool outer = p && p->outer_on &&
      (x < p->outer_x || x >= p->outer_x + p->outer_w || y < p->outer_y || y >= p->outer_y + p->outer_h);
    fwrite((outer ? &p->outer : pal)->rgb[fb->pixels[i]], 1, 3, f);
  }
  fclose(f);
}

static bool press_next(const ColonizePlatform* p) {
  for (size_t i = p->next; i < p->count && p->recs[i].frame == p->frame; ++i) {
    if (p->recs[i].type == R_DOWN || p->recs[i].type == R_KEY_DOWN) {
      return true;
    }
  }
  return false;
}

/* ===== Save comparison ===== */

static int g_diff_lines;

static void json_diff(const JsonValue* a, const JsonValue* b, char* path, size_t len) {
  enum { MAX_DIFF_LINES = 40 };
  if (g_diff_lines >= MAX_DIFF_LINES) {
    return;
  }
  const size_t cap = 512;
  if (!a || !b || a->type != b->type) {
    printf("    %s: %s\n", path, !a ? "only in actual" : !b ? "only in expected" : "type differs");
    ++g_diff_lines;
  } else if (a->type == JV_OBJ) {
    for (size_t i = 0; i < a->obj.count; ++i) {
      snprintf(path + len, cap - len, ".%s", a->obj.keys[i]);
      json_diff(a->obj.vals[i], json_obj_get(b, a->obj.keys[i]), path, strlen(path));
    }
    for (size_t i = 0; i < b->obj.count; ++i) {
      if (!json_obj_get(a, b->obj.keys[i])) {
        snprintf(path + len, cap - len, ".%s", b->obj.keys[i]);
        json_diff(NULL, b->obj.vals[i], path, strlen(path));
      }
    }
  } else if (a->type == JV_ARR) {
    if (a->arr.count != b->arr.count) {
      printf("    %s: length expected %zu, actual %zu\n", path, a->arr.count, b->arr.count);
      ++g_diff_lines;
    }
    for (size_t i = 0; i < a->arr.count && i < b->arr.count; ++i) {
      snprintf(path + len, cap - len, "[%zu]", i);
      json_diff(a->arr.items[i], b->arr.items[i], path, strlen(path));
    }
  } else if (a->type == JV_NUM && a->num != b->num) {
    printf("    %s: expected %.17g, actual %.17g\n", path, a->num, b->num);
    ++g_diff_lines;
  } else if (a->type == JV_BOOL && a->b != b->b) {
    printf("    %s: expected %s, actual %s\n", path, a->b ? "true" : "false", b->b ? "true" : "false");
    ++g_diff_lines;
  } else if (a->type == JV_STR && strcmp(a->str, b->str) != 0) {
    if (strlen(a->str) <= 40 && strlen(b->str) <= 40) {
      printf("    %s: expected \"%s\", actual \"%s\"\n", path, a->str, b->str);
    } else {
      size_t i = 0;
      while (a->str[i] && a->str[i] == b->str[i]) {
        ++i;
      }
      printf("    %s: long string differs from char %zu\n", path, i);
    }
    ++g_diff_lines;
  }
  path[len] = '\0';
}

/* sav_json dump of `sav` next to it (`json`); NULL tree on failure. */
static JsonValue* save_to_json(const char* sav, const char* json) {
  ColonizeCol1Save s;
  col1_save_init(&s);
  char err[256];
  JsonValue* tree = NULL;
  FILE* f = NULL;
  if (col1_save_read_file(sav, &s, err, sizeof(err)) && (f = fopen(json, "wb")) != NULL) {
    col1_write_json(f, &s);
    fclose(f);
    char* text = read_text(json, NULL);
    tree = text ? json_parse(text, err, sizeof(err)) : NULL;
    free(text);
  }
  if (!tree) {
    printf("  could not dump %s as JSON: %s\n", sav, err);
  }
  col1_save_free(&s);
  return tree;
}

static bool compare_saves(const char* expected, const char* actual, const char* out_dir) {
  size_t exp_size = 0, act_size = 0;
  char* exp = read_text(expected, &exp_size);
  char* act = read_text(actual, &act_size);
  bool same = exp && act && exp_size == act_size && memcmp(exp, act, exp_size) == 0;
  if (!same && exp && act) {
    size_t i = 0;
    while (i < exp_size && i < act_size && exp[i] == act[i]) {
      ++i;
    }
    printf("  save mismatch: sizes expected %zu, actual %zu; first differing byte 0x%zx\n",
           exp_size, act_size, i);
    char exp_json[700], act_json[700];
    snprintf(exp_json, sizeof(exp_json), "%s/expected.json", out_dir);
    snprintf(act_json, sizeof(act_json), "%s/actual.json", out_dir);
    JsonValue* je = save_to_json(expected, exp_json);
    JsonValue* ja = save_to_json(actual, act_json);
    if (je && ja) {
      char path[512] = "";
      g_diff_lines = 0;
      printf("  field diff (first 40; full dumps %s, %s):\n", exp_json, act_json);
      json_diff(je, ja, path, 0);
      if (g_diff_lines == 0) {
        printf("    (none: the difference is outside the sav_json fields, e.g. the port extension)\n");
      }
    }
    json_free(je);
    json_free(ja);
  } else if (!exp) {
    printf("  cannot read %s\n", expected);
  }
  free(exp);
  free(act);
  return same;
}

/* ===== One test ===== */

static uint64_t wall_ms(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

/* What keeps the game off the main map (NULL = on it, idle): the test's
 * start/end state. */
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

static void make_dirs(const char* path) {
  char buf[600];
  snprintf(buf, sizeof(buf), "%s", path);
  for (char* s = buf + 1; *s; ++s) {
    if (*s == '/') {
      *s = '\0';
      mkdir(buf, 0755);
      *s = '/';
    }
  }
  mkdir(buf, 0755);
}

static const char* g_out_root = "regression_out";

static int run_test(const char* name) {
  char dir[512], in_sav[600], in_log[600], out_sav[600], out_dir[600];
  snprintf(dir, sizeof(dir), "%s/%s", REGRESSION_ROOT, name);
  snprintf(in_sav, sizeof(in_sav), "%s/input.sav", dir);
  snprintf(in_log, sizeof(in_log), "%s/input.jsonl", dir);
  snprintf(out_sav, sizeof(out_sav), "%s/output.sav", dir);
  snprintf(out_dir, sizeof(out_dir), "%s/%s", g_out_root, name);

  ColonizePlatform rp;
  memset(&rp, 0, sizeof(rp));
  int screen_w = 0, screen_h = 0;
  bool audio = false;
  if (!parse_log(in_log, &rp, &screen_w, &screen_h, &audio)) {
    return 1;
  }
  if (audio) {
    printf("  warning: recorded with audio on; music picks reseed the game RNG from the\n"
           "  wall clock (sound.c sound_pump), so replay can diverge. Record with --nosound.\n");
  }
  for (size_t i = 0; i < rp.count; ++i) {
    if (rp.recs[i].type == R_MOVE || rp.recs[i].type == R_DOWN || rp.recs[i].type == R_UP) {
      rp.mouse_x = rp.recs[i].lx; /* best guess where the pointer rested */
      rp.mouse_y = rp.recs[i].ly;
      break;
    }
  }
  const uint32_t duration_ms = rp.recs[rp.count - 1].t_ms - rp.recs[0].t_ms;
  const uint64_t limit_ms =
    2u * (uint64_t)duration_ms > MIN_TIME_LIMIT_MS ? 2u * (uint64_t)duration_ms : MIN_TIME_LIMIT_MS;
  make_dirs(out_dir);
  /* The options dialogs flush settings.json: keep it in the test's out dir (fresh
   * defaults every run), not ./settings.json at the repo root. */
  char settings_file[640];
  snprintf(settings_file, sizeof(settings_file), "%s/settings.json", out_dir);
  remove(settings_file);
  (void)settings_init(settings_file, NULL, 0);
#ifndef _WIN32
  /* The limit above is checked between frames; this catches one frame that
   * never returns (a stray Return ending the turn runs the AI inline). */
  alarm((unsigned)(limit_ms / 1000u) + 30u);
#endif

  screen_geom_set(screen_w, screen_h);
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = out_dir};
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    printf("  no COLONIZE/ assets\n");
    return TR_SKIP;
  }
  char err[256];
  if (!game_load_col1_file(game, in_sav, err, sizeof(err))) {
    printf("  cannot load %s: %s\n", in_sav, err);
    return 1;
  }
  game_set_platform(game, &rp);
  /* The tests stay inside one turn, so AI and end-of-turn changes never
   * move their expected saves. */
  const unsigned start_turn = game->col1.head.turn;
  const char* shots = getenv("REGRESSION_SHOTS");
  const bool want_shots = shots && shots[0] && strcmp(shots, "0") != 0;

  uint8_t* pixels = malloc((size_t)screen_w * (size_t)screen_h);
  ColonizeFramebuffer8 fb = {.width = screen_w, .height = screen_h, .pixels = pixels};
  ColonizePalette pal;
  const uint64_t start = wall_ms();
  uint32_t prev_ms = rp.now_ms;
  int settled = 0;
  const char* phase = "start (waiting for the main map)";
  int rc = 1;
  for (;;) {
    ColonizeInputState input = {0};
    platform_poll_input(&rp, &input);
    const uint32_t dt = rp.now_ms - prev_ms;
    prev_ms = rp.now_ms;
    if (!game_update(game, &input, dt)) {
      printf("  game quit during %s\n", phase);
      goto done;
    }
    game_render(game, &fb, &pal);
    if (!rp.replaying) {
      /* A hot-seat load opens on its @MULTINEXT (docs/hotseat.md); the
       * log answers it. */
      const bool hotseat_hello =
        game->ai_popups.open && game->ai_popups.current.tag == AI_POPUP_TAG_MULTINEXT;
      if (rp.next == 0 && (!off_map_reason(game) || hotseat_hello)) {
        /* Loaded and idle: hand the frames over to the log. */
        rp.replaying = true;
        rp.frame = rp.recs[0].frame;
        prev_ms = rp.recs[0].t_ms - IDLE_FRAME_MS;
        phase = "replay";
        if (want_shots) {
          write_shot(out_dir, "start", &fb, &pal, game);
        }
      }
    } else if (rp.next < rp.count) {
      if (want_shots && press_next(&rp)) {
        char tag[32];
        snprintf(tag, sizeof(tag), "%06u", (unsigned)rp.frame);
        write_shot(out_dir, tag, &fb, &pal, game);
      }
    } else {
      phase = "settle (after the last input)";
      settled = off_map_reason(game) ? 0 : settled + 1;
      if (settled >= SETTLE_FRAMES) {
        break;
      }
    }
    if (wall_ms() - start > limit_ms) {
      const char* why = off_map_reason(game);
      printf("  timeout: %llu ms (2x input span %u ms) during %s at frame %u; %s; status: %s\n",
             (unsigned long long)limit_ms, (unsigned)duration_ms, phase, (unsigned)rp.frame,
             why ? why : "on the main map", game_status_text(game));
      goto done;
    }
  }

  if (want_shots) {
    write_shot(out_dir, "end", &fb, &pal, game);
  }
  if (game->col1.head.turn != start_turn) {
    printf("  the test crossed a turn boundary (turn %u -> %u); keep it inside one turn\n",
           start_turn, (unsigned)game->col1.head.turn);
    goto done;
  }
  if (!game_save_col1_named(game, "actual.sav", err, sizeof(err))) {
    printf("  save failed: %s\n", err);
    goto done;
  }
  char act_sav[700];
  snprintf(act_sav, sizeof(act_sav), "%s/actual.sav", out_dir);
  if (compare_saves(out_sav, act_sav, out_dir)) {
    rc = 0;
  } else {
    printf("  actual save: %s\n", act_sav);
  }
done:
  free(pixels);
  free(rp.recs);
  game_destroy(game);
  return rc;
}

/* ===== Discovery ===== */

static int cmp_str(const void* a, const void* b) {
  return strcmp((const char*)a, (const char*)b);
}

static int run_isolated(const char* name) {
#ifdef _WIN32
  return run_test(name);
#else
  fflush(stdout);
  fflush(stderr);
  const pid_t pid = fork();
  if (pid == 0) {
    const int rc = run_test(name);
    fflush(stdout);
    _exit(rc);
  }
  int status = 0;
  if (pid < 0 || waitpid(pid, &status, 0) < 0) {
    printf("  fork/wait failed\n");
    return 1;
  }
  if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) {
    printf("  hung: one frame ran 30 s past the time limit (did the turn end?)\n");
    return 1;
  }
  if (WIFSIGNALED(status)) {
    printf("  crashed (signal %d)\n", WTERMSIG(status));
    return 1;
  }
  return WEXITSTATUS(status);
#endif
}

int main(int argc, char** argv) {
  (void)argc;
  /* Failure artefacts go next to the binary: build/debug/regression_out. */
  static char out_root[512];
  const char* slash = strrchr(argv[0], '/');
  if (slash) {
    snprintf(out_root, sizeof(out_root), "%.*s/regression_out", (int)(slash - argv[0]), argv[0]);
    g_out_root = out_root;
  }

  static char names[1024][256];
  int n = 0;
  DIR* d = opendir(REGRESSION_ROOT);
  for (struct dirent* e; d && (e = readdir(d)) != NULL && n < 1024; ) {
    char path[600];
    struct stat st;
    snprintf(path, sizeof(path), "%s/%s", REGRESSION_ROOT, e->d_name);
    if (e->d_name[0] != '.' && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
      snprintf(names[n++], sizeof(names[0]), "%s", e->d_name);
    }
  }
  if (d) {
    closedir(d);
  }
  qsort(names, (size_t)n, sizeof(names[0]), cmp_str);

  const char* list = getenv("COLONIZE_TEST_LIST");
  if (list && list[0] && strcmp(list, "0") != 0) {
    for (int i = 0; i < n; ++i) {
      printf("%s\n", names[i]);
    }
    return 0;
  }
  const char* only = getenv("COLONIZE_TEST_ONLY");
  int ran = 0, failed = 0, skipped = 0;
  for (int i = 0; i < n; ++i) {
    if (only && only[0] && strcmp(only, names[i]) != 0) {
      continue;
    }
    ++ran;
    printf("RUN  %s\n", names[i]);
    const int rc = run_isolated(names[i]);
    printf("%s %s\n", rc == 0 ? "PASS" : rc == TR_SKIP ? "SKIP" : "FAIL", names[i]);
    failed += rc != 0 && rc != TR_SKIP;
    skipped += rc == TR_SKIP;
  }
  if (only && only[0] && ran == 0) {
    printf("no test named %s in %s/\n", only, REGRESSION_ROOT);
    return 1;
  }
  printf("%d run, %d failed, %d skipped\n", ran, failed, skipped);
  if (failed) {
    return 1;
  }
  return ran == skipped ? TR_SKIP : 0;
}
