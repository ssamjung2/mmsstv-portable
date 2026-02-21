/**
 * Test VIS Code Decoding
 * 
 * Verifies the decoder can correctly identify SSTV modes from VIS codes.
 * 
 * VIS code structure (MMSSTV standard):
 *   - 1900 Hz leader tone (300ms)
 *   - Break (1200 Hz, 10ms)
 *   - VIS start bit (1200 Hz, 30ms)
 *   - 7 data bits (LSB first): 1080 Hz = 1, 1320 Hz = 0 (30ms each)
 *   - 1 parity bit (odd parity): 1080/1320 Hz (30ms)
 *   - Stop bit (1200 Hz, 30ms)
 * 
 * Total VIS sequence duration: ~600ms
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "sstv_decoder.h"
#include "sstv_encoder.h"

#define SAMPLE_RATE 48000
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    double *data;
    size_t count;
    size_t capacity;
} sample_buffer_t;

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

static int sample_buffer_reserve(sample_buffer_t *buf, size_t needed) {
    if (needed <= buf->capacity) return 0;
    size_t new_capacity = buf->capacity == 0 ? 4096 : buf->capacity;
    while (new_capacity < needed) new_capacity *= 2;
    double *new_data = (double *)realloc(buf->data, new_capacity * sizeof(double));
    if (!new_data) return -1;
    buf->data = new_data;
    buf->capacity = new_capacity;
    return 0;
}

static int sample_buffer_append_tone(sample_buffer_t *buf, double freq, double duration, double amplitude) {
    size_t num_samples = (size_t)(duration * SAMPLE_RATE);
    size_t start = buf->count;
    if (sample_buffer_reserve(buf, start + num_samples) != 0) return -1;
    for (size_t i = 0; i < num_samples; i++) {
        double t = (double)i / SAMPLE_RATE;
        buf->data[start + i] = amplitude * sin(2.0 * M_PI * freq * t);
    }
    buf->count += num_samples;
    return 0;
}

static int sample_buffer_append_silence(sample_buffer_t *buf, double duration) {
    size_t num_samples = (size_t)(duration * SAMPLE_RATE);
    size_t start = buf->count;
    if (sample_buffer_reserve(buf, start + num_samples) != 0) return -1;
    memset(buf->data + start, 0, num_samples * sizeof(double));
    buf->count += num_samples;
    return 0;
}

static int write_wav_file(const char *path, const double *samples, size_t count) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    uint32_t sample_rate = SAMPLE_RATE;
    uint16_t num_channels = 1;
    uint16_t bits_per_sample = 16;
    uint16_t block_align = (uint16_t)(num_channels * (bits_per_sample / 8));
    uint32_t byte_rate = sample_rate * block_align;
    uint32_t data_size = (uint32_t)(count * block_align);
    uint32_t riff_size = 36 + data_size;

    fwrite("RIFF", 1, 4, fp);
    fwrite(&riff_size, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1;
    fwrite(&fmt_size, 4, 1, fp);
    fwrite(&audio_format, 2, 1, fp);
    fwrite(&num_channels, 2, 1, fp);
    fwrite(&sample_rate, 4, 1, fp);
    fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits_per_sample, 2, 1, fp);
    fwrite("data", 1, 4, fp);
    fwrite(&data_size, 4, 1, fp);

    for (size_t i = 0; i < count; i++) {
        double s = samples[i];
        if (s > 1.0) s = 1.0;
        if (s < -1.0) s = -1.0;
        int16_t v = (int16_t)(s * 32767.0);
        fwrite(&v, sizeof(int16_t), 1, fp);
    }

    fclose(fp);
    return 0;
}

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

/* Build a VIS code sequence as samples:
 *   Leader (1900 Hz, 300ms) → Break (1200 Hz, 10ms) → Start bit (1200 Hz, 30ms)
 *   → 7 data bits (LSB first, 30ms each) → Parity (30ms) → Stop (1200 Hz, 30ms)
 */
static int build_vis_samples(sample_buffer_t *buf, uint8_t vis_code) {
    if (!buf) return -1;

    if (sample_buffer_append_tone(buf, 1900.0, 0.300, 0.8) != 0) return -1;
    if (sample_buffer_append_tone(buf, 1200.0, 0.010, 0.8) != 0) return -1;
    if (sample_buffer_append_tone(buf, 1200.0, 0.030, 0.8) != 0) return -1;

    /* 7 data bits (LSB first) - MMSSTV standard: 1080 Hz = 1, 1320 Hz = 0 */
    for (int bit = 0; bit < 7; bit++) {
        int bit_value = (vis_code >> bit) & 1;
        double freq = bit_value ? 1080.0 : 1320.0;
        if (sample_buffer_append_tone(buf, freq, 0.030, 0.8) != 0) return -1;
    }

    /* Parity bit (odd parity for MMSSTV) - parity bit = popcount of data bits */
    int parity = 0;
    for (int bit = 0; bit < 7; bit++) {
        if ((vis_code >> bit) & 1) parity ^= 1;
    }
    /* MMSSTV odd parity: parity bit equals the popcount & 1 of the data bits */
    if (sample_buffer_append_tone(buf, parity ? 1080.0 : 1320.0, 0.030, 0.8) != 0) return -1;

    /* Stop bit (1200 Hz) */
    if (sample_buffer_append_tone(buf, 1200.0, 0.030, 0.8) != 0) return -1;

    /* Extra silence for decoder to finish processing */
    if (sample_buffer_append_silence(buf, 0.180) != 0) return -1;

    return 0;
}

static int decode_wav_into_decoder(sstv_decoder_t *dec, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    wav_info_t info;
    if (read_wav_header(fp, &info) != 0) {
        fclose(fp);
        return -1;
    }

    if (info.audio_format != 1 || info.num_channels != 1 || info.bits_per_sample != 16) {
        fclose(fp);
        return -1;
    }

    const size_t frame_samples = 2048;
    int16_t *pcm = (int16_t *)malloc(frame_samples * sizeof(int16_t));
    if (!pcm) {
        fclose(fp);
        return -1;
    }

    while (!feof(fp)) {
        size_t n = fread(pcm, sizeof(int16_t), frame_samples, fp);
        if (n == 0) break;
        for (size_t i = 0; i < n; i++) {
            double s = (double)pcm[i];
            sstv_decoder_feed_sample(dec, (float)s);
        }
    }

    free(pcm);
    fclose(fp);
    return 0;
}

/* Test structure: VIS code → expected mode */
typedef struct {
    uint8_t vis_code;
    sstv_mode_t expected_mode;
    const char *mode_name;
} vis_test_case_t;

static const vis_test_case_t VIS_TEST_CASES[] = {
    { 0x88, SSTV_R36, "Robot 36" },
    { 0x0C, SSTV_R72, "Robot 72" },
    { 0x84, SSTV_R24, "Robot 24" },
    { 0x3C, SSTV_SCOTTIE1, "Scottie 1" },
    { 0xB8, SSTV_SCOTTIE2, "Scottie 2" },
    { 0xCC, SSTV_SCOTTIEX, "Scottie DX" },
    { 0xAC, SSTV_MARTIN1, "Martin 1" },
    { 0x28, SSTV_MARTIN2, "Martin 2" },
    { 0xDD, SSTV_PD50, "PD 50" },
    { 0x63, SSTV_PD90, "PD 90" },
    { 0x60, SSTV_PD180, "PD 180" },
};

#define NUM_TEST_CASES (sizeof(VIS_TEST_CASES) / sizeof(vis_test_case_t))

int main(void) {
    int total_tests = 0;
    int passed = 0;
    int failed = 0;
    
    printf("================================================================================\n");
    printf("              VIS CODE DECODING TESTS\n");
    printf("================================================================================\n\n");
    
    /* Test each VIS code */
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        const vis_test_case_t *test = &VIS_TEST_CASES[i];
        
        printf("TEST %zu: VIS 0x%02X (%s)\n", i + 1, test->vis_code, test->mode_name);
        
        /* Create decoder */
        sstv_decoder_t *dec = sstv_decoder_create(SAMPLE_RATE);
        if (!dec) {
            printf("  FAIL (could not create decoder)\n");
            failed++;
            total_tests++;
            continue;
        }
        
        /* Enable VIS decoding */
        sstv_decoder_set_debug_level(dec, 2);

        /* Build WAV for VIS sequence */
        sample_buffer_t buf = {0};
        if (build_vis_samples(&buf, test->vis_code) != 0) {
            printf("  FAIL (could not build VIS samples)\n");
            sstv_decoder_free(dec);
            failed++;
            total_tests++;
            continue;
        }

        char wav_path[] = "/tmp/vis_test_XXXXXX.wav";
        int fd = mkstemps(wav_path, 4);
        if (fd < 0) {
            printf("  FAIL (could not create temp WAV)\n");
            free(buf.data);
            sstv_decoder_free(dec);
            failed++;
            total_tests++;
            continue;
        }
        close(fd);

        if (write_wav_file(wav_path, buf.data, buf.count) != 0) {
            printf("  FAIL (could not write WAV)\n");
            unlink(wav_path);
            free(buf.data);
            sstv_decoder_free(dec);
            failed++;
            total_tests++;
            continue;
        }

        /* Decode using WAV input */
        if (decode_wav_into_decoder(dec, wav_path) != 0) {
            printf("  FAIL (could not decode WAV)\n");
            unlink(wav_path);
            free(buf.data);
            sstv_decoder_free(dec);
            failed++;
            total_tests++;
            continue;
        }

        unlink(wav_path);
        free(buf.data);
        
        /* Check decoder state */
        sstv_decoder_state_t state;
        if (sstv_decoder_get_state(dec, &state) < 0) {
            printf("  FAIL (could not get decoder state)\n");
            sstv_decoder_free(dec);
            failed++;
            total_tests++;
            continue;
        }
        
        /* Verify mode detected */
        if (state.current_mode == test->expected_mode) {
            printf("  PASS (mode=%d)\n", state.current_mode);
            passed++;
        } else {
            printf("  FAIL (expected mode=%d, got mode=%d)\n", 
                   test->expected_mode, state.current_mode);
            failed++;
        }
        
        sstv_decoder_free(dec);
        total_tests++;
        }

        printf("\n================================================================================\n");
        printf("RESULT: %d passed, %d failed\n", passed, failed);
        printf("================================================================================\n");

        return (failed > 0) ? 1 : 0;
    }
