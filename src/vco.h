/*
 * VCO (Voltage Controlled Oscillator) - internal header
 */
#ifndef SSTV_ENCODER_VCO_H
#define SSTV_ENCODER_VCO_H

class VCO {
public:
    explicit VCO(double sample_rate);
    ~VCO();

    void setFreeFreq(double freq_hz);
    void setGain(double gain);
    void initPhase(void);  /* Reset phase to 0 for line synchronization */
    double process(double input);

private:
    double *sine_table;
    int table_size;
    double sample_freq;
    double free_freq;
    double c1;
    double c2;
    double phase;
};

#endif
