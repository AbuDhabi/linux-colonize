# Cheats

Player reference for the cheat mode that DOS Colonization 3.0 shipped
(COLONIZE/README.TXT, "Cheat Mode") and that OpenCol reproduces. MicroProse's own
warning still applies: these were development tools and were never polished for
players. Save first.

## Turning cheat mode on and off

- **On:** on the main map press **Alt-W**, **Alt-I**, **Alt-N** in that order. Any
  other key in between restarts the sequence. The **CHEAT** menu appears in the menu bar.
- **Off:** press any one of Alt-W, Alt-I or Alt-N while cheat mode is on.
- The switch is stored **in the save game**, not in `settings.json`. A save made
  with cheats on loads with the CHEAT menu showing. Other saves are not affected.
- All effects listed below need cheat mode on. The only exceptions are a map view
  picked with Reveal Map and Debug Info Flags; see their entries.

> **Warning: F10 ends the game.** With cheat mode on, **F10** (and REPORTS > Score)
> skips the Score report and goes straight into the retire sequence: score,
> exploits screen, Hall of Fame, then the title screen. There is no confirmation.
> This is DOS behaviour. Turn cheats off before you press F10.

## CHEAT menu (main map)

| Item | Hotkey | Effect |
|---|---|---|
| Create Unit | Shift-F1 | Puts a new unit on the tile under the map cursor. See below. |
| Debug Info Flags | Shift-F2 | Opens a checklist of debug displays. See below. |
| Reveal Map | Shift-F4 | Changes whose map you see. See below. |
| Set Human Player | Shift-F5 | Lets you switch sides. See below. |
| Kill Indians | Shift-F6 | Destroys a whole tribe. See below. |
| Advance Revolution Status | Shift-F7 | Moves the War of Independence on by one step per use. See below. |
| Sound Test | menu only | Asks for a number (0-255) and plays that sound effect. |
| Memory Check | menu only | Shows memory use and how full the unit and colony tables are in the status line. DOS showed its own memory pools here; OpenCol has none, so it shows the closest equivalents. |
| Show Strategy | Shift-F8 | Turns a map overlay on or off. For each computer-run European nation it labels the tile of that nation's current top AI goal, for example `F:FOUND` (French, found a colony). The other labels are CONTACT, LABOR, MILIT, COLONY, MILEXP and COLONY2. Your own nation is not shown. |
| Show Colony Sites | Shift-F9 | Turns a map overlay on or off. It marks the best colony site each AI nation has found so far, for example `S*` for the Spanish. |
| Test Routine | Shift-F10 | Shows the number of units and colonies in the game in the status line. |

The two overlays and the debug flags are not stored in the save.

### Create Unit (Shift-F1)

- The unit is placed on the tile under the map cursor. The game's normal unit
  limit still applies.
- Rows: Colonist, Pioneer, Soldier, Missionary, Scout, Artillery, Wagon Train,
  Treasure, Ship, four Indian unit types, and Foreign Unit.
  - **Treasure** is always worth 2800 gold. You are not asked for an amount.
  - **Ship** opens a second list: Caravel, Merchantman, Galleon, Privateer,
    Frigate or Man-O-War.
  - **The Indian rows** create the unit for the tribe whose village is nearest
    the cursor. Nothing is created if there are no villages.
  - **Foreign Unit** does not create a unit. You pick a European nation, the list
    reopens, and the next unit you create belongs to that nation.
- After you declare independence, the Indian rows and Foreign Unit are replaced by
  Continental Army and Continental Cavalry (yours), and Regulars and Cavalry (the
  Crown's).
- A created unit is visible to all four European nations at once.
- README.TXT warns that putting units of different nations on the same tile, or
  on a foreign colony or an Indian village, "may cause significant problems in the
  game".

### Debug Info Flags (Shift-F2)

The checklist has seven flags. Only two of them have a visible effect in OpenCol:

- **Indian AI movement:** when an Indian brave picks a move, the score of each
  neighbouring tile is drawn on the map.
- **Foreign AI planning modes:** on units of computer-run European nations, the
  letter on the unit badge shows the unit's internal AI plan code instead of its
  order. `E` means a plan code of 128 or higher, and an empty box means no plan.
  This one also needs cheat mode on.

Anger & Friction Levels, Supply and Demand (Indians), Close Moves, Far Moves and
All Movement can be ticked but show nothing.

### Reveal Map (Shift-F4)

- **English / French / Spanish / Dutch Map:** you see the map with that nation's
  fog of war. This is a view only. Your own fog of war (what you have explored) is
  not changed.
- **Complete Map:** no fog at all, plus the following:
  - Rival moves are animated wherever they happen, not only near your units
    and colonies. The *Show Foreign Moves* and *Show Indian Moves* game options
    still apply.
  - GAME > Find Colony finds any colony, even one you have never seen.
  - The right-hand panel shows full details of any nation's units, and lists
    foreign stacks in full.
  - The Foreign Affairs report shows the full detail that normally needs Jan de
    Witt.
  - Picking Complete Map switches off the *Show Indian Moves* game option.
- **No Special View** (or Esc) returns to your normal view. This row is listed
  only while a special view is on.
- The chosen view is stored in the save and stays after you turn cheats off.
- With cheats on, the location line in the right-hand panel ends with the
  tile's region number when a unit is selected as well, not only for empty
  tiles. With Complete Map and cheats on, unit lines in the panel also start
  with the unit's internal index.
- You cannot open a foreign colony's screen from the map. To see inside
  another nation's colonies, use Set Human Player.

### Set Human Player (Shift-F5)

- Choosing a nation makes you that nation. All other nations, including your
  old one, become computer-controlled. Gold, tax rate, the Europe docks and trade
  routes switch to the new nation's.
- Every nation that had left the game is put back under computer control, as
  in DOS. That includes nations removed by the War of the Spanish Succession or
  by your declaration of independence.
- **None** does nothing in OpenCol. In DOS it handed every nation to the computer
  and could run the game unattended. OpenCol does not have that mode yet.

### Kill Indians (Shift-F6)

- Lists the tribes that still exist. Nothing opens if every tribe is gone.
- Destroys every village of the chosen tribe, as if you had conquered them. Units
  that belong to those villages go with them, and you get the usual message that
  the tribe has been wiped out.

### Advance Revolution Status (Shift-F7)

Each use does the next step that has not happened yet:

1. Raises rebel sentiment to 75%. If a War of the Spanish Succession is due, it
   happens now.
2. Declares independence with no confirmation, and the first wave of the Royal
   Expeditionary Force arrives.
3. Announces the foreign intervention.
4. Forces your victory ("Independence forced at end of next turn"). At the next
   check the War of Independence ends in your favour, even if the Crown still has
   colonies, land units or reinforcements left.

## Main map: Shift-D (disband) in View mode

With cheat mode on, Shift-D in View mode works on any tile, not only your own:

- If there are units on the tile, the top unit is disbanded after the usual
  confirmation, whoever owns it.
- An empty colony, yours or foreign, is deleted after a confirmation.
- An Indian village is destroyed at once, with no confirmation.

## Colony screen keys

These keys work only on the main colony screen, not in its sub-panels.

| Key | Effect |
|---|---|
| T | Adds a Free Colonist to the colony, up to the population limit. |
| S | The selected colonist becomes an expert in the job they are doing now. For example, a colonist farming becomes an Expert Farmer. If the selected unit is outside the colony, it gets the default profession for its unit type. |
| $ | Adds 1000 gold to the treasury of the nation that owns the colony. |
| % | Adds 100 of the selected good (click it in the warehouse first). The warehouse limit is ignored. |
| ^ | Removes 100 of the selected good. The stock does not go below 0. |
| ! | Gives the colony every building at its highest level, and raises the Warehouse and Capitol levels by one. Press it only once. |
| Space | Runs one free production turn for this colony only. The rest of the world does not move. Each press is another turn, so food can run out. If the colony starves to death you are returned to the map. README.TXT notes that this can leave odd graphics, which go away when you leave the colony screen. |

## Europe screen keys

| Key | Effect |
|---|---|
| $ | Adds 1000 gold. |
| ] | Raises the tax rate by 1%. |
| [ | Lowers the tax rate by 1%. |

OpenCol keeps the tax rate between 0% and 100%. In DOS it could wrap around.

## Extra numbers in reports and dialogs

While cheat mode is on:

- **Religious Advisor (F2):** "(x of y)" over the crosses bar: crosses collected
  and crosses needed for the next immigrant.
- **Continental Congress (F3):** the heading line ends with "(x in y)": how many
  liberty bells are still missing, and how many the next Founding Father needs.
- **Combat Analysis** (when the game option is on): each side gets two extra
  rows. The attacker's side shows its final attack strength and the random roll.
  The defender's side shows its final defence strength and the sum of both. The
  roll is drawn from 1 to that sum and decides the battle.
