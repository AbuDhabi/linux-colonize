#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/col1_save.h"
#include "core/save_load_dialog.h"
#include "core/savegame.h"
#include "platform/platform.h"

static int copy_file(const char* src, const char* dst) {
  FILE* in = fopen(src, "rb");
  if (!in) {
    return -1;
  }
  FILE* out = fopen(dst, "wb");
  if (!out) {
    fclose(in);
    return -1;
  }
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      fclose(in);
      fclose(out);
      return -1;
    }
  }
  fclose(in);
  fclose(out);
  return 0;
}

int main(void) {
  const char* dir = "./test-saves-slot-ui";
  mkdir(dir, 0755);

  /* Ensure a clean slate for empty-slot checks. */
  for (int slot = 0; slot <= 9; ++slot) {
    char path[256];
    if (savegame_colony_slot_path(dir, slot, path, sizeof(path))) {
      unlink(path);
    }
  }

  ColonizeSaveSlotInfo empty_info;
  if (!savegame_probe_col1_slot(dir, 0, &empty_info)) {
    fprintf(stderr, "probe empty dir failed\n");
    return 1;
  }
  if (empty_info.occupied) {
    fprintf(stderr, "expected empty slot 0\n");
    return 1;
  }

  char path[256];
  if (!savegame_colony_slot_path(dir, 0, path, sizeof(path))) {
    fprintf(stderr, "slot path failed\n");
    return 1;
  }
  if (copy_file("original_saves/COLONY00.SAV", path) != 0) {
    fprintf(stderr, "copy fixture failed (need original_saves/COLONY00.SAV)\n");
    return 1;
  }

  ColonizeSaveSlotInfo info;
  if (!savegame_probe_col1_slot(dir, 0, &info) || !info.occupied) {
    fprintf(stderr, "probe occupied failed\n");
    return 1;
  }
  if (info.leader_name[0] == '\0' || info.year == 0) {
    fprintf(stderr, "probe missing name/year (name='%s' year=%u)\n", info.leader_name, info.year);
    return 1;
  }

  SaveLoadDialog dlg;
  if (!save_load_open(&dlg, SAVE_LOAD_MODE_SAVE, dir, NULL, NULL)) {
    fprintf(stderr, "save open failed\n");
    return 1;
  }
  if (!dlg.open || dlg.option_count != 8) {
    fprintf(stderr, "save dialog expected 8 slots, got %d\n", dlg.option_count);
    return 1;
  }
  if (!dlg.slot_occupied[0] || strstr(dlg.options[0], info.leader_name) == NULL) {
    fprintf(stderr, "save dialog label missing leader: '%s'\n", dlg.options[0]);
    return 1;
  }
  save_load_close(&dlg);

  if (!save_load_open(&dlg, SAVE_LOAD_MODE_LOAD, dir, NULL, NULL)) {
    fprintf(stderr, "load open failed\n");
    return 1;
  }
  if (!dlg.open || dlg.option_count != 10) {
    fprintf(stderr, "load dialog expected 10 slots, got %d\n", dlg.option_count);
    return 1;
  }

  ColonizeInputState input;
  memset(&input, 0, sizeof(input));
  input.last_key = COLONIZE_KEY_ENTER;
  dlg.selection = 0;
  save_load_handle_input(&dlg, &input);
  if (!dlg.has_result || dlg.result_slot != 0 || dlg.result_mode != SAVE_LOAD_MODE_LOAD) {
    fprintf(stderr, "load confirm failed\n");
    return 1;
  }

  /* Empty slot must not confirm in Load mode. */
  if (!save_load_open(&dlg, SAVE_LOAD_MODE_LOAD, dir, NULL, NULL)) {
    fprintf(stderr, "load reopen failed\n");
    return 1;
  }
  dlg.selection = 1; /* Empty */
  memset(&input, 0, sizeof(input));
  input.last_key = COLONIZE_KEY_ENTER;
  save_load_handle_input(&dlg, &input);
  if (dlg.has_result || !dlg.open) {
    fprintf(stderr, "load should not confirm empty slot\n");
    return 1;
  }

  printf("smoke_save_load_dialog ok (leader=%s year=%u)\n", info.leader_name, info.year);
  return 0;
}
