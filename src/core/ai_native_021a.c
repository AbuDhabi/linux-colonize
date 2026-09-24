#include "core/internal.h"
#include "core/ai.h"
#include "core/combat_strength.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/founding_fathers.h"
#include "core/ai_goals.h"
#include "core/ai_king.h"
#include "core/col1_bridge.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/ai_internal.h"
#include "core/dos_rng.h"
#include "core/map_gen.h"
#include "core/new_game.h"
#include "core/strutil.h"
#include "core/turn.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * Sections:
 *  - 021a direction-scorer structural port
 */

/*
 * ===========================================================================
 * FUN_4d56_021a — the REAL quiet Brave move picker (2026-09-15).
 *
 * The 20e6 "quiet Brave" branch above was never what DOS runs for an Indian
 * unit. 4d56:021a..14fd (ndisasm of overlay 13, origin 0x21a) carries its OWN
 * nine-way direction loop (dirs 0..7 plus index 8 = stay, DS:0xb4/0xbe both
 * hold a 9th (0,0) entry). The far call at 021a:1182 that the docs read as
 * "reaches the 521d scorer through thunk 291f:012c" is FUN_7a65_0008 — the
 * on-map debug number plotter behind DEBUG.TXT "Indian AI movement" (DS:0x894
 * bit 1), called
 * with (x, y, score, colour 0xf). It never scores anything.
 *
 * Shape (asm offsets are 4d56:xxxx):
 *   0x21a  prologue: cool = turn - turn/25; unit fa/river; 0bce adjacent
 *          foreign probe (+ DS:0x8cfa nation); 15eb_0142 nearest colony of
 *          any nation on this continent; home village (destroy if bad);
 *          home unreachable when on another continent; "encroaching" colony
 *          = dist(colony, village) <= max(2, pop/2); Euro military stacks
 *          adjacent to the VILLAGE (count - colony pop/4, total >= 2);
 *          angry = #(alarm >= 75) + #(village attitude >= 0x80) over Euros.
 *   0x5aa  for d in 0..8: score = 200, flags = 0
 *          skip ocean/HS, DOS rumour tile (137f_0598), arctic 0x18
 *          owner / presence / settlement owner / fa / river / prime resource
 *          foreign owner: flags|=1, grudge/hostile (|=0x40 / |=4)
 *          occupied by foreign: flags|=2
 *          !occupied: adjacent Euro colony (100) / wagon (50) + alarm/2, best
 *            -> cooldown gate, += val + 2*(cool - last_visit), flags|=0x80
 *          !occupied & no visit & encroaching colony & cooldown: += (12-d)*10
 *          occupied: other tribe -> skip; land only -> -= 50-alarm; units ->
 *            +4*found[terr], per-type stack bonuses (treasure/artillery/wagon
 *            loot arms with RNG(50,100) and RNG(0,7) draws), colony +20/+10,
 *            -= 50-alarm, need loot or grudge, flags|=8 (attack)
 *          own village tile: musket/horse upgrade +20
 *          own unit on dest: 2+ off-village -> skip, else -40
 *          stay: (1-stack)*40, -25 (attack intent or RNG(0,(tech+1)*4)==0)
 *          facing +4 / +3 adjacent (byte math: facing 8 counts 1 and 7) / -6
 *          road pair +4 else cardinal river pair +4
 *          home tether: dist>2 -> -= 3*dist (>>1 angry, >>1 armed, >>2 mtd)
 *          09dc adjacent-foreign at dest: Euro -> +50 unmet, +(alarm-50)/4
 *            when alarm >= 25; other tribe -> -25
 *          stay-only arms (adjacent foreign at own tile, no-stay unless upg)
 *          !angry: unclaimed +5, drift toward nearest colony
 *            += (tier(alarm)+1)*(12-d)>>2 within 12
 *          angry: +5, +10 resource, colony +500, unit stack combat estimate,
 *            hostile colony -2*d (met) else drift
 *          += RNG(1,5); clamp 0; strict > keeps first
 *   0x11ae facing = dir (8 on stay); stay: orders 5/6 latch + in-field
 *          arm/mount; move: orders = 0
 *   0x1277 tail: contact_state[owner]==2 -> stay; ==1 stamp; contact/attack
 *          need 3 thirds left; human-colony warning chrome (not ported);
 *          pending-encounter bit (unit +0x3148 & 8) -> encounter, stay;
 *          peaceful visit (0x80): last_visit = turn, bit 8 set;
 *          encroach march via 6662_0f74 pathfinder after the long cooldown;
 *          foreign-land quiet move with spent > 0 -> stay.
 *
 * DOS unit +0x12 word (COL1 cargo_hold[2..3], the "spawn turn stamp" with
 * "no reader found") is this routine's last-visit stamp: port keeps it in
 * col1_hold_raw[6..7].
 * ===========================================================================
 */

static AiNativeScorePlotFn s_native_score_plot;
static void* s_native_score_plot_user;

void ai_set_native_score_plot(AiNativeScorePlotFn fn, void* user) {
  s_native_score_plot = fn;
  s_native_score_plot_user = user;
}

/* ===================== 021a direction-scorer structural port (tile/occupant/terrain/angry/score) (ai_021a_settle_owner .. ai_021a_trace_enabled) ===================== */
int ai_021a_settle_owner(const ColonizeWorldMap* map, int x, int y) {
  /* FUN_281f_06be -> FUN_137f_03e4: layer2 bit 0x02 then the owner nibble. */
  if (!map || !map->layer2 || !map_coords_inset(map, x, y)) {
    return -1;
  }
  if ((map->layer2[(size_t)y * (size_t)map->width + (size_t)x] & 0x02u) == 0) {
    return -1;
  }
  return ai_owner_nibble(map, x, y);
}

COLONIZE_INTERNAL int ai_021a_colony_at(const ColonizeColonyPool* colonies, int x, int y) {
  /* FUN_15eb_0a76: colony index at tile (pool order == COL1 order). */
  if (!colonies) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (c->active && c->x == x && c->y == y) {
      return i;
    }
  }
  return -1;
}

COLONIZE_INTERNAL int ai_021a_type_attack(const ColonizeUnitPool* units, int type_index) {
  const ColonizeUnitType* t = units_type(units, type_index);
  return t ? t->attack : 0;
}

/* FUN_1427_0fec / 0fc0 — armed / mounted type sets. */
COLONIZE_INTERNAL int ai_021a_type_armed(int t) {
  return t == 1 || t == 4 || t == 0xb || t == 0x14 || t == 0x16;
}
COLONIZE_INTERNAL int ai_021a_type_mounted(int t) {
  return t == 4 || t == 5 || t == 0x15 || t == 0x16;
}

/*
 * FUN_1427_0b08 (via 0bce) / FUN_1427_09dc: first adjacent foreign owner on
 * the same medium. 0b08 checks unit presence then settlement per tile (the
 * settlement write wins), stopping after the first tile that found one; 09dc
 * checks settlement first, else presence, stopping at the first hit. Both
 * leave the nation in DS:0x8cfa.
 */
static int s_021a_adj_fx = -1;
static int s_021a_adj_fy = -1;
COLONIZE_INTERNAL int ai_021a_adjacent_foreign(
  const ColonizeWorldMap* map, int x, int y, int nation_id, int settlement_first
) {
  const int medium = ai_is_ocean_hs(map, x, y);
  int found = -1;
  s_021a_adj_fx = s_021a_adj_fy = -1;
  for (int d = 0; d < 8 && found < 0; ++d) {
    const int nx = x + k_ai_dir8_dx[d];
    const int ny = y + k_ai_dir8_dy[d];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    if (ai_is_ocean_hs(map, nx, ny) != medium) {
      continue;
    }
    const int p = map_tile_owner_or_presence(map, nx, ny);
    const int s = ai_021a_settle_owner(map, nx, ny);
    if (settlement_first) {
      const int o = s >= 0 ? s : p;
      if (o >= 0 && o != nation_id) {
        found = o;
      }
    } else {
      if (p >= 0 && p != nation_id) {
        found = p;
      }
      if (s >= 0 && s != nation_id) {
        found = s;
      }
    }
    if (found >= 0) {
      s_021a_adj_fx = nx;
      s_021a_adj_fy = ny;
    }
  }
  return found;
}

COLONIZE_INTERNAL int ai_021a_visit_turn(const ColonizeUnit* u) {
  if (!u->col1_hold_raw_valid) {
    return 0;
  }
  return (int)(int16_t)((unsigned)u->col1_hold_raw[6] | ((unsigned)u->col1_hold_raw[7] << 8));
}

COLONIZE_INTERNAL void ai_021a_set_visit_turn(ColonizeUnit* u, int turn) {
  u->col1_hold_raw[6] = (uint8_t)(turn & 0xff);
  u->col1_hold_raw[7] = (uint8_t)((turn >> 8) & 0xff);
  u->col1_hold_raw_valid = 1;
}

/* FUN_15dc_00a2 alarm tier: <25 0, <50 1, <75 2, else 3. */
COLONIZE_INTERNAL int ai_021a_alarm_tier(int alarm) {
  if (alarm < 0x19) return 0;
  if (alarm < 0x32) return 1;
  if (alarm < 0x4b) return 2;
  return 3;
}

/* struct ai_021a_ctx / Ai021aDirStatus now live in ai_internal.h. */

/*
 * 021a:0x59a-0x8f7 — tile facts, owner/grudge, occupancy, the adjacent-visit
 * scan and the encroachment pull. Extracted verbatim from
 * ai_native_pick_dir_021a.
 */
COLONIZE_INTERNAL Ai021aDirStatus ai_021a_dir_tile(struct ai_021a_ctx* c) {
  const ColonizeWorldMap* const map = c->map;
  const ColonizeUnitPool* const units = c->units;
  const ColonizeCol1Save* const col1 = c->col1;
  const int nation_id = c->nation_id;
  const int x = c->x;
  const int y = c->y;
  const int cool = c->cool;
  const int dump = c->dump;
  const ColonizeColony* const col = c->col;
  const ColonizeCol1Tribe* const village = c->village;
  const int vx = c->vx;
  const int vy = c->vy;
  const int encroach = c->encroach;
  const int threat_nation = c->threat_nation;
  const int visit_turn = c->visit_turn;
  const int d = c->d;
  int grudge = c->grudge;

  const int nx = x + (d < 8 ? k_ai_dir8_dx[d] : 0);
  const int ny = y + (d < 8 ? k_ai_dir8_dy[d] : 0);
  int score = 200;
  int flags = 0;
  const int terr = ai_dos_terr_class(map, nx, ny);
  if (terr == 0x19 || terr == 0x1a) {
    if (dump) fprintf(stderr, "AI_021A d=%d skip water\n", d);
    c->grudge = grudge; /* the grudge latch outlives the skip */
    return AI_021A_DIR_SKIP;
  }
  if (map_dos_0598_rumour_tile(map, nx, ny)) {
    if (dump) fprintf(stderr, "AI_021A d=%d skip rumour dest=(%d,%d)\n", d, nx, ny);
    c->grudge = grudge; /* the grudge latch outlives the skip */
    return AI_021A_DIR_SKIP;
  }
  if (terr == 0x18) {
    if (dump) fprintf(stderr, "AI_021A d=%d skip arctic\n", d);
    c->grudge = grudge; /* the grudge latch outlives the skip */
    return AI_021A_DIR_SKIP;
  }
  const int owner = ai_owner_nibble(map, nx, ny);
  const int presence = map_tile_owner_or_presence(map, nx, ny);
  const int settle = ai_021a_settle_owner(map, nx, ny);
  const int dfa = ai_mask_fa_flags(map, nx, ny) & 0x0a;
  const int driver = (int)(map_get_terrain_or(map, nx, ny, 25) & 0x40u);
  const int dres = map_resource_type_at(map, nx, ny) >= 0;
  int hostile = 0;
  int att = 0;
  if (owner < 0 || owner == nation_id) {
    hostile = 0;
    grudge = 0;
    att = 0;
  } else {
    flags |= 1;
    att = (owner < 4 && village) ? col1_tribe_attitude(village, owner) : 0;
    if (owner >= 4) {
      hostile = 0;
    } else {
      grudge = att >= 0x80;
      hostile = ai_diplo_indian_alarm(col1, nation_id, owner) >= 0x4b;
      if (hostile) {
        grudge = 1;
      }
      if (owner == threat_nation) {
        grudge = 1;
      }
    }
    if (hostile) {
      flags |= 4;
    }
    if (grudge) {
      flags |= 0x40;
    }
  }
  int occ = 0;
  {
    int t = presence >= 0 ? presence : settle;
    if (t == nation_id) {
      t = -1;
    }
    if (t >= 0) {
      flags |= 2;
      occ = 1;
    }
  }
  int visit_nation = -1;
  int visit_val = -1;
  if (!occ) {
    for (int n = 0; n < 8; ++n) {
      const int ax = nx + k_ai_dir8_dx[n];
      const int ay = ny + k_ai_dir8_dy[n];
      if (ax == x && ay == y) {
        continue;
      }
      int val = 100;
      int e = ai_021a_settle_owner(map, ax, ay);
      if (e < 0) {
        /* 021a:07dc calls FUN_281f_07e0: read the terminal tile-chain
         * unit, whose nation/type decide whether an adjacent wagon counts. */
        const int uid = units_tile_head_id_at(units, ax, ay);
        const ColonizeUnit* au = uid >= 0 ? units_get_const(units, uid) : NULL;
        if (!au) {
          continue;
        }
        if (au->nation_id == nation_id) {
          continue;
        }
        if (au->type_index != UNITS_KIND_WAGON) {
          continue;
        }
        e = au->nation_id;
        val >>= 1;
      }
      if (e < 0 || e >= 4) {
        continue;
      }
      val += ai_diplo_indian_alarm(col1, nation_id, e) >> 1;
      if (val >= visit_val) {
        visit_val = val;
        visit_nation = e;
      }
    }
  }
  const int vdist = map_dos_dist(nx - vx, ny - vy);
  if (visit_nation >= 0) {
    att = village ? col1_tribe_attitude(village, visit_nation) : 0;
    grudge = att >= 0x80;
    hostile = ai_diplo_indian_alarm(col1, nation_id, visit_nation) >= 0x4b;
    if (!grudge && !hostile) {
      if (cool - visit_turn < vdist * 2 + 5) {
        if (dump) fprintf(stderr, "AI_021A d=%d skip visit-cooldown\n", d);
        c->grudge = grudge; /* the grudge latch outlives the skip */
        return AI_021A_DIR_SKIP;
      }
    }
    visit_val += (cool - visit_turn) * 2;
    score += visit_val;
    if (!hostile) {
      flags |= 0x80;
    }
  }
  if (!occ && visit_nation < 0 && encroach >= 0 && col) {
    const int cn = col->nation_id;
    const int census = (cn >= 0 && cn < 4) ? (int)col1->stuff.census_pop_proxy[cn] : 0;
    const int cx = (census >> 3) + encroach * 2 + 5;
    if (cx <= cool - visit_turn) {
      const int dcol = map_dos_dist(col->x - nx, col->y - ny);
      if (dcol < 12) {
        score += (12 - dcol) * 10;
      }
    }
  }

  c->nx = nx;
  c->ny = ny;
  c->score = score;
  c->flags = flags;
  c->terr = terr;
  c->owner = owner;
  c->presence = presence;
  c->settle = settle;
  c->dfa = dfa;
  c->driver = driver;
  c->dres = dres;
  c->hostile = hostile;
  c->att = att;
  c->occ = occ;
  c->visit_nation = visit_nation;
  c->visit_val = visit_val;
  c->vdist = vdist;
  c->grudge = grudge;
  return AI_021A_DIR_OK;
}

/* 021a:0x8f8-0xbb5 — occupied-destination attack intent / alarm arms. */
COLONIZE_INTERNAL Ai021aDirStatus ai_021a_dir_occupant(struct ai_021a_ctx* c) {
  AiRng* const rng = c->rng;
  const ColonizeUnitPool* const units = c->units;
  const ColonizeCol1Save* const col1 = c->col1;
  const int nation_id = c->nation_id;
  const int dump = c->dump;
  const int threat = c->threat;
  const int lone = c->lone;
  const int d = c->d;
  const int nx = c->nx;
  const int ny = c->ny;
  const int terr = c->terr;
  const int owner = c->owner;
  const int presence = c->presence;
  const int settle = c->settle;
  const int att = c->att;
  const int occ = c->occ;
  const int vdist = c->vdist;
  int grudge = c->grudge;
  int score = c->score;
  int flags = c->flags;

  int attack_intent = 0;
  int alarm = 0;
  if (occ) {
    if (owner >= 4) {
      if (dump) fprintf(stderr, "AI_021A d=%d skip other-tribe\n", d);
      c->grudge = grudge; /* the grudge latch outlives the skip */
      return AI_021A_DIR_SKIP;
    }
    alarm = ai_diplo_indian_alarm(col1, nation_id, owner);
    if (alarm > 100) {
      alarm = 100;
    }
    if (presence < 0 && settle < 0) {
      score -= 50 - alarm;
    } else {
      int loot = 0;
      int want = 0;
      int treasure = 0;
      if (presence >= 0) {
        score += map_dos_terr_found_score_byte(terr & 31) << 2;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* su = &units->units[i];
          if (!su->active || su->aboard_ship_id >= 0 || su->x != nx || su->y != ny) {
            continue;
          }
          const int st = su->type_index;
          switch (st) {
            case 0:
              score += 5;
              loot = lone; /* 0xa42 -> 0xa4c */
              break;
            case 1:
            case 4:
              if (vdist <= 1) {
                score += (threat + 5) * 4;
              }
              break;
            case 2:
              score += 10;
              loot = lone; /* 0xa48 -> 0xa4c */
              break;
            case 5:
              score += 10;
              break;
            case 0xa:
              if (lone) {
                score += 50;
                loot = 1;
              }
              treasure = 1;
              break;
            case 0xb:
              score += 35;
              want = 1;
              loot = lone;
              break;
            case 0xc: {
              score += 2;
              int holds = 0;
              for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
                if (su->hold_goods_amount[h] > 0 && su->hold_goods_amount[h] < 255) {
                  holds++;
                }
              }
              if (holds != 0) {
                want = 1;
                flags |= 0x10;
                score += 8;
              }
              if (!want) {
                if (ai_rng_range(rng, 50, 100) >= alarm) {
                  break;
                }
              }
              /* +0x315c/+0x315e both negative: not in a transport chain. */
              if (su->aboard_ship_id < 0) {
                score += 10;
                want = 1;
                loot = 1; /* 0xaa8: ax=1 -> [bp-0x20] */
              }
              break;
            }
            default:
              break;
          }
        }
      }
      if (settle >= 0) {
        if (alarm >= 0x4b) {
          score += 0x14;
        }
        if (alarm >= 0x32) {
          score += 10;
        }
      }
      if (alarm <= 0x19 && !grudge) {
        if (!loot) {
          c->grudge = grudge; /* the grudge latch outlives the skip */
          return AI_021A_DIR_SKIP;
        }
        if (!treasure) {
          c->grudge = grudge; /* the grudge latch outlives the skip */
          return AI_021A_DIR_SKIP;
        }
        if (ai_rng_range(rng, 0, 7) != 0) {
          c->grudge = grudge; /* the grudge latch outlives the skip */
          return AI_021A_DIR_SKIP;
        }
      }
      if (settle < 0 && att >= 0x20) {
        grudge = 1;
      }
      score -= 0x32 - alarm;
      if (!loot && !grudge) {
        c->grudge = grudge; /* the grudge latch outlives the skip */
        return AI_021A_DIR_SKIP;
      }
      flags |= 8;
      attack_intent = 1;
    }
  }

  c->score = score;
  c->flags = flags;
  c->attack_intent = attack_intent;
  c->alarm = alarm;
  c->grudge = grudge;
  return AI_021A_DIR_OK;
}

/* 021a:0xbb6-0xd5b — upgrade beacon, facing/road/river bias, home tether. */
COLONIZE_INTERNAL Ai021aDirStatus ai_021a_dir_terrain(struct ai_021a_ctx* c) {
  AiRng* const rng = c->rng;
  const ColonizeWorldMap* const map = c->map;
  const ColonizeUnitPool* const units = c->units;
  const ColonizeCol1Save* const col1 = c->col1;
  const ColonizeUnit* const u = c->u;
  const int nation_id = c->nation_id;
  const int unit_fa = c->unit_fa;
  const int unit_river = c->unit_river;
  const int dump = c->dump;
  const int adj_foreign = c->adj_foreign;
  const int adj_nation = c->adj_nation;
  const int vx = c->vx;
  const int vy = c->vy;
  const int home_reach = c->home_reach;
  const int angry = c->angry;
  const int self_stack = c->self_stack;
  const ColonizeCol1Indian* const ind = c->ind;
  const int tech = c->tech;
  const int facing = c->facing;
  const int d = c->d;
  const int nx = c->nx;
  const int ny = c->ny;
  const int presence = c->presence;
  const int settle = c->settle;
  const int dfa = c->dfa;
  const int driver = c->driver;
  const int attack_intent = c->attack_intent;
  int score = c->score;

  /* 0xbb6: own village tile — musket / horse upgrade beacon. */
  int upg = 0;
  if (settle == nation_id && ind) {
    if ((int8_t)ind->muskets > 0 &&
        (u->type_index == UNITS_KIND_BRAVE ||
         u->type_index == UNITS_KIND_MTD_BRAVE)) {
      score += 0x14;
      upg = 1;
    }
    if (ind->horse_breeding >= 0x19 && units_max_mp(units, u->id) <= 3) {
      score += 0x14;
      upg = 1;
    }
  }
  /* 0xc0b */
  if (d != 8) {
    if (presence >= 0 && presence == nation_id) {
      if (units_count_at(units, nx, ny) >= 2 && settle < 0) {
        if (dump) fprintf(stderr, "AI_021A d=%d skip own-stack\n", d);
        return AI_021A_DIR_SKIP;
      }
      score -= 0x28;
    }
  } else {
    score += (1 - self_stack) * 0x28;
    if (attack_intent) {
      score -= 0x19;
    } else if (ai_rng_range(rng, 0, (tech + 1) * 4) == 0) {
      score -= 0x19;
    }
  }
  /* 0xc82: facing / road / river */
  if (d != 8) {
    if (facing == d) {
      score += 4;
    } else if (((facing + 1) & 7) == d || ((facing - 1) & 7) == d) {
      score += 3;
    } else if (((facing ^ 4) & 0xff) == d) {
      score -= 6;
    }
    if (dfa && unit_fa) {
      score += 4;
    } else if ((d & 1) == 0 && driver && unit_river) {
      score += 4;
    }
  } else {
    /* 0xdf6: stay-only arms */
    if (!angry) {
      if (adj_foreign) {
        if (adj_nation < 4) {
          const int a = ai_diplo_indian_alarm(col1, nation_id, adj_nation);
          if (ai_021a_alarm_tier(a) > 0) {
            score += ((a - 0x32) >> 1) + 8;
          }
        }
      } else if (!upg) {
        if (dump) fprintf(stderr, "AI_021A d=8 skip stay (tech roll drawn)\n");
        return AI_021A_DIR_SKIP;
      }
    } else if (adj_foreign && adj_nation < 4) {
      const int a = ai_diplo_indian_alarm(col1, nation_id, adj_nation);
      if (a >= 0x5f) {
        score += 0x10;
      } else if (ai_021a_alarm_tier(a) > 0) {
        score += ((a - 0x32) >> 1) + 5;
      }
    }
  }
  /* 0xcea: home tether */
  if (home_reach >= 0) {
    const int hd = map_dos_dist(nx - vx, ny - vy);
    if (hd > 2) {
      int t = hd * 3;
      if (angry) {
        t >>= 1;
      }
      if (ai_021a_type_armed(u->type_index)) {
        t >>= 1;
      }
      if (ai_021a_type_mounted(u->type_index)) {
        t >>= 2;
      }
      score -= t;
    }
  }
  /* 0xd5c: FUN_1427_09dc adjacent foreign at dest */
  {
    const int fn = ai_021a_adjacent_foreign(map, nx, ny, nation_id, 1);
    if (fn >= 0) {
      if (fn < 4) {
        const uint8_t rel = ai_diplo_read(col1, nation_id, fn);
        if ((rel & 0x20) == 0) {
          score += 0x32;
        }
        const int a = ai_diplo_indian_alarm(col1, nation_id, fn);
        if (ai_021a_alarm_tier(a) > 0) {
          score += (a - 0x32) >> 2;
        }
      } else {
        score -= 0x19;
      }
    }
  }

  c->score = score;
  c->upg = upg;
  return AI_021A_DIR_OK;
}

/* 021a:0xeba-0x1157 — the quiet / angry destination bands. */
COLONIZE_INTERNAL Ai021aDirStatus ai_021a_dir_angry(struct ai_021a_ctx* c) {
  const ColonizeWorldMap* const map = c->map;
  const ColonizeUnitPool* const units = c->units;
  const ColonizeCol1Save* const col1 = c->col1;
  const ColonizeColonyPool* const colonies = c->colonies;
  const ColonizeUnit* const u = c->u;
  const int nation_id = c->nation_id;
  const int continent = c->continent;
  const ColonizeColony* const col = c->col;
  const int angry = c->angry;
  const int grudge = c->grudge;
  const int nx = c->nx;
  const int ny = c->ny;
  const int owner = c->owner;
  const int presence = c->presence;
  const int settle = c->settle;
  const int dres = c->dres;
  const int hostile = c->hostile;
  int score = c->score;

  /* 0xeba */
  if (!angry) {
    if (owner < 0) {
      score += 5;
    }
    if (col && ai_continent_id(map, nx, ny) == continent) {
      const int dcol = map_dos_dist(col->x - nx, col->y - ny);
      const int a = ai_diplo_indian_alarm(col1, nation_id, col->nation_id);
      if (dcol < 12) {
        score += ((ai_021a_alarm_tier(a) + 1) * (12 - dcol)) >> 2;
      }
    }
  } else {
    if (!grudge && !hostile) {
      if (settle >= 0) {
        return AI_021A_DIR_SKIP;
      }
    } else {
      score += 5;
      if (dres) {
        score += 10;
      }
      const int ci = ai_021a_colony_at(colonies, nx, ny);
      if (ci >= 0) {
        score += 500;
      } else if (presence >= 0) {
        ColonizeCombatStrengthCtx sctx;
        sctx.units = units;
        sctx.map = map;
        sctx.colonies = colonies;
        sctx.col1 = col1;
        const int def_id = units_best_defender_at(units, col1, nx, ny, u->id, -1);
        const int atk = (combat_unit_base_x8(&sctx, u->id, 1, NULL) * 3) >> 1;
        int defs = 0;
        if (def_id >= 0) {
          defs = combat_engagement_strength(&sctx, def_id, u->id, NULL);
          const ColonizeUnit* du = units_get_const(units, def_id);
          if (du && du->type_index == UNITS_KIND_ARTILLERY) {
            defs >>= 3;
          }
        }
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* su = &units->units[i];
          if (!su->active || su->aboard_ship_id >= 0 || su->x != nx || su->y != ny) {
            continue;
          }
          /*
           * DOS-LITERAL 4d56:0x1036-0x105c — the ANGRY band's own stack
           * scorer is a flat `cmp ax,0xc / ja / jmp [cs:bx+0x1044]` jump
           * table over @UNIT types 0..0x0c (bugs.md #811, ndisasm of RTLink
           * overlay segment 13 at origin 0):
           *   0x1044 words: 1024 1030 101e 101e 102a 101e 105e 105e 105e
           *                 105e 1018 1018 1018
           *   0x1018 `add word [bp-0x24],byte +0x10`  types 0x0a/0x0b/0x0c
           *   0x101e `add ... +0x08`                  types 2/3/5
           *   0x1024 `add ... +0x04`                  type 0
           *   0x102a `dec word [bp-0x24]`             type 4
           *   0x1030 `sub ... byte +0x02`             type 1
           *   0x105e  (fall through, no delta)        types 6..9
           * Treasure, Artillery and Wagon Train share ONE table slot here:
           * the equality is DOS, not a port flattening. The per-type loot
           * arms with the RNG(50,100) / RNG(0,7) draws live in the OTHER
           * band (0x5aa, asm 0xa40-0xab0) and are ported at the `case 0xa/
           * 0xb/0xc` switch earlier in this file — do not copy them here.
           */
          switch (su->type_index) {
            case UNITS_KIND_COLONIST: score += 4; break;   /* 0x1024 */
            case UNITS_KIND_SOLDIER: score -= 2; break;    /* 0x1030 */
            case UNITS_KIND_PIONEER:
            case UNITS_KIND_MISSIONARY:
            case UNITS_KIND_SCOUT: score += 8; break;      /* 0x101e */
            case UNITS_KIND_DRAGOON: score -= 1; break;    /* 0x102a */
            case UNITS_KIND_TREASURE:
            case UNITS_KIND_ARTILLERY:
            case UNITS_KIND_WAGON: score += 0x10; break;   /* 0x1018 */
            default: break;
          }
        }
        if (defs <= atk) {
          score += (atk - defs) + 0x1e;
        } else {
          score += (atk - defs) * 2;
        }
      }
    }
    if (col && ai_continent_id(map, nx, ny) == continent) {
      const int dcol = map_dos_dist(col->x - nx, col->y - ny);
      const uint8_t rel = ai_diplo_read(col1, nation_id, col->nation_id);
      const int a = ai_diplo_indian_alarm(col1, nation_id, col->nation_id);
      if (a >= 0x4b) {
        if (rel & 0x20) {
          score -= dcol * 2;
        }
      } else if (dcol < 12) {
        score += ((ai_021a_alarm_tier(a) + 1) * (12 - dcol)) >> 2;
      }
    }
  }

  c->score = score;
  return AI_021A_DIR_OK;
}

/* One direction: the four scoring stages plus the 021a:0x1158 roll/pick tail. */
COLONIZE_INTERNAL void ai_021a_score_dir(struct ai_021a_ctx* c) {
  if (ai_021a_dir_tile(c) == AI_021A_DIR_SKIP) {
    return;
  }
  if (ai_021a_dir_occupant(c) == AI_021A_DIR_SKIP) {
    return;
  }
  if (ai_021a_dir_terrain(c) == AI_021A_DIR_SKIP) {
    return;
  }
  if (ai_021a_dir_angry(c) == AI_021A_DIR_SKIP) {
    return;
  }

  AiRng* const rng = c->rng;
  const ColonizeWorldMap* const map = c->map;
  const ColonizeUnitPool* const units = c->units;
  const int nation_id = c->nation_id;
  const int unit_fa = c->unit_fa;
  const int unit_river = c->unit_river;
  const int dump = c->dump;
  const int vx = c->vx;
  const int vy = c->vy;
  const int d = c->d;
  const int nx = c->nx;
  const int ny = c->ny;
  const int flags = c->flags;
  const int terr = c->terr;
  const int owner = c->owner;
  const int presence = c->presence;
  const int settle = c->settle;
  const int dfa = c->dfa;
  const int driver = c->driver;
  const int dres = c->dres;
  const int occ = c->occ;
  const int visit_nation = c->visit_nation;
  const int visit_val = c->visit_val;
  const int vdist = c->vdist;
  int score = c->score;
  int best = c->best;
  int best_dir = c->best_dir;
  int best_flags = c->best_flags;

  /* 0x1158 */
  const int roll = ai_rng_range(rng, 1, 5);
  score += roll;
  if (score < 0) {
    score = 0;
  }
  /* 021a:1172..1182: FUN_7a65_0008 plots every accepted, clamped score. */
  if (c->score_tile_count < 9) {
    AiNativeScoreTile* tile = &c->score_tiles[c->score_tile_count++];
    tile->x = nx;
    tile->y = ny;
    tile->score = score;
  }
  if (dump) {
    fprintf(
      stderr,
      "AI_021A d=%d dest=(%d,%d) score=%d roll=%d flags=%02x terr=%02x own=%d pres=%d set=%d "
      "fa=%d/%d riv=%d/%d res=%d vdist=%d adjf=%d visit=%d/%d occ=%d rum=%d hd=%d cont=%d\n",
      d, nx, ny, score, roll, flags, terr, owner, presence, settle, unit_fa, dfa,
      unit_river != 0, driver != 0, dres, vdist,
      ai_021a_adjacent_foreign(map, nx, ny, nation_id, 1), visit_nation, visit_val, occ,
      map_dos_0598_rumour_tile(map, nx, ny), map_dos_dist(nx - vx, ny - vy),
      ai_continent_id(map, nx, ny)
    );
    if (s_021a_adj_fx >= 0) {
      fprintf(
        stderr, "AI_021A   adjf tile=(%d,%d) l2=%02x l3=%02x unit=%d\n", s_021a_adj_fx,
        s_021a_adj_fy, ai_layer2_at(map, s_021a_adj_fx, s_021a_adj_fy),
        map_get_layer3(map, s_021a_adj_fx, s_021a_adj_fy),
        ai_unit_index_on_tile(units, s_021a_adj_fx, s_021a_adj_fy)
      );
    }
  }
  if (score > best) {
    best = score;
    best_dir = d;
    best_flags = flags;
  }

  c->score = score;
  c->best = best;
  c->best_dir = best_dir;
  c->best_flags = best_flags;
}

int ai_native_pick_dir_021a(
  AiRng* rng,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  const ColonizeUnit* u,
  int nation_id,
  Ai021aResult* out
) {
  out->dir = 8;
  out->flags = 0;
  if (!rng || !map || !units || !col1 || !u) {
    return 8;
  }
  const int x = u->x;
  const int y = u->y;
  const int indian = nation_id - 4;
  const int turn_w = (int)col1->head.turn;
  const int cool = turn_w + turn_w / -25; /* 021a:22b idiv by -25 */
  const int unit_fa = ai_mask_fa_flags(map, x, y) & 0x0a;
  const int unit_river = (int)(map_get_terrain_or(map, x, y, 25) & 0x40u);
  const int home = u->home_tribe_id;
  const int dump = ai_score_at_match(nation_id, x, y) ||
                   (ai_peel_audit_enabled() && ai_s_seed100_midturn_turn > 0);

  /* 0x291: FUN_1427_0bce — 0 when standing on a settlement tile. */
  int adj_foreign = 0;
  int adj_nation = -1;
  if (ai_021a_settle_owner(map, x, y) < 0) {
    adj_nation = ai_021a_adjacent_foreign(map, x, y, nation_id, 0);
    adj_foreign = adj_nation >= 0;
  }
  const int continent = ai_continent_id(map, x, y);
  /* 0x31d: nearest colony of any nation on this continent. */
  int col_dist = 9999;
  const int col_idx =
    ai_goals_nearest_colony_15eb_0142(map, colonies, x, y, -1, continent, &col_dist);
  const ColonizeColony* col = (col_idx >= 0) ? &colonies->colonies[col_idx] : NULL;
  /* Home village (validity already enforced by the pulse). */
  const ColonizeCol1Tribe* village =
    (home >= 0 && home < (int)col1->head.tribe_count) ? &col1->tribe[home] : NULL;
  const int vx = village ? (int)village->x : x;
  const int vy = village ? (int)village->y : y;
  int home_reach = home;
  if (village && ai_continent_id(map, vx, vy) != continent) {
    home_reach = -1;
  }
  /* 0x3bd: encroaching colony = within max(2, pop/2) of the village. */
  int encroach = -1;
  if (col) {
    const int d = map_dos_dist(col->x - vx, col->y - vy);
    int lim = (int)((int8_t)col->population >> 1);
    if (lim < 2) {
      lim = 2;
    }
    encroach = (lim >= d) ? d : -1;
  }
  /* 0x40a: Euro military stacks adjacent to the village. */
  int threat = 0;
  int threat_nation = -1;
  for (int d = 0; d < 8; ++d) {
    const int nx = vx + k_ai_dir8_dx[d];
    const int ny = vy + k_ai_dir8_dy[d];
    const int o = ai_owner_nibble(map, nx, ny);
    if (o < 0 || o >= 4) {
      continue;
    }
    if (ai_is_ocean_hs(map, nx, ny)) {
      continue;
    }
    if (ai_unit_index_on_tile(units, nx, ny) < 0) {
      continue;
    }
    int cnt = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* o_u = &units->units[i];
      if (!o_u->active || o_u->aboard_ship_id >= 0 || o_u->x != nx || o_u->y != ny) {
        continue;
      }
      if (o_u->type_index >= 0xd && o_u->type_index <= 0x12) {
        continue;
      }
      if (ai_021a_type_attack(units, o_u->type_index) > 1) {
        cnt++;
      }
    }
    if (cnt > 0) {
      const int ci = ai_021a_colony_at(colonies, nx, ny);
      if (ci >= 0) {
        cnt -= (int)((int8_t)colonies->colonies[ci].population >> 2);
      }
    }
    if (cnt > 0) {
      threat += cnt;
      threat_nation = o;
    }
  }
  if (threat < 2) {
    threat = 0;
    threat_nation = -1;
  }
  /* 0x52e: angry count. */
  int angry = 0;
  for (int e = 0; e < 4; ++e) {
    if (ai_diplo_indian_alarm(col1, nation_id, e) >= 0x4b) {
      angry++;
    }
    if (village && col1_tribe_attitude(village, e) >= 0x80) {
      angry++;
    }
  }
  const int visit_turn = ai_021a_visit_turn(u);
  const int self_stack = units_count_at(units, x, y);
  const int lone = self_stack == 1;
  const ColonizeCol1Indian* ind =
    (indian >= 0 && indian < 8) ? &col1->indian[indian] : NULL;
  const int tech = ind ? (int)ind->tech : 0;
  const int facing = u->last_dir;

  {
    /* AI_021A_WATCH="x:y" — print that tile's layer2/layer3 at every act. */
    static int wx = -2, wy = -2;
    if (wx == -2) {
      const char* e = getenv("AI_021A_WATCH");
      if (!e || sscanf(e, "%d:%d", &wx, &wy) != 2) {
        wx = wy = -1;
      }
    }
    if (wx >= 0) {
      fprintf(
        stderr, "AI_021A_WATCH n=%d act@(%d,%d) tile(%d,%d) l2=%02x l3=%02x\n", nation_id, x, y,
        wx, wy, ai_layer2_at(map, wx, wy), map_get_layer3(map, wx, wy)
      );
    }
  }
  if (dump) {
    fprintf(
      stderr,
      "AI_021A begin n=%d xy=(%d,%d) facing=%d cool=%d visit=%d adj=%d/%d col=%d encroach=%d "
      "threat=%d/%d angry=%d stack=%d seed=%u\n",
      nation_id, x, y, facing, cool, visit_turn, adj_foreign, adj_nation, col_idx, encroach,
      threat, threat_nation, angry, self_stack, (unsigned)map->prime_resource_seed
    );
  }

  int best = -1;
  int best_dir = 8;
  int best_flags = 0;
  int grudge = 0; /* [bp-0x14] — NOT reset on the other-tribe owner arm (DOS) */

  struct ai_021a_ctx c;
  memset(&c, 0, sizeof(c));
  c.rng = rng;
  c.map = map;
  c.units = units;
  c.col1 = col1;
  c.colonies = colonies;
  c.u = u;
  c.nation_id = nation_id;
  c.x = x;
  c.y = y;
  c.indian = indian;
  c.turn_w = turn_w;
  c.cool = cool;
  c.unit_fa = unit_fa;
  c.unit_river = unit_river;
  c.home = home;
  c.dump = dump;
  c.adj_foreign = adj_foreign;
  c.adj_nation = adj_nation;
  c.continent = continent;
  c.col_dist = col_dist;
  c.col_idx = col_idx;
  c.col = col;
  c.village = village;
  c.vx = vx;
  c.vy = vy;
  c.home_reach = home_reach;
  c.encroach = encroach;
  c.threat = threat;
  c.threat_nation = threat_nation;
  c.angry = angry;
  c.visit_turn = visit_turn;
  c.self_stack = self_stack;
  c.lone = lone;
  c.ind = ind;
  c.tech = tech;
  c.facing = facing;
  c.grudge = grudge;
  c.best = best;
  c.best_dir = best_dir;
  c.best_flags = best_flags;

  for (int d = 0; d < 9; ++d) {
    c.d = d;
    ai_021a_score_dir(&c);
  }
  if (s_native_score_plot && c.score_tile_count > 0) {
    s_native_score_plot(s_native_score_plot_user, u, c.score_tiles, c.score_tile_count);
  }
  best = c.best;
  best_dir = c.best_dir;
  best_flags = c.best_flags;
  if (dump) {
    fprintf(stderr, "AI_021A best=%d score=%d flags=%02x\n", best_dir, best, best_flags);
  }
  out->dir = best_dir;
  out->flags = best_flags;
  return best_dir;
}

/*
 * FUN_4d56_021a post-loop tail (021a:1277..14e6). Returns the final dir (8 =
 * stay). Not ported: the human-colony warning chrome (021a:1329..13ae) and
 * the pending-encounter resolver call (2a1f_0192 -> 5bfb_3180) — the port's
 * §9 contact pass after the pulse plays the visit from the unit's position,
 * so here the bit only costs the act, as in DOS.
 */
int ai_native_021a_tail(
  ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  AiRng* rng,
  ColonizeUnit* u,
  int nation_id,
  int dir,
  int flags
) {
  const int indian = nation_id - 4;
  ColonizeCol1Indian* ind = (indian >= 0 && indian < 8) ? &col1->indian[indian] : NULL;
  const int turn_w = (int)col1->head.turn;
  const int cool = turn_w + turn_w / -25;
  int owner = -1;
  if (flags & 0x0a) {
    const int dx = (dir < 8) ? u->x + k_ai_dir8_dx[dir] : u->x;
    const int dy = (dir < 8) ? u->y + k_ai_dir8_dy[dir] : u->y;
    owner = ai_owner_nibble(map, dx, dy);
    if (ind && owner >= 0 && owner < 4) {
      if (ind->contact_state[owner] == 2) {
        return 8;
      }
      ind->contact_state[owner] = 1;
    }
  }
  const int hostile = flags & 0x04;
  if (flags & 0x0a) {
    /* 021a:12f8 — a contact/attack step needs a full move left. */
    if (units_max_mp(units, u->id) - u->moves < 3) {
      return 8;
    }
  }
  if ((flags & 0x0a) || hostile) {
    u->col1_flags15 &= (uint8_t)~0x08u;
    flags &= 0x7f;
  }
  if (u->col1_flags15 & 0x08u) {
    /* 021a:13cc — pending encounter at own tile resolves, unit stays. */
    u->col1_flags15 &= (uint8_t)~0x08u;
    return 8;
  }
  if (flags & 0x80) {
    ai_021a_set_visit_turn(u, turn_w);
    u->col1_flags15 |= 0x08u;
  }
  if ((flags & 0x8a) == 0 && ai_s_native_colonies) {
    /* 021a:1419 — march on an encroaching colony after the long cooldown. */
    const int continent = ai_continent_id(map, u->x, u->y);
    int col_dist = 9999;
    const int col_idx = ai_goals_nearest_colony_15eb_0142(
      map, ai_s_native_colonies, u->x, u->y, -1, continent, &col_dist
    );
    const int home = u->home_tribe_id;
    const ColonizeCol1Tribe* village =
      (home >= 0 && home < (int)col1->head.tribe_count) ? &col1->tribe[home] : NULL;
    if (col_idx >= 0 && village) {
      const ColonizeColony* col = &ai_s_native_colonies->colonies[col_idx];
      const int d = map_dos_dist(col->x - (int)village->x, col->y - (int)village->y);
      int lim = (int)((int8_t)col->population >> 1);
      if (lim < 2) {
        lim = 2;
      }
      if (lim >= d) {
        const int cn = col->nation_id;
        const int census = (cn >= 0 && cn < 4) ? (int)col1->stuff.census_pop_proxy[cn] : 0;
        const int cx = (census >> 3) + d * 4 + 10;
        if (cx <= cool - ai_021a_visit_turn(u)) {
          const int save_gx = u->goto_x;
          const int save_gy = u->goto_y;
          u->goto_x = col->x;
          u->goto_y = col->y;
          int sx = 0;
          int sy = 0;
          const bool ok =
            units_next_goto_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .colonies=(ColonizeColonyPool*)(ai_s_native_colonies), .map=(ColonizeWorldMap*)(map), .rng=(ColonizeDosRng*)(rng)}, u->id, &sx, &sy);
          u->goto_x = save_gx;
          u->goto_y = save_gy;
          if (ok) {
            int step = -1;
            for (int k = 0; k < 8; ++k) {
              if (u->x + k_ai_dir8_dx[k] == sx && u->y + k_ai_dir8_dy[k] == sy) {
                step = k;
              }
            }
            if (step >= 0) {
              const int so = ai_021a_settle_owner(map, sx, sy);
              if (so < 0 || so == nation_id) {
                const int po = map_tile_owner_or_presence(map, sx, sy);
                if (po < 0 || po == nation_id) {
                  return step;
                }
              }
            }
          }
        }
      }
    }
  }
  /* 021a:14ca — quiet step onto foreign-owned land only with a fresh allotment. */
  if ((flags & 0x1a) == 0 && (flags & 0x01) && u->moves != 0) {
    return 8;
  }
  return dir;
}

/* AI_021A_TRACE=1 — one line per Brave act (pick, flags, final dir). */
int ai_021a_trace_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    const char* e = getenv("AI_021A_TRACE");
    cached = (e && e[0] == '1') ? 1 : 0;
  }
  return cached;
}
