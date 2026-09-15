#ifndef COLONIZE_TESTS_COMMON_JOINT_FIXTURE_H
#define COLONIZE_TESTS_COMMON_JOINT_FIXTURE_H

/*
 * Shared pieces of the joint Euro<->Indian smoke pair (smoke_ai_mid01 /
 * smoke_ai_late01) — duplication audit TT-11 / TT-12.
 */

#include <stdbool.h>
#include <stdint.h>

#include "core/col1_save.h"

/*
 * Joint surface snapshot check: units present on both sides (Euro nations
 * 0-3 and Braves 4-11), tribes present, colonies present, and the three
 * diplo rows readable. Returns 1 on success, 0 after printing the reason.
 * The colony check came from late01; mid01's copy lacked it.
 */
int joint_assert_fields(const ColonizeCol1Save* s, const char* tag);

/*
 * The "read a .SAV, stamp hostility scalars, write it back as the next
 * fixture" step both tests do. Fields left at their skip value are not
 * touched. Prints the reason and returns false on an I/O failure.
 */
typedef struct {
  uint16_t year_floor; /* when head.year < year_floor ... */
  uint16_t year_set;   /*   ... head.year becomes year_set */
  uint8_t sticky_value;
  bool sticky_force; /* true: assign; false: raise only when below */
  uint8_t relation_above; /* relation_by_indian > relation_above (0 rows skipped) ... */
  uint8_t relation_set;   /*   ... becomes relation_set */
  uint8_t alarm_value;    /* indian[].alarm_by_player[e] where euro_diplo[e] */
  bool alarm_force;       /* true: assign; false: raise only when below */
  int friction_min;       /* < 0: leave tribe alarm friction alone */
} JointHostilityStamp;

bool joint_stamp_hostility_fixture(
  const char* in_path, const char* out_path, const JointHostilityStamp* st
);

#endif /* COLONIZE_TESTS_COMMON_JOINT_FIXTURE_H */
