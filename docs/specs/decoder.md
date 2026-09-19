# Decoder architecture

Reference for `libsstv_decoder` (`src/decoder.cpp`, `src/dsp_filters.cpp`,
`include/sstv_decoder.h`). It is a port of MMSSTV's receiver `CSSTVDEM`
(`sstv.cpp`) and its picture drawing (`TMmsstv::DrawSSTV*`, `Main.cpp`);
differences from MMSSTV are called out. Current behaviour and known
limitations are summarised in [decoder status](decoder-status.md).

## 1. Per-sample pipeline

Every sample passed to `sstv_decoder_feed()` goes through
`decoder_process_sample()`:

```text
input (16-bit PCM scale)
  │ clip to ±24576                         (MMSSTV only flags overflow here)
  │ 2-tap average  d = (x[n] + x[n-1]) / 2
  │ band-pass FIR  HBPFS 400–2500 Hz until an image starts,
  │                HBPF 1100–2600 Hz while an image is decoded
  │ level AGC      gain = 16384 / 100-ms peak  (skipped if SSTV_AGC_OFF)   ──► ad
  │
  ├─► Hilbert FM demodulator (on ad) ─────────────────────► pixel signal
  │
  │ limiter        d = clamp(ad × 32, ±16384)
  ▼
  tone detectors (resonator → |x| → 50 Hz Butterworth LPF):
     d11 1080 Hz   d12 1200 Hz   d13 1320 Hz   d19 1900 Hz   dsp 2100 Hz
  │
  ├─► image line decoder   (while an image is being decoded)
  ├─► N-VIS FSK decoder    (narrow-mode header, d19 vs dsp)
  └─► VIS state machine    (d12, d19, d11 vs d13)
```

### Front-end parameters

| Stage | Parameters | MMSSTV source |
| --- | --- | --- |
| Band-pass FIR | `MakeFilter` BPF, `24 × fs / 11025` taps (104 at 48 kHz), att = 20 dB. att < 21 selects no Kaiser window (α = 0, rectangular), so the worst stop-band rejection is only about 25–28 dB (measured; see [the filter specifications](filters.md)), not 60 dB. HBPF low edge 1100 Hz because MMSSTV's `m_SyncRestart` defaults to 1. Group delay = taps / 2 (1.08 ms). | `CSSTVDEM::CalcBPF` ("wide" setting) |
| Level AGC | `CLVL`, fast mode: every 100 ms, gain = 16384 / peak of the last 100 ms; if the peak ≤ 32 the gain is 512. | `CLVL::Do/Fix/AGC` |
| Limiter | `×32`, clamp ±16384. After the AGC this is a hard limiter: the tone detectors always see a constant-amplitude signal. | `CSSTVDEM::Do` |
| Resonators | `CIIRTANK::SetFreq(f, fs, bw)`; the third argument is the **bandwidth in Hz** (pole radius `exp(−π·bw/fs)`), not a Q factor. 1080 Hz/80 Hz, 1200 Hz/100 Hz, 1320 Hz/80 Hz, 1900 Hz/100 Hz, 2100 Hz/100 Hz. | `CSSTVDEM` constructor / `SetFreq` |
| Envelope LPF | `CIIR` 2nd-order Butterworth, 50 Hz, one per detector. | `m_lpf11…19`, `m_lpffsk` |

Why 1080/1320 Hz: MMSSTV *transmits* VIS bits at 1100/1300 Hz but centres its
receive detectors 20 Hz outside them. With 80 Hz bandwidth each tone still
falls well inside its detector, and the decision only compares `d11` with
`d13`. See [the VIS tone frequencies](vis-tone-frequencies.md).

### Hilbert FM demodulator

Port of MMSSTV `CHILL` (the default demodulator, `m_Type = 2`):

- Hilbert FIR of `round(12 × fs / 11025)` taps (even, minimum 6), designed by
  `MakeHilbert(100 Hz, fs/2 − 100 Hz)`; the in-phase signal is the delay-line
  centre tap.
- Phase difference over 1, 2 or 4 samples (`fs` < 16 kHz, < 40 kHz, ≥ 40 kHz).
- Re-centred and scaled by `hill_set_width()` (MMSSTV `CHILL::SetWidth`):
  centre 1900 Hz / span 800 Hz, or for the narrow modes centre 2172 Hz /
  span 256 Hz (2044–2300 Hz video).
- 1800 Hz 3rd-order Butterworth LPF.
- Output: +16384 at black (1500 Hz, or 2044 Hz narrow), −16384 at white (2300 Hz).

The demodulator runs on the AGC output (before the limiter).

## 2. Header detection

VIS and N-VIS detection run only while no image is being decoded and while
VIS is enabled (`sstv_decoder_set_vis_enabled`, default on).

### VIS state machine (MMSSTV `CSSTVDEM::Do` sync modes 0–2, 9)

| State | Action |
| --- | --- |
| 0 | Wait for 1200 Hz: `d12 > d19`, `d12 > s_lvl`, `d12 − d19 ≥ s_lvl`. |
| 1 | The same condition must hold for 15 ms continuously. This rejects the 10 ms break between the leaders. Then sample every 30 ms. |
| 2 | At each 30 ms tick: if `d11 < d19` **and** `d13 < d19` **and** `\|d11 − d13\| < s_lvl2`, reset; otherwise bit = `d11 > d13` (1100 Hz tone = 1), accumulated LSB first. After 8 bits: 0x23 (or its inverse 0x5C) → state 9 for the second byte; otherwise look the byte up. |
| 9 | Second byte of a 16-bit VIS, looked up among the MR/MP/ML codes. |

- Because of the detector latency (~9.5 ms), each bit is sampled 24.5 ms after
  it starts; the envelope then reflects the middle of the bit.
- `s_lvl` = 2400 and `s_lvl2` = 80 (sense level 0; there is no API to change
  it). MMSSTV uses `s_lvl2` = 1200 and rejects a bit if *either* condition
  holds (`||`). The port is deliberately more permissive: it uses `&&` and a
  lower `s_lvl2`.
- Even parity is checked, but a failure does not reject the code. The odd-
  parity codes (B/W 12 = 0x86, and the MR/MP/ML bytes when sent as plain
  8-bit codes) rely on this. If the byte is unknown, its bitwise inverse is
  tried as well.
- The lookup table (`VIS_CODE_MAP`) holds the 24 standard codes and the 13
  MR/MP/ML second bytes. MR/MP/ML second bytes are also accepted as plain
  8-bit codes (some recordings send them that way); 0x86 resolves to B/W 12
  as a plain code and to ML240 after 0x23.

### Narrow-mode N-VIS (MMSSTV `CSSTVDEM::DecodeFSK`, N-VIS path)

Mark `d19` (1900 Hz) against space `dsp` (2100 Hz), decision threshold
`|d19 − dsp| ≥ 2048`:

1. 2100 Hz guard present for 50 ms; then wait up to 100 ms for the 1900 Hz start bit.
2. Confirm the start bit at its midpoint (11 ms), then sample every 22 ms.
3. 6-bit words, LSB first: 0x2D, 0x15, code, check = 0x15 ⊕ code.
4. Code 0x02/0x04/0x05/0x14/0x15/0x16 → MP73-N/MP110-N/MP140-N/MC110-N/MC140-N/MC180-N.

### Image start

When a header is decoded, the image buffer is allocated and the line position
`line_pos` is set negative by the time remaining until line 0 starts:

| Start | Lead to line 0 | Reason |
| --- | --- | --- |
| VIS | 35.5 ms | 5.5 ms of the last bit (sampled 24.5 ms in) + 30 ms stop bit |
| N-VIS | 1.5 ms | last bit sampled 11 ms + ~9.5 ms latency into its 22 ms |
| Mode hint (no header) | 30 ms | decoding starts at the first sample fed |
| AVT 90, after VIS | + 910 + 910 + 32 × 17 × 9.7646 + 0.305 ms, + (BPF taps + Hilbert taps) / 2 samples | two more VIS, the digital header, and the demodulator delay (AVT has no line sync to absorb it) |
| Scottie | + (9 + tw − OF − KS) ms | first line starts after the extra 9 ms sync and the G/B part of the line |

The AVT lead assumes the first of the three VIS headers was decoded; the AVT
digital header itself is not decoded.

## 3. Image line decoder

`decoder_process_image_sample()` advances `line_pos` by one sample per input
sample and maps it onto the scan line using the per-mode table
`SCAN_TIMING` (ms, from MMSSTV `CSSTVSET::SetSampFreq` / `GetTiming`):

| Field | Meaning |
| --- | --- |
| `tw` | scan-line length (`GetTiming`) |
| `of` | sync + porch at the line start (`m_OF`); positions below are relative to its end |
| `ofp` | expected position of the sync-detector peak from the line start (`m_OFP`) |
| `ks`, `sg`/`cg`, `sb`/`cb` | channel 0 width; channel 1 and 2 start/end (`m_KS`, `m_SG`, `m_CG`, `m_SB`, `m_CB`) |
| `ks2` | chroma channel width for Y/C modes (`m_KS2`) |
| `kss_div` | pixel-mapping width `KSS = KS × (1 − 1/kss_div)` (`m_KSS`, `m_KS2S`; MR73 uses 1024 for chroma) |
| `row_double` | one scan line fills two image rows (Robot 24, B/W 8/12) |
| `color_type` | channel layout (below) |

Pixel column `x = position_in_channel × width / KSS` (or `/KS2S` for chroma),
as in MMSSTV (`x = ps * Width / m_KSS`). The last sample that lands on a
column wins.

| `color_type` | Modes | Channels |
| --- | --- | --- |
| `SCAN_RGB` | AVT, Scottie, SC2, P3/5/7, MC-N | R, G, B |
| `SCAN_MRT` | Martin 1/2 | G, B, R |
| `SCAN_YC` | Robot 72, Robot 24, MR, ML | Y, R−Y, B−Y |
| `SCAN_YC_PD` | PD, MP, MP-N | Y(row n), R−Y, B−Y, Y(row n+1) → 2 rows |
| `SCAN_R36` | Robot 36 | Y, separator tone, R−Y *or* B−Y (below) |
| `SCAN_BW` | B/W 8/12 | Y only |

Levels (MMSSTV `GetPictureLevel` / `GetPixelLevel`), with `sig` the
demodulator output: luma/RGB `v = (16384 − sig) / 128` (0…256); chroma
`c = −sig / 128` (−128…+128). Y/C → RGB (MMSSTV `YCtoRGB`):
`R = 1.164457 (Y−16) + 1.596128 c_R`,
`G = 1.164457 (Y−16) − 0.813022 c_R − 0.391786 c_B`,
`B = 1.164457 (Y−16) + 2.017364 c_B`, clamped to 0…255.

**Robot 36** (4:2:0): the separator tone before the chroma is averaged; a
level ≥ 64 (2300 Hz) means B−Y follows, ≤ −64 (1500 Hz) means R−Y, anything in
between alternates from the previous line (MMSSTV `m_DSEL`). The newest R−Y
and B−Y rows are kept, and every output row combines its own Y with them.

At the end of each scan line the channel buffers are converted into one or
two image rows. When the last row is written the state becomes complete and
`sstv_decoder_feed()` returns `SSTV_RX_IMAGE_READY`. `sstv_decoder_finish()`
flushes a partial last line at end of input.

## 4. Sync tracking and timing correction

Every scan line the decoder re-locks to the line sync, like MMSSTV's
`m_SyncPos` tracking:

1. Over the whole line, record the position of the maximum sync-tone
   envelope: `d12` (1200 Hz), or `d19` (1900 Hz) for the narrow modes.
2. At the end of the line (skipped for the first line, and when the peak is
   below `s_lvl / 2`): `error = peak_pos − ofp`, wrapped into ±TW/2 so a sync
   that arrived before the decoder's line start counts as negative.
3. Proportional re-lock: `line_pos −= clamp(error, ±0.15 × OF) × 0.5`.
4. Timing correction (on by default, `sstv_decoder_enable_timing_correction`):
   the clamped errors are averaged over 8 lines, and
   `correction += gain × average` (gain 0.04 by default,
   `sstv_decoder_set_timing_correction_gain`). `line_pos −= correction` every
   line, capped at 1000 ppm of the line length. This integral term learns a
   steady TX/RX sample-clock mismatch, so the proportional term does not have
   to hold a phase offset.

AVT has `ofp = 0` (no line sync), which disables both steps. Scottie lines
are framed to start at their mid-line sync, so MMSSTV's `m_OFP` applies there
too.

`sstv_decoder_get_state()` reports `timing_error` (the last 8-line average,
in samples) and `timing_correction` (samples per line).

## 5. API behaviour summary

| Call | Behaviour |
| --- | --- |
| `sstv_decoder_create(fs)` | Any `fs` > 0. |
| `sstv_decoder_feed(dec, x, n)` | Samples on the 16-bit PCM scale. Returns `SSTV_RX_IMAGE_READY` once the last row is written, otherwise `SSTV_RX_NEED_MORE`. |
| `sstv_decoder_finish(dec)` | Call once at end of input; flushes a partial last line. |
| `sstv_decoder_get_image(dec, &img)` | RGB24, `stride = width × 3`; the buffer belongs to the decoder (valid until reset/free). |
| `sstv_decoder_set_mode_hint(dec, m)` | Takes effect at the next `feed`: the image starts immediately, no header detection. |
| `sstv_decoder_set_vis_enabled(dec, 0)` | Disables VIS and N-VIS detection (a hint is then required). |
| `sstv_decoder_set_agc_mode(dec, SSTV_AGC_OFF)` | Bypasses the level AGC; every other value enables it (default `SSTV_AGC_AUTO`). |
| `sstv_decoder_set_vis_tones(dec, mark, space)` | Moves the 1080/1320 Hz VIS detectors. |
| `sstv_decoder_enable_debug_wav(...)` | Writes the signal after the 2-tap average, after the BPF, after the AGC, and "final" (= AGC output × 2, not the limiter output). See [the debug WAV guide](../guide/debug-wav.md). |
| `sstv_decoder_reset(dec)` | Clears the detected mode, detection state, the image and the mode hint. Once an image has started, the decoder no longer looks for headers, so call this before decoding the next transmission. |
