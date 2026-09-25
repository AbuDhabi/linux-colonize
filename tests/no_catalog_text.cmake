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
)

foreach(needle IN LISTS forbidden_catalog_text)
  string(FIND "${binary_strings}" "${needle}" found_at)
  if(NOT found_at EQUAL -1)
    message(FATAL_ERROR "Catalog wording compiled into colonize_linux: ${needle}")
  endif()
endforeach()
