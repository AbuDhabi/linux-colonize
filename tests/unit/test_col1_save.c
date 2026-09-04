#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_bridge.h"
#include "core/col1_post_map.h"
#include "core/col1_save.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/units.h"
#include "platform/diagnostics.h"

static void fill_pattern(uint8_t* p, size_t n, uint8_t seed) {
  for (size_t i = 0; i < n; ++i) {
    p[i] = (uint8_t)(seed + (uint8_t)i * 3u + (uint8_t)(i >> 8));
  }
}

/* Read → encode → compare to on-disk bytes (codec only — not Linux→DOS playability).
 * FF pool stash is skipped until side table is initialized (sync/accrual). */
static bool assert_byte_identical_roundtrip(const char* path, ColonizeCol1Save* out, char* err, size_t err_size) {
  col1_save_init(out);
  if (!col1_save_read_file(path, out, err, err_size)) {
    fprintf(stderr, "fixture read failed %s: %s\n", path, err);
    return false;
  }
  uint8_t* enc = NULL;
  size_t enc_n = 0;
  if (!col1_save_write_memory(out, &enc, &enc_n, err, err_size)) {
    fprintf(stderr, "fixture encode failed %s: %s\n", path, err);
    col1_save_free(out);
    return false;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", path);
    free(enc);
    col1_save_free(out);
    return false;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);
  uint8_t* raw = malloc((size_t)sz);
  if (!raw || sz < 0 || fread(raw, 1, (size_t)sz, f) != (size_t)sz) {
    fprintf(stderr, "cannot reread %s\n", path);
    fclose(f);
    free(raw);
    free(enc);
    col1_save_free(out);
    return false;
  }
  fclose(f);
  if ((size_t)sz != enc_n || memcmp(raw, enc, enc_n) != 0) {
    fprintf(stderr, "fixture round-trip mismatch %s (%ld vs %zu)\n", path, sz, enc_n);
    if ((size_t)sz == enc_n) {
      for (size_t i = 0; i < enc_n; ++i) {
        if (raw[i] != enc[i]) {
          fprintf(stderr, "first diff at offset %zu: %02x vs %02x\n", i, raw[i], enc[i]);
          break;
        }
      }
    }
    free(raw);
    free(enc);
    col1_save_free(out);
    return false;
  }
  free(raw);
  free(enc);
  return true;
}

/* Phase 5: report every contiguous codec round-trip diff range, not just the
 * first byte (diagnostic; non-fatal). Used for any fixture still marked
 * byte_identical=false in k_fixtures below; capped at 40 ranges so a fully
 * garbled file doesn't flood stderr. W1.5 (2026-08-24): this used to stop at
 * the first differing byte, which was not enough to triage a real drift by
 * section/field — kept as the tool for any future non-identical fixture. */
static void report_codec_roundtrip_diff(const char* path) {
  char err[256];
  ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file(path, &save, err, sizeof(err))) {
    fprintf(stderr, "codec diff read fail %s: %s\n", path, err);
    return;
  }
  uint8_t* enc = NULL;
  size_t enc_n = 0;
  if (!col1_save_write_memory(&save, &enc, &enc_n, err, sizeof(err))) {
    fprintf(stderr, "codec diff encode fail %s: %s\n", path, err);
    col1_save_free(&save);
    return;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    free(enc);
    col1_save_free(&save);
    return;
  }
  fseek(f, 0, SEEK_END);
  long raw_n = ftell(f);
  rewind(f);
  uint8_t* raw = NULL;
  if (raw_n > 0) {
    raw = malloc((size_t)raw_n);
    if (raw) {
      fread(raw, 1, (size_t)raw_n, f);
    }
  }
  fclose(f);
  if (!raw || raw_n < 0) {
    free(raw);
    free(enc);
    col1_save_free(&save);
    return;
  }
  if ((size_t)raw_n != enc_n) {
    fprintf(
      stderr,
      "codec diff %s size mismatch raw=%ld enc=%zu\n",
      path,
      raw_n,
      enc_n
    );
  } else {
    size_t i = 0;
    int nranges = 0;
    while (i < enc_n) {
      if (raw[i] == enc[i]) {
        ++i;
        continue;
      }
      const size_t start = i;
      while (i < enc_n && raw[i] != enc[i]) {
        ++i;
      }
      ++nranges;
      if (nranges <= 40) {
        fprintf(
          stderr,
          "codec diff %s range=[%zu,%zu) len=%zu raw[0]=%02x enc[0]=%02x\n",
          path,
          start,
          i,
          i - start,
          raw[start],
          enc[start]
        );
      }
    }
    if (nranges > 40) {
      fprintf(stderr, "codec diff %s ... %d more diff ranges not shown\n", path, nranges - 40);
    }
  }
  free(raw);
  free(enc);
  col1_save_free(&save);
}

/* DOS UNITFLAG/COLONYFLAG structural checks (mask ↔ pools). */
static bool assert_mask_occupancy_consistent(const ColonizeCol1Save* save, const char* label) {
  if (!save || !save->map.mask) {
    fprintf(stderr, "%s: no mask\n", label);
    return false;
  }
  const int w = (int)save->head.map_size_x;
  const int h = (int)save->head.map_size_y;
  if (w <= 0 || h <= 0) {
    fprintf(stderr, "%s: bad map size\n", label);
    return false;
  }

  int* unit_count = calloc((size_t)w * (size_t)h, sizeof(int));
  int* city_count = calloc((size_t)w * (size_t)h, sizeof(int));
  if (!unit_count || !city_count) {
    free(unit_count);
    free(city_count);
    fprintf(stderr, "%s: oom occupancy grids\n", label);
    return false;
  }

  for (uint16_t i = 0; i < save->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &save->unit[i];
    if (u->x >= 200 || u->y >= 200 || (int)u->x >= w || (int)u->y >= h) {
      continue;
    }
    /* Passengers share ship coords; has_unit is per-tile presence, so count is fine. */
    unit_count[(size_t)u->y * (size_t)w + (size_t)u->x]++;
  }
  for (uint16_t i = 0; i < save->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &save->colony[i];
    if ((int)c->x >= w || (int)c->y >= h) {
      continue;
    }
    city_count[(size_t)c->y * (size_t)w + (size_t)c->x]++;
  }
  for (uint16_t i = 0; i < save->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &save->tribe[i];
    if ((int)t->x >= w || (int)t->y >= h) {
      continue;
    }
    city_count[(size_t)t->y * (size_t)w + (size_t)t->x]++;
  }

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t idx = (size_t)y * (size_t)w + (size_t)x;
      const uint8_t m = save->map.mask[idx];
      const int has_unit = (m & MAP_OCCUPANCY_HAS_UNIT) != 0;
      const int has_city = (m & MAP_OCCUPANCY_HAS_CITY) != 0;
      if (has_unit && unit_count[idx] == 0) {
        fprintf(stderr, "%s: stray has_unit at (%d,%d)\n", label, x, y);
        free(unit_count);
        free(city_count);
        return false;
      }
      if (!has_unit && unit_count[idx] > 0) {
        fprintf(stderr, "%s: missing has_unit at (%d,%d)\n", label, x, y);
        free(unit_count);
        free(city_count);
        return false;
      }
      if (has_city && city_count[idx] == 0) {
        fprintf(stderr, "%s: stray has_city at (%d,%d)\n", label, x, y);
        free(unit_count);
        free(city_count);
        return false;
      }
      if (!has_city && city_count[idx] > 0) {
        fprintf(stderr, "%s: missing has_city at (%d,%d)\n", label, x, y);
        free(unit_count);
        free(city_count);
        return false;
      }
    }
  }
  free(unit_count);
  free(city_count);
  return true;
}

/*
 * Cross-check decomp-backed field meanings against a loaded save.
 * colony_counts must match live colonies; census unit totals may lag (stale
 * for withdrawn AI) so we only bound them. Occupancy is checked separately.
 */
static bool assert_mapped_fields_consistent(const ColonizeCol1Save* save, const char* label) {
  if (!save) {
    fprintf(stderr, "%s: null save\n", label);
    return false;
  }
  if (save->head.save_version != COLONIZE_COL1_SAVE_VERSION) {
    fprintf(
      stderr,
      "%s: unexpected save_version %u\n",
      label,
      (unsigned)save->head.save_version
    );
    return false;
  }
  if (save->head.map_size_x != COLONIZE_COL1_MAP_W_STD ||
      save->head.map_size_y != COLONIZE_COL1_MAP_H_STD) {
    fprintf(
      stderr,
      "%s: unexpected map size %ux%u\n",
      label,
      save->head.map_size_x,
      save->head.map_size_y
    );
    return false;
  }
  if (save->head.difficulty > 4) {
    fprintf(stderr, "%s: difficulty %u out of range\n", label, (unsigned)save->head.difficulty);
    return false;
  }
  if (save->head.human_player >= COLONIZE_COL1_NATION_COUNT) {
    fprintf(stderr, "%s: human_player %u\n", label, (unsigned)save->head.human_player);
    return false;
  }
  if (save->head.show_entire_map > 1) {
    fprintf(stderr, "%s: show_entire_map %u\n", label, (unsigned)save->head.show_entire_map);
    return false;
  }

  unsigned colony_by_nation[COLONIZE_COL1_NATION_COUNT];
  memset(colony_by_nation, 0, sizeof(colony_by_nation));
  for (uint16_t i = 0; i < save->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &save->colony[i];
    if (c->nation_id >= COLONIZE_COL1_NATION_COUNT) {
      fprintf(stderr, "%s: colony[%u] nation %u\n", label, (unsigned)i, (unsigned)c->nation_id);
      return false;
    }
    colony_by_nation[c->nation_id]++;
    if (c->population > COLONIZE_COL1_COLONY_POP_MAX) {
      fprintf(stderr, "%s: colony[%u] pop %u\n", label, (unsigned)i, (unsigned)c->population);
      return false;
    }
    if (c->warehouse_level > 2) {
      fprintf(
        stderr,
        "%s: colony[%u] warehouse_level %u\n",
        label,
        (unsigned)i,
        (unsigned)c->warehouse_level
      );
      return false;
    }
    if (c->capitol_level > 2) {
      fprintf(
        stderr,
        "%s: colony[%u] capitol_level %u\n",
        label,
        (unsigned)i,
        (unsigned)c->capitol_level
      );
      return false;
    }
    if (c->depletion_counter > 50) {
      fprintf(
        stderr,
        "%s: colony[%u] depletion_counter %u\n",
        label,
        (unsigned)i,
        (unsigned)c->depletion_counter
      );
      return false;
    }
    if (c->improve_timer > 0x7f || c->cargo_idle_turns > 0x7f) {
      fprintf(
        stderr,
        "%s: colony[%u] timer improve=%u cargo_idle=%u\n",
        label,
        (unsigned)i,
        (unsigned)c->improve_timer,
        (unsigned)c->cargo_idle_turns
      );
      return false;
    }
    /* Extended tile slots [8..19] are empty (0xff) in observed DOS saves. */
    for (unsigned t = COLONIZE_COL1_COLONY_TILE_RING; t < COLONIZE_COL1_COLONY_TILES; ++t) {
      if ((uint8_t)c->tiles[t] != 0xff) {
        fprintf(
          stderr,
          "%s: colony[%u] tiles[%u]=%d (expected 0xff)\n",
          label,
          (unsigned)i,
          t,
          (int)c->tiles[t]
        );
        return false;
      }
    }
  }
  for (unsigned n = 0; n < COLONIZE_COL1_NATION_COUNT; ++n) {
    if (save->stuff.colony_counts[n] != colony_by_nation[n]) {
      fprintf(
        stderr,
        "%s: colony_counts[%u]=%u live=%u\n",
        label,
        n,
        (unsigned)save->stuff.colony_counts[n],
        colony_by_nation[n]
      );
      return false;
    }
  }

  if (save->head.map_mode > 1) {
    fprintf(stderr, "%s: map_mode %u\n", label, (unsigned)save->head.map_mode);
    return false;
  }
  if (save->stuff.zoom_level > 3) {
    fprintf(stderr, "%s: zoom_level %u\n", label, (unsigned)save->stuff.zoom_level);
    return false;
  }

  for (uint16_t ti = 0; ti < COLONIZE_COL1_INDIAN_COUNT; ++ti) {
    for (unsigned e = 0; e < COLONIZE_COL1_NATION_COUNT; ++e) {
      const int16_t st = save->indian[ti].contact_state[e];
      if (st < 0 || st > 2) {
        fprintf(
          stderr,
          "%s: indian[%u].contact_state[%u]=%d\n",
          label,
          (unsigned)ti,
          e,
          (int)st
        );
        return false;
      }
      const int accum = (int)save->indian[ti].euro_relation_accum[e];
      /* Soft bound: spill normalizes around ±7; allow mid-spill saves. */
      if (accum < -15 || accum > 15) {
        fprintf(
          stderr,
          "%s: indian[%u].euro_relation_accum[%u]=%d\n",
          label,
          (unsigned)ti,
          e,
          accum
        );
        return false;
      }
    }
  }

  unsigned euro_units[COLONIZE_COL1_NATION_COUNT];
  memset(euro_units, 0, sizeof(euro_units));
  for (uint16_t i = 0; i < save->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &save->unit[i];
    if (u->nation_id < COLONIZE_COL1_NATION_COUNT) {
      euro_units[u->nation_id]++;
    }
    /* ai_plan is free-form ASCII; starters use 'X'. Allow any byte. */
    (void)u->ai_plan;
    (void)u->vis_mask;
  }
  for (unsigned n = 0; n < COLONIZE_COL1_NATION_COUNT; ++n) {
    /* all_unit_counts tracks euro units but may lag live (DOS census lag — preserve). */
    if (save->stuff.all_unit_counts[n] > euro_units[n] + 8u) {
      fprintf(
        stderr,
        "%s: all_unit_counts[%u]=%u far above live=%u\n",
        label,
        n,
        (unsigned)save->stuff.all_unit_counts[n],
        euro_units[n]
      );
      return false;
    }
    const ColonizeCol1Nation* nat = &save->nation[n];
    if (nat->tax_rate > 100) {
      fprintf(stderr, "%s: nation[%u] tax %u\n", label, n, (unsigned)nat->tax_rate);
      return false;
    }
    if (nat->rebel_sentiment > 100) {
      fprintf(
        stderr,
        "%s: nation[%u] rebel_sentiment %u\n",
        label,
        n,
        (unsigned)nat->rebel_sentiment
      );
      return false;
    }
    if (save->player[n].control > 2) {
      fprintf(
        stderr,
        "%s: player[%u] control %u\n",
        label,
        n,
        (unsigned)save->player[n].control
      );
      return false;
    }
  }

  if (save->head.rebel_sentiment_report < 0 || save->head.rebel_sentiment_report > 100) {
    fprintf(
      stderr,
      "%s: rebel_sentiment_report %d\n",
      label,
      (int)save->head.rebel_sentiment_report
    );
    return false;
  }

  /* Lategame / mapgen saves should have connectivity planes; starters too. */
  {
    int sea_nz = 0;
    int land_nz = 0;
    for (size_t i = 0; i < COLONIZE_COL1_CONNECT_PLANE_SIZE; ++i) {
      if (save->post_map.sea_connectivity[i]) {
        sea_nz++;
      }
      if (save->post_map.land_connectivity[i]) {
        land_nz++;
      }
    }
    if (sea_nz == 0 || land_nz == 0) {
      fprintf(stderr, "%s: blank post_map connectivity sea_nz=%d land_nz=%d\n", label, sea_nz, land_nz);
      return false;
    }
  }

  if (save->post_map.prime_resource_seed == 0) {
    fprintf(stderr, "%s: prime_resource_seed is zero\n", label);
    return false;
  }

  return true;
}

static bool build_synthetic(ColonizeCol1Save* save, char* err, size_t err_size) {
  col1_save_init(save);
  memset(&save->head, 0, sizeof(save->head));
  col1_save_stamp_head(&save->head);
  save->head.map_size_x = COLONIZE_COL1_MAP_W_STD;
  save->head.map_size_y = COLONIZE_COL1_MAP_H_STD;
  save->head.year = 1492;
  save->head.turn = 1;
  save->head.colony_count = 2;
  save->head.unit_count = 3;
  save->head.tribe_count = 4;
  save->head.difficulty = 2;

  if (!col1_save_alloc_sections(save, err, err_size)) {
    return false;
  }

  fill_pattern((uint8_t*)save->player, sizeof(save->player), 0x11);
  fill_pattern(save->other, sizeof(save->other), 0x22);
  fill_pattern((uint8_t*)save->colony, save->head.colony_count * sizeof(ColonizeCol1Colony), 0x33);
  fill_pattern((uint8_t*)save->unit, save->head.unit_count * sizeof(ColonizeCol1Unit), 0x44);
  fill_pattern((uint8_t*)save->nation, sizeof(save->nation), 0x55);
  fill_pattern((uint8_t*)save->tribe, save->head.tribe_count * sizeof(ColonizeCol1Tribe), 0x66);
  fill_pattern((uint8_t*)save->indian, sizeof(save->indian), 0x77);
  fill_pattern((uint8_t*)&save->stuff, sizeof(save->stuff), 0x88);
  fill_pattern(save->map.tile, save->map.tile_count, 0x91);
  fill_pattern(save->map.mask, save->map.tile_count, 0xa2);
  fill_pattern(save->map.path, save->map.tile_count, 0xb3);
  fill_pattern(save->map.seen, save->map.tile_count, 0xc4);
  fill_pattern((uint8_t*)&save->post_map, sizeof(save->post_map), 0xd5);
  fill_pattern((uint8_t*)save->trade_route, sizeof(save->trade_route), 0xf7);

  /* Keep signature / version intact after pattern fills. */
  col1_save_stamp_head(&save->head);
  save->head.map_size_x = COLONIZE_COL1_MAP_W_STD;
  save->head.map_size_y = COLONIZE_COL1_MAP_H_STD;
  save->head.colony_count = 2;
  save->head.unit_count = 3;
  save->head.tribe_count = 4;

  save->colony[0].x = 10;
  save->colony[0].y = 20;
  snprintf(save->colony[0].name, sizeof(save->colony[0].name), "Jamestown");
  save->colony[0].population = 3;
  save->colony[0].stock[0] = 200;

  save->unit[0].x = 11;
  save->unit[0].y = 21;
  save->unit[0].type = 0; /* colonist */
  save->unit[0].nation_id = 0;

  return true;
}

int main(void) {
  diag_init(0, NULL);

  char err[256];
  if (!col1_save_check_layout(err, sizeof(err))) {
    fprintf(stderr, "layout check failed: %s\n", err);
    return 1;
  }
  fprintf(stderr, "col1 layout sizes ok\n");

  ColonizeCol1Save save;
  if (!build_synthetic(&save, err, sizeof(err))) {
    fprintf(stderr, "build_synthetic failed: %s\n", err);
    return 1;
  }

  const size_t expect = col1_save_expected_size(&save);
  fprintf(
    stderr,
    "synthetic size=%zu (colonies=%u units=%u tribes=%u)\n",
    expect,
    save.head.colony_count,
    save.head.unit_count,
    save.head.tribe_count
  );

  uint8_t* blob = NULL;
  size_t blob_size = 0;
  if (!col1_save_write_memory(&save, &blob, &blob_size, err, sizeof(err))) {
    fprintf(stderr, "write_memory failed: %s\n", err);
    col1_save_free(&save);
    return 1;
  }
  if (blob_size != expect) {
    fprintf(stderr, "blob size %zu != expect %zu\n", blob_size, expect);
    free(blob);
    col1_save_free(&save);
    return 1;
  }

  /* Round-trip via file */
  const char* path = "./test-saves-col1/COLONY00.SAV";
  {
    /* ensure dir */
    system("mkdir -p ./test-saves-col1");
  }
  if (!col1_save_write_file(path, &save, err, sizeof(err))) {
    fprintf(stderr, "write_file failed: %s\n", err);
    free(blob);
    col1_save_free(&save);
    return 1;
  }

  ColonizeCol1Save loaded;
  col1_save_init(&loaded);
  if (!col1_save_read_file(path, &loaded, err, sizeof(err))) {
    fprintf(stderr, "read_file failed: %s\n", err);
    free(blob);
    col1_save_free(&save);
    return 1;
  }

  uint8_t* blob2 = NULL;
  size_t blob2_size = 0;
  if (!col1_save_write_memory(&loaded, &blob2, &blob2_size, err, sizeof(err))) {
    fprintf(stderr, "re-encode failed: %s\n", err);
    free(blob);
    col1_save_free(&save);
    col1_save_free(&loaded);
    return 1;
  }

  if (blob2_size != blob_size || memcmp(blob, blob2, blob_size) != 0) {
    fprintf(stderr, "byte-identical round-trip FAILED (%zu vs %zu)\n", blob_size, blob2_size);
    if (blob2_size == blob_size) {
      for (size_t i = 0; i < blob_size; ++i) {
        if (blob[i] != blob2[i]) {
          fprintf(stderr, "first diff at offset %zu: %02x vs %02x\n", i, blob[i], blob2[i]);
          break;
        }
      }
    }
    free(blob);
    free(blob2);
    col1_save_free(&save);
    col1_save_free(&loaded);
    return 1;
  }

  if (strcmp(loaded.colony[0].name, "Jamestown") != 0 || loaded.colony[0].stock[0] != 200) {
    fprintf(stderr, "colony fields not preserved\n");
    free(blob);
    free(blob2);
    col1_save_free(&save);
    col1_save_free(&loaded);
    return 1;
  }

  /* Memory path round-trip */
  ColonizeCol1Save mem;
  col1_save_init(&mem);
  if (!col1_save_read_memory(blob, blob_size, &mem, err, sizeof(err))) {
    fprintf(stderr, "read_memory failed: %s\n", err);
    free(blob);
    free(blob2);
    col1_save_free(&save);
    col1_save_free(&loaded);
    return 1;
  }

  fprintf(stderr, "col1 save round-trip ok (%zu bytes)\n", blob_size);
  free(blob);
  free(blob2);
  col1_save_free(&save);
  col1_save_free(&loaded);
  col1_save_free(&mem);

  /* Fixture Col1 saves: byte-identical round-trip (+ bridge apply for samples). */
  typedef struct {
    const char* path;
    bool expect_starter_gold; /* early COLONY00/01 gold==1000 */
    bool validate_mapping; /* mapped-field + occupancy checks */
    bool byte_identical; /* read→encode→memcmp; false = read-only smoke */
  } Col1Fixture;
  static const Col1Fixture k_fixtures[] = {
    {"original_saves/COLONY00.SAV", true, true, true},
    {"original_saves/COLONY01.SAV", true, true, true},
    /* Lategame + TURN byte_identical promoted true 2026-08-24 (W1.5):
     * confirmed via full-range diff (not just first-byte) that every
     * fixture below round-trips byte-for-byte on this HEAD. The drift
     * once documented here was fixed same-day by 753662d "Fix FF + I
     * work" (2026-08-22), which stashes/restores nation.unknown21_pad
     * (FF_POOL_STASH_MARKER) alongside liberty_bells_last_turn on
     * save write; docs/savegame.md + docs/save_format_map.md just
     * never got the "still drifting" language removed. See
     * docs/save_format_map.md and docs/port_plan.md W1.5. */
    {"original_saves/valid-lategame-saves/COLONY00.SAV", false, true, true},
    {"original_saves/valid-lategame-saves/COLONY01.SAV", false, true, true},
    {"original_saves/valid-lategame-saves/COLONY02.SAV", false, true, true},
    {"original_saves/valid-lategame-saves/COLONY03.SAV", false, true, true},
    {"original_saves/valid-lategame-saves/COLONY04.SAV", false, true, true},
    {"original_saves/valid-lategame-saves/COLONY05.SAV", false, true, true},
    {"original_saves/valid-lategame-saves/COLONY06.SAV", false, true, true},
    {"original_saves/valid-lategame-saves/COLONY07.SAV", false, true, true},
    {"original_saves/valid-lategame-saves/COLONY08.SAV", false, true, true},
    {"original_saves/valid-lategame-saves/COLONY10.SAV", false, true, true},
    /* Dutch + French campaign sets (added 2026-09-03): original DOS saves.
     * dutch-campaign COLONY00-08/10 are byte-identical duplicates of the
     * valid-lategame-saves fixtures above, so only the one new file (the
     * early-game COLONY09) is listed here. */
    {"original_saves/dutch-campaign/COLONY09.SAV", false, true, true},
    {"original_saves/french-campaign/COLONY00.SAV", false, true, true},
    {"original_saves/french-campaign/COLONY01.SAV", false, true, true},
    {"original_saves/french-campaign/COLONY02.SAV", false, true, true},
    {"original_saves/french-campaign/COLONY03.SAV", false, true, true},
    {"original_saves/french-campaign/COLONY04.SAV", false, true, true},
    {"original_saves/french-campaign/COLONY08.SAV", false, true, true},
    {"original_saves/french-campaign/COLONY09.SAV", false, true, true},
    {"test-saves-ai/TURN1.SAV", false, false, true},
    {"test-saves-ai/TURN2.SAV", false, false, true},
    {"test-saves-ai/TURN3.SAV", false, false, true},
    {"test-saves-ai/TURN4.SAV", false, false, true},
    {"test-saves-ai/TURN5.SAV", false, false, true},
    {"test-saves-ai/TURN6.SAV", false, false, true},
    {"test-saves-ai/TURN7.SAV", false, false, true},
    /* Pre-fix Dutch port save with stale head human/nation_turn/view = 0;
     * apply must derive human=3 from control and recapture must stamp 3/3/3
     * (bugs.md fog_and_popup: DOS fogged whole map + instant @PRICEUP). */
    {"port_saves/campaign2/fog_and_popup.SAV", false, false, true},
  };
  for (size_t oi = 0; oi < sizeof(k_fixtures) / sizeof(k_fixtures[0]); ++oi) {
    const Col1Fixture* fix = &k_fixtures[oi];
    /* Each fixture is an independent file: don't let founding_fathers'
     * global bells-since-elect pool (set by the previous fixture's
     * col1_bridge_apply below) leak into this fixture's byte-identical
     * codec check or bridge apply. */
    founding_fathers_reset();
    if (!fix->byte_identical) {
      report_codec_roundtrip_diff(fix->path);
    }
    ColonizeCol1Save orig;
    if (fix->byte_identical) {
      if (!assert_byte_identical_roundtrip(fix->path, &orig, err, sizeof(err))) {
        return 1;
      }
    } else {
      col1_save_init(&orig);
      if (!col1_save_read_file(fix->path, &orig, err, sizeof(err))) {
        fprintf(stderr, "fixture read failed %s: %s\n", fix->path, err);
        return 1;
      }
    }

    if (fix->validate_mapping) {
      if (!assert_mask_occupancy_consistent(&orig, fix->path)) {
        col1_save_free(&orig);
        return 1;
      }
      if (!assert_mapped_fields_consistent(&orig, fix->path)) {
        col1_save_free(&orig);
        return 1;
      }
    }

    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 23;
    for (int t = 0; t < units.type_count; ++t) {
      snprintf(units.types[t].name, sizeof(units.types[t].name), "T%d", t);
      units.types[t].movement = 1;
      units.types[t].domain = (t >= 13 && t <= 18) ? COLONIZE_UNIT_DOMAIN_SEA
                                                   : COLONIZE_UNIT_DOMAIN_LAND;
      units.types[t].cargo = (t >= 13 && t <= 18) ? 6 : 0;
    }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    EuropeScreen europe;
    memset(&europe, 0, sizeof(europe));
    europe.cargo_count = 16;
    ColonizeCol1BridgeResult br;
    if (!col1_bridge_apply(&orig, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
      fprintf(stderr, "bridge apply failed %s: %s\n", fix->path, err);
      col1_save_free(&orig);
      return 1;
    }
    if (fix->expect_starter_gold && (br.imported_units < 3 || europe.gold != 1000)) {
      fprintf(
        stderr,
        "bridge apply unexpected %s units=%d gold=%d\n",
        fix->path,
        br.imported_units,
        europe.gold
      );
      map_free(&map);
      col1_save_free(&orig);
      return 1;
    }
    /* Terrain conversion identity on this map. */
    for (size_t ti = 0; ti < orig.map.tile_count; ++ti) {
      const uint8_t a = orig.map.tile[ti];
      const uint8_t b = col1_mp_terrain_to_tile(col1_tile_to_mp_terrain(a));
      if (a != b) {
        fprintf(stderr, "terrain convert drift at %zu: %02x -> %02x\n", ti, a, b);
        map_free(&map);
        col1_save_free(&orig);
        return 1;
      }
    }
    /*
     * P10.1 port-written save net: apply → capture must not lose records.
     * Colony/unit counts must survive (32-colony cap truncated 33-colony
     * lategame saves; human Europe harbor/Expected/Bound ships lived only in
     * the EuropeScreen and vanished from the unit list), and a second apply
     * of the captured save must see the same Europe ship lanes.
     */
    {
      ColonizeCol1Save cap;
      col1_save_init(&cap);
      if (!col1_save_read_file(fix->path, &cap, err, sizeof(err))) {
        fprintf(stderr, "recapture reread failed %s\n", fix->path);
        return 1;
      }
      if (!col1_bridge_capture(&cap, &map, &units, &colonies, &europe, br.year, br.autumn,
                               br.turn_number, br.human_nation, br.cursor_x, br.cursor_y,
                               units.selected_id, err, sizeof(err))) {
        fprintf(stderr, "recapture failed %s: %s\n", fix->path, err);
        return 1;
      }
      if (cap.head.colony_count != orig.head.colony_count ||
          cap.head.unit_count != orig.head.unit_count) {
        fprintf(
          stderr,
          "recapture lost records %s: colonies %u->%u units %u->%u\n",
          fix->path,
          orig.head.colony_count,
          cap.head.colony_count,
          orig.head.unit_count,
          cap.head.unit_count
        );
        return 1;
      }
      /* DOS in-game saves carry DS:0x5394/0x5396/0x5398 all == human slot
       * (Dutch goldens: 3/3/3); a stale 0 makes DOS run England's EOT as
       * "human" at load (immediate @PRICEUP) and draw fog for viewer 0
       * (bugs.md fog_and_popup.SAV). */
      if (cap.head.human_player != (uint16_t)br.human_nation ||
          cap.head.nation_turn != (uint16_t)br.human_nation ||
          cap.head.curr_nation_map_view != (uint16_t)br.human_nation) {
        fprintf(
          stderr,
          "recapture human-slot heads %s: human %u nation_turn %u view %u expected %d\n",
          fix->path,
          (unsigned)cap.head.human_player,
          (unsigned)cap.head.nation_turn,
          (unsigned)cap.head.curr_nation_map_view,
          br.human_nation
        );
        return 1;
      }
      ColonizeWorldMap map2;
      memset(&map2, 0, sizeof(map2));
      ColonizeUnitPool units2;
      units_reset(&units2);
      units2.type_count = units.type_count;
      memcpy(units2.types, units.types, sizeof(units.types));
      ColonizeColonyPool colonies2;
      colonies_init(&colonies2);
      EuropeScreen europe2;
      memset(&europe2, 0, sizeof(europe2));
      europe2.cargo_count = 16;
      ColonizeCol1BridgeResult br2;
      founding_fathers_reset();
      if (!col1_bridge_apply(&cap, &map2, &units2, &colonies2, &europe2, &br2, err, sizeof(err))) {
        fprintf(stderr, "recapture re-apply failed %s: %s\n", fix->path, err);
        return 1;
      }
      if (europe2.harbor_ships != europe.harbor_ships ||
          europe2.expected_ships != europe.expected_ships ||
          europe2.bound_ships != europe.bound_ships) {
        fprintf(
          stderr,
          "recapture Europe lanes drift %s: harbor %d->%d expected %d->%d bound %d->%d\n",
          fix->path,
          europe.harbor_ships,
          europe2.harbor_ships,
          europe.expected_ships,
          europe2.expected_ships,
          europe.bound_ships,
          europe2.bound_ships
        );
        return 1;
      }
      /* COLONY04: human (3) Merchantman at 247 = sailing to Europe, 1 turn left. */
      if (strstr(fix->path, "lategame-saves/COLONY04.SAV") &&
          (europe.expected_ships != 1 || europe.expected[0].turns_left != 1 ||
           europe.expected[0].exit_x != 55 || europe.expected[0].exit_y != 48)) {
        fprintf(stderr, "COLONY04 expected lane wrong: n=%d turns=%d exit=(%d,%d)\n",
                europe.expected_ships, europe.expected[0].turns_left,
                europe.expected[0].exit_x, europe.expected[0].exit_y);
        return 1;
      }
      /* COLONY06: human (3) fleet in port at 231 (Galleon + 2 passengers on chain). */
      if (strstr(fix->path, "lategame-saves/COLONY06.SAV")) {
        if (europe.harbor_ships < 2) {
          fprintf(stderr, "COLONY06 harbor lane wrong: n=%d\n", europe.harbor_ships);
          return 1;
        }
        /* Bound lane (232+n): no fixture carries one — sail a harbor ship
         * out, capture, re-apply, expect it back in Bound with its voyage. */
        if (!europe_set_sail_from_harbor(&europe, 0, 2, &units, br.human_nation)) {
          fprintf(stderr, "COLONY06 set sail failed\n");
          return 1;
        }
        if (!col1_bridge_capture(&cap, &map, &units, &colonies, &europe, br.year, br.autumn,
                                 br.turn_number, br.human_nation, br.cursor_x, br.cursor_y,
                                 units.selected_id, err, sizeof(err))) {
          fprintf(stderr, "COLONY06 bound capture failed: %s\n", err);
          return 1;
        }
        ColonizeWorldMap map3;
        memset(&map3, 0, sizeof(map3));
        ColonizeUnitPool units3;
        units_reset(&units3);
        units3.type_count = units.type_count;
        memcpy(units3.types, units.types, sizeof(units.types));
        ColonizeColonyPool colonies3;
        colonies_init(&colonies3);
        EuropeScreen europe3;
        memset(&europe3, 0, sizeof(europe3));
        europe3.cargo_count = 16;
        ColonizeCol1BridgeResult br3;
        founding_fathers_reset();
        if (!col1_bridge_apply(&cap, &map3, &units3, &colonies3, &europe3, &br3, err, sizeof(err))) {
          fprintf(stderr, "COLONY06 bound re-apply failed: %s\n", err);
          return 1;
        }
        if (europe3.bound_ships != 1 || europe3.bound[0].turns_left != 2 ||
            europe3.harbor_ships != europe.harbor_ships) {
          fprintf(stderr, "COLONY06 bound lane wrong: bound=%d turns=%d harbor=%d/%d\n",
                  europe3.bound_ships, europe3.bound[0].turns_left, europe3.harbor_ships,
                  europe.harbor_ships);
          return 1;
        }
        map_free(&map3);
      }
      map_free(&map2);
      col1_save_free(&cap);
    }
    fprintf(
      stderr,
      "fixture %s ok (units=%d year=%u gold=%d colonies=%u)\n",
      fix->path,
      br.imported_units,
      br.year,
      europe.gold,
      (unsigned)orig.head.colony_count
    );
    map_free(&map);
    col1_save_free(&orig);
  }

  /* Capture/apply must preserve colony buildings (starters + upgrades). */
  {
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
      fprintf(stderr, "building roundtrip: NAMES.TXT load failed\n");
      return 1;
    }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    if (!colonies_load_buildings(&colonies, &names) || !colonies_load_names(&colonies, "COLONIZE/COLONY.TXT")) {
      fprintf(stderr, "building roundtrip: buildings/names failed\n");
      assets_msg_free(&names);
      return 1;
    }
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
      fprintf(stderr, "building roundtrip: map load: %s\n", err);
      assets_msg_free(&names);
      return 1;
    }
    int fx = -1, fy = -1;
    for (int y = 1; y < (int)map.height - 1 && fx < 0; ++y) {
      for (int x = 1; x < (int)map.width - 1 && fx < 0; ++x) {
        if (map_tile_is_land(&map, x, y) && colonies_can_found(&colonies, &map, x, y)) {
          fx = x;
          fy = y;
        }
      }
    }
    if (fx < 0) {
      fprintf(stderr, "building roundtrip: no founding site\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const int cid = colonies_found(&colonies, &map, fx, fy, 0, 0, -1, 0, 0, 0);
    ColonizeColony* col = colonies_get_mut(&colonies, cid);
    const int stockade = colonies_find_building(&colonies, "Stockade");
    const int warehouse = colonies_find_building(&colonies, "Warehouse");
    const int carpenter = colonies_find_building(&colonies, "Carpenter's Shop");
    const int town_hall = colonies_find_building(&colonies, "Town Hall");
    if (!col || stockade < 0 || warehouse < 0 || carpenter < 0 || town_hall < 0) {
      fprintf(stderr, "building roundtrip: setup failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    col->has_building[stockade] = true;
    col->has_building[warehouse] = true;
    /* Found with a Veteran Soldier skill working Town Hall. */
    if (col->colonist_count < 1) {
      col->colonists[0].active = true;
      col->colonists[0].building_type = town_hall;
      col->colonists[0].field_job = -1;
      col->colonist_count = 1;
      col->population = 1;
    }
    col->colonists[0].profession = UNITS_JOB_SOLDIER;
    col->colonists[0].building_type = town_hall;
    col->colonists[0].field_job = -1;
    /* Opaque outer tile slots 8..19 must survive capture/apply byte-exact. */
    for (int i = 0; i < 12; ++i) {
      col->col1_outer_tiles[i] = (int8_t)(0x20 + i);
    }

    ColonizeCol1Save save;
    if (!col1_bridge_init_template(&save, map.width, map.height, err, sizeof(err))) {
      fprintf(stderr, "building roundtrip: template: %s\n", err);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnitPool units;
    units_reset(&units);
    if (!units_load_types(&units, &names)) {
      fprintf(stderr, "building roundtrip: unit types failed\n");
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    EuropeScreen europe;
    memset(&europe, 0, sizeof(europe));
    europe.cargo_count = 16;
    if (!col1_bridge_capture(
          &save, &map, &units, &colonies, &europe, 1492, 0, 1, 0, fx, fy, -1, err, sizeof(err)
        )) {
      fprintf(stderr, "building roundtrip: capture: %s\n", err);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (save.head.colony_count < 1 ||
        save.colony[0].buildings.town_hall == 0 ||
        save.colony[0].buildings.carpenters_shop == 0 ||
        save.colony[0].buildings.fortification == 0 ||
        save.colony[0].buildings.warehouse == 0 ||
        save.colony[0].profession[0] != (uint8_t)UNITS_JOB_SOLDIER) {
      fprintf(
        stderr,
        "building roundtrip: encode missing hall=%u carpenter=%u fort=%u wh=%u skill=%u\n",
        (unsigned)save.colony[0].buildings.town_hall,
        (unsigned)save.colony[0].buildings.carpenters_shop,
        (unsigned)save.colony[0].buildings.fortification,
        (unsigned)save.colony[0].buildings.warehouse,
        (unsigned)save.colony[0].profession[0]
      );
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    for (int i = 0; i < 12; ++i) {
      if ((uint8_t)save.colony[0].tiles[8 + i] != (uint8_t)(0x20 + i)) {
        fprintf(stderr, "outer tiles: capture lost slot %d (%02x)\n", 8 + i,
                (unsigned)(uint8_t)save.colony[0].tiles[8 + i]);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
    {
      ColonizeColonyPool pool2;
      colonies_init(&pool2);
      colonies_load_buildings(&pool2, &names);
      colonies_load_names(&pool2, "COLONIZE/COLONY.TXT");
      ColonizeUnitPool units2;
      units_reset(&units2);
      units_load_types(&units2, &names);
      EuropeScreen europe2;
      memset(&europe2, 0, sizeof(europe2));
      europe2.cargo_count = 16;
      ColonizeCol1BridgeResult br2;
      if (!col1_bridge_apply(&save, &map, &units2, &pool2, &europe2, &br2, err, sizeof(err))) {
        fprintf(stderr, "outer tiles: apply: %s\n", err);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      for (int i = 0; i < 12; ++i) {
        if ((uint8_t)pool2.colonies[0].col1_outer_tiles[i] != (uint8_t)(0x20 + i)) {
          fprintf(stderr, "outer tiles: apply lost slot %d\n", 8 + i);
          col1_save_free(&save);
          map_free(&map);
          assets_msg_free(&names);
          return 1;
        }
      }
    }

    ColonizeColonyPool loaded;
    colonies_init(&loaded);
    if (!colonies_load_buildings(&loaded, &names)) {
      fprintf(stderr, "building roundtrip: reload buildings failed\n");
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeWorldMap map2;
    memset(&map2, 0, sizeof(map2));
    ColonizeUnitPool units2;
    units_reset(&units2);
    if (!units_load_types(&units2, &names)) {
      fprintf(stderr, "building roundtrip: reload unit types failed\n");
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    EuropeScreen europe2;
    memset(&europe2, 0, sizeof(europe2));
    europe2.cargo_count = 16;
    ColonizeCol1BridgeResult br;
    if (!col1_bridge_apply(&save, &map2, &units2, &loaded, &europe2, &br, err, sizeof(err))) {
      fprintf(stderr, "building roundtrip: apply: %s\n", err);
      col1_save_free(&save);
      map_free(&map);
      map_free(&map2);
      assets_msg_free(&names);
      return 1;
    }
    const ColonizeColony* got = NULL;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      if (loaded.colonies[i].active) {
        got = &loaded.colonies[i];
        break;
      }
    }
    const int l_hall = colonies_find_building(&loaded, "Town Hall");
    const int l_carpenter = colonies_find_building(&loaded, "Carpenter's Shop");
    const int l_stockade = colonies_find_building(&loaded, "Stockade");
    const int l_warehouse = colonies_find_building(&loaded, "Warehouse");
    if (!got || l_hall < 0 || !got->has_building[l_hall] || !got->has_building[l_carpenter] ||
        !got->has_building[l_stockade] || !got->has_building[l_warehouse] ||
        got->colonist_count < 1 || got->colonists[0].profession != UNITS_JOB_SOLDIER) {
      fprintf(
        stderr,
        "building roundtrip: apply wiped buildings/skills (skill=%d)\n",
        got && got->colonist_count > 0 ? got->colonists[0].profession : -1
      );
      col1_save_free(&save);
      map_free(&map);
      map_free(&map2);
      assets_msg_free(&names);
      return 1;
    }
    fprintf(stderr, "building capture/apply roundtrip ok\n");
    col1_save_free(&save);
    map_free(&map);
    map_free(&map2);
    assets_msg_free(&names);
  }

  fprintf(stderr, "col1 fixture saves + bridge ok\n");

  /*
   * Linux→DOS structural interop: occupancy + ai_plan after capture.
   * Codec byte-identical round-trips above do NOT prove this.
   */
  {
    static const char* k_unit_flags_path = "tests-save-misc/unit flags error.sav";
    ColonizeCol1Save broken;
    col1_save_init(&broken);
    if (!col1_save_read_file(k_unit_flags_path, &broken, err, sizeof(err))) {
      fprintf(stderr, "unit-flags fixture read failed: %s\n", err);
      return 1;
    }
    if (assert_mask_occupancy_consistent(&broken, "pre-fix broken fixture")) {
      fprintf(stderr, "unit-flags fixture unexpectedly already consistent\n");
      col1_save_free(&broken);
      return 1;
    }
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 23;
    for (int t = 0; t < units.type_count; ++t) {
      snprintf(units.types[t].name, sizeof(units.types[t].name), "T%d", t);
      units.types[t].movement = 1;
      units.types[t].domain = (t >= 13 && t <= 18) ? COLONIZE_UNIT_DOMAIN_SEA
                                                   : COLONIZE_UNIT_DOMAIN_LAND;
      units.types[t].cargo = (t >= 13 && t <= 18) ? 6 : 0;
    }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    EuropeScreen europe;
    memset(&europe, 0, sizeof(europe));
    europe.cargo_count = 16;
    ColonizeCol1BridgeResult br;
    if (!col1_bridge_apply(&broken, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
      fprintf(stderr, "unit-flags apply failed: %s\n", err);
      col1_save_free(&broken);
      return 1;
    }
    if (!col1_bridge_capture(
          &broken,
          &map,
          &units,
          &colonies,
          &europe,
          (uint16_t)br.year,
          (uint16_t)br.autumn,
          br.turn_number,
          br.human_nation,
          br.cursor_x,
          br.cursor_y,
          units.selected_id,
          err,
          sizeof(err)
        )) {
      fprintf(stderr, "unit-flags capture failed: %s\n", err);
      map_free(&map);
      col1_save_free(&broken);
      return 1;
    }
    if (!assert_mask_occupancy_consistent(&broken, "unit-flags after capture")) {
      map_free(&map);
      col1_save_free(&broken);
      return 1;
    }
    for (uint16_t ui = 0; ui < broken.head.unit_count; ++ui) {
      if (broken.unit[ui].ai_plan != COL1_UNIT_UNKNOWN16_HI_DEFAULT) {
        fprintf(
          stderr,
          "unit-flags: unit[%u] ai_plan=0x%02x want 0x%02x\n",
          (unsigned)ui,
          broken.unit[ui].ai_plan,
          COL1_UNIT_UNKNOWN16_HI_DEFAULT
        );
        map_free(&map);
        col1_save_free(&broken);
        return 1;
      }
    }
    fprintf(stderr, "unit-flags error.sav apply→capture occupancy ok\n");
    map_free(&map);
    col1_save_free(&broken);
  }

  /* New-game template → spawn → capture: occupancy + unknown16 default. */
  {
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
      fprintf(stderr, "newgame export: NAMES.TXT load failed\n");
      return 1;
    }
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    if (!map_alloc(&map, COLONIZE_COL1_MAP_W_STD, COLONIZE_COL1_MAP_H_STD, err, sizeof(err))) {
      fprintf(stderr, "newgame export: map_alloc: %s\n", err);
      assets_msg_free(&names);
      return 1;
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.terrain[i] = 1; /* plains-ish land */
    }
    /* Western ocean strip so density sync can set pacific / offshore suppress. */
    for (int y = 0; y < (int)map.height; ++y) {
      for (int x = 0; x < 8; ++x) {
        map.terrain[y * (int)map.width + x] = 25; /* ocean */
      }
    }
    ColonizeCol1Save save;
    if (!col1_bridge_init_template(&save, map.width, map.height, err, sizeof(err))) {
      fprintf(stderr, "newgame export: template: %s\n", err);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* One village so has_city is exercised. */
    save.head.tribe_count = 1;
    {
      ColonizeCol1Tribe* neu = calloc(1, sizeof(ColonizeCol1Tribe));
      if (!neu) {
        fprintf(stderr, "newgame export: tribe oom\n");
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      neu->x = 20;
      neu->y = 30;
      neu->nation_id = 6; /* Arawak */
      free(save.tribe);
      save.tribe = neu;
    }
    ColonizeUnitPool units;
    units_reset(&units);
    if (!units_load_types(&units, &names)) {
      fprintf(stderr, "newgame export: unit types failed\n");
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_set_occupancy_map(&map);
    const int freeman = units_find_type(&units, "Free Colonist");
    const int uid = units_spawn(&units, freeman >= 0 ? freeman : 0, 10, 12);
    if (uid < 0) {
      fprintf(stderr, "newgame export: spawn failed\n");
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    {
      ColonizeUnit* u = units_get(&units, uid);
      if (!u || u->col1_ai_plan != COL1_UNIT_UNKNOWN16_HI_DEFAULT) {
        fprintf(stderr, "newgame export: spawn unknown16_hi default missing\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    EuropeScreen europe;
    memset(&europe, 0, sizeof(europe));
    europe.cargo_count = 16;
    if (!col1_bridge_capture(
          &save, &map, &units, &colonies, &europe, 1492, 0, 1, 0, 10, 12, uid, err, sizeof(err)
        )) {
      fprintf(stderr, "newgame export: capture: %s\n", err);
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (!assert_mask_occupancy_consistent(&save, "newgame export")) {
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (save.head.unit_count < 1 ||
        save.unit[0].ai_plan != COL1_UNIT_UNKNOWN16_HI_DEFAULT) {
      fprintf(stderr, "newgame export: captured ai_plan wrong\n");
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    fprintf(stderr, "newgame template capture occupancy ok\n");
    /* Blank census fill (FUN_4962_0018 template-only). */
    if (save.stuff.all_unit_counts[0] < 1) {
      fprintf(stderr, "newgame export: blank census all_unit_counts[0] still 0\n");
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    if (save.head.unit_count < 1 || save.unit[0].vis_mask == 0) {
      fprintf(stderr, "newgame export: vis_mask not set on spawned euro unit\n");
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Discovery must stay 0 until land is seen (COLONY00-shaped). */
    if (save.head.event.discovery_of_the_new_world || save.player[0].named_new_world) {
      fprintf(stderr, "newgame export: discovery flags set too early\n");
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Reveal land then sync discovery for mid-campaign DOS continue. */
    map_reveal_tile(&map, 20, 20, 0);
    col1_bridge_sync_new_world_discovery(&save, &map, 0);
    if (!save.head.event.discovery_of_the_new_world || !save.player[0].named_new_world) {
      fprintf(stderr, "newgame export: discovery flags not set after land reveal\n");
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    fprintf(stderr, "newgame discovery flag sync ok\n");
    {
      int pacific = 0;
      int suppress = 0;
      for (size_t i = 0; i < save.map.tile_count; ++i) {
        if ((save.map.mask[i] & 0x20u) != 0) {
          pacific++;
        }
        if ((save.map.mask[i] & 0x04u) != 0) {
          suppress++;
        }
      }
      if (pacific == 0) {
        fprintf(stderr, "newgame export: pacific density bit never set\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (suppress == 0) {
        fprintf(stderr, "newgame export: suppress density bit never set\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      fprintf(
        stderr,
        "newgame density ok (pacific=%d suppress=%d census0=%u)\n",
        pacific,
        suppress,
        (unsigned)save.stuff.all_unit_counts[0]
      );
    }
    /* Blank template post_map must be rebuilt (FUN_67f4_0088) on capture. */
    {
      int sea_nz = 0;
      int land_nz = 0;
      for (size_t i = 0; i < COLONIZE_COL1_CONNECT_PLANE_SIZE; ++i) {
        if (save.post_map.sea_connectivity[i]) {
          sea_nz++;
        }
        if (save.post_map.land_connectivity[i]) {
          land_nz++;
        }
      }
      /* Mixed ocean/land template: land plane should light up. */
      if (land_nz == 0) {
        fprintf(stderr, "newgame export: post_map land connectivity still blank\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (save.post_map.prime_resource_seed != 0 || save.post_map.save_path_blob[0] != 0) {
        fprintf(stderr, "newgame export: post_map tail should stay zero on template\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      (void)sea_nz;
    }
    /* Starter fleet + brave: holds_occupied goods-only; native vis_mask 0. */
    {
      units_reset(&units);
      if (!units_load_types(&units, &names)) {
        fprintf(stderr, "fleet export: reload types failed\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      const int ship_id = units_spawn_euro_starter_fleet(&units, 0, 0, 40, 30, 45, 30);
      if (ship_id < 0) {
        fprintf(stderr, "fleet export: starter fleet failed\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      /* Match human deploy: idle ship with goto==xy (not stale landfall). */
      {
        ColonizeUnit* ship = units_get(&units, ship_id);
        if (ship) {
          ship->orders = 0;
          ship->goto_x = ship->x;
          ship->goto_y = ship->y;
          for (int c = 0; c < ship->cargo_count; ++c) {
            ColonizeUnit* pax = units_get(&units, ship->cargo_ids[c]);
            if (pax) {
              pax->goto_x = ship->x;
              pax->goto_y = ship->y;
            }
          }
        }
      }
      const int brave_ty = units_find_type(&units, "Brave");
      const int bid = units_spawn_allow_stack(&units, brave_ty >= 0 ? brave_ty : 0, 25, 25);
      if (bid >= 0) {
        ColonizeUnit* b = units_get(&units, bid);
        if (b) {
          units_set_nation(b, 6); /* Arawak */
        }
      }
      if (!col1_bridge_capture(
            &save, &map, &units, &colonies, &europe, 1492, 0, 1, 0, 40, 30, ship_id, err,
            sizeof(err)
          )) {
        fprintf(stderr, "fleet export: capture: %s\n", err);
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      int ship_ci = -1;
      int native_vis_bad = 0;
      for (uint16_t ui = 0; ui < save.head.unit_count; ++ui) {
        const ColonizeCol1Unit* u = &save.unit[ui];
        if ((u->nation_id & 0xF) >= 4 && u->vis_mask != 0) {
          native_vis_bad++;
        }
        if (u->type == 13 || (units_type(&units, u->type) &&
                              strstr(units_type(&units, u->type)->name, "Caravel"))) {
          ship_ci = (int)ui;
        }
      }
      /* Find caravel by cargo chain: ship with prev pointing to passenger. */
      for (uint16_t ui = 0; ui < save.head.unit_count; ++ui) {
        const ColonizeCol1Unit* u = &save.unit[ui];
        if (u->transport_chain.prev_unit_idx >= 0 && u->transport_chain.next_unit_idx < 0 &&
            (u->nation_id & 0xF) < 4) {
          ship_ci = (int)ui;
          break;
        }
      }
      if (ship_ci < 0) {
        fprintf(stderr, "fleet export: ship not found\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (save.unit[ship_ci].holds_occupied != 0) {
        fprintf(
          stderr,
          "fleet export: ship holds_occupied=%u want 0 (passengers≠goods)\n",
          (unsigned)save.unit[ship_ci].holds_occupied
        );
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      /* Walk passenger chain: origin 0xff, pioneer tools in cargo_hold[5]. */
      {
        int cur = -1;
        for (uint16_t ui = 0; ui < save.head.unit_count; ++ui) {
          if (save.unit[ui].transport_chain.next_unit_idx == (int16_t)ship_ci &&
              save.unit[ui].nation_id == 0) {
            cur = (int)ui;
            break;
          }
        }
        /* Prefer chain head (prev < 0). */
        for (uint16_t ui = 0; ui < save.head.unit_count; ++ui) {
          const ColonizeCol1Unit* u = &save.unit[ui];
          if (u->nation_id != 0 || u->transport_chain.next_unit_idx < 0) {
            continue;
          }
          if (u->transport_chain.prev_unit_idx < 0) {
            cur = (int)ui;
            break;
          }
        }
        int saw_pax = 0;
        int saw_tools = 0;
        for (int step = 0; step < 8 && cur >= 0 && cur < (int)save.head.unit_count; ++step) {
          const ColonizeCol1Unit* u = &save.unit[cur];
          if (cur == ship_ci) {
            break;
          }
          saw_pax++;
          if (u->origin != 0xff) {
            fprintf(
              stderr,
              "fleet export: passenger[%d] origin=%u want 0xff\n",
              cur,
              (unsigned)u->origin
            );
            units_set_occupancy_map(NULL);
            col1_save_free(&save);
            map_free(&map);
            assets_msg_free(&names);
            return 1;
          }
          if (u->type == 2 && u->cargo_hold[5] == 100) {
            saw_tools = 1;
          }
          if (u->profession == 20) {
            fprintf(
              stderr,
              "fleet export: English Discoverer passenger[%d] prof=%u want 28 (not Hardy)\n",
              cur,
              (unsigned)u->profession
            );
            units_set_occupancy_map(NULL);
            col1_save_free(&save);
            map_free(&map);
            assets_msg_free(&names);
            return 1;
          }
          cur = u->transport_chain.next_unit_idx;
        }
        if (saw_pax < 1) {
          fprintf(stderr, "fleet export: no passengers in chain to ship_ci=%d\n", ship_ci);
          units_set_occupancy_map(NULL);
          col1_save_free(&save);
          map_free(&map);
          assets_msg_free(&names);
          return 1;
        }
        if (!saw_tools) {
          fprintf(stderr, "fleet export: pioneer missing cargo_hold[5]=100 tools\n");
          units_set_occupancy_map(NULL);
          col1_save_free(&save);
          map_free(&map);
          assets_msg_free(&names);
          return 1;
        }
        if (save.unit[ship_ci].profession != 0) {
          fprintf(
            stderr,
            "fleet export: ship profession=%u want 0 (FUN_1427_06b4 transport)\n",
            (unsigned)save.unit[ship_ci].profession
          );
          units_set_occupancy_map(NULL);
          col1_save_free(&save);
          map_free(&map);
          assets_msg_free(&names);
          return 1;
        }
        if (save.unit[ship_ci].moves != 0) {
          fprintf(
            stderr,
            "fleet export: ship moves_spent=%u want 0 (full MP)\n",
            (unsigned)save.unit[ship_ci].moves
          );
          units_set_occupancy_map(NULL);
          col1_save_free(&save);
          map_free(&map);
          assets_msg_free(&names);
          return 1;
        }
        /* FUN_1427_02ca: ship tile path owner nibble = nation (not 0xf unowned). */
        {
          const uint8_t sx = save.unit[ship_ci].x;
          const uint8_t sy = save.unit[ship_ci].y;
          const size_t ti = (size_t)sy * (size_t)save.map.width + (size_t)sx;
          const uint8_t path = (save.map.path && ti < save.map.tile_count) ? save.map.path[ti] : 0xffu;
          const uint8_t owner = (uint8_t)((path >> 4) & 0x0fu);
          if (owner != 0) {
            fprintf(
              stderr,
              "fleet export: ship tile path=0x%02x owner=%u want nation 0 (FUN_137f_0228)\n",
              (unsigned)path,
              (unsigned)owner
            );
            units_set_occupancy_map(NULL);
            col1_save_free(&save);
            map_free(&map);
            assets_msg_free(&names);
            return 1;
          }
        }
        /* Idle ship (orders==0): goto must equal xy (not stale landfall). */
        if (save.unit[ship_ci].orders == 0 &&
            (save.unit[ship_ci].goto_x != save.unit[ship_ci].x ||
             save.unit[ship_ci].goto_y != save.unit[ship_ci].y)) {
          fprintf(
            stderr,
            "fleet export: idle ship goto=(%u,%u) want xy=(%u,%u) (COLONY00)\n",
            (unsigned)save.unit[ship_ci].goto_x,
            (unsigned)save.unit[ship_ci].goto_y,
            (unsigned)save.unit[ship_ci].x,
            (unsigned)save.unit[ship_ci].y
          );
          units_set_occupancy_map(NULL);
          col1_save_free(&save);
          map_free(&map);
          assets_msg_free(&names);
          return 1;
        }
      }
      if (native_vis_bad) {
        fprintf(stderr, "fleet export: %d natives with nonzero vis_mask\n", native_vis_bad);
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      fprintf(stderr, "fleet export holds/vis ok (ship_ci=%d)\n", ship_ci);
    }
    units_set_occupancy_map(NULL);
    col1_save_free(&save);
    map_free(&map);
    assets_msg_free(&names);
  }

  /*
   * bugs.md regression: capture's DOS-hygiene pass must NOT board land units
   * that merely share a LAND tile (colony dock / coastal stack) with a ship —
   * it used to force the whole garrison aboard (orders→Sentry, live pool
   * mutated) on every save. Only a land unit stranded on WATER (an orphan
   * state DOS can't represent) may be auto-boarded.
   */
  {
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
      fprintf(stderr, "dock-garrison: NAMES.TXT load failed\n");
      return 1;
    }
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    if (!map_alloc(&map, COLONIZE_COL1_MAP_W_STD, COLONIZE_COL1_MAP_H_STD, err, sizeof(err))) {
      fprintf(stderr, "dock-garrison: map_alloc: %s\n", err);
      assets_msg_free(&names);
      return 1;
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.terrain[i] = 25; /* ocean */
    }
    /* One land tile at (20,20). */
    map.terrain[20 * (int)map.width + 20] = 1;
    ColonizeCol1Save save;
    if (!col1_bridge_init_template(&save, map.width, map.height, err, sizeof(err))) {
      fprintf(stderr, "dock-garrison: template: %s\n", err);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeUnitPool units;
    units_reset(&units);
    if (!units_load_types(&units, &names)) {
      fprintf(stderr, "dock-garrison: unit types failed\n");
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_set_occupancy_map(&map);
    const int freeman = units_find_type(&units, "Free Colonist");
    const int caravel = units_find_type(&units, "Caravel");
    /* Ship docked on the land tile (colony dock stand-in) + no-orders unit. */
    const int ship_id = units_spawn_allow_stack(&units, caravel >= 0 ? caravel : 13, 20, 20);
    const int land_id = units_spawn_allow_stack(&units, freeman >= 0 ? freeman : 0, 20, 20);
    /* Land unit stranded on a WATER tile beside a second ship (orphan). */
    const int ship2_id = units_spawn_allow_stack(&units, caravel >= 0 ? caravel : 13, 22, 20);
    const int orphan_id = units_spawn_allow_stack(&units, freeman >= 0 ? freeman : 0, 22, 20);
    if (ship_id < 0 || land_id < 0 || ship2_id < 0 || orphan_id < 0) {
      fprintf(stderr, "dock-garrison: spawns failed\n");
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    {
      ColonizeUnit* lu = units_get(&units, land_id);
      if (lu) {
        lu->orders = UNITS_ORDER_NONE;
      }
    }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    EuropeScreen europe;
    memset(&europe, 0, sizeof(europe));
    europe.cargo_count = 16;
    if (!col1_bridge_capture(
          &save, &map, &units, &colonies, &europe, 1492, 0, 1, 0, 20, 20, ship_id, err,
          sizeof(err)
        )) {
      fprintf(stderr, "dock-garrison: capture: %s\n", err);
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const ColonizeUnit* lu = units_get(&units, land_id);
    if (!lu || lu->aboard_ship_id >= 0 || lu->orders != UNITS_ORDER_NONE) {
      fprintf(
        stderr,
        "dock-garrison: land-tile unit boarded by capture (aboard=%d orders=%d)\n",
        lu ? lu->aboard_ship_id : -99,
        lu ? lu->orders : -99
      );
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    const ColonizeUnit* ou = units_get(&units, orphan_id);
    if (!ou || ou->aboard_ship_id != ship2_id) {
      fprintf(
        stderr,
        "dock-garrison: water orphan not boarded (aboard=%d want %d)\n",
        ou ? ou->aboard_ship_id : -99,
        ship2_id
      );
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    fprintf(stderr, "dock-garrison capture hygiene ok\n");
    units_set_occupancy_map(NULL);
    col1_save_free(&save);
    map_free(&map);
    assets_msg_free(&names);
  }

  /* FUN_67f4_0088 rebuild vs COLONY00: tallies exact; planes near-match. */
  {
    ColonizeCol1Save orig;
    col1_save_init(&orig);
    if (!col1_save_read_file("original_saves/COLONY00.SAV", &orig, err, sizeof(err))) {
      fprintf(stderr, "post_map rebuild: COLONY00 read failed: %s\n", err);
      return 1;
    }
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    if (!map_alloc(&map, orig.head.map_size_x, orig.head.map_size_y, err, sizeof(err))) {
      fprintf(stderr, "post_map rebuild: map_alloc: %s\n", err);
      col1_save_free(&orig);
      return 1;
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.terrain[i] = col1_tile_to_mp_terrain(orig.map.tile[i]);
      if (map.layer3 && orig.map.path) {
        map.layer3[i] = orig.map.path[i];
      }
    }
    ColonizeCol1PostMap rebuilt;
    memset(&rebuilt, 0, sizeof(rebuilt));
    /* Keep a distinctive tail to prove rebuild preserves it. */
    rebuilt.prime_resource_seed = 0x1234;
    col1_post_map_rebuild_connectivity(&rebuilt, &map);
    if (rebuilt.prime_resource_seed != 0x1234) {
      fprintf(stderr, "post_map rebuild: tail not preserved\n");
      map_free(&map);
      col1_save_free(&orig);
      return 1;
    }
    for (int i = 0; i < 16; ++i) {
      if (rebuilt.continent_tally_a[i] != orig.post_map.continent_tally_a[i] ||
          rebuilt.continent_tally_b[i] != orig.post_map.continent_tally_b[i]) {
        fprintf(
          stderr,
          "post_map rebuild: tally[%d] a=%u/%u b=%u/%u\n",
          i,
          (unsigned)rebuilt.continent_tally_a[i],
          (unsigned)orig.post_map.continent_tally_a[i],
          (unsigned)rebuilt.continent_tally_b[i],
          (unsigned)orig.post_map.continent_tally_b[i]
        );
        map_free(&map);
        col1_save_free(&orig);
        return 1;
      }
    }
    int sea_diff = 0;
    int land_diff = 0;
    for (size_t i = 0; i < COLONIZE_COL1_CONNECT_PLANE_SIZE; ++i) {
      if (rebuilt.sea_connectivity[i] != orig.post_map.sea_connectivity[i]) {
        sea_diff++;
      }
      if (rebuilt.land_connectivity[i] != orig.post_map.land_connectivity[i]) {
        land_diff++;
      }
    }
    /* Exact plane match once 00f2 dest-cost cache is modeled. */
    if (sea_diff != 0 || land_diff != 0) {
      fprintf(
        stderr,
        "post_map rebuild: plane drift sea_diff=%d land_diff=%d\n",
        sea_diff,
        land_diff
      );
      map_free(&map);
      col1_save_free(&orig);
      return 1;
    }
    fprintf(stderr, "COLONY00 post_map rebuild ok (planes+tallies exact)\n");
    /* Stuff census window (FUN_4962_0018) — nation0 unit total matches type row. */
    {
      unsigned type_sum = 0;
      for (int t = 0; t < 19; ++t) {
        type_sum += orig.stuff.unit_type_counts[0][t];
      }
      if (orig.stuff.all_unit_counts[0] != (uint8_t)type_sum) {
        fprintf(
          stderr,
          "stuff census: all_unit_counts[0]=%u type_sum=%u\n",
          (unsigned)orig.stuff.all_unit_counts[0],
          type_sum
        );
        map_free(&map);
        col1_save_free(&orig);
        return 1;
      }
      fprintf(
        stderr,
        "COLONY00 stuff census ok (all_unit_counts[0]=%u)\n",
        (unsigned)orig.stuff.all_unit_counts[0]
      );
    }
    map_free(&map);
    col1_save_free(&orig);
  }

  /* Lategame: connectivity rebuild stays byte-exact; mapped fields already checked above. */
  {
    ColonizeCol1Save orig;
    col1_save_init(&orig);
    const char* late = "original_saves/valid-lategame-saves/COLONY00.SAV";
    if (!col1_save_read_file(late, &orig, err, sizeof(err))) {
      fprintf(stderr, "lategame rebuild: read failed: %s\n", err);
      return 1;
    }
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    if (!map_alloc(&map, orig.head.map_size_x, orig.head.map_size_y, err, sizeof(err))) {
      fprintf(stderr, "lategame rebuild: map_alloc: %s\n", err);
      col1_save_free(&orig);
      return 1;
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.terrain[i] = col1_tile_to_mp_terrain(orig.map.tile[i]);
      if (map.layer3 && orig.map.path) {
        map.layer3[i] = orig.map.path[i];
      }
    }
    ColonizeCol1PostMap rebuilt;
    memset(&rebuilt, 0, sizeof(rebuilt));
    rebuilt.prime_resource_seed = orig.post_map.prime_resource_seed;
    col1_post_map_rebuild_connectivity(&rebuilt, &map);
    int sea_diff = 0;
    int land_diff = 0;
    for (size_t i = 0; i < COLONIZE_COL1_CONNECT_PLANE_SIZE; ++i) {
      if (rebuilt.sea_connectivity[i] != orig.post_map.sea_connectivity[i]) {
        sea_diff++;
      }
      if (rebuilt.land_connectivity[i] != orig.post_map.land_connectivity[i]) {
        land_diff++;
      }
    }
    for (int i = 0; i < 16; ++i) {
      if (rebuilt.continent_tally_a[i] != orig.post_map.continent_tally_a[i] ||
          rebuilt.continent_tally_b[i] != orig.post_map.continent_tally_b[i]) {
        fprintf(stderr, "lategame rebuild: tally mismatch at %d\n", i);
        map_free(&map);
        col1_save_free(&orig);
        return 1;
      }
    }
    if (sea_diff != 0 || land_diff != 0) {
      fprintf(stderr, "lategame rebuild: plane drift sea=%d land=%d\n", sea_diff, land_diff);
      map_free(&map);
      col1_save_free(&orig);
      return 1;
    }
    if (rebuilt.prime_resource_seed != 541) {
      fprintf(
        stderr,
        "lategame rebuild: unexpected prime_resource_seed %u\n",
        (unsigned)rebuilt.prime_resource_seed
      );
      map_free(&map);
      col1_save_free(&orig);
      return 1;
    }
    fprintf(
      stderr,
      "lategame COLONY00 rebuild ok (year=%u colonies=%u seed=%u)\n",
      (unsigned)orig.head.year,
      (unsigned)orig.head.colony_count,
      (unsigned)orig.post_map.prime_resource_seed
    );
    map_free(&map);
    col1_save_free(&orig);
  }

  /* Original starters already consistent (sanity for the assert helper). */
  {
    ColonizeCol1Save orig;
    col1_save_init(&orig);
    if (!col1_save_read_file("original_saves/COLONY00.SAV", &orig, err, sizeof(err))) {
      fprintf(stderr, "COLONY00 occupancy read failed: %s\n", err);
      return 1;
    }
    if (!assert_mask_occupancy_consistent(&orig, "COLONY00")) {
      col1_save_free(&orig);
      return 1;
    }
    col1_save_free(&orig);
    fprintf(stderr, "COLONY00 occupancy consistent\n");
  }

  /* FUN_75c2_0840 version / map-size probe (cite: @LOADNOT / @LOADOLD / @LOADSIZE). */
  {
    ColonizeCol1Head h;
    memset(&h, 0, sizeof(h));
    col1_save_stamp_head(&h);
    h.map_size_x = 58;
    h.map_size_y = 72;
    if (!col1_save_validate_head(&h, -1, -1, err, sizeof(err))) {
      fprintf(stderr, "valid head rejected: %s\n", err);
      return 1;
    }
    h.save_version = 72;
    if (col1_save_validate_head(&h, -1, -1, err, sizeof(err)) ||
        strstr(err, "obsolete") == NULL) {
      fprintf(stderr, "expected obsolete reject, got '%s'\n", err);
      return 1;
    }
    h.save_version = 74;
    if (col1_save_validate_head(&h, -1, -1, err, sizeof(err)) ||
        strstr(err, "not a valid") == NULL) {
      fprintf(stderr, "expected invalid newer reject, got '%s'\n", err);
      return 1;
    }
    h.save_version = COLONIZE_COL1_SAVE_VERSION;
    h.sig_eof = 0;
    if (col1_save_validate_head(&h, -1, -1, err, sizeof(err)) ||
        strstr(err, "not a valid") == NULL) {
      fprintf(stderr, "expected bad eof reject, got '%s'\n", err);
      return 1;
    }
    col1_save_stamp_head(&h);
    h.map_size_x = 58;
    h.map_size_y = 72;
    if (col1_save_validate_head(&h, 40, 50, err, sizeof(err)) ||
        strstr(err, "map size") == NULL) {
      fprintf(stderr, "expected map-size reject, got '%s'\n", err);
      return 1;
    }
    fprintf(stderr, "col1 FUN_75c2_0840 validate ok\n");
  }

  /*
   * Player-reported: already-explored Lost City Rumours still showed on the
   * overland map after loading dutch-reports.SAV. Root cause: rumours are
   * purely procedural (map_procedural_rumour_at, a hash of position + seed)
   * with no dedicated "already explored" bit in the Col1 format itself —
   * map_clear_rumour only ever sets our own runtime-only layer2 bit
   * (MAP_LAYER2_RUMOUR_CLEARED), which starts zero on every fresh import.
   * Fix: col1_bridge_apply now also seeds that bit from the Col1 `path`
   * field's own visitor-history nibble (0xf = nobody has ever occupied this
   * tile) — resolving a rumour always means a unit physically stood on it,
   * so "has anyone ever visited" implies "any rumour here was already
   * triggered". Assert the general invariant this fix guarantees: no tile
   * anyone has ever visited can still report an active rumour.
   */
  {
    founding_fathers_reset();
    ColonizeCol1Save orig;
    col1_save_init(&orig);
    if (!col1_save_read_file(
          "original_saves/report-screen-goldens/dutch-reports.SAV", &orig, err, sizeof(err)
        )) {
      fprintf(stderr, "dutch-reports.SAV read failed: %s\n", err);
      return 1;
    }
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 23;
    for (int t = 0; t < units.type_count; ++t) {
      snprintf(units.types[t].name, sizeof(units.types[t].name), "T%d", t);
      units.types[t].movement = 1;
      units.types[t].domain =
        (t >= 13 && t <= 18) ? COLONIZE_UNIT_DOMAIN_SEA : COLONIZE_UNIT_DOMAIN_LAND;
      units.types[t].cargo = (t >= 13 && t <= 18) ? 6 : 0;
    }
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    EuropeScreen europe;
    memset(&europe, 0, sizeof(europe));
    europe.cargo_count = 16;
    ColonizeCol1BridgeResult br;
    if (!col1_bridge_apply(&orig, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
      fprintf(stderr, "dutch-reports.SAV bridge apply failed: %s\n", err);
      col1_save_free(&orig);
      return 1;
    }
    int visited_tiles = 0;
    int stale_rumours = 0;
    for (int y = 0; y < (int)map.height; ++y) {
      for (int x = 0; x < (int)map.width; ++x) {
        if ((map_get_layer3(&map, x, y) >> 4) == 0x0fu) {
          continue; /* never visited */
        }
        visited_tiles++;
        if (map_tile_has_rumour(&map, x, y)) {
          stale_rumours++;
        }
      }
    }
    if (visited_tiles == 0) {
      fprintf(stderr, "dutch-reports.SAV LCR test: no visited tiles found (fixture stale?)\n");
      map_free(&map);
      col1_save_free(&orig);
      return 1;
    }
    if (stale_rumours != 0) {
      fprintf(
        stderr,
        "dutch-reports.SAV LCR test: %d already-visited tile(s) still report a rumour\n",
        stale_rumours
      );
      map_free(&map);
      col1_save_free(&orig);
      return 1;
    }
    map_free(&map);
    col1_save_free(&orig);
    fprintf(
      stderr, "dutch-reports.SAV: no stale rumours on any of %d visited tiles ok\n", visited_tiles
    );
  }

  /*
   * bugs.md: nation+6 (the Recruit passage ladder counter) was never bridged,
   * so game_apply_col1_save's europe_reset_campaign dropped it to 0 on every
   * load and the Europe passage price fell back to its opening rung. Round-trip
   * it in both directions, and check apply recomputes the displayed price.
   */
  {
    ColonizeWorldMap map;
    char err[256];
    if (!map_alloc(&map, 32, 32, err, sizeof(err))) {
      fprintf(stderr, "recruit_count roundtrip: map_alloc: %s\n", err);
      return 1;
    }
    ColonizeCol1Save save;
    if (!col1_bridge_init_template(&save, map.width, map.height, err, sizeof(err))) {
      fprintf(stderr, "recruit_count roundtrip: template: %s\n", err);
      map_free(&map);
      return 1;
    }
    save.head.difficulty = 3;
    save.nation[save.head.human_player & 3].recruit_count = 6;
    save.nation[save.head.human_player & 3].current_crosses = 0;
    save.nation[save.head.human_player & 3].needed_crosses = 9;

    ColonizeUnitPool units;
    units_reset(&units);
    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    EuropeScreen europe;
    memset(&europe, 0, sizeof(europe));
    europe.cargo_count = 16;
    ColonizeCol1BridgeResult br;
    if (!col1_bridge_apply(&save, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
      fprintf(stderr, "recruit_count roundtrip: apply: %s\n", err);
      col1_save_free(&save);
      map_free(&map);
      return 1;
    }
    const int want_passage = europe_compute_recruit_passage(6, 3, 0, 9);
    if (europe.recruit_count != 6 || europe.recruit_passage != want_passage) {
      fprintf(
        stderr,
        "recruit_count roundtrip: apply gave count=%u passage=%d (want 6/%d)\n",
        (unsigned)europe.recruit_count,
        europe.recruit_passage,
        want_passage
      );
      col1_save_free(&save);
      map_free(&map);
      return 1;
    }
    europe.recruit_count = 9;
    if (!col1_bridge_capture(
          &save, &map, &units, &colonies, &europe, br.year, br.autumn, br.turn_number,
          br.human_nation, br.cursor_x, br.cursor_y, -1, err, sizeof(err)
        )) {
      fprintf(stderr, "recruit_count roundtrip: capture: %s\n", err);
      col1_save_free(&save);
      map_free(&map);
      return 1;
    }
    if (save.nation[br.human_nation & 3].recruit_count != 9) {
      fprintf(
        stderr,
        "recruit_count roundtrip: capture kept %u (want 9)\n",
        (unsigned)save.nation[br.human_nation & 3].recruit_count
      );
      col1_save_free(&save);
      map_free(&map);
      return 1;
    }
    col1_save_free(&save);
    map_free(&map);
    fprintf(stderr, "recruit_count survives the col1 bridge both ways ok\n");
  }

  /*
   * bugs.md (trade_route_wagon.SAV): a wagon/ship on a trade route came back
   * from a save with no route at all, so game_trade_route_retarget bailed on
   * follow_unit_id == -1 and the unit just sat in the activation queue. DOS
   * keeps the cursor in the profession byte a route-running unit does not
   * use (unit +0x17 = DS:0x315b; FUN_1427_0f64/0f74 low nibble = trade_route
   * slot, FUN_1427_0f8e/0fa0 high nibble = stop index) — bridge both ways.
   */
  {
    ColonizeMsgCatalog names;
    assets_msg_init(&names);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
      fprintf(stderr, "trade-route cursor: NAMES.TXT load failed\n");
      return 1;
    }
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    if (!map_alloc(&map, COLONIZE_COL1_MAP_W_STD, COLONIZE_COL1_MAP_H_STD, err, sizeof(err))) {
      fprintf(stderr, "trade-route cursor: map_alloc: %s\n", err);
      assets_msg_free(&names);
      return 1;
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.terrain[i] = 1; /* land */
    }
    ColonizeCol1Save save;
    if (!col1_bridge_init_template(&save, map.width, map.height, err, sizeof(err))) {
      fprintf(stderr, "trade-route cursor: template: %s\n", err);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    /* Slot 2 is a live two-stop route; slot 3 stays empty. */
    save.head.trade_route_count = 3;
    snprintf(save.trade_route[2].name, sizeof(save.trade_route[2].name), "%s", "Wagon Loop");
    save.trade_route[2].sea = 0;
    save.trade_route[2].dest_count = 2;
    save.trade_route[2].stop[0].colony_index = 0;
    save.trade_route[2].stop[1].colony_index = 1;

    ColonizeUnitPool units;
    units_reset(&units);
    if (!units_load_types(&units, &names)) {
      fprintf(stderr, "trade-route cursor: unit types failed\n");
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    units_set_occupancy_map(&map);
    const int wagon_t = units_find_type(&units, "Wagon Train");
    const int wagon = units_spawn(&units, wagon_t >= 0 ? wagon_t : 12, 20, 20);
    const int stray = units_spawn(&units, wagon_t >= 0 ? wagon_t : 12, 22, 20);
    ColonizeUnit* w = units_get(&units, wagon);
    ColonizeUnit* sv = units_get(&units, stray);
    if (!w || !sv) {
      fprintf(stderr, "trade-route cursor: spawns failed\n");
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    w->nation_id = 0;
    w->orders = UNITS_ORDER_TRADE_ROUTE;
    w->follow_unit_id = 2;
    w->turns_worked = 1;
    sv->nation_id = 0;
    sv->orders = UNITS_ORDER_TRADE_ROUTE;
    sv->follow_unit_id = 3; /* route with no stops */
    sv->turns_worked = 0;
    const int wagon_x = w->x;
    const int wagon_y = w->y;
    const int stray_x = sv->x;
    const int stray_y = sv->y;

    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    EuropeScreen europe;
    memset(&europe, 0, sizeof(europe));
    europe.cargo_count = 16;
    if (!col1_bridge_capture(
          &save, &map, &units, &colonies, &europe, 1492, 0, 1, 0, 20, 20, wagon, err, sizeof(err)
        )) {
      fprintf(stderr, "trade-route cursor: capture: %s\n", err);
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    {
      int packed = -1;
      for (uint16_t i = 0; i < save.head.unit_count; ++i) {
        if (save.unit[i].x == (uint8_t)wagon_x && save.unit[i].y == (uint8_t)wagon_y) {
          packed = (int)save.unit[i].profession;
        }
      }
      if (packed != ((1 << 4) | 2)) {
        fprintf(
          stderr, "trade-route cursor: capture packed 0x%02x (want 0x12)\n", (unsigned)packed
        );
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
    }

    ColonizeUnitPool units2;
    units_reset(&units2);
    if (!units_load_types(&units2, &names)) {
      fprintf(stderr, "trade-route cursor: unit types (2) failed\n");
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    ColonizeColonyPool colonies2;
    colonies_init(&colonies2);
    EuropeScreen europe2;
    memset(&europe2, 0, sizeof(europe2));
    europe2.cargo_count = 16;
    ColonizeWorldMap map2;
    memset(&map2, 0, sizeof(map2));
    ColonizeCol1BridgeResult br2;
    if (!col1_bridge_apply(&save, &map2, &units2, &colonies2, &europe2, &br2, err, sizeof(err))) {
      fprintf(stderr, "trade-route cursor: apply: %s\n", err);
      units_set_occupancy_map(NULL);
      col1_save_free(&save);
      map_free(&map);
      assets_msg_free(&names);
      return 1;
    }
    int rc = 0;
    const ColonizeUnit* w2 = NULL;
    const ColonizeUnit* sv2 = NULL;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = units_get_const(&units2, i);
      if (!u || !u->active) {
        continue;
      }
      if (u->x == wagon_x && u->y == wagon_y) {
        w2 = u;
      } else if (u->x == stray_x && u->y == stray_y) {
        sv2 = u;
      }
    }
    if (!w2 || w2->orders != UNITS_ORDER_TRADE_ROUTE || w2->follow_unit_id != 2 ||
        w2->turns_worked != 1) {
      fprintf(
        stderr,
        "trade-route cursor: apply gave orders=%d route=%d stop=%d (want 2/2/1)\n",
        w2 ? w2->orders : -1,
        w2 ? w2->follow_unit_id : -1,
        w2 ? w2->turns_worked : -1
      );
      rc = 1;
    }
    if (!sv2 || sv2->orders != UNITS_ORDER_NONE || sv2->follow_unit_id != -1) {
      fprintf(
        stderr,
        "trade-route cursor: empty route should drop the order (orders=%d route=%d)\n",
        sv2 ? sv2->orders : -1,
        sv2 ? sv2->follow_unit_id : -1
      );
      rc = 1;
    }
    units_set_occupancy_map(NULL);
    map_free(&map2);
    map_free(&map);
    col1_save_free(&save);
    assets_msg_free(&names);
    if (rc != 0) {
      return 1;
    }
    fprintf(stderr, "trade-route cursor survives the col1 bridge both ways ok\n");
  }

  diag_shutdown();
  return 0;
}
