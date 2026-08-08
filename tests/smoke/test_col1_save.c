#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_bridge.h"
#include "core/col1_post_map.h"
#include "core/col1_save.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/units.h"
#include "platform/diagnostics.h"

static void fill_pattern(uint8_t* p, size_t n, uint8_t seed) {
  for (size_t i = 0; i < n; ++i) {
    p[i] = (uint8_t)(seed + (uint8_t)i * 3u + (uint8_t)(i >> 8));
  }
}

/* Read → encode → compare to on-disk bytes (codec only — not Linux→DOS playability). */
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
  } Col1Fixture;
  static const Col1Fixture k_fixtures[] = {
    {"original_saves/COLONY00.SAV", true, true},
    {"original_saves/COLONY01.SAV", true, true},
    {"original_saves/valid-lategame-saves/COLONY00.SAV", false, true},
    {"original_saves/valid-lategame-saves/COLONY01.SAV", false, true},
    {"original_saves/valid-lategame-saves/COLONY02.SAV", false, true},
    {"original_saves/valid-lategame-saves/COLONY03.SAV", false, true},
    {"original_saves/valid-lategame-saves/COLONY04.SAV", false, true},
    {"original_saves/valid-lategame-saves/COLONY05.SAV", false, true},
    {"original_saves/valid-lategame-saves/COLONY06.SAV", false, true},
    {"original_saves/valid-lategame-saves/COLONY07.SAV", false, true},
    {"original_saves/valid-lategame-saves/COLONY08.SAV", false, true},
    {"original_saves/valid-lategame-saves/COLONY10.SAV", false, true},
    {"test-saves-ai/TURN1.SAV", false, false},
    {"test-saves-ai/TURN2.SAV", false, false},
    {"test-saves-ai/TURN3.SAV", false, false},
    {"test-saves-ai/TURN4.SAV", false, false},
    {"test-saves-ai/TURN5.SAV", false, false},
    {"test-saves-ai/TURN6.SAV", false, false},
    {"test-saves-ai/TURN7.SAV", false, false},
  };
  for (size_t oi = 0; oi < sizeof(k_fixtures) / sizeof(k_fixtures[0]); ++oi) {
    const Col1Fixture* fix = &k_fixtures[oi];
    ColonizeCol1Save orig;
    if (!assert_byte_identical_roundtrip(fix->path, &orig, err, sizeof(err))) {
      return 1;
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
      /* Artificial all-plains map: land plane should light up; sea may be empty. */
      if (land_nz == 0) {
        fprintf(stderr, "newgame export: post_map land connectivity still blank\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      if (save.post_map.prime_resource_seed != 0 || save.post_map.unknown_post_604[0] != 0) {
        fprintf(stderr, "newgame export: post_map tail should stay zero on template\n");
        units_set_occupancy_map(NULL);
        col1_save_free(&save);
        map_free(&map);
        assets_msg_free(&names);
        return 1;
      }
      (void)sea_nz;
    }
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

  diag_shutdown();
  return 0;
}
