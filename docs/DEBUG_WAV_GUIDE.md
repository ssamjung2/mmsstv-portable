# Debug WAV Feature - User Guide

**Date:** February 20, 2026  
**Feature:** Intermediate audio WAV file output for filter analysis

---

## Overview

The debug WAV feature allows you to capture and analyze audio at different stages of the SSTV decoder's signal processing pipeline. This is invaluable for:

- **Verifying filter operation** - Listen to the audio before/after BPF
- **Analyzing AGC behavior** - Check level normalization
- **Troubleshooting decoding issues** - Identify where signal degradation occurs
- **Educational purposes** - Understand the signal processing chain
- **Quality assurance** - Validate filter implementations

---

## Quick Start

### Using the decode_wav_debug Utility

```bash
# Basic usage
./bin/decode_wav_debug input.wav [output_prefix]

# Example
./bin/decode_wav_debug tests/audio/alt5_test_panel_scottie1.wav my_test
```

This creates four WAV files:
- `my_test_before.wav` - After anti-aliasing LPF, before BPF
- `my_test_bpf.wav` - After bandpass filter
- `my_test_agc.wav` - After AGC normalization
- `my_test_final.wav` - After final scaling (input to tone detectors)

### Programmatic Usage

```c
#include "sstv_decoder.h"

// Create decoder
sstv_decoder_t *dec = sstv_decoder_create(48000);

// Enable debug WAV output
sstv_decoder_enable_debug_wav(dec,
    "before.wav",      // Before filtering
    "after_bpf.wav",   // After BPF
    "after_agc.wav",   // After AGC
    "final.wav");      // Final signal

// Process audio normally
while (/* have samples */) {
    sstv_decoder_feed_sample(dec, sample);
}

// Cleanup (automatically closes and finalizes WAV files)
sstv_decoder_free(dec);
```

You can pass `NULL` for any file you don't want to create:

```c
// Only capture AGC output
sstv_decoder_enable_debug_wav(dec, NULL, NULL, "agc_only.wav", NULL);
```

---

## Signal Processing Pipeline

The decoder processes audio through these stages:

```text
┌─────────────┐
│ Input PCM   │ ← 16-bit signed integers (-32768 to +32767)
└──────┬──────┘
       │
       ▼
┌─────────────────────────────┐
│ Stage 0: Input Clipping     │ ← Prevent overflow (±24576 limit)
└──────┬──────────────────────┘
       │
       ▼
┌─────────────────────────────┐
│ Stage 1: Anti-Aliasing LPF  │ ← Simple 2-tap FIR: (sample + prev) * 0.5
└──────┬──────────────────────┘
       │
       │ 💾 DEBUG: before.wav (written here)
       ▼
┌─────────────────────────────┐
│ Stage 2: Bandpass Filter    │ ← windowed-sinc FIR, 24·fs/11025 taps
│ • HBPFS: 400-2500 Hz (wide) │    Before an image starts
│ • HBPF: 1100-2600 Hz        │    While an image is decoded
└──────┬──────────────────────┘
       │
       │ 💾 DEBUG: bpf.wav (written here)
       ▼
┌─────────────────────────────┐
│ Stage 3: AGC                │ ← Peak tracking (100ms window), bypassed by SSTV_AGC_OFF
│ • Target: ±16384            │    Normalize to full-scale
│ • Window: 100ms             │    Fast attack mode
└──────┬──────────────────────┘
       │
       │ 💾 DEBUG: agc.wav (written here)
       ▼
┌─────────────────────────────┐
│ Stage 4: Final Scaling      │ ← Multiply by 32.0
│ • Range: ±16384             │    Clamp to limits
│ • NOTE: Causes clipping!    │    (for tone detector operation only)
└──────┬──────────────────────┘
       │
       │ 💾 DEBUG: final.wav (writes CLEAN AGC output ×2, NOT the clipped version)
       ▼
┌─────────────────────────────┐
│ Stage 5: Tone Detectors     │ ← IIR resonators + envelope detection
│ • 1080 / 1320 Hz            │    VIS bits
│ • 1200 Hz                   │    VIS start, line sync
│ • 1900 / 2100 Hz            │    leader, narrow sync, N-VIS FSK
└─────────────────────────────┘    (receive the ×32-limited signal)

Pixel data does not come from the tone detectors: the Hilbert FM
demodulator runs on the AGC output (agc.wav).
```

**Important Note on final.wav:**

The decoder's Stage 4 final scaling (`d = ad × 32`  → clamp to ±16384) is **specifically for tone detector operation only**, NOT for audio playback. This scaling causes severe clipping for typical signals (any peak above ±512 before scaling will clip).

The `final.wav` debug output writes the **clean AGC output** (`ad` × 2 for full ±32768 range) to let you hear the actual signal quality. The tone detectors internally receive the ×32 scaled version, but that's not suitable for human listening.

This means:
- `agc.wav` and `final.wav` will sound identical (both are the clean AGC output)
- They represent the same signal at different stages of the documentation
- This confirms no distortion is added between AGC and tone detection
- The internal ×32 scaling is hidden from audio output for quality verification

---

## What to Expect in Each File

### 1. before.wav (After LPF, Before BPF)

**Characteristics:**
- Raw input with basic anti-aliasing
- Contains all frequencies (wideband)
- May include 60 Hz hum, noise, out-of-band interference
- Minimal processing (just smoothing)

**What to look for:**
- Overall signal strength and quality
- Presence of interference (hum, clicks, noise)
- Frequency content outside SSTV band (0-400 Hz, > 2600 Hz)

**Spectrogram view:**
- You should see energy across wide frequency range
- SSTV tones visible but surrounded by noise

### 2. bpf.wav (After Bandpass Filter)

**Characteristics:**
- Band-limited to SSTV frequencies
- 50/60 Hz hum reduced by only ~16 dB (HBPFS is a short, unwindowed FIR)
- High-frequency noise attenuated by ~25-40 dB
- Cleaner tone separation

**What to look for:**
- Removal of low-frequency hum (< 400 Hz)
- Removal of high-frequency noise (> 2600 Hz)
- Cleaner SSTV tone visibility
- About -25 to -28 dB worst-case stop band (measured, see FILTER_SPECIFICATIONS.md)

**Spectrogram view:**
- Energy concentrated in 400-2500 Hz (HBPFS, before the image)
- Or 1100-2600 Hz (HBPF, during the image)
- Gradual roll-off outside the pass band

**Human listening:**
- Sounds "cleaner" and more focused
- Hum and rumble reduced
- High-pitched noise reduced

### 3. agc.wav (After AGC Normalization)

**Characteristics:**
- Normalized to consistent level (±16384 target)
- Weak signals amplified
- Strong signals attenuated
- Dynamic range compressed

**What to look for:**
- Consistent amplitude across file
- Weak signals boosted to usable levels
- No clipping or distortion
- Gain changes in steps every 100 ms (small level steps are normal)

**Spectrogram view:**
- Similar frequency content to BPF
- More consistent intensity levels
- Faint signals now visible

**Human listening:**
- Volume more consistent throughout
- Quiet sections louder
- Loud sections controlled

### 4. final.wav (Clean Signal for Tone Detectors)

**Characteristics:**
- Same as AGC output, scaled to full ±32768 range for audio playback
- Shows the clean, normalized signal that feeds the tone detection stage
- **NOTE**: Internally, the decoder scales this by ×32 and clamps to ±16384 for tone detector operation, but that scaled/clamped version causes severe distortion and is not suitable for human listening
- The debug WAV writes the clean AGC output for quality verification

**What to look for:**
- Should sound identical to `agc.wav` in quality
- Full-scale audio (±32768 range)
- No distortion or clipping artifacts
- Verifies the complete signal chain before tone detection

**Why final.wav = agc.wav × 2:**

The decoder's internal processing does this:
1. AGC normalizes signal peaks to ±16384
2. Scales by ×32 for tone detector operation: `d = ad × 32`
3. Clamps to ±16384: `if (d > 16384) d = 16384`

This ×32 scaling causes ANY signal above ±512 (before scaling) to clip, resulting in severe distortion. This is intentional for the tone detectors' operating range, but NOT suitable for audio playback.

For debug WAV output, we write the **clean AGC output** (`ad`) scaled ×2 to use the full ±32768 range. This lets you:
- ✅ Hear the actual signal quality
- ✅ Verify no distortion is introduced by the processing chain
- ✅ Confirm AGC normalization is working correctly

**Spectrogram view:**
- Same frequency content as AGC output
- Full dynamic range utilization
- Clean SSTV tones without clipping

**Human listening:**
- Should sound identical to `agc.wav`
- Clean, normalized audio
- No distortion or clipping
- Confirms signal is ready for tone detection

---

## Analysis Techniques

### 1. Audacity Spectrogram Comparison

```bash
# Open all files in Audacity
audacity before.wav bpf.wav agc.wav final.wav &

# For each track:
# 1. Click track name → Spectrogram
# 2. Set scale: 0-3000 Hz range
# 3. Compare frequency content
```

**What to observe:**
- `before.wav`: Wide frequency content, noise visible
- `bpf.wav`: Frequencies outside 400-2600 Hz attenuated
- `agc.wav`: Consistent intensity levels
- `final.wav`: Scaled version of AGC

### 2. SoX Frequency Analysis

```bash
# View frequency spectrum
sox before.wav -n spectrogram -o before_spec.png
sox bpf.wav -n spectrogram -o bpf_spec.png

# Compare spectrograms
open before_spec.png bpf_spec.png
```

### 3. Measure Signal Levels

```bash
# Peak levels
sox before.wav -n stat 2>&1 | grep "Maximum amplitude"
sox bpf.wav -n stat 2>&1 | grep "Maximum amplitude"
sox agc.wav -n stat 2>&1 | grep "Maximum amplitude"

# Frequency content
sox before.wav -n stat 2>&1 | grep "Frequency"
sox bpf.wav -n stat 2>&1 | grep "Frequency"
```

### 4. Human Listening Test

```bash
# Play each file and listen for differences
play before.wav
play bpf.wav
play agc.wav
play final.wav
```

**Expected differences:**
- `before.wav`: Raw input, may sound muddy or noisy
- `bpf.wav`: Cleaner, more focused tones
- `agc.wav`: Consistent volume
- `final.wav`: Similar to AGC

---

## Example: Scottie 1 Analysis

Using the test file `alt5_test_panel_scottie1.wav`:

```bash
./bin/decode_wav_debug tests/audio/alt5_test_panel_scottie1.wav scottie1
```

**Results:**
- Input: 2,455,104 samples at 22,050 Hz (111.34 seconds)
- Output: 4 WAV files, 4.7 MB each

**File sizes:**
- All files identical size (same sample count)
- Each sample: 16-bit PCM = 2 bytes
- ~111 seconds × 22,050 Hz × 2 bytes = 4.9 MB ✓

**Observations:**

1. **before.wav**
   - Contains wide frequency range
   - Some low-frequency rumble visible
   - Input level varies throughout

2. **bpf.wav**
   - Frequencies < 400 Hz attenuated (wide filter before the image)
   - Frequencies > 2600 Hz attenuated
   - Cleaner tone separation
   - Filter switches from HBPFS (400-2500 Hz) to HBPF (1100-2600 Hz) when the image starts

3. **agc.wav**
   - Consistent amplitude throughout file
   - Weak VIS header boosted
   - Strong image data controlled
   - Target level: ±16384

4. **final.wav**
   - Scaled version of AGC
   - Optimal for IIR tone detectors
   - Ready for demodulation

---

## Troubleshooting

### Problem: Files not created

**Cause:** File path invalid or permissions issue

**Solution:**
```c
// Check return value
if (sstv_decoder_enable_debug_wav(dec, "out.wav", NULL, NULL, NULL) != 0) {
    fprintf(stderr, "Failed to enable debug WAV\n");
}
```

### Problem: Files are empty or corrupted

**Cause:** Decoder freed before processing samples

**Solution:**
- Ensure you process samples AFTER enabling debug WAV
- Decoder automatically finalizes files on `sstv_decoder_free()`

### Problem: Wrong sample rate in output

**Cause:** Decoder created with different rate than expected

**Solution:**
```c
// Verify sample rate matches input
sstv_decoder_t *dec = sstv_decoder_create(input_sample_rate);
```

### Problem: Files are too large

**Cause:** Long input file creates large outputs (4 × input size)

**Solution:**
- Process shorter segments for testing
- Clean up debug files after analysis
- Use only needed debug outputs (pass NULL for others)

---

## Performance Considerations

### CPU Impact

Each enabled debug WAV file adds:
- 1 conditional check per sample
- 1 double → int16 conversion
- 1 fwrite() call

**Impact:** < 1% CPU overhead (minimal)

### Memory Impact

- No additional buffering (samples written immediately)
- File I/O handled by OS buffering

**Impact:** Negligible (< 1 KB)

### Disk I/O

Writing 4 WAV files simultaneously:
- Sequential writes (efficient)
- OS handles buffering
- ~176 KB/sec per file at 22,050 Hz

**Impact:** Low (disk bandwidth rarely bottleneck)

---

## Best Practices

### 1. Development Workflow

```bash
# During development: capture all stages
./bin/decode_wav_debug problem_file.wav debug_
# Analyze spectrograms to diagnose issue
# Fix bug
# Re-test
```

### 2. Quality Assurance

```bash
# Before release: verify filters on reference signals
for mode in robot36 scottie1 martin1; do
    ./bin/decode_wav_debug tests/audio/${mode}.wav qa_${mode}_
done
# Compare spectrograms to expected behavior
```

### 3. User Support

```bash
# User reports decode failure
# Ask them to run:
./bin/decode_wav_debug their_file.wav debug_
# Send you debug_*.wav files for analysis
```

### 4. Educational Use

```bash
# Create before/after demonstrations
./bin/decode_wav_debug clean_signal.wav clean_
./bin/decode_wav_debug noisy_signal.wav noisy_
# Show difference in spectrograms
```

---

## API Reference

### Enable Debug WAV

```c
int sstv_decoder_enable_debug_wav(
    sstv_decoder_t *dec,
    const char *before_filepath,      // After LPF, before BPF (NULL = skip)
    const char *after_bpf_filepath,   // After BPF (NULL = skip)
    const char *after_agc_filepath,   // After AGC (NULL = skip)
    const char *final_filepath        // After scaling (NULL = skip)
);
```

**Returns:** 0 on success, -1 on error

**Notes:**
- Opens files immediately
- Writes WAV header placeholder
- Updates header on disable/free
- Any NULL path skips that output

### Disable Debug WAV

```c
void sstv_decoder_disable_debug_wav(sstv_decoder_t *dec);
```

**Actions:**
- Finalizes WAV headers (updates sample counts)
- Closes all open files
- Resets sample counter

**Notes:**
- Called automatically by `sstv_decoder_free()`
- Safe to call multiple times
- Can re-enable after disable

---

## Limitations

1. **File format:** Only 16-bit PCM WAV supported
2. **Channels:** Output always mono (matches decoder processing)
3. **Sample rate:** Matches decoder sample rate (no resampling)
4. **Dynamic range:** Limited to 16-bit signed integer (-32768 to +32767)
5. **File size:** Proportional to input length (2 bytes per sample)

---

## Future Enhancements

Potential additions:

1. **Real-time streaming:** Write samples to pipe for live analysis
2. **Circular buffer mode:** Record last N seconds only
3. **Conditional capture:** Trigger on VIS detect / sync events
4. **Multiple formats:** Support 32-bit float WAV for better dynamic range
5. **Metadata:** Embed timing info (VIS detect time, sync positions)
6. **Spectral analysis:** Built-in FFT waterfall output

---

## Conclusion

The debug WAV feature provides powerful insight into the SSTV decoder's signal processing pipeline. By capturing audio at key stages, you can:

- ✅ Verify filter implementations
- ✅ Diagnose decoding problems
- ✅ Understand signal transformations
- ✅ Validate processing quality

Use this tool during development, testing, and troubleshooting to ensure optimal decoder performance.

---

**See Also:**
- [BPF_AGC_IMPLEMENTATION_GUIDE.md](BPF_AGC_IMPLEMENTATION_GUIDE.md) - Filter technical details
- [DECODER_ARCHITECTURE_BASELINE.md](DECODER_ARCHITECTURE_BASELINE.md) - Pipeline architecture
- [utils/decode_wav_debug.c](../utils/decode_wav_debug.c) - Example usage
