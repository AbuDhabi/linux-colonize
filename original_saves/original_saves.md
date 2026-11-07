Most of these saves (in the `original_saves` folder) are taken from the original DOS. Do not modify these without explicit instructions to do so. They serve as authoritative data for expected outcomes.

`mapgen/DOS_PLANES_S<seed>.bin` (tile|mask|path planes, 3 x 4176 bytes, at `FUN_684c_08c0`'s epilogue) and
`mapgen/DOS_PRERIVER_S105.bin` (tile|layer2 at the `FUN_684c_04a6` river-pass entry) are DOSBox memory
dumps of VR_SEED map generation (2026-11-07, port_bugs.md #1559); `unit_map_gen` compares against them.
