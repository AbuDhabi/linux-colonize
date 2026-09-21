#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#include "core/assets.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

static int write_file(const char* path, const char* content) {
  FILE* f = fopen(path, "wb");
  if (!f) {
    return 0;
  }
  fwrite(content, 1, strlen(content), f);
  fclose(f);
  return 1;
}

/* Shared setup: write GAME.TXT fixture and load it into `catalog`. Each
 * case calls this fresh rather than sharing a file-level catalog, so no
 * reset hook is needed between cases. */
static int setup(ColonizeMsgCatalog* catalog, const ColonizeMsgSection** section) {
  diag_init(0, NULL);
  const char* dir = "./test-assets-msg";
  mkdir(dir, 0755);

  char path[256];
  snprintf(path, sizeof(path), "%s/GAME.TXT", dir);
  if (!write_file(
        path,
        "; comment\r\n"
        "@BEGINMENU\r\n"
        "@width=160\r\n"
        "@options\r\n"
        "Menu Option New\r\n"
        "Menu Option Load\r\n"
      )) {
    return 0;
  }

  assets_msg_init(catalog);
  if (!assets_msg_load_file(catalog, path)) {
    return 0;
  }
  *section = assets_msg_find(catalog, "BEGINMENU");
  return 1;
}

static int case_beginmenu_line_count(void) {
  ColonizeMsgCatalog catalog;
  const ColonizeMsgSection* section = NULL;
  if (!setup(&catalog, &section)) {
    return 1;
  }
  if (!section || section->line_count < 3) {
    fprintf(stderr, "BEGINMENU parse failed count=%d\n", section ? section->line_count : -1);
    assets_msg_free(&catalog);
    return 1;
  }
  assets_msg_free(&catalog);
  diag_shutdown();
  return 0;
}

static int case_beginmenu_first_line(void) {
  ColonizeMsgCatalog catalog;
  const ColonizeMsgSection* section = NULL;
  if (!setup(&catalog, &section)) {
    return 1;
  }
  if (!section || section->line_count < 1 || strcmp(section->lines[0], "@width=160") != 0) {
    fprintf(stderr, "unexpected first line: %s\n", section ? section->lines[0] : "(none)");
    assets_msg_free(&catalog);
    return 1;
  }
  assets_msg_free(&catalog);
  diag_shutdown();
  return 0;
}

static const TestCase k_cases[] = {
    {"case_beginmenu_line_count", case_beginmenu_line_count},
    {"case_beginmenu_first_line", case_beginmenu_first_line},
};
TEST_MAIN(k_cases)
