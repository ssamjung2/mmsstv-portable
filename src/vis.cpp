/*
 * VIS (Vertical Interval Signaling) Code Encoder
 * Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
 * 
 * VIS code identifies the SSTV mode at the start of transmission
 * Standard 8-bit VIS:
 *   - Leader tone: 1900 Hz for 300ms
 *   - Break: 1200 Hz for 10ms  
 *   - Leader tone: 1900 Hz for 300ms
 *   - Start bit: 1200 Hz for 30ms
 *   - 8 data bits (LSB first): bit 0 = 1100 Hz, bit 1 = 1300 Hz (30ms each)
 *   - Stop bit: 1200 Hz for 30ms
 *   Total duration: 910ms
 *
 * 16-bit VIS (MR/MP/ML modes per SSTV Handbook):
 *   - Leader tone: 1900 Hz for 300ms
 *   - Break: 1200 Hz for 10ms  
 *   - Leader tone: 1900 Hz for 300ms
 *   - Start bit: 1200 Hz for 30ms
 *   - 8 data bits LSB byte (30ms each)
 *   - Odd parity bit (30ms)
 *   - 8 data bits MSB byte (30ms each)
 *   - Odd parity bit (30ms)
 *   - Stop bit: 1200 Hz for 30ms
 *   Total duration: 1210ms
 */

#include <cstddef>
#include <cstdio>

#include "vis.h"

VISEncoder::VISEncoder() : vis_code(0), state(0), sample_freq(48000.0), samples_remaining(0), is_16bit(false) {}

int VISEncoder::calculate_parity(unsigned char code) {
    int ones = 0;
    for (int i = 0; i < 8; i++) {
        if (code & (1 << i)) {
            ones++;
        }
    }
    // Odd parity: if we have an even number of 1s in the data,
    // set parity to 1 to make total odd. If odd number of 1s, set parity to 0.
    return (ones % 2) ? 0 : 1;  // Return parity bit value
}

void VISEncoder::start(unsigned char code, double samplerate) {
    vis_code = code;
    sample_freq = samplerate;
    state = 0;
    is_16bit = false;
    samples_remaining = (int)(0.300 * sample_freq);  // 300ms leader
}

void VISEncoder::start_16bit(unsigned short code, double samplerate) {
    vis_code = code;
    sample_freq = samplerate;
    state = 0;
    is_16bit = true;
    samples_remaining = (int)(0.300 * sample_freq);  // 300ms leader
}

double VISEncoder::get_frequency() {
    int final_state = is_16bit ? 21 : 13;
    
    static int call_count = 0;
    if (call_count < 5 || state >= 4) {
        fprintf(stderr, "[VIS ENC get_frequency] call=%d state=%d final=%d samples_remaining=%d\n",
                call_count++, state, final_state, samples_remaining);
    }
    
    if (state >= final_state) {
        return 0.0;  // VIS complete
    }

    if (samples_remaining > 0) {
        samples_remaining--;

        // Leader tone (state 0, 2)
        if (state == 0 || state == 2) {
            return 1900.0;
        }
        // Break (state 1)
        if (state == 1) {
            return 1200.0;
        }
        // Start bit
        if (state == 3) {
            return 1200.0;
        }
        // Stop bit
        if (state == (final_state - 1)) {
            return 1200.0;
        }
        
        // 8-bit VIS: states 4-11 are data bits
        // MMSSTV VIS: bit 1 = 1080 Hz (mark), bit 0 = 1320 Hz (space) per sstv.cpp:1988
        // LSB-first transmission (bit 0 transmitted first)
        if (!is_16bit && state >= 4 && state <= 11) {
            int bit_idx = state - 4;  // Transmit bit 0 first (LSB-first)
            int bit_val = (vis_code & (1 << bit_idx)) ? 1 : 0;
            double freq = bit_val ? 1080.0 : 1320.0;
            if (bit_idx < 8) {
                fprintf(stderr, "[VIS ENC SAMPLE] state=%d bit_idx=%d bit_val=%d freq=%.0f vis_code=0x%02x\n", 
                        state, bit_idx, bit_val, freq, vis_code);
            }
            return freq;
        }
        
        // 16-bit VIS: Two sequential 8-bit VIS codes (MMSSTV extended VIS)  
        // States 4-11: first VIS code (0x23 extended marker)
        // States 12-19: second VIS code (actual mode code)
        // State 12 is just the first bit of the second VIS - no stop bit!
        if (is_16bit) {
            if (state >= 4 && state <= 11) {
                // First VIS code (LSB byte): bits 0-7 where bit 7 is parity
                int bit_idx = state - 4;
                unsigned char lsb = vis_code & 0xFF;
                int bit_val = (lsb & (1 << bit_idx)) ? 1 : 0;
                double freq = bit_val ? 1080.0 : 1320.0;
                if (bit_idx < 8) {
                    fprintf(stderr, "[VIS ENC 16bit] state=%d bit=%d val=%d freq=%.0f lsb=0x%02X\n",
                            state, bit_idx, bit_val, freq, lsb);
                }
                return freq;
            }
            if (state >= 12 && state <= 19) {
                // Second VIS code (MSB byte): bits 0-7 where bit 7 is parity
                // No stop bit - transitions directly from first VIS to second VIS
                int bit_idx = state - 12;
                unsigned char msb = (vis_code >> 8) & 0xFF;
                int bit_val = (msb & (1 << bit_idx)) ? 1 : 0;
                double freq = bit_val ? 1080.0 : 1320.0;
                if (bit_idx < 8) {
                    fprintf(stderr, "[VIS ENC 16bit] state=%d bit=%d val=%d freq=%.0f msb=0x%02X\n",
                            state, bit_idx, bit_val, freq, msb);
                }
                return freq;
            }
            if (state == 20) {
                fprintf(stderr, "[VIS ENC 16bit] state=20 STOP BIT (1200 Hz)\n");
                // Final stop bit after both VIS codes
                return 1200.0;  // 30ms stop bit
            }
        }
        
        return 0.0;
    }

    state++;

    //fprintf(stderr, "[VIS_ENC] state=%d final_state=%d is_16bit=%d\n", state, final_state, is_16bit);

    if (state == 1) {
        samples_remaining = (int)(0.010 * sample_freq);
        return 1200.0;
    }
    if (state == 2) {
        samples_remaining = (int)(0.300 * sample_freq);
        return 1900.0;
    }
    if (state == 3) {
        samples_remaining = (int)(0.030 * sample_freq);
        return 1200.0;
    }
    
    // 8-bit VIS data bits (states 4-11)
    if (!is_16bit && state >= 4 && state <= 11) {
        samples_remaining = (int)(0.030 * sample_freq);
        int bit_idx = state - 4;
        int bit_val = (vis_code & (1 << bit_idx)) ? 1 : 0;
        double freq = bit_val ? 1080.0 : 1320.0;  /* MMSSTV: bit 1 = 1080 Hz */
        fprintf(stderr, "[VIS ENC TRANSITION] state=%d→next bit_idx=%d bit_val=%d freq=%.0f vis_code=0x%02x\n",
                state, bit_idx, bit_val, freq, vis_code);
        return freq;
    }
    
    // 16-bit VIS
    if (is_16bit) {
        if (state >= 4 && state <= 11) {
            // LSB byte
            samples_remaining = (int)(0.030 * sample_freq);
            int bit_idx = state - 4;
            return (vis_code & (1 << bit_idx)) ? 1080.0 : 1320.0;  /* MMSSTV */
        }
        if (state == 12) {
            // Parity for LSB
            samples_remaining = (int)(0.030 * sample_freq);
            int parity = calculate_parity(vis_code & 0xFF);
            return parity ? 1080.0 : 1320.0;  /* MMSSTV */
        }
        if (state >= 13 && state <= 20) {
            // MSB byte
            samples_remaining = (int)(0.030 * sample_freq);
            int bit_idx = state - 13;
            return ((vis_code >> 8) & (1 << bit_idx)) ? 1080.0 : 1320.0;  /* MMSSTV */
        }
        if (state == 21) {
            // Parity for MSB
            samples_remaining = (int)(0.030 * sample_freq);
            int parity = calculate_parity((vis_code >> 8) & 0xFF);
            return parity ? 1080.0 : 1320.0;  /* MMSSTV */
        }
    }
    
    // Stop bit
    if (state == (is_16bit ? 22 : 12)) {
        samples_remaining = (int)(0.030 * sample_freq);
        return 1200.0;
    }

    state = final_state;
    return 0.0;
}

bool VISEncoder::is_complete() const {
    return state >= (is_16bit ? 23 : 13);
}

int VISEncoder::get_total_samples() const {
    double duration = is_16bit ? 1.210 : 0.910;
    return (int)(duration * sample_freq);
}
