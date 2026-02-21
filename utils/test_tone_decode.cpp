#include <cstdio>
#include <cstdint>
#include "../src/dsp_filters.h"

using namespace sstv_dsp;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <wav_file>\n", argv[0]);
        return 1;
    }
    
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", argv[1]);
        return 1;
    }
    
    // Skip WAV header (44 bytes)
    fseek(f, 44, SEEK_SET);
    
    double sample_rate = 48000.0;
    
    // Initialize IIR filters (MMSSTV frequencies)
    CIIRTANK iir11, iir12, iir13, iir19;
    iir11.SetFreq(1080.0, sample_rate, 80.0);
    iir12.SetFreq(1200.0, sample_rate, 100.0);
    iir13.SetFreq(1320.0, sample_rate, 80.0);
    iir19.SetFreq(1900.0, sample_rate, 100.0);
    
    // Initialize LPFs
    CIIR lpf11, lpf12, lpf13, lpf19;
    lpf11.MakeIIR(50.0, sample_rate, 2, 0, 0);
    lpf12.MakeIIR(50.0, sample_rate, 2, 0, 0);
    lpf13.MakeIIR(50.0, sample_rate, 2, 0, 0);
    lpf19.MakeIIR(50.0, sample_rate, 2, 0, 0);
    
    int16_t pcm;
    int sample_count = 0;
    double d11_sum = 0, d13_sum = 0, d19_sum = 0;
    int sample_window = 0;
    
    while (fread(&pcm, sizeof(int16_t), 1, f) == 1) {
        // Convert to double
        double sample = (double)pcm;
        
        // Scale like decoder (simple LPF + scaling)
        double d = sample * 32.0;
        if (d > 16384.0) d = 16384.0;
        if (d < -16384.0) d = -16384.0;
        
        // Run through IIR filters
        double d11 = iir11.Do(d);
        if (d11 < 0.0) d11 = -d11;
        d11 = lpf11.Do(d11);
        
        double d13 = iir13.Do(d);
        if (d13  < 0.0) d13 = -d13;
        d13 = lpf13.Do(d13);
        
        double d19 = iir19.Do(d);
        if (d19 < 0.0) d19 = -d19;
        d19 = lpf19.Do(d19);
        
        d11_sum += d11;
        d13_sum += d13;
        d19_sum += d19;
        sample_window++;
        
        // Print average every 1000 samples (~22ms at 44100 Hz)
        if (sample_window >= 1000) {
            double avg11 = d11_sum / sample_window;
            double avg13 = d13_sum / sample_window;
            double avg19 = d19_sum / sample_window;
            
            printf("Samples %d-%d: d11=%.1f d13=%.1f d19=%.1f | ",
                   sample_count, sample_count + sample_window - 1,
                   avg11, avg13, avg19);
            
            if (avg11 > avg13 * 1.5 && avg11 > avg19 * 0.5) {
                printf("DETECTED: 1080 Hz\n");
            } else if (avg13 > avg11 * 1.5 && avg13 > avg19 * 0.5) {
                printf("DETECTED: 1320 Hz\n");
            } else if (avg19 > avg11 && avg19 > avg13) {
                printf("DETECTED: 1900 Hz\n");
            } else {
                printf("INDETERMINATE (ratio d13/d11=%.2f)\n", avg13/avg11);
            }
            
            d11_sum = 0;
            d13_sum = 0;
            d19_sum = 0;
            sample_count += sample_window;
            sample_window = 0;
        }
    }
    
    fclose(f);
    return 0;
}
