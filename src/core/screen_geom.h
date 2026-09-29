#ifndef COLONIZE_SCREEN_GEOM_H
#define COLONIZE_SCREEN_GEOM_H

/*
 * Port-only: the logical screen size the main map view is laid out for.
 *
 * DOS is a fixed 320x200 and every other screen in the port still is — they
 * render into a 320x200 buffer that game_render centres in the window, with
 * WOODTILE padding around it. Only the overland map view reads these values
 * and grows: the menu bar and the message log strip stretch horizontally, the
 * right sidebar keeps its 80px width and grows downwards, the viewport takes
 * the rest.
 *
 * Set once at startup and again on every window resize (main.c, from the
 * platform's logical framebuffer size); defaults to 320x200 so tests and the
 * headless paths that never call the setter see DOS geometry.
 */

#define SCREEN_BASE_W 320
#define SCREEN_BASE_H 200

/* Upper bound on the logical screen; the window can be larger, the extra is
 * letterboxed. Sizes main.c's framebuffer allocation. */
#define SCREEN_MAX_W 1920
#define SCREEN_MAX_H 1200

void screen_geom_set(int width, int height);
int screen_geom_w(void);
int screen_geom_h(void);

#endif
