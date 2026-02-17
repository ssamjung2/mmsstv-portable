# RX Features Porting Plan (Decoder/Demod)

**Date:** Feb 5, 2026  
**Project:** mmsstv-portable (encoder-only → add RX)  
**Goal:** Add a minimal, portable SSTV receiver pipeline with optional DSP features.

---

## Scope (MVP)

1. **Audio Input Interface (user-provided)**
   - API accepts float samples (mono) at arbitrary sample rates.
   - No real-time audio I/O inside the library.

2. **Demod Core**
   - Tone tracking / FM demod into instantaneous frequency.
   - Sync detection and line timing.
   - VIS decoding (RX).
   - Image reconstruction (RGB24 output).

3. **Minimal DSP**
   - Bandpass/lowpass filters for SSTV band.
   - Optional notch/LMS disabled by default.

4. **Optional UX Features (non-core)**
   - FFT/spectrum/waterfall (diagnostic only).
   - Slant correction, AFC/PLL tuning.

---

## Source Mapping (original MMSSTV)

**Core decode / demod (primary extraction):**
- sstv.h / sstv.cpp: CSSTVDEM (demod, sync, VIS decode)
- Sound.cpp: audio capture path (used as reference only)

**DSP utilities:**
- fir.*: FIR filters
- LinearDs.*: linear downsampler
- Fft.*: FFT/spectrum analyzer (optional diagnostics)
- Scope.* / FreqDisp.*: visualization (optional)

**Timing & mode tables:**
- sstv.h / sstv.cpp: mode timing tables (already extracted in encoder)

---

## Proposed Public RX API (C)

```c
typedef struct sstv_decoder_s sstv_decoder_t;

typedef enum {
    SSTV_RX_OK = 0,
    SSTV_RX_NEED_MORE = 1,
    SSTV_RX_IMAGE_READY = 2,
    SSTV_RX_ERROR = -1
} sstv_rx_status_t;

sstv_decoder_t* sstv_decoder_create(double sample_rate);
void sstv_decoder_free(sstv_decoder_t* dec);

void sstv_decoder_reset(sstv_decoder_t* dec);
void sstv_decoder_set_mode_hint(sstv_decoder_t* dec, sstv_mode_t mode);
void sstv_decoder_set_vis_enabled(sstv_decoder_t* dec, int enable);

sstv_rx_status_t sstv_decoder_feed(
    sstv_decoder_t* dec,
    const float* samples,
    size_t sample_count
);

int sstv_decoder_get_image(
    sstv_decoder_t* dec,
    sstv_image_t* out_image
);
```

---

## Phased Implementation Plan

### Phase A — Minimal Demod Pipeline (MVP)
- Implement FM demod to instantaneous frequency.
- Implement VIS RX (standard + 16-bit modes).
- Implement sync detection (line start / porch / sync pulses).
- Reconstruct grayscale for a single stable mode (e.g., Robot 36).

### Phase B — Full Mode Coverage
- Add all 43 modes using existing timing tables.
- Add color reconstruction for RGB and Y/C modes.
- Add line/field buffering and image assembly.

### Phase C — Robustness
- AFC / slant correction.
- Auto mode detection.
- Error resilience (line drop, noise).

### Phase D — Diagnostics (Optional)
- FFT / spectrum / waterfall (ported from Fft.*)
- Debug overlays (tone markers, sync events).

---

## Immediate Next Actions

1. **Define decoder struct and stubs** (new include + src files).
2. **Port minimal FM demod** (from CSSTVDEM).
3. **Implement VIS RX decode** and unit tests.
4. **Create a small WAV-based test harness** for offline decoding.

---

## Notes

- Keep RX code separate from encoder to preserve library portability.
- Avoid VCL/Windows UI dependencies from MMSSTV.
- Prioritize deterministic, testable decode steps over GUI features.
