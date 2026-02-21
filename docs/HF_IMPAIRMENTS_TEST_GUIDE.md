# HF Impairments DSP Pipeline Test - Usage Guide

## Overview

The HF impairments test validates DSP filter performance under realistic 20m HF propagation conditions. It simulates noise, fading, and interference typical of amateur radio SSTV transmission, then processes the signal through the complete decoder DSP pipeline.

## Quick Start

```bash
# Build the test
cd build
cmake .. -DBUILD_RX=ON -DBUILD_TESTS=ON
make test_hf_impairments

# Run with default S9 noise floor (RMS ~8000)
./bin/test_hf_impairments tests/audio/alt5_test_panel_scottie1.wav
```

## Command Line Usage

```bash
./bin/test_hf_impairments <input.wav> [<output_dir>] [<snr_db>]
```

**Arguments:**
- `input.wav` - Clean SSTV signal (16-bit mono PCM WAV required)
- `output_dir` - Output directory (default: `./hf_test_output`)
- `snr_db` - Signal dB above S9 noise floor (default: 12.0 for S9+10 signal)
- `signal_scale` - Input amplitude scale (default: 0.5, lower = fainter signal)

## SNR / S-Meter Conversion

**Important:** This test uses a **constant S9 noise floor** (RMS ~8000). The `snr_db` parameter specifies how many dB the signal is **above** this noise floor. The `signal_scale` parameter lets you make the input signal even fainter for realism.

| Signal dB | S-Meter (approx) | Condition | Decode Quality |
|-----------|------------------|-----------|----------------|
| +20 dB | S9+20 | Very strong | Perfect |
| +12 dB | S9+10 | Strong signal | Perfect, clean |
| **+6 dB** | **S9** | **Realistic HF** | **Marginal, some noise** |
| 0 dB | S7 | Weak signal | Visible noise, usable |
| -6 dB | S5 | Very weak | Heavy noise, difficult |

**Typical SSTV operation:** Signal 6-12 dB above S9 noise floor

## Example Scenarios

### 1. S9+10 Signal in S9 Noise Floor (Strong)
```bash
./bin/test_hf_impairments clean.wav ./s9_test 12.0 0.5
```
**Conditions:**
- S9 constant noise floor (RMS ~8000, atmospheric + receiver thermal)
- Signal +12 dB above noise (S9+10 strength)
- Input signal scaled to 0.5 (fainter)
- Slow Rayleigh fading (QSB ~0.2 Hz, 6 dB depth)

**Expected:** Perfect decode with minimal artifacts

### 2. S9 Signal in S9 Noise Floor (Marginal)
```bash
./bin/test_hf_impairments clean.wav ./s9real 6.0 0.5
```
**Conditions:**
- S9 constant noise floor
- Signal +6 dB above noise (S9 strength)
- Input signal scaled to 0.5
- Moderate fading

**Expected:** Usable decode with visible noise in image

### 3. S7 Signal in S9 Noise Floor (Weak)
```bash
./bin/test_hf_impairments clean.wav ./s7weak 0.0 0.5
```
**Conditions:**
- S9 constant noise floor
- Signal at noise level (S7 strength)
- Input signal scaled to 0.5
- Significant fading

**Expected:** Difficult decode, heavy noise

### 4. S5 Signal in S9 Noise Floor (Very Weak)
```bash
./bin/test_hf_impairments clean.wav ./s5veryweak -6.0 0.5
```
**Conditions:**
- S9 constant noise floor
- Signal -6 dB below noise (S5 strength)
- Input signal scaled to 0.5
- Heavy fading

**Expected:** Very difficult decode, barely usable

## Output Files

All outputs are saved to the specified directory (default: `./hf_test_output/`):

| File | Description | Use Case |
|------|-------------|----------|
| `00_clean_input.wav` | Original signal | Baseline reference |
| `01_with_noise.wav` | After HF impairments | What you'd hear on HF |
| `02_after_lpf.wav` | After 2-tap LPF | Anti-aliasing stage |
| `03_after_bpf.wav` | After bandpass FIR | Noise rejection verification |
| `04_after_agc.wav` | After AGC | Fading compensation check |
| `05_final.wav` | Final output | Input to tone detectors |

## Analysis Workflow

### Step 1: Generate Test Signal
```bash
# Use existing test file
cp tests/audio/alt5_test_panel_scottie1.wav clean.wav

# Or encode your own
./bin/encode_wav --mode scottie1 --input test.jpg --output clean.wav
```

### Step 2: Apply HF Impairments
```bash
# S7 signal in S7 noise (+6 dB, typical marginal contact)
./bin/test_hf_impairments clean.wav ./hf_test 6.0
```

### Step 3: Listen to Stages
```bash
# Original (clean)
play hf_test/00_clean_input.wav

# After adding noise (what you'd hear on radio)
play hf_test/01_with_noise.wav

# After BPF (noise rejection)
play hf_test/03_after_bpf.wav

# After AGC (normalized)
play hf_test/04_after_agc.wav
```

### Step 4: Verify Decode
```bash
# Decode final output to verify filter effectiveness
./bin/decode_wav hf_test/05_final.wav
```

### Step 5: Compare Spectrograms
```bash
# Generate spectrograms for visual analysis
sox hf_test/01_with_noise.wav -n spectrogram -o noisy_spec.png
sox hf_test/03_after_bpf.wav -n spectrogram -o filtered_spec.png
sox hf_test/04_after_agc.wav -n spectrogram -o agc_spec.png
```

## HF Propagation Model

### S7 Constant Noise Floor
- **Model:** Always present Gaussian white noise at RMS ~2000 (16-bit PCM)
- **Represents:** Atmospheric noise + receiver thermal noise typical of 20m band
- **Characteristics:** Independent of signal level, always audible
- **Purpose:** Realistic HF environment baseline

### Variable Signal Strength
- **Controlled by:** `snr_db` parameter (dB above S7 noise floor)
- **Examples:** 
  - +12 dB → S9 signal (strong, clean decode)
  - +6 dB → S7 signal (marginal, noisy but readable)
  - 0 dB → S5 signal (weak, difficult)
  - -6 dB → S3 signal (very weak, barely usable)

### Rayleigh Fading (QSB)
- **Rate:** 0.2 Hz (slow ionospheric variations)
- **Depth:** 6 dB (moderate fading, not flutter)
- **Model:** Low-pass filtered complex Gaussian (I/Q)
- **Purpose:** Simulates ionospheric multipath on 20m skip

### Background Hum
- **Frequencies:** 50 Hz + 100 Hz + 150 Hz harmonics
- **Level:** -40 dB relative to signal
- **Purpose:** Simulates power supply ripple or mains pickup

## DSP Pipeline Stages

### Stage 1: 2-Tap Simple LPF
```
h[n] = (x[n] + x[n-1]) / 2
```
- **Purpose:** Anti-aliasing, reduce high-frequency noise
- **Response:** -3 dB @ fs/2

### Stage 2: Kaiser Bandpass FIR
- **Taps:** ~104 @ 48 kHz (scaled from 24 @ 11025 Hz)
- **HBPF:** 1080-2600 Hz (narrow, for VIS + data)
- **HBPFS:** 400-2500 Hz (wide, for initial sync)
- **Stopband:** 60 dB rejection
- **Purpose:** Suppress out-of-band noise (especially 50 Hz hum)

### Stage 3: Level AGC
- **Target:** 512× peak normalization
- **Tracking:** 100 ms time constant
- **Range:** 60 dB dynamic range
- **Purpose:** Compensate for fading, normalize amplitude

### Stage 4: Final Scaling
```
output = AGC_output × 2.0
clamp to ±16384
```
- **Purpose:** Scale for tone detector input range

## Performance Metrics

### Expected Filter Performance

**BPF Noise Rejection:**
- 50 Hz hum: ~60 dB suppression
- Out-of-band no: 0 dB (signal = noise, S5-S6)
- Good decode: +6 dB (signal 4× noise power, S7-S8)
- Perfect decode: +12 dB (signal 16× noise power, 
**AGC Fading Compensation:**
- 6 dB fade: Fully compensated
- 12 dB fade: Partially compensated
- Response time: ~100 ms

**Overall Sensitivity:**
- Minimum useful SNR: ~12 at +6 dB signal above S7 noise
4. ✅ Tone detector input range stays within ±16384

**Test FAILS if:**
1. ❌ BPF allows 50 Hz hum through (>10 dB suppression only)
2. ❌ AGC introduces distortion or clipping
3. ❌ Decoder cannot lock VIS at +6 dB above S7 noise
1. ✅ BPF suppresses 50 Hz hum by >40 dB
2. ✅ AGC normalizes fading to <3 dB variation
3. ✅ Final output decodes successfully at S7 (25 dB SNR)
4. ✅ Tone detector input range stays within ±16384

**Test FAILS if:**
1. ❌ BPF allows 50 Hz hum to pass through (>10 dB suppression)
2. ❌ AGC introduces distortion or clipping
3. ❌ Decoder cannot lock VIS at S7 conditions
4. ❌ Output amplitude unstable (>10 dB variation)

## Troubleshooting

### Problem: Output files are silent
**Cause:** Input file already has very low amplitude  
**Solution:** Check AGC statistics, may need to adjust input gain

### Problem: Decode fails even at S9
**Cause:** BPF may be mistuned or AGC saturating  
**Solution:** Check `03_after_bpf.wav` spectrum, verify SSTV tones present

### Problem: Heavy distortion in output
**Cause:** AGC overdriving or clipping  
**Solution:** Reduce input amplitude or adjust AGC target (512 → 256)

### Problem: Fading not visible in output
**Cause:** AGC working correctly (compensating fading)  
**Solution:** This is normal! Compare `01_with_noise.wav` (fading visible) vs `04_after_agc.wav` (fading removed)

## References

- **Filter Specifications:** [docs/FILTER_SPECIFICATIONS.md](../docs/FILTER_SPECIFICATIONS.md)
- **Implementation Verification:** [docs/FILTER_IMPLEMENTATION_VERIFICATION.md](../docs/FILTER_IMPLEMENTATION_VERIFICATION.md)
- **DSP Pipeline Source:** [src/decoder.cpp](../src/decoder.cpp) lines 752-850
- **Test Source:** [tests/test_hf_impairments.cpp](test_hf_impairments.cpp)

## Example Output

```
=== HF Impairments DSP Pipeline Test ===
Input: tests/audio/alt5_test_panel_scottie1.wav
Output directory: ./hf_s7_test
Signal above S7 noise floor: 6.0 dB (S7-S8 signal)
Noise floor: S7 (constant atmospheric + receiver noise)

Sample rate: 22050 Hz
Duration: 111.3 seconds

BPF taps: 48
HBPF: 1080-2600 Hz
HBPFS: 400-2500 Hz

Processing 2455104 samples...
Progress: 2455104/2455104 samples (100.0%)    

=== Test Complete ===
Output files written to: ./hf_s7_test/

AGC Statistics:
  Final gain: 0.03
  Peak level: 17142.44
  S/N ratio: 0.846

To decode final output:
  ./bin/decode_wav ./hf_s7_test/05_final.wav
```

---

**Created:** February 20, 2026  
**Version:** 1.0  
**Part of:** mmsstv-portable DSP validation suite
