#include "core/pedia.h"

#include <stdio.h>
#include <string.h>

#include "data/viceroy_tables.h"

static const char* k_category_labels[PEDIA_CAT_COUNT] = {
  "Cargo Types",
  "Unit Types",
  "Terrain Types",
  "Colonist Skills",
  "Colony Buildings",
  "Founding Fathers",
  "Miscellaneous"
};

static const char* k_category_prefixes[] = {
  "CARGO",
  "UNIT",
  "TERRAIN",
  "JOB",
  "BUILDING",
  "FATHER",
  NULL /* misc — titles from @MISCELLANEOUS */
};

static const int k_category_counts[] = {
  PEDIA_CARGO_COUNT,
  PEDIA_UNIT_COUNT,
  PEDIA_TERRAIN_COUNT,
  PEDIA_JOB_COUNT,
  PEDIA_BUILDING_COUNT,
  PEDIA_FATHER_COUNT,
  PEDIA_MISC_COUNT
};

/* @MISCELLANEOUS titles (PEDIA.TXT). Bodies are not in PEDIA.TXT — short
 * reference blurbs from the manual / GAME.TXT tutorials. */
static const char* k_misc_titles[PEDIA_MISC_COUNT] = {
  "Disband",
  "Fortify",
  "Plowing",
  "Roads",
  "Sentry",
  "Trade Route",
  "Veteran Units",
  "Prices",
  "Taxes",
  "Liberty Bells",
  "Crosses",
  "Hammers"
};

static const char* k_misc_bodies[PEDIA_MISC_COUNT] = {
  "Remove a unit from the game permanently. Disbanding a colonist in a "
  "colony adds that person to the settlement. Ships at sea cannot be "
  "disbanded while carrying passengers or cargo.",

  "Order a land unit to dig in. A fortified unit gains a defensive bonus "
  "in combat. Ships use the same order to anchor in a colony harbor.",

  "Pioneers plow clear land with the P key. Plowing raises food, tobacco, "
  "cotton, and sugar yields by one. Plowing a forest clears it first, "
  "making room for agriculture.",

  "Pioneers build roads with the R key. Roads speed movement and increase "
  "fur, lumber, ore, and silver production in the square.",

  "Sentry puts a unit on standby. Units on sentry board the next available "
  "ship, and wake when enemies approach or you activate them.",

  "Automate a ship or wagon along a saved route of colony and Europe "
  "stops with load/unload orders. Edit routes from the Trade menu.",

  "Soldiers and dragoons that win battles (or graduate from a college) "
  "may become veterans, fighting more effectively. Expert soldiers also "
  "immigrate from Europe or the Royal University.",

  "European market prices rise when you buy and fall when you sell. "
  "Boycotted goods cannot be traded until back taxes are paid (or Jakob "
  "Fugger joins Congress).",

  "The King raises your tax rate over time. You may accept the increase "
  "or protest by dumping cargo (boycott). Higher taxes cut into sale "
  "profits in your home port.",

  "Produced by statesmen in town halls (and boosted by presses and "
  "newspapers). Liberty bells elect Founding Fathers and raise rebel "
  "sentiment toward independence.",

  "Produced by churches, cathedrals, and preachers. Crosses accumulate "
  "until a new immigrant appears on the European docks.",

  "Produced by carpenters from lumber. Hammers accumulate toward the "
  "colony's current building project until it is completed."
};

/* ICONS.SS indices for cargo (colony warehouse strip). */
static const int k_cargo_icons[PEDIA_CARGO_COUNT] = {
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37
};

/* Fallback unit icons: 0-based ICONS.SS blit indices (NAMES @UNIT is 1-based). */
static const int k_unit_icons[PEDIA_UNIT_COUNT] = {
  100, 102, 101, 105, 104, 103, 125, 129, 126, 128, 16, 9, 8, 5, 6, 7, 14, 15, 127,
  109, 110, 111, 112,
  100 /* UNIT23 stub */
};

/* Skill pages: prefer matching cargo / unit icons when useful. */
static const int k_job_icons[PEDIA_JOB_COUNT] = {
  22,  /* Farmer → Food */
  23,  /* Sugar */
  24,  /* Tobacco */
  25,  /* Cotton */
  26,  /* Fur trapper */
  27,  /* Lumberjack */
  28,  /* Ore miner */
  29,  /* Silver */
  22,  /* Fisherman → Food */
  31,  /* Distiller → Rum */
  32,  /* Tobacconist → Cigars */
  33,  /* Weaver → Cloth */
  34,  /* Fur trader → Coats */
  101, /* Carpenter → Pioneer tools vibe */
  36,  /* Blacksmith → Tools */
  37,  /* Gunsmith → Muskets */
  100, /* Preacher */
  100, /* Statesman */
  100, /* Teacher */
  100, /* Free colonist */
  101, /* Pioneer */
  102, /* Soldier */
  103, /* Scout */
  104, /* Dragoon */
  105, /* Missionary */
  100, /* Indentured */
  100, /* Criminal */
  109  /* Convert → Brave-ish */
};

int pedia_category_count(PediaCategory category) {
  if (category < 0 || category >= PEDIA_CAT_COUNT) {
    return 0;
  }
  return k_category_counts[category];
}

const char* pedia_category_label(PediaCategory category) {
  if (category < 0 || category >= PEDIA_CAT_COUNT) {
    return "Colonizopedia";
  }
  return k_category_labels[category];
}

const char* pedia_category_section_prefix(PediaCategory category) {
  if (category < 0 || category >= PEDIA_CAT_COUNT) {
    return NULL;
  }
  return k_category_prefixes[category];
}

static int pedia_cleared_base_for_forest(int forest_index) {
  return forest_index & 7;
}

static int pedia_forest_phys0(int forest_index) {
  return viceroy_forest_phys0_sprite(forest_index & 7);
}

static int pedia_terrain_base_sprite(int terrain_index) {
  if (terrain_index >= 0 && terrain_index <= 7) {
    return terrain_index;
  }
  if (terrain_index >= 8 && terrain_index <= 23) {
    const int forest_index = 8 + (terrain_index & 7);
    if (forest_index == 9) {
      return 8;
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
    static const int coast_corners[] = {150, 151, 152, 153};
    for (int i = 0; i < 4 && out->phys0_count < PEDIA_PREVIEW_PHYS0_MAX; ++i) {
      out->phys0_sprites[out->phys0_count++] = coast_corners[i];
    }
    return;
  }

  if (terrain_index == 27 || out->class_flag == VICEROY_TERRAIN_CLASS_MOUNTAIN) {
    if (out->phys0_count < PEDIA_PREVIEW_PHYS0_MAX) {
      out->phys0_sprites[out->phys0_count++] = 36;
    }
    return;
  }

  if (terrain_index == 28 || out->class_flag == VICEROY_TERRAIN_CLASS_HILLS) {
    if (out->phys0_count < PEDIA_PREVIEW_PHYS0_MAX) {
      out->phys0_sprites[out->phys0_count++] = 48;
    }
  }
}

static void pedia_strip_markup(char* text) {
  char* dst = text;
  for (char* src = text; *src; ++src) {
    if (*src == '~' || *src == '{' || *src == '}' || *src == '^') {
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
    open += 1;
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

static void pedia_fill_from_section(
  const ColonizeMsgSection* section,
  PediaPage* out,
  const char* fallback_title
) {
  if (fallback_title && fallback_title[0]) {
    snprintf(out->title, sizeof(out->title), "%s", fallback_title);
  }
  if (!section) {
    return;
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
    snprintf(out->body[out->body_line_count], PEDIA_BODY_LINE_LEN, "%s", cleaned);
    out->body_line_count++;
  }
}

static const ColonizeMsgSection* pedia_find_indexed(
  const ColonizeMsgCatalog* catalog,
  const char* prefix,
  int index
) {
  if (!catalog || !prefix) {
    return NULL;
  }
  char name[32];
  snprintf(name, sizeof(name), "%s%d", prefix, index);
  return assets_msg_find(catalog, name);
}

static void pedia_names_line_title(
  const ColonizeMsgCatalog* names,
  const char* section,
  int index,
  char* out,
  size_t out_size
) {
  out[0] = '\0';
  if (!names) {
    return;
  }
  const ColonizeMsgSection* sec = assets_msg_find(names, section);
  if (!sec || index < 0 || index >= sec->line_count) {
    return;
  }
  const char* line = sec->lines[index];
  const char* comma = strchr(line, ',');
  size_t n = comma ? (size_t)(comma - line) : strlen(line);
  while (n > 0 && line[n - 1] == ' ') {
    n--;
  }
  if (n >= out_size) {
    n = out_size - 1;
  }
  memcpy(out, line, n);
  out[n] = '\0';
}

static int pedia_unit_icon_from_names(const ColonizeMsgCatalog* names, int index) {
  if (index >= 0 && index < PEDIA_UNIT_COUNT) {
    /* Prefer NAMES.TXT @UNIT icon field when present. */
    if (names) {
      const ColonizeMsgSection* sec = assets_msg_find(names, "UNIT");
      if (sec && index < sec->line_count) {
        const char* p = strchr(sec->lines[index], ',');
        if (p) {
          int icon = 0;
          if (sscanf(p + 1, " %d", &icon) == 1 && icon > 0) {
            return icon - 1; /* NAMES 1-based → ICONS.SS 0-based */
          }
        }
      }
    }
    return k_unit_icons[index];
  }
  return -1;
}

static void pedia_set_preview(PediaPage* out, PediaCategory cat, int index, const ColonizeMsgCatalog* names) {
  out->preview_kind = PEDIA_PREVIEW_NONE;
  out->icon_sprite = -1;
  out->building_sprite = -1;
  out->father_index = -1;
  memset(&out->terrain, 0, sizeof(out->terrain));
  out->terrain.terrain_sprite = -1;

  switch (cat) {
    case PEDIA_CAT_TERRAIN:
      out->preview_kind = PEDIA_PREVIEW_TERRAIN;
      pedia_terrain_preview(index, &out->terrain);
      break;
    case PEDIA_CAT_CARGO:
      out->preview_kind = PEDIA_PREVIEW_ICON;
      out->icon_sprite = (index >= 0 && index < PEDIA_CARGO_COUNT) ? k_cargo_icons[index] : -1;
      break;
    case PEDIA_CAT_UNIT:
      out->preview_kind = PEDIA_PREVIEW_ICON;
      out->icon_sprite = pedia_unit_icon_from_names(names, index);
      break;
    case PEDIA_CAT_JOB:
      out->preview_kind = PEDIA_PREVIEW_ICON;
      out->icon_sprite = (index >= 0 && index < PEDIA_JOB_COUNT) ? k_job_icons[index] : -1;
      break;
    case PEDIA_CAT_BUILDING:
      out->preview_kind = PEDIA_PREVIEW_BUILDING;
      out->building_sprite = index; /* BUILDING.SS order matches @BUILDING / PEDIA */
      break;
    case PEDIA_CAT_FATHER:
      out->preview_kind = PEDIA_PREVIEW_FATHER;
      out->father_index = index;
      break;
    case PEDIA_CAT_MISC:
    default:
      break;
  }
}

bool pedia_page(
  const ColonizeMsgCatalog* pedia,
  const ColonizeMsgCatalog* names,
  PediaCategory category,
  int index,
  PediaPage* out
) {
  if (!out) {
    return false;
  }
  memset(out, 0, sizeof(*out));

  const int count = pedia_category_count(category);
  if (count <= 0) {
    return false;
  }
  if (index < 0) {
    index = 0;
  }
  if (index >= count) {
    index = count - 1;
  }

  out->category = category;
  out->index = index;
  out->flat_index = index;
  out->flat_count = count;
  snprintf(out->category_label, sizeof(out->category_label), "%s", pedia_category_label(category));

  char fallback[PEDIA_TITLE_LEN];
  fallback[0] = '\0';

  if (category == PEDIA_CAT_MISC) {
    if (index >= 0 && index < PEDIA_MISC_COUNT) {
      snprintf(out->title, sizeof(out->title), "%s", k_misc_titles[index]);
      const char* src = k_misc_bodies[index];
      out->body_line_count = 0;
      while (*src && out->body_line_count < PEDIA_BODY_MAX_LINES) {
        size_t take = 0;
        size_t last_space = 0;
        while (src[take] && take < 56) {
          if (src[take] == ' ') {
            last_space = take;
          }
          take++;
        }
        if (src[take] && last_space > 0) {
          take = last_space;
        }
        snprintf(out->body[out->body_line_count], PEDIA_BODY_LINE_LEN, "%.*s", (int)take, src);
        out->body_line_count++;
        src += take;
        while (*src == ' ') {
          src++;
        }
      }
    }
    pedia_set_preview(out, category, index, names);
    return true;
  }

  switch (category) {
    case PEDIA_CAT_CARGO:
      pedia_names_line_title(names, "CARGO", index, fallback, sizeof(fallback));
      break;
    case PEDIA_CAT_UNIT:
      pedia_names_line_title(names, "UNIT", index, fallback, sizeof(fallback));
      break;
    case PEDIA_CAT_JOB:
      pedia_names_line_title(names, "JOB", index, fallback, sizeof(fallback));
      break;
    case PEDIA_CAT_BUILDING:
      pedia_names_line_title(names, "BUILDING", index, fallback, sizeof(fallback));
      break;
    case PEDIA_CAT_FATHER: {
      const ColonizeMsgSection* fathers = names ? assets_msg_find(names, "FATHERS") : NULL;
      if (fathers && index >= 0 && index < fathers->line_count) {
        const char* line = fathers->lines[index];
        const char* comma = strchr(line, ',');
        size_t n = comma ? (size_t)(comma - line) : strlen(line);
        while (n > 0 && line[n - 1] == ' ') {
          n--;
        }
        if (n >= sizeof(fallback)) {
          n = sizeof(fallback) - 1;
        }
        memcpy(fallback, line, n);
        fallback[n] = '\0';
      }
      break;
    }
    case PEDIA_CAT_TERRAIN:
      snprintf(fallback, sizeof(fallback), "Terrain %d", index);
      break;
    default:
      break;
  }

  if (fallback[0] == '\0') {
    snprintf(fallback, sizeof(fallback), "%s %d", pedia_category_label(category), index);
  }

  const char* prefix = pedia_category_section_prefix(category);
  const ColonizeMsgSection* section = pedia_find_indexed(pedia, prefix, index);
  pedia_fill_from_section(section, out, fallback);
  pedia_set_preview(out, category, index, names);
  return section != NULL || out->title[0] != '\0';
}

bool pedia_terrain_page(const ColonizeMsgCatalog* catalog, int terrain_index, PediaPage* out) {
  return pedia_page(catalog, NULL, PEDIA_CAT_TERRAIN, terrain_index, out);
}

bool pedia_entry_title(
  const ColonizeMsgCatalog* pedia,
  const ColonizeMsgCatalog* names,
  PediaCategory category,
  int index,
  char* out,
  size_t out_size
) {
  if (!out || out_size == 0) {
    return false;
  }
  PediaPage page;
  if (!pedia_page(pedia, names, category, index, &page) || page.title[0] == '\0') {
    out[0] = '\0';
    return false;
  }
  snprintf(out, out_size, "%s", page.title);
  return true;
}

static int pedia_text_width(const ColonizeFont* font, const char* text) {
  if (!text) {
    return 0;
  }
  int w = 0;
  for (const char* p = text; *p; ++p) {
    const unsigned char ch = (unsigned char)*p;
    if (font && font->section_data && ch < 128 && font->char_widths[ch] > 0) {
      w += font->char_widths[ch];
    } else {
      w += 6;
    }
  }
  return w;
}

static int pedia_list_line_h(const ColonizeFont* font) {
  const int h = font ? (font->max_height + 2) : 8;
  return h < 8 ? 8 : h;
}

/* Column-major layout: fill down each column, then next. */
static void pedia_list_cell(
  int entry_index,
  int count,
  const ColonizeFont* font,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h
) {
  const int line_h = pedia_list_line_h(font);
  const int rows = (count + PEDIA_LIST_COLS - 1) / PEDIA_LIST_COLS;
  const int col_w = 104;
  const int origin_x = 8;
  const int origin_y = 20;
  const int col = (rows > 0) ? (entry_index / rows) : 0;
  const int row = (rows > 0) ? (entry_index % rows) : 0;
  if (out_x) {
    *out_x = origin_x + col * col_w;
  }
  if (out_y) {
    *out_y = origin_y + row * line_h;
  }
  if (out_w) {
    *out_w = col_w - 4;
  }
  if (out_h) {
    *out_h = line_h;
  }
}

static void pedia_list_exit_rect(
  const ColonizeFont* font,
  int fb_w,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h
) {
  const int tw = pedia_text_width(font, PEDIA_LIST_EXIT);
  const int line_h = pedia_list_line_h(font);
  if (out_w) {
    *out_w = tw + 4;
  }
  if (out_h) {
    *out_h = line_h;
  }
  if (out_x) {
    *out_x = fb_w - tw - 8;
  }
  if (out_y) {
    *out_y = 4;
  }
}

void pedia_list_render(
  const ColonizeMsgCatalog* pedia,
  const ColonizeMsgCatalog* names,
  PediaCategory category,
  const ColonizePikImage* wood_bg,
  const ColonizeFont* font,
  int hover_entry,
  ColonizeFramebuffer8* framebuffer
) {
  if (!framebuffer || !framebuffer->pixels) {
    return;
  }
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);
  if (wood_bg && wood_bg->pixels) {
    pik_blit(wood_bg, framebuffer, 0, 0);
  }

  font_draw_text(font, framebuffer, 8, 4, PEDIA_LIST_HEADER, PEDIA_COL_HEADER);

  int ex, ey, ew, eh;
  pedia_list_exit_rect(font, framebuffer->width, &ex, &ey, &ew, &eh);
  font_draw_text(font, framebuffer, ex, ey, PEDIA_LIST_EXIT, PEDIA_COL_LINK);

  const int count = pedia_category_count(category);
  for (int i = 0; i < count; ++i) {
    char title[PEDIA_TITLE_LEN];
    if (!pedia_entry_title(pedia, names, category, i, title, sizeof(title))) {
      snprintf(title, sizeof(title), "%d", i);
    }
    int x, y, w, h;
    pedia_list_cell(i, count, font, &x, &y, &w, &h);
    const uint8_t color = (i == hover_entry) ? PEDIA_COL_LINK_HOVER : PEDIA_COL_LINK;
    font_draw_text(font, framebuffer, x, y, title, color);
  }
}

PediaListHit pedia_list_hit(
  const ColonizeMsgCatalog* pedia,
  const ColonizeMsgCatalog* names,
  PediaCategory category,
  const ColonizeFont* font,
  int mouse_x,
  int mouse_y
) {
  (void)pedia;
  (void)names;
  PediaListHit hit = {PEDIA_LIST_HIT_NONE, -1};
  int ex, ey, ew, eh;
  pedia_list_exit_rect(font, 320, &ex, &ey, &ew, &eh);
  if (mouse_x >= ex && mouse_x < ex + ew && mouse_y >= ey && mouse_y < ey + eh) {
    hit.kind = PEDIA_LIST_HIT_EXIT;
    return hit;
  }

  const int count = pedia_category_count(category);
  for (int i = 0; i < count; ++i) {
    int x, y, w, h;
    pedia_list_cell(i, count, font, &x, &y, &w, &h);
    /* Tighten hit box to text width when possible. */
    char title[PEDIA_TITLE_LEN];
    if (pedia_entry_title(pedia, names, category, i, title, sizeof(title))) {
      const int tw = pedia_text_width(font, title);
      if (tw + 4 < w) {
        w = tw + 4;
      }
    }
    if (mouse_x >= x && mouse_x < x + w && mouse_y >= y && mouse_y < y + h) {
      hit.kind = PEDIA_LIST_HIT_ENTRY;
      hit.entry_index = i;
      return hit;
    }
  }
  return hit;
}
