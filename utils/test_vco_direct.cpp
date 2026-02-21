#include <cstdio>
#include "../src/vco.h"
#include "../src/dsp_filters.h"

int main() {
    double sample_rate = 48000.0;
    VCO vco(sample_rate);
    vco.setFreeFreq(1080.0);
    vco.setGain(1220.0);
    
    // Create IIR filters to measure output
    sstv_dsp::CIIRTANK iir11, iir13;
    sstv_dsp::CIIR lpf11, lpf13;
    iir11.SetFreq(1080.0, sample_rate, 80.0);
    iir13.SetFreq(1320.0, sample_rate, 80.0);
    lpf11.MakeIIR(50.0, sample_rate, 2, 0, 0);
    lpf13.MakeIIR(50.0, sample_rate, 2, 0, 0);
    
    printf("=== Test 1: VCO with norm=0.0 (should generate 1080 Hz) ===\n");
    vco.initPhase();
    
    double norm_1080 = 0.0;  // Should produce 1080 Hz
    double d11 = 0, d13 = 0;
    
    for (int i = 0; i < 10000; i++) {
        double sample = vco.process(norm_1080) * 32.0;
        
        double out11 = iir11.Do(sample);
        if (out11 < 0) out11 = -out11;
        out11 = lpf11.Do(out11);
        
        double out13 = iir13.Do(sample);
        if (out13 < 0) out13 = -out13;
        out13 = lpf13.Do(out13);
        
        if (i >= 9000) {  // Last 1000 samples after settling
            d11 += out11;
            d13 += out13;
        }
    }
    d11 /= 1000.0;
    d13 /= 1000.0;
    
    printf("Results: d11=%.2f  d13=%.2f  ratio(d11/d13)=%.2f\n", d11, d13, d11/d13);
    printf("Expected: d11 >> d13 (ratio > 5)\n");
    printf("Actual: %s\n\n", (d11/d13 > 5.0) ? "PASS ✓" : "FAIL ✗");
    
    printf("=== Test 2: VCO with norm=0.1967 (should generate 1320 Hz) ===\n");
    
    // Reset filters
    iir11.SetFreq(1080.0, sample_rate, 80.0);
    iir13.SetFreq(1320.0, sample_rate, 80.0);
    lpf11.MakeIIR(50.0, sample_rate, 2, 0, 0);
    lpf13.MakeIIR(50.0, sample_rate, 2, 0, 0);
    
    vco.initPhase();
    double norm_1320 = (1320.0 - 1080.0) / 1220.0;  // Should produce 1320 Hz
    d11 = 0; d13 = 0;
    
    for (int i = 0; i < 10000; i++) {
        double sample = vco.process(norm_1320) * 32.0;
        
        double out11 = iir11.Do(sample);
        if (out11 < 0) out11 = -out11;
        out11 = lpf11.Do(out11);
        
        double out13 = iir13.Do(sample);
        if (out13 < 0) out13 = -out13;
        out13 = lpf13.Do(out13);
        
        if (i >= 9000) {
            d11 += out11;
            d13 += out13;
        }
    }
    d11 /= 1000.0;
    d13 /= 1000.0;
    
    printf("Results: d11=%.2f  d13=%.2f  ratio(d13/d11)=%.2f\n", d11, d13, d13/d11);
    printf("Expected: d13 >> d11 (ratio > 5)\n");
    printf("Actual: %s\n", (d13/d11 > 5.0) ? "PASS ✓" : "FAIL ✗");
    
    return 0;
}
