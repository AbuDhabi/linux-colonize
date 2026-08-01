#include "core/new_game.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/ui_colors.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

#define NEW_GAME_SAIL_FRAME_MS 1600u

static const char* k_nation_names[4] = {"England", "France", "Spain", "Netherlands"};
static const char* k_nation_ports[4] = {"London", "Paris", "Seville", "Amsterdam"};
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

static const char* k_finished_label = "(Click Here When Finished)";

const char* new_game_nation_name(int nation) {
  if (nation < 0 || nation > 3) {
    return "England";
  }
  return k_nation_names[nation];
}

const char* new_game_nation_port(int nation) {
  if (nation < 0 || nation > 3) {
    return "London";
  }
  return k_nation_ports[nation];
}

const char* new_game_nation_ruler_title(int nation) {
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

static void new_game_ensure_king(NewGameWizard* ng) {
  new_game_load_pik(ng, "KINGLSS1.PIK", &ng->kinglss_pik, &ng->kinglss_ok);
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
          snprintf(ng->prompt_lines[ng->prompt_line_count], sizeof(ng->prompt_lines[0]), "%s", line);
          ng->prompt_line_count++;
        }
      } else if (i > options_at && ng->option_count < NEW_GAME_OPTION_MAX) {
        snprintf(ng->options[ng->option_count], sizeof(ng->options[0]), "%s", line);
        ng->option_count++;
      }
    }
  } else {
    const char* content[COLONIZE_MSG_MAX_LINES];
    int content_count = 0;
    for (int i = 0; i < section->line_count; ++i) {
      const char* line = section->lines[i];
      if (!line || line[0] == '\0' || new_game_is_directive(line) || new_game_line_is_filler(line)) {
        continue;
      }
      if (content_count < COLONIZE_MSG_MAX_LINES) {
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
      snprintf(ng->prompt_lines[ng->prompt_line_count], sizeof(ng->prompt_lines[0]), "%s", content[i]);
      ng->prompt_line_count++;
    }
    for (int i = split; i < content_count && ng->option_count < NEW_GAME_OPTION_MAX; ++i) {
      snprintf(ng->options[ng->option_count], sizeof(ng->options[0]), "%s", content[i]);
      ng->option_count++;
    }
  }

  if (ng->selection >= ng->option_count) {
    ng->selection = 0;
  }
}

static void new_game_seed_leader_name(NewGameWizard* ng) {
  static const char* defaults[4] = {
    "Walter Raleigh", "Jacques Cartier", "Christopher Columbus", "Michiel De Ruyter"
  };
  const char* name = defaults[ng->nation < 0 || ng->nation > 3 ? 0 : ng->nation];
  if (ng->names_txt) {
    const ColonizeMsgSection* section = assets_msg_find(ng->names_txt, "LEADERNAME");
    if (section) {
      int idx = 0;
      for (int i = 0; i < section->line_count; ++i) {
        const char* line = section->lines[i];
        if (!line || line[0] == '\0' || line[0] == ';' || line[0] == '@') {
          continue;
        }
        if (idx == ng->nation) {
          char buf[NEW_GAME_LEADER_NAME_MAX];
          snprintf(buf, sizeof(buf), "%s", line);
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
            snprintf(ng->leader_name, sizeof(ng->leader_name), "%s", buf);
            return;
          }
        }
        idx++;
      }
    }
  }
  snprintf(ng->leader_name, sizeof(ng->leader_name), "%s", name);
}

static void new_game_enter_difficulty(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_DIFFICULTY;
  new_game_ensure_difficul(ng);
  new_game_load_choice_section(ng, "DIFFICULTY");
  if (ng->option_count == 0) {
    static const char* d[] = {"Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"};
    snprintf(ng->prompt_lines[0], sizeof(ng->prompt_lines[0]), "Select a Difficulty Level");
    ng->prompt_line_count = 1;
    for (int i = 0; i < 5; ++i) {
      snprintf(ng->options[i], sizeof(ng->options[0]), "%s", d[i]);
    }
    ng->option_count = 5;
  }
}

static void new_game_enter_nation(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_NATION;
  new_game_ensure_nations(ng);
  new_game_load_choice_section(ng, "PICKNATION");
  if (ng->option_count == 0) {
    snprintf(ng->prompt_lines[0], sizeof(ng->prompt_lines[0]), "Select a European Power");
    ng->prompt_line_count = 1;
    for (int i = 0; i < 4; ++i) {
      snprintf(ng->options[i], sizeof(ng->options[0]), "%s", k_nation_names[i]);
    }
    ng->option_count = 4;
  }
}

static void new_game_enter_leader_name(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_LEADER_NAME;
  new_game_seed_leader_name(ng);
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
    snprintf(ng->options[ng->option_count], sizeof(ng->options[0]), "%s", name);
    ng->option_count++;
  }
  closedir(dir);
  if (ng->option_count == 0) {
    snprintf(ng->options[0], sizeof(ng->options[0]), "AMER2.MP");
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
  snprintf(ng->map_file, sizeof(ng->map_file), "AMER2.MP");
  snprintf(ng->data_dir, sizeof(ng->data_dir), "%s", data_dir);
  ng->sail_frame = 0;
  ng->sail_accum_ms = 0;

  if (path == NEW_GAME_PATH_AMERICA) {
    ng->phase = NEW_GAME_PHASE_AMERICA_CHOICE;
    new_game_load_choice_section(ng, "AMERICA");
    if (ng->option_count == 0) {
      snprintf(ng->prompt_lines[0], sizeof(ng->prompt_lines[0]), "Original Americas or Map Editor?");
      ng->prompt_line_count = 1;
      snprintf(ng->options[0], sizeof(ng->options[0]), "Original Americas");
      snprintf(ng->options[1], sizeof(ng->options[1]), "Map Editor");
      ng->option_count = 2;
    }
  } else {
    new_game_enter_difficulty(ng);
  }
  return true;
}

bool new_game_scenario_start(
  const ColonizeMsgCatalog* names_txt,
  const char* map_stem,
  int nation,
  int* out_x,
  int* out_y
) {
  /* Defaults: AMER2 England landing. */
  int xs[4] = {39, 47, 50, 50};
  int ys[4] = {10, 61, 33, 33};
  if (names_txt && map_stem) {
    const ColonizeMsgSection* section = assets_msg_find(names_txt, "SCENARIO");
    if (section) {
      for (int i = 0; i < section->line_count; ++i) {
        const char* line = section->lines[i];
        if (!line || line[0] == '\0' || line[0] == ';') {
          continue;
        }
        char stem[64];
        int a = 0, b = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
        int n = sscanf(
          line,
          "%63[^,], %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
          stem,
          &a,
          &b,
          &x0,
          &y0,
          &x1,
          &y1,
          &x2,
          &y2,
          &x3,
          &y3
        );
        /* Trim stem spaces */
        size_t sl = strlen(stem);
        while (sl > 0 && (stem[sl - 1] == ' ' || stem[sl - 1] == '\t')) {
          stem[--sl] = '\0';
        }
        if (strcasecmp(stem, map_stem) != 0) {
          continue;
        }
        if (n >= 6) {
          xs[0] = x0;
          ys[0] = y0;
        }
        if (n >= 8) {
          xs[1] = x1;
          ys[1] = y1;
        }
        if (n >= 10) {
          xs[2] = x2;
          ys[2] = y2;
        }
        if (n >= 12) {
          xs[3] = x3;
          ys[3] = y3;
        } else if (n >= 10) {
          xs[3] = x2;
          ys[3] = y2;
        }
        break;
      }
    }
  }
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  if (out_x) {
    *out_x = xs[nation];
  }
  if (out_y) {
    *out_y = ys[nation];
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
      if (s == 1) {
        s = 0;
      } else if (s == 3) {
        s = 2;
      } else if (s == 4) {
        s = 3;
      }
      break;
    case COLONIZE_KEY_RIGHT:
      if (s == 0) {
        s = 1;
      } else if (s == 2) {
        s = 3;
      } else if (s == 3) {
        s = 4;
      }
      break;
    case COLONIZE_KEY_UP:
      if (s == 2) {
        s = 0;
      } else if (s == 3) {
        s = 0;
      } else if (s == 4) {
        s = 1;
      }
      break;
    case COLONIZE_KEY_DOWN:
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
      if (s == 1 || s == 3) {
        s--;
      }
      break;
    case COLONIZE_KEY_RIGHT:
      if (s == 0 || s == 2) {
        s++;
      }
      break;
    case COLONIZE_KEY_UP:
      if (s >= 2) {
        s -= 2;
      }
      break;
    case COLONIZE_KEY_DOWN:
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

bool new_game_handle_input(NewGameWizard* ng, const ColonizeInputState* input) {
  if (!ng || !input || !new_game_active(ng) || ng->phase == NEW_GAME_PHASE_COMMIT) {
    return false;
  }

  const bool pre_king = ng->phase == NEW_GAME_PHASE_AMERICA_CHOICE ||
    ng->phase == NEW_GAME_PHASE_MAP_PICK || ng->phase == NEW_GAME_PHASE_DIFFICULTY ||
    ng->phase == NEW_GAME_PHASE_NATION || ng->phase == NEW_GAME_PHASE_LEADER_NAME;

  if (input->last_key == COLONIZE_KEY_ESCAPE && pre_king) {
    new_game_cancel(ng);
    return true;
  }

  /* Image-region pick: DIFFICULTY / NATION — no list popup. */
  if (ng->phase == NEW_GAME_PHASE_DIFFICULTY || ng->phase == NEW_GAME_PHASE_NATION) {
    const NewGameRect* rects =
      (ng->phase == NEW_GAME_PHASE_DIFFICULTY) ? k_difficul_rects : k_nation_rects;
    const int count = (ng->phase == NEW_GAME_PHASE_DIFFICULTY) ? 5 : 4;

    /* Hover selects. */
    const int hover = new_game_point_in_rects(rects, count, input->mouse_x, input->mouse_y);
    if (hover >= 0) {
      ng->selection = hover;
    }

    if (input->last_key == COLONIZE_KEY_LEFT || input->last_key == COLONIZE_KEY_RIGHT ||
        input->last_key == COLONIZE_KEY_UP || input->last_key == COLONIZE_KEY_DOWN) {
      if (ng->phase == NEW_GAME_PHASE_DIFFICULTY) {
        new_game_difficul_nav(ng, input->last_key);
      } else {
        new_game_nation_nav(ng, input->last_key);
      }
      return true;
    }
    if (input->last_key == COLONIZE_KEY_ENTER || input->last_key == COLONIZE_KEY_SPACE) {
      new_game_activate_list(ng);
      return true;
    }
    if (input->mouse_left_clicked) {
      if (hover >= 0) {
        ng->selection = hover;
        /* Click portrait selects only; confirm via finished / Enter (DOS style). */
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
    if (input->last_key == COLONIZE_KEY_BACKSPACE) {
      size_t n = strlen(ng->leader_name);
      if (n > 0) {
        ng->leader_name[n - 1] = '\0';
      }
      return true;
    }
    for (int i = 0; i < input->text_input_len; ++i) {
      char ch = input->text_input[i];
      if (ch >= 32 && ch < 127) {
        size_t n = strlen(ng->leader_name);
        if (n + 1 < sizeof(ng->leader_name)) {
          ng->leader_name[n] = ch;
          ng->leader_name[n + 1] = '\0';
        }
      }
    }
    if (input->last_key == COLONIZE_KEY_ENTER) {
      new_game_advance_cinematic(ng);
      return true;
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
  if (input->last_key == COLONIZE_KEY_UP && ng->selection > 0) {
    ng->selection--;
    return true;
  }
  if (input->last_key == COLONIZE_KEY_DOWN && ng->selection + 1 < ng->option_count) {
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

static void new_game_draw_markup_line(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t normal_color,
  uint8_t emphasis_color
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
    font_draw_text(font, fb, cx, y, chbuf, color);
    const unsigned char ch = (unsigned char)*p;
    if (font && font->section_data && ch < 128 && font->char_widths[ch] > 0) {
      cx += font->char_widths[ch];
    } else {
      cx += 6;
    }
  }
}

static int new_game_text_width(const ColonizeFont* font, const char* text) {
  if (!text) {
    return 0;
  }
  int w = 0;
  for (const char* p = text; *p; ++p) {
    if (*p == '{' || *p == '}' || *p == '^' || *p == '_') {
      continue;
    }
    const unsigned char ch = (unsigned char)*p;
    if (font && font->section_data && ch < 128 && font->char_widths[ch] > 0) {
      w += font->char_widths[ch];
    } else {
      w += 6;
    }
  }
  return w;
}

static void new_game_fill_rect(ColonizeFramebuffer8* fb, int x, int y, int w, int h, uint8_t c) {
  if (!fb || !fb->pixels || w <= 0 || h <= 0) {
    return;
  }
  for (int yy = y; yy < y + h; ++yy) {
    if (yy < 0 || yy >= fb->height) {
      continue;
    }
    for (int xx = x; xx < x + w; ++xx) {
      if (xx < 0 || xx >= fb->width) {
        continue;
      }
      fb->pixels[yy * fb->width + xx] = c;
    }
  }
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
  const int prompt_h = ng->prompt_line_count * line_h;
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
  int dialog_w = ng->dialog_width;
  if (dialog_w > fb->width - 4) {
    dialog_w = fb->width - 4;
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
  for (int i = 0; i < ng->prompt_line_count; ++i) {
    new_game_draw_markup_line(font, fb, inner_x + pad_x, cy, ng->prompt_lines[i], text_color, hilite_color);
    cy += line_h;
  }

  ng->dialog_x = dialog_x;
  ng->dialog_y = dialog_y;
  ng->dialog_w = dialog_w;
  ng->dialog_h = dialog_h;
  ng->line_h = line_h;
  ng->list_y0 = cy;

  if (ng->phase == NEW_GAME_PHASE_LEADER_NAME) {
    char field[NEW_GAME_LEADER_NAME_MAX + 2];
    snprintf(field, sizeof(field), "%s_", ng->leader_name);
    new_game_draw_markup_line(font, fb, inner_x + pad_x, cy, field, text_color, hilite_color);
    return;
  }

  for (int i = 0; i < ng->option_count; ++i) {
    if (i == ng->selection) {
      new_game_fill_rect(fb, inner_x + 2, cy, inner_w - 4, line_h, select_color);
    }
    new_game_draw_markup_line(
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

static void new_game_draw_rect_border(
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  int w,
  int h,
  uint8_t color
) {
  if (!fb || !fb->pixels || w <= 0 || h <= 0) {
    return;
  }
  for (int xx = x; xx < x + w; ++xx) {
    if (xx < 0 || xx >= fb->width) {
      continue;
    }
    if (y >= 0 && y < fb->height) {
      fb->pixels[y * fb->width + xx] = color;
    }
    const int y2 = y + h - 1;
    if (y2 >= 0 && y2 < fb->height) {
      fb->pixels[y2 * fb->width + xx] = color;
    }
  }
  for (int yy = y; yy < y + h; ++yy) {
    if (yy < 0 || yy >= fb->height) {
      continue;
    }
    if (x >= 0 && x < fb->width) {
      fb->pixels[yy * fb->width + x] = color;
    }
    const int x2 = x + w - 1;
    if (x2 >= 0 && x2 < fb->width) {
      fb->pixels[yy * fb->width + x2] = color;
    }
  }
}

static void new_game_blit_woodpanl(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette
) {
  memset(fb->pixels, 4, (size_t)fb->width * (size_t)fb->height);
  if (ng->woodpanl && ng->woodpanl->pixels) {
    pik_blit(ng->woodpanl, fb, 0, 0);
    if (ng->woodpanl->has_palette && out_palette) {
      new_game_copy_palette(out_palette, &ng->woodpanl->palette);
    }
  }
}

static void new_game_render_region_pick(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t border_color
) {
  (void)text_color;
  (void)hilite_color;
  const bool difficul = (ng->phase == NEW_GAME_PHASE_DIFFICULTY);
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (difficul && ng->difficul_ok) {
    pik_blit(&ng->difficul_pik, fb, 0, 0);
    if (ng->difficul_pik.has_palette && out_palette) {
      new_game_copy_palette(out_palette, &ng->difficul_pik.palette);
    }
  } else if (!difficul && ng->nations_ok) {
    pik_blit(&ng->nations_pik, fb, 0, 0);
    if (ng->nations_pik.has_palette && out_palette) {
      new_game_copy_palette(out_palette, &ng->nations_pik.palette);
    }
  }

  const NewGameRect* rects = difficul ? k_difficul_rects : k_nation_rects;
  const int count = difficul ? 5 : 4;
  if (ng->selection >= 0 && ng->selection < count) {
    const NewGameRect* r = &rects[ng->selection];
    /* Double border like DOS highlight. */
    new_game_draw_rect_border(fb, r->x, r->y, r->w, r->h, border_color);
    new_game_draw_rect_border(fb, r->x + 1, r->y + 1, r->w - 2, r->h - 2, border_color);
  }

  /*
   * FF glyphs only anti-alias when ink is 15 or 7; any other index fills every
   * shade solid and looks bold. Use 15 for regular body text on these screens.
   */
  const ColonizeFont* font = ng->ui_font;
  const uint8_t ink = 15;
  const int line_h = font ? (font->max_height + 2) : 8;
  const char* title = ng->prompt_line_count > 0 ? ng->prompt_lines[0]
    : (difficul ? "Select a Difficulty Level" : "Select a European Power");
  const int tx = 8;
  const int ty = 10;
  new_game_draw_markup_line(font, fb, tx, ty, title, ink, ink);

  /* Difficulty / nation name centered near the bottom of each portrait. */
  for (int i = 0; i < count; ++i) {
    const char* label = NULL;
    if (i < ng->option_count && ng->options[i][0]) {
      label = ng->options[i];
    } else if (difficul) {
      static const char* k_diff[5] = {
        "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
      };
      label = k_diff[i];
    } else {
      label = k_nation_names[i];
    }
    const NewGameRect* r = &rects[i];
    const int lw = new_game_text_width(font, label);
    int lx = r->x + (r->w - lw) / 2;
    if (lx < r->x + 1) {
      lx = r->x + 1;
    }
    const int ly = r->y + r->h - line_h - 3;
    new_game_draw_markup_line(font, fb, lx, ly, label, ink, ink);
  }

  const char* finished = new_game_finished_text(ng);
  char finished_buf[80];
  if (finished[0] == '(') {
    snprintf(finished_buf, sizeof(finished_buf), "%s", finished);
  } else {
    snprintf(finished_buf, sizeof(finished_buf), "(%s)", finished);
  }
  /* Lower in the empty left column, near vertical mid-screen. */
  const int fy = 92;
  new_game_draw_markup_line(font, fb, tx, fy, finished_buf, ink, ink);
  ng->finished_x = tx;
  ng->finished_y = fy;
  ng->finished_w = new_game_text_width(font, finished_buf) + 4;
  ng->finished_h = line_h;
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
  /* Strip centering markers for measurement. */
  char prompt_clean[COLONIZE_MSG_LINE_LEN];
  size_t po = 0;
  for (const char* p = prompt; *p && po + 1 < sizeof(prompt_clean); ++p) {
    if (*p == '^' || *p == '_') {
      continue;
    }
    prompt_clean[po++] = *p;
  }
  prompt_clean[po] = '\0';

  char field[NEW_GAME_LEADER_NAME_MAX + 2];
  snprintf(field, sizeof(field), "%s_", ng->leader_name);

  const int prompt_w = new_game_text_width(font, prompt_clean);
  const int field_w = new_game_text_width(font, field);
  const int box_w = prompt_w > field_w ? prompt_w : field_w;
  const int cx = (fb->width - box_w) / 2;
  const int cy = (fb->height - line_h * 2 - 8) / 2;
  new_game_draw_markup_line(font, fb, cx, cy, prompt_clean, text_color, hilite_color);
  new_game_draw_markup_line(font, fb, cx, cy + line_h + 6, field, text_color, hilite_color);
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
  const int line_h = font ? (font->max_height + 2) : 10;
  const int margin_x = 12;
  const int margin_y = 12;
  int cy = margin_y;

  if (!section) {
    new_game_draw_markup_line(
      font, fb, margin_x, cy, "Nation history unavailable.", text_color, hilite_color
    );
    return;
  }
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0' || new_game_is_directive(line)) {
      continue;
    }
    if (cy + line_h > fb->height - margin_y) {
      break;
    }
    new_game_draw_markup_line(font, fb, margin_x, cy, line, text_color, hilite_color);
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
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (ng->kinglss_ok) {
    pik_blit(&ng->kinglss_pik, fb, 0, 0);
    if (ng->kinglss_pik.has_palette) {
      new_game_copy_palette(out_palette, &ng->kinglss_pik.palette);
    }
  }
  if (ng->nation_art_ok && ng->nation_art.sprite_count > 0) {
    ss_blit_sprite(&ng->nation_art, 0, fb, 8, 20);
  }
  if (ng->king1_ok && ng->king1.sprite_count > 0) {
    /* Large king figure — place toward left/center. */
    ss_blit_sprite(&ng->king1, 0, fb, 20, 8);
  }

  const char* section_name = (ng->nation == 3) ? "VICEROY2" : "VICEROY";
  const ColonizeMsgSection* section =
    ng->game_txt ? assets_msg_find(ng->game_txt, section_name) : NULL;
  const ColonizeFont* font = ng->fontking_ok ? &ng->fontking : ng->ui_font;
  const int line_h = font ? (font->max_height + 2) : 8;
  int tx = 232;
  int ty = 21;
  int tw = 78;
  if (section) {
    for (int i = 0; i < section->line_count; ++i) {
      const char* line = section->lines[i];
      if (!line || line[0] == '\0') {
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
        continue;
      }
      if (new_game_is_directive(line)) {
        continue;
      }
      char buf[COLONIZE_MSG_LINE_LEN];
      new_game_subst_country(buf, sizeof(buf), line, new_game_nation_name(ng->nation));
      /* Center lines marked with ^^ roughly within column */
      int x = tx;
      if (buf[0] == '^') {
        const char* body = buf;
        while (*body == '^') {
          body++;
        }
        int w = new_game_text_width(font, body);
        x = tx + (tw - w) / 2;
        if (x < tx) {
          x = tx;
        }
        new_game_draw_markup_line(font, fb, x, ty, body, text_color, hilite_color);
      } else {
        new_game_draw_markup_line(font, fb, x, ty, buf, text_color, hilite_color);
      }
      ty += line_h;
      if (ty > fb->height - line_h) {
        break;
      }
    }
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
  const int frame = ng->sail_frame;
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (frame >= 0 && frame < NEW_GAME_SAIL_FRAMES && ng->levn_ok[frame]) {
    pik_blit(&ng->levn[frame], fb, 0, 0);
    if (ng->levn[frame].has_palette) {
      new_game_copy_palette(out_palette, &ng->levn[frame].palette);
    }
  }

  char section_name[16];
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
    const char* body = buf;
    while (*body == '^') {
      body++;
    }
    int w = new_game_text_width(font, body);
    int x = (fb->width - w) / 2;
    if (tw < fb->width) {
      x = (fb->width - tw) / 2 + (tw - w) / 2;
    }
    if (x < 0) {
      x = 0;
    }
    new_game_draw_markup_line(font, fb, x, ty, body, text_color, hilite_color);
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
    case NEW_GAME_PHASE_DIFFICULTY:
    case NEW_GAME_PHASE_NATION:
      new_game_render_region_pick(
        ng, framebuffer, out_palette, text_color, hilite_color, hilite_color
      );
      break;
    case NEW_GAME_PHASE_AMERICA_CHOICE:
    case NEW_GAME_PHASE_MAP_PICK:
      memset(framebuffer->pixels, 1, (size_t)framebuffer->width * (size_t)framebuffer->height);
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
