# Changelog

Notable changes to mmsstv-portable, newest first. There are no released
versions yet, so entries are dated rather than numbered.

Sources: entries from 2026-02-16 onward follow the git history; earlier ones
come from the development notes now in [docs/archive/](docs/archive/), which
are the only record of that period. Commits bundled several days of work, so
an entry is dated by when the work was done, not by when it was committed.

## Unreleased

### Changed

- Documentation reorganised by kind: [standards](docs/standards/) (what SSTV
  is), [specifications](docs/specs/) (how this library works),
  [guide](docs/guide/) (how to use it), [plans](docs/plans/) (what was
  intended) and [archive](docs/archive/) (what happened). Every document now
  sits in exactly one of those, and each directory has an index.
- The project README moved from `docs/README.md` to the repository root.
- Protocol details (tones, preamble, VIS, N-VIS, AVT header, line structure)
  moved out of the encoder specification into
  [docs/standards/sstv-signal-format.md](docs/standards/sstv-signal-format.md),
  so the standard and the implementation are no longer described in the same
  place.

### Added

- This changelog.
- Consumer documentation: [getting started](docs/guide/getting-started.md),
  [building and installing](docs/guide/building.md),
  [encoder API](docs/guide/encoder-api.md),
  [decoder API](docs/guide/decoder-api.md) and
  [command-line tools](docs/guide/cli-tools.md).

### Removed

- `docs/DOCUMENTATION_INDEX.md` and `docs/START_HERE.md`, replaced by
  [the documentation index](docs/README.md) and
  [getting started](docs/guide/getting-started.md).

## 2026-09-19

### Fixed

- `sstv_decoder_reset()` did not clear the detected mode, so a decoder reused
  for a second transmission produced a garbage image.

### Changed

- All reference documentation rewritten against the code, replacing claims
  that were never true or had gone stale: VIS is 910 ms rather than 640 or
  940 ms, VIS bits are 1100 Hz for 1 and 1300 Hz for 0, the band-pass filters
  reach about 25–28 dB of stop-band rejection rather than 60 dB, and
  `CIIRTANK`'s third argument is a bandwidth in Hz, not a Q.
- Parity documented correctly in `src/vis.h`, `src/vis.cpp`, the VIS tests and
  `list_modes`: 23 standard codes use even parity; the MR/MP/ML bytes, the
  `0x23` prefix and MMSSTV's B/W 12 code (`0x86`) are odd.
- `tests/vis_codes.json` regenerated from the mode table, with per-mode bit
  patterns, tone frequencies and sequence durations.
- 21 superseded documents marked as historical records, each listing its own
  errors.

### Added

- Decoder-reuse case in `test_roundtrip` (decode, reset, decode a different
  mode).

## 2026-09-18

A correctness pass over both libraries. Round-trip decoding went from roughly
half the modes producing garbage to all 43 passing, and the recorded-audio
suite from 7 of 12 to 12 of 12.

### Added

- `sstv_decoder_finish()`, to flush the last scan line when the input ends.
- Timing-correction controls: `sstv_decoder_enable_timing_correction()` and
  `sstv_decoder_set_timing_correction_gain()`, plus `timing_error` and
  `timing_correction` in `sstv_decoder_state_t`.
- The headers MMSSTV actually sends for AVT (VIS three times plus the 32-frame
  countdown) and for the narrow modes (the FSK N-VIS header).
- `tests/test_roundtrip.cpp` (all modes, clock mismatch),
  `tests/test_timing_correction.cpp` (slant regression on recordings) and the
  `evaluate_decoded_images` tool.

### Fixed

Encoder:

- The oscillator used a 1900 Hz base where MMSSTV uses 1100 Hz with a 1200 Hz
  span, so every tone was wrong: sync measured 1181.7 Hz instead of 1200.0,
  black 1486.7 instead of 1500.0.
- VIS now uses 1100 Hz for 1 and 1300 Hz for 0, and `vis.cpp` builds both the
  910 ms and the 1150 ms sequences; the previous 16-bit code contradicted
  itself.
- Predicted sample counts are within ±1 sample for every mode; they had been
  off by +12, by +444 for Scottie and by −2859 for MR/MP/ML.
- A memory leak, and a stray call left in the Robot 72 line writer.

Decoder:

- Per-line sync re-lock only reset its search when a line started exactly at
  sample 0, which stopped happening after the first correction; the decoder
  then chased a stale value and slanted the image. This alone accounted for 13
  modes decoding as garbage.
- Timing correction pushed the wrong way: its sign was reversed. With the fix,
  Martin 1 at +1000 ppm improved from a mean pixel error of 64.7 to 6.9.
- Scottie modes had sync tracking switched off, so any clock mismatch wrecked
  the image (error 95–131, now 3–9).
- VIS sampling and image start follow MMSSTV: the extra 12 ms wait before
  sampling is gone and the image starts at the measured time rather than
  6.5 ms late.
- Robot 36 decodes in colour (error 91 to 8.8).
- Narrow modes are detected on their own through the N-VIS header, with
  MMSSTV's narrow demodulator width (error 88–131 to 4–6, no mode hint
  needed).
- Filter parameters match MMSSTV: 1100 Hz band-pass low edge and the correct
  pixel-width table.
- `sstv_decoder_set_agc_mode()` and `sstv_decoder_set_vis_enabled()` had no
  effect; invented narrow-mode VIS codes were removed.

Tools:

- `decode_wav` called `sstv_decoder_finish()` after every chunk, which stopped
  decoding after 10–25% of the audio. It now calls it once, at the end.

## 2026-03-07

### Added

- The decoder rebuilt along MMSSTV's design: a scan-timing table covering all
  43 modes, the `CHILL` Hilbert-transform FM demodulator, per-line sync
  re-lock, and a line decoder with the colour layouts each family uses (RGB,
  YC, PD, Robot 36 and black-and-white).
- `tests/test_decode_images` and `tests/test_decode_modes`, which decode
  recorded audio and generated audio for every mode.
- A pkg-config file for the encoder.

## 2026-02-21

### Added

- The HF impairment test harness (noise, fading and interference sweeps).
- A standalone spectral-subtraction DNR module. The decoder does not use it.
- Debug WAV output from the decoder front end, and the `decode_wav_debug`
  tool.

### Changed

- Repository reorganised: documentation into `docs/`, development scripts and
  diagnostic programs into `utils/`.

### Removed

- Build outputs that had been committed under `bin/`, and a stale
  `decoder.cpp.bak`.

## 2026-02-19

### Changed

- The band-pass filter and the CLVL level AGC, which had been disabled for
  baseline testing, were re-enabled in the decoder front end.
- VIS test audio regenerated.

## 2026-02-16

### Added

- Repository created. Initial import of the encoder for all 43 modes, the
  first decoder, the DSP primitives, the test suite, the development
  documentation and the SSTV Handbook.

## 2026-02-05

### Added

- First receiver pipeline: band-pass filter, tone detectors, gain tracking,
  sync detection and VIS bit accumulation. Its demodulator was replaced in
  March.
- The 17-test DSP reference harness comparing the ported filters against
  MMSSTV's coefficients.

## 2026-01-31

### Fixed

- Oscillator phase increments computed in floating point instead of truncated
  integers, with MMSSTV's 1100 Hz base and 1200 Hz span.
- Martin modes send the trailing 1500 Hz porch after the red channel, which
  receivers need to frame the next line.

## 2026-01-30

### Added

- All 43 modes generate audio; batch generator for the whole mode set.
- VIS test suite with JSON fixtures covering every mode's code and bit
  pattern.

## 2026-01-28

### Added

- Project started: analysis of the MMSSTV source, the encoder-only plan, the
  project skeleton, the mode table, the oscillator and the VIS encoder.
