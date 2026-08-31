#ifndef COLONIZE_POPUP_MSG_H
#define COLONIZE_POPUP_MSG_H

#include <stdbool.h>
#include <stddef.h>

#include "core/assets.h"

/*
 * GAME.TXT helpers for wood popups: collect body lines (skip @directives and
 * choice-only trails) and substitute common %STRING/%NUMBER/{brace} tokens.
 */

typedef struct PopupMsgTokens {
  const char* string0;
  const char* string1;
  const char* string2;
  const char* string3;
  const char* string4;
  const char* country;
  int number0;
  int number1;
  int number2;
  bool has_number0;
  bool has_number1;
  bool has_number2;
} PopupMsgTokens;

/* True if line is an @directive (width/default/checkbox/…). */
bool popup_msg_is_directive(const char* line);

/*
 * Append non-directive prose lines from section into out (space-joined).
 * Stops before trailing Yes/No / OK choice rows when stop_before_choices.
 * Returns bytes written (excluding NUL), or 0 on empty.
 */
size_t popup_msg_section_body(
  const ColonizeMsgSection* section,
  char* out,
  size_t out_size,
  bool stop_before_choices
);

/* In-place / into dst: replace %STRING0..4, %NUMBER0..2, %COUNTRY, strip {} . */
void popup_msg_apply_tokens(
  char* dst,
  size_t dst_size,
  const char* src,
  const PopupMsgTokens* tok
);

/*
 * Convenience: find section in catalog, build body with tokens.
 * Falls back to fallback if section missing or empty. Always NUL-terminates.
 */
/*
 * GAME.TXT `@width=NNN` of a section (DOS 6f74 compositor dialog width in
 * 320-px screen units), 0 when the section has no directive.
 */
int popup_msg_section_width(const ColonizeMsgSection* section);
/*
 * Width side-channel (port_plan P11.3): popup_msg_fill records the section's
 * @width; the next ai_popup enqueue takes it (and clears it) so every
 * existing fill→enqueue call site sizes like DOS without a signature change.
 */
int popup_msg_take_pending_width(void);
/*
 * MSS graphic side-channel (same pattern as the width one): DOS sets a
 * DS:0x1f5e latch (FUN_281f_0652(tag, index)) before a popup so the 6f74
 * compositor decorates it with MSS{index}.SS (0 admiral / 1 continental
 * soldier / 2 courtier / 3 frontiersman / 4 friar / 5 nun). The call-site →
 * index pairs were lifted from the VICEROY.EXE asm and keyed here by section
 * name; popup_msg_fill records the section's index (-1 = no graphic) and the
 * next ai_popup enqueue takes it. MYR (Euro diplomacy ruler, index = nation)
 * stays explicit via ai_popup_set_last_graphic_myr.
 */
int popup_msg_take_pending_graphic(void);
/* Section-name → MSS index (-1 when the section has no DOS graphic). */
int popup_msg_mss_index_for_section(const char* section_name);
void popup_msg_fill(
  const ColonizeMsgCatalog* catalog,
  const char* section_name,
  const PopupMsgTokens* tok,
  const char* fallback,
  char* out,
  size_t out_size
);

/* Must fit longest GAME.TXT choice (@DECLARE Never… = 53 with quotes). */
#define POPUP_MSG_CHOICE_LEN 64

/* Extract Yes/No (or listed) choice labels from section; returns count (0..max). */
int popup_msg_choices(
  const ColonizeMsgSection* section,
  char out[][POPUP_MSG_CHOICE_LEN],
  int max_choices
);

#endif
