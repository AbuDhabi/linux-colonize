/*
 * DOS VR_SEED.EXE (seed 100) idle-campaign autosaves. Each invocation starts
 * from the first save in one uninterrupted yearly run, then advances the
 * headless game one skipped human turn at a time. This is an opt-in future
 * fidelity gate: the whole serialized save must match DOS after every turn.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/common/golden_fixture.h"

#define SAVE_DIR "original_saves/1492-1600-seed-100"
#define VR_SEED 100u

static int read_bytes(const char* path, uint8_t** data, size_t* size) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", path);
    return 0;
  }
  if (fseek(f, 0, SEEK_END) != 0) goto fail;
  long len = ftell(f);
  if (len < 0 || fseek(f, 0, SEEK_SET) != 0) goto fail;
  *data = malloc((size_t)len ? (size_t)len : 1);
  if (!*data) goto fail;
  *size = (size_t)len;
  if (fread(*data, 1, *size, f) != *size) {
    free(*data);
    *data = NULL;
    goto fail;
  }
  fclose(f);
  return 1;
fail:
  fprintf(stderr, "cannot read %s\n", path);
  fclose(f);
  return 0;
}

static int compare_save(const ColonizeCol1Save* got, const char* expected_path) {
  uint8_t *actual = NULL, *expected = NULL;
  size_t actual_size = 0, expected_size = 0;
  char err[256];
  int ok = 0;
  if (!col1_save_write_memory(got, &actual, &actual_size, err, sizeof(err))) {
    fprintf(stderr, "serialize %s: %s\n", expected_path, err);
    goto done;
  }
  if (!read_bytes(expected_path, &expected, &expected_size)) goto done;
  const size_t common = actual_size < expected_size ? actual_size : expected_size;
  size_t offset = 0;
  while (offset < common && actual[offset] == expected[offset]) ++offset;
  if (offset == common && actual_size == expected_size) {
    ok = 1;
  } else if (offset < common) {
    fprintf(stderr, "%s: first byte mismatch at 0x%zx: got %02x, DOS %02x\n",
            expected_path, offset, actual[offset], expected[offset]);
  } else {
    fprintf(stderr, "%s: size mismatch: got %zu bytes, DOS %zu bytes\n",
            expected_path, actual_size, expected_size);
  }
  if (!ok) {
    const char* dump = getenv("GOLDEN_IDLE_DUMP");
    if (dump && *dump) {
      FILE* f = fopen(dump, "wb");
      int written = f && fwrite(actual, 1, actual_size, f) == actual_size;
      if (f && fclose(f) != 0) written = 0;
      if (written) {
        fprintf(stderr, "wrote simulated save to %s\n", dump);
      } else {
        fprintf(stderr, "could not write simulated save to %s\n", dump);
      }
    }
  }
done:
  free(actual);
  free(expected);
  return ok;
}

static int parse_year(const char* text, int* year) {
  char* end;
  long value = strtol(text, &end, 10);
  if (*text == '\0' || *end != '\0' || value < 1492 || value > 1599) return 0;
  *year = (int)value;
  return 1;
}

static void save_path(char* path, size_t size, int year) {
  snprintf(path, size, SAVE_DIR "/year_%d.sav", year);
}

int main(int argc, char** argv) {
  int first, last;
  if (argc != 3 || !parse_year(argv[1], &first) ||
      !parse_year(argv[2], &last) || first >= last) {
    fprintf(stderr, "usage: %s FIRST_YEAR LAST_YEAR (one contiguous saved run)\n", argv[0]);
    return 2;
  }

  char from[128], expected[128];
  save_path(from, sizeof(from), first);
  save_path(expected, sizeof(expected), first + 1);
  GoldenFixture fx;
  if (!golden_open(from, expected, VR_SEED, &fx)) {
    golden_close(&fx);
    return 1;
  }
  int ok = 1;
  if (fx.orig.head.year != first || fx.expect.head.year != first + 1) {
    fprintf(stderr, "save filename/calendar mismatch near %d\n", first);
    ok = 0;
  }
  for (int year = first + 1; ok && year <= last; ++year) {
    save_path(expected, sizeof(expected), year);
    if (year != first + 1) {
      char err[256];
      col1_save_free(&fx.expect);
      col1_save_init(&fx.expect);
      if (!col1_save_read_file(expected, &fx.expect, err, sizeof(err))) {
        fprintf(stderr, "read %s: %s\n", expected, err);
        ok = 0;
        break;
      }
    }
    if (fx.expect.head.year != year) {
      fprintf(stderr, "%s records year %u\n", expected, fx.expect.head.year);
      ok = 0;
      break;
    }
    if (!golden_turn(&fx)) {
      ok = 0;
      break;
    }
    if (!compare_save(&fx.start, expected)) {
      ok = 0;
      break;
    }
    printf("%d -> %d byte-identical\n", year - 1, year);
  }
  golden_close(&fx);
  return ok ? 0 : 1;
}
