#ifndef COLONIZE_CORE_REPORTS_NAMES_H
#define COLONIZE_CORE_REPORTS_NAMES_H

/*
 * Internal seam between reports_names.c (SIM/SHARED: NAMES.TXT / LABELS.TXT
 * lookups) and reports.c (UI: the F2-F10 report screens). The public display
 * -name accessors stay declared in reports.h; this header only exposes the
 * helpers and fallback tables that used to be `static` inside reports.c
 * before the 2026-09-16 sim/UI split. Nothing new, nothing renamed.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void reports_names_free_catalogs(void);

const char* reports_names_field(const char* section, int row, int col);
const char* reports_labels_field(const char* section, int index);
const char* reports_misc_word(int index, const char* fallback, char* out, size_t out_sz);
const char* reports_ctitle_word(int index);

const char* reports_ff_name(int idx);
const char* reports_job_name(int job);
const char* reports_cargo_name(int cargo);
const char* reports_tribe_name(int t);
const char* reports_nation_adjective(int nation);
const char* reports_tribe_level(uint8_t tech);

extern const int k_job_count;

#endif /* COLONIZE_CORE_REPORTS_NAMES_H */
