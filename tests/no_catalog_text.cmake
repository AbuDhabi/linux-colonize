if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
  message(FATAL_ERROR "colonize_linux binary not found: ${BINARY}")
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
    message(FATAL_ERROR "Catalog wording compiled into colonize_linux: ${needle}")
  endif()
endforeach()
