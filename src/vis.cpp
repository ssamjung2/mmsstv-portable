/*
 * VIS (Vertical Interval Signaling) Code Encoder
 * Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
 *
 * VIS identifies the SSTV mode at the start of a transmission. The sequence
 * below is exactly what MMSSTV transmits (Main.cpp, TMmsstv VIS output) and
 * matches the SSTV Handbook (sec. 3.6.2, 4.4.3):
 *
 *   Leader      1900 Hz  300 ms
 *   Break       1200 Hz   10 ms
 *   Leader      1900 Hz  300 ms
 *   Start bit   1200 Hz   30 ms
 *   Data bits   LSB first, 30 ms each: 1100 Hz = 1, 1300 Hz = 0
 *   Stop bit    1200 Hz   30 ms
 *
 * Standard VIS sends 8 data bits whose bit 7 is the parity bit (even parity,
 * e.g. Robot 36 = 0x88 = code 0x08 + parity; MMSSTV sends B/W 12 as 0x86,
 * which is odd): 910 ms in total.
 * MMSSTV's MR/MP/ML modes send a 16-bit code (0x23 low byte first, then the
 * mode byte, each with odd parity in bit 7): 1150 ms in total.
 */

#include "vis.h"

int vis_build_tones(unsigned short code, int nbits, VisTone *out) {
    int n = 0;
    out[n++] = VisTone{1900.0, 300.0};
    out[n++] = VisTone{1200.0, 10.0};
    out[n++] = VisTone{1900.0, 300.0};
    out[n++] = VisTone{1200.0, 30.0};
    for (int i = 0; i < nbits; i++) {
        out[n++] = VisTone{(code & 0x0001) ? 1100.0 : 1300.0, 30.0};
        code = (unsigned short)(code >> 1);
    }
    out[n++] = VisTone{1200.0, 30.0};
    return n;
}

double vis_duration_ms(int nbits) {
    return 300.0 + 10.0 + 300.0 + 30.0 + 30.0 * nbits + 30.0;
}

VISEncoder::VISEncoder()
    : tone_count(0), tone_index(0), sample_freq(48000.0),
      samples_remaining(0), fraction(0.0), nbits(8) {}

void VISEncoder::begin(unsigned short code, int bits, double samplerate) {
    nbits = bits;
    sample_freq = samplerate;
    tone_count = vis_build_tones(code, bits, tones);
    tone_index = -1;
    samples_remaining = 0;
    fraction = 0.0;
}

void VISEncoder::start(unsigned char code, double samplerate) {
    begin(code, 8, samplerate);
}

void VISEncoder::start_16bit(unsigned short code, double samplerate) {
    begin(code, 16, samplerate);
}

double VISEncoder::get_frequency() {
    while (samples_remaining <= 0) {
        if (tone_index + 1 >= tone_count) {
            tone_index = tone_count;
            return 0.0;  /* VIS complete */
        }
        tone_index++;
        double exact = tones[tone_index].ms * sample_freq / 1000.0 + fraction;
        samples_remaining = (long)exact;
        fraction = exact - (double)samples_remaining;
    }
    samples_remaining--;
    return tones[tone_index].freq_hz;
}

bool VISEncoder::is_complete() const {
    return tone_index >= tone_count;
}

int VISEncoder::get_total_samples() const {
    return (int)(vis_duration_ms(nbits) * sample_freq / 1000.0);
}
