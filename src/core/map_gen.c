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

/* Cardinal dirs at DS:0xa8 / 0xae (N E S W). */
static const int k_dir4_dx[4] = {0, 1, 0, -1};
static const int k_dir4_dy[4] = {-1, 0, 1, 0};

/* 20-neighbour offsets at DS:0xc8 / 0xde (river forest sprinkle). */
static const int k_nbr20_dx[20] = {
  0, 1, 0, -1, -1, 1, 1, -1, 0, 2, 0, -2, -1, 1, -1, 1, -2, -2, 2, 2
};
static const int k_nbr20_dy[20] = {
  -1, 0, 1, 0, -1, -1, 1, 1, -2, 0, 2, 0, -2, -2, 2, 2, -1, 1, -1, 1
};

static int is_water_tile(uint8_t tile) {
  const uint8_t t = (uint8_t)(tile & 0x1f);
  return t == T_OCEAN || t == T_HIGH_SEAS;
}

static int inset_bounds(int x, int y, int w, int h) {
  /* FUN_137f_000a / FUN_281f_0302 */
  return x >= 1 && y >= 1 && x < w - 1 && y < h - 1;
}

/*
 * Rivers (FUN_684c_04a6): walk inland until ocean/existing river, OR 0x40 on
 * path. Failed short walks restore a terrain backup. Stops after 512 attempts
 * or (climate + mass + 2) * 8 successful rivers.
 */
static void rivers_pass(
  uint8_t* terrain,
  int w,
  int h,
  MapGenRng* rng,
  int climate,
  int land_mass
) {
  const int n = w * h;
  uint8_t* backup = (uint8_t*)malloc((size_t)n);
  if (!backup) {
    return;
  }

  int attempts = 0;
  int successes = 0;
  const int success_target = (climate + land_mass + 2) * 8;

  while (attempts < 0x200 && successes < success_target) {
    memcpy(backup, terrain, (size_t)n);
    attempts++;

    int x, y;
    for (;;) {
      x = rng_range(rng, 1, w - 2);
      y = rng_range(rng, 1, h - 2);
      /* DOS tests coverage-plane bit 0x20 (never set) then FUN_281f_0768 water. */
      if (is_water_tile(terrain[idx(x, y, w)])) {
        continue;
      }
      break;
    }

    int dir = rng_range(rng, 0, 3) << 1; /* even 8-dir index */
    int turn_flag = rng_range(rng, 0, 1);
    int length = 0;
    int connected = 0;
    const int origin_x = x;
    const int origin_y = y;
    int join_x = x;
    int join_y = y;

    for (;;) {
      {
        const int i = idx(x, y, w);
        terrain[i] = (uint8_t)(terrain[i] | F_RIVER);
      }
      length++;

      connected = 0;
      for (int c = 0; c < 4; ++c) {
        const int nx = x + k_dir4_dx[c];
        const int ny = y + k_dir4_dy[c];
        if (!in_bounds(nx, ny, w, h)) {
          continue;
        }
        const uint8_t nt = terrain[idx(nx, ny, w)];
        /* Only ocean/HS: coverage-plane 0x40 never flags our own trail. */
        if (is_water_tile(nt)) {
          connected = 1;
          join_x = nx;
          join_y = ny;
          terrain[idx(nx, ny, w)] = (uint8_t)(nt | F_RIVER);
          break;
        }
      }

      const int r = rng_range(rng, 0, 99);
      if (r >= 0x3c) {
        if (r > 0x5f) {
          turn_flag = turn_flag == 0 ? 1 : 0;
        }
        if (turn_flag == 0) {
          dir = (dir + 6) % 8;
        } else {
          dir = (dir + 2) % 8;
        }
        turn_flag = turn_flag == 0 ? 1 : 0;
      }

      x += k_dir8_dx[dir];
      y += k_dir8_dy[dir];

      if (connected) {
        break;
      }
      if (!inset_bounds(x, y, w, h)) {
        break;
      }
      {
        const uint8_t t = terrain[idx(x, y, w)];
        if ((t & F_RIVER) || (t & F_HILL)) {
          break;
        }
      }
    }

    if (connected && length > 2) {
      successes++;

      if (connected) {
        const int thr = (climate + 6) * 2;
        if (rng_range(rng, 1, thr) > 6) {
          int steps = rng_range(rng, 1, climate * 2 + 3);
          int mx = join_x;
          int my = join_y;
          while (steps-- > 0) {
            if (!in_bounds(mx, my, w, h)) {
              break;
            }
            /* Don't mountain-flag ocean join tiles. */
            if (!is_water_tile(terrain[idx(mx, my, w)])) {
              terrain[idx(mx, my, w)] = (uint8_t)(terrain[idx(mx, my, w)] | F_MOUNTAIN);
            }
            int found = 0;
            int nx = mx;
            int ny = my;
            for (int c = 0; c < 4; ++c) {
              const int tx = mx + k_dir4_dx[c];
              const int ty = my + k_dir4_dy[c];
              if (!in_bounds(tx, ty, w, h)) {
                continue;
              }
              const uint8_t t = terrain[idx(tx, ty, w)];
              if ((t & F_RIVER) && !(t & F_MOUNTAIN)) {
                found = 1;
                nx = tx;
                ny = ty;
                break;
              }
            }
            if (!found) {
              break;
            }
            mx = nx;
            my = ny;
          }
        }
      }

      for (int c = 0; c < 0x14; ++c) {
        const int nx = origin_x + k_nbr20_dx[c];
        const int ny = origin_y + k_nbr20_dy[c];
        if (!in_bounds(nx, ny, w, h)) {
          continue;
        }
        uint8_t t = terrain[idx(nx, ny, w)];
        if ((t & 0x1f) < 0x10) {
          if (rng_range(rng, 0, 1) != 0) {
            terrain[idx(nx, ny, w)] = (uint8_t)(t + 8);
          }
        }
      }
    } else {
      memcpy(terrain, backup, (size_t)n);
    }
  }

  free(backup);
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

/*
 * Climate humidity walk (FUN_684c_08c0 after latitude paint).
 * Per row: L→R with humidity = range(0, |h/4 - dist| + climate*4), then R→L
 * starting at humidity 0. Ocean tiles adjust humidity; land tiles shift type
 * arid←→wet and mountains reduce humidity.
 */
static void climate_humidity_pass(
  uint8_t* terrain,
  uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  int climate
) {
  climate = clamp_i(climate, 0, 2);

  for (int y = 0; y < h; ++y) {
    int dist = (h >> 1) - y;
    if (dist < 0) {
      dist = -dist;
    }

    /* --- Forward L→R --- */
    int span = (h >> 2) - dist;
    if (span < 0) {
      span = -span;
    }
    span += climate * 4;
    int humidity = rng_range(rng, 0, span);

    for (int x = 0; x < w; ++x) {
      const int i = idx(x, y, w);
      uint8_t tile = terrain[i];
      uint8_t base = (uint8_t)(tile & 0x1f);
      uint8_t feat = (uint8_t)(tile & 0xe0);

      if (base == T_OCEAN) {
        int thresh = (h >> 2) - dist;
        if (thresh < 0) {
          thresh = -thresh;
        }
        thresh += climate * 4;
        if (humidity > thresh) {
          humidity++;
        }
        continue;
      }

      if (feat & F_MOUNTAIN) {
        humidity -= 3;
      } else if (feat & F_HILL) {
        feat = (uint8_t)(feat & (uint8_t)~F_HILL);
      } else if (humidity < 0) {
        if (base == T_TUNDRA) {
          mask[i] = 2;
        } else if (base == T_PLAINS) {
          base = T_TUNDRA;
        } else if (base == T_PRAIRIE) {
          int mag = humidity < 0 ? -humidity : humidity;
          if (rng_range(rng, 0, mag) != 0) {
            base = T_DESERT;
          } else {
            base = T_PLAINS;
            humidity--;
          }
        } else if (base == T_GRASSLAND) {
          base = T_PRAIRIE;
        }
      } else if (humidity > 0) {
        if (base == T_TUNDRA) {
          base = T_PLAINS;
        } else if (base == T_PLAINS) {
          base = T_PRAIRIE;
        } else if (base == T_PRAIRIE) {
          base = T_GRASSLAND;
        } else if (base == T_GRASSLAND) {
          humidity -= 2;
          if (rng_range(rng, 0, 3) == 0) {
            base = T_MARSH;
          }
        } else if (base == T_SAVANNAH) {
          humidity -= 2;
          if (rng_range(rng, 0, 3) == 0) {
            base = T_SWAMP;
          }
        }
      }

      if (humidity > 0) {
        int hi = 7 - 2 * climate;
        if (hi < 1) {
          hi = 1;
        }
        humidity -= rng_range(rng, 1, hi);
      } else if (humidity < 0) {
        humidity++;
      }

      terrain[i] = (uint8_t)(feat | base);
    }

    /* --- Reverse R→L, humidity starts at 0 --- */
    humidity = 0;
    for (int x = w - 1; x >= 0; --x) {
      const int i = idx(x, y, w);
      uint8_t tile = terrain[i];
      uint8_t base = (uint8_t)(tile & 0x1f);
      uint8_t feat = (uint8_t)(tile & 0xe0);

      if (base == T_OCEAN) {
        int thresh = (dist >> 1) + climate;
        if (humidity > thresh) {
          humidity++;
        }
        continue;
      }

      if (feat & F_MOUNTAIN) {
        humidity -= 3;
      } else if (feat & F_HILL) {
        feat = (uint8_t)(feat & (uint8_t)~F_HILL);
      } else if (humidity > 0) {
        if (base == T_TUNDRA) {
          base = T_PLAINS;
        } else if (base == T_PLAINS) {
          base = T_PRAIRIE;
        } else if (base == T_PRAIRIE) {
          base = T_GRASSLAND;
        } else if (base == T_GRASSLAND) {
          humidity -= 2;
          if (rng_range(rng, 0, 1) == 0) {
            base = T_MARSH;
          }
        } else if (base == T_SAVANNAH) {
          humidity -= 2;
          if (rng_range(rng, 0, 1) == 0) {
            base = T_SWAMP;
          }
        }
      }

      if (humidity > 0) {
        int hi = 7 - 2 * climate;
        if (hi < 1) {
          hi = 1;
        }
        humidity -= rng_range(rng, 1, hi);
      } else if (humidity < 0) {
        humidity++;
      }

      terrain[i] = (uint8_t)(feat | base);
    }
  }
}

/*
 * FUN_684c_03e4: true when (x,y) is in-bounds for a 1-tile inset and all four
 * diagonals are non-ocean. Inland mountains are demoted; coastal ones stay.
 */
static int mountain_landlocked(const uint8_t* terrain, int x, int y, int w, int h) {
  if (x <= 0 || y <= 0 || x >= w - 1 || y >= h - 1) {
    return 0;
  }
  const int dxy[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
  for (int n = 0; n < 4; ++n) {
    const int nx = x + dxy[n][0];
    const int ny = y + dxy[n][1];
    if ((terrain[idx(nx, ny, w)] & 0x1f) == T_OCEAN) {
      return 0;
    }
  }
  return 1;
}

/* Apply hill/mountain chance epilogue at LAB_684c_11de. */
static void forest_hill_epilogue(uint8_t* feat, int hill_chance, int mtn_chance, MapGenRng* rng) {
  if (hill_chance <= 0) {
    return;
  }
  if (rng_range(rng, 0, hill_chance) != 0) {
    return;
  }
  *feat = (uint8_t)(*feat | F_HILL);
  if (mtn_chance <= 0) {
    return;
  }
  if (rng_range(rng, 0, mtn_chance) == 0) {
    *feat = (uint8_t)(*feat | F_MOUNTAIN);
  }
}

/*
 * Forest wander (FUN_684c_08c0 @ 0x0fcc / 0x123d): (forest_extra+1)*0x320 steps.
 * Even: new interior point. Odd: step by DS:0xb4/0xbe dir from range(0,8).
 * Mutates land types / hills / mountains; forest canopy is the later 0x1286 pass.
 */
static void forest_wander_pass(
  uint8_t* terrain,
  uint8_t* mask,
  int w,
  int h,
  MapGenRng* rng,
  int forest_extra
) {
  forest_extra = clamp_i(forest_extra, 0, 2);
  const int attempts = (forest_extra + 1) * 0x320;
  int x = 1;
  int y = 1;
  /* DOS reuses these across early-outs; start cleared like a fresh frame. */
  int hill_chance = 0;
  int mtn_chance = 0;

  for (int n = 0; n < attempts; ++n) {
    if ((n & 1) == 0) {
      x = rng_range(rng, 1, w - 2);
      y = rng_range(rng, 1, h - 2);
    } else {
      int d = rng_range(rng, 0, 8);
      /* DOS indexes [BX+0xb4] with no clamp; BX==8 reads past the 8-byte table.
       * Adjacent bytes behave as a no-step for our port (tables are 8 entries). */
      if (d >= 0 && d <= 7) {
        x += k_dir8_dx[d];
        y += k_dir8_dy[d];
      }
    }
    if (!in_bounds(x, y, w, h)) {
      continue;
    }

    const int i = idx(x, y, w);
    uint8_t feat = terrain[i];
    uint8_t typ = (uint8_t)(feat & 0x1f);
    int write_mask_val = -1; /* >=0 → also write coverage mask */

    if (feat & F_MOUNTAIN) {
      /* Coastal / OOB-diagonal mountains kept; inland demoted (AND 0x5f). */
      if (mountain_landlocked(terrain, x, y, w, h)) {
        feat = (uint8_t)(feat & 0x5fu);
      }
      forest_hill_epilogue(&feat, hill_chance, mtn_chance, rng);
      terrain[i] = (uint8_t)((feat & 0xe0) | typ);
      continue;
    }

    if (feat & F_HILL) {
      feat = (uint8_t)(feat | F_MOUNTAIN);
      mask[i] = 1;
      forest_hill_epilogue(&feat, hill_chance, mtn_chance, rng);
      terrain[i] = (uint8_t)((feat & 0xe0) | typ);
      continue;
    }

    /* LAB_684c_106a: reset chances, switch on base type 0..7. */
    hill_chance = 0;
    mtn_chance = 0;
    if (typ > 7) {
      /* JA → 11de: still runs hill epilogue with the cleared chances (no-op). */
      forest_hill_epilogue(&feat, hill_chance, mtn_chance, rng);
      terrain[i] = (uint8_t)((feat & 0xe0) | typ);
      continue;
    }

    switch (typ) {
      case 0: /* tundra */
        hill_chance = 1;
        mtn_chance = 0;
        if (rng_range(rng, 0, 1) == 0) {
          typ = T_PLAINS;
        }
        break;
      case 1: /* desert */
        hill_chance = 1;
        mtn_chance = 1;
        if (rng_range(rng, 0, 1) == 0) {
          typ = T_PRAIRIE;
        }
        break;
      case 2: /* plains */
      case 3: /* prairie */
        if (typ == T_PRAIRIE && rng_range(rng, 0, 2) == 0) {
          typ = T_PLAINS;
        }
        hill_chance = 2;
        mtn_chance = 2;
        if (rng_range(rng, 0, 1) == 0) {
          write_mask_val = 2;
        }
        break;
      case 4: /* grassland */
        hill_chance = 3;
        mtn_chance = 1;
        if (rng_range(rng, 0, 1) == 0) {
          typ = T_MARSH;
        }
        if (rng_range(rng, 0, 1) == 0) {
          write_mask_val = 1;
        }
        break;
      case 5: /* savannah */
        hill_chance = 3;
        mtn_chance = 2;
        if (rng_range(rng, 0, 1) == 0) {
          typ = T_SWAMP;
        }
        if (rng_range(rng, 0, 1) == 0) {
          write_mask_val = 1;
        }
        break;
      case 6: /* marsh */
        hill_chance = 5;
        mtn_chance = 3;
        if (rng_range(rng, 0, 1) == 0) {
          typ = T_GRASSLAND;
        }
        if (rng_range(rng, 0, 1) == 0) {
          write_mask_val = 2;
        }
        break;
      case 7: /* swamp */
        hill_chance = 5;
        mtn_chance = 3;
        if (rng_range(rng, 0, 1) != 0) {
          typ = T_SAVANNAH;
        }
        if (rng_range(rng, 0, 1) != 0) {
          write_mask_val = 2;
        }
        break;
      default:
        break;
    }

    if (write_mask_val >= 0) {
      mask[i] = (uint8_t)write_mask_val;
    }
    forest_hill_epilogue(&feat, hill_chance, mtn_chance, rng);
    terrain[i] = (uint8_t)((feat & 0xe0) | typ);
  }
}

/*
 * Whole-map forest-bit pass (LAB_684c_1286): ocean skipped; coverage==1 always
 * gains +8 or +0x10; coverage!=1 goes through FUN_281f_0d12 (unit/path probe
 * that returns 0 during mapgen) so hills/mountains keep their cleared type.
 */
static void forest_bit_pass(uint8_t* terrain, const uint8_t* mask, int w, int h, MapGenRng* rng) {
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int i = idx(x, y, w);
      uint8_t tile = terrain[i];
      if ((tile & 0x1f) == T_OCEAN) {
        continue;
      }
      if (mask[i] == 1) {
        if (rng_range(rng, 0, 8) == 0) {
          tile = (uint8_t)(tile + 8);
        } else {
          tile = (uint8_t)(tile + 0x10);
        }
        terrain[i] = tile;
        continue;
      }
      /* FUN_281f_0d12 → FUN_15eb_00a2: no units during mapgen → 0, no RNG. */
      (void)rng;
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
  climate_humidity_pass(out->terrain, mask, w, h, rng, p.climate);
  forest_wander_pass(out->terrain, mask, w, h, rng, p.forest_extra);
  forest_bit_pass(out->terrain, mask, w, h, rng);
  rivers_pass(out->terrain, w, h, rng, p.climate, p.land_mass);
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
