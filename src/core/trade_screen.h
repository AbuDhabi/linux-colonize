#ifndef COLONIZE_TRADE_SCREEN_H
#define COLONIZE_TRADE_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/font.h"
#include "core/popup.h"
#include "core/ss.h"
#include "platform/platform.h"

/*
 * EDIT TRADE ROUTE screen (DOS FUN_647e_115c input loop / FUN_647e_09da
 * draw / FUN_647e_1064 + FUN_647e_10d2 mouse dispatch).
 *
 * Layout (320×200, DOS pixel geometry):
 *   y 25..44   "Route Name: <name>" — click renames (@TRADENAME)
 *   y ~35      "Route Type: Sea|Land"
 *   y ~50      column headers (LABELS.TXT @ROUTE): Destination /
 *              Unload Cargo (x 125) / Load Cargo (x 208)
 *   y 61+20i   stop rows 0..3: "N. <stop>" + unload icon strip (x 125) +
 *              load icon strip (x 208); ICONS.SS #22+cargo
 *   y ≥ 169    click exits (Enter / Esc too); "OK" bottom right
 *
 * Clicking a stop row: destination column opens the colony picker
 * (999 = Europe, 1000 = "(Delete Destination)"); a cargo column click on an
 * existing icon removes that entry (FUN_647e_0f2c), past the end it opens the
 * @CARGOUNLOAD / @CARGOLOAD single-cargo picker (max 6 per list). Clicking
 * the first empty destination row appends a stop (max 4).
 *
 * The screen itself never opens sub-dialogs — it posts a request that
 * game_loop services (pickers / name entry), mirroring how DOS re-enters
 * FUN_647e_09da after each helper returns.
 */

typedef enum TradeScreenRequest {
  TRADE_SCREEN_REQ_NONE = 0,
  TRADE_SCREEN_REQ_CLOSE,
  TRADE_SCREEN_REQ_RENAME,
  TRADE_SCREEN_REQ_DEST,      /* request_stop == dest_count → append */
  TRADE_SCREEN_REQ_CARGO_ADD, /* request_stop + request_is_load */
  TRADE_SCREEN_REQ_CARGO_REMOVE /* request_stop + request_is_load + request_slot */
} TradeScreenRequest;

typedef struct TradeScreen {
  bool open;
  int route; /* col1.trade_route slot */
  TradeScreenRequest request;
  int request_stop;
  int request_slot;
  bool request_is_load;
  /* LABELS.TXT @ROUTE strings (DS:0x93de table); English fallbacks. */
  char lab_title[32];
  char lab_name[24];
  char lab_type[24];
  char lab_sea[16];
  char lab_land[16];
  char lab_dest[24];
  char lab_unload[24];
  char lab_load[24];
  char lab_delete[32]; /* "(Delete Destination)" — used by game_loop's picker */
  char lab_ok[8];      /* LABELS.TXT @MISC 46 (DS:0x2e16) */
} TradeScreen;

void trade_screen_init(TradeScreen* ts, const ColonizeMsgCatalog* labels);
void trade_screen_open(TradeScreen* ts, int route);
void trade_screen_close(TradeScreen* ts);

/*
 * Stop label: colony name, or the nation's Europe port for colony_index 999.
 * europe_label may be NULL → "Europe".
 */
const char* trade_screen_stop_label(
  const ColonizeColonyPool* colonies,
  uint16_t colony_index,
  const char* europe_label
);

/* Consumes input; sets ts->request for game_loop to service. */
bool trade_screen_handle_input(
  TradeScreen* ts,
  const ColonizeCol1Save* col1,
  const ColonizeSpriteSheet* icons,
  const ColonizeInputState* input
);

void trade_screen_render(
  const TradeScreen* ts,
  const ColonizeCol1Save* col1,
  const ColonizeColonyPool* colonies,
  const char* europe_label,
  const ColonizeFont* font,
  const ColonizeSpriteSheet* wood_tile,
  const ColonizeSpriteSheet* icons,
  const ColonizePopupColors* colors,
  ColonizeFramebuffer8* framebuffer
);

#endif
