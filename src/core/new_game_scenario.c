/*
 * @SCENARIO start-tile lookup, split out of new_game.c 2026-09-16 so ai.c can
 * place the landfall without linking the new-game wizard screens. Pure move:
 * the body is byte-identical to its new_game.c original.
 *
 * This file is SIM.
 */
#include "core/new_game.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "core/assets.h"

bool new_game_scenario_start(
  const ColonizeMsgCatalog* names_txt,
  const char* map_stem,
  int nation,
  int* out_x,
  int* out_y
) {
  /*
   * @SCENARIO row is `stem, x0, y0, x1, y1, x2, y2, x3, y3` -- four start
   * tiles, one per European power (docs/data_vs_hardcoded.md: "Per-scenario
   * start tiles for four European powers"). All four AMER2 pairs are the
   * westernmost high-seas tile of their own row, i.e. the Atlantic spot each
   * nation's starting ship occupies. An earlier parse read the first pair as
   * two unrelated leading fields, which shifted every nation down one slot and
   * left nations 2 and 3 sharing a tile -- two AI fleets stacked on the same
   * water with nowhere to sail.
   */
  int xs[4] = {34, 39, 47, 50};
  int ys[4] = {20, 10, 61, 33};
  if (names_txt && map_stem) {
    const ColonizeMsgSection* section = assets_msg_find(names_txt, "SCENARIO");
    if (section) {
      for (int i = 0; i < section->line_count; ++i) {
        const char* line = section->lines[i];
        if (!line || line[0] == '\0' || line[0] == ';') {
          continue;
        }
        char stem[64];
        int px[4] = {0, 0, 0, 0};
        int py[4] = {0, 0, 0, 0};
        int n = sscanf(
          line,
          "%63[^,], %d, %d, %d, %d, %d, %d, %d, %d",
          stem,
          &px[0],
          &py[0],
          &px[1],
          &py[1],
          &px[2],
          &py[2],
          &px[3],
          &py[3]
        );
        /* Trim stem spaces */
        size_t sl = strlen(stem);
        while (sl > 0 && (stem[sl - 1] == ' ' || stem[sl - 1] == '\t')) {
          stem[--sl] = '\0';
        }
        if (strcasecmp(stem, map_stem) != 0) {
          continue;
        }
        /* Short rows repeat the last parsed pair rather than leaving a stale
         * default from another scenario. */
        int last = -1;
        for (int k = 0; k < 4; ++k) {
          if (n >= 3 + k * 2) {
            xs[k] = px[k];
            ys[k] = py[k];
            last = k;
          } else if (last >= 0) {
            xs[k] = xs[last];
            ys[k] = ys[last];
          }
        }
        break;
      }
    }
  }
  if (nation < 0 || nation > 3) {
    nation = 0;
  }
  if (out_x) {
    *out_x = xs[nation];
  }
  if (out_y) {
    *out_y = ys[nation];
  }
  return true;
}
