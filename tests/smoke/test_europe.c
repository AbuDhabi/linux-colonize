#include <stdio.h>
#include <string.h>

#include "core/europe.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  EuropeScreen eu;
  char err[256];
  if (!europe_load(&eu, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "europe_load failed: %s\n", err);
    return 1;
  }

  if (!eu.background_ok || eu.background.width != 320 || eu.background.height != 200) {
    fprintf(
      stderr,
      "EUROPE.PIK expected 320x200, got %dx%d ok=%d\n",
      eu.background.width,
      eu.background.height,
      eu.background_ok ? 1 : 0
    );
    europe_free(&eu);
    return 1;
  }

  if (eu.cargo_count < 16) {
    fprintf(stderr, "expected >=16 cargo quotes, got %d\n", eu.cargo_count);
    europe_free(&eu);
    return 1;
  }
  if (eu.class_count < 1) {
    fprintf(stderr, "expected recruit classes from @CLASS\n");
    europe_free(&eu);
    return 1;
  }

  /* Food: start_lo=1, burden=7 → ask = bid + burden + 1 = 9 */
  if (strcmp(eu.cargo[0].name, "Food") != 0 || eu.cargo[0].bid != 1 || eu.cargo[0].ask != 9) {
    fprintf(
      stderr,
      "Food quote expected bid=1 ask=9 got name='%s' bid=%d ask=%d\n",
      eu.cargo[0].name,
      eu.cargo[0].bid,
      eu.cargo[0].ask
    );
    europe_free(&eu);
    return 1;
  }

  const int gold_before = eu.gold;
  const int dock_before = eu.dock_count;
  if (!europe_recruit(&eu)) {
    fprintf(stderr, "recruit failed: %s\n", eu.status);
    europe_free(&eu);
    return 1;
  }
  if (eu.dock_count != dock_before + 1 || eu.gold >= gold_before) {
    fprintf(stderr, "recruit did not update dock/gold (dock %d→%d gold %d→%d)\n",
      dock_before, eu.dock_count, gold_before, eu.gold);
    europe_free(&eu);
    return 1;
  }

  const int gold_after_recruit = eu.gold;
  europe_cheat_add_gold(&eu, 1000);
  if (eu.gold != gold_after_recruit + 1000) {
    fprintf(stderr, "gold cheat expected %d got %d\n", gold_after_recruit + 1000, eu.gold);
    europe_free(&eu);
    return 1;
  }

  europe_cheat_adjust_tax(&eu, -1);
  if (eu.tax_percent != 0) {
    fprintf(stderr, "tax should clamp at 0, got %d\n", eu.tax_percent);
    europe_free(&eu);
    return 1;
  }

  fprintf(
    stderr,
    "europe tests ok (cargo=%d classes=%d gold=%d dock=%d)\n",
    eu.cargo_count,
    eu.class_count,
    eu.gold,
    eu.dock_count
  );
  europe_free(&eu);
  diag_shutdown();
  return 0;
}
