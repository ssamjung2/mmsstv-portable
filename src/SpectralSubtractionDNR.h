// SpectralSubtractionDNR.h
// Simple spectral subtraction DNR module for SSTV DSP pipeline
// (C) 2026
#pragma once
#include <vector>
#include <cstddef>

class SpectralSubtractionDNR {
public:
    SpectralSubtractionDNR(size_t frame_size = 1024, size_t hop_size = 256); // 75% overlap by default
    // Process a mono audio buffer in-place
    void process(std::vector<double>& audio);
    // Optionally set noise estimate externally
    void set_noise_estimate(const std::vector<double>& noise_mag);
private:
    size_t frame_size_;
    size_t hop_size_;
    std::vector<double> noise_mag_; // running noise estimate (magnitude spectrum)
    bool noise_initialized_ = false;
    // Placeholder FFT/IFFT methods (replace with your FFT library)
    void fft(const std::vector<double>& in, std::vector<std::complex<double>>& out);
    void ifft(const std::vector<std::complex<double>>& in, std::vector<double>& out);
    // Noise estimation (simple running minimum)
    void update_noise_estimate(const std::vector<double>& mag);
};
