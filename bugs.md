This is a user-maintained list of bugs. User puts stuff in, user removes. Agents may annotate with "FIXED" if they fix it. User will verify.


- Colony food production on the minimap tiles in the colony UI is mismatched with food production in the population view. I had a 1 pop colony making 2 food in the town center, but was shown in the population view to produce-and-consume 2 and 1 surplus. When I added another colonist to produce another 3 food, the food production showed 5 producd-and-consumed 1 surplus. This makes no sense - the colony had no horses to eat extra food.
  NOT REPRODUCED (2026-08-28) — needs a screenshot. Checked against your own
  save `test-saves-play/COLONY09.SAV` (Jamestown, pop 1): town-commons badge
  2 food, sum of tile badges 2, population view produced 2, eaten 2 (pop x 2),
  surplus 0, and the end-of-turn tick moves food by exactly 0. No horse
  breeding (needs >= 2 horses in store; the colony has none) and nothing else
  touches the food total, so tiles and the People band cannot disagree here.
  Two things that may explain what you saw: (a) at surplus 0 the surplus
  column is not drawn at all, so the "1" right of the food icons is the
  **crosses** meter (cross icon, 1/turn base), with bells "1" after it; (b)
  the 2-colonist reading is self-consistent — 5 produced, 4 eaten, 1 surplus.
  Like DOS, the People band shows produced and surplus but never the eaten
  amount, which makes the pair look unbalanced. Screenshot of the mismatch
  (or a save taken at that moment) and I will chase it further.
- Celebratory SFX should not be played every time the colony UI is opened. That's only when the colony is founded.
  FIXED — DOS `FUN_2f2b_6cd4` plays event 0x54 only when `DS:0x34a >= 0`, i.e.
  when a building has just finished and the screen runs its "new building
  appears" reveal. Ported as `ColonizeColony.pending_build_reveal` (set by
  `colonies_try_complete_building`, consumed on colony open); founding keeps
  its own 0x54 (`FUN_479b_076e`).
- There should not be a cannonade when the prices fall popup is shown, or when the immigration popup appears. WTF.
  FIXED — the cannon was AI-vs-AI combat resolving during end-of-turn, landing
  on top of whatever popup was on screen. DOS `FUN_5fef_1b0e` only plays the
  fire/win sounds when its `param_4` visible flag is set (`FUN_465b_0000`
  passes 1 for the viewport nation or a human side; the AI move scorer at
  `521d:52aa` passes 0). Port now gates both the event SFX and the military
  music sting on human involvement.
- A go-to order should be possible for a ship into the high seas / sea lane tile. In which case the ship should automatically return to Europe on landing on that special tile. (Normal manual movement by arrow keys should not trigger automatic returning to Europe.) In fact, it seems that I can't move a ship onto a sea lane either way.
  FIXED — the port denied any eastward step onto a lane tile; DOS
  `FUN_4720_015c` denies it only when the ship is **already on** a lane tile
  and steps further east without a Go To / Trade Route order. Entering the
  lane from ocean now works, Go To accepts a lane destination, and a Go To
  that ends on a lane tile sails the ship to Europe with its passengers and
  holds. Manual arrow-key steps onto the lane still just move.
