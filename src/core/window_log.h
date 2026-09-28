#ifndef COLONIZE_WINDOW_LOG_H
#define COLONIZE_WINDOW_LOG_H

/*
 * Port-only message log strip under the 320x200 screen (settings.json
 * "display.window_log_lines", windowed mode only; main.c sizes the window and
 * the framebuffer, game_render paints the strip).
 *
 * Sim-side store: popup presentation pushes one flattened line here, {}
 * emphasis markup and all; the UI colours it and elides it to fit. No DOS
 * counterpart.
 */

#define WINDOW_LOG_MAX_LINES 24
/* Whole popup bodies land here — the renderer elides to fit, so truncating
 * the text on the way in would throw away the ending it wants to keep. */
#define WINDOW_LOG_LINE_LEN 384
/* FONTTINY.height (6) + 1px gap — docs/reports.md's row pitch. */
#define WINDOW_LOG_LINE_H 7

/* Tallest strip the framebuffer must be able to hold (main.c's allocation). */
#define WINDOW_LOG_MAX_STRIP_H (2 + WINDOW_LOG_MAX_LINES * WINDOW_LOG_LINE_H)

/* Pixel height of the strip for `lines` rows: 1px black rule + 1px top
 * margin + the rows. */
int window_log_strip_height(int lines);

/* Clamp a settings.json value into 0..WINDOW_LOG_MAX_LINES. */
int window_log_clamp_lines(long long lines);

/* Newest line last. Empty / NULL text is ignored. */
void window_log_push(const char* text);

/* How many lines are held (0..WINDOW_LOG_MAX_LINES). */
int window_log_count(void);

/* age 0 = newest; NULL past the end. */
const char* window_log_line(int age);

void window_log_clear(void);

#endif
