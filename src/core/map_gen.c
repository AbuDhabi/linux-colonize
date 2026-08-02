#include "core/map_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/dos_rng.h"
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

typedef ColonizeDosRng MapGenRng;

static void rng_seed(MapGenRng* rng, uint32_t seed) {
  dos_rng_seed(rng, seed ? seed : 1u);
}

static uint32_t rng_next(MapGenRng* rng) {
  return dos_rng_next(rng);
}

static int rng_range(MapGenRng* rng, int lo, int hi_inclusive) {
  return dos_rng_range(rng, lo, hi_inclusive);
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
  MapGenRng local;
  MapGenRng* rng = &local;
  if (out->rng) {
    rng = out->rng;
  } else {
    rng_seed(&local, seed ? seed : 1u);
  }
  out->seed = seed ? seed : 1u;
  out->land_mass = rng_range(rng, 0, 2);
  out->land_form = rng_range(rng, 0, 2);
  out->temperature = rng_range(rng, 0, 2);
  out->climate = rng_range(rng, 0, 2);
  out->forest_extra = rng_range(rng, 0, 2);
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
 * 8-dir tables at VICEROY DS:0xb4 / 0xbe (file 0x1da54 / 0x1da5e):
 *   N NE E SE S SW W NW
 * FUN_684c_009c / 0116 index diagonals via (2*d)-1 for d in 1..4.
 * FUN_684c_021c / rivers index cardinals via 2*(d-1) for d in 1..4.
 */
static const int k_dir8_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int k_dir8_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

typedef struct MapGenMargins {
  int x0; /* 2d20: exclusive lower — need x > x0 */
  int x1; /* 2d1e: exclusive upper — need x < x1 */
  int y0; /* 2d21 */
  int y1; /* 2d1f */
} MapGenMargins;

static bool in_margins(int x, int y, const MapGenMargins* m) {
  return x > m->x0 && x < m->x1 && y > m->y0 && y < m->y1;
}

/* FUN_684c_0004: stamp land at (x,y) and optional +E / +S neighbours. */
static void stamp_land(uint8_t* mask, int w, int h, int x, int y) {
  if (x <= 0 || y <= 0 || x >= w || y >= h) {
    return;
  }
  int order_x[3];
  int order_y[3];
  int n = 0;
  order_x[n] = x;
  order_y[n] = y;
  n++;
  if (x < w - 1) {
    order_x[n] = x + 1;
    order_y[n] = y;
    n++;
  }
  if (y < h - 1) {
    order_x[n] = x;
    order_y[n] = y + 1;
    n++;
  }
  for (int i = 0; i < n; ++i) {
    const int px = order_x[i];
    const int py = order_y[i];
    if (!in_bounds(px, py, w, h)) {
      continue;
    }
    mask[idx(px, py, w)] = 1;
  }
}

/* Single-tile stamp used by FUN_684c_021c (no +E/+S thicken). */
static void stamp_one(uint8_t* mask, int w, int h, int x, int y) {
  if (x <= 0 || y <= 0 || x >= w || y >= h) {
    return;
  }
  mask[idx(x, y, w)] = 1;
}

/* FUN_684c_009c — archipelago / normal wander (diagonal steps). */
static void walk_009c(
  uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenMargins* m,
  int x,
  int y
) {
  int steps = rng_range(rng, 1, 0x40) + 2;
  while (steps > 0 && in_margins(x, y, m)) {
    stamp_land(mask, w, h, x, y);
    const int d = rng_range(rng, 1, 4);
    const int di = d * 2 - 1; /* 1,3,5,7 */
    x += k_dir8_dx[di];
    y += k_dir8_dy[di];
    steps--;
  }
}

/* FUN_684c_0116 — continents: wander + random diagonal thicken. */
static void walk_0116(
  uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenMargins* m,
  int x,
  int y
) {
  int steps = rng_range(rng, 1, 0x30) + 2;
  while (steps > 0 && in_margins(x, y, m)) {
    stamp_land(mask, w, h, x, y);
    if (rng_range(rng, 1, 4) == 1) {
      stamp_land(mask, w, h, x + 1, y + 1);
    }
    if (rng_range(rng, 1, 4) == 1) {
      stamp_land(mask, w, h, x - 1, y + 1);
    }
    if (rng_range(rng, 1, 4) == 1) {
      stamp_land(mask, w, h, x + 1, y - 1);
    }
    if (rng_range(rng, 1, 4) == 1) {
      stamp_land(mask, w, h, x - 1, y - 1);
    }
    const int d = rng_range(rng, 1, 4);
    const int di = d * 2 - 1;
    x += k_dir8_dx[di];
    y += k_dir8_dy[di];
    steps--;
  }
}

/* FUN_684c_021c — short cardinal wander (extra-mass param_1!=0 path). */
static void walk_021c(
  uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenMargins* m,
  int x,
  int y
) {
  int steps = rng_range(rng, 1, 0x10) + 2;
  while (steps > 0 && in_margins(x, y, m)) {
    stamp_one(mask, w, h, x, y);
    const int d = rng_range(rng, 1, 4);
    const int di = (d - 1) * 2; /* 0,2,4,6 cardinals */
    x += k_dir8_dx[di];
    y += k_dir8_dy[di];
    steps--;
  }
}

/*
 * FUN_684c_02a8 — one land blob.
 * Stamps into a cleared temp buffer, then merges: every temp tile increments the
 * permanent coverage mask and the budget counter (overlaps across blobs count again).
 */
static void place_blob(
  uint8_t* mask,
  int* land_count,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenMargins* m,
  const MapGenParams* params,
  int param_extra
) {
  const int n = w * h;
  uint8_t* temp = (uint8_t*)calloc((size_t)n, 1);
  if (!temp) {
    return;
  }

  int x = rng_range(rng, 1, w - 0x10) + 7;
  int y = rng_range(rng, 1, h - 8) + 3;
  if (param_extra != 0) {
    int tries = 0;
    while (tries < 32 && in_bounds(x, y, w, h) && mask[idx(x, y, w)]) {
      x = rng_range(rng, 1, w - 0x10) + 7;
      y = rng_range(rng, 1, h - 8) + 3;
      tries++;
    }
  }

  if (param_extra == 0) {
    if (params->land_form < 2) {
      walk_009c(temp, w, h, rng, m, x, y);
    } else {
      walk_0116(temp, w, h, rng, m, x, y);
    }
  } else {
    const int r = rng_range(rng, 1, 10);
    walk_021c(temp, w, h, rng, m, x, y);
    if (r > 6) {
      walk_021c(temp, w, h, rng, m, x, y);
    }
    if (r > 7) {
      walk_021c(temp, w, h, rng, m, x, y);
    }
  }

  for (int i = 0; i < n; ++i) {
    if (temp[i]) {
      mask[i]++;
      (*land_count)++;
    }
  }
  free(temp);
}

static void generate_land_mask(
  uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  const MapGenParams* params
) {
  const int budget = land_budget(params);
  memset(mask, 0, (size_t)w * (size_t)h);

  MapGenMargins m;
  m.x0 = 3;
  m.x1 = w - 6;
  m.y0 = 0;
  m.y1 = h;
  /* FUN_684c_08c0: coin-flip shrinks south vs raises north margin. */
  if (rng_range(rng, 0, 1) == 0) {
    m.y1 = h - 6;
  } else {
    m.y0 = 5;
  }

  int land_count = 0;
  int guard = 0;
  /* DOS: continue while land < budget (2d22 <= budget && 2d22 != budget). */
  while (land_count < budget && guard < 500) {
    place_blob(mask, &land_count, w, h, rng, &m, params, 0);
    guard++;
  }

  /*
   * Extras: 0xf - nonzero(DS:85c8[0..15]) after FUN_67bf_0000.
   * At this stage terrain is still ocean-filled; 67bf leaves 85c8 empty for NEW
   * WORLD land-blob labeling, so extras = 15. (seed-100 land mask bit-matches.)
   * When land_form > 0, DOS randomly reduces the extra count.
   */
  int extras = 15;
  if (params->land_form > 0) {
    const int cut = rng_range(rng, 0, extras);
    extras -= cut;
  }
  for (int e = 0; e < extras; ++e) {
    place_blob(mask, &land_count, w, h, rng, &m, params, 1);
  }
}

/*
 * FUN_684c_08c0 awkward-diagonal pass: inspect each 2×2 (x,y)/(x+1,y)/(x,y+1)/(x+1,y+1).
 * Bits: (x,y)=1, (x+1,y)=2, (x,y+1)=4, (x+1,y+1)=8. Patterns 6 and 9 are pure diagonals;
 * DOS fills the missing cells (writes 1), then backs the scan up one step.
 */
static void cleanup_diagonal_land(uint8_t* mask, int w, int h) {
  int y = 1;
  while (y < h - 1) {
    int x = 1;
    while (x < w - 1) {
      int m = 0;
      if (mask[idx(x, y, w)]) {
        m |= 1;
      }
      if (mask[idx(x + 1, y, w)]) {
        m |= 2;
      }
      if (mask[idx(x, y + 1, w)]) {
        m |= 4;
      }
      if (mask[idx(x + 1, y + 1, w)]) {
        m |= 8;
      }
      if (m == 6 || m == 9) {
        mask[idx(x + 1, y, w)] = 1;
        mask[idx(x, y + 1, w)] = 1;
        mask[idx(x + 1, y + 1, w)] = 1;
        if (x > 0) {
          x--;
        }
        if (y > 0) {
          y--;
        }
        continue;
      }
      x++;
    }
    y++;
  }
}

/*
 * Latitude band from FUN_684c_08c0: two range(1,0x10) draws, then
 *   dist = height/2 - rng - y + 8   (or abs form when first result <= 0),
 *   dist += (1 - temperature) * 2, band = dist >> 2.
 * Band → fixed base terrain (ASM switch at 0xbac); band > 5 → tundra (0).
 */
static uint8_t latitude_terrain(int y, int h, int temperature, MapGenRng* rng) {
  int r1 = rng_range(rng, 1, 0x10);
  int dist = (h >> 1) - r1 - y + 8;
  if (dist <= 0) {
    int r2 = rng_range(rng, 1, 0x10);
    dist = (h >> 1) - r2 - y + 8;
    if (dist < 0) {
      dist = -dist;
    }
  } else {
    (void)rng_range(rng, 1, 0x10); /* second draw always consumed */
  }
  dist += (1 - temperature) * 2;
  if (dist < 0) {
    dist = 0;
  }
  const int band = dist >> 2;
  if (band > 5) {
    return T_TUNDRA;
  }
  switch (band) {
    case 0:
      return T_SAVANNAH;
    case 1:
      return T_GRASSLAND;
    case 2:
      return T_DESERT;
    case 3:
      return T_PRAIRIE;
    case 4:
    case 5:
    default:
      return T_PLAINS;
  }
}

/* Polar arctic rows + near-polar stomps + E/W high-seas (FUN_684c_08c0 tail). */
static void paint_arctic_and_poles(uint8_t* terrain, int w, int h, MapGenRng* rng) {
  /* 0x28 random arctic tiles on y=1 and y=h-2. */
  for (int n = 0; n < 0x28; ++n) {
    int x = rng_range(rng, 1, w) - 1;
    if (x >= 0 && x < w && h > 2) {
      terrain[idx(x, 1, w)] = T_ARCTIC;
    }
    x = rng_range(rng, 1, w) - 1;
    if (x >= 0 && x < w && h > 2) {
      terrain[idx(x, h - 2, w)] = T_ARCTIC;
    }
  }
}

/* Eastern approach: convert open ocean from the east rim westward to high seas. */
static void paint_high_seas(uint8_t* terrain, const uint8_t* mask, int w, int h) {
  (void)mask;
  /* FUN_281f_00ce: full-height high-seas columns at x=0 and x=1. */
  for (int y = 0; y < h - 1; ++y) {
    terrain[idx(0, y, w)] = (uint8_t)((terrain[idx(0, y, w)] & 0xe0) | T_HIGH_SEAS);
    if (w > 1) {
      terrain[idx(1, y, w)] = (uint8_t)((terrain[idx(1, y, w)] & 0xe0) | T_HIGH_SEAS);
    }
  }
  /*
   * DOS also walks west from the east rim via FUN_281f_0768 (sea-lane connectivity),
   * not a blind ocean fill. Approximate: only convert pure ocean on the far-east
   * columns (x >= w-2), preserving arctic stomps inland.
   */
  for (int y = 1; y < h - 1; ++y) {
    for (int x = w - 1; x >= w - 2 && x >= 0; --x) {
      const int i = idx(x, y, w);
      if ((terrain[i] & 0x1f) == T_OCEAN) {
        terrain[i] = (uint8_t)((terrain[i] & 0xe0) | T_HIGH_SEAS);
      }
    }
  }
  /* Solid arctic on y=0 and y=h-1 (FUN_281f_00ba height=1). */
  if (h > 0) {
    for (int x = 0; x < w; ++x) {
      terrain[idx(x, 0, w)] = T_ARCTIC;
      terrain[idx(x, h - 1, w)] = T_ARCTIC;
    }
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
      /* Always consume latitude RNGs (DOS does, even for water). */
      uint8_t base = latitude_terrain(y, h, temp, rng);
      if (!mask[i]) {
        terrain[i] = T_OCEAN;
        continue;
      }
      /* Coverage ≥2 → hill, ≥3 → mountain (FUN_684c_08c0). */
      if (mask[i] >= 2) {
        base = (uint8_t)(base | F_HILL);
      }
      if (mask[i] >= 3) {
        base = (uint8_t)(base | F_MOUNTAIN);
      }
      terrain[i] = base;
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

  MapGenRng local;
  MapGenRng* rng = &local;
  if (params->rng) {
    rng = params->rng;
  } else {
    rng_seed(&local, p.seed);
  }

  /* FUN_684c_08c0: first range(1,0x7fff) → DS:0x190 resource seed. */
  (void)rng_range(rng, 1, 0x7fff);

  generate_land_mask(mask, w, h, rng, &p);
  cleanup_diagonal_land(mask, w, h);

  paint_terrain(out->terrain, mask, w, h, rng, &p);
  add_forests(out->terrain, mask, w, h, rng, &p);
  /* Hills/mountains come from blob coverage during paint_terrain (DOS). */
  add_rivers(out->terrain, mask, w, h, rng, &p);
  /* Arctic stomps + E/W high seas + polar arctic rows (after forest/river passes). */
  paint_arctic_and_poles(out->terrain, w, h, rng);
  paint_high_seas(out->terrain, mask, w, h);

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
