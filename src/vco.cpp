/*
 * VCO (Voltage Controlled Oscillator) Implementation
 * Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
 */

#include <cmath>
#include <cstdlib>

#include "vco.h"

/* Stub implementation - will be completed */
VCO::VCO(double sample_rate) {
    sample_freq = sample_rate;
    free_freq = 1900.0;  /* Default free-running frequency (used in receiver mode) */
    table_size = (int)(sample_rate * 2);
    sine_table = new double[table_size];
    
    /* Generate sine table */
    const double pi2 = 2.0 * M_PI;
    for (int i = 0; i < table_size; i++) {
        sine_table[i] = std::sin((double)i * pi2 / (double)table_size);
    }
    
    /* For SSTV transmitter: frequency range 1100-2300 Hz (1200 Hz span)
       Input normalization: norm = (freq - 1100) / 1200
       So at norm=0: freq=1100, at norm=1: freq=2300
       Phase increment: phase += c2 + c1 * norm
       We need: c2 corresponds to 1100 Hz, c1*norm spans 1200 Hz
    */
    c1 = (double)table_size * 1200.0 / sample_freq;  /* 1200 Hz span */
    c2 = (double)table_size * 1100.0 / sample_freq;  /* Base frequency 1100 Hz */
    phase = 0.0;
}

VCO::~VCO() {
    delete[] sine_table;
}

void VCO::setFreeFreq(double freq_hz) {
    free_freq = freq_hz;
    c2 = (double)table_size * free_freq / sample_freq;
}

void VCO::setGain(double gain) {
    c1 = table_size * gain / sample_freq;
}

void VCO::initPhase(void) {
    phase = 0.0;
}

double VCO::process(double input) {
    phase += c2 + c1 * input;
    while (phase >= table_size) phase -= table_size;
    while (phase < 0) phase += table_size;

    int idx = (int)phase;
    return sine_table[idx];
}
