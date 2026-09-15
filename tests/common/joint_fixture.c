#include "tests/common/joint_fixture.h"

#include <stdio.h>

int joint_assert_fields(const ColonizeCol1Save* s, const char* tag) {
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
  if (s->head.colony_count == 0) {
    fprintf(stderr, "%s: no colonies\n", tag);
    return 0;
  }
  (void)s->nation[0].indian_hostility_sticky;
  (void)s->nation[0].relation_by_indian[0];
  (void)s->nation[0].euro_relation[1];
  return 1;
}

bool joint_stamp_hostility_fixture(
  const char* in_path, const char* out_path, const JointHostilityStamp* st
) {
  char err[256];
  ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file(in_path, &save, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", in_path, err);
    return false;
  }

  if (save.head.year < st->year_floor) {
    save.head.year = st->year_set;
  }
  for (int e = 0; e < 4; ++e) {
    if (st->sticky_force || save.nation[e].indian_hostility_sticky < st->sticky_value) {
      save.nation[e].indian_hostility_sticky = st->sticky_value;
    }
    for (int i = 0; i < 8; ++i) {
      if (save.nation[e].relation_by_indian[i] == 0) {
        continue;
      }
      if (save.nation[e].relation_by_indian[i] > st->relation_above) {
        save.nation[e].relation_by_indian[i] = st->relation_set;
      }
    }
  }
  for (int n = 0; n < 8; ++n) {
    ColonizeCol1Indian* ind = &save.indian[n];
    for (int e = 0; e < 4; ++e) {
      if (!ind->euro_diplo[e]) {
        continue;
      }
      if (st->alarm_force || ind->alarm_by_player[e] < st->alarm_value) {
        ind->alarm_by_player[e] = st->alarm_value;
      }
    }
  }
  if (st->friction_min >= 0 && save.tribe) {
    for (uint16_t ti = 0; ti < save.head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &save.tribe[ti];
      for (int e = 0; e < 4; ++e) {
        if ((int)t->alarm[e].friction < st->friction_min) {
          t->alarm[e].friction = (uint8_t)st->friction_min;
        }
      }
    }
  }

  if (!col1_save_write_file(out_path, &save, err, sizeof(err))) {
    fprintf(stderr, "write %s: %s\n", out_path, err);
    col1_save_free(&save);
    return false;
  }
  col1_save_free(&save);
  return true;
}
