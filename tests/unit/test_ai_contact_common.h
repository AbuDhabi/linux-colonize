#ifndef COLONIZE_TEST_AI_CONTACT_COMMON_H
#define COLONIZE_TEST_AI_CONTACT_COMMON_H

/* Smoke: Indian meet + friction raid loot (@RAID* kinds) + prelude encroachment. */
#include "../common/test_catalogs.h"
#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/colony_production.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units_move.h"
#include "core/village_trade_intel.h"

#include "../common/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_contact: FAIL %s\n", msg);
  return 1;
}

#endif /* COLONIZE_TEST_AI_CONTACT_COMMON_H */
