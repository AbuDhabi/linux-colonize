#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/game_loop.h"
#include "core/savegame.h"
#include "core/screen_geom.h"
#include "core/text_edit.h"
#include "core/settings.h"
#include "core/sound.h"
#include "core/window_log.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

typedef struct CliConfig {
  const char* data_dir;
  const char* save_dir;
  bool windowed;
  bool no_sound;
  int window_scale;
  uint32_t rng_seed;
  bool debug_menu;
  /* Which flags the command line actually set; those win over settings.json.
   * Anything else falls back to the stored preference if that key is valid,
   * else the hardcoded default already in this struct. */
  bool data_dir_from_cli;
  bool save_dir_from_cli;
  bool windowed_from_cli;
  bool nosound_from_cli;
  bool scale_from_cli;
  bool seed_from_cli;
  bool debug_menu_from_cli;
} CliConfig;

static CliConfig cli_defaults(void) {
  CliConfig cfg;
  cfg.data_dir = "./COLONIZE";
  cfg.save_dir = savegame_default_dir();
  cfg.windowed = true;
  cfg.no_sound = false;
  cfg.window_scale = 2;
  cfg.rng_seed = 0;
  cfg.debug_menu = false;
  cfg.data_dir_from_cli = false;
  cfg.save_dir_from_cli = false;
  cfg.windowed_from_cli = false;
  cfg.nosound_from_cli = false;
  cfg.scale_from_cli = false;
  cfg.seed_from_cli = false;
  cfg.debug_menu_from_cli = false;
  return cfg;
}

static bool parse_args(int argc, char** argv, CliConfig* cfg) {
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (strcmp(arg, "--data-dir") == 0 && i + 1 < argc) {
      cfg->data_dir = argv[++i];
      cfg->data_dir_from_cli = true;
    } else if (strcmp(arg, "--save-dir") == 0 && i + 1 < argc) {
      cfg->save_dir = argv[++i];
      cfg->save_dir_from_cli = true;
    } else if (strcmp(arg, "--windowed") == 0) {
      cfg->windowed = true;
      cfg->windowed_from_cli = true;
    } else if (strcmp(arg, "--fullscreen") == 0) {
      cfg->windowed = false;
      cfg->windowed_from_cli = true;
    } else if (strcmp(arg, "--nosound") == 0) {
      cfg->no_sound = true;
      cfg->nosound_from_cli = true;
    } else if (strcmp(arg, "--scale") == 0 && i + 1 < argc) {
      /* Same 1..8 range the settings.json path clamps to (settings.c). */
      cfg->window_scale = settings_clamp_window_scale(atoi(argv[++i]));
      cfg->scale_from_cli = true;
    } else if (strcmp(arg, "--seed") == 0 && i + 1 < argc) {
      cfg->rng_seed = (uint32_t)strtoul(argv[++i], NULL, 0);
      cfg->seed_from_cli = true;
    } else if (strcmp(arg, "--debug-menu") == 0) {
      cfg->debug_menu = true;
      cfg->debug_menu_from_cli = true;
    } else if (strcmp(arg, "--no-debug-menu") == 0) {
      cfg->debug_menu = false;
      cfg->debug_menu_from_cli = true;
    } else {
      fprintf(stderr, "Unknown argument: %s\n", arg);
      return false;
    }
  }
  return true;
}

int main(int argc, char** argv) {
  if (!diag_init(argc, argv)) {
    fprintf(stderr, "Warning: diagnostics log unavailable; continuing without file logging.\n");
  }

  CliConfig cli = cli_defaults();
  if (!parse_args(argc, argv, &cli)) {
    diag_shutdown();
    return 2;
  }

  /* settings.json sits next to the executable; a missing file is first run. */
  char settings_err[256];
  if (!settings_init(NULL, settings_err, sizeof(settings_err))) {
    fprintf(stderr, "Warning: %s; using default options.\n", settings_err);
  }
  {
    const ColonizeSettings* prefs = settings_get();
    if (!cli.data_dir_from_cli && prefs->data_dir[0]) {
      cli.data_dir = prefs->data_dir;
    }
    if (!cli.save_dir_from_cli && prefs->save_dir[0]) {
      cli.save_dir = prefs->save_dir;
    }
    if (!cli.windowed_from_cli) {
      cli.windowed = prefs->windowed;
    }
    if (!cli.nosound_from_cli) {
      cli.no_sound = prefs->no_sound;
    }
    if (!cli.scale_from_cli) {
      cli.window_scale = prefs->window_scale;
    }
    if (!cli.seed_from_cli && prefs->seed_present) {
      cli.rng_seed = prefs->seed;
      cli.seed_from_cli = true;
    }
    if (!cli.debug_menu_from_cli) {
      cli.debug_menu = prefs->debug_menu;
    }
    diag_set_info_enabled(prefs->debug_logs);
  }

  diag_info("CLI data_dir=%s", cli.data_dir);
  diag_info("CLI save_dir=%s", cli.save_dir);
  diag_info("CLI windowed=%s scale=%d nosound=%s seed=%u",
    cli.windowed ? "yes" : "no",
    cli.window_scale,
    cli.no_sound ? "yes" : "no",
    cli.rng_seed);
  diag_info(
    "NOTE: UI uses GAME.TXT @BEGINMENU + VICEROY.PAL; "
    "MADSPACK .PIK/.SS art and the MAPEDIT-faithful map compositor are live."
  );

  /* Port-only message log strip under the screen; windowed mode only, and the
   * framebuffer grows with it so game_render can paint it (core/window_log.h). */
  const int log_strip_h =
    cli.windowed ? window_log_strip_height(settings_get()->window_log_lines) : 0;

  ColonizePlatformConfig platform_cfg = {
    .data_dir = cli.data_dir,
    .windowed = cli.windowed,
    .no_sound = cli.no_sound,
    .window_scale = cli.window_scale,
    .window_width = settings_get()->window_width,
    .window_height = settings_get()->window_height,
    .extra_height = log_strip_h
  };

  ColonizePlatform* platform = platform_create(&platform_cfg);
  if (!platform) {
    diag_error("Failed to initialize SDL2 platform runtime.");
    fprintf(stderr, "Failed to initialize SDL2 platform runtime.\n");
    fprintf(stderr, "See diagnostics log: %s\n", diag_log_path());
    diag_shutdown();
    return 1;
  }

  {
    int fb_w0 = SCREEN_BASE_W;
    int fb_h0 = SCREEN_BASE_H + log_strip_h;
    platform_framebuffer_size(platform, &fb_w0, &fb_h0);
    screen_geom_set(fb_w0, fb_h0 - platform_log_strip_height(platform));
  }

  /* Text fields (leader / colony names) cut and paste via the system clipboard. */
  const TextEditClipboard clipboard = {platform_clipboard_get, platform_clipboard_set};
  text_edit_set_clipboard(&clipboard);

  ColonizeGameConfig game_cfg = {
    .data_dir = cli.data_dir,
    .save_dir = cli.save_dir,
    .rng_seed = cli.rng_seed,
    .rng_seed_set = cli.seed_from_cli,
    .debug_menu = cli.debug_menu,
    .debug_menu_set = true,
    .show_mouse_coords = settings_get()->show_mouse_coords,
    .show_mouse_coords_set = true,
    .show_building_rects = settings_get()->show_building_rects,
    .show_building_rects_set = true,
    .debug_logs = settings_get()->debug_logs,
    .debug_logs_set = true
  };

  ColonizeGameState* game = game_create(&game_cfg);
  if (!game) {
    platform_destroy(platform);
    diag_error("Failed to initialize game state.");
    fprintf(stderr, "Failed to initialize game state.\n");
    fprintf(stderr, "See diagnostics log: %s\n", diag_log_path());
    diag_shutdown();
    return 1;
  }

  if (!game_assets_ok(game)) {
    /* No built-in copy of the game's text exists to fall back on. */
    fprintf(stderr, "%s\n", game_assets_error(game));
    fprintf(stderr, "The original COLONIZE/ data files are required to run.\n");
    fprintf(stderr, "Point the port at them with --data-dir <path>.\n");
    diag_error("Startup aborted: %s", game_assets_error(game));
    game_destroy(game);
    platform_destroy(platform);
    diag_shutdown();
    return 1;
  }

  sound_set_soundfont(settings_get()->soundfont);
  sound_set_midi_backend(settings_get()->midi_backend);
  sound_init(cli.data_dir, platform_audio_enabled(platform));
  sound_set_options(settings_sound_options(settings_get()));
  if (!game_try_start_intro(game)) {
    /* Intro skipped: the title menu is the first screen, so arm its pool here
     * (game_finish_intro does it when the cinematic does run). */
    sound_set_bgm(SOUND_TITLE_BGM_POOL);
    if (!sound_playback_enabled()) {
      diag_info(
        "Music autoplay disabled; use GAME → Pick Music to preview songs%s.",
        platform_audio_enabled(platform) ? "" : " (audio device off)"
      );
    }
  }
  /* Resume after the opening cue is queued, so the pool pump cannot draw a
   * tune ahead of the intro's 0x34. */
  if (platform_audio_enabled(platform)) {
    platform_audio_resume(platform);
  }

  diag_info("Diagnostics log path (for bug reports): %s", diag_log_path());

  static uint8_t framebuffer_pixels[SCREEN_MAX_W * (SCREEN_MAX_H + WINDOW_LOG_MAX_STRIP_H)];
  ColonizeFramebuffer8 framebuffer = {
    .width = SCREEN_BASE_W,
    .height = SCREEN_BASE_H + log_strip_h,
    .pixels = framebuffer_pixels
  };
  ColonizePalette palette;

  /*
   * Remember the window size across launches (settings.json display.*). The
   * window manager delivers a resize event per dragged pixel, so the write is
   * debounced: the size has to hold still before settings.json is rewritten.
   */
  const uint32_t k_resize_save_delay_ms = 1000u;
  int saved_w = settings_get()->window_width;
  int saved_h = settings_get()->window_height;
  bool resize_pending = false;
  uint32_t resize_at_ms = 0;

  uint32_t prev_ticks = platform_ticks_ms();
  bool running = true;
  while (running) {
    ColonizeInputState input = {0};
    if (!platform_poll_input(platform, &input)) {
      break;
    }
    if (input.quit_requested) {
      running = false;
    }

    uint32_t now = platform_ticks_ms();
    uint32_t dt = now - prev_ticks;
    prev_ticks = now;

    /* Window may have been resized: the logical framebuffer follows it, and
     * the map view lays itself out for the game part (strip excluded). */
    int fb_w = SCREEN_BASE_W;
    int fb_h = SCREEN_BASE_H + log_strip_h;
    platform_framebuffer_size(platform, &fb_w, &fb_h);
    const int strip_h = platform_log_strip_height(platform);
    framebuffer.width = fb_w;
    framebuffer.height = fb_h;
    screen_geom_set(fb_w, fb_h - strip_h);

    if (cli.windowed && settings_is_loaded() &&
        (screen_geom_w() != saved_w || screen_geom_h() != saved_h)) {
      resize_pending = true;
      resize_at_ms = now;
    }
    if (resize_pending && now - resize_at_ms >= k_resize_save_delay_ms) {
      ColonizeSettings prefs = *settings_get();
      prefs.window_width = screen_geom_w();
      prefs.window_height = screen_geom_h();
      settings_set(&prefs);
      char save_err[256];
      if (settings_flush(save_err, sizeof(save_err))) {
        diag_info("Window size %dx%d stored in %s",
          prefs.window_width, prefs.window_height, settings_path());
      } else {
        diag_warn("Could not store window size: %s", save_err);
      }
      saved_w = prefs.window_width;
      saved_h = prefs.window_height;
      resize_pending = false;
    }

    game_set_platform(game, platform);
    if (!game_update(game, &input, dt)) {
      running = false;
    }
    game_apply_mouse_cursor(game, platform, input.mouse_x, input.mouse_y);
    /* The game draws into the top (screen) part only; the log strip below is
     * painted after, so no screen renderer sees the taller framebuffer. Same
     * pixel array and same stride — the strip is simply the bottom rows. */
    ColonizeFramebuffer8 screen_fb = {
      .width = fb_w, .height = fb_h - strip_h, .pixels = framebuffer_pixels
    };
    game_render(game, &screen_fb, &palette);
    game_render_window_log(game, &framebuffer, &palette);
    platform_set_window_title(platform, game_status_text(game));
    if (!platform_present(platform, &framebuffer, &palette)) {
      running = false;
    }

    platform_sleep_ms(16);
  }

  game_destroy(game);
  sound_shutdown();
  platform_destroy(platform);
  diag_shutdown();
  return 0;
}
