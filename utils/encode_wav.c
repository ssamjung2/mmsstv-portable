/*
 * Example: Encode a generated test pattern to WAV
 */

#include "sstv_encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void write_u32_le(FILE *f, unsigned int v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xff);
    b[1] = (unsigned char)((v >> 8) & 0xff);
    b[2] = (unsigned char)((v >> 16) & 0xff);
    b[3] = (unsigned char)((v >> 24) & 0xff);
    fwrite(b, 1, 4, f);
}

static void write_u16_le(FILE *f, unsigned short v) {
    unsigned char b[2];
    b[0] = (unsigned char)(v & 0xff);
    b[1] = (unsigned char)((v >> 8) & 0xff);
    fwrite(b, 1, 2, f);
}

static void write_wav_header(FILE *f, unsigned int sample_rate, unsigned int num_samples) {
    unsigned int data_bytes = num_samples * sizeof(short);
    unsigned int riff_size = 36 + data_bytes;

    fwrite("RIFF", 1, 4, f);
    write_u32_le(f, riff_size);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    write_u32_le(f, 16);               /* PCM chunk size */
    write_u16_le(f, 1);                /* PCM */
    write_u16_le(f, 1);                /* mono */
    write_u32_le(f, sample_rate);
    write_u32_le(f, sample_rate * 2);  /* byte rate */
    write_u16_le(f, 2);                /* block align */
    write_u16_le(f, 16);               /* bits per sample */

    fwrite("data", 1, 4, f);
    write_u32_le(f, data_bytes);
}

static void generate_color_bars(unsigned char *rgb, unsigned int width, unsigned int height) {
    static const unsigned char colors[8][3] = {
        {255, 255, 255},
        {255, 255,   0},
        {  0, 255, 255},
        {  0, 255,   0},
        {255,   0, 255},
        {255,   0,   0},
        {  0,   0, 255},
        {  0,   0,   0}
    };

    unsigned int bar_width = width / 8;
    if (bar_width == 0) bar_width = 1;

    for (unsigned int y = 0; y < height; y++) {
        for (unsigned int x = 0; x < width; x++) {
            unsigned int bar = x / bar_width;
            if (bar > 7) bar = 7;
            unsigned int idx = (y * width + x) * 3;
            rgb[idx + 0] = colors[bar][0];
            rgb[idx + 1] = colors[bar][1];
            rgb[idx + 2] = colors[bar][2];
        }
    }
}

int main(int argc, char **argv) {
    const char *out_path = NULL;
    const char *mode_name = "scottie 1";
    double sample_rate = 48000.0;

    if (argc < 2) {
        printf("Usage: %s output.wav [mode_name] [sample_rate]\n", argv[0]);
        return 1;
    }

    out_path = argv[1];
    if (argc >= 3) {
        mode_name = argv[2];
    }
    if (argc >= 4) {
        sample_rate = atof(argv[3]);
        if (sample_rate <= 0) sample_rate = 48000.0;
    }

    int mode = sstv_find_mode_by_name(mode_name);
    if (mode < 0) {
        printf("Unknown mode: %s\n", mode_name);
        return 1;
    }

    const sstv_mode_info_t *info = sstv_get_mode_info((sstv_mode_t)mode);
    if (!info) {
        printf("Mode info not found.\n");
        return 1;
    }

    unsigned int width = info->width;
    unsigned int height = info->height;

    unsigned char *rgb = (unsigned char *)malloc(width * height * 3);
    if (!rgb) {
        printf("Out of memory.\n");
        return 1;
    }

    generate_color_bars(rgb, width, height);

    sstv_image_t image = sstv_image_from_rgb(rgb, width, height);
    sstv_encoder_t *encoder = sstv_encoder_create((sstv_mode_t)mode, sample_rate);
    if (!encoder) {
        printf("Failed to create encoder.\n");
        free(rgb);
        return 1;
    }

    if (sstv_encoder_set_image(encoder, &image) != 0) {
        printf("Image size mismatch for mode %s.\n", info->name);
        sstv_encoder_free(encoder);
        free(rgb);
        return 1;
    }

    sstv_encoder_set_vis_enabled(encoder, 1);

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        printf("Failed to open output file.\n");
        sstv_encoder_free(encoder);
        free(rgb);
        return 1;
    }

    /* Placeholder header; will be rewritten with actual sample count */
    write_wav_header(out, (unsigned int)sample_rate, 0);

    float buffer[4096];
    short pcm[4096];

    size_t written_samples = 0;
    while (!sstv_encoder_is_complete(encoder)) {
        size_t generated = sstv_encoder_generate(encoder, buffer, 4096);
        if (generated == 0) {
            break;
        }
        for (size_t i = 0; i < generated; i++) {
            float v = buffer[i];
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            pcm[i] = (short)lrintf(v * 32767.0f);
        }
        fwrite(pcm, sizeof(short), generated, out);
        written_samples += generated;
    }

    /* Rewrite header with actual samples written */
    fseek(out, 0, SEEK_SET);
    write_wav_header(out, (unsigned int)sample_rate, (unsigned int)written_samples);

    fclose(out);
    sstv_encoder_free(encoder);
    free(rgb);

    printf("Wrote %s (%s, %.0f Hz).\n", out_path, info->name, sample_rate);
    return 0;
}
