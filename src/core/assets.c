#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "core/assets.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static bool file_exists(const char* path) {
  struct stat st;
  return path && stat(path, &st) == 0;
}

bool assets_validate_required_files(const char* data_dir, char* err_buf, size_t err_buf_size) {
  static const char* required[] = {
    "MODULES.DB",
    "ERRORS.DB",
    "GAME.TXT",
    "MENU.TXT"
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
