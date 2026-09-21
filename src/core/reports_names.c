/*
 * NAMES.TXT / LABELS.TXT display-name lookups.
 *
 * Split out of reports.c 2026-09-16 so the simulation side (turn, units,
 * colony, the AI planners, col1_bridge, europe) can read DOS display names
 * without linking the report *screens*. Pure move: every function below is
 * byte-identical to its reports.c original — only reports_names_load_catalogs
 * is new, and it is exactly the two catalog blocks that used to sit inline in
 * reports_load, called from the same point.
 *
 * This file is SIM/SHARED. It must not call fb_/font_/ss_/pik_.
 */
#include "core/reports.h"
#include "core/reports_names.h"

#include "core/assets.h"
#include "core/strutil.h"

#include <stdio.h>
#include <string.h>

#include "platform/platform.h"
#include "platform/diagnostics.h"

/*
 * The one live NAMES.TXT parse, loaded in reports_load and shared by every
 * accessor below.
 *
 * There are no built-in name tables behind these any more (2026-09-20): the
 * port ships none of the game's own wording, so a name that the catalog does
 * not supply comes back as the empty string and the caller draws nothing.
 * assets_validate_required_files makes a missing NAMES.TXT fatal at startup,
 * so an empty return means a modded or truncated section, not a normal run.
 */
static ColonizeMsgCatalog g_reports_names;
static bool g_reports_names_ok = false;

/* The one live LABELS.TXT parse; same no-fallback rule as above. */
static ColonizeMsgCatalog g_reports_labels;
static bool g_reports_labels_ok = false;


/*
 * @JOB row count. The number of professions is structure, not wording, so it
 * stays a constant; the names themselves are read live.
 */
const int k_job_count = 28;


/* NAMES.TXT @TRIBES column 0 (plural display name — golden: indian.png
 * "Arawaks:" / "Cherokee:"). Only Inca/Aztec/Arawak actually change in
 * plural; the rest are already the same word. */



/*
 * Shared fallback tables for the NAMES.TXT-backed name accessors below.
 * Every one of these was retyped in a dozen other files before the
 * 2026-09-14 duplication pass (audit theme D); the literals here are the
 * shipped COLONIZE/NAMES.TXT rows and are only reached when no catalog is
 * loaded (slim unit-test targets, tools).
 */

/* @COUNTRY column 0. The rows carry a trailing ", <colour>" number that
 * reports_names_field drops with the rest of the line at the first comma.
 * "Netherlands", never "Holland" (turn.c:3025 was the odd one out). */

/* @HOMEPORT column 0. France is La Rochelle — new_game.c's "Paris" was
 * wrong (audit SC-7). */

/* @DIFFICULTY column 0. */

/* @TRIBES column 1 (singular). Column 0 is the plural k_tribe_names above. */

/* @BUILDING rows 0..2 column 0 — the fortification chain, low tier first. */

/* @UNIT rows 19..22 column 0 — the Indian arms/mounts ladder. The DOS
 * spellings are abbreviated ("Mtd."), and these strings double as
 * units_find_type keys, so they must stay the @UNIT text. */
#define REPORTS_UNIT_ROW_BRAVES 19

/* @UNIT rows 0..5 column 0 — the Europe dock immigrant types, in DOS
 * dos_type order (europe.h EUROPE_DOCK_TYPE_*). */
/*
 * 0-based line index within LABELS.TXT `@MISC` for each report title,
 * `k_report_titles` order (Religious/Congress/Labor/Economic/Colony/Naval/
 * Foreign/Indian/Score). Computed 2026-08-26 by counting non-blank,
 * non-comment `@MISC` lines directly (`COLONIZE/LABELS.TXT`) — re-derive
 * the same way if the asset ever changes.
 */
static const int k_report_title_labels_index[COLONIZE_REPORT_COUNT] = {
  30, 37, 49, 50, 51, 52, 93, 29, 114
};

/*
 * Nth non-blank, non-comment (';') line of `section` in g_reports_labels —
 * LABELS.TXT has no comma columns (unlike NAMES.TXT/reports_names_field),
 * just one string per line. NULL when the live catalog, section, or index
 * isn't available. Static return buffer: use/copy before the next call.
 */
const char* reports_labels_field(const char* section, int index) {
  if (!g_reports_labels_ok || index < 0) {
    return NULL;
  }
  const ColonizeMsgSection* sec = assets_msg_find(&g_reports_labels, section);
  if (!sec) {
    return NULL;
  }
  int i2 = 0;
  for (int i = 0; i < sec->line_count; ++i) {
    const char* line = sec->lines[i];
    if (!line || line[0] == '\0' || line[0] == ';') {
      continue;
    }
    if (i2 != index) {
      i2++;
      continue;
    }
    static char live[64];
    str_copy_trunc(live, sizeof(live), line);
    return live;
  }
  return NULL;
}

/*
 * `@MISC` word copied into the caller's buffer (so two words can be composed
 * into one line without aliasing reports_labels_field's static buffer);
 * `fallback` when the live catalog isn't loaded.
 */
const char* reports_misc_word(int index, const char* fallback, char* out, size_t out_sz) {
  const char* live = reports_labels_field("MISC", index);
  str_copy_trunc(out, out_sz, live ? live : fallback);
  return out;
}

/*
 * Rotating buffer set, not one shared scratch: callers routinely fetch
 * several @MISC words into one PopupMsgTokens before the text is filled
 * (nation + defeat verb + place in the combat popups), and a single static
 * made every token read back the word resolved last — the same aliasing bug
 * reports_cargo_name documents above.
 */
const char* reports_misc_display_word(int index, const char* fallback) {
  static char buf[8][64];
  static int next = 0;
  char* out = buf[next];
  next = (next + 1) % 8;
  return reports_misc_word(index, fallback ? fallback : "", out, sizeof(buf[0]));
}

const char* reports_title(ColonizeReportId id) {
  if (id < 0 || id >= COLONIZE_REPORT_COUNT) {
    return "REPORT";
  }
  const char* live = reports_labels_field("MISC", k_report_title_labels_index[id]);
  return live ? live : "";
}
/*
 * `col`-th (0-based) comma-separated field of the `row`-th (0-based,
 * comment/blank lines skipped) data line in `section` of g_reports_names.
 * Returns NULL when the live catalog, the section, the row, or the field
 * isn't available — callers fall back to their own static table.
 * Static return buffer: use/copy before the next call (matches the existing
 * ai_contact_tribe_flavor_good idiom — no caller here needs two at once).
 */
const char* reports_names_field(const char* section, int row, int col) {
  if (!g_reports_names_ok || row < 0) {
    return NULL;
  }
  const ColonizeMsgSection* sec = assets_msg_find(&g_reports_names, section);
  if (!sec) {
    return NULL;
  }
  int r = 0;
  for (int i = 0; i < sec->line_count; ++i) {
    const char* line = sec->lines[i];
    if (!line || line[0] == '\0' || line[0] == ';') {
      continue;
    }
    if (r != row) {
      r++;
      continue;
    }
    const char* p = line;
    for (int c = 0; c < col; ++c) {
      p = strchr(p, ',');
      if (!p) {
        return NULL;
      }
      ++p;
    }
    while (*p == ' ' || *p == '\t') {
      ++p;
    }
    const char* end = strchr(p, ',');
    size_t n = end ? (size_t)(end - p) : strlen(p);
    static char live[64];
    if (n >= sizeof(live)) {
      n = sizeof(live) - 1;
    }
    memcpy(live, p, n);
    live[n] = '\0';
    while (n > 0 && (live[n - 1] == ' ' || live[n - 1] == '\t')) {
      live[--n] = '\0';
    }
    return live[0] ? live : NULL;
  }
  return NULL;
}

const char* reports_job_name(int job) {
  if (job < 0 || job >= k_job_count) {
    return "Colonist";
  }
  /* @JOB: name(0), expert_name(1), school_tier(2), europe_hire_cost(3). */
  const char* live = reports_names_field("JOB", job, 1);
  return live ? live : "";
}

/*
 * Per-index buffer, same reason as reports_tribe_name below: the village
 * trade dialogs (@BRING, @BADCARGO, @BUYWHICH, @CHIEFHOWDY) fetch three cargo
 * names into one token set before filling the text, and the shared
 * reports_names_field scratch made all three read back the last one
 * ("Cigars, Cigars and Cigars", three "Ore" choice rows). bugs 2026-09-15.
 */
const char* reports_cargo_name(int cargo) {
  static char buf[COLONIZE_COL1_CARGO_TYPES][32];
  if (cargo < 0 || cargo >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return "cargo";
  }
  const char* live = reports_names_field("CARGO", cargo, 0);
  snprintf(buf[cargo], sizeof(buf[cargo]), "%s", live ? live : "");
  return buf[cargo];
}

const char* reports_ff_name(int idx) {
  if (idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT) {
    return "(none)";
  }
  const char* live = reports_names_field("FATHERS", idx, 0);
  return live ? live : "";
}

/*
 * NAMES.TXT @TRIBES column 0. Unlike the single-shot ff/job/cargo lookups
 * above (used-immediately, one at a time), the Indian Adviser builds a
 * whole `rows[]` array of these before drawing — reports_names_field's one
 * shared scratch buffer would have every row alias the last tribe parsed.
 * A per-index buffer here keeps each tribe's name independently alive.
 */
const char* reports_tribe_name(int t) {
  static char live[COLONIZE_COL1_INDIAN_COUNT][40];
  if (t < 0 || t >= (int)COLONIZE_COL1_INDIAN_COUNT) {
    return "Tribe";
  }
  const char* field = reports_names_field("TRIBES", t, 0);
  if (field) {
    snprintf(live[t], sizeof(live[t]), "%s", field);
    return live[t];
  }
  return "";
}

const char* reports_ff_display_name(int idx) {
  return reports_ff_name(idx);
}

/*
 * NAMES.TXT @FOUNDING (the six category words that sit above @FATHERS in the
 * same file). DOS keeps them as the DS:0x96e8 stride-2 pointer table the
 * Congress debate builder indexes by @FATHERS type column.
 */
static const char* k_ff_category_names[] = {
  "", "", "", "", "", ""
};

const char* reports_ff_category_display_name(int type) {
  static char buf[32];
  const int count = (int)(sizeof(k_ff_category_names) / sizeof(k_ff_category_names[0]));
  if (type < 0 || type >= count) {
    return "";
  }
  const char* live = reports_names_field("FOUNDING", type, 0);
  str_copy_trunc(buf, sizeof(buf), live ? live : k_ff_category_names[type]);
  return buf;
}

const char* reports_job_display_name(int job) {
  return reports_job_name(job);
}

/*
 * @JOB column 0 — the singular job word ("Distiller"), as opposed to
 * reports_job_display_name's column 1 recruit/specialty plural ("Master
 * Distiller"). colony.c's profession label and the colony-yield field job
 * names are this column; the map panel's unit profession line and the
 * Europe recruit pool are column 1.
 */
const char* reports_job_short_name(int job) {
  static char live[32][24];
  if (job < 0 || job >= k_job_count || job >= 32) {
    return "";
  }
  const char* field = reports_names_field("JOB", job, 0);
  if (!field) {
    return "";
  }
  snprintf(live[job], sizeof(live[job]), "%s", field);
  return live[job];
}

const char* reports_cargo_display_name(int cargo) {
  return reports_cargo_name(cargo);
}

const char* reports_tribe_display_name(int t) {
  return reports_tribe_name(t);
}

/*
 * Copy one NAMES.TXT field into a caller-owned buffer, falling back to a
 * literal when the catalog is absent. Callers pass a per-index buffer rather
 * than sharing reports_names_field's single scratch one: several of these
 * accessors get called back-to-back while a screen builds a rows[] array
 * (same hazard reports_tribe_name / reports_nation_adjective document).
 */
static const char* reports_names_or(
  char* buf, size_t buf_size, const char* section, int row, int col, const char* literal
) {
  const char* live = reports_names_field(section, row, col);
  snprintf(buf, buf_size, "%s", live ? live : literal);
  return buf;
}

const char* reports_nation_country_name(int nation) {
  static char live[COLONIZE_COL1_NATION_COUNT][24];
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return "";
  }
  return reports_names_or(
    live[nation], sizeof(live[nation]), "COUNTRY", nation, 0, "");
}

const char* reports_home_port_name(int nation) {
  static char live[COLONIZE_COL1_NATION_COUNT][24];
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return "";
  }
  return reports_names_or(
    live[nation], sizeof(live[nation]), "HOMEPORT", nation, 0, "");
}

const char* reports_difficulty_title(int level) {
  static char live[5][24];
  if (level < 0 || level > 4) {
    return "?";
  }
  return reports_names_or(
    live[level], sizeof(live[level]), "DIFFICULTY", level, 0, "");
}

/*
 * @TRIBES column 1 — the singular tribe word ("Inca"), as opposed to
 * reports_tribe_display_name's column 0 plural ("Incas").
 *
 * Which one DOS uses is per-string, and GAME.TXT settles it: the tribe token
 * is substituted into both shapes, e.g. `@INDIANRAID` ("The ^ raid ...", a
 * plural subject) and `@CAPTUREVILLAGE` ("^ village", a singular modifier).
 * The Indian Adviser report's own column heading is plural (golden
 * indian.png "Arawaks:"), while the map panel / cheat list / unit labels
 * name one settlement or one brave and are singular. Only Inca/Aztec/Arawak
 * actually differ between the two columns; the other five rows are the same
 * word twice.
 */
const char* reports_tribe_singular_name(int t) {
  static char live[COLONIZE_COL1_INDIAN_COUNT][24];
  if (t < 0 || t >= (int)COLONIZE_COL1_INDIAN_COUNT) {
    return "Tribe";
  }
  return reports_names_or(live[t], sizeof(live[t]), "TRIBES", t, 1, "");
}

/* Fortification chain name, tier 0 = Stockade. NAMES.TXT has no separate
 * "fort tier" list: DOS reads these off the first three @BUILDING rows,
 * which are the chain in order. */
/* NAMES.TXT @SEASONS: row 0 Spring, row 1 Autumn. The season word was
 * retyped as an "autumn ? \"Autumn\" : \"Spring\"" ternary in the save/load
 * dialog, the report screens, turn.c and ai_contact.c. */
const char* reports_season_name(bool autumn) {
  static char live[2][16];
  const int row = autumn ? 1 : 0;
  return reports_names_or(
    live[row], sizeof(live[row]), "SEASONS", row, 0, ""
  );
}

const char* reports_fort_tier_name(int tier) {
  static char live[3][24];
  if (tier < 0 || tier > 2) {
    return "";
  }
  return reports_names_or(
    live[tier], sizeof(live[tier]), "BUILDING", tier, 0, "");
}

/* Indian arms/mounts ladder, 0 = Braves … 3 = Mtd. Warriors (@UNIT rows
 * 19..22). Also used as a units_find_type key, so it must stay the @UNIT
 * spelling rather than a prettier "Mounted Warriors". */
const char* reports_brave_ladder_name(int rank) {
  static char live[4][24];
  if (rank < 0 || rank > 3) {
    return "";
  }
  return reports_names_or(
    live[rank], sizeof(live[rank]), "UNIT", REPORTS_UNIT_ROW_BRAVES + rank, 0, "");
}

/* Europe dock immigrant type name, indexed by DOS dos_type 0..5 — which is
 * exactly @UNIT row order for those six rows. */
const char* reports_dock_type_name(int dos_type) {
  static char live[6][24];
  if (dos_type < 0 || dos_type > 5) {
    return "";
  }
  return reports_names_or(
    live[dos_type], sizeof(live[dos_type]), "UNIT", dos_type, 0, "");
}

/*
 * NAMES.TXT @NATIONALITY (row = nation). Dedicated per-nation buffer, not
 * the shared ff/job/cargo scratch one — Foreign Affairs stores this into a
 * `rows[]` array (r->leader / r->adjective, two calls per row) before
 * drawing, same aliasing hazard as reports_tribe_name.
 */
const char* reports_nation_adjective(int nation) {
  static char live[COLONIZE_COL1_NATION_COUNT][20];
  if (nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return "";
  }
  const char* field = reports_names_field("NATIONALITY", nation, 0);
  if (field) {
    snprintf(live[nation], sizeof(live[nation]), "%s", field);
    return live[nation];
  }
  return "";
}

/* NAMES.TXT @LEVELS column 0 (row = tech, capped 0..3). Own buffer for the
 * same reason as reports_nation_adjective — used inside the Indian
 * Adviser's rows[] builder alongside reports_tribe_name. */
const char* reports_tribe_level(uint8_t tech) {
  static char live[4][20];
  if (tech > 3) {
    tech = 3;
  }
  const char* field = reports_names_field("LEVELS", tech, 0);
  if (field) {
    snprintf(live[tech], sizeof(live[tech]), "%s", field);
    return live[tech];
  }
  return "";
}

const char* reports_nation_adjective_display_name(int nation) {
  return reports_nation_adjective(nation);
}

const char* reports_tribe_level_display_name(uint8_t tech) {
  return reports_tribe_level(tech);
}
void reports_names_load_catalogs(const char* data_dir) {
  if (!data_dir) {
    return;
  }
  /* NAMES.TXT @FATHERS live names (reports_ff_name) — best-effort, falls back
   * to the hand-typed k_ff_names table when missing (tests, old data dirs). */
  if (g_reports_names_ok) {
    assets_msg_free(&g_reports_names);
    g_reports_names_ok = false;
  }
  char names_path[512];
  assets_msg_init(&g_reports_names);
  if (dos_compat_normalize_asset_path(data_dir, "NAMES.TXT", names_path, sizeof(names_path)) &&
      assets_msg_load_file(&g_reports_names, names_path)) {
    g_reports_names_ok = true;
  } else {
    assets_msg_free(&g_reports_names);
  }

  /* LABELS.TXT live report titles (reports_title) — same best-effort/
   * fallback shape as the NAMES.TXT block above. */
  if (g_reports_labels_ok) {
    assets_msg_free(&g_reports_labels);
    g_reports_labels_ok = false;
  }
  char labels_path[512];
  assets_msg_init(&g_reports_labels);
  if (dos_compat_normalize_asset_path(data_dir, "LABELS.TXT", labels_path, sizeof(labels_path)) &&
      assets_msg_load_file(&g_reports_labels, labels_path)) {
    g_reports_labels_ok = true;
  } else {
    assets_msg_free(&g_reports_labels);
  }}

void reports_names_free_catalogs(void) {
  if (g_reports_names_ok) {
    assets_msg_free(&g_reports_names);
    g_reports_names_ok = false;
  }
  if (g_reports_labels_ok) {
    assets_msg_free(&g_reports_labels);
    g_reports_labels_ok = false;
  }
}
