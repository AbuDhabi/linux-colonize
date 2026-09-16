#include "core/new_game.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/fb.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/strutil.h"
#include "core/ui_colors.h"
#include "core/turn.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

#define NEW_GAME_SAIL_FRAME_MS 1600u

static const char* k_nation_ss[4] = {"ENGLND1.SS", "FRANCE1.SS", "SPAIN1.SS", "DUTCH1.SS"};

typedef struct NewGameRect {
  int x;
  int y;
  int w;
  int h;
} NewGameRect;

/* DIFFICUL.PIK: 3×2 grid with top-left empty (Discoverer…Viceroy). */
static const NewGameRect k_difficul_rects[5] = {
  {129, 8, 66, 88},
  {234, 8, 66, 88},
  {24, 104, 66, 88},
  {129, 104, 66, 88},
  {234, 104, 66, 88},
};

/* NATIONS.PIK: 2×2 flags on the right. */
static const NewGameRect k_nation_rects[4] = {
  {112, 14, 86, 84},
  {212, 14, 86, 84},
  {112, 104, 86, 84},
  {212, 104, 86, 84},
};

/* CUSTOMIZ.PIK: 4 columns (params) × 3 rows (values). FUN_733a_0000. */
static const NewGameRect k_customiz_rects[4][3] = {
  {{10, 16, 72, 48}, {10, 76, 72, 48}, {10, 135, 72, 48}},
  {{86, 16, 72, 48}, {86, 76, 72, 48}, {86, 135, 72, 48}},
  {{162, 16, 72, 48}, {162, 76, 72, 48}, {162, 135, 72, 48}},
  {{238, 16, 72, 48}, {238, 76, 72, 48}, {238, 135, 72, 48}},
};

static const char* k_difficul_levels[5] = {
  "Easiest", "Easy", "Moderate", "Tough", "Toughest"
};
/* FUN_733a_0512 local_5a: Discoverer..Viceroy palette indices. */
static const uint8_t k_difficul_colors[5] = {10, 9, 14, 13, 12};
static const char* k_nation_bonuses[4] = {
  "Immigration", "Cooperation", "Conquest", "Trade"
};
static const char* k_finished_label = "(Click Here When Finished)";
static const char* k_customiz_title = "CUSTOMIZE NEW WORLD";
static const char* k_customiz_cats[4] = {"Land Mass", "Land Form", "Temperature", "Climate"};
static const char* k_customiz_vals[4][3] = {
  {"Small", "Moderate", "Large"},
  {"Archipelago", "Normal", "Continents"},
  {"Cool", "Temperate", "Warm"},
  {"Arid", "Normal", "Wet"},
};

/*
 * The wizard's country / home-port / difficulty labels all come from the one
 * NAMES.TXT-backed accessor set in reports.c (@COUNTRY, @HOMEPORT,
 * @DIFFICULTY), so a renamed or translated NAMES.TXT reaches this screen too.
 * The literal tables that used to live here are gone: the home-port one said
 * "Paris" for France, while NAMES.TXT @HOMEPORT (and europe.c/units.c) say
 * "La Rochelle" (audit SC-7). Out-of-range nations keep resolving to the
 * English row, as the old local tables did.
 */
const char* new_game_nation_name(int nation) {
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  return reports_nation_country_name(nation);
}

static const char* new_game_nation_port(int nation) {
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  return reports_home_port_name(nation);
}

static const char* new_game_nation_ruler_title(int nation) {
  return (nation == 3) ? "Stadtholder" : "King";
}

void new_game_init(NewGameWizard* ng) {
  if (!ng) {
    return;
  }
  memset(ng, 0, sizeof(*ng));
  ng->dialog_width = 190;
  ng->pref_dialog_y = -1;
  snprintf(ng->map_file, sizeof(ng->map_file), "AMER2.MP");
}

static void new_game_free_assets(NewGameWizard* ng) {
  if (!ng) {
    return;
  }
  if (ng->difficul_ok) {
    pik_free(&ng->difficul_pik);
    ng->difficul_ok = false;
  }
  if (ng->nations_ok) {
    pik_free(&ng->nations_pik);
    ng->nations_ok = false;
  }
  if (ng->customiz_ok) {
    pik_free(&ng->customiz_pik);
    ng->customiz_ok = false;
  }
  if (ng->kinglss_ok) {
    pik_free(&ng->kinglss_pik);
    ng->kinglss_ok = false;
  }
  if (ng->king1_ok) {
    ss_free(&ng->king1);
    ng->king1_ok = false;
  }
  if (ng->nation_art_ok) {
    ss_free(&ng->nation_art);
    ng->nation_art_ok = false;
  }
  if (ng->fontking_ok) {
    ff_free(&ng->fontking);
    ng->fontking_ok = false;
  }
  if (ng->endking_ok) {
    ss_free(&ng->endking);
    ng->endking_ok = false;
    ng->endking_name[0] = '\0';
  }
  for (int i = 0; i < NEW_GAME_SAIL_FRAMES; ++i) {
    if (ng->levn_ok[i]) {
      pik_free(&ng->levn[i]);
      ng->levn_ok[i] = false;
    }
  }
}

void new_game_free(NewGameWizard* ng) {
  if (!ng) {
    return;
  }
  new_game_free_assets(ng);
  memset(ng, 0, sizeof(*ng));
}

bool new_game_active(const NewGameWizard* ng) {
  return ng && ng->phase != NEW_GAME_PHASE_IDLE;
}

bool new_game_wants_commit(const NewGameWizard* ng) {
  return ng && ng->phase == NEW_GAME_PHASE_COMMIT;
}

void new_game_cancel(NewGameWizard* ng) {
  if (!ng) {
    return;
  }
  new_game_free_assets(ng);
  ng->phase = NEW_GAME_PHASE_IDLE;
  ng->option_count = 0;
  ng->selection = 0;
  ng->sail_frame = 0;
  ng->sail_accum_ms = 0;
}

static bool new_game_load_pik(NewGameWizard* ng, const char* name, ColonizePikImage* out, bool* ok) {
  char path[512];
  char err[256];
  if (*ok) {
    return true;
  }
  if (!dos_compat_normalize_asset_path(ng->data_dir, name, path, sizeof(path))) {
    return false;
  }
  if (!pik_load(path, out, err, sizeof(err))) {
    diag_warn("new_game: failed to load %s: %s", name, err);
    return false;
  }
  *ok = true;
  return true;
}

static bool new_game_load_ss(NewGameWizard* ng, const char* name, ColonizeSpriteSheet* out, bool* ok) {
  char path[512];
  char err[256];
  if (*ok) {
    return true;
  }
  if (!dos_compat_normalize_asset_path(ng->data_dir, name, path, sizeof(path))) {
    return false;
  }
  if (!ss_load(path, out, err, sizeof(err))) {
    diag_warn("new_game: failed to load %s: %s", name, err);
    return false;
  }
  *ok = true;
  return true;
}

static void new_game_ensure_difficul(NewGameWizard* ng) {
  new_game_load_pik(ng, "DIFFICUL.PIK", &ng->difficul_pik, &ng->difficul_ok);
}

static void new_game_ensure_nations(NewGameWizard* ng) {
  new_game_load_pik(ng, "NATIONS.PIK", &ng->nations_pik, &ng->nations_ok);
}

static void new_game_ensure_customiz(NewGameWizard* ng) {
  new_game_load_pik(ng, "CUSTOMIZ.PIK", &ng->customiz_pik, &ng->customiz_ok);
}

/* KINGLSS1.PIK is the throne-room backdrop for both the wizard audience and
 * the endgame one (audit SC-42: the load was typed out at both sites). */
static void new_game_ensure_kinglss(NewGameWizard* ng) {
  new_game_load_pik(ng, "KINGLSS1.PIK", &ng->kinglss_pik, &ng->kinglss_ok);
}

static void new_game_ensure_king(NewGameWizard* ng) {
  new_game_ensure_kinglss(ng);
  new_game_load_ss(ng, "KING1.SS", &ng->king1, &ng->king1_ok);
  if (ng->nation_art_ok) {
    ss_free(&ng->nation_art);
    ng->nation_art_ok = false;
  }
  new_game_load_ss(ng, k_nation_ss[ng->nation < 0 || ng->nation > 3 ? 0 : ng->nation], &ng->nation_art, &ng->nation_art_ok);
  if (!ng->fontking_ok) {
    char path[512];
    char err[256];
    if (dos_compat_normalize_asset_path(ng->data_dir, "FONTKING.FF", path, sizeof(path)) &&
        ff_load(path, &ng->fontking, err, sizeof(err))) {
      ng->fontking_ok = true;
    } else {
      diag_warn("new_game: FONTKING.FF missing");
    }
  }
}

static void new_game_ensure_sail_frame(NewGameWizard* ng, int frame) {
  if (frame < 0 || frame >= NEW_GAME_SAIL_FRAMES || ng->levn_ok[frame]) {
    return;
  }
  char name[32];
  snprintf(name, sizeof(name), "LEVN%04d.PIK", frame + 1);
  new_game_load_pik(ng, name, &ng->levn[frame], &ng->levn_ok[frame]);
}

static bool new_game_is_directive(const char* line) {
  return line && line[0] == '@';
}

static void new_game_clear_list(NewGameWizard* ng) {
  ng->option_count = 0;
  ng->selection = 0;
  ng->prompt_line_count = 0;
  ng->dialog_width = 190;
  ng->pref_dialog_y = -1;
  memset(ng->options, 0, sizeof(ng->options));
  memset(ng->prompt_lines, 0, sizeof(ng->prompt_lines));
}

static bool new_game_line_is_filler(const char* line) {
  if (!line || line[0] == '\0') {
    return true;
  }
  for (const char* p = line; *p; ++p) {
    if (*p != '_' && *p != ' ' && *p != '\t' && *p != '^') {
      return false;
    }
  }
  return true;
}

static void new_game_load_choice_section(NewGameWizard* ng, const char* section_name) {
  new_game_clear_list(ng);
  if (!ng->game_txt) {
    return;
  }
  const ColonizeMsgSection* section = assets_msg_find(ng->game_txt, section_name);
  if (!section) {
    diag_warn("new_game: missing @%s", section_name);
    return;
  }

  int options_at = -1;
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (!line) {
      continue;
    }
    if (strcmp(line, "@options") == 0) {
      options_at = i;
      continue;
    }
    if (new_game_is_directive(line)) {
      if (strncmp(line, "@width=", 7) == 0) {
        ng->dialog_width = atoi(line + 7);
        if (ng->dialog_width < 80) {
          ng->dialog_width = 80;
        }
        if (ng->dialog_width > 320) {
          ng->dialog_width = 320;
        }
      } else if (strncmp(line, "@y=", 3) == 0) {
        ng->pref_dialog_y = atoi(line + 3);
      } else if (strncmp(line, "@default=", 9) == 0 && isdigit((unsigned char)line[9])) {
        ng->selection = atoi(line + 9) - 1;
        if (ng->selection < 0) {
          ng->selection = 0;
        }
      }
    }
  }

  if (options_at >= 0) {
    for (int i = 0; i < section->line_count; ++i) {
      const char* line = section->lines[i];
      if (!line || line[0] == '\0' || new_game_is_directive(line) || new_game_line_is_filler(line)) {
        continue;
      }
      if (i < options_at) {
        if (ng->prompt_line_count < 8) {
          str_copy_trunc(ng->prompt_lines[ng->prompt_line_count], sizeof(ng->prompt_lines[0]), line);
          ng->prompt_line_count++;
        }
      } else if (i > options_at && ng->option_count < NEW_GAME_OPTION_MAX) {
        str_copy_trunc(ng->options[ng->option_count], sizeof(ng->options[0]), line);
        ng->option_count++;
      }
    }
  } else {
    enum { NEW_GAME_CONTENT_MAX = 64 };
    const char* content[NEW_GAME_CONTENT_MAX];
    int content_count = 0;
    for (int i = 0; i < section->line_count; ++i) {
      const char* line = section->lines[i];
      if (!line || line[0] == '\0' || new_game_is_directive(line) || new_game_line_is_filler(line)) {
        continue;
      }
      if (content_count < NEW_GAME_CONTENT_MAX) {
        content[content_count++] = line;
      }
    }
    int split = 1;
    if (strcmp(section_name, "AMERICA") == 0) {
      split = 0;
      for (int i = 0; i < content_count; ++i) {
        split = i + 1;
        if (strchr(content[i], '?') != NULL) {
          break;
        }
      }
    } else if (content_count == 0) {
      split = 0;
    }
    for (int i = 0; i < split && i < content_count && ng->prompt_line_count < 8; ++i) {
      str_copy_trunc(ng->prompt_lines[ng->prompt_line_count], sizeof(ng->prompt_lines[0]), content[i]);
      ng->prompt_line_count++;
    }
    for (int i = split; i < content_count && ng->option_count < NEW_GAME_OPTION_MAX; ++i) {
      str_copy_trunc(ng->options[ng->option_count], sizeof(ng->options[0]), content[i]);
      ng->option_count++;
    }
  }

  if (ng->selection >= ng->option_count) {
    ng->selection = 0;
  }
}

void new_game_default_leader_name(
  const ColonizeMsgCatalog* names_txt, int nation, char* out, size_t out_size
) {
  static const char* defaults[4] = {
    "Walter Raleigh", "Jacques Cartier", "Christopher Columbus", "Michiel De Ruyter"
  };
  const int n_idx = (nation < 0 || nation > 3) ? 0 : nation;
  if (names_txt) {
    const ColonizeMsgSection* section = assets_msg_find(names_txt, "LEADERNAME");
    if (section) {
      int idx = 0;
      for (int i = 0; i < section->line_count; ++i) {
        const char* line = section->lines[i];
        if (!line || line[0] == '\0' || line[0] == ';' || line[0] == '@') {
          continue;
        }
        if (idx == n_idx) {
          char buf[NEW_GAME_LEADER_NAME_MAX];
          str_copy_trunc(buf, sizeof(buf), line);
          char* comma = strchr(buf, ',');
          if (comma) {
            *comma = '\0';
          }
          /* trim trailing spaces */
          size_t n = strlen(buf);
          while (n > 0 && (buf[n - 1] == ' ' || buf[n - 1] == '\t')) {
            buf[--n] = '\0';
          }
          if (buf[0]) {
            str_copy_trunc(out, out_size, buf);
            return;
          }
        }
        idx++;
      }
    }
  }
  str_copy_trunc(out, out_size, defaults[n_idx]);
}

static void new_game_seed_leader_name(NewGameWizard* ng) {
  new_game_default_leader_name(
    ng->names_txt, ng->nation, ng->leader_name, sizeof(ng->leader_name)
  );
}

static void new_game_enter_difficulty(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_DIFFICULTY;
  new_game_ensure_difficul(ng);
  new_game_load_choice_section(ng, "DIFFICULTY");
  if (ng->option_count == 0) {
    snprintf(ng->prompt_lines[0], sizeof(ng->prompt_lines[0]), "Select a Difficulty Level");
    ng->prompt_line_count = 1;
    for (int i = 0; i < 5; ++i) {
      snprintf(ng->options[i], sizeof(ng->options[0]), "%s", reports_difficulty_title(i));
    }
    ng->option_count = 5;
  }
}

static int* new_game_gen_param_axis(MapGenParams* p, int col) {
  if (!p) {
    return NULL;
  }
  switch (col) {
    case 0:
      return &p->land_mass;
    case 1:
      return &p->land_form;
    case 2:
      return &p->temperature;
    case 3:
      return &p->climate;
    default:
      return NULL;
  }
}

static int new_game_gen_param_value(const MapGenParams* p, int col) {
  if (!p) {
    return 1;
  }
  int v = 1;
  switch (col) {
    case 0:
      v = p->land_mass;
      break;
    case 1:
      v = p->land_form;
      break;
    case 2:
      v = p->temperature;
      break;
    case 3:
      v = p->climate;
      break;
    default:
      break;
  }
  if (v < 0) {
    return 0;
  }
  if (v > 2) {
    return 2;
  }
  return v;
}

static void new_game_set_gen_param_value(MapGenParams* p, int col, int value) {
  int* axis = new_game_gen_param_axis(p, col);
  if (!axis) {
    return;
  }
  if (value < 0) {
    value = 0;
  }
  if (value > 2) {
    value = 2;
  }
  *axis = value;
}

static void new_game_enter_customize(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_CUSTOMIZE;
  ng->customize_focus = 0;
  new_game_ensure_customiz(ng);
  new_game_clear_list(ng);
  snprintf(ng->prompt_lines[0], sizeof(ng->prompt_lines[0]), "%s", k_customiz_title);
  ng->prompt_line_count = 1;
}

static void new_game_enter_nation(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_NATION;
  new_game_ensure_nations(ng);
  new_game_load_choice_section(ng, "PICKNATION");
  if (ng->option_count == 0) {
    snprintf(ng->prompt_lines[0], sizeof(ng->prompt_lines[0]), "Select a European Power");
    ng->prompt_line_count = 1;
    for (int i = 0; i < 4; ++i) {
      snprintf(ng->options[i], sizeof(ng->options[0]), "%s", new_game_nation_name(i));
    }
    ng->option_count = 4;
  }
}

static void new_game_enter_leader_name(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_LEADER_NAME;
  new_game_seed_leader_name(ng);
  /* DOS opens the field with bit 0x80 set — whole text selected. */
  text_edit_reset(&ng->leader_edit, ng->leader_name, true);
  new_game_load_choice_section(ng, "LEADERNAME");
  if (ng->prompt_line_count == 0) {
    snprintf(ng->prompt_lines[0], sizeof(ng->prompt_lines[0]), "Please Enter Your Name.");
    ng->prompt_line_count = 1;
  }
  ng->option_count = 0; /* text field, not a list */
  ng->dialog_width = 300;
}

static void new_game_enter_lore(NewGameWizard* ng, bool page_b) {
  ng->phase = page_b ? NEW_GAME_PHASE_NATION_LORE_B : NEW_GAME_PHASE_NATION_LORE_A;
  new_game_ensure_nations(ng);
  new_game_clear_list(ng);
  ng->dialog_width = 300;
}

static void new_game_enter_king(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_KING;
  new_game_ensure_king(ng);
}

static void new_game_enter_sail(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_SAIL;
  ng->sail_frame = 0;
  ng->sail_accum_ms = 0;
  new_game_ensure_sail_frame(ng, 0);
}

static void new_game_scan_mp_files(NewGameWizard* ng) {
  new_game_clear_list(ng);
  snprintf(ng->prompt_lines[0], sizeof(ng->prompt_lines[0]), "Select Map File to Load");
  ng->prompt_line_count = 1;
  ng->dialog_width = 220;

  DIR* dir = opendir(ng->data_dir);
  if (!dir) {
    snprintf(ng->options[0], sizeof(ng->options[0]), "AMER2.MP");
    ng->option_count = 1;
    return;
  }
  struct dirent* ent;
  while ((ent = readdir(dir)) != NULL && ng->option_count < NEW_GAME_OPTION_MAX) {
    const char* name = ent->d_name;
    size_t n = strlen(name);
    if (n < 4) {
      continue;
    }
    if (strcasecmp(name + n - 3, ".MP") != 0) {
      continue;
    }
    str_copy_trunc(ng->options[ng->option_count], sizeof(ng->options[0]), name);
    ng->option_count++;
  }
  closedir(dir);
  if (ng->option_count == 0) {
    str_copy_trunc(ng->options[0], sizeof(ng->options[0]), "AMER2.MP");
    ng->option_count = 1;
  }
}

bool new_game_begin(
  NewGameWizard* ng,
  NewGamePath path,
  const char* data_dir,
  const ColonizeMsgCatalog* game_txt,
  const ColonizeMsgCatalog* names_txt
) {
  if (!ng || !data_dir) {
    return false;
  }
  new_game_free_assets(ng);
  ng->phase = NEW_GAME_PHASE_IDLE;
  ng->path = path;
  ng->game_txt = game_txt;
  ng->names_txt = names_txt;
  ng->difficulty = 0;
  ng->nation = 0;
  ng->leader_name[0] = '\0';
  ng->generate_map = false;
  memset(&ng->gen_params, 0, sizeof(ng->gen_params));
  ng->customize_focus = 0;
  snprintf(ng->map_file, sizeof(ng->map_file), "AMER2.MP");
  snprintf(ng->data_dir, sizeof(ng->data_dir), "%s", data_dir);
  ng->sail_frame = 0;
  ng->sail_accum_ms = 0;

  if (path == NEW_GAME_PATH_AMERICA) {
    ng->phase = NEW_GAME_PHASE_AMERICA_CHOICE;
    new_game_load_choice_section(ng, "AMERICA");
    /* GAME.TXT @width=160 is tight for FONTINTR; widen so the prompt wraps to 3 lines. */
    if (ng->dialog_width < 180) {
      ng->dialog_width = 180;
    }
    if (ng->option_count == 0) {
      snprintf(ng->prompt_lines[0], sizeof(ng->prompt_lines[0]), "Original Americas or Map Editor?");
      ng->prompt_line_count = 1;
      snprintf(ng->options[0], sizeof(ng->options[0]), "Original Americas");
      snprintf(ng->options[1], sizeof(ng->options[1]), "Map Editor");
      ng->option_count = 2;
    }
  } else if (path == NEW_GAME_PATH_CUSTOMIZE) {
    /* DOS: all five words = 1; UI edits 0..3 only. */
    ng->generate_map = true;
    ng->map_file[0] = '\0';
    ng->gen_params.land_mass = 1;
    ng->gen_params.land_form = 1;
    ng->gen_params.temperature = 1;
    ng->gen_params.climate = 1;
    ng->gen_params.forest_extra = 1;
    ng->gen_params.seed = 0;
    new_game_enter_customize(ng);
  } else {
    /* NEW WORLD: procedural map (params randomized at commit if seed 0). */
    ng->generate_map = true;
    ng->map_file[0] = '\0';
    new_game_enter_difficulty(ng);
  }
  return true;
}

static void new_game_request_commit(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_COMMIT;
}

static void new_game_activate_list(NewGameWizard* ng) {
  if (ng->selection < 0 || ng->selection >= ng->option_count) {
    return;
  }
  switch (ng->phase) {
    case NEW_GAME_PHASE_AMERICA_CHOICE:
      if (ng->selection == 0) {
        snprintf(ng->map_file, sizeof(ng->map_file), "AMER2.MP");
        new_game_enter_difficulty(ng);
      } else {
        ng->phase = NEW_GAME_PHASE_MAP_PICK;
        new_game_scan_mp_files(ng);
      }
      break;
    case NEW_GAME_PHASE_MAP_PICK:
      snprintf(ng->map_file, sizeof(ng->map_file), "%s", ng->options[ng->selection]);
      new_game_enter_difficulty(ng);
      break;
    case NEW_GAME_PHASE_DIFFICULTY:
      ng->difficulty = ng->selection;
      new_game_enter_nation(ng);
      break;
    case NEW_GAME_PHASE_NATION:
      ng->nation = ng->selection;
      if (ng->nation < 0) {
        ng->nation = 0;
      }
      if (ng->nation > 3) {
        ng->nation = 3;
      }
      new_game_enter_leader_name(ng);
      break;
    default:
      break;
  }
}

static bool new_game_advance_cinematic(NewGameWizard* ng) {
  switch (ng->phase) {
    case NEW_GAME_PHASE_NATION_LORE_A:
      new_game_enter_lore(ng, true);
      return true;
    case NEW_GAME_PHASE_NATION_LORE_B:
      new_game_enter_king(ng);
      return true;
    case NEW_GAME_PHASE_KING:
      new_game_enter_sail(ng);
      return true;
    case NEW_GAME_PHASE_SAIL:
      new_game_request_commit(ng);
      return true;
    case NEW_GAME_PHASE_LEADER_NAME:
      if (ng->leader_name[0] == '\0') {
        new_game_seed_leader_name(ng);
      }
      new_game_enter_lore(ng, false);
      return true;
    default:
      return false;
  }
}

static int new_game_point_in_rects(const NewGameRect* rects, int count, int mx, int my) {
  for (int i = 0; i < count; ++i) {
    const NewGameRect* r = &rects[i];
    if (mx >= r->x && my >= r->y && mx < r->x + r->w && my < r->y + r->h) {
      return i;
    }
  }
  return -1;
}

static bool new_game_point_in_finished(const NewGameWizard* ng, int mx, int my) {
  if (!ng || ng->finished_w <= 0 || ng->finished_h <= 0) {
    return false;
  }
  return mx >= ng->finished_x && my >= ng->finished_y && mx < ng->finished_x + ng->finished_w &&
    my < ng->finished_y + ng->finished_h;
}

/* Keyboard navigation on DIFFICUL 3×2 (empty TL):
 *   . 0 1
 *   2 3 4
 */
static void new_game_difficul_nav(NewGameWizard* ng, ColonizeKey key) {
  int s = ng->selection;
  if (s < 0 || s > 4) {
    s = 0;
  }
  switch (key) {
    case COLONIZE_KEY_LEFT:
    case COLONIZE_KEY_KP4:
      if (s == 1) {
        s = 0;
      } else if (s == 3) {
        s = 2;
      } else if (s == 4) {
        s = 3;
      }
      break;
    case COLONIZE_KEY_RIGHT:
    case COLONIZE_KEY_KP6:
      if (s == 0) {
        s = 1;
      } else if (s == 2) {
        s = 3;
      } else if (s == 3) {
        s = 4;
      }
      break;
    case COLONIZE_KEY_UP:
    case COLONIZE_KEY_KP8:
      if (s == 2) {
        s = 0;
      } else if (s == 3) {
        s = 0;
      } else if (s == 4) {
        s = 1;
      }
      break;
    case COLONIZE_KEY_DOWN:
    case COLONIZE_KEY_KP2:
      if (s == 0) {
        s = 3;
      } else if (s == 1) {
        s = 4;
      }
      break;
    default:
      break;
  }
  ng->selection = s;
}

/* NATIONS 2×2:
 *   0 1
 *   2 3
 */
static void new_game_nation_nav(NewGameWizard* ng, ColonizeKey key) {
  int s = ng->selection;
  if (s < 0 || s > 3) {
    s = 0;
  }
  switch (key) {
    case COLONIZE_KEY_LEFT:
    case COLONIZE_KEY_KP4:
      if (s == 1 || s == 3) {
        s--;
      }
      break;
    case COLONIZE_KEY_RIGHT:
    case COLONIZE_KEY_KP6:
      if (s == 0 || s == 2) {
        s++;
      }
      break;
    case COLONIZE_KEY_UP:
    case COLONIZE_KEY_KP8:
      if (s >= 2) {
        s -= 2;
      }
      break;
    case COLONIZE_KEY_DOWN:
    case COLONIZE_KEY_KP2:
      if (s < 2) {
        s += 2;
      }
      break;
    default:
      break;
  }
  ng->selection = s;
}

static const char* new_game_finished_text(const NewGameWizard* ng) {
  if (ng && ng->labels_txt) {
    const ColonizeMsgSection* misc = assets_msg_find(ng->labels_txt, "MISC");
    if (misc) {
      for (int i = 0; i < misc->line_count; ++i) {
        if (misc->lines[i][0] && strstr(misc->lines[i], "Finished") != NULL) {
          return misc->lines[i];
        }
      }
    }
  }
  return k_finished_label;
}

/* Resolve CUSTOMIZE labels from @MISC starting at "Land Mass"; else English fallbacks. */
static void new_game_customiz_labels(
  const NewGameWizard* ng,
  const char** out_cats,
  const char** out_vals,
  const char** out_title
) {
  for (int c = 0; c < 4; ++c) {
    out_cats[c] = k_customiz_cats[c];
    for (int r = 0; r < 3; ++r) {
      out_vals[c * 3 + r] = k_customiz_vals[c][r];
    }
  }
  *out_title = k_customiz_title;
  if (!ng || !ng->labels_txt) {
    return;
  }
  const ColonizeMsgSection* misc = assets_msg_find(ng->labels_txt, "MISC");
  if (!misc) {
    return;
  }
  int start = -1;
  for (int i = 0; i < misc->line_count; ++i) {
    if (misc->lines[i][0] && strcmp(misc->lines[i], "Land Mass") == 0) {
      start = i;
      break;
    }
  }
  if (start < 0 || start + 16 >= misc->line_count) {
    return;
  }
  for (int c = 0; c < 4; ++c) {
    if (misc->lines[start + c][0]) {
      out_cats[c] = misc->lines[start + c];
    }
  }
  for (int i = 0; i < 12; ++i) {
    if (misc->lines[start + 4 + i][0]) {
      out_vals[i] = misc->lines[start + 4 + i];
    }
  }
  if (misc->lines[start + 16][0]) {
    *out_title = misc->lines[start + 16];
  }
}

static int new_game_point_in_customiz(int mx, int my, int* out_col, int* out_row) {
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 3; ++r) {
      const NewGameRect* rect = &k_customiz_rects[c][r];
      if (mx >= rect->x && my >= rect->y && mx < rect->x + rect->w && my < rect->y + rect->h) {
        if (out_col) {
          *out_col = c;
        }
        if (out_row) {
          *out_row = r;
        }
        return 1;
      }
    }
  }
  return 0;
}

bool new_game_handle_input(NewGameWizard* ng, const ColonizeInputState* input) {
  if (!ng || !input || !new_game_active(ng) || ng->phase == NEW_GAME_PHASE_COMMIT) {
    return false;
  }

  const bool pre_king = ng->phase == NEW_GAME_PHASE_AMERICA_CHOICE ||
    ng->phase == NEW_GAME_PHASE_MAP_PICK || ng->phase == NEW_GAME_PHASE_CUSTOMIZE ||
    ng->phase == NEW_GAME_PHASE_DIFFICULTY || ng->phase == NEW_GAME_PHASE_NATION ||
    ng->phase == NEW_GAME_PHASE_LEADER_NAME;

  if (input->last_key == COLONIZE_KEY_ESCAPE && pre_king) {
    new_game_cancel(ng);
    return true;
  }

  /* CUSTOMIZE: 4×3 param grid (FUN_733a_0270). */
  if (ng->phase == NEW_GAME_PHASE_CUSTOMIZE) {
    if (colonize_key_left(input->last_key) || input->last_key == COLONIZE_KEY_BACKSPACE) {
      ng->customize_focus = (ng->customize_focus + 3) % 4;
      return true;
    }
    if (colonize_key_right(input->last_key)) {
      ng->customize_focus = (ng->customize_focus + 1) % 4;
      return true;
    }
    if (colonize_key_up(input->last_key)) {
      int v = new_game_gen_param_value(&ng->gen_params, ng->customize_focus);
      new_game_set_gen_param_value(&ng->gen_params, ng->customize_focus, (v + 2) % 3);
      return true;
    }
    if (colonize_key_down(input->last_key) || input->last_key == COLONIZE_KEY_SPACE) {
      int v = new_game_gen_param_value(&ng->gen_params, ng->customize_focus);
      new_game_set_gen_param_value(&ng->gen_params, ng->customize_focus, (v + 1) % 3);
      return true;
    }
    if (input->last_key == COLONIZE_KEY_ENTER) {
      new_game_enter_difficulty(ng);
      return true;
    }
    if (input->mouse_left_clicked) {
      int col = 0, row = 0;
      if (new_game_point_in_customiz(input->mouse_x, input->mouse_y, &col, &row)) {
        ng->customize_focus = col;
        new_game_set_gen_param_value(&ng->gen_params, col, row);
        return true;
      }
      /* DOS: finished = full-width strip y > 184. */
      if (input->mouse_y > 184 || new_game_point_in_finished(ng, input->mouse_x, input->mouse_y)) {
        new_game_enter_difficulty(ng);
        return true;
      }
      return true;
    }
    return true;
  }

  /* Image-region pick: DIFFICULTY / NATION — no list popup. */
  if (ng->phase == NEW_GAME_PHASE_DIFFICULTY || ng->phase == NEW_GAME_PHASE_NATION) {
    const NewGameRect* rects =
      (ng->phase == NEW_GAME_PHASE_DIFFICULTY) ? k_difficul_rects : k_nation_rects;
    const int count = (ng->phase == NEW_GAME_PHASE_DIFFICULTY) ? 5 : 4;

    if (colonize_key_left(input->last_key) || colonize_key_right(input->last_key) ||
        colonize_key_up(input->last_key) || colonize_key_down(input->last_key)) {
      if (ng->phase == NEW_GAME_PHASE_DIFFICULTY) {
        new_game_difficul_nav(ng, input->last_key);
      } else {
        new_game_nation_nav(ng, input->last_key);
      }
      return true;
    }
    if (input->last_key == COLONIZE_KEY_ENTER) {
      new_game_activate_list(ng);
      return true;
    }
    if (input->mouse_left_clicked) {
      const int hit = new_game_point_in_rects(rects, count, input->mouse_x, input->mouse_y);
      if (hit >= 0) {
        /* bugs.md #283: clicking the already-selected entry confirms like
         * Enter; a click elsewhere still only moves the selection. */
        if (hit == ng->selection) {
          new_game_activate_list(ng);
        } else {
          ng->selection = hit;
        }
        return true;
      }
      if (new_game_point_in_finished(ng, input->mouse_x, input->mouse_y)) {
        new_game_activate_list(ng);
        return true;
      }
      return true;
    }
    return true;
  }

  /* Leader name text entry. */
  if (ng->phase == NEW_GAME_PHASE_LEADER_NAME) {
    if (text_edit_handle_mouse(
          &ng->leader_edit,
          ng->leader_name,
          ng->leader_field_font,
          input,
          ng->leader_field_x,
          ng->leader_field_y,
          ng->leader_field_h
        )) {
      return true;
    }
    switch (text_edit_handle_input(
      &ng->leader_edit, ng->leader_name, sizeof(ng->leader_name), input
    )) {
      case TEXT_EDIT_ACTION_CONFIRM:
        new_game_advance_cinematic(ng);
        return true;
      case TEXT_EDIT_ACTION_EDIT:
        return true;
      case TEXT_EDIT_ACTION_CANCEL:
      case TEXT_EDIT_ACTION_NONE:
        break;
    }
    if (input->mouse_left_clicked) {
      new_game_advance_cinematic(ng);
      return true;
    }
    return true;
  }

  /* Lore / king / sail: Enter or LMB advances (sail: skips to commit). */
  if (ng->phase == NEW_GAME_PHASE_NATION_LORE_A || ng->phase == NEW_GAME_PHASE_NATION_LORE_B ||
      ng->phase == NEW_GAME_PHASE_KING || ng->phase == NEW_GAME_PHASE_SAIL) {
    if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE ||
        input->mouse_left_clicked) {
      new_game_advance_cinematic(ng);
      return true;
    }
    return true;
  }

  /* Remaining list dialogs (America / map pick). */
  if (colonize_key_up(input->last_key) && ng->selection > 0) {
    ng->selection--;
    return true;
  }
  if (colonize_key_down(input->last_key) && ng->selection + 1 < ng->option_count) {
    ng->selection++;
    return true;
  }
  if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
    new_game_activate_list(ng);
    return true;
  }
  if (input->mouse_left_clicked && ng->option_count > 0 && ng->line_h > 0) {
    const int mx = input->mouse_x;
    const int my = input->mouse_y;
    if (mx >= ng->dialog_x && my >= ng->dialog_y && mx < ng->dialog_x + ng->dialog_w &&
        my < ng->dialog_y + ng->dialog_h) {
      const int rel = my - ng->list_y0;
      if (rel >= 0) {
        const int idx = rel / ng->line_h;
        if (idx >= 0 && idx < ng->option_count) {
          ng->selection = idx;
          new_game_activate_list(ng);
        }
      }
    }
    return true;
  }
  return true;
}

void new_game_update(NewGameWizard* ng, uint32_t dt_ms) {
  if (!ng || ng->phase != NEW_GAME_PHASE_SAIL) {
    return;
  }
  ng->sail_accum_ms += dt_ms;
  while (ng->sail_accum_ms >= NEW_GAME_SAIL_FRAME_MS) {
    ng->sail_accum_ms -= NEW_GAME_SAIL_FRAME_MS;
    if (ng->sail_frame + 1 >= NEW_GAME_SAIL_FRAMES) {
      new_game_request_commit(ng);
      return;
    }
    ng->sail_frame++;
    new_game_ensure_sail_frame(ng, ng->sail_frame);
  }
}

static void new_game_draw_markup_line_ex(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t normal_color,
  uint8_t emphasis_color,
  bool unbold
) {
  if (!fb || !text) {
    return;
  }
  uint8_t color = normal_color;
  int cx = x;
  char chbuf[2] = {0, 0};
  for (const char* p = text; *p; ++p) {
    if (*p == '{') {
      color = emphasis_color;
      continue;
    }
    if (*p == '}') {
      color = normal_color;
      continue;
    }
    if (*p == '^') {
      continue; /* centering markers — ignore for left-draw */
    }
    if (*p == '_') {
      continue; /* indent markers */
    }
    chbuf[0] = *p;
    if (unbold) {
      font_draw_text_unbold(font, fb, cx, y, chbuf, color);
    } else {
      font_draw_text(font, fb, cx, y, chbuf, color);
    }
    const unsigned char ch = (unsigned char)*p;
    if (font && font->section_data && ch < 128 && font->char_widths[ch] > 0) {
      cx += font->char_widths[ch];
    } else {
      cx += 6;
    }
  }
}

static void new_game_draw_markup_line(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t normal_color,
  uint8_t emphasis_color
) {
  new_game_draw_markup_line_ex(font, fb, x, y, text, normal_color, emphasis_color, false);
}

static void new_game_draw_markup_line_unbold(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t normal_color,
  uint8_t emphasis_color
) {
  new_game_draw_markup_line_ex(font, fb, x, y, text, normal_color, emphasis_color, true);
}

/*
 * Word-wrap prompt lines to max_w (pixels), joining file lines into one flow.
 * The wrap itself is popup_wrap_text (audit SC-35 — this file used to carry
 * two private copies of the same greedy engine). The only thing left here is
 * the flattening the old copy did inline: drop each source line's leading
 * spaces/tabs and its GAME.TXT `_` / `^` layout markers, then join the lines
 * with a single space so the paragraph re-flows across them.
 *
 * Verified byte-identical to the retired local engine over every @-section of
 * the shipped COLONIZE/GAME.TXT, all five shipped fonts and every max_w from
 * 20 to 320, except @GAMEOPTIONS / @SOUNDOPTIONS (`~` hotkey markers) and
 * @LEARNMAD (`_` inside a word) — three sections the wizard never loads.
 * popup_wrap_text measures with popup_markup_text_width, which skips `~`/`#`
 * and counts `^`/`_`; the old loop did the reverse.
 */
static void new_game_wrap_prompt_flow(
  const ColonizeFont* font,
  char src[][COLONIZE_MSG_LINE_LEN],
  int src_count,
  char dst[][COLONIZE_MSG_LINE_LEN],
  int* dst_count,
  int max_dst,
  int max_w
) {
  if (!dst || !dst_count) {
    return;
  }
  *dst_count = 0;
  if (!src || src_count <= 0 || max_dst <= 0) {
    return;
  }
  if (max_w < 20) {
    max_w = 20;
  }
  char flow[8 * COLONIZE_MSG_LINE_LEN];
  size_t len = 0;
  flow[0] = '\0';
  for (int i = 0; i < src_count; ++i) {
    const char* p = src[i];
    if (!p) {
      continue;
    }
    while (*p == ' ' || *p == '\t' || *p == '_' || *p == '^') {
      p++;
    }
    if (!*p) {
      continue;
    }
    if (len && len + 1 < sizeof(flow)) {
      flow[len++] = ' ';
    }
    while (*p && len + 1 < sizeof(flow)) {
      flow[len++] = *p++;
    }
    flow[len] = '\0';
  }
  *dst_count = popup_wrap_text(font, flow, dst[0], COLONIZE_MSG_LINE_LEN, NULL, max_dst, max_w);
}

static void new_game_render_list_dialog(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  const ColonizePopupColors* popup_colors,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color
) {
  const ColonizeFont* font = ng->ui_font;
  const int line_h = font ? (font->max_height + 2) : 8;
  const int pad_x = 6;
  const int pad_y = 4;
  const uint8_t shadow = 0;

  int dialog_w = ng->dialog_width;
  if (dialog_w > fb->width - 4) {
    dialog_w = fb->width - 4;
  }
  const int text_max_w = dialog_w - POPUP_FRAME_INSET * 2 - pad_x * 2;

  char wrapped[8][COLONIZE_MSG_LINE_LEN];
  int wrapped_count = 0;
  new_game_wrap_prompt_flow(
    font, ng->prompt_lines, ng->prompt_line_count, wrapped, &wrapped_count, 8, text_max_w
  );
  if (wrapped_count == 0) {
    wrapped_count = ng->prompt_line_count;
    for (int i = 0; i < wrapped_count && i < 8; ++i) {
      str_copy_trunc(wrapped[i], sizeof(wrapped[0]), ng->prompt_lines[i]);
    }
  }

  const int prompt_h = wrapped_count * line_h;
  const int options_h = ng->option_count * line_h;
  int dialog_h = POPUP_FRAME_INSET * 2 + pad_y + prompt_h + options_h + pad_y;
  if (ng->phase == NEW_GAME_PHASE_LEADER_NAME) {
    dialog_h += line_h + 4;
  }
  if (dialog_h < 40) {
    dialog_h = 40;
  }
  if (dialog_h > fb->height - 4) {
    dialog_h = fb->height - 4;
  }
  int dialog_x = (fb->width - dialog_w) / 2;
  int dialog_y = ng->pref_dialog_y >= 0 ? ng->pref_dialog_y : (fb->height - dialog_h) / 2;
  if (dialog_y < 0) {
    dialog_y = 0;
  }
  if (dialog_y + dialog_h > fb->height) {
    dialog_y = fb->height - dialog_h;
  }

  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  popup_draw(
    fb,
    dialog_x,
    dialog_y,
    dialog_w,
    dialog_h,
    ng->wood_tile,
    popup_colors,
    &inner_x,
    &inner_y,
    &inner_w,
    &inner_h
  );

  int cy = inner_y + pad_y;
  for (int i = 0; i < wrapped_count; ++i) {
    new_game_draw_markup_line_unbold(
      font, fb, inner_x + pad_x + 1, cy + 1, wrapped[i], shadow, shadow
    );
    new_game_draw_markup_line_unbold(
      font, fb, inner_x + pad_x, cy, wrapped[i], text_color, hilite_color
    );
    cy += line_h;
  }

  ng->dialog_x = dialog_x;
  ng->dialog_y = dialog_y;
  ng->dialog_w = dialog_w;
  ng->dialog_h = dialog_h;
  ng->line_h = line_h;
  ng->list_y0 = cy;

  if (ng->phase == NEW_GAME_PHASE_LEADER_NAME) {
    ng->leader_field_font = font;
    ng->leader_field_x = inner_x + pad_x;
    ng->leader_field_y = cy;
    ng->leader_field_h = line_h;
    TextEditColors edit_colors;
    text_edit_default_colors(&edit_colors);
    edit_colors.text = text_color;
    text_edit_render(
      &ng->leader_edit, ng->leader_name, font, fb, ng->leader_field_x, cy, line_h, &edit_colors
    );
    return;
  }

  for (int i = 0; i < ng->option_count; ++i) {
    if (i == ng->selection) {
      fb_fill_rect(fb, inner_x + 2, cy - 1, inner_w - 4, line_h, select_color);
    }
    new_game_draw_markup_line_unbold(
      font, fb, inner_x + pad_x + 1, cy + 1, ng->options[i], shadow, shadow
    );
    new_game_draw_markup_line_unbold(
      font, fb, inner_x + pad_x, cy, ng->options[i], text_color, hilite_color
    );
    cy += line_h;
  }
}

static void new_game_copy_palette(ColonizePalette* dst, const ColonizePalette* src) {
  if (dst && src) {
    *dst = *src;
  }
}

/* Full-screen PIK backdrop plus "adopt its palette if it carries one" — the
 * same three lines at all five backdrop sites (audit SC-40). */
static void new_game_blit_bg(
  const ColonizePikImage* pik, ColonizeFramebuffer8* fb, ColonizePalette* out_palette
) {
  if (!pik) {
    return;
  }
  pik_blit(pik, fb, 0, 0);
  if (pik->has_palette) {
    new_game_copy_palette(out_palette, &pik->palette);
  }
}

static void new_game_blit_woodpanl(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette
) {
  memset(fb->pixels, 4, (size_t)fb->width * (size_t)fb->height);
  if (ng->woodpanl && ng->woodpanl->pixels) {
    new_game_blit_bg(ng->woodpanl, fb, out_palette);
  }
}

static void new_game_draw_shadowed_line(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t color,
  uint8_t shadow
) {
  new_game_draw_markup_line_unbold(font, fb, x + 1, y + 1, text, shadow, shadow);
  new_game_draw_markup_line_unbold(font, fb, x, y, text, color, color);
}

/* Like shadowed_line, but {keywords} use hilite on the foreground pass. */
static void new_game_draw_shadowed_markup(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t color,
  uint8_t hilite,
  uint8_t shadow
) {
  new_game_draw_markup_line_unbold(font, fb, x + 1, y + 1, text, shadow, shadow);
  new_game_draw_markup_line_unbold(font, fb, x, y, text, color, hilite);
}

/* Intended euro fill RGBs (art-palette 112/9/14/13); remap onto NATIONS.PIK. */
static const int k_euro_fill_rgb[4][3] = {
  {243, 0, 0},   /* England — saturated red */
  {4, 138, 227}, /* France */
  {223, 186, 0}, /* Spain */
  {255, 113, 0}, /* Netherlands */
};

static uint8_t new_game_nation_ink_on_pik(const NewGameWizard* ng, int nation_id) {
  if (nation_id < 0 || nation_id > 3) {
    nation_id = 0;
  }
  if (ng && ng->nations_ok && ng->nations_pik.has_palette) {
    return assets_palette_nearest_rgb(
      &ng->nations_pik.palette,
      k_euro_fill_rgb[nation_id][0],
      k_euro_fill_rgb[nation_id][1],
      k_euro_fill_rgb[nation_id][2]
    );
  }
  return unit_chrome_nation_color(nation_id);
}

static int new_game_centered_x(const ColonizeFont* font, const char* text, int anchor_cx) {
  const int w = font_text_width_skip(font, text, FONT_SKIP_LAYOUT);
  int x = anchor_cx - w / 2;
  if (x < 0) {
    x = 0;
  }
  return x;
}

static void new_game_format_finished(const NewGameWizard* ng, char* out, size_t out_size) {
  const char* finished = new_game_finished_text(ng);
  if (finished[0] == '(') {
    snprintf(out, out_size, "%s", finished);
  } else {
    snprintf(out, out_size, "(%s)", finished);
  }
}

/* Two tight lines centered in rect (no extra gap between lines). */
static void new_game_draw_centered_pair(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  const NewGameRect* rect,
  const char* top,
  const char* bottom,
  uint8_t color
) {
  const int line_h = font ? font->max_height : 6;
  const int block_h = line_h * 2;
  int y0 = rect->y + (rect->h - block_h) / 2;
  if (y0 < rect->y + 1) {
    y0 = rect->y + 1;
  }
  const int top_w = font_text_width_skip(font, top, FONT_SKIP_LAYOUT);
  const int bot_w = font_text_width_skip(font, bottom, FONT_SKIP_LAYOUT);
  int top_x = rect->x + (rect->w - top_w) / 2;
  int bot_x = rect->x + (rect->w - bot_w) / 2;
  if (top_x < rect->x + 1) {
    top_x = rect->x + 1;
  }
  if (bot_x < rect->x + 1) {
    bot_x = rect->x + 1;
  }
  new_game_draw_markup_line(font, fb, top_x, y0, top, color, color);
  new_game_draw_markup_line(font, fb, bot_x, y0 + line_h, bottom, color, color);
}

static void new_game_draw_nation_pair(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  const NewGameRect* rect,
  const char* top,
  const char* bottom,
  uint8_t color
) {
  /* Nation name at top, bonus at bottom — maximize vertical whitespace. */
  const int line_h = font ? font->max_height : 6;
  const int pad = 4;
  const int top_w = font_text_width_skip(font, top, FONT_SKIP_LAYOUT);
  const int bot_w = font_text_width_skip(font, bottom, FONT_SKIP_LAYOUT);
  int top_x = rect->x + (rect->w - top_w) / 2;
  int bot_x = rect->x + (rect->w - bot_w) / 2;
  if (top_x < rect->x + 1) {
    top_x = rect->x + 1;
  }
  if (bot_x < rect->x + 1) {
    bot_x = rect->x + 1;
  }
  new_game_draw_markup_line(font, fb, top_x, rect->y + pad, top, color, color);
  new_game_draw_markup_line(
    font, fb, bot_x, rect->y + rect->h - pad - line_h, bottom, color, color
  );
}

static void new_game_render_region_pick(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t border_color
) {
  (void)hilite_color;
  const bool difficul = (ng->phase == NEW_GAME_PHASE_DIFFICULTY);
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (difficul && ng->difficul_ok) {
    new_game_blit_bg(&ng->difficul_pik, fb, out_palette);
  } else if (!difficul && ng->nations_ok) {
    new_game_blit_bg(&ng->nations_pik, fb, out_palette);
  }

  const NewGameRect* rects = difficul ? k_difficul_rects : k_nation_rects;
  const int count = difficul ? 5 : 4;
  const ColonizeFont* tiny = ng->tiny_font ? ng->tiny_font : ng->ui_font;
  const ColonizeFont* title_font = ng->ui_font;
  const uint8_t green = text_color;

  uint8_t nation_ink = green;
  if (!difficul) {
    int sel = ng->selection;
    if (sel < 0 || sel > 3) {
      sel = 0;
    }
    nation_ink = new_game_nation_ink_on_pik(ng, sel);
  }

  if (ng->selection >= 0 && ng->selection < count) {
    const NewGameRect* r = &rects[ng->selection];
    uint8_t border = border_color;
    if (difficul) {
      border = k_difficul_colors[ng->selection];
    } else {
      border = nation_ink;
    }
    fb_rect_outline(fb, r->x, r->y, r->w, r->h, border, border);
  }

  const uint8_t shadow = 0;
  char finished_buf[80];
  new_game_format_finished(ng, finished_buf, sizeof(finished_buf));
  const int finished_line_h = tiny ? tiny->max_height : 6;
  const int tx = 8;
  const int finished_w = font_text_width_skip(tiny, finished_buf, FONT_SKIP_LAYOUT);
  const int anchor_cx = tx + finished_w / 2;
  const int title_lh = title_font ? title_font->max_height + 1 : 8;

  if (difficul) {
    const int title_y = 10;
    new_game_draw_shadowed_line(
      title_font,
      fb,
      new_game_centered_x(title_font, "Choose", anchor_cx),
      title_y,
      "Choose",
      green,
      shadow
    );
    new_game_draw_shadowed_line(
      title_font,
      fb,
      new_game_centered_x(title_font, "Difficulty Level", anchor_cx),
      title_y + title_lh,
      "Difficulty Level",
      green,
      shadow
    );
    /* Labels only on the selected difficulty (FUN_733a_0512). */
    if (ng->selection >= 0 && ng->selection < count) {
      char top[32];
      snprintf(top, sizeof(top), "%s:", reports_difficulty_title(ng->selection));
      new_game_draw_centered_pair(
        tiny,
        fb,
        &rects[ng->selection],
        top,
        k_difficul_levels[ng->selection],
        k_difficul_colors[ng->selection]
      );
    }
    const int fy = 92;
    new_game_draw_markup_line(tiny, fb, tx, fy, finished_buf, green, green);
    ng->finished_x = tx;
    ng->finished_y = fy;
    ng->finished_w = finished_w + 4;
    ng->finished_h = finished_line_h + 2;
  } else {
    /* Same green+shadow two-line title; a couple lines lower than difficulty. */
    const int title_y = 10 + 2 * title_lh;
    new_game_draw_shadowed_line(
      title_font,
      fb,
      new_game_centered_x(title_font, "Select", anchor_cx),
      title_y,
      "Select",
      green,
      shadow
    );
    new_game_draw_shadowed_line(
      title_font,
      fb,
      new_game_centered_x(title_font, "European Power", anchor_cx),
      title_y + title_lh,
      "European Power",
      green,
      shadow
    );
    /* Labels only when that nation is selected. */
    if (ng->selection >= 0 && ng->selection < count) {
      char top[32];
      snprintf(top, sizeof(top), "%s:", new_game_nation_name(ng->selection));
      for (char* p = top; *p; ++p) {
        if (*p >= 'a' && *p <= 'z') {
          *p = (char)(*p - 'a' + 'A');
        }
      }
      new_game_draw_nation_pair(
        tiny,
        fb,
        &rects[ng->selection],
        top,
        k_nation_bonuses[ng->selection],
        nation_ink
      );
    }
    const int fy = fb->height - finished_line_h - 4;
    new_game_draw_markup_line(tiny, fb, tx, fy, finished_buf, green, green);
    ng->finished_x = tx;
    ng->finished_y = fy;
    ng->finished_w = finished_w + 4;
    ng->finished_h = finished_line_h + 2;
  }
}

static void new_game_render_customize(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette,
  uint8_t focus_border,
  uint8_t other_border
) {
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (ng->customiz_ok) {
    new_game_blit_bg(&ng->customiz_pik, fb, out_palette);
  }

  const char* cats[4];
  const char* vals[12];
  const char* title = k_customiz_title;
  new_game_customiz_labels(ng, cats, vals, &title);

  const ColonizeFont* tiny = ng->tiny_font ? ng->tiny_font : ng->ui_font;
  const ColonizeFont* title_font = ng->ui_font;
  const uint8_t green = other_border;
  const uint8_t yellow = focus_border;
  const uint8_t unbold = 15;

  new_game_draw_markup_line(title_font, fb, 8, 2, title, unbold, unbold);

  for (int c = 0; c < 4; ++c) {
    const int row = new_game_gen_param_value(&ng->gen_params, c);
    const NewGameRect* rect = &k_customiz_rects[c][row];
    const bool focused = (c == ng->customize_focus);
    const uint8_t ink = focused ? yellow : green;
    fb_rect_outline(fb, rect->x, rect->y, rect->w, rect->h, ink, ink);
    new_game_draw_centered_pair(tiny, fb, rect, cats[c], vals[c * 3 + row], ink);
  }

  char finished_buf[80];
  new_game_format_finished(ng, finished_buf, sizeof(finished_buf));
  const int fw = font_text_width_skip(tiny, finished_buf, FONT_SKIP_LAYOUT);
  const int fx = (fb->width - fw) / 2;
  const int fy = 186;
  new_game_draw_markup_line(tiny, fb, fx, fy, finished_buf, green, green);
  ng->finished_x = 0;
  ng->finished_y = 185;
  ng->finished_w = fb->width;
  ng->finished_h = fb->height - 185;
}

static void new_game_render_leader_name(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette,
  uint8_t text_color,
  uint8_t hilite_color
) {
  new_game_blit_woodpanl(ng, fb, out_palette);
  const ColonizeFont* font = ng->ui_font;
  const int line_h = font ? (font->max_height + 3) : 10;
  const char* prompt =
    ng->prompt_line_count > 0 ? ng->prompt_lines[0] : "Please Enter Your Name.";
  char prompt_clean[COLONIZE_MSG_LINE_LEN];
  size_t po = 0;
  for (const char* p = prompt; *p && po + 1 < sizeof(prompt_clean); ++p) {
    if (*p == '^' || *p == '_') {
      continue;
    }
    prompt_clean[po++] = *p;
  }
  prompt_clean[po] = '\0';

  const int field_w = text_edit_field_width(font, ng->leader_name);
  int box_w = field_w * 2;
  if (box_w < 120) {
    box_w = 120;
  }
  const int prompt_w = font_text_width_skip(font, prompt_clean, FONT_SKIP_LAYOUT);
  if (box_w < prompt_w) {
    box_w = prompt_w;
  }
  const int cx = (fb->width - box_w) / 2;
  const int cy = (fb->height - line_h * 2 - 8) / 2;
  const uint8_t green = text_color;
  (void)hilite_color;
  const uint8_t shadow = 0;
  new_game_draw_shadowed_line(font, fb, cx, cy, prompt_clean, green, shadow);

  const int field_y = cy + line_h + 6;
  int fx = 0, fy = 0, fw = 0, fh = 0;
  text_edit_frame_rect(font, cx, field_y, box_w, &fx, &fy, &fw, &fh);
  text_edit_draw_frame(fb, fx, fy, fw, fh, green);
  ng->leader_field_font = font;
  ng->leader_field_x = cx;
  ng->leader_field_y = field_y;
  ng->leader_field_h = line_h;
  TextEditColors edit_colors;
  text_edit_default_colors(&edit_colors);
  edit_colors.text = green;
  text_edit_render(
    &ng->leader_edit,
    ng->leader_name,
    font,
    fb,
    ng->leader_field_x,
    field_y,
    line_h,
    &edit_colors
  );
}

/*
 * Nation lore (DOS FUN_6f74_1198): consecutive non-centered body lines flow
 * together and word-wrap to @width (pixel). ^/^^ lines and blank spacers are
 * hard breaks. GAME.TXT newlines alone are not display breaks — e.g. England's
 * "{religious" / "strife}" join into one hilited phrase under FONTINTR @ 300.
 */
#define NEW_GAME_LORE_MAX_OUT 48

static void new_game_lore_emit(
  char out[][COLONIZE_MSG_LINE_LEN],
  bool out_center[],
  int* n_out,
  const char* text,
  bool center
) {
  if (!n_out || *n_out >= NEW_GAME_LORE_MAX_OUT) {
    return;
  }
  str_copy_trunc(out[*n_out], COLONIZE_MSG_LINE_LEN, text ? text : "");
  out_center[*n_out] = center;
  (*n_out)++;
}

/*
 * Paragraph buffer -> wrapped output. `para` accumulates the body runs of
 * consecutive source lines (DOS FUN_6f74_1198 reflows across file newlines);
 * the flush hands the whole paragraph to popup_wrap_text, which is the one
 * word-wrap engine in the port (audit SC-35 retired the two private copies
 * this file used to carry). Verified byte-identical to the retired engine
 * over every section of the shipped COLONIZE/GAME.TXT and all five shipped
 * fonts; see new_game_wrap_prompt_flow for the three sections that do differ
 * and why they are unreachable from here.
 */
#define NEW_GAME_LORE_PARA_MAX 2048

static void new_game_lore_flush_para(
  const ColonizeFont* font,
  int max_w,
  char* para,
  char out[][COLONIZE_MSG_LINE_LEN],
  bool out_center[],
  int* n_out
) {
  if (!para || !para[0]) {
    return;
  }
  if (n_out && *n_out < NEW_GAME_LORE_MAX_OUT) {
    *n_out += popup_wrap_text(
      font,
      para,
      out[*n_out],
      COLONIZE_MSG_LINE_LEN,
      out_center + *n_out,
      NEW_GAME_LORE_MAX_OUT - *n_out,
      max_w
    );
  }
  para[0] = '\0';
}

/* Append one source line's body to the running paragraph, space-joined. */
static void new_game_lore_add_run(char* para, size_t para_size, const char* text) {
  if (!para || para_size == 0 || !text) {
    return;
  }
  size_t len = strlen(para);
  if (len && len + 1 < para_size) {
    para[len++] = ' ';
  }
  while (*text && len + 1 < para_size) {
    para[len++] = *text++;
  }
  para[len] = '\0';
}

static void new_game_render_lore(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette,
  uint8_t text_color,
  uint8_t hilite_color
) {
  new_game_blit_woodpanl(ng, fb, out_palette);

  char section_name[16];
  snprintf(
    section_name,
    sizeof(section_name),
    "NATION%d%c",
    ng->nation,
    ng->phase == NEW_GAME_PHASE_NATION_LORE_B ? 'B' : 'A'
  );
  const ColonizeMsgSection* section =
    ng->game_txt ? assets_msg_find(ng->game_txt, section_name) : NULL;

  const ColonizeFont* font = ng->lore_font ? ng->lore_font : ng->ui_font;
  const int line_h = font ? (font->max_height + 1) : 9;
  const int margin_x = 10;
  const int margin_y = 8;
  const uint8_t green = text_color;
  const uint8_t yellow = hilite_color;
  const uint8_t shadow = 0;

  if (!section) {
    new_game_draw_shadowed_markup(
      font, fb, margin_x, fb->height / 2, "Nation history unavailable.", green, yellow, shadow
    );
    return;
  }

  int max_w = 300;
  char out[NEW_GAME_LORE_MAX_OUT][COLONIZE_MSG_LINE_LEN];
  bool out_center[NEW_GAME_LORE_MAX_OUT];
  int n_out = 0;
  char para[NEW_GAME_LORE_PARA_MAX];
  para[0] = '\0';

  for (int i = 0; i < section->line_count && n_out < NEW_GAME_LORE_MAX_OUT; ++i) {
    const char* line = section->lines[i];
    if (!line) {
      continue;
    }
    if (strncmp(line, "@width=", 7) == 0) {
      max_w = atoi(line + 7);
      if (max_w < 40) {
        max_w = 40;
      }
      continue;
    }
    if (new_game_is_directive(line)) {
      continue;
    }
    /* DOS FUN_6f74_0c32 eats at most two carets; a third is body text. */
    const char* p = line;
    const bool center = popup_msg_caret_flags(line, &p) != 0;
    while (*p == '_' || *p == ' ') {
      p++;
    }
    if (*p == '\0') {
      new_game_lore_flush_para(font, max_w, para, out, out_center, &n_out);
      new_game_lore_emit(out, out_center, &n_out, "", false);
      continue;
    }
    if (center) {
      new_game_lore_flush_para(font, max_w, para, out, out_center, &n_out);
      new_game_lore_emit(out, out_center, &n_out, p, true);
      continue;
    }
    new_game_lore_add_run(para, sizeof(para), p);
  }
  new_game_lore_flush_para(font, max_w, para, out, out_center, &n_out);

  const int block_h = n_out * line_h;
  int cy = (fb->height - block_h) / 2;
  if (cy < margin_y) {
    cy = margin_y;
  }
  if (cy + block_h > fb->height - margin_y) {
    cy = margin_y;
  }

  for (int i = 0; i < n_out; ++i) {
    if (cy + line_h > fb->height - margin_y) {
      break;
    }
    const char* text = out[i];
    if (text[0]) {
      int x = margin_x;
      if (out_center[i]) {
        const int w = font_text_width_skip(font, text, FONT_SKIP_LAYOUT);
        x = (fb->width - w) / 2;
        if (x < margin_x) {
          x = margin_x;
        }
      }
      new_game_draw_shadowed_markup(font, fb, x, cy, text, green, yellow, shadow);
    }
    cy += line_h;
  }
}

static void new_game_subst_country(char* dst, size_t dst_size, const char* src, const char* country) {
  if (!dst || dst_size == 0) {
    return;
  }
  dst[0] = '\0';
  if (!src) {
    return;
  }
  size_t out = 0;
  for (const char* p = src; *p && out + 1 < dst_size;) {
    if (strncmp(p, "%COUNTRY", 8) == 0) {
      size_t n = strlen(country);
      if (out + n >= dst_size) {
        n = dst_size - out - 1;
      }
      memcpy(dst + out, country, n);
      out += n;
      p += 8;
    } else {
      dst[out++] = *p++;
    }
  }
  dst[out] = '\0';
}

static void new_game_render_king(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette,
  uint8_t text_color,
  uint8_t hilite_color
) {
  (void)text_color;
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (ng->kinglss_ok) {
    new_game_blit_bg(&ng->kinglss_pik, fb, out_palette);
  }
  if (ng->nation_art_ok && ng->nation_art.sprite_count > 0 && ng->king1_ok &&
      ng->king1.sprite_count > 0) {
    /*
     * ENGLND1/FRANCE1/… is one sprite with left+right banners and a transparent
     * mid-gap (opaque gap columns ~51..122 → center offset 86). King+dog are one
     * KING1.SS bitmap. Nudge: flags −2px, king −21px from the prior (20 / 34) align.
     */
    const ColonizeSprite* king = &ng->king1.sprites[0];
    const int king_x = 20 - 21;
    const int king_y = fb->height - king->height; /* typically 13 on 200px */
    const int flag_x = 20 + king->width / 2 - 86 + 6 - 2;
    const int flag_y = 0;
    ss_blit_sprite(&ng->nation_art, 0, fb, flag_x, flag_y);
    ss_blit_sprite(&ng->king1, 0, fb, king_x, king_y);
  } else {
    if (ng->nation_art_ok && ng->nation_art.sprite_count > 0) {
      ss_blit_sprite(&ng->nation_art, 0, fb, 28 + 6 - 2, 0);
    }
    if (ng->king1_ok && ng->king1.sprite_count > 0) {
      ss_blit_sprite(
        &ng->king1, 0, fb, 20 - 21, fb->height - ng->king1.sprites[0].height
      );
    }
  }

  const char* section_name = (ng->nation == 3) ? "VICEROY2" : "VICEROY";
  const ColonizeMsgSection* section =
    ng->game_txt ? assets_msg_find(ng->game_txt, section_name) : NULL;
  const ColonizeFont* font = ng->fontking_ok ? &ng->fontking : ng->ui_font;
  const int line_h = font ? (font->max_height + 1) : 8;
  int tx = 232;
  int ty = 21;
  int tw = 78;
  /* Audience ink is black on KINGLSS (not @COLORS basic, which is brown here). */
  const uint8_t ink = 0;
  const uint8_t hilite = hilite_color;

  if (!section) {
    return;
  }

  char out[NEW_GAME_LORE_MAX_OUT][COLONIZE_MSG_LINE_LEN];
  bool out_center[NEW_GAME_LORE_MAX_OUT];
  int n_out = 0;
  char para[NEW_GAME_LORE_PARA_MAX];
  para[0] = '\0';

  for (int i = 0; i < section->line_count && n_out < NEW_GAME_LORE_MAX_OUT; ++i) {
    const char* line = section->lines[i];
    if (!line) {
      continue;
    }
    if (strncmp(line, "@x=", 3) == 0) {
      tx = atoi(line + 3);
      continue;
    }
    if (strncmp(line, "@y=", 3) == 0) {
      ty = atoi(line + 3);
      continue;
    }
    if (strncmp(line, "@width=", 7) == 0) {
      tw = atoi(line + 7);
      if (tw < 20) {
        tw = 20;
      }
      continue;
    }
    if (new_game_is_directive(line)) {
      continue;
    }

    char buf[COLONIZE_MSG_LINE_LEN];
    new_game_subst_country(buf, sizeof(buf), line, new_game_nation_name(ng->nation));

    /* DOS FUN_6f74_0c32 eats at most two carets; a third is body text. */
    const char* p = buf;
    const bool center = popup_msg_caret_flags(buf, &p) != 0;
    while (*p == '_' || *p == ' ') {
      p++;
    }
    if (*p == '\0') {
      /* Lone ^ / blank — vertical spacer (DOS hard break). */
      new_game_lore_flush_para(font, tw, para, out, out_center, &n_out);
      new_game_lore_emit(out, out_center, &n_out, "", false);
      continue;
    }
    if (center) {
      new_game_lore_flush_para(font, tw, para, out, out_center, &n_out);
      new_game_lore_emit(out, out_center, &n_out, p, true);
      continue;
    }
    /* Body: flow-wrap at @width (FUN_6f74_1198); file newlines are not hard breaks. */
    new_game_lore_add_run(para, sizeof(para), p);
  }
  new_game_lore_flush_para(font, tw, para, out, out_center, &n_out);

  int y = ty;
  for (int i = 0; i < n_out; ++i) {
    if (y > fb->height - line_h) {
      break;
    }
    const char* text = out[i];
    if (text[0]) {
      int x = tx;
      if (out_center[i]) {
        const int w = font_text_width_skip(font, text, FONT_SKIP_LAYOUT);
        x = tx + (tw - w) / 2;
        if (x < tx) {
          x = tx;
        }
      }
      new_game_draw_markup_line(font, fb, x, y, text, ink, hilite);
    }
    y += line_h;
  }
}

/*
 * End-of-war throne audience (DOS FUN_75c2_20e2). Same composition as the
 * new-game audience — KINGLSS1 backdrop + nation banners — but the king
 * sprite comes from KINGLOSE.SS (player won) or KINGWIN.SS (King won), and
 * the scroll text is the already-token-filled dialog body, flow-wrapped to
 * the GAME.TXT section's @width. Ink is black on KINGLSS, as in the wizard.
 */
void new_game_render_throne_audience(
  NewGameWizard* ng,
  const char* data_dir,
  int nation,
  const char* king_sheet,
  const char* body,
  int text_x,
  int text_y,
  int text_w,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette
) {
  if (!ng || !fb || !fb->pixels || !king_sheet) {
    return;
  }
  if (ng->data_dir[0] == '\0' && data_dir && data_dir[0]) {
    snprintf(ng->data_dir, sizeof(ng->data_dir), "%s", data_dir);
  }
  new_game_ensure_kinglss(ng);
  if (ng->endking_ok && strcmp(ng->endking_name, king_sheet) != 0) {
    ss_free(&ng->endking);
    ng->endking_ok = false;
  }
  if (!ng->endking_ok) {
    if (new_game_load_ss(ng, king_sheet, &ng->endking, &ng->endking_ok)) {
      snprintf(ng->endking_name, sizeof(ng->endking_name), "%s", king_sheet);
    }
  }
  /* The banner slot may hold another nation's flags from an earlier wizard. */
  const int nat = (nation < 0 || nation > 3) ? 0 : nation;
  if (ng->nation_art_ok && ng->nation != nat) {
    ss_free(&ng->nation_art);
    ng->nation_art_ok = false;
  }
  if (!ng->nation_art_ok) {
    if (new_game_load_ss(ng, k_nation_ss[nat], &ng->nation_art, &ng->nation_art_ok)) {
      ng->nation = nat;
    }
  }
  if (!ng->fontking_ok) {
    char path[512];
    char err[256];
    if (dos_compat_normalize_asset_path(ng->data_dir, "FONTKING.FF", path, sizeof(path)) &&
        ff_load(path, &ng->fontking, err, sizeof(err))) {
      ng->fontking_ok = true;
    }
  }

  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (ng->kinglss_ok) {
    new_game_blit_bg(&ng->kinglss_pik, fb, out_palette);
  }
  if (ng->endking_ok && ng->endking.sprite_count > 0) {
    const ColonizeSprite* king = &ng->endking.sprites[0];
    /* Same anchors as the wizard audience (new_game_render_king). */
    const int king_x = 20 - 21;
    const int king_y = fb->height - king->height;
    if (ng->nation_art_ok && ng->nation_art.sprite_count > 0) {
      const int flag_x = 20 + king->width / 2 - 86 + 6 - 2;
      ss_blit_sprite(&ng->nation_art, 0, fb, flag_x, 0);
    }
    ss_blit_sprite(&ng->endking, 0, fb, king_x, king_y);
  } else if (ng->nation_art_ok && ng->nation_art.sprite_count > 0) {
    ss_blit_sprite(&ng->nation_art, 0, fb, 28 + 6 - 2, 0);
  }

  if (!body || !body[0]) {
    return;
  }
  const ColonizeFont* font = ng->fontking_ok ? &ng->fontking : ng->ui_font;
  if (!font) {
    return;
  }
  if (text_w < 20) {
    text_w = 20;
  }
  char out[NEW_GAME_LORE_MAX_OUT][COLONIZE_MSG_LINE_LEN];
  bool out_center[NEW_GAME_LORE_MAX_OUT];
  int n_out = 0;
  char para[NEW_GAME_LORE_PARA_MAX];
  para[0] = '\0';
  new_game_lore_add_run(para, sizeof(para), body);
  new_game_lore_flush_para(font, text_w, para, out, out_center, &n_out);
  const int line_h = font->max_height + 1;
  int y = text_y;
  for (int i = 0; i < n_out; ++i) {
    if (y > fb->height - line_h) {
      break;
    }
    if (out[i][0]) {
      new_game_draw_markup_line(font, fb, text_x, y, out[i], 0, 0);
    }
    y += line_h;
  }
}

static void new_game_subst_build(
  char* dst,
  size_t dst_size,
  const char* src,
  const char* s0,
  const char* s1
) {
  if (!dst || dst_size == 0) {
    return;
  }
  dst[0] = '\0';
  if (!src) {
    return;
  }
  size_t out = 0;
  for (const char* p = src; *p && out + 1 < dst_size;) {
    if (strncmp(p, "%STRING0", 8) == 0) {
      const char* rep = s0 ? s0 : "";
      size_t n = strlen(rep);
      if (out + n >= dst_size) {
        n = dst_size - out - 1;
      }
      memcpy(dst + out, rep, n);
      out += n;
      p += 8;
    } else if (strncmp(p, "%STRING1", 8) == 0) {
      const char* rep = s1 ? s1 : "";
      size_t n = strlen(rep);
      if (out + n >= dst_size) {
        n = dst_size - out - 1;
      }
      memcpy(dst + out, rep, n);
      out += n;
      p += 8;
    } else {
      dst[out++] = *p++;
    }
  }
  dst[out] = '\0';
}

static void new_game_render_sail(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette,
  uint8_t text_color,
  uint8_t hilite_color
) {
  (void)text_color;
  (void)hilite_color;
  const int frame = ng->sail_frame;
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (frame >= 0 && frame < NEW_GAME_SAIL_FRAMES && ng->levn_ok[frame]) {
    new_game_blit_bg(&ng->levn[frame], fb, out_palette);
  }

  char section_name[24];
  snprintf(section_name, sizeof(section_name), "BUILD%d", frame + 1);
  const ColonizeMsgSection* section =
    ng->game_txt ? assets_msg_find(ng->game_txt, section_name) : NULL;
  const ColonizeFont* font = ng->ui_font;
  const int line_h = font ? (font->max_height + 2) : 8;
  int ty = 30;
  int tw = 310;

  const char* leader = ng->leader_name;
  const char* country = new_game_nation_name(ng->nation);
  const char* port = new_game_nation_port(ng->nation);
  const char* ruler = new_game_nation_ruler_title(ng->nation);

  /* BUILD caption string mapping (DOS order):
   * 2: %STRING0=leader %STRING1=title-ish (Governor / explorer label) — use nation demonym role
   * 3: %STRING0=port
   * 4: %STRING1=King/Stadtholder %STRING0=country
   * 7: %STRING0=country
   */
  const char* s0 = "";
  const char* s1 = "";
  switch (frame + 1) {
    case 2:
      s0 = leader;
      s1 = "Navigator";
      break;
    case 3:
      s0 = port;
      break;
    case 4:
      s0 = country;
      s1 = ruler;
      break;
    case 7:
      s0 = country;
      break;
    default:
      break;
  }

  /* Unbold white (ink 15 AA) + black drop-shadow. */
  const uint8_t white = 15;
  const uint8_t black = 0;

  if (!section) {
    return;
  }
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0') {
      continue;
    }
    if (strncmp(line, "@y=", 3) == 0) {
      ty = atoi(line + 3);
      continue;
    }
    if (strncmp(line, "@width=", 7) == 0) {
      tw = atoi(line + 7);
      continue;
    }
    if (new_game_is_directive(line)) {
      continue;
    }
    char buf[COLONIZE_MSG_LINE_LEN];
    new_game_subst_build(buf, sizeof(buf), line, s0, s1);
    /* DOS FUN_6f74_0c32 eats at most two carets; a third is body text. */
    const char* body = buf;
    (void)popup_msg_caret_flags(buf, &body);
    int w = font_text_width_skip(font, body, FONT_SKIP_LAYOUT);
    int x = (fb->width - w) / 2;
    if (tw < fb->width) {
      x = (fb->width - tw) / 2 + (tw - w) / 2;
    }
    if (x < 0) {
      x = 0;
    }
    new_game_draw_markup_line(font, fb, x + 1, ty + 1, body, black, black);
    new_game_draw_markup_line(font, fb, x, ty, body, white, white);
    ty += line_h;
  }
}

void new_game_render(
  NewGameWizard* ng,
  ColonizeFramebuffer8* framebuffer,
  ColonizePalette* out_palette,
  const ColonizePopupColors* popup_colors,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color
) {
  if (!ng || !framebuffer || !framebuffer->pixels || !new_game_active(ng)) {
    return;
  }

  switch (ng->phase) {
    case NEW_GAME_PHASE_CUSTOMIZE:
      /* hilite = yellow (focus), text_color = green (other columns). */
      new_game_render_customize(ng, framebuffer, out_palette, hilite_color, text_color);
      break;
    case NEW_GAME_PHASE_DIFFICULTY:
    case NEW_GAME_PHASE_NATION:
      new_game_render_region_pick(
        ng, framebuffer, out_palette, text_color, hilite_color, hilite_color
      );
      break;
    case NEW_GAME_PHASE_AMERICA_CHOICE:
    case NEW_GAME_PHASE_MAP_PICK:
      /* Black field; OPENTILE popup needs OPENMENU/OPENTILE palette (set by caller). */
      memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);
      new_game_render_list_dialog(ng, framebuffer, popup_colors, text_color, hilite_color, select_color);
      break;
    case NEW_GAME_PHASE_LEADER_NAME:
      new_game_render_leader_name(ng, framebuffer, out_palette, text_color, hilite_color);
      break;
    case NEW_GAME_PHASE_NATION_LORE_A:
    case NEW_GAME_PHASE_NATION_LORE_B:
      new_game_render_lore(ng, framebuffer, out_palette, text_color, hilite_color);
      break;
    case NEW_GAME_PHASE_KING:
      new_game_render_king(ng, framebuffer, out_palette, text_color, hilite_color);
      break;
    case NEW_GAME_PHASE_SAIL:
      new_game_render_sail(ng, framebuffer, out_palette, text_color, hilite_color);
      break;
    default:
      break;
  }
}
