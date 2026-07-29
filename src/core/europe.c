#include "core/europe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static void europe_trim(char* s) {
  char* start = s;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != s) {
    memmove(s, start, strlen(start) + 1);
  }
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
    s[--n] = '\0';
  }
}

static bool europe_parse_int_field(const char** cursor, int* out) {
  while (**cursor == ' ' || **cursor == '\t' || **cursor == ',') {
    ++(*cursor);
  }
  if (**cursor == '\0') {
    return false;
  }
  char* end = NULL;
  long v = strtol(*cursor, &end, 10);
  if (end == *cursor) {
    return false;
  }
  *out = (int)v;
  *cursor = end;
  return true;
}

static void europe_set_status(EuropeScreen* eu, const char* text) {
  if (!eu) {
    return;
  }
  snprintf(eu->status, sizeof(eu->status), "%s", text ? text : "");
}

static bool europe_load_tables(EuropeScreen* eu, const ColonizeMsgCatalog* names) {
  eu->cargo_count = 0;
  eu->class_count = 0;

  const ColonizeMsgSection* cargo = assets_msg_find(names, "CARGO");
  if (cargo) {
    for (int i = 0; i < cargo->line_count && eu->cargo_count < EUROPE_CARGO_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", cargo->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      /* Skip non-tradeable end markers (Hammers, Crosses, …) — no commas with prices. */
      char* comma = strchr(line, ',');
      if (!comma) {
        continue;
      }
      *comma = '\0';
      europe_trim(line);
      if (line[0] == '\0') {
        continue;
      }

      const char* p = comma + 1;
      int start_lo = 0;
      int start_hi = 0;
      int low = 0;
      int high = 0;
      int burden = 0;
      int rise = 0;
      int fall = 0;
      int attrition = 0;
      int volatility = 0;
      if (!europe_parse_int_field(&p, &start_lo) || !europe_parse_int_field(&p, &start_hi) ||
          !europe_parse_int_field(&p, &low) || !europe_parse_int_field(&p, &high) ||
          !europe_parse_int_field(&p, &burden) || !europe_parse_int_field(&p, &rise) ||
          !europe_parse_int_field(&p, &fall) || !europe_parse_int_field(&p, &attrition) ||
          !europe_parse_int_field(&p, &volatility)) {
        continue;
      }
      (void)low;
      (void)high;
      (void)rise;
      (void)fall;
      (void)attrition;
      (void)volatility;

      EuropeCargoQuote* q = &eu->cargo[eu->cargo_count++];
      snprintf(q->name, sizeof(q->name), "%s", line);
      q->bid = start_lo;
      if (q->bid < 1) {
        q->bid = 1;
      }
      /* NAMES.TXT: burden 0 ⇒ ask is 1 higher than bid. */
      q->ask = q->bid + burden + 1;
    }
  }

  const ColonizeMsgSection* classes = assets_msg_find(names, "CLASS");
  if (classes) {
    for (int i = 0; i < classes->line_count && eu->class_count < EUROPE_CLASS_MAX; ++i) {
      char line[COLONIZE_MSG_LINE_LEN];
      snprintf(line, sizeof(line), "%s", classes->lines[i]);
      if (line[0] == ';' || line[0] == '\0') {
        continue;
      }
      char* comma = strchr(line, ',');
      if (!comma) {
        continue;
      }
      *comma = '\0';
      europe_trim(line);
      const char* p = comma + 1;
      int cost = 0;
      if (!europe_parse_int_field(&p, &cost) || cost <= 0 || line[0] == '\0') {
        continue;
      }
      EuropeRecruitClass* c = &eu->classes[eu->class_count++];
      snprintf(c->name, sizeof(c->name), "%s", line);
      c->cost = cost;
    }
  }

  return eu->cargo_count > 0;
}

void europe_reset_campaign(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  snprintf(eu->port_city, sizeof(eu->port_city), "%s", "London");
  snprintf(eu->nation_name, sizeof(eu->nation_name), "%s", "England");
  eu->gold = 1000;
  eu->tax_percent = 0;
  eu->harbor_ships = 0;
  eu->dock_count = 0;
  memset(eu->dock, 0, sizeof(eu->dock));
  /* Three free colonists already waiting — matches early-game dock feel. */
  static const char* starters[] = {"Free Colonist", "Free Colonist", "Indentured Servant"};
  for (int i = 0; i < 3 && i < EUROPE_DOCK_MAX; ++i) {
    snprintf(eu->dock[i].name, sizeof(eu->dock[i].name), "%s", starters[i]);
    eu->dock[i].present = true;
    eu->dock_count = i + 1;
  }
  europe_set_status(eu, "Home port ready. Recruit / Train / Esc.");
}

bool europe_load(EuropeScreen* eu, const char* data_dir, char* err, size_t err_size) {
  if (!eu || !data_dir) {
    snprintf(err, err_size, "europe_load bad args");
    return false;
  }
  memset(eu, 0, sizeof(*eu));

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "NAMES.TXT", names_path, sizeof(names_path)) ||
      !assets_msg_load_file(&names, names_path)) {
    snprintf(err, err_size, "failed to load NAMES.TXT for Europe market");
    assets_msg_free(&names);
    return false;
  }
  if (!europe_load_tables(eu, &names)) {
    snprintf(err, err_size, "NAMES.TXT missing usable @CARGO table");
    assets_msg_free(&names);
    return false;
  }
  assets_msg_free(&names);

  char pik_path[512];
  char pik_err[256];
  if (!dos_compat_normalize_asset_path(data_dir, "EUROPE.PIK", pik_path, sizeof(pik_path))) {
    snprintf(err, err_size, "EUROPE.PIK path resolve failed");
    return false;
  }
  if (!pik_load(pik_path, &eu->background, pik_err, sizeof(pik_err))) {
    snprintf(err, err_size, "EUROPE.PIK: %s", pik_err);
    return false;
  }
  eu->background_ok = true;

  europe_reset_campaign(eu);
  diag_info(
    "Europe screen loaded (%dx%d, %d cargo types, %d recruit classes)",
    eu->background.width,
    eu->background.height,
    eu->cargo_count,
    eu->class_count
  );
  return true;
}

void europe_free(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  pik_free(&eu->background);
  memset(eu, 0, sizeof(*eu));
}

bool europe_recruit(EuropeScreen* eu) {
  if (!eu) {
    return false;
  }
  if (eu->dock_count >= EUROPE_DOCK_MAX) {
    europe_set_status(eu, "Docks are full.");
    return false;
  }
  if (eu->class_count <= 0) {
    europe_set_status(eu, "No recruit classes loaded.");
    return false;
  }

  /* Cheapest class first (Petty Criminals in stock NAMES.TXT). */
  int best = 0;
  for (int i = 1; i < eu->class_count; ++i) {
    if (eu->classes[i].cost < eu->classes[best].cost) {
      best = i;
    }
  }
  const EuropeRecruitClass* cls = &eu->classes[best];
  if (eu->gold < cls->cost) {
    snprintf(eu->status, sizeof(eu->status), "Need %d$ for %s.", cls->cost, cls->name);
    return false;
  }

  eu->gold -= cls->cost;
  EuropeDockImmigrant* slot = &eu->dock[eu->dock_count++];
  snprintf(slot->name, sizeof(slot->name), "%s", cls->name);
  slot->present = true;
  snprintf(eu->status, sizeof(eu->status), "Recruited %s (-%d$).", cls->name, cls->cost);
  return true;
}

bool europe_pop_dock_immigrant(EuropeScreen* eu, char* out_name, size_t out_name_size) {
  if (!eu || eu->dock_count <= 0) {
    return false;
  }
  if (out_name && out_name_size > 0) {
    snprintf(out_name, out_name_size, "%s", eu->dock[0].name);
  }
  for (int i = 1; i < eu->dock_count; ++i) {
    eu->dock[i - 1] = eu->dock[i];
  }
  eu->dock_count--;
  eu->dock[eu->dock_count].present = false;
  eu->dock[eu->dock_count].name[0] = '\0';
  return true;
}

void europe_train_stub(EuropeScreen* eu) {
  europe_set_status(eu, "Train: not implemented yet.");
}

void europe_cheat_add_gold(EuropeScreen* eu, int amount) {
  if (!eu) {
    return;
  }
  eu->gold += amount;
  if (eu->gold < 0) {
    eu->gold = 0;
  }
  snprintf(eu->status, sizeof(eu->status), "Treasury now %d$.", eu->gold);
}

void europe_cheat_adjust_tax(EuropeScreen* eu, int delta) {
  if (!eu) {
    return;
  }
  eu->tax_percent += delta;
  if (eu->tax_percent < 0) {
    eu->tax_percent = 0;
  }
  if (eu->tax_percent > 100) {
    eu->tax_percent = 100;
  }
  snprintf(eu->status, sizeof(eu->status), "Tax rate %d%%.", eu->tax_percent);
}
