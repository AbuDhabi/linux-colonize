# Americas idle campaign (`golden_idle_campaign america`)

STATUS: alignment started 2026-10-19. Not a CTest gate yet. Sister campaigns (done):
[idle_campaign.md](idle_campaign.md), [idle_independence.md](idle_independence.md), whose method,
ignore list and DOSBox recipes apply here too.

## Fixtures

`original_saves/golden_idle_america_seed-100/` (readme.md there): VR_SOUND.EXE (seed 100, no
music picks), original Americas map, Spanish human (`head.human_player = 2`). Saves
`000001`..`000465` are every-turn autosaves (COLONY09), turn 9 (1501) .. turn 507 (1799). Turns
whose autosave went to the decade slot are missing (33 gaps, for example 17 -> 19); the harness
plays through them. The archive is one continuous DOS session from a load of 000001; the player
only presses Space.

Human slot 2 means England (0) and France (1) act after the day-top spent-MP clear and before
the autosave, so their spent bytes are compared; the Netherlands (3) act after it.

## Running

```
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign america 1 465
./build/debug/golden_idle_campaign america N-1 N
```

The whole archive takes over an hour once the run has diverged; use `america 1 N` for a prefix.
Single pairs from a DOS save are hints only: a load drops unsaved DOS state, and 459 of 464
pairs differ (2026-10-19), most of them past save 80 by hundreds of fields.

## Divergence log

First continuous run (HEAD f0cc3bb4, 2026-10-19): saves 2..5, 7, 9, 10 byte-identical; small
diffs through save 26; hundreds of fields from save 27. After round 1: 2..20 byte-identical; 2..22 after the 5d04 retype fix; 2..26 after the 0def else-limb; 2..27 after the LCR burial fix; 2..31 after the save-28 fixes; 2..37, 39, 40 after the save-32 haul fix.

| Save | Class | State |
|---|---|---|
| 6, 11, 13, 18 | New England/France units carry spent MP 3/12, DOS 0 | FIXED: Europe immigrant / 5c3c ship buy / 5d04 hire / Artillery spawn set `moves = 0`; FUN_1427_06b4 raw 7716 leaves +0x3149 spent at 0, so the unit keeps `units_max_mp` |
| 8 | `nations[0].rebel_sentiment` 5 vs 3 (self-heals at 9) | FIXED: `ai_king_sol_percent` counted the Col1 record of a colony the pool abandoned that tick (DOS deleted it, raw 58154). Lead: a colony founded earlier in the same turn has no Col1 record until export, so it is not counted (does DOS count it?) |
| 12..26 | `indians[7].alarm_by_player[1]` 0 vs 7 | FIXED: FUN_15eb_06d2 reads the 26e4 claim table built at the colony tick top, not the live map, and 07c2 prices without the purchased test; a Pioneer's purchase earlier in the tick still raises alarm (colony.c claim snapshot) |
| 19..23 | England's ship 0 reaches the high seas one turn late | FIXED: at an own berth 20e6's local_a8 is the carrier stack (0920 parks hull + boarded at -2,-2), not the colony tile's residents, so an emptied hull takes the 457e sail-home arm |
| 21, 22 | AI unit re-typed in Europe keeps old MP (spent 9 vs 0) | FIXED: the 5d04 hire ladder's type write (raw 92770-92783) leaves +0x3149 spent alone; `units_set_type_keep_spent` |
| 23..26 | `tribes[51].alarm[0].friction` 6 vs 0 | FIXED: a non-generous Brave visit after another village's generous one (`contact_state` 2) takes LAB_5bfb_0def's else-limb (FUN_5bfb_022e raw 96981-96986) and zeroes the visiting village's word; `ai_contact_try_village_reparations` skipped it |
| 27 | DOS has one more unit, tribe frictions | FIXED: Dutch dragoon's LCR burial mounds. `units_lcr_nearest_tribe_dist` used Manhattan + first-wins; FUN_4cc6_0356 (raw 80944-80987) uses `map_dos_dist` octile and `<=` (last wins), shifting the claim draw. The +100 alarm goes through the whole FUN_4cc6_00f2 (rand(0,10) escalation tail), not the bare relation delta |
| 28 | Brave facings, unit 64 path | FIXED (DOSBox from-load 27 trace): (1) FUN_5fef_1b0e raw 100753-100758: on an attacker loss the winner's promotion roll precedes the loss outcome and raid handoff; (2) the 20e6 wander fog enable (bVar20, raw 88620) calls FUN_2a1f_047c with 1, not 0, so adjacent Braves switch the far-probe fog terms off |
| 32, 33 | England Caravel units[58] full of sugar/furs roams instead of sailing home | FIXED: port-only early return in `ai_euro_try_ship_trade_haul_snapshot` (no tools/lumber/ore/muskets/horses/food and no free hold -> 0) skipped DOS's occupancy gate (raw 2166-2168 -> LAB_3fa6, sail home 'E') |
| 38 | 8 new England colonists in Europe (ai_plan 'X') get orders 0, DOS 1 | analysing |
| 41, 42 | England unit 66 goto (50,41) vs DOS back at (45,40); 42 blows up (Dutch colony 3, unit count) | analysing |
| 28+ | not analysed | deferred to the next continuous pass |

Pair-only (deferred, not seen in the continuous run): pairs 16..26 show
`colonies[5].ai_flags.short_defenders` True vs False and `specialty_cargo` 15 vs 8 from a load;
pairs 3..5 miss a colonist joining colony 1. Likely unsaved DOS state; revisit only if the
continuous run shows them.
