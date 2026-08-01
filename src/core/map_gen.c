#include "core/map_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

/* Terrain indices (bits 0–4); see docs/assets.md. */
#define T_TUNDRA 0
#define T_DESERT 1
#define T_PLAINS 2
#define T_PRAIRIE 3
#define T_GRASSLAND 4
#define T_SAVANNAH 5
#define T_MARSH 6
#define T_SWAMP 7
#define T_FOREST_BIT 8 /* OR onto 0–7 */
#define T_ARCTIC 24
#define T_OCEAN 25
#define T_HIGH_SEAS 26
#define F_HILL 0x20
#define F_RIVER 0x40
#define F_MOUNTAIN 0x80

typedef struct MapGenRng {
  uint32_t state;
} MapGenRng;

static void rng_seed(MapGenRng* rng, uint32_t seed) {
  rng->state = seed ? seed : 1u;
}

/* DOS-like linear congruential; FUN_281f_04d4 is a wrapped rand — approximate. */
static uint32_t rng_next(MapGenRng* rng) {
  rng->state = rng->state * 1103515245u + 12345u;
  return (rng->state >> 16) & 0x7fffu;
}

static int rng_range(MapGenRng* rng, int lo, int hi_inclusive) {
  if (hi_inclusive <= lo) {
    return lo;
  }
  return lo + (int)(rng_next(rng) % (uint32_t)(hi_inclusive - lo + 1));
}

static int clamp_i(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

void map_gen_params_random(MapGenParams* out, uint32_t seed) {
  if (!out) {
    return;
  }
  MapGenRng rng;
  rng_seed(&rng, seed ? seed : 1u);
  out->seed = seed ? seed : 1u;
  out->land_mass = rng_range(&rng, 0, 2);
  out->land_form = rng_range(&rng, 0, 2);
  out->temperature = rng_range(&rng, 0, 2);
  out->climate = rng_range(&rng, 0, 2);
  out->forest_extra = rng_range(&rng, 0, 2);
}

static int idx(int x, int y, int w) {
  return y * w + x;
}

static bool in_bounds(int x, int y, int w, int h) {
  return x >= 0 && y >= 0 && x < w && y < h;
}

/* Land budget from FUN_684c_08c0: (form + mass + 1) * 0x140. */
static int land_budget(const MapGenParams* p) {
  const int mass = clamp_i(p->land_mass, 0, 2);
  const int form = clamp_i(p->land_form, 0, 2);
  return (form + mass + 1) * 0x140;
}

static int count_land(const uint8_t* mask, int n) {
  int c = 0;
  for (int i = 0; i < n; ++i) {
    if (mask[i]) {
      c++;
    }
  }
  return c;
}

/*
 * Grow a land blob from (sx,sy). Style depends on land_form:
 *  0 archipelago — short wanders
 *  1 normal — medium
 *  2 continents — thicker radial growth
 * Approximates FUN_684c_02a8 / 0116 / 085a / 021c.
 */
static void grow_blob(
  uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  int sx,
  int sy,
  int target_add,
  int land_form
) {
  static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
  static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
  int x = sx;
  int y = sy;
  int added = 0;
  int steps = target_add * (land_form == 0 ? 3 : (land_form == 1 ? 4 : 6));
  if (steps < target_add) {
    steps = target_add;
  }

  if (!in_bounds(x, y, w, h) || x < 2 || y < 2 || x >= w - 2 || y >= h - 2) {
    return;
  }
  if (!mask[idx(x, y, w)]) {
    mask[idx(x, y, w)] = 1;
    added++;
  }

  for (int s = 0; s < steps && added < target_add; ++s) {
    int n = land_form >= 2 ? rng_range(rng, 0, 3) : rng_range(rng, 0, 7);
    int nx = x + dx[n];
    int ny = y + dy[n];
    /* Stay inside margins (DOS 0x2d20 / 0x2d1e style). */
    if (nx < 3 || ny < 2 || nx >= w - 3 || ny >= h - 2) {
      x = clamp_i(x + rng_range(rng, -2, 2), 3, w - 4);
      y = clamp_i(y + rng_range(rng, -2, 2), 2, h - 3);
      continue;
    }
    x = nx;
    y = ny;
    if (!mask[idx(x, y, w)]) {
      mask[idx(x, y, w)] = 1;
      added++;
      /* Continents: thicken */
      if (land_form >= 2) {
        for (int k = 0; k < 4 && added < target_add; ++k) {
          int tx = x + dx[k];
          int ty = y + dy[k];
          if (in_bounds(tx, ty, w, h) && tx >= 3 && ty >= 2 && tx < w - 3 && ty < h - 2 &&
              !mask[idx(tx, ty, w)]) {
            mask[idx(tx, ty, w)] = 1;
            added++;
          }
        }
      }
    }
  }
}

static void generate_land_mask(
  uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenParams* params
) {
  const int budget = land_budget(params);
  const int form = clamp_i(params->land_form, 0, 2);
  memset(mask, 0, (size_t)w * (size_t)h);

  /* Prefer starting on one side of the map (DOS margin flip). */
  int prefer_north = rng_range(rng, 0, 1) == 0;

  int attempts = 0;
  while (count_land(mask, w * h) < budget && attempts < 200) {
    attempts++;
    int sx = rng_range(rng, 7, w - 8);
    int sy = prefer_north ? rng_range(rng, 3, h / 2) : rng_range(rng, h / 2, h - 4);
    int remaining = budget - count_land(mask, w * h);
    int chunk = remaining;
    if (form == 0) {
      chunk = rng_range(rng, 20, 80);
    } else if (form == 1) {
      chunk = rng_range(rng, 60, 200);
    } else {
      chunk = rng_range(rng, 150, 400);
    }
    if (chunk > remaining) {
      chunk = remaining;
    }
    grow_blob(mask, w, h, rng, sx, sy, chunk, form);
  }

  /* Extra masses when land_form > 0 (DOS local_2c loop). */
  if (form > 0) {
    int extras = form + rng_range(rng, 0, 2);
    for (int e = 0; e < extras; ++e) {
      int sx = rng_range(rng, 7, w - 8);
      int sy = rng_range(rng, 3, h - 4);
      grow_blob(mask, w, h, rng, sx, sy, rng_range(rng, 30, 120), form);
    }
  }
}

/* Cardinal land mask bits: N=1 E=2 S=4 W=8. Awkward diagonals 6 (E+S) and 9 (N+W)
 * get cleared — FUN_684c_08c0 local_34[2]==6||9. */
static void cleanup_diagonal_land(uint8_t* mask, int w, int h) {
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      if (!mask[idx(x, y, w)]) {
        continue;
      }
      int m = 0;
      if (mask[idx(x, y - 1, w)]) {
        m |= 1;
      }
      if (mask[idx(x + 1, y, w)]) {
        m |= 2;
      }
      if (mask[idx(x, y + 1, w)]) {
        m |= 4;
      }
      if (mask[idx(x - 1, y, w)]) {
        m |= 8;
      }
      if (m == 6 || m == 9) {
        mask[idx(x, y, w)] = 0;
      }
    }
  }
}

/* Base terrain from latitude band + temperature shift (FUN_684c_08c0 paint). */
static uint8_t latitude_terrain(int y, int h, int temperature, MapGenRng* rng) {
  /* Distance from equator (mid-map), shifted by temperature. */
  const int mid = h / 2;
  int jitter = rng_range(rng, 0, 3);
  int dist = mid - y;
  if (dist < 0) {
    dist = -dist;
  }
  dist = dist + jitter;
  dist = dist + (1 - temperature) * 2;
  if (dist < 0) {
    dist = 0;
  }
  const int band = dist >> 2; /* /4 as in decomp iVar12 >> 2 */

  /* Near equator (small dist): tropical; near poles: arctic/tundra. */
  if (band >= 6) {
    return T_ARCTIC;
  }
  switch (band) {
    case 0:
      return (uint8_t)rng_range(rng, T_SAVANNAH, T_SWAMP);
    case 1:
      return (rng_next(rng) & 1) ? T_GRASSLAND : T_SAVANNAH;
    case 2:
      return (rng_next(rng) & 1) ? T_PLAINS : T_PRAIRIE;
    case 3:
      return (rng_next(rng) & 1) ? T_PLAINS : T_DESERT;
    case 4:
      return (rng_next(rng) & 1) ? T_TUNDRA : T_DESERT;
    case 5:
    default:
      return (rng_next(rng) & 1) ? T_TUNDRA : T_ARCTIC;
  }
}

static void paint_terrain(
  uint8_t* terrain,
  const uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenParams* params
) {
  const int temp = clamp_i(params->temperature, 0, 2);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int i = idx(x, y, w);
      if (!mask[i]) {
        /* High seas near map edges (east/west), ocean otherwise. */
        if (x <= 1 || x >= w - 2) {
          terrain[i] = T_HIGH_SEAS;
        } else {
          terrain[i] = T_OCEAN;
        }
        continue;
      }
      terrain[i] = latitude_terrain(y, h, temp, rng);
      if (terrain[i] == T_ARCTIC) {
        /* Keep arctic as-is */
      }
    }
  }
}

static void add_forests(
  uint8_t* terrain,
  const uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenParams* params
) {
  /* Forest pass count ~ (forest_extra + 1) * 800 attempts (DOS 0x1e86). */
  const int climate = clamp_i(params->climate, 0, 2);
  const int extra = clamp_i(params->forest_extra, 0, 2);
  int attempts = (extra + 1) * 800;
  /* Arid fewer forests, wet more. */
  attempts = attempts * (climate + 1) / 2;
  if (attempts < 200) {
    attempts = 200;
  }

  for (int a = 0; a < attempts; ++a) {
    int x = rng_range(rng, 1, w - 2);
    int y = rng_range(rng, 1, h - 2);
    int i = idx(x, y, w);
    if (!mask[i]) {
      continue;
    }
    uint8_t t = terrain[i] & 0x1f;
    if (t >= 8 || t == T_ARCTIC || t == T_OCEAN || t == T_HIGH_SEAS) {
      continue;
    }
    /* Desert less likely to forest unless wet. */
    if (t == T_DESERT && climate < 2 && (rng_next(rng) % 3) != 0) {
      continue;
    }
    if (climate == 0 && (rng_next(rng) % 3) != 0) {
      continue;
    }
    terrain[i] = (uint8_t)(t | T_FOREST_BIT);
  }
}

static void add_hills_mountains(
  uint8_t* terrain,
  const uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenParams* params
) {
  (void)params;
  const int attempts = w * h / 8;
  for (int a = 0; a < attempts; ++a) {
    int x = rng_range(rng, 2, w - 3);
    int y = rng_range(rng, 2, h - 3);
    int i = idx(x, y, w);
    if (!mask[i]) {
      continue;
    }
    uint8_t base = terrain[i] & 0x1f;
    if (base >= T_ARCTIC) {
      continue;
    }
    if ((rng_next(rng) % 5) == 0) {
      terrain[i] = (uint8_t)((terrain[i] & 0x1f) | F_HILL | F_MOUNTAIN);
    } else if ((rng_next(rng) % 3) == 0) {
      terrain[i] = (uint8_t)((terrain[i] & 0x1f) | F_HILL);
    }
  }
}

static void add_rivers(
  uint8_t* terrain,
  const uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenParams* params
) {
  /* River density from climate: (climate + mass + 2) * 8 style threshold. */
  const int climate = clamp_i(params->climate, 0, 2);
  const int mass = clamp_i(params->land_mass, 0, 2);
  int rivers = (climate + mass + 2) * 2;
  if (rivers < 4) {
    rivers = 4;
  }
  if (rivers > 24) {
    rivers = 24;
  }

  static const int dx[4] = {1, -1, 0, 0};
  static const int dy[4] = {0, 0, 1, -1};

  for (int r = 0; r < rivers; ++r) {
    int x = rng_range(rng, 4, w - 5);
    int y = rng_range(rng, 4, h - 5);
    if (!mask[idx(x, y, w)]) {
      continue;
    }
    int len = rng_range(rng, 8, 28);
    int dir = rng_range(rng, 0, 3);
    for (int s = 0; s < len; ++s) {
      if (!in_bounds(x, y, w, h) || !mask[idx(x, y, w)]) {
        break;
      }
      uint8_t t = terrain[idx(x, y, w)];
      uint8_t base = t & 0x1f;
      if (base < T_ARCTIC) {
        uint8_t feat = (uint8_t)(t & (F_HILL | F_MOUNTAIN));
        uint8_t river = F_RIVER;
        if ((rng_next(rng) % 4) == 0) {
          river = (uint8_t)(F_RIVER | F_MOUNTAIN); /* major river bit combo */
        }
        terrain[idx(x, y, w)] = (uint8_t)(base | feat | river);
      }
      if ((rng_next(rng) % 5) == 0) {
        dir = rng_range(rng, 0, 3);
      }
      x += dx[dir];
      y += dy[dir];
    }
  }
}

bool map_generate(ColonizeWorldMap* out, const MapGenParams* params, char* err, size_t err_size) {
  if (!out || !params) {
    if (err && err_size) {
      snprintf(err, err_size, "map_generate bad args");
    }
    return false;
  }

  MapGenParams p = *params;
  p.land_mass = clamp_i(p.land_mass, 0, 2);
  p.land_form = clamp_i(p.land_form, 0, 2);
  p.temperature = clamp_i(p.temperature, 0, 2);
  p.climate = clamp_i(p.climate, 0, 2);
  p.forest_extra = clamp_i(p.forest_extra, 0, 2);
  if (p.seed == 0) {
    p.seed = 1;
  }

  if (!map_alloc(out, MAP_GEN_WIDTH, MAP_GEN_HEIGHT, err, err_size)) {
    return false;
  }

  const int w = MAP_GEN_WIDTH;
  const int h = MAP_GEN_HEIGHT;
  const int n = w * h;
  uint8_t* mask = (uint8_t*)calloc((size_t)n, 1);
  if (!mask) {
    map_free(out);
    if (err && err_size) {
      snprintf(err, err_size, "map_generate oom");
    }
    return false;
  }

  MapGenRng rng;
  rng_seed(&rng, p.seed);

  generate_land_mask(mask, w, h, &rng, &p);
  cleanup_diagonal_land(mask, w, h);
  /* Second cleanup pass for residual corners. */
  cleanup_diagonal_land(mask, w, h);

  paint_terrain(out->terrain, mask, w, h, &rng, &p);
  add_forests(out->terrain, mask, w, h, &rng, &p);
  add_hills_mountains(out->terrain, mask, w, h, &rng, &p);
  add_rivers(out->terrain, mask, w, h, &rng, &p);

  const int land = count_land(mask, n);
  diag_info(
    "map_generate seed=%u mass=%d form=%d temp=%d clim=%d land=%d/%d budget=%d",
    p.seed,
    p.land_mass,
    p.land_form,
    p.temperature,
    p.climate,
    land,
    n,
    land_budget(&p)
  );

  free(mask);
  return true;
}

static bool is_coastal_land(const ColonizeWorldMap* map, int x, int y) {
  if (!map_tile_is_land(map, x, y)) {
    return false;
  }
  static const int dx[4] = {1, -1, 0, 0};
  static const int dy[4] = {0, 0, 1, -1};
  for (int i = 0; i < 4; ++i) {
    if (map_tile_is_water(map, x + dx[i], y + dy[i])) {
      return true;
    }
  }
  return false;
}

bool map_gen_pick_start(
  const ColonizeWorldMap* map,
  int nation,
  int avoid_x,
  int avoid_y,
  int min_dist,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y || map->width == 0 || map->height == 0) {
    return false;
  }
  if (nation < 0 || nation > 3) {
    nation = 0;
  }

  /* Prefer latitude bands: Eng/Fr temperate, Spain warmer (lower y = north). */
  int y0, y1;
  switch (nation) {
    case 0: /* England */
      y0 = map->height / 5;
      y1 = map->height / 2;
      break;
    case 1: /* France */
      y0 = map->height / 4;
      y1 = (map->height * 3) / 5;
      break;
    case 2: /* Spain */
      y0 = map->height / 3;
      y1 = (map->height * 3) / 4;
      break;
    default: /* Netherlands */
      y0 = map->height / 5;
      y1 = (map->height * 2) / 3;
      break;
  }

  int best_x = -1, best_y = -1;
  int best_score = -1;
  for (int y = y0; y < y1; ++y) {
    for (int x = 2; x < map->width - 2; ++x) {
      if (!is_coastal_land(map, x, y)) {
        continue;
      }
      if (avoid_x >= 0 && avoid_y >= 0) {
        int dx = x - avoid_x;
        int dy = y - avoid_y;
        if (dx * dx + dy * dy < min_dist * min_dist) {
          continue;
        }
      }
      /* Prefer eastern coasts (New World approach from Atlantic). */
      int score = x * 2 - abs(y - (y0 + y1) / 2);
      if (score > best_score) {
        best_score = score;
        best_x = x;
        best_y = y;
      }
    }
  }

  if (best_x < 0) {
    /* Fallback: any coastal land. */
    for (int y = 2; y < map->height - 2; ++y) {
      for (int x = 2; x < map->width - 2; ++x) {
        if (is_coastal_land(map, x, y)) {
          *out_x = x;
          *out_y = y;
          return true;
        }
      }
    }
    /* Any land. */
    for (int y = 0; y < map->height; ++y) {
      for (int x = 0; x < map->width; ++x) {
        if (map_tile_is_land(map, x, y)) {
          *out_x = x;
          *out_y = y;
          return true;
        }
      }
    }
    return false;
  }

  *out_x = best_x;
  *out_y = best_y;
  return true;
}
