# Encoder implementation

How `libsstv_encoder` produces a transmission, and where each part lives in
the code: `src/encoder.cpp`, `src/vco.cpp`, `src/vis.cpp`, `src/modes.cpp`.

The signal it produces — tones, preamble, VIS and other headers, line
structure — is specified in
[the SSTV signal format](../standards/sstv-signal-format.md). This document
does not repeat it; it describes the implementation and the places where the
port makes a choice.

Everything here is a port of MMSSTV's transmitter: `TMmsstv::OutHEAD()` for
the preamble, the VIS and header code in `TMmsstv`'s TX start-up, the per-mode
`TMmsstv::Line*()` writers (`Main.cpp`), `CSSTVMOD` for the tone generator
(`sstv.cpp`) and `ColorToFreq()` / `GetRY()` (`ComLib.cpp`). Line references
are in [the MMSSTV source map](mmsstv-source-map.md).

## Pipeline

```text
sstv_encoder_generate()
  stage 0: header segments   = preamble (800/400 ms) + VIS header
  stage 2: line segments     = one scan line at a time (write_line_*)
           │
           ▼
  segment queue (frequency, sample count) ──► VCO ──► float samples in [-1, +1]
```

Every tone is a *segment*: a frequency and a duration. Segments are queued,
then converted to samples.

- `push_segment_ms()` converts a duration in milliseconds to a sample count
  and **carries the fractional remainder into the next segment**. That is what
  keeps timing exact over a whole transmission at any sample rate; without it
  the rounding error per line would accumulate into a visible slant.
- Frequency 0 means silence. It is used once, after the AVT header, matching
  `CSSTVMOD::Do()`.
- Lines are generated one at a time by `generate_next_line_segments()`, so
  memory use does not depend on the length of the transmission.

## The oscillator

`src/vco.cpp` is MMSSTV's `CVCO`: a sine table of `2 × sample_rate` entries
and a phase accumulator, with no interpolation. It is configured exactly as
MMSSTV configures it, base 1100 Hz and gain 1200 Hz, so a frequency `f` is fed
in as `(f − 1100) / 1200`.

The phase is continuous across every segment boundary, including between the
header and the image, which is what lets a receiver's sync detector work at
all.

This 1100 Hz base is not cosmetic: with a 1900 Hz base the generated tones
were measurably wrong (1181.7 Hz where 1200 Hz was intended). See the
changelog entry for 2026-01-31.

## Code map

| Part of the signal | Function | Notes |
| --- | --- | --- |
| Preamble | `write_preamble()` | Tone list chosen by `get_preamble_ms()` / narrow test |
| VIS tone sequence | `vis_build_tones()`, `VISEncoder` (`src/vis.cpp`) | Shared with the tests; builds 8-bit and 16-bit sequences |
| Header dispatch | `write_vis_header()` | Picks 8-bit VIS, 16-bit VIS, AVT triple VIS + countdown, or the narrow FSK header |
| FSK words | `write_fsk()` | Port of `CSSTVMOD::WriteFSK` |
| 16-bit VIS word per mode | `get_mmsstv_vis_word()` | `0x23` low byte plus the mode byte |
| N-VIS code per mode | `get_narrow_nvis()` | |
| Scan lines | `write_line_r24/r36/r72/avt/sct/mrt/sc2/pd/p/mp/mr/rm/mn/mc()` | One per mode family |
| Line dispatch and timing | `generate_next_line_segments()`, `get_line_ms()`, `compute_mode_timing()` | |
| Pixel to tone | `color_to_freq()`, `color_to_freq_narrow()` | Integer arithmetic, as in MMSSTV |
| Colour conversion | `get_ry()` | Y, R−Y, B−Y, clamped to 0…255 |
| Pixel fetch | `get_pixel_rgb()` | Honours `stride`; expands `SSTV_GRAY8` to R = G = B |
| Total length | `get_vis_header_ms()`, `recompute_total_samples()` | |

Scottie, Martin and SC2 always emit 320 pixels per channel and sample the
source image with nearest-neighbour indexing; those modes require a 320-wide
image anyway, which `sstv_encoder_set_image()` enforces.

## Length of a transmission

`sstv_encoder_get_total_samples()` returns
`(preamble + header + line_ms × lines) × sample_rate / 1000`.

The generated sample count matches that prediction to within ±1 sample for
every one of the 43 modes. `tests/test_roundtrip.cpp` checks the length and
decodes each mode back to an image at 11025 Hz, repeats the sweep with the
transmitter 500 ppm off frequency, and separately checks generated tone
frequencies at 48 kHz.

## Deliberate omissions

Present in MMSSTV, not implemented here:

- The transmit band-pass filter and per-colour output gain (`CSSTVMOD`
  `m_bpf`, `m_VariOut`).
- CW identifier, FSK callsign identifier, and appended MMV audio clips.
- A switch to suppress the preamble (VIS can be disabled, the preamble cannot).
