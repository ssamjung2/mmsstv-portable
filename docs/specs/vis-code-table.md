# VIS code test (`test_vis_codes`)

`tests/test_vis_codes.c` is a self-contained C99 check of the VIS byte table
for the 43 modes. It does not call the library; the library's actual VIS
output is verified by `tests/test_roundtrip.cpp` (tones and decoding). All
tests are listed in [the test suite](../../tests/README.md).

## What it checks

For each mode with a VIS byte (37 of 43), the test:

1. Prints the byte LSB first, the order in which it is sent (Robot 36 = 0x88 → `00010001`).
2. Prints each bit's tone: **1100 Hz = 1, 1300 Hz = 0**.
3. Checks the parity of the whole byte against the expected value in its
   table. Bit 7 is the parity bit: the 23 standard codes have even parity;
   the 13 MR/MP/ML mode bytes have odd parity (SSTV Handbook §4.4.3), and
   MMSSTV's B/W 12 code 0x86 is odd as well.
4. Prints the sequence length: 910 ms.

The six narrow modes (MP73-N … MC180-N) have no VIS; the test skips them.
MMSSTV identifies them with an FSK N-VIS header instead (see
[the SSTV signal format](../standards/sstv-signal-format.md#n-vis-the-narrow-modes)).

The MR/MP/ML entries hold only the mode byte of MMSSTV's 16-bit VIS; the
0x23 prefix byte (odd parity) is not part of the table.

The VIS framing itself (leader, break, start bit, 8 or 16 data bits, stop bit)
is specified in
[the SSTV signal format](../standards/sstv-signal-format.md#vis-the-mode-identifier).

## Result

2026-09-18: 43/43 pass (37 codes checked, 6 narrow modes skipped).

## Reference data

`tests/vis_codes.json` lists, for every mode, the 8-bit code or 16-bit word,
its bits in transmission order, the tone of each bit and the N-VIS code of
the narrow modes. It is generated from `src/modes.cpp` and its JSON syntax is
checked at build time; no test reads it.
