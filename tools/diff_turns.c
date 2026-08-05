#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_save.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s A.SAV B.SAV\n", argv[0]);
    return 1;
  }
  ColonizeCol1Save a, b;
  char err[256];
  col1_save_init(&a);
  col1_save_init(&b);
  if (!col1_save_read_file(argv[1], &a, err, sizeof err)) {
    fprintf(stderr, "%s: %s\n", argv[1], err);
    return 1;
  }
  if (!col1_save_read_file(argv[2], &b, err, sizeof err)) {
    fprintf(stderr, "%s: %s\n", argv[2], err);
    return 1;
  }
  printf("=== %s vs %s ===\n", argv[1], argv[2]);
  unsigned n = a.head.unit_count < b.head.unit_count ? a.head.unit_count : b.head.unit_count;
  for (unsigned i = 0; i < n; i++) {
    const ColonizeCol1Unit* u = &a.unit[i];
    const ColonizeCol1Unit* v = &b.unit[i];
    if (u->x != v->x || u->y != v->y || u->type != v->type || u->nation_id != v->nation_id ||
        u->orders != v->orders || u->goto_x != v->goto_x || u->goto_y != v->goto_y ||
        u->moves != v->moves || u->turns_worked != v->turns_worked ||
        u->holds_occupied != v->holds_occupied) {
      printf(
        "DIFF[%u] t=%u n=%u: (%u,%u)ord=%ug=(%u,%u)mv=%utw=%uh=%u -> "
        "(%u,%u)ord=%ug=(%u,%u)mv=%utw=%uh=%u\n",
        i,
        (unsigned)u->type,
        (unsigned)u->nation_id,
        (unsigned)u->x,
        (unsigned)u->y,
        (unsigned)u->orders,
        (unsigned)u->goto_x,
        (unsigned)u->goto_y,
        (unsigned)u->moves,
        (unsigned)u->turns_worked,
        (unsigned)u->holds_occupied,
        (unsigned)v->x,
        (unsigned)v->y,
        (unsigned)v->orders,
        (unsigned)v->goto_x,
        (unsigned)v->goto_y,
        (unsigned)v->moves,
        (unsigned)v->turns_worked,
        (unsigned)v->holds_occupied
      );
    }
  }
  if (a.head.unit_count != b.head.unit_count) {
    printf("unit_count %u -> %u\n", a.head.unit_count, b.head.unit_count);
  }
  for (unsigned i = 0; i < a.head.tribe_count && i < b.head.tribe_count; i++) {
    if (a.tribe[i].population != b.tribe[i].population ||
        a.tribe[i].unknown28[0] != b.tribe[i].unknown28[0] || a.tribe[i].x != b.tribe[i].x ||
        a.tribe[i].y != b.tribe[i].y) {
      printf(
        "TRIBE[%u] pop=%u acc=%u xy=(%u,%u) -> pop=%u acc=%u xy=(%u,%u)\n",
        i,
        a.tribe[i].population,
        a.tribe[i].unknown28[0],
        a.tribe[i].x,
        a.tribe[i].y,
        b.tribe[i].population,
        b.tribe[i].unknown28[0],
        b.tribe[i].x,
        b.tribe[i].y
      );
    }
  }
  for (int nati = 0; nati < 4; nati++) {
    if (a.nation[nati].current_crosses != b.nation[nati].current_crosses ||
        a.nation[nati].needed_crosses != b.nation[nati].needed_crosses) {
      printf(
        "NATION[%d] crosses %u/%u -> %u/%u\n",
        nati,
        a.nation[nati].current_crosses,
        a.nation[nati].needed_crosses,
        b.nation[nati].current_crosses,
        b.nation[nati].needed_crosses
      );
    }
  }
  col1_save_free(&a);
  col1_save_free(&b);
  return 0;
}
