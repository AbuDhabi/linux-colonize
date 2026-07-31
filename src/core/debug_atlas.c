#include "core/debug_atlas.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "platform/diagnostics.h"
#include "platform/platform.h"

void debug_atlas_init(DebugAtlas* atlas) {
  if (!atlas) {
    return;
  }
  memset(atlas, 0, sizeof(*atlas));
  atlas->index = -1;
}

void debug_atlas_free(DebugAtlas* atlas) {
  if (!atlas) {
    return;
  }
  if (atlas->loaded) {
    if (atlas->loaded_kind == DEBUG_ATLAS_KIND_SS) {
      ss_free(&atlas->ss);
    } else {
      pik_free(&atlas->pik);
    }
  }
  memset(atlas, 0, sizeof(*atlas));
  atlas->index = -1;
}

static void debug_atlas_upper_ext(char* ext) {
  for (char* p = ext; *p; ++p) {
    *p = (char)toupper((unsigned char)*p);
  }
}

static int debug_atlas_entry_cmp(const void* a, const void* b) {
  const DebugAtlasEntry* ea = (const DebugAtlasEntry*)a;
  const DebugAtlasEntry* eb = (const DebugAtlasEntry*)b;
  return strcasecmp(ea->name, eb->name);
}

int debug_atlas_scan(DebugAtlas* atlas, const char* data_dir) {
  if (!atlas || !data_dir) {
    return 0;
  }
  debug_atlas_free(atlas);
  debug_atlas_init(atlas);

  DIR* dir = opendir(data_dir);
  if (!dir) {
    snprintf(atlas->load_error, sizeof(atlas->load_error), "opendir failed");
    return 0;
  }

  struct dirent* ent;
  while ((ent = readdir(dir)) != NULL && atlas->count < DEBUG_ATLAS_MAX_ENTRIES) {
    if (ent->d_name[0] == '.') {
      continue;
    }
    const char* dot = strrchr(ent->d_name, '.');
    if (!dot || dot == ent->d_name) {
      continue;
    }
    char ext[8];
    snprintf(ext, sizeof(ext), "%s", dot);
    debug_atlas_upper_ext(ext);

    DebugAtlasKind kind;
    if (strcmp(ext, ".SS") == 0) {
      kind = DEBUG_ATLAS_KIND_SS;
    } else if (strcmp(ext, ".PIK") == 0) {
      kind = DEBUG_ATLAS_KIND_PIK;
    } else {
      continue;
    }

    DebugAtlasEntry* e = &atlas->entries[atlas->count++];
    snprintf(e->name, sizeof(e->name), "%s", ent->d_name);
    e->kind = kind;
  }
  closedir(dir);

  qsort(atlas->entries, (size_t)atlas->count, sizeof(atlas->entries[0]), debug_atlas_entry_cmp);
  diag_info("Debug atlas scanned %d graphic files in %s", atlas->count, data_dir);
  return atlas->count;
}

static void debug_atlas_unload(DebugAtlas* atlas) {
  if (!atlas || !atlas->loaded) {
    return;
  }
  if (atlas->loaded_kind == DEBUG_ATLAS_KIND_SS) {
    ss_free(&atlas->ss);
  } else {
    pik_free(&atlas->pik);
  }
  atlas->loaded = false;
  atlas->load_error[0] = '\0';
}

bool debug_atlas_load(DebugAtlas* atlas, const char* data_dir, int index) {
  if (!atlas || !data_dir || atlas->count <= 0) {
    return false;
  }
  if (index < 0) {
    index = 0;
  }
  if (index >= atlas->count) {
    index = atlas->count - 1;
  }

  if (atlas->loaded && atlas->index == index) {
    return true;
  }

  debug_atlas_unload(atlas);
  atlas->index = index;
  atlas->scroll = 0;

  const DebugAtlasEntry* e = &atlas->entries[index];
  char path[512];
  char err[256];
  if (!dos_compat_normalize_asset_path(data_dir, e->name, path, sizeof(path))) {
    snprintf(atlas->load_error, sizeof(atlas->load_error), "path resolve failed");
    return false;
  }

  if (e->kind == DEBUG_ATLAS_KIND_SS) {
    memset(&atlas->ss, 0, sizeof(atlas->ss));
    if (!ss_load(path, &atlas->ss, err, sizeof(err))) {
      snprintf(atlas->load_error, sizeof(atlas->load_error), "%.90s", err);
      return false;
    }
    atlas->loaded_kind = DEBUG_ATLAS_KIND_SS;
    atlas->loaded = true;
    return true;
  }

  memset(&atlas->pik, 0, sizeof(atlas->pik));
  if (!pik_load(path, &atlas->pik, err, sizeof(err))) {
    snprintf(atlas->load_error, sizeof(atlas->load_error), "%.90s", err);
    return false;
  }
  atlas->loaded_kind = DEBUG_ATLAS_KIND_PIK;
  atlas->loaded = true;
  return true;
}

void debug_atlas_next_file(DebugAtlas* atlas, const char* data_dir, int step) {
  if (!atlas || atlas->count <= 0) {
    return;
  }
  if (step < 1) {
    step = 1;
  }
  int next = atlas->index + step;
  if (next >= atlas->count) {
    next = next % atlas->count;
  }
  debug_atlas_load(atlas, data_dir, next);
}

void debug_atlas_prev_file(DebugAtlas* atlas, const char* data_dir, int step) {
  if (!atlas || atlas->count <= 0) {
    return;
  }
  if (step < 1) {
    step = 1;
  }
  int prev = atlas->index - step;
  while (prev < 0) {
    prev += atlas->count;
  }
  debug_atlas_load(atlas, data_dir, prev);
}

/* Cell sized to the largest sprite so wide art (fence, docks) does not overlap neighbours. */
static void debug_atlas_ss_cell_size(const ColonizeSpriteSheet* sheet, int* out_w, int* out_h) {
  int max_w = 16;
  int max_h = 16;
  if (sheet) {
    for (int i = 0; i < sheet->sprite_count; ++i) {
      if (sheet->sprites[i].width > max_w) {
        max_w = sheet->sprites[i].width;
      }
      if (sheet->sprites[i].height > max_h) {
        max_h = sheet->sprites[i].height;
      }
    }
  }
  if (out_w) {
    *out_w = max_w + 4;
  }
  if (out_h) {
    *out_h = max_h + 10; /* room for index label */
  }
}

static bool debug_atlas_ss_single_mode(const ColonizeSpriteSheet* sheet) {
  if (!sheet || sheet->sprite_count <= 0) {
    return true;
  }
  if (sheet->sprite_count == 1) {
    return true;
  }
  int cell_w = 0;
  debug_atlas_ss_cell_size(sheet, &cell_w, NULL);
  /* Fall back to one-at-a-time when even two columns won't fit. */
  return cell_w > 160;
}

static int debug_atlas_max_scroll(const DebugAtlas* atlas) {
  if (!atlas || !atlas->loaded) {
    return 0;
  }
  if (atlas->loaded_kind == DEBUG_ATLAS_KIND_PIK) {
    const int overflow = atlas->pik.height - (200 - 14);
    if (overflow <= 0) {
      return 0;
    }
    return (overflow + 7) / 8;
  }
  if (debug_atlas_ss_single_mode(&atlas->ss)) {
    return atlas->ss.sprite_count > 0 ? atlas->ss.sprite_count - 1 : 0;
  }
  int cell_w = 20;
  int cell_h = 26;
  debug_atlas_ss_cell_size(&atlas->ss, &cell_w, &cell_h);
  const int cols = 320 / cell_w;
  const int rows = (200 - 14) / cell_h;
  if (cols < 1 || rows < 1) {
    return atlas->ss.sprite_count > 0 ? atlas->ss.sprite_count - 1 : 0;
  }
  const int visible = cols * rows;
  if (atlas->ss.sprite_count <= visible) {
    return 0;
  }
  const int last_start = atlas->ss.sprite_count - visible;
  return (last_start + cols - 1) / cols;
}

void debug_atlas_scroll_by(DebugAtlas* atlas, int delta) {
  if (!atlas) {
    return;
  }
  const int max_scroll = debug_atlas_max_scroll(atlas);
  int next = atlas->scroll + delta;
  if (next < 0) {
    next = 0;
  }
  if (next > max_scroll) {
    next = max_scroll;
  }
  atlas->scroll = next;
}

void debug_atlas_page_down(DebugAtlas* atlas) {
  if (!atlas || !atlas->loaded) {
    return;
  }
  if (atlas->loaded_kind == DEBUG_ATLAS_KIND_PIK) {
    debug_atlas_scroll_by(atlas, 10);
    return;
  }
  if (debug_atlas_ss_single_mode(&atlas->ss)) {
    debug_atlas_scroll_by(atlas, 1);
    return;
  }
  int cell_h = 26;
  debug_atlas_ss_cell_size(&atlas->ss, NULL, &cell_h);
  const int rows = (200 - 14) / cell_h;
  debug_atlas_scroll_by(atlas, rows > 0 ? rows : 1);
}

const ColonizePalette* debug_atlas_palette(const DebugAtlas* atlas) {
  if (!atlas || !atlas->loaded) {
    return NULL;
  }
  if (atlas->loaded_kind == DEBUG_ATLAS_KIND_SS && atlas->ss.has_palette) {
    return &atlas->ss.palette;
  }
  if (atlas->loaded_kind == DEBUG_ATLAS_KIND_PIK && atlas->pik.has_palette) {
    return &atlas->pik.palette;
  }
  return NULL;
}

static void debug_atlas_fill_checker(
  ColonizeFramebuffer8* framebuffer,
  int ox,
  int oy,
  int w,
  int h
) {
  for (int py = 0; py < h; ++py) {
    for (int px = 0; px < w; ++px) {
      const int dx = ox + px;
      const int dy = oy + py;
      if (dx < 0 || dy < 0 || dx >= framebuffer->width || dy >= framebuffer->height) {
        continue;
      }
      framebuffer->pixels[dy * framebuffer->width + dx] =
        (uint8_t)(((px / 4) ^ (py / 4)) & 1 ? 8 : 0);
    }
  }
}

void debug_atlas_render(
  const DebugAtlas* atlas,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!framebuffer || !framebuffer->pixels) {
    return;
  }
  memset(framebuffer->pixels, 0, (size_t)framebuffer->width * (size_t)framebuffer->height);

  char hud[120];
  if (!atlas || atlas->count <= 0) {
    font_draw_text(font, framebuffer, 4, 4, "Debug atlas: no .SS/.PIK files found", 12);
    return;
  }

  const DebugAtlasEntry* e = (atlas->index >= 0 && atlas->index < atlas->count)
    ? &atlas->entries[atlas->index]
    : NULL;
  const char* name = e ? e->name : "?";
  const char* kind = e ? (e->kind == DEBUG_ATLAS_KIND_SS ? "SS" : "PIK") : "?";

  if (!atlas->loaded) {
    snprintf(
      hud,
      sizeof(hud),
      "%s [%d/%d] LOAD FAIL  L/R file [/] +/-10 Esc",
      name,
      atlas->index + 1,
      atlas->count
    );
    font_draw_text(font, framebuffer, 2, 1, hud, 15);
    font_draw_text(font, framebuffer, 4, 20, atlas->load_error, 12);
    return;
  }

  if (atlas->loaded_kind == DEBUG_ATLAS_KIND_PIK) {
    const int pan_y = -(atlas->scroll * 8);
    pik_blit(&atlas->pik, framebuffer, 0, 12 + pan_y);
    snprintf(
      hud,
      sizeof(hud),
      "%s [%d/%d] %dx%d pan=%d  L/R file Up/Dn pan Esc",
      name,
      atlas->index + 1,
      atlas->count,
      atlas->pik.width,
      atlas->pik.height,
      atlas->scroll
    );
    /* Clear HUD band so text stays readable over busy art. */
    for (int y = 0; y < 12; ++y) {
      memset(&framebuffer->pixels[y * framebuffer->width], 0, (size_t)framebuffer->width);
    }
    font_draw_text(font, framebuffer, 2, 1, hud, 15);
    return;
  }

  /* Sprite sheet */
  const ColonizeSpriteSheet* sheet = &atlas->ss;
  if (debug_atlas_ss_single_mode(sheet)) {
    int idx = atlas->scroll;
    if (idx < 0) {
      idx = 0;
    }
    if (idx >= sheet->sprite_count) {
      idx = sheet->sprite_count - 1;
    }
    const ColonizeSprite* spr = &sheet->sprites[idx];
    const int w = spr && spr->pixels ? spr->width : 0;
    const int h = spr && spr->pixels ? spr->height : 0;
    const int ox = w > 0 ? (framebuffer->width - w) / 2 : 0;
    const int oy = 14 + (h > 0 && h < framebuffer->height - 14 ? (framebuffer->height - 14 - h) / 2 : 0);
    debug_atlas_fill_checker(framebuffer, ox, oy, w > 0 ? w : 16, h > 0 ? h : 16);
    if (w > 0) {
      ss_blit_sprite(sheet, idx, framebuffer, ox, oy);
    }
    snprintf(
      hud,
      sizeof(hud),
      "%s#%d [%d/%d] %dx%d (%d spr)  L/R file Up/Dn # Esc",
      name,
      idx,
      atlas->index + 1,
      atlas->count,
      w,
      h,
      sheet->sprite_count
    );
    /* PHYS0: labels are 0-based blit indices (MAPEDIT IDs are index+1). */
    if (strstr(name, "PHYS0") != NULL) {
      snprintf(
        hud,
        sizeof(hud),
        "%s#%d (=MAPEDIT %d) [%d/%d] %dx%d  L/R Up/Dn Esc",
        name,
        idx,
        idx + 1,
        atlas->index + 1,
        atlas->count,
        w,
        h
      );
    }
    for (int y = 0; y < 12; ++y) {
      memset(&framebuffer->pixels[y * framebuffer->width], 0, (size_t)framebuffer->width);
    }
    font_draw_text(font, framebuffer, 2, 1, hud, 15);
    (void)kind;
    return;
  }

  const int cell_w_default = 20;
  const int cell_h_default = 26;
  int cell_w = cell_w_default;
  int cell_h = cell_h_default;
  debug_atlas_ss_cell_size(sheet, &cell_w, &cell_h);
  const int cols = framebuffer->width / cell_w;
  const int rows = (framebuffer->height - 14) / cell_h;
  if (cols < 1 || rows < 1) {
    /* Should have taken single-mode; draw first visible sprite centered. */
    int idx = atlas->scroll;
    if (idx < 0) {
      idx = 0;
    }
    if (idx >= sheet->sprite_count) {
      idx = sheet->sprite_count - 1;
    }
    ss_blit_sprite(sheet, idx, framebuffer, 4, 14);
    return;
  }
  const int first = atlas->scroll * cols;
  const int last = first + cols * rows - 1;
  const int shown_last = last >= sheet->sprite_count ? sheet->sprite_count - 1 : last;

  snprintf(
    hud,
    sizeof(hud),
    "%s [%d/%d] #%d-%d/%d cell %dx%d  L/R file Up/Dn Esc",
    name,
    atlas->index + 1,
    atlas->count,
    sheet->sprite_count > 0 ? first : 0,
    shown_last >= 0 ? shown_last : 0,
    sheet->sprite_count,
    cell_w,
    cell_h
  );
  if (strstr(name, "PHYS0") != NULL) {
    snprintf(
      hud,
      sizeof(hud),
      "%s [%d/%d] #%d-%d (0-based; MAPEDIT=+1)  L/R Up/Dn Esc",
      name,
      atlas->index + 1,
      atlas->count,
      sheet->sprite_count > 0 ? first : 0,
      shown_last >= 0 ? shown_last : 0
    );
  }
  font_draw_text(font, framebuffer, 2, 1, hud, 15);

  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const int idx = first + row * cols + col;
      if (idx < 0 || idx >= sheet->sprite_count) {
        continue;
      }
      const ColonizeSprite* spr = &sheet->sprites[idx];
      const int sw = spr && spr->pixels ? spr->width : 16;
      const int sh = spr && spr->pixels ? spr->height : 16;
      const int ox = col * cell_w + 2;
      const int oy = 14 + row * cell_h;
      debug_atlas_fill_checker(framebuffer, ox, oy, sw > 0 ? sw : 16, sh > 0 ? sh : 16);
      ss_blit_sprite(sheet, idx, framebuffer, ox, oy);
      char label[12];
      snprintf(label, sizeof(label), "%d", idx);
      font_draw_text(font, framebuffer, ox, oy + sh + 1, label, 14);
    }
  }
}
