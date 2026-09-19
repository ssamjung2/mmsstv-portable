# Decoder status

As of 2026-09-18. How the decoder works is described in
[DECODER_ARCHITECTURE_BASELINE.md](DECODER_ARCHITECTURE_BASELINE.md).

## What works

| Area | Status |
| --- | --- |
| Front end | 2-tap average, MMSSTV band-pass (HBPFS/HBPF), level AGC and limiter, five tone detectors; all enabled. |
| Mode detection | Standard VIS, MMSSTV 16-bit VIS (MR/MP/ML) and the narrow-mode FSK N-VIS; or a caller-supplied mode hint. |
| Image decoding | All 43 modes, RGB24 output. Robot 36 is decoded in colour (4:2:0 with the separator-tone chroma selector). |
| Line sync | Re-locks to the line sync every line (1200 Hz; 1900 Hz for the narrow modes), whole-line search. |
| Clock mismatch | Timing-correction integral term; images stay straight at ±300 ppm and at +1000 ppm (tested on Martin, Scottie, PD, MR, P modes). |
| End of input | `sstv_decoder_finish()` flushes a partial last line. |
| Reuse | `sstv_decoder_reset()` returns the decoder to header detection for the next transmission. |

## Verification

From the test suite ([tests/README.md](../tests/README.md)), 2026-09-18:

- `roundtrip`: every mode encoded by this library, decoded from the header
  alone. MAE against the source image is 1.5–16 at 48 kHz and 2–15 at
  11025 Hz. Also covers +500 ppm clock mismatch and decoder reuse after reset.
- `decode_images`: 12/12 recorded WAVs within MAE 60 of their reference
  images (actual MAE 11–29).
- `timing_correction`: the three cases with recordings present pass.
- `decode_modes`: 43/43 (mode, size and image produced; quality not checked).

## Known limitations

1. **VIS acceptance is looser than MMSSTV.** A bit is rejected only when both
   tones are below the 1900 Hz level *and* `|d11 − d13| < 80` (MMSSTV: either
   condition, threshold 1200). Parity failures are accepted, and a code's
   bitwise inverse is tried if the code is unknown. This favours weak signals
   but allows more false detections in noise. The sense level is fixed at 0
   (no API).
2. **AVT 90**: the digital header is not decoded; the image start is computed
   from the first VIS. If that VIS is missed and a later one is decoded, the
   image is misplaced. AVT has no line sync, so there is no sync tracking or
   clock-mismatch correction for AVT.
3. **Horizontal offset**: a steady offset of a few pixels (about ±5 px on a
   320-pixel line) remains in some modes. It comes from MMSSTV's per-mode
   sync-peak positions (`m_OFP`), which were calibrated for MMSSTV's own
   receiver. When the initial alignment is off, the first ~10–15 lines can be
   skewed while the re-lock converges (limited to 7.5 % of the sync length per line).
4. **No AFC**: a mistuned signal (tone offset) is not tracked.
   `sstv_decoder_set_vis_tones()` only moves the two VIS detectors.
5. **Mode hint**: decoding starts at the first sample fed; the decoder does
   not search for the image start. Start the audio at the image, or let the
   header be detected instead.
6. **One image per run**: once an image has started, headers are ignored
   until `sstv_decoder_reset()`.
7. **Only MMSSTV's "wide" band-pass and the Hilbert demodulator** are
   implemented (no narrow BPF option, no PLL or zero-crossing demodulator).
8. **Callsign FSK ID** (MMSSTV 0x2A header) is not decoded.
9. **Input** is clipped at ±24576 (MMSSTV only flags overflow).
10. **AGC modes**: `SSTV_AGC_LOW/MED/HIGH/SEMI` behave like `SSTV_AGC_AUTO`;
    only `SSTV_AGC_OFF` differs.
11. **Code hygiene**: the sync-tracker helpers `sync_tracker_trig/max/start`
    and `decoder_try_vis_from_buffer()` are unused, and some debug-logging
    counters are `static` (shared by all decoder instances; affects log
    output only).
