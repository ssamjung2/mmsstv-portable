/*
 * test_vis_decode_wav - VIS decode test using external WAV input
 *
 * Usage:
 *   test_vis_decode_wav <input.wav> <expected_mode_int> [--debug N] [--tone-offset HZ]
 *
 * WAV requirements: 16-bit PCM, mono.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "sstv_decoder.h"

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

static uint32_t read_u32_le(FILE *fp) {
    uint8_t b[4];
    if (fread(b, 1, 4, fp) != 4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint16_t read_u16_le(FILE *fp) {
    uint8_t b[2];
    if (fread(b, 1, 2, fp) != 2) return 0;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static int read_wav_header(FILE *fp, wav_info_t *info) {
    uint8_t riff[12];
    if (fread(riff, 1, sizeof(riff), fp) != sizeof(riff)) return -1;
    if (memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) return -1;

    int fmt_found = 0;
    int data_found = 0;

    while (!feof(fp)) {
        char chunk_id[4];
        uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, fp) != 4) break;
        chunk_size = read_u32_le(fp);
        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            info->audio_format = read_u16_le(fp);
            info->num_channels = read_u16_le(fp);
            info->sample_rate = read_u32_le(fp);
            (void)read_u32_le(fp); /* byte rate */
            (void)read_u16_le(fp); /* block align */
            info->bits_per_sample = read_u16_le(fp);

            /* Skip any extra fmt bytes */
            if (chunk_size > 16) {
                fseek(fp, (long)(chunk_size - 16), SEEK_CUR);
            }
            fmt_found = 1;
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            info->data_offset = (uint32_t)ftell(fp);
            info->data_size = chunk_size;
            fseek(fp, (long)chunk_size, SEEK_CUR);
            data_found = 1;
        } else {
            fseek(fp, (long)chunk_size, SEEK_CUR);
        }

        if (fmt_found && data_found) {
            break;
        }
    }

    return (fmt_found && data_found) ? 0 : -1;
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <input.wav> <expected_mode_int> [--debug N]\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    int expected_mode = atoi(argv[2]);
    int debug_level = 0;
    double tone_offset = 0.0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 && i + 1 < argc) {
            debug_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--tone-offset") == 0 && i + 1 < argc) {
            tone_offset = atof(argv[++i]);
        }
    }

    FILE *fp = fopen(input_path, "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    wav_info_t info;
    memset(&info, 0, sizeof(info));
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
    sstv_decoder_set_debug_level(dec, debug_level);
    if (tone_offset != 0.0) {
        sstv_decoder_set_vis_tones(dec, 1100.0 + tone_offset, 1300.0 + tone_offset);
    }

    if (fseek(fp, (long)info.data_offset, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to seek to WAV data.\n");
        sstv_decoder_free(dec);
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

    size_t remaining = info.data_size / sizeof(int16_t);
    size_t total = 0;
    while (remaining > 0) {
        size_t to_read = remaining > frame_samples ? frame_samples : remaining;
        size_t n = fread(pcm, sizeof(int16_t), to_read, fp);
        if (n == 0) break;
        for (size_t i = 0; i < n; i++) {
            samples[i] = (float)pcm[i];
        }
        sstv_rx_status_t st = sstv_decoder_feed(dec, samples, n);
        if (st == SSTV_RX_ERROR) {
            fprintf(stderr, "Decoder error.\n");
            break;
        }
        total += n;
        remaining -= n;
    }

    sstv_decoder_state_t state;
    if (sstv_decoder_get_state(dec, &state) != 0) {
        fprintf(stderr, "Failed to get decoder state.\n");
        free(pcm);
        free(samples);
        sstv_decoder_free(dec);
        fclose(fp);
        return 1;
    }

    /* Use current_mode if VIS was detected; report mode 0 if not */
    int decoded_mode = (state.current_mode != SSTV_MODE_COUNT) ? (int)state.current_mode : 0;
    printf("Decoded mode=%d (expected=%d)\n", decoded_mode, expected_mode);
    printf("Processed %zu samples at %u Hz.\n", total, info.sample_rate);

    free(pcm);
    free(samples);
    sstv_decoder_free(dec);
    fclose(fp);

    return (decoded_mode == expected_mode) ? 0 : 1;
}