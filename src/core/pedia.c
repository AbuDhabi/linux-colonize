#include "core/pedia.h"

#include <stdio.h>
#include <string.h>

#include "data/viceroy_tables.h"

static int pedia_cleared_base_for_forest(int forest_index) {
  const int low3 = forest_index & 7;
  if (low3 == 0) {
    return 4; /* boreal forest clears to grassland art in AMER2 fixtures */
  }
  return low3;
}

static int pedia_forest_phys0(int forest_index) {
  switch (forest_index & 7) {
    case 0:
      return 40; /* boreal transition */
    case 5:
      return 99; /* tropical timber */
    default:
      return -1;
  }
}

static int pedia_terrain_base_sprite(int terrain_index) {
  if (terrain_index >= 0 && terrain_index <= 7) {
    return terrain_index;
  }
  if (terrain_index >= 8 && terrain_index <= 23) {
    const int forest_index = 8 + (terrain_index & 7);
    if (forest_index == 9) {
      return 8; /* scrub forest is TERRAIN-only */
    }
    if (pedia_forest_phys0(forest_index) >= 0) {
      return pedia_cleared_base_for_forest(forest_index);
    }
    return 8;
  }
  if (terrain_index == 24) {
    return 9;
  }
  if (terrain_index == 25) {
    return 10;
  }
  if (terrain_index == 26) {
    return 11;
  }
  /* Mountains / hills: show a cleared land base under the feature. */
  if (terrain_index == 27 || terrain_index == 28) {
    return 4;
  }
  return 0;
}

void pedia_terrain_preview(int terrain_index, PediaTerrainPreview* out) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->terrain_sprite = -1;
  out->class_flag = viceroy_terrain_class(terrain_index);

  if (terrain_index < 0 || terrain_index >= PEDIA_TERRAIN_COUNT) {
    return;
  }

  out->terrain_sprite = pedia_terrain_base_sprite(terrain_index);

  if (terrain_index >= 8 && terrain_index <= 23) {
    const int forest_index = 8 + (terrain_index & 7);
    const int phys = pedia_forest_phys0(forest_index);
    if (phys >= 0 && out->phys0_count < PEDIA_PREVIEW_PHYS0_MAX) {
      out->phys0_sprites[out->phys0_count++] = phys;
    }
    return;
  }

  if (terrain_index == 25 || terrain_index == 26) {
    /* Enclosed coastal pocket: all four 2×2 corner pieces (map draw order). */
    static const int coast_corners[] = {153, 152, 151, 150};
    for (int i = 0; i < 4 && out->phys0_count < PEDIA_PREVIEW_PHYS0_MAX; ++i) {
      out->phys0_sprites[out->phys0_count++] = coast_corners[i];
    }
    return;
  }

  if (terrain_index == 27 || out->class_flag == VICEROY_TERRAIN_CLASS_MOUNTAIN) {
    if (out->phys0_count < PEDIA_PREVIEW_PHYS0_MAX) {
      out->phys0_sprites[out->phys0_count++] = 36; /* isolated mountain */
    }
    return;
  }

  if (terrain_index == 28 || out->class_flag == VICEROY_TERRAIN_CLASS_HILLS) {
    if (out->phys0_count < PEDIA_PREVIEW_PHYS0_MAX) {
      out->phys0_sprites[out->phys0_count++] = 48; /* isolated hill */
    }
  }
}

static void pedia_strip_markup(char* text) {
  char* dst = text;
  for (char* src = text; *src; ++src) {
    if (*src == '~') {
      continue;
    }
    if (*src == '{' || *src == '}') {
      continue;
    }
    if (*src == '^') {
      continue;
    }
    *dst++ = *src;
  }
  *dst = '\0';
}

static bool pedia_extract_title(const char* line, char* out, size_t out_size) {
  const char* open = strstr(line, "^{");
  if (!open) {
    open = strchr(line, '{');
    if (!open) {
      return false;
    }
  } else {
    open += 1; /* point at '{' */
  }
  if (*open != '{') {
    return false;
  }
  open++;
  const char* close = strchr(open, '}');
  if (!close || close <= open) {
    return false;
  }
  size_t n = (size_t)(close - open);
  if (n >= out_size) {
    n = out_size - 1;
  }
  memcpy(out, open, n);
  out[n] = '\0';
  return true;
}

bool pedia_terrain_page(const ColonizeMsgCatalog* catalog, int terrain_index, PediaTerrainPage* out) {
  if (!out) {
    return false;
  }
  memset(out, 0, sizeof(*out));
  out->index = terrain_index;
  pedia_terrain_preview(terrain_index, &out->preview);

  snprintf(out->title, sizeof(out->title), "Terrain %d", terrain_index);

  if (!catalog || terrain_index < 0 || terrain_index >= PEDIA_TERRAIN_COUNT) {
    return false;
  }

  char section_name[32];
  snprintf(section_name, sizeof(section_name), "TERRAIN%d", terrain_index);
  const ColonizeMsgSection* section = assets_msg_find(catalog, section_name);
  if (!section) {
    return false;
  }

  bool have_title = false;
  for (int i = 0; i < section->line_count; ++i) {
    const char* line = section->lines[i];
    if (line[0] == '@') {
      continue;
    }
    if (!have_title && pedia_extract_title(line, out->title, sizeof(out->title))) {
      have_title = true;
      continue;
    }
    if (out->body_line_count >= PEDIA_BODY_MAX_LINES) {
      continue;
    }
    char cleaned[PEDIA_BODY_LINE_LEN];
    snprintf(cleaned, sizeof(cleaned), "%s", line);
    pedia_strip_markup(cleaned);
    if (cleaned[0] == '\0') {
      continue;
    }
    snprintf(
      out->body[out->body_line_count],
      PEDIA_BODY_LINE_LEN,
      "%s",
      cleaned
    );
    out->body_line_count++;
  }
  return true;
}
