/*
 * Test-only: fixture name -> ColonizeUnitKind.
 *
 * The production binary carries no unit names (a unit type's identity is its
 * NAMES.TXT @UNIT row, stamped by units_load_types). Many fixtures still
 * hand-build a ColonizeUnitPool out of a few named types at arbitrary slots;
 * this table — formerly units.c's k_units_kind_names — lets those resolve,
 * and is registered before main() in every test binary (colonize_add_test
 * appends this file).
 */
#include <stddef.h>
#include <string.h>

#include "core/colony.h"
#include "core/units.h"

static const struct {
  const char* key;
  ColonizeUnitKind kind;
} k_units_kind_names[] = {
  {"Cont. Cav", UNITS_KIND_CONT_CAV},
  {"Continental Cav", UNITS_KIND_CONT_CAV},
  {"Cont. Army", UNITS_KIND_CONT_ARMY},
  {"Continental Army", UNITS_KIND_CONT_ARMY},
  {"Regular", UNITS_KIND_REGULAR},
  {"Cavalry", UNITS_KIND_CAVALRY},
  {"Cav.", UNITS_KIND_CAVALRY},
  {"Man-O-War", UNITS_KIND_MAN_O_WAR},
  {"Man-o-War", UNITS_KIND_MAN_O_WAR},
  {"Man O War", UNITS_KIND_MAN_O_WAR},
  {"Man of War", UNITS_KIND_MAN_O_WAR},
  {"Man-O'-War", UNITS_KIND_MAN_O_WAR},
  {"Man-o'-War", UNITS_KIND_MAN_O_WAR},
  {"Merchantman", UNITS_KIND_MERCHANTMAN},
  {"Galleon", UNITS_KIND_GALLEON},
  {"Privateer", UNITS_KIND_PRIVATEER},
  {"Frigate", UNITS_KIND_FRIGATE},
  {"Caravel", UNITS_KIND_CARAVEL},
  {"Treasure", UNITS_KIND_TREASURE},
  {"Artillery", UNITS_KIND_ARTILLERY},
  {"Cannon", UNITS_KIND_ARTILLERY},
  {"Wagon", UNITS_KIND_WAGON},
  {"Mtd. Warrior", UNITS_KIND_MTD_WARRIOR},
  {"Mtd Warrior", UNITS_KIND_MTD_WARRIOR},
  {"Mounted Warrior", UNITS_KIND_MTD_WARRIOR},
  {"Mtd. Brave", UNITS_KIND_MTD_BRAVE},
  {"Mtd Brave", UNITS_KIND_MTD_BRAVE},
  {"Mounted Brave", UNITS_KIND_MTD_BRAVE},
  {"Armed Brave", UNITS_KIND_ARMED_BRAVE},
  {"Brave", UNITS_KIND_BRAVE},
  {"Dragoon", UNITS_KIND_DRAGOON},
  {"Scout", UNITS_KIND_SCOUT},
  {"Pioneer", UNITS_KIND_PIONEER},
  {"Hardy", UNITS_KIND_PIONEER},
  {"Missionar", UNITS_KIND_MISSIONARY},
  {"Mission", UNITS_KIND_MISSIONARY},
  {"Jesuit", UNITS_KIND_MISSIONARY},
  {"Soldier", UNITS_KIND_SOLDIER},
  {"Colonist", UNITS_KIND_COLONIST},
  /* Last resort: the abbreviated WoI/King spellings the equip ladders used. */
  {"Cav", UNITS_KIND_CAVALRY},
  {"Army", UNITS_KIND_CONT_ARMY},
};

static ColonizeUnitKind test_name_kind(const char* name) {
  for (size_t i = 0; i < sizeof(k_units_kind_names) / sizeof(k_units_kind_names[0]); ++i) {
    if (strstr(name, k_units_kind_names[i].key) != NULL) {
      return k_units_kind_names[i].kind;
    }
  }
  return UNITS_KIND_UNKNOWN;
}


/* Fixture building name -> NAMES.TXT @BUILDING row (exact match). */
static const struct {
  const char* name;
  int row;
} k_building_rows[] = {
  {"Stockade", 0},
  {"Fort", 1},
  {"Fortress", 2},
  {"Armory", 3},
  {"Magazine", 4},
  {"Arsenal", 5},
  {"Docks", 6},
  {"Drydock", 7},
  {"Shipyard", 8},
  {"Town Hall", 9},
  {"Schoolhouse", 12},
  {"College", 13},
  {"University", 14},
  {"Warehouse", 15},
  {"Warehouse Expansion", 16},
  {"Stable", 17},
  {"Custom House", 18},
  {"Printing Press", 19},
  {"Newspaper", 20},
  {"Weaver's House", 21},
  {"Weaver's Shop", 22},
  {"Textile Mill", 23},
  {"Tobacconist's House", 24},
  {"Tobacconist's Shop", 25},
  {"Cigar Factory", 26},
  {"Rum Distiller's House", 27},
  {"Rum Distillery", 28},
  {"Rum Factory", 29},
  {"Capitol", 30},
  {"Capitol Expansion", 31},
  {"Fur Trader's House", 32},
  {"Fur Trading Post", 33},
  {"Fur Factory", 34},
  {"Carpenter's Shop", 35},
  {"Lumber Mill", 36},
  {"Church", 37},
  {"Cathedral", 38},
  {"Blacksmith's House", 39},
  {"Blacksmith's Shop", 40},
  {"Iron Works", 41},
};

static int test_building_row(const char* name) {
  for (size_t i = 0; i < sizeof(k_building_rows) / sizeof(k_building_rows[0]); ++i) {
    if (strcmp(name, k_building_rows[i].name) == 0) {
      return k_building_rows[i].row;
    }
  }
  return -1;
}

static const char* test_building_name(int row) {
  for (size_t i = 0; i < sizeof(k_building_rows) / sizeof(k_building_rows[0]); ++i) {
    if (k_building_rows[i].row == row) {
      return k_building_rows[i].name;
    }
  }
  return NULL;
}

/* Weak: a few test binaries do not link the units module at all. */
extern void units_set_name_kind_resolver(UnitsNameKindResolver fn) __attribute__((weak));
extern void colonies_set_building_name_row_resolver(ColoniesBuildingNameRowResolver fn)
  __attribute__((weak));
extern void colonies_set_building_row_name_resolver(ColoniesBuildingRowNameResolver fn)
  __attribute__((weak));

/* The display-name accessors have no built-in copy to fall back on, so every
 * test binary reads the real NAMES.TXT / LABELS.TXT (cwd = repo root). */
extern void reports_names_load_catalogs(const char* data_dir) __attribute__((weak));

__attribute__((constructor)) static void test_name_kinds_install(void) {
  if (reports_names_load_catalogs) {
    reports_names_load_catalogs("COLONIZE");
  }
  if (units_set_name_kind_resolver) {
    units_set_name_kind_resolver(test_name_kind);
  }
  if (colonies_set_building_name_row_resolver) {
    colonies_set_building_name_row_resolver(test_building_row);
  }
  if (colonies_set_building_row_name_resolver) {
    colonies_set_building_row_name_resolver(test_building_name);
  }
}
