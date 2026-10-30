#include "core/popup_msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/strutil.h"
#include "platform/diagnostics.h"

bool popup_msg_is_directive(const char* line) {
  return line && line[0] == '@';
}

/*
 * A field-caption row (e.g. "Name:", "Amount:", "Colony:") is never body or
 * choice text: in every catalog section that has one, it is the LAST
 * content line (verified against GAME.TXT/DEBUG.TXT for the shipped
 * catalogs). Structural rule, not a literal-name comparison: a line ending
 * in ':' with no further content line after it in the section.
 */
static bool popup_msg_is_trailing_caption(const ColonizeMsgSection* section, int index) {
  if (!section) {
    return false;
  }
  const char* line = section->lines[index];
  if (!line || line[0] == '\0') {
    return false;
  }
  size_t len = strlen(line);
  if (line[len - 1] != ':') {
    return false;
  }
  for (int j = index + 1; j < section->line_count; ++j) {
    const char* rest = section->lines[j];
    if (rest && rest[0] != '\0' && !popup_msg_is_directive(rest)) {
      return false;
    }
  }
  /* A caption FOLLOWS prose across a blank line ("How much ...?" / blank /
   * "Amount:"). A prompt that merely ends in ':' and is the section's only
   * text (@TRADESELECT "Select a trade route:") is the body, not a caption. */
  if (!section->blank_before || !section->blank_before[index]) {
    return false;
  }
  for (int j = 0; j < index; ++j) {
    const char* prev = section->lines[j];
    if (prev && prev[0] != '\0' && !popup_msg_is_directive(prev)) {
      return true;
    }
  }
  return false;
}

/*
 * DOS FUN_6f74_0c32 caret rule — the single source both the popup body
 * collector below and pedia_caret_flags() (core/pedia.c) run on. The DOS
 * parser tests the first byte, then the second, and stops: "^^" eats two and
 * stores flag 1, a lone "^" eats one and ORs flag 2, and a third caret is
 * ordinary body text.
 */
int popup_msg_caret_flags(const char* line, const char** out_rest) {
  int flags = 0;
  const char* rest = line ? line : "";
  if (rest[0] == '^') {
    if (rest[1] == '^') {
      flags = POPUP_MSG_CARET_CENTER;
      rest += 2;
    } else {
      flags = POPUP_MSG_CARET_OWN_LINE;
      rest += 1;
    }
  }
  if (out_rest) {
    *out_rest = rest;
  }
  return flags;
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
    /* Skip a trailing field-caption row ("Name:" / "Amount:" / "Colony:")
     * for body — structural rule, see popup_msg_is_trailing_caption(). */
    if (popup_msg_is_trailing_caption(section, i)) {
      continue;
    }
    /* DOS FUN_6f74_32a4: a blank line after body prose switches the parser
     * to choice state — everything past it is choice rows, never body.
     * blank_before is always populated by the real loader, so this is the
     * sole mechanism (no keyword backstop). */
    if (stop_before_choices && saw_prose && boundary) {
      break;
    }
    /*
     * GAME.TXT '^' line prefix (DOS FUN_6f74_0c32 + the 1198 wrap loop): a
     * caret-led line is NOT word-wrapped into the running paragraph. It gets
     * an output line of its own, drawn verbatim — '^^' additionally centres
     * it (the flag-1 arm measures the string before drawing; '^' alone sets
     * flag 2 and stays left-aligned). port_bugs.md: the caret was copied through
     * literally, so @BUYME1 read "...100$. ^Treasury: 500$." on one line.
     * Encoded for the renderer as a newline break plus a leading
     * POPUP_MSG_LINE_MARK / POPUP_MSG_CENTER_MARK. At most two carets are
     * consumed (popup_msg_caret_flags); a third stays as body text.
     */
    const char* caret_rest = line;
    const int caret = popup_msg_caret_flags(line, &caret_rest);
    if (caret != 0) {
      /* A caret line right after another one already ends in its '\n': a second break would
       * be an empty row DOS never draws (port_bugs.md #1222, @FATHER1's "^" + "^{When..."). */
      if (used > 0 && out[used - 1] != '\n' && used + 1 < out_size) {
        out[used++] = '\n';
      }
      if (used + 1 < out_size) {
        out[used++] =
          (caret == POPUP_MSG_CARET_CENTER) ? POPUP_MSG_CENTER_MARK : POPUP_MSG_LINE_MARK;
      }
      out[used] = '\0';
      line = caret_rest;
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
    if (caret != 0 && used + 1 < out_size) {
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
      if (strncmp(src + i, "%NUMBER3", 8) == 0) {
        char num[16];
        snprintf(num, sizeof(num), "%d", tok->has_number3 ? tok->number3 : 0);
        popup_msg_append(dst, dst_size, &used, num);
        i += 8;
        continue;
      }
    }
    /* FUN_6f74_309c: "%%" -> "%" (raw 117225-117228), with or without tokens. */
    if (src[i] == '%' && src[i + 1] == '%') {
      dst[used++] = '%';
      dst[used] = '\0';
      i += 2;
      continue;
    }
    dst[used++] = src[i++];
    dst[used] = '\0';
  }
}

static int g_popup_msg_pending_width = 0;
static int g_popup_msg_pending_graphic = -1;
static int g_popup_msg_pending_default = 0;
static int g_popup_msg_pending_chief = 0;

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
  /* Europe "shall we set sail?" confirm: FUN_38fd_2a92 @ 38fd:2aba
   * `SUB DX,DX / MOV word [0x1f5e],DX`, tag `LEA AX,[0xffc]` = @SAILAWAY
   * (DS string, EXE offset 121248+0xffc) — a direct latch, no 0652 pair. */
  {"SAILAWAY", 0},
  {"REFIT", 0},       {"SEIZURE", 0},     {"ROUTELOOP", 0},  {"SHIPRUN", 0},
  {"SHIPSLOW", 0},    {"CARGOCAPTURE", 0}, {"SHIPDAMAGE", 0}, {"SHIPSUNK", 0},
  {"EVASIVE", 0},
  /* New World name prompt: FUN_2b5a_001e @ 2b5a:0028 `MOV word [0x1f5e],0` before
   * FUN_291f_0120(0x17) — a direct latch, no 0652 pair (port_bugs.md #1183). */
  {"LANDHO", 0},
  /* port_bugs.md #1220, a second asm scan (viceroy_ndisasm.asm): FUN_281f_0652 pairs whose
   * `PUSH idx / PUSH tag` jump to a shared CALL (2b5a:255a.., 2f2b:5826.., 5fef ..), and
   * FUN_2b5a_0f92's direct `MOV [0x1f5e],DX` (DX 0) before @OVERBOARD (ndisasm 0x21ef7). */
  {"SEACOLONY", 0},   {"NODOCKS", 0},     {"SEIZURESEA", 0}, {"SHIPLAKE", 0},
  {"LANDFIRST", 0},   {"SAILHOME", 0},    {"SHIPCOMBAT", 0}, {"OVERBOARD", 0},
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
  /* #1220 scan (see MSS0): @SIEGE is `PUSH 1 / PUSH 0xc7c` at 2f2b (ndisasm 0x28ada). */
  {"SIEGE", 1},       {"INTERVENE", 1},   {"MOBILIZE", 1},   {"REBELUP50", 1},
  {"TOOTORY", 1},     {"CANCELPEACE", 1}, {"WAGONCAPTURE", 1}, {"COLONISTCAPTURE", 1},
  {"COLONISTCAPTURE2", 1}, {"CONTINENTAL", 1}, {"VETERAN", 1}, {"EUROPEWIN", 1},
  {"BURNED", 1},      {"CAPTURED3", 1},   {"INDIANSURPRISE", 1},
  /* Digit-patched tags (stack buffer + FUN_281f_0652(buf, 1)): FUN_3844_0442 "WARN0" + n
   * (ndisasm 0x2f724) and "SOONRETIRING" + 0/1 (0x2f9e5), FUN_5fef_1b0e "INDIANWIN0" + n
   * (0x5e107). */
  {"WARN1", 1},       {"WARN2", 1},       {"WARN3", 1},      {"SOONRETIRING0", 1},
  {"SOONRETIRING1", 1}, {"INDIANWIN0", 1}, {"INDIANWIN1", 1}, {"INDIANWIN2", 1},
  /* Colony events: FUN_364b_0000 stores its param_6 in DS:0x1f5e (raw 56849); the callers
   * in 364b push it (ndisasm 0x2d149-0x2e9b0, the 6th push before `CALL 0xef5f`). */
  {"REBELMAJORITY", 1}, {"REBELUNANIMOUS", 1}, {"TORYMINORITY", 1}, {"TORYMAJORITY", 1},
  {"SONSDOWN", 1},
  /* MSS2 — courtier: gold, prices, treaties, court news. */
  {"MULTINEXT", 2},   {"OTHERMIGHT", 2},  {"OTHERLESS", 2},  {"PRICEUP", 2},
  {"PRICEDOWN", 2},   {"SOMEBOYCOTT", 2}, {"KISSUP", 2},     {"KISSSORRY", 2},
  {"REALLYBUY", 2},   {"SUCCESSION", 2},  {"LOOTCASH", 2},   {"LOOTFOREIGN", 2},
  {"SIGNTREATY", 2},  {"CASHTREASURE", 2}, {"DECLAREWAR", 2}, {"BURNED3", 2},
  {"CAPTURED2", 2},   {"CANCELTREATY", 2},
  /* FUN_364b_0000 param_6 (see MSS1). */
  {"TRAINPROFESSION", 2}, {"TRAINFAIL", 2},
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
  /* #1220 scan (see MSS0) and FUN_364b_0000 param_6 (@DEPLETION). */
  {"TOOMOUNTAIN", 3}, {"TOONEARBUILD", 3}, {"LANDFALL", 3}, {"LANDFALL2", 3},
  {"LOSTOURSCOUTS", 3}, {"WELLSEASONED", 3}, {"DEPLETION", 3},
  /* MSS4 — friar: religion, converts, unrest. */
  {"TUTORIAL19", 4},  {"DEADCONVERTS", 4}, {"UNREST", 4},
  /* #1220 scan; "MISSION0" + n is digit-patched (ndisasm 0x48b53-0x48c4f); @NEWCOLONIST is
   * FUN_364b_0000 param_6. */
  {"HERESY0", 4},     {"HERESY1", 4},     {"MISSION0", 4},   {"MISSION1", 4},
  {"MISSION2", 4},    {"MISSION3", 4},    {"NEWCOLONIST", 4},
  /* MSS5 — nun: colony stores, raids on colonies. */
  {"TUTORIAL4", 5},   {"TUTORIAL8", 5},   {"TUTORIAL12", 5}, {"TUTORIAL15", 5},
  {"LOBOTOMIZE", 5},  {"WAREHOUSEFULL", 5}, {"BUYME0", 5},   {"BUYME1", 5},
  {"CLEARCUT", 5},    {"RAIDSTORES", 5},  {"RAIDBURN", 5},   {"RAIDSHIP", 5},
  {"RAIDGOLD", 5},    {"RAIDNOTHING", 5},
  /* #1220 scan (2f2b colony-screen refusals, see MSS0). */
  {"TOONEAR", 5},     {"MORETHANTHREE", 5}, {"FULL", 5},     {"GRADUATE", 5},
  {"UNIV3", 5},       {"COLLEGE2", 5},    {"SCHOOL1", 5},    {"NOTEACHER", 5},
  {"NEEDCOLLEGE", 5}, {"NEEDUNIVERSITY", 5}, {"ASSEMBLY", 5}, {"TOWNHALL", 5},
  {"MEETINGHALL", 5}, {"KEEPSTOCKADE", 5},
  /* FUN_364b_0000 param_6 (see MSS1); @NEEDTOOLS / "0", @SPOIL1 / 2 and "CARGOREADY0" + n
   * are built in a stack buffer (ndisasm 0x2e648, 0x2e8b8, 0x2e96c). */
  {"NOMOREWAREHOUSE", 5}, {"NOMOREWAGONS", 5}, {"BUILT", 5}, {"INEFFICIENT", 5},
  {"EFFICIENT", 5},   {"STARVE2", 5},     {"FOODLOW", 5},    {"LUMBER", 5},
  {"COTTON", 5},      {"TOBACCO", 5},     {"CANESUGAR", 5},  {"FURS", 5},
  {"ORE", 5},         {"TOOLS", 5},       {"ALREADYHAVE", 5}, {"NEEDTOOLS", 5},
  {"NEEDTOOLS0", 5},  {"SPOIL1", 5},      {"SPOIL2", 5},     {"CARGOREADY0", 5},
  {"CARGOREADY1", 5}, {"CARGOREADY2", 5},
  /* Colony-screen abandon confirm: DOS 2f2b builds "ABANDON" (+"2" after
   * 1575 with no other colony) into a stack buffer and calls
   * FUN_281f_0652(name, 5) — a dynamic tag, so it is keyed by name here. */
  {"ABANDON", 5},     {"ABANDON2", 5},
  /* Colony name prompt: FUN_479b_076e @ 479b:0823 `MOV word [0x1f5e],5` before
   * FUN_291f_0120(0x17), human nations only. @RENAMECOLONY, @TRADENAME and
   * @HOWMUCH1-5 latch nothing in DOS and stay bare. */
  {"COLONY", 5},
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

/*
 * Sections DOS shows through FUN_291f_019c(tag, tribe), the chief-portrait entry
 * (DS:0x1f5c = DS:0x8d52): every immediate tag pushed at, or jumped into, the 38
 * `CALLF 291f:019c` sites, plus the stack-built @TRADE0+n / @BUYn / @BADHAGGLEn /
 * @LEARN+suffix tags (ndisasm 0x49b08, 0x4a1b1, 0x4a7b7). A native dialog shown
 * through FUN_281f_0652 (@RAID*, @HERESYn, @KILLWAGONS, ...) or FUN_281f_03fe
 * (@TEACHCONVERT, @INDIANGRUDGE) gets no portrait. Prefix rows end in '*'.
 */
static const char* const k_popup_msg_chief[] = {
  "INDIANBRIBE", "BADCARGO", "BADHAGGLE*", "BRING", "DEFICIT", "BUYWHICH", "NOTENOUGH",
  "MADATWAGONS", "GRUDGEWAGONS", "LEARN*", "CHIEF*", "EXTORT*", "NOCONTACT",
  "INDIANWARPATH2", "UNFORTUNATE", "ALREADYSMITE", "MADATSHIPS", "INDIANPEACE",
  "INDIANCOME", "INDIANWELCOME", "INDIANSHUN", "INDIANBEGFOOD", "INDIANSCONVERT",
  "INDIANGIVEFOOD", "INDIANGIVESTUFF", "INDIANCOMMENT", "INDIANCITY", "INDIANWAGONS",
  "INDIANBOW", "TRADE0", "TRADE1", "BUY0", "BUY1",
};

int popup_msg_section_has_chief_portrait(const char* section_name) {
  if (!section_name) {
    return 0;
  }
  for (size_t i = 0; i < sizeof(k_popup_msg_chief) / sizeof(k_popup_msg_chief[0]); ++i) {
    const char* row = k_popup_msg_chief[i];
    const size_t n = strlen(row);
    if (row[n - 1] == '*' ? strncmp(row, section_name, n - 1) == 0
                          : strcmp(row, section_name) == 0) {
      return 1;
    }
  }
  return 0;
}

int popup_msg_take_pending_chief(void) {
  const int c = g_popup_msg_pending_chief;
  g_popup_msg_pending_chief = 0;
  return c;
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
  /* FUN_6f74_06d0 box[+0x28] default (port_bugs.md #1216); @width overrides it as given. */
  return 0x50;
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
  char raw[1024]; /* @TUTORIAL17's body is 766 chars */
  raw[0] = '\0';
  const ColonizeMsgSection* sec =
    (catalog && section_name) ? assets_msg_find(catalog, section_name) : NULL;
  g_popup_msg_pending_width = sec ? popup_msg_section_width(sec) : 0;
  g_popup_msg_pending_default = sec ? popup_msg_section_default(sec) : 0;
  /* DOS keys the graphic off the call site, not GAME.TXT content — set it
   * from the name even when the section is missing and the fallback shows. */
  g_popup_msg_pending_graphic = popup_msg_mss_index_for_section(section_name);
  g_popup_msg_pending_chief = popup_msg_section_has_chief_portrait(section_name);
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

void popup_msg_fill_status(
  const ColonizeMsgCatalog* catalog,
  const char* section_name,
  const PopupMsgTokens* tok,
  char* out,
  size_t out_size
) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  const ColonizeMsgSection* sec =
    (catalog && section_name) ? assets_msg_find(catalog, section_name) : NULL;
  if (!sec) {
    return;
  }
  char raw[512];
  raw[0] = '\0';
  popup_msg_section_body(sec, raw, sizeof(raw), true);
  for (char* p = raw; *p; ++p) {
    if ((*p == '.' || *p == '!' || *p == '?') && (p[1] == ' ' || p[1] == '\n' || p[1] == '\0')) {
      p[1] = '\0';
      break;
    }
  }
  popup_msg_apply_tokens(out, out_size, raw, tok);
  popup_msg_strip_markup(out);
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
   * next blank (state 3 = ignored trailer). This is the sole mechanism for
   * a real catalog (blank_before is always populated by the loader). */
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
      if (popup_msg_is_trailing_caption(section, i)) {
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
  /*
   * section->blank_before is always populated by the real loader
   * (assets_msg_load_file, assets.c) — the keyword-matching fallback that
   * used to run here for a NULL blank_before never executes against a real
   * catalog and was deleted (2026-09-21, no-dos-text-in-binary sweep).
   * What remains: choice-only fragments (e.g. @TAXOPTIONS, no prose at
   * all) collect every non-directive line below.
   */
  /* Only a section with NO prose at all is a choice-only fragment; a body
   * with no blank-line boundary simply has no choices (HEAD's `!saw_prose`
   * guard, dropped by mistake when the keyword backstop was deleted). */
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
  return count;
}


/*
 * Every content row of a section, in order — for the GAME.TXT fragments DOS
 * reads line by line instead of through the dialog parser (@TAXOPTIONS:
 * 38fd:40a9 FUN_291f_0928 + 091c appends its two rows to the @KINGTAX box),
 * so they have no body/choice split for popup_msg_choices to find.
 */
int popup_msg_rows(
  const ColonizeMsgSection* section, char out[][POPUP_MSG_CHOICE_LEN], int max_rows
) {
  if (!section || !out || max_rows <= 0) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < section->line_count && count < max_rows; ++i) {
    const char* line = section->lines[i];
    if (!line || line[0] == '\0' || popup_msg_is_directive(line)) {
      continue;
    }
    str_copy_trunc(out[count], POPUP_MSG_CHOICE_LEN, line);
    count++;
  }
  return count;
}

int popup_msg_section_labels(
  const ColonizeMsgCatalog* catalog,
  const char* section_name,
  const PopupMsgTokens* tok,
  const char* fallback0,
  const char* fallback1,
  char buf[2][POPUP_MSG_CHOICE_LEN],
  const char* labels[2]
) {
  if (!buf || !labels) {
    return 0;
  }
  buf[0][0] = '\0';
  buf[1][0] = '\0';
  labels[0] = buf[0];
  labels[1] = buf[1];
  const ColonizeMsgSection* section =
    (catalog && section_name) ? assets_msg_find(catalog, section_name) : NULL;
  char raw[2][POPUP_MSG_CHOICE_LEN];
  const int found = popup_msg_choices(section, raw, 2);
  const char* src[2] = {fallback0, fallback1};
  if (found >= 2) {
    src[0] = raw[0];
    src[1] = raw[1];
  }
  for (int i = 0; i < 2; ++i) {
    popup_msg_apply_tokens(buf[i], POPUP_MSG_CHOICE_LEN, src[i] ? src[i] : "", tok);
  }
  return found >= 2 ? found : 0;
}
