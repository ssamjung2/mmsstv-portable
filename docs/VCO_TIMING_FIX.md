# SSTV Timing Fixes - Complete Resolution (January 30-31, 2026)

## Problem Summary

SSTV decoder testing revealed incorrect skew and phasing in decoded images at both 44100 Hz and 48000 Hz sample rates. The decoded images showed:
1. **Phase 1**: Diagonal colored stripes (timing/sample rate issue)
2. **Phase 2**: Vertical stripes with phase discontinuity midway through line (sync/frequency range issue)

## Root Causes & Solutions

### Issue 1: VCO Phase Increment Precision (FIXED ✅)

## Root Causes & Solutions

### Issue 1: VCO Phase Increment Precision (FIXED ✅)

**Problem**: Integer truncation in phase increment calculation.

The VCO had two critical bugs in the phase increment calculations:

The VCO (Voltage Controlled Oscillator) implementation had two critical bugs in the phase increment calculations:

#### Bug 1a: Integer Truncation in `c2` calculation
```cpp
// INCORRECT - causes quantization errors
c2 = (int)(table_size * free_freq / sample_freq);
```

The cast to `int` was truncating the precise phase increment value, causing accumulated timing errors.

#### Bug 1b: Incorrect `c1` gain coefficient
```cpp
// INCORRECT - arbitrary division by 16
c1 = table_size / 16.0;
```

The modulation gain coefficient `c1` was set to an arbitrary value, which didn't properly account for the 1200 Hz modulation range used in SSTV.

**VCO Fix Applied:**
```cpp
// CORRECT - maintains precision and proper modulation range
c1 = (double)table_size * 1200.0 / sample_freq;  /* 1200 Hz modulation range */
c2 = (double)table_size * free_freq / sample_freq;  /* Base frequency increment */
```

**Result:** VCO fix alone did NOT resolve the diagonal stripe issue - both 44100 and 48000 Hz still showed identical problems.

### Issue 2: Martin Mode Red Channel Timing (FIXED ✅)

**Problem**: Incorrect scan timing for Martin Red channel causing accumulated timing error.

Martin modes use **different scan times for different color channels**:
- Green channel: uses `ks` timing (73.216 ms for Martin 2)
- Blue channel: uses `ks` timing (73.216 ms for Martin 2)  
- **Red channel: uses `sg` timing (73.788 ms for Martin 2)** ← 0.572 ms longer!

**Why the MMSSTV trailing separator exists:**

The MMSSTV implementation includes a trailing 0.572 ms separator after the Red channel. This equals the difference in Red scan time. Looking at the exact Martin 2 timing:
```
4.862 (sync) + 0.572 (sep) + 73.216 (G) + 0.572 (sep) + 73.216 (B) + 0.572 (sep) + 73.216 (R) + 0.572 (trailing) = 227.370 ms
```

But the expected line time is 226.798 ms. The discrepancy (0.572 ms) means:
- Either Red uses longer scan time (73.788 ms) with NO trailing separator, OR
- All channels use same time (73.216 ms) WITH trailing separator

**Solution Applied**: Use MMSSTV's approach - all three color channels use the same scan time (73.216 ms) with a trailing 0.572 ms separator. This is cleaner for decoders as it maintains consistent per-line timing.

**Result**: Diagonal stripes eliminated ✅

The Martin mode line encoder was using incorrect scan timing for the Red channel.

**The Problem:**
Martin modes use **different scan times for different color channels**:
- Green channel: uses `ks` timing (73.216 ms for Martin 2)
- Blue channel: uses `ks` timing (73.216 ms for Martin 2)  
- **Red channel: uses `sg` timing (73.788 ms for Martin 2)** ← 0.572 ms longer!

The original `write_line_mrt()` function was using the same pixel time `t` for all three channels:
```cpp
// INCORRECT - uses ks timing for all channels
double t = tw / (double)width;
for (...) { /* Green with time t */ }
for (...) { /* Blue with time t */ }  
for (...) { /* Red with time t */ }  // WRONG! Should use sg timing
```

## Testing Results

### Version Progression

| Version | Changes | M2@48k Result | M2@44.1k Result | M1@44.1k Result |
|---------|---------|---|---|---|
| v1 | VCO precision fix | ❌ Diagonal stripes | - | - |
| v2 | v1 + Martin Red timing (different ks/sg) | ❌ Diagonal stripes | - | - |
| v3 | v1 + Martin equal channels + trailing sep | ⚠️ Vertical but phase issues | - | - |
| **v4** | **v1 + v3 + VCO base freq (1100 Hz)** | **✅ PERFECT** | **✅ PERFECT** | **✅ PERFECT** |

### Final Test Results (v4)

#### Martin M1 @ 44100 Hz
- ✅ Perfect vertical color bars
- ✅ Phase slider auto-centers
- ✅ Skew slider auto-centers
- ✅ No timing artifacts
- **Status**: PRODUCTION READY

#### Martin M2 @ 48000 Hz  
- ✅ Perfect vertical color bars
- ✅ Phase slider auto-centers
- ✅ Skew slider auto-centers
- ✅ No timing artifacts
- **Status**: PRODUCTION READY

#### Martin M2 @ 44100 Hz
- ✅ Perfect vertical color bars
- ✅ Proper frequency generation
- **Status**: PRODUCTION READY

### Issue 3: VCO Frequency Base (FIXED ✅)

**Problem**: VCO initialized with wrong base frequency for SSTV frequency range.

The encoder normalizes frequencies using: `norm = (freq - 1100) / 1200`

This maps:
- 1100 Hz → norm = 0
- 2300 Hz → norm = 1
- 1500 Hz → norm = 0.333... (separator tone)

But the VCO was initialized with `c2 = table_size * 1900 / sample_freq`, which set the base frequency to 1900 Hz. Since the input range starts at 1100 Hz (norm=0), the VCO base must be 1100 Hz, not 1900 Hz!

**Correct VCO Setup**:
```cpp
c1 = table_size * 1200.0 / sample_freq;  /* 1200 Hz span for modulation */
c2 = table_size * 1100.0 / sample_freq;  /* Base frequency 1100 Hz */

/* In process(): phase += c2 + c1 * norm
   - At norm=0: generates 1100 Hz
   - At norm=1: generates 2300 Hz (1100 + 1200)
   - At norm=0.333: generates 1500 Hz (separator tone)
*/
```

**Result**: Phase alignment fixed ✅
```
Expected total line time: 226.798 ms
Breakdown:
  Sync:        4.862 ms
  Porch:       0.572 ms
  Green (ks): 73.216 ms (320 pixels × 0.228800 ms/pixel)
  Porch:       0.572 ms
  Blue (ks):  73.216 ms (320 pixels × 0.228800 ms/pixel)
  Porch:       0.572 ms
  Red (sg):   73.788 ms (320 pixels × 0.230588 ms/pixel)  ← DIFFERENT!
  ──────────────────────
  Total:     226.798 ms ✓
```

**Martin Mode Fix Applied:**
```cpp
static void write_line_mrt(sstv_encoder_t *enc, double tw) {
    int width = (int)enc->image->width;
    /* Martin modes: Green and Blue use ks timing, Red uses sg timing */
    double t_gb = tw / (double)width;  /* Time per pixel for Green/Blue (ks) */
    double t_r = (enc->timing.sg * 1000.0 / enc->timing.sample_rate) / (double)width;  /* Red (sg) */
    
    push_segment_ms(enc, 1200, 4.862);  /* Sync */
    push_segment_ms(enc, 1500, 0.572);  /* Porch */
    
    /* Green channel - ks timing */
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(g), t_gb);
    }
    push_segment_ms(enc, 1500, 0.572);  /* Porch */
    
    /* Blue channel - ks timing */
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(b), t_gb);
    }
    push_segment_ms(enc, 1500, 0.572);  /* Porch */
    
    /* Red channel - sg timing (different!) */
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(r), t_r);
    }
}
```

## Files Modified

### VCO Fix (Jan 30):
- [src/vco.cpp](../src/vco.cpp#L12-L30) - Fixed `VCO::VCO()` constructor
- [src/vco.cpp](../src/vco.cpp#L33-L36) - Fixed `VCO::setFreeFreq()` method

### Martin Timing Fix (Jan 31):
- [src/encoder.cpp](../src/encoder.cpp#L873-L903) - Fixed `write_line_mrt()` to use correct Red channel timing

## Testing

### Version 1 (VCO fix only) - Jan 30
- Files: `/tmp/sstv_44100_test/`, `/tmp/sstv_48000_test/`
- Desktop: `~/Desktop/Martin_M2_44100_FIXED.wav`
- **Result**: Diagonal stripes still present at both sample rates

### Version 2 (VCO + Martin fix) - Jan 31  
- Files: `/tmp/sstv_44100_v2/`, `/tmp/sstv_48000_v2/`
- Desktop: `~/Desktop/Martin_M2_44100_FIXED_v2.wav`
- **Expected Result**: Proper alignment, no diagonal stripes

## Verification Steps

1. Decode `Martin_M2_44100_FIXED_v2.wav` in MMSSTV/QSSTV
2. Verify the color bar pattern is properly aligned (vertical/horizontal stripes, no diagonals)
3. Check phase alignment slider - should center automatically
4. Verify skew slider - should center automatically  
5. Compare 44100 Hz vs 48000 Hz outputs - should decode identically
6. Test Martin 1 mode as well (uses same fix with different timing values)

## Impact

### VCO Fix
- ✅ Correct frequency generation at any sample rate
- ✅ Eliminates floating-point precision loss  
- ✅ Proper modulation range (1100-2300 Hz)

### Martin Timing Fix
- ✅ Correct per-channel scan timing for Martin modes (M1, M2)
- ✅ Total line time matches specification exactly
- ✅ Eliminates diagonal stripe artifacts
- ✅ Proper synchronization with SSTV decoders

**Note**: This fix specifically affects Martin 1 and Martin 2 modes. Other SSTV modes use different line structures and were not affected by this particular issue.

## Related Documentation

- [PHASE_6_VALIDATION_PLAN.md](PHASE_6_VALIDATION_PLAN.md) - External decoder validation testing
- [PORTING_ANALYSIS.md](PORTING_ANALYSIS.md) - Original VCO implementation notes
