#ifndef COLONIZE_TOOLS_RENDER_COMMON_H
#define COLONIZE_TOOLS_RENDER_COMMON_H

/*
 * Shared bring-up for the three standalone renderers (render_report,
 * render_colony, render_map_panel) — duplication audit TT-33 / TT-34.
 *
 * Each tool used to retype the "read the .SAV, load NAMES.TXT, units +
 * colonies + unit_chrome, europe_load, col1_bridge_apply" prologue and the
 * 320x200 indexed-to-PPM writer. The prologue's error handling was divergent
 * (render_report only warned when the bridge failed and then rendered with
 * NULL pools); it aborts everywhere now, because a failed bridge means the
 * pixels this tool exists to compare are garbage.
 */

#include <stdbool.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/fb.h"
#include "core/font.h"
#include "core/map.h"
#include "core/pik.h"
#include "core/ss.h"
#include "core/units.h"

typedef struct {
  ColonizeCol1Save save;
  int human;
  ColonizeMsgCatalog names;
  ColonizeMsgCatalog labels;
  bool labels_ok;
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  ColonizeWorldMap map;
  EuropeScreen europe;
  ColonizeCol1BridgeResult bridge;
} RenderSaveBundle;

/* Reads the save and brings up live pools exactly as game_apply_col1_save()
 * does. Prints the reason and returns false on any failure (LABELS.TXT is
 * the one optional asset). */
bool render_load_save(const char* data_dir, const char* save_path, RenderSaveBundle* out);

/* Optional sprite sheet / font; both warn and return false when missing. */
bool render_load_sheet(const char* data_dir, const char* name, ColonizeSpriteSheet* out);
bool render_load_font(const char* data_dir, const char* name, ColonizeFont* out);

/* 320x200 indexed framebuffer over `pixels`, cleared to index 0 first — an
 * uncleared buffer leaks stack garbage into whatever the renderer does not
 * paint, which is exactly what these golden dumps must not contain. */
void render_fb_init(ColonizeFramebuffer8* fb, uint8_t* pixels);

/* Binary PPM of a 320x200 indexed framebuffer expanded through `pal`. */
bool render_write_ppm(const char* out_path, const uint8_t* pixels, const ColonizePalette* pal);

#endif /* COLONIZE_TOOLS_RENDER_COMMON_H */
