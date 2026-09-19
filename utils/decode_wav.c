/*
 * decode_wav - SSTV RX CLI (scaffold)
 *
 * Reads 16-bit PCM mono WAV and feeds samples into decoder.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

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

static void write_ppm(const char *path, const sstv_image_t *image) {
    FILE *out = fopen(path, "wb");
    if (!out) {
        fprintf(stderr, "Failed to open %s for writing: %s\n", path, strerror(errno));
        return;
    }

    fprintf(out, "P6\n%u %u\n255\n", image->width, image->height);
    fwrite(image->pixels, 1, (size_t)image->width * image->height * 3, out);
    fclose(out);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.wav> [output.ppm]\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = (argc >= 3) ? argv[2] : "decoded_output.ppm";
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
    
    /* Enable debug output */
    sstv_decoder_set_debug_level(dec, 2);
    sstv_decoder_set_vis_enabled(dec, 1);

    const size_t frame_samples = 2048;
    int image_ready = 0;
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
    sstv_rx_status_t st = SSTV_RX_NEED_MORE;
    while (!feof(fp)) {
        size_t n = fread(pcm, sizeof(int16_t), frame_samples, fp);
        if (n == 0) break;
        for (size_t i = 0; i < n; i++) {
            samples[i] = (float)pcm[i];
        }
        st = sstv_decoder_feed(dec, samples, n);
        total += n;
        if (st == SSTV_RX_IMAGE_READY || st == SSTV_RX_ERROR) break;
    }
    if (st == SSTV_RX_ERROR) {
        fprintf(stderr, "Decoder error.\n");
    } else {
        /* End of input: flush a partially received last line, if any.
         * finish() must only be called once the stream has ended. */
        if (st != SSTV_RX_IMAGE_READY) st = sstv_decoder_finish(dec);
        if (st == SSTV_RX_IMAGE_READY) {
            image_ready = 1;
            fprintf(stdout, "Image ready! Retrieving...\n");

            /* Get the decoded image */
            sstv_image_t image;
            if (sstv_decoder_get_image(dec, &image) == 0) {
                write_ppm(output_path, &image);
                fprintf(stdout, "Image saved to %s (%ux%u)\n",
                        output_path, image.width, image.height);
            } else {
                fprintf(stderr, "Failed to retrieve image\n");
            }
        }
    }

    if (!image_ready) {
        fprintf(stdout, "No image was produced from this input.\n");
    }
    fprintf(stdout, "Processed %zu samples at %u Hz.\n", total, info.sample_rate);

    free(pcm);
    free(samples);
    sstv_decoder_free(dec);
    fclose(fp);
    return 0;
}
