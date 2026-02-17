/*
================================================================================
                    DSP REFERENCE VECTOR TEST HARNESS
================================================================================

PURPOSE:
  This test suite validates DSP filter implementations used in SSTV RX decoder.
  Each test applies known inputs to filters and compares output against expected
  values computed from DSP theory.

WHAT EACH TEST DOES:
  1. CIIRTANK tests: Verify 2nd-order resonator (tone detection)
  2. CIIR tests: Verify multi-order IIR filters (audio processing)
  3. DoFIR tests: Verify single-tap FIR filters (impulse response)

HOW TO READ TEST OUTPUT:
  PASS line:
    PASS CIIRTANK a0: actual=0.001618 expected=0.001618 diff=3.19e-11 rel_error=1.97e-08
    ├─ PASS: Test succeeded
    ├─ diff=3.19e-11: Absolute difference (very small = excellent)
    └─ rel_error=1.97e-08: Relative error (0.00% = perfect match)

  FAIL line:
    FAIL filter: actual=X expected=Y diff=Z rel_error=W tol=T
    └─ Means: |actual - expected| > tolerance, so test failed

  What's "good":
    ✓ rel_error < 0.001% → Excellent precision
    ✓ rel_error < 0.1%   → Good precision
    ✓ rel_error < 1%     → Acceptable precision
    ✗ rel_error > 1%     → Needs investigation

FILTER TYPES TESTED:

  A) CIIRTANK (2nd-order IIR Resonator)
     Purpose: Detect specific tones (e.g., FSK marks at 2000 Hz, spaces at 1500 Hz)
     How it works: Narrow bandpass filter tuned to resonant frequency
     Test parameters: frequency (Hz), bandwidth (Hz), sample_rate (Hz)
     Expected behavior: Peak gain at resonant frequency, sharp rolloff elsewhere

  B) CIIR (Multi-order Butterworth/Chebyshev IIR)
     Purpose: General audio filtering (anti-aliasing, bandpass, lowpass)
     How it works: Cascaded biquad stages with configurable order and ripple
     Test parameters: cutoff_freq (Hz), sample_rate (Hz), order, filter_type
     Expected behavior: Flat passband (Butterworth) or ripple (Chebyshev),
                        steep rolloff above cutoff frequency

  C) DoFIR (Single-tap FIR via Circular Buffer)
     Purpose: Lightweight finite impulse response filtering
     How it works: Maintains circular buffer of previous samples, weighted by taps
     Test parameters: filter_taps (coefficients), input_samples
     Expected behavior: Output = sum(tap[i] * buffer[i])

INTERPRETING RESULTS:

  Coefficient Mismatch:
    If actual ≠ expected:
    - Check formula implementation (do math match theory?)
    - Verify input parameters (correct fc, fs, order?)
    - Compare precision (float vs double?)

  Filter Unstable:
    If IIR impulse response grows instead of decaying:
    - Poles outside unit circle (|pole| > 1)
    - Check bilinear transform (fc must be < fs/2)
    - Verify coefficient calculation

  DoFIR Wrong Output:
    If output doesn't match expected:
    - Buffer state not initialized correctly
    - Tap coefficients wrong order
    - Each test call needs fresh buffer

RUNNING THE TESTS:
  1. Build: cmake .. -DBUILD_RX=ON -DBUILD_TESTS=ON
  2. Run: ./bin/test_dsp_reference
  3. Check output for PASS/FAIL on each line

THEORY REFERENCES (if you need deeper understanding):
  - Resonators: Oppenheim & Schafer "Discrete-Time Signal Processing" Ch 6
  - IIR Design: Oppenheim & Schafer "Discrete-Time Signal Processing" Ch 7-8
  - FIR Design: Harris "On the Use of Windows for Harmonic Analysis with the DFT"
  - Bilinear Transform: https://en.wikipedia.org/wiki/Bilinear_transform
  - Butterworth Filters: https://en.wikipedia.org/wiki/Butterworth_filter

================================================================================
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "dsp_filters.h"

using sstv_dsp::CIIRTANK;
using sstv_dsp::CIIR;
using sstv_dsp::CFIR2;
using sstv_dsp::DoFIR;
using sstv_dsp::MakeIIR;
using sstv_dsp::MakeFilter;
using sstv_dsp::MakeHilbert;

// Mathematical constant
constexpr double kPi = 3.1415926535897932384626433832795;

// Helper to compare double values and report PASS/FAIL with context
// Includes both absolute and relative error metrics
static int compare_double(const char *label, double actual, double expected, double tol) {
    double diff = std::fabs(actual - expected);
    double rel_error = 0.0;
    if (expected != 0.0) {
        rel_error = diff / std::fabs(expected);
    }
    if (diff > tol) {
        std::printf("FAIL %s: actual=%.9f expected=%.9f diff=%.9e rel_error=%.2e tol=%.9f\n",
                    label, actual, expected, diff, rel_error, tol);
        return 0;
    }
    std::printf("PASS %s: actual=%.9f expected=%.9f diff=%.9e rel_error=%.2e\n",
                label, actual, expected, diff, rel_error);
    return 1;
}

// Helper to print test header with documentation
static void print_test_header(const char *test_name, const char *description) {
    std::printf("\n================================================================\n");
    std::printf("TEST: %s\n", test_name);
    std::printf("DESC: %s\n", description);
    std::printf("================================================================\n");
}

static int test_ciirtank_coefficients() {
    /*
    CIIRTANK: Second-order IIR resonator (bandpass filter)
    
    Theory: A narrow bandpass filter for tone detection with adjustable resonant frequency.
    The resonator coefficient formula is:
        w = 2π·f / fs  (normalized angular frequency)
        a0 = sin(w) / ((fs/6) / BW)
        b1 = 2·cos(w)·exp(-π·BW/fs)
        b2 = -exp(-2π·BW/fs)
    
    Reference: Oppenheim & Schafer, "Discrete-Time Signal Processing" (Chap 6)
               Lyons, "Understanding Digital Signal Processing" (Resonators)
    
    Test Case: f=2000Hz, fs=48000Hz, BW=50Hz
        Expected: a0≈0.001618, b1≈1.9255, b2≈-0.9935
    */
    print_test_header("test_ciirtank_coefficients", 
                      "Second-order resonator impulse response and coefficient extraction");
    
    CIIRTANK tank;
    tank.SetFreq(2000.0, 48000.0, 50.0);

    // Reference coefficients computed using the MMSSTV formula
    const double expected_a0 = 0.001617619;
    const double expected_b1 = 1.925542;
    const double expected_b2 = -0.993472;

    // Probe via unit impulse: h[0]=a0, h[1]=a0*b1, h[2]=a0*b1^2 + a0*b2
    double y0 = tank.Do(1.0);      // First output sample (a0)
    double y1 = tank.Do(0.0);      // Second output sample
    double y2 = tank.Do(0.0);      // Third output sample

    int ok = 1;
    ok &= compare_double("CIIRTANK a0", y0, expected_a0, 1e-6);
    ok &= compare_double("CIIRTANK b1", y1 / y0, expected_b1, 1e-4);

    // Extract b2 from y2 = a0*b1^2 + a0*b2 => b2 = (y2/a0) - b1^2
    double inferred_b2 = (y2 / y0) - (expected_b1 * expected_b1);
    ok &= compare_double("CIIRTANK b2", inferred_b2, expected_b2, 5e-4);
    
    return ok;
}

// ===== Additional CIIRTANK test case: different frequency =====
static int test_ciirtank_100hz() {
    /*
    Test CIIRTANK with 100 Hz resonant frequency (FSK tone detection range)
    
    Test Case: f=100Hz, fs=48000Hz, BW=10Hz (Q=10, narrow resonator)
        Expected: high Q means sharp resonance with low a0 gain
    */
    print_test_header("test_ciirtank_100hz",
                      "100Hz resonator for FSK/tone detection (tight bandwidth Q=10)");
    
    CIIRTANK tank;
    tank.SetFreq(100.0, 48000.0, 10.0);
    
    // For 100Hz at 48kHz with 10Hz BW:
    // w = 2π·100/48000 = 0.01309
    // a0 = sin(w) / ((48000/6) / 10) = sin(0.01309) / 800 ≈ 0.0000164
    const double expected_a0 = 0.0000164;
    
    double y0 = tank.Do(1.0);
    
    // Use relaxed tolerance due to small magnitudes
    int ok = compare_double("CIIRTANK 100Hz a0", y0, expected_a0, 5e-6);
    
    return ok;
}

static int test_ciir_coefficients() {
    /*
    CIIR: Multi-order IIR filter using Butterworth or Chebyshev poles
    
    Theory: Butterworth filter with maximally flat passband. Uses bilinear transform
    to map analog poles to digital poles:
        z = (2 + s·Ts) / (2 - s·Ts)
    
    For a 2nd-order Butterworth (1 biquad stage):
        - Rolloff: -40 dB/decade (-12 dB/octave) above cutoff
        - No passband ripple
        - Phase response: -90° at cutoff
    
    Reference: Oppenheim & Schafer "Discrete-Time Signal Processing" (Chap 7-8)
               Proakis & Manolakis "Digital Signal Processing" (Chap 7)
    
    Test Case: fc=1000Hz, fs=48000Hz, order=2, Butterworth
    */
    print_test_header("test_ciir_butterworth_1khz",
                      "2nd-order Butterworth lowpass at 1kHz, 48kHz sampling");
    
    CIIR iir;
    iir.MakeIIR(1000.0, 48000.0, 2, 0, 0.0);  // bc=0 => Butterworth

    // Reference coefficients for order=2, Butterworth, fc=1000, fs=48000
    // Computed via bilinear transform with cutoff frequency
    const double expected_b0 = 0.003915;

    // Feed unit impulse and collect response
    double y0 = iir.Do(1.0);
    double y1 = iir.Do(0.0);
    double y2 = iir.Do(0.0);

    // First output sample is b0
    int ok = 1;
    ok &= compare_double("CIIR b0", y0, expected_b0, 5e-4);

    // Verify filter is stable: collect more samples and check for divergence
    double y3 = iir.Do(0.0);
    double y4 = iir.Do(0.0);
    double y5 = iir.Do(0.0);
    
    // Stable filter should have bounded impulse response that eventually decays
    // Check that later samples are smaller than early samples (on average)
    double early_sum = std::fabs(y1) + std::fabs(y2);
    double late_sum = std::fabs(y3) + std::fabs(y4) + std::fabs(y5);
    
    if (late_sum < early_sum) {
        std::printf("PASS CIIR stability: converging (early_sum=%.2e late_sum=%.2e)\n", early_sum, late_sum);
    } else {
        // Stability still OK if all samples bounded (not growing)
        double max_sample = std::max({std::fabs(y0), std::fabs(y1), std::fabs(y2), 
                                       std::fabs(y3), std::fabs(y4), std::fabs(y5)});
        if (max_sample < 1.0) {  // Reasonable bound for normalized filter
            std::printf("PASS CIIR stability: bounded (max=%.9f < 1.0)\n", max_sample);
        } else {
            std::printf("FAIL CIIR stability: unbounded response\n");
            ok = 0;
        }
    }

    return ok;
}

// ===== Additional CIIR test case: different cutoff frequency =====
static int test_ciir_butterworth_8khz() {
    /*
    Test CIIR with 8kHz cutoff (typical for audio filtering)
    
    Test Case: fc=8000Hz, fs=48000Hz, order=2, Butterworth
    Normalized: fc/fs = 8000/48000 = 0.1667
    
    Expected behavior: Higher cutoff => higher passband gain (and DC gain)
    For fc=8000Hz, the DC gain is much higher than fc=1000Hz
    */
    print_test_header("test_ciir_butterworth_8khz",
                      "2nd-order Butterworth lowpass at 8kHz (audio bandwidth)");
    
    CIIR iir;
    iir.MakeIIR(8000.0, 48000.0, 2, 0, 0.0);
    
    // For fc=8000Hz (normalized 0.1667), the DC gain is much higher
    // Expected to be roughly 0.15 based on empirical validation
    double y0 = iir.Do(1.0);
    
    // Just verify we get a reasonable positive output
    int ok = 1;
    if (y0 > 0.1 && y0 < 0.2) {
        std::printf("PASS CIIR 8kHz b0: actual=%.9f (in expected range 0.1-0.2)\n", y0);
    } else {
        std::printf("FAIL CIIR 8kHz b0: actual=%.9f (expected range 0.1-0.2)\n", y0);
        ok = 0;
    }
    
    return ok;
}

// ===== Additional CIIR test case: higher order =====
static int test_ciir_butterworth_4th_order() {
    /*
    Test 4th-order Butterworth (cascaded 2 biquads)
    
    Theory: 4th-order provides -80 dB/decade rolloff (vs -40 for 2nd order)
    More selective but more phase distortion
    
    Test Case: fc=2000Hz, fs=48000Hz, order=4, Butterworth
    */
    print_test_header("test_ciir_butterworth_4th_order",
                      "4th-order Butterworth lowpass at 2kHz (steeper rolloff)");
    
    CIIR iir;
    iir.MakeIIR(2000.0, 48000.0, 4, 0, 0.0);
    
    // 4th-order will have much smaller DC gain than 2nd-order due to cascading
    double y0 = iir.Do(1.0);
    int ok = 1;
    if (y0 > 0.0001 && y0 < 0.01) {
        std::printf("PASS CIIR 4th-order b0: actual=%.9f (in expected range 0.0001-0.01)\n", y0);
    } else {
        std::printf("FAIL CIIR 4th-order b0: actual=%.9f (expected range 0.0001-0.01)\n", y0);
        ok = 0;
    }
    
    return ok;
}

static int test_dofir_identity() {
    /*
    DoFIR: Single-tap FIR evaluation utility with circular buffer
    
    Theory: Circular buffer maintains tap+1 previous samples.
    IMPORTANT: hp and zp are incremented during computation, so each test
    needs its own fresh buffers to avoid pointer arithmetic errors.
    
    Implementation:
        zp buffer shifted left: zp[0..tap-1] = zp[1..tap]
        zp[tap] = new sample d
        output: sum of (zp[i] * hp[i]) for all taps
    
    Test: Identity filter (all-pass): hp = [1, 0, 0]
    After settling (2 samples), output should equal latest input
    
    Reference: Oppenheim & Schafer "Discrete-Time Signal Processing" (Chap 2)
    */
    print_test_header("test_dofir_identity",
                      "Identity FIR (pass-through): hp=[1, 0, 0]");
    
    // Each test run gets fresh buffers to avoid pointer effects
    double hp[3] = {1.0, 0.0, 0.0};
    double zp[3] = {0.0, 0.0, 0.0};
    
    // Manual trace for tap=2 (3 coefficients):
    // Input: [0.25, -0.5, 0.75, -1.0]
    // Call 0: zp=[0,0,0.25], out = 1*0 + 0*0 + 0*0.25 = 0
    // Call 1: zp=[0,0.25,-0.5], out = 1*0 + 0*0.25 + 0*(-0.5) = 0
    // Call 2: zp=[0.25,-0.5,0.75], out = 1*0.25 + 0*(-0.5) + 0*0.75 = 0.25
    // Call 3: zp=[-0.5,0.75,-1.0], out = 1*(-0.5) + 0*0.75 + 0*(-1.0) = -0.5
    
    double expected[4] = {0.0, 0.0, 0.25, -0.5};
    double in[4] = {0.25, -0.5, 0.75, -1.0};
    
    int ok = 1;
    for (int i = 0; i < 4; i++) {
        // Must recreate hp buffer for each call due to pointer increment in DoFIR
        double hp_copy[3] = {1.0, 0.0, 0.0};
        double out = DoFIR(hp_copy, zp, in[i], 2);
        char label[64];
        std::snprintf(label, sizeof(label), "DoFIR identity[%d]", i);
        ok &= compare_double(label, out, expected[i], 1e-9);
    }
    return ok;
}

// ===== Additional FIR test case: simple gain filter =====
static int test_dofir_gain() {
    /*
    Test FIR with gain factor: hp = [0.5, 0, 0]
    Expected: y[n] = 0.5 * zp[0] (current sample scaled)
    
    With tap=2:
    Call 0: zp=[0,0,x0], out = 0.5*0 = 0
    Call 1: zp=[0,x0,x1], out = 0.5*0 = 0
    Call 2: zp=[x0,x1,x2], out = 0.5*x0
    Call 3: zp=[x1,x2,x3], out = 0.5*x1
    */
    print_test_header("test_dofir_gain",
                      "FIR gain filter: hp=[0.5, 0, 0] (scale by 0.5)");
    
    double zp[3] = {0.0, 0.0, 0.0};
    double in[4] = {1.0, 2.0, -1.0, 0.5};
    
    // Expected outputs based on buffer state
    double expected[4] = {0.0, 0.0, 0.5, 1.0};  // 0.5*[0,0,1,-1] with delay
    
    int ok = 1;
    for (int i = 0; i < 4; i++) {
        double hp[3] = {0.5, 0.0, 0.0};
        double out = DoFIR(hp, zp, in[i], 2);
        char label[64];
        std::snprintf(label, sizeof(label), "DoFIR gain[%d]", i);
        ok &= compare_double(label, out, expected[i], 1e-9);
    }
    return ok;
}

// ===== Additional FIR test case: simple moving average =====
static int test_dofir_moving_average() {
    /*
    Test FIR with moving average taps: hp = [0.5, 0.5, 0]
    Expected: y[n] = 0.5*zp[0] + 0.5*zp[1]
    
    With tap=2 and input [1,2,3,4]:
    Call 0: zp=[0,0,1], out = 0.5*0 + 0.5*0 = 0
    Call 1: zp=[0,1,2], out = 0.5*0 + 0.5*1 = 0.5
    Call 2: zp=[1,2,3], out = 0.5*1 + 0.5*2 = 1.5
    Call 3: zp=[2,3,4], out = 0.5*2 + 0.5*3 = 2.5
    */
    print_test_header("test_dofir_moving_average",
                      "2-tap moving average: hp=[0.5, 0.5, 0]");
    
    double zp[3] = {0.0, 0.0, 0.0};
    double in[4] = {1.0, 2.0, 3.0, 4.0};
    double expected[4] = {0.0, 0.5, 1.5, 2.5};
    
    int ok = 1;
    for (int i = 0; i < 4; i++) {
        double hp[3] = {0.5, 0.5, 0.0};
        double out = DoFIR(hp, zp, in[i], 2);
        char label[64];
        std::snprintf(label, sizeof(label), "DoFIR MA[%d]", i);
        ok &= compare_double(label, out, expected[i], 1e-9);
    }
    return ok;
}

// ===== CFIR2 test: Kaiser FIR symmetry and normalization =====
static int test_cfir2_lpf_symmetry() {
    /*
    CFIR2 / MakeFilter validation

    Purpose: Validate FIR tap symmetry (linear phase) and unity gain
    for a Kaiser-windowed low-pass filter designed by MakeFilter().
    */
    print_test_header("test_cfir2_lpf_symmetry",
                      "CFIR2 LPF taps are symmetric and normalized");

    const int tap = 63;  // 64 taps (tap+1)
    const double fs = 48000.0;
    const double fc = 2000.0;
    const double att = 60.0;

    CFIR2 fir;
    fir.Create(tap, sstv_dsp::kFfLPF, fs, fc, fc, att, 1.0);

    int ok = 1;
    const int last = fir.GetTap();
    const int mid = last / 2;

    // Symmetry check: h[i] == h[last - i]
    for (int i = 0; i <= mid; i++) {
        double hi = fir.GetHD(i);
        double hj = fir.GetHD(last - i);
        char label[64];
        std::snprintf(label, sizeof(label), "CFIR2 symmetry[%d]", i);
        ok &= compare_double(label, hi, hj, 1e-8);
    }

    // Normalization check: sum of taps should be ~1.0 for LPF
    double sum = 0.0;
    for (int i = 0; i <= last; i++) {
        sum += fir.GetHD(i);
    }
    ok &= compare_double("CFIR2 sum", sum, 1.0, 1e-3);

    return ok;
}

// ===== Hilbert test: anti-symmetry and zero DC =====
static int test_hilbert_taps() {
    /*
    MakeHilbert validation

    Purpose: Validate that generated Hilbert taps are anti-symmetric
    and have near-zero DC response (sum of taps ~ 0).
    */
    print_test_header("test_hilbert_taps",
                      "Hilbert taps are anti-symmetric and sum to ~0");

    const int n = 63;  // tap count = n+1
    const double fs = 48000.0;
    const double fc1 = 300.0;
    const double fc2 = 3000.0;

    std::vector<double> h(n + 1, 0.0);
    MakeHilbert(h.data(), n, fs, fc1, fc2);

    int ok = 1;
    const int mid = n / 2;

    // Center tap should be ~0 for Hilbert
    ok &= compare_double("Hilbert center", h[mid], 0.0, 1e-8);

    // Anti-symmetry: h[mid + k] == -h[mid - k]
    for (int k = 1; k <= mid; k++) {
        double hp = h[mid + k];
        double hn = h[mid - k];
        char label[64];
        std::snprintf(label, sizeof(label), "Hilbert antisym[%d]", k);
        ok &= compare_double(label, hp, -hn, 1e-8);
    }

    // DC response should be near zero
    double sum = 0.0;
    for (double v : h) sum += v;
    ok &= compare_double("Hilbert sum", sum, 0.0, 1e-6);

    return ok;
}

// ===== CIIRTANK test: tone selectivity under overlapping signals =====
static int test_ciirtank_tone_selectivity() {
    /*
    Purpose: Ensure CIIRTANK responds more strongly to its target tone
    than to an overlapping nearby tone plus noise.
    */
    print_test_header("test_ciirtank_tone_selectivity",
                      "CIIRTANK selects target tone under overlap + noise");

    const double fs = 48000.0;
    const double f_target = 2000.0;
    const double f_interfere = 2300.0;
    const double bw = 50.0;

    CIIRTANK tank_target;
    CIIRTANK tank_interfere;
    tank_target.SetFreq(f_target, fs, bw);
    tank_interfere.SetFreq(f_interfere, fs, bw);

    // Deterministic pseudo-noise (LCG) for repeatability
    uint32_t seed = 0x12345678;
    auto next_noise = [&seed]() {
        seed = 1664525u * seed + 1013904223u;
        return (static_cast<double>(seed & 0xFFFF) / 32768.0) - 1.0; // [-1,1)
    };

    const int n = 2000;
    double energy_target = 0.0;
    double energy_interfere = 0.0;

    for (int i = 0; i < n; i++) {
        double t = static_cast<double>(i) / fs;
        double signal = 0.7 * std::sin(2.0 * kPi * f_target * t)
                      + 0.7 * std::sin(2.0 * kPi * f_interfere * t)
                      + 0.2 * next_noise();
        double y_target = tank_target.Do(signal);
        double y_interfere = tank_interfere.Do(signal);
        energy_target += y_target * y_target;
        energy_interfere += y_interfere * y_interfere;
    }

    // Expect higher energy at the target resonator than the interferer.
    if (energy_target > energy_interfere * 1.2) {
        std::printf("PASS CIIRTANK selectivity: target=%.3e interfere=%.3e\n",
                    energy_target, energy_interfere);
        return 1;
    }

    std::printf("FAIL CIIRTANK selectivity: target=%.3e interfere=%.3e\n",
                energy_target, energy_interfere);
    return 0;
}

// ===== CIIR test: bounded response under noisy input =====
static int test_ciir_noise_bounded() {
    /*
    Purpose: Ensure the IIR filter remains bounded under noisy input.
    */
    print_test_header("test_ciir_noise_bounded",
                      "CIIR output remains bounded under noise");

    CIIR iir;
    iir.MakeIIR(1000.0, 48000.0, 2, 0, 0.0);

    uint32_t seed = 0x9E3779B9;
    auto next_noise = [&seed]() {
        seed = 1664525u * seed + 1013904223u;
        return (static_cast<double>(seed & 0xFFFF) / 32768.0) - 1.0; // [-1,1)
    };

    double max_abs = 0.0;
    const int n = 4000;
    for (int i = 0; i < n; i++) {
        double x = 0.8 * next_noise();
        double y = iir.Do(x);
        double ay = std::fabs(y);
        if (ay > max_abs) max_abs = ay;
    }

    if (max_abs < 5.0) {  // generous bound for noisy input
        std::printf("PASS CIIR noise bounded: max=%.6f\n", max_abs);
        return 1;
    }

    std::printf("FAIL CIIR noise bounded: max=%.6f\n", max_abs);
    return 0;
}

// ===== DoFIR test: step response approaches expected steady state =====
static int test_dofir_step_response() {
    /*
    Purpose: Verify a moving-average FIR converges on a step input.
    */
    print_test_header("test_dofir_step_response",
                      "DoFIR moving-average settles to expected gain");

    double zp[3] = {0.0, 0.0, 0.0};
    const double hp_template[3] = {0.5, 0.5, 0.0};

    const int n = 10;
    double y_last = 0.0;
    for (int i = 0; i < n; i++) {
        double hp[3] = {hp_template[0], hp_template[1], hp_template[2]};
        y_last = DoFIR(hp, zp, 1.0, 2);  // step input
    }

    // After settling, output should be close to 1.0
    return compare_double("DoFIR step steady", y_last, 1.0, 1e-6);
}

// ===== Stress tests: LPF/HPF/BPF/BEF cut behavior =====
static double run_sine_rms(CFIR2 &fir, double freq, double fs, int samples, int skip) {
    fir.Clear();
    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < samples; i++) {
        double t = static_cast<double>(i) / fs;
        double x = std::sin(2.0 * kPi * freq * t);
        double y = fir.Do(x);
        if (i >= skip) {
            sum += y * y;
            count++;
        }
    }
    if (count == 0) return 0.0;
    return std::sqrt(sum / static_cast<double>(count));
}

static int test_cfir2_lpf_cut() {
    print_test_header("test_cfir2_lpf_cut",
                      "LPF passes low tone, attenuates high tone");

    const int tap = 127;
    const double fs = 48000.0;
    const double fc = 1500.0;
    const double att = 60.0;

    CFIR2 fir;
    fir.Create(tap, sstv_dsp::kFfLPF, fs, fc, fc, att, 1.0);

    const int samples = 4000;
    const int skip = 500;
    double rms_low = run_sine_rms(fir, 500.0, fs, samples, skip);
    double rms_high = run_sine_rms(fir, 5000.0, fs, samples, skip);

    if (rms_low > rms_high * 5.0) {
        std::printf("PASS CFIR2 LPF: low=%.4f high=%.4f\n", rms_low, rms_high);
        return 1;
    }
    std::printf("FAIL CFIR2 LPF: low=%.4f high=%.4f\n", rms_low, rms_high);
    return 0;
}

static int test_cfir2_hpf_cut() {
    print_test_header("test_cfir2_hpf_cut",
                      "HPF passes high tone, attenuates low tone");

    const int tap = 127;
    const double fs = 48000.0;
    const double fc = 3000.0;
    const double att = 60.0;

    CFIR2 fir;
    fir.Create(tap, sstv_dsp::kFfHPF, fs, fc, fc, att, 1.0);

    const int samples = 4000;
    const int skip = 500;
    double rms_low = run_sine_rms(fir, 500.0, fs, samples, skip);
    double rms_high = run_sine_rms(fir, 5000.0, fs, samples, skip);

    if (rms_high > rms_low * 5.0) {
        std::printf("PASS CFIR2 HPF: low=%.4f high=%.4f\n", rms_low, rms_high);
        return 1;
    }
    std::printf("FAIL CFIR2 HPF: low=%.4f high=%.4f\n", rms_low, rms_high);
    return 0;
}

static int test_cfir2_bpf_narrowband() {
    print_test_header("test_cfir2_bpf_narrowband",
                      "BPF passes in-band tone, attenuates out-of-band");

    const int tap = 127;
    const double fs = 48000.0;
    const double fcl = 1800.0;
    const double fch = 2200.0;
    const double att = 60.0;

    CFIR2 fir;
    fir.Create(tap, sstv_dsp::kFfBPF, fs, fcl, fch, att, 1.0);

    const int samples = 4000;
    const int skip = 500;
    double rms_in = run_sine_rms(fir, 2000.0, fs, samples, skip);
    double rms_out = run_sine_rms(fir, 3000.0, fs, samples, skip);

    if (rms_in > rms_out * 5.0) {
        std::printf("PASS CFIR2 BPF: in=%.4f out=%.4f\n", rms_in, rms_out);
        return 1;
    }
    std::printf("FAIL CFIR2 BPF: in=%.4f out=%.4f\n", rms_in, rms_out);
    return 0;
}

static int test_cfir2_bef_notch() {
    print_test_header("test_cfir2_bef_notch",
                      "BEF (notch) attenuates in-band tone");

    const int tap = 127;
    const double fs = 48000.0;
    const double fcl = 1900.0;
    const double fch = 2100.0;
    const double att = 60.0;

    CFIR2 fir;
    fir.Create(tap, sstv_dsp::kFfBEF, fs, fcl, fch, att, 1.0);

    const int samples = 4000;
    const int skip = 500;
    double rms_notch = run_sine_rms(fir, 2000.0, fs, samples, skip);
    double rms_pass = run_sine_rms(fir, 1500.0, fs, samples, skip);

    if (rms_pass > rms_notch * 3.0) {
        std::printf("PASS CFIR2 BEF: pass=%.4f notch=%.4f\n", rms_pass, rms_notch);
        return 1;
    }
    std::printf("FAIL CFIR2 BEF: pass=%.4f notch=%.4f\n", rms_pass, rms_notch);
    return 0;
}

int main() {
    int ok = 1;
    std::printf("\n");
    std::printf("================================================================================\n");
    std::printf("              DSP REFERENCE VECTOR TESTS FOR SSTV DECODER\n");
    std::printf("================================================================================\n");
    std::printf("Reference: docs/DSP_CONSOLIDATED_GUIDE.md for end-to-end DSP documentation\n");
    std::printf("================================================================================\n");

    // CIIRTANK tests
    ok &= test_ciirtank_coefficients();
    ok &= test_ciirtank_100hz();
    
    // CIIR tests
    ok &= test_ciir_coefficients();
    ok &= test_ciir_butterworth_8khz();
    ok &= test_ciir_butterworth_4th_order();
    
    // DoFIR tests
    ok &= test_dofir_identity();
    ok &= test_dofir_gain();
    ok &= test_dofir_moving_average();

    // CFIR2 / Hilbert tests
    ok &= test_cfir2_lpf_symmetry();
    ok &= test_hilbert_taps();

    // Non-happy path / robustness tests
    ok &= test_ciirtank_tone_selectivity();
    ok &= test_ciir_noise_bounded();
    ok &= test_dofir_step_response();

    // Stress tests: LPF/HPF/BPF/BEF cut behavior
    ok &= test_cfir2_lpf_cut();
    ok &= test_cfir2_hpf_cut();
    ok &= test_cfir2_bpf_narrowband();
    ok &= test_cfir2_bef_notch();

    std::printf("\n");
    std::printf("================================================================================\n");
    if (!ok) {
        std::printf("RESULT: DSP REFERENCE TESTS FAILED\n");
        std::printf("================================================================================\n");
        return 1;
    }
    std::printf("RESULT: DSP REFERENCE TESTS PASSED\n");
    std::printf("================================================================================\n");
    return 0;
}
