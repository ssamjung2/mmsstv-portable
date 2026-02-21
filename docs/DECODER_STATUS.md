# Decoder Implementation Status Summary

**Date:** February 19, 2026  
**Component:** RX Decoder Audio Processing Pipeline

---

## Quick Answer: What's Currently Enabled/Disabled?

| Component | Status | Location | Reason |
|-----------|--------|----------|--------|
| **Simple LPF** | ✅ ENABLED | decoder.cpp:607-609 | Basic smoothing |
| **BPF (Bandpass)** | ❌ DISABLED | decoder.cpp:611-621 `#if 0` | Baseline testing |
| **AGC (Auto Gain)** | ❌ DISABLED | decoder.cpp:623-629 `#if 0` | Baseline testing |
| **Scaling (×32)** | ✅ ENABLED | decoder.cpp:631-633 | Required for IIR |
| **IIR Tone Detectors** | ✅ ENABLED | decoder.cpp:634-651 | Core functionality |
| **50 Hz LPF (post-IIR)** | ✅ ENABLED | decoder.cpp:635, 639, 645, 649 | Envelope smoothing |

## Current Audio Processing Pipeline

```
Input Sample (±32768)
         ↓
   [Clip to ±24576]
         ↓
   [Simple LPF: (s + prev_s) * 0.5]  ✅ ENABLED
         ↓
   [BPF: 400-2500 Hz or 1080-2600 Hz]  ❌ DISABLED (#if 0)
         ↓
   [AGC: Normalize to 16384]  ❌ DISABLED (#if 0)
         ↓
   [Scale: d * 32, clamp ±16384]  ✅ ENABLED
         ↓
         ├─→ [IIR11 @ 1080 Hz, Q=80] → |abs| → [LPF 50Hz] → d11  ✅
         ├─→ [IIR12 @ 1200 Hz, Q=100] → |abs| → [LPF 50Hz] → d12  ✅
         ├─→ [IIR13 @ 1320 Hz, Q=80] → |abs| → [LPF 50Hz] → d13  ✅
         └─→ [IIR19 @ 1900 Hz, Q=100] → |abs| → [LPF 50Hz] → d19  ✅
                                                                    ↓
                                            [Decision Logic: VIS/Sync/Image]
```

## Why BPF and AGC Are Disabled

**Purpose:** Establish baseline decoder behavior with minimal signal processing

**Benefits:**
1. **Simpler debugging** - Fewer variables when troubleshooting VIS decode
2. **Isolate IIR filters** - Verify tone detection works without interference
3. **Direct signal path** - Easier to trace signal levels through pipeline
4. **Component testing** - Validate each stage independently

**Trade-offs:**
- ❌ No protection from out-of-band noise
- ❌ No automatic level adjustment for varying signals
- ❌ Less robust to real-world radio conditions

## Production Configuration (Recommended)

For production deployment, **ENABLE both BPF and AGC**:

```cpp
// In src/decoder.cpp, change:

// Line 611: BPF
#if 1  // Change from #if 0
if (dec->use_bpf) {
    if (dec->sync_mode >= 3 && !dec->hbpf.empty()) {
        d = dec->bpf.Do(d, dec->hbpf.data());
    } else if (!dec->hbpfs.empty()) {
        d = dec->bpf.Do(d, dec->hbpfs.data());
    }
}
#endif

// Line 623: AGC
#if 1  // Change from #if 0
level_agc_do(&dec->lvl, d);
level_agc_fix(&dec->lvl);
double ad = level_agc_apply(&dec->lvl, d);
#else
double ad = d;
#endif
```

## Comparison: MMSSTV Original vs Our Port

| Feature | MMSSTV Original | Our Port (Current) | Our Port (Production) |
|---------|-----------------|--------------------|-----------------------|
| Simple LPF | ✅ Always on | ✅ Always on | ✅ Always on |
| BPF | ✅ Enabled (m_bpf flag) | ❌ Disabled | ✅ Should enable |
| AGC | ✅ Always on | ❌ Disabled | ✅ Should enable |
| IIR Resonators | ✅ 4 filters | ✅ 4 filters | ✅ 4 filters |
| 50 Hz LPF | ✅ After IIR | ✅ After IIR | ✅ After IIR |
| VIS Frequencies | 1080/1320 Hz | 1080/1320 Hz ✅ | 1080/1320 Hz ✅ |

## Architecture Documentation

**Complete technical details:** See [`DECODER_ARCHITECTURE_BASELINE.md`](DECODER_ARCHITECTURE_BASELINE.md)

Contents include:
- Full pipeline flowchart
- Filter coefficient calculations
- Timing & sampling strategy
- Decision logic & thresholds
- MMSSTV source code references
- Production recommendations

## Test Status

**Component-Level Tests:** ✅ All passing
- IIR filters produce correct coefficients
- Tone discrimination verified (10:1 ratio)
- LPF settling behavior confirmed

**Integration Tests:** ⚠️ Partial
- Encoder generates correct 1080/1320 Hz tones ✅
- Decoder IIR filters tuned to 1080/1320 Hz ✅
- **VIS decoding still failing** ❌ (investigating)

## Next Steps

1. **Debug VIS Decode:** Investigate why decoder reads 0x00 instead of 0x88
   - Add debug output for d11/d13/d19 values during VIS bits ✅ (already done)
   - Verify VCO actually outputs 1080/1320 Hz via FFT analysis
   - Check signal levels through entire pipeline

2. **Enable AGC:** Test with automatic gain control
   - Verify normalization to 16384 target
   - Test with weak and strong signals
   - Validate discrimination threshold remains effective

3. **Enable BPF:** Test with bandpass filtering
   - Verify 1080 Hz is within HBPF passband
   - Test rejection of out-of-band noise
   - Measure impact on VIS detection

4. **Production Testing:** Full pipeline validation
   - Real-world signal tests
   - Noise immunity testing
   - Cross-compatibility with other SSTV software

---

**For detailed engineering specifications, see:**
- [`DECODER_ARCHITECTURE_BASELINE.md`](DECODER_ARCHITECTURE_BASELINE.md) - Complete architecture
- [`DSP_CONSOLIDATED_GUIDE.md`](DSP_CONSOLIDATED_GUIDE.md) - DSP component details
- [`PORTING_ANALYSIS.md`](PORTING_ANALYSIS.md) - Original MMSSTV analysis
