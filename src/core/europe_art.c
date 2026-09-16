/*
 * Europe screen asset load/free, split out of europe.c 2026-09-16 so the
 * market/pool/voyage simulation in europe.c does not reach pik_/ss_. Pure
 * move: europe_load and europe_free are byte-identical to their originals.
 *
 * This file is UI.
 */
#include "core/europe.h"
#include "core/europe_art.h"

#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/pik.h"
#include "core/ss.h"
#include "platform/platform.h"
#include "platform/diagnostics.h"

bool europe_load(EuropeScreen* eu, const char* data_dir, char* err, size_t err_size) {
  if (!eu || !data_dir) {
    snprintf(err, err_size, "europe_load bad args");
    return false;
  }
  memset(eu, 0, sizeof(*eu));

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    snprintf(err, err_size, "failed to load NAMES.TXT for Europe market");
    assets_msg_free(&names);
    return false;
  }
  if (!europe_load_tables(eu, &names)) {
    snprintf(err, err_size, "NAMES.TXT missing usable @CARGO table");
    assets_msg_free(&names);
    return false;
  }

  char pik_path[512];
  char pik_err[256];
  if (!dos_compat_normalize_asset_path(data_dir, "EUROPE.PIK", pik_path, sizeof(pik_path))) {
    snprintf(err, err_size, "EUROPE.PIK path resolve failed");
    assets_msg_free(&names);
    return false;
  }
  if (!pik_load(pik_path, &eu->background, pik_err, sizeof(pik_err))) {
    snprintf(err, err_size, "EUROPE.PIK: %s", pik_err);
    assets_msg_free(&names);
    return false;
  }
  eu->background_ok = true;

  char ss_path[512];
  char ss_err[256];
  if (dos_compat_normalize_asset_path(data_dir, "WOODTILE.SS", ss_path, sizeof(ss_path)) &&
      ss_load(ss_path, &eu->wood_tile, ss_err, sizeof(ss_err))) {
    if (eu->background.has_palette) {
      /*
       * REMAP, not merge: WOODTILE.SS reserves no DAC block of its own (it is
       * black across 152..251, the same block EUROPE.PIK leaves black, plus
       * EUROPE's 120..127 water ramp) — the merge rule is for sheets that ship
       * entries for the host's black block (KING, IND<t>A<n>, MSSn, MYRn, SCORE<nn>).
       * WOODTILE paints only 11 indices and EUROPE.PIK carries identical RGB
       * for all 11, so this remap is an identity today; it stays as insurance
       * against a modded EUROPE.PIK.
       */
      assets_sheet_remap_to_palette(&eu->wood_tile, &eu->background.palette);
    }
    eu->wood_tile_ok = true;
  } else {
    eu->wood_tile_ok = false;
    diag_warn("Europe WOODTILE.SS unavailable");
  }

  europe_reset_campaign(eu);
  europe_set_nation(eu, 0, &names);
  /* Re-init pool/purchase after reset; train table already from load_tables. */
  {
    int train_count = eu->train_count;
    EuropeTrainOption train_copy[EUROPE_TRAIN_MAX];
    memcpy(train_copy, eu->train, sizeof(train_copy));
    europe_reset_campaign(eu);
    eu->train_count = train_count;
    memcpy(eu->train, train_copy, sizeof(train_copy));
    europe_set_nation(eu, 0, &names);
    europe_init_purchase_table(eu);
  }
  assets_msg_free(&names);

  diag_info(
    "Europe screen loaded (%dx%d, %d cargo, %d train, %d purchase)",
    eu->background.width,
    eu->background.height,
    eu->cargo_count,
    eu->train_count,
    eu->purchase_count
  );
  return true;
}

void europe_free(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  pik_free(&eu->background);
  ss_free(&eu->wood_tile);
  memset(eu, 0, sizeof(*eu));
}
