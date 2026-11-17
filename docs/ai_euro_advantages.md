# Euro AI: rules that differ from the player

Every place where a computer-controlled European nation plays by different rules
than the human: resources it receives without earning them, shortcuts past the
unit/transport mechanics the player must use, difficulty terms that favour it,
and the handicaps it carries. All entries are ported DOS behaviour (cite in the
named function); none is a port invention. Collected 2026-11-15 as the starting
list for later AI work. Changing any of them is a divergence from DOS and
belongs behind a mod/divergence switch (docs/project_goals.md,
docs/known_divergences.md).

Control flow context: [ai_euro_logic_map.yaml](ai_euro_logic_map.yaml).
Difficulty tables: [difficulty.md](difficulty.md). Combat terms: [combat.md](combat.md).

`diff` = difficulty 0 (Discoverer) .. 4 (Viceroy). "AI" = nation ≤ 3 with
`player[n].control != 0` (DOS `0x543f` table) unless noted.

Sections: 1 Gold · 2 Goods into colonies · 3 Tile improvement · 4 Colonists ·
5 Production · 6 Europe and natives · 7 Combat · 8 Restraints on the AI ·
9 Checked, no difference

## 1. Gold

| # | Difference | Rule | Where |
|---|-----------|------|-------|
| G1 | Free gold every turn | `score = colonies + (year−1500)/50` (0 before turn 20, ×2 after 1699); `gold += 4·diff·score`, ×1.5 Governor, ×2 Viceroy. Discoverer gets 0; harder levels get more | `ai_euro_europe.c` `ai_euro_5d04_treasury_bump` (FUN_521d_5d04 head) |
| G2 | Gold topped up to ship prices | Treasury raised to at least: Caravel price (1000) when the nation has no ships; Privateer (2000) when privateer-threatened; Frigate (5000) when frigate-threatened. The ship-buy ladder then buys that hull | `ai_euro_5d04_apply_naval_gold_floors`, prices from the FUN_521d_5c3c table DS:0x978d |
| G3 | Colony surplus sold with no transport | Warehouse overflow sold to Europe from the colony itself, untaxed, at the sell price, before spoilage | `europe_market.c` `europe_ai_colony_dump_sell_w` (FUN_364b_0688 phase O, raw ~57806-57848) |
| G4 | Price cap on war goods | AI/Crown bid for Horses (8), Tools (14), Muskets (15) capped at `((4−diff)·3)/2 + 3`: 9 on Discoverer, 3 on Viceroy | `europe_harbor.c` `europe_market_phase4` (raw 58994-58999) |

## 2. Goods that appear in colonies

All four arms run inside the AI colony tick (FUN_5952_035e,
`ai_euro_colony_tick_5952`); the goods land in the colony's stock with no ship,
wagon or Europe trip, inland colonies included.

| # | Difference | Rule | Where |
|---|-----------|------|-------|
| C1 | Tools delivered | When the colony wants Pioneer work (or every 10th turn) and has < 20 tools: +20 tools, paid at the sell price (Europe ask − 1) | `ai_euro_land.c` `ai_euro_5952_tools_supply_and_connect` (raw 94370-94388, port_bugs.md #640) |
| C2 | Emergency lumber | No lumberjack seated, lumber < 2, turn % 8 == 0: +100 lumber **unconditionally**; 200 gold is charged only if the purse holds 200 | `ai_euro_colony_jobs.c` `ai_euro_5952_lumber_purchase` (raw 94680-94689) |
| C3 | Starter horses | Turn ≥ 40, horses < 2, a unit on the tile, gold ≥ 10: stock set to 2 horses for 10 gold | `ai_euro_5952_seed_horses` (raw 95959-95972) |
| C4 | Musket bank | Garrison not short, muskets > 199, bank < 20 lots: −50 muskets, +1 lot in nation `+0x49`. The Europe hire arm spends a lot to arm a recruit for free, so muskets move colony → Europe with no ship | `ai_euro_goals.c` (raw 94362-94367, port_bugs.md #914); spent in `ai_euro_europe.c` hire ladder |
| C5 | Project tools handed over | A unit or building project short of tools at completion has its tools stock set to the cost, then spends it: the tools are free. The player is refused (@NEEDTOOLS) | `colony_build.c` `colonies_try_complete_unit_construction`, `colonies_try_complete_building_ex` (FUN_364b_0688 raw 57748-57774, port_bugs.md #755); MOD_AI_IMPROVEMENTS drops it (#1863, #1864) |

## 3. Tile improvement without Pioneer units

| # | Difference | Rule | Where |
|---|-----------|------|-------|
| P1 | Colony improves its own plots | With 20+ tools, colony flag WANTS_PIONEER_WORK, turn % 7 != 0: picks the best ring plot, spawns a phantom colonist there with `+0x315a = 99` (job completes on the next work tick), runs the real Pioneer body (clear/plow or road), removes the phantom. Costs 20 tools from stock; no unit walks or spends turns | `ai_euro_land.c` `ai_euro_5952_improve_best_plot` (raw 94402-94551, port_bugs.md #612) |
| P2 | Roads between colonies | 20+ tools, turn % 7 == 0, 2+ own colonies on the continent: walks the path toward another colony, builds the first road gap with the same phantom; 20 tools charged | `ai_euro_5952_road_connect_0000` (FUN_5952_0000, port_bugs.md #641) |
| P3 | Pioneer units mostly join colonies | Per the code, P1 is the AI's only tile-improvement path. A Pioneer reaching a colony is absorbed (case @JOB 0x14) and its tools are refunded to stock, which then feeds P1/P2 | `ai_euro_goals.c` `ai_euro_5952_absorb_equip` |

## 4. Colonists: professions and equipment

| # | Difference | Rule | Where |
|---|-----------|------|-------|
| S1 | Experts bought in the colony | Colony short of food, tax ≤ 25%, purse ≥ the @JOB row-0 price: the last non-expert becomes Expert Fisherman (Docks, no fisherman) or Expert Farmer. No ship, no school. DOS charges the price row indexed by the colonist **slot** (bug kept literal). Also raises the AI tax by 1, no dialog | `ai_euro_colony_jobs.c` `ai_euro_5952_specialist_arms` / `ai_euro_5952_train_pick` (ARM 2, port_bugs.md #571/#572) |
| S2 | Free promotion with a schoolhouse | School chain owned and `improve_timer` high enough: a random non-expert gets an expert profession (DS:0x864 craft table; DOS writes the row index, bug kept). No teacher, no gold | same, ARM 1 |
| S3 | Random Master Carpenter | Non-expert carpenter, no Master Carpenter in colony, pop > 5: `rng(0, 16−diff) == 0` grants the specialty | `ai_euro_5952_carpenter_pick` |
| S4 | Servant/Criminal upgraded | A Servant or Criminal picked for the carpenter job is rewritten to Free Colonist | `ai_euro_5952_carpenter_pick` (FUN_281f_0cae(slot, 0x1c)) |
| S5 | Automatic re-equip | Every turn the tick absorbs units on the tile (equipment refunded) and re-types colonists into Soldier/Dragoon/Scout/Pioneer from stock. The player can do the same by hand; the AI does it in bulk with no transport or menu cost | `ai_euro_5952_absorb_equip` |

## 5. Colony production

| # | Difference | Rule | Where |
|---|-----------|------|-------|
| F1 | Food subsidy | AI colony food `+= diff >> 1` (0/0/1/1/2) | `turn_production.c` (FUN_364b_0688 Phase B, raw 57245) |
| F2 | Small famine forgiven | Food shortfall < 3 is zeroed for non-human colonies, so no starvation from it | `turn_production.c` (DS:0x8e5a, raw 57623-57625) |
| F3 | Uncapped horse breeding | AI colony gains the uncapped breeding potential (human gets the capped figure); overflow is dump-sold (G3) | `turn_production.c` horse arm (raw 57247) |
| F4 | No Tory penalty | AI Tory count treated as 0, so the `−⌊tories/(10−diff)⌋` production penalty never applies; SoL latches still add +1/+2 | `colony_production.c` `colony_prod_sol_bonus` |
| F5 | Bolivar bell subsidy | AI nation owning Bolivar: colony bells `+= (pop+3)/5` | `colony_production.c` bells composer (FUN_15eb_1f72 raw 12634-12641, port_bugs.md #938b) |
| F6 | Cheaper Founding Fathers | Base AI `(14−diff)·8` vs human `(diff+3)·16`. First FF on Viceroy: AI 40, human 56; Discoverer: AI 56, human 24 | `founding_fathers.c` `founding_fathers_bells_needed` (FUN_4345_0982) |
| F7 | AI-only Custom House gate | AI colonies use a fixed deny list (Food, Lumber, Horses, Tools, Muskets; Ore when Armory or tool/musket production) instead of the player's per-cargo bits | `europe_market.c` (FUN_364b_0636) |

## 6. Europe and natives

| # | Difference | Rule | Where |
|---|-----------|------|-------|
| E1 | Faster immigration on harder levels | AI crosses needed scaled by `(8−diff)/8`: 100% Discoverer … 50% Viceroy | `europe_harbor.c` `europe_compute_immigration_score_w` |
| E2 | Cheaper native land on harder levels | Human `score = 2(diff+3) + tech + bought − dist`, cost `65·score/2`; AI `score = tech + bought − diff − dist + 12`, cost `50·score/2` | `colony.c` `colonies_indian_land_purchase_gold` |

## 7. Combat

The AI has no strength bonus of its own; the human gets easy-level bonuses
that vanish at Viceroy (combat.md difficulty table).

| # | Difference | Rule | Where |
|---|-----------|------|-------|
| K1 | Human easy-level strength | Human Euro side `str −= (diff − 4)`: +4 Discoverer … 0 Viceroy | combat.md |
| K2 | Human colony dampers | `diff < 2`, turn < 80: attackers of human colonies/units halved or −25%; Discoverer stand-in defender cannot lose; Discoverer human attacker doubled | combat.md (raw 100536-100549) |
| K3 | Crown open-field bonus | WoI crown attacker on land `+= diff·atk/20` | combat.md |

## 8. Restraints on the AI

| # | Difference | Rule | Where |
|---|-----------|------|-------|
| R1 | Fair-play attack gates | Approach/attack of human colonies skipped while `diff·turn < 0xb5` / `< 0xc9` and the colony is unseen | `ai_euro_goals.c` (gates #1/#2, ~832 / ~918) |
| R2 | No starter veteran | Discoverer/Explorer Veteran Soldier start is human-only | `units_cargo.c` (FUN_75c2_235c, port_bugs.md #488) |
| R3 | Tax creep | Every S1 purchase raises the AI tax by 1 | `ai_king_tax_delta_ai` after S1 |

## 9. Checked, no difference found

- Construction: the AI picks projects from a fixed priority chain
  (`ai_euro_5952_build_cascade_for_owner`, port_bugs.md #483) and pays normal
  hammers/lumber. No rush-buy or free buildings found.
- Ship purchases and Europe crossings go through the shared purchase table and
  mover.
