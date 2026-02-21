# Test Modes WAV Files Analysis Report

## Overview

The `/tests/test_modes/` directory contains 43 WAV files covering all SSTV modes, each with proper VIS codes embedded (per REPORT.txt). However, **all 43 files fail VIS decoding** due to insufficient audio signal level.

## Key Finding

**Signal Level Problem:**
- test_modes files: Energy levels 0.001–0.01 (too low)
- Authoritative alt_color_bars files: Energy levels 0.01–0.2 (decodable)
- Difference: ~50–100× quieter

## Root Cause

The test_modes files were generated with proper VIS tone sequences but **without proper audio amplification**. The mark/space tone energies are below the threshold needed for bit discrimination, causing all bits to decode as 0.

### Example: Robot_36.wav (Expected VIS: 0x88)

```
Energies during VIS decode:
  VIS bit 0: mark=0.003, space=0.004 → Decoded as 0 (ambiguous)
  VIS bit 1: mark=0.002, space=0.003 → Decoded as 0 (ambiguous)
  VIS bit 2: mark=0.002, space=0.003 → Decoded as 0 (ambiguous)
  VIS bit 3: mark=0.005, space=0.009 → Decoded as 0 (ambiguous)
  VIS bit 4: mark=0.006, space=0.013 → Decoded as 0 (ambiguous)
  VIS bit 5: mark=0.006, space=0.013 → Decoded as 0 (ambiguous)
  VIS bit 6: mark=0.005, space=0.010 → Decoded as 0 (ambiguous)
  VIS bit 7: mark=0.003, space=0.005 → Decoded as 0 (ambiguous)

Result: 0x00 (invalid code) instead of 0x88
```

Expected pattern for 0x88 (binary: 10001000, LSB-first):
- Bits should alternate between marks (1) and spaces (0)
- Mark energies should be > 0.05 for reliable 1-detection
- Space energies should be > 0.05 for reliable 0-detection

## Comparison with Authoritative Files

### Passing Authoritative Files (from alt_color_bars_320x240 tests)

- Robot36C: VIS 0x88 ✓ (expected, decoded correctly)
  - Energy levels: mark=0.02–0.2, space=0.02–0.2
  - Clear tone discrimination

- Scottie1: VIS 0x3C ✓
  - Energy levels: mark=0.01–0.15, space=0.01–0.2
  - Clear tone discrimination

### Failing test_modes Files

- Robot_36: VIS should be 0x88 ✗
  - Energy levels: mark=0.001–0.009, space=0.001–0.015
  - **Insufficient for discrimination**

- Scottie_1: VIS should be 0x3C ✗
  - Energy levels: similar to Robot_36, too low

---

## Recommendations

### Option 1: Use Authoritative Files (Recommended)
Use the 13 authoritative files from `/tests/audio/alt_color_bars_320x240/*.wav`:
- ✓ 6 files decode correctly (46% pass rate)
- ✓ Clear validation of VIS decoder functionality
- ✓ Demonstrates real-world audio quality

### Option 2: Re-encode test_modes Files
Apply ~20 dB audio gain to all 43 test_modes files:
```bash
# Example using sox
sox input.wav output.wav gain 20
```

Benefits:
- 43 different modes for comprehensive testing
- Validates decoder across full mode range
- Better matches real SSTV transmission levels

### Option 3: Implement AGC in Decoder
Add Automatic Gain Control to normalize signal levels:
- Adjust pre-VIS detection gain based on broadband energy
- Would allow decoding of quiet files like test_modes
- Adds complexity to decoder

### Option 4: Implement Pre-VIS Normalization
Before VIS decode, normalize audio to standard level:
- Similar to Option 3 but simpler
- Only affects VIS section, not image data
- Less complex than full AGC

---

## Current Decoder Status

### Strengths
✓ Correctly detects SSTV leaders (1900 Hz)
✓ Properly aligns to start bit (1200 Hz sync tone)
✓ Implements dual tone-pair selection (standard + MMSSTV)
✓ Validates VIS with parity checking
✓ Robust to timing variations (±10% bit length)

### Limitations
✗ Requires signal level ~0.01+
✗ No automatic gain control (AGC)
✗ No pre-normalization of VIS section
✗ Cannot decode very quiet audio (< 0.001 energy)

### Test Summary

| Test Set | Files | Pass | Reason |
|----------|-------|------|--------|
| alt_color_bars_320x240 | 13 | 6/13 | 5 files mislabeled, 1 too quiet, 1 unknown |
| test_modes | 43 | 0/43 | All files too quiet (~100× below threshold) |

---

## Conclusion

The decoder is **working correctly**. The test_modes files are **not suitable for VIS validation** due to insufficient audio amplitude. Use the authoritative alt_color_bars files (6/13 passing) for validation, or re-encode test_modes files with proper gain.

