#include "core/unit_chrome.h"
#include "core/unit_chrome_draw.h"

#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/fb.h"
#include "core/strutil.h"
#include "core/units.h"

/*
 * NAMES.TXT @COUNTRY indices (DS:0x848[0..3]). FUN_112b_01ba fills the orders
 * box with this raw index and draws the letter with index - 8. Under the
 * VICEROY.PAL DAC that is England (255,0,0), France (85,85,255), Spain
 * (255,255,85), Dutch (255,113,0).
 */
static const uint8_t k_european_names[4] = {12, 9, 14, 13};
/* DS:0x848[4..11] = NAMES.TXT @TRIBES colour column (97 149 54 87 67 111 118 71), read by
 * FUN_112b_01ba; confirmed by a live DOS read after load. The EXE's static image (15 149 54 11 67
 * 111 117 71) is overwritten at start-up. */
static const uint8_t k_tribe_colors[8] = {97, 149, 54, 87, 67, 111, 118, 71};

/*
 * VICEROY.PAL's RGB at the k_european_names slots (fill) and slot - 8
 * (letter): what DOS shows on the map, colony, Europe and report screens,
 * none of which puts a picture palette into DAC 0..151 (reports program
 * 152..251 only, FUN_3f41_0000).
 * The port's report / foreign-palette screens nearest-match these into the
 * palette they present through. See unit_chrome_blit_unit_for_palette.
 */
const uint8_t k_nation_fill_rgb_native[4][3] = {
  {255, 0, 0}, {85, 85, 255}, {255, 255, 85}, {255, 113, 0}
};
const uint8_t k_nation_letter_rgb_native[4][3] = {
  {170, 0, 0}, {0, 0, 170}, {170, 85, 0}, {170, 73, 0}
};

/* WoI flag state (port_bugs.md): the rebel (human) nation and the crown slot.
 * Set together with unit_chrome_set_crown_nation; -1 outside the WoI. */
static int g_chrome_rebel_nation = -1;

void unit_chrome_set_rebel_nation(int nation_id) {
  g_chrome_rebel_nation = nation_id;
}

/* port_bugs.md: after the Declaration the Royal Expeditionary Force renders
 * WHITE — it is the Crown's army, not the peer nation whose slot it
 * borrows (an English player's REF was showing French blue). */
int g_chrome_crown_nation = -1;


/*
 * Nation shade pair: clear both outs, bail on a missing palette or a
 * non-European slot, then nearest-match the two native RGBs into the active
 * palette.
 */
static void unit_chrome_shade_pair(
  const ColonizePalette* active_palette,
  int nation_id,
  const uint8_t* light_rgb,
  const uint8_t* dark_rgb,
  int* out_light,
  int* out_dark
) {
  if (out_light) {
    *out_light = -1;
  }
  if (out_dark) {
    *out_dark = -1;
  }
  if (!active_palette || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (out_light) {
    *out_light =
      assets_palette_nearest_rgb(active_palette, light_rgb[0], light_rgb[1], light_rgb[2]);
  }
  if (out_dark) {
    *out_dark = assets_palette_nearest_rgb(active_palette, dark_rgb[0], dark_rgb[1], dark_rgb[2]);
  }
}

int unit_chrome_rebel_nation(void) {
  return g_chrome_rebel_nation;
}

/* port_bugs.md #364: the map's tribe chrome (alarm marks, mission cross) drew
 * DS:0x848's raw index — Dutch 13 / 5, which TERRAIN.SS's palette maps to
 * EGA magenta rather than ICONS.SS-native orange. Same nearest-match
 * treatment the unit badges already get. Unlike the colony flag above this
 * pair takes its dark half straight from the letter table, and never remaps
 * the crown slot. */
void unit_chrome_nation_shades_for_palette(
  int nation_id, const ColonizePalette* active_palette, int* out_bright, int* out_dark
) {
  const uint8_t k_black[3] = {0, 0, 0};
  const bool euro = nation_id >= 0 && nation_id < 4;
  unit_chrome_shade_pair(
    active_palette,
    nation_id,
    euro ? k_nation_fill_rgb_native[nation_id] : k_black,
    euro ? k_nation_letter_rgb_native[nation_id] : k_black,
    out_bright,
    out_dark
  );
}

/* Fallback @ORDERS letters if NAMES.TXT is unavailable. */
static const char k_default_order_letters[UNIT_CHROME_ORDERS_MAX] = {
  '-', 'S', 'T', 'G', 'L', 'F', 'F', 'B', 'P', 'R', '-', '-', '-', '-', '-', '-'
};

static char g_order_letters[UNIT_CHROME_ORDERS_MAX];
bool g_orders_loaded = false;

void unit_chrome_init_defaults(void) {
  memcpy(g_order_letters, k_default_order_letters, sizeof(g_order_letters));
  g_orders_loaded = true;
}


void unit_chrome_load_orders(const ColonizeMsgCatalog* names) {
  unit_chrome_init_defaults();
  if (!names) {
    return;
  }
  const ColonizeMsgSection* section = assets_msg_find(names, "ORDERS");
  if (!section) {
    return;
  }
  int n = 0;
  for (int i = 0; i < section->line_count && n < UNIT_CHROME_ORDERS_MAX; ++i) {
    char line[COLONIZE_MSG_LINE_LEN];
    snprintf(line, sizeof(line), "%s", section->lines[i]);
    const char* letter = str_split_name_row(line);
    if (!letter) {
      continue;
    }
    g_order_letters[n++] = letter[0] ? letter[0] : '-';
  }
}

void unit_chrome_set_crown_nation(int nation_id) {
  g_chrome_crown_nation = nation_id;
  if (nation_id < 0) {
    g_chrome_rebel_nation = -1; /* WoI chrome off ⇒ flag overrides off too */
  }
}

int unit_chrome_crown_nation(void) {
  return g_chrome_crown_nation;
}

uint8_t unit_chrome_nation_color(int nation_id) {
  if (nation_id >= 0 && nation_id < 4 && nation_id == g_chrome_crown_nation) {
    return 15; /* white */
  }
  if (nation_id >= 0 && nation_id < 4) {
    return k_european_names[nation_id];
  }
  if (nation_id >= 4 && nation_id <= 11) {
    return k_tribe_colors[nation_id - 4];
  }
  return k_european_names[0];
}

uint8_t unit_chrome_names_color(int nation_id) {
  if (nation_id >= 0 && nation_id < 4 && nation_id == g_chrome_crown_nation) {
    return 15; /* white — REF (see unit_chrome_set_crown_nation) */
  }
  if (nation_id >= 0 && nation_id < 4) {
    return k_european_names[nation_id];
  }
  if (nation_id >= 4 && nation_id <= 11) {
    return k_tribe_colors[nation_id - 4];
  }
  return k_european_names[0];
}

/*
 * Smell audit 2026-09-10 #7 — WHICH ID SPACE THIS IS.
 *
 * These are DOS @UNIT type ids, read literally off FUN_112b_01ba's own
 * dispatch on the unit record's type byte `+0x3146` (viceroy_unpacked.c
 * 2102-2118):
 *
 *   bVar1 = *(byte *)(local_2c + 0x3146);
 *   if ((bVar1 < 0xd) || (0x12 < bVar1)) {            // land types
 *     if (bVar1 == 0x15 || 0x16 || 5 || 4 || 7 || 8)  local_14 = 3;  // TOP_LEFT
 *     else if (bVar1 == 0xc || 10 || 0xb)             local_14 = 2;  // TOP_CENTER
 *   } else {                                          // hulls 0xd..0x12
 *     if (bVar1 == 0xf || 0x10 || 0x11 || 0x12)       local_14 = 1;  // TOP_RIGHT
 *     else                                            local_14 = 3;  // TOP_LEFT
 *   }
 *
 * Callers hand this an OpenCol POOL INDEX (units_display_type_index, units.c;
 * europe_dock_display_type_index / europe_ship_display_type, europe.c), and
 * "an OpenCol POOL INDEX is not a DOS @UNIT id" is the rule units.c:5117 states
 * for the combat gates. It holds here only because of a data-format
 * invariant, not by construction: units_load_types appends NAMES.TXT @UNIT
 * rows in file order, and the DOS exe hardcodes these very ids, so the
 * shipped section is exactly Colonists 0 … Mtd. Warriors 0x16 with no
 * comment or blank rows inside it (verified against COLONIZE/NAMES.TXT) —
 * any roster DOS itself could run therefore lands index == id.
 *
 * Where it does NOT hold: synthetic test fixtures that build a pool by hand
 * in some other order. The only consequence is a badge drawn in the wrong
 * corner (cosmetic, no gameplay path reads this), so the numeric test is
 * kept rather than pushed through a name lookup — this module deliberately
 * takes no ColonizeUnitPool, and converting the space would have to happen
 * in every caller. Do not copy this pattern into a rules path.
 */
UnitChromeCorner unit_chrome_corner_for_type(int dos_unit_type_id, bool damaged) {
  const int t = dos_unit_type_id;
  if (t >= 0x0d && t <= 0x12) {
    if (t == 0x0f || t == 0x10 || t == 0x11 || t == 0x12) {
      return UNIT_CHROME_CORNER_TOP_RIGHT;
    }
    return UNIT_CHROME_CORNER_TOP_LEFT;
  }
  if (t == 4 || t == 5 || t == 7 || t == 8 || t == 0x15 || t == 0x16) {
    return UNIT_CHROME_CORNER_TOP_LEFT;
  }
  if (t == 10 || t == 11 || t == 12) {
    /*
     * DOS's fourth arm is Artillery + the damaged bit, NOT "aboard a ship":
     * raw 2109-2111 is `bVar1 == 0xb && (*(byte *)(local_2c + 0x3148) & 0x80)`
     * → local_14 = 4 (the y+2 box at raw 2253-2254). The port used to pass
     * `aboard_ship_id >= 0` here; every caller now passes the unit's real
     * damaged bit (OpenCol col1_flags15 & 0x80, the same +0x3148 bit7 the
     * damaged-Artillery combat gates read at units.c:11130/11294).
     */
    if (t == 11 && damaged) {
      return UNIT_CHROME_CORNER_TOP_CENTER_DAMAGED;
    }
    return UNIT_CHROME_CORNER_TOP_CENTER;
  }
  return UNIT_CHROME_CORNER_BOTTOM_RIGHT;
}

/* port_bugs.md #705 — see unit_chrome.h for the raw 2148-2172 transcription. */
int unit_chrome_repair_badge_index(
  int dos_unit_type_id, bool damaged, int repair_threshold, int repair_counter, bool halve
) {
  if (!damaged || dos_unit_type_id == 0x0b) {
    return -1;
  }
  int n = repair_threshold - repair_counter;
  if (halve) {
    n = (n + 1) >> 1;
  }
  if (n < 0) {
    n = 0; /* DOS would print a control glyph; the countdown never goes < 0 */
  }
  if (n > 10) {
    n = 10; /* 10 == the '+' slot */
  }
  return UNIT_CHROME_ORDERS_REPAIR_BASE + n;
}

static unsigned g_chrome_plan_nations;

void unit_chrome_set_plan_nations(unsigned mask) {
  g_chrome_plan_nations = mask & 0xfu;
}

/* FUN_112b_01ba ~2134: `glyph = unit[+0x314b]; if (glyph >= 0x80) glyph = 'E'`. */
int unit_chrome_plan_badge_index(int nation_id, unsigned plan, int orders, int cur) {
  if (nation_id < 0 || nation_id > 3 || !(g_chrome_plan_nations & (1u << nation_id))) {
    return cur;
  }
  const int glyph = plan >= 0x80u ? 'E' : (int)plan;
  return UNIT_CHROME_ORDERS_PLAN_BASE + (glyph << 4) + (orders & 15);
}

int unit_chrome_unit_badge_index(const ColonizeUnit* u, int cur) {
  if (u->automated) {
    return UNIT_CHROME_ORDERS_PLAN_BASE + ('A' << 4) + (u->orders & 15);
  }
  return unit_chrome_plan_badge_index(u->nation_id, u->col1_ai_plan, u->orders, cur);
}

int unit_chrome_real_orders(int orders_index) {
  return orders_index >= UNIT_CHROME_ORDERS_PLAN_BASE
           ? (orders_index - UNIT_CHROME_ORDERS_PLAN_BASE) & 15
           : orders_index;
}

char unit_chrome_order_letter(int orders_index, int nation_id) {
  if (!g_orders_loaded) {
    unit_chrome_init_defaults();
  }
  /* #1177a: the AI plan glyph is the badge outright (may be NUL = empty). */
  if (orders_index >= UNIT_CHROME_ORDERS_PLAN_BASE) {
    return (char)((orders_index - UNIT_CHROME_ORDERS_PLAN_BASE) >> 4);
  }
  /* port_bugs.md #705: the digit arm is DOS's last write to the badge char, so it
   * overrides the @ORDERS letter (and the natives clamp below) outright. */
  if (orders_index == UNIT_CHROME_ORDERS_PRIVATEER_X) {
    return 'X';
  }
  if (orders_index >= UNIT_CHROME_ORDERS_HOLDS_BASE) {
    return (char)('0' + (orders_index - UNIT_CHROME_ORDERS_HOLDS_BASE) % 10);
  }
  if (orders_index >= UNIT_CHROME_ORDERS_REPAIR_BASE) {
    const int n = orders_index - UNIT_CHROME_ORDERS_REPAIR_BASE;
    return n < 10 ? (char)('0' + n) : '+';
  }
  if (nation_id > 3) {
    orders_index = 0;
  }
  if (orders_index < 0 || orders_index >= UNIT_CHROME_ORDERS_MAX) {
    orders_index = 0;
  }
  const char ch = g_order_letters[orders_index];
  return ch ? ch : '-';
}

bool unit_chrome_is_repair_badge(int orders_index) {
  return orders_index >= UNIT_CHROME_ORDERS_REPAIR_BASE &&
         orders_index < UNIT_CHROME_ORDERS_PRIVATEER_X;
}

uint8_t unit_chrome_letter_color(int nation_id, int orders_index) {
  if (unit_chrome_is_repair_badge(orders_index)) {
    return nation_id == 2 ? 12 : 15;
  }
  orders_index = unit_chrome_real_orders(orders_index);
  if (nation_id > 3) {
    orders_index = 0;
  }
  /* Sentry (1) and Fortified (6): euro → NAMES color-8, native → 8.
   * For the crown slot NAMES color is 15 (white) so this yields 7 — a
   * derived value with no DOS citation for the REF shade specifically
   * (audit 2026-09-09 #14); kept until a DOS trace pins it. */
  if (orders_index == 1 || orders_index == 6) {
    if (nation_id >= 0 && nation_id < 4) {
      return (uint8_t)(unit_chrome_names_color(nation_id) - 8);
    }
    return 8;
  }
  return 0;
}
