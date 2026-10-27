/*
 * opencol-mapedit: MAPEDIT.EXE, the Colonization map editor (docs/mapedit.md).
 *
 * FUN_1000_28d8 argv (mapedit.c 2332-2416): a "-" or "/" argument is a cluster of
 * upper-cased switches, C = force a new map (DS:0x54), M:file = edit that file
 * (DS:0x4a18); "?" prints the usage. Port extras: --data-dir DIR, --scale N.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/mapedit.h"
#include "core/screen_geom.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/* FUN_1000_27f6: the banner and usage lines, read from MAPEDIT.EXE (DS 0x3aa-0x458). */
static void usage(const char* data_dir) {
  size_t n = 0;
  uint8_t* exe = mapedit_exe_load(data_dir, &n);
  static const unsigned k_lines[] = {MAPEDIT_DS_TITLE, MAPEDIT_DS_COPYRIGHT, 0, MAPEDIT_DS_OPTIONS,
                                     MAPEDIT_DS_USAGE1, MAPEDIT_DS_USAGE2, MAPEDIT_DS_USAGE3};
  for (size_t i = 0; i < sizeof k_lines / sizeof k_lines[0]; ++i) {
    printf("%s\n", k_lines[i] ? mapedit_exe_str(exe, n, k_lines[i]) : "");
  }
  free(exe);
}

/* "Exit value: %d" (DS 0x487), printed when DS:0x62 is set. */
static void print_exit_value(const MapEditor* ed, const char* data_dir, int code) {
  size_t n = ed->exe_size;
  uint8_t* exe = ed->exe ? NULL : mapedit_exe_load(data_dir, &n);
  char line[64];
  mapedit_fmt_ints(line, sizeof line, mapedit_exe_str(ed->exe ? ed->exe : exe, n, MAPEDIT_DS_EXIT_VALUE),
                   code, 0);
  printf("%s\n", line);
  free(exe);
}

int main(int argc, char** argv) {
  (void)diag_init(argc, argv);
  MapEditOptions opt = {.data_dir = "./COLONIZE", .force_new = false, .map_file = NULL};
  int scale = 2;
  static char file_buf[64];
  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    if (strcmp(a, "--data-dir") == 0 && i + 1 < argc) {
      opt.data_dir = argv[++i];
    } else if (strcmp(a, "--scale") == 0 && i + 1 < argc) {
      scale = atoi(argv[++i]);
      if (scale < 1) scale = 1;
      if (scale > 8) scale = 8;
    } else if (a[0] == '?') {
      usage(opt.data_dir);
      return 0;
    } else if (a[0] == '-' || a[0] == '/') {
      for (const char* p = a + 1; *p; ++p) {
        const int c = toupper((unsigned char)*p);
        if (c == 'C') {
          opt.force_new = true;
        } else if (c == 'M' && p[1] == ':') {
          snprintf(file_buf, sizeof file_buf, "%s", p + 2);
          opt.map_file = file_buf;
          break;
        }
      }
    } else {
      fprintf(stderr, "Unknown argument: %s\n", a);
      return 2;
    }
  }

  static MapEditor ed;
  if (!mapedit_init(&ed, &opt)) {
    fprintf(stderr, "Map editor could not start (data in %s?)\n", opt.data_dir);
    print_exit_value(&ed, opt.data_dir, ed.exit_code);
    mapedit_free(&ed);
    diag_shutdown();
    return 1;
  }

  ColonizePlatformConfig cfg = {
    .data_dir = opt.data_dir,
    .windowed = true,
    .no_sound = true,
    .window_scale = scale,
    .window_width = SCREEN_BASE_W,
    .window_height = SCREEN_BASE_H,
    .extra_height = 0,
    .input_log_dir = NULL
  };
  ColonizePlatform* platform = platform_create(&cfg);
  if (!platform) {
    fprintf(stderr, "Failed to initialize SDL2.\n");
    mapedit_free(&ed);
    diag_shutdown();
    return 1;
  }
  platform_set_window_title(platform, mapedit_exe_str(ed.exe, ed.exe_size, MAPEDIT_DS_TITLE));

  static uint8_t pixels[SCREEN_MAX_W * SCREEN_MAX_H];
  uint32_t prev = platform_ticks_ms();
  for (;;) {
    ColonizeInputState input = {0};
    if (!platform_poll_input(platform, &input)) {
      break;
    }
    const uint32_t now = platform_ticks_ms();
    const uint32_t dt = now - prev;
    prev = now;
    if (!mapedit_update(&ed, platform, &input, dt)) {
      break;
    }
    int w = SCREEN_BASE_W;
    int h = SCREEN_BASE_H;
    platform_framebuffer_size(platform, &w, &h);
    if (w > SCREEN_MAX_W) w = SCREEN_MAX_W;
    if (h > SCREEN_MAX_H) h = SCREEN_MAX_H;
    ColonizeFramebuffer8 fb = {.width = w, .height = h, .pixels = pixels};
    mapedit_render(&ed, &fb);
    if (!platform_present(platform, &fb, &ed.palette)) {
      break;
    }
    platform_sleep_ms(16);
  }
  const int code = ed.exit_code;
  if (code != 0) {
    print_exit_value(&ed, opt.data_dir, code);
  }
  mapedit_free(&ed);
  platform_destroy(platform);
  diag_shutdown();
  return code;
}
