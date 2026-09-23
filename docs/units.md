# units.h design notes

Design/rationale prose moved out of `src/core/units.h` so the header stays
declaration-focused. Each heading below names the symbol the prose documents;
content is verbatim from the original header comment.

## units_set_native_fallout_context

Optional post-win native settlement fallout context for
units_resolve_land_combat_ff (FUN_5fef_31ea-shaped). When col1/map are non-NULL
and attacker beats defender nation>=4, units_try_native_settlement_fallout runs.
conquest_gold: caller-known treasure amount, or -1 → Cortes peels FUN_5fef_31ea
amount via combat rng (non-Cortes still skips). Pass NULL map to disable.

## units_set_combat_europe

Europe screen for the DOS damaged-ship teleport (FUN_5fef_0352 raw 99610-
99623 / asm 5fef:0bc0-5fef:0cf9): a ship that loses at sea and has no own
Drydock/Shipyard colony is unlinked from its tile and re-placed at
(nation-20, nation-20) — the off-map Europe slot — on the spot. Pass NULL
to clear (headless callers leave it unset and keep the ship on its tile).

## units_set_native_combat_chrome_owned

ai_contact's ambush arm draws its own @INDIANWIN1/2 chrome (muskets/horses
seizure line, chief portrait), so it sets this around its
units_resolve_land_combat call to keep the generic native-attacker
@INDIANWIN0/@INDIANLOSE chrome in units_combat_outcome_popups quiet.
Every other native attack path (braves stepping onto human tiles via
units_try_move, alarm marches) gets the generic chrome.

## units_last_native_gear_step

FUN_5fef_1b0e's `bVar13` / `bVar14` gear-step flags from the last land loss
resolved (bugs.md #645). `armed` = the brave stepped to Armed (@INDIANWIN
tag suffix '1'), `mounted` = stepped to Mtd. (suffix '2'). The type step and
the tribe tally happen inside the combat path; these are only the chrome
picks for the owner of the @INDIANWIN popup. Clear before a resolve.

## units_set_combat_music_hooks

Optional sound.c hooks for the DOS-evidenced combat "Military" BGM sting
(SOUND_MILITARY_BGM_ID, see sound.h) — kept as function pointers rather
than a direct link so units.c stays linkable without sound.c (several
unit_* test binaries compile units.c standalone). Pass NULL/NULL to
clear; game_loop wires the real sound_play/sound_active_song_id once at
startup, matching units_set_combat_popups's wiring convention.

## ColonizeUnitType cap_bits

NAMES.TXT @UNIT column 12, the 8-character capability BIT-STRING
(DOS `DS:0x523d + type*0xe`, loader raw 121132-121134 reads it with the
bit-string reader FUN_2a1f_0b2e, not the numeric one). MSB-first: the
leftmost character is bit 7, so "00111100" = 0x3c (row 4 Dragoons) and
"00011100" = 0x1c (row 7 Cont. Cav.). Consumed by the AI capability
tables (FUN_521d_20e6 / the goal walk) that used to hardcode it.

## ColonizeUnit park_nights

Port-only nights-parked counter for the units_wake MP refund (DOS derives
wake MP from the spent byte alone; +0x16 is the shared treasure-clock /
repair-timer / route-stop / cower counter and must not be borrowed for
this). Not serialized: a freshly loaded parked unit imports its real
moves, so no refund is needed before the first turn refresh.

## ColonizeUnit mp_spent_turn

Port-only "this zero is a SPEND, not a park" flag for units whose
moves the port zeroes for bookkeeping reasons. Boarding parks a
passenger at moves 0 while DOS's spent byte (+0x3149) may hold
either 0 (loaded in port) or max_mp (walked aboard from open shore —
the 465b_05ca ocean force-to-max), and the two cases behave
differently: FUN_4720_015c only offers landfall to cargo whose
spent byte is BELOW its max (viceroy_unpacked.c:76010-76026). Set when
the port zeroes an allotment that DOS would have spent; cleared by the
per-turn refresh (DOS clears every spent byte at the day top, viceroy
6355-6357). Not serialized — a reloaded unit imports its real
moves / spent byte.

## ColonizeUnit aboard_moves

Port-only: the allotment (thirds) a passenger still has while it rides
in a hold, i.e. max_mp minus DOS's spent byte +0x3149, which DOS leaves
untouched aboard — the ship's move and dock path never write a
passenger's +0x3149 (only the day-top reset FUN_130d_0290 does). Needed
because `moves` is the hold's park zero while aboard. -1 = full
allotment (nothing spent this turn / unknown). Set at boarding, read
when the passenger wakes or is put ashore, reset by the per-turn
refresh. Round-trips through the Col1 spent byte (bugs.md #544).

## ColonizeUnit col1_hold_raw

Raw DOS unit bytes +0x0c..+0x15 (holds_occupied, cargo_item nibbles,
cargo_hold[6]) exactly as loaded. DOS repurposes this region on land
units for state the port doesn't model (french-campaign originals:
braves carry a per-settlement counter in hold[2]; Euro land units and
even ships carry 196/216/236 in hold[5] — not pioneer tools). Kept so
capture can round-trip those bytes instead of fabricating sentinels.

## ColonizeUnit ai_landfall_wait

Port-only, not saved (COLNXEXT does not carry it; a save/load mid-opening
loses it, same as repair_pending above). AI first-colony landfall goto
memory: set alongside `orders = UNITS_ORDER_SENTRY` at every AI unload
site that means "wait ashore for the next landfall act", cleared once the
unit gets a real order. Exists because DOS's own `+0x314c == 1` after an
AI unload is just the leftover "aboard ship" value (`FUN_1427_10be`
writes it, raw 8297/8674) that `FUN_521d_0a60`'s turn-top clear (raw
87560-87564) wipes back to 0 on the unit's own nation's next turn — it
carries no landfall memory in DOS. The port used to piggyback that same
`orders == SENTRY` value to remember "this unit is mid-landfall", which
collided with the DOS clear. Cite: bugs.md #528.

## Unit construction raw-code decode

Colony construction raw-code decode — DOS-LITERAL FUN_15eb_32f8
(viceroy_unpacked.c raw 13423-13448), reached from the colony EOT via
FUN_281f_0cc2 -> FUN_364b_0114 (raw 56897):

  code < 0       -> kind 0 (no project)
  code < 0x2a    -> kind 1, @BUILDING index = code
  code - 0x2a < 7 -> kind 2, @UNIT index = code - 0x1f

@BUILDING has exactly 0x2a = 42 rows, so the seven unit codes 42..48 map to
@UNIT rows 11..17: Artillery, Wagon Train, Caravel, Merchantman, Galleon,
Privateer, Frigate. Man-O-War is @UNIT row 18 and is NOT reachable — the
`< 7` bound is what excludes it (FUN_15eb_38ba/38e8 likewise stop the
build-menu walk at code 0x30).

## units_build_project_info

Name + colony cost of a unit construction project — DOS-LITERAL
FUN_15eb_33aa kind-2 arm (viceroy_unpacked.c raw 13482-13509), thunked as
FUN_281f_0ac4 and read by the colony EOT at raw 57742:

  hammers = 0x5239[idx] * 0x20;
  if (hammers < 0x28) hammers = 0x28; else if (hammers < 0x34) hammers = 0x34;
  tools   = 0x523a[idx] * 10;

0x5239 / 0x523a are the @UNIT "cost" / "tools" columns (stride 0xe from
DS:0x5230, the same record `space` reads at 0x5238). Artillery = 6*32 = 192
hammers / 4*10 = 40 tools (golden-confirmed); Wagon Train = 1*32 = 32, which
the first clamp lifts to 40.

The table is cached by units_load_types, so this takes no pool (the UI, the
turn loop and the dialogs all resolve raw codes without one). Falls back to
the shipped NAMES.TXT columns when no catalog has been loaded (tests).
Returns false for anything that is not a unit project code.

## units_equip_role_type_name

Destination @UNIT type name for a COLONIZE_EJECT_* equipment change: the
flat DS:0x2f5 @JOB->@UNIT row DOS re-types through (FUN_15eb_0916). The
tier is NOT preserved — a Continental or royal body that changes its gear
lands on the plain colonial type, exactly as in DOS. cur_type_index is
unused and kept only for call-site shape. See units.c.

## ColonizeUnitKind DOS type codes

DOS @UNIT type codes (COLONIZE/NAMES.TXT @UNIT row order, which is what DOS
stores in unit +0x3146 and what every `type < 0xb` / `type == 0x12` range
test in the decompile means; ai_euro.c:11742 ai_euro_20e6_dos_type carries
the same table). A Linux pool index is NOT a DOS code — synthetic fixtures
place types at arbitrary slots — so the mapping goes through the @UNIT name.

## units_name_kind classification rules

Classify an @UNIT type name. Every spelling that any call site in the tree
used before the predicates landed is accepted, most-specific first:
  7  "Cont. Cav" / "Continental Cav"
  9  "Cont. Army" / "Continental Army" / bare "Army"
  6  "Regular"
  8  "Cavalry" / "Cav." / bare "Cav"
  18 "Man-O-War" / "Man-o-War" / "Man O War" / "Man of War" / "Man-O'-War"
  14 "Merchantman"  15 "Galleon"  16 "Privateer"  17 "Frigate"  13 "Caravel"
  10 "Treasure"     11 "Artillery" / "Cannon"     12 "Wagon"
  22 "Mtd. Warrior" / "Mtd Warrior" / "Mounted Warrior"
  21 "Mtd. Brave" / "Mtd Brave" / "Mounted Brave"
  20 "Armed Brave"  19 "Brave"
  4  "Dragoon"      5  "Scout"    2  "Pioneer" / "Hardy"
  3  "Missionar" / "Mission" / "Jesuit"
  1  "Soldier"      0  "Colonist"
Returns UNITS_KIND_UNKNOWN for a name none of those match.

## units_kind_type_index

Pool slot for a @UNIT row / ColonizeUnitKind. Unit types are loaded from
NAMES.TXT @UNIT in file order, so the kind IS the row index; this only
range-checks it against what the catalog actually provided. Prefer it over
units_find_type(pool, "Artillery") — the port compiles no MicroProse names
of its own, and a renamed row must not break the lookup.

## units_is_missionary

Unit-level missionary test (AC-34 / IN-45): the @UNIT Missionaries type or
the NAMES @JOB 24 Missionary profession, which is what "Jesuit Missionaries"
is in this port (units_display_name never spells "Jesuit"). Widest of the
three former rules (ai_contact "Mission", ai_euro "Missionary"|"Jesuit",
col1_bridge "Missionary").

## units_reveal_sight_w

FUN_13f1_02f8 → 0158 unit sight reveal with the DOS per-tile side effects
(FUN_13f1_000a): seen bit; unowned non-rumour tiles get the nation's owner
nibble (FUN_137f_0228); units on the tile get this nation's vis bit
(FUN_1427_09ac — natives only inside the |d|<2 core); a colony on the tile
gets its pop/fort snapshot (FUN_364b_1b4c). colonies / col1 may be NULL.
Returns true when the core ring touched a Pacific-strip water tile
(FUN_13f1_0158 DS:0x1e8 arm) — caller decides on the woodcut.

## units_reset_hooks

Test-fixture hygiene: process-global callback hooks (move/combat watch,
dissolve, raid-repelled, popup pump, bgm, tax-change) persist across
units_reset(pool) since they aren't per-pool state. Call this to put them
all back to their unregistered (NULL) initial values, e.g. at the top of a
shared test fixture's setup, so one test's registrations can't leak into
the next.

## units_reset_state

Per-unit-id shadow state that outlives units_reset(pool) because it is
indexed by unit_id rather than owned by the pool: the goto anti-backtrack
shadow (s_units_goto_last_dir). Unit ids are reused by a fresh pool on New
Game / Load, so without this a slot's stale direction from the outgoing
campaign could false-positive the anti-backtrack check for a unrelated
unit that happens to reuse the same id. Call at the same point as the
other new-game/load resets (ai_init_new_game, game_apply_col1_save).

## units_spawn_euro_starter_fleet

Spawn European starter fleet (ship + Pioneer + Soldier) at (x,y).
FUN_75c2_235c (raw 121612-121647): type 0xd ship (0xe for the Dutch, nation 3),
then type 2 Pioneers, then type 1 Soldiers. Profession overrides: French
(nation 1) Pioneers → @JOB 0x14 Hardy Pioneer; Soldiers → @JOB 0x15 Veteran
Soldier when the nation is Spanish (2) **or** this nation is the human
(`is_human`) on difficulty < 2. Returns ship unit id or -1.

## units_job_icon_sprite

ICONS.SS index per NAMES.TXT @JOB profession (0..28) for a colonist
working inside a colony / waiting on a dock — no field equipment, unlike
the on-map UNITS_ICON_* sprites; -1 if a profession has no dedicated
portrait (currently none). Job 18's sprite exists but the Expert Teacher
colonist type was cut from the final DOS game (unreachable leftover).

## units_type_default_job

DOS DS:0x30e indexed by @UNIT type — the default @JOB that type carries,
-1 when the type has no profession slot at all (FUN_15eb_0902, reached as
FUN_281f_0b78). True for the colonist-carrying types only; ships, wagons,
artillery and treasure trains are false. DOS uses this, not a unit's own
profession byte, to decide who is a person: the census population count
(FUN_4962_0018 → DS:0x9410) and the sidebar profession line both gate on it.

## units_top_on_map_tile

Which unit (if any) draws on the map at (x,y): prefers the selected unit
(subject to selected_visible's blink-off hide), else highest id — except
on a colony tile (map_tile_has_city), which never shows a non-selected
garrison unit at all, only the active/selected one while it's actually
visible. -1 = nothing drawn. Exposed (not just used internally by
units_render_on_map) so this rule is directly testable without a
framebuffer/sprite sheet.
