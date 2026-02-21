// SpectralSubtractionDNR.cpp
// Simple spectral subtraction DNR module for SSTV DSP pipeline
// (C) 2026
#include "SpectralSubtractionDNR.h"
#include <algorithm>
#include <complex>
#include <cmath>

SpectralSubtractionDNR::SpectralSubtractionDNR(size_t frame_size, size_t hop_size)
    : frame_size_(frame_size), hop_size_(hop_size) {}

void SpectralSubtractionDNR::process(std::vector<double>& audio) {
    size_t n = audio.size();
    if (n < frame_size_) return;
    std::vector<double> window(frame_size_, 0.0);
    // Hann window
    for (size_t i = 0; i < frame_size_; ++i)
        window[i] = 0.5 * (1 - cos(2 * M_PI * i / (frame_size_ - 1)));
    std::vector<double> output(n, 0.0);
    std::vector<double> overlap(frame_size_ - hop_size_, 0.0);
    constexpr double noise_floor_factor = 0.08; // 8% spectral floor (more aggressive)
    constexpr double noise_smooth_alpha = 0.90; // Faster EMA smoothing for noise estimate
    for (size_t pos = 0; pos + frame_size_ <= n; pos += hop_size_) {
        std::vector<double> frame(audio.begin() + pos, audio.begin() + pos + frame_size_);
        for (size_t i = 0; i < frame_size_; ++i) frame[i] *= window[i];
        std::vector<std::complex<double>> spectrum(frame_size_);
        fft(frame, spectrum);
        std::vector<double> mag(frame_size_);
        for (size_t i = 0; i < frame_size_; ++i) mag[i] = std::abs(spectrum[i]);
        if (!noise_initialized_) {
            noise_mag_ = mag;
            noise_initialized_ = true;
        } else {
            // Exponential moving average smoothing for noise estimate
            for (size_t i = 0; i < frame_size_; ++i) {
                noise_mag_[i] = noise_smooth_alpha * noise_mag_[i] + (1.0 - noise_smooth_alpha) * mag[i];
            }
        }
        // Spectral subtraction with spectral floor
        for (size_t i = 0; i < frame_size_; ++i) {
            double floor_val = noise_floor_factor * noise_mag_[i];
            double clean_mag = std::max(mag[i] - noise_mag_[i], floor_val);
            double phase = std::arg(spectrum[i]);
            spectrum[i] = std::polar(clean_mag, phase);
        }
        std::vector<double> frame_out(frame_size_);
        ifft(spectrum, frame_out);
        // Overlap-add
        for (size_t i = 0; i < frame_size_; ++i) {
            size_t out_idx = pos + i;
            if (out_idx < n) {
                output[out_idx] += frame_out[i] * window[i];
            }
        }
    }
    audio = output;
}

void SpectralSubtractionDNR::set_noise_estimate(const std::vector<double>& noise_mag) {
    noise_mag_ = noise_mag;
    noise_initialized_ = true;
}

void SpectralSubtractionDNR::update_noise_estimate(const std::vector<double>& mag) {
    // Running minimum (can be improved)
    for (size_t i = 0; i < mag.size(); ++i) {
        noise_mag_[i] = std::min(noise_mag_[i], mag[i]);
    }
}


// Minimal radix-2 Cooley-Tukey FFT (recursive, not optimized)
static void fft_rec(const std::vector<std::complex<double>>& in, std::vector<std::complex<double>>& out, size_t stride, size_t offset) {
    size_t N = out.size();
    if (N == 1) {
        out[0] = in[offset];
        return;
    }
    size_t N2 = N / 2;
    std::vector<std::complex<double>> even(N2), odd(N2);
    fft_rec(in, even, stride * 2, offset);
    fft_rec(in, odd, stride * 2, offset + stride);
    for (size_t k = 0; k < N2; ++k) {
        std::complex<double> t = std::polar(1.0, -2 * M_PI * k / N) * odd[k];
        out[k] = even[k] + t;
        out[k + N2] = even[k] - t;
    }
}

void SpectralSubtractionDNR::fft(const std::vector<double>& in, std::vector<std::complex<double>>& out) {
    size_t N = in.size();
    std::vector<std::complex<double>> tmp(N);
    for (size_t i = 0; i < N; ++i) tmp[i] = in[i];
    fft_rec(tmp, out, 1, 0);
}

// Inverse FFT (normalize by N)
static void ifft_rec(const std::vector<std::complex<double>>& in, std::vector<std::complex<double>>& out, size_t stride, size_t offset) {
    size_t N = out.size();
    if (N == 1) {
        out[0] = in[offset];
        return;
    }
    size_t N2 = N / 2;
    std::vector<std::complex<double>> even(N2), odd(N2);
    ifft_rec(in, even, stride * 2, offset);
    ifft_rec(in, odd, stride * 2, offset + stride);
    for (size_t k = 0; k < N2; ++k) {
        std::complex<double> t = std::polar(1.0, 2 * M_PI * k / N) * odd[k];
        out[k] = even[k] + t;
        out[k + N2] = even[k] - t;
    }
}

void SpectralSubtractionDNR::ifft(const std::vector<std::complex<double>>& in, std::vector<double>& out) {
    size_t N = in.size();
    std::vector<std::complex<double>> tmp(N);
    ifft_rec(in, tmp, 1, 0);
    for (size_t i = 0; i < N; ++i) out[i] = tmp[i].real() / N;
}
