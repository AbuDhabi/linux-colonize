#ifndef COLONIZE_DEBUG_ATLAS_H
#define COLONIZE_DEBUG_ATLAS_H

#include <stdbool.h>
#include <stddef.h>

#include "core/font.h"
#include "core/pik.h"
#include "core/ss.h"
#include "platform/platform.h"

#define DEBUG_ATLAS_MAX_ENTRIES 512
#define DEBUG_ATLAS_NAME_MAX 48

typedef enum DebugAtlasKind {
  DEBUG_ATLAS_KIND_SS = 0,
  DEBUG_ATLAS_KIND_PIK = 1
} DebugAtlasKind;

typedef struct DebugAtlasEntry {
  char name[DEBUG_ATLAS_NAME_MAX];
  DebugAtlasKind kind;
} DebugAtlasEntry;

typedef struct DebugAtlas {
  DebugAtlasEntry entries[DEBUG_ATLAS_MAX_ENTRIES];
  int count;
  int index;  /* current file */
  int scroll; /* sprite row / sprite index / vertical pan */
  ColonizeSpriteSheet ss;
  ColonizePikImage pik;
  bool loaded;
  DebugAtlasKind loaded_kind;
  char load_error[96];
} DebugAtlas;

void debug_atlas_init(DebugAtlas* atlas);
void debug_atlas_free(DebugAtlas* atlas);

/* Scan data_dir for *.SS / *.PIK (sorted). Returns entry count. */
int debug_atlas_scan(DebugAtlas* atlas, const char* data_dir);

/* Load entries[index]. Frees any previously loaded asset. */
bool debug_atlas_load(DebugAtlas* atlas, const char* data_dir, int index);

void debug_atlas_next_file(DebugAtlas* atlas, const char* data_dir, int step);
void debug_atlas_prev_file(DebugAtlas* atlas, const char* data_dir, int step);
void debug_atlas_scroll_by(DebugAtlas* atlas, int delta);
void debug_atlas_page_down(DebugAtlas* atlas);

const ColonizePalette* debug_atlas_palette(const DebugAtlas* atlas);

void debug_atlas_render(
  const DebugAtlas* atlas,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
);

#endif
