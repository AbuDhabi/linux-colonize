#!/usr/bin/env python3
"""Rebuild VR_B465R/A/L/F/X/E from stock VICEROY.EXE.

ADD1 hang builds (R/A/L): CALL into reloc-safe stub in the force-max window
(same overlay page as ADD1). Do NOT use distant cave 0x3ECD0 (EMS evm0015).

Force-max probe (F): leave ADD1 stock; replace force-max window with JGE-skip
+ CMP BX,Sioux + hang.

Exit probe (X): leave ADD1 + force-max stock. JMP from LAB_465b_0c19 (0x3F909)
into a 14-byte stub at 0x3F90E (start of FUN_465b_0c1e — dump-only overwrite).
Stub rebuilds BX, hangs on Sioux, else RETF. Do NOT put the exit stub in the
force-max window: that overwrites the ocean JGE and RETFs every unit after ADD
with no xy commit (`dump_vrb465x2`).

Do NOT patch the act_counter>=0x14 CALL 0934 chrome at 0x3F8FE — that is not
the common exit (first-step Braves JC over it to 0c19).

0934 probe (E): hang at FUN_1427_155e IMUL for Sioux (`--with-e`).

See tools/brave_dump/midturn_465b.md.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COLONIZE = ROOT / "COLONIZE"
VICEROY = COLONIZE / "VICEROY.EXE"

OFF_ADD1 = 0x3F2D3
OFF_E4D2 = 0xE4D2
OFF_FORCE = 0x3F31D  # JGE + force-max body (17 bytes)
OFF_STUB = 0x3F31F  # CALL target = ADD inside R/A/L stub (after EB 0F)
OFF_EXIT = 0x3F909  # LAB_465b_0c19: POP SI; POP DI; LEAVE; RETF
OFF_EXIT_STUB = 0x3F90E  # after RETF NOP — was FUN_465b_0c1e prologue
OFF_0934_IMUL = 0x7BDD  # FUN_1427_155e IMUL BX,SI,1C

STOCK_ADD1 = bytes.fromhex("00874931")
STOCK_FORCE = bytes.fromhex("7d0fff76069a0c091f1883c40288844931")  # 17 bytes
STOCK_EXIT = bytes.fromhex("5e5fc9cb")
STOCK_EXIT_STUB_SITE = bytes.fromhex("558bec568b5e088a87be00986b76")  # 14 bytes
STOCK_E4D2 = bytes.fromhex("bb40008ec3bb6c00268b07268b5702cb")
STOCK_0934 = bytes.fromhex("6bde1c888749315ec9cb")
E4D2_TRAMPOLINE = bytes.fromhex("b86400ba0000cb909090909090909090")

REL16_ADD = (OFF_STUB - (OFF_ADD1 + 3)) & 0xFFFF
PATCH_ADD1 = bytes([0xE8, REL16_ADD & 0xFF, (REL16_ADD >> 8) & 0xFF, 0x90])

REL16_EXIT = (OFF_EXIT_STUB - (OFF_EXIT + 3)) & 0xFFFF
PATCH_EXIT = bytes([0xE9, REL16_EXIT & 0xFF, (REL16_EXIT >> 8) & 0xFF, 0x90])


def force_stub(cmp_imm_le: bytes) -> bytes:
  body = (
    bytes.fromhex("eb0f")
    + bytes.fromhex("00874931")
    + bytes.fromhex("eb02")
    + bytes.fromhex("9090")
    + bytes.fromhex("81fb")
    + cmp_imm_le
    + bytes.fromhex("74fec3")
  )
  if len(body) != 17:
    raise SystemExit(f"force stub len {len(body)}")
  return body


def force_stub_logger() -> bytes:
  body = (
    bytes.fromhex("eb0f")
    + bytes.fromhex("00874931")
    + bytes.fromhex("891e0070")
    + bytes.fromhex("a20270")
    + bytes.fromhex("c3909090")
  )
  if len(body) != 17:
    raise SystemExit(f"logger stub len {len(body)}")
  return body


def force_probe_sioux() -> bytes:
  body = bytes.fromhex("7d0f81fbf80174fe") + (b"\x90" * 9)
  if len(body) != 17:
    raise SystemExit(f"probe stub len {len(body)}")
  return body


def exit_hang_sioux() -> bytes:
  """14 bytes at 0x3F90E: rebuild BX, hang Sioux, else RETF. No force-window reloc."""
  body = (
    bytes.fromhex("6b5e061c")  # IMUL BX,[BP+6],1C
    + bytes.fromhex("81fbf801")  # CMP BX,0x1F8
    + bytes.fromhex("74fe")  # JZ hang
    + bytes.fromhex("5e5fc9cb")  # POP SI; POP DI; LEAVE; RETF
  )
  if len(body) != 14:
    raise SystemExit(f"exit stub len {len(body)}")
  return body


def hang_0934_sioux() -> bytes:
  """13 bytes at IMUL: filter hang (dump-only; skips MOV for non-match path)."""
  body = bytes.fromhex("6bde1c81fbf80174fe5ec9cb90")
  if len(body) != 13:
    raise SystemExit(f"E stub len {len(body)}")
  return body


STUB_R = force_stub(bytes.fromhex("f801"))
STUB_A = force_stub(bytes.fromhex("a401"))
STUB_L = force_stub_logger()
STUB_F = force_probe_sioux()
STUB_X = exit_hang_sioux()


def build_ral_f(
  *,
  out_name: str,
  stub: bytes,
  patch_add1: bool,
  use_e4d2_tramp: bool,
) -> Path:
  raw = bytearray(VICEROY.read_bytes())
  if raw[OFF_ADD1 : OFF_ADD1 + 4] != STOCK_ADD1:
    raise SystemExit(f"stock ADD1 mismatch at {OFF_ADD1:#x}")
  if raw[OFF_FORCE : OFF_FORCE + len(STOCK_FORCE)] != STOCK_FORCE:
    raise SystemExit(f"stock force-max mismatch at {OFF_FORCE:#x}")
  if len(stub) != 17:
    raise SystemExit(f"stub must be 17 bytes, got {len(stub)}")

  if use_e4d2_tramp:
    raw[OFF_E4D2 : OFF_E4D2 + len(E4D2_TRAMPOLINE)] = E4D2_TRAMPOLINE
  if patch_add1:
    raw[OFF_ADD1 : OFF_ADD1 + 4] = PATCH_ADD1
  raw[OFF_FORCE : OFF_FORCE + 17] = stub

  out = COLONIZE / out_name
  out.write_bytes(raw)
  return out


def build_x() -> Path:
  raw = bytearray(VICEROY.read_bytes())
  if raw[OFF_ADD1 : OFF_ADD1 + 4] != STOCK_ADD1:
    raise SystemExit("X: stock ADD1 mismatch")
  if raw[OFF_FORCE : OFF_FORCE + len(STOCK_FORCE)] != STOCK_FORCE:
    raise SystemExit("X: stock force mismatch")
  if raw[OFF_EXIT : OFF_EXIT + 4] != STOCK_EXIT:
    raise SystemExit(
      f"X: stock exit mismatch at {OFF_EXIT:#x}: {raw[OFF_EXIT:OFF_EXIT+4].hex()}"
    )
  if raw[OFF_EXIT_STUB : OFF_EXIT_STUB + 14] != STOCK_EXIT_STUB_SITE:
    raise SystemExit(
      f"X: stub site mismatch at {OFF_EXIT_STUB:#x}: "
      f"{raw[OFF_EXIT_STUB:OFF_EXIT_STUB+14].hex()}"
    )
  if raw[OFF_E4D2 : OFF_E4D2 + 16] != STOCK_E4D2:
    raise SystemExit("X: stock E4D2 mismatch")
  # Force-max stays stock. Epilogue JMP → stub after RETF.
  raw[OFF_EXIT_STUB : OFF_EXIT_STUB + len(STUB_X)] = STUB_X
  raw[OFF_EXIT : OFF_EXIT + 4] = PATCH_EXIT
  out = COLONIZE / "VR_B465X.EXE"
  out.write_bytes(raw)
  return out


def build_e() -> Path:
  raw = bytearray(VICEROY.read_bytes())
  stock = raw[OFF_0934_IMUL : OFF_0934_IMUL + 10]
  if stock != STOCK_0934:
    raise SystemExit(
      f"E: stock 0934 mismatch at {OFF_0934_IMUL:#x}: {stock.hex()}"
    )
  stub = hang_0934_sioux()
  follow = raw[OFF_0934_IMUL + 10 : OFF_0934_IMUL + len(stub)]
  raw[OFF_0934_IMUL : OFF_0934_IMUL + len(stub)] = stub
  out = COLONIZE / "VR_B465E.EXE"
  out.write_bytes(raw)
  print(f"E: overwrote trailing {follow.hex()} after stock 0934")
  return out


def verify_ral_f(
  path: Path,
  *,
  stub: bytes,
  expect_add1: bytes,
  expect_e4d2: bytes,
  expect_pat: bytes | None,
) -> None:
  b = path.read_bytes()
  assert b[OFF_ADD1 : OFF_ADD1 + 4] == expect_add1, path.name
  assert b[OFF_FORCE : OFF_FORCE + 17] == stub, path.name
  assert b[OFF_E4D2 : OFF_E4D2 + len(expect_e4d2)] == expect_e4d2, path.name
  if b[0x3ECD0 : 0x3ECD0 + 16] != b"\x00" * 16:
    raise SystemExit(f"{path.name}: unexpected bytes in old cave 0x3ECD0")
  if expect_pat and expect_pat not in stub:
    raise SystemExit(f"{path.name}: missing {expect_pat.hex()}")
  print(
    f"OK {path.name}: ADD1={b[OFF_ADD1:OFF_ADD1+4].hex()} "
    f"force={stub.hex()} e4d2={'tramp' if expect_e4d2 == E4D2_TRAMPOLINE else 'stock'}"
  )


def verify_x(path: Path) -> None:
  b = path.read_bytes()
  assert b[OFF_ADD1 : OFF_ADD1 + 4] == STOCK_ADD1, path.name
  assert b[OFF_FORCE : OFF_FORCE + 17] == STOCK_FORCE, path.name
  assert b[OFF_EXIT : OFF_EXIT + 4] == PATCH_EXIT, path.name
  assert b[OFF_EXIT_STUB : OFF_EXIT_STUB + len(STUB_X)] == STUB_X, path.name
  assert b[OFF_E4D2 : OFF_E4D2 + 16] == STOCK_E4D2, path.name
  assert REL16_EXIT == 2, REL16_EXIT
  chrome_call = b[0x3F8FE : 0x3F8FE + 11]
  assert chrome_call == bytes.fromhex("ff76069a34091f1883c402"), chrome_call.hex()
  print(
    f"OK {path.name}: ADD1=stock force=stock exit={PATCH_EXIT.hex()} "
    f"stub@{OFF_EXIT_STUB:#x}={STUB_X.hex()} e4d2=stock"
  )


def main() -> int:
  if not VICEROY.is_file():
    print(f"missing {VICEROY}", file=sys.stderr)
    return 1
  args = set(sys.argv[1:])
  print(f"ADD CALL rel16={REL16_ADD:#06x} EXIT JMP rel16={REL16_EXIT:#06x}")
  specs = [
    ("VR_B465R.EXE", STUB_R, True, True, PATCH_ADD1, E4D2_TRAMPOLINE, bytes.fromhex("81fbf801")),
    ("VR_B465A.EXE", STUB_A, True, True, PATCH_ADD1, E4D2_TRAMPOLINE, bytes.fromhex("81fba401")),
    ("VR_B465L.EXE", STUB_L, True, True, PATCH_ADD1, E4D2_TRAMPOLINE, bytes.fromhex("891e0070")),
    ("VR_B465F.EXE", STUB_F, False, False, STOCK_ADD1, STOCK_E4D2, bytes.fromhex("81fbf801")),
  ]
  for name, stub, patch_add, tramp, exp_add, exp_e4, pat in specs:
    out = build_ral_f(
      out_name=name, stub=stub, patch_add1=patch_add, use_e4d2_tramp=tramp
    )
    verify_ral_f(out, stub=stub, expect_add1=exp_add, expect_e4d2=exp_e4, expect_pat=pat)
  verify_x(build_x())
  if "--with-e" in args:
    e = build_e()
    b = e.read_bytes()
    stub = hang_0934_sioux()
    assert b[OFF_0934_IMUL : OFF_0934_IMUL + len(stub)] == stub
    print(f"OK {e.name}: 0934={stub.hex()}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
