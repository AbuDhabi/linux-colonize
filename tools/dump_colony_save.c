#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_save.h"

static const char* k_cargo_names[COLONIZE_COL1_CARGO_TYPES] = {
  "Food", "Sugar", "Tobacco", "Cotton", "Furs", "Lumber", "Ore", "Silver",
  "Horses", "Rum", "Cigars", "Cloth", "Coats", "TradeGoods", "Tools", "Muskets"
};

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <path/to/COLONY.SAV> [colony_name_filter]\n", argv[0]);
    return 1;
  }

  ColonizeCol1Save save;
  char err[256];
  col1_save_init(&save);
  if (!col1_save_read_file(argv[1], &save, err, sizeof(err))) {
    fprintf(stderr, "Failed to read %s: %s\n", argv[1], err);
    return 1;
  }

  const char* filter = argc > 2 ? argv[2] : NULL;

  printf("Save: %s (Turn %u, Year %u, Season %s, %u colonies)\n\n",
         argv[1],
         (unsigned)save.head.turn,
         (unsigned)save.head.year,
         save.head.autumn ? "Autumn" : "Spring",
         (unsigned)save.head.colony_count);

  for (unsigned i = 0; i < save.head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &save.colony[i];
    if (filter && strcasestr(c->name, filter) == NULL) {
      continue;
    }

    printf("=================================================================\n");
    printf("Colony #%u: '%s' at (%u, %u) - Nation %u, Population %u\n",
           i, c->name, (unsigned)c->x, (unsigned)c->y,
           (unsigned)c->nation_id, (unsigned)c->population);
    printf("Rebel Accumulators: %u / %u (%u%% SoL), Flags: 0x%02x\n",
           (unsigned)c->rebel_dividend, (unsigned)c->rebel_divisor,
           c->rebel_divisor ? (unsigned)((c->rebel_dividend * 100u) / c->rebel_divisor) : 0u,
           (unsigned)*(const uint8_t*)&c->flags);

    uint16_t ch_mask = 0;
    memcpy(&ch_mask, &c->custom_house, sizeof(ch_mask));
    printf("Custom House Bitmask: 0x%04x, Produced Mask: 0x%04x\n",
           (unsigned)ch_mask, (unsigned)c->cargo_produced_mask);

    printf("Colonists (%u):\n", (unsigned)c->population);
    for (unsigned p = 0; p < c->population && p < COLONIZE_COL1_COLONY_POP_MAX; ++p) {
      printf("  [%2u] occupation=%-3u (0x%02x)  profession=%-3u (0x%02x)\n",
             p,
             (unsigned)c->occupation[p], (unsigned)c->occupation[p],
             (unsigned)c->profession[p], (unsigned)c->profession[p]);
    }

    printf("Stock:\n");
    for (unsigned g = 0; g < COLONIZE_COL1_CARGO_TYPES; ++g) {
      if (c->stock[g] > 0) {
        printf("  %-12s: %4u\n", k_cargo_names[g], (unsigned)c->stock[g]);
      }
    }
    printf("\n");
  }

  col1_save_free(&save);
  return 0;
}
