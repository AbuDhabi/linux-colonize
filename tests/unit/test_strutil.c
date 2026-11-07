/* unit_strutil: the shared text helpers (strutil.c) and the sav_json reader /
 * writer (json_min.c). Coverage gaps found 2026-11-07: str_strip_quotes,
 * file_slurp / file_replace, json_get_hex, json_get_cstr_fixed, the array
 * accessors and the writer side had no direct case (json depth and number
 * grammar live in unit_settings test_json_min_edges). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/test_runner.h"
#include "core/json_min.h"
#include "core/strutil.h"

static int g_failures = 0;

static void check(bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    g_failures++;
  }
}

static const char* k_tmp = "build/test_strutil.tmp";
static const char* k_dst = "build/test_strutil.dat";

static void test_copy_and_join(void) {
  char buf[6];
  str_copy_trunc(buf, sizeof(buf), "abcdefgh");
  check(strcmp(buf, "abcde") == 0, "copy truncates to dst_sz-1");
  str_copy_trunc(buf, sizeof(buf), NULL);
  check(buf[0] == '\0', "copy of NULL is empty");
  str_copy_trunc(buf, 1, "x");
  check(buf[0] == '\0', "dst_sz 1 holds only the NUL");

  char path[16];
  str_path_join(path, sizeof(path), "dir", "f.txt");
  check(strcmp(path, "dir/f.txt") == 0, "join inserts one slash");
  str_path_join(path, sizeof(path), "dir/", "f.txt");
  check(strcmp(path, "dir/f.txt") == 0, "join keeps an existing slash");
  str_path_join(path, sizeof(path), "", "f.txt");
  check(strcmp(path, "f.txt") == 0, "empty dir gives name only");
  str_path_join(path, sizeof(path), NULL, NULL);
  check(path[0] == '\0', "NULL dir + NULL name is empty");
  str_path_join(path, 8, "abcdef", "ghij");
  check(strcmp(path, "abcdef/") == 0, "join truncates at dst_sz-1");
}

static void test_trim_and_strip(void) {
  char s[32];
  strcpy(s, " \t Farmer \t\r\n");
  str_trim(s);
  check(strcmp(s, "Farmer") == 0, "trim both ends");
  strcpy(s, "\rX");
  str_trim(s);
  check(strcmp(s, "\rX") == 0, "leading CR is not in the leading set");
  strcpy(s, "   ");
  str_trim(s);
  check(s[0] == '\0', "all-blank trims to empty");

  strcpy(s, "~F~ood{}^_");
  str_strip_chars(s, "~{}^_");
  check(strcmp(s, "Food") == 0, "strip_chars removes every set char");

  strcpy(s, "\"New Amsterdam\"");
  str_strip_quotes(s);
  check(strcmp(s, "New Amsterdam") == 0, "strip one quote pair");
  strcpy(s, "\"\"");
  str_strip_quotes(s);
  check(s[0] == '\0', "empty quoted string");
  strcpy(s, "\"");
  str_strip_quotes(s);
  check(strcmp(s, "\"") == 0, "lone quote untouched");
  strcpy(s, "\"open");
  str_strip_quotes(s);
  check(strcmp(s, "\"open") == 0, "unclosed quote untouched");
  strcpy(s, "a\"b\"");
  str_strip_quotes(s);
  check(strcmp(s, "a\"b\"") == 0, "quote not at start untouched");
}

static void test_fields_and_rows(void) {
  const char* cur = "  12,\t-3 ,7x";
  int v = 0;
  check(str_next_int_field(&cur, &v) && v == 12, "first int field");
  check(str_next_int_field(&cur, &v) && v == -3, "negative int field");
  check(str_next_int_field(&cur, &v) && v == 7, "field ends at non-digit");
  v = 99;
  check(!str_next_int_field(&cur, &v) && v == 99, "non-digit stops, out untouched");
  cur = " , ";
  check(!str_next_int_field(&cur, &v), "separators only is end of row");

  char row[64];
  strcpy(row, "  Hardy Pioneer , 3, 4 ; comment, with comma");
  char* rest = str_split_name_row(row);
  check(rest && strcmp(row, "Hardy Pioneer") == 0, "row name trimmed");
  check(rest && strcmp(rest, "3, 4") == 0, "row rest trimmed, comment dropped");
  strcpy(row, "; Name, a, b");
  check(str_split_name_row(row) == NULL, "comment row has no fields");
  strcpy(row, "nocomma");
  check(str_split_name_row(row) == NULL, "row without comma");
}

static void test_file_slurp_replace(void) {
  uint8_t* buf = (uint8_t*)1;
  size_t n = 7;
  char err[128];
  remove(k_dst);
  check(!file_slurp(k_dst, &buf, &n, err, sizeof(err)), "missing file fails");
  check(buf == NULL && n == 0, "failure clears outputs");
  check(strstr(err, "cannot open") != NULL, "missing file names the error");

  FILE* f = fopen(k_tmp, "wb");
  check(f != NULL, "open tmp");
  if (!f) {
    return;
  }
  fclose(f);
  check(file_slurp(k_tmp, &buf, &n, err, sizeof(err)), "empty file slurps");
  check(buf != NULL && n == 0 && err[0] == '\0', "empty file: buffer, size 0, err cleared");
  free(buf);

  f = fopen(k_dst, "wb");
  fputs("old", f);
  fclose(f);
  f = fopen(k_tmp, "wb");
  fwrite("new\0data", 1, 8, f);
  fclose(f);
  check(file_replace(k_tmp, k_dst), "replace over existing file");
  check(!file_slurp(k_tmp, &buf, &n, NULL, 0), "tmp is gone after replace");
  check(file_slurp(k_dst, &buf, &n, NULL, 0), "dst slurps");
  check(n == 8 && memcmp(buf, "new\0data", 8) == 0, "dst holds tmp bytes, NUL included");
  free(buf);
  remove(k_dst);
}

static void test_json_accessors(void) {
  char err[128];
  JsonValue* v = json_parse(
    " {\"a\":[1,2,3,4,5,6,7,8,9,10], \"t\":true, \"f\":false, \"z\":null,"
    " \"k0\":0,\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,\"k8\":8,"
    " \"h\":\"00fF10\", \"s\":\"Jamestown\"} ",
    err, sizeof(err)
  );
  check(v != NULL && err[0] == '\0', "doc with >8 items and >8 keys parses");
  if (!v) {
    return;
  }
  const JsonValue* a = json_obj_get(v, "a");
  check(json_arr_len(a) == 10, "array grew past initial cap");
  check(json_arr_at(a, 9) && json_arr_at(a, 9)->num == 10.0, "last array item");
  check(json_arr_at(a, 10) == NULL, "array index out of range");
  check(json_arr_len(v) == 0 && json_arr_at(v, 0) == NULL, "object is not an array");
  int64_t i = 0;
  check(json_get_i64(v, "k8", &i) && i == 8, "ninth key reads");
  bool b = false;
  check(json_get_bool(v, "t", &b) && b, "true");
  check(json_get_bool(v, "f", &b) && !b, "false");
  check(!json_get_bool(v, "k1", &b), "number is not a bool");
  check(json_obj_get(v, "z") && json_obj_get(v, "z")->type == JV_NULL, "null");
  check(json_get_str(v, "missing") == NULL && json_get_str(v, "k1") == NULL, "str missing / wrong type");
  check(json_obj_get(a, "x") == NULL, "obj_get on array");

  uint8_t hex[3] = {0};
  check(json_get_hex(v, "h", hex, 3) && hex[0] == 0 && hex[1] == 0xff && hex[2] == 0x10,
        "hex mixed case decodes");
  check(!json_get_hex(v, "h", hex, 2), "hex length mismatch");
  check(!json_get_hex(v, "s", hex, 3), "non-hex chars refused");
  check(!json_get_hex(v, "k1", hex, 1), "hex of a number refused");

  char fixed[6];
  memset(fixed, 'X', sizeof(fixed));
  check(json_get_cstr_fixed(v, "s", fixed, sizeof(fixed)), "cstr_fixed reads");
  check(memcmp(fixed, "James\0", 6) == 0, "cstr_fixed truncates to len-1");
  char pad[12];
  memset(pad, 'X', sizeof(pad));
  check(json_get_cstr_fixed(v, "s", pad, sizeof(pad)), "cstr_fixed short");
  check(strcmp(pad, "Jamestown") == 0 && pad[10] == 0 && pad[11] == 0, "cstr_fixed NUL-pads");
  check(!json_get_cstr_fixed(v, "k1", pad, sizeof(pad)), "cstr_fixed of a number");
  json_free(v);
}

static void test_json_errors(void) {
  static const char* const bad[] = {
    "{\"a\":1} x",      /* trailing data */
    "[\"open",          /* unterminated string */
    "[tru]",            /* bad literal */
    "[\"\\q\"]",        /* bad escape */
    "[\"\\u12g4\"]",    /* bad \u hex */
    "{\"a\" 1}",        /* missing colon */
    "{\"a\":1 \"b\":2}", /* missing comma */
    "[1 2]",
    "{1:2}",            /* non-string key */
    "",
  };
  for (size_t k = 0; k < sizeof(bad) / sizeof(bad[0]); ++k) {
    char err[128] = "unset";
    check(json_parse(bad[k], err, sizeof(err)) == NULL, bad[k]);
    check(strstr(err, "json parse error") != NULL, "error message written");
  }
  JsonValue* v = json_parse("[{}, [], \"\"]", NULL, 0);
  check(v && json_arr_len(v) == 3, "empty containers parse");
  json_free(v);
}

/* Writer output must parse back to the same bytes (sav_json round trip):
 * CP437 bytes pass raw, control chars go out as \u00XX, \u escapes decode
 * to UTF-8. */
static void test_json_writer_round_trip(void) {
  FILE* f = fopen(k_tmp, "wb");
  check(f != NULL, "open writer tmp");
  if (!f) {
    return;
  }
  const char name[] = "a\"b\\c\n\t\r\x01\x82z";
  fputs("{\"s\":", f);
  json_write_escaped_string(f, name, 64);
  fputs(",\"cut\":", f);
  json_write_escaped_string(f, "abcdef", 3);
  fputs(",\"h\":", f);
  const uint8_t bytes[] = {0x00, 0xab, 0x7f};
  json_write_hex(f, bytes, sizeof(bytes));
  fputs(",\"u\":\"\\u20ac\\/\\b\\f\"}", f);
  fclose(f);

  uint8_t* text = NULL;
  size_t n = 0;
  check(file_slurp(k_tmp, &text, &n, NULL, 0), "slurp writer output");
  char* z = realloc(text, n + 1);
  if (!z) {
    free(text);
    return;
  }
  z[n] = '\0';
  check(strstr(z, "\\u0001") != NULL, "control char written as \\u0001");
  JsonValue* v = json_parse(z, NULL, 0);
  free(z);
  remove(k_tmp);
  check(v != NULL, "writer output parses");
  if (!v) {
    return;
  }
  const char* s = json_get_str(v, "s");
  check(s && strcmp(s, name) == 0, "escaped string round-trips byte for byte");
  s = json_get_str(v, "cut");
  check(s && strcmp(s, "abc") == 0, "max_len caps the written string");
  uint8_t back[3] = {1, 1, 1};
  check(json_get_hex(v, "h", back, 3) && memcmp(back, bytes, 3) == 0, "hex round-trips");
  s = json_get_str(v, "u");
  check(s && strcmp(s, "\xe2\x82\xac/\b\f") == 0, "\\u20ac to UTF-8, simple escapes");
  json_free(v);
}

#define CASE_(fn) \
  static int tc_##fn(void) { \
    const int before = g_failures; \
    fn(); \
    return g_failures != before; \
  }
CASE_(test_copy_and_join)
CASE_(test_trim_and_strip)
CASE_(test_fields_and_rows)
CASE_(test_file_slurp_replace)
CASE_(test_json_accessors)
CASE_(test_json_errors)
CASE_(test_json_writer_round_trip)
#undef CASE_

static const TestCase k_cases[] = {
  {"test_copy_and_join", tc_test_copy_and_join},
  {"test_trim_and_strip", tc_test_trim_and_strip},
  {"test_fields_and_rows", tc_test_fields_and_rows},
  {"test_file_slurp_replace", tc_test_file_slurp_replace},
  {"test_json_accessors", tc_test_json_accessors},
  {"test_json_errors", tc_test_json_errors},
  {"test_json_writer_round_trip", tc_test_json_writer_round_trip},
};
TEST_MAIN(k_cases)
