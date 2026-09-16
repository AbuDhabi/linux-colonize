#ifndef COLONIZE_CORE_UNIT_CHROME_DRAW_H
#define COLONIZE_CORE_UNIT_CHROME_DRAW_H

/*
 * Internal seam between unit_chrome.c (SIM/SHARED colour + flag lookups) and
 * unit_chrome_draw.c (UI painters), created by the 2026-09-16 sim/UI split.
 * Public painter prototypes stay in unit_chrome.h; this header only exposes
 * the handful of helpers that used to be `static` in the one file.
 */

#include <stdbool.h>
#include <stdint.h>

uint8_t unit_chrome_names_color(int nation_id);
void unit_chrome_init_defaults(void);

extern bool g_orders_loaded;
extern int g_chrome_crown_nation;
extern const uint8_t k_nation_fill_rgb_native[4][3];
extern const uint8_t k_nation_letter_rgb_native[4][3];

#endif /* COLONIZE_CORE_UNIT_CHROME_DRAW_H */
