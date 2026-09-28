#include "core/window_log.h"

#include <stdio.h>
#include <string.h>

static char g_lines[WINDOW_LOG_MAX_LINES][WINDOW_LOG_LINE_LEN];
static int g_count;
static int g_head; /* next write slot */

int window_log_strip_height(int lines) {
  const int n = window_log_clamp_lines(lines);
  return n > 0 ? 2 + n * WINDOW_LOG_LINE_H : 0;
}

int window_log_clamp_lines(long long lines) {
  if (lines <= 0) {
    return 0;
  }
  if (lines > WINDOW_LOG_MAX_LINES) {
    return WINDOW_LOG_MAX_LINES;
  }
  return (int)lines;
}

void window_log_push(const char* text) {
  if (!text || text[0] == '\0') {
    return;
  }
  snprintf(g_lines[g_head], WINDOW_LOG_LINE_LEN, "%s", text);
  g_head = (g_head + 1) % WINDOW_LOG_MAX_LINES;
  if (g_count < WINDOW_LOG_MAX_LINES) {
    g_count++;
  }
}

int window_log_count(void) {
  return g_count;
}

const char* window_log_line(int age) {
  if (age < 0 || age >= g_count) {
    return NULL;
  }
  const int slot = (g_head - 1 - age + 2 * WINDOW_LOG_MAX_LINES) % WINDOW_LOG_MAX_LINES;
  return g_lines[slot];
}

void window_log_clear(void) {
  g_count = 0;
  g_head = 0;
  memset(g_lines, 0, sizeof(g_lines));
}
