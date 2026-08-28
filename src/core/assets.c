#include "core/assets.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "platform/diagnostics.h"
#include "platform/platform.h"
#include "core/strutil.h"

static bool file_exists(const char* path) {
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool dir_exists(const char* path) {
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void strip_crlf(char* line) {
  size_t n = strlen(line);
  while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
    line[--n] = '\0';
  }
}

static uint8_t vga6_to8(uint8_t v) {
  /* Classic VGA DAC values are 0..63. */
  if (v > 63) {
    return v;
  }
  return (uint8_t)((v << 2) | (v >> 4));
}

bool assets_resolve_data_dir(const char* override_dir, char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return false;
  }
  out[0] = '\0';

  if (override_dir && override_dir[0] != '\0' && dir_exists(override_dir)) {
    snprintf(out, out_size, "%s", override_dir);
    diag_info("Resolved data_dir from override: %s", out);
    return true;
  }

  const char* exe_dir = diag_exe_dir();
  if (exe_dir && exe_dir[0] != '\0') {
    char candidate[1024];
    snprintf(candidate, sizeof(candidate), "%s/COLONIZE", exe_dir);
    if (dir_exists(candidate)) {
      snprintf(out, out_size, "%s", candidate);
      diag_info("Resolved data_dir next to executable: %s", out);
      return true;
    }
  }

  if (dir_exists("./COLONIZE")) {
    snprintf(out, out_size, "%s", "./COLONIZE");
    diag_info("Resolved data_dir from working directory: %s", out);
    return true;
  }

  snprintf(out, out_size, "%s", override_dir && override_dir[0] ? override_dir : "./COLONIZE");
  diag_error("Could not locate COLONIZE data directory (tried override/exe/cwd). Using %s", out);
  return false;
}

bool assets_validate_required_files(const char* data_dir, char* err_buf, size_t err_buf_size) {
  static const char* required[] = {
    "MODULES.DB",
    "ERRORS.DB",
    "GAME.TXT",
    "MENU.TXT",
    "VICEROY.PAL"
  };
  if (!data_dir) {
    snprintf(err_buf, err_buf_size, "No data directory supplied.");
    return false;
  }
  for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
    char path[512];
    if (!dos_compat_normalize_asset_path(data_dir, required[i], path, sizeof(path))) {
      snprintf(err_buf, err_buf_size, "Missing required asset: %s/%s", data_dir, required[i]);
      diag_error("Asset missing (normalize failed): %s/%s", data_dir, required[i]);
      return false;
    }
    if (!file_exists(path)) {
      snprintf(err_buf, err_buf_size, "Missing required asset: %s", path);
      diag_error("Asset missing: %s", path);
      return false;
    }
    struct stat st;
    if (stat(path, &st) == 0) {
      diag_info("Asset OK: %s size=%lld bytes", path, (long long)st.st_size);
    } else {
      diag_warn("Asset exists check passed but stat failed for %s: %s", path, strerror(errno));
    }
  }
  if (err_buf && err_buf_size > 0) {
    err_buf[0] = '\0';
  }
  return true;
}

void assets_log_inventory(const char* data_dir) {
  if (!data_dir || !dir_exists(data_dir)) {
    diag_warn("Asset inventory skipped; data_dir unavailable.");
    return;
  }

  DIR* dir = opendir(data_dir);
  if (!dir) {
    diag_warn("opendir(%s) failed: %s", data_dir, strerror(errno));
    return;
  }

  int counts[16] = {0};
  const char* labels[16] = {
    ".TXT", ".PIK", ".SS", ".PAL", ".COL", ".DAT", ".DB", ".BIN",
    ".MP", ".MOV", ".EXE", ".FF", ".GIF", ".BAT", ".COM", "OTHER"
  };

  int total = 0;
  struct dirent* ent;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.') {
      continue;
    }
    const char* dot = strrchr(ent->d_name, '.');
    int bucket = 15;
    if (dot) {
      char ext[8];
      snprintf(ext, sizeof(ext), "%s", dot);
      for (char* p = ext; *p; ++p) {
        *p = (char)toupper((unsigned char)*p);
      }
      for (int i = 0; i < 15; ++i) {
        if (strcmp(ext, labels[i]) == 0) {
          bucket = i;
          break;
        }
      }
    }
    counts[bucket]++;
    total++;
  }
  closedir(dir);

  diag_info("Asset inventory for %s: %d files", data_dir, total);
  for (int i = 0; i < 16; ++i) {
    if (counts[i] > 0) {
      diag_info("  %s: %d", labels[i], counts[i]);
    }
  }
}

bool assets_detect_madspack(const char* path, char* info, size_t info_size) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  unsigned char hdr[16];
  size_t n = fread(hdr, 1, sizeof(hdr), f);
  fclose(f);
  if (n < 14 || memcmp(hdr, "MADSPACK 2.0", 12) != 0) {
    return false;
  }
  uint16_t chunks = (uint16_t)(hdr[13] | (hdr[14] << 8));
  snprintf(info, info_size, "MADSPACK 2.0 chunks=%u", chunks);
  return true;
}

void assets_palette_from_col768(const uint8_t* raw, size_t raw_size, ColonizePalette* out_palette) {
  if (!raw || !out_palette || raw_size < 768) {
    return;
  }
  for (int i = 0; i < 256; ++i) {
    out_palette->rgb[i][0] = vga6_to8(raw[i * 3 + 0]);
    out_palette->rgb[i][1] = vga6_to8(raw[i * 3 + 1]);
    out_palette->rgb[i][2] = vga6_to8(raw[i * 3 + 2]);
  }
}

void assets_palette_from_viceroy1024(const uint8_t* raw, size_t raw_size, ColonizePalette* out_palette) {
  if (!raw || !out_palette || raw_size < 1024) {
    return;
  }
  for (int i = 0; i < 256; ++i) {
    out_palette->rgb[i][0] = vga6_to8(raw[i * 4 + 0]);
    out_palette->rgb[i][1] = vga6_to8(raw[i * 4 + 1]);
    out_palette->rgb[i][2] = vga6_to8(raw[i * 4 + 2]);
  }
}

bool assets_load_palette(const char* data_dir, ColonizePalette* out_palette) {
  if (!data_dir || !out_palette) {
    return false;
  }

  char path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "VICEROY.PAL", path, sizeof(path))) {
    diag_warn("VICEROY.PAL not found under %s", data_dir);
    return false;
  }

  FILE* f = fopen(path, "rb");
  if (!f) {
    diag_warn("Failed to open palette %s: %s", path, strerror(errno));
    return false;
  }

  uint8_t raw[1024];
  size_t n = fread(raw, 1, sizeof(raw), f);
  fclose(f);
  if (n != sizeof(raw)) {
    diag_warn("Unexpected VICEROY.PAL size: read %zu expected 1024", n);
    return false;
  }

  assets_palette_from_viceroy1024(raw, sizeof(raw), out_palette);
  diag_info("Loaded VICEROY.PAL (%s)", path);
  return true;
}

void assets_msg_init(ColonizeMsgCatalog* catalog) {
  if (!catalog) {
    return;
  }
  memset(catalog, 0, sizeof(*catalog));
}

void assets_msg_free(ColonizeMsgCatalog* catalog) {
  if (!catalog) {
    return;
  }
  for (int i = 0; i < catalog->section_count; ++i) {
    free(catalog->sections[i].lines);
  }
  free(catalog->sections);
  catalog->sections = NULL;
  catalog->section_count = 0;
  catalog->section_capacity = 0;
}

static ColonizeMsgSection* msg_add_section(ColonizeMsgCatalog* catalog, const char* name) {
  if (catalog->section_count >= catalog->section_capacity) {
    int next = catalog->section_capacity == 0 ? 32 : catalog->section_capacity * 2;
    ColonizeMsgSection* resized = realloc(catalog->sections, (size_t)next * sizeof(*resized));
    if (!resized) {
      return NULL;
    }
    catalog->sections = resized;
    catalog->section_capacity = next;
  }
  ColonizeMsgSection* section = &catalog->sections[catalog->section_count++];
  memset(section, 0, sizeof(*section));
  snprintf(section->name, sizeof(section->name), "%s", name);
  /* PEDIA.TXT has a few tags with trailing spaces (e.g. "@JOB12 "). */
  size_t n = strlen(section->name);
  while (n > 0 && (section->name[n - 1] == ' ' || section->name[n - 1] == '\t')) {
    section->name[--n] = '\0';
  }
  return section;
}

bool assets_msg_load_file(ColonizeMsgCatalog* catalog, const char* path) {
  if (!catalog || !path) {
    return false;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    diag_error("Failed to open message catalog %s: %s", path, strerror(errno));
    return false;
  }

  ColonizeMsgSection* current = NULL;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    strip_crlf(line);
    if (line[0] == '\0' || line[0] == ';') {
      continue;
    }
    if (line[0] == '@' && (line[1] < 'a' || line[1] > 'z')) {
      /* New section if @NAME style (not @width/@options/@default). */
      const char* body = line + 1;
      bool is_directive = false;
      for (const char* p = body; *p; ++p) {
        if (*p == '=') {
          is_directive = true;
          break;
        }
      }
      if (!is_directive &&
          strcmp(body, "options") != 0 &&
          strcmp(body, "smallfont") != 0) {
        /* Skip @; comment markers used in PEDIA.TXT / NAMES.TXT. */
        if (body[0] == ';') {
          current = NULL;
          continue;
        }
        current = msg_add_section(catalog, body);
        if (!current) {
          fclose(f);
          return false;
        }
        continue;
      }
    }
    if (!current) {
      continue;
    }
    if (current->line_count >= current->line_capacity) {
      const int next = current->line_capacity == 0 ? 16 : current->line_capacity * 2;
      char (*grown)[COLONIZE_MSG_LINE_LEN] =
        realloc(current->lines, (size_t)next * sizeof(*grown));
      if (!grown) {
        fclose(f);
        return false;
      }
      current->lines = grown;
      current->line_capacity = next;
    }
    str_copy_trunc(current->lines[current->line_count], COLONIZE_MSG_LINE_LEN, line);
    current->line_count++;
  }
  fclose(f);
  diag_info("Loaded message catalog %s (%d sections)", path, catalog->section_count);
  return true;
}

const ColonizeMsgSection* assets_msg_find(const ColonizeMsgCatalog* catalog, const char* section_name) {
  if (!catalog || !section_name) {
    return NULL;
  }
  for (int i = 0; i < catalog->section_count; ++i) {
    if (strcmp(catalog->sections[i].name, section_name) == 0) {
      return &catalog->sections[i];
    }
  }
  return NULL;
}
