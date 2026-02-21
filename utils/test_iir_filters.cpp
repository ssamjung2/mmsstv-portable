#include <iostream>
#include <cmath>
#include <vector>
#include "../src/dsp_filters.h"

const double kPi = 3.14159265358979323846;

using namespace sstv_dsp;

int main() {
    double sample_rate = 44100.0;
    
    // Create filters matching decoder settings
    CIIRTANK iir11, iir13;
    iir11.SetFreq(1100.0, sample_rate, 80.0);
    iir13.SetFreq(1300.0, sample_rate, 80.0);
    
    // Test 1: Pure 1100 Hz tone
    std::cout << "\n=== Test 1: Pure 1100 Hz tone ===\n";
    double phase = 0.0;
    double freq = 1100.0;
    for (int i = 0; i < 3000; i++) {  // ~68ms at 44100 Hz
        double sample = std::sin(2.0 * kPi * phase);
        phase += freq / sample_rate;
        if (phase >= 1.0) phase -= 1.0;
        
        double d11 = iir11.Do(sample);
        if (d11 < 0.0) d11 = -d11;
        
        double d13 = iir13.Do(sample);
        if (d13 < 0.0) d13 = -d13;
        
        // Print every 15ms (661 samples at 44100)
        if (i % 661 == 0) {
            std::cout << "t=" << (i * 1000.0 / sample_rate) << "ms: "
                      << "d11=" << d11 << " d13=" << d13 
                      << " ratio=" << (d11/d13) << std::endl;
        }
    }
    
    // Test 2: Switch to 1300 Hz tone
    std::cout << "\n=== Test 2: Switch to 1300 Hz tone ===\n";
    freq = 1300.0;
    phase = 0.0;
    for (int i = 0; i < 3000; i++) {  // ~68ms
        double sample = std::sin(2.0 * kPi * phase);
        phase += freq / sample_rate;
        if (phase >= 1.0) phase -= 1.0;
        
        double d11 = iir11.Do(sample);
        if (d11 < 0.0) d11 = -d11;
        
        double d13 = iir13.Do(sample);
        if (d13 < 0.0) d13 = -d13;
        
        // Print every 15ms
        if (i % 661 == 0) {
            std::cout << "t=" << (i * 1000.0 / sample_rate) << "ms: "
                      << "d11=" << d11 << " d13=" << d13 
                      << " ratio=" << (d13/d11) << std::endl;
        }
    }
    
    // Test 3: Simulate VIS sequence - alternating like Robot 36 (0x88)
    // Bits: 0,0,0,1,0,0,0,1 → freqs: 1100,1100,1100,1300,1100,1100,1100,1300
    std::cout << "\n=== Test 3: Robot 36 VIS sequence (0x88) ===\n";
    int vis_bits[] = {0, 0, 0, 1, 0, 0, 0, 1};
    
    // Reset filters
    iir11.SetFreq(1100.0, sample_rate, 80.0);
    iir13.SetFreq(1300.0, sample_rate, 80.0);
    
    for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
        freq = (vis_bits[bit_idx] == 1) ? 1300.0 : 1100.0;
        phase = 0.0;
        
        // Each bit is 30ms = 1323 samples at 44100 Hz
        int samples_per_bit = (int)(30.0 * sample_rate / 1000.0);
        
        for (int i = 0; i < samples_per_bit; i++) {
            double sample = std::sin(2.0 * kPi * phase);
            phase += freq / sample_rate;
            if (phase >= 1.0) phase -= 1.0;
            
            double d11 = iir11.Do(sample);
            if (d11 < 0.0) d11 = -d11;
            
            double d13 = iir13.Do(sample);
            if (d13 < 0.0) d13 = -d13;
            
            // Sample at center of bit period (15ms = 661 samples)
            if (i == samples_per_bit / 2) {
                int decoded_bit = (d13 > d11) ? 1 : 0;
                std::cout << "Bit[" << bit_idx << "] freq=" << freq << "Hz: "
                          << "d11=" << d11 << " d13=" << d13 
                          << " → decoded=" << decoded_bit 
                          << (decoded_bit == vis_bits[bit_idx] ? " ✓" : " ✗")
                          << std::endl;
            }
        }
    }
    
    return 0;
}
