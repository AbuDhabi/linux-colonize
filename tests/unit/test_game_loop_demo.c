/* Autoplay (DS:0x826) and the self-running demo (DS:0x828), src/core/game_loop_demo.c
 * (docs/demo.md). Real COLONIZE/ assets (skips without them) and a scripted platform:
 * every poll returns an empty frame unless a case queued a key or a click. Saves go
 * to a private mkdtemp dir. The cases pin current behaviour. */
#define _DEFAULT_SOURCE /* mkdtemp */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <unistd.h>

#include "core/game_dialogs.h"
#include "core/game_loop.h"
#include "core/game_loop_internal.h"
#include "core/new_game.h"
#include "core/opening.h"
#include "core/settings.h"
#include "core/turn.h"
#include "platform/platform.h"
#include "tests/common/test_runner.h"

/* A French 1599 save (COLONY04.SAV re-saved by the port). */
#define DEMO_START_SAV "tests-automated-regression/map_found_colony/input.sav"
/* Frame cap for a loop that waits on a condition (an AI day is a few hundred). */
#define DEMO_FRAME_CAP 20000

struct ColonizePlatform {
  ColonizeKey key;   /* served once by the next poll */
  char text;
  bool click;        /* left click served once by the next poll */
  int polls;
  int presents;
};

bool platform_poll_input(ColonizePlatform* p, ColonizeInputState* out) {
  memset(out, 0, sizeof(*out));
  p->polls++;
  if (p->key != COLONIZE_KEY_NONE) {
    out->last_key = p->key;
    if (p->text) {
      out->text_input[0] = p->text;
      out->text_input_len = 1;
    }
    p->key = COLONIZE_KEY_NONE;
    p->text = 0;
  }
  if (p->click) {
    out->mouse_left_clicked = true;
    out->mouse_left_down = true;
    p->click = false;
  }
  return true;
}

bool platform_present(
  ColonizePlatform* p, const ColonizeFramebuffer8* fb, const ColonizePalette* pal
) {
  (void)fb;
  (void)pal;
  p->presents++;
  return true;
}

void platform_set_outer_palette(
  ColonizePlatform* p, const ColonizePalette* outer, int x, int y, int w, int h
) {
  (void)p;
  (void)outer;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
}

static char g_dir[64];
static ColonizePlatform g_plat;

static ColonizeGameState* demo_game(bool demo) {
#ifdef _WIN32
  (void)demo;
  return NULL; /* mkdtemp / rm -rf: POSIX only */
#else
  struct stat st;
  if (stat("COLONIZE/VICEROY.EXE", &st) != 0 && stat("COLONIZE/viceroy.exe", &st) != 0) {
    return NULL;
  }
  snprintf(g_dir, sizeof(g_dir), "/tmp/opencol_demo_XXXXXX");
  if (!mkdtemp(g_dir)) {
    return NULL;
  }
  char settings_file[128];
  snprintf(settings_file, sizeof(settings_file), "%s/settings.json", g_dir);
  (void)settings_init(settings_file, NULL, 0);
  ColonizeGameConfig cfg = {.data_dir = "./COLONIZE", .save_dir = g_dir, .demo = demo};
  ColonizeGameState* game = game_create(&cfg);
  if (!game || !game_assets_ok(game)) {
    game_destroy(game);
    return NULL;
  }
  memset(&g_plat, 0, sizeof(g_plat));
  game_set_platform(game, &g_plat);
  return game;
#endif
}

static void demo_cleanup(ColonizeGameState* game) {
  game_destroy(game);
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
  (void)system(cmd);
}

static ColonizeGameState* demo_loaded(bool demo) {
  ColonizeGameState* game = demo_game(demo);
  if (!game) {
    return NULL;
  }
  char err[256];
  if (!game_load_col1_file(game, DEMO_START_SAV, err, sizeof(err))) {
    fprintf(stderr, "load %s: %s\n", DEMO_START_SAV, err);
    demo_cleanup(game);
    return NULL;
  }
  return game;
}

/* One main-loop frame: poll the platform, update, 16 ms. */
static bool demo_frame(ColonizeGameState* game) {
  ColonizeInputState in;
  platform_poll_input(&g_plat, &in);
  return game_update(game, &in, 16);
}

static bool slot5_exists(void) {
  char path[128];
  struct stat st;
  snprintf(path, sizeof(path), "%s/COLONY05.SAV", g_dir);
  return stat(path, &st) == 0;
}

#define CHECK(c)                                                    \
  do {                                                              \
    if (!(c)) {                                                     \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); \
      rc = 1;                                                       \
      goto out;                                                     \
    }                                                               \
  } while (0)

/* Autoplay off: the filter hands the frame's own input through. */
static int demo_filter_passthrough(void) {
  ColonizeGameState* game = demo_loaded(false);
  if (!game) {
    return TR_SKIP;
  }
  int rc = 0;
  ColonizeInputState in = {.last_key = COLONIZE_KEY_A};
  ColonizeInputState scratch;
  CHECK(game_autoplay_filter_input(game, &in, &scratch) == &in);
  /* after_turn is a no-op without autoplay. */
  game_autoplay_after_turn(game);
  CHECK(!game->autoplay);
out:
  demo_cleanup(game);
  return rc;
}

/*
 * CHEAT None + @SETAUTO Yes outside the demo: every Euro control byte 1, autoplay on.
 * The day runs on its own (6d8e watch: the view nation's units, cursor moves and
 * 0x1e-tick pauses presented through the platform); a click during it stops autoplay
 * at the end of the day.
 */
static int demo_autoplay_click_stops(void) {
  ColonizeGameState* game = demo_loaded(false);
  if (!game) {
    return TR_SKIP;
  }
  int rc = 0;
  char err[256];
  CHECK(game_set_all_ai(game, err, sizeof(err)));
  game->col1.head.fixed_nation_map_view = 1; /* watch France (DS:0x53a4) */
  game->autoplay = true;
  const unsigned turn0 = game->col1.head.turn;
  int f = 0;
  for (; f < DEMO_FRAME_CAP &&
         (game->col1.head.turn == turn0 || turn_processor_active(&game->turn_proc));
       ++f) {
    if (f == 3) {
      g_plat.click = true;
    }
    CHECK(demo_frame(game));
  }
  CHECK(game->col1.head.turn == turn0 + 1);
  CHECK(g_plat.presents > 0); /* watch presents + pauses ran */
  CHECK(!game->autoplay);     /* the click stopped it at the end of the day */
  CHECK(!game->autoplay_clicked);
out:
  demo_cleanup(game);
  return rc;
}

/* Autoplay with a dialog up: no input reaches it; after 0x78 ticks the filter answers
 * Enter (a modal) or Escape (a screen owning the display). */
static int demo_autoplay_answers_screen(void) {
  ColonizeGameState* game = demo_loaded(false);
  if (!game) {
    return TR_SKIP;
  }
  int rc = 0;
  game->autoplay = true;
  CHECK(game_try_enter_europe(game));
  CHECK(game->in_europe);
  ColonizeInputState in = {.last_key = COLONIZE_KEY_A, .mouse_left_clicked = true};
  ColonizeInputState scratch;
  const ColonizeInputState* out = game_autoplay_filter_input(game, &in, &scratch);
  CHECK(out == &scratch && out->last_key == COLONIZE_KEY_NONE && !out->mouse_left_clicked);
  CHECK(game->autoplay_clicked); /* noted for the end of the day */
  CHECK(game->autoplay_wait_ms != 0);
  game->elapsed_ms = game->autoplay_wait_ms;
  in = (ColonizeInputState){0};
  out = game_autoplay_filter_input(game, &in, &scratch);
  CHECK(out->last_key == COLONIZE_KEY_ESCAPE);
  CHECK(game->autoplay_wait_ms == 0);
  /* The frame loop: the filter answers Escape again after 0x78 ticks; Europe closes. */
  for (int f = 0; f < 1000 && game->in_europe; ++f) {
    CHECK(demo_frame(game));
  }
  CHECK(!game->in_europe);
  /* A window close during autoplay ends the clock (main loop quits). */
  in = (ColonizeInputState){.quit_requested = true};
  game->autoplay_clicked = false;
  game->in_europe = true;
  (void)game_autoplay_filter_input(game, &in, &scratch);
  CHECK(game->elapsed_ms == UINT32_MAX);
  game->in_europe = false;
out:
  demo_cleanup(game);
  return rc;
}

/*
 * The demo with no COLONY05.SAV: OPENING first (a key there skips it), then
 * FUN_75c2_2778's New World (difficulty 2, the sail pages), every control byte 1,
 * the days run back to back; 'J' at a 6d8e pass ends the cycle: slot 5 is saved and
 * OPENING runs again.
 */
static int demo_new_world_cycle_end(void) {
  ColonizeGameState* game = demo_game(true);
  if (!game) {
    return TR_SKIP;
  }
  int rc = 0;
  int f = 0;
  /* main.c: OPENING first (or straight to the demo start without its art). */
  (void)game_try_start_intro(game);
  for (; f < DEMO_FRAME_CAP && !game->autoplay; ++f) {
    if (game->opening.open && f % 20 == 19) {
      g_plat.key = COLONIZE_KEY_ESCAPE;
    }
    CHECK(demo_frame(game));
  }
  CHECK(game->autoplay && game->demo);
  CHECK(!slot5_exists());
  /* The sail pages run on; the campaign gets built. */
  for (; f < DEMO_FRAME_CAP && !(game->col1_ok && !new_game_active(&game->new_game)); ++f) {
    CHECK(demo_frame(game));
  }
  CHECK(game->col1_ok && !new_game_active(&game->new_game));
  for (int i = 0; i < 4; ++i) {
    CHECK(game->col1.player[i].control == 1);
  }
  CHECK(game->col1.head.difficulty == 2);
  const unsigned turn0 = game->col1.head.turn;
  for (; f < DEMO_FRAME_CAP && game->col1.head.turn == turn0; ++f) {
    CHECK(demo_frame(game));
  }
  CHECK(game->col1.head.turn == turn0 + 1);
  /* 'j' waits for the next 6d8e pass, which ends the cycle. */
  g_plat.key = COLONIZE_KEY_A;
  g_plat.text = 'j';
  for (; f < DEMO_FRAME_CAP && !slot5_exists(); ++f) {
    CHECK(demo_frame(game));
  }
  CHECK(slot5_exists());
  CHECK(!game->autoplay);
  CHECK(!game->demo_end && !game->demo_end_due);
out:
  demo_cleanup(game);
  return rc;
}

/* The demo with a COLONY05.SAV older than 1700 resumes it; any other key at a 6d8e
 * pass saves slot 5 and quits (elapsed_ms = UINT32_MAX ends main.c's loop). */
static int demo_resume_slot5_key_quits(void) {
  ColonizeGameState* game = demo_loaded(true);
  if (!game) {
    return TR_SKIP;
  }
  int rc = 0;
  char err[256];
  CHECK(game_save_col1_slot(game, 5, err, sizeof(err)));
  game_demo_start(game);
  CHECK(game->autoplay && !game->in_menu);
  CHECK(game->units.selected_id == -1 && game->view_pieces_mode);
  CHECK(!new_game_active(&game->new_game));
  CHECK(game_set_all_ai(game, err, sizeof(err)));
  char slot[128];
  snprintf(slot, sizeof(slot), "%s/COLONY05.SAV", g_dir);
  remove(slot);
  g_plat.key = COLONIZE_KEY_A;
  g_plat.text = 'a';
  int f = 0;
  for (; f < DEMO_FRAME_CAP && game->elapsed_ms != UINT32_MAX; ++f) {
    if (!demo_frame(game)) {
      break;
    }
  }
  CHECK(game->elapsed_ms == UINT32_MAX);
  CHECK(slot5_exists());
  CHECK(!game->demo_quit && game->demo_pending_key == 0);
out:
  demo_cleanup(game);
  return rc;
}

/* A key or click on the demo's sail pages exits with nothing saved (FUN_1262_02fe). */
static int demo_sail_key_exits(void) {
  ColonizeGameState* game = demo_game(true);
  if (!game) {
    return TR_SKIP;
  }
  int rc = 0;
  game->in_menu = false;
  game->move_rng.state = 1;
  game_demo_start(game); /* no slot 5: New World */
  CHECK(new_game_active(&game->new_game));
  ColonizeInputState in = {.mouse_right_clicked = true};
  ColonizeInputState scratch;
  const ColonizeInputState* out = game_autoplay_filter_input(game, &in, &scratch);
  CHECK(out == &scratch && !out->mouse_right_clicked);
  CHECK(game->elapsed_ms == UINT32_MAX);
  CHECK(!slot5_exists());
out:
  demo_cleanup(game);
  return rc;
}

/*
 * The year tail every fourth turn (130d_0290 raw 6467-6500): turn % 12 == 0 a splash,
 * turn % 3 == 1 a colony of DS:0x5398 nearest a random point, else its Europe; each
 * view stays 600 ticks (then Escape) and a key there saves slot 5 and quits. After
 * 1725 the cycle ends.
 */
static int demo_year_tail_views(void) {
  ColonizeGameState* game = demo_loaded(true);
  if (!game) {
    return TR_SKIP;
  }
  int rc = 0;
  char err[256];
  CHECK(game_set_all_ai(game, err, sizeof(err)));
  game->autoplay = true;
  game->demo_cycle_start_ms = game->elapsed_ms;
  ColonizeInputState in = {0};
  ColonizeInputState scratch;

  /* turn 8: Europe; it closes itself after 600 ticks. */
  game->turn_number = 8;
  game_autoplay_after_turn(game);
  CHECK(game->in_europe && game->demo_screen_until_ms != 0);
  CHECK(!game->demo_end && !game->demo_end_due);
  CHECK(game_autoplay_filter_input(game, &in, &scratch)->last_key == COLONIZE_KEY_NONE);
  game->elapsed_ms = game->demo_screen_until_ms;
  CHECK(game_autoplay_filter_input(game, &in, &scratch)->last_key == COLONIZE_KEY_ESCAPE);
  CHECK(game->demo_screen_until_ms == 0);
  for (int f = 0; f < 1000 && game->in_europe; ++f) {
    CHECK(demo_frame(game));
  }
  CHECK(!game->in_europe);

  /* turn 4: the French colony nearest a random point; a key there quits. */
  game->turn_number = 4;
  game_autoplay_after_turn(game);
  CHECK(game->in_colony && game->demo_screen_until_ms != 0);
  CHECK(!slot5_exists());
  in.last_key = COLONIZE_KEY_A;
  (void)game_autoplay_filter_input(game, &in, &scratch);
  CHECK(slot5_exists());
  CHECK(game->elapsed_ms == UINT32_MAX && game->demo_screen_until_ms == 0);
  game->in_colony = false;
  game->elapsed_ms = 0;
  in.last_key = COLONIZE_KEY_NONE;

  /* turn 12: the splash tour counts DS:0x150 up until one shows (or 25). */
  game->turn_number = 12;
  game->demo_splash = 0;
  game_autoplay_after_turn(game);
  CHECK(game->demo_splash > 0 && game->demo_splash <= 25);

  /* turn 5: nothing shown; after 1725 the cycle ends. */
  game->turn_number = 5;
  game->game_year = 1726;
  game_autoplay_after_turn(game);
  CHECK(game->demo_end && game->demo_end_due);
  /* With the turn idle, the next filtered frame runs the cycle end. */
  char slot[128];
  snprintf(slot, sizeof(slot), "%s/COLONY05.SAV", g_dir);
  remove(slot);
  CHECK(!turn_processor_active(&game->turn_proc));
  (void)game_autoplay_filter_input(game, &in, &scratch);
  CHECK(slot5_exists());
  CHECK(!game->autoplay && !game->demo_end_due);
out:
  demo_cleanup(game);
  return rc;
}

/* The 6d8e watch hook: ignored without autoplay; a PASS takes the demo's waiting key
 * ('J' ends the cycle, others quit); BEFORE_ACT on a watched unit off screen moves the
 * cursor to it. */
static int demo_watch_hook(void) {
  ColonizeGameState* game = demo_loaded(true);
  if (!game) {
    return TR_SKIP;
  }
  int rc = 0;
  game->demo_pending_key = 'j';
  game_autoplay_watch(game, 1, -1, AI_EURO_WATCH_PASS, false);
  CHECK(game->demo_pending_key == 'j'); /* autoplay off: untouched */
  game->autoplay = true;
  game_autoplay_watch(game, 1, -1, AI_EURO_WATCH_PASS, false);
  CHECK(game->demo_end && game->demo_pending_key == 0);
  game->demo_end = false;
  game->demo_pending_key = 'x';
  game_autoplay_watch(game, 1, -1, AI_EURO_WATCH_PASS, false);
  CHECK(game->demo_quit && !game->demo_end);
  game->demo_quit = false;

  /* A French unit far from the view: the cursor jumps to it before its act. */
  int uid = -1;
  for (int i = 0; i < (int)game->units.unit_count && uid < 0; ++i) {
    const ColonizeUnit* u = units_get_const(&game->units, i);
    if (u && u->active && u->nation_id == 1 && units_is_on_map(u)) {
      uid = i;
    }
  }
  CHECK(uid >= 0);
  const ColonizeUnit* u = units_get_const(&game->units, uid);
  game->col1.head.fixed_nation_map_view = 1;
  game->map_view_x = (u->x + 40) % (int)game->world_map.width;
  game->map_view_y = (u->y + 30) % (int)game->world_map.height;
  const int presents0 = g_plat.presents;
  game_autoplay_watch(game, 1, uid, AI_EURO_WATCH_BEFORE_ACT, false);
  CHECK(game->map_cursor_x == u->x && game->map_cursor_y == u->y);
  CHECK(g_plat.presents == presents0 + 1);
  /* An unwatched nation does nothing. */
  game_autoplay_watch(game, 2, uid, AI_EURO_WATCH_BEFORE_ACT, false);
  CHECK(g_plat.presents == presents0 + 1);
  /* After the unit's last act: the 0x1e-tick pause, presented frame by frame; a key in
   * it waits for the next pass. */
  g_plat.key = COLONIZE_KEY_A;
  g_plat.text = 'J';
  game_autoplay_watch(game, 1, uid, AI_EURO_WATCH_AFTER_ACT, true);
  CHECK(g_plat.presents > presents0 + 10);
  CHECK(game->demo_pending_key == 'J');
out:
  demo_cleanup(game);
  return rc;
}

/* game_demo_after_new_campaign only acts in the demo with a campaign loaded. */
static int demo_after_new_campaign(void) {
  ColonizeGameState* game = demo_loaded(false);
  if (!game) {
    return TR_SKIP;
  }
  int rc = 0;
  game_demo_after_new_campaign(game);
  CHECK(game->col1.player[1].control == 0);
  game->demo = true;
  game_demo_after_new_campaign(game);
  for (int i = 0; i < 4; ++i) {
    CHECK(game->col1.player[i].control == 1);
  }
  game_demo_after_new_campaign(NULL);
out:
  demo_cleanup(game);
  return rc;
}

/* Each case in a forked child, as regression_replay does: module statics (hooks, the
 * units layer's save pointer) would otherwise outlive the destroyed game. exit(), not
 * _exit(), so a --coverage build flushes its counters. */
static int run_forked(int (*fn)(void)) {
#ifdef _WIN32
  return fn();
#else
  fflush(stdout);
  fflush(stderr);
  const pid_t pid = fork();
  if (pid == 0) {
    exit(fn());
  }
  int status = 0;
  if (pid < 0 || waitpid(pid, &status, 0) < 0 || !WIFEXITED(status)) {
    fprintf(stderr, "case crashed or fork failed (status %d)\n", status);
    return 1;
  }
  return WEXITSTATUS(status);
#endif
}

static int demo_filter_passthrough_forked(void) {
  return run_forked(demo_filter_passthrough);
}
static int demo_autoplay_click_stops_forked(void) {
  return run_forked(demo_autoplay_click_stops);
}
static int demo_autoplay_answers_screen_forked(void) {
  return run_forked(demo_autoplay_answers_screen);
}
static int demo_new_world_cycle_end_forked(void) {
  return run_forked(demo_new_world_cycle_end);
}
static int demo_resume_slot5_key_quits_forked(void) {
  return run_forked(demo_resume_slot5_key_quits);
}
static int demo_sail_key_exits_forked(void) {
  return run_forked(demo_sail_key_exits);
}
static int demo_year_tail_views_forked(void) {
  return run_forked(demo_year_tail_views);
}
static int demo_watch_hook_forked(void) {
  return run_forked(demo_watch_hook);
}
static int demo_after_new_campaign_forked(void) {
  return run_forked(demo_after_new_campaign);
}

static const TestCase k_cases[] = {
  {"demo_filter_passthrough", demo_filter_passthrough_forked},
  {"demo_autoplay_click_stops", demo_autoplay_click_stops_forked},
  {"demo_autoplay_answers_screen", demo_autoplay_answers_screen_forked},
  {"demo_new_world_cycle_end", demo_new_world_cycle_end_forked},
  {"demo_resume_slot5_key_quits", demo_resume_slot5_key_quits_forked},
  {"demo_sail_key_exits", demo_sail_key_exits_forked},
  {"demo_year_tail_views", demo_year_tail_views_forked},
  {"demo_watch_hook", demo_watch_hook_forked},
  {"demo_after_new_campaign", demo_after_new_campaign_forked},
};

TEST_MAIN(k_cases)
