/*
 * sav_json: COLONY##.SAV <-> JSON converter.
 *
 *   sav_json COLONY00.SAV COLONY00.json   -- SAV -> JSON
 *   sav_json COLONY00.json COLONY00.SAV   -- JSON -> SAV
 *   sav_json COLONY00.SAV                 -- writes COLONY00.SAV.json
 *   sav_json COLONY00.json                -- writes COLONY00.json.SAV
 *
 * Direction is chosen from the input file's extension (case-insensitive
 * ".sav" vs anything else treated as JSON). Output defaults to the input
 * path with the format swapped; pass an explicit output path to override.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "col1_json.h"
#include "core/col1_save.h"
#include "json_min.h"

static bool has_sav_ext(const char* path) {
  size_t len = strlen(path);
  if (len < 4) {
    return false;
  }
  const char* ext = path + len - 4;
  return (ext[0] == '.') &&
         (tolower((unsigned char)ext[1]) == 's') &&
         (tolower((unsigned char)ext[2]) == 'a') &&
         (tolower((unsigned char)ext[3]) == 'v');
}

static char* read_whole_file(const char* path, size_t* out_len) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }
  fseek(f, 0, SEEK_SET);
  char* buf = malloc((size_t)size + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t got = fread(buf, 1, (size_t)size, f);
  fclose(f);
  buf[got] = '\0';
  if (out_len) {
    *out_len = got;
  }
  return buf;
}

static int sav_to_json(const char* in_path, const char* out_path) {
  ColonizeCol1Save save;
  col1_save_init(&save);
  char err[256];
  if (!col1_save_read_file(in_path, &save, err, sizeof err)) {
    fprintf(stderr, "sav_json: reading %s: %s\n", in_path, err);
    return 1;
  }
  FILE* out = strcmp(out_path, "-") == 0 ? stdout : fopen(out_path, "wb");
  if (!out) {
    fprintf(stderr, "sav_json: cannot open %s for write\n", out_path);
    col1_save_free(&save);
    return 1;
  }
  col1_write_json(out, &save);
  if (out != stdout) {
    fclose(out);
  }
  col1_save_free(&save);
  fprintf(stderr, "sav_json: %s -> %s\n", in_path, out_path);
  return 0;
}

static int json_to_sav(const char* in_path, const char* out_path) {
  size_t len = 0;
  char* text = read_whole_file(in_path, &len);
  if (!text) {
    fprintf(stderr, "sav_json: cannot read %s\n", in_path);
    return 1;
  }
  char perr_buf[256];
  JsonValue* root = json_parse(text, perr_buf, sizeof perr_buf);
  free(text);
  if (!root) {
    fprintf(stderr, "sav_json: %s: %s\n", in_path, perr_buf);
    return 1;
  }

  ColonizeCol1Save save;
  col1_save_init(&save);
  char err[256];
  bool ok = col1_read_json(root, &save, err, sizeof err);
  json_free(root);
  if (!ok) {
    fprintf(stderr, "sav_json: %s: %s\n", in_path, err);
    col1_save_free(&save);
    return 1;
  }

  if (!col1_save_write_file(out_path, &save, err, sizeof err)) {
    fprintf(stderr, "sav_json: writing %s: %s\n", out_path, err);
    col1_save_free(&save);
    return 1;
  }
  col1_save_free(&save);
  fprintf(stderr, "sav_json: %s -> %s\n", in_path, out_path);
  return 0;
}

static char* default_out_path(const char* in_path, bool in_is_sav) {
  size_t len = strlen(in_path);
  char* out;
  if (in_is_sav) {
    /* COLONY00.SAV -> COLONY00.SAV.json (kept distinct from the input; also
     * handles inputs with no recognizable "swap the extension" shape). */
    out = malloc(len + 6);
    snprintf(out, len + 6, "%s.json", in_path);
  } else {
    out = malloc(len + 5);
    snprintf(out, len + 5, "%s.SAV", in_path);
  }
  return out;
}

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    fprintf(
      stderr,
      "usage: %s <in.SAV|in.json> [out.json|out.SAV]\n"
      "  SAV -> JSON if the input ends in .SAV/.sav, JSON -> SAV otherwise.\n",
      argv[0]
    );
    return 2;
  }

  const char* in_path = argv[1];
  bool in_is_sav = has_sav_ext(in_path);
  char* default_out = NULL;
  const char* out_path = argc == 3 ? argv[2] : (default_out = default_out_path(in_path, in_is_sav));

  int rc = in_is_sav ? sav_to_json(in_path, out_path) : json_to_sav(in_path, out_path);
  free(default_out);
  return rc;
}
