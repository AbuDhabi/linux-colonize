#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#include "core/assets.h"
#include "platform/diagnostics.h"

static int write_file(const char* path, const char* content) {
  FILE* f = fopen(path, "wb");
  if (!f) {
    return 0;
  }
  fwrite(content, 1, strlen(content), f);
  fclose(f);
  return 1;
}

int main(void) {
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
        "Start a Game in NEW WORLD\r\n"
        "LOAD Game\r\n"
      )) {
    return 1;
  }

  ColonizeMsgCatalog catalog;
  assets_msg_init(&catalog);
  if (!assets_msg_load_file(&catalog, path)) {
    return 1;
  }
  const ColonizeMsgSection* section = assets_msg_find(&catalog, "BEGINMENU");
  if (!section || section->line_count < 3) {
    fprintf(stderr, "BEGINMENU parse failed count=%d\n", section ? section->line_count : -1);
    return 1;
  }
  if (strcmp(section->lines[0], "@width=160") != 0) {
    fprintf(stderr, "unexpected first line: %s\n", section->lines[0]);
    return 1;
  }

  assets_msg_free(&catalog);
  diag_shutdown();
  return 0;
}
