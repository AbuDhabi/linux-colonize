#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/savegame.h"

#define COLONIZE_SAVE_MAGIC 0x5a4c4f43u
#define COLONIZE_SAVE_VERSION 1u

static const char* safe_getenv(const char* name) {
  const char* value = getenv(name);
  return (value && value[0] != '\0') ? value : NULL;
}

const char* savegame_default_dir(void) {
  static char save_dir[512];
  const char* xdg = safe_getenv("XDG_DATA_HOME");
  const char* home = safe_getenv("HOME");
  if (xdg) {
    snprintf(save_dir, sizeof(save_dir), "%s/colonize-linux/saves", xdg);
  } else if (home) {
    snprintf(save_dir, sizeof(save_dir), "%s/.local/share/colonize-linux/saves", home);
  } else {
    snprintf(save_dir, sizeof(save_dir), "./saves");
  }
  return save_dir;
}

static bool ensure_dir(const char* path, char* err_buf, size_t err_buf_size) {
  if (mkdir(path, 0755) == 0 || errno == EEXIST) {
    return true;
  }
  snprintf(err_buf, err_buf_size, "Failed to create directory %s: %s", path, strerror(errno));
  return false;
}

static void path_for_slot(char* out, size_t out_size, const char* save_dir, const char* slot_name) {
  snprintf(out, out_size, "%s/%s.sav", save_dir, slot_name);
}

bool savegame_write(
  const char* save_dir,
  const char* slot_name,
  const ColonizeSavePayload* payload,
  char* err_buf,
  size_t err_buf_size
) {
  if (!save_dir || !slot_name || !payload) {
    snprintf(err_buf, err_buf_size, "Invalid savegame_write arguments.");
    return false;
  }
  if (!ensure_dir(save_dir, err_buf, err_buf_size)) {
    return false;
  }

  char path[640];
  path_for_slot(path, sizeof(path), save_dir, slot_name);
  FILE* f = fopen(path, "wb");
  if (!f) {
    snprintf(err_buf, err_buf_size, "Failed to open %s for write: %s", path, strerror(errno));
    return false;
  }

  ColonizeSaveHeader header = {
    .magic = COLONIZE_SAVE_MAGIC,
    .version = COLONIZE_SAVE_VERSION,
    .payload_size = (uint32_t)sizeof(*payload)
  };

  bool ok = fwrite(&header, sizeof(header), 1, f) == 1 && fwrite(payload, sizeof(*payload), 1, f) == 1;
  fclose(f);
  if (!ok) {
    snprintf(err_buf, err_buf_size, "Failed to write savegame %s.", path);
    return false;
  }
  if (err_buf && err_buf_size > 0) {
    err_buf[0] = '\0';
  }
  return true;
}

bool savegame_read(
  const char* save_dir,
  const char* slot_name,
  ColonizeSavePayload* out_payload,
  char* err_buf,
  size_t err_buf_size
) {
  if (!save_dir || !slot_name || !out_payload) {
    snprintf(err_buf, err_buf_size, "Invalid savegame_read arguments.");
    return false;
  }

  char path[640];
  path_for_slot(path, sizeof(path), save_dir, slot_name);
  FILE* f = fopen(path, "rb");
  if (!f) {
    snprintf(err_buf, err_buf_size, "Failed to open %s for read: %s", path, strerror(errno));
    return false;
  }

  ColonizeSaveHeader header = {0};
  if (fread(&header, sizeof(header), 1, f) != 1) {
    fclose(f);
    snprintf(err_buf, err_buf_size, "Failed to read savegame header %s.", path);
    return false;
  }
  if (header.magic != COLONIZE_SAVE_MAGIC || header.version != COLONIZE_SAVE_VERSION || header.payload_size != sizeof(*out_payload)) {
    fclose(f);
    snprintf(err_buf, err_buf_size, "Incompatible savegame format in %s.", path);
    return false;
  }
  if (fread(out_payload, sizeof(*out_payload), 1, f) != 1) {
    fclose(f);
    snprintf(err_buf, err_buf_size, "Failed to read savegame payload %s.", path);
    return false;
  }
  fclose(f);
  if (err_buf && err_buf_size > 0) {
    err_buf[0] = '\0';
  }
  return true;
}
