# MMSSTV Frequency Standard Analysis

## Critical Question

Does MMSSTV transmit and receive at **1080/1320 Hz** or **1100/1300 Hz**?

## Evidence Analysis

### 1. Decoder Configuration (RECEIVE Frequencies)

From `/Users/ssamjung/Desktop/WIP/mmsstv/sstv.cpp` lines 1772-1774:
```cpp
m_iir11.SetFreq(1080 + g_dblToneOffset, SampFreq, 80.0);  
m_iir13.SetFreq(1320 + g_dblToneOffset, SampFreq, 80.0);
```

With `g_dblToneOffset = 0.0` (default), decoder expects:
- **iir11 = 1080 Hz** 
- **iir13 = 1320 Hz**

VIS Bit Decoding Logic (line 1988):
```cpp
if( d11 > d13 ) m_VisData |= 0x0080;  // If 1080 Hz stronger → bit = 1
```

### 2. Encoder/Transmitter Configuration (TRANSMIT Frequencies)

From `/Users/ssamjung/Desktop/WIP/mmsstv/sstv.cpp` lines 2777-2778:
```cpp
m_vco.SetFreeFreq(1100 + g_dblToneOffset);  // Base = 1100 Hz (with offset=0)
m_vco.SetGain(2300 - 1100);                  // Range = 1200 Hz
```

VCO Output Formula:
```cpp
output_freq = FreeFreq + input_normalized * Gain
```

For VIS encoding with standard SSTV (1100/1300 Hz):
- `input = (1100 - 1100) / 1200 = 0.0` → outputs **1100 Hz**
- `input = (1300 - 1100) / 1200 = 0.167` → outputs **1300 Hz**

### 3. The Contradiction

**MMSSTV appears to:**
- **TRANSMIT** at 1100/1300 Hz (standard SSTV via VCO)
- **RECEIVE** at 1080/1320 Hz (via IIR filters)

This creates a **20 Hz incompatibility** between TX and RX!

### 4. Possible Explanations

#### Theory A: AFC (Automatic Frequency Control)
The `g_dblToneOffset` variable suggests AFC capability:
- During reception, AFC might detect actual signal frequency offset
- Adjusts decoder filters dynamically to match transmitter drift
- **Default offset = 0.0** means 1080/1320 Hz reception at startup

**Problem with this theory:** Why would the base frequencies be offset by -20/+20 Hz when AFC offset is 0?

#### Theory B: MMSSTV Uses Non-Standard Frequencies Throughout
Perhaps MMSSTV intentionally uses 1080/1320 Hz for both TX and RX:
- Would need to verify actual VCO base frequency in transmit code
- might be different from what we see in test/calibration code
-

 **Need to find:** Actual VIS encoding sequence in modulator

#### Theory C: Decoding Polarity Confusion
Let me verify the bit polarity mapping:

**Standard SSTV Spec:**
- 1100 Hz = SPACE = bit 0
- 1300 Hz = MARK = bit 1

**MMSSTV Decoder Logic:**
- 1080 Hz (iir11 > iir13) → bit = 1 → **MARK**
- 1320 Hz (iir13 > iir11) → bit = 0 → **SPACE**

If MMSSTV is transmitting at 1100/1300 Hz but receiving at 1080/1320 Hz, then:
- Transmitted 1300 Hz MARK would be decoded by... which filter?
  - 1300 Hz is closer to 1320 Hz (20 Hz away) than 1080 Hz (220 Hz away)
  - So iir13 (1320 Hz filter) would respond more strongly
  - Line 1988: `if( d11 > d13 )` would be FALSE
  - Bit = 0 ❌ **INVERTED!**

This would cause **incorrect decoding** unless:
1. MMSSTV also transmits at 1080/1320 Hz, OR
2. The filters are wide enough that 1300 Hz triggers iir11 (1080 Hz centered)

#### Theory D: Filter Bandwidth Analysis

IIR Filter Q-factor = 80 for data tones.

Bandwidth calculation:
```
BW = f0 / Q = 1080 / 80 = 13.5 Hz (±6.75 Hz)
```

So:
- **iir11 centered at 1080 Hz:** Responds to 1073-1087 Hz
- **iir13 centered at 1320 Hz:** Responds to 1313-1327 Hz

If transmitting at 1100/1300 Hz:
- 1100 Hz is **13 Hz away** from iir11's passband edge (1087 Hz)  
- 1300 Hz is **13 Hz away** from iir13's passband edge (1313 Hz)

With Q=80, filters have steep rolloff. At 13 Hz offset:
- Response might be -10 to -20 dB below peak

**This would cause weak discrimination!** Explains our failed VIS decoding.

## Experimental Verification Needed

### Test 1: Measure Real MMSSTV Signal
If we have an MMSSTV-generated WAV file:
```bash
# Analyze actual transmitted frequencies
python analyze_vis_detailed.py mmsstv_robot36.wav
# Expected: 1100/1300 Hz (standard) or 1080/1320 Hz (non-standard)?
```

### Test 2: Decoder Filter Response
Create test tones and measure IIR filter outputs:
```python
# Generate test tones: 1080, 1100, 1300, 1320 Hz
# Process through decoder configured with 1080/1320 Hz filters
# Measure discrimination ratio
```

### Test 3: Cross-Compatibility
```bash
# Generate signal with our encoder (1100/1300 Hz)
./bin/encode_wav our_signal.wav "robot 36" 44100

# Try to decode with decoder configured for:
# A) 1100/1300 Hz (standard - should work)
# B) 1080/1320 Hz (MMSSTV - might fail)
```

## Implications for Our Port

### Current Status

**Our Encoder:**
- Generates **1100/1300 Hz** (standard SSTV) ✓

**Our Decoder:**  
- Currently configured for **1100/1300 Hz** (standard SSTV) ✓  

**Compatibility:**
- Our encoder → Our decoder: **Should work** ✓
- Our encoder → MMSSTV decoder: **Might fail** if MMSSTV expects 1080/1320 ❌
- MMSSTV encoder → Our decoder: **Might fail** if MMSSTV transmits 1080/1320 ❌

### Solution Options

#### Option 1: Match MMSSTV Exactly
```cpp
// decoder.cpp
dec->iir11.SetFreq(1080.0, sample_rate, 80.0);  
dec->iir13.SetFreq(1320.0, sample_rate, 80.0);

// encoder.cpp - would need to change VIS freq generation
// From: 1100/1300 Hz
// To: 1080/1320 Hz
```

**Pros:** Full MMSSTV compatibility  
**Cons:** Breaks standard SSTV compliance

#### Option 2: Keep Standard SSTV
**Pros:** Interoperability with other SSTV software (QSSTV, RX-SSTV, etc.)  
**Cons:** Potential incompatibility with MMSSTV

#### Option 3: Dual-Mode Support
```cpp
enum sstv_frequency_standard {
    SSTV_FREQ_STANDARD = 0,  // 1100/1300 Hz
    SSTV_FREQ_MMSSTV = 1,    // 1080/1320 Hz  
};

// Decoder: Try both, auto-select based on signal strength
// Encoder: Configurable mode
```

**Pros:** Maximum compatibility  
**Cons:** Additional complexity

#### Option 4: Implement AFC
Follow MMSSTV's AFC architecture:
- Detect actual signal frequency offset during reception
- Dynamically adjust all filter frequencies
- Auto-adapt to transmitter frequency drift

**Pros:** Robust to frequency variations, handles radio drift  
**Cons:** Significant implementation effort

## Next Steps

1. **URGENT:** Generate an MMSSTV signal and measure actual TX frequencies
   - Use real MMSSTV Windows application  
   - Or find existing MMSSTV WAV samples
   - Run FFT analysis to confirm 1080/1320 vs 1100/1300

2. **Test filter discrimination** at various frequency offsets
   - Confirm Q=80 bandwidth calculations
   - Measure actual response at ±20 Hz offset

3. **Decision:** Choose frequency standard based on:
   - Measured MMSSTV TX frequencies (step 1)
   - Target use case (MMSSTV compatibility vs broader SSTV ecosystem)
   - Implementation complexity tolerance

4. **Implement chosen solution** and validate with cross-compatibility tests

## Preliminary Recommendation

Based on current evidence, **recommend Option 1 (Match MMSSTV)**:

**Reasoning:**
1. This is an MMSSTV port - primary goal is MMSSTV compatibility
2. The original uses 1080/1320 Hz in decoder (confirmed)
3. VCO base frequency might also use 1080 Hz (needs verification)  
4. User's use case involves "radio data interface" - suggests real-world SSTV reception where MMSSTV-generated signals are common

**Action:** Verify MMSSTV TX frequencies, then update both encoder and decoder to use 1080/1320 Hz if confirmed.
