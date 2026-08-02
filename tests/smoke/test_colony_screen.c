#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/colony_screen.h"
#include "core/ff.h"
#include "core/map.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  ColonyScreenView view;
  char err[256];
  if (!colony_screen_load(&view, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "colony_screen_load failed: %s\n", err);
    return 1;
  }

  if (!view.frame_ok || view.frame.width != 320 || view.frame.height != 200) {
    fprintf(
      stderr,
      "WOODPANL.PIK expected 320x200, got %dx%d ok=%d\n",
      view.frame.width,
      view.frame.height,
      view.frame_ok ? 1 : 0
    );
    colony_screen_free(&view);
    return 1;
  }
  if (!view.frame.has_palette) {
    fprintf(stderr, "WOODPANL.PIK should carry the colony screen palette\n");
    colony_screen_free(&view);
    return 1;
  }

  if (!view.parch_ok || view.parch.sprite_count < 1) {
    fprintf(stderr, "PARCH.SS missing after load\n");
    colony_screen_free(&view);
    return 1;
  }

  if (!view.wood_tile_ok || view.wood_tile.sprite_count < 1) {
    fprintf(stderr, "WOODTILE.SS missing after load\n");
    colony_screen_free(&view);
    return 1;
  }

  if (!view.buildings_ok || view.buildings.sprite_count < 48) {
    fprintf(stderr, "BUILDING.SS missing/short after load (count=%d, need 48 for tree placeholders)\n", view.buildings.sprite_count);
    colony_screen_free(&view);
    return 1;
  }

  if (!view.bottom_panel_ok || view.bottom_panel.width != 320 || view.bottom_panel.height != 72) {
    fprintf(
      stderr,
      "COLONY.PIK expected 320x72, got %dx%d ok=%d\n",
      view.bottom_panel.width,
      view.bottom_panel.height,
      view.bottom_panel_ok ? 1 : 0
    );
    colony_screen_free(&view);
    return 1;
  }

  if (!view.icons_ok || view.icons.sprite_count < COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_COUNT) {
    fprintf(
      stderr,
      "ICONS.SS missing/short for cargo icons (count=%d, need %d+)\n",
      view.icons.sprite_count,
      COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_COUNT
    );
    colony_screen_free(&view);
    return 1;
  }

  ColonizeColonyPool pool;
  colonies_init(&pool);
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") || !colonies_load_buildings(&pool, &names)) {
    fprintf(stderr, "building type load failed\n");
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  const int pioneer = units_find_type(&units, "Pioneers");
  if (pioneer < 0) {
    fprintf(stderr, "Pioneers type missing\n");
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  ColonizeWorldMap map;
  ColonizeSpriteSheet terrain;
  memset(&map, 0, sizeof(map));
  memset(&terrain, 0, sizeof(terrain));
  if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
    fprintf(stderr, "map_load_mp failed: %s\n", err);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  if (!ss_load("COLONIZE/TERRAIN.SS", &terrain, err, sizeof(err))) {
    fprintf(stderr, "ss_load TERRAIN failed: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  int land_x = -1;
  int land_y = -1;
  /* Prefer a coastal tile so we can assert the empty-coast placeholder. */
  for (int y = 0; y < (int)map.height && land_x < 0; ++y) {
    for (int x = 0; x < (int)map.width && land_x < 0; ++x) {
      if (map_tile_is_coastal(&map, x, y)) {
        land_x = x;
        land_y = y;
      }
    }
  }
  if (land_x < 0) {
    for (int y = 0; y < (int)map.height && land_x < 0; ++y) {
      for (int x = 0; x < (int)map.width && land_x < 0; ++x) {
        if (map_tile_is_land(&map, x, y)) {
          land_x = x;
          land_y = y;
        }
      }
    }
  }
  if (land_x < 0) {
    fprintf(stderr, "no land tile for colony\n");
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  const bool sample_coastal = map_tile_is_coastal(&map, land_x, land_y);

  const int cid = colonies_found(&pool, &map, land_x, land_y, pioneer, 100, 0, 0);
  if (cid < 0) {
    fprintf(stderr, "colonies_found failed\n");
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  const ColonizeColony* sample = colonies_get(&pool, cid);

  ColonizeSpriteSheet phys0;
  memset(&phys0, 0, sizeof(phys0));
  const bool phys0_ok = ss_load("COLONIZE/PHYS0.SS", &phys0, err, sizeof(err));

  /* Need a font so cargo amounts are drawn into the strip. */
  ColonizeFont font;
  memset(&font, 0, sizeof(font));
  const bool font_ok = ff_load("COLONIZE/FONTTINY.FF", &font, err, sizeof(err));

  uint8_t pixels[320 * 200];
  ColonizeFramebuffer8 fb = {.width = 320, .height = 200, .pixels = pixels};
  colony_screen_render(
    &view, &pool, sample, &units, &map, &terrain, phys0_ok ? &phys0 : NULL, 1492, 0, 1000,
    font_ok ? &font : NULL, &fb
  );

  if (pixels[0] == 0) {
    fprintf(stderr, "render produced empty top-left pixel (WOODPANL missing?)\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  const int bottom_idx = COLONY_BOTTOM_PANEL_Y * 320;
  if (pixels[bottom_idx] == 0) {
    fprintf(stderr, "bottom panel row looks empty at y=%d\n", COLONY_BOTTOM_PANEL_Y);
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  /* Town Hall sprite is blitted near viewport (70,4). */
  bool building_pixel = false;
  for (int y = COLONY_VIEWPORT_Y + 4; y < COLONY_VIEWPORT_Y + 40 && !building_pixel; ++y) {
    for (int x = 70; x < 120; ++x) {
      if (pixels[y * 320 + x] != 0) {
        building_pixel = true;
        break;
      }
    }
  }
  if (!building_pixel) {
    fprintf(stderr, "expected building pixels in Town Hall slot\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  /* Warehouse is not a starter — empty slot should show a tree clump (BUILDING.SS #43). */
  const int warehouse = colonies_find_building(&pool, "Warehouse");
  if (warehouse < 0 || (sample && sample->has_building[warehouse])) {
    fprintf(stderr, "expected founded colony without Warehouse\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  bool tree_pixel = false;
  for (int y = COLONY_VIEWPORT_Y + 72; y < COLONY_VIEWPORT_Y + 94 && !tree_pixel; ++y) {
    for (int x = 88; x < 132; ++x) {
      if (pixels[y * 320 + x] != 0) {
        tree_pixel = true;
        break;
      }
    }
  }
  if (!tree_pixel) {
    fprintf(stderr, "expected tree-clump placeholder in Warehouse slot\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  const int stockade = colonies_find_building(&pool, "Stockade");
  if (stockade < 0 || (sample && sample->has_building[stockade])) {
    fprintf(stderr, "expected founded colony without Stockade\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  const int docks = colonies_find_building(&pool, "Docks");
  if (docks < 0 || (sample && sample->has_building[docks])) {
    fprintf(stderr, "expected founded colony without Docks\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  /* Fence (BUILDING.SS #16) bottom-right of the buildings section. */
  const int fence_x = COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - 73;
  const int fence_y = COLONY_VIEWPORT_Y + COLONY_VIEWPORT_H - 18;
  bool fence_pixel = false;
  for (int y = fence_y; y < fence_y + 18 && !fence_pixel; ++y) {
    for (int x = fence_x; x < fence_x + 73; ++x) {
      if (pixels[y * 320 + x] != 0) {
        fence_pixel = true;
        break;
      }
    }
  }
  if (!fence_pixel) {
    fprintf(stderr, "expected fence pixels (BUILDING.SS #16) without Stockade at bottom-right\n");
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  /* Coastal colonies without Docks show empty coast placeholder (#45) above the fence. */
  if (sample_coastal) {
    const int coast_x = COLONY_VIEWPORT_X + COLONY_VIEWPORT_W - 75;
    const int coast_y = fence_y - 48;
    bool coast_pixel = false;
    for (int y = coast_y; y < coast_y + 48 && !coast_pixel; ++y) {
      for (int x = coast_x; x < coast_x + 75; ++x) {
        if (pixels[y * 320 + x] != 0) {
          coast_pixel = true;
          break;
        }
      }
    }
    if (!coast_pixel) {
      fprintf(stderr, "expected coastal placeholder (BUILDING.SS #45) above fence\n");
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
  }

  /* 1px black section separators (top/middle, middle/bottom, buildings/minimap). */
  for (int x = 0; x < 320; ++x) {
    if (pixels[COLONY_TOP_SEPARATOR_Y * 320 + x] != 0) {
      fprintf(stderr, "expected black top separator at y=%d\n", COLONY_TOP_SEPARATOR_Y);
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    if (pixels[COLONY_BOTTOM_SEPARATOR_Y * 320 + x] != 0) {
      fprintf(stderr, "expected black bottom separator at y=%d\n", COLONY_BOTTOM_SEPARATOR_Y);
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
  }
  for (int y = COLONY_MIDDLE_Y; y < COLONY_BOTTOM_SEPARATOR_Y; ++y) {
    if (pixels[y * 320 + COLONY_VIEWPORT_W] != 0) {
      fprintf(stderr, "expected black vertical separator at x=%d y=%d\n", COLONY_VIEWPORT_W, y);
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
  }

  /* 3×3 minimap centered in the WOODTILE section with equal L/R and T/B margins. */
  const int grid_px = COLONY_MINIMAP_GRID * COLONY_MINIMAP_TILE;
  const int margin_x = (COLONY_MINIMAP_SECTION_W - grid_px) / 2;
  const int margin_y = (COLONY_MINIMAP_SECTION_H - grid_px) / 2;
  if (margin_x != margin_y || margin_x < 1) {
    fprintf(
      stderr,
      "expected equal WOODTILE margins, got L/R=%d T/B=%d (section %dx%d grid %d)\n",
      margin_x,
      margin_y,
      COLONY_MINIMAP_SECTION_W,
      COLONY_MINIMAP_SECTION_H,
      grid_px
    );
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }
  const int mini_x = COLONY_MINIMAP_SECTION_X + margin_x;
  const int mini_y = COLONY_MINIMAP_SECTION_Y + margin_y;
  const int mini_x1 = mini_x + grid_px - 1;
  const int mini_y1 = mini_y + grid_px - 1;
  if (pixels[mini_y * 320 + mini_x] == 0 || pixels[mini_y1 * 320 + mini_x1] == 0) {
    fprintf(stderr, "minimap corners look empty (expected centered 3x3)\n");
    if (font_ok) {
      ff_free(&font);
    }
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  /* Cargo strip: Food icon (#22) in first slot, amount digits below. */
  {
    const int food_slot_x = COLONY_CARGO_SLOT_X0;
    bool icon_pixel = false;
    for (int y = COLONY_CARGO_STRIP_Y; y < COLONY_CARGO_STRIP_Y + 12 && !icon_pixel; ++y) {
      for (int x = food_slot_x; x < food_slot_x + COLONY_CARGO_SLOT_W; ++x) {
        if (pixels[y * 320 + x] != 0 && pixels[y * 320 + x] != 56) {
          icon_pixel = true;
          break;
        }
      }
    }
    if (!icon_pixel) {
      fprintf(stderr, "expected Food cargo icon in first COLONY.PIK slot\n");
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    bool amount_pixel = false;
    for (int y = COLONY_CARGO_NUM_Y; y < COLONY_CARGO_NUM_Y + 6 && !amount_pixel; ++y) {
      for (int x = food_slot_x; x < food_slot_x + COLONY_CARGO_SLOT_W; ++x) {
        if (pixels[y * 320 + x] == 15) {
          amount_pixel = true;
          break;
        }
      }
    }
    if (font_ok && !amount_pixel) {
      fprintf(stderr, "expected Food amount digits under cargo icon\n");
      ff_free(&font);
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    /* Tools slot (index 14) should also have an icon after founding with 100 tools. */
    const int tools_slot_x = COLONY_CARGO_SLOT_X0 + COLONIZE_CARGO_TOOLS * COLONY_CARGO_PITCH;
    bool tools_icon = false;
    for (int y = COLONY_CARGO_STRIP_Y; y < COLONY_CARGO_STRIP_Y + 12 && !tools_icon; ++y) {
      for (int x = tools_slot_x; x < tools_slot_x + COLONY_CARGO_SLOT_W; ++x) {
        if (pixels[y * 320 + x] != 0 && pixels[y * 320 + x] != 56) {
          tools_icon = true;
          break;
        }
      }
    }
    if (!tools_icon) {
      fprintf(stderr, "expected Tools cargo icon in slot 14\n");
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
  }

  colony_screen_set_status(&view, "test status");
  if (strcmp(view.status, "test status") != 0) {
    fprintf(stderr, "set_status failed\n");
    if (font_ok) {
      ff_free(&font);
    }
    if (phys0_ok) {
      ss_free(&phys0);
    }
    ss_free(&terrain);
    map_free(&map);
    assets_msg_free(&names);
    colony_screen_free(&view);
    return 1;
  }

  /* Hit-tests: colonist row, carpenter slot, construction banner, exit. */
  {
    colony_screen_reset_ui(&view);
    ColonyScreenHitResult hit =
      colony_screen_hit_test(
        &view, &pool, sample, &units, COLONY_COLONIST_LIST_X + 4, COLONY_COLONIST_LIST_Y0 + 2
      );
    if (hit.kind != COLONY_HIT_COLONIST || hit.index != 0) {
      fprintf(stderr, "expected colonist hit got kind=%d idx=%d\n", (int)hit.kind, hit.index);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    const int carpenter = colonies_find_building(&pool, "Carpenter's Shop");
    hit = colony_screen_hit_test(
      &view,
      &pool,
      sample,
      &units,
      COLONY_VIEWPORT_X + 8 + 8,
      COLONY_VIEWPORT_Y + 44 + 8
    );
    if (hit.kind != COLONY_HIT_BUILDING || hit.index != carpenter) {
      fprintf(
        stderr,
        "expected carpenter building hit got kind=%d idx=%d (carpenter=%d)\n",
        (int)hit.kind,
        hit.index,
        carpenter
      );
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    {
      int ox = 0, oy = 0;
      colony_screen_minimap_origin(&ox, &oy);
      /* North surround tile (center of top-middle cell). */
      hit = colony_screen_hit_test(
        &view,
        &pool,
        sample,
        &units,
        ox + COLONY_MINIMAP_TILE + COLONY_MINIMAP_TILE / 2,
        oy + COLONY_MINIMAP_TILE / 2
      );
      if (hit.kind != COLONY_HIT_AREA_TILE || hit.index != 0) {
        fprintf(
          stderr,
          "expected north area tile hit got kind=%d idx=%d\n",
          (int)hit.kind,
          hit.index
        );
        if (font_ok) {
          ff_free(&font);
        }
        if (phys0_ok) {
          ss_free(&phys0);
        }
        ss_free(&terrain);
        map_free(&map);
        assets_msg_free(&names);
        colony_screen_free(&view);
        return 1;
      }
    }

    hit = colony_screen_hit_test(
      &view, &pool, sample, &units, COLONY_VIEWPORT_X + 10, COLONY_CONSTRUCTION_BANNER_Y + 2
    );
    if (hit.kind != COLONY_HIT_CONSTRUCTION_BANNER) {
      fprintf(stderr, "expected construction banner hit got kind=%d\n", (int)hit.kind);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    hit =
      colony_screen_hit_test(&view, &pool, sample, &units, COLONY_EXIT_X + 2, COLONY_BOTTOM_PANEL_Y + 4);
    if (hit.kind != COLONY_HIT_EXIT) {
      fprintf(stderr, "expected exit hit got kind=%d\n", (int)hit.kind);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    colony_screen_open_construction(&view, &pool, cid);
    if (!view.construction_open || view.buildable_count <= 0) {
      fprintf(stderr, "construction popup failed to open/list\n");
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    /* Seed dialog metrics as render would. */
    view.construction_dialog_x = 70;
    view.construction_dialog_y = 24;
    view.construction_dialog_w = 180;
    view.construction_dialog_h = 80;
    view.construction_list_y0 = 40;
    view.construction_line_h = 10;
    hit = colony_screen_hit_test(&view, &pool, sample, &units, 80, 42);
    if (hit.kind != COLONY_HIT_CONSTRUCTION_CLEAR) {
      fprintf(stderr, "expected construction clear hit got kind=%d\n", (int)hit.kind);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    hit = colony_screen_hit_test(&view, &pool, sample, &units, 80, 52);
    if (hit.kind != COLONY_HIT_CONSTRUCTION_ROW || hit.index != 0) {
      fprintf(stderr, "expected construction row 0 got kind=%d idx=%d\n", (int)hit.kind, hit.index);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    hit = colony_screen_hit_test(&view, &pool, sample, &units, 10, 10);
    if (hit.kind != COLONY_HIT_CONSTRUCTION_OUTSIDE) {
      fprintf(stderr, "expected construction outside hit got kind=%d\n", (int)hit.kind);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    colony_screen_close_construction(&view);
  }

  /* Warehouse ↔ transport transfer + capacity. */
  {
    ColonizeColony* col = colonies_get_mut(&pool, cid);
    const int caravel = units_find_type(&units, "Caravel");
    if (!col || caravel < 0) {
      fprintf(stderr, "missing colony or Caravel for transfer test\n");
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    const int ship_id = units_spawn_allow_stack(&units, caravel, col->x, col->y);
    ColonizeUnit* ship = units_get(&units, ship_id);
    if (!ship || !units_is_transport(&units, ship_id)) {
      fprintf(stderr, "failed to dock Caravel transport id=%d\n", ship_id);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    ship->nation_id = col->nation_id;

    col->stock[COLONIZE_CARGO_SUGAR] = 150;
    const int cap0 = colonies_warehouse_capacity(&pool, col, COLONIZE_CARGO_SUGAR);
    if (cap0 != 100) {
      fprintf(stderr, "expected base sugar capacity 100 got %d\n", cap0);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    const int food_cap = colonies_warehouse_capacity(&pool, col, COLONIZE_CARGO_FOOD);
    if (food_cap != 199) {
      fprintf(stderr, "expected food capacity 199 got %d\n", food_cap);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    const int loaded =
      colonies_transfer_to_unit(&pool, cid, &units, ship_id, COLONIZE_CARGO_SUGAR, 100);
    if (loaded != 100 || col->stock[COLONIZE_CARGO_SUGAR] != 50) {
      fprintf(
        stderr,
        "load failed loaded=%d stock=%d\n",
        loaded,
        col->stock[COLONIZE_CARGO_SUGAR]
      );
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    if (ship->hold_goods_amount[0] != 100 || ship->hold_goods_type[0] != COLONIZE_CARGO_SUGAR) {
      fprintf(stderr, "hold not filled after load\n");
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    bool full = false;
    const int unloaded =
      colonies_transfer_from_unit(&pool, cid, &units, ship_id, 0, &full);
    if (unloaded != 50 || !full || col->stock[COLONIZE_CARGO_SUGAR] != 100) {
      fprintf(
        stderr,
        "capacity unload expected 50/full stock=100 got unloaded=%d full=%d stock=%d\n",
        unloaded,
        full ? 1 : 0,
        col->stock[COLONIZE_CARGO_SUGAR]
      );
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    if (ship->hold_goods_amount[0] != 50) {
      fprintf(stderr, "expected 50 sugar left in hold got %d\n", ship->hold_goods_amount[0]);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    if (warehouse >= 0) {
      col->has_building[warehouse] = true;
    }
    const int cap_wh = colonies_warehouse_capacity(&pool, col, COLONIZE_CARGO_SUGAR);
    if (cap_wh != 200) {
      fprintf(stderr, "expected Warehouse capacity 200 got %d\n", cap_wh);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    full = false;
    const int unloaded2 =
      colonies_transfer_from_unit(&pool, cid, &units, ship_id, 0, &full);
    if (unloaded2 != 50 || full || col->stock[COLONIZE_CARGO_SUGAR] != 150 ||
        ship->hold_goods_amount[0] != 0) {
      fprintf(
        stderr,
        "second unload failed moved=%d full=%d stock=%d hold=%d\n",
        unloaded2,
        full ? 1 : 0,
        col->stock[COLONIZE_CARGO_SUGAR],
        ship->hold_goods_amount[0]
      );
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    const int best = colonies_best_load_cargo(col);
    if (best != COLONIZE_CARGO_SUGAR) {
      fprintf(stderr, "best load cargo expected sugar got %d\n", best);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    colony_screen_reset_ui(&view);
    colony_screen_refresh_transports(&view, &units, col);
    if (view.docked_transport_count < 1 || view.transport_unit_id != ship_id) {
      fprintf(
        stderr,
        "expected docked transport auto-select count=%d id=%d (ship=%d)\n",
        view.docked_transport_count,
        view.transport_unit_id,
        ship_id
      );
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }

    ColonyScreenHitResult thit = colony_screen_hit_test(
      &view,
      &pool,
      col,
      &units,
      COLONY_CARGO_SLOT_X0 + COLONIZE_CARGO_SUGAR * COLONY_CARGO_PITCH + 4,
      COLONY_CARGO_STRIP_Y + 2
    );
    if (thit.kind != COLONY_HIT_CARGO_SLOT || thit.index != COLONIZE_CARGO_SUGAR) {
      fprintf(
        stderr,
        "expected cargo slot sugar hit got kind=%d idx=%d\n",
        (int)thit.kind,
        thit.index
      );
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    thit = colony_screen_hit_test(
      &view, &pool, col, &units, COLONY_TRANSPORT_X + 4, COLONY_TRANSPORT_Y + 4
    );
    if (thit.kind != COLONY_HIT_TRANSPORT || thit.index != 0) {
      fprintf(
        stderr, "expected transport hit got kind=%d idx=%d\n", (int)thit.kind, thit.index
      );
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
    /* Reload one hold so hold hit-test / unload path stays meaningful. */
    colonies_transfer_to_unit(&pool, cid, &units, ship_id, COLONIZE_CARGO_SUGAR, 40);
    thit =
      colony_screen_hit_test(&view, &pool, col, &units, COLONY_HOLD_X + 2, COLONY_HOLD_Y + 2);
    if (thit.kind != COLONY_HIT_HOLD || thit.index != 0) {
      fprintf(stderr, "expected hold hit got kind=%d idx=%d\n", (int)thit.kind, thit.index);
      if (font_ok) {
        ff_free(&font);
      }
      if (phys0_ok) {
        ss_free(&phys0);
      }
      ss_free(&terrain);
      map_free(&map);
      assets_msg_free(&names);
      colony_screen_free(&view);
      return 1;
    }
  }

  fprintf(
    stderr,
    "colony screen tests ok (WOODPANL=%dx%d PARCH=%d WOODTILE=%d BUILDING=%d ICONS=%d COLONY=%dx%d pop=%d mini=%dx%d@%d,%d margin=%d)\n",
    view.frame.width,
    view.frame.height,
    view.parch.sprite_count,
    view.wood_tile.sprite_count,
    view.buildings.sprite_count,
    view.icons.sprite_count,
    view.bottom_panel.width,
    view.bottom_panel.height,
    sample ? sample->population : -1,
    COLONY_MINIMAP_GRID,
    COLONY_MINIMAP_TILE,
    mini_x,
    mini_y,
    margin_x
  );
  if (font_ok) {
    ff_free(&font);
  }
  if (phys0_ok) {
    ss_free(&phys0);
  }
  ss_free(&terrain);
  map_free(&map);
  assets_msg_free(&names);
  colony_screen_free(&view);
  diag_shutdown();
  return 0;
}
