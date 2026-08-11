/*
 * Joint mid-turn golden scaffold (T3 continuation Series D).
 * Linux-derived MID01: load TURN7, stamp mid-war Indian×Euro fields, write
 * test-saves-ai/MID01.SAV, reload and assert in-scope joint fields present.
 * MID02 pair compare stays OPEN until a follow-on save lands.
 */
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/units.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

#define MID01_PATH "test-saves-ai/MID01.SAV"
#define TURN7_PATH "test-saves-ai/TURN7.SAV"

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_mid01: FAIL %s\n", msg);
  return 1;
}

static int assert_joint_fields(const ColonizeCol1Save* s, const char* tag) {
  if (!s || s->head.unit_count == 0) {
    fprintf(stderr, "%s: no units\n", tag);
    return 0;
  }
  if (s->head.tribe_count == 0 || !s->tribe) {
    fprintf(stderr, "%s: no tribes\n", tag);
    return 0;
  }
  int euro_u = 0;
  int brave_u = 0;
  for (unsigned i = 0; i < s->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &s->unit[i];
    if (u->nation_id < 4) {
      euro_u++;
    } else if (u->nation_id <= 11) {
      brave_u++;
    }
  }
  if (euro_u == 0 || brave_u == 0) {
    fprintf(stderr, "%s: euro_u=%d brave_u=%d\n", tag, euro_u, brave_u);
    return 0;
  }
  /* Sticky + at least one Indian relation / euro_relation row readable. */
  (void)s->nation[0].indian_hostility_sticky;
  (void)s->nation[0].relation_by_indian[0];
  (void)s->nation[0].euro_relation[1];
  return 1;
}

int main(void) {
  char err[256];
  ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file(TURN7_PATH, &save, err, sizeof(err))) {
    fprintf(stderr, "read TURN7: %s\n", err);
    return fail("load TURN7.SAV");
  }

  /*
   * Mid-war stamp (Linux-derived, not hang dump): raise Indian×Euro hostility
   * and sticky so MID01 encodes joint raid/hunt surfaces.
   */
  for (int e = 0; e < 4; ++e) {
    save.nation[e].indian_hostility_sticky = 2;
    for (int i = 0; i < 8; ++i) {
      if (save.nation[e].relation_by_indian[i] == 0) {
        continue;
      }
      if (save.nation[e].relation_by_indian[i] > 40) {
        save.nation[e].relation_by_indian[i] = 35;
      }
    }
  }
  for (int n = 0; n < 8; ++n) {
    ColonizeCol1Indian* ind = &save.indian[n];
    for (int e = 0; e < 4; ++e) {
      if (ind->euro_diplo[e]) {
        ind->alarm_by_player[e] = 60;
      }
    }
  }
  /* Bump calendar past early TURN7 so filename reads as mid-campaign. */
  if (save.head.year < 1500) {
    save.head.year = 1505;
  }

  if (!col1_save_write_file(MID01_PATH, &save, err, sizeof(err))) {
    fprintf(stderr, "write MID01: %s\n", err);
    col1_save_free(&save);
    return fail("write MID01.SAV");
  }
  col1_save_free(&save);

  ColonizeCol1Save mid;
  col1_save_init(&mid);
  if (!col1_save_read_file(MID01_PATH, &mid, err, sizeof(err))) {
    fprintf(stderr, "reload MID01: %s\n", err);
    return fail("reload MID01.SAV");
  }
  if (!assert_joint_fields(&mid, "MID01")) {
    col1_save_free(&mid);
    return fail("MID01 joint field snapshot");
  }
  if (mid.head.year < 1500) {
    col1_save_free(&mid);
    return fail("MID01 should be stamped mid-campaign year");
  }
  if (mid.nation[0].indian_hostility_sticky < 2) {
    col1_save_free(&mid);
    return fail("MID01 sticky stamp");
  }

  col1_save_free(&mid);
  printf("smoke_ai_mid01: ok (wrote+verified %s from TURN7 mid-war stamp)\n", MID01_PATH);
  return 0;
}
