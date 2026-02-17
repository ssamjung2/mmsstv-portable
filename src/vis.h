/*
 * VIS encoder - internal header
 */
#ifndef SSTV_ENCODER_VIS_H
#define SSTV_ENCODER_VIS_H

class VISEncoder {
public:
    VISEncoder();

    void start(unsigned char code, double samplerate);
    void start_16bit(unsigned short code, double samplerate);
    double get_frequency();
    bool is_complete() const;
    int get_total_samples() const;

private:
    unsigned short vis_code;
    int state;
    double sample_freq;
    int samples_remaining;
    bool is_16bit;

    int calculate_parity(unsigned char code);
};

#endif
