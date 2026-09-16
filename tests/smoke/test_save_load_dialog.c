#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

#include "core/col1_save.h"
#include "core/save_load_dialog.h"
#include "core/savegame.h"
#include "platform/platform.h"

#include "../common/test_runner.h"

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

static const char* const k_dir = "./test-saves-slot-ui";

/* Ensure a clean slate: dir exists, no slot occupies 0..9. */
static int clear_all_slots(void) {
  mkdir(k_dir, 0755);
  for (int slot = 0; slot <= 9; ++slot) {
    char path[256];
    if (savegame_colony_slot_path(k_dir, slot, path, sizeof(path))) {
      unlink(path);
    }
  }
  return 1;
}

/* Clean slate, then seed slot 0 with the real fixture save. Every case that
 * needs an occupied slot 0 calls this itself (rather than relying on a
 * previous case having copied the fixture in), so cases stay independent
 * of declared/shuffled/reversed order. */
static int seed_slot0(void) {
  if (!clear_all_slots()) {
    return 0;
  }
  char path[256];
  if (!savegame_colony_slot_path(k_dir, 0, path, sizeof(path))) {
    fprintf(stderr, "slot path failed\n");
    return 0;
  }
  if (copy_file("original_saves/COLONY00.SAV", path) != 0) {
    fprintf(stderr, "copy fixture failed (need original_saves/COLONY00.SAV)\n");
    return 0;
  }
  return 1;
}

static int probe_slot0(ColonizeSaveSlotInfo* info) {
  return savegame_probe_col1_slot(k_dir, 0, info);
}

static int case_probe_empty_slot(void) {
  if (!clear_all_slots()) {
    return 1;
  }
  ColonizeSaveSlotInfo empty_info;
  if (!probe_slot0(&empty_info)) {
    fprintf(stderr, "probe empty dir failed\n");
    return 1;
  }
  if (empty_info.occupied) {
    fprintf(stderr, "expected empty slot 0\n");
    return 1;
  }
  return 0;
}

static int case_probe_occupied_slot(void) {
  if (!seed_slot0()) {
    return 1;
  }
  ColonizeSaveSlotInfo info;
  if (!probe_slot0(&info) || !info.occupied) {
    fprintf(stderr, "probe occupied failed\n");
    return 1;
  }
  if (info.leader_name[0] == '\0' || info.year == 0) {
    fprintf(stderr, "probe missing name/year (name='%s' year=%u)\n", info.leader_name, info.year);
    return 1;
  }
  return 0;
}

static int case_save_dialog_lists_leader(void) {
  if (!seed_slot0()) {
    return 1;
  }
  ColonizeSaveSlotInfo info;
  if (!probe_slot0(&info) || !info.occupied) {
    fprintf(stderr, "probe occupied failed\n");
    return 1;
  }

  SaveLoadDialog dlg;
  if (!save_load_open(&dlg, SAVE_LOAD_MODE_SAVE, k_dir, NULL, NULL)) {
    fprintf(stderr, "save open failed\n");
    return 1;
  }
  int rc = 0;
  if (!dlg.open || dlg.option_count != 8) {
    fprintf(stderr, "save dialog expected 8 slots, got %d\n", dlg.option_count);
    rc = 1;
  } else if (!dlg.slot_occupied[0] || strstr(dlg.options[0], info.leader_name) == NULL) {
    fprintf(stderr, "save dialog label missing leader: '%s'\n", dlg.options[0]);
    rc = 1;
  }
  save_load_close(&dlg);
  return rc;
}

static int case_load_dialog_slot_count(void) {
  if (!seed_slot0()) {
    return 1;
  }
  SaveLoadDialog dlg;
  if (!save_load_open(&dlg, SAVE_LOAD_MODE_LOAD, k_dir, NULL, NULL)) {
    fprintf(stderr, "load open failed\n");
    return 1;
  }
  int rc = 0;
  if (!dlg.open || dlg.option_count != 10) {
    fprintf(stderr, "load dialog expected 10 slots, got %d\n", dlg.option_count);
    rc = 1;
  }
  save_load_close(&dlg);
  return rc;
}

static int case_load_confirms_occupied_slot(void) {
  if (!seed_slot0()) {
    return 1;
  }
  SaveLoadDialog dlg;
  if (!save_load_open(&dlg, SAVE_LOAD_MODE_LOAD, k_dir, NULL, NULL)) {
    fprintf(stderr, "load open failed\n");
    return 1;
  }
  ColonizeInputState input;
  memset(&input, 0, sizeof(input));
  input.last_key = COLONIZE_KEY_ENTER;
  dlg.selection = 0;
  save_load_handle_input(&dlg, &input);
  int rc = 0;
  if (!dlg.has_result || dlg.result_slot != 0 || dlg.result_mode != SAVE_LOAD_MODE_LOAD) {
    fprintf(stderr, "load confirm failed\n");
    rc = 1;
  }
  save_load_close(&dlg);
  return rc;
}

static int case_load_rejects_empty_slot(void) {
  if (!seed_slot0()) {
    return 1;
  }
  /* Empty slot must not confirm in Load mode. */
  SaveLoadDialog dlg;
  if (!save_load_open(&dlg, SAVE_LOAD_MODE_LOAD, k_dir, NULL, NULL)) {
    fprintf(stderr, "load reopen failed\n");
    return 1;
  }
  dlg.selection = 1; /* Empty */
  ColonizeInputState input;
  memset(&input, 0, sizeof(input));
  input.last_key = COLONIZE_KEY_ENTER;
  save_load_handle_input(&dlg, &input);
  int rc = 0;
  if (dlg.has_result || !dlg.open) {
    fprintf(stderr, "load should not confirm empty slot\n");
    rc = 1;
  }
  save_load_close(&dlg);
  return rc;
}

static const TestCase k_cases[] = {
    {"case_probe_empty_slot", case_probe_empty_slot},
    {"case_probe_occupied_slot", case_probe_occupied_slot},
    {"case_save_dialog_lists_leader", case_save_dialog_lists_leader},
    {"case_load_dialog_slot_count", case_load_dialog_slot_count},
    {"case_load_confirms_occupied_slot", case_load_confirms_occupied_slot},
    {"case_load_rejects_empty_slot", case_load_rejects_empty_slot},
};

TEST_MAIN(k_cases)
