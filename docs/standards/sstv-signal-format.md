# SSTV signal format

What an SSTV transmission consists of: tones, preamble, mode identification
headers and scan-line structure. This is the protocol reference. How this
library generates it is in [the encoder specification](../specs/encoder.md);
how it reads it back is in [the decoder specification](../specs/decoder.md).

Two authorities: the SSTV Handbook ([sstv-handbook.com](https://www.sstv-handbook.com))
and the MMSSTV source, which is what the amateur SSTV world actually
interoperates with. **Where they differ, this document follows MMSSTV** and
says so, because MMSSTV's behaviour is what this library implements and what
other stations expect. The known differences are noted in
[avt.md](avt.md) and [wraase-sc2.md](wraase-sc2.md).

## Tones

An SSTV signal is a single audio tone whose frequency carries everything:
sync, timing and pixel brightness.

| Purpose | Frequency |
| --- | --- |
| Sync pulse | 1200 Hz |
| Black | 1500 Hz |
| White | 2300 Hz |
| Video, in general | `1500 + v × 800 / 256` Hz for a value `v` of 0…255 |
| Narrow-mode video | `2044 + v × 256 / 256` Hz (2044–2300 Hz) |
| VIS leader | 1900 Hz |
| VIS bits | 1100 Hz = 1, 1300 Hz = 0 |

Colour modes send either red, green and blue in sequence, or luma and two
colour-difference channels:

| Quantity | Formula (MMSSTV `GetRY`, BT.601 studio range) |
| --- | --- |
| Y | `16 + 0.256773 R + 0.504097 G + 0.097900 B` |
| R−Y | `128 + 0.439187 R − 0.367766 G − 0.071421 B` |
| B−Y | `128 − 0.148213 R − 0.290974 G + 0.439187 B` |

All three are clamped to 0…255 before being turned into a tone.

## Preamble

| Modes | Tones, 100 ms each | Length |
| --- | --- | --- |
| All except narrow | 1900, 1500, 1900, 1500, 2300, 1500, 2300, 1500 Hz | 800 ms |
| Narrow (MP73-N … MC180-N) | 1900, 2300, 1900, 2300 Hz | 400 ms |

The preamble carries no information. It lets receivers and operators notice
that a transmission has started.

## VIS: the mode identifier

VIS (Vertical Interval Signalling) is the 8-bit code that tells the receiver
which mode follows.

| Part | Tone | Length |
| --- | --- | --- |
| Leader | 1900 Hz | 300 ms |
| Break | 1200 Hz | 10 ms |
| Leader | 1900 Hz | 300 ms |
| Start bit | 1200 Hz | 30 ms |
| 8 data bits, LSB first | 1100 Hz = 1, 1300 Hz = 0 | 8 × 30 ms |
| Stop bit | 1200 Hz | 30 ms |
| **Total** | | **910 ms** |

Bit 7 of the byte is the parity bit, so there is **no separate parity tone**
and the code is 7 bits of mode plus parity. The 23 standard codes use even
parity: the whole byte has an even number of ones. MMSSTV sends B/W 12 as
`0x86`, which is odd, and the MR/MP/ML codes below are odd by design.

Example: Robot 36 is `0x88`, that is mode `0x08` with parity bit 1, sent LSB
first as `0 0 0 1 0 0 0 1` = 1300, 1300, 1300, 1100, 1300, 1300, 1300,
1100 Hz.

A receiver that samples the bit tones at 1080 and 1320 Hz rather than 1100 and
1300 Hz is not misreading the standard: see
[why the detectors sit there](../specs/vis-tone-frequencies.md).

### 16-bit VIS (MMSSTV's MR, MP and ML modes)

MMSSTV's own modes need more codes than 7 bits allow, so they use the same
framing with 16 data bits: the extended marker `0x23` as the low byte first,
then the mode byte, each with **odd** parity in bit 7 (SSTV Handbook §4.4.3).
MR73, for example, sends `0x4523`. Total **1150 ms**.

### N-VIS: the narrow modes

The narrow modes carry no VIS. MMSSTV identifies them with an FSK header
instead: 6 bits per word, LSB first, 22 ms per bit, 1900 Hz = 1, 2100 Hz = 0.

| Part | Tone | Length |
| --- | --- | --- |
| Tone | 1900 Hz | 300 ms |
| Guard | 2100 Hz | 100 ms |
| Start bit | 1900 Hz | 22 ms |
| Words | `0x2D`, `0x15`, N-VIS code, code ⊕ `0x15` | 4 × 6 × 22 ms |
| **Total** | | **950 ms** |

N-VIS codes (SSTV Handbook table 4.10): MP73-N `0x02`, MP110-N `0x04`,
MP140-N `0x05`, MC110-N `0x14`, MC140-N `0x15`, MC180-N `0x16`.

### AVT 90

AVT sends its 8-bit VIS (`0x44`) **three times**, then a digital header, then
0.30514375 ms of silence:

- 32 frames, each a 1900 Hz pulse followed by 16 bits, every element
  9.7646 ms long.
- Bits go MSB first: 1600 Hz = 1, 2200 Hz = 0.
- The word starts at `0x5FA0` (mode 010 plus a countdown of 11111, and an
  inverted copy in the low byte). After each frame the high byte is
  decremented and the low byte incremented, ending at `0x40BF`. MMSSTV's
  receiver checks that the high byte is in `0x40`–`0x5F` and the low byte is
  its complement.

Total header: 3 × 910 + 32 × 17 × 9.7646 + 0.305 ≈ **8042.2 ms**. AVT lines
carry no sync pulses at all, which is why this countdown exists.

### Scottie

Scottie 1, 2 and DX send one extra 1200 Hz sync pulse of 9 ms after the VIS,
before the first line, because a Scottie line ends rather than begins with its
sync.

## Scan lines

Times in milliseconds. `tw` is the time for one colour channel, so a pixel
lasts `tw / width`. "Line" is the total scan-line time and "Lines" is how many
are sent. Bold names are pixel data; everything else is a fixed tone.

| Family | Line structure | tw | Line | Lines |
| --- | --- | --- | --- | --- |
| Robot 36 | 1200 9, 1500 3, **Y** 88, separator 4.5 (1500 on even lines = R−Y follows, 2300 on odd lines = B−Y follows), 1900 1.5, **R−Y or B−Y** 44 | – | 150 | 240 |
| Robot 72 | 1200 9, 1500 3, **Y** 138, 1500 4.5, 1900 1.5, **R−Y** 69, 2300 4.5, 1900 1.5, **B−Y** 69 | – | 300 | 240 |
| Robot 24 | 1200 6, 1500 2, **Y** 92, 1500 3, 1900 1, **R−Y** 46, 2300 3, 1900 1, **B−Y** 46; every second image row | – | 200 | 120 |
| AVT 90 | **R**, **G**, **B** (no sync, no gaps) | 125 | 375 | 240 |
| Scottie 1 / 2 / DX | 1500 1.5, **G**, 1500 1.5, **B**, 1200 9, 1500 1.5, **R** | 138.24 / 88.064 / 345.6 | 428.22 / 277.692 / 1050.3 | 256 |
| Martin 1 / 2 | 1200 4.862, 1500 0.572, **G**, 1500 0.572, **B**, 1500 0.572, **R**, 1500 0.572 | 146.432 / 73.216 | 446.446 / 226.798 | 256 |
| SC2 180 / 120 / 60 | 1200 S, 1500 0.5, **R**, **G**, **B** (S = 5.5437 / 5.52248 / 5.5006) | 235 / 156.5 / 78.128 | 711.0437 / 475.52248 / 240.3846 | 256 |
| PD50 … PD290 | 1200 20, 1500 2.08, **Y(row n)**, **R−Y**, **B−Y**, **Y(row n+1)**; chroma from row n | 91.52 / 170.24 / 121.6 / 195.584 / 183.04 / 244.48 / 228.8 | 388.16 / 703.04 / 508.48 / 804.416 / 754.24 / 1000 / 937.28 | height / 2 |
| P3 / P5 / P7 | 1200 S, 1500 P, **R**, 1500 P, **G**, 1500 P, **B**, 1500 P (S, P = 5.208, 1.042 / 7.813, 1.562375 / 10.417, 2.083) | 133.333 / 200 / 266.667 | 409.375 / 614.0625 / 818.75 | 496 |
| MP73 … MP175 | 1200 9, 1500 1, **Y(n)**, **R−Y**, **B−Y**, **Y(n+1)** | 140 / 223 / 270 / 340 | 570 / 902 / 1090 / 1370 | 128 |
| MR73 … MR175, ML180 … ML320 | 1200 9, 1500 1, **Y** tw, hold 0.1, **R−Y** tw/2, hold 0.1, **B−Y** tw/2, hold 0.1 (holds repeat the last pixel's tone) | MR 138 / 171 / 220 / 269 / 337; ML 176.5 / 236.5 / 277.5 / 317.5 | MR 286.3 … 684.3; ML 363.3 … 645.3 | MR 256, ML 496 |
| B/W 8 / 12 | 1200 6, 1500 2, **Y** = average of rows n and n+1 | 58.89709 / 92 | 66.89709 / 100 | 120 |
| MP73-N … MP140-N | 1900 9, 2044 1, **Y(n)**, **R−Y**, **B−Y**, **Y(n+1)** on narrow tones | 140 / 212 / 270 | 570 / 858 / 1090 | 128 |
| MC110-N … MC180-N | 1900 8, 2044 0.5, **R**, **G**, **B** on narrow tones | 140 / 180 / 232 | 428.5 / 548.5 / 704.5 | 256 |

Notes:

- A Scottie line as transmitted carries the green and blue of one row, then
  the sync, then that row's red. Receivers frame the line from the sync pulse.
- Scottie, Martin and SC2 always send 320 pixels per channel.
- Per-mode image sizes, VIS codes and durations are tabulated in the
  [project README](../../README.md).

## Transmission length

`preamble + header + line time × lines`, where the header is 910 ms for a
standard VIS, 1150 ms for the 16-bit VIS, 950 ms for the narrow FSK header, or
about 8042.2 ms for AVT 90, plus Scottie's extra 9 ms.
