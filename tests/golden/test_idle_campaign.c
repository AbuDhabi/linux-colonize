/*
 * DOS VR_SEED.EXE (seed 100) idle-campaign autosaves. Each invocation starts
 * from the first save in one uninterrupted yearly run, then advances the
 * headless game one skipped human turn at a time. The whole serialized save
 * must match DOS after every turn, except the fields compare_save ignores.
 * CTest runs it continuously from year_1492 to year_1599
 * (GOLDEN_IDLE_KEEP_GOING); docs/idle_campaign.md has the status.
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

/*
 * Archived years whose DOS save carries unsaved DOS state the port cannot
 * reach (docs/idle_campaign.md "1505/1506"): simulated through, not compared.
 * The continuous run matches again from 1508.
 */
static int known_residue_year(int year) {
  return year == 1505 || year == 1506;
}

/*
 * Ignored when comparing: the human's first-turn UI state, which the headless
 * harness never plays (View mode, tutorial flags, map focus tile), and the
 * port-only COLNXEXT tail past the DOS sections. They are copied from the DOS
 * save into the serialized copy only; the simulated state keeps its own.
 */
static int compare_save(
  ColonizeCol1Save* got, const ColonizeCol1Save* expect, const char* expected_path, int year
) {
  uint8_t *actual = NULL, *expected = NULL;
  size_t actual_size = 0, expected_size = 0;
  char err[256];
  int ok = 0;
  const uint16_t keep_map_mode = got->head.map_mode;
  const ColonizeCol1Tut2 keep_tut2 = got->head.tut2;
  const uint16_t keep_x = got->stuff.x, keep_y = got->stuff.y;
  if (expect) {
    got->head.map_mode = expect->head.map_mode;
    got->head.tut2 = expect->head.tut2;
    got->stuff.x = expect->stuff.x;
    got->stuff.y = expect->stuff.y;
  }
  const int written = col1_save_write_memory(got, &actual, &actual_size, err, sizeof(err));
  got->head.map_mode = keep_map_mode;
  got->head.tut2 = keep_tut2;
  got->stuff.x = keep_x;
  got->stuff.y = keep_y;
  if (!written) {
    fprintf(stderr, "serialize %d: %s\n", year, err);
    goto done;
  }
  /* No archived save for this year: only dump. */
  if (!expected_path) goto dump;
  if (!read_bytes(expected_path, &expected, &expected_size)) goto done;
  if (actual_size > expected_size + 8 && memcmp(actual + expected_size, "COLNXEXT", 8) == 0) {
    actual_size = expected_size;
  }
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
dump:
  if (!ok) {
    /* A "%d" in GOLDEN_IDLE_DUMP takes the year (one file per KEEP_GOING turn). */
    const char* dump_env = getenv("GOLDEN_IDLE_DUMP");
    char dump[512];
    if (dump_env && *dump_env) {
      const char* pct = strstr(dump_env, "%d");
      if (pct) {
        snprintf(dump, sizeof(dump), "%.*s%d%s", (int)(pct - dump_env), dump_env, year, pct + 2);
      } else {
        snprintf(dump, sizeof(dump), "%s", dump_env);
      }
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

  char from[512], expected[128];
  save_path(from, sizeof(from), first);
  /* GOLDEN_IDLE_FROM: start from this save instead of year_FIRST.sav (e.g. a
   * DOSBox from-load autosave). */
  const char* from_env = getenv("GOLDEN_IDLE_FROM");
  if (from_env && *from_env) snprintf(from, sizeof(from), "%s", from_env);
  /* The fixture wants an expected save; the loop reloads it per year. */
  GoldenFixture fx;
  if (!golden_open(from, from, VR_SEED, &fx)) {
    golden_close(&fx);
    return 1;
  }
  int ok = 1;
  if (fx.orig.head.year != first) {
    fprintf(stderr, "save filename/calendar mismatch near %d\n", first);
    ok = 0;
  }
  /* GOLDEN_IDLE_KEEP_GOING: keep simulating past a mismatch (multi-turn
   * from-load DOSBox targets, or one continuous run from year_1492 through
   * the archive's missing years; GOLDEN_IDLE_DUMP may take a "%d"). */
  const char* keep = getenv("GOLDEN_IDLE_KEEP_GOING");
  const int keep_going = keep && *keep;
  for (int year = first + 1; (ok || keep_going) && year <= last; ++year) {
    save_path(expected, sizeof(expected), year);
    int have = 1;
    {
      char err[256];
      col1_save_free(&fx.expect);
      col1_save_init(&fx.expect);
      if (!col1_save_read_file(expected, &fx.expect, err, sizeof(err))) {
        /* The archive skips some years; a continuous run plays through them. */
        if (keep_going) {
          have = 0;
        } else {
          fprintf(stderr, "read %s: %s\n", expected, err);
          ok = 0;
          break;
        }
      }
    }
    if (have && fx.expect.head.year != year) {
      fprintf(stderr, "%s records year %u\n", expected, fx.expect.head.year);
      ok = 0;
      break;
    }
    /* The skipped human turn: FUN_2b5a_3b68 opens Move Pieces with
     * DS:0x5392 = 0xffff (raw 46833) and the idle player ends it there. */
    fx.units.selected_id = -1;
    if (!golden_turn(&fx)) {
      ok = 0;
      break;
    }
    col1_bridge_stamp_autosave_head(&fx.start);
    if (!have || known_residue_year(year)) {
      compare_save(&fx.start, NULL, NULL, year);
      continue;
    }
    if (!compare_save(&fx.start, &fx.expect, expected, year)) {
      ok = 0;
      if (keep_going) continue;
      break;
    }
    printf("%d -> %d byte-identical\n", year - 1, year);
  }
  golden_close(&fx);
  return ok ? 0 : 1;
}
