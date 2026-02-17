# DSP Documentation (Consolidated, Verified)

This is the **single end‑to‑end DSP document** for this repository. It is written for readers who are **new to DSP design and testing** and want a clear path from *what the filters are* → *how they’re tested* → *how they integrate with the project* → *how to verify against the original MMSSTV code*.

## 1) Scope and Entry Points

**Core DSP code**:
- [src/dsp_filters.h](../src/dsp_filters.h)
- [src/dsp_filters.cpp](../src/dsp_filters.cpp)

**Test harness**:
- [tests/test_dsp_reference.cpp](../tests/test_dsp_reference.cpp) (17 tests)

**Original MMSSTV code for verification**:
- [mmsstv/fir.cpp](../../mmsstv/fir.cpp)
- [mmsstv/fir.h](../../mmsstv/fir.h)

## 2) DSP Basics (for newcomers)

If you are not familiar with DSP, here are the minimum concepts used in the tests:

- **Sample rate $f_s$**: how many samples per second (Hz).
- **Cutoff frequency $f_c$**: the frequency where a low‑pass starts rolling off.
- **Bandwidth $BW$**: how wide a resonator’s passband is.
- **Filter order**: how steep the filter rolls off (higher = steeper).
- **Impulse response**: output when input is a single 1 followed by zeros; used to validate coefficients.
- **Stability**: an IIR filter must have a bounded impulse response (no growth).

## 3) Quick Start (Build + Run)

```bash
cd /path/to/mmsstv-portable
mkdir -p build
cd build
cmake .. -DBUILD_RX=ON -DBUILD_TESTS=ON
make test_dsp_reference
./bin/test_dsp_reference
```

## 4) DSP Components (Implementation‑Aligned)

### CIIRTANK (2nd‑order resonator)
**Purpose**: Narrow bandpass for tone detection (FSK marks/spaces).  
**Implementation**: [src/dsp_filters.cpp](../src/dsp_filters.cpp) → `CIIRTANK::SetFreq()` and `CIIRTANK::Do()`.

**Difference equation**:
$$y[n] = a0\,x[n] + b1\,y[n-1] + b2\,y[n-2]$$

**Coefficient calculation (as implemented)**:
Let $w = 2\pi f / f_s$.
$$b1 = 2\,e^{-\pi BW/f_s}\cos(w)$$
$$b2 = -e^{-2\pi BW/f_s}$$
$$a0 = \begin{cases}
\sin(w) / ((f_s/6)/BW), & BW \ne 0\\
\sin(w), & BW = 0
\end{cases}$$

**What it proves in tests**: the resonator math in `SetFreq()` matches expected theory and MMSSTV behavior.

### CIIR (cascaded biquad IIR)
**Purpose**: Low‑pass / band shaping filters (Butterworth or Chebyshev).  
**Implementation**: [src/dsp_filters.cpp](../src/dsp_filters.cpp) → `MakeIIR()` + `CIIR::Do()`.

**Notes**:
- `bc=0` → Butterworth, `bc=1` → Chebyshev (uses ripple `rp`).
- Coefficients are stored per biquad; `CIIR::Do()` cascades them.

**What it proves in tests**: coefficient generation is numerically correct and impulse response is stable.

### CFIR2 (FIR with circular buffer)
**Purpose**: FIR convolution with optional designed taps.  
**Implementation**: [src/dsp_filters.cpp](../src/dsp_filters.cpp) → `CFIR2::Create()` and `CFIR2::Do()`.

### DoFIR (simple FIR evaluation)
**Purpose**: Lightweight FIR evaluation for small tap counts.  
**Implementation**: [src/dsp_filters.cpp](../src/dsp_filters.cpp) → `DoFIR()`.

**Important**: `DoFIR()` increments the `hp` and `zp` pointers during accumulation.  
Pass a fresh `hp` pointer (or a copy) per call if you reuse the same array.

### MakeFilter (Kaiser FIR designer)
**Purpose**: Kaiser‑windowed FIR design with normalization.  
**Implementation**: [src/dsp_filters.cpp](../src/dsp_filters.cpp) → `MakeFilter()`.

**Kaiser alpha selection (as implemented)**:
$$\alpha =
\begin{cases}
0.1102\,(A-8.7), & A \ge 50\\
0.5842\,(A-21)^{0.4} + 0.07886\,(A-21), & 21 \le A < 50\\
0, & A < 21
\end{cases}$$

### MakeHilbert (Hilbert transformer taps)
**Purpose**: FIR taps for 90° phase shift between `fc1` and `fc2`.

## 5) How DSP Fits Into mmsstv‑portable

**Current state**: the DSP filters here are **core building blocks** and are validated via the test harness.  
**Original MMSSTV integration**: these filters feed SSTV RX chains (tone detection, filtering, demodulation). The original call sites are in:
- [mmsstv/sstv.cpp](../../mmsstv/sstv.cpp)
- [mmsstv/Option.cpp](../../mmsstv/Option.cpp)
- [mmsstv/Sound.cpp](../../mmsstv/Sound.cpp)

**Typical architecture flow (conceptual)**:
1. **Input audio** → **CIIR** (anti‑aliasing / band shaping)
2. **Tone selection** → **CIIRTANK** (narrowband resonators)
3. **FIR smoothing** → **DoFIR / CFIR2**
4. **Decoder logic** uses the filtered outputs for synchronization and demodulation

This consolidated doc focuses on the correctness of those building blocks.

## 6) Test Harness (What Is Verified)

**File**: [tests/test_dsp_reference.cpp](../tests/test_dsp_reference.cpp)

**Total tests**: 17

### CIIRTANK Tests
1. **test_ciirtank_coefficients**  
	- Inputs: $f=2000$ Hz, $f_s=48000$ Hz, $BW=50$ Hz  
	- Expected: $a0 \approx 0.001617619$, $b1 \approx 1.925542$, $b2 \approx -0.993472$  
	- Proves: resonator coefficients match theory and MMSSTV implementation.

2. **test_ciirtank_100hz**  
	- Inputs: $f=100$ Hz, $f_s=48000$ Hz, $BW=10$ Hz (Q≈10)  
	- Expected: $a0 \approx 1.64\times10^{-5}$ with tolerance $5\times10^{-6}$  
	- Proves: low‑frequency behavior and bandwidth scaling are correct.

### CIIR Tests (Butterworth)
3. **test_ciir_butterworth_1khz**  
	- Inputs: $f_c=1000$ Hz, order=2  
	- Expected: $b0 \approx 0.003915$ with tolerance $5\times10^{-4}$  
	- Also checks stability (bounded impulse response).

4. **test_ciir_butterworth_8khz**  
	- Inputs: $f_c=8000$ Hz, order=2  
	- Expected: $b0$ in range $0.1 \le b0 \le 0.2$  
	- Proves: higher cutoff yields larger DC gain.

5. **test_ciir_butterworth_4th_order**  
	- Inputs: $f_c=2000$ Hz, order=4  
	- Expected: $b0$ in range $0.0001 \le b0 \le 0.01$  
	- Proves: cascaded stages yield steeper rolloff and smaller gain.

### DoFIR Tests
6. **test_dofir_identity**  
	- Taps: [1, 0, 0]  
	- Input: [0.25, −0.5, 0.75, −1.0]  
	- Expected output: [0.0, 0.0, 0.25, −0.5]  
	- Proves: circular buffer and tap alignment are correct.

7. **test_dofir_gain**  
	- Taps: [0.5, 0, 0]  
	- Input: [1.0, 2.0, −1.0, 0.5]  
	- Expected output: [0.0, 0.0, 0.5, 1.0]  
	- Proves: linear gain scaling works.

8. **test_dofir_moving_average**  
	- Taps: [0.5, 0.5, 0]  
	- Input: [1.0, 2.0, 3.0, 4.0]  
	- Expected output: [0.0, 0.5, 1.5, 2.5]  
	- Proves: multi‑tap accumulation and history usage are correct.

### CFIR2 / Hilbert Tests
9. **test_cfir2_lpf_symmetry**  
	- Inputs: Kaiser‑window LPF, tap=63 (64 taps), $f_s=48000$ Hz, $f_c=2000$ Hz, $A=60$ dB  
	- Expected: symmetric taps ($h[i]=h[N-i]$) and sum ≈ 1.0  
	- Proves: FIR linear‑phase symmetry and normalization in `MakeFilter()`.

10. **test_hilbert_taps**  
	- Inputs: $n=63$, $f_s=48000$ Hz, band 300–3000 Hz  
	- Expected: anti‑symmetric taps and sum ≈ 0 (no DC)  
	- Proves: Hilbert tap generation is structurally correct.

### Robustness / Non‑Happy‑Path Tests
11. **test_ciirtank_tone_selectivity**  
	- Input: overlapping tones (2000 Hz + 2300 Hz) with additive noise  
	- Expected: target resonator energy > interferer (by margin)  
	- Proves: resonator remains selective under interference.

12. **test_ciir_noise_bounded**  
	- Input: noise signal  
	- Expected: bounded output (no instability / blow‑up)  
	- Proves: IIR remains stable under noisy conditions.

13. **test_dofir_step_response**  
	- Input: step (all ones) into a moving‑average FIR  
	- Expected: output converges to ~1.0  
	- Proves: FIR steady‑state behavior under sustained input.

### Stress Tests (NB / DNF / LPF / HPF cuts)
+14. **test_cfir2_lpf_cut**  
+	- Inputs: LPF with $f_c=1500$ Hz, compare 500 Hz vs 5000 Hz tones  
+	- Expected: low‑frequency RMS ≫ high‑frequency RMS  
+	- Proves: low‑pass cut attenuates high‑frequency content.
+
+15. **test_cfir2_hpf_cut**  
+	- Inputs: HPF with $f_c=3000$ Hz, compare 500 Hz vs 5000 Hz tones  
+	- Expected: high‑frequency RMS ≫ low‑frequency RMS  
+	- Proves: high‑pass cut attenuates low‑frequency content.
+
+16. **test_cfir2_bpf_narrowband**  
+	- Inputs: BPF with 1800–2200 Hz band, compare 2000 Hz vs 3000 Hz  
+	- Expected: in‑band RMS ≫ out‑of‑band RMS  
+	- Proves: narrow‑band filter passes target tone and rejects nearby content.
+
+17. **test_cfir2_bef_notch**  
+	- Inputs: BEF (notch) with 1900–2100 Hz band, compare 2000 Hz vs 1500 Hz  
+	- Expected: notch RMS ≪ pass‑band RMS  
+	- Proves: notch cut suppresses interfering tone.

**Note**: DoFIR outputs zero for initial samples due to the circular buffer settling. This is expected.

## 7) How to Read Test Output

```
PASS/FAIL <metric>: actual=<value> expected=<value> diff=<value> rel_error=<value>
```

- $diff = |actual - expected|$
- $rel\_error = |actual - expected| / |expected|$

**Rule of thumb**:
- rel_error $< 0.001\%$ → excellent
- rel_error $< 0.1\%$ → good
- rel_error $> 1\%$ → investigate

## 8) Troubleshooting (Most Common)

**Coefficient mismatch**
- Verify formula inputs (fs, fc, BW, order).
- Confirm `MakeIIR()` vs `CIIRTANK::SetFreq()` formula usage.

**IIR stability failure**
- Ensure $f_c < f_s/2$.
- Check bilinear transform and pole placement logic in `MakeIIR()`.

**DoFIR mismatch**
- Ensure buffer init to zero.
- Use a fresh `hp` pointer for each call.

## 9) MMSSTV Mapping (Verification)

| Portable Component | Original MMSSTV File | Original Symbol |
|---|---|---|
| `DoFIR(...)` | [mmsstv/fir.cpp](../../mmsstv/fir.cpp) | `DoFIR` |
| `MakeFilter(...)` | [mmsstv/fir.cpp](../../mmsstv/fir.cpp) | `MakeFilter` |
| `MakeHilbert(...)` | [mmsstv/fir.cpp](../../mmsstv/fir.cpp) | `MakeHilbert` |
| `MakeIIR(...)` | [mmsstv/fir.cpp](../../mmsstv/fir.cpp) | `MakeIIR` |
| `CIIRTANK` | [mmsstv/fir.cpp](../../mmsstv/fir.cpp) | `CIIRTANK` |
| `CIIR` | [mmsstv/fir.cpp](../../mmsstv/fir.cpp) | `CIIR` |
| `CFIR2` | [mmsstv/fir.cpp](../../mmsstv/fir.cpp) | `CFIR2` |
| Declarations | [mmsstv/fir.h](../../mmsstv/fir.h) | same names |

## 10) External References (DSP Theory)

- Oppenheim & Schafer, *Discrete‑Time Signal Processing* (Ch. 6–8)
- Proakis & Manolakis, *Digital Signal Processing* (IIR/FIR design)
- Lyons, *Understanding Digital Signal Processing* (resonators and intuition)
- Smith (CCRMA): https://ccrma.stanford.edu/~jos/filters/
- Butterworth filter: https://en.wikipedia.org/wiki/Butterworth_filter
- Bilinear transform: https://en.wikipedia.org/wiki/Bilinear_transform
- Kaiser window: https://en.wikipedia.org/wiki/Kaiser_window

## 11) Additional Enhancements to Consider

- **Expand CFIR2 tests**: add FFT‑based frequency response checks (passband ripple, stopband attenuation).
- **Expand Hilbert tests**: verify 90° phase shift on a sine sweep.
- **Add integration tests**: chain CIIR → CIIRTANK → DoFIR using synthetic SSTV tones.
- **Add regression vectors**: store reference outputs for fixed inputs.
- **Add performance tests**: measure CPU cost per sample and buffer sizes.
- **Add stress tests**: verify stability under random noise inputs.

## 12) Additional Types of Testing & Evaluation

- **Impulse response tests**: confirm expected coefficient extraction (already used).
- **Frequency response tests**: run FFT on impulse response and compare to expected passband/stopband.
- **Noise robustness**: verify tone detectors remain stable under noise.
- **Numerical stability**: long‑run tests to ensure IIR filters do not drift.
- **Cross‑tool validation**: compare against MATLAB/Octave/SciPy designs.

## 13) Minimal External Verification (Optional)

**CIIRTANK coefficients** (2000 Hz, 50 Hz BW, 48 kHz):
$$a0 = \sin(2\pi\cdot2000/48000) / ( (48000/6)/50 )$$
$$b1 = 2\,e^{-\pi\cdot50/48000}\cos(2\pi\cdot2000/48000)$$
$$b2 = -e^{-2\pi\cdot50/48000}$$

**Butterworth 2nd order (1 kHz)**:
```python
from scipy.signal import butter
b, a = butter(2, 1000, fs=48000, btype='low')
print(b[0], a[1], a[2])
```

## 14) Summary (One‑Screen Checklist)

- Code: [src/dsp_filters.cpp](../src/dsp_filters.cpp)
- Tests: [tests/test_dsp_reference.cpp](../tests/test_dsp_reference.cpp)
- 17 tests total, covering CIIRTANK, CIIR, DoFIR, CFIR2, Hilbert, and stress cuts
- CIIRTANK coefficients use the exact MMSSTV formula in `SetFreq()`
- DoFIR requires careful pointer handling
- Mapping table provides direct parity with MMSSTV sources
