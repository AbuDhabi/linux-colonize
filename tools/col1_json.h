#ifndef COLONIZE_COL1_JSON_H
#define COLONIZE_COL1_JSON_H

/*
 * COLONY##.SAV <-> JSON. Field-for-field mapping of ColonizeCol1Save
 * (src/core/col1_save.h) onto a JSON document: named fields for everything
 * with a known meaning, hex strings for opaque/pad byte blobs and the four
 * raw map planes (tile/mask/path/seen — one hex string each, map_w*map_h
 * bytes). See docs/save_format_map.md for what each field means.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "core/col1_save.h"
#include "json_min.h"

/* Streams a full JSON document for `s` to `f`. */
void col1_write_json(FILE* f, const ColonizeCol1Save* s);

/* Fills `out` (freshly col1_save_init'd) from a parsed JSON tree. Allocates
 * variable-length sections (colony/unit/tribe/map) itself once head counts
 * are known. Returns false with a message in err/err_size on a structural
 * problem (missing required field, bad array length, bad hex). */
bool col1_read_json(const JsonValue* root, ColonizeCol1Save* out, char* err, size_t err_size);

#endif
