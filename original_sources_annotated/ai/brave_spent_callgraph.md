# Quiet Brave spent call graph

Focused RE for seed-100 mid-turn **moves_spent** (`0x3149`) on the quiet NEW
WORLD path. Not a raid / Euro planner map.

## End-to-end (quiet)

```
FUN_4d56_1816 indian_nation_turn
  └─ while unit_has_moves_remaining (097a → 1427_13b0)
       └─ indian_unit_act   [behavioral target of Ghidra func_0x00042191]
            │   ASM body: FUN_4d56_14fe (Ghidra abs 42191 collides with 41f2)
            ├─ indian_pick_dir  (stub 4c3b → FUN_4d56_021a; the quiet
            │                     LAB_521d_4ea9 scorer sits one level below
            │                     it, reached via thunk 291f:012c)
            ├─ dir==8 → unit_exhaust_mp (0934 → 1427_155e)   [stay]
            └─ dir!=8 → step_unit_in_dir (2a1f_0150 → 465b_0c1e)
                 └─ FUN_465b_0000 move_spent_add
                      ├─ cost head → local_40
                      ├─ ADD local_40 to 0x3149
                      ├─ ocean force → spent=max_mp   [ruled out: dump_b465f3]
                      └─ post-ADD chrome (Section 6)  [no Brave 3149 write]
```

Annotated sources:

| Piece | File |
|-------|------|
| Nation turn + 14fe act | `ai/indian_nation_turn.c` |
| Cost / ADD / chrome | `ai/move_spent.c` |
| `FUN_1427_*` MP helpers | `ai/unit_mp.c` |
| Quiet dir score | `ai/quiet_brave_scoring.c` |

## Every `0x3149` writer on this path

| Site | Function | Writes | Quiet T2 holdout? |
|------|----------|--------|-------------------|
| `465b:01ce` | early foreign remaining<3 → `0934` | yes | **cannot** (bVar4 foreign only) |
| `465b:05f0` | ADD `local_40` | yes | fires (Sioux AL=9, Apache head=6) |
| `465b:0628` | ocean force → `090c`/`065a` | yes | **cannot** (f3) |
| `465b:08f8` | `0934`/`155e` if cargo/wagon + colony | yes | **cannot** (type 19) |
| `465b:0bd1` | act>0x13 → `0934` | yes | **cannot** (first act) |
| `1816` loop | act≥0x15 → `0934` | yes | **cannot** |
| `14fe` stay | dir==8 → `0934` | yes | **cannot** (XY moves) |
| Post-ADD `0916/0948/08da/084e/07fe/07d6/08e4/088a` | tile/stack chrome | **no** | — |
| After `465b` RETF | unlabeled / conditional `0934`? | **open** | **suspect** (vrb465x2) |

## Ruled out vs open

**Ruled out (static + dumps):** cost-head caps from presence/ocean-adj/capital;
ocean force; in-465b cargo exhaust; stay / act-spin exhaust; inventing
`spent = max` when cost>max; **`465b:01ce` early exhaust** (requires
`foreign_tile` / `bVar4` — quiet holdouts are friendly land).

**RESOLVED 2026-09-08 (static, no hang needed).** The writer is
`FUN_5bfb_022e`'s exhaust tail (`LAB_5bfb_1005`, decomp ~97094): 465b's own
commit tail calls `FUN_281f_0984` (adjacent-foreign probe, decomp 75794) →
`FUN_2a1f_0192` → `FUN_5bfb_3180` (encounter resolver) → `2a1f_066c` →
`FUN_5bfb_022e`. On FIRST contact (met bit 0x20 clear) the ceremony runs and
the tail does `if (local_1a && mover nation > 3) 0934(mover)` — spent :=
max MP (= 3, Brave row of DS:0x5234). Answer to the hang question: at 465b
RETF Sioux spent is ALREADY 3.

Evidence: TURN3's only two vis-bit Braves are exactly the two spent-3
holdouts; unmet French (50,38) / Spanish (47,53),(47,54) land units adjacent
to the dest tiles; Euro phase runs BEFORE the Indian phase (dump_1816 /
vr_2a02_v3: Euro units at TURN3 positions while Braves hold TURN2 positions
with spent reset to 0). TURN5's fresh-vis Brave keeps spent=9 because the
pair was already met (0x20 set → already-met arm early-outs, no exhaust).

**Trap:** the Section-6 post-ADD table below only walked the `1427_*` chrome
thunks and missed the `0984`/`0192` far-call pair — the write hides two
overlay hops away, inside 465b, before RETF.

## Port status

Ported 2026-09-08: `ai_contact_indian_meet_trade` first-meet arm sets
`brave->moves_left = units_max_mp(...)` (natives keep the DOS spent byte in
`moves_left`). `k_quiet_brave_t2` overlay retired from `src/core/ai.c`;
`golden_ai_turns`/`golden_ai_joint` + full ctest 60/60 green.
