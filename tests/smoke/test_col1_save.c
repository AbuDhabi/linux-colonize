#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_bridge.h"
#include "core/col1_save.h"
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

static bool build_synthetic(ColonizeCol1Save* save, char* err, size_t err_size) {
  col1_save_init(save);
  memset(&save->head, 0, sizeof(save->head));
  memcpy(save->head.sig_colonize, COLONIZE_COL1_SIG, 8);
  save->head.sig_colonize[8] = 0x1a;
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
  fill_pattern(save->unknown_e, sizeof(save->unknown_e), 0xd5);
  fill_pattern(save->unknown_f, sizeof(save->unknown_f), 0xe6);
  fill_pattern((uint8_t*)save->trade_route, sizeof(save->trade_route), 0xf7);

  /* Keep signature intact after pattern fills on head-adjacent areas. */
  memcpy(save->head.sig_colonize, COLONIZE_COL1_SIG, 8);
  save->head.sig_colonize[8] = 0x1a;
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

  /* Original DOS saves: byte-identical round-trip + bridge apply. */
  static const char* k_originals[] = {
    "original_saves/COLONY00.SAV",
    "original_saves/COLONY01.SAV"
  };
  for (size_t oi = 0; oi < sizeof(k_originals) / sizeof(k_originals[0]); ++oi) {
    ColonizeCol1Save orig;
    col1_save_init(&orig);
    if (!col1_save_read_file(k_originals[oi], &orig, err, sizeof(err))) {
      fprintf(stderr, "original read failed %s: %s\n", k_originals[oi], err);
      return 1;
    }
    uint8_t* enc = NULL;
    size_t enc_n = 0;
    if (!col1_save_write_memory(&orig, &enc, &enc_n, err, sizeof(err))) {
      fprintf(stderr, "original encode failed %s: %s\n", k_originals[oi], err);
      col1_save_free(&orig);
      return 1;
    }
    FILE* f = fopen(k_originals[oi], "rb");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    uint8_t* raw = malloc((size_t)sz);
    if (!raw || fread(raw, 1, (size_t)sz, f) != (size_t)sz) {
      fprintf(stderr, "cannot reread %s\n", k_originals[oi]);
      fclose(f);
      free(raw);
      free(enc);
      col1_save_free(&orig);
      return 1;
    }
    fclose(f);
    if ((size_t)sz != enc_n || memcmp(raw, enc, enc_n) != 0) {
      fprintf(stderr, "original round-trip mismatch %s\n", k_originals[oi]);
      free(raw);
      free(enc);
      col1_save_free(&orig);
      return 1;
    }
    free(raw);
    free(enc);

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
      fprintf(stderr, "bridge apply failed %s: %s\n", k_originals[oi], err);
      col1_save_free(&orig);
      return 1;
    }
    if (br.imported_units < 3 || europe.gold != 1000) {
      fprintf(
        stderr,
        "bridge apply unexpected %s units=%d gold=%d\n",
        k_originals[oi],
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
      "original %s ok (units=%d year=%u gold=%d)\n",
      k_originals[oi],
      br.imported_units,
      br.year,
      europe.gold
    );
    map_free(&map);
    col1_save_free(&orig);
  }

  fprintf(stderr, "col1 original saves + bridge ok\n");
  diag_shutdown();
  return 0;
}
