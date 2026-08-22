#ifndef COLONIZE_JSON_MIN_H
#define COLONIZE_JSON_MIN_H

/*
 * Minimal JSON reader for the sav_json tool. Not a general-purpose library:
 * just enough object/array/string/number/bool/null parsing to round-trip
 * col1_json's own output. Writing is done directly with fprintf in
 * col1_json.c (streaming, no tree needed on that side).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
  JV_NULL,
  JV_BOOL,
  JV_NUM,
  JV_STR,
  JV_ARR,
  JV_OBJ,
} JsonType;

typedef struct JsonValue JsonValue;

struct JsonValue {
  JsonType type;
  union {
    bool b;
    double num;
    char* str;
    struct {
      JsonValue** items;
      size_t count;
    } arr;
    struct {
      char** keys;
      JsonValue** vals;
      size_t count;
    } obj;
  };
};

/* Parses `text` (NUL-terminated) into a tree. Returns NULL on syntax error
 * (message in err/err_size). Caller owns the result; free with json_free. */
JsonValue* json_parse(const char* text, char* err, size_t err_size);
void json_free(JsonValue* v);

JsonValue* json_obj_get(const JsonValue* obj, const char* key);
size_t json_arr_len(const JsonValue* arr);
JsonValue* json_arr_at(const JsonValue* arr, size_t idx);

/* Typed getters: return false (leaving *out untouched) if the key is
 * missing or the wrong type, so callers can default/err as they choose. */
bool json_get_i64(const JsonValue* obj, const char* key, int64_t* out);
bool json_get_u64(const JsonValue* obj, const char* key, uint64_t* out);
bool json_get_bool(const JsonValue* obj, const char* key, bool* out);
const char* json_get_str(const JsonValue* obj, const char* key); /* NULL if missing/wrong type */

/* Decodes an even-length hex string field into `out` (exactly out_len bytes).
 * Returns false on missing field / length mismatch / bad hex. */
bool json_get_hex(const JsonValue* obj, const char* key, uint8_t* out, size_t out_len);

/* Copies a string field into a fixed-size buffer, NUL-padding the rest
 * (matches the fixed char[] fields in ColonizeCol1Save). Truncates silently
 * if longer than buf_len-1. Returns false if the key is missing/not a string. */
bool json_get_cstr_fixed(const JsonValue* obj, const char* key, char* buf, size_t buf_len);

/* --- writer-side helpers (used by col1_json.c's streaming fprintf writer) --- */
void json_write_escaped_string(FILE* f, const char* s, size_t max_len);
void json_write_hex(FILE* f, const uint8_t* data, size_t len);

#endif
