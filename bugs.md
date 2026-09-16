This is a user-maintained list of bugs. User puts reports in. Agents may annotate with a resolution (Status FIXED + one-sentence resolution). User verifies, then rows are moved to [docs/archive/bugs_closed.md](docs/archive/bugs_closed.md).

IDs in the `#` column are permanent and never reused; the archive holds #1-#455. **Next free ID: 456.** Cite rows as `bugs.md #NNN`.

Status: OPEN = no resolution yet. FIXED = agent claims a fix, awaiting user verification. CLOSED / REFUTED = verified, archived.

| # | Status | Bug Description | Resolution |
|---|--------|------------------|------------|
| 461 | FIXED | Scout "Meet With Mayor" does nothing (no diplomacy dialog opens with the colony owner). | Talk machine wedge: an Esc on any FUN_5bfb_153e talk popup left `s_talk.active` set, so every later encounter (Meet With Mayor, adjacent-unit talk) returned 0 silently until restart. Cancelled DIPLO_TALK results now end the talk (`ai_diplo_apply_popup_result`), and new game / load call `ai_diplo_talk_reset`. Headless repro: Meet → Esc → Meet queued no DIPLO_TALK before, queues greeting + CHOICE after. |
| Upon being damaged, my Merchantman didn't properly disappear; the dissolve animation appears to have been rendered under it. It was still visible until my turn. | |
| Pretty sure that when ships are damaged and go to Europe by fallback, they don't appear in the inbound lane, but rather are instantly teleported there and put on a timeout (as normal for damaged ships). Check DOS. | |
| When ships are defeated by coastal forts, they should have the dissolve animation happen and then disappear. Animation was missing and the sprite didn't disappear when I expected it to. | |
| Coastal forts should not fire on neutrals or allies. At present they seem to fire on just about anyone; even nations I'm at peace with. | |
| Something is wrong with hammer production and lumber consumption. In a colony where hammer production capacity exceeded lumber production, lumber seemed to go down on one turn (as it should; extra was taken from stockpile), and then the next turn returned to maximum (it should have continued to decrease, since usage exceeded production). | |
