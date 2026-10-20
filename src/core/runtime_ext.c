#include "core/runtime_ext.h"

#include <stdlib.h>
#include <string.h>

#include "core/ai.h"
#include "core/bytes.h"
#include "core/ai_euro_internal.h"
#include "core/ai_goals.h"
#include "core/colony.h"
#include "core/turn.h"
#include "core/turn_internal.h"
#include "core/units.h"
#include "core/units_cargo.h"

/* Record ids. Bump a record's version when its layout changes; readers of an
 * unknown version skip the record (the table then stays reset). */
enum {
  RT_RNG = 1,          /* v1: u32 lcg state, u32 reseed word, i32 DS:0x83a6 */
  RT_EURO_STANCE = 2,  /* v1: 4x16 stance (DS:0x9870), 4x16 rival strength (DS:0x9e98) */
  RT_UNIT_TAIL = 3,    /* v1: per compacted DOS slot: u16 index, valid, goto_x, goto_y, facing, raw_valid, raw[10] */
  RT_AI_GOALS = 4,     /* v1: ai_goals_runtime_export */
  RT_TURN = 5,         /* v1: rank cache[4] (DS:0x917c), profession census [4][32] (DS:0x9fb8) */
  RT_FOG = 6,          /* v1: coarse fog plane DS:0x9faa, 0x10e bytes */
  RT_EURO_MISC = 7,    /* v1: 0a60 masks mil/found [4] u16, work_registered [4] i16, explorers[16],
                        *     village_visited[128], ship_pressure[4] {u8,u8,i32,i32}, sticky unit/count i32 */
  RT_COLONY_PLOTS = 8, /* retired: per pool slot, which a load compacts (no longer read) */
  RT_PATH = 9,         /* v1: units_path_state_export (DS:0x1dd2 + flood cache) */
  RT_COLONY_PLOTS_REC = 10, /* v1: u32 plot_was_worked_mask per colony in record order (26e4 carry) */
  RT_HOLD_AMOUNT = 11, /* v1: i32 DS:0x8dc4 units_last_hold_amount (last 317c slot amount) */
  RT_LCR_COUNTS = 12,  /* v1: u8 DS:0x1dc6 rumours explored, u8 DS:0x1dc7 Cibola count */
};

#define RT_UNIT_TAIL_ENTRY 17
#define RT_TURN_SIZE (4 + COLONIZE_COL1_NATION_COUNT * 32)
#define RT_EURO_MISC_SIZE (8 + 8 + 8 + 16 + AI_20E6_VILLAGE_MAX + 4 * 10 + 8)

static ColonizeDosRng* s_rng;
static uint32_t* s_seed_word;
static bool s_rng_restored;

void runtime_ext_set_rng(ColonizeDosRng* rng, uint32_t* seed_word) {
  s_rng = rng;
  s_seed_word = seed_word;
}

bool runtime_ext_rng_restored(void) {
  return s_rng_restored;
}

/* ---- little-endian record writer ---- */
typedef struct {
  uint8_t* buf;
  size_t len, cap;
  bool oom;
} RtBuf;

static void rt_bytes(RtBuf* b, const void* p, size_t n) {
  if (b->oom) return;
  if (b->len + n > b->cap) {
    size_t cap = b->cap ? b->cap * 2 : 1024;
    while (cap < b->len + n) cap *= 2;
    uint8_t* nb = realloc(b->buf, cap);
    if (!nb) {
      b->oom = true;
      return;
    }
    b->buf = nb;
    b->cap = cap;
  }
  memcpy(b->buf + b->len, p, n);
  b->len += n;
}
static void rt_u16(RtBuf* b, uint32_t v) {
  const uint8_t p[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
  rt_bytes(b, p, 2);
}
static void rt_u32(RtBuf* b, uint32_t v) {
  const uint8_t p[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
  rt_bytes(b, p, 4);
}
static void rt_record_head(RtBuf* b, int id, int version, size_t n) {
  rt_u16(b, (uint32_t)id);
  rt_u16(b, (uint32_t)version);
  rt_u32(b, (uint32_t)n);
}
static void rt_record(RtBuf* b, int id, int version, const void* p, size_t n) {
  rt_record_head(b, id, version, n);
  rt_bytes(b, p, n);
}
static uint8_t* wr_u16(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  return p + 2;
}
static uint8_t* wr_u32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
  return p + 4;
}

static void euro_misc_export(uint8_t out[RT_EURO_MISC_SIZE]) {
  uint8_t* p = out;
  for (int n = 0; n < 4; ++n) p = wr_u16(p, ai_euro_s_0a60_mask_mil[n]);
  for (int n = 0; n < 4; ++n) p = wr_u16(p, ai_euro_s_0a60_mask_found[n]);
  for (int n = 0; n < 4; ++n) p = wr_u16(p, (uint16_t)ai_euro_s_0a60_work_registered[n]);
  memcpy(p, ai_euro_s_20e6_explorers, 16);
  p += 16;
  memcpy(p, ai_euro_s_20e6_village_visited, AI_20E6_VILLAGE_MAX);
  p += AI_20E6_VILLAGE_MAX;
  for (int n = 0; n < 4; ++n) {
    *p++ = ai_euro_s_ship_pressure[n].frigate_colonies;
    *p++ = ai_euro_s_ship_pressure[n].other_colonies;
    p = wr_u32(p, (uint32_t)ai_euro_s_ship_pressure[n].frigate_pop);
    p = wr_u32(p, (uint32_t)ai_euro_s_ship_pressure[n].other_pop);
  }
  int unit, count;
  ai_euro_sticky_get(&unit, &count);
  p = wr_u32(p, (uint32_t)unit);
  p = wr_u32(p, (uint32_t)count);
}

static void euro_misc_import(const uint8_t in[RT_EURO_MISC_SIZE]) {
  const uint8_t* p = in;
  for (int n = 0; n < 4; ++n, p += 2) ai_euro_s_0a60_mask_mil[n] = (uint16_t)rd_u16_le(p);
  for (int n = 0; n < 4; ++n, p += 2) ai_euro_s_0a60_mask_found[n] = (uint16_t)rd_u16_le(p);
  for (int n = 0; n < 4; ++n, p += 2) ai_euro_s_0a60_work_registered[n] = (int16_t)rd_u16_le(p);
  memcpy(ai_euro_s_20e6_explorers, p, 16);
  p += 16;
  memcpy(ai_euro_s_20e6_village_visited, p, AI_20E6_VILLAGE_MAX);
  p += AI_20E6_VILLAGE_MAX;
  for (int n = 0; n < 4; ++n) {
    ai_euro_s_ship_pressure[n].frigate_colonies = *p++;
    ai_euro_s_ship_pressure[n].other_colonies = *p++;
    ai_euro_s_ship_pressure[n].frigate_pop = (int)rd_u32_le(p);
    p += 4;
    ai_euro_s_ship_pressure[n].other_pop = (int)rd_u32_le(p);
    p += 4;
  }
  ai_euro_sticky_set((int)rd_u32_le(p), (int)rd_u32_le(p + 4));
}

uint8_t* runtime_ext_serialize(
  const ColonizeUnitPool* units, const ColonizeColonyPool* colonies, size_t* out_size
) {
  RtBuf b = {0};
  if (s_rng && s_seed_word) {
    uint8_t p[12];
    wr_u32(wr_u32(wr_u32(p, s_rng->state), *s_seed_word), (uint32_t)dos_rng_get_83a6());
    rt_record(&b, RT_RNG, 1, p, sizeof p);
  }
  {
    uint8_t p[AI_EURO_STANCE_TABLES_SIZE];
    ai_euro_stance_tables_export(p);
    rt_record(&b, RT_EURO_STANCE, 1, p, sizeof p);
  }
  if (units) {
    size_t n = 0;
    for (int i = 0; i < COLONIZE_UNITS_DOS_MAX; ++i) n += units->dos_tail[i].valid ? 1 : 0;
    rt_record_head(&b, RT_UNIT_TAIL, 1, n * RT_UNIT_TAIL_ENTRY);
    for (int i = 0; i < COLONIZE_UNITS_DOS_MAX; ++i) {
      if (!units->dos_tail[i].valid) continue;
      uint8_t p[RT_UNIT_TAIL_ENTRY];
      wr_u16(p, (uint32_t)i);
      p[2] = units->dos_tail[i].valid;
      p[3] = units->dos_tail[i].goto_x;
      p[4] = units->dos_tail[i].goto_y;
      p[5] = (uint8_t)units->dos_tail[i].facing;
      p[6] = units->dos_tail[i].raw_valid;
      memcpy(p + 7, units->dos_tail[i].raw, 10);
      rt_bytes(&b, p, sizeof p);
    }
  }
  {
    uint8_t p[AI_GOALS_RUNTIME_SIZE];
    ai_goals_runtime_export(p);
    rt_record(&b, RT_AI_GOALS, 1, p, sizeof p);
  }
  {
    uint8_t p[RT_TURN_SIZE];
    turn_rank_cache_export(p);
    memcpy(p + 4, turn_prof_census, sizeof turn_prof_census);
    rt_record(&b, RT_TURN, 1, p, sizeof p);
  }
  {
    uint8_t p[AI_COARSE_FOG_BYTES];
    ai_coarse_fog_export(p);
    rt_record(&b, RT_FOG, 1, p, sizeof p);
  }
  {
    uint8_t p[RT_EURO_MISC_SIZE];
    euro_misc_export(p);
    rt_record(&b, RT_EURO_MISC, 1, p, sizeof p);
  }
  if (colonies) {
    /* Record order = active colonies in slot order (the save drops holes). */
    uint8_t p[COLONIZE_COLONIES_MAX * 4];
    int n = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      if (colonies->colonies[i].active) wr_u32(p + n++ * 4, colonies->colonies[i].plot_was_worked_mask);
    }
    rt_record(&b, RT_COLONY_PLOTS_REC, 1, p, (size_t)n * 4);
  }
  {
    uint8_t p[4];
    wr_u32(p, (uint32_t)units_last_hold_amount);
    rt_record(&b, RT_HOLD_AMOUNT, 1, p, sizeof p);
  }
  {
    uint8_t p[2];
    units_lcr_counters_export(p);
    rt_record(&b, RT_LCR_COUNTS, 1, p, sizeof p);
  }
  {
    uint8_t p[UNITS_PATH_STATE_SIZE];
    units_path_state_export(p);
    rt_record(&b, RT_PATH, 1, p, sizeof p);
  }
  if (b.oom) {
    free(b.buf);
    *out_size = 0;
    return NULL;
  }
  *out_size = b.len;
  return b.buf;
}

void runtime_ext_deserialize(
  ColonizeUnitPool* units, ColonizeColonyPool* colonies, const ColonizeWorldMap* map,
  const uint8_t* data, size_t size
) {
  s_rng_restored = false;
  if (!data) return;
  size_t off = 0;
  while (off + 8 <= size) {
    const uint32_t id = rd_u16_le(data + off);
    const uint32_t ver = rd_u16_le(data + off + 2);
    const uint32_t len = rd_u32_le(data + off + 4);
    off += 8;
    if (len > size - off) return; /* truncated: stop, keep what applied */
    const uint8_t* p = data + off;
    off += len;
    if (ver != 1) continue;
    switch (id) {
      case RT_RNG:
        if (len == 12 && s_rng && s_seed_word) {
          s_rng->state = rd_u32_le(p);
          *s_seed_word = rd_u32_le(p + 4);
          dos_rng_set_83a6((int32_t)rd_u32_le(p + 8));
          s_rng_restored = true;
        }
        break;
      case RT_EURO_STANCE:
        if (len == AI_EURO_STANCE_TABLES_SIZE) ai_euro_stance_tables_import(p);
        break;
      case RT_UNIT_TAIL:
        if (units && len % RT_UNIT_TAIL_ENTRY == 0) {
          memset(units->dos_tail, 0, sizeof units->dos_tail);
          for (size_t k = 0; k + RT_UNIT_TAIL_ENTRY <= len; k += RT_UNIT_TAIL_ENTRY) {
            const uint8_t* e = p + k;
            const uint32_t i = rd_u16_le(e);
            if (i >= COLONIZE_UNITS_DOS_MAX) continue;
            units->dos_tail[i].valid = e[2];
            units->dos_tail[i].goto_x = e[3];
            units->dos_tail[i].goto_y = e[4];
            units->dos_tail[i].facing = (int8_t)e[5];
            units->dos_tail[i].raw_valid = e[6];
            memcpy(units->dos_tail[i].raw, e + 7, 10);
          }
        }
        break;
      case RT_AI_GOALS:
        if (len == AI_GOALS_RUNTIME_SIZE) ai_goals_runtime_import(p);
        break;
      case RT_TURN:
        if (len == RT_TURN_SIZE) {
          turn_rank_cache_import(p);
          memcpy(turn_prof_census, p + 4, sizeof turn_prof_census);
        }
        break;
      case RT_FOG:
        if (len == AI_COARSE_FOG_BYTES) ai_coarse_fog_import(p);
        break;
      case RT_EURO_MISC:
        if (len == RT_EURO_MISC_SIZE) euro_misc_import(p);
        break;
      case RT_COLONY_PLOTS_REC:
        if (colonies && len % 4 == 0) {
          size_t k = 0;
          for (int i = 0; i < COLONIZE_COLONIES_MAX && k + 4 <= len; ++i) {
            if (!colonies->colonies[i].active) continue;
            colonies->colonies[i].plot_was_worked_mask = rd_u32_le(p + k);
            k += 4;
          }
        }
        break;
      case RT_HOLD_AMOUNT:
        if (len == 4) units_last_hold_amount = (int)rd_u32_le(p);
        break;
      case RT_LCR_COUNTS:
        if (len == 2) units_lcr_counters_import(p);
        break;
      case RT_PATH:
        if (len == UNITS_PATH_STATE_SIZE) units_path_state_import(p, map);
        break;
      default:
        break;
    }
  }
}
