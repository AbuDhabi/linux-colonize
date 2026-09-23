#ifndef COLONIZE_TEST_UNITS_COMMON_H
#define COLONIZE_TEST_UNITS_COMMON_H

/* Shared include set for the tests/unit/test_units_*.c suites (split from the
 * former 12k-line tests/unit/test_units.c). No shared helpers live here: each
 * suite keeps its own statics. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/test_catalogs.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/combat_analysis.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/col1_save.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/ai_diplo.h"
#include "core/ai_popup.h"
#include "core/map.h"
#include "core/sound.h"
#include "core/ss.h"
#include "core/unit_chrome.h"
#include "core/unit_stack.h"
#include "core/reports.h"
#include "core/units.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

#endif /* COLONIZE_TEST_UNITS_COMMON_H */
