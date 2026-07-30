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

void reports_render(
  const ColonizeReportsView* view,
  ColonizeReportId id,
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const EuropeScreen* europe,
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

  int y = 0;
  const int step = reports_line_step(font);
  char line[128];
  reports_render_body_start(view, id, font, framebuffer, &y);

  switch (id) {
    case COLONIZE_REPORT_RELIGIOUS: {
      int crosses_stub = 0;
      int dock = europe ? europe->dock_count : 0;
      reports_draw_line(font, framebuffer, 8, y, "Immigration / crosses (stub)", 15);
      y += step;
      snprintf(line, sizeof(line), "Dock immigrants waiting: %d", dock);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      y += step;
      snprintf(line, sizeof(line), "Cross production tracked: %d (not simulated)", crosses_stub);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      y += step + 4;
      reports_draw_line(
        font, framebuffer, 8, y, "Church / cathedral effects come with colony production.", 14
      );
      break;
    }
    case COLONIZE_REPORT_CONGRESS: {
      reports_draw_line(font, framebuffer, 8, y, "Founding Fathers (stub)", 15);
      y += step;
      reports_draw_line(font, framebuffer, 8, y, "Members in Congress: 0", 15);
      y += step;
      reports_draw_line(font, framebuffer, 8, y, "Now debating: (none)", 15);
      y += step;
      reports_draw_line(font, framebuffer, 8, y, "Next session: not scheduled", 15);
      y += step + 4;
      reports_draw_line(
        font, framebuffer, 8, y, "Liberty bells / FF election not implemented yet.", 14
      );
      break;
    }
    case COLONIZE_REPORT_LABOR: {
      reports_draw_line(font, framebuffer, 8, y, "Colonists by profession (current data)", 15);
      y += step;
      int counts[64];
      memset(counts, 0, sizeof(counts));
      int total = 0;
      if (colonies) {
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
      snprintf(line, sizeof(line), "Total colonists: %d", total);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      y += step;
      if (units && total > 0) {
        for (int t = 0; t < 64 && y < 180; ++t) {
          if (counts[t] <= 0) {
            continue;
          }
          const ColonizeUnitType* ut = units_type(units, t);
          snprintf(
            line,
            sizeof(line),
            "  %s: %d",
            ut ? ut->name : "Unknown",
            counts[t]
          );
          reports_draw_line(font, framebuffer, 8, y, line, 15);
          y += step;
        }
      } else {
        reports_draw_line(font, framebuffer, 8, y, "No colonists in colonies yet.", 14);
      }
      break;
    }
    case COLONIZE_REPORT_ECONOMIC: {
      const int gold = europe ? europe->gold : 0;
      const int tax = europe ? europe->tax_percent : 0;
      reports_draw_line(font, framebuffer, 8, y, "Treasury and market (available data)", 15);
      y += step;
      snprintf(line, sizeof(line), "Gold: %d", gold);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      y += step;
      snprintf(line, sizeof(line), "Tax rate: %d%%", tax);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      y += step;
      if (europe && europe->cargo_count > 0) {
        reports_draw_line(font, framebuffer, 8, y, "Europe market (bid/ask):", 15);
        y += step;
        for (int i = 0; i < europe->cargo_count && i < 8 && y < 170; ++i) {
          snprintf(
            line,
            sizeof(line),
            "  %s  %d / %d",
            europe->cargo[i].name,
            europe->cargo[i].bid,
            europe->cargo[i].ask
          );
          reports_draw_line(font, framebuffer, 8, y, line, 15);
          y += step;
        }
        if (europe->cargo_count > 8) {
          reports_draw_line(font, framebuffer, 8, y, "  ...", 14);
          y += step;
        }
      }
      y += 4;
      reports_draw_line(
        font, framebuffer, 8, y, "Colony production / upkeep not simulated yet.", 14
      );
      break;
    }
    case COLONIZE_REPORT_COLONY: {
      reports_draw_line(font, framebuffer, 8, y, "Colonies (click zoom later)", 15);
      y += step;
      int n = 0;
      if (colonies) {
        for (int i = 0; i < COLONIZE_COLONIES_MAX && y < 180; ++i) {
          const ColonizeColony* c = &colonies->colonies[i];
          if (!c->active) {
            continue;
          }
          snprintf(
            line,
            sizeof(line),
            "  %s  (%d,%d)  pop %d",
            c->name,
            c->x,
            c->y,
            c->population
          );
          reports_draw_line(font, framebuffer, 8, y, line, 15);
          y += step;
          n++;
        }
      }
      if (n == 0) {
        reports_draw_line(font, framebuffer, 8, y, "No colonies founded.", 14);
      } else {
        y += 4;
        snprintf(line, sizeof(line), "Total colonies: %d", n);
        reports_draw_line(font, framebuffer, 8, y, line, 15);
      }
      break;
    }
    case COLONIZE_REPORT_NAVAL: {
      reports_draw_line(font, framebuffer, 8, y, "Ships on map / in Europe", 15);
      y += step;
      int sea = 0;
      if (units) {
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &units->units[i];
          if (!u->active || !units_is_sea(units, u->id)) {
            continue;
          }
          const ColonizeUnitType* ut = units_type(units, u->type_index);
          snprintf(
            line,
            sizeof(line),
            "  %s at (%d,%d)  hold %d",
            ut ? ut->name : "Ship",
            u->x,
            u->y,
            u->cargo_count
          );
          reports_draw_line(font, framebuffer, 8, y, line, 15);
          y += step;
          sea++;
        }
      }
      if (europe) {
        snprintf(line, sizeof(line), "Europe harbor ships: %d", europe->harbor_ships);
        reports_draw_line(font, framebuffer, 8, y, line, 15);
        y += step;
        for (int i = 0; i < europe->harbor_ships && y < 180; ++i) {
          snprintf(
            line,
            sizeof(line),
            "  Harbor: %s (+%d passengers)",
            europe->harbor[i].name,
            europe->harbor[i].cargo_count
          );
          reports_draw_line(font, framebuffer, 8, y, line, 15);
          y += step;
        }
      }
      if (sea == 0 && (!europe || europe->harbor_ships == 0)) {
        reports_draw_line(font, framebuffer, 8, y, "No ships in play.", 14);
      }
      break;
    }
    case COLONIZE_REPORT_FOREIGN: {
      reports_draw_line(font, framebuffer, 8, y, "European rivals (stub)", 15);
      y += step;
      reports_draw_line(font, framebuffer, 8, y, "Other powers are not simulated yet.", 14);
      y += step;
      reports_draw_line(font, framebuffer, 8, y, "War / peace / strength tables: n/a", 14);
      y += step + 4;
      if (europe) {
        snprintf(line, sizeof(line), "Your nation: %s", europe->nation_name);
        reports_draw_line(font, framebuffer, 8, y, line, 15);
      }
      break;
    }
    case COLONIZE_REPORT_INDIAN: {
      reports_draw_line(font, framebuffer, 8, y, "Native tribes (stub)", 15);
      y += step;
      reports_draw_line(font, framebuffer, 8, y, "Indian villages are not placed yet.", 14);
      y += step;
      reports_draw_line(font, framebuffer, 8, y, "Alarm / missions / training: n/a", 14);
      break;
    }
    case COLONIZE_REPORT_SCORE: {
      int pop = 0;
      int colony_n = 0;
      if (colonies) {
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          if (!colonies->colonies[i].active) {
            continue;
          }
          colony_n++;
          pop += colonies->colonies[i].population;
        }
      }
      const int gold = europe ? europe->gold : 0;
      const int score = pop * 2 + colony_n * 10 + gold / 1000;
      reports_draw_line(font, framebuffer, 8, y, "Final score (incomplete rules)", 15);
      y += step;
      snprintf(line, sizeof(line), "Turn: %u", turn_number);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      y += step;
      snprintf(line, sizeof(line), "Citizens: %d", pop);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      y += step;
      snprintf(line, sizeof(line), "Colonies: %d", colony_n);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      y += step;
      snprintf(line, sizeof(line), "Treasury score: %d  (gold/1000)", gold / 1000);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      y += step;
      reports_draw_line(font, framebuffer, 8, y, "Congress / independence / villages: 0", 14);
      y += step;
      snprintf(line, sizeof(line), "Total Score: %d", score);
      reports_draw_line(font, framebuffer, 8, y, line, 15);
      break;
    }
    default:
      break;
  }
}
