#ifndef COLONIZE_SETTINGS_H
#define COLONIZE_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/col1_save.h"
#include "core/sound.h"

/*
 * Port-only preference store (settings.json next to the executable).
 *
 * DOS Colonization has no options file: FUN_75c2_235c hard-codes the new-game
 * words (DS:0x5382 = 0xc600, DS:0x5384 = 0, DS:0x5386 = 0x0e) and the only
 * persistence is whatever ends up inside a COLONY0n.SAV. This module keeps
 * the same option bits but adds a preference file so a fresh game and a fresh
 * process start from the player's last choice instead of the shipped one.
 *
 * Authority, deliberately split:
 *   - A loaded save wins. Options the player set during that game ride in its
 *     head and are restored with it; settings.json is not applied on Load.
 *   - settings.json seeds a New Game (over the DOS word) and the audio mixer
 *     at startup, and is rewritten whenever an options dialog is confirmed.
 *   - With no file loaded (every test and golden harness — they never call
 *     settings_init), nothing here is consulted and the DOS defaults stand.
 *
 * Only user-facing options live here. The DS:0x5382 low bits (woi,
 * ref_present, the independence latches) and cheats_enabled are game state,
 * not preferences, and are never read or written by this module.
 */

/* Bumped when a field's meaning changes; unknown versions load best-effort. */
#define COLONIZE_SETTINGS_VERSION 1

/*
 * Everything here is stored in the player-facing sense: true means the thing
 * happens. Several DOS bits are inverted suppress flags (water cycling, all
 * ten colony reports); settings_apply_to_head does the flipping.
 */
typedef struct ColonizeSettings {
  /* @GAMEOPTIONS. */
  bool show_indian_moves;
  bool show_foreign_moves;
  bool fast_piece_slide;
  bool end_of_turn;
  bool autosave;
  bool combat_analysis;
  bool water_color_cycling;
  bool tutorial_hints;

  /* @COLONYOPTIONS — true = report shown (DOS stores the suppress bit). */
  bool labels_on_buildings;
  bool labels_on_cargo_and_terrain;
  bool report_when_colonists_trained;
  bool report_food_shortages;
  bool report_raw_materials_shortages;
  bool report_tools_needed_for_production;
  bool report_inefficient_government;
  bool report_new_cargos_available;
  bool report_sons_of_liberty_membership;
  bool report_rebel_majorities;

  /* @SOUNDOPTIONS. */
  bool background_music;
  bool event_music;
  bool sound_effects;

  /* Port-only launch prefs. CLI flags still win when given; a missing or
   * invalid settings.json key keeps the hardcoded default. */
  bool windowed;
  int window_scale; /* 1..8 */
  bool no_sound;
  char data_dir[512];
  char save_dir[512]; /* empty = platform default (<exe>/COLONIZE) */
  uint32_t seed;      /* campaign RNG; used only when seed_present */
  bool seed_present;  /* JSON number (including 0); null/omitted = unset */
  bool debug_menu;         /* show DEBUG pulldown on the map navbar */
  bool show_mouse_coords;  /* pixel HUD follows the pointer */
  bool show_building_rects; /* colony-screen building sprite outlines */
} ColonizeSettings;

/* DOS new-game state (0x5382=0xc600, 0x5384=0, 0x5386=0x0e) plus port
 * launch defaults (windowed / scale 2 / ./COLONIZE). seed writes as null
 * until a file actually sets a number. */
void settings_defaults(ColonizeSettings* out);

/*
 * Process-wide singleton. settings_init resolves the file path (exe dir +
 * "/settings.json" unless `path` is given) and loads it, writing a defaults
 * file when none exists yet. Returns false only when a file exists but could
 * not be parsed — the settings are left at defaults in that case, the bad
 * file is left untouched, and the game still starts.
 *
 * settings_is_loaded reports whether settings_init ran at all. Callers that
 * must stay DOS-faithful when no preference file is in play (the New Game
 * path) gate on it, which is what keeps the goldens golden.
 */
bool settings_init(const char* path, char* err, size_t err_size);
bool settings_is_loaded(void);
const ColonizeSettings* settings_get(void);
const char* settings_path(void);

/* Replace the live settings; does not touch disk (call settings_flush). */
void settings_set(const ColonizeSettings* in);

/* Write the current settings to settings_path(). Atomic (temp file + rename). */
bool settings_flush(char* err, size_t err_size);

/* File-level helpers, usable without the singleton (tests). */
bool settings_load_file(const char* path, ColonizeSettings* out, char* err, size_t err_size);
bool settings_save_file(const char* path, const ColonizeSettings* in, char* err, size_t err_size);

/*
 * Bridge to the Col1 head bitfields. Only the option words are touched;
 * the WoI/REF latches and cheats_enabled in game_options are preserved.
 */
void settings_apply_to_head(const ColonizeSettings* s, ColonizeCol1Head* head);
void settings_capture_from_head(ColonizeSettings* s, const ColonizeCol1Head* head);

/* Bridge to the audio mixer options. */
ColonizeSoundOptions settings_sound_options(const ColonizeSettings* s);

#endif /* COLONIZE_SETTINGS_H */
