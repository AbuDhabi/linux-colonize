#include "col1_json.h"

#include <string.h>

/* strnlen is POSIX, not ISO C11 (project builds with -std=c11, extensions
 * off) — a tiny local stand-in avoids fiddling with feature-test macros. */
static size_t col1_json_strnlen(const char* s, size_t max_len) {
  size_t i = 0;
  while (i < max_len && s[i] != '\0') {
    ++i;
  }
  return i;
}

/* ============================== writer side ============================== */

static void jcomma(FILE* f, int* n) {
  if (*n) {
    fputc(',', f);
  }
  *n = 1;
}
static void jkey(FILE* f, int* n, const char* k) {
  jcomma(f, n);
  fprintf(f, "\"%s\":", k);
}

static void wi(FILE* f, int* n, const char* k, long long v) {
  jkey(f, n, k);
  fprintf(f, "%lld", v);
}
static void wu(FILE* f, int* n, const char* k, unsigned long long v) {
  jkey(f, n, k);
  fprintf(f, "%llu", v);
}
static void wb(FILE* f, int* n, const char* k, bool v) {
  jkey(f, n, k);
  fputs(v ? "true" : "false", f);
}
static void ws(FILE* f, int* n, const char* k, const char* buf, size_t fixed_len) {
  jkey(f, n, k);
  json_write_escaped_string(f, buf, col1_json_strnlen(buf, fixed_len));
}
static void wh(FILE* f, int* n, const char* k, const uint8_t* d, size_t len) {
  jkey(f, n, k);
  json_write_hex(f, d, len);
}

/* elem_size 1/2/4, is_signed selects int8/16/32 vs uint8/16/32 read. */
static void wi_arr(
  FILE* f,
  int* n,
  const char* k,
  const void* data,
  size_t count,
  size_t elem_size,
  bool is_signed
) {
  jkey(f, n, k);
  fputc('[', f);
  for (size_t i = 0; i < count; ++i) {
    if (i) {
      fputc(',', f);
    }
    if (is_signed) {
      long long v = elem_size == 1   ? ((const int8_t*)data)[i]
                     : elem_size == 2 ? ((const int16_t*)data)[i]
                                       : ((const int32_t*)data)[i];
      fprintf(f, "%lld", v);
    } else {
      unsigned long long v = elem_size == 1   ? ((const uint8_t*)data)[i]
                              : elem_size == 2 ? ((const uint16_t*)data)[i]
                                                 : ((const uint32_t*)data)[i];
      fprintf(f, "%llu", v);
    }
  }
  fputc(']', f);
}
#define W_I8ARR(f, n, k, p, c) wi_arr((f), (n), (k), (p), (c), 1, true)
#define W_U8ARR(f, n, k, p, c) wi_arr((f), (n), (k), (p), (c), 1, false)
#define W_I16ARR(f, n, k, p, c) wi_arr((f), (n), (k), (p), (c), 2, true)
#define W_U16ARR(f, n, k, p, c) wi_arr((f), (n), (k), (p), (c), 2, false)
#define W_I32ARR(f, n, k, p, c) wi_arr((f), (n), (k), (p), (c), 4, true)
#define W_U32ARR(f, n, k, p, c) wi_arr((f), (n), (k), (p), (c), 4, false)

static void write_tut1(FILE* f, const ColonizeCol1Tut1* t) {
  int n = 0;
  fputc('{', f);
  wb(f, &n, "nr13", t->nr13);
  wb(f, &n, "nr14", t->nr14);
  wb(f, &n, "unused06", t->unused06);
  wb(f, &n, "nr15", t->nr15);
  wb(f, &n, "nr16", t->nr16);
  wb(f, &n, "nr17", t->nr17);
  wb(f, &n, "unused08", t->unused08);
  wb(f, &n, "nr19", t->nr19);
  fputc('}', f);
}
static void read_tut1(const JsonValue* o, ColonizeCol1Tut1* t) {
  bool v;
  if (json_get_bool(o, "nr13", &v)) t->nr13 = v;
  if (json_get_bool(o, "nr14", &v)) t->nr14 = v;
  if (json_get_bool(o, "unused06", &v)) t->unused06 = v;
  if (json_get_bool(o, "nr15", &v)) t->nr15 = v;
  if (json_get_bool(o, "nr16", &v)) t->nr16 = v;
  if (json_get_bool(o, "nr17", &v)) t->nr17 = v;
  if (json_get_bool(o, "unused08", &v)) t->unused08 = v;
  if (json_get_bool(o, "nr19", &v)) t->nr19 = v;
}

static void write_game_options(FILE* f, const ColonizeCol1GameOptions* g) {
  int n = 0;
  fputc('{', f);
  wb(f, &n, "woi", g->woi);
  wb(f, &n, "ref_present", g->ref_present);
  wb(f, &n, "woi_crosses_event", g->woi_crosses_event);
  wb(f, &n, "independence_chrome", g->independence_chrome);
  wb(f, &n, "calendar_latch", g->calendar_latch);
  wb(f, &n, "independence_force", g->independence_force);
  wb(f, &n, "ref_unit_threshold", g->ref_unit_threshold);
  wb(f, &n, "tutorial_hints", g->tutorial_hints);
  wb(f, &n, "water_color_cycling", g->water_color_cycling);
  wb(f, &n, "combat_analysis", g->combat_analysis);
  wb(f, &n, "autosave", g->autosave);
  wb(f, &n, "end_of_turn", g->end_of_turn);
  wb(f, &n, "fast_piece_slide", g->fast_piece_slide);
  wb(f, &n, "cheats_enabled", g->cheats_enabled);
  wb(f, &n, "show_foreign_moves", g->show_foreign_moves);
  wb(f, &n, "show_indian_moves", g->show_indian_moves);
  fputc('}', f);
}
static void read_game_options(const JsonValue* o, ColonizeCol1GameOptions* g) {
  bool v;
  if (json_get_bool(o, "woi", &v)) g->woi = v;
  if (json_get_bool(o, "ref_present", &v)) g->ref_present = v;
  if (json_get_bool(o, "woi_crosses_event", &v)) g->woi_crosses_event = v;
  if (json_get_bool(o, "independence_chrome", &v)) g->independence_chrome = v;
  if (json_get_bool(o, "calendar_latch", &v)) g->calendar_latch = v;
  if (json_get_bool(o, "independence_force", &v)) g->independence_force = v;
  if (json_get_bool(o, "ref_unit_threshold", &v)) g->ref_unit_threshold = v;
  if (json_get_bool(o, "tutorial_hints", &v)) g->tutorial_hints = v;
  if (json_get_bool(o, "water_color_cycling", &v)) g->water_color_cycling = v;
  if (json_get_bool(o, "combat_analysis", &v)) g->combat_analysis = v;
  if (json_get_bool(o, "autosave", &v)) g->autosave = v;
  if (json_get_bool(o, "end_of_turn", &v)) g->end_of_turn = v;
  if (json_get_bool(o, "fast_piece_slide", &v)) g->fast_piece_slide = v;
  if (json_get_bool(o, "cheats_enabled", &v)) g->cheats_enabled = v;
  if (json_get_bool(o, "show_foreign_moves", &v)) g->show_foreign_moves = v;
  if (json_get_bool(o, "show_indian_moves", &v)) g->show_indian_moves = v;
}

static void write_colony_report_options(FILE* f, const ColonizeCol1ColonyReportOptions* r) {
  int n = 0;
  fputc('{', f);
  wb(f, &n, "labels_on_cargo_and_terrain", r->labels_on_cargo_and_terrain);
  wb(f, &n, "labels_on_buildings", r->labels_on_buildings);
  wb(f, &n, "report_new_cargos_available", r->report_new_cargos_available);
  wb(f, &n, "report_inefficient_government", r->report_inefficient_government);
  wb(f, &n, "report_tools_needed_for_production", r->report_tools_needed_for_production);
  wb(f, &n, "report_raw_materials_shortages", r->report_raw_materials_shortages);
  wb(f, &n, "report_food_shortages", r->report_food_shortages);
  wb(f, &n, "report_when_colonists_trained", r->report_when_colonists_trained);
  wb(f, &n, "report_sons_of_liberty_membership", r->report_sons_of_liberty_membership);
  wb(f, &n, "report_rebel_majorities", r->report_rebel_majorities);
  wu(f, &n, "unused03", r->unused03);
  fputc('}', f);
}
static void read_colony_report_options(const JsonValue* o, ColonizeCol1ColonyReportOptions* r) {
  bool v;
  uint64_t u;
  if (json_get_bool(o, "labels_on_cargo_and_terrain", &v)) r->labels_on_cargo_and_terrain = v;
  if (json_get_bool(o, "labels_on_buildings", &v)) r->labels_on_buildings = v;
  if (json_get_bool(o, "report_new_cargos_available", &v)) r->report_new_cargos_available = v;
  if (json_get_bool(o, "report_inefficient_government", &v)) r->report_inefficient_government = v;
  if (json_get_bool(o, "report_tools_needed_for_production", &v))
    r->report_tools_needed_for_production = v;
  if (json_get_bool(o, "report_raw_materials_shortages", &v))
    r->report_raw_materials_shortages = v;
  if (json_get_bool(o, "report_food_shortages", &v)) r->report_food_shortages = v;
  if (json_get_bool(o, "report_when_colonists_trained", &v))
    r->report_when_colonists_trained = v;
  if (json_get_bool(o, "report_sons_of_liberty_membership", &v))
    r->report_sons_of_liberty_membership = v;
  if (json_get_bool(o, "report_rebel_majorities", &v)) r->report_rebel_majorities = v;
  if (json_get_u64(o, "unused03", &u)) r->unused03 = (uint16_t)u;
}

static void write_tut2(FILE* f, const ColonizeCol1Tut2* t) {
  int n = 0;
  fputc('{', f);
  wb(f, &n, "howtowin", t->howtowin);
  wb(f, &n, "background_music", t->background_music);
  wb(f, &n, "event_music", t->event_music);
  wb(f, &n, "sound_effects", t->sound_effects);
  wb(f, &n, "nr1", t->nr1);
  wb(f, &n, "unused04", t->unused04);
  wb(f, &n, "nr3", t->nr3);
  wb(f, &n, "nr4", t->nr4);
  fputc('}', f);
}
static void read_tut2(const JsonValue* o, ColonizeCol1Tut2* t) {
  bool v;
  if (json_get_bool(o, "howtowin", &v)) t->howtowin = v;
  if (json_get_bool(o, "background_music", &v)) t->background_music = v;
  if (json_get_bool(o, "event_music", &v)) t->event_music = v;
  if (json_get_bool(o, "sound_effects", &v)) t->sound_effects = v;
  if (json_get_bool(o, "nr1", &v)) t->nr1 = v;
  if (json_get_bool(o, "unused04", &v)) t->unused04 = v;
  if (json_get_bool(o, "nr3", &v)) t->nr3 = v;
  if (json_get_bool(o, "nr4", &v)) t->nr4 = v;
}

static void write_tut3(FILE* f, const ColonizeCol1Tut3* t) {
  int n = 0;
  fputc('{', f);
  wb(f, &n, "nr5", t->nr5);
  wb(f, &n, "nr6", t->nr6);
  wb(f, &n, "nr7", t->nr7);
  wb(f, &n, "nr8", t->nr8);
  wb(f, &n, "nr9", t->nr9);
  wb(f, &n, "nr10", t->nr10);
  wb(f, &n, "nr11", t->nr11);
  wb(f, &n, "nr12", t->nr12);
  fputc('}', f);
}
static void read_tut3(const JsonValue* o, ColonizeCol1Tut3* t) {
  bool v;
  if (json_get_bool(o, "nr5", &v)) t->nr5 = v;
  if (json_get_bool(o, "nr6", &v)) t->nr6 = v;
  if (json_get_bool(o, "nr7", &v)) t->nr7 = v;
  if (json_get_bool(o, "nr8", &v)) t->nr8 = v;
  if (json_get_bool(o, "nr9", &v)) t->nr9 = v;
  if (json_get_bool(o, "nr10", &v)) t->nr10 = v;
  if (json_get_bool(o, "nr11", &v)) t->nr11 = v;
  if (json_get_bool(o, "nr12", &v)) t->nr12 = v;
}

static void write_event_flags(FILE* f, const ColonizeCol1EventFlags* e) {
  int n = 0;
  fputc('{', f);
  wb(f, &n, "discovery_of_the_new_world", e->discovery_of_the_new_world);
  wb(f, &n, "building_a_colony", e->building_a_colony);
  wb(f, &n, "meeting_the_natives", e->meeting_the_natives);
  wb(f, &n, "the_aztec_empire", e->the_aztec_empire);
  wb(f, &n, "the_inca_nation", e->the_inca_nation);
  wb(f, &n, "discovery_of_the_pacific_ocean", e->discovery_of_the_pacific_ocean);
  wb(f, &n, "entering_indian_village", e->entering_indian_village);
  wb(f, &n, "the_fountain_of_youth", e->the_fountain_of_youth);
  wb(f, &n, "cargo_from_the_new_world", e->cargo_from_the_new_world);
  wb(f, &n, "meeting_fellow_europeans", e->meeting_fellow_europeans);
  wb(f, &n, "colony_burning", e->colony_burning);
  wb(f, &n, "colony_destroyed", e->colony_destroyed);
  wb(f, &n, "indian_raid", e->indian_raid);
  wb(f, &n, "woodcut14", e->woodcut14);
  wb(f, &n, "woodcut15", e->woodcut15);
  wb(f, &n, "woodcut16", e->woodcut16);
  fputc('}', f);
}
static void read_event_flags(const JsonValue* o, ColonizeCol1EventFlags* e) {
  bool v;
  if (json_get_bool(o, "discovery_of_the_new_world", &v)) e->discovery_of_the_new_world = v;
  if (json_get_bool(o, "building_a_colony", &v)) e->building_a_colony = v;
  if (json_get_bool(o, "meeting_the_natives", &v)) e->meeting_the_natives = v;
  if (json_get_bool(o, "the_aztec_empire", &v)) e->the_aztec_empire = v;
  if (json_get_bool(o, "the_inca_nation", &v)) e->the_inca_nation = v;
  if (json_get_bool(o, "discovery_of_the_pacific_ocean", &v))
    e->discovery_of_the_pacific_ocean = v;
  if (json_get_bool(o, "entering_indian_village", &v)) e->entering_indian_village = v;
  if (json_get_bool(o, "the_fountain_of_youth", &v)) e->the_fountain_of_youth = v;
  if (json_get_bool(o, "cargo_from_the_new_world", &v)) e->cargo_from_the_new_world = v;
  if (json_get_bool(o, "meeting_fellow_europeans", &v)) e->meeting_fellow_europeans = v;
  if (json_get_bool(o, "colony_burning", &v)) e->colony_burning = v;
  if (json_get_bool(o, "colony_destroyed", &v)) e->colony_destroyed = v;
  if (json_get_bool(o, "indian_raid", &v)) e->indian_raid = v;
  if (json_get_bool(o, "woodcut14", &v)) e->woodcut14 = v;
  if (json_get_bool(o, "woodcut15", &v)) e->woodcut15 = v;
  if (json_get_bool(o, "woodcut16", &v)) e->woodcut16 = v;
}

static void write_head(FILE* f, const ColonizeCol1Head* h) {
  int n = 0;
  fputc('{', f);
  wi(f, &n, "save_version", h->save_version);
  wi(f, &n, "map_size_x", h->map_size_x);
  wi(f, &n, "map_size_y", h->map_size_y);
  jkey(f, &n, "tut1");
  write_tut1(f, &h->tut1);
  wi(f, &n, "hotseat_woi_redirect_pending", h->hotseat_woi_redirect_pending);
  jkey(f, &n, "game_options");
  write_game_options(f, &h->game_options);
  jkey(f, &n, "colony_report_options");
  write_colony_report_options(f, &h->colony_report_options);
  jkey(f, &n, "tut2");
  write_tut2(f, &h->tut2);
  jkey(f, &n, "tut3");
  write_tut3(f, &h->tut3);
  wi(f, &n, "start_mode_marker", h->start_mode_marker);
  wi(f, &n, "year", h->year);
  wi(f, &n, "autumn", h->autumn);
  wi(f, &n, "turn", h->turn);
  wi(f, &n, "map_mode", h->map_mode);
  wi(f, &n, "active_unit", h->active_unit);
  wi(f, &n, "nation_turn", h->nation_turn);
  wi(f, &n, "curr_nation_map_view", h->curr_nation_map_view);
  wi(f, &n, "human_player", h->human_player);
  wi(f, &n, "tribe_count", h->tribe_count);
  wi(f, &n, "unit_count", h->unit_count);
  wi(f, &n, "colony_count", h->colony_count);
  wi(f, &n, "trade_route_count", h->trade_route_count);
  wi(f, &n, "show_entire_map", h->show_entire_map);
  wi(f, &n, "fixed_nation_map_view", h->fixed_nation_map_view);
  wi(f, &n, "difficulty", h->difficulty);
  wi(f, &n, "king_audience_streak", h->king_audience_streak);
  wi(f, &n, "king_audience_last_pick", h->king_audience_last_pick);
  W_I8ARR(f, &n, "founding_father", h->founding_father, COLONIZE_COL1_FF_COUNT);
  wi(f, &n, "turn_loop_running", h->turn_loop_running);
  wi(f, &n, "map_modal_active", h->map_modal_active);
  wi(f, &n, "no_unit_selected", h->no_unit_selected);
  W_I16ARR(f, &n, "nation_relation", h->nation_relation, 4);
  wi(f, &n, "rebel_sentiment_report", h->rebel_sentiment_report);
  wi(f, &n, "crown_nation_id", h->crown_nation_id);
  wi(f, &n, "rival_nation_slot_1", h->rival_nation_slot_1);
  wi(f, &n, "rival_nation_slot_2", h->rival_nation_slot_2);
  wi(f, &n, "sol_pct_last_notified", h->sol_pct_last_notified);
  W_U16ARR(f, &n, "expeditionary_force", h->expeditionary_force, 4);
  W_U16ARR(f, &n, "backup_force", h->backup_force, 4);
  W_U16ARR(f, &n, "price_group_state", h->price_group_state, 16);
  jkey(f, &n, "event");
  write_event_flags(f, &h->event);
  wh(f, &n, "unknown05_hex", h->unknown05, sizeof h->unknown05);
  fputc('}', f);
}

static void read_head(const JsonValue* o, ColonizeCol1Head* h) {
  int64_t i;
  uint64_t u;
  if (json_get_u64(o, "map_size_x", &u)) h->map_size_x = (uint16_t)u;
  if (json_get_u64(o, "map_size_y", &u)) h->map_size_y = (uint16_t)u;
  JsonValue* sub = json_obj_get(o, "tut1");
  if (sub) read_tut1(sub, &h->tut1);
  if (json_get_u64(o, "hotseat_woi_redirect_pending", &u)) h->hotseat_woi_redirect_pending = (uint8_t)u;
  sub = json_obj_get(o, "game_options");
  if (sub) read_game_options(sub, &h->game_options);
  sub = json_obj_get(o, "colony_report_options");
  if (sub) read_colony_report_options(sub, &h->colony_report_options);
  sub = json_obj_get(o, "tut2");
  if (sub) read_tut2(sub, &h->tut2);
  sub = json_obj_get(o, "tut3");
  if (sub) read_tut3(sub, &h->tut3);
  if (json_get_u64(o, "start_mode_marker", &u)) h->start_mode_marker = (uint16_t)u;
  if (json_get_u64(o, "year", &u)) h->year = (uint16_t)u;
  if (json_get_u64(o, "autumn", &u)) h->autumn = (uint16_t)u;
  if (json_get_u64(o, "turn", &u)) h->turn = (uint16_t)u;
  if (json_get_u64(o, "map_mode", &u)) h->map_mode = (uint16_t)u;
  if (json_get_u64(o, "active_unit", &u)) h->active_unit = (uint16_t)u;
  if (json_get_u64(o, "nation_turn", &u)) h->nation_turn = (uint16_t)u;
  if (json_get_u64(o, "curr_nation_map_view", &u)) h->curr_nation_map_view = (uint16_t)u;
  if (json_get_u64(o, "human_player", &u)) h->human_player = (uint16_t)u;
  if (json_get_u64(o, "tribe_count", &u)) h->tribe_count = (uint16_t)u;
  if (json_get_u64(o, "unit_count", &u)) h->unit_count = (uint16_t)u;
  if (json_get_u64(o, "colony_count", &u)) h->colony_count = (uint16_t)u;
  if (json_get_u64(o, "trade_route_count", &u)) h->trade_route_count = (uint16_t)u;
  if (json_get_u64(o, "show_entire_map", &u)) h->show_entire_map = (uint16_t)u;
  if (json_get_u64(o, "fixed_nation_map_view", &u)) h->fixed_nation_map_view = (uint16_t)u;
  if (json_get_u64(o, "difficulty", &u)) h->difficulty = (uint8_t)u;
  if (json_get_u64(o, "king_audience_streak", &u)) h->king_audience_streak = (uint8_t)u;
  if (json_get_u64(o, "king_audience_last_pick", &u)) h->king_audience_last_pick = (uint8_t)u;
  JsonValue* ff = json_obj_get(o, "founding_father");
  if (ff && ff->type == JV_ARR) {
    size_t n2 = json_arr_len(ff);
    if (n2 > COLONIZE_COL1_FF_COUNT) n2 = COLONIZE_COL1_FF_COUNT;
    for (size_t i2 = 0; i2 < n2; ++i2) {
      JsonValue* it = json_arr_at(ff, i2);
      if (it && it->type == JV_NUM) h->founding_father[i2] = (int8_t)it->num;
    }
  }
  if (json_get_i64(o, "turn_loop_running", &i)) h->turn_loop_running = (uint16_t)i;
  if (json_get_i64(o, "map_modal_active", &i)) h->map_modal_active = (uint16_t)i;
  if (json_get_i64(o, "no_unit_selected", &i)) h->no_unit_selected = (uint16_t)i;
  JsonValue* nr = json_obj_get(o, "nation_relation");
  if (nr && nr->type == JV_ARR) {
    for (size_t i2 = 0; i2 < 4 && i2 < json_arr_len(nr); ++i2) {
      JsonValue* it = json_arr_at(nr, i2);
      if (it && it->type == JV_NUM) h->nation_relation[i2] = (int16_t)it->num;
    }
  }
  if (json_get_i64(o, "rebel_sentiment_report", &i)) h->rebel_sentiment_report = (int16_t)i;
  if (json_get_i64(o, "crown_nation_id", &i)) h->crown_nation_id = (int16_t)i;
  if (json_get_i64(o, "rival_nation_slot_1", &i)) h->rival_nation_slot_1 = (int16_t)i;
  if (json_get_i64(o, "rival_nation_slot_2", &i)) h->rival_nation_slot_2 = (int16_t)i;
  if (json_get_i64(o, "sol_pct_last_notified", &i)) h->sol_pct_last_notified = (int16_t)i;
  JsonValue* ef = json_obj_get(o, "expeditionary_force");
  if (ef && ef->type == JV_ARR) {
    for (size_t i2 = 0; i2 < 4 && i2 < json_arr_len(ef); ++i2) {
      JsonValue* it = json_arr_at(ef, i2);
      if (it && it->type == JV_NUM) h->expeditionary_force[i2] = (uint16_t)it->num;
    }
  }
  JsonValue* bf = json_obj_get(o, "backup_force");
  if (bf && bf->type == JV_ARR) {
    for (size_t i2 = 0; i2 < 4 && i2 < json_arr_len(bf); ++i2) {
      JsonValue* it = json_arr_at(bf, i2);
      if (it && it->type == JV_NUM) h->backup_force[i2] = (uint16_t)it->num;
    }
  }
  JsonValue* pg = json_obj_get(o, "price_group_state");
  if (pg && pg->type == JV_ARR) {
    for (size_t i2 = 0; i2 < 16 && i2 < json_arr_len(pg); ++i2) {
      JsonValue* it = json_arr_at(pg, i2);
      if (it && it->type == JV_NUM) h->price_group_state[i2] = (uint16_t)it->num;
    }
  }
  sub = json_obj_get(o, "event");
  if (sub) read_event_flags(sub, &h->event);
  json_get_hex(o, "unknown05_hex", h->unknown05, sizeof h->unknown05);
}

static void write_player(FILE* f, const ColonizeCol1Player* p) {
  int n = 0;
  fputc('{', f);
  ws(f, &n, "name", p->name, sizeof p->name);
  ws(f, &n, "country_name", p->country_name, sizeof p->country_name);
  wu(f, &n, "unknown06_lo", p->unknown06_lo);
  wb(f, &n, "lcr_case5_bonus_used", p->lcr_case5_bonus_used);
  wb(f, &n, "named_new_world", p->named_new_world);
  wi(f, &n, "control", p->control);
  wi(f, &n, "founded_colonies", p->founded_colonies);
  wi(f, &n, "diplomacy", p->diplomacy);
  fputc('}', f);
}
static void read_player(const JsonValue* o, ColonizeCol1Player* p) {
  json_get_cstr_fixed(o, "name", p->name, sizeof p->name);
  json_get_cstr_fixed(o, "country_name", p->country_name, sizeof p->country_name);
  uint64_t u;
  bool v;
  if (json_get_u64(o, "unknown06_lo", &u)) p->unknown06_lo = (uint8_t)u;
  if (json_get_bool(o, "lcr_case5_bonus_used", &v)) p->lcr_case5_bonus_used = v;
  if (json_get_bool(o, "named_new_world", &v)) p->named_new_world = v;
  if (json_get_u64(o, "control", &u)) p->control = (uint8_t)u;
  if (json_get_u64(o, "founded_colonies", &u)) p->founded_colonies = (uint8_t)u;
  if (json_get_u64(o, "diplomacy", &u)) p->diplomacy = (uint8_t)u;
}

static void write_buildings(FILE* f, const ColonizeCol1Buildings* b) {
  int n = 0;
  fputc('{', f);
  wu(f, &n, "fortification", b->fortification);
  wu(f, &n, "armory", b->armory);
  wu(f, &n, "docks", b->docks);
  wu(f, &n, "town_hall", b->town_hall);
  wu(f, &n, "schoolhouse", b->schoolhouse);
  wu(f, &n, "warehouse", b->warehouse);
  wu(f, &n, "stables", b->stables);
  wu(f, &n, "custom_house", b->custom_house);
  wu(f, &n, "printing_press", b->printing_press);
  wu(f, &n, "weavers_house", b->weavers_house);
  wu(f, &n, "tobacconists_house", b->tobacconists_house);
  wu(f, &n, "rum_distillers_house", b->rum_distillers_house);
  wu(f, &n, "capitol", b->capitol);
  wu(f, &n, "fur_traders_house", b->fur_traders_house);
  wu(f, &n, "carpenters_shop", b->carpenters_shop);
  wu(f, &n, "church", b->church);
  wu(f, &n, "blacksmiths_house", b->blacksmiths_house);
  wu(f, &n, "unused05", b->unused05);
  fputc('}', f);
}
static void read_buildings(const JsonValue* o, ColonizeCol1Buildings* b) {
  uint64_t u;
  if (json_get_u64(o, "fortification", &u)) b->fortification = (uint32_t)u;
  if (json_get_u64(o, "armory", &u)) b->armory = (uint32_t)u;
  if (json_get_u64(o, "docks", &u)) b->docks = (uint32_t)u;
  if (json_get_u64(o, "town_hall", &u)) b->town_hall = (uint32_t)u;
  if (json_get_u64(o, "schoolhouse", &u)) b->schoolhouse = (uint32_t)u;
  if (json_get_u64(o, "warehouse", &u)) b->warehouse = (uint32_t)u;
  if (json_get_u64(o, "stables", &u)) b->stables = (uint32_t)u;
  if (json_get_u64(o, "custom_house", &u)) b->custom_house = (uint32_t)u;
  if (json_get_u64(o, "printing_press", &u)) b->printing_press = (uint32_t)u;
  if (json_get_u64(o, "weavers_house", &u)) b->weavers_house = (uint32_t)u;
  if (json_get_u64(o, "tobacconists_house", &u)) b->tobacconists_house = (uint32_t)u;
  if (json_get_u64(o, "rum_distillers_house", &u)) b->rum_distillers_house = (uint32_t)u;
  if (json_get_u64(o, "capitol", &u)) b->capitol = (uint32_t)u;
  if (json_get_u64(o, "fur_traders_house", &u)) b->fur_traders_house = (uint16_t)u;
  if (json_get_u64(o, "carpenters_shop", &u)) b->carpenters_shop = (uint16_t)u;
  if (json_get_u64(o, "church", &u)) b->church = (uint16_t)u;
  if (json_get_u64(o, "blacksmiths_house", &u)) b->blacksmiths_house = (uint16_t)u;
  if (json_get_u64(o, "unused05", &u)) b->unused05 = (uint16_t)u;
}

static void write_custom_house(FILE* f, const ColonizeCol1CustomHouse* c) {
  int n = 0;
  fputc('{', f);
  wb(f, &n, "food", c->food);
  wb(f, &n, "sugar", c->sugar);
  wb(f, &n, "tobacco", c->tobacco);
  wb(f, &n, "cotton", c->cotton);
  wb(f, &n, "furs", c->furs);
  wb(f, &n, "lumber", c->lumber);
  wb(f, &n, "ore", c->ore);
  wb(f, &n, "silver", c->silver);
  wb(f, &n, "horses", c->horses);
  wb(f, &n, "rum", c->rum);
  wb(f, &n, "cigars", c->cigars);
  wb(f, &n, "cloth", c->cloth);
  wb(f, &n, "coats", c->coats);
  wb(f, &n, "trade_goods", c->trade_goods);
  wb(f, &n, "tools", c->tools);
  wb(f, &n, "muskets", c->muskets);
  fputc('}', f);
}
static void read_custom_house(const JsonValue* o, ColonizeCol1CustomHouse* c) {
  bool v;
  if (json_get_bool(o, "food", &v)) c->food = v;
  if (json_get_bool(o, "sugar", &v)) c->sugar = v;
  if (json_get_bool(o, "tobacco", &v)) c->tobacco = v;
  if (json_get_bool(o, "cotton", &v)) c->cotton = v;
  if (json_get_bool(o, "furs", &v)) c->furs = v;
  if (json_get_bool(o, "lumber", &v)) c->lumber = v;
  if (json_get_bool(o, "ore", &v)) c->ore = v;
  if (json_get_bool(o, "silver", &v)) c->silver = v;
  if (json_get_bool(o, "horses", &v)) c->horses = v;
  if (json_get_bool(o, "rum", &v)) c->rum = v;
  if (json_get_bool(o, "cigars", &v)) c->cigars = v;
  if (json_get_bool(o, "cloth", &v)) c->cloth = v;
  if (json_get_bool(o, "coats", &v)) c->coats = v;
  if (json_get_bool(o, "trade_goods", &v)) c->trade_goods = v;
  if (json_get_bool(o, "tools", &v)) c->tools = v;
  if (json_get_bool(o, "muskets", &v)) c->muskets = v;
}

static void write_colony_ai_flags(FILE* f, const ColonizeCol1ColonyAiFlags* a) {
  int n = 0;
  fputc('{', f);
  wb(f, &n, "nearby_armed_ship", a->nearby_armed_ship);
  wb(f, &n, "nearby_man_o_war", a->nearby_man_o_war);
  wb(f, &n, "needs_military", a->needs_military);
  wb(f, &n, "defense_surplus", a->defense_surplus);
  wb(f, &n, "needs_colonists", a->needs_colonists);
  wb(f, &n, "specialist_pressure", a->specialist_pressure);
  wb(f, &n, "needs_garrison", a->needs_garrison);
  wb(f, &n, "expansion_pressure", a->expansion_pressure);
  fputc('}', f);
}
static void read_colony_ai_flags(const JsonValue* o, ColonizeCol1ColonyAiFlags* a) {
  bool v;
  if (json_get_bool(o, "nearby_armed_ship", &v)) a->nearby_armed_ship = v;
  if (json_get_bool(o, "nearby_man_o_war", &v)) a->nearby_man_o_war = v;
  if (json_get_bool(o, "needs_military", &v)) a->needs_military = v;
  if (json_get_bool(o, "defense_surplus", &v)) a->defense_surplus = v;
  if (json_get_bool(o, "needs_colonists", &v)) a->needs_colonists = v;
  if (json_get_bool(o, "specialist_pressure", &v)) a->specialist_pressure = v;
  if (json_get_bool(o, "needs_garrison", &v)) a->needs_garrison = v;
  if (json_get_bool(o, "expansion_pressure", &v)) a->expansion_pressure = v;
}

static void write_colony_flags(FILE* f, const ColonizeCol1ColonyFlags* c) {
  int n = 0;
  fputc('{', f);
  wb(f, &n, "ref_landing", c->ref_landing);
  wb(f, &n, "sol_100", c->sol_100);
  wb(f, &n, "sol_50", c->sol_50);
  wb(f, &n, "starvation", c->starvation);
  wb(f, &n, "small_colony_ai", c->small_colony_ai);
  wb(f, &n, "wagon_train", c->wagon_train);
  wb(f, &n, "coastal", c->coastal);
  wb(f, &n, "build_complete", c->build_complete);
  fputc('}', f);
}
static void read_colony_flags(const JsonValue* o, ColonizeCol1ColonyFlags* c) {
  bool v;
  if (json_get_bool(o, "ref_landing", &v)) c->ref_landing = v;
  if (json_get_bool(o, "sol_100", &v)) c->sol_100 = v;
  if (json_get_bool(o, "sol_50", &v)) c->sol_50 = v;
  if (json_get_bool(o, "starvation", &v)) c->starvation = v;
  if (json_get_bool(o, "small_colony_ai", &v)) c->small_colony_ai = v;
  if (json_get_bool(o, "wagon_train", &v)) c->wagon_train = v;
  if (json_get_bool(o, "coastal", &v)) c->coastal = v;
  if (json_get_bool(o, "build_complete", &v)) c->build_complete = v;
}

static void write_colony(FILE* f, const ColonizeCol1Colony* c) {
  int n = 0;
  fputc('{', f);
  wi(f, &n, "x", c->x);
  wi(f, &n, "y", c->y);
  ws(f, &n, "name", c->name, sizeof c->name);
  wi(f, &n, "nation_id", c->nation_id);
  jkey(f, &n, "ai_flags");
  write_colony_ai_flags(f, &c->ai_flags);
  jkey(f, &n, "flags");
  write_colony_flags(f, &c->flags);
  wi(f, &n, "build_ai_flags", c->build_ai_flags);
  wi(f, &n, "garrison_quota", c->garrison_quota);
  wi(f, &n, "population", c->population);
  W_U8ARR(f, &n, "occupation", c->occupation, COLONIZE_COL1_COLONY_POP_MAX);
  W_U8ARR(f, &n, "profession", c->profession, COLONIZE_COL1_COLONY_POP_MAX);
  jkey(f, &n, "specialty");
  fputc('[', f);
  for (int i = 0; i < 16; ++i) {
    if (i) fputc(',', f);
    fprintf(f, "{\"even\":%u,\"odd\":%u}", c->specialty[i].even, c->specialty[i].odd);
  }
  fputc(']', f);
  W_I8ARR(f, &n, "tiles", c->tiles, COLONIZE_COL1_COLONY_TILES);
  jkey(f, &n, "buildings");
  write_buildings(f, &c->buildings);
  jkey(f, &n, "custom_house");
  write_custom_house(f, &c->custom_house);
  wi(f, &n, "improve_timer", c->improve_timer);
  wi(f, &n, "specialty_cargo", c->specialty_cargo);
  wi(f, &n, "labor_shortage", c->labor_shortage);
  wi(f, &n, "cargo_idle_turns", c->cargo_idle_turns);
  wi(f, &n, "cargo_produced_mask", c->cargo_produced_mask);
  wi(f, &n, "hammers", c->hammers);
  wi(f, &n, "building_in_production", c->building_in_production);
  wi(f, &n, "warehouse_level", c->warehouse_level);
  wi(f, &n, "capitol_level", c->capitol_level);
  wi(f, &n, "depletion_counter", c->depletion_counter);
  wi(f, &n, "hammers_purchased", c->hammers_purchased);
  W_U16ARR(f, &n, "stock", c->stock, COLONIZE_COL1_CARGO_TYPES);
  W_U8ARR(f, &n, "visible_to_euro", c->visible_to_euro, 4);
  W_U8ARR(f, &n, "unknown13_pad", c->unknown13_pad, 4);
  wu(f, &n, "rebel_dividend", c->rebel_dividend);
  wu(f, &n, "rebel_divisor", c->rebel_divisor);
  fputc('}', f);
}

static void read_colony(const JsonValue* o, ColonizeCol1Colony* c) {
  uint64_t u;
  if (json_get_u64(o, "x", &u)) c->x = (uint8_t)u;
  if (json_get_u64(o, "y", &u)) c->y = (uint8_t)u;
  json_get_cstr_fixed(o, "name", c->name, sizeof c->name);
  if (json_get_u64(o, "nation_id", &u)) c->nation_id = (uint8_t)u;
  JsonValue* sub = json_obj_get(o, "ai_flags");
  if (sub) read_colony_ai_flags(sub, &c->ai_flags);
  sub = json_obj_get(o, "flags");
  if (sub) read_colony_flags(sub, &c->flags);
  if (json_get_u64(o, "build_ai_flags", &u)) c->build_ai_flags = (uint8_t)u;
  if (json_get_u64(o, "garrison_quota", &u)) c->garrison_quota = (uint8_t)u;
  if (json_get_u64(o, "population", &u)) c->population = (uint8_t)u;
  JsonValue* arr = json_obj_get(o, "occupation");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_COLONY_POP_MAX && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) c->occupation[i] = (uint8_t)it->num;
    }
  }
  arr = json_obj_get(o, "profession");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_COLONY_POP_MAX && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) c->profession[i] = (uint8_t)it->num;
    }
  }
  arr = json_obj_get(o, "specialty");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < 16 && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      int64_t ev, od;
      if (json_get_i64(it, "even", &ev)) c->specialty[i].even = (uint8_t)ev;
      if (json_get_i64(it, "odd", &od)) c->specialty[i].odd = (uint8_t)od;
    }
  }
  arr = json_obj_get(o, "tiles");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_COLONY_TILES && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) c->tiles[i] = (int8_t)it->num;
    }
  }
  sub = json_obj_get(o, "buildings");
  if (sub) read_buildings(sub, &c->buildings);
  sub = json_obj_get(o, "custom_house");
  if (sub) read_custom_house(sub, &c->custom_house);
  if (json_get_u64(o, "improve_timer", &u)) c->improve_timer = (uint8_t)u;
  if (json_get_u64(o, "specialty_cargo", &u)) c->specialty_cargo = (uint8_t)u;
  if (json_get_u64(o, "labor_shortage", &u)) c->labor_shortage = (uint8_t)u;
  if (json_get_u64(o, "cargo_idle_turns", &u)) c->cargo_idle_turns = (uint8_t)u;
  if (json_get_u64(o, "cargo_produced_mask", &u)) c->cargo_produced_mask = (uint16_t)u;
  if (json_get_u64(o, "hammers", &u)) c->hammers = (uint16_t)u;
  if (json_get_u64(o, "building_in_production", &u)) c->building_in_production = (uint8_t)u;
  if (json_get_u64(o, "warehouse_level", &u)) c->warehouse_level = (uint8_t)u;
  if (json_get_u64(o, "capitol_level", &u)) c->capitol_level = (uint8_t)u;
  if (json_get_u64(o, "depletion_counter", &u)) c->depletion_counter = (uint8_t)u;
  if (json_get_u64(o, "hammers_purchased", &u)) c->hammers_purchased = (uint16_t)u;
  arr = json_obj_get(o, "stock");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_CARGO_TYPES && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) c->stock[i] = (uint16_t)it->num;
    }
  }
  arr = json_obj_get(o, "visible_to_euro");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < 4 && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) c->visible_to_euro[i] = (uint8_t)it->num;
    }
  }
  arr = json_obj_get(o, "unknown13_pad");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < 4 && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) c->unknown13_pad[i] = (uint8_t)it->num;
    }
  }
  if (json_get_u64(o, "rebel_dividend", &u)) c->rebel_dividend = (uint32_t)u;
  if (json_get_u64(o, "rebel_divisor", &u)) c->rebel_divisor = (uint32_t)u;
}

static void write_unit(FILE* f, const ColonizeCol1Unit* u) {
  int n = 0;
  fputc('{', f);
  wi(f, &n, "x", u->x);
  wi(f, &n, "y", u->y);
  wi(f, &n, "type", u->type);
  wu(f, &n, "nation_id", u->nation_id);
  wu(f, &n, "vis_mask", u->vis_mask);
  jkey(f, &n, "flags");
  {
    int m = 0;
    fputc('{', f);
    wb(f, &m, "unknown15_bit0", u->unknown15_bit0);
    wb(f, &m, "roam_reeval_pending", u->roam_reeval_pending);
    wb(f, &m, "stack_has_founders_or_military", u->stack_has_founders_or_military);
    wb(f, &m, "stack_has_military", u->stack_has_military);
    wb(f, &m, "wander_dest_chosen", u->wander_dest_chosen);
    wb(f, &m, "garrison_request_pending", u->garrison_request_pending);
    wb(f, &m, "bound_in_transit", u->bound_in_transit);
    wb(f, &m, "ship_damaged", u->ship_damaged);
    fputc('}', f);
  }
  wi(f, &n, "moves", u->moves);
  wi(f, &n, "origin", u->origin);
  wi(f, &n, "ai_plan", u->ai_plan);
  wi(f, &n, "orders", u->orders);
  wi(f, &n, "goto_x", u->goto_x);
  wi(f, &n, "goto_y", u->goto_y);
  wu(f, &n, "facing", u->facing);
  wu(f, &n, "facing_pad", u->facing_pad);
  wi(f, &n, "holds_occupied", u->holds_occupied);
  jkey(f, &n, "cargo_items");
  fprintf(
    f,
    "[%u,%u,%u,%u,%u,%u]",
    u->cargo_item_0,
    u->cargo_item_1,
    u->cargo_item_2,
    u->cargo_item_3,
    u->cargo_item_4,
    u->cargo_item_5
  );
  W_U8ARR(f, &n, "cargo_hold", u->cargo_hold, sizeof u->cargo_hold);
  wi(f, &n, "turns_worked", u->turns_worked);
  wi(f, &n, "profession", u->profession);
  jkey(f, &n, "transport_chain");
  fprintf(
    f,
    "{\"next_unit_idx\":%d,\"prev_unit_idx\":%d}",
    u->transport_chain.next_unit_idx,
    u->transport_chain.prev_unit_idx
  );
  fputc('}', f);
}

static void read_unit(const JsonValue* o, ColonizeCol1Unit* u) {
  uint64_t v;
  int64_t iv;
  if (json_get_u64(o, "x", &v)) u->x = (uint8_t)v;
  if (json_get_u64(o, "y", &v)) u->y = (uint8_t)v;
  if (json_get_u64(o, "type", &v)) u->type = (uint8_t)v;
  if (json_get_u64(o, "nation_id", &v)) u->nation_id = (uint8_t)v;
  if (json_get_u64(o, "vis_mask", &v)) u->vis_mask = (uint8_t)v;
  JsonValue* fl = json_obj_get(o, "flags");
  if (fl) {
    bool b;
    if (json_get_bool(fl, "unknown15_bit0", &b)) u->unknown15_bit0 = b;
    if (json_get_bool(fl, "roam_reeval_pending", &b)) u->roam_reeval_pending = b;
    if (json_get_bool(fl, "stack_has_founders_or_military", &b)) u->stack_has_founders_or_military = b;
    if (json_get_bool(fl, "stack_has_military", &b)) u->stack_has_military = b;
    if (json_get_bool(fl, "wander_dest_chosen", &b)) u->wander_dest_chosen = b;
    if (json_get_bool(fl, "garrison_request_pending", &b)) u->garrison_request_pending = b;
    if (json_get_bool(fl, "bound_in_transit", &b)) u->bound_in_transit = b;
    if (json_get_bool(fl, "ship_damaged", &b)) u->ship_damaged = b;
  }
  if (json_get_u64(o, "moves", &v)) u->moves = (uint8_t)v;
  if (json_get_u64(o, "origin", &v)) u->origin = (uint8_t)v;
  if (json_get_u64(o, "ai_plan", &v)) u->ai_plan = (uint8_t)v;
  if (json_get_u64(o, "orders", &v)) u->orders = (uint8_t)v;
  if (json_get_u64(o, "goto_x", &v)) u->goto_x = (uint8_t)v;
  if (json_get_u64(o, "goto_y", &v)) u->goto_y = (uint8_t)v;
  if (json_get_u64(o, "facing", &v)) u->facing = (uint8_t)v;
  if (json_get_u64(o, "facing_pad", &v)) u->facing_pad = (uint8_t)v;
  if (json_get_u64(o, "holds_occupied", &v)) u->holds_occupied = (uint8_t)v;
  JsonValue* ci = json_obj_get(o, "cargo_items");
  if (ci && ci->type == JV_ARR && json_arr_len(ci) >= 6) {
    uint8_t vals[6];
    for (int i = 0; i < 6; ++i) {
      JsonValue* it = json_arr_at(ci, (size_t)i);
      vals[i] = (it && it->type == JV_NUM) ? (uint8_t)it->num : 0;
    }
    u->cargo_item_0 = vals[0];
    u->cargo_item_1 = vals[1];
    u->cargo_item_2 = vals[2];
    u->cargo_item_3 = vals[3];
    u->cargo_item_4 = vals[4];
    u->cargo_item_5 = vals[5];
  }
  JsonValue* ch = json_obj_get(o, "cargo_hold");
  if (ch && ch->type == JV_ARR) {
    for (size_t i = 0; i < sizeof u->cargo_hold && i < json_arr_len(ch); ++i) {
      JsonValue* it = json_arr_at(ch, i);
      if (it && it->type == JV_NUM) u->cargo_hold[i] = (uint8_t)it->num;
    }
  }
  if (json_get_u64(o, "turns_worked", &v)) u->turns_worked = (uint8_t)v;
  if (json_get_u64(o, "profession", &v)) u->profession = (uint8_t)v;
  JsonValue* tc = json_obj_get(o, "transport_chain");
  if (tc) {
    if (json_get_i64(tc, "next_unit_idx", &iv)) u->transport_chain.next_unit_idx = (int16_t)iv;
    if (json_get_i64(tc, "prev_unit_idx", &iv)) u->transport_chain.prev_unit_idx = (int16_t)iv;
  }
}

static void write_nation_trade(FILE* f, const ColonizeCol1NationTrade* t) {
  int n = 0;
  fputc('{', f);
  W_U8ARR(f, &n, "euro_price", t->euro_price, COLONIZE_COL1_CARGO_TYPES);
  W_I16ARR(f, &n, "nr", t->nr, COLONIZE_COL1_CARGO_TYPES);
  W_I32ARR(f, &n, "gold", t->gold, COLONIZE_COL1_CARGO_TYPES);
  W_I32ARR(f, &n, "tons", t->tons, COLONIZE_COL1_CARGO_TYPES);
  W_I32ARR(f, &n, "tons2", t->tons2, COLONIZE_COL1_CARGO_TYPES);
  fputc('}', f);
}
static void read_nation_trade(const JsonValue* o, ColonizeCol1NationTrade* t) {
  JsonValue* arr = json_obj_get(o, "euro_price");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_CARGO_TYPES && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) t->euro_price[i] = (uint8_t)it->num;
    }
  }
  arr = json_obj_get(o, "nr");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_CARGO_TYPES && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) t->nr[i] = (int16_t)it->num;
    }
  }
  arr = json_obj_get(o, "gold");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_CARGO_TYPES && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) t->gold[i] = (int32_t)it->num;
    }
  }
  arr = json_obj_get(o, "tons");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_CARGO_TYPES && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) t->tons[i] = (int32_t)it->num;
    }
  }
  arr = json_obj_get(o, "tons2");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_CARGO_TYPES && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) t->tons2[i] = (int32_t)it->num;
    }
  }
}

static void write_nation(FILE* f, const ColonizeCol1Nation* nt) {
  int n = 0;
  fputc('{', f);
  wi(f, &n, "nation_flags", nt->nation_flags);
  wi(f, &n, "tax_rate", nt->tax_rate);
  W_U8ARR(f, &n, "recruit", nt->recruit, 3);
  wi(f, &n, "tax_hike_count", nt->tax_hike_count);
  wi(f, &n, "recruit_count", nt->recruit_count);
  W_U8ARR(f, &n, "founding_fathers", nt->founding_fathers, 4);
  wi(f, &n, "unknown21_pad", nt->unknown21_pad);
  wi(f, &n, "liberty_bells_total", nt->liberty_bells_total);
  wi(f, &n, "liberty_bells_last_turn", nt->liberty_bells_last_turn);
  wi(f, &n, "king_audience_tax_delta", nt->king_audience_tax_delta);
  wi(f, &n, "next_founding_father", nt->next_founding_father);
  wi(f, &n, "founding_father_count", nt->founding_father_count);
  wi(f, &n, "ff_count_end_prob", nt->ff_count_end_prob);
  wi(f, &n, "villages_burned", nt->villages_burned);
  wi(f, &n, "rebel_sentiment", nt->rebel_sentiment);
  wi(f, &n, "rebellion_pct_last_notified", nt->rebellion_pct_last_notified);
  W_U8ARR(f, &n, "unknown23_pad", nt->unknown23_pad, 3);
  wi(f, &n, "artillery_count", nt->artillery_count);
  wi(f, &n, "boycott_bitmap", nt->boycott_bitmap);
  wi(f, &n, "royal_money", nt->royal_money);
  W_U8ARR(f, &n, "unknown24_pad", nt->unknown24_pad, 4);
  wu(f, &n, "gold", nt->gold);
  wi(f, &n, "current_crosses", nt->current_crosses);
  wi(f, &n, "needed_crosses", nt->needed_crosses);
  wi(f, &n, "return_from_europe_x", nt->return_from_europe_x);
  wi(f, &n, "return_from_europe_y", nt->return_from_europe_y);
  W_U8ARR(f, &n, "euro_relation", nt->euro_relation, 4);
  W_U8ARR(f, &n, "relation_by_indian", nt->relation_by_indian, 8);
  W_U8ARR(f, &n, "treaty_timer", nt->treaty_timer, 4);
  W_U8ARR(f, &n, "diplo_flag", nt->diplo_flag, 4);
  wi(f, &n, "indian_hostility_sticky", nt->indian_hostility_sticky);
  wi(f, &n, "privateer_spawn_mask", nt->privateer_spawn_mask);
  W_U8ARR(f, &n, "unknown26_pad", nt->unknown26_pad, 2);
  jkey(f, &n, "trade");
  write_nation_trade(f, &nt->trade);
  fputc('}', f);
}

static void read_arr_u8(const JsonValue* o, const char* key, uint8_t* dst, size_t count) {
  JsonValue* arr = json_obj_get(o, key);
  if (!arr || arr->type != JV_ARR) return;
  for (size_t i = 0; i < count && i < json_arr_len(arr); ++i) {
    JsonValue* it = json_arr_at(arr, i);
    if (it && it->type == JV_NUM) dst[i] = (uint8_t)it->num;
  }
}

static void read_nation(const JsonValue* o, ColonizeCol1Nation* nt) {
  uint64_t u;
  int64_t i;
  if (json_get_u64(o, "nation_flags", &u)) nt->nation_flags = (uint8_t)u;
  if (json_get_u64(o, "tax_rate", &u)) nt->tax_rate = (uint8_t)u;
  read_arr_u8(o, "recruit", nt->recruit, 3);
  if (json_get_u64(o, "tax_hike_count", &u)) nt->tax_hike_count = (uint8_t)u;
  if (json_get_u64(o, "recruit_count", &u)) nt->recruit_count = (uint8_t)u;
  read_arr_u8(o, "founding_fathers", nt->founding_fathers, 4);
  if (json_get_u64(o, "unknown21_pad", &u)) nt->unknown21_pad = (uint8_t)u;
  if (json_get_u64(o, "liberty_bells_total", &u)) nt->liberty_bells_total = (uint16_t)u;
  if (json_get_u64(o, "liberty_bells_last_turn", &u)) nt->liberty_bells_last_turn = (uint16_t)u;
  if (json_get_i64(o, "king_audience_tax_delta", &i)) nt->king_audience_tax_delta = (int16_t)i;
  if (json_get_i64(o, "next_founding_father", &i)) nt->next_founding_father = (int16_t)i;
  if (json_get_u64(o, "founding_father_count", &u)) nt->founding_father_count = (uint16_t)u;
  if (json_get_u64(o, "ff_count_end_prob", &u)) nt->ff_count_end_prob = (uint16_t)u;
  if (json_get_u64(o, "villages_burned", &u)) nt->villages_burned = (uint8_t)u;
  if (json_get_u64(o, "rebel_sentiment", &u)) nt->rebel_sentiment = (uint8_t)u;
  if (json_get_u64(o, "rebellion_pct_last_notified", &u)) nt->rebellion_pct_last_notified = (uint8_t)u;
  read_arr_u8(o, "unknown23_pad", nt->unknown23_pad, 3);
  if (json_get_u64(o, "artillery_count", &u)) nt->artillery_count = (uint16_t)u;
  if (json_get_u64(o, "boycott_bitmap", &u)) nt->boycott_bitmap = (uint16_t)u;
  if (json_get_i64(o, "royal_money", &i)) nt->royal_money = (int32_t)i;
  read_arr_u8(o, "unknown24_pad", nt->unknown24_pad, 4);
  if (json_get_u64(o, "gold", &u)) nt->gold = (uint32_t)u;
  if (json_get_u64(o, "current_crosses", &u)) nt->current_crosses = (uint16_t)u;
  if (json_get_u64(o, "needed_crosses", &u)) nt->needed_crosses = (uint16_t)u;
  if (json_get_u64(o, "return_from_europe_x", &u)) nt->return_from_europe_x = (uint8_t)u;
  if (json_get_u64(o, "return_from_europe_y", &u)) nt->return_from_europe_y = (uint8_t)u;
  read_arr_u8(o, "euro_relation", nt->euro_relation, 4);
  read_arr_u8(o, "relation_by_indian", nt->relation_by_indian, 8);
  read_arr_u8(o, "treaty_timer", nt->treaty_timer, 4);
  read_arr_u8(o, "diplo_flag", nt->diplo_flag, 4);
  if (json_get_u64(o, "indian_hostility_sticky", &u)) nt->indian_hostility_sticky = (uint8_t)u;
  if (json_get_u64(o, "privateer_spawn_mask", &u)) nt->privateer_spawn_mask = (uint8_t)u;
  read_arr_u8(o, "unknown26_pad", nt->unknown26_pad, 2);
  JsonValue* sub = json_obj_get(o, "trade");
  if (sub) read_nation_trade(sub, &nt->trade);
}

static void write_tribe(FILE* f, const ColonizeCol1Tribe* t) {
  int n = 0;
  fputc('{', f);
  wi(f, &n, "x", t->x);
  wi(f, &n, "y", t->y);
  wi(f, &n, "nation_id", t->nation_id);
  jkey(f, &n, "state");
  {
    int m = 0;
    fputc('{', f);
    wb(f, &m, "artillery", t->state.artillery);
    wb(f, &m, "learned", t->state.learned);
    wb(f, &m, "capital", t->state.capital);
    wb(f, &m, "scouted", t->state.scouted);
    wb(f, &m, "needs_colonist", t->state.needs_colonist);
    wu(f, &m, "unused09", t->state.unused09);
    fputc('}', f);
  }
  wi(f, &n, "population", t->population);
  wi(f, &n, "mission", t->mission);
  wi(f, &n, "growth_accum", t->growth_accum);
  wi(f, &n, "sticky_trade_good", t->sticky_trade_good);
  wi(f, &n, "last_bought", t->last_bought);
  wi(f, &n, "last_sold", t->last_sold);
  jkey(f, &n, "alarm");
  fputc('[', f);
  for (int i = 0; i < 4; ++i) {
    if (i) fputc(',', f);
    fprintf(f, "{\"friction\":%u,\"attacks\":%u}", t->alarm[i].friction, t->alarm[i].attacks);
  }
  fputc(']', f);
  fputc('}', f);
}
static void read_tribe(const JsonValue* o, ColonizeCol1Tribe* t) {
  uint64_t u;
  if (json_get_u64(o, "x", &u)) t->x = (uint8_t)u;
  if (json_get_u64(o, "y", &u)) t->y = (uint8_t)u;
  if (json_get_u64(o, "nation_id", &u)) t->nation_id = (uint8_t)u;
  JsonValue* st = json_obj_get(o, "state");
  if (st) {
    bool b;
    if (json_get_bool(st, "artillery", &b)) t->state.artillery = b;
    if (json_get_bool(st, "learned", &b)) t->state.learned = b;
    if (json_get_bool(st, "capital", &b)) t->state.capital = b;
    if (json_get_bool(st, "scouted", &b)) t->state.scouted = b;
    if (json_get_bool(st, "needs_colonist", &b)) t->state.needs_colonist = b;
    if (json_get_u64(st, "unused09", &u)) t->state.unused09 = (uint8_t)u;
  }
  if (json_get_u64(o, "population", &u)) t->population = (uint8_t)u;
  if (json_get_u64(o, "mission", &u)) t->mission = (uint8_t)u;
  if (json_get_u64(o, "growth_accum", &u)) t->growth_accum = (uint8_t)u;
  if (json_get_u64(o, "sticky_trade_good", &u)) t->sticky_trade_good = (uint8_t)u;
  if (json_get_u64(o, "last_bought", &u)) t->last_bought = (uint8_t)u;
  if (json_get_u64(o, "last_sold", &u)) t->last_sold = (uint8_t)u;
  JsonValue* al = json_obj_get(o, "alarm");
  if (al && al->type == JV_ARR) {
    for (size_t i = 0; i < 4 && i < json_arr_len(al); ++i) {
      JsonValue* it = json_arr_at(al, i);
      int64_t fr, at;
      if (json_get_i64(it, "friction", &fr)) t->alarm[i].friction = (uint8_t)fr;
      if (json_get_i64(it, "attacks", &at)) t->alarm[i].attacks = (uint8_t)at;
    }
  }
}

static void write_indian(FILE* f, const ColonizeCol1Indian* ind) {
  int n = 0;
  fputc('{', f);
  wi(f, &n, "capitol_x", ind->capitol_x);
  wi(f, &n, "capitol_y", ind->capitol_y);
  wi(f, &n, "tech", ind->tech);
  wu(f, &n, "unknown31_lo_pad", ind->unknown31_lo_pad);
  wb(f, &n, "woi_defect_resolved", ind->woi_defect_resolved);
  wb(f, &n, "woi_defect_forced", ind->woi_defect_forced);
  wb(f, &n, "extinct", ind->extinct);
  wi(f, &n, "unknown31b", ind->unknown31b_pad);
  wi(f, &n, "lands_bought", ind->lands_bought);
  wi(f, &n, "unknown31_flags", ind->unknown31_flags);
  wi(f, &n, "muskets", ind->muskets);
  wi(f, &n, "horse_herds", ind->horse_herds);
  wi(f, &n, "unknown31c", ind->unknown31c_pad);
  wi(f, &n, "horse_breeding", ind->horse_breeding);
  wi(f, &n, "hill_silver_bid_bonus", ind->hill_silver_bid_bonus);
  W_I16ARR(f, &n, "tons", ind->tons, COLONIZE_COL1_CARGO_TYPES);
  W_I16ARR(f, &n, "contact_state", ind->contact_state, 4);
  W_I8ARR(f, &n, "euro_relation_accum", ind->euro_relation_accum, 4);
  W_U8ARR(f, &n, "euro_diplo", ind->euro_diplo, 4);
  W_U8ARR(f, &n, "unknown33", ind->unknown33_pad, 8);
  W_U16ARR(f, &n, "alarm_by_player", ind->alarm_by_player, 4);
  fputc('}', f);
}
static void read_indian(const JsonValue* o, ColonizeCol1Indian* ind) {
  uint64_t u;
  int64_t i;
  bool b;
  if (json_get_u64(o, "capitol_x", &u)) ind->capitol_x = (uint8_t)u;
  if (json_get_u64(o, "capitol_y", &u)) ind->capitol_y = (uint8_t)u;
  if (json_get_u64(o, "tech", &u)) ind->tech = (uint8_t)u;
  if (json_get_u64(o, "unknown31_lo_pad", &u)) ind->unknown31_lo_pad = (uint8_t)u;
  if (json_get_bool(o, "woi_defect_resolved", &b)) ind->woi_defect_resolved = b;
  if (json_get_bool(o, "woi_defect_forced", &b)) ind->woi_defect_forced = b;
  if (json_get_bool(o, "extinct", &b)) ind->extinct = b;
  if (json_get_u64(o, "unknown31b", &u)) ind->unknown31b_pad = (uint8_t)u;
  if (json_get_u64(o, "lands_bought", &u)) ind->lands_bought = (uint8_t)u;
  if (json_get_u64(o, "unknown31_flags", &u)) ind->unknown31_flags = (uint8_t)u;
  if (json_get_u64(o, "muskets", &u)) ind->muskets = (uint8_t)u;
  if (json_get_u64(o, "horse_herds", &u)) ind->horse_herds = (uint8_t)u;
  if (json_get_u64(o, "unknown31c", &u)) ind->unknown31c_pad = (uint8_t)u;
  if (json_get_u64(o, "horse_breeding", &u)) ind->horse_breeding = (uint16_t)u;
  if (json_get_i64(o, "hill_silver_bid_bonus", &i)) ind->hill_silver_bid_bonus = (int16_t)i;
  JsonValue* arr = json_obj_get(o, "tons");
  if (arr && arr->type == JV_ARR) {
    for (size_t k = 0; k < COLONIZE_COL1_CARGO_TYPES && k < json_arr_len(arr); ++k) {
      JsonValue* it = json_arr_at(arr, k);
      if (it && it->type == JV_NUM) ind->tons[k] = (int16_t)it->num;
    }
  }
  arr = json_obj_get(o, "contact_state");
  if (arr && arr->type == JV_ARR) {
    for (size_t k = 0; k < 4 && k < json_arr_len(arr); ++k) {
      JsonValue* it = json_arr_at(arr, k);
      if (it && it->type == JV_NUM) ind->contact_state[k] = (int16_t)it->num;
    }
  }
  arr = json_obj_get(o, "euro_relation_accum");
  if (arr && arr->type == JV_ARR) {
    for (size_t k = 0; k < 4 && k < json_arr_len(arr); ++k) {
      JsonValue* it = json_arr_at(arr, k);
      if (it && it->type == JV_NUM) ind->euro_relation_accum[k] = (int8_t)it->num;
    }
  }
  read_arr_u8(o, "euro_diplo", ind->euro_diplo, 4);
  read_arr_u8(o, "unknown33", ind->unknown33_pad, 8);
  arr = json_obj_get(o, "alarm_by_player");
  if (arr && arr->type == JV_ARR) {
    for (size_t k = 0; k < 4 && k < json_arr_len(arr); ++k) {
      JsonValue* it = json_arr_at(arr, k);
      if (it && it->type == JV_NUM) ind->alarm_by_player[k] = (uint16_t)it->num;
    }
  }
}

static void write_stuff(FILE* f, const ColonizeCol1Stuff* s) {
  int n = 0;
  fputc('{', f);
  wh(f, &n, "unknown34_pad_hex", s->unknown34_pad, sizeof s->unknown34_pad);
  W_U8ARR(f, &n, "all_unit_counts", s->all_unit_counts, 4);
  W_U8ARR(f, &n, "colony_counts", s->colony_counts, 4);
  W_U8ARR(f, &n, "free_colonist_counts", s->free_colonist_counts, 4);
  W_U8ARR(f, &n, "colony_pop_totals", s->colony_pop_totals, 4);
  W_U8ARR(f, &n, "census_pop_proxy", s->census_pop_proxy, 4);
  W_U8ARR(f, &n, "land_combat_totals", s->land_combat_totals, 4);
  W_U8ARR(f, &n, "ship_cargo_totals", s->ship_cargo_totals, 4);
  W_U8ARR(f, &n, "ship_counts", s->ship_counts, 4);
  W_U16ARR(f, &n, "land_combat_strength", s->land_combat_strength, 4);
  W_U8ARR(f, &n, "armed_ship_counts", s->armed_ship_counts, 4);
  W_U8ARR(f, &n, "veteran_teach_threshold", s->veteran_teach_threshold, 4);
  W_U8ARR(f, &n, "field_combat_totals", s->field_combat_totals, 4);
  jkey(f, &n, "unit_type_counts");
  fputc('[', f);
  for (int i = 0; i < 4; ++i) {
    if (i) fputc(',', f);
    fputc('[', f);
    for (int j = 0; j < 19; ++j) {
      if (j) fputc(',', f);
      fprintf(f, "%u", s->unit_type_counts[i][j]);
    }
    fputc(']', f);
  }
  fputc(']', f);
  wh(f, &n, "unknown_ds_947e_hex", s->village_counts_by_continent, sizeof s->village_counts_by_continent);
  wh(f, &n, "unknown_ds_95f2_hex", s->unknown_ds_95f2, sizeof s->unknown_ds_95f2);
  wh(f, &n, "unknown_ds_94a6_hex", s->land_unit_counts_by_continent, sizeof s->land_unit_counts_by_continent);
  wh(f, &n, "unknown_ds_94e6_hex", s->unknown_ds_94e6, sizeof s->unknown_ds_94e6);
  wh(f, &n, "unknown_ds_95b2_hex", s->field_combat_strength_by_continent, sizeof s->field_combat_strength_by_continent);
  wh(f, &n, "unknown_ds_9526_hex", s->skilled_unit_counts_by_continent, sizeof s->skilled_unit_counts_by_continent);
  wh(f, &n, "unknown_ds_918c_hex", s->unit_value_sum_by_continent, sizeof s->unit_value_sum_by_continent);
  wh(f, &n, "unknown_ds_9572_hex", s->combat_value_sum_by_continent, sizeof s->combat_value_sum_by_continent);
  wh(f, &n, "unknown_ds_944e_hex", s->unknown_ds_944e, sizeof s->unknown_ds_944e);
  wi(f, &n, "ui_toggle_336", s->ui_toggle_336);
  wh(f, &n, "tribe_data_9184_hex", s->tribe_data_9184, sizeof s->tribe_data_9184);
  wh(f, &n, "unknown_ds_9622_hex", s->tribe_population_totals, sizeof s->tribe_population_totals);
  wh(f, &n, "unknown_ds_962a_hex", s->tribe_village_counts, sizeof s->tribe_village_counts);
  wh(f, &n, "tribe_dwellings_91cc_hex", s->tribe_dwellings_91cc, sizeof s->tribe_dwellings_91cc);
  wi(f, &n, "x", s->x);
  wi(f, &n, "y", s->y);
  wi(f, &n, "zoom_level", s->zoom_level);
  wi(f, &n, "zoom_pad", s->zoom_pad);
  wi(f, &n, "viewport_x", s->viewport_x);
  wi(f, &n, "viewport_y", s->viewport_y);
  fputc('}', f);
}

static void read_stuff(const JsonValue* o, ColonizeCol1Stuff* s) {
  uint64_t u;
  json_get_hex(o, "unknown34_pad_hex", s->unknown34_pad, sizeof s->unknown34_pad);
  read_arr_u8(o, "all_unit_counts", s->all_unit_counts, 4);
  read_arr_u8(o, "colony_counts", s->colony_counts, 4);
  read_arr_u8(o, "free_colonist_counts", s->free_colonist_counts, 4);
  read_arr_u8(o, "colony_pop_totals", s->colony_pop_totals, 4);
  read_arr_u8(o, "census_pop_proxy", s->census_pop_proxy, 4);
  read_arr_u8(o, "land_combat_totals", s->land_combat_totals, 4);
  read_arr_u8(o, "ship_cargo_totals", s->ship_cargo_totals, 4);
  read_arr_u8(o, "ship_counts", s->ship_counts, 4);
  JsonValue* arr = json_obj_get(o, "land_combat_strength");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < 4 && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) s->land_combat_strength[i] = (uint16_t)it->num;
    }
  }
  read_arr_u8(o, "armed_ship_counts", s->armed_ship_counts, 4);
  read_arr_u8(o, "veteran_teach_threshold", s->veteran_teach_threshold, 4);
  read_arr_u8(o, "field_combat_totals", s->field_combat_totals, 4);
  JsonValue* utc = json_obj_get(o, "unit_type_counts");
  if (utc && utc->type == JV_ARR) {
    for (size_t i = 0; i < 4 && i < json_arr_len(utc); ++i) {
      JsonValue* row = json_arr_at(utc, i);
      if (row && row->type == JV_ARR) {
        for (size_t j = 0; j < 19 && j < json_arr_len(row); ++j) {
          JsonValue* it = json_arr_at(row, j);
          if (it && it->type == JV_NUM) s->unit_type_counts[i][j] = (uint8_t)it->num;
        }
      }
    }
  }
  json_get_hex(o, "unknown_ds_947e_hex", s->village_counts_by_continent, sizeof s->village_counts_by_continent);
  json_get_hex(o, "unknown_ds_95f2_hex", s->unknown_ds_95f2, sizeof s->unknown_ds_95f2);
  json_get_hex(o, "unknown_ds_94a6_hex", s->land_unit_counts_by_continent, sizeof s->land_unit_counts_by_continent);
  json_get_hex(o, "unknown_ds_94e6_hex", s->unknown_ds_94e6, sizeof s->unknown_ds_94e6);
  json_get_hex(o, "unknown_ds_95b2_hex", s->field_combat_strength_by_continent, sizeof s->field_combat_strength_by_continent);
  json_get_hex(o, "unknown_ds_9526_hex", s->skilled_unit_counts_by_continent, sizeof s->skilled_unit_counts_by_continent);
  json_get_hex(o, "unknown_ds_918c_hex", s->unit_value_sum_by_continent, sizeof s->unit_value_sum_by_continent);
  json_get_hex(o, "unknown_ds_9572_hex", s->combat_value_sum_by_continent, sizeof s->combat_value_sum_by_continent);
  json_get_hex(o, "unknown_ds_944e_hex", s->unknown_ds_944e, sizeof s->unknown_ds_944e);
  if (json_get_u64(o, "ui_toggle_336", &u)) s->ui_toggle_336 = (uint8_t)u;
  json_get_hex(o, "tribe_data_9184_hex", s->tribe_data_9184, sizeof s->tribe_data_9184);
  json_get_hex(o, "unknown_ds_9622_hex", s->tribe_population_totals, sizeof s->tribe_population_totals);
  json_get_hex(o, "unknown_ds_962a_hex", s->tribe_village_counts, sizeof s->tribe_village_counts);
  json_get_hex(o, "tribe_dwellings_91cc_hex", s->tribe_dwellings_91cc, sizeof s->tribe_dwellings_91cc);
  if (json_get_u64(o, "x", &u)) s->x = (uint16_t)u;
  if (json_get_u64(o, "y", &u)) s->y = (uint16_t)u;
  if (json_get_u64(o, "zoom_level", &u)) s->zoom_level = (uint8_t)u;
  if (json_get_u64(o, "zoom_pad", &u)) s->zoom_pad = (uint8_t)u;
  if (json_get_u64(o, "viewport_x", &u)) s->viewport_x = (uint16_t)u;
  if (json_get_u64(o, "viewport_y", &u)) s->viewport_y = (uint16_t)u;
}

static void write_post_map(FILE* f, const ColonizeCol1PostMap* p) {
  int n = 0;
  fputc('{', f);
  wh(f, &n, "sea_connectivity_hex", p->sea_connectivity, sizeof p->sea_connectivity);
  wh(f, &n, "land_connectivity_hex", p->land_connectivity, sizeof p->land_connectivity);
  W_U16ARR(f, &n, "continent_tally_a", p->continent_tally_a, 16);
  W_U16ARR(f, &n, "continent_tally_b", p->continent_tally_b, 16);
  wh(f, &n, "save_path_blob_hex", p->save_path_blob, sizeof p->save_path_blob);
  wu(f, &n, "boot_timer", p->boot_timer);
  wi(f, &n, "prime_resource_seed", p->prime_resource_seed);
  fputc('}', f);
}
static void read_post_map(const JsonValue* o, ColonizeCol1PostMap* p) {
  uint64_t u;
  json_get_hex(o, "sea_connectivity_hex", p->sea_connectivity, sizeof p->sea_connectivity);
  json_get_hex(o, "land_connectivity_hex", p->land_connectivity, sizeof p->land_connectivity);
  JsonValue* arr = json_obj_get(o, "continent_tally_a");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < 16 && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) p->continent_tally_a[i] = (uint16_t)it->num;
    }
  }
  arr = json_obj_get(o, "continent_tally_b");
  if (arr && arr->type == JV_ARR) {
    for (size_t i = 0; i < 16 && i < json_arr_len(arr); ++i) {
      JsonValue* it = json_arr_at(arr, i);
      if (it && it->type == JV_NUM) p->continent_tally_b[i] = (uint16_t)it->num;
    }
  }
  json_get_hex(o, "save_path_blob_hex", p->save_path_blob, sizeof p->save_path_blob);
  if (json_get_u64(o, "boot_timer", &u)) p->boot_timer = (uint32_t)u;
  if (json_get_u64(o, "prime_resource_seed", &u)) p->prime_resource_seed = (uint16_t)u;
}

static void write_trade_route(FILE* f, const ColonizeCol1TradeRoute* r) {
  int n = 0;
  fputc('{', f);
  ws(f, &n, "name", r->name, sizeof r->name);
  wi(f, &n, "sea", r->sea);
  wi(f, &n, "dest_count", r->dest_count);
  jkey(f, &n, "stops");
  fputc('[', f);
  for (int i = 0; i < 4; ++i) {
    if (i) fputc(',', f);
    const ColonizeCol1TradeStop* st = &r->stop[i];
    fprintf(f, "{\"colony_index\":%u,", st->colony_index);
    fprintf(f, "\"unload_count\":%u,\"load_count\":%u,", st->unload_count, st->load_count);
    fputs("\"unload_cargo\":[", f);
    for (int k = 0; k < 6; ++k) {
      if (k) fputc(',', f);
      fprintf(f, "%d", col1_trade_nibble_cargo(st->unload_cargo_nibbles, k));
    }
    fputs("],\"load_cargo\":[", f);
    for (int k = 0; k < 6; ++k) {
      if (k) fputc(',', f);
      fprintf(f, "%d", col1_trade_nibble_cargo(st->load_cargo_nibbles, k));
    }
    fprintf(f, "],\"pad\":%u}", st->pad);
  }
  fputc(']', f);
  fputc('}', f);
}
static void read_trade_route(const JsonValue* o, ColonizeCol1TradeRoute* r) {
  json_get_cstr_fixed(o, "name", r->name, sizeof r->name);
  uint64_t u;
  if (json_get_u64(o, "sea", &u)) r->sea = (uint8_t)u;
  if (json_get_u64(o, "dest_count", &u)) r->dest_count = (uint8_t)u;
  JsonValue* stops = json_obj_get(o, "stops");
  if (stops && stops->type == JV_ARR) {
    for (size_t i = 0; i < 4 && i < json_arr_len(stops); ++i) {
      JsonValue* st = json_arr_at(stops, i);
      ColonizeCol1TradeStop* out = &r->stop[i];
      if (json_get_u64(st, "colony_index", &u)) out->colony_index = (uint16_t)u;
      if (json_get_u64(st, "unload_count", &u)) out->unload_count = (uint8_t)u;
      if (json_get_u64(st, "load_count", &u)) out->load_count = (uint8_t)u;
      if (json_get_u64(st, "pad", &u)) out->pad = (uint8_t)u;
      JsonValue* uc = json_obj_get(st, "unload_cargo");
      if (uc && uc->type == JV_ARR) {
        for (int k = 0; k < 6 && (size_t)k < json_arr_len(uc); ++k) {
          JsonValue* it = json_arr_at(uc, (size_t)k);
          if (it && it->type == JV_NUM)
            col1_trade_nibble_set(out->unload_cargo_nibbles, k, (int)it->num);
        }
      }
      JsonValue* lc = json_obj_get(st, "load_cargo");
      if (lc && lc->type == JV_ARR) {
        for (int k = 0; k < 6 && (size_t)k < json_arr_len(lc); ++k) {
          JsonValue* it = json_arr_at(lc, (size_t)k);
          if (it && it->type == JV_NUM)
            col1_trade_nibble_set(out->load_cargo_nibbles, k, (int)it->num);
        }
      }
    }
  }
}

void col1_write_json(FILE* f, const ColonizeCol1Save* s) {
  int n = 0;
  fputc('{', f);
  jkey(f, &n, "head");
  write_head(f, &s->head);
  jkey(f, &n, "players");
  fputc('[', f);
  for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT; ++i) {
    if (i) fputc(',', f);
    write_player(f, &s->player[i]);
  }
  fputc(']', f);
  wh(f, &n, "other_hex", s->other, sizeof s->other);
  jkey(f, &n, "colonies");
  fputc('[', f);
  for (unsigned i = 0; i < s->head.colony_count; ++i) {
    if (i) fputc(',', f);
    write_colony(f, &s->colony[i]);
  }
  fputc(']', f);
  jkey(f, &n, "units");
  fputc('[', f);
  for (unsigned i = 0; i < s->head.unit_count; ++i) {
    if (i) fputc(',', f);
    write_unit(f, &s->unit[i]);
  }
  fputc(']', f);
  jkey(f, &n, "nations");
  fputc('[', f);
  for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT; ++i) {
    if (i) fputc(',', f);
    write_nation(f, &s->nation[i]);
  }
  fputc(']', f);
  jkey(f, &n, "tribes");
  fputc('[', f);
  for (unsigned i = 0; i < s->head.tribe_count; ++i) {
    if (i) fputc(',', f);
    write_tribe(f, &s->tribe[i]);
  }
  fputc(']', f);
  jkey(f, &n, "indians");
  fputc('[', f);
  for (int i = 0; i < (int)COLONIZE_COL1_INDIAN_COUNT; ++i) {
    if (i) fputc(',', f);
    write_indian(f, &s->indian[i]);
  }
  fputc(']', f);
  jkey(f, &n, "stuff");
  write_stuff(f, &s->stuff);
  jkey(f, &n, "map");
  {
    int m = 0;
    fputc('{', f);
    wi(f, &m, "width", s->map.width);
    wi(f, &m, "height", s->map.height);
    wh(f, &m, "tile_hex", s->map.tile, s->map.tile_count);
    wh(f, &m, "mask_hex", s->map.mask, s->map.tile_count);
    wh(f, &m, "path_hex", s->map.path, s->map.tile_count);
    wh(f, &m, "seen_hex", s->map.seen, s->map.tile_count);
    fputc('}', f);
  }
  jkey(f, &n, "post_map");
  write_post_map(f, &s->post_map);
  jkey(f, &n, "trade_routes");
  fputc('[', f);
  for (int i = 0; i < (int)COLONIZE_COL1_TRADE_ROUTE_COUNT; ++i) {
    if (i) fputc(',', f);
    write_trade_route(f, &s->trade_route[i]);
  }
  fputc(']', f);
  fputc('}', f);
  fputc('\n', f);
}

bool col1_read_json(const JsonValue* root, ColonizeCol1Save* out, char* err, size_t err_size) {
  if (!root || root->type != JV_OBJ) {
    if (err && err_size) snprintf(err, err_size, "top-level JSON value must be an object");
    return false;
  }
  col1_save_free(out);
  col1_save_init(out);

  JsonValue* head = json_obj_get(root, "head");
  if (!head) {
    if (err && err_size) snprintf(err, err_size, "missing \"head\" object");
    return false;
  }
  read_head(head, &out->head);
  if (out->head.map_size_x == 0 || out->head.map_size_y == 0) {
    if (err && err_size) snprintf(err, err_size, "head.map_size_x/map_size_y must be nonzero");
    return false;
  }

  if (!col1_save_alloc_sections(out, err, err_size)) {
    return false;
  }

  JsonValue* players = json_obj_get(root, "players");
  if (players && players->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_NATION_COUNT && i < json_arr_len(players); ++i) {
      read_player(json_arr_at(players, i), &out->player[i]);
    }
  }
  json_get_hex(root, "other_hex", out->other, sizeof out->other);

  JsonValue* colonies = json_obj_get(root, "colonies");
  if (colonies && colonies->type == JV_ARR) {
    size_t cnt = json_arr_len(colonies);
    if (cnt != out->head.colony_count) {
      if (err && err_size)
        snprintf(err, err_size, "colonies array length %zu != head.colony_count %u", cnt, out->head.colony_count);
      return false;
    }
    for (size_t i = 0; i < cnt; ++i) {
      read_colony(json_arr_at(colonies, i), &out->colony[i]);
    }
  } else if (out->head.colony_count > 0) {
    if (err && err_size) snprintf(err, err_size, "missing \"colonies\" array");
    return false;
  }

  JsonValue* units = json_obj_get(root, "units");
  if (units && units->type == JV_ARR) {
    size_t cnt = json_arr_len(units);
    if (cnt != out->head.unit_count) {
      if (err && err_size)
        snprintf(err, err_size, "units array length %zu != head.unit_count %u", cnt, out->head.unit_count);
      return false;
    }
    for (size_t i = 0; i < cnt; ++i) {
      read_unit(json_arr_at(units, i), &out->unit[i]);
    }
  } else if (out->head.unit_count > 0) {
    if (err && err_size) snprintf(err, err_size, "missing \"units\" array");
    return false;
  }

  JsonValue* nations = json_obj_get(root, "nations");
  if (nations && nations->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_NATION_COUNT && i < json_arr_len(nations); ++i) {
      read_nation(json_arr_at(nations, i), &out->nation[i]);
    }
  }

  JsonValue* tribes = json_obj_get(root, "tribes");
  if (tribes && tribes->type == JV_ARR) {
    size_t cnt = json_arr_len(tribes);
    if (cnt != out->head.tribe_count) {
      if (err && err_size)
        snprintf(err, err_size, "tribes array length %zu != head.tribe_count %u", cnt, out->head.tribe_count);
      return false;
    }
    for (size_t i = 0; i < cnt; ++i) {
      read_tribe(json_arr_at(tribes, i), &out->tribe[i]);
    }
  } else if (out->head.tribe_count > 0) {
    if (err && err_size) snprintf(err, err_size, "missing \"tribes\" array");
    return false;
  }

  JsonValue* indians = json_obj_get(root, "indians");
  if (indians && indians->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_INDIAN_COUNT && i < json_arr_len(indians); ++i) {
      read_indian(json_arr_at(indians, i), &out->indian[i]);
    }
  }

  JsonValue* stuff = json_obj_get(root, "stuff");
  if (stuff) {
    read_stuff(stuff, &out->stuff);
  }

  JsonValue* map = json_obj_get(root, "map");
  if (!map) {
    if (err && err_size) snprintf(err, err_size, "missing \"map\" object");
    return false;
  }
  if (!json_get_hex(map, "tile_hex", out->map.tile, out->map.tile_count) ||
      !json_get_hex(map, "mask_hex", out->map.mask, out->map.tile_count) ||
      !json_get_hex(map, "path_hex", out->map.path, out->map.tile_count) ||
      !json_get_hex(map, "seen_hex", out->map.seen, out->map.tile_count)) {
    if (err && err_size)
      snprintf(err, err_size, "map tile/mask/path/seen hex must each be %zu bytes (map %ux%u)",
        out->map.tile_count, out->head.map_size_x, out->head.map_size_y);
    return false;
  }

  JsonValue* post_map = json_obj_get(root, "post_map");
  if (post_map) {
    read_post_map(post_map, &out->post_map);
  }

  JsonValue* routes = json_obj_get(root, "trade_routes");
  if (routes && routes->type == JV_ARR) {
    for (size_t i = 0; i < COLONIZE_COL1_TRADE_ROUTE_COUNT && i < json_arr_len(routes); ++i) {
      read_trade_route(json_arr_at(routes, i), &out->trade_route[i]);
    }
  }

  if (err && err_size) err[0] = '\0';
  return true;
}
