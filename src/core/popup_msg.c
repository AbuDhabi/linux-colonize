#include "core/popup_msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/strutil.h"
#include "platform/diagnostics.h"

bool popup_msg_is_directive(const char* line) {
  return line && line[0] == '@';
}

static bool popup_msg_is_choice_word(const char* line) {
  if (!line || line[0] == '\0') {
    return false;
  }
  if (strcmp(line, "Yes") == 0 || strcmp(line, "No") == 0 || strcmp(line, "OK") == 0 ||
      strcmp(line, "Never mind.") == 0 || strncmp(line, "Unload the", 10) == 0) {
    return true;
  }
  /* Common trailing choice labels (blank lines are stripped by assets_msg_load). */
  if (strcmp(line, "Stay With Ships") == 0 || strcmp(line, "Make Landfall") == 0 ||
      strcmp(line, "No thank you.") == 0 || strncmp(line, "Pay ", 4) == 0 ||
      strncmp(line, "Kiss pinky", 10) == 0 || strncmp(line, "Hold '", 6) == 0 ||
      strncmp(line, "Yes, it is God's will", 21) == 0 ||
      strncmp(line, "Never! That would be folly", 26) == 0 ||
      strncmp(line, "\"Never! That would be treasonous", 32) == 0 ||
      strncmp(line, "\"Yes! Give me liberty", 21) == 0 ||
      strncmp(line, "\"Oh, I forgot about that.", 25) == 0 ||
      strncmp(line, "\"And that is exactly what I had in mind.", 40) == 0 ||
      strcmp(line, "Cancel Action.") == 0 || strcmp(line, "Break Treaty.") == 0 ||
      strcmp(line, "Accept") == 0 || strcmp(line, "Refuse") == 0 ||
      strcmp(line, "That's all.") == 0 || strcmp(line, "Keep playing anyway.") == 0) {
    return true;
  }
  return false;
}

size_t popup_msg_section_body(
  const ColonizeMsgSection* section,
  char* out,
  size_t out_size,
  bool stop_before_choices
) {
  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!section) {
    return 0;
  }
  size_t used = 0;
  int saw_prose = 0;
  int boundary = 0; /* blank-line boundary carried across skipped lines */
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    /* Only blanks after the first prose line count — a blank between the
     * @directives and the body must not end an empty body. */
    if (saw_prose && section->blank_before && section->blank_before[i]) {
      boundary = 1;
    }
    if (!line || line[0] == '\0' || popup_msg_is_directive(line)) {
      continue;
    }
    /* Skip lone "Name:" / "Amount:" / "Colony:" prompt labels for body. */
    if (strcmp(line, "Name:") == 0 || strcmp(line, "Amount:") == 0 ||
        strcmp(line, "Colony:") == 0) {
      continue;
    }
    /* DOS FUN_6f74_32a4: a blank line after body prose switches the parser
     * to choice state — everything past it is choice rows, never body. The
     * keyword list stays as a backstop for blankless catalogs (tests). */
    if (stop_before_choices && saw_prose && (boundary || popup_msg_is_choice_word(line))) {
      break;
    }
    /*
     * GAME.TXT '^' line prefix (DOS FUN_6f74_0cb0 + the 1198 wrap loop): a
     * caret-led line is NOT word-wrapped into the running paragraph. It gets
     * an output line of its own, drawn verbatim — '^^' additionally centres
     * it (the flag-1 arm measures the string before drawing; '^' alone sets
     * flag 2 and stays left-aligned). bugs.md: the caret was copied through
     * literally, so @BUYME1 read "...100$. ^Treasury: 500$." on one line.
     * Encoded for the renderer as a newline break plus a leading
     * POPUP_MSG_LINE_MARK / POPUP_MSG_CENTER_MARK.
     */
    int caret = 0;
    while (line[caret] == '^') {
      caret++;
    }
    if (caret > 0) {
      if (used > 0 && used + 1 < out_size) {
        out[used++] = '\n';
      }
      if (used + 1 < out_size) {
        out[used++] = caret >= 2 ? POPUP_MSG_CENTER_MARK : POPUP_MSG_LINE_MARK;
      }
      out[used] = '\0';
      line += caret;
    } else if (used > 0 && used + 1 < out_size) {
      out[used++] = ' ';
    }
    const size_t n = strlen(line);
    if (used + n >= out_size) {
      size_t copy = out_size - 1 - used;
      memcpy(out + used, line, copy);
      used += copy;
      out[used] = '\0';
      return used;
    }
    memcpy(out + used, line, n);
    used += n;
    if (caret > 0 && used + 1 < out_size) {
      out[used++] = '\n'; /* the next line starts fresh, never joins this one */
    }
    out[used] = '\0';
    saw_prose = 1;
  }
  return used;
}

void popup_msg_strip_markup(char* s) {
  if (!s) {
    return;
  }
  char* w = s;
  for (char* r = s; *r; ++r) {
    if (*r == '{' || *r == '}' || *r == POPUP_MSG_LINE_MARK ||
        *r == POPUP_MSG_CENTER_MARK) {
      continue;
    }
    *w++ = (*r == '\n') ? ' ' : *r;
  }
  *w = '\0';
}

static void popup_msg_append(char* dst, size_t dst_size, size_t* used, const char* add) {
  if (!dst || !used || dst_size == 0 || !add) {
    return;
  }
  while (*add && *used + 1 < dst_size) {
    dst[(*used)++] = *add++;
  }
  dst[*used] = '\0';
}

void popup_msg_apply_tokens(
  char* dst,
  size_t dst_size,
  const char* src,
  const PopupMsgTokens* tok
) {
  if (!dst || dst_size == 0) {
    return;
  }
  dst[0] = '\0';
  if (!src) {
    return;
  }
  size_t used = 0;
  for (size_t i = 0; src[i] != '\0' && used + 1 < dst_size;) {
    /* Keep {} — the DOS dialog writer colors braced spans with the hilite
     * ink (FUN_6f74_0538); brace-aware renderers eat them at draw time. */
    if (src[i] == '%' && tok) {
      if (strncmp(src + i, "%STRING0", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string0 ? tok->string0 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%STRING1", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string1 ? tok->string1 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%STRING2", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string2 ? tok->string2 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%STRING3", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string3 ? tok->string3 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%STRING4", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->string4 ? tok->string4 : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%COUNTRY", 8) == 0) {
        popup_msg_append(dst, dst_size, &used, tok->country ? tok->country : "");
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%NUMBER0", 8) == 0) {
        char num[16];
        snprintf(num, sizeof(num), "%d", tok->has_number0 ? tok->number0 : 0);
        popup_msg_append(dst, dst_size, &used, num);
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%NUMBER1", 8) == 0) {
        char num[16];
        snprintf(num, sizeof(num), "%d", tok->has_number1 ? tok->number1 : 0);
        popup_msg_append(dst, dst_size, &used, num);
        i += 8;
        continue;
      }
      if (strncmp(src + i, "%NUMBER2", 8) == 0) {
        char num[16];
        snprintf(num, sizeof(num), "%d", tok->has_number2 ? tok->number2 : 0);
        popup_msg_append(dst, dst_size, &used, num);
        i += 8;
        continue;
      }
      if (src[i + 1] == '%') {
        dst[used++] = '%';
        dst[used] = '\0';
        i += 2;
        continue;
      }
    }
    dst[used++] = src[i++];
    dst[used] = '\0';
  }
}

static int g_popup_msg_pending_width = 0;
static int g_popup_msg_pending_graphic = -1;
static int g_popup_msg_pending_default = 0;

/*
 * GAME.TXT section → MSS{n}.SS popup decoration. Every constant
 * `FUN_281f_0652(tag, index)` pair in VICEROY.EXE, tag resolved through the
 * DS-string rule (EXE offset 121248 + addr). Dynamic-tag DOS sites are
 * covered by name family instead: FUN_65dd_0004 latches 3 for every Lost
 * City Rumour dialog (@LOSTCITYn/@BURIALn/@SCREWED).
 */
typedef struct PopupMsgGraphicRow {
  const char* section;
  int8_t mss;
} PopupMsgGraphicRow;

static const PopupMsgGraphicRow k_popup_msg_mss[] = {
  /* MSS0 — admiral: ships, naval combat, sea trade. */
  {"TUTORIAL1", 0},   {"TUTORIAL2", 0},   {"TUTORIAL5", 0},  {"TUTORIAL6", 0},
  {"TUTORIAL11", 0},  {"DISBANDSHIP", 0}, {"TRADENONE", 0},  {"FORTFIRE", 0},
  {"REFIT", 0},       {"SEIZURE", 0},     {"ROUTELOOP", 0},  {"SHIPRUN", 0},
  {"SHIPSLOW", 0},    {"CARGOCAPTURE", 0}, {"SHIPDAMAGE", 0}, {"SHIPSUNK", 0},
  {"EVASIVE", 0},
  /* MSS1 — continental soldier: land war, king's forces, revolution. */
  {"TUTORIAL7", 1},   {"TUTORIAL14", 1},  {"HAVETREATY", 1}, {"TEAPARTY", 1},
  {"FOREIGNNOTAVAIL", 1}, {"AMBUSHHINT", 1}, {"CONSIDER", 1}, {"TORYUPRISING", 1},
  {"INVASION", 1},    {"INTERVENTION", 1}, {"INDEPENDENCE", 1}, {"KINGBUY", 1},
  {"KINGMOBILIZE", 1}, {"MERCENARIES", 1}, {"MERCS", 1},     {"REBELDOWN", 1},
  {"REBELUP", 1},     {"MULTIREV", 1},    {"DECLARE", 1},    {"WHACKINDIANS", 1},
  {"INDIANBURN", 1},  {"INDIANWARFARE", 1}, {"NOMAYORSDURINGREV", 1},
  {"DEMOTE", 1},      {"ARTILLERY", 1},   {"ARTILLERY2", 1}, {"HALF", 1},
  {"HOWTOWIN", 1},    {"INDIANLOSE", 1},  {"INDIANSLAVES", 1}, {"LOOT", 1},
  {"LOOT2", 1},       {"NOCOLONIESEITHER", 1}, {"SEIZURELAND", 1},
  {"MOBILIZE2", 1},   {"ALREADYREVOLUTION", 1}, {"LOOTCAPTURE", 1},
  {"VALOR", 1},       {"EUROPELOSE", 1},  {"BURNED2", 1},    {"CAPTURED", 1},
  {"INDIANBURNCOLONY", 1}, {"INDIANWINCOLONY", 1},
  /* MSS2 — courtier: gold, prices, treaties, court news. */
  {"MULTINEXT", 2},   {"OTHERMIGHT", 2},  {"OTHERLESS", 2},  {"PRICEUP", 2},
  {"PRICEDOWN", 2},   {"SOMEBOYCOTT", 2}, {"KISSUP", 2},     {"KISSSORRY", 2},
  {"REALLYBUY", 2},   {"SUCCESSION", 2},  {"LOOTCASH", 2},   {"LOOTFOREIGN", 2},
  {"SIGNTREATY", 2},  {"CASHTREASURE", 2}, {"DECLAREWAR", 2}, {"BURNED3", 2},
  {"CAPTURED2", 2},   {"CANCELTREATY", 2},
  /* MSS3 — frontiersman: terrain work, scouts, rumours. */
  {"TUTORIAL3", 3},   {"TUTORIAL9", 3},   {"TUTORIAL10", 3}, {"TUTORIAL13", 3},
  {"NOPLOW", 3},      {"NOROAD", 3},      {"NOPORT", 3},     {"TUTNOSPACES", 3},
  {"TUTNOLUMBER", 3}, {"USEDUPTOOLS", 3}, {"EXTINCT", 3},    {"KILLWAGONS", 3},
  {"SCOUTCOLONY", 3}, {"RAIDWREAK", 3},   {"LOSTTHEIRSCOUTS", 3},
  {"INDIANBURNCOLONY2", 3}, {"INDIANWINCOLONY2", 3},
  {"LOSTCITY1", 3},   {"LOSTCITY2", 3},   {"LOSTCITY3", 3},  {"LOSTCITY4", 3},
  {"LOSTCITY5", 3},   {"LOSTCITY6", 3},   {"LOSTCITY7", 3},  {"LOSTCITY8", 3},
  {"LOSTCITY9", 3},   {"BURIAL1", 3},     {"BURIAL2", 3},    {"BURIAL3", 3},
  {"SCREWED", 3},
  /* MSS4 — friar: religion, converts, unrest. */
  {"TUTORIAL19", 4},  {"DEADCONVERTS", 4}, {"UNREST", 4},
  /* MSS5 — nun: colony stores, raids on colonies. */
  {"TUTORIAL4", 5},   {"TUTORIAL8", 5},   {"TUTORIAL12", 5}, {"TUTORIAL15", 5},
  {"LOBOTOMIZE", 5},  {"WAREHOUSEFULL", 5}, {"BUYME0", 5},   {"BUYME1", 5},
  {"CLEARCUT", 5},    {"RAIDSTORES", 5},  {"RAIDBURN", 5},   {"RAIDSHIP", 5},
  {"RAIDGOLD", 5},    {"RAIDNOTHING", 5},
  /* Colony-screen abandon confirm: DOS 2f2b builds "ABANDON" (+"2" after
   * 1575 with no other colony) into a stack buffer and calls
   * FUN_281f_0652(name, 5) — a dynamic tag, so it is keyed by name here. */
  {"ABANDON", 5},     {"ABANDON2", 5},
};

int popup_msg_mss_index_for_section(const char* section_name) {
  if (!section_name) {
    return -1;
  }
  for (size_t i = 0; i < sizeof(k_popup_msg_mss) / sizeof(k_popup_msg_mss[0]); ++i) {
    if (strcmp(k_popup_msg_mss[i].section, section_name) == 0) {
      return k_popup_msg_mss[i].mss;
    }
  }
  return -1;
}

int popup_msg_take_pending_graphic(void) {
  const int g = g_popup_msg_pending_graphic;
  g_popup_msg_pending_graphic = -1;
  return g;
}

int popup_msg_section_width(const ColonizeMsgSection* section) {
  if (!section) {
    return 0;
  }
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (line && strncmp(line, "@width=", 7) == 0) {
      const int w = atoi(line + 7);
      return w > 0 ? w : 0;
    }
  }
  return 0;
}

int popup_msg_take_pending_width(void) {
  const int w = g_popup_msg_pending_width;
  g_popup_msg_pending_width = 0;
  return w;
}

int popup_msg_section_default(const ColonizeMsgSection* section) {
  if (!section) {
    return 0;
  }
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (line && strncmp(line, "@default=", 9) == 0) {
      const int d = atoi(line + 9);
      return d > 0 ? d : 0;
    }
  }
  return 0;
}

int popup_msg_take_pending_default(void) {
  const int d = g_popup_msg_pending_default;
  g_popup_msg_pending_default = 0;
  return d;
}

void popup_msg_fill(
  const ColonizeMsgCatalog* catalog,
  const char* section_name,
  const PopupMsgTokens* tok,
  const char* fallback,
  char* out,
  size_t out_size
) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  char raw[512];
  raw[0] = '\0';
  const ColonizeMsgSection* sec =
    (catalog && section_name) ? assets_msg_find(catalog, section_name) : NULL;
  g_popup_msg_pending_width = sec ? popup_msg_section_width(sec) : 0;
  g_popup_msg_pending_default = sec ? popup_msg_section_default(sec) : 0;
  /* DOS keys the graphic off the call site, not GAME.TXT content — set it
   * from the name even when the section is missing and the fallback shows. */
  g_popup_msg_pending_graphic = popup_msg_mss_index_for_section(section_name);
  if (sec) {
    popup_msg_section_body(sec, raw, sizeof(raw), true);
  } else if (catalog && section_name) {
    /* A loaded catalog that lacks the section is a real content gap; a NULL
     * catalog is just a headless caller with no GAME.TXT. */
    diag_warn("GAME.TXT section @%s missing — using fallback text", section_name);
  }
  if (raw[0] == '\0' && fallback) {
    str_copy_trunc(raw, sizeof(raw), fallback);
  }
  popup_msg_apply_tokens(out, out_size, raw, tok);
  if (out[0] == '\0' && fallback) {
    str_copy_trunc(out, out_size, fallback);
  }
}

int popup_msg_choices(
  const ColonizeMsgSection* section,
  char out[][POPUP_MSG_CHOICE_LEN],
  int max_choices
) {
  if (!section || !out || max_choices <= 0) {
    return 0;
  }
  int count = 0;
  int saw_prose = 0;
  /* Primary: DOS FUN_6f74_32a4 blank-line state machine — body until the
   * first blank line after prose, then every line is a choice row until the
   * next blank (state 3 = ignored trailer). Quoted diplomacy/King rows the
   * keyword list below never knew about are caught by this pass. */
  if (section->blank_before) {
    int state = 1; /* 1 body, 2 choices, 3 done */
    for (int i = 0; i < section->line_count; ++i) {
      const char* line = section->lines[i];
      if (saw_prose && section->blank_before[i]) {
        state++;
        if (state > 2) {
          break;
        }
      }
      if (!line || line[0] == '\0' || popup_msg_is_directive(line)) {
        continue;
      }
      if (strcmp(line, "Name:") == 0 || strcmp(line, "Amount:") == 0 ||
          strcmp(line, "Colony:") == 0) {
        continue;
      }
      if (state == 1) {
        saw_prose = 1;
        continue;
      }
      if (count < max_choices) {
        str_copy_trunc(out[count], POPUP_MSG_CHOICE_LEN, line);
        count++;
      }
    }
    if (count > 0) {
      return count;
    }
  }
  saw_prose = 0;
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0' || popup_msg_is_directive(line)) {
      continue;
    }
    if (strcmp(line, "Name:") == 0 || strcmp(line, "Amount:") == 0 ||
        strcmp(line, "Colony:") == 0) {
      continue;
    }
    if (popup_msg_is_choice_word(line)) {
      if (count < max_choices) {
        str_copy_trunc(out[count], POPUP_MSG_CHOICE_LEN, line);
        count++;
      }
      continue;
    }
    /* Once a choice was seen, further non-directive lines are also choices
     * (e.g. second landfall option if matcher missed one). */
    if (count > 0 && saw_prose) {
      if (count < max_choices) {
        str_copy_trunc(out[count], POPUP_MSG_CHOICE_LEN, line);
        count++;
      }
      continue;
    }
    saw_prose = 1;
  }
  /* Choice-only fragments (e.g. @TAXOPTIONS): no prose, collect all lines. */
  if (count == 0 && !saw_prose) {
    for (int i = 0; i < section->line_count && count < max_choices; ++i) {
      const char* line = section->lines[i];
      if (!line || line[0] == '\0' || popup_msg_is_directive(line)) {
        continue;
      }
      str_copy_trunc(out[count], POPUP_MSG_CHOICE_LEN, line);
      count++;
    }
  }
  if (count == 0) {
    for (int i = 0; i < section->line_count && count < max_choices; ++i) {
      const char* line = section->lines[i];
      if (line && (strcmp(line, "Yes") == 0 || strcmp(line, "No") == 0)) {
        str_copy_trunc(out[count], POPUP_MSG_CHOICE_LEN, line);
        count++;
      }
    }
  }
  return count;
}
