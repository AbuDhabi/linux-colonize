#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_save.h"
#include "core/settings.h"

static int g_failures = 0;

static void check(bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    g_failures++;
  }
}

static const char* k_path = "build/test_settings.json";

/* A missing file must load as the DOS 0xc680 defaults, not as an error. */
static void test_missing_file_defaults(void) {
  remove(k_path);
  ColonizeSettings s;
  char err[256] = {0};
  check(settings_load_file(k_path, &s, err, sizeof(err)), "missing file loads");
  check(s.show_indian_moves && s.show_foreign_moves, "default moves shown");
  check(s.autosave && s.combat_analysis && s.tutorial_hints, "default helpers on");
  check(!s.fast_piece_slide && !s.end_of_turn, "default slide/eot off");
  check(s.water_color_cycling, "default water cycling on");
  check(s.background_music && s.event_music && s.sound_effects, "default sound on");
  check(s.window_scale == 2 && s.windowed, "default display");
  check(!s.no_sound && s.seed == 0 && !s.seed_present, "default launch flags off");
  check(!s.skip_intro, "default skip_intro false (play intro first run)");
  check(strcmp(s.data_dir, "./COLONIZE") == 0, "default data_dir");
  check(s.save_dir[0] == '\0', "default save_dir empty (platform default)");
  check(!s.debug_menu && !s.show_mouse_coords && !s.show_building_rects, "default debug overlay off");
}

static void test_roundtrip(void) {
  ColonizeSettings out;
  settings_defaults(&out);
  out.fast_piece_slide = true;
  out.tutorial_hints = false;
  out.water_color_cycling = false;
  out.report_rebel_majorities = false;
  out.labels_on_buildings = false;
  out.event_music = false;
  out.window_scale = 3;
  out.windowed = false;
  out.no_sound = true;
  out.seed = 100;
  out.seed_present = true;
  out.debug_menu = false;
  out.show_mouse_coords = false;
  out.show_building_rects = true;
  out.skip_intro = true;
  snprintf(out.data_dir, sizeof(out.data_dir), "/tmp/col-data");
  snprintf(out.save_dir, sizeof(out.save_dir), "/tmp/col-saves");

  char err[256] = {0};
  check(settings_save_file(k_path, &out, err, sizeof(err)), "save file");

  ColonizeSettings back;
  check(settings_load_file(k_path, &back, err, sizeof(err)), "load file");
  check(memcmp(&out, &back, sizeof(out)) == 0, "round-trip is exact");
}

/* Unknown keys and a partial object keep defaults for whatever is absent. */
static void test_partial_file(void) {
  FILE* f = fopen(k_path, "wb");
  check(f != NULL, "open partial file");
  if (!f) {
    return;
  }
  fprintf(f, "{\"version\": 1, \"future_key\": 7,\n");
  fprintf(f, " \"game_options\": {\"end_of_turn\": true}}\n");
  fclose(f);

  ColonizeSettings s;
  char err[256] = {0};
  check(settings_load_file(k_path, &s, err, sizeof(err)), "partial file loads");
  check(s.end_of_turn, "partial value applied");
  check(s.autosave, "absent key kept default");
  check(s.background_music, "absent section kept default");
  check(strcmp(s.data_dir, "./COLONIZE") == 0, "absent data_dir kept default");
  check(s.save_dir[0] == '\0', "absent save_dir kept empty");
  check(!s.no_sound && s.seed == 0 && !s.seed_present, "absent launch flags kept default");
  check(!s.debug_menu && !s.show_mouse_coords && !s.show_building_rects, "absent debug keys kept default");
}

/* Empty / wrong-type launch keys are not valid values, so defaults stand. */
static void test_invalid_launch_keys_keep_defaults(void) {
  FILE* f = fopen(k_path, "wb");
  check(f != NULL, "open invalid-launch file");
  if (!f) {
    return;
  }
  fprintf(f, "{\"version\": 1,\n");
  fprintf(f, " \"data_dir\": \"\",\n");
  fprintf(f, " \"save_dir\": \"\",\n");
  fprintf(f, " \"no_sound\": \"yes\",\n");
  fprintf(f, " \"seed\": -1}\n");
  fclose(f);

  ColonizeSettings s;
  char err[256] = {0};
  check(settings_load_file(k_path, &s, err, sizeof(err)), "invalid-launch file loads");
  check(strcmp(s.data_dir, "./COLONIZE") == 0, "empty data_dir ignored");
  check(s.save_dir[0] == '\0', "empty save_dir ignored");
  check(!s.no_sound, "non-bool no_sound ignored");
  check(s.seed == 0 && !s.seed_present, "negative seed ignored");
}

/* "seed": 0 is a valid override. null and omitted both mean unset. */
static void test_seed_null_omitted_and_zero(void) {
  FILE* f = fopen(k_path, "wb");
  check(f != NULL, "open seed-zero file");
  if (!f) {
    return;
  }
  fprintf(f, "{\"version\": 1, \"seed\": 0}\n");
  fclose(f);

  ColonizeSettings s;
  char err[256] = {0};
  check(settings_load_file(k_path, &s, err, sizeof(err)), "seed 0 loads");
  check(s.seed_present && s.seed == 0, "seed 0 counts as present");

  check(settings_save_file(k_path, &s, err, sizeof(err)), "seed 0 rewrites");
  f = fopen(k_path, "rb");
  check(f != NULL, "open rewritten seed-zero file");
  if (!f) {
    return;
  }
  char buf[4096] = {0};
  size_t n = fread(buf, 1, sizeof(buf) - 1, f);
  fclose(f);
  buf[n] = '\0';
  check(strstr(buf, "\"seed\": 0") != NULL, "present seed 0 is written back");
  check(strstr(buf, "\"seed\": null") == NULL, "present seed is not null");

  f = fopen(k_path, "wb");
  check(f != NULL, "open seed-null file");
  if (!f) {
    return;
  }
  fprintf(f, "{\"version\": 1, \"seed\": null}\n");
  fclose(f);
  check(settings_load_file(k_path, &s, err, sizeof(err)), "seed null loads");
  check(!s.seed_present && s.seed == 0, "null seed is unset");

  settings_defaults(&s);
  check(settings_save_file(k_path, &s, err, sizeof(err)), "defaults rewrite");
  f = fopen(k_path, "rb");
  check(f != NULL, "open defaults rewrite");
  if (!f) {
    return;
  }
  n = fread(buf, 1, sizeof(buf) - 1, f);
  fclose(f);
  buf[n] = '\0';
  check(strstr(buf, "\"seed\": null") != NULL, "unset seed written as null");
}

static void test_bad_file_is_an_error(void) {
  FILE* f = fopen(k_path, "wb");
  if (f) {
    fprintf(f, "{ this is not json");
    fclose(f);
  }
  ColonizeSettings s;
  char err[256] = {0};
  check(!settings_load_file(k_path, &s, err, sizeof(err)), "corrupt file rejected");
  check(err[0] != '\0', "corrupt file explains itself");
  /* Even on failure the caller gets usable defaults. */
  check(s.autosave, "defaults survive a parse error");
}

/* The head bridge must leave game state (WoI, REF, cheats) alone. */
static void test_head_bridge_preserves_state(void) {
  ColonizeCol1Head head;
  memset(&head, 0, sizeof(head));
  head.game_options.woi = 1;
  head.game_options.ref_present = 1;
  head.game_options.independence_force = 1;
  head.game_options.cheats_enabled = 1;

  ColonizeSettings s;
  settings_defaults(&s);
  s.water_color_cycling = false;
  s.end_of_turn = true;
  s.report_food_shortages = false;
  settings_apply_to_head(&s, &head);

  check(head.game_options.woi == 1, "woi preserved");
  check(head.game_options.ref_present == 1, "ref_present preserved");
  check(head.game_options.independence_force == 1, "independence_force preserved");
  check(head.game_options.cheats_enabled == 1, "cheats_enabled preserved");
  check(head.game_options.end_of_turn == 1, "end_of_turn applied");
  /* DOS stores the water checkbox inverted: off means the bit is set. */
  check(head.game_options.water_color_cycling == 1, "water bit inverted on write");
  /* Report bits are DOS suppress flags: "show it" stores a 0. */
  check(head.colony_report_options.report_food_shortages == 1, "report off sets the bit");
  check(head.colony_report_options.report_rebel_majorities == 0, "report on clears the bit");
  check(head.tut2.background_music == 1, "sound bit applied");

  ColonizeSettings back;
  settings_defaults(&back);
  settings_capture_from_head(&back, &head);
  check(!back.water_color_cycling, "water bit inverted on read");
  check(back.end_of_turn, "end_of_turn captured");
  check(!back.report_food_shortages, "report bit captured");
  check(back.report_rebel_majorities, "cleared report bit captured as on");
}

/*
 * settings_init materializes a defaults file on first run, and reports that a
 * preference file is now in play. Harnesses that never call it must keep
 * settings_is_loaded() false so the DOS new-game words stand.
 */
static void test_init_creates_file(void) {
  check(!settings_is_loaded(), "no preference file before settings_init");

  remove(k_path);
  char err[256] = {0};
  check(settings_init(k_path, err, sizeof(err)), "init with no file");
  check(settings_is_loaded(), "init marks settings loaded");
  check(strcmp(settings_path(), k_path) == 0, "init keeps the given path");

  FILE* f = fopen(k_path, "rb");
  check(f != NULL, "init created the file");
  char created[4096] = {0};
  if (f) {
    const size_t n = fread(created, 1, sizeof(created) - 1, f);
    created[n] = '\0';
    fclose(f);
  }
  check(strstr(created, "\"seed\": null") != NULL, "first-run file has seed null");

  ColonizeSettings written;
  ColonizeSettings expected;
  settings_defaults(&expected);
  check(settings_load_file(k_path, &written, err, sizeof(err)), "created file reloads");
  check(memcmp(&written, &expected, sizeof(expected)) == 0, "created file holds defaults");
}

/* A corrupt file must not be overwritten — the player's edits survive for
 * them to fix, and the session runs on defaults. */
static void test_init_keeps_corrupt_file(void) {
  FILE* f = fopen(k_path, "wb");
  check(f != NULL, "open corrupt file");
  if (!f) {
    return;
  }
  fprintf(f, "{ nope");
  fclose(f);

  char err[256] = {0};
  check(!settings_init(k_path, err, sizeof(err)), "init reports the parse error");
  check(settings_get()->autosave, "init falls back to defaults");

  char kept[64] = {0};
  f = fopen(k_path, "rb");
  check(f != NULL, "corrupt file still there");
  if (f) {
    (void)fgets(kept, sizeof(kept), f);
    fclose(f);
  }
  check(strncmp(kept, "{ nope", 6) == 0, "corrupt file left untouched");
}

int main(void) {
  test_missing_file_defaults();
  test_roundtrip();
  test_partial_file();
  test_invalid_launch_keys_keep_defaults();
  test_seed_null_omitted_and_zero();
  test_bad_file_is_an_error();
  test_head_bridge_preserves_state();
  /* Keep the settings_init cases last: they latch the process-wide state. */
  test_init_creates_file();
  test_init_keeps_corrupt_file();
  remove(k_path);

  if (g_failures != 0) {
    fprintf(stderr, "test_settings: %d failure(s)\n", g_failures);
    return 1;
  }
  printf("test_settings: ok\n");
  return 0;
}
