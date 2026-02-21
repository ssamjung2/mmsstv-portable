# VIS Test Suite - Quick Reference

## 📊 Test Summary

```
Location:    /Users/ssamjung/Desktop/WIP/mmsstv-portable/tests/
Build:       cmake -DBUILD_TESTS=ON && cmake --build .
Run:         ../bin/test_vis_codes

Status:      ✅ 43/43 tests PASSING (100%)
Coverage:    All SSTV modes + even/odd parity verification
```

## 📁 Test Files

| File | Size | Purpose |
|------|------|---------|
| `vis_codes.json` | 16 KB | Test fixture (43 modes with VIS data) |
| `test_vis_codes.c` | 7.7 KB | C test program (267 lines) |
| `CMakeLists.txt` | 554 B | Build configuration |

## 🧪 What Gets Tested

### 1. **Bit Pattern Validation**
   - VIS code → 8-bit binary (LSB-first)
   - Verified for all 43 modes
   - Example: 0x88 (Robot 36) → `00010001`

### 2. **Frequency Mapping**
   - Bit 0 → 1100 Hz
   - Bit 1 → 1300 Hz
   - Validated per data bit
   - 8 bits per mode tested

### 3. **Parity Calculation**
   - Even parity: count bits, 0=even, 1=odd
   - Even modes (24): Robot36, Scottie1, Martin1, etc.
   - Odd modes (13): MR73, MP73, ML180, etc.
   - No-VIS modes (6): MP73-N through MC180-N

### 4. **Sequence Timing**
   - Leader(1900Hz/300ms)
   - Break(1200Hz/10ms)  
   - Leader(1900Hz/300ms)
   - Start(1200Hz/30ms)
   - Data(8×30ms)
   - Parity(30ms)
   - Stop(1200Hz/30ms)
   - **Total: 640ms (VIS standard)**

## 🏃 Quick Build & Test

```bash
cd /Users/ssamjung/Desktop/WIP/mmsstv-portable
rm -rf build && mkdir build && cd build
cmake -DBUILD_TESTS=ON ..
cmake --build .

# Run tests
../bin/test_vis_codes

# Expected output:
# [TEST 1] Robot 36 (VIS 0x88 = 136 decimal)
#   Binary conversion: 00010001
#   Bit frequencies: 11 11 11 13 11 11 11 13
#   Parity: 0 (even=1) → frequency 1100 Hz
#   ✓ PASS
# ...
# TEST RESULTS:
#   Total modes tested: 43
#   Modes passed:       43 (100.0%)
#   Total errors:       0
# ✓ ALL TESTS PASSED
```

## 🎯 Test Coverage

### By Mode Type
- **Color modes**: 37 (86%)
- **B/W modes**: 2 (5%)
- **No-VIS modes**: 6 (14%)

### By Parity
- **Even parity**: 24 modes
- **Odd parity**: 13 modes
- **No transmission**: 6 modes

### Modes Tested
Robot 36/72/24, AVT 90, Scottie 1/2/DX, Martin 1/2, SC2 60/120/180,
PD 50/90/120/160/180/240/290, P3/P5/P7, MR73/90/115/140/175,
MP73/115/140/175, ML180/240/280/320, B/W 8/12,
MP73-N/MP110-N/MP140-N/MC110-N/MC140-N/MC180-N

## 📝 Test Data Format (JSON)

```json
{
  "metadata": {
    "total_modes": 43,
    "specification": "VIS sequence documentation"
  },
  "modes": [
    {
      "id": 1,
      "name": "Robot 36",
      "vis_code": 136,
      "vis_hex": "0x88",
      "bits_binary": "00010001",
      "bits_lsb": [1,0,0,0,1,0,0,0],
      "bit_frequencies": [1100,1100,1100,1300,1100,1100,1100,1300],
      "parity": 0,
      "duration_ms": 36000,
      "type": "color"
    },
    ...
  ]
}
```

## 🔍 Key Features

✅ **Standalone C test** - No dependencies, pure C99  
✅ **CMake integration** - Builds with main project  
✅ **JSON fixture** - Machine-readable test data  
✅ **100% coverage** - All 43 modes tested  
✅ **Parity verified** - Even and odd modes handled correctly  
✅ **Sequence timing** - Complete VIS transmission validated  

## 🚀 Next Steps

After VIS tests pass:

1. **Phase 4**: Main Encoder (CSSTVMOD extraction)
   - Image line scanning
   - Color encoding (RGB → SSTV frequencies)
   - Line timing per mode

2. **Phase 5**: Audio Output
   - WAV file writer
   - Sample rate handling
   - Output buffering

3. **Phase 6**: Integration
   - Complete end-to-end example
   - Cross-platform testing
   - Performance benchmarks

## 📖 References

- **Test Report**: `VIS_TEST_SUITE_REPORT.md` (complete analysis)
- **Plan Document**: `ENCODING_ONLY_PLAN.md` (full schedule)
- **Source**: MMSSTV sstv.cpp lines 2000-2150 (VIS codes)
- **Spec**: SSTV VIS standard (~640ms total)

---

**Last Updated**: February 20, 2026  
**Status**: ✅ All tests passing  
**Project**: mmsstv-portable (SSTV Encoder/Decoder Library)

---

## 🧪 Advanced Signal Processing Tests

### test_hf_impairments - HF Propagation Impairment Simulator

**Purpose:** Validates DSP pipeline performance under realistic 20m HF propagation conditions.

**What it does:**
- Applies S7 noise floor (~25 dB SNR typical for weak SSTV signals)
- Simulates Rayleigh fading (QSB - slow amplitude variations)
- Adds background hum (50 Hz + harmonics from mains interference)
- Processes through complete DSP pipeline (LPF → BPF → AGC → Resonators)
- Outputs intermediate WAV files at each processing stage

**Build:**
```bash
cd build
cmake .. -DBUILD_RX=ON -DBUILD_TESTS=ON
make test_hf_impairments
```

**Usage:**
```bash
# Run with defaults (S7 conditions, 25 dB SNR)
./bin/test_hf_impairments path/to/clean_sstv.wav

# Specify output directory and custom SNR
./bin/test_hf_impairments clean.wav ./s5_test 18.0

# Test different signal strengths
./bin/test_hf_impairments clean.wav ./s9_test 35.0  # Strong signal
./bin/test_hf_impairments clean.wav ./s7_test 25.0  # Typical weak signal
./bin/test_hf_impairments clean.wav ./s5_test 18.0  # Marginal signal
./bin/test_hf_impairments clean.wav ./s3_test 12.0  # Very weak signal
```

**Output Files:**
- `00_clean_input.wav` - Original signal (baseline reference)
- `01_with_noise.wav` - After HF impairments (noise + fading + hum)
- `02_after_lpf.wav` - After 2-tap simple LPF (anti-aliasing)
- `03_after_bpf.wav` - After bandpass FIR filter (HBPF/HBPFS)
- `04_after_agc.wav` - After AGC normalization
- `05_final.wav` - Final output ready for tone detection/decoding

**SNR Guidelines:**
- **35 dB (S9):** Strong signal, perfect decode expected
- **25 dB (S7):** Typical weak signal, clean decode with minor artifacts
- **18 dB (S5):** Marginal signal, visible noise in image
- **12 dB (S3):** Very weak signal, heavy noise, difficult decode

**Use Cases:**
1. **Filter performance validation** - Compare before/after to verify noise rejection
2. **AGC effectiveness** - Check if fading is properly compensated
3. **Weak signal testing** - Verify decoder works at various S-meter readings
4. **Documentation** - Generate example signals showing DSP pipeline operation

**Expected Results:**
- BPF should suppress out-of-band noise (especially 50 Hz hum)
- AGC should normalize fading without introducing distortion
- Final output should be clean enough for tone detector operation
- Decode should succeed even with visible noise in intermediate stages

**Example Workflow:**
```bash
# 1. Generate clean SSTV signal
./bin/encode_wav --mode scottie1 --input test.jpg --output clean.wav

# 2. Apply HF impairments (S7 conditions)
./bin/test_hf_impairments clean.wav ./hf_test 25.0

# 3. Listen to intermediate stages
play hf_test/01_with_noise.wav   # Noisy signal (what you'd hear on HF)
play hf_test/04_after_agc.wav     # Clean normalized output

# 4. Decode final output to verify filter effectiveness
./bin/decode_wav hf_test/05_final.wav
```

**Technical Details:**

*HF Impairment Model:*
- **AWGN:** Additive White Gaussian Noise scaled to target SNR
- **Rayleigh Fading:** Low-pass filtered (0.2 Hz) for slow QSB, 6 dB depth
- **Background Hum:** 50 Hz + 100 Hz + 150 Hz harmonics at -40 dB

*DSP Pipeline (matches decoder.cpp):*
1. 2-tap LPF: `(x[n] + x[n-1]) / 2` - Simple anti-aliasing
2. BPF: Kaiser FIR ~104 taps, 400-2600 Hz passband
3. AGC: Peak tracking with 512× target normalization
4. Scaling: Final ×2 for tone detector range

**References:**
- Filter specs: `docs/FILTER_SPECIFICATIONS.md`
- Implementation verification: `docs/FILTER_IMPLEMENTATION_VERIFICATION.md`
- DSP pipeline: `src/decoder.cpp` lines 752-850
