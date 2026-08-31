#include "core/pedia.h"

#include <stdio.h>
#include <string.h>

#include "core/unit_chrome.h"
#include "core/units.h"
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
  109, 110, 111, 112
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
  PediaCategory category,
  int entry_index,
  int count,
  const ColonizeFont* font,
  int* out_x,
  int* out_y,
  int* out_w,
  int* out_h
) {
  const int line_h = pedia_list_line_h(font);
  /* Founding Fathers list uses two wider columns instead of three. */
  const int cols = (category == PEDIA_CAT_FATHER) ? 2 : PEDIA_LIST_COLS;
  const int rows = (count + cols - 1) / cols;
  const int col_w = (category == PEDIA_CAT_FATHER) ? 150 : 104;
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

/*
 * List rows can be a filtered view of the category (Colonist Skills hides
 * the cut Teacher profession, @JOB 18 — leftover from pre-release). Slot =
 * visual row, id = the real category index the article opens with.
 */
static int pedia_list_slot_count(PediaCategory category) {
  const int count = pedia_category_count(category);
  if (category == PEDIA_CAT_JOB) {
    return count - 1;
  }
  return count;
}

static int pedia_list_slot_id(PediaCategory category, int slot) {
  if (category == PEDIA_CAT_JOB && slot >= 18) {
    return slot + 1;
  }
  return slot;
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

  const int count = pedia_list_slot_count(category);
  for (int i = 0; i < count; ++i) {
    const int id = pedia_list_slot_id(category, i);
    char title[PEDIA_TITLE_LEN];
    if (!pedia_entry_title(pedia, names, category, id, title, sizeof(title))) {
      snprintf(title, sizeof(title), "%d", id);
    }
    int x, y, w, h;
    pedia_list_cell(category, i, count, font, &x, &y, &w, &h);
    const uint8_t color = (id == hover_entry) ? PEDIA_COL_LINK_HOVER : PEDIA_COL_LINK;
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

  const int count = pedia_list_slot_count(category);
  for (int i = 0; i < count; ++i) {
    const int id = pedia_list_slot_id(category, i);
    int x, y, w, h;
    pedia_list_cell(category, i, count, font, &x, &y, &w, &h);
    /* Tighten hit box to text width when possible. */
    char title[PEDIA_TITLE_LEN];
    if (pedia_entry_title(pedia, names, category, id, title, sizeof(title))) {
      const int tw = pedia_text_width(font, title);
      if (tw + 4 < w) {
        w = tw + 4;
      }
    }
    if (mouse_x >= x && mouse_x < x + w && mouse_y >= y && mouse_y < y + h) {
      hit.kind = PEDIA_LIST_HIT_ENTRY;
      hit.entry_index = id;
      return hit;
    }
  }
  return hit;
}

/* ------------------------------------------------------------------ */
/* DOS-fidelity article pages (segment 6cb2 article builders).        */
/* ------------------------------------------------------------------ */

/*
 * DOS DS tables, extracted from a running VICEROY.EXE image
 * (dosbox-x-dumps/find_memory, DS=237D):
 *  - DS:0x2f4  job -> building-chain start (FUN_15eb_0aec)
 *  - DS:0x2ca  building -> worker job (FUN_15eb_14d6)
 *  - building record +4: next building in upgrade chain (walked by 6cb2_1820)
 *  - building record +3: prerequisite building (shown by 6cb2_1ba8)
 *  - DS:0x192  pedia terrain -> prime resource id
 */
static const signed char k_pedia_job_chain_start[19] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, 27, 24, 21, 32, 35, 39, 3, 37, 9, 12
};

static const signed char k_pedia_building_job[PEDIA_BUILDING_COUNT] = {
  21, 21, 21, 15, 15, 15, -1, -1, -1, 17, 17, 17, 18, 18, 18, -1, -1, -1, -1, -1, -1,
  11, 11, 11, 10, 10, 10, 9,  9,  9,  17, 17, 12, 12, 12, 13, 13, 16, 16, 14, 14, 14
};

static const signed char k_pedia_building_next[PEDIA_BUILDING_COUNT] = {
  1,  2,  -1, 4,  5,  -1, 7,  8,  -1, -1, 11, -1, 13, 14, -1, 16, -1, -1, -1, 20, -1,
  22, 23, -1, 25, 26, -1, 28, 29, -1, 31, -1, 33, 34, -1, 36, -1, 38, -1, 40, 41, -1
};

static const signed char k_pedia_building_prereq[PEDIA_BUILDING_COUNT] = {
  -1, 0,  1,  -1, 3,  4,  -1, 6,  7,  -1, 9,  10, -1, 12, 13, -1, 15, -1, -1, -1, 19,
  -1, 21, 22, -1, 24, 25, -1, 27, 28, 11, 30, -1, 32, 33, -1, 35, -1, 37, -1, 39, 40
};

static const signed char k_pedia_terrain_resource[PEDIA_TERRAIN_COUNT] = {
  0, 1, 2, 3, 4, 5, 6, 6, 9, 1, 8, 9, 10, 10, 6, 6, 9, 1, 8, 9, 10, 10, 6, 6, -1, 7, -1, 12, 13
};

/*
 * (resource, field job 0..8) -> yield effect (FUN_15eb_17fa, mirrors
 * colony_yield.c's byte-exact copy). -1 = "double" sentinel.
 */
#define PEDIA_RES_DOUBLE (-1)
static int pedia_resource_effect(int resource, int job) {
  int v = 0;
  if (resource == 9 && job == 0) v = 2;
  if (resource == 1 && job == 0) v += 2;
  if (resource == 2 && job == 0) v += 2;
  if (resource == 9 && job == 4) v += 2;
  if (resource == 8 && job == 4) v += 2;
  if (resource == 3 && job == 3) v = PEDIA_RES_DOUBLE;
  if (resource == 4 && job == 2) v = PEDIA_RES_DOUBLE;
  if (resource == 5 && job == 1) v = PEDIA_RES_DOUBLE;
  if (resource == 10 && job == 5) v += 2;
  if (resource == 6 && job == 6) v += 3;
  if (resource == 13 && job == 6) v += 2;
  if (resource == 6 && job == 7) v += 1;
  if (resource == 12 && job == 7) v += 2;
  if (resource == 7 && job == 8) v += 3;
  return v;
}

/* LABELS.TXT @MISC line, with literal fallback (DOS DS:0x2dba pointer table). */
static const char* pedia_label(const ColonizeMsgCatalog* labels, int idx, const char* fallback) {
  if (labels) {
    const ColonizeMsgSection* sec = assets_msg_find(labels, "MISC");
    if (sec && idx >= 0 && idx < sec->line_count && sec->lines[idx][0]) {
      return sec->lines[idx];
    }
  }
  return fallback;
}

/* @PEDIA category subtitle ("Cargo Type" .. "Game Concept"). */
static void pedia_category_subtitle(
  const ColonizeMsgCatalog* pedia, PediaCategory cat, char* out, size_t out_size
) {
  static const char* k_fallback[PEDIA_CAT_COUNT] = {
    "Cargo Type", "Unit Type", "Terrain Type", "Colonist Skill",
    "Colony Building", "Founding Father", "Game Concept"
  };
  const int line = (int)cat;
  out[0] = '\0';
  if (pedia) {
    const ColonizeMsgSection* sec = assets_msg_find(pedia, "PEDIA");
    if (sec && line >= 0 && line < sec->line_count) {
      snprintf(out, out_size, "%s", sec->lines[line]);
      return;
    }
  }
  if (line >= 0 && line < PEDIA_CAT_COUNT) {
    snprintf(out, out_size, "%s", k_fallback[line]);
  }
}

/* First comma-separated field of a NAMES.TXT section line (trimmed). */
static void pedia_names_field(
  const ColonizeMsgCatalog* names,
  const char* section,
  int line_idx,
  int field,
  char* out,
  size_t out_size
) {
  out[0] = '\0';
  if (!names) {
    return;
  }
  const ColonizeMsgSection* sec = assets_msg_find(names, section);
  if (!sec || line_idx < 0 || line_idx >= sec->line_count) {
    return;
  }
  const char* p = sec->lines[line_idx];
  for (int f = 0; f < field && p; ++f) {
    p = strchr(p, ',');
    if (p) {
      p++;
    }
  }
  if (!p) {
    return;
  }
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  const char* end = strchr(p, ',');
  size_t n = end ? (size_t)(end - p) : strlen(p);
  while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t')) {
    n--;
  }
  if (n >= out_size) {
    n = out_size - 1;
  }
  memcpy(out, p, n);
  out[n] = '\0';
}

static int pedia_names_int(
  const ColonizeMsgCatalog* names, const char* section, int line_idx, int field
) {
  char buf[32];
  pedia_names_field(names, section, line_idx, field, buf, sizeof(buf));
  int v = 0;
  sscanf(buf, "%d", &v);
  return v;
}

/* @CARGO name for id 0..19 (16 Hammers, 17 Crosses, 18 Liberty Bells, 19 Flags). */
static void pedia_cargo_display_name(
  const ColonizeMsgCatalog* names, int cargo, char* out, size_t out_size
) {
  pedia_names_field(names, "CARGO", cargo, 0, out, out_size);
  if (!out[0]) {
    snprintf(out, out_size, "Cargo %d", cargo);
  }
}

static void pedia_job_display_name(
  const ColonizeMsgCatalog* names, int job, char* out, size_t out_size
) {
  pedia_names_field(names, "JOB", job, 0, out, out_size);
  if (!out[0]) {
    snprintf(out, out_size, "Skill %d", job);
  }
}

static void pedia_job_expert_name(
  const ColonizeMsgCatalog* names, int job, char* out, size_t out_size
) {
  pedia_names_field(names, "JOB", job, 1, out, out_size);
}

/* NAMES terrain display name; forest classes get " Forest" appended (DOS 033a). */
static void pedia_terrain_display_name(
  const ColonizeMsgCatalog* names,
  const ColonizeMsgCatalog* labels,
  int idx,
  char* out,
  size_t out_size
) {
  out[0] = '\0';
  if (idx >= 0 && idx <= 7) {
    pedia_names_field(names, "UNFORESTED", idx, 0, out, out_size);
  } else if (idx >= 8 && idx <= 23) {
    pedia_names_field(names, "FORESTED", idx & 7, 0, out, out_size);
    const char* forest = "Forest";
    if (names) {
      const ColonizeMsgSection* other = assets_msg_find(names, "OTHER_NAMES");
      if (other && other->line_count > 0) {
        forest = other->lines[0];
      }
    }
    (void)labels;
    size_t len = strlen(out);
    snprintf(out + len, out_size > len ? out_size - len : 0, " %s", forest);
  } else if (idx >= 24 && idx <= 28) {
    pedia_names_field(names, "OTHER", idx - 24, 0, out, out_size);
  }
  if (!out[0]) {
    snprintf(out, out_size, "Terrain %d", idx);
  }
}

static void pedia_draw_centered(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y,
  const char* text,
  uint8_t color
) {
  const int w = pedia_text_width(font, text);
  int x = (fb->width - w) / 2;
  if (x < 0) {
    x = 0;
  }
  font_draw_text(font, fb, x, y, text, color);
}

static void pedia_fill_rect_outline(
  ColonizeFramebuffer8* fb, int x0, int y0, int x1, int y1, uint8_t color
) {
  for (int x = x0; x <= x1; ++x) {
    if (x >= 0 && x < fb->width) {
      if (y0 >= 0 && y0 < fb->height) {
        fb->pixels[y0 * fb->width + x] = color;
      }
      if (y1 >= 0 && y1 < fb->height) {
        fb->pixels[y1 * fb->width + x] = color;
      }
    }
  }
  for (int y = y0; y <= y1; ++y) {
    if (y >= 0 && y < fb->height) {
      if (x0 >= 0 && x0 < fb->width) {
        fb->pixels[y * fb->width + x0] = color;
      }
      if (x1 >= 0 && x1 < fb->width) {
        fb->pixels[y * fb->width + x1] = color;
      }
    }
  }
}

static void pedia_blit(
  const ColonizeSpriteSheet* sheet, int sprite, ColonizeFramebuffer8* fb, int x, int y
) {
  if (sheet && sprite >= 0 && sprite < sheet->sprite_count) {
    ss_blit_sprite(sheet, sprite, fb, x, y);
  }
}

/* ---------------- PEDIA.TXT body renderer ---------------- */

/*
 * DOS renders the article body through the popup text engine at DS:0x1f5a
 * with no box (flag 0x20): section text wrapped to @width (default 300),
 * prose lines flow left-aligned, a '^' source line is drawn centered on its
 * own (blank if empty), '{...}' = hilite color, "%%" = '%'.
 */
typedef struct PediaBodySeg {
  int start; /* into word buffer */
  int width;
  uint8_t color;
} PediaBodySeg;

typedef struct PediaBodyCtx {
  const ColonizeFont* font;
  ColonizeFramebuffer8* fb;
  int x0;
  int width;
  int y;
  int line_h;
  int space_w;
  bool hilite;
  char word_buf[1024];
  PediaBodySeg segs[64];
  int seg_count;
  int buf_used;
  int line_w;
} PediaBodyCtx;

static void pedia_body_draw_segs(
  PediaBodyCtx* c, const PediaBodySeg* segs, int n, int line_w, bool center
) {
  int x = center ? c->x0 + (c->width - line_w) / 2 : c->x0;
  for (int i = 0; i < n; ++i) {
    font_draw_text(c->font, c->fb, x, c->y, c->word_buf + segs[i].start, segs[i].color);
    x += segs[i].width;
  }
}

/* Flush the flowing paragraph line (left-aligned like the DOS dialog engine). */
static void pedia_body_flush_flow(PediaBodyCtx* c) {
  if (c->seg_count > 0) {
    pedia_body_draw_segs(c, c->segs, c->seg_count, c->line_w, false);
    c->y += c->line_h;
  }
  c->seg_count = 0;
  c->buf_used = 0;
  c->line_w = 0;
}

/* Next word from *pp (handles {} color switches, %% escape); empty at end. */
static bool pedia_body_next_word(
  PediaBodyCtx* c, const char** pp, char* word, size_t word_size, bool* out_hilite
) {
  const char* p = *pp;
  int wn = 0;
  bool word_hilite = c->hilite;
retry:
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  wn = 0;
  word_hilite = c->hilite;
  bool started = false;
  while (*p && (*p != ' ' && *p != '\t')) {
    if (*p == '{') {
      c->hilite = true;
      if (!started) {
        word_hilite = true;
      }
      p++;
      continue;
    }
    if (*p == '}') {
      c->hilite = false;
      p++;
      continue;
    }
    if (p[0] == '%' && p[1] == '%') {
      if (wn < (int)word_size - 1) {
        word[wn++] = '%';
      }
      started = true;
      p += 2;
      continue;
    }
    if ((unsigned char)*p >= 0x80) {
      /* CP437 bullets etc. — no glyphs in the 128-char FF fonts; drop. */
      p++;
      continue;
    }
    if (wn < (int)word_size - 1) {
      word[wn++] = *p;
    }
    started = true;
    p++;
  }
  if (wn == 0 && *p) {
    /* Token dissolved entirely (bullet byte, bare braces) — keep scanning. */
    goto retry;
  }
  word[wn] = '\0';
  *pp = p;
  *out_hilite = word_hilite;
  return wn > 0;
}

/* Feed one prose line into the flowing, word-wrapped paragraph. */
static void pedia_body_flow_text(PediaBodyCtx* c, const char* text) {
  const char* p = text;
  char word[128];
  bool word_hilite;
  while (pedia_body_next_word(c, &p, word, sizeof(word), &word_hilite)) {
    const int ww = pedia_text_width(c->font, word);
    if (c->seg_count > 0 && c->line_w + c->space_w + ww > c->width) {
      pedia_body_flush_flow(c);
    }
    if (c->y > c->fb->height - c->line_h) {
      return;
    }
    const int wn = (int)strlen(word);
    if (c->seg_count < (int)(sizeof(c->segs) / sizeof(c->segs[0])) &&
        c->buf_used + wn + 2 < (int)sizeof(c->word_buf)) {
      char* dst = c->word_buf + c->buf_used;
      int extra = 0;
      if (c->seg_count > 0) {
        dst[0] = ' ';
        memcpy(dst + 1, word, (size_t)wn + 1);
        extra = 1;
      } else {
        memcpy(dst, word, (size_t)wn + 1);
      }
      c->segs[c->seg_count].start = c->buf_used;
      c->segs[c->seg_count].width = ww + (extra ? c->space_w : 0);
      c->segs[c->seg_count].color =
        word_hilite ? (uint8_t)PEDIA_COL_LINK_HOVER : (uint8_t)PEDIA_COL_LINK;
      c->line_w += c->segs[c->seg_count].width;
      c->buf_used += wn + extra + 1;
      c->seg_count++;
    }
  }
}

/*
 * A '^' source line: flush the paragraph, draw this line on its own.
 * `{...}` headings are centered; plain '^' lines (e.g. the schoolhouse
 * teachable-skill lists, whose CP437 bullet bytes are dropped above) stay
 * left-aligned with a small indent like DOS.
 */
static void pedia_body_own_line(PediaBodyCtx* c, const char* text, bool centered) {
  pedia_body_flush_flow(c);
  if (c->y > c->fb->height - c->line_h) {
    return;
  }
  PediaBodySeg segs[32];
  char buf[512];
  int n = 0;
  int used = 0;
  int line_w = 0;
  const char* p = text;
  char word[128];
  bool word_hilite;
  while (pedia_body_next_word(c, &p, word, sizeof(word), &word_hilite)) {
    const int wn = (int)strlen(word);
    if (n >= (int)(sizeof(segs) / sizeof(segs[0])) || used + wn + 2 >= (int)sizeof(buf)) {
      break;
    }
    char* dst = buf + used;
    int extra = 0;
    if (n > 0) {
      dst[0] = ' ';
      memcpy(dst + 1, word, (size_t)wn + 1);
      extra = 1;
    } else {
      memcpy(dst, word, (size_t)wn + 1);
    }
    segs[n].start = used;
    segs[n].width = pedia_text_width(c->font, word) + (extra ? c->space_w : 0);
    segs[n].color = word_hilite ? (uint8_t)PEDIA_COL_LINK_HOVER : (uint8_t)PEDIA_COL_LINK;
    line_w += segs[n].width;
    used += wn + extra + 1;
    n++;
  }
  if (n == 0) {
    c->y += c->line_h; /* bare ^ = blank separator line */
    return;
  }
  int x = centered ? c->x0 + (c->width - line_w) / 2 : c->x0 + 6;
  for (int i = 0; i < n; ++i) {
    font_draw_text(c->font, c->fb, x, c->y, buf + segs[i].start, segs[i].color);
    x += segs[i].width;
  }
  c->y += c->line_h;
}

static void pedia_body_ctx_init(
  PediaBodyCtx* c, const ColonizeFont* font, ColonizeFramebuffer8* fb, int y, int width
) {
  memset(c, 0, sizeof(*c));
  c->font = font;
  c->fb = fb;
  c->width = width;
  c->x0 = (fb->width - width) / 2;
  c->y = y;
  c->line_h = (font ? font->max_height : 6) + 1;
  c->space_w = pedia_text_width(font, " ");
}

static void pedia_body_render_text(
  const char* text, const ColonizeFont* font, ColonizeFramebuffer8* fb, int y, int width
) {
  PediaBodyCtx c;
  pedia_body_ctx_init(&c, font, fb, y, width);
  pedia_body_flow_text(&c, text);
  pedia_body_flush_flow(&c);
}

static void pedia_body_render_section(
  const ColonizeMsgCatalog* pedia,
  const char* tag,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int y
) {
  const ColonizeMsgSection* sec = pedia ? assets_msg_find(pedia, tag) : NULL;
  if (!sec) {
    return;
  }
  int width = 300;
  for (int i = 0; i < sec->line_count; ++i) {
    const char* line = sec->lines[i];
    int w = 0;
    if (sscanf(line, "@width=%d", &w) == 1 && w > 0 && w <= 320) {
      width = w;
    }
  }
  PediaBodyCtx c;
  pedia_body_ctx_init(&c, font, fb, y, width);
  for (int i = 0; i < sec->line_count; ++i) {
    const char* line = sec->lines[i];
    if (line[0] == '@') {
      continue;
    }
    if (line[0] == '^') {
      const char* p = line;
      while (*p == '^') {
        p++;
      }
      const char* q = p;
      while (*q == ' ' || *q == '\t') {
        q++;
      }
      pedia_body_own_line(&c, p, *q == '{');
      continue;
    }
    pedia_body_flow_text(&c, line);
  }
  pedia_body_flush_flow(&c);
}

/* ---------------- per-category article content ---------------- */

/* One cargo icon row (FUN_6cb2_048c): job icon, stacked cargo icons, caption. */
static void pedia_cargo_row(
  const PediaArticleAssets* a,
  ColonizeFramebuffer8* fb,
  int cargo,
  int job,
  int y
) {
  int x = 10;
  int sprite = 22 + cargo;
  if (cargo == 16) {
    sprite = 54; /* Hammers */
  }
  char name[64];
  if (cargo < 0) {
    sprite = 57; /* Fish */
    job = 8;
    snprintf(name, sizeof(name), "%s", pedia_label(a->labels, 177, "Fish"));
  } else {
    pedia_cargo_display_name(a->names, cargo, name, sizeof(name));
  }
  if (job >= 0) {
    pedia_blit(a->icons, 81 + job, fb, x, y - 2);
    x += 14;
  }
  pedia_blit(a->icons, sprite, fb, x, y);
  x += 16;
  for (int i = 0; i < 6; ++i) {
    pedia_blit(a->icons, sprite, fb, x, y);
    x += 4;
  }
  x += 12;
  char text[160];
  if (job >= 0) {
    char expert[64];
    pedia_job_expert_name(a->names, job, expert, sizeof(expert));
    snprintf(
      text, sizeof(text), "%s (%s %s)", name, pedia_label(a->labels, 178, "With"), expert
    );
  } else {
    snprintf(text, sizeof(text), "%s", name);
  }
  font_draw_text(a->font, fb, x, y + 4, text, PEDIA_COL_LINK);
}

/* FUN_6cb2_05ce cargo article content; returns body y. */
static int pedia_article_cargo(
  const PediaArticleAssets* a, ColonizeFramebuffer8* fb, int c, int y
) {
  int rows_cargo[3];
  int rows_job[3];
  int n = 0;
  if (c == 0) {
    rows_cargo[0] = 0;
    rows_job[0] = 0;
    rows_cargo[1] = -1;
    rows_job[1] = -1;
    n = 2;
  } else if (c == 8 || c == 13) {
    rows_cargo[0] = c;
    rows_job[0] = -1;
    n = 1;
  } else if (c == 7) {
    rows_cargo[0] = 7;
    rows_job[0] = 7;
    n = 1;
  } else if (c == 6 || c == 14 || c == 15) {
    rows_cargo[0] = 6;
    rows_job[0] = 6;
    rows_cargo[1] = 14;
    rows_job[1] = 14;
    rows_cargo[2] = 15;
    rows_job[2] = 15;
    n = 3;
  } else if (c == 5) {
    rows_cargo[0] = 5;
    rows_job[0] = 5;
    rows_cargo[1] = 16;
    rows_job[1] = 13;
    n = 2;
  } else if (c < 8) {
    rows_cargo[0] = c;
    rows_job[0] = c;
    rows_cargo[1] = c + 8;
    rows_job[1] = c + 8;
    n = 2;
  } else {
    rows_cargo[0] = c - 8;
    rows_job[0] = c - 8;
    rows_cargo[1] = c;
    rows_job[1] = c;
    n = 2;
  }
  for (int i = 0; i < n; ++i) {
    pedia_cargo_row(a, fb, rows_cargo[i], rows_job[i], y);
    y += 20;
  }
  return y + 10;
}

/* Draw one figure with unit chrome (DOS FUN_281f_02bc at 100%). */
static void pedia_unit_figure(
  const PediaArticleAssets* a,
  ColonizeFramebuffer8* fb,
  int sprite,
  int type_index,
  int x,
  int y
) {
  if (!a->icons || sprite < 0) {
    return;
  }
  unit_chrome_blit_unit_for_palette(
    fb,
    a->chrome_font,
    a->icons,
    sprite,
    x,
    y,
    type_index,
    a->human_nation,
    UNITS_ORDER_NONE,
    false,
    false,
    a->palette
  );
}

/* FUN_6cb2_07e6 unit article content; returns body y. */
static int pedia_article_unit(
  const PediaArticleAssets* a, ColonizeFramebuffer8* fb, int t, int y
) {
  const int fh = a->font ? a->font->max_height : 6;
  (void)fh;
  int x = 8;

  const int icon_1based = pedia_names_int(a->names, "UNIT", t, 1);
  const int movement = pedia_names_int(a->names, "UNIT", t, 2);
  const int attack = pedia_names_int(a->names, "UNIT", t, 3);
  const int combat = pedia_names_int(a->names, "UNIT", t, 4);
  const int cargo = pedia_names_int(a->names, "UNIT", t, 5);
  const int plain_icon = icon_1based > 0 ? icon_1based - 1 : -1;

  char name[64];
  pedia_names_field(a->names, "UNIT", t, 0, name, sizeof(name));

  /* Default expert profession per type (DOS FUN_281f_0b78 / DS:0x30e). */
  static const signed char k_default_job[6] = {19, 21, 20, 24, 23, 22};
  /* Plain figure icons for the profession-carrying types (map poses). */
  static const int k_plain_pose[6] = {100, 74, 73, 105, 76, 75};
  static const int k_expert_pose[6] = {100, 102, 101, 77, 104, 103};

  int expert_job = -1;
  if (t == 0) {
    /* Colonists: every profession figure, 17 per row (DOS order). */
    pedia_unit_figure(a, fb, units_job_icon_sprite(28), 0, x, y);
    x = 26;
    int count = 1;
    static const signed char k_grid_profs[] = {
      25, 26, 27, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20, 21, 22
    };
    for (size_t i = 0; i < sizeof(k_grid_profs); ++i) {
      const int prof = k_grid_profs[i];
      int sprite = units_job_icon_sprite(prof);
      if (sprite < 0) {
        sprite = 100;
      }
      pedia_unit_figure(a, fb, sprite, 0, x, y);
      x += 18;
      count++;
      if (i >= 3 && count >= 17) {
        count = 0;
        x = 8;
        y += 20;
      }
    }
  } else if (t >= 1 && t <= 5) {
    expert_job = k_default_job[t];
    pedia_unit_figure(a, fb, k_plain_pose[t], t, 8, y);
    pedia_unit_figure(a, fb, k_expert_pose[t], t, 26, y);
    x = 44;
  } else {
    pedia_unit_figure(a, fb, plain_icon, t, 8, y);
    x = 26;
    if (t == 11) {
      /* Artillery: damaged variant beside it. */
      pedia_unit_figure(a, fb, plain_icon >= 0 ? plain_icon + 1 : -1, t, 26, y);
      x = 44;
    }
  }

  /* Caption line beside the figures. */
  char line[192];
  if (expert_job >= 0) {
    char expert[64];
    pedia_job_expert_name(a->names, expert_job, expert, sizeof(expert));
    snprintf(
      line, sizeof(line), "%s (%s %s)", name, pedia_label(a->labels, 106, "and"), expert
    );
  } else if (t == 11) {
    snprintf(
      line,
      sizeof(line),
      "%s (%s %s %s)",
      name,
      pedia_label(a->labels, 106, "and"),
      pedia_label(a->labels, 201, "Damaged"),
      name
    );
  } else {
    snprintf(line, sizeof(line), "%s", name);
  }
  font_draw_text(a->font, fb, x, y + 6, line, PEDIA_COL_LINK);
  y += 24;

  /* Stats line. */
  char stats[256];
  size_t used = (size_t)snprintf(
    stats, sizeof(stats), "%s: %d", pedia_label(a->labels, 179, "Combat"), combat
  );
  if (t == 11) {
    used += (size_t)snprintf(
      stats + used,
      sizeof(stats) - used,
      "   (%s: +%d %s: -2)",
      pedia_label(a->labels, 180, "Attack"),
      attack - combat,
      pedia_label(a->labels, 201, "Damaged")
    );
  } else if (t == 1 || t == 4) {
    used += (size_t)snprintf(
      stats + used,
      sizeof(stats) - used,
      "   (%s: %d)",
      pedia_label(a->labels, 65, "Veteran"),
      attack * 3 / 2
    );
  }
  used += (size_t)snprintf(
    stats + used, sizeof(stats) - used, "   %s: %d", pedia_label(a->labels, 182, "Moves"), movement
  );
  if (cargo != 0) {
    used += (size_t)snprintf(
      stats + used,
      sizeof(stats) - used,
      "   (%s: %d)",
      pedia_label(a->labels, 181, "Cargo Holds"),
      cargo
    );
  }
  font_draw_text(a->font, fb, 8, y, stats, PEDIA_COL_LINK);
  return y + 12;
}

/* FUN_6cb2_0eac terrain article content; returns body y. */
static int pedia_article_terrain(
  const PediaArticleAssets* a, ColonizeFramebuffer8* fb, int idx, int y
) {
  const int top = y;
  const bool is_mtn_hills = (idx == 27 || idx == 28);
  const bool is_ocean = (idx == 25 || idx == 26);
  const bool is_arctic = (idx == 24);
  const bool is_forest = (idx >= 8 && idx <= 23);
  const bool is_scrub_forest = is_forest && (idx & 7) == 1;

  /* Base tile per cell (port TERRAIN.SS indices; DOS uses its own sheet ids). */
  int base = idx;
  if (is_mtn_hills) {
    base = 3;
  } else if (is_forest) {
    base = is_scrub_forest ? 8 : (idx & 7);
  } else if (idx == 24) {
    base = 9;
  } else if (idx == 25) {
    base = 10;
  } else if (idx == 26) {
    base = 11;
  }

  const int res = (idx >= 0 && idx < PEDIA_TERRAIN_COUNT) ? k_pedia_terrain_resource[idx] : -1;

  /* Double frame around the 3x3 preview (DOS colors border2 / border0). */
  pedia_fill_rect_outline(fb, 7, top, 7 + 51, top + 51, COLONIZE_COL_BORDER2);
  pedia_fill_rect_outline(fb, 8, top + 1, 7 + 50, top + 50, COLONIZE_COL_BORDER0);

  /* 3x3 block overlay pieces (PHYS0, DOS table {5,7,6,d,f,e,9,b,a}+base). */
  static const int k_block[3][3] = {{5, 7, 6}, {13, 15, 14}, {9, 11, 10}};

  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      const int cx = 9 + c * 16;
      const int cy = top + 2 + r * 16;
      pedia_blit(a->terrain, base, fb, cx, cy);
      if (is_forest && !is_scrub_forest) {
        pedia_blit(a->phys0, k_block[r][c] + 0x40, fb, cx, cy);
      }
      if (idx == 28) {
        pedia_blit(a->phys0, k_block[r][c] + 0x30, fb, cx, cy);
      }
      if (idx == 27) {
        pedia_blit(a->phys0, k_block[r][c] + 0x20, fb, cx, cy);
      }
      const bool plain = !is_mtn_hills && !is_ocean && !is_forest && !is_arctic;
      if (plain && r != 1 && c != 1) {
        if (idx == 1) {
          pedia_blit(a->terrain, 8, fb, cx, cy); /* desert scrub variant */
        } else {
          pedia_blit(a->phys0, 64, fb, cx, cy); /* vegetation tuft */
        }
      }
      if (plain && c == 1 && r == 2) {
        pedia_blit(a->phys0, 149, fb, cx, cy); /* river mouth */
      }
      if (!is_mtn_hills && !is_ocean && !is_arctic && c == 0) {
        if (r == 1) {
          pedia_blit(a->phys0, 22, fb, cx, cy);
        } else if (r == 2) {
          pedia_blit(a->phys0, 26, fb, cx, cy);
        }
      }
      if (!is_ocean && c == 2) {
        if (r == 0) {
          pedia_blit(a->phys0, 82, fb, cx, cy);
          pedia_blit(a->phys0, 85, fb, cx, cy);
        } else if (r == 1) {
          pedia_blit(a->phys0, 81, fb, cx, cy);
          pedia_blit(a->phys0, 84, fb, cx, cy);
        }
      }
      if (c == 1 && r == 1 && res >= 0) {
        pedia_blit(a->phys0, 89 + res, fb, cx, cy);
      }
    }
  }

  /* Yield rows to the right of the preview (x icon 63, text 75). */
  const char* terrain_sec = (idx <= 7) ? "UNFORESTED" : (idx <= 23) ? "FORESTED" : "OTHER";
  const int terrain_line = (idx <= 7) ? idx : (idx <= 23) ? (idx & 7) : (idx - 24);
  const int move_cost = pedia_names_int(a->names, terrain_sec, terrain_line, 1);
  const int defense = pedia_names_int(a->names, terrain_sec, terrain_line, 2);

  y = top;
  for (int j = 0; j < 9; ++j) {
    const int yield = pedia_names_int(a->names, terrain_sec, terrain_line, 5 + j);
    if (yield == 0) {
      continue;
    }
    pedia_blit(a->icons, 81 + j, fb, 63, y);
    int x = 75;
    char buf[96];
    char jobname[48];
    pedia_job_display_name(a->names, j, jobname, sizeof(jobname));
    int shown = yield;
    if (j == 0 || j == 8) {
      shown += 1;
    }
    if (j == 5) {
      shown *= 2;
    }
    snprintf(buf, sizeof(buf), "%s: %d", jobname, shown);
    font_draw_text(a->font, fb, x, y + 6, buf, PEDIA_COL_LINK_HOVER);
    x += pedia_text_width(a->font, buf);

    const char* first = (j <= 3) ? pedia_label(a->labels, 183, "Plow")
      : (j < 8) ? pedia_label(a->labels, 31, "Road")
                : pedia_label(a->labels, 185, "Coast");
    snprintf(
      buf,
      sizeof(buf),
      "    %s/%s: +%d",
      first,
      pedia_label(a->labels, 184, "River"),
      1 + (j == 4) + (j == 5)
    );
    font_draw_text(a->font, fb, x, y + 6, buf, PEDIA_COL_LINK);
    x += pedia_text_width(a->font, buf);

    const int eff = (res >= 0) ? pedia_resource_effect(res, j) : 0;
    if (eff != 0) {
      x += pedia_text_width(a->font, " ");
      pedia_blit(a->phys0, 89 + res, fb, x, y);
      x += 18;
      char resname[48];
      if (res == 4) {
        snprintf(resname, sizeof(resname), "%s", pedia_label(a->labels, 200, "Prime"));
      } else {
        pedia_names_field(a->names, "RESOURCE", res, 0, resname, sizeof(resname));
      }
      if (eff < 0) {
        snprintf(buf, sizeof(buf), "%s: x2", resname);
      } else {
        int shown_eff = (j == 5) ? eff * 2 : eff;
        if (j == 0 || j == 8) {
          snprintf(buf, sizeof(buf), "%s: +%d/%d", resname, shown_eff, eff * 2);
        } else {
          snprintf(buf, sizeof(buf), "%s: +%d", resname, shown_eff);
        }
      }
      font_draw_text(a->font, fb, x, y + 6, buf, PEDIA_COL_LINK_HOVER);
      x += pedia_text_width(a->font, buf);
    }

    if (j == 0 || j == 8) {
      snprintf(buf, sizeof(buf), "    %s: +3", pedia_label(a->labels, 4, "Expert"));
    } else {
      snprintf(buf, sizeof(buf), "    %s: x2", pedia_label(a->labels, 4, "Expert"));
    }
    font_draw_text(a->font, fb, x, y + 6, buf, PEDIA_COL_LINK);
    y += 16;
  }

  {
    char buf[128];
    snprintf(
      buf,
      sizeof(buf),
      "%s: %d    %s: +%d%%",
      pedia_label(a->labels, 186, "Move Cost"),
      move_cost,
      pedia_label(a->labels, 187, "Defense or Ambush Bonus"),
      defense * 25
    );
    font_draw_text(a->font, fb, 63, y + 6, buf, PEDIA_COL_LINK);
    y += 0x17;
  }

  const int min_body = top + 0x40;
  return y > min_body ? y : min_body;
}

/* FUN_6cb2_1820 skill article content; returns body y. */
static int pedia_article_job(
  const PediaArticleAssets* a, ColonizeFramebuffer8* fb, int job, int y
) {
  int chain = (job >= 0 && job < 19) ? k_pedia_job_chain_start[job] : -1;
  const bool had_chain = chain >= 0;
  if (chain < 0) {
    y += 11;
  }
  const int top = y;

  int h1 = 0;
  if (chain >= 0 && a->buildings && chain < a->buildings->sprite_count) {
    h1 = a->buildings->sprites[chain].height;
  }
  int dy1 = (chain >= 0) ? h1 / 2 - 7 : 0;
  if (dy1 < 0) {
    dy1 = 0;
  }
  const int icon_y = top + dy1;

  int job_sprite = 81 + job;
  if (job == 27) {
    job_sprite = 66; /* Indian Convert */
  }
  pedia_blit(a->icons, job_sprite, fb, 10, icon_y);
  char expert[64];
  pedia_job_expert_name(a->names, job, expert, sizeof(expert));
  font_draw_text(a->font, fb, 24, icon_y + 6, expert, PEDIA_COL_LINK_HOVER);
  int bx = 24 + pedia_text_width(a->font, expert) + 24;
  int prod_x = bx;

  bool first = true;
  while (chain >= 0) {
    int w = 0;
    int h = 24;
    if (a->buildings && chain < a->buildings->sprite_count) {
      pedia_blit(a->buildings, chain, fb, bx, y);
      w = a->buildings->sprites[chain].width;
      h = a->buildings->sprites[chain].height;
    }
    char bname[64];
    pedia_names_field(a->names, "BUILDING", chain, 0, bname, sizeof(bname));
    int ty = y + h / 2 - 7;
    if (ty < y) {
      ty = y;
    }
    font_draw_text(a->font, fb, bx + w + 3, ty + 6, bname, PEDIA_COL_LINK_HOVER);
    if (first) {
      prod_x = bx + w + 3 + pedia_text_width(a->font, bname) + 24;
      first = false;
    }
    y += h + 4;
    chain = k_pedia_building_next[chain];
  }

  if (job < 19) {
    int cargo_sprite = 22 + job;
    int cargo_name_id = job;
    if (job == 8) {
      cargo_sprite = 57;
    }
    if (job == 13) {
      cargo_sprite = 54;
      cargo_name_id = 16;
    }
    if (job == 16) {
      cargo_sprite = 56;
      cargo_name_id = 17;
    }
    if (job == 17) {
      cargo_sprite = 62;
      cargo_name_id = 18;
    }
    pedia_blit(a->icons, cargo_sprite, fb, prod_x, icon_y + 2);
    char cname[64];
    if (job == 8) {
      snprintf(cname, sizeof(cname), "%s", pedia_label(a->labels, 177, "Fish"));
    } else {
      pedia_cargo_display_name(a->names, cargo_name_id, cname, sizeof(cname));
    }
    font_draw_text(a->font, fb, prod_x + 16, icon_y + 6, cname, PEDIA_COL_LINK_HOVER);
  }

  return had_chain ? y + 4 : top + 20;
}

/* FUN_6cb2_1ba8 building article content; returns body y. */
static int pedia_article_building(
  const PediaArticleAssets* a, ColonizeFramebuffer8* fb, int b, int y
) {
  const int top = y;
  int w = 0;
  int h = 24;
  if (b != 16 && b != 31) {
    int disp = (b == 17) ? 46 : b; /* Stable uses dedicated art */
    if (a->buildings && disp < a->buildings->sprite_count) {
      pedia_blit(a->buildings, disp, fb, 10, top);
      w = a->buildings->sprites[disp].width;
      h = a->buildings->sprites[disp].height;
    }
  } else {
    w = -3; /* name at x=10 like DOS (w+3 == 0) */
  }
  int dy = h / 2 - 7;
  if (dy < 0) {
    dy = 0;
  }
  const int icon_y = top + dy;

  char bname[64];
  pedia_names_field(a->names, "BUILDING", b, 0, bname, sizeof(bname));
  font_draw_text(a->font, fb, 10 + w + 3, icon_y + 6, bname, PEDIA_COL_LINK_HOVER);
  int x = 10 + w + 3 + pedia_text_width(a->font, bname) + 24;

  int job = (b >= 0 && b < PEDIA_BUILDING_COUNT) ? k_pedia_building_job[b] : -1;
  if (job == 18 || job == 21) {
    job = -1; /* Teacher / Soldier rows are not shown (DOS 1ba8) */
  }
  if (job >= 0) {
    pedia_blit(a->icons, 81 + job, fb, x, icon_y);
    char expert[64];
    pedia_job_expert_name(a->names, job, expert, sizeof(expert));
    font_draw_text(a->font, fb, x + 14, icon_y + 6, expert, PEDIA_COL_LINK_HOVER);
    x += 14 + pedia_text_width(a->font, expert) + 24;

    int cargo_sprite = 22 + job;
    int cargo_name_id = job;
    if (job == 13) {
      cargo_sprite = 54;
      cargo_name_id = 16;
    }
    if (job == 16) {
      cargo_sprite = 56;
      cargo_name_id = 17;
    }
    if (job == 17) {
      cargo_sprite = 62;
      cargo_name_id = 18;
    }
    pedia_blit(a->icons, cargo_sprite, fb, x, icon_y + 2);
    char cname[64];
    pedia_cargo_display_name(a->names, cargo_name_id, cname, sizeof(cname));
    font_draw_text(a->font, fb, x + 14, icon_y + 6, cname, PEDIA_COL_LINK_HOVER);
  }

  y = top + h + 12;
  const int prereq = (b >= 0 && b < PEDIA_BUILDING_COUNT) ? k_pedia_building_prereq[b] : -1;
  if (prereq >= 0) {
    char pname[64];
    char buf[128];
    pedia_names_field(a->names, "BUILDING", prereq, 0, pname, sizeof(pname));
    snprintf(
      buf, sizeof(buf), "%s: %s", pedia_label(a->labels, 188, "Prerequisite"), pname
    );
    font_draw_text(a->font, fb, 10, y, buf, PEDIA_COL_LINK);
    y += 20;
  }
  return y;
}

void pedia_article_render(
  const PediaArticleAssets* a,
  PediaCategory category,
  int index,
  ColonizeFramebuffer8* fb
) {
  if (!a || !fb || !fb->pixels) {
    return;
  }
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (a->wood_bg && a->wood_bg->pixels) {
    pik_blit(a->wood_bg, fb, 0, 0);
  }

  const int count = pedia_category_count(category);
  if (count <= 0) {
    return;
  }
  if (index < 0) {
    index = 0;
  }
  if (index >= count) {
    index = count - 1;
  }

  const int fh = a->font ? a->font->max_height : 6;

  /* Centered header + "(Name: Category)" title, both hilite (DOS 6cb2). */
  pedia_draw_centered(
    a->font, fb, 5,
    pedia_label(a->labels, 108, PEDIA_LIST_HEADER),
    PEDIA_COL_LINK_HOVER
  );

  char name[64];
  name[0] = '\0';
  switch (category) {
    case PEDIA_CAT_CARGO:
      pedia_cargo_display_name(a->names, index, name, sizeof(name));
      break;
    case PEDIA_CAT_UNIT:
      pedia_names_field(a->names, "UNIT", index, 0, name, sizeof(name));
      break;
    case PEDIA_CAT_TERRAIN:
      pedia_terrain_display_name(a->names, a->labels, index, name, sizeof(name));
      break;
    case PEDIA_CAT_JOB:
      pedia_job_display_name(a->names, index, name, sizeof(name));
      break;
    case PEDIA_CAT_BUILDING:
      pedia_names_field(a->names, "BUILDING", index, 0, name, sizeof(name));
      break;
    case PEDIA_CAT_FATHER:
      pedia_names_field(a->names, "FATHERS", index, 0, name, sizeof(name));
      break;
    case PEDIA_CAT_MISC:
    default: {
      const ColonizeMsgSection* misc =
        a->pedia ? assets_msg_find(a->pedia, "MISCELLANEOUS") : NULL;
      /* @MISCELLANEOUS line 0 is the count; titles follow. */
      if (misc && index + 1 < misc->line_count) {
        snprintf(name, sizeof(name), "%s", misc->lines[index + 1]);
      } else if (index >= 0 && index < PEDIA_MISC_COUNT) {
        snprintf(name, sizeof(name), "%s", k_misc_titles[index]);
      }
      break;
    }
  }

  if (!name[0]) {
    /* e.g. @UNIT23 stub with no NAMES row: fall back to the PEDIA body title. */
    pedia_entry_title(a->pedia, a->names, category, index, name, sizeof(name));
  }

  {
    char subtitle[48];
    char title[128];
    pedia_category_subtitle(a->pedia, category, subtitle, sizeof(subtitle));
    snprintf(title, sizeof(title), "(%s: %s)", name, subtitle);
    pedia_draw_centered(a->font, fb, fh + 7, title, PEDIA_COL_LINK_HOVER);
  }

  int body_y = 2 * fh + 0x15;
  switch (category) {
    case PEDIA_CAT_CARGO:
      body_y = pedia_article_cargo(a, fb, index, 2 * fh + 0x15);
      break;
    case PEDIA_CAT_UNIT:
      body_y = pedia_article_unit(a, fb, index, 2 * fh + 0x15);
      break;
    case PEDIA_CAT_TERRAIN:
      body_y = pedia_article_terrain(a, fb, index, 2 * fh + 9);
      break;
    case PEDIA_CAT_JOB:
      body_y = pedia_article_job(a, fb, index, 2 * fh + 10);
      break;
    case PEDIA_CAT_BUILDING:
      body_y = pedia_article_building(a, fb, index, 2 * fh + 0x15);
      break;
    case PEDIA_CAT_FATHER:
    case PEDIA_CAT_MISC:
    default:
      break;
  }

  const char* prefix = pedia_category_section_prefix(category);
  if (prefix) {
    char tag[32];
    snprintf(tag, sizeof(tag), "%s%d", prefix, index);
    pedia_body_render_section(a->pedia, tag, a->font, fb, body_y);
  } else if (category == PEDIA_CAT_MISC && index >= 0 && index < PEDIA_MISC_COUNT) {
    /*
     * DOS looks up @MISC<n> in PEDIA.TXT, which has no such sections — its
     * Game Concept pages are title-only. Keep the port's short reference
     * blurbs as the body instead of an empty page.
     */
    pedia_body_render_text(k_misc_bodies[index], a->font, fb, body_y, 300);
  }
}
