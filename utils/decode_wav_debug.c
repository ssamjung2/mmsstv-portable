/*
 * decode_wav_debug - SSTV Decoder with intermediate WAV output
 *
 * This utility demonstrates the debug WAV feature, allowing you to:
 * - Listen to audio at different processing stages
 * - Compare before/after BPF filtering
 * - Verify AGC normalization
 * - Analyze final signal going to tone detectors
 *
 * Usage:
 *   decode_wav_debug input.wav [output_prefix]
 *
 * Output files:
 *   [prefix]_before.wav   - After LPF, before BPF (raw input)
 *   [prefix]_bpf.wav      - After BPF (band-limited)
 *   [prefix]_agc.wav      - After AGC (normalized)
 *   [prefix]_final.wav    - After scaling (detector input)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sstv_decoder.h"

/* WAV file header */
typedef struct {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
} __attribute__((packed)) wav_header_t;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s input.wav [output_prefix]\n", argv[0]);
        fprintf(stderr, "\nCreates intermediate WAV files:\n");
        fprintf(stderr, "  [prefix]_before.wav  - After LPF, before BPF\n");
        fprintf(stderr, "  [prefix]_bpf.wav     - After bandpass filter\n");
        fprintf(stderr, "  [prefix]_agc.wav     - After AGC\n");
        fprintf(stderr, "  [prefix]_final.wav   - Final signal (detector input)\n");
        fprintf(stderr, "\nDefault prefix: 'debug'\n");
        return 1;
    }

    const char *input_path = argv[1];
    const char *prefix = (argc >= 3) ? argv[2] : "debug";

    /* Open input WAV file */
    FILE *in = fopen(input_path, "rb");
    if (!in) {
        fprintf(stderr, "Error: Cannot open input file: %s\n", input_path);
        return 1;
    }

    /* Read WAV header */
    wav_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, in) != 1) {
        fprintf(stderr, "Error: Cannot read WAV header\n");
        fclose(in);
        return 1;
    }

    /* Validate WAV format */
    if (memcmp(hdr.riff, "RIFF", 4) != 0 || memcmp(hdr.wave, "WAVE", 4) != 0) {
        fprintf(stderr, "Error: Not a valid WAV file\n");
        fclose(in);
        return 1;
    }

    if (hdr.format != 1) {
        fprintf(stderr, "Error: Only PCM WAV files supported\n");
        fclose(in);
        return 1;
    }

    if (hdr.channels != 1 && hdr.channels != 2) {
        fprintf(stderr, "Error: Only mono or stereo WAV files supported\n");
        fclose(in);
        return 1;
    }

    if (hdr.bits_per_sample != 16) {
        fprintf(stderr, "Error: Only 16-bit WAV files supported\n");
        fclose(in);
        return 1;
    }

    printf("Input WAV file: %s\n", input_path);
    printf("  Sample rate: %u Hz\n", hdr.sample_rate);
    printf("  Channels: %u\n", hdr.channels);
    printf("  Bits per sample: %u\n", hdr.bits_per_sample);
    printf("  Duration: %.2f seconds\n", 
           (double)hdr.data_size / (hdr.channels * sizeof(int16_t)) / hdr.sample_rate);

    /* Create decoder */
    sstv_decoder_t *dec = sstv_decoder_create(hdr.sample_rate);
    if (!dec) {
        fprintf(stderr, "Error: Cannot create decoder\n");
        fclose(in);
        return 1;
    }

    /* Build output filenames */
    char before_path[512], bpf_path[512], agc_path[512], final_path[512];
    snprintf(before_path, sizeof(before_path), "%s_before.wav", prefix);
    snprintf(bpf_path, sizeof(bpf_path), "%s_bpf.wav", prefix);
    snprintf(agc_path, sizeof(agc_path), "%s_agc.wav", prefix);
    snprintf(final_path, sizeof(final_path), "%s_final.wav", prefix);

    /* Enable debug WAV output */
    printf("\nEnabling debug WAV output:\n");
    printf("  Before filtering: %s\n", before_path);
    printf("  After BPF:        %s\n", bpf_path);
    printf("  After AGC:        %s\n", agc_path);
    printf("  Final signal:     %s\n", final_path);

    if (sstv_decoder_enable_debug_wav(dec, before_path, bpf_path, agc_path, final_path) != 0) {
        fprintf(stderr, "Error: Cannot enable debug WAV output\n");
        sstv_decoder_free(dec);
        fclose(in);
        return 1;
    }

    /* Enable verbose output */
    sstv_decoder_set_debug_level(dec, 2);
    sstv_decoder_set_vis_enabled(dec, 1);

    /* Process audio samples */
    printf("\nProcessing audio...\n");
    int16_t pcm[2048];
    size_t total_samples = 0;
    size_t frames_read;

    while ((frames_read = fread(pcm, hdr.channels * sizeof(int16_t), 2048, in)) > 0) {
        for (size_t i = 0; i < frames_read; i++) {
            /* For stereo, use left channel */
            int16_t sample = (hdr.channels == 2) ? pcm[i * 2] : pcm[i];
            sstv_decoder_feed_sample(dec, (float)sample);
            total_samples++;
        }

        /* Progress indicator */
        if (total_samples % (hdr.sample_rate * 5) == 0) {
            printf("  Processed %zu samples (%.1f seconds)...\n", 
                   total_samples, (double)total_samples / hdr.sample_rate);
        }
    }

    printf("Completed: %zu samples (%.2f seconds)\n", 
           total_samples, (double)total_samples / hdr.sample_rate);

    /* Cleanup */
    printf("\nFinalizing debug WAV files...\n");
    sstv_decoder_free(dec);  /* This closes and finalizes WAV headers */
    fclose(in);

    printf("\nDone! You can now:\n");
    printf("  1. Play the WAV files to hear the difference\n");
    printf("  2. Open in Audacity/SoX to view spectrograms\n");
    printf("  3. Compare frequency content before/after BPF\n");
    printf("  4. Verify AGC normalization levels\n");

    return 0;
}
