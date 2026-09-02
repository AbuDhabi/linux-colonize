#include "core/settings.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/json_min.h"
#include "platform/diagnostics.h"

#define SETTINGS_FILE_NAME "settings.json"

static ColonizeSettings g_settings;
static char g_settings_path[640];
static bool g_settings_ready = false;
static bool g_settings_loaded = false;

static void set_err(char* err, size_t err_size, const char* fmt, ...) {
  if (!err || err_size == 0) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(err, err_size, fmt, ap);
  va_end(ap);
}

void settings_defaults(ColonizeSettings* out) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  /*
   * DOS new-game state, FUN_75c2_235c (viceroy_unpacked_2.c:112401):
   *   DS:0x5382 = 0xc600 — Indian moves, foreign moves, autosave and combat
   *   analysis on; fast slide and end-of-turn off; the water bit is an
   *   inverted disable flag, so clear = cycling on.
   * Tutorial hints (0x5382 bit 7) are NOT in that word — DOS ORs them in
   * afterwards only at difficulty 0 (Discoverer). A preference file has no
   * difficulty to consult, so the shipped value is the Discoverer one; the
   * DOS rule still governs any game started without a settings file.
   */
  out->show_indian_moves = true;
  out->show_foreign_moves = true;
  out->fast_piece_slide = false;
  out->end_of_turn = false;
  out->autosave = true;
  out->combat_analysis = true;
  out->water_color_cycling = true;
  out->tutorial_hints = true;

  /* DS:0x5384/0x5385 is all-zero at new game and every bit is a suppress
   * flag, so DOS starts with all ten reports and labels showing. */
  out->labels_on_buildings = true;
  out->labels_on_cargo_and_terrain = true;
  out->report_when_colonists_trained = true;
  out->report_food_shortages = true;
  out->report_raw_materials_shortages = true;
  out->report_tools_needed_for_production = true;
  out->report_inefficient_government = true;
  out->report_new_cargos_available = true;
  out->report_sons_of_liberty_membership = true;
  out->report_rebel_majorities = true;

  /* DS:0x5386 = 0x0e at new game: all three audio bits on, howtowin clear. */
  out->background_music = true;
  out->event_music = true;
  out->sound_effects = true;

  out->windowed = true;
  out->window_scale = 2;
  out->no_sound = false;
  snprintf(out->data_dir, sizeof(out->data_dir), "./COLONIZE");
  out->save_dir[0] = '\0';
  out->seed = 0;
  out->seed_present = false;
}

/* ---------------------------------------------------------------- writing */

static void wb(FILE* f, const char* key, bool v, bool last) {
  fprintf(f, "    \"%s\": %s%s\n", key, v ? "true" : "false", last ? "" : ",");
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
  wb(f, "show_indian_moves", in->show_indian_moves, false);
  wb(f, "show_foreign_moves", in->show_foreign_moves, false);
  wb(f, "fast_piece_slide", in->fast_piece_slide, false);
  wb(f, "end_of_turn", in->end_of_turn, false);
  wb(f, "autosave", in->autosave, false);
  wb(f, "combat_analysis", in->combat_analysis, false);
  wb(f, "water_color_cycling", in->water_color_cycling, false);
  wb(f, "tutorial_hints", in->tutorial_hints, true);
  fprintf(f, "  },\n");

  fprintf(f, "  \"colony_report_options\": {\n");
  wb(f, "labels_on_buildings", in->labels_on_buildings, false);
  wb(f, "labels_on_cargo_and_terrain", in->labels_on_cargo_and_terrain, false);
  wb(f, "report_when_colonists_trained", in->report_when_colonists_trained, false);
  wb(f, "report_food_shortages", in->report_food_shortages, false);
  wb(f, "report_raw_materials_shortages", in->report_raw_materials_shortages, false);
  wb(f, "report_tools_needed_for_production", in->report_tools_needed_for_production, false);
  wb(f, "report_inefficient_government", in->report_inefficient_government, false);
  wb(f, "report_new_cargos_available", in->report_new_cargos_available, false);
  wb(f, "report_sons_of_liberty_membership", in->report_sons_of_liberty_membership, false);
  wb(f, "report_rebel_majorities", in->report_rebel_majorities, true);
  fprintf(f, "  },\n");

  fprintf(f, "  \"sound_options\": {\n");
  wb(f, "background_music", in->background_music, false);
  wb(f, "event_music", in->event_music, false);
  wb(f, "sound_effects", in->sound_effects, true);
  fprintf(f, "  },\n");

  fprintf(f, "  \"display\": {\n");
  wb(f, "windowed", in->windowed, false);
  fprintf(f, "    \"window_scale\": %d\n", in->window_scale);
  fprintf(f, "  },\n");

  fprintf(f, "  \"data_dir\": ");
  json_write_escaped_string(f, in->data_dir, sizeof(in->data_dir) - 1);
  fprintf(f, ",\n");
  fprintf(f, "  \"save_dir\": ");
  json_write_escaped_string(f, in->save_dir, sizeof(in->save_dir) - 1);
  fprintf(f, ",\n");
  fprintf(f, "  \"no_sound\": %s,\n", in->no_sound ? "true" : "false");
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

bool settings_load_file(const char* path, ColonizeSettings* out, char* err, size_t err_size) {
  if (!path || !path[0] || !out) {
    set_err(err, err_size, "settings: bad arguments");
    return false;
  }
  settings_defaults(out);

  FILE* f = fopen(path, "rb");
  if (!f) {
    /* No file yet is the normal first-run case, not an error. */
    return true;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    set_err(err, err_size, "settings: cannot size %s", path);
    return false;
  }
  const long len = ftell(f);
  if (len < 0 || len > (1 << 20)) {
    fclose(f);
    set_err(err, err_size, "settings: %s is not a settings file", path);
    return false;
  }
  rewind(f);
  char* text = (char*)malloc((size_t)len + 1);
  if (!text) {
    fclose(f);
    set_err(err, err_size, "settings: out of memory");
    return false;
  }
  const size_t got = fread(text, 1, (size_t)len, f);
  fclose(f);
  text[got] = '\0';

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

  /* Every field is optional: a partial or older file keeps the defaults. */
  const JsonValue* g = json_obj_get(root, "game_options");
  rb(g, "show_indian_moves", &out->show_indian_moves);
  rb(g, "show_foreign_moves", &out->show_foreign_moves);
  rb(g, "fast_piece_slide", &out->fast_piece_slide);
  rb(g, "end_of_turn", &out->end_of_turn);
  rb(g, "autosave", &out->autosave);
  rb(g, "combat_analysis", &out->combat_analysis);
  rb(g, "water_color_cycling", &out->water_color_cycling);
  rb(g, "tutorial_hints", &out->tutorial_hints);

  const JsonValue* c = json_obj_get(root, "colony_report_options");
  rb(c, "labels_on_buildings", &out->labels_on_buildings);
  rb(c, "labels_on_cargo_and_terrain", &out->labels_on_cargo_and_terrain);
  rb(c, "report_when_colonists_trained", &out->report_when_colonists_trained);
  rb(c, "report_food_shortages", &out->report_food_shortages);
  rb(c, "report_raw_materials_shortages", &out->report_raw_materials_shortages);
  rb(c, "report_tools_needed_for_production", &out->report_tools_needed_for_production);
  rb(c, "report_inefficient_government", &out->report_inefficient_government);
  rb(c, "report_new_cargos_available", &out->report_new_cargos_available);
  rb(c, "report_sons_of_liberty_membership", &out->report_sons_of_liberty_membership);
  rb(c, "report_rebel_majorities", &out->report_rebel_majorities);

  const JsonValue* s = json_obj_get(root, "sound_options");
  rb(s, "background_music", &out->background_music);
  rb(s, "event_music", &out->event_music);
  rb(s, "sound_effects", &out->sound_effects);

  const JsonValue* d = json_obj_get(root, "display");
  rb(d, "windowed", &out->windowed);
  int64_t scale = 0;
  if (d && json_get_i64(d, "window_scale", &scale)) {
    out->window_scale = (int)(scale < 1 ? 1 : (scale > 8 ? 8 : scale));
  }

  const char* data_dir = json_get_str(root, "data_dir");
  if (data_dir && data_dir[0]) {
    snprintf(out->data_dir, sizeof(out->data_dir), "%s", data_dir);
  }
  const char* save_dir = json_get_str(root, "save_dir");
  if (save_dir && save_dir[0]) {
    snprintf(out->save_dir, sizeof(out->save_dir), "%s", save_dir);
  }
  rb(root, "no_sound", &out->no_sound);
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
  g_settings_loaded = true;
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

  char load_err[256] = {0};
  if (!settings_load_file(g_settings_path, &g_settings, load_err, sizeof(load_err))) {
    /* Keep defaults so a corrupt file never blocks startup; leave the bad
     * file in place rather than silently overwriting the player's edits. */
    settings_defaults(&g_settings);
    diag_warn("%s", load_err);
    set_err(err, err_size, "%s", load_err);
    return false;
  }

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
