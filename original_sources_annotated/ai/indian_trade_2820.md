# Indian trade / meet decision (`FUN_4d56_2820` + nest)

Layer D map for the **trade shell** and nested buy/haggle/demand helpers.
Linux keeps thin auto-trade / gift-demand / hard-bargain stand-ins in
`ai_contact_*` — deep body **PARKED** for port; **mapped** here.

Related: [`indian_contact.md`](indian_contact.md). Stubs:
[`indian_trade_helpers.c`](indian_trade_helpers.c).

## Line ranges

| Piece | Decomp lines | Size |
|-------|--------------|------|
| `FUN_4d56_2820` shell | 82064–82282 | ~219 |
| Nest `2aac`…`311e` | 82286–83456 | ~1171 |
| `FUN_4d56_3582` closer | 83460–83476 | ~17 |
| Catalog “~1396” | shell + nest | — |

Sibling (not caller): `FUN_4d56_2154` @81743–82062 via thunk `2a1f_0434`
(raid-adjacent action). `2820` thunk: `2a1f_044c`. Mid-turn `1b3a` does **not**
call either.

## Call graph

```
thunk 2a1f_044c → FUN_4d56_2820(unit, ?, euro_nation)
  ├─ human Euro gate (control==0) → LCG chrome / BGM 5|6|7
  ├─ clear wagon flag 0x3158 if land unit
  ├─ init cargo-index shuffle arrays; reseed; tribe price table 291f_0ed0
  ├─ param_3 < 0 → LAB_3f41_16ea abort chrome
  ├─ empty cargo → LAB_4d56_2a9b → 3582
  ├─ multi-hold: pick trade good (local_c8) via dialog / AI
  │    LAB_4d56_2a78 cargo pick → common LAB_4d56_2a9b
  └─ dispatch → FUN_4d56_2aac
         ├─ selected_good < 0 → 2e92 (no-deal)
         ├─ AI / non-human (BP−6==0) → 2bbc
         ├─ last-goods conflict → 2b92 (player buy) or refuse subst 0x1561 → 3582
         └─ else refuse path → 3582

FUN_4d56_2b92  player buy-offer loop
  ├─ sticky last-good (tribe+7) → string 0x156a
  ├─ else price from LCG + cargo-type tables (−0x5a base 6/7…)
  ├─ accept → apply gold/goods → maybe 311e demand
  ├─ choice 2 → 2f96 haggle
  └─ choice 3 → 306c hard-bargain

FUN_4d56_2bbc  AI buy-offer (same pricing; auto choices)
FUN_4d56_2e92  no-deal → 311e or 3582
FUN_4d56_2f96  haggle: bump offer/tension; resume loop
FUN_4d56_306c  hard-bargain: worse terms + tension; resume
FUN_4d56_311e  counter-demand tribute goods + buy-back dialog
FUN_4d56_3582  friction / alarm floor helper (post-trade close)
FUN_4d56_2af6  last-goods clear + refuse dialog 0x1561 → 3582
```

## Dialog / string IDs (shell + nest)

| ID | Site | Role |
|----|------|------|
| `0x1561` | `2aac` / `2af6` | Refuse / “not interested” (with tribe name slots) |
| `0x156a` | `2b92` sticky good | Already-trading-that-good line |
| (others in nest) | `2b92`/`2bbc`/`311e` | Buy / haggle / demand bodies — see catalog one-liners; full string census PARKED for VGA |

Subst slots: `281f_0438` slots 0..3 load cargo-name ptrs from table `−0x6840`.
`291f_019c(msg, DS:0x8d52)` presents Indian dialog with nation voice index.

## Shell phases (`2820`)

1. **Peer gate** — `param_3` Euro nation 0..3 with `control==0` → `local_8=1` (human trade UI); else AI silent path.
2. **Chrome** — if human: `range(0,3)==0` may queue BGM events 5/6/7 via `0498`.
3. **Land clear** — non-ship clears `0x3158` (wagon/trade flag).
4. **Tables** — fill 0..15 index arrays; `ai_reseed_from_timer`; `291f_0ed0` builds tribe price vector @ `0x9e78`.
5. **Abort** — `param_3 < 0` → `LAB_3f41_16ea` (UI abort).
6. **Cargo** — empty holds → close via `2a9b`/`3582`; else pick `local_c8` good (human dialog or AI).
7. **Hand off** — `2aac` dispatch.

## Linux thin vs PARKED

| Behavior | Linux (`ai_contact`) | This map |
|----------|----------------------|----------|
| Auto-trade / gift | Stand-in relation bumps | Full `2bbc` / `2b92` pricing |
| Hard-bargain mid-alarm | Thin Done | Full `306c` loop |
| Gift-amount CHOICE | `ai_popup` Done | Deep nest still PARKED |
| VGA wood dialog | PARKED | `291f_019c` / `0438` subst |

## Open RE

- Exact price formulas inside `2b92`/`2bbc` (cargo-type deltas for goods 8/0xd/0xe/0xf)
- Full string ID list for haggle / demand beyond `0x1561`/`0x156a`
- Second entry into `2820` after `4528` blob (~86762) — confirm args
