#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "core/savegame.h"

int main(void) {
  const char* dir = "./test-saves-bad";
  const char* file_path = "./test-saves-bad/bad.sav";
  char err[256];
  ColonizeSavePayload payload = {0};

  if (mkdir(dir, 0755) != 0) {
    /* continue if exists */
  }
  FILE* f = fopen(file_path, "wb");
  if (!f) {
    fprintf(stderr, "failed to create malformed save\n");
    return 1;
  }
  /* Deliberately write a wrong header format. */
  fwrite("NOT_A_VALID_SAVE", 1, 16, f);
  fclose(f);

  if (savegame_read(dir, "bad", &payload, err, sizeof(err))) {
    fprintf(stderr, "expected incompatible save read to fail\n");
    return 1;
  }
  if (strstr(err, "Incompatible") == NULL && strstr(err, "header") == NULL) {
    fprintf(stderr, "unexpected error message: %s\n", err);
    return 1;
  }
  return 0;
}
