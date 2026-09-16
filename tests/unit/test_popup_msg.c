/*
 * P11.4: token substitution audit. Walks every GAME.TXT @SECTION this port
 * actually references via popup_msg_fill (a literal ALL-CAPS string in
 * src/core/*.c that also names a real @SECTION — built by intersecting a
 * grep of both, see docs/port_plan.md P11.4), fills each with a fixture
 * PopupMsgTokens (every STRING0-4/COUNTRY/NUMBER0-2 slot populated with a
 * distinct sentinel), and asserts:
 *   1. the real GAME.TXT body was used (not the deliberately-wrong fallback
 *      — proves the section was found and non-empty), and
 *   2. no raw "%STRING"/"%NUMBER"/"%COUNTRY" marker survives in the output
 *      — a survivor means that section uses a token index popup_msg's
 *      apply function doesn't handle (e.g. %STRING5, %NUMBER3), which
 *      would otherwise print literally to the player.
 *
 * This list is not exhaustive of every section in GAME.TXT (PARKED/deep
 * sections aren't wired at all, correctly excluded), and a handful of
 * entries may be incidental name collisions with non-popup strings (cargo
 * names etc.) rather than confirmed popup_msg_fill call sites — harmless
 * over-coverage, not a correctness risk for this test.
 */
#include "core/assets.h"
#include "core/popup_msg.h"

#include <stdio.h>
#include <string.h>

#include "../common/test_runner.h"

static int fail(const char* msg) {
  fprintf(stderr, "unit_popup_msg: FAIL %s\n", msg);
  return 1;
}

/* Built 2026-08-26: grep -oE '"[A-Z][A-Z0-9_]{2,}"' src/core/*.c (dedup)
 * intersected with grep -oE '^@[A-Z][A-Z0-9_]*' COLONIZE/GAME.TXT (dedup). */
static const char* const k_used_sections[] = {
  "ABANDON",       "ABANDON2",      "ALREADYHAVE",   "AMERICA",       "BEGINMENU",
  "BUILT",         "BURIAL1",       "BURIAL2",       "BURIAL3",       "BURNED",
  "BURNED3",       "BUYME0",        "BUYME1",        "CANCELPEACE",   "CANESUGAR",
  "CAPTURED",      "CAPTURED2",     "CAPTURED3",     "CARGOCAPTURE",  "CARGOREADY0",
  "CARGOREADY1",   "CARGOREADY2",   "CLEARCUT",      "COLONISTCAPTURE",
  "COLONISTCAPTURE2", "COLONY",     "COLONYOPTIONS", "COLONYUNIT",    "COTTON",
  "CUSTOM",        "DECLARE",       "DECLAREWAR",    "DEFOREST",      "DEMOTE",
  "DEPLETION",     "DIFFICULTY",    "DISBANDSHIP",   "DONTKNOWSHIPS", "DOS",
  "EFFICIENT",     "EUROPELOSE",    "EUROPEWIN",     "FINDCITY",      "FOOD1",
  "FOOD2",         "FOODLOW",       "FREEDOM",       "FULL",          "FURS",
  "GAMEOPTIONS",   "HOWMUCH1",      "HOWMUCH4",      "HOWMUCH5",      "HOWTOWIN",
  "INDEPENDENCE",  "INDIANBEGFOOD", "INDIANCOME",    "INDIANCOMMENT", "INDIANLOSE",
  "INDIANPEACE",   "INDIANSHUN",    "INDIANWELCOME", "INDIANWIN0",    "INDIANWIN1",
  "INDIANWIN2",    "INEFFICIENT",   "INTERVENE",     "INTERVENTION",  "INVASION",
  "KEEPSTOCKADE",  "KINGTAX",       "LANDFALL",      "LANDHO",        "LEADERNAME",
  "LEARNCRIMINAL", "LEARNMAD",      "LEARNMASTER",   "LOOT",          "LOOT2",
  "LOOTCAPTURE",   "LOSING1",       "LOSING2",       "LOSING3",       "LOSTCITY1",
  "LOSTCITY2",     "LOSTCITY3",     "LOSTCITY5",     "LOSTCITY6",     "LOSTCITY7",
  "LOSTCITY8",     "LOSTCITY9",     "LUMBER",        "MADATSHIPS",    "MERCENARIES",
  "MERCS",         "MORETHANTHREE", "NEEDCOLLEGE",   "NEEDTOOLS",     "NEEDTOOLS0",
  "NEEDUNIVERSITY", "NEWCOLONIST",  "NOCITY",        "NOMOREWAREHOUSE", "NOPLOW",
  "NOPORT",        "NOROAD",        "NOTEACHER",     "ONLYPIO",       "ORE",
  "OVERBOARD",     "PICKINDEPENDENCE", "PICKINDIAN", "PICKMILITARY",  "PICKMUSIC",
  "PICKNATION",    "RAIDBURN",      "RAIDGOLD",      "RAIDNOTHING",   "RAIDSCALP",
  "RAIDSHIP",      "RAIDSTORES",    "REBELMAJORITY", "REBELUNANIMOUS", "REFIT",
  "RENAMECOLONY",  "RETIRE",        "RETIRING",      "RETIRING2",     "SCORED",
  "SCREWED",       "SEACOLONY",     "SEIZURELAND",   "SEIZURESEA",    "SHIPDAMAGE",
  "SHIPOPTIONS",   "SHIPSUNK",      "SIGNTREATY",    "SNEAK",         "SONSDOWN",
  "SONSUP",        "SOONRETIRING0", "SOONRETIRING1", "SOUNDOPTIONS",  "SPOIL1",
  "SPOIL2",        "SPOIL3",        "SPOIL4",        "STARVE1",       "STARVE2",
  "SUREDELETE",    "SUREDISBAND",   "TAXOPTIONS",    "TEAPARTY",      "TOBACCO",
  "TOOLS",         "TOOMOUNTAIN",   "TOONEAR",       "TOOTORY",       "TORYMAJORITY",
  "TORYMINORITY",  "TRADEDELETE",   "TRADESELECT",   "TRAINCRIMINAL", "TRAINFAIL",
  "TRAININDENTURED", "TRAINPROFESSION", "UNITOPTIONS", "UNREST",      "USEDUPTOOLS",
  "VANISH",        "VICEROY",       "VICEROY2",      "VIOLATE",       "WAGONCAPTURE",
  "WAREHOUSEFULL", "WARN1",         "WARN2",         "WARN3",         "WELLSEASONED",
  "WHICHFREEDOM",  "WINNING",
};

/* Catalog is read-only after load, so one process-wide load is safe to
 * share across cases (no reset needed between cases). */
static ColonizeMsgCatalog g_catalog;
static int g_catalog_ready = 0;

static int ensure_catalog(void) {
  if (g_catalog_ready) {
    return 0;
  }
  assets_msg_init(&g_catalog);
  if (!assets_msg_load_file(&g_catalog, "COLONIZE/GAME.TXT")) {
    fprintf(stderr, "unit_popup_msg: GAME.TXT load failed (run from repo root)\n");
    return 1;
  }
  g_catalog_ready = 1;
  return 0;
}

static int case_token_substitution(void) {
  if (ensure_catalog() != 0) {
    return 1;
  }

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = "Alpha";
  tok.string1 = "Bravo";
  tok.string2 = "Charlie";
  tok.string3 = "Delta";
  tok.string4 = "Echo";
  tok.country = "Wonderland";
  tok.number0 = 111;
  tok.has_number0 = true;
  tok.number1 = 222;
  tok.has_number1 = true;
  tok.number2 = 333;
  tok.has_number2 = true;

  static const char* const sentinel = "\x01SHOULD_NOT_SURFACE\x01";
  int checked = 0;
  int rc = 0;
  const size_t n = sizeof(k_used_sections) / sizeof(k_used_sections[0]);
  for (size_t i = 0; i < n; ++i) {
    const char* name = k_used_sections[i];
    char body[2048];
    popup_msg_fill(&g_catalog, name, &tok, sentinel, body, sizeof(body));
    if (strcmp(body, sentinel) == 0) {
      fprintf(stderr, "unit_popup_msg: @%s not found / empty in GAME.TXT\n", name);
      rc = fail("wired section missing from GAME.TXT");
      continue;
    }
    if (strstr(body, "%STRING") || strstr(body, "%NUMBER") || strstr(body, "%COUNTRY")) {
      fprintf(stderr, "unit_popup_msg: @%s left an unresolved token: '%s'\n", name, body);
      rc = fail("unresolved token marker survived substitution");
      continue;
    }
    ++checked;
  }

  if (rc != 0) {
    return rc;
  }
  fprintf(stderr, "unit_popup_msg: ok (%d/%zu sections clean)\n", checked, n);
  return 0;
}

/*
 * bugs.md (King galleon / Euro diplomacy: "the options are appended in the
 * text"): DOS FUN_6f74_32a4 is a blank-line state machine — state 1 body,
 * state 2 choices, state 3 done — so no choice row may ever appear in the
 * body. Sweep every section in the shipped GAME.TXT, not just the wired
 * ones, so a newly wired dialog cannot regress silently.
 */
static int case_choice_leak_sweep(void) {
  if (ensure_catalog() != 0) {
    return 1;
  }

  int rc = 0;
  int swept = 0;
  for (int s = 0; s < g_catalog.section_count; ++s) {
    const ColonizeMsgSection* sec = &g_catalog.sections[s];
    /* @TAXOPTIONS is not a dialog: DOS reads its two rows line-by-line
     * (38fd:40a9 FUN_291f_0928 + 091c) and appends them to the @KINGTAX box,
     * so it has no body/choice split of its own. */
    if (strcmp(sec->name, "TAXOPTIONS") == 0) {
      continue;
    }
    char body[2048];
    popup_msg_section_body(sec, body, sizeof(body), true);
    char choices[8][POPUP_MSG_CHOICE_LEN];
    const int nch = popup_msg_choices(sec, choices, 8);
    for (int c = 0; c < nch; ++c) {
      if (choices[c][0] && strstr(body, choices[c]) != NULL) {
        fprintf(
          stderr, "unit_popup_msg: @%s choice row leaked into the body: '%s'\n", sec->name,
          choices[c]
        );
        rc = fail("choice row present in dialog body");
        break;
      }
    }
    ++swept;
  }

  if (rc != 0) {
    return rc;
  }
  fprintf(stderr, "unit_popup_msg: ok (%d swept for choice leaks)\n", swept);
  return 0;
}

/* @ABANDON: DOS 2f2b caseD_a shows it through the normal compositor with
 * @default=2 pre-selected ("Never! That would be folly."). */
static int case_abandon_default(void) {
  if (ensure_catalog() != 0) {
    return 1;
  }

  int rc = 0;
  const ColonizeMsgSection* ab = assets_msg_find(&g_catalog, "ABANDON");
  if (!ab || popup_msg_section_default(ab) != 2) {
    rc = fail("@ABANDON lost its @default=2");
  }
  if (popup_msg_mss_index_for_section("ABANDON") != 5 ||
      popup_msg_mss_index_for_section("ABANDON2") != 5) {
    rc = fail("@ABANDON/@ABANDON2 lost the MSS5 figure");
  }
  return rc;
}

static const TestCase k_cases[] = {
    {"unit_popup_msg_token_substitution", case_token_substitution},
    {"unit_popup_msg_choice_leak_sweep", case_choice_leak_sweep},
    {"unit_popup_msg_abandon_default", case_abandon_default},
};
TEST_MAIN(k_cases)
