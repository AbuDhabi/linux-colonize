# Changelog

Player-facing changes. Bracketed numbers are [bugs.md](bugs.md) row ids (closed
rows live in [docs/archive/bugs_closed.md](docs/archive/bugs_closed.md)).

## Unreleased — since 0.5-alpha

Covers `0.5-alpha` (2026-09-05) to 2026-09-18: 152 commits, ~115 user-reported
bug rows resolved.

Two themes dominate. First, the rival-European and native AI stopped being an
approximation: the original's own decision logic now runs end to end, and around
five thousand lines of Linux-only invented AI behaviour were deleted. Second, a
long audit pass removed invented rules throughout the game — Founding Father
effects, native mechanics, combat rules and trade flows that came from wikis and
guesswork rather than the 1994 binary — and replaced them with what the original
actually does. Expect rival nations to behave noticeably differently, and expect
a handful of familiar "features" to be gone because they were never real.

### Highlights

- Colonies with a Shipyard can build ships — Caravel, Merchantman, Galleon,
  Privateer and Frigate, at the original's costs. Previously the build list held
  only Artillery and Wagon Trains. [481]
- Rival AI ships no longer sail in visible loops or wobble in place for dozens of
  turns; idle ships roam and warships engage. [449] [451]
- Rival AI land forces are no longer passive during the War of Independence; the
  war now actually resolves. [522]
- Trading with a foreign European colony works the way the original does: a
  loaded ship or wagon bumping the colony opens the real trade dialogs, where
  they name the cargo they want and you choose goods or gold. It now requires
  Jan de Witt (otherwise you are refused for mercantilism), is blocked during
  war, and is human-only — the AI never attempts it. The old invented "dock
  inside a foreign colony and transfer cargo freely" flow is gone. [475]
- Difficulty affects combat again, as in the original: beginner handicaps that
  were missing are in, on Discoverer an undefended colony of yours cannot be
  taken in the first 80 turns and your own attacks hit twice as hard, and a
  fabricated Discoverer penalty on your attacks is gone.
- Turn order follows the original's interleaving — some rivals, then the natives,
  then the remaining rivals, then the King — rather than a simplified
  human-first order.

### Colonies and production

- Hammer production now debits lumber every turn. Lumber used to drop correctly
  one turn and then bounce back to full the next while carpenters kept working.
  [466]
- Production previews now match what the turn actually produces (food, hammers,
  and the lumber consumed).
- Sons of Liberty percentage updates the moment a colonist joins or leaves,
  instead of at end of turn. [447]
- Schoolhouse teaching progress survives save and reload, and exported colonists
  no longer look fully trained when they are not. [506]
- Specialists in a colonist's job menu gained the **Clear Specialty** option,
  returning them to Free Colonist. [431]
- Running out of tools now reverts the right specialists instead of flattening
  them to plain Colonists, and on-the-job skill learning happens at most once per
  job. [509]
- Cargo can be dragged straight from one docked ship into another's hold, without
  a detour through the warehouse; shift-dragging performs a real transfer with an
  amount prompt instead of being misrouted through warehouse logic. [432]
- A colony's "enemy ship blockading the port" flag, which gates the Custom House,
  no longer gets stuck — it refreshes every turn.
- Clicking an already-built Carpenter's Shop (and some other buildings) no longer
  claims it has not been built. [430]
- The warehouse-full spoilage warning quotes the correct existing stock, capacity
  and deposited amounts. [433]
- Opening a colony lands on the Construction view rather than Production. [443]
- The construction cost popup uses green text, right-aligns hammer and tool
  costs, and pages through long lists instead of splitting into columns. [436]
- The build preview no longer shows a hammers restriction that does not exist in
  the original.

### Combat

- Attacking a foreign colony always asks for confirmation, including an
  undefended one. [437]
- Destroyed, sunk and demoted units pixelate away with the original's dissolve
  effect instead of vanishing or leaving a stale sprite. Damaged ships now
  disappear inside that animation instead of sitting visible on the map until
  your next turn. [462]
- Ships sunk by coastal forts play the sink animation. [464]
- Coastal forts fire only on enemies and Privateers — no longer on nations you
  are at peace with. [465]
- Ships sailing past enemy warships, Forts and Fortresses are slowed or stopped
  and lose movement points, with the original's escape messages. This replaces an
  AI-only invented "naval ambush". [455]
- Native raids were badly broken: villages could attack and gift in the same
  encounter, raid without ever reporting losses, and re-raid the same colony free
  of charge every turn. Fixed. [442]
- A ship that loses to a coastal fort sails on undamaged; the port used to freeze
  its movement.
- Paul Revere's colony auto-defender no longer spends 50 muskets from the
  warehouse — that was invented. It swaps in a temporary Soldier that always
  disappears after the fight.
- Braves attacking Artillery auto-lose, as in the original; the port matched that
  rule too broadly before.
- Post-battle promotion checks the right nation, and only converts soldiers to
  Continental Army under the right conditions. [504]
- The Combat Analysis dialog shows icons beside the bombardment, ambush/terrain,
  fort tier and village-type rows, and the mislabelled "Tories"/"Rebels" rows now
  read "Tory Unrest"/"Rebel Unrest".

### War of Independence

- The King's reinforcements generate Regulars and Cavalry for the correct side.
  [505]
- Royal Expeditionary Force units move under the full rival-nation turn logic
  instead of a special-case hunting routine.
- The Crown's invasion force is seeded at the start of the game rather than at
  your declaration, at the original's per-difficulty sizes, and grows by a saved
  royal purse that buys units periodically — replacing an invented
  growth-on-tax-event rule.
- When the Crown's land force runs dry it falls back to a difficulty-scaled Tory
  uprising against one of your colonies, with the original's trigger chance and
  targeting.
- The intervention force is sized from your ally's actual military strength with
  a fixed composition per landing, replacing an invented "three landings on hard"
  rule.
- The King's Man-O-War sails back out to the high seas after a naval battle
  instead of disappearing.
- A tribe defecting to the Crown shows the original's text, and its musket and
  horse adjustment uses the right value instead of over- or under-arming it.
- Trying to sail for Europe after declaring independence explains itself in a
  popup rather than a status-bar line.
- Popups about rival monarchs weighing independence no longer appear absurdly
  early in the game. [424]

### Europe, trade and the Crown

- The King's tax audience was rebuilt from a fabricated schedule onto the
  original's turn- and year-based system, with the real scoring formula — rebel
  sentiment, tax rate, treasury, population, turn count — deciding whether and by
  how much taxes move. It shows the real text for each trigger too: victory,
  royal wedding, war, Naval Act, Stamp Act, instead of one generic message.
- Negotiations gained the original's **Military Assistance** request.
- The trade-route sea-stop picker shows a proper destination list.
- Paying back taxes on a boycotted good raises the original's Pay/Cancel dialog
  quoting the sum, and tells you when you cannot afford it, rather than silently
  doing nothing.
- Veteran Soldiers recruited, trained or bought in Europe can arrive already
  mounted as Dragoons, as in the original. [508]
- Damaged ships with no repair port go straight to Europe for repairs instead of
  loitering on the map. [463]
- Custom House and European Status sale lines scroll three times faster. [425]
- The Merchantman's order box is drawn in the right place, and Master Carpenters
  carrying tools look like colonists rather than Hardy Pioneers. [419] [452]

### Natives and diplomacy

- Cancelling a talk popup with Escape no longer wedges diplomacy permanently —
  every later meeting used to fail silently until you restarted. [461]
- Contact and encounter popups for different tribes or nations no longer
  interleave; one meeting finishes before the next starts. [427]
- Moving into a native settlement walks all the way in and opens the menu instead
  of stopping a tile short. [418]
- The village menu lists its options in the original's order, with Attack near the
  end. [501]
- A village holding a grudge follows through and attacks after you refuse a
  demand; distant villages with no grievance can no longer demand anything at
  all; and gifts or demands happen once per real encounter rather than back to
  back. [417] [439] [441]
- Trade, haggle and demand dialogs show the chief's portrait. [422] [426]
- A chief's tales of nearby lands reveal land on the same continent only, instead
  of also uncovering ocean and other landmasses. [492]
- Treaty and diplomacy messages use the correct nationality ("Dutch", "Spanish")
  instead of wrong or post-independence country names. [468]
- Buying horses from a village tops the wagon's hold up to 100 and spills the rest
  into a free slot, instead of producing an illegal 135-horse stack. [471]
- A stray map marker could prompt you to meet a tribe you had never approached.
  Fixed. [416]
- Disturbing a burial ground checks whether you have actually met the tribe
  before applying the relations penalty. [497]
- Removed an invented mechanic where villages could randomly displace or kill
  your Scouts. [499]
- Speaking with a chief who turns hostile plays its sound effect, and a failed
  Cibola search no longer counts against your treasure limit. [502]
- The Incite Indians target menu lists every nation; it used to hide ones you
  supposedly could not afford, which is not how the original behaves.
- Villages breed horses over time, and a village's herd decides whether it can
  field Mounted Braves rather than plain ones.
- Fixed relations disagreeing between how you see a tribe and how the tribe sees
  you.
- The surprise-raid warning is tied to actual war state rather than an internal
  attack counter, and native aggression build-up now reads the right data.
- Razing a native capital, or a village hosting your mission, announces itself;
  the mechanic worked but was silent. Wiping out a tribe's last village now
  announces the tribe's extinction.
- A new sidebar readout — an addition, not in the original — shows what a village
  buys and sells once you have visited it.

### Rival European AI

- The rival-European and native AI now run the original's own logic for goal
  selection, Europe hiring and buying, land unit orders, colony management,
  village growth and Brave movement.
- Rival ships no longer loop endlessly or wobble near one spot for dozens of
  turns. [449] [451]
- Rival ships flee coastal fort fire correctly instead of dawdling beside
  batteries and getting sunk. [470]
- Rival nations can no longer attack you right after signing a treaty without
  declaring war; a sneak attack now triggers the declaration (Privateers
  excepted). [472]
- Rival nations receive cross-driven immigrants and draw from their Europe
  recruit pool; previously they accumulated crosses to no effect. [479]
- Rival colonies staff indoor jobs with the original's logic, choose to build
  units as well as buildings, and Jefferson correctly doubles their bell output.
  [480] [483]
- Rival Scouts explore, patrol and visit villages using the original's behaviour
  instead of an invented wander that left them idle, stay exploring in wartime,
  and are properly mounted when hired in Europe. [493] [494] [495] [498]
- Rival garrisons stop disbanding themselves into colonists. [512] [515]
- Rival Privateers now unload, deliver and sell captured cargo properly. [518]
- Rival nations no longer over-recruit Veteran Soldiers and other experts from
  Europe, and no longer send Soldiers, Pioneers or Colonists on ship-only
  missions such as founding overseas colonies. [510] [511]
- Scenario starts (America and similar) sail their fleets in from Europe and land
  their colonists, instead of stalling on an order to sail to the tile they were
  already on. [490]
- Rival Europeans no longer form or break alliances with each other — that
  machinery was a port invention the 1994 game never had.
- Removed, as inventions with no basis in the original: land units auto-boarding
  transports, Artillery hunting siege targets, evacuating threatened troops,
  ships attacking neighbours on their own initiative, hunting distant enemies,
  building special forts under threat, hand-picking colonists for expert jobs,
  and special-case tool delivery and Europe selling. [513] [516] [517] [519]
  [520]
- The Discoverer trait's free Veteran Soldier is human-only again, and AI Cortes
  no longer collects an invented treasure windfall. [478] [488]

### Map, movement and exploration

- Ships bought in Europe reveal fog on arrival instead of sitting blind until
  moved. [421]
- A "this unit cannot attack" warning during a Go To order no longer traps you in
  a popup loop. [469]
- A unit that has spent its moves cannot disembark until next turn. [423]
- Wagon Trains can no longer be loaded onto ships. [482]
- A Treasure Train fills a Galleon: one per ship, not six. [484]
- Units aboard a ship cannot attack an adjacent enemy unit, village or colony
  from the deck — land first. [485]
- Ships can no longer sail onto the unreachable strip past the western sea lane.
  [429]
- Rival ship movement is visible only near your own units and colonies, not on
  every tile you have ever explored. [450]
- Hills and mountains on the Americas map no longer show forest underneath. [446]
- New-game fleet starting positions are correct; a fleet could previously start
  many tiles from its intended landing spot. [487]
- The Fountain of Youth ruin grants immigrants to rival nations too. [496]
- **Go to Port** and **Go to Place** now open a real destination picker listing
  your coastal colonies plus Europe, with the original's titles and rows.

### Founding Fathers

- Elections resolve at the start of your turn rather than only after you end it.
  [434]
- John Paul Jones' Frigate starts docked in Europe and sails over like a
  purchase, instead of appearing beside a colony. [467]
- Added missing election effects: Bolivar's loyalty boost, Brebeuf upgrading
  existing missions, Las Casas' conversions and Pocahontas' anger reset. [477]
- Removed effects that never existed: Franklin's instant peace, Magellan's extra
  movement bonus, La Salle's yearly stockade re-sweep and Fugger's automatic
  boycott clearing. [476]

### Interface and presentation

- Nearly every message drawn from the game's own text file is now wired with its
  real wording and choices; rows still using approximate or port-written text
  dropped from roughly 190 to 2.
- The Continental Congress bell bar, the Religion cross bar and the chief
  portraits on the report screens match the original pixel for pixel. The bell
  bar no longer collapses into a solid black block, the Indian Adviser's mood
  headband uses the right sprite and position, and the chief portrait in meet and
  diplomacy dialogs sits where it should.
- Aztec and Inca settlements show their proper city (and capital) icon in the
  sidebar instead of a generic village. [414]
- Fixed a palette glitch that put grey patches in the Iroquois chief portrait and
  others. [415]
- Unskilled missionaries wear their own plain sprite rather than the expert
  Jesuit's. [420]
- The Indian Advisor report uses the correct small font for tribe names and
  advancement level. [428]
- The colony sidebar shows singular job titles ("Veteran", "Expert") as the
  original does. [507]
- A Scout entering a foreign colony gets its own menu — meet the mayor,
  infiltrate, attack, or nothing — and both "Meet with Mayor" and "Nothing" now
  work instead of redisplaying the dialog. [438] [453] [454]

### Sound

- The wagon-wheels effect plays when a Wagon Train reaches a colony, not on every
  step. [440]

### Packaging and platform

These landed in commits made a few hours after the `0.5-alpha` tag itself, so
they are already present in the published 0.5-alpha downloads; they are listed
here because they are not in the tagged tree.

- A Windows build: a standalone `colonize.exe` bundling SDL2, FluidSynth and the
  soundfont. You still supply the original game data.
- The Linux release is a single self-contained binary requiring only glibc 2.17
  or newer (any 64-bit distro from roughly 2014) plus X11 or Wayland, rather than
  a folder of libraries.
- Music plays through a bundled TinySoundFont synthesizer when FluidSynth is
  unavailable, instead of dropping to square-wave beeps.
- Soundfont selection moved from an environment variable into `settings.json`,
  which also gained a setting to force which synthesizer is used.
- The window scale option is clamped to a sane 1–8.
- Releases ship third-party license texts and source URLs for every bundled
  library.

### Under the hood

- The codebase was split into a simulation library and a UI library, with the
  dependency direction enforced at link time.
- Three code-smell audits and two duplication audits drove the
  invention-removal and de-duplication passes above.
- Documentation was consolidated: the AI plans merged into one port plan,
  oversized documents split, and conventions / debug-variable / archive indexes
  added.
- A machine-readable map of the rival-European AI control flow was added, with a
  tool that renders it and checks it against the source.
- A naming audit renamed misleading identifiers; the largest source files gained
  section indexes.
- Debug logging now covers Custom House sales, European money movements, Europe
  recruiting and training, and which option you picked in a popup. [410] [411]
  [412] [413]
- Substantial test growth (unit plus golden and smoke AI gates); obsolete DOSBox
  tracing tooling removed.
- Fidelity work with no visible effect in play: a stray dice roll removed from
  chief meetings, a veteran-status check tightened, a mismatched ship-bump
  message corrected, and the rival AI's course-setting and goal-walk rewired onto
  the original's own saved unit fields. [486] [491] [503] [523] [525] [526]

### Investigated, no change

Reports checked against the original and found already correct: native alarm
under Pocahontas [435], Brave wander range [444], horse gifts [445], the odds of
a chief sharing tales [448], the attack/defense stat pairing [514], rival Scouts
visiting your colonies [500], and turn-1 nation placement [489] — though that
last one turned up and fixed a save-compatibility bug for ships en route to
Europe.

### Still open

[473] bogus ship-damaged popup after a land combat; [474] diplomacy text filling
the wrong name tokens; [521] [524] [527] [528] [529] rival-AI fidelity residue.
