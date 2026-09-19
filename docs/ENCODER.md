# Encoder: transmitted signal and implementation

This describes exactly what `libsstv_encoder` (`src/encoder.cpp`, `src/vco.cpp`,
`src/vis.cpp`, `src/modes.cpp`) generates. The signal is a port of MMSSTV's
transmitter: `TMmsstv::OutHEAD()` (preamble), the VIS/header code in
`TMmsstv` TX start-up, the per-mode `TMmsstv::Line*()` writers (MMSSTV
`Main.cpp`), `CSSTVMOD` (tone generator, `sstv.cpp`) and `ColorToFreq()` /
`GetRY()` (`ComLib.cpp`).

## Generation pipeline

```text
sstv_encoder_generate()
  stage 0: header segments   = preamble (800/400 ms) + VIS header
  stage 2: line segments     = one scan line at a time (write_line_*)
           │
           ▼
  segment queue (frequency, sample count) ──► VCO ──► float samples in [-1, +1]
```

- Every tone is a *segment* `(frequency, duration)`. `push_segment_ms()`
  converts the duration to samples and carries the fractional remainder into
  the next segment, so timing stays exact over the whole transmission at any
  sample rate. The generated length matches `sstv_encoder_get_total_samples()`
  to within ±1 sample.
- Frequency 0 means silence (used once, after the AVT header), as in
  MMSSTV `CSSTVMOD::Do()`.
- The VCO is MMSSTV's `CVCO`: a sine table of `2 × sample_rate` entries and a
  phase accumulator (no interpolation). It is configured as MMSSTV configures
  it, base 1100 Hz and gain 1200 Hz, and every frequency `f` is fed as
  `(f − 1100) / 1200`. The phase is continuous across all segments.

## Preamble

Always sent (there is no API to disable it), before the header.

| Modes | Tones, 100 ms each | Length |
| --- | --- | --- |
| All except narrow | 1900, 1500, 1900, 1500, 2300, 1500, 2300, 1500 Hz | 800 ms |
| Narrow (MP73-N … MC180-N) | 1900, 2300, 1900, 2300 Hz | 400 ms |

## Header ("VIS")

Sent when VIS is enabled (default; `sstv_encoder_set_vis_enabled(enc, 0)`
turns it off, including Scottie's extra sync pulse).

### Standard VIS (8-bit)

| Part | Tone | Length |
| --- | --- | --- |
| Leader | 1900 Hz | 300 ms |
| Break | 1200 Hz | 10 ms |
| Leader | 1900 Hz | 300 ms |
| Start bit | 1200 Hz | 30 ms |
| 8 data bits, LSB first | 1100 Hz = 1, 1300 Hz = 0 | 8 × 30 ms |
| Stop bit | 1200 Hz | 30 ms |
| **Total** | | **910 ms** |

Bit 7 of the VIS byte is the parity bit (even parity for the standard codes;
MMSSTV sends B/W 12 as 0x86, which has odd parity); there is no separate
parity tone. Example: Robot 36 = 0x88 = code 0x08 with parity 1, sent as
`0 0 0 1 0 0 0 1` = 1300, 1300, 1300, 1100, 1300, 1300, 1300, 1100 Hz.

### MR / MP / ML: 16-bit VIS

Same framing with 16 data bits: the extended marker 0x23 (low byte, first)
followed by the mode byte, each with odd parity in bit 7 (SSTV Handbook
§4.4.3), e.g. MR73 =
`0x4523`. Total **1150 ms**.

### AVT 90

MMSSTV sends the 8-bit VIS (0x44) **three times**, then a digital header,
then 0.30514375 ms of silence:

- 32 frames; each is a 1900 Hz pulse followed by 16 bits, all 9.7646 ms long.
- Bits are sent MSB first: 1600 Hz = 1, 2200 Hz = 0.
- The 16-bit word starts at 0x5FA0 (mode 010 + countdown 11111 / inverted
  copy) and after each frame the high byte is decremented and the low byte
  incremented, ending at 0x40BF. This matches the SSTV Handbook (§4.2.5) and
  MMSSTV's receiver check (`h` in 0x40–0x5F, `l = 0xFF − h`).

Total header: 3 × 910 + 32 × 17 × 9.7646 + 0.305 ≈ **8042.2 ms**.

### Narrow modes: FSK N-VIS

The narrow modes have no VIS. MMSSTV sends an FSK header instead
(`CSSTVMOD::WriteFSK`: 6 bits, LSB first, 22 ms per bit, 1900 Hz = 1,
2100 Hz = 0):

| Part | Tone | Length |
| --- | --- | --- |
| Tone | 1900 Hz | 300 ms |
| Guard | 2100 Hz | 100 ms |
| Start bit | 1900 Hz | 22 ms |
| Words | 0x2D, 0x15, N-VIS code, code ⊕ 0x15 | 4 × 6 × 22 ms |
| **Total** | | **950 ms** |

N-VIS codes: MP73-N 0x02, MP110-N 0x04, MP140-N 0x05, MC110-N 0x14,
MC140-N 0x15, MC180-N 0x16 (SSTV Handbook table 4.10).

### Scottie

After the VIS, Scottie 1/2/DX send one extra 1200 Hz sync of 9 ms before the
first line (MMSSTV; the Scottie line starts mid-line with its colour data, see
below).

## Pixel values to tones

| Quantity | Formula (MMSSTV) |
| --- | --- |
| Video tone | `f = 1500 + v × 800 / 256` (integer arithmetic; v = 0…255) |
| Narrow video tone | `f = 2044 + v × 256 / 256` (2044–2300 Hz) |
| Luma | `Y = 16 + 0.256773 R + 0.504097 G + 0.097900 B` |
| R−Y | `128 + 0.439187 R − 0.367766 G − 0.071421 B` |
| B−Y | `128 − 0.148213 R − 0.290974 G + 0.439187 B` |

Y, R−Y and B−Y are clamped to 0…255 (BT.601 studio range, MMSSTV `GetRY`).
GRAY8 images are expanded to R = G = B.

## Scan lines

Times in ms. `tw` is the channel (colour component) time; one pixel lasts
`tw / width`. "Line" is the total scan-line time (MMSSTV `GetTiming`), and
"Lines" is the number of scan lines sent.

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

- The Scottie line as generated starts with the G and B of one row and puts
  the sync before that row's R. The decoder frames Scottie lines from the sync
  pulse (see the decoder document).
- Scottie, Martin and SC2 always generate 320 pixels per channel; the image
  is sampled with nearest-neighbour indexing (it must be 320 wide anyway).
- Mode dimensions are enforced by `sstv_encoder_set_image()`.

## Length of a transmission

`sstv_encoder_get_total_samples()` =
`(preamble + header + line_ms × lines) × sample_rate / 1000`, where the header
is 910 ms (8-bit VIS), 1150 ms (16-bit VIS), ≈ 8042.2 ms (AVT), 950 ms
(narrow), plus 9 ms for Scottie, or 0 when VIS is disabled. The generated
count equals this to within ±1 sample (verified for all 43 modes by
`tests/test_roundtrip.cpp`).

## Not implemented (present in MMSSTV)

- TX band-pass filter and per-colour output gain (`CSSTVMOD` `m_bpf`,
  `m_VariOut`).
- CW ID, FSK callsign ID and MMV audio clips appended to a transmission.
- A public switch to disable the preamble.
