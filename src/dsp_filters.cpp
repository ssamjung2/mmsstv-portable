/*
 * Portable DSP Filters and Utilities
 * Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
 *
 * Scope (OSS-friendly summary):
 *  - CIIRTANK: 2nd‑order resonator for tone detection
 *  - CIIR: cascaded biquad IIR (Butterworth/Chebyshev)
 *  - CFIR2 + MakeFilter: Kaiser‑windowed FIR design + runtime convolution
 *  - MakeHilbert: FIR Hilbert transformer taps
 *  - DoFIR: lightweight FIR evaluate with circular buffer
 *
 * Tests: tests/test_dsp_reference.cpp
 * Consolidated documentation: docs/DSP_CONSOLIDATED_GUIDE.md
 */

#include "dsp_filters.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace sstv_dsp {

// High-precision pi used for coefficient generation.
constexpr double kPi = 3.1415926535897932384626433832795;

// Modified Bessel function I0(x) for Kaiser window design.
static double I0(double x) {
    double sum = 1.0;
    double xj = 1.0;
    int j = 1;
    while (true) {
        xj *= ((0.5 * x) / static_cast<double>(j));
        sum += (xj * xj);
        j++;
        if (((0.00000001 * sum) - (xj * xj)) > 0) break;
    }
    return sum;
}

// Evaluate FIR using a circular buffer.
// NOTE: hp/zp pointers are advanced during accumulation; pass a fresh hp pointer per call.
double DoFIR(double *hp, double *zp, double d, int tap) {
    std::memmove(zp, &zp[1], sizeof(double) * tap);
    zp[tap] = d;
    d = 0.0;
    for (int i = 0; i <= tap; i++, hp++, zp++) {
        d += (*zp) * (*hp);
    }
    return d;
}

// Convenience overload to build FIR taps from scalar parameters.
void MakeFilter(double *hp, int tap, int type, double fs, double fcl, double fch, double att, double gain) {
    FirSpec fir;
    fir.typ = type;
    fir.n = tap;
    fir.fs = fs;
    fir.fcl = fcl;
    fir.fch = fch;
    fir.att = att;
    fir.gain = gain;
    MakeFilter(hp, &fir);
}

// Kaiser-windowed FIR designer.
// Builds a symmetric tap set (length tap+1) and normalizes gain.
void MakeFilter(double *hp, FirSpec *fp) {
    int j, m;
    double alpha, win, fm, w0, sum;
    double *h;
    
    // Allocate temporary buffer for half-band coefficients (symmetric FIR).
    std::vector<double> temp(fp->n / 2 + 1);

    if (fp->typ == kFfHPF) {
        fp->fc = 0.5 * fp->fs - fp->fcl;
    } else if (fp->typ != kFfLPF) {
        fp->fc = (fp->fch - fp->fcl) / 2.0;
    } else {
        fp->fc = fp->fcl;
    }

    if (fp->att >= 50.0) {
        alpha = 0.1102 * (fp->att - 8.7);
    } else if (fp->att >= 21.0) {
        alpha = (0.5842 * std::pow(fp->att - 21.0, 0.4)) + (0.07886 * (fp->att - 21.0));
    } else {
        alpha = 0.0;
    }

    h = temp.data();
    sum = kPi * 2.0 * fp->fc / fp->fs;
    if (fp->att >= 21.0) {
        for (j = 0; j <= (fp->n / 2); j++, h++) {
            fm = static_cast<double>(2 * j) / static_cast<double>(fp->n);
            win = I0(alpha * std::sqrt(1.0 - (fm * fm))) / I0(alpha);
            if (!j) {
                *h = fp->fc * 2.0 / fp->fs;
            } else {
                *h = (1.0 / (kPi * static_cast<double>(j))) * std::sin(static_cast<double>(j) * sum) * win;
            }
        }
    } else {
        for (j = 0; j <= (fp->n / 2); j++, h++) {
            if (!j) {
                *h = fp->fc * 2.0 / fp->fs;
            } else {
                *h = (1.0 / (kPi * static_cast<double>(j))) * std::sin(static_cast<double>(j) * sum);
            }
        }
    }

    h = temp.data();
    sum = *h++;
    for (j = 1; j <= (fp->n / 2); j++, h++) sum += 2.0 * (*h);
    h = temp.data();
    if (sum > 0.0) {
        for (j = 0; j <= (fp->n / 2); j++, h++) (*h) /= sum;
    }

    if (fp->typ == kFfHPF) {
        h = temp.data();
        for (j = 0; j <= (fp->n / 2); j++, h++) (*h) *= std::cos(static_cast<double>(j) * kPi);
    } else if (fp->typ != kFfLPF) {
        w0 = kPi * (fp->fcl + fp->fch) / fp->fs;
        if (fp->typ == kFfBPF) {
            h = temp.data();
            for (j = 0; j <= (fp->n / 2); j++, h++) (*h) *= 2.0 * std::cos(static_cast<double>(j) * w0);
        } else {
            h = temp.data();
            *h = 1.0 - (2.0 * (*h));
            for (h++, j = 1; j <= (fp->n / 2); j++, h++) (*h) *= -2.0 * std::cos(static_cast<double>(j) * w0);
        }
    }

    for (m = fp->n / 2, h = &temp[m], j = m; j >= 0; j--, h--) {
        *hp++ = (*h) * fp->gain;
    }
    for (h = &temp[1], j = 1; j <= (fp->n / 2); j++, h++) {
        *hp++ = (*h) * fp->gain;
    }
}

// FIR Hilbert transformer taps using a windowed band-limited design.
void MakeHilbert(double *h, int n, double fs, double fc1, double fc2) {
    int l = n / 2;
    double t = 1.0 / fs;

    double w1 = 2 * kPi * fc1;
    double w2 = 2 * kPi * fc2;

    double w;
    int i;
    double x1, x2;
    for (i = 0; i <= n; i++) {
        if (i == l) {
            x1 = x2 = 0.0;
        } else if ((i - l)) {
            x1 = ((i - l) * w1 * t);
            x1 = std::cos(x1) / x1;
            x2 = ((i - l) * w2 * t);
            x2 = std::cos(x2) / x2;
        } else {
            x1 = x2 = 1.0;
        }
        w = 0.54 - 0.46 * std::cos(2 * kPi * i / (n));
        h[i] = -(2 * fc2 * t * x2 - 2 * fc1 * t * x1) * w;
    }

    if (n < 8) {
        w = 0;
        for (i = 0; i <= n; i++) {
            w += std::fabs(h[i]);
        }
        if (w) {
            w = 1.0 / w;
            for (i = 0; i <= n; i++) {
                h[i] *= w;
            }
        }
    }
}

// asinh replacement for older toolchains.
static double asinh_compat(double x) {
    return std::log(x + std::sqrt(x * x + 1.0));
}

// IIR coefficient generator (Butterworth/Chebyshev).
// a[] stores biquad denominator triplets, b[] stores numerator pairs.
void MakeIIR(double *a, double *b, double fc, double fs, int order, int bc, double rp) {
    double w0, wa, u, zt, x;
    int j, n;

    if (bc) {
        u = 1.0 / static_cast<double>(order) * asinh_compat(1.0 / std::sqrt(std::pow(10.0, 0.1 * rp) - 1.0));
    }
    wa = std::tan(kPi * fc / fs);
    w0 = 1.0;
    n = (order & 1) + 1;
    double *pA = a;
    double *pB = b;
    double d1, d2;
    for (j = 1; j <= order / 2; j++, pA += 3, pB += 2) {
        if (bc) {
            d1 = std::sinh(u) * std::cos(n * kPi / (2 * order));
            d2 = std::cosh(u) * std::sin(n * kPi / (2 * order));
            w0 = std::sqrt(d1 * d1 + d2 * d2);
            zt = std::sinh(u) * std::cos(n * kPi / (2 * order)) / w0;
        } else {
            w0 = 1.0;
            zt = std::cos(n * kPi / (2 * order));
        }
        pA[0] = 1 + wa * w0 * 2 * zt + wa * w0 * wa * w0;
        pA[1] = -2 * (wa * w0 * wa * w0 - 1) / pA[0];
        pA[2] = -(1.0 - wa * w0 * 2 * zt + wa * w0 * wa * w0) / pA[0];
        pB[0] = wa * w0 * wa * w0 / pA[0];
        pB[1] = 2 * pB[0];
        n += 2;
    }
    if (bc && !(order & 1)) {
        x = std::pow(1.0 / std::pow(10.0, rp / 20.0), 1 / static_cast<double>(order / 2));
        pB = b;
        for (j = 1; j <= order / 2; j++, pB += 2) {
            pB[0] *= x;
            pB[1] *= x;
        }
    }
    if (order & 1) {
        if (bc) w0 = std::sinh(u);
        j = (order / 2);
        pA = a + (j * 3);
        pB = b + (j * 2);
        pA[0] = 1 + wa * w0;
        pA[1] = -(wa * w0 - 1) / pA[0];
        pB[0] = wa * w0 / pA[0];
        pB[1] = pB[0];
    }
}

// CIIRTANK: 2nd-order resonator (tone detector).
CIIRTANK::CIIRTANK() : z1(0.0), z2(0.0), a0(0.0), b1(0.0), b2(0.0) {
    SetFreq(2000.0, 48000.0, 50.0);
}

// Configure resonant frequency f and bandwidth bw for sample rate smp.
void CIIRTANK::SetFreq(double f, double smp, double bw) {
    double lb1, lb2, la0;
    lb1 = 2 * std::exp(-kPi * bw / smp) * std::cos(2 * kPi * f / smp);
    lb2 = -std::exp(-2 * kPi * bw / smp);
    if (bw) {
        la0 = std::sin(2 * kPi * f / smp) / ((smp / 6.0) / bw);
    } else {
        la0 = std::sin(2 * kPi * f / smp);
    }
    b1 = lb1;
    b2 = lb2;
    a0 = la0;
}

// Process one sample through the resonator.
double CIIRTANK::Do(double d) {
    d *= a0;
    d += (z1 * b1);
    d += (z2 * b2);
    z2 = z1;
    if (std::fabs(d) < 1e-37) d = 0.0;
    z1 = d;
    return d;
}

// CIIR: cascaded biquad IIR filter.
CIIR::CIIR() : order_(0), bc_(0), rp_(0.0) {
    a_.assign(kIirMax * 3, 0.0);
    b_.assign(kIirMax * 2, 0.0);
    z_.assign(kIirMax * 2, 0.0);
}

// Reset internal filter state.
void CIIR::Clear(void) {
    std::fill(z_.begin(), z_.end(), 0.0);
}

// Build IIR coefficients and clear state if needed.
void CIIR::MakeIIR(double fc, double fs, int order, int bc, double rp) {
    if (order < 1) order = 1;
    if (order > kIirMax) order = kIirMax;
    order_ = order;
    bc_ = bc;
    rp_ = rp;
    ::sstv_dsp::MakeIIR(a_.data(), b_.data(), fc, fs, order, bc, rp);
}

// Process one sample through cascaded biquads (Direct Form II-like state).
double CIIR::Do(double d) {
    double *pA = a_.data();
    double *pB = b_.data();
    double *pZ = z_.data();
    double o;
    for (int i = 0; i < order_ / 2; i++, pA += 3, pB += 2, pZ += 2) {
        d += pZ[0] * pA[1] + pZ[1] * pA[2];
        o = d * pB[0] + pZ[0] * pB[1] + pZ[1] * pB[0];
        pZ[1] = pZ[0];
        if (std::fabs(d) < 1e-37) d = 0.0;
        pZ[0] = d;
        d = o;
    }
    if (order_ & 1) {
        d += pZ[0] * pA[1];
        o = d * pB[0] + pZ[0] * pB[0];
        if (std::fabs(d) < 1e-37) d = 0.0;
        pZ[0] = d;
        d = o;
    }
    return d;
}

// CFIR2: FIR with circular buffer and optional precomputed taps.
CFIR2::CFIR2() : zp_(nullptr), w_(0), tap_(0), tap_half_(0) {}

// Allocate/reset circular buffer for a given tap count.
void CFIR2::Create(int tap) {
    if (!tap) {
        z_.clear();
        zp_ = nullptr;
    } else if ((tap_ != tap) || z_.empty()) {
        z_.assign((tap + 1) * 2, 0.0);
        w_ = 0;
    }
    tap_ = tap;
    tap_half_ = tap / 2;
}

// Allocate taps + buffer, then design the FIR using MakeFilter.
void CFIR2::Create(int tap, int type, double fs, double fcl, double fch, double att, double gain) {
    if ((tap_ != tap) || z_.empty() || h_.empty()) {
        z_.assign((tap + 1) * 2, 0.0);
        h_.assign(tap + 1, 0.0);
        w_ = 0;
    }
    tap_ = tap;
    tap_half_ = tap / 2;
    MakeFilter(h_.data(), tap, type, fs, fcl, fch, att, gain);
}

// Clear FIR state buffer (zeros the delay line).
void CFIR2::Clear(void) {
    if (!z_.empty()) {
        std::fill(z_.begin(), z_.end(), 0.0);
    }
}

// Convolve one sample using internally stored taps.
double CFIR2::Do(double d) {
    double *dp1 = &z_[w_ + tap_ + 1];
    zp_ = dp1;
    *dp1 = d;
    z_[w_] = d;
    d = 0;
    double *hp = h_.data();
    for (int i = 0; i <= tap_; i++) {
        d += (*dp1--) * (*hp++);
    }
    w_++;
    if (w_ > tap_) w_ = 0;
    return d;
}

// Convolve one sample using caller-supplied taps.
double CFIR2::Do(double d, double *hp) {
    double *dp1 = &z_[w_ + tap_ + 1];
    zp_ = dp1;
    *dp1 = d;
    z_[w_] = d;
    d = 0;
    for (int i = 0; i <= tap_; i++) {
        d += (*dp1--) * (*hp++);
    }
    w_++;
    if (w_ > tap_) w_ = 0;
    return d;
}

// Convolve using the last written sample position (zp_).
double CFIR2::Do(double *hp) {
    double d = 0;
    double *dp = zp_;
    for (int i = 0; i <= tap_; i++) {
        d += (*dp--) * (*hp++);
    }
    return d;
}

// Dual output: j = filtered sample, d = delayed sample at half the tap length.
void CFIR2::Do(double &d, double &j, double *hp) {
    double *dp1 = &z_[w_ + tap_ + 1];
    zp_ = dp1;
    *dp1 = d;
    z_[w_] = d;
    double dd = 0;
    for (int i = 0; i <= tap_; i++) {
        dd += (*dp1--) * (*hp++);
    }
    j = dd;
    d = z_[w_ + tap_half_ + 1];
    w_++;
    if (w_ > tap_) w_ = 0;
}

} // namespace sstv_dsp
