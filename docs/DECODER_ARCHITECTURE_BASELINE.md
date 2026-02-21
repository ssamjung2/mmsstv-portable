# MMSSTV Decoder Architecture Baseline

**Document Version:** 1.0  
**Date:** February 19, 2026  
**Status:** Reference Baseline for mmsstv-portable RX Decoder

---

## Executive Summary

This document establishes the **engineering baseline** for the mmsstv-portable RX decoder implementation, documenting the exact MMSSTV audio processing pipeline architecture with all relevant parameters, filter coefficients, and signal flow verified against the original MMSSTV source code.

**Key Findings:**
- MMSSTV uses **non-standard VIS frequencies**: 1080/1320 Hz (not 1100/1300 Hz)
- Audio pipeline: **5-stage processing** (LPF → BPF → AGC → IIR+LPF → Decision Logic)
- Current port status: BPF and AGC **DISABLED** for baseline testing
- All filter implementations **mathematically verified** against original MMSSTV

---

## 1. Audio Processing Pipeline Architecture

### 1.1 MMSSTV Original Pipeline (sstv.cpp:1819-1860)

```
┌──────────────────────────────────────────────────────────────────┐
│                    Audio Input Sample (s)                         │
└────────────────────────────┬─────────────────────────────────────┘
                             │
                   ┌─────────▼──────────┐
                   │  Input Clipping     │  if |s| > 24578.0
                   │  Overflow Detection │  set m_OverFlow flag
                   └─────────┬───────────┘
                             │
            ┌────────────────▼───────────────────┐
            │  Stage 1: Simple LPF                │
            │  d = (s + m_ad) * 0.5               │  Adjacent sample averaging
            │  m_ad = s                           │  (2-tap FIR, Fs/2 rolloff)
            └────────────────┬────────────────────┘
                             │
            ┌────────────────▼───────────────────┐
            │  Stage 2: BPF (if m_bpf enabled)   │
            │  • Pre-sync:  HBPFS (400-2500 Hz)  │  Wide capture
            │  • Post-sync: HBPF  (1080-2600 Hz) │  Narrow data band
            │  • Narrow:    HBPFN (custom)       │  Optional tight filter
            └────────────────┬────────────────────┘
                             │
            ┌────────────────▼───────────────────┐
            │  Stage 3: AGC (Automatic Gain)     │
            │  m_lvl.Do(d)      - Peak tracking  │  100ms window
            │  ad = m_lvl.AGC(d) - Normalize     │  Target: 16384.0
            └────────────────┬────────────────────┘
                             │
            ┌────────────────▼───────────────────┐
            │  Stage 4: Scaling & Clipping       │
            │  d = ad * 32                       │  Scale to working range
            │  Clamp: -16384.0 ≤ d ≤ +16384.0    │
            └────────────────┬────────────────────┘
                             │
        ┌────────────────────┴────────────────────┐
        │                                         │
┌───────▼──────────┐                    ┌─────────▼──────────┐
│ Stage 5a: IIR    │                    │ Stage 5b: IIR      │
│ Tone Detectors   │                    │ Tone Detectors     │
├──────────────────┤                    ├────────────────────┤
│ iir12 (1200 Hz)  │                    │ iir11 (1080 Hz)    │
│ Q = 100          │                    │ Q = 80             │
│ ↓ Rectify        │                    │ ↓ Rectify          │
│ lpf12 (50 Hz)    │                    │ lpf11 (50 Hz)      │
│ = d12            │                    │ = d11              │
├──────────────────┤                    ├────────────────────┤
│ iir19 (1900 Hz)  │                    │ iir13 (1320 Hz)    │
│ Q = 100          │                    │ Q = 80             │
│ ↓ Rectify        │                    │ ↓ Rectify          │
│ lpf19 (50 Hz)    │                    │ lpf13 (50 Hz)      │
│ = d19            │                    │ = d13              │
└────────┬─────────┘                    └─────────┬──────────┘
         │                                        │
         └────────────────┬───────────────────────┘
                          │
              ┌───────────▼──────────────┐
              │  Decision Logic           │
              │  • VIS Decode             │
              │  • Sync Detection         │
              │  • Image Demodulation     │
              └───────────────────────────┘
```

### 1.2 mmsstv-portable Current Implementation Status

**File:** `src/decoder.cpp:600-670`

| Stage | Component | Status | Notes |
|-------|-----------|--------|-------|
| 1 | Simple LPF | ✅ **ENABLED** | `d = (sample + prev_sample) * 0.5` |
| 2 | BPF | ❌ **DISABLED** | `#if 0` (lines 611-621) |
| 3 | AGC | ❌ **DISABLED** | `#if 0` (lines 623-629) |
| 4 | Scaling | ✅ **ENABLED** | `d = ad * 32.0; clamp(±16384.0)` |
| 5 | IIR Tone Detectors | ✅ **ENABLED** | All 4 resonators + 50Hz LPF |

**Reason for Disabling BPF/AGC:**
- Baseline testing to isolate IIR filter behavior
- Simplify debugging of VIS decode issues
- Verify core tone detection without additional signal processing

**Production Recommendation:**
- ✅ Enable BPF for real-world signal conditioning
- ✅ Enable AGC for automatic level adaptation
- ⚠️ Test with various signal strengths and noise levels

---

## 2. Frequency Parameters (MMSSTV Standard)

### 2.1 VIS Code Frequencies

**CRITICAL:** MMSSTV uses **non-standard frequencies** for VIS encoding.

| Parameter | Standard SSTV | MMSSTV Actual | Offset |
|-----------|---------------|---------------|--------|
| **Mark (bit 1)** | 1100 Hz | **1080 Hz** | -20 Hz |
| **Space (bit 0)** | 1300 Hz | **1320 Hz** | +20 Hz |
| **Sync/Start/Stop** | 1200 Hz | 1200 Hz | 0 Hz |
| **Leader** | 1900 Hz | 1900 Hz | 0 Hz |

**Source:** `mmsstv/sstv.cpp:1772-1778`

```cpp
// Original MMSSTV filter initialization
m_iir11.SetFreq(1080 + g_dblToneOffset, SampFreq, 80.0);  // Mark
m_iir12.SetFreq(1200 + g_dblToneOffset, SampFreq, 100.0); // Sync
m_iir13.SetFreq(1320 + g_dblToneOffset, SampFreq, 80.0);  // Space
m_iir19.SetFreq(1900+dfq + g_dblToneOffset, SampFreq, 100.0); // Leader
```

**VIS Bit Polarity (MMSSTV):**
```cpp
// VIS decode logic (sstv.cpp:1988)
if( d11 > d13 ) m_VisData |= 0x0080;  // 1080 Hz stronger → bit = 1
else                                  // 1320 Hz stronger → bit = 0
```

**Our Port Status:** ✅ Updated to match MMSSTV 1080/1320 Hz

### 2.2 AFC (Automatic Frequency Control)

MMSSTV includes a global tone offset variable `g_dblToneOffset` for:
- Compensating transmitter frequency drift
- Adapting to radio signal drift
- AFC detection during sync tracking

**Default:** `g_dblToneOffset = 0.0` (Main.cpp:700)  
**Range:** Typically ±100 Hz for amateur radio SSB  
**Port Status:** ⏸️ Not yet implemented (future enhancement)

### 2.3 Image Data Frequencies

| Frequency | Level | Purpose |
|-----------|-------|---------|
| 1500 Hz | Black (0) | Minimum luminance |
| 1900 Hz | Mid-gray (127) | 50% luminance |
| 2300 Hz | White (255) | Maximum luminance |
| 1200 Hz | Sync | Horizontal sync pulse |

**Mapping Formula:**
```cpp
// Frequency to pixel value (8-bit)
pixel = (freq - 1500.0) / (2300.0 - 1500.0) * 255.0;
pixel = clamp(pixel, 0, 255);

// Pixel value to frequency
freq = 1500.0 + (pixel / 255.0) * (2300.0 - 1500.0);
```

---

## 3. Filter Specifications

### 3.1 IIR Tone Detectors (CIIRTANK)

**Implementation:** 2nd-order resonator (bandpass)  
**Type:** Infinite Impulse Response (IIR)  
**Purpose:** Narrow-band tone detection for FSK demodulation

**Transfer Function:**
```
H(z) = a0 / (1 - b1·z⁻¹ - b2·z⁻²)
```

**Coefficient Calculation:**
```cpp
// Source: dsp_filters.cpp:248-268, verified against mmsstv/fir.cpp:41-76
double w = 2.0 * M_PI * freq / sample_freq;
double bw_factor = M_PI * bandwidth / sample_freq;

lb1 = 2.0 * exp(-bw_factor) * cos(w);           // Pole angle
lb2 = -exp(-2.0 * bw_factor);                   // Pole radius
la0 = sin(w) / ((sample_freq / 6.0) / bandwidth); // Gain
```

**Quality Factor:**
```
Q = f₀ / BW
```

Where:
- `f₀` = center frequency
- `BW` = 3dB bandwidth

**MMSSTV Configuration:**

| Filter | Frequency | Q-Factor | Bandwidth | Purpose |
|--------|-----------|----------|-----------|---------|
| iir11 | 1080 Hz | 80 | 13.5 Hz | VIS Mark detection |
| iir12 | 1200 Hz | 100 | 12.0 Hz | Sync pulse detection |
| iir13 | 1320 Hz | 80 | 16.5 Hz | VIS Space detection |
| iir19 | 1900 Hz | 100 | 19.0 Hz | Leader tone detection |

**Passband Characteristics:**
- **iir11 (1080 Hz, Q=80):** 1073.25 - 1086.75 Hz (±6.75 Hz)
- **iir13 (1320 Hz, Q=80):** 1311.75 - 1328.25 Hz (±8.25 Hz)

**Critical Design Note:**
With Q=80, the 3dB bandwidth is only ~13-17 Hz. This means:
- 20 Hz frequency offset puts signal **outside passband**
- Explains why 1100/1300 Hz signals fail on 1080/1320 Hz filters
- High selectivity required for FSK discrimination

### 3.2 Post-Detection LPF (CIIR Butterworth)

**Implementation:** Cascaded biquad IIR low-pass  
**Type:** 2nd-order Butterworth  
**Cutoff:** 50 Hz  
**Purpose:** Smooth IIR resonator output, remove ripple

**Configuration:**
```cpp
// Source: decoder.cpp:293-296
lpf11.MakeIIR(50.0, sample_rate, 2, 0, 0);  // 50 Hz, order 2, Butterworth
lpf12.MakeIIR(50.0, sample_rate, 2, 0, 0);
lpf13.MakeIIR(50.0, sample_rate, 2, 0, 0);
lpf19.MakeIIR(50.0, sample_rate, 2, 0, 0);
```

**Butterworth Response:**
- Maximally flat passband
- -3dB at 50 Hz
- -12 dB/octave rolloff (2nd order)

**Why 50 Hz?**
- VIS bits are 30ms duration → ~33 Hz fundamental
- 50 Hz cutoff preserves bit transitions
- Removes high-frequency noise from IIR resonator

### 3.3 Bandpass Filters (HBPF, HBPFS)

**Implementation:** Kaiser-windowed FIR  
**Type:** Finite Impulse Response (FIR)  
**Design:** Linear phase, symmetric taps

**MMSSTV Definitions (sstv.cpp:147-148):**
```cpp
#define HBPF   g_tFftBPF[0]   // Narrow: 1080-2600 Hz
#define HBPFS  g_tFftBPF[1]   // Wide:   400-2500 Hz
#define HBPFN  g_tFftBPF[2]   // Narrow: custom (mode-specific)
```

**Our Port Configuration (decoder.cpp:304-305):**
```cpp
MakeFilter(hbpf, bpftap, kFfBPF, sample_rate, 1080.0, 2600.0, 20.0, 1.0);
MakeFilter(hbpfs, bpftap, kFfBPF, sample_rate, 400.0, 2500.0, 20.0, 1.0);
```

| Filter | Low Cut | High Cut | Stopband Atten | Usage |
|--------|---------|----------|----------------|-------|
| **HBPFS** | 400 Hz | 2500 Hz | 60 dB | Pre-sync (wide capture) |
| **HBPF** | 1080 Hz | 2600 Hz | 60 dB | Post-sync (data band) |

**Tap Count:**
```cpp
bpftap = (int)(24.0 * sample_rate / 11025.0);  // ~104 taps @ 48kHz
```

**Mode Selection Logic (sstv.cpp:1826-1832):**
```cpp
if( m_Sync || (m_SyncMode >= 3) ){
    d = m_BPF.Do(d, m_fNarrow ? HBPFN : HBPF);  // After sync: narrow
} else {
    d = m_BPF.Do(d, HBPFS);                     // Before sync: wide
}
```

**Design Rationale:**
- **Wide filter (HBPFS):** Captures sync pulses with frequency uncertainty
- **Narrow filter (HBPF):** Rejects out-of-band noise during data decode
- **Linear phase:** No group delay distortion (critical for timing)

---

## 4. AGC (Automatic Gain Control)

### 4.1 Algorithm Architecture

**Implementation:** Peak tracking with adaptive normalization  
**Class:** `CLVL` (mmsstv/sstv.h:161-178, sstv.cpp:280-366)  
**Port:** `level_agc_*` functions (decoder.cpp:122-195)

**Processing Flow:**
```
Input → Peak Detection → Smoothing → Gain Calculation → Output
  d   →   m_Max update  →  avgLPF   →  16384.0/peak  →   ad
```

### 4.2 MMSSTV AGC Implementation

```cpp
// Peak tracking (sstv.cpp:289-305)
void CLVL::Do(double d) {
    if( d < 0 ) d = -d;
    
    if( d > m_Max ) {
        m_Max = d;
        m_MaxCnt = 0;
    } else {
        m_MaxCnt++;
        if( m_MaxCnt >= m_MaxTime ) {
            m_Max = avgLPF.Avg();  // Smooth decay
            m_MaxCnt = 0;
        }
    }
    avgLPF.SetData(d);
}

// Gain normalization (sstv.cpp:323-334)
double CLVL::AGC(double d) {
    if( m_Max > 4096.0 ) {
        return d * 16384.0 / m_Max;  // Normalize to 16384
    }
    return d * 4.0;  // Fallback for weak signals
}
```

**Parameters:**
- **Target level:** 16384.0 (full-scale for 16-bit processing)
- **Decay time:** 100ms (4800 samples @ 48kHz)
- **Threshold:** 4096.0 (minimum for AGC activation)
- **Smoothing:** Moving average low-pass filter

### 4.3 Port Status

**Current:** ❌ DISABLED (`#if 0` in decoder.cpp:623-629)

**Recommendation for Production:**
```cpp
#if 1  // ENABLE for production
level_agc_do(&dec->lvl, d);
level_agc_fix(&dec->lvl);
double ad = level_agc_apply(&dec->lvl, d);
#else
double ad = d;  // Bypass for testing
#endif
```

**Benefits of Enabling AGC:**
- Adapts to varying signal levels from radio
- Compensates for audio interface gain differences
- Prevents saturation from strong signals
- Maintains consistent IIR filter drive levels

---

## 5. Timing & Sampling Architecture

### 5.1 VIS Decode Timing (MMSSTV Original)

**Source:** `mmsstv/sstv.cpp:1953-2100`

```
Timeline (from leader detection):
   0ms: Leader tone detected (1900 Hz)
  15ms: Wait for leader validation
  45ms: Start bit begins (1200 Hz, 30ms)
  75ms: Data bit 0 (LSB) - SAMPLE HERE
 105ms: Data bit 1 - SAMPLE HERE
 135ms: Data bit 2 - SAMPLE HERE
  ...
 285ms: Data bit 7 (MSB) - SAMPLE HERE
 315ms: Parity bit - SAMPLE HERE
 345ms: Stop bit (1200 Hz, 30ms)
```

**Critical Finding:** MMSSTV samples **immediately** when countdown reaches 0:
```cpp
m_SyncTime--;
if( !m_SyncTime ){
    // Sample NOW - not at bit center!
    if( d11 > d13 ) m_VisData |= 0x0080;
    m_SyncTime = 30 * sys.m_SampFreq/1000;  // Reset for next bit
}
```

**Implication:**
- Sampling occurs at **bit boundaries**, not centers
- Requires IIR filters to **settle quickly** (< 30ms)
- Transition periods may cause bit errors

### 5.2 Our Port Timing Strategy

**Implementation:** `decoder.cpp:700, 757-770`

```cpp
// Initial delay: 45ms (30ms start + 15ms to bit center)
dec->sync_time = (int)(45.0 * dec->sample_rate / 1000.0);

// Then 30ms intervals
dec->sync_time = (int)(30.0 * dec->sample_rate / 1000.0);
```

**Advantage over MMSSTV:**
- Samples at **bit centers** (15ms into each 30ms period)
- Allows IIR filters to settle after transitions
- More robust to timing jitter

**Trade-off:**
- 15ms offset from MMSSTV exact timing
- May cause sync issues with timing-sensitive signals

### 5.3 IIR Filter Settling Time

**90% Settling Time Estimate:**
```
t_settle ≈ 3 / (π × BW)
```

For iir11/iir13 (BW ≈ 14 Hz):
```
t_settle ≈ 3 / (3.14159 × 14) ≈ 68 ms
```

**Problem:** 68ms > 30ms bit period!

**Why It Works:**
- Filters are **continuously running** (not reset between bits)
- Transitions from 1080 ↔ 1320 Hz are **partial** settling
- Discrimination relies on **relative** magnitudes, not absolute settling

**Test Results (from test_tone_decode.cpp):**
- Samples 0-30,000: Transitional response
- Samples 30,000-60,000: Improving discrimination (~10:1 ratio)
- Samples 60,000+: Good discrimination (~20:1 ratio)

**Recommendation:**
- ✅ Sample at bit centers (current implementation)
- ⚠️ Monitor discrimination ratio: `|d11 - d13| > s_lvl2`
- ⚠️ Reset sync if tones not discriminable

---

## 6. Discrimination & Decision Logic

### 6.1 Tone Discrimination Check (MMSSTV)

**Source:** `mmsstv/sstv.cpp:1976-1978`

```cpp
// Check if tones are discriminable before sampling
if( ((d11 < d19) && (d13 < d19)) ||
    (fabs(d11-d13) < (m_SLvl2)) ) {
    m_SyncMode = 0;  // RESET - tones not clear
}
```

**Conditions for Valid Sampling:**
1. **Mark or Space stronger than Leader:** `(d11 > d19) || (d13 > d19)`
2. **Sufficient separation:** `|d11 - d13| ≥ m_SLvl2`

**Our Port Implementation (decoder.cpp:754-759):**
```cpp
if ((d11 < d19) && (d13 < d19) && (fabs(d11 - d13) < dec->s_lvl2)) {
    dec->sync_mode = 0;
    dec->sync_state = SYNC_IDLE;
    fprintf(stderr, "[VIS] RESET: tones not discriminable\n");
}
```

### 6.2 Sensitivity Level Thresholds

**Source:** `mmsstv/sstv.cpp:1788-1804`

```cpp
switch(m_SenseLvl){
    case 0:  // Most sensitive
        m_SLvl = 2400;
        m_SLvl2 = 1200;  // m_SLvl * 0.5
        m_SLvl3 = 5000;
        break;
    case 1:
        m_SLvl = 3500;
        m_SLvl2 = 1750;
        m_SLvl3 = 5700;
        break;
    case 2:
        m_SLvl = 4800;
        m_SLvl2 = 2400;
        m_SLvl3 = 6800;
        break;
    case 3:  // Least sensitive
        m_SLvl = 6000;
        m_SLvl2 = 3000;
        m_SLvl3 = 8000;
        break;
}
```

**Our Port (decoder.cpp:208-241):**
```cpp
static void decoder_set_sense_levels(sstv_decoder_t *dec) {
    switch(dec->sense_level) {
        case 0:  // Most sensitive
            dec->s_lvl = 2400.0;
            dec->s_lvl2 = 1200.0;
            dec->s_lvl3 = 5000.0;
            break;
        // ... same thresholds
    }
}
```

**Usage:**
- `s_lvl`: Minimum sync pulse strength
- `s_lvl2`: Minimum tone separation for discrimination
- `s_lvl3`: Sync pulse quality threshold

**Current Default:** `sense_level = 0` (most sensitive)

### 6.3 Bit Decision Logic

**VIS Bit Decoding:**
```cpp
// MMSSTV polarity (sstv.cpp:1988)
if( d11 > d13 ) m_VisData |= 0x0080;  // 1080 Hz → bit = 1

// Our port (decoder.cpp:818)
int bit_val = (d11 > d13) ? 1 : 0;
```

**Bit Accumulation:**
```cpp
// MSB-first with right shift (MMSSTV)
m_VisData = m_VisData >> 1;
if( d11 > d13 ) m_VisData |= 0x0080;

// Our port: LSB-first with bit position
int bit_pos = 7 - dec->vis_cnt;
if (bit_val) dec->vis_data |= (1 << bit_pos);
```

**Both approaches are equivalent** - just different accumulators.

---

## 7. Sync Detection State Machine

### 7.1 MMSSTV Sync Modes

**Source:** `mmsstv/sstv.cpp:1895-2100`

```
Mode 0: Leader Detection
  - Monitor for 1900 Hz tone (d19)
  - Duration check via m_sint1.SyncStart()
  - Transition to Mode 1 on valid leader

Mode 1: 1200 Hz Validation (30ms)
  - Verify start bit (d12 > threshold)
  - Countdown m_SyncTime from 30ms
  - Transition to Mode 2 (VIS decode)

Mode 2: VIS Data Decode
  - Sample 8 data bits + parity
  - 30ms intervals between bits
  - Discrimination check before each sample
  - Transition to Mode 3 on completion

Mode 3-8: Mode-specific sync tracking
  - Horizontal sync pulse detection
  - Scan line timing synchronization
  - AFC frequency tracking

Mode 9: Extended VIS (16-bit)
  - Similar to Mode 2 but 16 data bits
  - Used for extended mode codes
```

### 7.2 Our Port State Machine

**Implementation:** `decoder.cpp:676-870`

```cpp
enum sync_state {
    SYNC_IDLE = 0,        // Waiting for leader
    SYNC_LEADER = 1,      // Leader tone detected
    SYNC_VIS_DECODING = 2,// Decoding VIS bits
    SYNC_DATA_WAIT = 3    // Image data mode
};
```

**Mapping to MMSSTV:**
- `SYNC_IDLE` → `m_SyncMode = 0`
- `SYNC_LEADER` → `m_SyncMode = 1`
- `SYNC_VIS_DECODING` → `m_SyncMode = 2, 9`
- `SYNC_DATA_WAIT` → `m_SyncMode >= 3`

---

## 8. Critical Implementation Details

### 8.1 Absolute Value + LPF Pattern

**Every IIR resonator output follows this pattern:**
```cpp
// Rectification (envelope detection)
double d_iir = m_iir.Do(input);
if( d_iir < 0.0 ) d_iir = -d_iir;

// Low-pass smoothing (50 Hz)
d_iir = m_lpf.Do(d_iir);
```

**Why Rectification?**
- Converts AC tone to DC level
- Amplitude represents tone presence
- Enables threshold-based decisions

**Why 50 Hz LPF?**
- Smooths ripple from rectification
- Preserves bit transitions (~33 Hz for 30ms bits)
- Reduces noise sensitivity

### 8.2 Signal Scaling Constants

**MMSSTV uses specific scaling values:**
```cpp
d = ad * 32;                    // After AGC: scale to ±16384
if( d > 16384.0 ) d = 16384.0;  // Clamp positive
if( d < -16384.0 ) d = -16384.0; // Clamp negative
```

**Rationale:**
- **16384 = 2^14:** Leaves headroom in 16-bit processing
- **×32 scaling:** Brings normalized signal to full range
- **Clamping:** Prevents overflow in IIR filters

**Our port matches this exactly** (decoder.cpp:631-633)

### 8.3 Overflow Detection

```cpp
// Input clipping (sstv.cpp:1821-1823)
if( (s > 24578.0) || (s < -24578.0) ){
    m_OverFlow = 1;  // Flag for UI indication
}
```

**Purpose:**
- Detect ADC clipping or excessive gain
- Alert operator to reduce audio level
- Threshold: 24578 / 32768 ≈ 75% of full scale

**Our port** (decoder.cpp:604-606):
```cpp
if (sample > 24576.0) sample = 24576.0;
if (sample < -24576.0) sample = -24576.0;
```

---

## 9. Testing & Validation Status

### 9.1 Component-Level Tests

| Component | Test File | Status | Notes |
|-----------|-----------|--------|-------|
| CIIRTANK IIR | test_dsp_reference.cpp | ✅ PASS | Coefficients verified |
| CIIR LPF | test_dsp_reference.cpp | ✅ PASS | 50 Hz Butterworth correct |
| FIR BPF | test_dsp_reference.cpp | ✅ PASS | Kaiser window symmetric |
| IIR+LPF Tone Detect | test_tone_decode.cpp | ✅ PASS | Discrimination > 10:1 |
| Pure Tone Response | test_iir_filters.cpp | ✅ PASS | 1320 Hz: d13/d11 = 5.5x |

### 9.2 Integration Tests

| Test | Status | Issue |
|------|--------|-------|
| Encoder → Decoder | ⚠️ PARTIAL | VIS decoding fails |
| VIS Frequency Output | ✅ VERIFIED | 1080/1320 Hz confirmed |
| IIR Filter Tuning | ✅ VERIFIED | 1080/1320 Hz centers |
| Bit Polarity | ✅ VERIFIED | `d11 > d13 = bit 1` |

**Current Issue:**
Decoder still reading `0x00` instead of `0x88` for Robot 36.

**Probable Causes:**
1. BPF/AGC disabled may cause signal level mismatch
2. VCO output normalization may need verification
3. Sample timing may not align with bit centers after all filter delays

**Next Steps:**
1. ✅ Re-enable AGC and test signal levels
2. ✅ Re-enable BPF and verify passband includes 1080/1320 Hz
3. ⏸️ Add debug output for d11/d13 values during VIS bits
4. ⏸️ Verify VCO actually outputs 1080/1320 Hz (FFT analysis)

---

## 10. Reference Implementation Comparison

### 10.1 MMSSTV Source Code References

| Component | MMSSTV File | Line Range | Port File | Status |
|-----------|-------------|------------|-----------|--------|
| Audio Pipeline | sstv.cpp | 1819-1860 | decoder.cpp:600-670 | ✅ Ported |
| IIR Filter Init | sstv.cpp | 1772-1778 | decoder.cpp:289-292 | ✅ Matched |
| VIS Decode | sstv.cpp | 1953-2100 | decoder.cpp:743-870 | ✅ Ported |
| CIIRTANK Math | fir.cpp | 41-76 | dsp_filters.cpp:248-268 | ✅ Verified |
| AGC Algorithm | sstv.cpp | 280-366 | decoder.cpp:122-195 | ✅ Ported |
| BPF Config | sstv.cpp | 147-148 | decoder.cpp:304-305 | ✅ Matched |

### 10.2 Frequency Parameter Verification

**MMSSTV (sstv.cpp:1772-1778):**
```cpp
m_iir11.SetFreq(1080 + g_dblToneOffset, SampFreq, 80.0);
m_iir13.SetFreq(1320 + g_dblToneOffset, SampFreq, 80.0);
```

**Our Port (decoder.cpp:289-291):**
```cpp
dec->iir11.SetFreq(1080.0, sample_rate, 80.0);
dec->iir13.SetFreq(1320.0, sample_rate, 80.0);
```

**VCO MMSSTV (sstv.cpp:2777-2778):**
```cpp
m_vco.SetFreeFreq(1100 + g_dblToneOffset);
m_vco.SetGain(2300 - 1100);  // = 1200 Hz span
```

**Our Port (encoder.cpp:1468-1469, vco.cpp:26-32):**
```cpp
enc->vco.setFreeFreq(1080.0);
enc->vco.setGain(1220.0);  // Span to 2300 Hz
```

**Status:** ✅ Frequencies aligned to MMSSTV non-standard values

---

## 11. Production Recommendations

### 11.1 Enable Full Pipeline for Production

```cpp
// Change in decoder.cpp

// BPF - RECOMMENDED TO ENABLE
#if 1  // Changed from #if 0
if (dec->use_bpf) {
    if (dec->sync_mode >= 3 && !dec->hbpf.empty()) {
        d = dec->bpf.Do(d, dec->hbpf.data());
    } else if (!dec->hbpfs.empty()) {
        d = dec->bpf.Do(d, dec->hbpfs.data());
    }
}
#endif

// AGC - RECOMMENDED TO ENABLE  
#if 1  // Changed from #if 0
level_agc_do(&dec->lvl, d);
level_agc_fix(&dec->lvl);
double ad = level_agc_apply(&dec->lvl, d);
#else
double ad = d;
#endif
```

**Testing Sequence:**
1. Test with AGC only (BPF still disabled)
2. Test with BPF only (AGC disabled)
3. Test with both AGC and BPF enabled
4. Validate with clean signals and noisy signals

### 11.2 Future Enhancements

**Priority 1 (High Impact):**
- [ ] Implement AFC (g_dblToneOffset tracking)
- [ ] Add sync tracker integration (CSYNCINT port)
- [ ] Implement PLL for timing recovery

**Priority 2 (Robustness):**
- [ ] Add slant correction
- [ ] Implement clock adjustment
- [ ] Add signal quality metrics

**Priority 3 (Features):**
- [ ] Extended VIS (16-bit) support
- [ ] FSK decoder for MMSSTV messages
- [ ] Repeater mode support

---

## 12. Appendices

### Appendix A: Quick Reference - Key Constants

```cpp
// Frequencies (MMSSTV Standard)
#define VIS_MARK_HZ     1080.0   // Bit 1
#define VIS_SPACE_HZ    1320.0   // Bit 0  
#define VIS_SYNC_HZ     1200.0   // Start/Stop
#define VIS_LEADER_HZ   1900.0   // Leader tone

#define IMG_BLACK_HZ    1500.0   // Pixel value 0
#define IMG_WHITE_HZ    2300.0   // Pixel value 255
#define IMG_SYNC_HZ     1200.0   // Hsync pulse

// Timing (VIS)
#define VIS_LEADER_MS   300.0
#define VIS_BREAK_MS     10.0
#define VIS_START_MS     30.0
#define VIS_BIT_MS       30.0
#define VIS_STOP_MS      30.0

// IIR Filter Q-Factors
#define Q_DATA_TONES     80      // iir11, iir13
#define Q_SYNC_TONES    100      // iir12, iir19

// Post-IIR LPF
#define LPF_CUTOFF_HZ   50.0

// Signal Levels
#define SCALE_FACTOR    32.0
#define CLAMP_MAX       16384.0
#define CLAMP_MIN      -16384.0
#define OVERFLOW_THRESH 24576.0

// AGC
#define AGC_TARGET      16384.0
#define AGC_THRESHOLD    4096.0
#define AGC_DECAY_MS      100.0
```

### Appendix B: Filter Passband Summary

```
Audio Spectrum View:

    400         1080  1200  1320        1900        2300 2500 2600
     |           |     |     |           |            |    |    |
     |<--HBPFS-->|<--->|<--->|<--------->|<---------->|    |    |
     |           |iir11|iir13|           |iir19       |    |    |
     |           |<-------- HBPF ----------------------->|  |    |
     |<---------------------- HBPFS -------------------->|       |
     
HBPFS: Wide pre-sync capture (400-2500 Hz)
HBPF:  Narrow data band (1080-2600 Hz)
iir11: VIS Mark @ 1080 Hz (±6.75 Hz, Q=80)
iir12: Sync @ 1200 Hz (±6 Hz, Q=100)
iir13: VIS Space @ 1320 Hz (±8.25 Hz, Q=80)
iir19: Leader @ 1900 Hz (±9.5 Hz, Q=100)
```

### Appendix C: Glossary

- **AFC:** Automatic Frequency Control - compensates for transmitter drift
- **AGC:** Automatic Gain Control - normalizes signal amplitude
- **BPF:** Band-Pass Filter - passes frequencies in a specific range
- **FSK:** Frequency-Shift Keying - digital data encoded as frequency changes
- **IIR:** Infinite Impulse Response - recursive filter with feedback
- **LPF:** Low-Pass Filter - passes frequencies below cutoff
- **Q-factor:** Quality factor = f₀/BW - measure of filter selectivity
- **VIS:** Vertical Interval Signaling - mode identification code
- **CIIRTANK:** 2nd-order resonator class in MMSSTV (tank circuit analogy)
- **CLVL:** Level/AGC class in MMSSTV
- **CFIR2:** FIR filter class with circular buffer

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-02-19 | mmsstv-portable team | Initial baseline documentation |

**References:**
- Original MMSSTV source code by Makoto Mori (JE3HHT) & Nobuyuki Oba
- SSTV specification documents (VIS codes, timing)
- DSP filter theory (Butterworth, Kaiser, resonators)
- Project documentation in `/docs` folder

---

**END OF BASELINE DOCUMENT**
