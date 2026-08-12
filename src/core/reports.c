#include "core/reports.h"

#include <stdio.h>
#include <string.h>

#include "platform/diagnostics.h"

static const char* k_report_files[COLONIZE_REPORT_COUNT] = {
  "REPORT2.PIK", /* Religious Adviser */
  "CCBKGD.PIK",  /* Continental Congress */
  "REPORT4.PIK", /* Labor Adviser */
  "REPORT5.PIK", /* Economic Adviser */
  "REPORT6.PIK", /* Colony Adviser */
  "REPORT7.PIK", /* Naval Adviser */
  "REPORT8.PIK", /* Foreign Affairs */
  "REPORT9.PIK", /* Indian Adviser */
  "WOODPANL.PIK" /* Colonization Score — full-screen wood */
};

static const char* k_report_titles[COLONIZE_REPORT_COUNT] = {
  "RELIGIOUS ADVISER REPORT",
  "CONTINENTAL CONGRESS ACTIVITIES",
  "LABOR ADVISER REPORT",
  "ECONOMIC ADVISER REPORT",
  "COLONY ADVISER REPORT",
  "NAVAL ADVISER REPORT",
  "FOREIGN AFFAIRS REPORT",
  "INDIAN ADVISER REPORT",
  "COLONIZATION SCORE"
};

/* NAMES.TXT @FATHERS order. */
static const char* k_ff_names[COLONIZE_COL1_FF_COUNT] = {
  "Adam Smith",
  "Jakob Fugger",
  "Peter Minuit",
  "Peter Stuyvesant",
  "Jan de Witt",
  "Ferdinand Magellan",
  "Francisco Coronado",
  "Hernando de Soto",
  "Henry Hudson",
  "Sieur De La Salle",
  "Hernan Cortes",
  "George Washington",
  "Paul Revere",
  "Francis Drake",
  "John Paul Jones",
  "Thomas Jefferson",
  "Pocahontas",
  "Thomas Paine",
  "Simon Bolivar",
  "Benjamin Franklin",
  "William Brewster",
  "William Penn",
  "Jean de Brebeuf",
  "Juan de Sepulveda",
  "Bartolome de las Casas"
};

/* NAMES.TXT @JOB column 2 (recruit / specialty display names). */
static const char* k_job_names[] = {
  "Expert Farmers",
  "Master Sugar Planters",
  "Master Tobacco Planters",
  "Master Cotton Planters",
  "Expert Fur Trappers",
  "Expert Lumberjacks",
  "Expert Ore Miners",
  "Expert Silver Miners",
  "Expert Fishermen",
  "Master Distiller",
  "Master Tobacconists",
  "Master Weavers",
  "Master Fur Traders",
  "Master Carpenters",
  "Master Blacksmiths",
  "Master Gunsmiths",
  "Firebrand Preachers",
  "Elder Statesmen",
  "Expert Teachers",
  "Free Colonists",
  "Hardy Pioneers",
  "Veteran Soldiers",
  "Seasoned Scouts",
  "Veteran Dragoons",
  "Jesuit Missionaries",
  "Indentured Servants",
  "Petty Criminals",
  "Indian Converts"
};
static const int k_job_count = (int)(sizeof(k_job_names) / sizeof(k_job_names[0]));

static const char* k_cargo_names[COLONIZE_COL1_CARGO_TYPES] = {
  "Food",
  "Sugar",
  "Tobacco",
  "Cotton",
  "Furs",
  "Lumber",
  "Ore",
  "Silver",
  "Horses",
  "Rum",
  "Cigars",
  "Cloth",
  "Coats",
  "Trade Goods",
  "Tools",
  "Muskets"
};

static const char* k_tribe_names[COLONIZE_COL1_INDIAN_COUNT] = {
  "Inca", "Aztec", "Arawak", "Iroquois", "Cherokee", "Apache", "Sioux", "Tupi"
};

static const char* k_tribe_levels[] = {"Semi-Nomadic", "Agrarian", "Advanced", "Civilized"};

static const char* k_attitudes[] = {"Content", "Uneasy", "Restless", "Angry", "War"};

static const char* k_euro_short[COLONIZE_COL1_NATION_COUNT] = {
  "English", "French", "Spanish", "Dutch"
};

void reports_init(ColonizeReportsView* view) {
  if (!view) {
    return;
  }
  memset(view, 0, sizeof(*view));
  view->active = COLONIZE_REPORT_RELIGIOUS;
}

void reports_free(ColonizeReportsView* view) {
  if (!view) {
    return;
  }
  for (int i = 0; i < COLONIZE_REPORT_COUNT; ++i) {
    pik_free(&view->backgrounds[i]);
  }
  memset(view, 0, sizeof(*view));
}

bool reports_load(ColonizeReportsView* view, const char* data_dir, char* err, size_t err_size) {
  if (!view || !data_dir) {
    if (err && err_size) {
      snprintf(err, err_size, "reports_load bad args");
    }
    return false;
  }
  reports_free(view);
  reports_init(view);

  int ok_count = 0;
  for (int i = 0; i < COLONIZE_REPORT_COUNT; ++i) {
    char path[512];
    char pik_err[256];
    if (!dos_compat_normalize_asset_path(data_dir, k_report_files[i], path, sizeof(path))) {
      diag_warn("Report background path failed: %s", k_report_files[i]);
      continue;
    }
    if (!pik_load(path, &view->backgrounds[i], pik_err, sizeof(pik_err))) {
      diag_warn("Failed to load %s: %s", k_report_files[i], pik_err);
      continue;
    }
    view->background_ok[i] = true;
    ok_count++;
  }
  view->loaded = ok_count > 0;
  if (!view->loaded) {
    snprintf(err, err_size, "no report backgrounds loaded");
    return false;
  }
  diag_info("Report screens loaded (%d/%d backgrounds)", ok_count, COLONIZE_REPORT_COUNT);
  return true;
}

const char* reports_title(ColonizeReportId id) {
  if (id < 0 || id >= COLONIZE_REPORT_COUNT) {
    return "REPORT";
  }
  return k_report_titles[id];
}

const char* reports_background_name(ColonizeReportId id) {
  if (id < 0 || id >= COLONIZE_REPORT_COUNT) {
    return "";
  }
  return k_report_files[id];
}

bool reports_id_from_fkey(int fkey_number, ColonizeReportId* out_id) {
  if (fkey_number < 2 || fkey_number > 10 || !out_id) {
    return false;
  }
  *out_id = (ColonizeReportId)(fkey_number - 2);
  return true;
}

static void reports_draw_line(
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int x,
  int y,
  const char* text,
  uint8_t color
) {
  font_draw_text(font, fb, x, y, text, color);
}

static int reports_line_step(const ColonizeFont* font) {
  const int h = font ? (font->max_height + 2) : 8;
  return h < 8 ? 8 : h;
}

static void reports_render_body_start(
  const ColonizeReportsView* view,
  ColonizeReportId id,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* out_y
) {
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (view && view->background_ok[id]) {
    pik_blit(&view->backgrounds[id], fb, 0, 0);
  }

  reports_draw_line(font, fb, 8, 4, reports_title(id), 15);
  reports_draw_line(font, fb, 8, 4 + reports_line_step(font), "Esc returns to map", 14);
  *out_y = 4 + reports_line_step(font) * 2 + 4;
}

static int reports_clamp_nation(int human_nation) {
  if (human_nation < 0 || human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return 0;
  }
  return human_nation;
}

static const char* reports_job_name(int job) {
  if (job < 0 || job >= k_job_count) {
    return "Colonist";
  }
  return k_job_names[job];
}

static const char* reports_ff_name(int idx) {
  if (idx < 0 || idx >= (int)COLONIZE_COL1_FF_COUNT) {
    return "(none)";
  }
  return k_ff_names[idx];
}

static bool reports_ff_joined(int8_t status) {
  return status > 0;
}

static const char* reports_attitude_from_alarm(unsigned alarm) {
  /* Rough bands matching @ATTITUDE labels Content..War. */
  if (alarm <= 2) {
    return k_attitudes[0];
  }
  if (alarm <= 5) {
    return k_attitudes[1];
  }
  if (alarm <= 8) {
    return k_attitudes[2];
  }
  if (alarm <= 11) {
    return k_attitudes[3];
  }
  return k_attitudes[4];
}

static const char* reports_tribe_level(uint8_t tech) {
  if (tech > 3) {
    tech = 3;
  }
  return k_tribe_levels[tech];
}

static bool reports_unit_in_europe(int x, int y) {
  return x >= 200 || y >= 200;
}

static int reports_colony_rebel_pct(const ColonizeCol1Colony* c) {
  if (!c || c->rebel_divisor == 0) {
    return 0;
  }
  return (int)((c->rebel_dividend * 100u) / c->rebel_divisor);
}

static void reports_render_religious(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "Immigration / religious unrest", 15);
  *y += step;

  if (col1) {
    const ColonizeCol1Nation* nat = &col1->nation[human];
    snprintf(
      line,
      line_sz,
      "Crosses: %u / %u  (toward next immigrant)",
      (unsigned)nat->current_crosses,
      (unsigned)nat->needed_crosses
    );
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;

    const unsigned need = nat->needed_crosses > nat->current_crosses
                            ? (unsigned)(nat->needed_crosses - nat->current_crosses)
                            : 0u;
    snprintf(line, line_sz, "Crosses still needed: %u", need);
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;

    reports_draw_line(font, fb, 8, *y, "Recruitment pool:", 15);
    *y += step;
    for (int i = 0; i < 3; ++i) {
      snprintf(line, line_sz, "  %d. %s", i + 1, reports_job_name(nat->recruit[i]));
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
    }

    int churches = 0;
    int cathedrals = 0;
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if (c->nation_id != (uint8_t)human) {
        continue;
      }
      if (c->buildings.church >= 2) {
        cathedrals++;
      } else if (c->buildings.church >= 1) {
        churches++;
      }
    }
    snprintf(line, line_sz, "Churches: %d   Cathedrals: %d", churches, cathedrals);
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
  } else {
    reports_draw_line(font, fb, 8, *y, "Cross production: (no Col1 save loaded)", 14);
    *y += step;
  }

  const int dock = europe ? europe->dock_count : 0;
  snprintf(line, line_sz, "Immigrants waiting on docks: %d", dock);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  if (europe && europe->dock_count > 0) {
    for (int i = 0; i < europe->dock_count && i < 6 && *y < 180; ++i) {
      if (!europe->dock[i].present) {
        continue;
      }
      snprintf(line, line_sz, "  Dock: %s", europe->dock[i].name);
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
    }
  }

  if (!col1 && colonies) {
    *y += 2;
    reports_draw_line(font, fb, 8, *y, "Build church/cathedral to speed immigration.", 14);
  }
}

static void reports_render_congress(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  if (!col1) {
    reports_draw_line(font, fb, 8, *y, "Founding Fathers (no Col1 save loaded)", 14);
    *y += step;
    reports_draw_line(font, fb, 8, *y, "Members in Congress: 0", 15);
    *y += step;
    reports_draw_line(font, fb, 8, *y, "Now debating: (none)", 15);
    return;
  }

  const ColonizeCol1Nation* nat = &col1->nation[human];
  snprintf(
    line,
    line_sz,
    "Year %u%s   Turn %u",
    (unsigned)col1->head.year,
    col1->head.autumn ? " Autumn" : " Spring",
    (unsigned)col1->head.turn
  );
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step;

  /* FUN_3f41_0618 congress report includes tax rate. */
  snprintf(line, line_sz, "Tax rate: %u%%", (unsigned)nat->tax_rate);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;

  snprintf(
    line,
    line_sz,
    "Liberty bells: %u total  (+%u last turn)",
    (unsigned)nat->liberty_bells_total,
    (unsigned)nat->liberty_bells_last_turn
  );
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;

  /* Average rebel sentiment across human colonies. */
  int rebel_sum = 0;
  int rebel_n = 0;
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &col1->colony[i];
    if (c->nation_id != (uint8_t)human || c->population == 0) {
      continue;
    }
    rebel_sum += reports_colony_rebel_pct(c);
    rebel_n++;
  }
  if (rebel_n > 0) {
    snprintf(line, line_sz, "Rebel sentiment (avg): %d%%", rebel_sum / rebel_n);
  } else {
    snprintf(line, line_sz, "Rebel sentiment: n/a (no colonies)");
  }
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;

  if (nat->next_founding_father >= 0) {
    snprintf(line, line_sz, "Now debating: %s", reports_ff_name(nat->next_founding_father));
  } else {
    snprintf(line, line_sz, "Now debating: (none)");
  }
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;

  int members = 0;
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if (reports_ff_joined(col1->head.founding_father[i])) {
      members++;
    }
  }
  snprintf(line, line_sz, "Founding Fathers in Congress: %d", members);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;

  if (members == 0) {
    reports_draw_line(font, fb, 8, *y, "  (none yet — produce liberty bells)", 14);
    *y += step;
  } else {
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT && *y < 160; ++i) {
      if (!reports_ff_joined(col1->head.founding_father[i])) {
        continue;
      }
      snprintf(line, line_sz, "  %s", reports_ff_name(i));
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
    }
  }

  snprintf(
    line,
    line_sz,
    "Expeditionary Force: %u reg  %u drag  %u MoW  %u art",
    (unsigned)col1->head.expeditionary_force[0],
    (unsigned)col1->head.expeditionary_force[1],
    (unsigned)col1->head.expeditionary_force[2],
    (unsigned)col1->head.expeditionary_force[3]
  );
  reports_draw_line(font, fb, 8, *y, line, 15);
}

static void reports_render_labor(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "Colonists by profession", 15);
  *y += step;
  reports_draw_line(font, fb, 8, *y, "(Click on item to zoom — not wired)", 14);
  *y += step;

  int counts[64];
  memset(counts, 0, sizeof(counts));
  int total = 0;

  if (col1) {
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if (c->nation_id != (uint8_t)human) {
        continue;
      }
      const int pop = c->population > COLONIZE_COL1_COLONY_POP_MAX ? COLONIZE_COL1_COLONY_POP_MAX
                                                                   : (int)c->population;
      for (int p = 0; p < pop; ++p) {
        int job = c->profession[p];
        if (job < 0) {
          job = 0;
        }
        if (job >= 64) {
          job = 63;
        }
        counts[job]++;
        total++;
      }
    }
    /* Land units outside colonies (same nation). */
    for (uint16_t i = 0; i < col1->head.unit_count; ++i) {
      const ColonizeCol1Unit* u = &col1->unit[i];
      if ((int)u->nation_id != human) {
        continue;
      }
      if (u->type >= 13 && u->type <= 18) {
        continue; /* ships */
      }
      if (reports_unit_in_europe(u->x, u->y)) {
        continue;
      }
      int job = u->profession;
      if (job < 0 || job >= 64) {
        job = u->type < 64 ? u->type : 0;
      }
      counts[job]++;
      total++;
    }
  } else if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active) {
        continue;
      }
      for (int p = 0; p < c->colonist_count; ++p) {
        const ColonizeColonist* col = &c->colonists[p];
        if (!col->active) {
          continue;
        }
        int t = col->unit_type_index;
        if (t < 0) {
          t = 0;
        }
        if (t >= 64) {
          t = 63;
        }
        counts[t]++;
        total++;
      }
    }
  }

  snprintf(line, line_sz, "Total colonists: %d", total);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;

  if (total == 0) {
    reports_draw_line(font, fb, 8, *y, "No colonists in play yet.", 14);
    return;
  }

  for (int t = 0; t < 64 && *y < 185; ++t) {
    if (counts[t] <= 0) {
      continue;
    }
    const char* name = NULL;
    if (col1) {
      name = reports_job_name(t);
    } else if (units) {
      const ColonizeUnitType* ut = units_type(units, t);
      name = ut ? ut->name : "Unknown";
    } else {
      name = "Unknown";
    }
    snprintf(line, line_sz, "  %s: %d", name, counts[t]);
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
  }
}

static void reports_render_economic(
  const ColonizeCol1Save* col1,
  int human,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "Treasury and European trade", 15);
  *y += step;

  const uint32_t gold = col1 ? col1->nation[human].gold : (uint32_t)(europe ? europe->gold : 0);
  const int tax = col1 ? (int)col1->nation[human].tax_rate : (europe ? europe->tax_percent : 0);
  snprintf(line, line_sz, "Gold: %u    Tax rate: %d%%", (unsigned)gold, tax);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;

  if (col1) {
    const ColonizeCol1Nation* nat = &col1->nation[human];
    if (nat->boycott_bitmap != 0) {
      reports_draw_line(font, fb, 8, *y, "Boycotts:", 15);
      *y += step;
      for (int c = 0; c < (int)COLONIZE_COL1_CARGO_TYPES && *y < 100; ++c) {
        if ((nat->boycott_bitmap & (1u << c)) == 0) {
          continue;
        }
        snprintf(line, line_sz, "  %s", k_cargo_names[c]);
        reports_draw_line(font, fb, 8, *y, line, 15);
        *y += step;
      }
    } else {
      reports_draw_line(font, fb, 8, *y, "Boycotts: (none)", 14);
      *y += step;
    }

    reports_draw_line(font, fb, 8, *y, "Trade ledger (tons / gold):", 15);
    *y += step;
    int shown = 0;
    for (int c = 0; c < (int)COLONIZE_COL1_CARGO_TYPES && *y < 175; ++c) {
      const int32_t tons = nat->trade.tons[c];
      const int32_t g = nat->trade.gold[c];
      if (tons == 0 && g == 0) {
        continue;
      }
      snprintf(
        line,
        line_sz,
        "  %-12s  tons %d  gold %d",
        k_cargo_names[c],
        (int)tons,
        (int)g
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      shown++;
    }
    if (shown == 0) {
      reports_draw_line(font, fb, 8, *y, "  (no cargo traded yet)", 14);
      *y += step;
    }

    reports_draw_line(font, fb, 8, *y, "Europe prices (bid):", 15);
    *y += step;
    for (int c = 0; c < 8 && *y < 190; ++c) {
      snprintf(
        line,
        line_sz,
        "  %-12s  %u",
        k_cargo_names[c],
        (unsigned)nat->trade.euro_price[c]
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
    }
  } else if (europe && europe->cargo_count > 0) {
    reports_draw_line(font, fb, 8, *y, "Europe market (bid/ask):", 15);
    *y += step;
    for (int i = 0; i < europe->cargo_count && i < 10 && *y < 180; ++i) {
      snprintf(
        line,
        line_sz,
        "  %s  %d / %d",
        europe->cargo[i].name,
        europe->cargo[i].bid,
        europe->cargo[i].ask
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
    }
  }
}

static void reports_render_colony(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "Colony warehouses / status", 15);
  *y += step;
  reports_draw_line(font, fb, 8, *y, "(Click on item to zoom — not wired)", 14);
  *y += step;

  int totals[COLONIZE_COL1_CARGO_TYPES];
  memset(totals, 0, sizeof(totals));
  int n = 0;

  if (col1) {
    for (uint16_t i = 0; i < col1->head.colony_count && *y < 150; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if (c->nation_id != (uint8_t)human) {
        continue;
      }
      snprintf(
        line,
        line_sz,
        "%s (%u,%u) pop %u  rebel %d%%",
        c->name,
        (unsigned)c->x,
        (unsigned)c->y,
        (unsigned)c->population,
        reports_colony_rebel_pct(c)
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      snprintf(
        line,
        line_sz,
        "  food %u  lumber %u  tools %u  muskets %u  horses %u",
        (unsigned)c->stock[0],
        (unsigned)c->stock[5],
        (unsigned)c->stock[14],
        (unsigned)c->stock[15],
        (unsigned)c->stock[8]
      );
      reports_draw_line(font, fb, 8, *y, line, 14);
      *y += step;
      for (int g = 0; g < (int)COLONIZE_COL1_CARGO_TYPES; ++g) {
        totals[g] += (int)c->stock[g];
      }
      n++;
    }
  } else if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX && *y < 160; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active) {
        continue;
      }
      snprintf(
        line,
        line_sz,
        "%s (%d,%d) pop %d",
        c->name,
        c->x,
        c->y,
        c->population
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      snprintf(
        line,
        line_sz,
        "  food %d  lumber %d  tools %d  muskets %d  horses %d",
        c->stock[COLONIZE_CARGO_FOOD],
        c->stock[COLONIZE_CARGO_LUMBER],
        c->stock[COLONIZE_CARGO_TOOLS],
        c->stock[COLONIZE_CARGO_MUSKETS],
        c->stock[COLONIZE_CARGO_HORSES]
      );
      reports_draw_line(font, fb, 8, *y, line, 14);
      *y += step;
      for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
        totals[g] += c->stock[g];
      }
      n++;
    }
  }

  if (n == 0) {
    reports_draw_line(font, fb, 8, *y, "No colonies founded.", 14);
    return;
  }

  snprintf(line, line_sz, "Total colonies: %d", n);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  reports_draw_line(font, fb, 8, *y, "Warehouse totals:", 15);
  *y += step;
  for (int g = 0; g < (int)COLONIZE_COL1_CARGO_TYPES && *y < 195; ++g) {
    if (totals[g] <= 0) {
      continue;
    }
    snprintf(line, line_sz, "  %s: %d", k_cargo_names[g], totals[g]);
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
  }
}

static void reports_render_naval(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeUnitPool* units,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "Ships — cargo / location / destination", 15);
  *y += step;

  int sea = 0;

  if (col1) {
    for (uint16_t i = 0; i < col1->head.unit_count && *y < 180; ++i) {
      const ColonizeCol1Unit* u = &col1->unit[i];
      if ((int)u->nation_id != human) {
        continue;
      }
      if (u->type < 13 || u->type > 18) {
        continue;
      }
      const char* ship_name = "Ship";
      if (units) {
        const ColonizeUnitType* ut = units_type(units, u->type);
        if (ut) {
          ship_name = ut->name;
        }
      }
      const char* loc = reports_unit_in_europe(u->x, u->y) ? "Off Mapboard (Europe)" : "On Mapboard";
      if (u->orders == 0 && u->goto_x == 0xFF) {
        snprintf(
          line,
          line_sz,
          "  %s  %s (%u,%u)  holds %u",
          ship_name,
          loc,
          (unsigned)u->x,
          (unsigned)u->y,
          (unsigned)u->holds_occupied
        );
      } else {
        snprintf(
          line,
          line_sz,
          "  %s  %s (%u,%u)  holds %u  dest (%u,%u)",
          ship_name,
          loc,
          (unsigned)u->x,
          (unsigned)u->y,
          (unsigned)u->holds_occupied,
          (unsigned)u->goto_x,
          (unsigned)u->goto_y
        );
      }
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      sea++;
    }
  } else if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX && *y < 170; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!u->active || !units_is_sea(units, u->id)) {
        continue;
      }
      if (u->nation_id >= 0 && u->nation_id < 4 && u->nation_id != human) {
        continue;
      }
      const ColonizeUnitType* ut = units_type(units, u->type_index);
      const char* loc =
        reports_unit_in_europe(u->x, u->y) ? "Off Mapboard (Europe)" : "On Mapboard";
      snprintf(
        line,
        line_sz,
        "  %s  %s (%d,%d)  hold %d",
        ut ? ut->name : "Ship",
        loc,
        u->x,
        u->y,
        u->cargo_count
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      sea++;
    }
  }

  if (europe) {
    snprintf(line, line_sz, "Europe harbor ships: %d", europe->harbor_ships);
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
    for (int i = 0; i < europe->harbor_ships && *y < 190; ++i) {
      snprintf(
        line,
        line_sz,
        "  Harbor: %s (+%d passengers)",
        europe->harbor[i].name,
        europe->harbor[i].cargo_count
      );
      reports_draw_line(font, fb, 8, *y, line, 15);
      *y += step;
      sea++;
    }
  }

  if (sea == 0) {
    reports_draw_line(font, fb, 8, *y, "No ships in play.", 14);
  }
}

static void reports_count_nation_forces(
  const ColonizeCol1Save* col1,
  int nation,
  int* out_colonies,
  int* out_pop,
  int* out_military,
  int* out_naval,
  int* out_merchant
) {
  int colonies = 0;
  int pop = 0;
  int military = 0;
  int naval = 0;
  int merchant = 0;
  if (!col1) {
    *out_colonies = 0;
    *out_pop = 0;
    *out_military = 0;
    *out_naval = 0;
    *out_merchant = 0;
    return;
  }
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    if (col1->colony[i].nation_id == (uint8_t)nation) {
      colonies++;
      pop += col1->colony[i].population;
    }
  }
  for (uint16_t i = 0; i < col1->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &col1->unit[i];
    if ((int)u->nation_id != nation) {
      continue;
    }
    if (u->type >= 13 && u->type <= 18) {
      if (u->type == 13 || u->type == 14 || u->type == 15) {
        merchant++;
      } else {
        naval++;
      }
    } else if (u->type == 1 || u->type == 4 || u->type == 6 || u->type == 7 || u->type == 8 ||
               u->type == 9 || u->type == 11) {
      military++;
    }
  }
  *out_colonies = colonies;
  *out_pop = pop;
  *out_military = military;
  *out_naval = naval;
  *out_merchant = merchant;
}

static void reports_render_foreign(
  const ColonizeCol1Save* col1,
  int human,
  const EuropeScreen* europe,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "European rivals", 15);
  *y += step;

  if (!col1) {
    reports_draw_line(font, fb, 8, *y, "Other powers require a loaded Col1 save.", 14);
    *y += step;
    if (europe) {
      snprintf(line, line_sz, "Your nation: %s", europe->nation_name);
      reports_draw_line(font, fb, 8, *y, line, 15);
    }
    return;
  }

  const bool detailed = reports_ff_joined(col1->head.founding_father[4]); /* Jan de Witt */
  if (!detailed) {
    reports_draw_line(
      font, fb, 8, *y, "Detailed strength unlocks with Jan de Witt.", 14
    );
    *y += step;
  }

  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT && *y < 170; ++n) {
    const ColonizeCol1Player* p = &col1->player[n];
    const char* ctrl =
      p->control == 0 ? "Player" : (p->control == 2 ? "Withdrawn" : "AI");
    int colonies = 0, pop = 0, mil = 0, nav = 0, mer = 0;
    reports_count_nation_forces(col1, n, &colonies, &pop, &mil, &nav, &mer);
    const int avg = colonies > 0 ? pop / colonies : 0;

    snprintf(
      line,
      line_sz,
      "%s%s (%s)",
      p->country_name[0] ? p->country_name : k_euro_short[n],
      n == human ? " *" : "",
      ctrl
    );
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;

    if (detailed || n == human) {
      snprintf(
        line,
        line_sz,
        "  colonies %d  pop %d  avg %d  mil %d  naval %d  merchants %d",
        colonies,
        pop,
        avg,
        mil,
        nav,
        mer
      );
      reports_draw_line(font, fb, 8, *y, line, 14);
      *y += step;
    } else {
      snprintf(line, line_sz, "  colonies founded: %u", (unsigned)p->founded_colonies);
      reports_draw_line(font, fb, 8, *y, line, 14);
      *y += step;
    }
  }

  snprintf(
    line,
    line_sz,
    "Royal Expeditionary Force: %u/%u/%u/%u",
    (unsigned)col1->head.expeditionary_force[0],
    (unsigned)col1->head.expeditionary_force[1],
    (unsigned)col1->head.expeditionary_force[2],
    (unsigned)col1->head.expeditionary_force[3]
  );
  reports_draw_line(font, fb, 8, *y, line, 15);
}

static void reports_render_indian(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  reports_draw_line(font, fb, 8, *y, "Native tribes contacted", 15);
  *y += step;

  if (!col1 || !col1->tribe) {
    reports_draw_line(font, fb, 8, *y, "Indian villages require a loaded Col1 save.", 14);
    return;
  }

  int shown = 0;
  for (int t = 0; t < (int)COLONIZE_COL1_INDIAN_COUNT && *y < 185; ++t) {
    const ColonizeCol1Indian* ind = &col1->indian[t];
    const uint8_t nation_id = (uint8_t)(t + 4);
    int villages = 0;
    int pop = 0;
    int missions = 0;
    int capitals = 0;
    int alarm_sum = 0;
    int alarm_n = 0;

    for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* tr = &col1->tribe[i];
      if (tr->nation_id != nation_id) {
        continue;
      }
      villages++;
      pop += tr->population;
      if (tr->mission != 0xFF) {
        missions++;
      }
      if (tr->state.capital) {
        capitals++;
      }
      if (human >= 0 && human < 4) {
        alarm_sum += tr->alarm[human].friction;
        alarm_n++;
      }
    }

    const bool met = ind->euro_diplo[human] != 0 || villages > 0;
    if (!met && villages == 0) {
      continue;
    }

    const unsigned alarm =
      ind->alarm_by_player[human] != 0
        ? (unsigned)ind->alarm_by_player[human]
        : (alarm_n > 0 ? (unsigned)(alarm_sum / alarm_n) : 0u);

    snprintf(
      line,
      line_sz,
      "%s (%s)  villages %d  pop %d",
      k_tribe_names[t],
      reports_tribe_level(ind->tech),
      villages,
      pop
    );
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
    snprintf(
      line,
      line_sz,
      "  %s  missions %d  capitals %d  alarm %u",
      reports_attitude_from_alarm(alarm),
      missions,
      capitals,
      alarm
    );
    reports_draw_line(font, fb, 8, *y, line, 14);
    *y += step;
    shown++;
  }

  if (shown == 0) {
    reports_draw_line(font, fb, 8, *y, "No tribes contacted yet.", 14);
  }
}

/* Unit type → default @JOB index when profession is out of range. */
static int reports_profession_from_unit_type(int type) {
  static const int k_map[] = {
    19, /* Colonists → Free Colonists */
    21, /* Soldiers */
    20, /* Pioneers */
    24, /* Missionaries */
    23, /* Dragoons */
    22, /* Scouts */
    21, /* Regulars → Veteran Soldiers */
    23, /* Cont. Cav. */
    23, /* Cavalry */
    21, /* Cont. Army */
    -1, /* Treasure */
    -1, /* Artillery */
    -1, /* Wagon Train */
    -1, /* Caravel … ships */
    -1,
    -1,
    -1,
    -1,
    -1
  };
  if (type < 0 || type >= (int)(sizeof(k_map) / sizeof(k_map[0]))) {
    return -1;
  }
  return k_map[type];
}

static bool reports_unit_type_is_scored_colonist(int type) {
  return reports_profession_from_unit_type(type) >= 0;
}

/* Manual schedule: criminal/servant +1, free/convert +2, skilled +4. */
static int reports_citizen_points_for_job(int job) {
  if (job == 25 || job == 26) {
    return 1; /* Indentured Servants, Petty Criminals */
  }
  if (job == 19 || job == 27) {
    return 2; /* Free Colonists, Indian Converts */
  }
  if (job >= 0 && job < k_job_count) {
    return 4; /* specialists / veterans / teachers / etc. */
  }
  return 2;
}

static int reports_resolve_job(int profession, int unit_type) {
  if (profession >= 0 && profession < k_job_count) {
    return profession;
  }
  if (unit_type >= 0) {
    const int fallback = reports_profession_from_unit_type(unit_type);
    if (fallback >= 0) {
      return fallback;
    }
  }
  return 19; /* Free Colonists */
}

static int reports_count_ff_for_nation(const ColonizeCol1Save* col1, int human) {
  int ff = 0;
  if (!col1) {
    return 0;
  }
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    /* Per-nation ownership: value stores the European nation id that elected them. */
    if (col1->head.founding_father[i] == (int8_t)human) {
      ff++;
    }
  }
  if (ff > 0) {
    return ff;
  }
  const uint16_t counted = col1->nation[human].founding_father_count;
  if (counted > 0 && counted <= COLONIZE_COL1_FF_COUNT) {
    return (int)counted;
  }
  /* Bitmask fallback in nation.founding_fathers[4]. */
  for (int b = 0; b < 4; ++b) {
    uint8_t bits = col1->nation[human].founding_fathers[b];
    while (bits) {
      ff += bits & 1u;
      bits >>= 1;
    }
  }
  return ff > (int)COLONIZE_COL1_FF_COUNT ? (int)COLONIZE_COL1_FF_COUNT : ff;
}

static int reports_rebel_sentiment_pct(const ColonizeCol1Save* col1, int human) {
  if (!col1) {
    return 0;
  }
  uint64_t weighted = 0;
  uint64_t pop = 0;
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    const ColonizeCol1Colony* c = &col1->colony[i];
    if (c->nation_id != (uint8_t)human || c->population == 0) {
      continue;
    }
    weighted += (uint64_t)reports_colony_rebel_pct(c) * (uint64_t)c->population;
    pop += c->population;
  }
  if (pop == 0) {
    return 0;
  }
  int pct = (int)(weighted / pop);
  if (pct < 0) {
    pct = 0;
  }
  if (pct > 100) {
    pct = 100;
  }
  return pct;
}

static int reports_foreign_recognition_pct(int prior_nations, bool achieved) {
  if (!achieved) {
    return 0;
  }
  if (prior_nations <= 0) {
    return 100;
  }
  if (prior_nations == 1) {
    return 50;
  }
  if (prior_nations == 2) {
    return 25;
  }
  return 0;
}

static int reports_early_revolution_pct(bool declared, int declare_year) {
  if (!declared || declare_year <= 0 || declare_year >= 1780) {
    return 0;
  }
  return 1780 - declare_year;
}

void reports_compute_score(
  ColonizeScoreBreakdown* out,
  const ColonizeCol1Save* col1,
  int human_nation,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe
) {
  memset(out, 0, sizeof(*out));
  const int human = reports_clamp_nation(human_nation);

  if (col1) {
    out->year = (int)col1->head.year;
    out->difficulty = (int)col1->head.difficulty;
    if (out->difficulty < 0) {
      out->difficulty = 0;
    }
    if (out->difficulty > 4) {
      out->difficulty = 4;
    }

    /* Colony citizens by profession. */
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if (c->nation_id != (uint8_t)human) {
        continue;
      }
      const int pop =
        c->population > COLONIZE_COL1_COLONY_POP_MAX ? COLONIZE_COL1_COLONY_POP_MAX
                                                     : (int)c->population;
      for (int p = 0; p < pop; ++p) {
        const int job = reports_resolve_job((int)c->profession[p], -1);
        out->citizens += reports_citizen_points_for_job(job);
      }
    }

    /* Map / Europe land colonists (not ships, wagons, artillery, treasure). */
    for (uint16_t i = 0; i < col1->head.unit_count; ++i) {
      const ColonizeCol1Unit* u = &col1->unit[i];
      if ((int)u->nation_id != human) {
        continue;
      }
      if (!reports_unit_type_is_scored_colonist((int)u->type)) {
        continue;
      }
      const int job = reports_resolve_job((int)u->profession, (int)u->type);
      out->citizens += reports_citizen_points_for_job(job);
    }

    out->congress = reports_count_ff_for_nation(col1, human) * 5;
    out->treasury = (int)(col1->nation[human].gold / 1000u);
    out->rebel_sentiment = reports_rebel_sentiment_pct(col1, human);
    out->villages_burned = (int)col1->nation[human].villages_burned;
    out->villages_penalty = -(out->difficulty + 1) * out->villages_burned;

    /*
     * Independence: WoI latch head.unknown46[0] (ai_king). Declare year not
     * separately latched yet — early-revolution % stays 0 until a year field
     * is wired. Achieve stays false until revolution victory sequence.
     * AI "withdrawn" (control==2) still counts toward foreign recognition.
     */
    for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
      if (n == human) {
        continue;
      }
      if (col1->player[n].control == 2) {
        out->prior_nations++;
      }
    }
    out->independence_declared = col1->head.unknown46[0] != 0;
    out->independence_achieved = col1->head.unknown46[4] == 1;
    out->declare_year = 0;
  } else {
    out->year = 0;
    out->difficulty = 0;
    if (colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &colonies->colonies[i];
        if (!c->active) {
          continue;
        }
        for (int p = 0; p < c->colonist_count; ++p) {
          if (!c->colonists[p].active) {
            continue;
          }
          /* Runtime pool lacks profession; count as free colonists. */
          out->citizens += 2;
        }
      }
    }
    const uint32_t gold = europe ? (uint32_t)europe->gold : 0u;
    out->treasury = (int)(gold / 1000u);
  }

  out->early_revolution_pct =
    reports_early_revolution_pct(out->independence_declared, out->declare_year);
  out->foreign_recognition_pct =
    reports_foreign_recognition_pct(out->prior_nations, out->independence_achieved);

  out->base_total = out->citizens + out->congress + out->treasury + out->rebel_sentiment +
                    out->villages_penalty + out->intervention_bells;

  const int bonus_pct = out->early_revolution_pct + out->foreign_recognition_pct;
  if (bonus_pct > 0 && out->base_total > 0) {
    out->total = out->base_total + (out->base_total * bonus_pct) / 100;
  } else {
    out->total = out->base_total;
  }
}

static void reports_render_score(
  const ColonizeCol1Save* col1,
  int human,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  uint32_t turn_number,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb,
  int* y,
  int step,
  char* line,
  size_t line_sz
) {
  ColonizeScoreBreakdown sc;
  reports_compute_score(&sc, col1, human, colonies, europe);

  static const char* k_diff[] = {
    "Discoverer", "Explorer", "Conquistador", "Governor", "Viceroy"
  };
  const char* diff_name =
    (sc.difficulty >= 0 && sc.difficulty <= 4) ? k_diff[sc.difficulty] : "?";

  if (col1) {
    snprintf(
      line,
      line_sz,
      "Year %d%s   Turn %u   %s",
      sc.year,
      col1->head.autumn ? " Autumn" : " Spring",
      (unsigned)(turn_number ? turn_number : col1->head.turn),
      diff_name
    );
  } else {
    snprintf(line, line_sz, "Turn %u", (unsigned)turn_number);
  }
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step + 2;

  snprintf(line, line_sz, "Citizens                %d", sc.citizens);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(line, line_sz, "Continental Congress    %d", sc.congress);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(line, line_sz, "Gold                    %d", sc.treasury);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(line, line_sz, "Rebel Sentiment         %d", sc.rebel_sentiment);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(
    line,
    line_sz,
    "Villages Burned         %d  (%d)",
    sc.villages_penalty,
    sc.villages_burned
  );
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  if (sc.intervention_bells != 0) {
    snprintf(line, line_sz, "Intervention Bells      %d", sc.intervention_bells);
    reports_draw_line(font, fb, 8, *y, line, 15);
    *y += step;
  }

  *y += 2;
  reports_draw_line(font, fb, 8, *y, "Independence", 15);
  *y += step;
  snprintf(
    line,
    line_sz,
    "  Declared              %s",
    sc.independence_declared ? "Yes" : "No"
  );
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step;
  snprintf(
    line,
    line_sz,
    "  Achieved              %s",
    sc.independence_achieved ? "Yes" : "No"
  );
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step;
  snprintf(line, line_sz, "  Early Revolution      +%d%%", sc.early_revolution_pct);
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step;
  snprintf(
    line,
    line_sz,
    "  Foreign Recognition   +%d%%  (%d prior nations)",
    sc.foreign_recognition_pct,
    sc.prior_nations
  );
  reports_draw_line(font, fb, 8, *y, line, 14);
  *y += step + 2;

  snprintf(line, line_sz, "Subtotal                %d", sc.base_total);
  reports_draw_line(font, fb, 8, *y, line, 15);
  *y += step;
  snprintf(line, line_sz, "Total Score             %d", sc.total);
  reports_draw_line(font, fb, 8, *y, line, 15);

  if (!sc.independence_achieved) {
    *y += step + 2;
    reports_draw_line(
      font, fb, 8, *y, "Win independence to apply revolution bonuses.", 14
    );
  }
}

void reports_render_hall_of_fame(
  const ColonizeReportsView* view,
  const ColonizeHofRow* entries,
  int entry_count,
  const ColonizeFont* font,
  ColonizeFramebuffer8* fb
) {
  if (!fb || !fb->pixels) {
    return;
  }
  memset(fb->pixels, 0, (size_t)fb->width * (size_t)fb->height);
  if (view && view->background_ok[COLONIZE_REPORT_SCORE]) {
    pik_blit(&view->backgrounds[COLONIZE_REPORT_SCORE], fb, 0, 0);
  }

  const int step = reports_line_step(font);
  int y = 4;
  reports_draw_line(font, fb, 8, y, "COLONIZATION HALL OF FAME", 15); /* LABELS.TXT #207 */
  y += step;
  reports_draw_line(font, fb, 8, y, "Esc / Enter returns to menu", 14);
  y += step + 4;

  reports_draw_line(font, fb, 8, y, "     Leader                    Nation      Score  A.D.", 15);
  y += step;

  char line[160];
  if (entry_count <= 0) {
    reports_draw_line(font, fb, 8, y, "  No retired games yet.", 14);
    return;
  }
  const int shown = entry_count > COLONIZE_HOF_ROW_MAX ? COLONIZE_HOF_ROW_MAX : entry_count;
  for (int i = 0; i < shown && y < 190; ++i) {
    const ColonizeHofRow* e = &entries[i];
    snprintf(
      line,
      sizeof(line),
      "%2d.  %-24s %-10s %6d  %d",
      i + 1,
      e->leader,
      e->nation,
      e->score,
      e->year
    );
    reports_draw_line(font, fb, 8, y, line, 15);
    y += step;
  }
}

void reports_render(
  const ColonizeReportsView* view,
  ColonizeReportId id,
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const EuropeScreen* europe,
  const ColonizeCol1Save* col1,
  int human_nation,
  int cursor_x,
  int cursor_y,
  uint32_t turn_number,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  (void)map;
  (void)cursor_x;
  (void)cursor_y;
  if (!framebuffer || !framebuffer->pixels) {
    return;
  }
  if (id < 0 || id >= COLONIZE_REPORT_COUNT) {
    id = COLONIZE_REPORT_RELIGIOUS;
  }

  const int human = reports_clamp_nation(human_nation);
  int y = 0;
  const int step = reports_line_step(font);
  char line[160];
  reports_render_body_start(view, id, font, framebuffer, &y);

  switch (id) {
    case COLONIZE_REPORT_RELIGIOUS:
      reports_render_religious(
        col1, human, colonies, europe, font, framebuffer, &y, step, line, sizeof(line)
      );
      break;
    case COLONIZE_REPORT_CONGRESS:
      reports_render_congress(col1, human, font, framebuffer, &y, step, line, sizeof(line));
      break;
    case COLONIZE_REPORT_LABOR:
      reports_render_labor(
        col1, human, colonies, units, font, framebuffer, &y, step, line, sizeof(line)
      );
      break;
    case COLONIZE_REPORT_ECONOMIC:
      reports_render_economic(
        col1, human, europe, font, framebuffer, &y, step, line, sizeof(line)
      );
      break;
    case COLONIZE_REPORT_COLONY:
      /* When unit icon rows are added, draw with unit_chrome_draw (FUN_112b_01ba). */
      reports_render_colony(
        col1, human, colonies, font, framebuffer, &y, step, line, sizeof(line)
      );
      break;
    case COLONIZE_REPORT_NAVAL:
      /* When ship icon rows are added, draw with unit_chrome_draw (FUN_112b_01ba). */
      reports_render_naval(
        col1, human, units, europe, font, framebuffer, &y, step, line, sizeof(line)
      );
      break;
    case COLONIZE_REPORT_FOREIGN:
      reports_render_foreign(
        col1, human, europe, font, framebuffer, &y, step, line, sizeof(line)
      );
      break;
    case COLONIZE_REPORT_INDIAN:
      reports_render_indian(col1, human, font, framebuffer, &y, step, line, sizeof(line));
      break;
    case COLONIZE_REPORT_SCORE:
      reports_render_score(
        col1,
        human,
        colonies,
        europe,
        turn_number,
        font,
        framebuffer,
        &y,
        step,
        line,
        sizeof(line)
      );
      break;
    default:
      break;
  }
}
