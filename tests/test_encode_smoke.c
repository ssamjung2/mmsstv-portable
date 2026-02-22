/*
 * Smoke test: Encode a single mode and validate output length
 */

#include "sstv_encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    const sstv_mode_t mode = SSTV_SCOTTIE1;
    const sstv_mode_info_t *info = sstv_get_mode_info(mode);
    if (!info) {
        printf("Mode info missing.\n");
        return 1;
    }

    const unsigned int width = info->width;
    const unsigned int height = info->height;
    unsigned char *rgb = (unsigned char *)calloc(width * height * 3, 1);
    if (!rgb) {
        printf("Out of memory.\n");
        return 1;
    }

    /* Solid red test pattern */
    for (unsigned int i = 0; i < width * height; i++) {
        rgb[i * 3 + 0] = 255;
        rgb[i * 3 + 1] = 0;
        rgb[i * 3 + 2] = 0;
    }

    sstv_image_t image = sstv_image_from_rgb(rgb, width, height);
    sstv_encoder_t *encoder = sstv_encoder_create(mode, 48000.0);
    if (!encoder) {
        printf("Failed to create encoder.\n");
        free(rgb);
        return 1;
    }

    if (sstv_encoder_set_image(encoder, &image) != 0) {
        printf("Image size mismatch.\n");
        sstv_encoder_free(encoder);
        free(rgb);
        return 1;
    }

    sstv_encoder_set_vis_enabled(encoder, 1);

    size_t total_samples = sstv_encoder_get_total_samples(encoder);
    size_t generated_total = 0;
    float max_abs = 0.0f;

    float buffer[4096];
    while (!sstv_encoder_is_complete(encoder)) {
        size_t generated = sstv_encoder_generate(encoder, buffer, 4096);
        if (generated == 0) {
            break;
        }
        generated_total += generated;
        for (size_t i = 0; i < generated; i++) {
            float v = buffer[i];
            float a = fabsf(v);
            if (a > max_abs) max_abs = a;
        }
    }

    sstv_encoder_free(encoder);
    free(rgb);

    long long diff = (long long)generated_total - (long long)total_samples;
    if (diff < 0) diff = -diff;

    /* Allow for fractional-sample accumulation rounding across 256 lines plus
     * any VIS-duration estimate error.  1000 samples ≈ 20 ms at 48 kHz,
     * comfortably within one Scottie1 scan-line (144 ms). */
    if (diff > 1000) {
        printf("Sample count mismatch: expected %zu got %zu\n", total_samples, generated_total);
        return 1;
    }

    if (max_abs < 0.01f) {
        printf("Output appears silent.\n");
        return 1;
    }

    printf("Smoke test OK: %zu samples, max=%.3f\n", generated_total, max_abs);
    return 0;
}
