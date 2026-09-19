/*
 * VIS encoder - internal header
 */
#ifndef SSTV_ENCODER_VIS_H
#define SSTV_ENCODER_VIS_H

/* One tone of the VIS sequence */
struct VisTone {
    double freq_hz;
    double ms;
};

/* Maximum tones in a VIS sequence: 4 header + 16 data bits + stop */
static const int kVisMaxTones = 21;

/*
 * Build the MMSSTV VIS tone sequence (TMmsstv TX):
 *   1900 Hz 300 ms, 1200 Hz 10 ms, 1900 Hz 300 ms, 1200 Hz 30 ms (start bit),
 *   `nbits` data bits LSB-first, 30 ms each (1100 Hz = 1, 1300 Hz = 0),
 *   1200 Hz 30 ms (stop bit).
 * The parity bit is part of the code (bit 7 of each byte: even parity for
 * standard codes, odd for the MR/MP/ML bytes); it is not sent separately.
 * nbits is 8 (standard VIS, 910 ms) or 16 (MMSSTV MR/MP/ML extended VIS:
 * 0x23 low byte first, then the mode byte, 1150 ms).
 * Returns the number of tones written to `out` (at most kVisMaxTones).
 */
int vis_build_tones(unsigned short code, int nbits, VisTone *out);

/* Total VIS duration in ms for 8 or 16 data bits */
double vis_duration_ms(int nbits);

/* Per-sample VIS tone generator built on vis_build_tones() */
class VISEncoder {
public:
    VISEncoder();

    void start(unsigned char code, double samplerate);
    void start_16bit(unsigned short code, double samplerate);
    double get_frequency();          /* next sample's frequency, 0.0 when done */
    bool is_complete() const;
    int get_total_samples() const;

private:
    void begin(unsigned short code, int nbits, double samplerate);

    VisTone tones[kVisMaxTones];
    int tone_count;
    int tone_index;
    double sample_freq;
    long samples_remaining;
    double fraction;                 /* carried sub-sample remainder */
    int nbits;
};

#endif
