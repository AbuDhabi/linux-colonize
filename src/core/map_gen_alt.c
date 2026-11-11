#include "core/map_gen.h"

/*
 * Alternate map generator (settings.json divergences.alternate_map_generator).
 * Port-only: there is no DOS counterpart, so nothing here cites the decomp.
 * docs/map_gen_alt.md describes the model; the tunables below are the knobs.
 *
 * A physical toy model on the 58x72 plane: tectonic plates drift and collide,
 * erosion smooths the result, latitude winds carry ocean moisture inland and
 * smear the temperature bands, and the terrain falls out of elevation,
 * temperature and rainfall. The Customize axes steer it: land_mass = land
 * share, land_form = plate count, temperature = band widths, climate = the
 * moisture winds carry. forest_extra is not used.
 *
 * The rim never takes part: 2 columns west / east (water, sea lanes), 1 row
 * north / south (arctic, as map_generate's finalize writes it). The tail is shared with
 * map_generate: prime_resource_seed (specials and rumours hash from it), high
 * seas, map_gen_finalize_border_and_forest, continents (+ Pacific / offshore
 * suppress) and the Euro landfalls. All draws come from the campaign RNG, so a
 * seed replays and tribe placement continues from the post-gen state.
 *
 * Sections:
 *  - Tunables & helpers
 *  - Plates & tectonics (steps 1-3)
 *  - Erosion (step 4)
 *  - Winds, rainfall, temperature (steps 5-7)
 *  - Terrain, relief, rivers, forest (step 8)
 *  - High seas & driver
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/map_gen_internal.h"
#include "core/strutil.h"
#include "platform/diagnostics.h"

/* ===== Tunables & helpers ===== */

enum {
  W = MAP_GEN_WIDTH,
  H = MAP_GEN_HEIGHT,
  N = W * H,
  EDGE_X = 2,            /* west / east rim kept as water (sea lanes) */
  EDGE_Y = 1,            /* north / south rim: row 0 / H-1 become arctic */
  ELEV_UNIT = 16,        /* initial land +16, water -16 */
  MAX_PLATES = 64,
  TECTONIC_ITERS = 6,
  WARP_CELL = 8,         /* crust warp: coarse noise lattice spacing (finer octaves 1/2, 3/8) */
  COLLISION_UPLIFT = 12, /* added to the higher crust where two plates meet */
  RIFT_ELEV = -32,       /* fresh crust on a tile a plate moved off and nobody filled */
  EROSION_ITERS = 4,
  EROSION_DIV = 8,       /* each pass moves 1/8 of every 4-neighbour difference */
  HILL_RELIEF = 18,      /* tile minus its lowest land 8-neighbour */
  MOUNTAIN_RELIEF = 30,
  RAIN_ITERS = 160,      /* advection sweeps; > longest wind path on the map */
  RAIN_SCALE = 1,        /* rain units per deposited moisture unit */
  TEMP_SMEAR_ITERS = 6,
  RIVER_MINOR_FLOW = 16, /* accumulated rain units upstream of a tile */
  RIVER_MAJOR_FLOW = 70,
  RIVER_MIN_LEN = 3,     /* land tiles from the farthest source to the mouth */
  ROUTE_CELL = 6,        /* drainage routing noise lattice spacing */
  ROUTE_NOISE = 24,      /* routing noise amplitude, elevation units */
  FOREST_PERCENT = 90,
};

/* land_form 0 Archipelago .. 2 Continents (3 from NEW WORLD's random axes). */
static const int k_plates_by_form[4] = {48, 28, 16, 10};
/* land_mass 0 Small .. 2 Large (3 random): plate area share started as land. */
static const int k_land_permille_by_mass[4] = {330, 420, 520, 600};
/* By temperature 0 Cool .. 2 Warm (3 random): the Cold band is a fixed number
 * of rows at each active rim (arctic is useless land); Hot, Warm, Lukewarm,
 * Temperate, Cool share the rest of equator-to-rim in these proportions.
 * Hot is one strip astride the equator; its entry is doubled so it reads wide. */
static const int k_cold_rows[4] = {2, 1, 0, 0};
static const int k_band_widths[4][5] = {
  {4, 2, 4, 8, 6},   /* Cool: Cool + Temperate grow */
  {8, 3, 6, 6, 3},   /* Temperate: Lukewarm + Temperate lead */
  {12, 5, 4, 4, 2},  /* Warm: Hot + Warm grow */
  {14, 6, 3, 3, 1},  /* NEW WORLD 3: past Warm, as DOS (1 - temperature) */
};
/* Erosion cap by distance from the west / east active edge: the column next to
 * the rim is always sea, the next few ramp up, so land tapers off instead of
 * ending in a cliff at the sea lane. */
static const int k_side_cap[] = {-ELEV_UNIT / 4, ELEV_UNIT / 4, ELEV_UNIT * 3 / 4, ELEV_UNIT * 3 / 2};
#define SIDE_ZONE ((int)(sizeof(k_side_cap) / sizeof(k_side_cap[0])))
enum { SIDE_REACH_MAX = 4 }; /* extra sea columns at the side, varying smoothly by row */
/* Moisture an ocean tile hands the wind, by climate 0 Arid .. 2 Wet (3 random). */
static const float k_moisture_by_climate[4] = {60.0f, 100.0f, 140.0f, 180.0f};
#define RAIN_BASE_DEP 0.05f   /* share of carried moisture dropped per land tile */
#define RAIN_OROGRAPHIC 0.02f /* extra share per elevation unit the wind climbs */
#define RAIN_LEE_FACTOR 0.5f  /* descending air drops less */
#define WARP_AMP 4.0f         /* crust warp: coarse octave displacement, tiles (finer 40%, 25%) */
#define WIND_DEFLECT 0.7f     /* share of the uphill wind component a steep slope turns */
#define WIND_GRAD_FULL 24.0f  /* slope (elevation units per tile) that deflects fully */
#define WIND_ONSHORE 3.0f     /* pull from sea toward land (monsoon / sea breeze) */

static const int k_dx4[4] = {0, 1, 0, -1};
static const int k_dy4[4] = {-1, 0, 1, 0};

/* Temperature band -> base land type (Hot 3 .. Cold -2). */
static const uint8_t k_band_terrain[6] = {T_ARCTIC, T_TUNDRA, T_PLAINS, T_PRAIRIE, T_GRASSLAND, T_SAVANNAH};

typedef struct AltGen {
  ColonizeDosRng* rng;
  int active[N]; /* indices of non-rim tiles */
  int active_count;
  int elev[N];
  int plate[N];
  int scratch_a[N];
  int scratch_b[N];
  float wx[N];
  float wy[N];
  float fa[N];
  float fb[N];
  float fc[N];
  int rain[N];
  int temp[N];
  uint8_t terrain[N];
  int plate_count;
  int vx[MAX_PLATES];
  int vy[MAX_PLATES];
} AltGen;

static bool s_alternate;

void map_gen_set_alternate(bool on) {
  s_alternate = on;
}

bool map_gen_alternate_enabled(void) {
  return s_alternate;
}

static int rnd(AltGen* g, int lo, int hi) {
  return dos_rng_range(g->rng, lo, hi);
}

static bool in_active(int x, int y) {
  return x >= EDGE_X && y >= EDGE_Y && x < W - EDGE_X && y < H - EDGE_Y;
}

static bool is_water_type(uint8_t t) {
  t = (uint8_t)(t & 0x1f);
  return t == T_OCEAN || t == T_HIGH_SEAS;
}

/* Add `amount` over the `a` tiles in `list` so the total changes by exactly
 * that: an even share each, the remainder one unit apiece from a random start. */
static void spread_evenly(AltGen* g, const int* list, int a, int amount) {
  const int q = amount / a;
  const int r = amount - q * a;
  for (int k = 0; k < a; ++k) {
    g->elev[list[k]] += q;
  }
  const int start = rnd(g, 0, a - 1);
  const int step = r > 0 ? 1 : -1;
  for (int k = 0; k < abs(r); ++k) {
    g->elev[list[(start + k) % a]] += step;
  }
}

static int active_elev_sum(const AltGen* g) {
  int s = 0;
  for (int k = 0; k < g->active_count; ++k) {
    s += g->elev[g->active[k]];
  }
  return s;
}

/* Bilinear read of a float plane; coordinates clamp to the map. */
static float sample(const float* f, float x, float y) {
  if (x < 0.0f) {
    x = 0.0f;
  }
  if (y < 0.0f) {
    y = 0.0f;
  }
  if (x > (float)(W - 1)) {
    x = (float)(W - 1);
  }
  if (y > (float)(H - 1)) {
    y = (float)(H - 1);
  }
  const int x0 = (int)x;
  const int y0 = (int)y;
  const int x1 = x0 + 1 < W ? x0 + 1 : x0;
  const int y1 = y0 + 1 < H ? y0 + 1 : y0;
  const float tx = x - (float)x0;
  const float ty = y - (float)y0;
  const float top = f[y0 * W + x0] * (1.0f - tx) + f[y0 * W + x1] * tx;
  const float bot = f[y1 * W + x0] * (1.0f - tx) + f[y1 * W + x1] * tx;
  return top * (1.0f - ty) + bot * ty;
}

/* ===== Plates & tectonics (steps 1-3) ===== */

/* Step 2: seed plate_count plates on random tiles and grow them by random
 * frontier picks until they tile the active area; pick land plates until the
 * land_mass share is closest, give each a random drift vector (-2..2 per axis). */
static void make_plates(AltGen* g, const MapGenParams* p) {
  int* frontier = g->scratch_a;
  int nf = 0;
  for (int i = 0; i < N; ++i) {
    g->plate[i] = -1;
  }
  g->plate_count = k_plates_by_form[p->land_form];
  for (int k = 0; k < g->plate_count;) {
    const int x = rnd(g, EDGE_X, W - 1 - EDGE_X);
    const int y = rnd(g, EDGE_Y, H - 1 - EDGE_Y);
    const int i = y * W + x;
    if (g->plate[i] >= 0) {
      continue;
    }
    g->plate[i] = k++;
    frontier[nf++] = i;
  }
  while (nf > 0) {
    const int f = rnd(g, 0, nf - 1);
    const int i = frontier[f];
    int open[4];
    int no = 0;
    for (int d = 0; d < 4; ++d) {
      const int nx = i % W + k_dx4[d];
      const int ny = i / W + k_dy4[d];
      if (in_active(nx, ny) && g->plate[ny * W + nx] < 0) {
        open[no++] = ny * W + nx;
      }
    }
    if (no == 0) {
      frontier[f] = frontier[--nf];
      continue;
    }
    const int j = open[no == 1 ? 0 : rnd(g, 0, no - 1)];
    g->plate[j] = g->plate[i];
    frontier[nf++] = j;
  }

  int area[MAX_PLATES] = {0};
  int order[MAX_PLATES];
  bool land[MAX_PLATES] = {false};
  for (int k = 0; k < g->active_count; ++k) {
    area[g->plate[g->active[k]]]++;
  }
  for (int k = 0; k < g->plate_count; ++k) {
    order[k] = k;
  }
  for (int k = g->plate_count - 1; k > 0; --k) {
    const int j = rnd(g, 0, k);
    const int t = order[k];
    order[k] = order[j];
    order[j] = t;
  }
  const int target = g->active_count * k_land_permille_by_mass[p->land_mass] / 1000;
  int land_area = 0;
  for (int k = 0; k < g->plate_count; ++k) {
    const int pk = order[k];
    if (abs(land_area + area[pk] - target) < abs(land_area - target)) {
      land[pk] = true;
      land_area += area[pk];
    }
  }
  for (int k = 0; k < g->plate_count; ++k) {
    g->vx[k] = rnd(g, 0, 4) - 2;
    g->vy[k] = rnd(g, 0, 4) - 2;
  }

  /* Step 1 + 2: flat plane, then -1 / +1 (scaled) by plate type; half the tiles
   * (coin per tile) start at -2 / +2. */
  for (int i = 0; i < N; ++i) {
    g->elev[i] = -ELEV_UNIT;
  }
  for (int k = 0; k < g->active_count; ++k) {
    const int i = g->active[k];
    const int e = ELEV_UNIT * (1 + rnd(g, 0, 1));
    g->elev[i] = land[g->plate[i]] ? e : -e;
  }
}

/* One axis step for a drift component: speed 2 moves every iteration, speed 1
 * every other on average, 0 never. */
static int drift_step(AltGen* g, int v) {
  if (v == 0) {
    return 0;
  }
  if (abs(v) == 1 && rnd(g, 0, 1) == 0) {
    return 0;
  }
  return v > 0 ? 1 : -1;
}

/*
 * Step 3: each iteration every plate shifts by its drift step, carrying its
 * elevation. Where two plates land on one tile the higher crust keeps it and
 * gains COLLISION_UPLIFT (the lower one is pushed under); a tile left empty
 * keeps its old owner and becomes low new crust (RIFT_ELEV). Crust pushed into the rim is gone.
 * The net change is spread back over the plane, so the total is constant.
 */
static void tectonics(AltGen* g) {
  int* new_plate = g->scratch_a;
  int* new_elev = g->scratch_b;
  for (int it = 0; it < TECTONIC_ITERS; ++it) {
    const int before = active_elev_sum(g);
    int sx[MAX_PLATES];
    int sy[MAX_PLATES];
    for (int k = 0; k < g->plate_count; ++k) {
      sx[k] = drift_step(g, g->vx[k]);
      sy[k] = drift_step(g, g->vy[k]);
    }
    for (int i = 0; i < N; ++i) {
      new_plate[i] = -1;
    }
    for (int k = 0; k < g->active_count; ++k) {
      const int i = g->active[k];
      const int pk = g->plate[i];
      const int tx = i % W + sx[pk];
      const int ty = i / W + sy[pk];
      if (!in_active(tx, ty)) {
        continue;
      }
      const int t = ty * W + tx;
      if (new_plate[t] < 0) {
        new_plate[t] = pk;
        new_elev[t] = g->elev[i];
      } else {
        if (g->elev[i] > new_elev[t]) {
          new_plate[t] = pk;
          new_elev[t] = g->elev[i];
        }
        new_elev[t] += COLLISION_UPLIFT;
      }
    }
    for (int k = 0; k < g->active_count; ++k) {
      const int i = g->active[k];
      if (new_plate[i] < 0) {
        g->elev[i] = RIFT_ELEV; /* the old crust moved on */
      } else {
        g->plate[i] = new_plate[i];
        g->elev[i] = new_elev[i];
      }
    }
    spread_evenly(g, g->active, g->active_count, before - active_elev_sum(g));
  }
}

/* Bilinear value noise: a lattice every `cell` (>= 3) tiles of values in
 * -amp..amp, added onto `out`. */
static void add_value_noise(AltGen* g, float* out, int cell, float amp) {
  const int lw = W / cell + 2;
  const int lh = H / cell + 2;
  float lattice[32 * 32];
  for (int k = 0; k < lw * lh; ++k) {
    lattice[k] = (float)(rnd(g, 0, 2000) - 1000) / 1000.0f * amp;
  }
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int lx = x / cell;
      const int ly = y / cell;
      const float tx = (float)(x % cell) / (float)cell;
      const float ty = (float)(y % cell) / (float)cell;
      const float* r0 = &lattice[ly * lw + lx];
      const float* r1 = r0 + lw;
      const float top = r0[0] * (1.0f - tx) + r0[1] * tx;
      const float bot = r1[0] * (1.0f - tx) + r1[1] * tx;
      out[y * W + x] += top * (1.0f - ty) + bot * ty;
    }
  }
}

/*
 * Step 3, end: rigid shifts leave rift and plate edges as straight lines
 * along the drift axes. Warp the crust so they bend: each tile takes the
 * elevation found at a spot displaced by a smooth random field (three value
 * noise octaves, WARP_CELL / WARP_AMP), then the net change is spread back
 * so the total stays constant.
 */
static void warp_crust(AltGen* g) {
  float* dx = g->fa;
  float* dy = g->fb;
  float* src = g->fc;
  memset(dx, 0, sizeof(g->fa));
  memset(dy, 0, sizeof(g->fb));
  add_value_noise(g, dx, WARP_CELL, WARP_AMP);
  add_value_noise(g, dx, WARP_CELL / 2, WARP_AMP * 0.4f);
  add_value_noise(g, dx, WARP_CELL * 3 / 8, WARP_AMP * 0.25f);
  add_value_noise(g, dy, WARP_CELL, WARP_AMP);
  add_value_noise(g, dy, WARP_CELL / 2, WARP_AMP * 0.4f);
  add_value_noise(g, dy, WARP_CELL * 3 / 8, WARP_AMP * 0.25f);
  const int before = active_elev_sum(g);
  for (int i = 0; i < N; ++i) {
    src[i] = (float)g->elev[i];
  }
  for (int k = 0; k < g->active_count; ++k) {
    const int i = g->active[k];
    float sx = (float)(i % W) + dx[i];
    float sy = (float)(i / W) + dy[i];
    sx = sx < EDGE_X ? EDGE_X : sx > W - 1 - EDGE_X ? W - 1 - EDGE_X : sx;
    sy = sy < EDGE_Y ? EDGE_Y : sy > H - 1 - EDGE_Y ? H - 1 - EDGE_Y : sy;
    g->elev[i] = (int)floorf(sample(src, sx, sy) + 0.5f);
  }
  spread_evenly(g, g->active, g->active_count, before - active_elev_sum(g));
}

/* ===== Erosion (step 4) ===== */

/* Diffusion between 4-neighbours inside the active area: each pair moves
 * 1/EROSION_DIV of its difference downhill (exact integer transfer). Then the
 * west / east side zones are worn down to k_side_cap and what they lose is
 * spread over the tiles inland of them, so the total stays the same. */
static void erosion(AltGen* g) {
  int* delta = g->scratch_a;
  int* inland = g->scratch_b;
  int inland_count = 0;
  for (int k = 0; k < g->active_count; ++k) {
    const int x = g->active[k] % W;
    const int zone = SIDE_ZONE + SIDE_REACH_MAX;
    if (x - EDGE_X >= zone && W - 1 - EDGE_X - x >= zone) {
      inland[inland_count++] = g->active[k];
    }
  }
  /* How many extra columns the cap reaches in, per row and side (smooth
   * noise, 0..SIDE_REACH_MAX), so the worn coast is not a straight line. */
  float* reach_w = g->fa;
  float* reach_e = g->fb;
  memset(reach_w, 0, sizeof(g->fa));
  memset(reach_e, 0, sizeof(g->fb));
  add_value_noise(g, reach_w, 4, 0.8f * (SIDE_REACH_MAX + 1));
  add_value_noise(g, reach_e, 4, 0.8f * (SIDE_REACH_MAX + 1));
  for (int it = 0; it < EROSION_ITERS; ++it) {
    memset(delta, 0, sizeof(g->scratch_a));
    for (int k = 0; k < g->active_count; ++k) {
      const int i = g->active[k];
      const int x = i % W;
      const int y = i / W;
      for (int d = 1; d <= 2; ++d) { /* E and S: each pair once */
        const int nx = x + k_dx4[d];
        const int ny = y + k_dy4[d];
        if (!in_active(nx, ny)) {
          continue;
        }
        const int j = ny * W + nx;
        const int flow = (g->elev[i] - g->elev[j]) / EROSION_DIV;
        delta[i] -= flow;
        delta[j] += flow;
      }
    }
    for (int k = 0; k < g->active_count; ++k) {
      g->elev[g->active[k]] += delta[g->active[k]];
    }
    int worn = 0;
    for (int k = 0; k < g->active_count; ++k) {
      const int i = g->active[k];
      const int x = i % W;
      const bool west = x - EDGE_X < W - 1 - EDGE_X - x;
      const int side = west ? x - EDGE_X : W - 1 - EDGE_X - x;
      /* The sea column widens by `reach` (column 0 is always sea). */
      const float r = (west ? reach_w : reach_e)[(i / W) * W] + 0.5f * (SIDE_REACH_MAX + 1);
      const int reach = clamp_int((int)r, 0, SIDE_REACH_MAX);
      const int step = side - reach < 0 ? 0 : side - reach;
      if (step < SIDE_ZONE && g->elev[i] > k_side_cap[step]) {
        worn += g->elev[i] - k_side_cap[step];
        g->elev[i] = k_side_cap[step];
      }
    }
    spread_evenly(g, inland, inland_count, worn);
  }
}

/* ===== Winds, rainfall, temperature (steps 5-7) ===== */

/* Latitude in -1 (south rim) .. +1 (north rim); equator at mid-height. */
static float latitude(int y) {
  const float cy = (float)(H - 1) * 0.5f;
  return (cy - (float)y) / (cy - (float)EDGE_Y);
}

/*
 * Step 5. Three-cell circulation by latitude: trade winds (easterlies blowing
 * toward the equator) below 30 degrees, westerlies blowing poleward from 30 to
 * 60, polar easterlies above. Air is drawn from sea toward land (land heats
 * faster; monsoon and sea breeze), and land turns the flow: on a slope, part of
 * the uphill component is deflected (mountains block and steer wind), more
 * the steeper it is. Two 3x3 blurs soften the belt edges, then each vector
 * is normalised to one tile per step.
 */
static void winds(AltGen* g) {
  for (int y = 0; y < H; ++y) {
    const float lat = latitude(y);
    const float deg = fabsf(lat) * 90.0f;
    float zonal;
    float poleward;
    if (deg < 30.0f) {
      zonal = -1.0f;
      poleward = -0.5f;
    } else if (deg < 60.0f) {
      zonal = 1.0f;
      poleward = 0.5f;
    } else {
      zonal = -1.0f;
      poleward = -0.5f;
    }
    for (int x = 0; x < W; ++x) {
      g->wx[y * W + x] = zonal;
      g->wy[y * W + x] = lat >= 0.0f ? -poleward : poleward; /* north = -y */
    }
  }

  /* Land heats faster than sea: air is drawn onshore, along the gradient of
   * a blurred land share (two 5x5 box passes), so coasts bend the belts. */
  float* land = g->fa;
  float* tmp = g->fb;
  for (int i = 0; i < N; ++i) {
    land[i] = g->elev[i] >= 0 ? 1.0f : 0.0f;
  }
  for (int pass = 0; pass < 2; ++pass) {
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        float sum = 0.0f;
        int c = 0;
        for (int dy = -2; dy <= 2; ++dy) {
          for (int dx = -2; dx <= 2; ++dx) {
            if (map_dims_in_bounds(x + dx, y + dy, W, H)) {
              sum += land[(y + dy) * W + x + dx];
              c++;
            }
          }
        }
        tmp[y * W + x] = sum / (float)c;
      }
    }
    memcpy(land, tmp, sizeof(float) * N);
  }
  for (int y = 1; y < H - 1; ++y) {
    for (int x = 1; x < W - 1; ++x) {
      const int i = y * W + x;
      g->wx[i] += WIND_ONSHORE * (land[i + 1] - land[i - 1]) * 0.5f;
      g->wy[i] += WIND_ONSHORE * (land[i + W] - land[i - W]) * 0.5f;
    }
  }

  float* height = g->fa;
  for (int i = 0; i < N; ++i) {
    height[i] = g->elev[i] > 0 ? (float)g->elev[i] : 0.0f;
  }
  for (int y = 1; y < H - 1; ++y) {
    for (int x = 1; x < W - 1; ++x) {
      const int i = y * W + x;
      if (g->elev[i] < 0) {
        continue;
      }
      const float gx = (height[i + 1] - height[i - 1]) * 0.5f;
      const float gy = (height[i + W] - height[i - W]) * 0.5f;
      const float g2 = gx * gx + gy * gy;
      const float dot = g->wx[i] * gx + g->wy[i] * gy;
      if (g2 <= 0.0f || dot <= 0.0f) {
        continue;
      }
      float s = sqrtf(g2) / WIND_GRAD_FULL;
      if (s > 1.0f) {
        s = 1.0f;
      }
      s *= WIND_DEFLECT * dot / g2;
      g->wx[i] -= s * gx;
      g->wy[i] -= s * gy;
    }
  }

  for (int pass = 0; pass < 2; ++pass) {
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        float sx = 0.0f;
        float sy = 0.0f;
        int c = 0;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx;
            const int ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) {
              continue;
            }
            sx += g->wx[ny * W + nx];
            sy += g->wy[ny * W + nx];
            c++;
          }
        }
        g->fb[y * W + x] = sx / (float)c;
        g->fc[y * W + x] = sy / (float)c;
      }
    }
    memcpy(g->wx, g->fb, sizeof(g->wx));
    memcpy(g->wy, g->fc, sizeof(g->wy));
  }
  for (int i = 0; i < N; ++i) {
    const float len = sqrtf(g->wx[i] * g->wx[i] + g->wy[i] * g->wy[i]);
    if (len > 0.05f) {
      g->wx[i] /= len;
      g->wy[i] /= len;
    } else {
      g->wx[i] = 0.0f;
      g->wy[i] = 0.0f;
    }
  }
}

/*
 * Step 6. Steady-state moisture advection: water tiles (sea, lakes, rim) hand
 * the wind the climate's moisture; each land tile takes what arrives from one
 * tile upwind and drops a share as rain. The share grows with the elevation
 * the wind climbs into the tile (orographic rain) and shrinks where it
 * descends, so a range leaves a wet windward and a dry lee side.
 */
static void rainfall(AltGen* g, int climate) {
  const float cap = k_moisture_by_climate[climate];
  float* height = g->fa;
  float* moist = g->fb;
  float* next = g->fc;
  float rain_f[N];
  for (int i = 0; i < N; ++i) {
    height[i] = g->elev[i] > 0 ? (float)g->elev[i] : 0.0f;
    moist[i] = g->elev[i] < 0 ? cap : 0.0f;
    rain_f[i] = 0.0f;
  }
  for (int it = 0; it < RAIN_ITERS; ++it) {
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        const int i = y * W + x;
        if (g->elev[i] < 0) {
          next[i] = cap;
          continue;
        }
        const float ux = (float)x - g->wx[i];
        const float uy = (float)y - g->wy[i];
        const float up = sample(moist, ux, uy);
        const float rise = height[i] - sample(height, ux, uy);
        float dep = rise > 0.0f ? RAIN_BASE_DEP + RAIN_OROGRAPHIC * rise
                                : RAIN_BASE_DEP * RAIN_LEE_FACTOR;
        if (dep > 0.95f) {
          dep = 0.95f;
        }
        rain_f[i] = up * dep;
        next[i] = up * (1.0f - dep);
      }
    }
    memcpy(moist, next, sizeof(float) * N);
  }
  for (int i = 0; i < N; ++i) {
    g->rain[i] = g->elev[i] < 0 ? 0 : (int)(rain_f[i] * (float)RAIN_SCALE + 0.5f);
  }
  /* No desert on a shore: dry coastal land gets the minimum rain. */
  for (int k = 0; k < g->active_count; ++k) {
    const int i = g->active[k];
    if (g->elev[i] < 0 || g->rain[i] > 0) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      if (g->elev[i + MAP_DIR8_DY[d] * W + MAP_DIR8_DX[d]] < 0) {
        g->rain[i] = 1;
        break;
      }
    }
  }
}

/* Rows from (y) to the nearer active north / south edge; negative in the rim. */
static int rows_from_pole(int y) {
  return y < H / 2 ? y - EDGE_Y : H - 1 - EDGE_Y - y;
}

/*
 * Step 7. Horizontal bands Hot (3), Warm (2), Lukewarm (1), Temperate (0),
 * Cool (-1), Cold (-2) from the equator to each rim:
 * Cold is the k_cold_rows rows at each active edge, Hot..Cool split the rest
 * by the temperature axis's proportions. Then the winds smear them: each pass
 * mixes a tile half-and-half with the air arriving from upwind, with the
 * north-south reach weighted up so the bands bend rather than just blur.
 * Smear never takes Cold past its rows.
 */
static void temperature(AltGen* g, int temp_axis) {
  const int cold = k_cold_rows[temp_axis];
  const int* widths = k_band_widths[temp_axis];
  const int total = widths[0] + widths[1] + widths[2] + widths[3] + widths[4];
  const float cy = (float)(H - 1) * 0.5f;
  float* t = g->fa;
  float* next = g->fb;
  for (int y = 0; y < H; ++y) {
    int band = 5;
    if (rows_from_pole(y) >= cold) {
      const float f = fabsf((float)y - cy) / (cy - (float)(EDGE_Y + cold) + 0.5f) * (float)total;
      band = 0;
      int edge = widths[0];
      while (band < 4 && f >= (float)edge) {
        band++;
        edge += widths[band];
      }
    }
    for (int x = 0; x < W; ++x) {
      t[y * W + x] = (float)(3 - band);
    }
  }
  for (int it = 0; it < TEMP_SMEAR_ITERS; ++it) {
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        const int i = y * W + x;
        const float up = sample(t, (float)x - 2.0f * g->wx[i], (float)y - 3.0f * g->wy[i]);
        next[i] = 0.5f * t[i] + 0.5f * up;
      }
    }
    memcpy(t, next, sizeof(float) * N);
  }
  /* Jitter of up to +-0.3 band ragged-edges the band boundaries. */
  for (int i = 0; i < N; ++i) {
    const float jitter = (float)(rnd(g, 0, 6) - 3) * 0.1f;
    const int lo = rows_from_pole(i / W) >= cold ? -1 : -2;
    g->temp[i] = clamp_int((int)floorf(t[i] + jitter + 0.5f), lo, 3);
  }
}

/* ===== Terrain, relief, rivers, forest (step 8) ===== */

/* Binary min-heap of (key, tile) for the drainage flood. */
typedef struct HeapItem {
  int key;
  int i;
} HeapItem;

static void heap_push(HeapItem* h, int* n, HeapItem it) {
  int c = (*n)++;
  while (c > 0) {
    const int p = (c - 1) / 2;
    if (h[p].key <= it.key) {
      break;
    }
    h[c] = h[p];
    c = p;
  }
  h[c] = it;
}

static HeapItem heap_pop(HeapItem* h, int* n) {
  const HeapItem top = h[0];
  const HeapItem last = h[--(*n)];
  int c = 0;
  for (;;) {
    int m = 2 * c + 1;
    if (m >= *n) {
      break;
    }
    if (m + 1 < *n && h[m + 1].key < h[m].key) {
      m++;
    }
    if (h[m].key >= last.key) {
      break;
    }
    h[c] = h[m];
    c = m;
  }
  if (*n > 0) {
    h[c] = last;
  }
  return top;
}

/* Base type by temperature; relief (tile minus its lowest land 8-neighbour;
 * a shore is not a cliff) makes hills and mountains. */
static void paint_land(AltGen* g) {
  for (int i = 0; i < N; ++i) {
    const int x = i % W;
    const int y = i / W;
    if (!in_active(x, y) || g->elev[i] < 0) {
      g->terrain[i] = T_OCEAN;
      continue;
    }
    uint8_t t = k_band_terrain[g->temp[i] + 2];
    if (t != T_ARCTIC) {
      int relief = 0;
      for (int d = 0; d < 8; ++d) {
        const int nx = x + MAP_DIR8_DX[d];
        const int ny = y + MAP_DIR8_DY[d];
        const int ne = g->elev[ny * W + nx]; /* active tiles have in-map neighbours */
        const int r = g->elev[i] - ne;
        if (ne >= 0 && r > relief) {
          relief = r;
        }
      }
      if (relief >= MOUNTAIN_RELIEF) {
        t = (uint8_t)(t | F_HILL | F_MOUNTAIN);
      } else if (relief >= HILL_RELIEF) {
        t = (uint8_t)(t | F_HILL);
      }
    }
    g->terrain[i] = t;
  }
}

/* Rainfall (river bonus included) makes deserts (no rain) and dries /
 * moistens the tails beyond one standard deviation of the land mean. */
static void moisture(AltGen* g) {
  double sum = 0.0;
  double sum2 = 0.0;
  int count = 0;
  for (int i = 0; i < N; ++i) {
    const uint8_t typ = (uint8_t)(g->terrain[i] & 0x1f);
    if (is_water_type(typ) || typ == T_ARCTIC) {
      continue;
    }
    sum += g->rain[i];
    sum2 += (double)g->rain[i] * g->rain[i];
    count++;
  }
  if (count == 0) {
    return;
  }
  const double mean = sum / count;
  const double var = sum2 / count - mean * mean;
  const double sd = var > 0.0 ? sqrt(var) : 0.0;
  for (int i = 0; i < N; ++i) {
    const uint8_t feat = (uint8_t)(g->terrain[i] & 0xe0);
    uint8_t typ = (uint8_t)(g->terrain[i] & 0x1f);
    if (is_water_type(typ) || typ == T_ARCTIC) {
      continue;
    }
    if (g->rain[i] == 0) {
      typ = T_DESERT;
    } else if (g->rain[i] < mean - sd) {
      typ = typ == T_PLAINS ? T_TUNDRA
          : typ == T_PRAIRIE ? T_PLAINS
          : typ == T_GRASSLAND ? T_PRAIRIE
          : typ == T_SAVANNAH ? T_GRASSLAND
          : typ;
    } else if (g->rain[i] > mean + sd) {
      typ = typ == T_SAVANNAH ? T_SWAMP
          : typ == T_GRASSLAND ? T_MARSH
          : typ == T_PRAIRIE ? T_GRASSLAND
          : typ == T_PLAINS ? T_PRAIRIE
          : typ == T_TUNDRA ? T_PLAINS
          : typ;
    }
    g->terrain[i] = (uint8_t)(feat | typ);
  }
}

/*
 * Drainage: a priority flood from every water tile (lowest first) gives each
 * land tile a downstream neighbour along a 4-connected path that never
 * climbs, pits draining as if filled. Routing reads elevation plus a coarse
 * value-noise field (ROUTE_NOISE over ROUTE_CELL tiles, routing only, the
 * terrain keeps its elevation) so flat plate interiors form valleys that
 * gather tributaries instead of draining in parallel strips to the nearest
 * coast. Rain accumulates down that tree; RIVER_MINOR_FLOW makes a minor
 * river, RIVER_MAJOR_FLOW a major one, so wet basins grow more and bigger
 * rivers. A river reaching the sea shorter than RIVER_MIN_LEN is dropped,
 * and so is a branch that would run alongside another.
 * River tiles lose hill / mountain (the valley; DOS never stores a river on a
 * hill either), arctic never carries one, and the water tile a river enters
 * gets the river bit (estuary). A river tile gains 1 rain (major 2) before
 * moisture() turns rain into terrain.
 */
static void rivers(AltGen* g) {
  int* parent = g->scratch_a;
  int* order = g->scratch_b;
  HeapItem* heap = (HeapItem*)malloc(sizeof(HeapItem) * N);
  bool* done = (bool*)calloc(N, sizeof(bool));
  int* filled = (int*)calloc(5 * (size_t)N, sizeof(int));
  if (!heap || !done || !filled) {
    free(heap);
    free(done);
    free(filled);
    return;
  }
  int* route = filled + N;
  int* len = filled + 2 * N;
  int* mouth_len = filled + 3 * N;
  int* acc = filled + 4 * N;
  memcpy(acc, g->rain, sizeof(int) * N);

  enum { LW = W / ROUTE_CELL + 2, LH = H / ROUTE_CELL + 2 };
  int lattice[LH][LW];
  for (int ly = 0; ly < LH; ++ly) {
    for (int lx = 0; lx < LW; ++lx) {
      lattice[ly][lx] = rnd(g, 0, ROUTE_NOISE);
    }
  }
  for (int i = 0; i < N; ++i) {
    const int x = i % W;
    const int y = i / W;
    const int lx = x / ROUTE_CELL;
    const int ly = y / ROUTE_CELL;
    const int tx = x % ROUTE_CELL;
    const int ty = y % ROUTE_CELL;
    const int top = lattice[ly][lx] * (ROUTE_CELL - tx) + lattice[ly][lx + 1] * tx;
    const int bot = lattice[ly + 1][lx] * (ROUTE_CELL - tx) + lattice[ly + 1][lx + 1] * tx;
    route[i] = (top * (ROUTE_CELL - ty) + bot * ty) / (ROUTE_CELL * ROUTE_CELL);
  }

  int hn = 0;
  int no = 0;
  for (int i = 0; i < N; ++i) {
    parent[i] = -1;
    if (is_water_type(g->terrain[i])) {
      done[i] = true;
      /* Rows 0 / H-1 turn arctic in the finalize pass: no river drains there. */
      if (i / W > 0 && i / W < H - 1) {
        heap_push(heap, &hn, (HeapItem){rnd(g, 0, 7), i});
      }
    }
  }
  while (hn > 0) {
    const HeapItem cur = heap_pop(heap, &hn);
    const int x = cur.i % W;
    const int y = cur.i / W;
    for (int d = 0; d < 4; ++d) {
      const int nx = x + k_dx4[d];
      const int ny = y + k_dy4[d];
      if (nx < 0 || ny < 0 || nx >= W || ny >= H) {
        continue;
      }
      const int j = ny * W + nx;
      if (done[j]) {
        continue;
      }
      done[j] = true;
      const int e = g->elev[j] + route[j];
      filled[j] = e > filled[cur.i] ? e : filled[cur.i];
      parent[j] = cur.i;
      order[no++] = j; /* parents always precede their children */
      heap_push(heap, &hn, (HeapItem){filled[j] * 8 + rnd(g, 0, 7), j});
    }
  }

  for (int k = no - 1; k >= 0; --k) {
    const int j = order[k];
    const int p = parent[j];
    if (p >= 0 && !is_water_type(g->terrain[p])) {
      acc[p] += acc[j];
    }
  }
  /* Longest river run upstream of each river tile, then each tile's mouth. */
  for (int k = no - 1; k >= 0; --k) {
    const int j = order[k];
    if (acc[j] < RIVER_MINOR_FLOW || (g->terrain[j] & 0x1f) == T_ARCTIC) {
      continue;
    }
    if (len[j] < 1) {
      len[j] = 1;
    }
    const int p = parent[j];
    if (p >= 0 && len[j] + 1 > len[p]) {
      len[p] = len[j] + 1;
    }
  }
  for (int k = 0; k < no; ++k) {
    const int j = order[k];
    const int p = parent[j];
    mouth_len[j] = (p >= 0 && len[p] > 0 && !is_water_type(g->terrain[p])) ? mouth_len[p] : len[j];
  }

  /* Mouth first, a tile keeps its river only when its downstream tile did
   * and no other river already runs 4-adjacent to it: two channels side by
   * side draw as one blob, so the later branch (and its upstream) goes. */
  for (int k = 0; k < no; ++k) {
    const int j = order[k];
    const int p = parent[j];
    if (len[j] == 0 || mouth_len[j] < RIVER_MIN_LEN ||
        (p >= 0 && !is_water_type(g->terrain[p]) && !(g->terrain[p] & F_RIVER))) {
      continue;
    }
    bool crowded = false;
    for (int d = 0; d < 4; ++d) {
      const int nx = j % W + k_dx4[d];
      const int ny = j / W + k_dy4[d];
      const int q = ny * W + nx;
      if (q != p && !is_water_type(g->terrain[q]) && (g->terrain[q] & F_RIVER)) {
        crowded = true;
      }
    }
    if (crowded) {
      continue;
    }
    const uint8_t typ = (uint8_t)(g->terrain[j] & 0x1f);
    const bool major = acc[j] >= RIVER_MAJOR_FLOW;
    const uint8_t bits = major ? (uint8_t)(F_RIVER | F_MOUNTAIN) : (uint8_t)F_RIVER;
    g->terrain[j] = (uint8_t)(typ | bits);
    g->rain[j] += major ? 2 : 1; /* the river waters its tile */
    if (p >= 0 && is_water_type(g->terrain[p]) && in_active(p % W, p / W)) {
      g->terrain[p] = (uint8_t)(g->terrain[p] | bits);
    }
  }
  free(heap);
  free(done);
  free(filled);
}

/* Forest on FOREST_PERCENT of flat land (desert -> scrub forest); never on
 * hills, mountains or arctic. */
static void forests(AltGen* g) {
  for (int i = 0; i < N; ++i) {
    const uint8_t t = g->terrain[i];
    if ((t & 0x1f) > 7 || (t & F_HILL)) {
      continue;
    }
    if (rnd(g, 0, 99) < FOREST_PERCENT) {
      g->terrain[i] = (uint8_t)(t + T_FOREST_BIT);
    }
  }
}

/* ===== High seas & driver ===== */


/*
 * Sea lane in the shape map_generate's leaves: per row, the open water reached
 * walking west from the east edge (east half only) becomes high seas, minus
 * anything within two tiles of land; then only the run still touching the
 * east edge stays, so the Euro landfall walk finds a continuous lane.
 */
static void high_seas(uint8_t* terrain) {
  for (int y = 1; y < H - 1; ++y) {
    for (int x = W - 1; x >= W / 2 && is_water_type(terrain[y * W + x]); --x) {
      terrain[y * W + x] = (uint8_t)((terrain[y * W + x] & 0xe0) | T_HIGH_SEAS);
    }
  }
  for (int y = 1; y < H - 1; ++y) {
    for (int x = W / 2; x < W; ++x) {
      const int i = y * W + x;
      if ((terrain[i] & 0x1f) != T_HIGH_SEAS) {
        continue;
      }
      bool near_land = false;
      for (int dy = -2; dy <= 2 && !near_land; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
          const int nx = x + dx;
          const int ny = y + dy;
          if (map_dims_in_bounds(nx, ny, W, H) && !is_water_type(terrain[ny * W + nx])) {
            near_land = true;
            break;
          }
        }
      }
      if (near_land) {
        terrain[i] = (uint8_t)((terrain[i] & 0xe0) | T_OCEAN);
      }
    }
  }
  for (int y = 1; y < H - 1; ++y) {
    bool fringe = true;
    for (int x = W - 1; x >= 1; --x) {
      const int i = y * W + x;
      if ((terrain[i] & 0x1f) != T_HIGH_SEAS) {
        fringe = false;
      } else if (!fringe) {
        terrain[i] = (uint8_t)((terrain[i] & 0xe0) | T_OCEAN);
      }
    }
  }
}

bool map_generate_alternate(
  ColonizeWorldMap* out,
  const MapGenParams* params,
  char* err,
  size_t err_size
) {
  MapGenParams p = *params;
  p.land_mass = clamp_int(p.land_mass, 0, 3);
  p.land_form = clamp_int(p.land_form, 0, 3);
  p.temperature = clamp_int(p.temperature, 0, 3);
  p.climate = clamp_int(p.climate, 0, 3);

  if (!map_alloc(out, W, H, err, err_size)) {
    return false;
  }
  AltGen* g = (AltGen*)calloc(1, sizeof(AltGen));
  if (!g) {
    map_free(out);
    if (err && err_size) {
      snprintf(err, err_size, "map_generate_alternate oom");
    }
    return false;
  }
  ColonizeDosRng local;
  g->rng = params->rng;
  if (!g->rng) {
    dos_rng_seed(&local, p.seed);
    g->rng = &local;
  }
  for (int y = EDGE_Y; y < H - EDGE_Y; ++y) {
    for (int x = EDGE_X; x < W - EDGE_X; ++x) {
      g->active[g->active_count++] = y * W + x;
    }
  }

  /* Same first draw as map_generate: the specials / rumour hash seed. */
  out->prime_resource_seed = (uint16_t)dos_rng_range(g->rng, 1, 0x7fff);

  make_plates(g, &p);
  tectonics(g);
  warp_crust(g);
  erosion(g);
  winds(g);
  rainfall(g, p.climate);
  temperature(g, p.temperature);
  paint_land(g);
  rivers(g);
  moisture(g);
  forests(g);
  high_seas(g->terrain);
  memcpy(out->terrain, g->terrain, N);

  map_gen_finalize_border_and_forest(out->terrain, W, H);
  map_gen_assign_continents(out);
  map_gen_assign_euro_landfalls(out, g->rng, p.focus_nation, false);

  int land = 0;
  for (int i = 0; i < N; ++i) {
    land += !is_water_type(out->terrain[i]);
  }
  diag_info(
    "map_generate_alternate seed=%u mass=%d form=%d temp=%d clim=%d land=%d/%d plates=%d",
    p.seed,
    p.land_mass,
    p.land_form,
    p.temperature,
    p.climate,
    land,
    N,
    g->plate_count
  );
  free(g);
  return true;
}
