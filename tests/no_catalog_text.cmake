# lint_no_catalog_text: no MicroProse wording compiled into opencol
# (docs/data_vs_hardcoded.md Part D).
#
# Method: run `strings` on the built binary, then
#  1. reject a fixed list of phrases that regressed before (below), and
#  2. reject any GAME.TXT template line copied in as a C format string. Every
#     COLONIZE/GAME.TXT line carrying a %STRINGn / %NUMBERn / %COUNTRY token is
#     normalised: {} emphasis and a leading ^/^^ dropped, %STRINGn and %COUNTRY
#     -> %s, %NUMBERn -> %d. Both that printf spelling ("Purchase %s for %d$?")
#     and the raw token spelling are searched in the strings dump: as a whole
#     C string always, and as a substring too once the line has 16+ letters
#     (short lines such as "near %s." occur inside port-authored text). Lines
#     whose literal part has fewer than 4 letters ("%s %s") are skipped.
#     Templates are line-wrapped in GAME.TXT, so a sentence that a C fallback
#     joined across a wrap is only caught through one of its lines.

if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
  message(FATAL_ERROR "opencol binary not found: ${BINARY}")
endif()

find_program(STRINGS_TOOL strings REQUIRED)
execute_process(
  COMMAND "${STRINGS_TOOL}" -n 5 "${BINARY}"
  RESULT_VARIABLE strings_rc
  OUTPUT_VARIABLE binary_strings
)
if(NOT strings_rc EQUAL 0)
  message(FATAL_ERROR "strings failed for ${BINARY}")
endif()

# Known regressions from the user-facing text audit. These phrases must be
# assembled from COLONIZE catalogs at runtime, never retained as C fallbacks.
set(forbidden_catalog_text
  "Purchase %STRING0 for %NUMBER0$?"
  "(Unexplored)"
  "(Plowed)"
  "(Road)"
  "(Major River)"
  "(River)"
  "(Lost City Rumor)"
  # Typed-fallback removals that had no guard until the 2026-09-26 test-gap
  # audit. Each needle was checked absent from the current binary before it
  # was added here; all are long enough for `strings -n 5`.
  "wagon train"                                  # bugs.md #802
  "Tribute paid; tensions ease with the %s."     # bugs.md #802
  "(Cost: %d)"                                   # bugs.md #602
  "The %s raid %s."                              # bugs.md #836
  "The %s raid your colony."                     # bugs.md #836
  "%s raiders set fires in %s."                  # bugs.md #836
  "%s raiders set fires."                        # bugs.md #836
  "The %s declare war! Prepare for WAR!"         # bugs.md #836
  "Cigar"                                        # bugs.md #905
)

foreach(needle IN LISTS forbidden_catalog_text)
  string(FIND "${binary_strings}" "${needle}" found_at)
  if(NOT found_at EQUAL -1)
    message(FATAL_ERROR "Catalog wording compiled into opencol: ${needle}")
  endif()
endforeach()

set(game_txt "${CMAKE_CURRENT_LIST_DIR}/../COLONIZE/GAME.TXT")
if(NOT EXISTS "${game_txt}")
  message(FATAL_ERROR "GAME.TXT not found: ${game_txt}")
endif()
file(STRINGS "${game_txt}" template_lines REGEX "%(STRING|NUMBER|COUNTRY)")
list(REMOVE_DUPLICATES template_lines)
foreach(line IN LISTS template_lines)
  string(REGEX REPLACE "[\r{}]" "" raw "${line}")
  string(REGEX REPLACE "^\\^+" "" raw "${raw}")
  string(STRIP "${raw}" raw)
  string(REGEX REPLACE "%(STRING[0-9]|COUNTRY)" "%s" printf_form "${raw}")
  string(REGEX REPLACE "%NUMBER[0-9]" "%d" printf_form "${printf_form}")
  string(REGEX REPLACE "%[sd%]|[^A-Za-z]" "" letters "${printf_form}")
  string(LENGTH "${letters}" letter_count)
  if(letter_count LESS 4)
    continue()
  endif()
  foreach(needle IN ITEMS "${printf_form}" "${raw}")
    if(letter_count LESS 16)
      string(FIND "\n${binary_strings}" "\n${needle}\n" found_at)
    else()
      string(FIND "${binary_strings}" "${needle}" found_at)
    endif()
    if(NOT found_at EQUAL -1)
      message(SEND_ERROR "GAME.TXT template compiled into opencol: ${needle}")
    endif()
  endforeach()
endforeach()
