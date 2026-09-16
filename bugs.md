This is a user-maintained list of bugs. User puts reports in. Agents may annotate with a resolution (Status FIXED + one-sentence resolution). User verifies, then rows are moved to [docs/archive/bugs_closed.md](docs/archive/bugs_closed.md).

IDs in the `#` column are permanent and never reused; the archive holds #1-#455. **Next free ID: 456.** Cite rows as `bugs.md #NNN`.

Status: OPEN = no resolution yet. FIXED = agent claims a fix, awaiting user verification. CLOSED / REFUTED = verified, archived.

| # | Status | Bug Description | Resolution |
|---|--------|------------------|------------|
| 461 | FIXED | Scout "Meet With Mayor" does nothing (no diplomacy dialog opens with the colony owner). | Talk machine wedge: an Esc on any FUN_5bfb_153e talk popup left `s_talk.active` set, so every later encounter (Meet With Mayor, adjacent-unit talk) returned 0 silently until restart. Cancelled DIPLO_TALK results now end the talk (`ai_diplo_apply_popup_result`), and new game / load call `ai_diplo_talk_reset`. Headless repro: Meet → Esc → Meet queued no DIPLO_TALK before, queues greeting + CHOICE after. |
| 462 | OPEN | Upon being damaged, my Merchantman didn't properly disappear; the dissolve animation appears to have been rendered under it. It was still visible until my turn. | |
| 463 | OPEN | Pretty sure that when ships are damaged and go to Europe by fallback, they don't appear in the inbound lane, but rather are instantly teleported there and put on a timeout (as normal for damaged ships). Check DOS. | |
| 464 | OPEN | When ships are defeated by coastal forts, they should have the dissolve animation happen and then disappear. Animation was missing and the sprite didn't disappear when I expected it to. | |
| 465 | OPEN | Coastal forts should not fire on neutrals or allies. At present they seem to fire on just about anyone; even nations I'm at peace with. | |
| 466 | REFUTED | Something is wrong with hammer production and lumber consumption. In a colony where hammer production capacity exceeded lumber production, lumber seemed to go down on one turn (as it should; extra was taken from stockpile), and then the next turn returned to maximum (it should have continued to decrease, since usage exceeded production). | REFUTED 2026-09-16: the alternation is real DOS behaviour — hammers (and therefore the 1:1 lumber debit) freeze on every Autumn tick, so the Lumberjack's income refills the warehouse on the off turn; real-DOS pair `original_saves/colony-prod-tests/COLONY00_no-transports.SAV` (Spring 1680) -> `COLONY01_no-transports.SAV` (Autumn 1680) shows all 32 colonies with `hammers` byte-for-byte unchanged and lumber only rising (New Amsterdam 74->98), and the port matches (new `unit_hammers_lumber_two_turns` in `tests/unit/test_turn.c` locks strict two-turn drain pre-1600 and Spring-spends/Autumn-frozen post-1600). |
| Pretty sure John Paul Jones' frigate is supposed to appear on a sealane tile somewhere, not adjacent to one of your colonies. Not sure. | |
| "New Netherlands and New Spain have signed a new treaty." Incorrect country names; they are both pre-independence, and even after independence, the names would be different. | |
| "This type of unit cannot attack" during go-to locks the user out of proceeding further. Popup simply reappears. | |
