/*
 * One-off probe: compare Linux-exported save vs DOS starter for fog / discovery /
 * unit vis_mask / transport-chain bugs.
 *
 * Build (from repo root, after cmake build):
 *   cc -O2 -I src -o /tmp/probe_unowned_visible_save \
 *     tools/probe_unowned_visible_save.c build/libcolonize_core.a -lm -lpthread
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_save.h"

static void print_tut1(const ColonizeCol1Tut1* t) {
  printf(
    "  tut1 bits: nr13=%u nr14=%u unused06=%u nr15=%u nr16=%u nr17=%u unused08=%u nr19=%u\n",
    (unsigned)t->nr13,
    (unsigned)t->nr14,
    (unsigned)t->unused06,
    (unsigned)t->nr15,
    (unsigned)t->nr16,
    (unsigned)t->nr17,
    (unsigned)t->unused08,
    (unsigned)t->nr19
  );
}

static int unit_is_ship(uint8_t type) {
  /* Col1 unit types: 15=Wagon? Ships are typically 11..16 range — use broad check
   * via cargo capacity convention; print type raw and classify common names later. */
  return type >= 15 && type <= 19; /* Caravel..Man-O-War typical indices */
}

static int unit_is_land_colonistish(uint8_t type) {
  return type <= 14; /* free colonist / pioneer / soldier / etc. */
}

static void analyze_seen(const ColonizeCol1Save* save, const char* label) {
  const size_t n = save->map.tile_count;
  unsigned any_euro[4] = {0};
  unsigned any_any = 0;
  unsigned full_f = 0;
  if (!save->map.seen) {
    printf("[%s] map.seen NULL\n", label);
    return;
  }
  for (size_t i = 0; i < n; ++i) {
    const uint8_t s = save->map.seen[i];
    const uint8_t euro = (uint8_t)((s >> 4) & 0xF);
    if (euro) {
      any_any++;
    }
    if (euro == 0xF) {
      full_f++;
    }
    for (int e = 0; e < 4; ++e) {
      if (euro & (1u << e)) {
        any_euro[e]++;
      }
    }
  }
  printf(
    "[%s] map.seen: tiles=%zu any_euro=%u full0xF=%u | n0=%u n1=%u n2=%u n3=%u\n",
    label,
    n,
    any_any,
    full_f,
    any_euro[0],
    any_euro[1],
    any_euro[2],
    any_euro[3]
  );

  /* Sample: for non-human units (nation!=human), is their tile seen by human? */
  const int human = (int)save->head.human_player;
  unsigned foreign_on_seen = 0, foreign_total = 0, foreign_on_unseen = 0;
  unsigned native_on_seen = 0, native_total = 0;
  unsigned ai_euro_on_seen = 0, ai_euro_total = 0;
  for (uint16_t i = 0; i < save->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &save->unit[i];
    const size_t idx = (size_t)u->y * (size_t)save->map.width + (size_t)u->x;
    if (idx >= n) {
      continue;
    }
    const uint8_t euro = (uint8_t)((save->map.seen[idx] >> 4) & 0xF);
    const int seen_by_human = human >= 0 && human < 4 && (euro & (1u << human)) != 0;
    if ((int)u->nation_id == human) {
      continue;
    }
    if (u->nation_id >= 4) {
      native_total++;
      if (seen_by_human) {
        native_on_seen++;
      }
    } else {
      ai_euro_total++;
      if (seen_by_human) {
        ai_euro_on_seen++;
      }
    }
    foreign_total++;
    if (seen_by_human) {
      foreign_on_seen++;
    } else {
      foreign_on_unseen++;
    }
  }
  printf(
    "[%s] foreign units on human-seen tiles: %u/%u (unseen %u); "
    "AI-euro %u/%u; native %u/%u\n",
    label,
    foreign_on_seen,
    foreign_total,
    foreign_on_unseen,
    ai_euro_on_seen,
    ai_euro_total,
    native_on_seen,
    native_total
  );
}

static void print_unit(const char* tag, unsigned i, const ColonizeCol1Unit* u) {
  printf(
    "  %s[%u] xy=(%u,%u) type=%u nat=%u vis=%X orders=%u holds_occ=%u "
    "cargo_hold=[%u,%u,%u,%u,%u,%u] items=[%u,%u,%u,%u,%u,%u] "
    "chain next=%d prev=%d ai_plan=0x%02X origin=%u moves=%u prof=%u "
    "unk15_lo=%u dmg=%u\n",
    tag,
    i,
    (unsigned)u->x,
    (unsigned)u->y,
    (unsigned)u->type,
    (unsigned)u->nation_id,
    (unsigned)u->vis_mask,
    (unsigned)u->orders,
    (unsigned)u->holds_occupied,
    (unsigned)u->cargo_hold[0],
    (unsigned)u->cargo_hold[1],
    (unsigned)u->cargo_hold[2],
    (unsigned)u->cargo_hold[3],
    (unsigned)u->cargo_hold[4],
    (unsigned)u->cargo_hold[5],
    (unsigned)u->cargo_item_0,
    (unsigned)u->cargo_item_1,
    (unsigned)u->cargo_item_2,
    (unsigned)u->cargo_item_3,
    (unsigned)u->cargo_item_4,
    (unsigned)u->cargo_item_5,
    (int)u->transport_chain.next_unit_idx,
    (int)u->transport_chain.prev_unit_idx,
    (unsigned)u->ai_plan,
    (unsigned)u->origin,
    (unsigned)u->moves,
    (unsigned)u->profession,
    (unsigned)u->unknown15_lo,
    (unsigned)u->ship_damaged
  );
  (void)unit_is_ship;
  (void)unit_is_land_colonistish;
}

static void analyze_units(const ColonizeCol1Save* save, const char* label) {
  const int human = (int)save->head.human_player;
  unsigned vis_nonzero = 0, vis_full = 0, vis_zero = 0;
  printf("[%s] unit_count=%u human=%d\n", label, (unsigned)save->head.unit_count, human);

  for (uint16_t i = 0; i < save->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &save->unit[i];
    if (u->vis_mask == 0) {
      vis_zero++;
    } else {
      vis_nonzero++;
    }
    if (u->vis_mask == 0xF) {
      vis_full++;
    }
  }
  printf(
    "[%s] vis_mask: zero=%u nonzero=%u full0xF=%u\n",
    label,
    vis_zero,
    vis_nonzero,
    vis_full
  );

  printf("[%s] human ships + colonist-ish units (and any unit on same tile as human ship):\n", label);
  /* First find human ship tiles */
  for (uint16_t i = 0; i < save->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &save->unit[i];
    if ((int)u->nation_id != human) {
      continue;
    }
    /* Print all human units — starter has few */
    print_unit("H", i, u);
  }

  /* Orphans: same tile as a ship, land unit, not in that ship's chain */
  printf("[%s] tile-share orphans (land unit on ship tile, not in any transport chain to a ship):\n", label);
  for (uint16_t i = 0; i < save->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &save->unit[i];
    /* Find ships on same tile */
    for (uint16_t s = 0; s < save->head.unit_count; ++s) {
      const ColonizeCol1Unit* ship = &save->unit[s];
      if (ship->x != u->x || ship->y != u->y || s == i) {
        continue;
      }
      /* Heuristic: ship if holds_occupied often set, or type in ship range,
       * or has chain pointing to passengers / is chain end. */
      int ship_like = (ship->type >= 15 && ship->type <= 19);
      if (!ship_like) {
        continue;
      }
      /* Walk chain from u: if we reach ship, OK */
      int reached = 0;
      int cur = (int)i;
      for (int step = 0; step < 16 && cur >= 0 && cur < (int)save->head.unit_count; ++step) {
        if (cur == (int)s) {
          reached = 1;
          break;
        }
        cur = save->unit[cur].transport_chain.next_unit_idx;
      }
      /* Also check if ship chain reaches u via prev walk */
      if (!reached) {
        cur = (int)s;
        for (int step = 0; step < 16 && cur >= 0 && cur < (int)save->head.unit_count; ++step) {
          if (cur == (int)i) {
            reached = 1;
            break;
          }
          cur = save->unit[cur].transport_chain.prev_unit_idx;
        }
      }
      if (!reached && u->type < 15) {
        printf(
          "  ORPHAN unit[%u] type=%u nat=%u on ship[%u] type=%u at (%u,%u) "
          "unit.chain n/p=%d/%d ship.chain n/p=%d/%d\n",
          (unsigned)i,
          (unsigned)u->type,
          (unsigned)u->nation_id,
          (unsigned)s,
          (unsigned)ship->type,
          (unsigned)u->x,
          (unsigned)u->y,
          (int)u->transport_chain.next_unit_idx,
          (int)u->transport_chain.prev_unit_idx,
          (int)ship->transport_chain.next_unit_idx,
          (int)ship->transport_chain.prev_unit_idx
        );
      }
    }
  }

  /* Dump vis_mask histogram by nation */
  unsigned by_nat_vis[16][16];
  memset(by_nat_vis, 0, sizeof(by_nat_vis));
  for (uint16_t i = 0; i < save->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &save->unit[i];
    by_nat_vis[u->nation_id & 15][u->vis_mask & 15]++;
  }
  printf("[%s] vis_mask counts by nation (nat -> mask:count):\n", label);
  for (int n = 0; n < 12; ++n) {
    int any = 0;
    for (int m = 0; m < 16; ++m) {
      if (by_nat_vis[n][m]) {
        any = 1;
      }
    }
    if (!any) {
      continue;
    }
    printf("  nat %d:", n);
    for (int m = 0; m < 16; ++m) {
      if (by_nat_vis[n][m]) {
        printf(" %X=%u", m, by_nat_vis[n][m]);
      }
    }
    printf("\n");
  }
}

static void analyze_discovery(const ColonizeCol1Save* save, const char* label) {
  const ColonizeCol1Head* h = &save->head;
  printf(
    "[%s] human_player=%u nation_turn=%u year=%u turn=%u autumn=%u "
    "show_entire_map=%u\n",
    label,
    (unsigned)h->human_player,
    (unsigned)h->nation_turn,
    (unsigned)h->year,
    (unsigned)h->turn,
    (unsigned)h->autumn,
    (unsigned)h->show_entire_map
  );
  printf(
    "[%s] event.discovery_of_the_new_world=%u building_a_colony=%u "
    "meeting_the_natives=%u\n",
    label,
    (unsigned)h->event.discovery_of_the_new_world,
    (unsigned)h->event.building_a_colony,
    (unsigned)h->event.meeting_the_natives
  );
  print_tut1(&h->tut1);
  for (int p = 0; p < 4; ++p) {
    const ColonizeCol1Player* pl = &save->player[p];
    printf(
      "  player[%d] name='%.24s' country='%.24s' named_new_world=%u control=%u "
      "founded=%u diplomacy=%u unk06_lo=%u\n",
      p,
      pl->name,
      pl->country_name,
      (unsigned)pl->named_new_world,
      (unsigned)pl->control,
      (unsigned)pl->founded_colonies,
      (unsigned)pl->diplomacy,
      (unsigned)pl->unknown06_lo
    );
  }
}

static int run_one(const char* path, const char* label) {
  ColonizeCol1Save save;
  char err[256];
  col1_save_init(&save);
  if (!col1_save_read_file(path, &save, err, sizeof(err))) {
    fprintf(stderr, "%s: read failed: %s\n", label, err);
    return 1;
  }
  printf("======== %s (%s) size_check ok ========\n", label, path);
  analyze_discovery(&save, label);
  analyze_seen(&save, label);
  analyze_units(&save, label);
  col1_save_free(&save);
  return 0;
}

int main(int argc, char** argv) {
  const char* bad =
    "/home/jan/projects/linux-colonize/tests-save-misc/unowned units all visible.sav";
  const char* good = "/home/jan/projects/linux-colonize/original_saves/COLONY00.SAV";
  if (argc >= 2) {
    bad = argv[1];
  }
  if (argc >= 3) {
    good = argv[2];
  }
  int rc = 0;
  rc |= run_one(bad, "LINUX_EXPORT");
  rc |= run_one(good, "DOS_STARTER");
  return rc;
}
