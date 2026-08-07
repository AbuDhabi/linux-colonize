# Quiet Brave spent call graph

Focused RE for seed-100 mid-turn **moves_spent** (`0x3149`) on the quiet NEW
WORLD path. Not a raid / Euro planner map.

## End-to-end (quiet)

```
FUN_4d56_1816 indian_nation_turn
  └─ while unit_has_moves_remaining (097a → 1427_13b0)
       └─ indian_unit_act   [behavioral target of Ghidra func_0x00042191]
            │   ASM body: FUN_4d56_14fe (Ghidra abs 42191 collides with 41f2)
            ├─ indian_pick_dir  (4219b / quiet LAB_521d_4ea9)
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
`spent = max` when cost>max.

**Open:** writer that leaves ADD=9 (or head=6) then ends golden spent=3 **after**
ADD / after `465b` returns. Hang target:

- Recipe: [`tools/brave_dump/midturn_465b.md`](../../tools/brave_dump/midturn_465b.md) **VR_B465X**
- Question: at `465b` RETF, is Sioux spent already 3 or still 9?
- If still 9 → next CALL after return (likely conditional `0934`/`155e`).
- If already 3 → unlabeled write inside late chrome missed by static table.

## Port status

No T1-safe spent rule found from this annotation. Linux keeps
`k_quiet_brave_t2` overlays in `src/core/ai.c`. Do not drop until hang X (or a
proven predicate) closes the two residuals.
