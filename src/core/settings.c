#include "core/settings.h"

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/json_min.h"
#include "core/strutil.h"
#include "platform/diagnostics.h"

#define SETTINGS_FILE_NAME "settings.json"

static ColonizeSettings g_settings;
static char g_settings_path[640];
static bool g_settings_ready = false;
static bool g_settings_loaded = false;
static bool g_settings_first_run = false;

static void set_err(char* err, size_t err_size, const char* fmt, ...) {
  if (!err || err_size == 0) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(err, err_size, fmt, ap);
  va_end(ap);
}

/*
 * One descriptor per JSON boolean option (audit 2026-09-14 IN-42). The same
 * 21 fields used to be spelled out three times — defaults, writer, reader —
 * and drifted silently when one copy was edited. `def` is the DOS new-game
 * value cited in settings_defaults' comments; the writer emits each group in
 * table order, which is the file layout. The ColonizeCol1Head bridge halves
 * (settings_apply_to_head / settings_capture_from_head) stay written out by
 * hand: those are bitfield members, which have no offsetof.
 */
typedef struct SettingsBoolOpt {
  const char* key;
  size_t offset;
  bool def;
} SettingsBoolOpt;

#define SETTINGS_BOOL(field, def_value) \
  {#field, offsetof(ColonizeSettings, field), (def_value)}

/* DS:0x5382 = 0xc600 at new game, FUN_75c2_235c (viceroy_unpacked_2.c:112401):
 * Indian moves, foreign moves, autosave and combat analysis on; fast slide and
 * end-of-turn off; the water bit is an inverted disable flag, so clear =
 * cycling on. Tutorial hints (0x5382 bit 7) are NOT in that word — DOS ORs
 * them in afterwards only at difficulty 0 (Discoverer). A preference file has
 * no difficulty to consult, so the shipped value is the Discoverer one; the
 * DOS rule still governs any game started without a settings file. */
static const SettingsBoolOpt k_game_opts[] = {
  SETTINGS_BOOL(show_indian_moves, true),
  SETTINGS_BOOL(show_foreign_moves, true),
  SETTINGS_BOOL(fast_piece_slide, false),
  SETTINGS_BOOL(end_of_turn, false),
  SETTINGS_BOOL(autosave, true),
  SETTINGS_BOOL(combat_analysis, true),
  SETTINGS_BOOL(water_color_cycling, true),
  SETTINGS_BOOL(tutorial_hints, true),
};

/* DS:0x5384/0x5385 is all-zero at new game and every bit is a suppress flag,
 * so DOS starts with all ten reports and labels showing. */
static const SettingsBoolOpt k_report_opts[] = {
  SETTINGS_BOOL(labels_on_buildings, true),
  SETTINGS_BOOL(labels_on_cargo_and_terrain, true),
  SETTINGS_BOOL(report_when_colonists_trained, true),
  SETTINGS_BOOL(report_food_shortages, true),
  SETTINGS_BOOL(report_raw_materials_shortages, true),
  SETTINGS_BOOL(report_tools_needed_for_production, true),
  SETTINGS_BOOL(report_inefficient_government, true),
  SETTINGS_BOOL(report_new_cargos_available, true),
  SETTINGS_BOOL(report_sons_of_liberty_membership, true),
  SETTINGS_BOOL(report_rebel_majorities, true),
};

/* DS:0x5386 = 0x0e at new game: all three audio bits on, howtowin clear. */
static const SettingsBoolOpt k_sound_opts[] = {
  SETTINGS_BOOL(background_music, true),
  SETTINGS_BOOL(event_music, true),
  SETTINGS_BOOL(sound_effects, true),
};

/* The "debug" object's JSON keys are shorter than the struct fields. */
static const SettingsBoolOpt k_debug_opts[] = {
  {"menu", offsetof(ColonizeSettings, debug_menu), false},
  {"mouse_coords", offsetof(ColonizeSettings, show_mouse_coords), false},
  {"building_rects", offsetof(ColonizeSettings, show_building_rects), false},
  {"logs", offsetof(ColonizeSettings, debug_logs), false},
};

#define SETTINGS_OPT_COUNT(t) ((int)(sizeof(t) / sizeof((t)[0])))

static bool* settings_bool_at(ColonizeSettings* s, const SettingsBoolOpt* opt) {
  return (bool*)((char*)s + opt->offset);
}

static bool settings_bool_of(const ColonizeSettings* s, const SettingsBoolOpt* opt) {
  return *(const bool*)((const char*)s + opt->offset);
}

int settings_clamp_window_scale(int64_t scale) {
  if (scale < COLONIZE_WINDOW_SCALE_MIN) {
    return COLONIZE_WINDOW_SCALE_MIN;
  }
  if (scale > COLONIZE_WINDOW_SCALE_MAX) {
    return COLONIZE_WINDOW_SCALE_MAX;
  }
  return (int)scale;
}

void settings_defaults(ColonizeSettings* out) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  for (int i = 0; i < SETTINGS_OPT_COUNT(k_game_opts); ++i) {
    *settings_bool_at(out, &k_game_opts[i]) = k_game_opts[i].def;
  }
  for (int i = 0; i < SETTINGS_OPT_COUNT(k_report_opts); ++i) {
    *settings_bool_at(out, &k_report_opts[i]) = k_report_opts[i].def;
  }
  for (int i = 0; i < SETTINGS_OPT_COUNT(k_sound_opts); ++i) {
    *settings_bool_at(out, &k_sound_opts[i]) = k_sound_opts[i].def;
  }
  for (int i = 0; i < SETTINGS_OPT_COUNT(k_debug_opts); ++i) {
    *settings_bool_at(out, &k_debug_opts[i]) = k_debug_opts[i].def;
  }
  out->soundfont[0] = '\0';
  out->midi_backend[0] = '\0';

  out->windowed = true;
  out->window_scale = 2;
  out->no_sound = false;
  snprintf(out->data_dir, sizeof(out->data_dir), "./COLONIZE");
  out->save_dir[0] = '\0';
  out->seed = 0;
  out->seed_present = false;
  /* Newly created settings.json writes true so later launches skip OPENING.EXE.
   * First launch still plays it because the file was absent (settings_first_run).
   * After that the key is the player's; the intro never writes it back. */
  out->skip_intro = true;
}

/* ---------------------------------------------------------------- writing */

static void wb(FILE* f, const char* key, bool v, bool last) {
  fprintf(f, "    \"%s\": %s%s\n", key, v ? "true" : "false", last ? "" : ",");
}

/* Emit a whole descriptor group; `last` says whether the final row closes the
 * object (no trailing comma) or another key follows it inside the object. */
static void wb_group(
  FILE* f, const ColonizeSettings* in, const SettingsBoolOpt* opts, int count, bool last
) {
  for (int i = 0; i < count; ++i) {
    wb(f, opts[i].key, settings_bool_of(in, &opts[i]), last && i == count - 1);
  }
}

bool settings_save_file(const char* path, const ColonizeSettings* in, char* err, size_t err_size) {
  if (!path || !path[0] || !in) {
    set_err(err, err_size, "settings: bad arguments");
    return false;
  }
  char tmp[700];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE* f = fopen(tmp, "wb");
  if (!f) {
    set_err(err, err_size, "settings: cannot write %s: %s", tmp, strerror(errno));
    return false;
  }
  fprintf(f, "{\n");
  fprintf(f, "  \"version\": %d,\n", COLONIZE_SETTINGS_VERSION);

  fprintf(f, "  \"game_options\": {\n");
  wb_group(f, in, k_game_opts, SETTINGS_OPT_COUNT(k_game_opts), true);
  fprintf(f, "  },\n");

  fprintf(f, "  \"colony_report_options\": {\n");
  wb_group(f, in, k_report_opts, SETTINGS_OPT_COUNT(k_report_opts), true);
  fprintf(f, "  },\n");

  fprintf(f, "  \"sound_options\": {\n");
  wb_group(f, in, k_sound_opts, SETTINGS_OPT_COUNT(k_sound_opts), false);
  fprintf(f, "    \"soundfont\": ");
  json_write_escaped_string(f, in->soundfont, sizeof(in->soundfont) - 1);
  fprintf(f, ",\n");
  fprintf(f, "    \"midi_backend\": ");
  json_write_escaped_string(f, in->midi_backend, sizeof(in->midi_backend) - 1);
  fprintf(f, "\n  },\n");

  fprintf(f, "  \"display\": {\n");
  wb(f, "windowed", in->windowed, false);
  fprintf(f, "    \"window_scale\": %d\n", in->window_scale);
  fprintf(f, "  },\n");

  fprintf(f, "  \"debug\": {\n");
  wb_group(f, in, k_debug_opts, SETTINGS_OPT_COUNT(k_debug_opts), true);
  fprintf(f, "  },\n");

  fprintf(f, "  \"data_dir\": ");
  json_write_escaped_string(f, in->data_dir, sizeof(in->data_dir) - 1);
  fprintf(f, ",\n");
  fprintf(f, "  \"save_dir\": ");
  json_write_escaped_string(f, in->save_dir, sizeof(in->save_dir) - 1);
  fprintf(f, ",\n");
  fprintf(f, "  \"no_sound\": %s,\n", in->no_sound ? "true" : "false");
  fprintf(f, "  \"skip_intro\": %s,\n", in->skip_intro ? "true" : "false");
  if (in->seed_present) {
    fprintf(f, "  \"seed\": %u\n", in->seed);
  } else {
    fprintf(f, "  \"seed\": null\n");
  }
  fprintf(f, "}\n");

  const bool io_ok = (ferror(f) == 0);
  if (fclose(f) != 0 || !io_ok) {
    set_err(err, err_size, "settings: write failed for %s", tmp);
    remove(tmp);
    return false;
  }
  if (rename(tmp, path) != 0) {
    set_err(err, err_size, "settings: cannot replace %s: %s", path, strerror(errno));
    remove(tmp);
    return false;
  }
  return true;
}

/* ---------------------------------------------------------------- reading */

static void rb(const JsonValue* obj, const char* key, bool* out) {
  bool v;
  if (obj && json_get_bool(obj, key, &v)) {
    *out = v;
  }
}

static void rb_group(
  const JsonValue* obj, ColonizeSettings* out, const SettingsBoolOpt* opts, int count
) {
  for (int i = 0; i < count; ++i) {
    rb(obj, opts[i].key, settings_bool_at(out, &opts[i]));
  }
}

bool settings_load_file(const char* path, ColonizeSettings* out, char* err, size_t err_size) {
  if (!path || !path[0] || !out) {
    set_err(err, err_size, "settings: bad arguments");
    return false;
  }
  settings_defaults(out);

  /* No file yet is the normal first-run case, not an error. */
  FILE* probe = fopen(path, "rb");
  if (!probe) {
    return true;
  }
  fclose(probe);

  uint8_t* raw = NULL;
  size_t raw_size = 0;
  char slurp_err[200] = {0};
  if (!file_slurp(path, &raw, &raw_size, slurp_err, sizeof(slurp_err))) {
    set_err(err, err_size, "settings: %s", slurp_err[0] ? slurp_err : "cannot read file");
    return false;
  }
  if (raw_size > (1u << 20)) {
    free(raw);
    set_err(err, err_size, "settings: %s is not a settings file", path);
    return false;
  }
  char* text = (char*)malloc(raw_size + 1);
  if (!text) {
    free(raw);
    set_err(err, err_size, "settings: out of memory");
    return false;
  }
  memcpy(text, raw, raw_size);
  text[raw_size] = '\0';
  free(raw);

  char perr[256] = {0};
  JsonValue* root = json_parse(text, perr, sizeof(perr));
  free(text);
  if (!root) {
    set_err(err, err_size, "settings: %s: %s", path, perr[0] ? perr : "parse error");
    return false;
  }
  if (root->type != JV_OBJ) {
    json_free(root);
    set_err(err, err_size, "settings: %s: top level is not an object", path);
    return false;
  }

  /*
   * "version" is written by settings_save_file; read it back so the constant
   * is an actual gate rather than write-only decoration. Version 1 is the
   * only shape that has ever shipped, so there is nothing to migrate yet —
   * any future bump adds its migration here, keyed on `file_version`. A file
   * from the future loads best-effort (every field below is optional) with a
   * warning; a missing key means a pre-versioned or hand-written file, which
   * is the same best-effort case.
   */
  int64_t file_version = 0;
  (void)json_get_i64(root, "version", &file_version);
  if (file_version > COLONIZE_SETTINGS_VERSION) {
    diag_warn(
      "settings: %s is version %lld, newer than %d; loading best-effort",
      path,
      (long long)file_version,
      COLONIZE_SETTINGS_VERSION
    );
  }

  /* Every field is optional: a partial or older file keeps the defaults. */
  const JsonValue* g = json_obj_get(root, "game_options");
  rb_group(g, out, k_game_opts, SETTINGS_OPT_COUNT(k_game_opts));

  const JsonValue* c = json_obj_get(root, "colony_report_options");
  rb_group(c, out, k_report_opts, SETTINGS_OPT_COUNT(k_report_opts));

  const JsonValue* s = json_obj_get(root, "sound_options");
  rb_group(s, out, k_sound_opts, SETTINGS_OPT_COUNT(k_sound_opts));
  const char* soundfont = s ? json_get_str(s, "soundfont") : NULL;
  if (soundfont) {
    snprintf(out->soundfont, sizeof(out->soundfont), "%s", soundfont);
  }
  const char* midi_backend = s ? json_get_str(s, "midi_backend") : NULL;
  if (midi_backend &&
      (midi_backend[0] == '\0' || strcmp(midi_backend, "fluidsynth") == 0 ||
       strcmp(midi_backend, "tsf") == 0)) {
    snprintf(out->midi_backend, sizeof(out->midi_backend), "%s", midi_backend);
  }

  const JsonValue* d = json_obj_get(root, "display");
  rb(d, "windowed", &out->windowed);
  int64_t scale = 0;
  if (d && json_get_i64(d, "window_scale", &scale)) {
    out->window_scale = settings_clamp_window_scale(scale);
  }

  const JsonValue* dbg = json_obj_get(root, "debug");
  rb_group(dbg, out, k_debug_opts, SETTINGS_OPT_COUNT(k_debug_opts));

  const char* data_dir = json_get_str(root, "data_dir");
  if (data_dir && data_dir[0]) {
    snprintf(out->data_dir, sizeof(out->data_dir), "%s", data_dir);
  }
  const char* save_dir = json_get_str(root, "save_dir");
  if (save_dir && save_dir[0]) {
    snprintf(out->save_dir, sizeof(out->save_dir), "%s", save_dir);
  }
  rb(root, "no_sound", &out->no_sound);
  rb(root, "skip_intro", &out->skip_intro);
  int64_t seed = 0;
  if (json_get_i64(root, "seed", &seed) && seed >= 0 && seed <= (int64_t)UINT32_MAX) {
    out->seed = (uint32_t)seed;
    out->seed_present = true;
  }

  json_free(root);
  return true;
}

/* -------------------------------------------------------------- singleton */

const char* settings_path(void) {
  if (g_settings_path[0] == '\0') {
    const char* exe_dir = diag_exe_dir();
    if (exe_dir && exe_dir[0] != '\0') {
      snprintf(g_settings_path, sizeof(g_settings_path), "%s/%s", exe_dir, SETTINGS_FILE_NAME);
    } else {
      snprintf(g_settings_path, sizeof(g_settings_path), "./%s", SETTINGS_FILE_NAME);
    }
  }
  return g_settings_path;
}

bool settings_init(const char* path, char* err, size_t err_size) {
  settings_defaults(&g_settings);
  g_settings_ready = true;
  /*
   * Only the success path below sets this. `settings_is_loaded` means "a
   * preference file is in play"; on the corrupt-file path no preferences were
   * recovered (we fall back to defaults and leave the bad file alone), so the
   * DOS-faithful no-preferences behaviour is what callers want there —
   * settings_apply_to_head is skipped for a new game (game_loop.c:7639) and
   * game_try_start_intro (game_loop.c:12091) stays out.
   */
  g_settings_loaded = false;
  if (path && path[0]) {
    snprintf(g_settings_path, sizeof(g_settings_path), "%s", path);
  } else {
    g_settings_path[0] = '\0';
    (void)settings_path();
  }

  FILE* probe = fopen(g_settings_path, "rb");
  const bool existed = (probe != NULL);
  if (probe) {
    fclose(probe);
  }
  g_settings_first_run = !existed;

  char load_err[256] = {0};
  if (!settings_load_file(g_settings_path, &g_settings, load_err, sizeof(load_err))) {
    /* Keep defaults so a corrupt file never blocks startup; leave the bad
     * file in place rather than silently overwriting the player's edits. */
    settings_defaults(&g_settings);
    diag_warn("%s", load_err);
    set_err(err, err_size, "%s", load_err);
    return false;
  }
  g_settings_loaded = true;

  if (!existed) {
    /* First run: materialize the file so the options are discoverable and
     * hand-editable without having to open a dialog first. */
    char save_err[256] = {0};
    if (settings_save_file(g_settings_path, &g_settings, save_err, sizeof(save_err))) {
      diag_info("Created default settings file: %s", g_settings_path);
    } else {
      diag_warn("Could not create %s: %s", g_settings_path, save_err);
    }
  }
  diag_info("Settings file: %s", g_settings_path);
  return true;
}

bool settings_is_loaded(void) {
  return g_settings_loaded;
}

bool settings_first_run(void) {
  return g_settings_loaded && g_settings_first_run;
}

const ColonizeSettings* settings_get(void) {
  if (!g_settings_ready) {
    settings_defaults(&g_settings);
    g_settings_ready = true;
  }
  return &g_settings;
}

void settings_set(const ColonizeSettings* in) {
  if (!in) {
    return;
  }
  g_settings = *in;
  g_settings_ready = true;
}

bool settings_flush(char* err, size_t err_size) {
  return settings_save_file(settings_path(), settings_get(), err, err_size);
}

/* ----------------------------------------------------------------- bridge */

void settings_apply_to_head(const ColonizeSettings* s, ColonizeCol1Head* head) {
  if (!s || !head) {
    return;
  }
  ColonizeCol1GameOptions* g = &head->game_options;
  g->show_indian_moves = s->show_indian_moves ? 1 : 0;
  g->show_foreign_moves = s->show_foreign_moves ? 1 : 0;
  g->fast_piece_slide = s->fast_piece_slide ? 1 : 0;
  g->end_of_turn = s->end_of_turn ? 1 : 0;
  g->autosave = s->autosave ? 1 : 0;
  g->combat_analysis = s->combat_analysis ? 1 : 0;
  /* DOS keeps this as an inverted disable bit. */
  g->water_color_cycling = s->water_color_cycling ? 0 : 1;
  g->tutorial_hints = s->tutorial_hints ? 1 : 0;

  /* Settings hold the player-facing sense (true = shown); the Col1 bits are
   * DOS suppress flags. Invert here and in settings_capture_from_head. */
  ColonizeCol1ColonyReportOptions* c = &head->colony_report_options;
  c->labels_on_buildings = s->labels_on_buildings ? 0 : 1;
  c->labels_on_cargo_and_terrain = s->labels_on_cargo_and_terrain ? 0 : 1;
  c->report_when_colonists_trained = s->report_when_colonists_trained ? 0 : 1;
  c->report_food_shortages = s->report_food_shortages ? 0 : 1;
  c->report_raw_materials_shortages = s->report_raw_materials_shortages ? 0 : 1;
  c->report_tools_needed_for_production = s->report_tools_needed_for_production ? 0 : 1;
  c->report_inefficient_government = s->report_inefficient_government ? 0 : 1;
  c->report_new_cargos_available = s->report_new_cargos_available ? 0 : 1;
  c->report_sons_of_liberty_membership = s->report_sons_of_liberty_membership ? 0 : 1;
  c->report_rebel_majorities = s->report_rebel_majorities ? 0 : 1;

  head->tut2.background_music = s->background_music ? 1 : 0;
  head->tut2.event_music = s->event_music ? 1 : 0;
  head->tut2.sound_effects = s->sound_effects ? 1 : 0;
}

void settings_capture_from_head(ColonizeSettings* s, const ColonizeCol1Head* head) {
  if (!s || !head) {
    return;
  }
  const ColonizeCol1GameOptions* g = &head->game_options;
  s->show_indian_moves = g->show_indian_moves != 0;
  s->show_foreign_moves = g->show_foreign_moves != 0;
  s->fast_piece_slide = g->fast_piece_slide != 0;
  s->end_of_turn = g->end_of_turn != 0;
  s->autosave = g->autosave != 0;
  s->combat_analysis = g->combat_analysis != 0;
  s->water_color_cycling = g->water_color_cycling == 0;
  s->tutorial_hints = g->tutorial_hints != 0;

  const ColonizeCol1ColonyReportOptions* c = &head->colony_report_options;
  s->labels_on_buildings = c->labels_on_buildings == 0;
  s->labels_on_cargo_and_terrain = c->labels_on_cargo_and_terrain == 0;
  s->report_when_colonists_trained = c->report_when_colonists_trained == 0;
  s->report_food_shortages = c->report_food_shortages == 0;
  s->report_raw_materials_shortages = c->report_raw_materials_shortages == 0;
  s->report_tools_needed_for_production = c->report_tools_needed_for_production == 0;
  s->report_inefficient_government = c->report_inefficient_government == 0;
  s->report_new_cargos_available = c->report_new_cargos_available == 0;
  s->report_sons_of_liberty_membership = c->report_sons_of_liberty_membership == 0;
  s->report_rebel_majorities = c->report_rebel_majorities == 0;

  s->background_music = head->tut2.background_music != 0;
  s->event_music = head->tut2.event_music != 0;
  s->sound_effects = head->tut2.sound_effects != 0;
}

ColonizeSoundOptions settings_sound_options(const ColonizeSettings* s) {
  ColonizeSoundOptions out;
  memset(&out, 0, sizeof(out));
  if (s) {
    out.background_music = s->background_music;
    out.event_music = s->event_music;
    out.sound_effects = s->sound_effects;
  }
  return out;
}
