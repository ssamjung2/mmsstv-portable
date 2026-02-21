#include <stdio.h>
#include <math.h>
#include "../src/vco.h"

int main() {
    double sample_rate = 48000.0;
    VCO vco(sample_rate);
    
    printf("Testing VCO with MMSSTV parameters:\n");
    vco.setFreeFreq(1080.0);
    vco.setGain(1220.0);
    
    // Test 1080 Hz (norm=0.0)
    printf("\n1080 Hz (norm=0.0):\n");
    vco.initPhase();
    for (int i = 0; i < 10; i++) {
        double out = vco.process(0.0);
        printf("  sample %d: %f\n", i, out);
    }
    
    // Test 1320 Hz (norm = (1320-1080)/1220 = 0.1967)
    printf("\n1320 Hz (norm=0.1967):\n");
    vco.initPhase();
    for (int i = 0; i < 10; i++) {
        double out = vco.process(0.1967);
        printf("  sample %d: %f\n", i, out);
    }
    
    return 0;
}
