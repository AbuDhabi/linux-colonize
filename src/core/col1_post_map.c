#include "core/col1_post_map.h"

#include <stdlib.h>
#include <string.h>

/* DS:0xb4 / 0xbe — same order as map_gen / ai k_dir8. */
static const int k_dir8_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int k_dir8_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

static int post_terrain_byte(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->terrain || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  return (int)map->terrain[y * map->width + x];
}

static int post_terr_class(const ColonizeWorldMap* map, int x, int y) {
  /* FUN_13e4_000e / FUN_281f_078c */
  const int b = post_terrain_byte(map, x, y);
  if ((b & 0x20) != 0) {
    return ((b & 0x80) != 0) ? 0x1c : 0x1b;
  }
  return b & 0x1f;
}

static int post_is_ocean_hs(const ColonizeWorldMap* map, int x, int y) {
  /* FUN_281f_0768 */
  const int t = post_terr_class(map, x, y);
  return t == 0x19 || t == 0x1a;
}

static int post_continent_id(const ColonizeWorldMap* map, int x, int y) {
  /* FUN_281f_06b4 — layer3 low nibble */
  if (!map || !map->layer3 || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  return (int)(map->layer3[y * map->width + x] & 0x0fu);
}

static int post_land_continent(const ColonizeWorldMap* map, int x, int y) {
  /* FUN_137f_02a0 / FUN_281f_0722 — land only, else −1 */
  if (!map_coords_inset(map, x, y) || post_is_ocean_hs(map, x, y)) {
    return -1;
  }
  return post_continent_id(map, x, y);
}

/*
 * FUN_67f4_0008 — scan 2×2 at (ox,oy). Sea requires continent id 1.
 * Returns continent id, or -1; writes representative tile to *out_x / *out_y.
 */
static int post_find_rep(
  const ColonizeWorldMap* map,
  int ox,
  int oy,
  int sea_flag,
  int* out_x,
  int* out_y
) {
  for (int x = ox; x <= ox + 1; ++x) {
    for (int y = oy; y <= oy + 1; ++y) {
      if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
        continue;
      }
      if (post_is_ocean_hs(map, x, y) != (sea_flag != 0)) {
        continue;
      }
      const int cid = post_continent_id(map, x, y);
      if (sea_flag == 0 || cid == 1) {
        if (out_x) {
          *out_x = x;
        }
        if (out_y) {
          *out_y = y;
        }
        return cid;
      }
    }
  }
  return -1;
}

static int post_walkable(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int sea_flag
) {
  if (!map_coords_inset(map, x, y)) {
    return 0;
  }
  if (sea_flag) {
    return post_is_ocean_hs(map, x, y) && post_continent_id(map, x, y) == 1;
  }
  return !post_is_ocean_hs(map, x, y);
}

/*
 * FUN_6662_0906 → FUN_6662_00f2 with DS:0x1dd4=1 (uniform +1/edge).
 *
 * Asm call from FUN_67f4_0088:
 *   PUSH dest_y; PUSH sea_flag; PUSH 9; AX=from_x; DX=from_y; BX=dest_x
 * 0906 sets 1dd2 = (sea_flag==0 ? 1 : 0xd), a14e/a14c = dest, then calls
 * 00f2 with AX/DX=from and BX=budget 9 (stack slot restored into BX).
 *
 * Dest cost grid starts at 1; accept link iff 0 < cost < 8.
 *
 * Critical: 00f2 caches the last dest flood at DS:2d1a/2d1c + grid @a270.
 * Same dest and cost[from]!=0 → skip flood, leave a370=0; 0906 then returns
 * 0 when next-step succeeds. FUN_67f4 requires cost > 0, so that NE after an
 * earlier E/SE to the same dest is rejected. Without this cache we over-link.
 */
typedef struct PostCheapCache {
  int tx;
  int ty;
  int sea_flag;
  int w;
  int h;
  uint8_t* dist;
} PostCheapCache;

static void post_cheap_cache_reset(PostCheapCache* c) {
  if (!c) {
    return;
  }
  free(c->dist);
  c->dist = NULL;
  c->tx = -1;
  c->ty = -1;
  c->sea_flag = -1;
  c->w = 0;
  c->h = 0;
}

static int post_cheap_cost(
  PostCheapCache* cache,
  const ColonizeWorldMap* map,
  int sx,
  int sy,
  int tx,
  int ty,
  int sea_flag
) {
  if (sx == tx && sy == ty) {
    return 0;
  }
  if (sx - tx >= 8 || tx - sx >= 8 || sy - ty >= 8 || ty - sy >= 8) {
    return -1;
  }
  if (!post_walkable(map, sx, sy, sea_flag) || !post_walkable(map, tx, ty, sea_flag)) {
    return -1;
  }

  const int w = map->width;
  const int h = map->height;
  const size_t n = (size_t)w * (size_t)h;

  /* Cache hit: DOS skips flood, a370 stays 0 → 0906 returns 0 → no link. */
  if (cache && cache->dist && cache->tx == tx && cache->ty == ty &&
      cache->sea_flag == sea_flag && cache->w == w && cache->h == h &&
      cache->dist[(size_t)sy * (size_t)w + (size_t)sx] != 0) {
    return 0;
  }

  uint8_t* dist = cache ? cache->dist : NULL;
  if (!dist || !cache || cache->w != w || cache->h != h) {
    free(dist);
    dist = (uint8_t*)calloc(n, 1);
    if (!dist) {
      return -1;
    }
    if (cache) {
      cache->dist = dist;
      cache->w = w;
      cache->h = h;
    }
  } else {
    memset(dist, 0, n);
  }

  int qx[300];
  int qy[300];
  int qh = 0;
  int qt = 0;
  /* DOS: expand while read_idx < 0xe1 (225). */
  int reads = 0;

  dist[(size_t)ty * (size_t)w + (size_t)tx] = 1;
  qx[qt] = tx;
  qy[qt] = ty;
  qt = 1;

  /* a370 starts as budget 9; shrinks to source cost when from is dequeued. */
  int budget = 9;
  int found = -1;
  while (qh < qt && reads < 0xe1) {
    const int x = qx[qh];
    const int y = qy[qh];
    ++qh;
    ++reads;
    const int d = (int)dist[(size_t)y * (size_t)w + (size_t)x];
    if (d > budget) {
      continue;
    }
    if (x == sx && y == sy) {
      found = d;
      budget = d;
      continue;
    }
    for (int i = 0; i < 8; ++i) {
      const int nx = x + k_dir8_dx[i];
      const int ny = y + k_dir8_dy[i];
      if (nx - tx >= 8 || tx - nx >= 8 || ny - ty >= 8 || ty - ny >= 8) {
        continue;
      }
      if (!post_walkable(map, nx, ny, sea_flag)) {
        continue;
      }
      const size_t ni = (size_t)ny * (size_t)w + (size_t)nx;
      const int nd = d + 1;
      if (nd > 255) {
        continue;
      }
      if (dist[ni] == 0 || (int)dist[ni] > nd) {
        dist[ni] = (uint8_t)nd;
        if (qt < 300) {
          qx[qt] = nx;
          qy[qt] = ny;
          ++qt;
        }
      }
    }
  }

  if (found < 0 && dist[(size_t)sy * (size_t)w + (size_t)sx] != 0) {
    found = (int)dist[(size_t)sy * (size_t)w + (size_t)sx];
  }

  if (cache) {
    cache->tx = tx;
    cache->ty = ty;
    cache->sea_flag = sea_flag;
  } else {
    free(dist);
  }
  return found;
}

static void post_fill_plane(
  const ColonizeWorldMap* map,
  uint8_t* plane,
  int sea_flag,
  PostCheapCache* cache
) {
  memset(plane, 0, COLONIZE_COL1_CONNECT_PLANE_SIZE);
  /* cx: map_x = 1,5,… while < 0x3d → 15; cy: map_y = 1,5,… while < 0x49 → 18 */
  for (int cx = 0; cx < (int)COLONIZE_COL1_CONNECT_PLANE_W; ++cx) {
    const int ox = 1 + cx * 4;
    for (int cy = 0; cy < (int)COLONIZE_COL1_CONNECT_PLANE_H; ++cy) {
      const int oy = 1 + cy * 4;
      int rx = 0;
      int ry = 0;
      const int cid = post_find_rep(map, ox, oy, sea_flag, &rx, &ry);
      if (cid < 0) {
        continue;
      }
      for (int d = 0; d < 4; ++d) {
        const int fox = ox + k_dir8_dx[d] * 4;
        const int foy = oy + k_dir8_dy[d] * 4;
        if (!map_coords_inset(map, fox, foy)) {
          continue;
        }
        int fx = 0;
        int fy = 0;
        const int fid = post_find_rep(map, fox, foy, sea_flag, &fx, &fy);
        if (fid < 0 || fid != cid) {
          continue;
        }
        /* Path from current rep → neighbor rep; budget 9; cache is per dest. */
        const int cost = post_cheap_cost(cache, map, rx, ry, fx, fy, sea_flag);
        if (cost <= 0 || cost >= 8) {
          continue;
        }
        plane[cx * 18 + cy] = (uint8_t)(plane[cx * 18 + cy] | (uint8_t)(1u << d));
        const int nx = cx + k_dir8_dx[d];
        const int ny = cy + k_dir8_dy[d];
        if (nx >= 0 && nx < (int)COLONIZE_COL1_CONNECT_PLANE_W && ny >= 0 &&
            ny < (int)COLONIZE_COL1_CONNECT_PLANE_H) {
          plane[nx * 18 + ny] =
            (uint8_t)(plane[nx * 18 + ny] | (uint8_t)(1u << ((d + 4) & 7)));
        }
      }
    }
  }
}

static void post_fill_tallies(const ColonizeWorldMap* map, ColonizeCol1PostMap* out) {
  memset(out->continent_tally_a, 0, sizeof(out->continent_tally_a));
  memset(out->continent_tally_b, 0, sizeof(out->continent_tally_b));
  for (int y = 0; y < map->height; ++y) {
    for (int x = 0; x < map->width; ++x) {
      if (!map_coords_inset(map, x, y)) {
        continue;
      }
      /* Inset land only (FUN_281f_0302 + non-ocean). */
      if (post_is_ocean_hs(map, x, y)) {
        continue;
      }
      const int cont = post_land_continent(map, x, y);
      if (cont < 0 || cont >= 16) {
        continue;
      }
      out->continent_tally_b[cont]++;
      const int cls = post_terr_class(map, x, y);
      if (cls < 0x18) {
        const int low = cls & 7;
        if (low >= 2 && low <= 5) {
          out->continent_tally_a[cont]++;
        }
      }
    }
  }
}

bool col1_post_map_is_blank(const ColonizeCol1PostMap* pm) {
  if (!pm) {
    return true;
  }
  const uint8_t* p = (const uint8_t*)pm;
  for (size_t i = 0; i < sizeof(*pm); ++i) {
    if (p[i] != 0) {
      return false;
    }
  }
  return true;
}

void col1_post_map_rebuild_connectivity(
  ColonizeCol1PostMap* out,
  const ColonizeWorldMap* map
) {
  if (!out || !map || !map->terrain || !map->layer3) {
    return;
  }
  if (map->width != COLONIZE_COL1_MAP_W_STD || map->height != COLONIZE_COL1_MAP_H_STD) {
    return;
  }

  /* Preserve 10-byte tail; clear planes + tallies then fill (FUN_67f4_0088). */
  uint8_t tail[10];
  memcpy(tail, out->unknown_post_604, 4);
  memcpy(tail + 4, out->unknown_ds_8d80, 4);
  memcpy(tail + 8, &out->prime_resource_seed, 2);

  memset(out, 0, sizeof(*out));
  /* Shared 00f2 dest cache across land then sea (DOS does not clear 2d1a). */
  PostCheapCache cache;
  memset(&cache, 0, sizeof(cache));
  cache.tx = -1;
  cache.ty = -1;
  cache.sea_flag = -1;
  /* Pass 0 = land @ DS:0x85e8; pass 1 = sea @ DS:0x86f6 */
  post_fill_plane(map, out->land_connectivity, 0, &cache);
  post_fill_plane(map, out->sea_connectivity, 1, &cache);
  post_cheap_cache_reset(&cache);
  post_fill_tallies(map, out);

  memcpy(out->unknown_post_604, tail, 4);
  memcpy(out->unknown_ds_8d80, tail + 4, 4);
  memcpy(&out->prime_resource_seed, tail + 8, 2);
  /* Blank-template export: stamp mapgen seed when live map has one. */
  if (out->prime_resource_seed == 0 && map->prime_resource_seed != 0) {
    out->prime_resource_seed = map->prime_resource_seed;
  }
}
