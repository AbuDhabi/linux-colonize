#include "core/screen_geom.h"

static int s_screen_w = SCREEN_BASE_W;
static int s_screen_h = SCREEN_BASE_H;

void screen_geom_set(int width, int height) {
  if (width < SCREEN_BASE_W) {
    width = SCREEN_BASE_W;
  }
  if (height < SCREEN_BASE_H) {
    height = SCREEN_BASE_H;
  }
  if (width > SCREEN_MAX_W) {
    width = SCREEN_MAX_W;
  }
  if (height > SCREEN_MAX_H) {
    height = SCREEN_MAX_H;
  }
  s_screen_w = width;
  s_screen_h = height;
}

int screen_geom_w(void) {
  return s_screen_w;
}

int screen_geom_h(void) {
  return s_screen_h;
}
