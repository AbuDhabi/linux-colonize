#include "core/col1_save.h"
#include "core/founding_fathers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

#define COL1_FAIL(err, err_size, ...)            \
  do {                                           \
    if ((err) && (err_size) > 0) {               \
      snprintf((err), (err_size), __VA_ARGS__);  \
    }                                            \
    return false;                                \
  } while (0)

void col1_save_init(ColonizeCol1Save* save) {
  if (!save) {
    return;
  }
  memset(save, 0, sizeof(*save));
  /* head.founding_father[i]: -1 unclaimed; 0..3 = owning Euro nation.
   * Zero-fill would falsely make nation 0 own every FF (Franklin gate, etc.). */
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    save->head.founding_father[i] = -1;
  }
  /* next_founding_father: -1 = need Continental Congress debate (FUN_4345_06d2).
   * Zero would falsely lock Adam Smith without a player choice. */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    save->nation[n].next_founding_father = -1;
  }
}

void col1_save_free(ColonizeCol1Save* save) {
  if (!save) {
    return;
  }
  if (save->owned) {
    free(save->colony);
    free(save->unit);
    free(save->tribe);
    free(save->map.tile);
    free(save->map.mask);
    free(save->map.path);
    free(save->map.seen);
  }
  memset(save, 0, sizeof(*save));
}

bool col1_save_check_layout(char* err, size_t err_size) {
#define CHECK_SIZE(type, expect)                                                 \
  do {                                                                           \
    const size_t got = sizeof(type);                                             \
    if (got != (size_t)(expect)) {                                               \
      COL1_FAIL(err, err_size, #type " size %zu, expected %u", got, (unsigned)(expect)); \
    }                                                                            \
  } while (0)

  CHECK_SIZE(ColonizeCol1Head, COLONIZE_COL1_HEAD_SIZE);
  CHECK_SIZE(ColonizeCol1Player, COLONIZE_COL1_PLAYER_SIZE);
  CHECK_SIZE(ColonizeCol1Colony, COLONIZE_COL1_COLONY_SIZE);
  CHECK_SIZE(ColonizeCol1Unit, COLONIZE_COL1_UNIT_SIZE);
  CHECK_SIZE(ColonizeCol1Nation, COLONIZE_COL1_NATION_SIZE);
  CHECK_SIZE(ColonizeCol1Tribe, COLONIZE_COL1_TRIBE_SIZE);
  CHECK_SIZE(ColonizeCol1Indian, COLONIZE_COL1_INDIAN_SIZE);
  CHECK_SIZE(ColonizeCol1Stuff, COLONIZE_COL1_STUFF_SIZE);
  CHECK_SIZE(ColonizeCol1PostMap, COLONIZE_COL1_POST_MAP_SIZE);
  CHECK_SIZE(ColonizeCol1TradeRoute, COLONIZE_COL1_TRADE_ROUTE_SIZE);
  CHECK_SIZE(ColonizeCol1Buildings, 6u);
  CHECK_SIZE(ColonizeCol1CustomHouse, 2u);
  CHECK_SIZE(ColonizeCol1Tile, 1u);
  CHECK_SIZE(ColonizeCol1Mask, 1u);
  CHECK_SIZE(ColonizeCol1Path, 1u);
  CHECK_SIZE(ColonizeCol1Seen, 1u);

  const size_t prefix = sizeof(ColonizeCol1Head) +
                        sizeof(ColonizeCol1Player) * COLONIZE_COL1_NATION_COUNT +
                        COLONIZE_COL1_OTHER_SIZE;
  if (prefix != COLONIZE_COL1_PREFIX_SIZE) {
    COL1_FAIL(err, err_size, "prefix size %zu, expected %u", prefix, COLONIZE_COL1_PREFIX_SIZE);
  }

  if (err && err_size > 0) {
    err[0] = '\0';
  }
  return true;
#undef CHECK_SIZE
}

void col1_save_stamp_head(ColonizeCol1Head* head) {
  if (!head) {
    return;
  }
  memcpy(head->sig_colonize, COLONIZE_COL1_SIG, 8);
  head->sig_colonize[8] = '\0';
  head->sig_eof = COLONIZE_COL1_SIG_EOF;
  head->save_version = COLONIZE_COL1_SAVE_VERSION;
}

bool col1_save_validate_head(
  const ColonizeCol1Head* head,
  int expect_map_w,
  int expect_map_h,
  char* err,
  size_t err_size
) {
  /*
   * Cite: FUN_75c2_0840 — read NUL-terminated "COLONIZE" (FUN_1b2c_0004),
   * consume 0x1A, compare version to DS:0x81a, then map W×H product vs live
   * map (0/0 = accept any). Error codes → @LOADNOT / @LOADOLD / @LOADSIZE.
   */
  if (!head) {
    COL1_FAIL(err, err_size, "is not a valid save file");
  }
  if (memcmp(head->sig_colonize, COLONIZE_COL1_SIG, 8) != 0 || head->sig_colonize[8] != '\0') {
    COL1_FAIL(err, err_size, "is not a valid save file");
  }
  if (head->sig_eof != COLONIZE_COL1_SIG_EOF) {
    COL1_FAIL(err, err_size, "is not a valid save file");
  }
  if (head->save_version < COLONIZE_COL1_SAVE_VERSION) {
    COL1_FAIL(err, err_size, "is an obsolete save file");
  }
  if (head->save_version > COLONIZE_COL1_SAVE_VERSION) {
    COL1_FAIL(err, err_size, "is not a valid save file");
  }
  if (head->map_size_x == 0 || head->map_size_y == 0) {
    COL1_FAIL(err, err_size, "is not a valid save file");
  }
  if (expect_map_w > 0 && expect_map_h > 0) {
    if ((int)head->map_size_x != expect_map_w || (int)head->map_size_y != expect_map_h) {
      COL1_FAIL(err, err_size, "does not match the current map size");
    }
  }
  if (err && err_size > 0) {
    err[0] = '\0';
  }
  return true;
}

size_t col1_save_expected_size_counts(
  uint16_t map_w,
  uint16_t map_h,
  uint16_t colony_count,
  uint16_t unit_count,
  uint16_t tribe_count
) {
  const size_t tiles = (size_t)map_w * (size_t)map_h;
  return (size_t)COLONIZE_COL1_PREFIX_SIZE +
         (size_t)colony_count * COLONIZE_COL1_COLONY_SIZE +
         (size_t)unit_count * COLONIZE_COL1_UNIT_SIZE +
         (size_t)COLONIZE_COL1_NATION_COUNT * COLONIZE_COL1_NATION_SIZE +
         (size_t)tribe_count * COLONIZE_COL1_TRIBE_SIZE +
         (size_t)COLONIZE_COL1_INDIAN_COUNT * COLONIZE_COL1_INDIAN_SIZE +
         (size_t)COLONIZE_COL1_STUFF_SIZE +
         tiles * 4u +
         (size_t)COLONIZE_COL1_POST_MAP_SIZE +
         (size_t)COLONIZE_COL1_TRADE_ROUTE_COUNT * COLONIZE_COL1_TRADE_ROUTE_SIZE;
}

size_t col1_save_expected_size(const ColonizeCol1Save* save) {
  if (!save) {
    return 0;
  }
  return col1_save_expected_size_counts(
    save->head.map_size_x,
    save->head.map_size_y,
    save->head.colony_count,
    save->head.unit_count,
    save->head.tribe_count
  );
}

static bool read_exact(FILE* f, void* dst, size_t n, char* err, size_t err_size, const char* what) {
  if (n == 0) {
    return true;
  }
  if (fread(dst, 1, n, f) != n) {
    COL1_FAIL(err, err_size, "failed reading %s (%zu bytes)", what, n);
  }
  return true;
}

static bool write_exact(
  FILE* f,
  const void* src,
  size_t n,
  char* err,
  size_t err_size,
  const char* what
) {
  if (n == 0) {
    return true;
  }
  if (fwrite(src, 1, n, f) != n) {
    COL1_FAIL(err, err_size, "failed writing %s (%zu bytes)", what, n);
  }
  return true;
}

static bool mem_take(
  const uint8_t** pp,
  size_t* remain,
  void* dst,
  size_t n,
  char* err,
  size_t err_size,
  const char* what
) {
  if (n > *remain) {
    COL1_FAIL(err, err_size, "truncated while reading %s (need %zu, have %zu)", what, n, *remain);
  }
  memcpy(dst, *pp, n);
  *pp += n;
  *remain -= n;
  return true;
}

static bool mem_put(
  uint8_t** pp,
  size_t* remain,
  const void* src,
  size_t n,
  char* err,
  size_t err_size,
  const char* what
) {
  if (n > *remain) {
    COL1_FAIL(err, err_size, "buffer overrun writing %s", what);
  }
  memcpy(*pp, src, n);
  *pp += n;
  *remain -= n;
  return true;
}

bool col1_save_alloc_sections(ColonizeCol1Save* save, char* err, size_t err_size) {
  if (!save) {
    COL1_FAIL(err, err_size, "null save");
  }
  if (save->owned) {
    free(save->colony);
    free(save->unit);
    free(save->tribe);
    free(save->map.tile);
    free(save->map.mask);
    free(save->map.path);
    free(save->map.seen);
    save->colony = NULL;
    save->unit = NULL;
    save->tribe = NULL;
    save->map.tile = NULL;
    save->map.mask = NULL;
    save->map.path = NULL;
    save->map.seen = NULL;
    save->owned = false;
  }

  const uint16_t mw = save->head.map_size_x;
  const uint16_t mh = save->head.map_size_y;
  if (mw == 0 || mh == 0 || (size_t)mw * (size_t)mh > 256u * 256u) {
    COL1_FAIL(err, err_size, "invalid map size %ux%u", mw, mh);
  }

  save->map.width = mw;
  save->map.height = mh;
  save->map.tile_count = (size_t)mw * (size_t)mh;

  ColonizeCol1Colony* colonies = NULL;
  ColonizeCol1Unit* units = NULL;
  ColonizeCol1Tribe* tribes = NULL;
  uint8_t* tile = NULL;
  uint8_t* mask = NULL;
  uint8_t* path = NULL;
  uint8_t* seen = NULL;

  if (save->head.colony_count > 0) {
    colonies = calloc(save->head.colony_count, sizeof(ColonizeCol1Colony));
    if (!colonies) {
      goto oom;
    }
  }
  if (save->head.unit_count > 0) {
    units = calloc(save->head.unit_count, sizeof(ColonizeCol1Unit));
    if (!units) {
      goto oom;
    }
  }
  if (save->head.tribe_count > 0) {
    tribes = calloc(save->head.tribe_count, sizeof(ColonizeCol1Tribe));
    if (!tribes) {
      goto oom;
    }
  }

  tile = calloc(save->map.tile_count, 1);
  mask = calloc(save->map.tile_count, 1);
  path = calloc(save->map.tile_count, 1);
  seen = calloc(save->map.tile_count, 1);
  if (!tile || !mask || !path || !seen) {
    goto oom;
  }

  save->colony = colonies;
  save->unit = units;
  save->tribe = tribes;
  save->map.tile = tile;
  save->map.mask = mask;
  save->map.path = path;
  save->map.seen = seen;
  save->owned = true;
  if (err && err_size > 0) {
    err[0] = '\0';
  }
  return true;

oom:
  free(colonies);
  free(units);
  free(tribes);
  free(tile);
  free(mask);
  free(path);
  free(seen);
  COL1_FAIL(err, err_size, "oom allocating save sections");
}

static bool parse_from_stream(
  bool (*take)(void* ctx, void* dst, size_t n, char* err, size_t err_size, const char* what),
  void* ctx,
  ColonizeCol1Save* out,
  char* err,
  size_t err_size
) {
  col1_save_free(out);
  col1_save_init(out);

  if (!take(ctx, &out->head, sizeof(out->head), err, err_size, "head")) {
    return false;
  }
  if (!col1_save_validate_head(&out->head, -1, -1, err, err_size)) {
    return false;
  }
  if (!take(ctx, out->player, sizeof(out->player), err, err_size, "players")) {
    return false;
  }
  if (!take(ctx, out->other, sizeof(out->other), err, err_size, "other")) {
    return false;
  }

  if (!col1_save_alloc_sections(out, err, err_size)) {
    return false;
  }

  if (out->head.colony_count > 0 &&
      !take(
        ctx,
        out->colony,
        (size_t)out->head.colony_count * sizeof(ColonizeCol1Colony),
        err,
        err_size,
        "colonies"
      )) {
    return false;
  }
  if (out->head.unit_count > 0 &&
      !take(
        ctx,
        out->unit,
        (size_t)out->head.unit_count * sizeof(ColonizeCol1Unit),
        err,
        err_size,
        "units"
      )) {
    return false;
  }
  if (!take(ctx, out->nation, sizeof(out->nation), err, err_size, "nations")) {
    return false;
  }
  if (out->head.tribe_count > 0 &&
      !take(
        ctx,
        out->tribe,
        (size_t)out->head.tribe_count * sizeof(ColonizeCol1Tribe),
        err,
        err_size,
        "tribes"
      )) {
    return false;
  }
  if (!take(ctx, out->indian, sizeof(out->indian), err, err_size, "indians")) {
    return false;
  }
  if (!take(ctx, &out->stuff, sizeof(out->stuff), err, err_size, "stuff")) {
    return false;
  }
  if (!take(ctx, out->map.tile, out->map.tile_count, err, err_size, "map.tile") ||
      !take(ctx, out->map.mask, out->map.tile_count, err, err_size, "map.mask") ||
      !take(ctx, out->map.path, out->map.tile_count, err, err_size, "map.path") ||
      !take(ctx, out->map.seen, out->map.tile_count, err, err_size, "map.seen")) {
    return false;
  }
  if (!take(ctx, &out->post_map, sizeof(out->post_map), err, err_size, "post_map") ||
      !take(ctx, out->trade_route, sizeof(out->trade_route), err, err_size, "trade_routes")) {
    return false;
  }

  if (err && err_size > 0) {
    err[0] = '\0';
  }
  return true;
}

typedef struct {
  FILE* f;
} Col1FileCtx;

static bool file_take(void* ctx, void* dst, size_t n, char* err, size_t err_size, const char* what) {
  Col1FileCtx* fc = ctx;
  return read_exact(fc->f, dst, n, err, err_size, what);
}

typedef struct {
  const uint8_t* p;
  size_t remain;
} Col1MemCtx;

static bool mem_take_ctx(void* ctx, void* dst, size_t n, char* err, size_t err_size, const char* what) {
  Col1MemCtx* mc = ctx;
  return mem_take(&mc->p, &mc->remain, dst, n, err, err_size, what);
}

static bool emit_to_stream(
  bool (*put)(void* ctx, const void* src, size_t n, char* err, size_t err_size, const char* what),
  void* ctx,
  const ColonizeCol1Save* save,
  char* err,
  size_t err_size
) {
  if (!save) {
    COL1_FAIL(err, err_size, "null save");
  }
  ColonizeCol1Head head = save->head;
  col1_save_stamp_head(&head);
  if (!col1_save_validate_head(&head, -1, -1, err, err_size)) {
    return false;
  }
  if (save->head.colony_count > 0 && !save->colony) {
    COL1_FAIL(err, err_size, "colony_count>0 but colony buffer null");
  }
  if (save->head.unit_count > 0 && !save->unit) {
    COL1_FAIL(err, err_size, "unit_count>0 but unit buffer null");
  }
  if (save->head.tribe_count > 0 && !save->tribe) {
    COL1_FAIL(err, err_size, "tribe_count>0 but tribe buffer null");
  }
  if (!save->map.tile || !save->map.mask || !save->map.path || !save->map.seen) {
    COL1_FAIL(err, err_size, "map layers incomplete");
  }
  if (save->map.width != save->head.map_size_x || save->map.height != save->head.map_size_y) {
    COL1_FAIL(err, err_size, "map dimension mismatch vs head");
  }
  if (save->map.tile_count != (size_t)save->map.width * (size_t)save->map.height) {
    COL1_FAIL(err, err_size, "map tile_count mismatch");
  }

  if (!put(ctx, &head, sizeof(head), err, err_size, "head") ||
      !put(ctx, save->player, sizeof(save->player), err, err_size, "players") ||
      !put(ctx, save->other, sizeof(save->other), err, err_size, "other")) {
    return false;
  }
  if (save->head.colony_count > 0 &&
      !put(
        ctx,
        save->colony,
        (size_t)save->head.colony_count * sizeof(ColonizeCol1Colony),
        err,
        err_size,
        "colonies"
      )) {
    return false;
  }
  if (save->head.unit_count > 0 &&
      !put(
        ctx,
        save->unit,
        (size_t)save->head.unit_count * sizeof(ColonizeCol1Unit),
        err,
        err_size,
        "units"
      )) {
    return false;
  }
  if (!put(ctx, save->nation, sizeof(save->nation), err, err_size, "nations")) {
    return false;
  }
  if (save->head.tribe_count > 0 &&
      !put(
        ctx,
        save->tribe,
        (size_t)save->head.tribe_count * sizeof(ColonizeCol1Tribe),
        err,
        err_size,
        "tribes"
      )) {
    return false;
  }
  if (!put(ctx, save->indian, sizeof(save->indian), err, err_size, "indians") ||
      !put(ctx, &save->stuff, sizeof(save->stuff), err, err_size, "stuff") ||
      !put(ctx, save->map.tile, save->map.tile_count, err, err_size, "map.tile") ||
      !put(ctx, save->map.mask, save->map.tile_count, err, err_size, "map.mask") ||
      !put(ctx, save->map.path, save->map.tile_count, err, err_size, "map.path") ||
      !put(ctx, save->map.seen, save->map.tile_count, err, err_size, "map.seen") ||
      !put(ctx, &save->post_map, sizeof(save->post_map), err, err_size, "post_map") ||
      !put(ctx, save->trade_route, sizeof(save->trade_route), err, err_size, "trade_routes")) {
    return false;
  }

  if (err && err_size > 0) {
    err[0] = '\0';
  }
  return true;
}

static bool file_put(void* ctx, const void* src, size_t n, char* err, size_t err_size, const char* what) {
  Col1FileCtx* fc = ctx;
  return write_exact(fc->f, src, n, err, err_size, what);
}

typedef struct {
  uint8_t* p;
  size_t remain;
} Col1MemOutCtx;

static bool mem_put_ctx(
  void* ctx,
  const void* src,
  size_t n,
  char* err,
  size_t err_size,
  const char* what
) {
  Col1MemOutCtx* mc = ctx;
  return mem_put(&mc->p, &mc->remain, src, n, err, err_size, what);
}

bool col1_save_read_file(const char* path, ColonizeCol1Save* out, char* err, size_t err_size) {
  if (!path || !out) {
    COL1_FAIL(err, err_size, "bad args");
  }
  if (!col1_save_check_layout(err, err_size)) {
    return false;
  }

  FILE* f = fopen(path, "rb");
  if (!f) {
    COL1_FAIL(err, err_size, "cannot open %s", path);
  }

  /* Size check: read counts from prefix then verify file length. */
  ColonizeCol1Head head_probe;
  if (fread(&head_probe, sizeof(head_probe), 1, f) != 1) {
    fclose(f);
    COL1_FAIL(err, err_size, "cannot read head from %s", path);
  }
  if (!col1_save_validate_head(&head_probe, -1, -1, err, err_size)) {
    fclose(f);
    return false;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    COL1_FAIL(err, err_size, "seek end failed");
  }
  long file_size = ftell(f);
  if (file_size < 0) {
    fclose(f);
    COL1_FAIL(err, err_size, "ftell failed");
  }
  const size_t expect = col1_save_expected_size_counts(
    head_probe.map_size_x,
    head_probe.map_size_y,
    head_probe.colony_count,
    head_probe.unit_count,
    head_probe.tribe_count
  );
  if ((size_t)file_size != expect) {
    fclose(f);
    COL1_FAIL(
      err,
      err_size,
      "size mismatch in %s: file=%ld expected=%zu (map %ux%u colonies=%u units=%u tribes=%u)",
      path,
      file_size,
      expect,
      head_probe.map_size_x,
      head_probe.map_size_y,
      head_probe.colony_count,
      head_probe.unit_count,
      head_probe.tribe_count
    );
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    COL1_FAIL(err, err_size, "seek start failed");
  }

  Col1FileCtx ctx = {.f = f};
  const bool ok = parse_from_stream(file_take, &ctx, out, err, err_size);
  fclose(f);
  if (ok) {
    diag_info(
      "col1_save_read_file %s: turn=%u year=%u colonies=%u units=%u tribes=%u map=%ux%u",
      path,
      out->head.turn,
      out->head.year,
      out->head.colony_count,
      out->head.unit_count,
      out->head.tribe_count,
      out->head.map_size_x,
      out->head.map_size_y
    );
  }
  return ok;
}

bool col1_save_write_file(const char* path, const ColonizeCol1Save* save, char* err, size_t err_size) {
  if (!path || !save) {
    COL1_FAIL(err, err_size, "bad args");
  }
  if (!col1_save_check_layout(err, err_size)) {
    return false;
  }

  ColonizeCol1Save* mut = (ColonizeCol1Save*)save;
  uint16_t saved_last[COLONIZE_COL1_NATION_COUNT];
  uint8_t saved_pad21[COLONIZE_COL1_NATION_COUNT];
  founding_fathers_stash_pools_into_col1(mut, saved_last, saved_pad21);

  FILE* f = fopen(path, "wb");
  if (!f) {
    founding_fathers_restore_col1_last_turn(mut, saved_last, saved_pad21);
    COL1_FAIL(err, err_size, "cannot open %s for write", path);
  }
  Col1FileCtx ctx = {.f = f};
  const bool ok = emit_to_stream(file_put, &ctx, save, err, err_size);
  if (fclose(f) != 0) {
    founding_fathers_restore_col1_last_turn(mut, saved_last, saved_pad21);
    COL1_FAIL(err, err_size, "fclose failed for %s", path);
  }
  founding_fathers_restore_col1_last_turn(mut, saved_last, saved_pad21);
  if (!ok) {
    return false;
  }
  diag_info("col1_save_write_file %s (%zu bytes)", path, col1_save_expected_size(save));
  if (err && err_size > 0) {
    err[0] = '\0';
  }
  return true;
}

bool col1_save_read_memory(
  const uint8_t* data,
  size_t size,
  ColonizeCol1Save* out,
  char* err,
  size_t err_size
) {
  if (!data || !out) {
    COL1_FAIL(err, err_size, "bad args");
  }
  if (!col1_save_check_layout(err, err_size)) {
    return false;
  }
  if (size < sizeof(ColonizeCol1Head)) {
    COL1_FAIL(err, err_size, "buffer too small for head");
  }

  ColonizeCol1Head head_probe;
  memcpy(&head_probe, data, sizeof(head_probe));
  if (!col1_save_validate_head(&head_probe, -1, -1, err, err_size)) {
    return false;
  }
  const size_t expect = col1_save_expected_size_counts(
    head_probe.map_size_x,
    head_probe.map_size_y,
    head_probe.colony_count,
    head_probe.unit_count,
    head_probe.tribe_count
  );
  if (size != expect) {
    COL1_FAIL(
      err,
      err_size,
      "size mismatch: have %zu expected %zu",
      size,
      expect
    );
  }

  Col1MemCtx ctx = {.p = data, .remain = size};
  return parse_from_stream(mem_take_ctx, &ctx, out, err, err_size);
}

bool col1_save_write_memory(
  const ColonizeCol1Save* save,
  uint8_t** out_data,
  size_t* out_size,
  char* err,
  size_t err_size
) {
  if (!save || !out_data || !out_size) {
    COL1_FAIL(err, err_size, "bad args");
  }
  if (!col1_save_check_layout(err, err_size)) {
    return false;
  }

  ColonizeCol1Save* mut = (ColonizeCol1Save*)save;
  uint16_t saved_last[COLONIZE_COL1_NATION_COUNT];
  uint8_t saved_pad21[COLONIZE_COL1_NATION_COUNT];
  founding_fathers_stash_pools_into_col1(mut, saved_last, saved_pad21);

  const size_t need = col1_save_expected_size(save);
  uint8_t* buf = malloc(need);
  if (!buf) {
    founding_fathers_restore_col1_last_turn(mut, saved_last, saved_pad21);
    COL1_FAIL(err, err_size, "oom output buffer");
  }

  Col1MemOutCtx ctx = {.p = buf, .remain = need};
  if (!emit_to_stream(mem_put_ctx, &ctx, save, err, err_size)) {
    founding_fathers_restore_col1_last_turn(mut, saved_last, saved_pad21);
    free(buf);
    return false;
  }
  if (ctx.remain != 0) {
    founding_fathers_restore_col1_last_turn(mut, saved_last, saved_pad21);
    free(buf);
    COL1_FAIL(err, err_size, "internal size mismatch (%zu bytes left)", ctx.remain);
  }

  founding_fathers_restore_col1_last_turn(mut, saved_last, saved_pad21);

  *out_data = buf;
  *out_size = need;
  if (err && err_size > 0) {
    err[0] = '\0';
  }
  return true;
}
