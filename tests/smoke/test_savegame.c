#include <stdio.h>
#include <string.h>

#include "core/savegame.h"

int main(void) {
  char err[256];
  ColonizeSavePayload wrote = {
    .turn_number = 42,
    .random_seed = 1337,
    .map_seed = 17
  };
  if (!savegame_write("./test-saves", "smoke", &wrote, err, sizeof(err))) {
    fprintf(stderr, "write failed: %s\n", err);
    return 1;
  }

  ColonizeSavePayload read = {0};
  if (!savegame_read("./test-saves", "smoke", &read, err, sizeof(err))) {
    fprintf(stderr, "read failed: %s\n", err);
    return 1;
  }

  if (memcmp(&wrote, &read, sizeof(wrote)) != 0) {
    fprintf(stderr, "roundtrip mismatch\n");
    return 1;
  }
  return 0;
}
