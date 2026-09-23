#include "core/colony_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony_craft.h"
#include "core/colony_preview.h"
#include "core/colony_production.h"
#include "core/colony_screen_internal.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/fb.h"
#include "core/ff.h"
#include "core/founding_fathers.h"
#include "core/map_menu.h"
#include "core/map_panel.h"
#include "core/popup.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/ui_colors.h"
#include "core/turn.h"
#include "core/ui_button.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * Sections:
 *  - Population/people panel rendering (colony_screen_sol_percent .. colony_screen_draw_people) (~line 37)
 *  - Multifunction production panel rendering (colony_screen_prod_slot_split .. colony_screen_draw_multifunction) (~line 351)
 */

/* ===================== Population/people panel rendering (colony_screen_sol_percent .. colony_screen_draw_people) ===================== */

static int colony_screen_sol_percent(const ColonizeCol1Save* col1, const ColonizeColony* colony) {
  return colony_prod_sol_percent(col1, colony);
}

/*
 * bugs.md: one layout for the People band, shared by draw and hit-test —
 * every active colonist plus every fence unit, packed naturally and
 * squeezed to fit the band when the natural row overflows (the old code
 * hard-stopped at the edge and silently dropped the rest of a big colony).
 */
int colony_screen_people_layout(
  const ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  PeopleEntry* ent,
  int max_ent
) {
  if (!view || !colony || !ent || max_ent <= 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < colony->colonist_count && n < max_ent; ++i) {
    const ColonizeColonist* c = &colony->colonists[i];
    if (!c->active) {
      continue;
    }
    const int sprite =
      units_working_colonist_sprite(units, c->unit_type_index, c->profession);
    if (sprite >= 0) {
      ent[n].sprite = sprite;
      ent[n].sel_colonist = i;
      ent[n].sel_unit = -1;
      n++;
    }
  }
  const int colonists_drawn = n;
  for (int i = 0; i < view->outside_unit_count && n < max_ent; ++i) {
    const ColonizeUnit* u = units_get_const(units, view->outside_unit_ids[i]);
    if (!u) {
      continue;
    }
    const int sprite = colony_screen_outside_display_sprite(units, u);
    if (sprite >= 0) {
      ent[n].sprite = sprite;
      ent[n].sel_colonist = -1;
      ent[n].sel_unit = u->id;
      n++;
    }
  }
  const int x0 = COLONY_PEOPLE_X + 2;
  const int avail = COLONY_PEOPLE_W - 4;
  int natural = 0;
  int last_w = 12;
  for (int i = 0; i < n; ++i) {
    const ColonizeSprite* sp =
      (view->icons_ok && ent[i].sprite < view->icons.sprite_count)
        ? &view->icons.sprites[ent[i].sprite]
        : NULL;
    ent[i].iw = (sp && sp->width > 0) ? sp->width : 12;
    last_w = ent[i].iw;
    natural += ent[i].iw + 2;
    if (i == colonists_drawn && colonists_drawn > 0) {
      natural += 6;
    }
  }
  natural -= 2;
  const bool squeeze = natural > avail && n > 1;
  int x = x0;
  for (int i = 0; i < n; ++i) {
    if (squeeze) {
      ent[i].px = x0 + (int)((long)i * (avail - last_w) / (n - 1));
    } else {
      if (i == colonists_drawn && colonists_drawn > 0) {
        x += 6;
      }
      ent[i].px = x;
      x += ent[i].iw + 2;
    }
  }
  return n;
}

void colony_screen_draw_people(
  ColonyScreenView* view,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeCol1Save* col1,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !colony || !framebuffer) {
    return;
  }
  const int sol = colony_screen_sol_percent(col1, colony);
  const int tory = 100 - sol;
  const int pop = colony->colonist_count > 0 ? colony->colonist_count : colony->population;
  const int sol_count = (pop > 0) ? ((pop * sol + 50) / 100) : 0;
  const int tory_count = pop > sol_count ? (pop - sol_count) : 0;
  const int sol_y = COLONY_PANEL_CONTENT_Y + 1;
  colony_screen_blit_icon(view, COLONY_ICON_FLAG, framebuffer, COLONY_PEOPLE_X + 2, sol_y);
  /* Tory flush to the right of the (widened) people band. */
  colony_screen_blit_icon(
    view, COLONY_ICON_CROWN, framebuffer, COLONY_PEOPLE_X + COLONY_PEOPLE_W - 14, sol_y
  );
  if (font) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d%% (%d)", sol, sol_count);
    font_draw_text(font, framebuffer, COLONY_PEOPLE_X + 16, sol_y + 2, buf, 15);
    snprintf(buf, sizeof(buf), "%d%% (%d)", tory, tory_count);
    const int tw = font_text_width_skip(font, buf, FONT_SKIP_NONE);
    font_draw_text(
      font, framebuffer, COLONY_PEOPLE_X + COLONY_PEOPLE_W - 16 - tw, sol_y + 2, buf, 15
    );
  }

  const int y_people = COLONY_PANEL_CONTENT_Y + 16;
  {
    PeopleEntry ent[COLONIZE_COLONY_POP_MAX + COLONY_OUTSIDE_MAX];
    const int n = colony_screen_people_layout(
      view, colony, units, ent, (int)(sizeof(ent) / sizeof(ent[0]))
    );
    for (int i = 0; i < n; ++i) {
      /* bugs.md: the People line-up uses the plain colonist chrome — no
       * drop shadow. */
      colony_screen_blit_icon(view, ent[i].sprite, framebuffer, ent[i].px, y_people);
      if ((ent[i].sel_colonist >= 0 && view->selected_colonist == ent[i].sel_colonist) ||
          (ent[i].sel_unit >= 0 && view->selected_outside_unit == ent[i].sel_unit)) {
        colony_screen_draw_icon_selection(view, framebuffer, ent[i].sprite, ent[i].px, y_people);
      }
    }
  }

  if (!view->preview_valid) {
    return;
  }
  const ColonizeColonyPreview* p = &view->preview;
  const int meter_y = COLONY_CARGO_STRIP_Y - 16;
  const int meter_h = 12;
  const int band = COLONY_PEOPLE_W - 4;
  const int gap = 4;
  /* golden-confirmed (New Amsterdam: 32, not food_produced's pre-breeding
   * 34): this badge shows the same post-breeding `goods[FOOD]` the
   * Production tab would net to (raw field/town-commons food minus the
   * turn's horse-breeding feed, since that subtraction lands in `goods[]`
   * not a separate field), not the raw pre-breeding `food_produced`. */
  const int food_amt = p->goods[COLONIZE_CARGO_FOOD];
  /*
   * Columns, in DOS reading order: fish/grain produced, the net food counter,
   * crosses, bells. Player-reported placement ("between food-produced-and-
   * eaten and crosses") and player-reported sizing (width mostly
   * proportional to each column's amount, not a flat 1/n split, with a
   * floor so a small amount like a 2-surplus still reads).
   *
   * bugs.md: the net counter carries *either* the surplus *or* the shortage —
   * never both, and nothing at all when the colony eats exactly what it grows.
   * A shortage is that counter drawn with the grey food icon and a red number,
   * not the produced-food number turned red: production is what it is, and
   * recolouring it hides how much is actually short.
   */
  const int food_net = p->food_net;
  const bool net_active = food_net != 0;
  const int net_amt = food_net > 0 ? food_net : -food_net;
  const int slot_count = net_active ? 4 : 3;
  long weight[4];
  int wi = 0;
  const int food_weight_idx = wi;
  weight[wi++] = food_amt > 0 ? food_amt : 1;
  int surplus_weight_idx = -1;
  if (net_active) {
    surplus_weight_idx = wi;
    weight[wi++] = net_amt;
  }
  const int cross_weight_idx = wi;
  weight[wi++] = p->crosses > 0 ? p->crosses : 1;
  const int bell_weight_idx = wi;
  weight[wi++] = p->bells > 0 ? p->bells : 1;

  const int min_w = 14; /* floor: smallest column still fits an icon + number */
  const int avail = band - (slot_count - 1) * gap;
  long weight_sum = 0;
  for (int i = 0; i < slot_count; ++i) {
    weight_sum += weight[i];
  }
  int width[4];
  int width_sum = 0;
  for (int i = 0; i < slot_count; ++i) {
    width[i] = weight_sum > 0 ? (int)((long)avail * weight[i] / weight_sum) : avail / slot_count;
    if (width[i] < min_w) {
      width[i] = min_w;
    }
    width_sum += width[i];
  }
  /* Rounding remainder (positive or negative, from the min-width floor)
   * goes to the heaviest column — bells, golden-confirmed as the largest
   * New Amsterdam value, so it's the safest place to absorb slack. */
  const int diff = avail - width_sum;
  if (diff != 0) {
    width[bell_weight_idx] += diff;
    if (width[bell_weight_idx] < min_w) {
      width[bell_weight_idx] = min_w;
    }
    width_sum = 0;
    for (int i = 0; i < slot_count; ++i) {
      width_sum += width[i];
    }
  }
  /*
   * bugs.md: the row must never escape the People box. When the floors +
   * bells re-floor leave the total past `avail` (e.g. one huge column plus
   * three floored ones), shave the widest columns back down to the floor
   * until it fits — 4 floored columns (68px) always fit the 102px band, so
   * this terminates inside the box.
   */
  for (int guard = 0; width_sum > avail && guard < 8; ++guard) {
    int widest = -1;
    for (int i = 0; i < slot_count; ++i) {
      if (width[i] > min_w && (widest < 0 || width[i] > width[widest])) {
        widest = i;
      }
    }
    if (widest < 0) {
      break;
    }
    int take = width_sum - avail;
    const int room = width[widest] - min_w;
    if (take > room) {
      take = room;
    }
    width[widest] -= take;
    width_sum -= take;
  }

  int meter_x = COLONY_PEOPLE_X + 2;
  {
    /*
     * DOS-LITERAL FUN_2f2b_* raw 48474-48487 — the summary band draws FOOD
     * TWICE. When DS:0x8e32 is 0 it first takes `local_4 = min(DS:0xa895,
     * DS:0x8e0a)` — the fish subtotal accumulated by the plot walk (raw
     * 12502 zeroes it, raw 12596 / 4270 / 5345 add each Fisherman plot's
     * yield) clamped to the colony's total food — emits that row, then falls
     * through to a second unconditional emit for the remainder. The port's
     * fish/grain pair is that same split, with COLONY_ICON_FISH (DOS icon id
     * 0x3a, 1-based; sprite index 57, the per-plot badge icon of raw 47750).
     * bugs.md #607.
     */
    const int fish_amt =
      (food_amt > 0) ? (p->food_fish > food_amt ? food_amt : p->food_fish) : 0;
    const int grain_amt = food_amt - fish_amt;
    colony_screen_draw_resource_count_pair(
      view,
      font,
      framebuffer,
      meter_x,
      meter_y,
      width[food_weight_idx],
      meter_h,
      COLONY_ICON_FISH,
      fish_amt,
      COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_FOOD,
      grain_amt,
      15,
      false /* bugs.md: People band follows the numbers toggle too */
    );
    meter_x += width[food_weight_idx] + gap;
  }
  if (net_active) {
    const bool shortage = food_net < 0;
    colony_screen_draw_resource_count(
      view,
      font,
      framebuffer,
      meter_x,
      meter_y,
      width[surplus_weight_idx],
      meter_h,
      shortage ? (COLONY_CARGO_GREY_BASE + COLONIZE_CARGO_FOOD)
               : (COLONY_CARGO_ICON_BASE + COLONIZE_CARGO_FOOD),
      net_amt,
      shortage ? 12 : 15,
      false
    );
    meter_x += width[surplus_weight_idx] + gap;
  }
  colony_screen_draw_resource_count(
    view, font, framebuffer, meter_x, meter_y, width[cross_weight_idx], meter_h,
    COLONY_ICON_CROSS, p->crosses, 15, false
  );
  meter_x += width[cross_weight_idx] + gap;
  colony_screen_draw_resource_count(
    view, font, framebuffer, meter_x, meter_y, width[bell_weight_idx], meter_h, COLONY_ICON_BELL,
    p->bells, 15, false
  );
}

typedef struct ColonyProdSlot {
  int icon0;
  int amount0;
  uint8_t color0;
  int icon1; /* < 0 => single value, cases 1/2 */
  int amount1;
  uint8_t color1;
} ColonyProdSlot;

/* "used + stored" white pair in one cell (Lumber->Hammers, craft inputs). */
/* ===================== Multifunction production panel rendering (colony_screen_prod_slot_split .. colony_screen_draw_multifunction) ===================== */

static void colony_screen_prod_slot_split(ColonyProdSlot* s, int c, int used, int stored) {
  s->icon0 = COLONY_CARGO_ICON_BASE + c;
  s->amount0 = used;
  s->color0 = 15;
  s->icon1 = COLONY_CARGO_ICON_BASE + c;
  s->amount1 = stored;
  s->color1 = 15;
}

void colony_screen_draw_multifunction(
  ColonyScreenView* view,
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  const ColonizeFont* font,
  const ColonizeMsgCatalog* labels,
  ColonizeFramebuffer8* framebuffer
) {
  if (!view || !framebuffer) {
    return;
  }
  colony_screen_blit_icon(
    view, COLONY_ICON_HOUSE, framebuffer, COLONY_MULTI_BTN_X, COLONY_PANEL_CONTENT_Y
  );
  colony_screen_blit_icon(
    view, COLONY_ICON_RIFLE, framebuffer, COLONY_MULTI_BTN_X, COLONY_PANEL_CONTENT_Y + 16
  );
  colony_screen_blit_icon(
    view, COLONY_ICON_HAMMER_BTN, framebuffer, COLONY_MULTI_BTN_X, COLONY_PANEL_CONTENT_Y + 32
  );
  {
    const int by = COLONY_PANEL_CONTENT_Y + (int)view->multi_mode * 16;
    colony_screen_draw_selection_box(framebuffer, COLONY_MULTI_BTN_X - 1, by - 1, 14, 14, 10);
  }

  const int px = COLONY_MULTI_X + 2;
  const int pane_w = COLONY_MULTI_W - 19; /* keep 15px clear before mode buttons */
  const int py = COLONY_PANEL_CONTENT_Y;
  const int pane_h = COLONY_PANEL_CONTENT_H;
  if (view->multi_mode == COLONY_MULTI_PRODUCTION && view->preview_valid) {
    /* Cargo goods + shortfalls + hammers (not crosses/bells). One slot per
     * cargo type — player-reported (New Amsterdam golden, pixel-checked):
     * a resource's produced/shortfall/potential numbers belong in a single
     * cell together, not scattered across separate grid cells:
     *   1. produced, nothing downstream wants it: one plain number.
     *   2. not produced, something wants it: one grey/red "short" number.
     *   3. produced, but less than something downstream wants: produced
     *      (white) + short (red) together, one cell, two side-by-side
     *      boxes with a spacer.
     *   4. produced in surplus of what's used: used + stored together,
     *      one cell, two side-by-side boxes with a spacer (both white).
     * Cases 3 and 4 share the same side-by-side rendering — the only
     * difference is the second box's icon/color (grey+red vs the same
     * cargo icon in white again).
     *
     * 2026-08-27, player-directed departure from DOS pixel-fidelity: case 4
     * now applies to *every* cargo a craft recipe draws on this tick, not
     * just Lumber->hammers (the golden only shows the split for Lumber —
     * Ore/Tools stay plain single numbers there even though the Blacksmith/
     * Armory visibly consume part of them). Explicitly not matching DOS
     * here — the player asked for the split everywhere as a UI
     * improvement, this pane only, not a "we got DOS wrong" fix.
     */
    const ColonizeColonyPreview* p = &view->preview;
    ColonyProdSlot slots[COLONIZE_CARGO_COUNT + 1];
    int slot_count = 0;
    /* Food is shown on the People band's fish/grain meter, not repeated
     * here — golden-confirmed (no Food badge in this pane). Every other
     * cargo shows its GROSS production this tick (field-worker output
     * plus, for a manufactured good, the building's own gross craft
     * output) rather than `goods[]`'s net-after-further-consumption — see
     * ColonizeColonyPreview.field_gross/craft_gross's header comment.
     * Horses has no craft recipe of its own (breeding only), so `goods[]`
     * is already the right (and only) figure for it. */
    for (int c = 1; c < COLONIZE_CARGO_COUNT; ++c) {
      if (slot_count >= (int)(sizeof(slots) / sizeof(slots[0]))) {
        break;
      }
      const int produced =
        (c == COLONIZE_CARGO_HORSES) ? p->goods[c] : (p->field_gross[c] + p->craft_gross[c]);
      const int short_amt = p->shortfall[c];

      if (c == COLONIZE_CARGO_LUMBER && short_amt <= 0 &&
          p->hammers_capacity > produced) {
        /* Player-reported (bugs.md): a carpenter demanding more lumber than
         * this tick *produces* is a lumber shortage — even when warehouse
         * stock is still feeding him (production 0, stock > 0), and doubly
         * so when there is no stock either. Same red pairing as the craft
         * recipes' input-side shortfall. */
        ColonyProdSlot* s = &slots[slot_count++];
        s->icon0 = produced > 0 ? COLONY_CARGO_ICON_BASE + c : -1;
        s->amount0 = produced;
        s->color0 = 15;
        s->icon1 = COLONY_CARGO_GREY_BASE + c;
        s->amount1 = p->hammers_capacity - produced;
        s->color1 = 12;
        continue;
      }

      if (c == COLONIZE_CARGO_LUMBER && short_amt <= 0 && p->hammers > 0 && produced > 0) {
        /* Case 4: Lumber->Hammers isn't a colony_craft_preview() recipe
         * (the Carpenter's hammers bank is `colony_prod_colony_hammers`, a
         * separate computation), so it never earns a shortfall[] entry —
         * but it's the one real surplus-of-what's-used case in this game.
         * Player-reported (New Amsterdam golden): 22 Lumber = 16 spent on
         * this tick's hammers + 6 left over, shown as two adjacent white
         * counters, not one plain "22". */
        int used = p->hammers;
        if (used > produced) {
          used = produced;
        }
        const int stored = produced - used;
        if (stored > 0) {
          colony_screen_prod_slot_split(&slots[slot_count++], c, used, stored);
          continue;
        }
      }

      if (short_amt <= 0 && produced > 0) {
        /* Case 4, general form: some *other* cargo's craft recipe (not
         * Lumber's hammers — that's the special case above) drew on this
         * tick's production as its raw input. Not DOS-accurate — DOS shows
         * Ore/Tools here as one plain number even when the Blacksmith/
         * Armory visibly consume part of it (checked against the golden:
         * 28 Ore, 24 Tools, both single) — a deliberate departure from
         * pixel-fidelity, player-requested: the Production tab is
         * explicitly not staying 1:1 with DOS here, splitting every
         * resource this way as a UI improvement. `goods[c]` is already the
         * net-of-consumption warehouse delta, so `produced - goods[c]` is
         * exactly what got drawn off this tick and `goods[c]` itself is
         * exactly what's left to store — no separate bookkeeping needed. */
        const int used = produced - p->goods[c];
        const int stored = p->goods[c];
        if (used > 0 && stored > 0) {
          colony_screen_prod_slot_split(&slots[slot_count++], c, used, stored);
          continue;
        }
      }

      if (short_amt > 0) {
        /* Cases 2/3: produced (white, 0 if nothing produced) paired with
         * the shortfall (red) in one cell — not summed into one number,
         * not two separate cells. */
        ColonyProdSlot* s = &slots[slot_count++];
        s->icon0 = produced > 0 ? COLONY_CARGO_ICON_BASE + c : -1;
        s->amount0 = produced;
        s->color0 = 15;
        s->icon1 = COLONY_CARGO_GREY_BASE + c;
        s->amount1 = short_amt;
        s->color1 = 12;
      } else if (produced > 0) {
        /* Case 1. */
        ColonyProdSlot* s = &slots[slot_count++];
        s->icon0 = COLONY_CARGO_ICON_BASE + c;
        s->amount0 = produced;
        s->color0 = 15;
        s->icon1 = -1;
        s->amount1 = 0;
        s->color1 = 0;
      }
    }
    if ((p->hammers > 0 || p->hammers_capacity > p->hammers) &&
        slot_count < (int)(sizeof(slots) / sizeof(slots[0]))) {
      /* Hammer shortfall (player-reported, bugs.md): a lumber-starved
       * carpenter shows the hammers he could not bank as a red number, in
       * the same one-cell pairing the cargo shortfalls use. */
      ColonyProdSlot* s = &slots[slot_count++];
      const int short_h = p->hammers_capacity - p->hammers;
      {
        s->icon0 = p->hammers > 0 ? COLONY_ICON_HAMMER : -1;
        s->amount0 = p->hammers;
        s->color0 = 15;
        if (short_h > 0) {
          s->icon1 = COLONY_ICON_HAMMER;
          s->amount1 = short_h;
          s->color1 = 12;
        } else {
          s->icon1 = -1;
          s->amount1 = 0;
          s->color1 = 0;
        }
      }
    }
    if (slot_count > 0 && pane_w > 0 && pane_h > 0) {
      /* Prefer a single column; add columns when rows would be shorter than icons. */
      const int min_row_h = 8;
      int cols = 1;
      int rows = slot_count;
      while (cols < slot_count && pane_h / rows < min_row_h) {
        cols++;
        rows = (slot_count + cols - 1) / cols;
      }
      const int cell_w = pane_w / cols;
      const int cell_h = pane_h / rows;
      for (int i = 0; i < slot_count; ++i) {
        const int col = i / rows;
        const int row = i % rows;
        const int sx = px + col * cell_w;
        const int sy = py + row * cell_h;
        const ColonyProdSlot* s = &slots[i];
        if (s->icon1 < 0 || s->icon0 < 0) {
          /* Single value: cases 1/2 outright, and case 2's "nothing
           * produced" collapses here too rather than splitting an empty
           * left half. */
          const int icon = s->icon1 < 0 ? s->icon0 : s->icon1;
          const int amount = s->icon1 < 0 ? s->amount0 : s->amount1;
          const uint8_t color = s->icon1 < 0 ? s->color0 : s->color1;
          colony_screen_draw_resource_count(
            view, font, framebuffer, sx, sy, cell_w, cell_h, icon, amount, color, false
          );
        } else {
          /* Cases 3/4: two independent boxes sharing this cell's width,
           * with the gap between them the spacer the player asked for —
           * produced/used on the left, shortfall/stored on the right. */
          const int half = cell_w / 2;
          colony_screen_draw_resource_count(
            view, font, framebuffer, sx, sy, half, cell_h, s->icon0, s->amount0, s->color0, false
          );
          colony_screen_draw_resource_count(
            view, font, framebuffer, sx + half, sy, cell_w - half, cell_h, s->icon1, s->amount1,
            s->color1, false
          );
        }
      }
    }
  } else if (view->multi_mode == COLONY_MULTI_UNITS && units) {
    /* Land units at the colony (soldiers, colonists, scouts, artillery, …);
     * ships/wagons stay on the Transport strip — see
     * colony_screen_multi_units_layout. LABELS.TXT @CMISC index 1 "Units
     * Present" title (golden-confirmed: New Amsterdam's Military tab),
     * live-resolved 2026-08-27 (was hardcoded — colony_screen_render
     * gained a `labels` param for this), centered, dark blue (WOODPANL.PIK
     * idx 57, exact RGB match against the golden's sampled ink color). */
    if (font) {
      const char* title = "";
      if (labels) {
        const ColonizeMsgSection* cmisc = assets_msg_find(labels, "CMISC");
        if (cmisc && cmisc->line_count > 1 && cmisc->lines[1][0]) {
          title = cmisc->lines[1];
        }
      }
      const int tw = font_text_width(font, title);
      font_draw_text(font, framebuffer, px + (pane_w - tw) / 2, py, title, 57);
    }
    ColonyMultiUnitSlot slots[COLONY_MULTI_UNITS_SLOT_MAX];
    const int slot_count = colony_screen_multi_units_layout(
      view, units, px, py + COLONY_MULTI_UNITS_TITLE_H, pane_w, pane_h - COLONY_MULTI_UNITS_TITLE_H,
      slots, COLONY_MULTI_UNITS_SLOT_MAX
    );
    for (int i = 0; i < slot_count; ++i) {
      const ColonizeUnit* u = units_get_const(units, slots[i].unit_id);
      const int sprite = u ? colony_screen_outside_display_sprite(units, u) : -1;
      if (!u || sprite < 0) {
        continue;
      }
      if (slots[i].mini) {
        /* Overflow rows: 3x5 miniature; selection is the small DOS box
         * (2f2b:1f7f with the row-1 params: x-1..x+3 by y-2..y+5). */
        colony_screen_blit_mini_unit(view, framebuffer, sprite, slots[i].x, slots[i].y);
        if (view->multi_unit_selected_id == u->id) {
          colony_screen_draw_selection_box(
            framebuffer, slots[i].x - 1, slots[i].y - 2, COLONY_MULTI_UNITS_MINI_W + 2,
            COLONY_MULTI_UNITS_MINI_H + 3, 10
          );
        }
        continue;
      }
      unit_chrome_blit_unit_for_palette(
        framebuffer,
        font,
        &view->icons,
        sprite,
        slots[i].x,
        slots[i].y,
        units_display_type_index(units, u->id),
        u->nation_id,
        u->orders,
        false,
        /* Chrome's 4th arm = damaged Artillery (+0x3148 bit7), not aboard. */
        (u->col1_flags15 & 0x80u) != 0,
        (view->frame_ok && view->frame.has_palette) ? &view->frame.palette : NULL
      );
      if (view->multi_unit_selected_id == u->id) {
        colony_screen_draw_chrome_selection(view, framebuffer, sprite, slots[i].x, slots[i].y);
      }
    }
  } else if (view->multi_mode == COLONY_MULTI_CONSTRUCTION && colony && pool) {
    char line[64];
    const ColonizeBuildingType* bt =
      (colony->building_in_production >= 0)
        ? colonies_building_type(pool, colony->building_in_production)
        : NULL;
    /* Col1 also encodes buildable *units* (only Artillery modeled) in this
     * same field, using codes past the @BUILDING table's own range
     * (colonies_building_type / colonies_find_building only cover real
     * buildings) — col1_bridge_apply copies the raw code through verbatim
     * for anything that isn't the one special-cased Stockade remap.
     * colonies_unit_build_info is the single source of truth for name/
     * hammers/tools_cost, shared with colonies_set_construction/_list_
     * buildable/_buy_construction/_try_complete_unit_construction. */
    const char* unit_name = NULL;
    int unit_hammers = 0;
    int unit_tools = 0;
    if (!bt) {
      colonies_unit_build_info(colony->building_in_production, &unit_name, &unit_hammers, &unit_tools);
    }
    if (font) {
      if (bt) {
        snprintf(line, sizeof(line), "%s", bt->name);
      } else if (unit_name) {
        snprintf(line, sizeof(line), "%s", unit_name);
      } else {
        /* FUN_2f2b_1e46: no project (+0x94 < 0) = empty title. */
        line[0] = '\0';
      }
      const int title_w = font_text_width(font, line);
      font_draw_text(font, framebuffer, px + (pane_w - title_w) / 2, py, line, 57);
      {
        UiButtonColors bc;
        bc.dark = 0x31;
        bc.light = 0x3f;
        bc.text = 15;
        bc.hotkey = 14;
        if (view->frame_ok && view->frame.has_palette) {
          /* Remap Europe-style blues into the colony frame palette. */
          const ColonizePalette* pal = &view->frame.palette;
          bc.dark = assets_palette_nearest_rgb(pal, 20, 40, 120);
          bc.light = assets_palette_nearest_rgb(pal, 180, 200, 255);
        }
        int buy_w = 0;
        int buy_h = 0;
        int chg_w = 0;
        int chg_h = 0;
        /* LABELS.TXT @CTITLE rows 2 / 3; the leading '~' is the port's
         * hotkey marker, not catalog text. */
        char buy_lbl[24];
        char chg_lbl[24];
        snprintf(buy_lbl, sizeof(buy_lbl), "~%s", assets_msg_line_or(labels, "CTITLE", 2, ""));
        snprintf(chg_lbl, sizeof(chg_lbl), "~%s", assets_msg_line_or(labels, "CTITLE", 3, ""));
        ui_button_measure(font, buy_lbl, &buy_w, &buy_h);
        ui_button_measure(font, chg_lbl, &chg_w, &chg_h);
        /* Player-reported: BUY aligned vertically with CHANGE (both 4px up
         * from the original placement). */
        /* FUN_2f2b_21da: BUY is drawn only when +0x94 >= 0; CHANGE always. */
        if (colony->building_in_production >= 0) {
          ui_button_draw(font, framebuffer, px, py + 10 - 4, buy_w, buy_h, buy_lbl, &bc);
        }
        const int change_x = COLONY_MULTI_X + COLONY_MULTI_W - chg_w - 4 - 10;
        ui_button_draw(font, framebuffer, change_x, py + 10 - 4, chg_w, chg_h, chg_lbl, &bc);
      }
    }
    /* Accumulated carpenter hammers toward the current project, as four
     * rows of one-fourth of the total need each — player-requested (was a
     * single packed row spanning the pane width, golden-confirmed for that
     * shape, but the player asked for a 4-row quartile layout instead, a
     * deliberate UI improvement over DOS here same as the Production tab's
     * surplus-split departure). Hammers fill row 0 first, then row 1, etc.
     * — not a proportional bar, an actual fill order. Each row's capacity
     * is need/4 (remainder spread across the first rows so all four
     * capacities sum to exactly `need`). */
    const int need = bt ? bt->hammers : unit_hammers;
    const int have = colony->hammers > 0 ? colony->hammers : 0;
    const int show = (need > 0 && have > need) ? need : have;
    if (need > 0 && view->icons_ok && COLONY_ICON_HAMMER < view->icons.sprite_count) {
      const ColonizeSprite* sp = &view->icons.sprites[COLONY_ICON_HAMMER];
      const int iw = (sp && sp->width > 0) ? sp->width : 8;
      const int ih = (sp && sp->height > 0) ? sp->height : 12;
      /* Player-reported: 3px lower (row 0 was overlapping the BUY/CHANGE
       * buttons above it). */
      const int area_y = py + 19;
      const int area_h = 24; /* leaves room above (buttons) and below (tools line) in the box */
      const int row_h = area_h / 4;
      const int base = need / 4;
      const int rem = need % 4;
      /* Player-reported: either every row with hammers shows a number, or
       * none do — never a mix. One density check for the whole bar (all
       * four rows share ~the same capacity, off by at most 1), based on the
       * fullest row: if that many icons can't fit without overlapping,
       * icons stop being individually countable, so every non-empty row
       * gets a number instead of icons; otherwise every row stays icons-only
       * with no numbers at all. */
      const int max_row_capacity = base + (rem > 0 ? 1 : 0);
      const bool dense = max_row_capacity > 0 && max_row_capacity * iw > pane_w;
      int row_filled[4] = {0, 0, 0, 0};
      int remaining = show;
      for (int r = 0; r < 4; ++r) {
        const int row_capacity = base + (r < rem ? 1 : 0);
        int filled = remaining;
        if (filled > row_capacity) {
          filled = row_capacity;
        }
        remaining -= filled;
        row_filled[r] = filled;
        if (filled <= 0 || row_capacity <= 0) {
          continue;
        }
        const int row_y = area_y + r * row_h;
        /* No black backing rect (player-reported, earlier fix) — draws
         * straight onto the pane background like every other resource
         * counter here. */
        const int iy = row_y + (row_h - ih) / 2;
        /* Player-reported: this is a progress bar, not a stretched fill —
         * icon slot positions are fixed by the row's total capacity, not by
         * how many are currently filled, so hammers pack in from the left
         * and only reach the right edge once the row is actually full
         * (instead of re-spreading across the whole width on every icon
         * added). */
        if (row_capacity <= 1 || pane_w <= iw) {
          ss_blit_sprite(&view->icons, COLONY_ICON_HAMMER, framebuffer, px, iy);
        } else {
          const int span = pane_w - iw;
          for (int i = 0; i < filled; ++i) {
            const int ix = px + (i * span) / (row_capacity - 1);
            ss_blit_sprite(&view->icons, COLONY_ICON_HAMMER, framebuffer, ix, iy);
          }
        }
      }
      /* Dense rows also get a number, drawn last so it sits on top of that
       * row's (overlapping, hard-to-count) icons instead of replacing them —
       * player-reported: numbers had been replacing the icons outright. */
      if (dense && font) {
        for (int r = 0; r < 4; ++r) {
          if (row_filled[r] <= 0) {
            continue;
          }
          char num[12];
          snprintf(num, sizeof(num), "%d", row_filled[r]);
          colony_screen_draw_outlined_number(font, framebuffer, px + 1, area_y + r * row_h, num, 15);
        }
      }
    }
    const int tools_cost = bt ? bt->tools_cost : unit_tools;
    if (tools_cost > 0 && font) {
      snprintf(line, sizeof(line), "(Requires %d Tools)", tools_cost);
      const int line_w = font_text_width(font, line);
      /* Player-reported: 6px higher (fits inside the box now — the old
       * py+46 sat 1px past COLONY_PANEL_CONTENT_H's bottom edge) and
       * centered horizontally. Grey when the colony already has enough
       * tools in store, white (the "needs attention" color used elsewhere
       * in this screen) when short. Index 8 (dark grey), not 7 — font.c's
       * draw_ff_glyph hardcodes color==7 to the same white AA blend as
       * color==15 (FF_COLOR_MAP, "unbold white"), so 7 renders
       * indistinguishable from white here; 8 hits the plain solid-ink path
       * instead. */
      const bool tools_ok = colony->stock[COLONIZE_CARGO_TOOLS] >= tools_cost;
      font_draw_text(
        font, framebuffer, px + (pane_w - line_w) / 2, py + 46 - 6, line, tools_ok ? 8 : 15
      );
    }
  }
}

/*
 * Frame for one colony-screen sub-dialog (audit CO-5): clamp the width to the
 * framebuffer, centre it horizontally at the caller's y, draw the standard
 * wood popup, and record the rect so colony_screen_hit_test can map a click
 * back onto the same rows that were drawn. The six pickers each wrote this
 * block out by hand. The height is clamped by the caller (each derives it
 * from its own row count first).
 */
