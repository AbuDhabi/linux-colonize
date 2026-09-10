/*
 * Strong link stubs for the four slim unit-test targets (unit_units,
 * unit_combat_strength, unit_colonies, unit_reports) that compile ai_diplo.c
 * without ai_contact.c / ai_euro.c. ONLY those targets may list this file.
 *
 * These used to be __attribute__((weak)) definitions inside ai_diplo.c.
 * That was a trap (2026-09-10, audit #15 fallout): in a static-archive link
 * (colonize_core) the linker only pulls an archive member when an UNRESOLVED
 * symbol demands it — a weak definition in ai_diplo.o already satisfies the
 * reference, so ai_contact.o / ai_euro.o were silently never linked into
 * unit_ai_diplo and the "fallbacks" replaced the real functions there.
 * Strong stubs in a file the full targets never compile cannot shadow.
 */
#include "ai_contact.h"
#include "ai_diplo.h"
#include "ai_euro.h"

/* Slim targets have no tribe table. */
const char* ai_contact_tribe_name(int nation_id) {
  (void)nation_id;
  return "natives";
}

/* Bare clamped delta; the 00f2 escalation tail (mission expel / @INDIANBURN)
 * needs ai_contact's machinery and is absent from the slim targets. */
void ai_contact_alarm_delta_00f2(ColonizeTurnContext* ctx, int nation_id, int euro, int delta) {
  ai_diplo_indian_alarm_delta(ctx->col1, nation_id, euro, delta);
}

/* No units visible to the exposure walk; slim targets never run the
 * Euro-vs-Euro 153e war tick. */
int ai_contact_land_combat_sum(
  const ColonizeTurnContext* ctx, int nation, int continent, int exposed_only, int cap
) {
  (void)ctx;
  (void)nation;
  (void)continent;
  (void)exposed_only;
  (void)cap;
  return 0;
}

/* Never war-worthy without ai_euro.c. */
int ai_euro_10ec_war_worthy(const ColonizeTurnContext* ctx, int a, int b) {
  (void)ctx;
  (void)a;
  (void)b;
  return 0;
}
