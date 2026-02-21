#!/bin/bash

# Quick test script to diagnose CIIRTANK amplitude behavior

cat > /tmp/test_ciirtank.cpp << 'TESTEOF'
#include <cmath>
#include <cstdio>

// From dsp_filters.cpp
constexpr double kPi = 3.1415926535897932384626433832795;

struct CIIRTANK {
    double z1, z2, a0, b1, b2;
    
    CIIRTANK() : z1(0.0), z2(0.0), a0(0.0), b1(0.0), b2(0.0) {}
    
    void SetFreq(double f, double smp, double bw) {
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
        
        printf("SetFreq(%f Hz, %f smp, %f bw):\n", f, smp, bw);
        printf("  sin(2*pi*f/smp) = %f\n", std::sin(2 * kPi * f / smp));
        printf("  Divisor = %f / %f = %f\n", smp / 6.0, bw, (smp / 6.0) / bw);
        printf("  a0 = %e\n", a0);
        printf("  b1 = %f\n", lb1);
        printf("  b2 = %f\n", lb2);
    }
    
    double Do(double d) {
        d *= a0;
        d += (z1 * b1);
        d += (z2 * b2);
        z2 = z1;
        if (std::fabs(d) < 1e-37) d = 0.0;
        z1 = d;
        return d;
    }
};

int main() {
    printf("Testing CIIRTANK amplitude at 1100 Hz, 1300 Hz, 1200 Hz\n");
    printf("========================================================\n\n");
    
    double sample_rate = 48000.0;
    CIIRTANK mark, space, sync;
    
    mark.SetFreq(1100.0, sample_rate, 100.0);
    printf("\n");
    space.SetFreq(1300.0, sample_rate, 100.0);
    printf("\n");
    sync.SetFreq(1200.0, sample_rate, 100.0);
    
    return 0;
}
TESTEOF

g++ -o /tmp/test_ciirtank /tmp/test_ciirtank.cpp && /tmp/test_ciirtank
