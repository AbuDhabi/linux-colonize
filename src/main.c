#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/game_loop.h"
#include "core/savegame.h"
#include "core/text_edit.h"
#include "core/settings.h"
#include "core/sound.h"
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
      cfg->window_scale = atoi(argv[++i]);
      if (cfg->window_scale < 1) {
        cfg->window_scale = 1;
      }
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

  ColonizePlatformConfig platform_cfg = {
    .data_dir = cli.data_dir,
    .windowed = cli.windowed,
    .no_sound = cli.no_sound,
    .window_scale = cli.window_scale
  };

  ColonizePlatform* platform = platform_create(&platform_cfg);
  if (!platform) {
    diag_error("Failed to initialize SDL2 platform runtime.");
    fprintf(stderr, "Failed to initialize SDL2 platform runtime.\n");
    fprintf(stderr, "See diagnostics log: %s\n", diag_log_path());
    diag_shutdown();
    return 1;
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

  sound_init(cli.data_dir, platform_audio_enabled(platform));
  sound_set_options(settings_sound_options(settings_get()));
  if (game_try_start_intro(game)) {
    /* Intro plays OPENING_BGM_ID (0x34); title music starts when it finishes. */
  } else if (sound_playback_enabled()) {
    sound_play(SOUND_TITLE_ID);
  } else {
    diag_info(
      "Music autoplay disabled; use GAME → Pick Music to preview songs%s.",
      platform_audio_enabled(platform) ? "" : " (audio device off)"
    );
  }
  /* Resume after the launch cue is queued so the first audible song is 0x34
   * (or title 0x33), not a random pool pick from the idle pump. */
  if (platform_audio_enabled(platform)) {
    platform_audio_resume(platform);
  }

  diag_info("Diagnostics log path (for bug reports): %s", diag_log_path());

  uint8_t framebuffer_pixels[320 * 200];
  ColonizeFramebuffer8 framebuffer = {
    .width = 320,
    .height = 200,
    .pixels = framebuffer_pixels
  };
  ColonizePalette palette;

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

    game_set_platform(game, platform);
    if (!game_update(game, &input, dt)) {
      running = false;
    }
    game_apply_mouse_cursor(game, platform, input.mouse_x, input.mouse_y);
    game_render(game, &framebuffer, &palette);
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
