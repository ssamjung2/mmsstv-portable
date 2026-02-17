/*
 * decode_wav - SSTV RX CLI (scaffold)
 *
 * Reads 16-bit PCM mono WAV and feeds samples into decoder.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sstv_decoder.h"

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

static int read_wav_header(FILE *fp, wav_info_t *info) {
    uint8_t header[44];
    if (fread(header, 1, sizeof(header), fp) != sizeof(header)) {
        return -1;
    }
    if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
        return -1;
    }
    if (memcmp(header + 12, "fmt ", 4) != 0) {
        return -1;
    }

    info->audio_format = *(uint16_t *)(header + 20);
    info->num_channels = *(uint16_t *)(header + 22);
    info->sample_rate = *(uint32_t *)(header + 24);
    info->bits_per_sample = *(uint16_t *)(header + 34);

    if (memcmp(header + 36, "data", 4) != 0) {
        return -1;
    }

    info->data_size = *(uint32_t *)(header + 40);
    info->data_offset = 44;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.wav>\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    FILE *fp = fopen(input_path, "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    wav_info_t info;
    if (read_wav_header(fp, &info) != 0) {
        fprintf(stderr, "Unsupported or invalid WAV file.\n");
        fclose(fp);
        return 1;
    }

    if (info.audio_format != 1 || info.num_channels != 1 || info.bits_per_sample != 16) {
        fprintf(stderr, "Only 16-bit PCM mono WAV is supported.\n");
        fclose(fp);
        return 1;
    }

    sstv_decoder_t *dec = sstv_decoder_create((double)info.sample_rate);
    if (!dec) {
        fprintf(stderr, "Failed to create decoder.\n");
        fclose(fp);
        return 1;
    }

    const size_t frame_samples = 2048;
    int16_t *pcm = (int16_t *)malloc(frame_samples * sizeof(int16_t));
    float *samples = (float *)malloc(frame_samples * sizeof(float));
    if (!pcm || !samples) {
        fprintf(stderr, "Out of memory.\n");
        free(pcm);
        free(samples);
        sstv_decoder_free(dec);
        fclose(fp);
        return 1;
    }

    size_t total = 0;
    while (!feof(fp)) {
        size_t n = fread(pcm, sizeof(int16_t), frame_samples, fp);
        if (n == 0) break;
        for (size_t i = 0; i < n; i++) {
            samples[i] = (float)pcm[i];
        }
        sstv_rx_status_t st = sstv_decoder_feed(dec, samples, n);
        if (st == SSTV_RX_IMAGE_READY) {
            fprintf(stdout, "Image ready (not yet implemented).\n");
            break;
        }
        if (st == SSTV_RX_ERROR) {
            fprintf(stderr, "Decoder error.\n");
            break;
        }
        total += n;
    }

    fprintf(stdout, "Processed %zu samples at %u Hz.\n", total, info.sample_rate);

    free(pcm);
    free(samples);
    sstv_decoder_free(dec);
    fclose(fp);
    return 0;
}
