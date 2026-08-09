#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/savegame.h"
#include "platform/diagnostics.h"

#define COLONIZE_SAVE_MAGIC 0x5a4c4f43u
#define COLONIZE_SAVE_VERSION 1u

static bool ensure_dir(const char* path, char* err_buf, size_t err_buf_size) {
  struct stat st;
  if (stat(path, &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      diag_info("Save directory exists: %s", path);
      return true;
    }
    snprintf(err_buf, err_buf_size, "Save path exists but is not a directory: %s", path);
    diag_error("%s", err_buf);
    return false;
  }
  diag_info("Creating save directory: %s", path);
  if (mkdir(path, 0755) == 0) {
    diag_info("Created save directory: %s", path);
    return true;
  }
  if (errno == EEXIST) {
    return true;
  }
  snprintf(err_buf, err_buf_size, "Failed to create directory %s: %s", path, strerror(errno));
  diag_error("%s", err_buf);
  return false;
}

const char* savegame_default_dir(void) {
  static char save_dir[512];
  static bool resolved = false;
  if (resolved) {
    return save_dir;
  }
  const char* exe_dir = diag_exe_dir();
  if (exe_dir && exe_dir[0] != '\0') {
    snprintf(save_dir, sizeof(save_dir), "%s/COLONIZE", exe_dir);
  } else {
    snprintf(save_dir, sizeof(save_dir), "./COLONIZE");
  }
  char err[256];
  if (!ensure_dir(save_dir, err, sizeof(err))) {
    diag_warn("Could not create default COLONIZE directory: %s", err);
  }
  diag_info("Default save directory resolved to: %s", save_dir);
  resolved = true;
  return save_dir;
}

static void path_for_slot(char* out, size_t out_size, const char* save_dir, const char* slot_name) {
  snprintf(out, out_size, "%s/%s.sav", save_dir, slot_name);
}

bool savegame_colony_slot_path(
  const char* save_dir,
  int slot,
  char* out_path,
  size_t out_path_size
) {
  if (!save_dir || !out_path || out_path_size == 0 || slot < 0 || slot > 9) {
    return false;
  }
  snprintf(out_path, out_path_size, "%s/COLONY%02d.SAV", save_dir, slot);
  return true;
}

bool savegame_write_col1(
  const char* save_dir,
  int slot,
  const ColonizeCol1Save* save,
  char* err_buf,
  size_t err_buf_size
) {
  if (!save_dir || !save) {
    snprintf(err_buf, err_buf_size, "Invalid savegame_write_col1 arguments.");
    return false;
  }
  if (!ensure_dir(save_dir, err_buf, err_buf_size)) {
    return false;
  }
  char path[640];
  if (!savegame_colony_slot_path(save_dir, slot, path, sizeof(path))) {
    snprintf(err_buf, err_buf_size, "Invalid COLONY slot %d.", slot);
    return false;
  }
  return col1_save_write_file(path, save, err_buf, err_buf_size);
}

bool savegame_read_col1(
  const char* save_dir,
  int slot,
  ColonizeCol1Save* out_save,
  char* err_buf,
  size_t err_buf_size
) {
  if (!save_dir || !out_save) {
    snprintf(err_buf, err_buf_size, "Invalid savegame_read_col1 arguments.");
    return false;
  }
  char path[640];
  if (!savegame_colony_slot_path(save_dir, slot, path, sizeof(path))) {
    snprintf(err_buf, err_buf_size, "Invalid COLONY slot %d.", slot);
    return false;
  }
  return col1_save_read_file(path, out_save, err_buf, err_buf_size);
}

bool savegame_probe_col1_slot(
  const char* save_dir,
  int slot,
  ColonizeSaveSlotInfo* out
) {
  if (!out) {
    return false;
  }
  memset(out, 0, sizeof(*out));
  if (!save_dir || slot < 0 || slot > 9) {
    return false;
  }

  char path[640];
  if (!savegame_colony_slot_path(save_dir, slot, path, sizeof(path))) {
    return false;
  }

  FILE* f = fopen(path, "rb");
  if (!f) {
    return true; /* not occupied */
  }

  uint8_t buf[COLONIZE_COL1_PREFIX_SIZE];
  const size_t n = fread(buf, 1, sizeof(buf), f);
  fclose(f);
  if (n < sizeof(buf)) {
    return true;
  }
  if (memcmp(buf, COLONIZE_COL1_SIG, 8) != 0) {
    return true;
  }

  ColonizeCol1Head head;
  ColonizeCol1Player players[COLONIZE_COL1_NATION_COUNT];
  memcpy(&head, buf, sizeof(head));
  memcpy(players, buf + sizeof(head), sizeof(players));

  int human = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT; ++i) {
    if (players[i].control == 0) {
      human = i;
      break;
    }
  }

  out->occupied = true;
  memcpy(out->leader_name, players[human].name, sizeof(out->leader_name));
  out->leader_name[sizeof(out->leader_name) - 1] = '\0';
  /* Trim trailing NULs already; strip trailing spaces if any. */
  for (int i = (int)strlen(out->leader_name) - 1; i >= 0; --i) {
    if (out->leader_name[i] == ' ' || out->leader_name[i] == '\t') {
      out->leader_name[i] = '\0';
    } else {
      break;
    }
  }
  if (out->leader_name[0] == '\0') {
    snprintf(out->leader_name, sizeof(out->leader_name), "Governor");
  }
  out->year = head.year;
  out->autumn = head.autumn;
  out->turn = head.turn;
  return true;
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
  diag_info("savegame_write path=%s turn=%u map_seed=%u random_seed=%u",
    path, payload->turn_number, payload->map_seed, payload->random_seed);
  FILE* f = fopen(path, "wb");
  if (!f) {
    snprintf(err_buf, err_buf_size, "Failed to open %s for write: %s", path, strerror(errno));
    diag_error("%s", err_buf);
    return false;
  }

  ColonizeSaveHeader header = {
    .magic = COLONIZE_SAVE_MAGIC,
    .version = COLONIZE_SAVE_VERSION,
    .payload_size = (uint32_t)sizeof(*payload)
  };

  bool ok = fwrite(&header, sizeof(header), 1, f) == 1 && fwrite(payload, sizeof(*payload), 1, f) == 1;
  if (fclose(f) != 0) {
    snprintf(err_buf, err_buf_size, "Failed to close savegame %s: %s", path, strerror(errno));
    diag_error("%s", err_buf);
    return false;
  }
  if (!ok) {
    snprintf(err_buf, err_buf_size, "Failed to write savegame %s.", path);
    diag_error("%s", err_buf);
    return false;
  }
  diag_info("savegame_write succeeded: %s (%zu bytes payload)", path, sizeof(*payload));
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
  diag_info("savegame_read path=%s", path);
  FILE* f = fopen(path, "rb");
  if (!f) {
    snprintf(err_buf, err_buf_size, "Failed to open %s for read: %s", path, strerror(errno));
    diag_error("%s", err_buf);
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
    diag_error("%s (magic=0x%x version=%u payload_size=%u expected_payload=%zu)",
      err_buf, header.magic, header.version, header.payload_size, sizeof(*out_payload));
    return false;
  }
  if (fread(out_payload, sizeof(*out_payload), 1, f) != 1) {
    fclose(f);
    snprintf(err_buf, err_buf_size, "Failed to read savegame payload %s.", path);
    diag_error("%s", err_buf);
    return false;
  }
  fclose(f);
  diag_info("savegame_read succeeded: turn=%u map_seed=%u random_seed=%u",
    out_payload->turn_number, out_payload->map_seed, out_payload->random_seed);
  if (err_buf && err_buf_size > 0) {
    err_buf[0] = '\0';
  }
  return true;
}
