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
#include "core/unit_chrome_draw.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

#define NEW_GAME_SAIL_FRAME_MS 1600u
/* FUN_75c2_10ae 75c2:118a PUSH 0x17: the @LEADERNAME field length (DS:0xa5b6). */
#define NEW_GAME_LEADER_FIELD_LEN 0x17

static const char* k_nation_ss[4] = {"ENGLND1.SS", "FRANCE1.SS", "SPAIN1.SS", "DUTCH1.SS"};

typedef struct NewGameRect {
  int x;
  int y;
  int w;
  int h;
} NewGameRect;

/* DIFFICUL.PIK cells: FUN_733a_04d0 (raw 118311) origin x = ((i + 1) % 3) * 0x69 + 0x17,
 * y = ((i + 1) / 3) * 0x60 + 7; FUN_733a_0512 outline x..x+0x43, y..y+0x59. */
static const NewGameRect k_difficul_rects[5] = {
  {128, 7, 0x44, 0x5a},
  {233, 7, 0x44, 0x5a},
  {23, 103, 0x44, 0x5a},
  {128, 103, 0x44, 0x5a},
  {233, 103, 0x44, 0x5a},
};

/* NATIONS.PIK cells: FUN_733a_0992 (raw 118542) origin x = (i % 2) * 99 + 0x70,
 * y = (i / 2) * 0x5b + 0xd; FUN_733a_09c6 outline x..x+0x57, y..y+0x51. */
static const NewGameRect k_nation_rects[4] = {
  {112, 13, 0x58, 0x52},
  {211, 13, 0x58, 0x52},
  {112, 104, 0x58, 0x52},
  {211, 104, 0x58, 0x52},
};

/* CUSTOMIZ.PIK: 4 columns (params) × 3 rows (values). FUN_733a_0000. */
static const NewGameRect k_customiz_rects[4][3] = {
  {{10, 16, 72, 48}, {10, 76, 72, 48}, {10, 135, 72, 48}},
  {{86, 16, 72, 48}, {86, 76, 72, 48}, {86, 135, 72, 48}},
  {{162, 16, 72, 48}, {162, 76, 72, 48}, {162, 135, 72, 48}},
  {{238, 16, 72, 48}, {238, 76, 72, 48}, {238, 135, 72, 48}},
};

/* FUN_733a_0512 local_5a: Discoverer..Viceroy palette indices. */
static const uint8_t k_difficul_colors[5] = {10, 9, 14, 13, 12};

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

/* GAME.TXT @MYLEADER row `nation` (0..3): King/King/King/Stadtholder. */
static const char* new_game_nation_ruler_title(
  const ColonizeMsgCatalog* game_txt, int nation
) {
  static char buf[32];
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  if (game_txt && assets_msg_row_field(game_txt, "MYLEADER", nation, 0, buf, sizeof(buf))) {
    return buf;
  }
  return "";
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
    font_file_free(&ng->fontking);
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
        font_file_load(path, &ng->fontking, err, sizeof(err))) {
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
  ng->dialog_width = 0x50; /* FUN_6f74_06d0 box[+0x28] default */
  ng->pref_dialog_x = -1;
  ng->pref_dialog_y = -1;
  memset(ng->options, 0, sizeof(ng->options));
  memset(ng->prompt_lines, 0, sizeof(ng->prompt_lines));
}

/* FUN_7314_0106 (raw 117915): every '_' of a GAME.TXT line is read as a space. */
static void new_game_copy_line(char* dst, size_t n, const char* src) {
  str_copy_trunc(dst, n, src);
  for (char* p = dst; *p; ++p) {
    if (*p == '_') {
      *p = ' ';
    }
  }
}

/*
 * FUN_6f74_32a4 (raw 117247) over one GAME.TXT section: prose (state 1) until a blank line
 * or @OPTIONS / @PROMPT, then one row per line (state 2) until the next blank; @TEXT goes
 * back to prose. @WIDTH sets box[+0x28] as given (FUN_6f74_0c22, no floor), @X / @Y the
 * origin, @DEFAULT the pre-selected row. A blank before any prose does not count, as in
 * popup_msg_choices.
 */
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
  int state = 1;
  bool saw_prose = false;
  for (int i = 0; i < section->line_count && state < 3; ++i) {
    const char* line = section->lines[i];
    if (!line) {
      continue;
    }
    if (saw_prose && section->blank_before && section->blank_before[i] && ++state > 2) {
      break;
    }
    if (new_game_is_directive(line)) {
      const char* d = line + 1;
      if (strcasecmp(d, "options") == 0 || strcasecmp(d, "prompt") == 0) {
        state = 2;
      } else if (strcasecmp(d, "text") == 0) {
        state = 1;
      } else if (strncasecmp(d, "width=", 6) == 0) {
        ng->dialog_width = atoi(d + 6);
      } else if (strncasecmp(d, "x=", 2) == 0) {
        ng->pref_dialog_x = atoi(d + 2);
      } else if (strncasecmp(d, "y=", 2) == 0) {
        ng->pref_dialog_y = atoi(d + 2);
      } else if (strncasecmp(d, "default=", 8) == 0 && isdigit((unsigned char)d[8])) {
        ng->selection = atoi(d + 8) - 1;
        if (ng->selection < 0) {
          ng->selection = 0;
        }
      }
      continue;
    }
    if (state == 1) {
      saw_prose = true;
      if (ng->prompt_line_count < NEW_GAME_PROMPT_MAX) {
        new_game_copy_line(
          ng->prompt_lines[ng->prompt_line_count++], sizeof(ng->prompt_lines[0]), line
        );
      }
    } else if (ng->option_count < NEW_GAME_OPTION_MAX) {
      new_game_copy_line(ng->options[ng->option_count++], sizeof(ng->options[0]), line);
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
    "", "", "", ""
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
    ng->prompt_lines[0][0] = '\0';
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
  ng->prompt_lines[0][0] = '\0';
  ng->prompt_line_count = 1;
}

static void new_game_enter_nation(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_NATION;
  new_game_ensure_nations(ng);
  new_game_load_choice_section(ng, "PICKNATION");
  if (ng->option_count == 0) {
    ng->prompt_lines[0][0] = '\0';
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
  /* FUN_75c2_10ae (75c2:118a-119d): FUN_291f_0120 on GAME @LEADERNAME, field length 0x17;
   * the row after @OPTIONS is the field label (options[0]). */
  new_game_load_choice_section(ng, "LEADERNAME");
}

static void new_game_enter_lore(NewGameWizard* ng, bool page_b) {
  ng->phase = page_b ? NEW_GAME_PHASE_NATION_LORE_B : NEW_GAME_PHASE_NATION_LORE_A;
  new_game_ensure_nations(ng);
  /* FUN_75c2_10ae: "NATION0A" (DS:0x219d) with byte 6 += nation, byte 7 += 1 for page B. */
  char section_name[16];
  snprintf(section_name, sizeof(section_name), "NATION%d%c", ng->nation, page_b ? 'B' : 'A');
  new_game_load_choice_section(ng, section_name);
}

static void new_game_subst_country(char* dst, size_t dst_size, const char* src, const char* country);

static void new_game_enter_king(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_KING;
  new_game_ensure_king(ng);
  new_game_load_choice_section(ng, (ng->nation == 3) ? "VICEROY2" : "VICEROY");
  for (int i = 0; i < ng->prompt_line_count; ++i) {
    char buf[COLONIZE_MSG_LINE_LEN];
    new_game_subst_country(buf, sizeof(buf), ng->prompt_lines[i], new_game_nation_name(ng->nation));
    str_copy_trunc(ng->prompt_lines[i], sizeof(ng->prompt_lines[0]), buf);
  }
}

static void new_game_enter_sail(NewGameWizard* ng) {
  ng->phase = NEW_GAME_PHASE_SAIL;
  ng->sail_frame = 0;
  ng->sail_accum_ms = 0;
  new_game_ensure_sail_frame(ng, 0);
}

static void new_game_scan_mp_files(NewGameWizard* ng) {
  /* Title and @width come from GAME.TXT @MAPTOLOAD, like every other wizard
   * page; the options are the .MP files on disk, appended below. */
  new_game_load_choice_section(ng, "MAPTOLOAD");
  ng->option_count = 0;

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
    if (ng->option_count == 0) {
      ng->options[0][0] = '\0';
      ng->options[1][0] = '\0';
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

/*
 * LABELS.TXT @MISC, addressed relative to the "Land Mass" anchor the
 * CUSTOMIZE labels already key off (row 144 in the shipped file):
 *
 *   +16 CUSTOMIZE NEW WORLD   +17 Click Here When Finished
 *   +18 Choose                +19 Difficulty Level   +20 Level
 *   +21..25 Easiest..Toughest
 *   +26 Select                +27 European Power     +28 Power
 *   +29..32 Immigration / Cooperation / Conquest / Trade
 *
 * Anchoring beats a bare index for the same reason new_game_customiz_labels
 * does it: the block moves as a unit if the file is edited. NULL catalog or a
 * missing anchor falls back to the built-in English.
 */
#define NEW_GAME_MISC_BASE_ROW 144 /* @MISC "Land Mass", first row of the wizard block */

static const char* new_game_misc_label(
  const NewGameWizard* ng, int offset, const char* fallback
) {
  if (!ng || !ng->labels_txt) {
    return fallback;
  }
  const ColonizeMsgSection* misc = assets_msg_find(ng->labels_txt, "MISC");
  if (!misc) {
    return fallback;
  }
  const int row = NEW_GAME_MISC_BASE_ROW + offset;
  if (row >= 0 && row < misc->line_count && misc->lines[row][0]) {
    return misc->lines[row];
  }
  return fallback;
}

static const char* new_game_finished_text(const NewGameWizard* ng) {
  return new_game_misc_label(ng, 17, "");
}

/* Resolve CUSTOMIZE labels from @MISC starting at "Land Mass"; a miss is "". */
static void new_game_customiz_labels(
  const NewGameWizard* ng,
  const char** out_cats,
  const char** out_vals,
  const char** out_title
) {
  for (int c = 0; c < 4; ++c) {
    out_cats[c] = "";
    for (int r = 0; r < 3; ++r) {
      out_vals[c * 3 + r] = "";
    }
  }
  *out_title = "";
  if (!ng || !ng->labels_txt) {
    return;
  }
  const ColonizeMsgSection* misc = assets_msg_find(ng->labels_txt, "MISC");
  if (!misc) {
    return;
  }
  const int start = NEW_GAME_MISC_BASE_ROW;
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
        /* port_bugs.md #283: clicking the already-selected entry confirms like
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
      &ng->leader_edit, ng->leader_name, NEW_GAME_LEADER_FIELD_LEN + 1, input
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

  /* Remaining list dialogs (America / map pick): FUN_6f74_2580 rows wrap (port_bugs.md #1212). */
  if (popup_list_key_step(input->last_key, &ng->selection, ng->option_count, 0)) {
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

/*
 * The wizard's picture screens write through the resident FUN_104b_* writers, one blit
 * through {skip, c1, c2, c3} per call and no drop shadow (port_bugs.md #1198):
 * FUN_281f_0100 -> FUN_104b_0318 (FONTTINY, raw 1007, map {skip, c, c, c}) and
 * FUN_281f_01c8 -> FUN_104b_0430 (FONTINTR, raw 1063, map {skip, c1, c2, 0}). Both
 * centre as x + (w >> 1) - (measure >> 1), floored at 0, the measures FUN_104b_0216 /
 * 0232 being the FUN_1c2a_0006 width - 1.
 */
static int new_game_dos_center_x(const ColonizeFont* font, const char* text, int x, int w) {
  const int measure = font_text_width_skip(font, text, FONT_SKIP_NONE) - 1;
  const int cx = x + (w >> 1) - (measure >> 1);
  return cx < 0 ? 0 : cx;
}

static void new_game_draw_centered(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  const char* text,
  int x,
  int w,
  int y,
  uint8_t c1,
  uint8_t c2,
  uint8_t c3
) {
  if (!font || !fb || !text || !text[0]) {
    return;
  }
  const uint8_t map[4] = {0, c1, c2, c3};
  font_draw_text_shaded(font, fb, new_game_dos_center_x(font, text, x, w), y, text, map);
}

/* A selected cell's line (FUN_733a_0512 / 09c6 / 002c): FUN_281f_0100 twice, ink 0 at
 * x + 1, then the cell ink at x, same y and w (733a:061b/0638, 0aaa/0ac7, 010e/012b). */
static void new_game_draw_cell_line(
  const ColonizeFont* font, ColonizeFramebuffer8* fb, const char* text, int x, int w, int y,
  uint8_t ink
) {
  new_game_draw_centered(font, fb, text, x + 1, w, y, 0, 0, 0);
  new_game_draw_centered(font, fb, text, x, w, y, ink, ink, ink);
}

/*
 * The prose lines as one FUN_6f74_1198 flow, encoded like popup_msg_section_body: a caret
 * line ('^' / '^^', FUN_6f74_0c32) is a row of its own behind POPUP_MSG_LINE_MARK /
 * POPUP_MSG_CENTER_MARK, every other line joins the paragraph with a space.
 */
static void new_game_prompt_body(const NewGameWizard* ng, char* out, size_t n) {
  size_t used = 0;
  out[0] = '\0';
  for (int i = 0; i < ng->prompt_line_count && used + 1 < n; ++i) {
    const char* rest = ng->prompt_lines[i];
    const int caret = popup_msg_caret_flags(rest, &rest);
    const bool fresh = used == 0 || out[used - 1] == '\n';
    int k;
    if (caret) {
      k = snprintf(
        out + used, n - used, "%s%c%s\n", fresh ? "" : "\n",
        caret == POPUP_MSG_CARET_CENTER ? POPUP_MSG_CENTER_MARK : POPUP_MSG_LINE_MARK, rest
      );
    } else {
      k = snprintf(out + used, n - used, "%s%s", fresh ? "" : " ", rest);
    }
    used = k < 0 ? used : (used + (size_t)k < n ? used + (size_t)k : n - 1);
  }
}

/*
 * A GAME.TXT section dialog as FUN_6f74_14c6 lays it out (raw 115882; FUN_281f_03fe ->
 * FUN_6f74_2580 -> 248e): content = max(box[+0x28] (@WIDTH), widest row as FUN_6f74_0a00
 * measures it = text + 2 * box[+0x48] + box[+0x22] (4)); prose wrapped to content -
 * 2 * box[+0x48] (FUN_6f74_1198, popup_wrap_text_dos), lines glyph_h + 1 apart, '^^' rows
 * shifted by popup_dos_center_dx; rows glyph_h + box[+0x46] apart from box[+0x26].
 * h = box[+0x46] + 2 * box[+0x2a] + list (+ box[+0x46] when list != 0), w = content +
 * 2 * box[+0x2a]; origin @X / @Y or 160 - (w >> 1) / 100 - (h >> 1), right / bottom
 * edges pulled back to 320 / 200. Framed: box[+0x2a] 3, [+0x46] 3, [+0x48] 2 and the
 * WOODTILE frame (popup_draw). `frameless` = box flag 0x10 (FUN_281f_040a ->
 * FUN_6f74_37f6, DS:0x1f56 |= 0x18; FUN_6f74_06d0 zeroes [+0x46] / [+0x48], 14c6 [+0x2a]):
 * the @LEADERNAME / @NATIONnA/B prompts of FUN_75c2_10ae and the king's audience
 * FUN_75c2_20e2. Row bar: FUN_6f74_1b7c, x + [+0x2a] + [+0x48] - 1, content - 2 wide,
 * glyph_h + 2 tall from the row top; row text one px lower, indented box[+0x22].
 */
static void new_game_render_dialog(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  const ColonizeFont* font,
  bool frameless,
  const ColonizePopupColors* colors,
  uint8_t text_color,
  uint8_t hilite_color,
  uint8_t select_color
) {
  enum { ROW_DX = 4 }; /* box[+0x22], FUN_6f74_06d0 default */
  const int fr = frameless ? 0 : 3;
  const int sp = frameless ? 0 : 3;
  const int margin = frameless ? 0 : 2;
  const int gh = popup_dos_glyph_h(font);
  int content = ng->dialog_width;
  for (int i = 0; i < ng->option_count && font; ++i) {
    const int w = popup_markup_text_width(font, ng->options[i]) + 2 * margin + ROW_DX;
    content = w > content ? w : content;
  }
  char body[NEW_GAME_PROMPT_MAX * COLONIZE_MSG_LINE_LEN];
  new_game_prompt_body(ng, body, sizeof(body));
  char lines[NEW_GAME_PROMPT_MAX][COLONIZE_MSG_LINE_LEN];
  bool centred[NEW_GAME_PROMPT_MAX];
  memset(centred, 0, sizeof(centred));
  const int n = font ? popup_wrap_text_dos(
                         font, body, lines[0], sizeof(lines[0]), centred, NEW_GAME_PROMPT_MAX,
                         content - 2 * margin
                       )
                     : 0;
  const int prose_h = n * (gh + 1);
  const int list = prose_h + (gh + sp) * ng->option_count;
  const int h = sp + 2 * fr + list + (list ? sp : 0);
  const int w = content + 2 * fr;
  int x = ng->pref_dialog_x >= 0 ? ng->pref_dialog_x : fb->width / 2 - (w >> 1);
  int y = ng->pref_dialog_y >= 0 ? ng->pref_dialog_y : fb->height / 2 - (h >> 1);
  x -= x + w > fb->width ? x + w - fb->width : 0;
  y -= y + h > fb->height ? y + h - fb->height : 0;
  if (!frameless) {
    popup_draw(fb, x, y, w, h, ng->wood_tile, colors, NULL, NULL, NULL, NULL);
  }
  const int tx = x + fr + margin;
  bool emph = false; /* DS:0x1f62, carried across every line and row */
  for (int i = 0; i < n; ++i) {
    const int lx = tx + (centred[i] ? popup_dos_center_dx(font, lines[i], content - 2 * margin) : 0);
    popup_draw_text_markup(
      font, fb, lx, y + fr + sp + i * (gh + 1), lines[i], text_color, hilite_color, colors, &emph
    );
  }
  ng->dialog_x = x;
  ng->dialog_y = y;
  ng->dialog_w = w;
  ng->dialog_h = h;
  ng->line_h = gh + sp;
  ng->list_y0 = y + fr + sp + (prose_h ? sp + prose_h : 0);
  for (int i = 0; i < ng->option_count; ++i) {
    const int row_y = ng->list_y0 + i * ng->line_h + 1;
    if (i == ng->selection) {
      fb_fill_rect(fb, tx - 1, row_y - 1, content - 2, gh + 2, select_color);
    }
    popup_draw_text_markup(
      font, fb, tx + ROW_DX, row_y, ng->options[i], text_color, hilite_color, colors, &emph
    );
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

/* FUN_733a_09c6 ink: DS:0x848[nation], NAMES.TXT @COUNTRY's colour column (read at
 * 75c2:18d5; 12/9/14/13 in the shipped file). */
static uint8_t new_game_nation_ink(const NewGameWizard* ng, int nation) {
  char buf[8];
  if (ng && ng->names_txt &&
      assets_msg_row_field(ng->names_txt, "COUNTRY", nation, 1, buf, sizeof(buf)) && buf[0]) {
    return (uint8_t)atoi(buf);
  }
  return unit_chrome_names_color(nation);
}

static void new_game_format_finished(const NewGameWizard* ng, char* out, size_t out_size) {
  const char* finished = new_game_finished_text(ng);
  if (finished[0] == '(') {
    snprintf(out, out_size, "%s", finished);
  } else {
    snprintf(out, out_size, "(%s)", finished);
  }
}

/* "(" DS:0x2efc ")" through FUN_281f_0100 in ink 0xfe, plus the port's hit box. */
static void new_game_draw_finished(
  NewGameWizard* ng, const ColonizeFont* tiny, ColonizeFramebuffer8* fb, int x, int w, int y
) {
  char buf[80];
  new_game_format_finished(ng, buf, sizeof(buf));
  new_game_draw_centered(tiny, fb, buf, x, w, y, 0xfe, 0xfe, 0xfe);
  ng->finished_x = new_game_dos_center_x(tiny, buf, x, w);
  ng->finished_y = y;
  ng->finished_w = font_text_width_skip(tiny, buf, FONT_SKIP_NONE) + 4;
  ng->finished_h = (tiny ? tiny->max_height : 6) + 2;
}

static void new_game_upper(char* s) {
  for (; *s; ++s) {
    *s = (char)toupper((unsigned char)*s);
  }
}

/*
 * Difficulty (FUN_733a_06a4, raw 118390) and nation (FUN_733a_0b3e, raw 118601) screens.
 * Two FONTINTR titles through FUN_104b_0430 {skip, 0xfe, 0xfd, 0}, centred in
 * x 0x17 w 0x44 / x 0 w 0x70 at y0 = 0x14 / 0x28 - (h >> 1) and y0 + h + 4; the finished
 * line FONTTINY 0xfe at y 0x51 / 0xb6. Only the selected cell (DS:0x53a6 / DS:0x5398) is
 * outlined and labelled: strupr(name) + ":" and the bonus line, each a cell line
 * (FUN_733a_0512 raw 118332: y + 0x2c - h and y + 0x2e; FUN_733a_09c6 raw 118554: y + 2
 * and y + 0x50 - h).
 */
static void new_game_render_region_pick(
  NewGameWizard* ng, ColonizeFramebuffer8* fb, ColonizePalette* out_palette
) {
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
  const ColonizeFont* intr = ng->ui_font;
  const int title_x = difficul ? 0x17 : 0;
  const int title_w = difficul ? 0x44 : 0x70;
  const int intr_h = intr ? intr->max_height : 0;
  const int y0 = (difficul ? 0x14 : 0x28) - (intr_h >> 1);
  const int y1 = y0 + intr_h + 4;
  new_game_draw_centered(
    intr, fb, new_game_misc_label(ng, difficul ? 18 : 26, ""), title_x, title_w, y0, 0xfe, 0xfd, 0
  );
  new_game_draw_centered(
    intr, fb, new_game_misc_label(ng, difficul ? 19 : 27, ""), title_x, title_w, y1, 0xfe, 0xfd, 0
  );
  new_game_draw_finished(ng, tiny, fb, title_x, title_w, difficul ? 0x51 : 0xb6);

  const int sel = ng->selection;
  if (sel < 0 || sel >= count) {
    return;
  }
  const NewGameRect* r = &rects[sel];
  const uint8_t ink = difficul ? k_difficul_colors[sel] : new_game_nation_ink(ng, sel);
  fb_rect_outline(fb, r->x, r->y, r->w, r->h, ink, ink);
  const int tiny_h = tiny ? tiny->max_height : 6;
  char top[32];
  snprintf(
    top, sizeof(top), "%s:", difficul ? reports_difficulty_title(sel) : new_game_nation_name(sel)
  );
  new_game_upper(top);
  const char* bottom = new_game_misc_label(ng, (difficul ? 21 : 29) + sel, "");
  new_game_draw_cell_line(
    tiny, fb, top, r->x, r->w, difficul ? r->y + 0x2c - tiny_h : r->y + 2, ink
  );
  new_game_draw_cell_line(
    tiny, fb, bottom, r->x, r->w, difficul ? r->y + 0x2e : r->y + 0x50 - tiny_h, ink
  );
}

/*
 * Customize (FUN_733a_01a4, raw 118139): title DS:0x2efa FONTINTR {skip, 0xfe, 0xfd, 0}
 * centred in 0..0x140 at y 4, the finished line FONTTINY 0xfe at y 0xbe, then each column's
 * current-value cell (FUN_733a_002c, raw 118092) is outlined in 0x0a, 0x0e for the focus
 * column (DS:0xa60a), with label + ":" (DS:0x2020) at y + 0x17 - h and the value at
 * y + 0x19 as cell lines.
 */
static void new_game_render_customize(
  NewGameWizard* ng, ColonizeFramebuffer8* fb, ColonizePalette* out_palette
) {
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (ng->customiz_ok) {
    new_game_blit_bg(&ng->customiz_pik, fb, out_palette);
  }

  const char* cats[4];
  const char* vals[12];
  const char* title = "";
  new_game_customiz_labels(ng, cats, vals, &title);

  const ColonizeFont* tiny = ng->tiny_font ? ng->tiny_font : ng->ui_font;
  const int tiny_h = tiny ? tiny->max_height : 6;
  new_game_draw_centered(ng->ui_font, fb, title, 0, 0x140, 4, 0xfe, 0xfd, 0);
  new_game_draw_finished(ng, tiny, fb, 0, 0x140, 0xbe);
  /* Port hit strip: the whole bottom band. */
  ng->finished_x = 0;
  ng->finished_y = 185;
  ng->finished_w = fb->width;
  ng->finished_h = fb->height - 185;

  for (int c = 0; c < 4; ++c) {
    const int row = new_game_gen_param_value(&ng->gen_params, c);
    const NewGameRect* rect = &k_customiz_rects[c][row];
    const uint8_t ink = (c == ng->customize_focus) ? 0x0e : 0x0a;
    fb_rect_outline(fb, rect->x, rect->y, rect->w, rect->h, ink, ink);
    char top[48];
    snprintf(top, sizeof(top), "%s:", cats[c]);
    new_game_draw_cell_line(tiny, fb, top, rect->x, rect->w, rect->y + 0x17 - tiny_h, ink);
    new_game_draw_cell_line(tiny, fb, vals[c * 3 + row], rect->x, rect->w, rect->y + 0x19, ink);
  }
}

static void new_game_render_leader_name(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette,
  const ColonizePopupColors* popup_colors,
  uint8_t text_color
) {
  new_game_blit_woodpanl(ng, fb, out_palette);
  /* FUN_75c2_10ae (75c2:1143-119d): FUN_281f_040a makes the box frameless, WOODPANL goes
   * up, then FUN_291f_0120 (FUN_6f74_37fc) shows @LEADERNAME with field length 0x17: one
   * FUN_6f74_0538 pass through the @COLORS map (port_bugs.md #1198, #1210). */
  char body[NEW_GAME_PROMPT_MAX * COLONIZE_MSG_LINE_LEN];
  new_game_prompt_body(ng, body, sizeof(body));
  PopupFieldGeom g;
  popup_field_frame_frameless(
    fb, ng->ui_font, popup_colors, ng->dialog_width, body,
    ng->option_count > 0 ? ng->options[0] : "", NEW_GAME_LEADER_FIELD_LEN, text_color, &g
  );
  ng->leader_field_font = ng->ui_font;
  ng->leader_field_x = g.text_x;
  ng->leader_field_y = g.text_y;
  ng->leader_field_h = g.text_h;
  TextEditColors edit_colors;
  text_edit_default_colors(&edit_colors);
  edit_colors.text = text_color;
  text_edit_render(
    &ng->leader_edit, ng->leader_name, ng->ui_font, fb, g.text_x, g.text_y, g.text_h, &edit_colors
  );
}

#define NEW_GAME_LORE_MAX_OUT 48

/* @NATIONnA / B (FUN_75c2_10ae, 75c2:1233 / 1289): frameless FUN_281f_03fe dialogs over
 * WOODPANL, one FUN_6f74_0538 pass through the @COLORS map (port_bugs.md #1198, #1210). */
static void new_game_render_lore(
  NewGameWizard* ng,
  ColonizeFramebuffer8* fb,
  ColonizePalette* out_palette,
  const ColonizePopupColors* popup_colors,
  uint8_t text_color,
  uint8_t hilite_color
) {
  new_game_blit_woodpanl(ng, fb, out_palette);
  new_game_render_dialog(
    ng, fb, ng->lore_font ? ng->lore_font : ng->ui_font, true, popup_colors, text_color,
    hilite_color, 0
  );
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

/*
 * FUN_75c2_20e2 (raw 121349) swaps the dialog style around its 03fe call (75c2:22b6-22c2):
 * DS:0x1f4a ink 0xf2, 0x1f50 enhance 0x2f, 0x1f52 shadow 0, restored after; 0x1f4e keeps
 * @COLORS hilite. FONTKING draws its body in shade 3, so the body comes out black.
 */
#define NEW_GAME_KING_INK 0xf2

static ColonizePopupColors new_game_king_colors(void) {
  ColonizePopupColors c;
  popup_colors_from_ui(&c);
  c.enhance = 0x2f;
  c.shadow = 0;
  return c;
}

static void new_game_render_king(
  NewGameWizard* ng, ColonizeFramebuffer8* fb, ColonizePalette* out_palette
) {
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (ng->kinglss_ok) {
    new_game_blit_bg(&ng->kinglss_pik, fb, out_palette);
  }
  /* FUN_75c2_20e2 (75c2:21a3 / 2215): the nation's ENGLND1.. sheet, then KING1, each
   * FUN_281f_02f8 -> FUN_1c56_0004 at 100% on its sprite's header anchor (sheet +0x46 /
   * +0x48): x = anchor_x - (w >> 1), y = anchor_y - h + 1 (port_bugs.md #1210). */
  if (ng->nation_art_ok) {
    ss_blit_anchored(&ng->nation_art, 0, fb, 0, 0);
  }
  if (ng->king1_ok) {
    ss_blit_anchored(&ng->king1, 0, fb, 0, 0);
  }
  /* 75c2:2287-22c2: FONTKING, the 0xf2 / 0x2f / 0 style, frameless GAME @VICEROY at its
   * @X / @Y / @WIDTH. */
  const ColonizePopupColors king_colors = new_game_king_colors();
  new_game_render_dialog(
    ng, fb, ng->fontking_ok ? &ng->fontking : ng->ui_font, true, &king_colors, NEW_GAME_KING_INK,
    COLONIZE_COL_HILITE, 0
  );
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
        font_file_load(path, &ng->fontking, err, sizeof(err))) {
      ng->fontking_ok = true;
    }
  }

  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (ng->kinglss_ok) {
    new_game_blit_bg(&ng->kinglss_pik, fb, out_palette);
  }
  /* Same FUN_75c2_20e2 blits as the wizard audience: banners, then KINGLOSE / KINGWIN,
   * each on its sprite's header anchor (new_game_render_king). */
  if (ng->nation_art_ok) {
    ss_blit_anchored(&ng->nation_art, 0, fb, 0, 0);
  }
  if (ng->endking_ok) {
    ss_blit_anchored(&ng->endking, 0, fb, 0, 0);
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
  /* The same frameless FUN_6f74_1198 flow as the wizard's audience text. */
  const int n_out =
    popup_wrap_text_dos(font, body, out[0], sizeof(out[0]), NULL, NEW_GAME_LORE_MAX_OUT, text_w);
  const int line_h = font->max_height + 1;
  const ColonizePopupColors king_colors = new_game_king_colors();
  bool hilite = false;
  int y = text_y;
  for (int i = 0; i < n_out; ++i) {
    if (y > fb->height - line_h) {
      break;
    }
    if (out[i][0]) {
      popup_draw_text_markup(
        font, fb, text_x, y, out[i], NEW_GAME_KING_INK, COLONIZE_COL_HILITE, &king_colors, &hilite
      );
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
  NewGameWizard* ng, ColonizeFramebuffer8* fb, ColonizePalette* out_palette
) {
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
  const char* ruler = new_game_nation_ruler_title(ng->game_txt, ng->nation);

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

  /* FUN_1262_0152 (raw 2982) swaps DS:0x1f4a to 0x0e and 0x1f50 to 0x36 around the
   * @BUILDn 03fe call (1262:02b6 / 02bc / 02c5); shadow keeps @COLORS 47. One
   * FUN_6f74_0538 pass, no drop shadow (port_bugs.md #1198). */
  ColonizePopupColors sail_colors;
  popup_colors_from_ui(&sail_colors);
  sail_colors.enhance = 0x36;

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
    popup_draw_text_markup(font, fb, x, ty, body, 0x0e, COLONIZE_COL_HILITE, &sail_colors, NULL);
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
      new_game_render_customize(ng, framebuffer, out_palette);
      break;
    case NEW_GAME_PHASE_DIFFICULTY:
    case NEW_GAME_PHASE_NATION:
      new_game_render_region_pick(ng, framebuffer, out_palette);
      break;
    case NEW_GAME_PHASE_AMERICA_CHOICE:
    case NEW_GAME_PHASE_MAP_PICK:
      /* Black field; the caller sets the game palette (WOODPANL family) and WOODTILE. */
      memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);
      new_game_render_dialog(
        ng, framebuffer, ng->ui_font, false, popup_colors, text_color, hilite_color, select_color
      );
      break;
    case NEW_GAME_PHASE_LEADER_NAME:
      new_game_render_leader_name(ng, framebuffer, out_palette, popup_colors, text_color);
      break;
    case NEW_GAME_PHASE_NATION_LORE_A:
    case NEW_GAME_PHASE_NATION_LORE_B:
      new_game_render_lore(ng, framebuffer, out_palette, popup_colors, text_color, hilite_color);
      break;
    case NEW_GAME_PHASE_KING:
      new_game_render_king(ng, framebuffer, out_palette);
      break;
    case NEW_GAME_PHASE_SAIL:
      new_game_render_sail(ng, framebuffer, out_palette);
      break;
    default:
      break;
  }
}
