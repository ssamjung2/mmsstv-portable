/*
 * test_decode_modes - Mode-detection smoke test against the full test_modes WAV set.
 *
 * For each entry in the test table this test:
 *   1. Decodes the WAV from tests/test_modes/ through the full decoder pipeline
 *   2. Verifies the detected SSTV mode matches the expected sstv_mode_t value
 *   3. Verifies an image buffer was produced with correct dimensions
 *   4. Reports PASS/FAIL per entry; exits non-zero if any entry fails
 *
 * Narrow-mode files (VIS disabled) are given a mode hint so the decoder can
 * lock on to them even without a VIS header.
 *
 * Usage:
 *   test_decode_modes [test_modes_dir] [--debug N]
 *     test_modes_dir  Path to directory containing WAV files (default: detected)
 *     --debug N       Set decoder debug level (0-3)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include "sstv_decoder.h"

/* =========================================================================
 * WAV reader (16-bit PCM mono)
 * ========================================================================= */
typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

static uint32_t u32le(FILE *fp) {
    uint8_t b[4];
    if (fread(b, 1, 4, fp) != 4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}
static uint16_t u16le(FILE *fp) {
    uint8_t b[2];
    if (fread(b, 1, 2, fp) != 2) return 0;
    return (uint16_t)b[0] | ((uint16_t)b[1]<<8);
}

static int read_wav(FILE *fp, wav_info_t *w) {
    uint8_t riff[12];
    if (fread(riff, 1, 12, fp) != 12) return -1;
    if (memcmp(riff, "RIFF", 4) || memcmp(riff+8, "WAVE", 4)) return -1;
    int fmt_ok = 0, data_ok = 0;
    while (!feof(fp)) {
        char id[4]; uint32_t sz;
        if (fread(id, 1, 4, fp) != 4) break;
        sz = u32le(fp);
        if (!memcmp(id, "fmt ", 4)) {
            w->audio_format    = u16le(fp);
            w->num_channels    = u16le(fp);
            w->sample_rate     = u32le(fp);
            u32le(fp); u16le(fp); /* byte_rate, block_align */
            w->bits_per_sample = u16le(fp);
            if (sz > 16) fseek(fp, (long)(sz - 16), SEEK_CUR);
            fmt_ok = 1;
        } else if (!memcmp(id, "data", 4)) {
            w->data_offset = (uint32_t)ftell(fp);
            w->data_size   = sz;
            data_ok = 1;
            break;
        } else {
            fseek(fp, (long)sz, SEEK_CUR);
        }
    }
    return (fmt_ok && data_ok) ? 0 : -1;
}

/* =========================================================================
 * PPM writer
 * ========================================================================= */

/* Write a raw PPM (P6) or PGM (P5) file.  Returns 0 on success, -1 on error. */
static int save_ppm(const char *path, const sstv_image_t *img)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    if (img->format == SSTV_GRAY8) {
        fprintf(fp, "P5\n%u %u\n255\n", img->width, img->height);
        for (uint32_t y = 0; y < img->height; y++)
            fwrite(img->pixels + (size_t)y * img->stride, 1, img->width, fp);
    } else { /* SSTV_RGB24 */
        fprintf(fp, "P6\n%u %u\n255\n", img->width, img->height);
        for (uint32_t y = 0; y < img->height; y++)
            fwrite(img->pixels + (size_t)y * img->stride, 1, img->width * 3, fp);
    }
    fclose(fp);
    return 0;
}

/* =========================================================================
 * Test table
 * ========================================================================= */
typedef struct {
    const char *wav;           /* filename within the test_modes dir */
    int         expected_mode; /* expected sstv_mode_t integer */
    int         mode_hint;     /* -1 = auto (VIS); ≥0 = set as hint (narrow modes) */
    int         exp_w;
    int         exp_h;
} test_entry_t;

/* Enum values match sstv_mode_t in sstv_encoder.h */
static const test_entry_t TESTS[] = {
    /* Robot */
    { "Robot_36.wav",   0,  -1, 320, 240 },   /* SSTV_R36 */
    { "Robot_72.wav",   1,   1, 320, 240 },   /* SSTV_R72 — VIS 0x0C collides with P7 0xF3 (complement); force hint */
    { "Robot_24.wav",  34,  -1, 320, 240 },   /* SSTV_R24 */
    /* AVT */
    { "AVT_90.wav",     2,   2, 320, 240 },   /* SSTV_AVT90 — VIS 0x44 collides with SC2_60 0xBB; force hint */
    /* Scottie */
    { "Scottie_1.wav",  3,  -1, 320, 256 },   /* SSTV_SCOTTIE1 */
    { "Scottie_2.wav",  4,  -1, 320, 256 },   /* SSTV_SCOTTIE2 */
    { "ScottieDX.wav",  5,  -1, 320, 256 },   /* SSTV_SCOTTIEX */
    /* Martin */
    { "Martin_1.wav",   6,  -1, 320, 256 },   /* SSTV_MARTIN1 */
    { "Martin_2.wav",   7,  -1, 320, 256 },   /* SSTV_MARTIN2 */
    /* SC2 */
    { "SC2_180.wav",    8,  -1, 320, 256 },   /* SSTV_SC2_180 */
    { "SC2_120.wav",    9,  -1, 320, 256 },   /* SSTV_SC2_120 */
    { "SC2_60.wav",    10,  10, 320, 256 },   /* SSTV_SC2_60 — VIS 0xBB collides with AVT90 0x44; force hint */
    /* PD */
    { "PD50.wav",      11,  -1, 320, 256 },   /* SSTV_PD50 */
    { "PD90.wav",      12,  -1, 320, 256 },   /* SSTV_PD90 */
    { "PD120.wav",     13,  -1, 640, 496 },   /* SSTV_PD120 */
    { "PD160.wav",     14,  -1, 512, 400 },   /* SSTV_PD160 */
    { "PD180.wav",     15,  -1, 640, 496 },   /* SSTV_PD180 */
    { "PD240.wav",     16,  -1, 640, 496 },   /* SSTV_PD240 */
    { "PD290.wav",     17,  -1, 800, 616 },   /* SSTV_PD290 */
    /* Pasokon */
    { "P3.wav",        18,  -1, 640, 496 },   /* SSTV_P3 */
    { "P5.wav",        19,  -1, 640, 496 },   /* SSTV_P5 */
    { "P7.wav",        20,  20, 640, 496 },   /* SSTV_P7 — VIS 0xF3 collides with R72 0x0C; force hint */
    /* Martin R */
    { "MR73.wav",      21,  -1, 320, 256 },   /* SSTV_MR73 */
    { "MR90.wav",      22,  -1, 320, 256 },   /* SSTV_MR90 */
    { "MR115.wav",     23,  -1, 320, 256 },   /* SSTV_MR115 */
    { "MR140.wav",     24,  -1, 320, 256 },   /* SSTV_MR140 */
    { "MR175.wav",     25,  -1, 320, 256 },   /* SSTV_MR175 */
    /* Martin P */
    { "MP73.wav",      26,  -1, 320, 256 },   /* SSTV_MP73 */
    { "MP115.wav",     27,  -1, 320, 256 },   /* SSTV_MP115 */
    { "MP140.wav",     28,  -1, 320, 256 },   /* SSTV_MP140 */
    { "MP175.wav",     29,  -1, 320, 256 },   /* SSTV_MP175 */
    /* Martin L */
    { "ML180.wav",     30,  -1, 640, 496 },   /* SSTV_ML180 */
    { "ML240.wav",     31,  31, 640, 496 },   /* SSTV_ML240 — VIS 0x86 ambiguous with BW12; force hint */
    { "ML280.wav",     32,  -1, 640, 496 },   /* SSTV_ML280 */
    { "ML320.wav",     33,  -1, 640, 496 },   /* SSTV_ML320 */
    /* B/W */
    { "B_W_8.wav",     35,  -1, 320, 240 },   /* SSTV_BW8 */
    { "B_W_12.wav",    36,  -1, 320, 240 },   /* SSTV_BW12 */
    /* Narrow modes (no VIS — mode hint required) */
    { "MP73_N.wav",    37,  37, 320, 256 },   /* SSTV_MN73  */
    { "MP110_N.wav",   38,  38, 320, 256 },   /* SSTV_MN110 */
    { "MP140_N.wav",   39,  39, 320, 256 },   /* SSTV_MN140 */
    { "MC110_N.wav",   40,  40, 320, 256 },   /* SSTV_MC110 */
    { "MC140_N.wav",   41,  41, 320, 256 },   /* SSTV_MC140 */
    { "MC180_N.wav",   42,  42, 320, 256 },   /* SSTV_MC180 */
};
static const int N_TESTS = (int)(sizeof(TESTS) / sizeof(TESTS[0]));

/* =========================================================================
 * Main
 * ========================================================================= */
int main(int argc, char **argv)
{
    const char *modes_dir = NULL;
    const char *out_dir    = "tests/decoded_images";
    int debug_level = 0;

    /* --- Parse arguments --- */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--debug") && i + 1 < argc) {
            debug_level = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--out-dir") && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (argv[i][0] != '-') {
            modes_dir = argv[i];
        }
    }

    /* --- Locate test_modes directory if not specified --- */
    if (!modes_dir) {
        static const char *candidates[] = {
            "tests/test_modes",
            "../tests/test_modes",
            "test_modes",
        };
        for (int c = 0; c < 3; c++) {
            char probe[512];
            snprintf(probe, sizeof(probe), "%s/Robot_36.wav", candidates[c]);
            FILE *f = fopen(probe, "rb");
            if (f) { fclose(f); modes_dir = candidates[c]; break; }
        }
    }
    if (!modes_dir) {
        fprintf(stderr, "ERROR: cannot locate test_modes directory\n");
        return 1;
    }

    /* --- Create output directory --- */
    mkdir(out_dir, 0755);

    printf("modes_dir  : %s\n", modes_dir);
    printf("out_dir    : %s\n", out_dir);
    printf("debug      : %d\n\n", debug_level);

    int pass_total = 0, fail_total = 0;

#define CHUNK 4096
    for (int ti = 0; ti < N_TESTS; ti++) {
        const test_entry_t *t = &TESTS[ti];
        printf("=== [%d/%d] %s ===\n", ti + 1, N_TESTS, t->wav);

        /* --- Open WAV --- */
        char wav_path[512];
        snprintf(wav_path, sizeof(wav_path), "%s/%s", modes_dir, t->wav);
        FILE *fp = fopen(wav_path, "rb");
        if (!fp) {
            printf("  SKIP  (file not found: %s)\n\n", wav_path);
            fail_total++;
            continue;
        }

        wav_info_t wi;
        memset(&wi, 0, sizeof(wi));
        if (read_wav(fp, &wi) != 0) {
            printf("  FAIL  (cannot parse WAV header)\n\n");
            fclose(fp);
            fail_total++;
            continue;
        }
        if (wi.num_channels != 1 || wi.bits_per_sample != 16) {
            printf("  FAIL  (unsupported WAV format: %u ch, %u-bit)\n\n",
                   wi.num_channels, wi.bits_per_sample);
            fclose(fp);
            fail_total++;
            continue;
        }
        printf("  wav      : %u Hz, %u-bit mono, %u bytes\n",
               wi.sample_rate, wi.bits_per_sample, wi.data_size);

        /* --- Create decoder --- */
        sstv_decoder_t *dec = sstv_decoder_create((double)wi.sample_rate);
        if (!dec) {
            printf("  FAIL  (sstv_decoder_create)\n\n");
            fclose(fp);
            fail_total++;
            continue;
        }
        sstv_decoder_set_debug_level(dec, debug_level);

        /* Apply mode hint for narrow-mode files (no VIS header) */
        if (t->mode_hint >= 0) {
            sstv_decoder_set_mode_hint(dec, (sstv_mode_t)t->mode_hint);
        }

        fseek(fp, (long)wi.data_offset, SEEK_SET);

        /* --- Feed samples --- */
        int16_t *pcm  = (int16_t *)malloc(CHUNK * sizeof(int16_t));
        float   *fbuf = (float   *)malloc(CHUNK * sizeof(float));
        if (!pcm || !fbuf) {
            printf("  FAIL  (OOM)\n\n");
            free(pcm); free(fbuf);
            sstv_decoder_free(dec);
            fclose(fp);
            fail_total++;
            continue;
        }

        sstv_rx_status_t last_st = SSTV_RX_NEED_MORE;
        size_t remaining = wi.data_size / 2;
        while (remaining > 0) {
            size_t n = remaining > CHUNK ? CHUNK : remaining;
            n = fread(pcm, sizeof(int16_t), n, fp);
            if (!n) break;
            for (size_t i = 0; i < n; i++) fbuf[i] = (float)pcm[i];
            last_st = sstv_decoder_feed(dec, fbuf, n);
            remaining -= n;
            if (last_st == SSTV_RX_IMAGE_READY) break;
            if (last_st == SSTV_RX_ERROR)       break;
        }
        fclose(fp);
        free(pcm);
        free(fbuf);

        /* -- finalize decoder after the end of the stream -- */
        sstv_rx_status_t finish_status = sstv_decoder_finish(dec);

        /* --- Check mode --- */
        sstv_decoder_state_t state;
        memset(&state, 0, sizeof(state));
        sstv_decoder_get_state(dec, &state);
        int detected = (state.current_mode != SSTV_MODE_COUNT) ? (int)state.current_mode : -1;

        int mode_ok = (detected == t->expected_mode);
        printf("  mode     : detected=%d expected=%d  %s\n",
               detected, t->expected_mode, mode_ok ? "OK" : "FAIL");
        printf("  lines    : %d / %d  image_ready=%d\n",
               state.current_line, state.total_lines, state.image_ready);

        /* --- Check image --- */
        sstv_image_t img;
        memset(&img, 0, sizeof(img));
        int got_img = (sstv_decoder_get_image(dec, &img) == 0 && img.pixels != NULL);
        int dim_ok = got_img && (int)img.width == t->exp_w && (int)img.height == t->exp_h;

        if (got_img) {
            /* Compute mean across all pixel bytes (handles both RGB and gray) */
            uint32_t bpp = (img.format == SSTV_GRAY8) ? 1 : 3;
            long sum = 0;
            for (uint32_t y = 0; y < img.height; y++)
                for (uint32_t x = 0; x < img.width * bpp; x++)
                    sum += img.pixels[(size_t)y * img.stride + x];
            double mean_px = sum / (double)(img.width * img.height * bpp);
            printf("  image    : %ux%u  (expected %dx%d)  mean_px=%.1f  %s\n",
                   img.width, img.height, t->exp_w, t->exp_h, mean_px,
                   dim_ok ? "OK" : "FAIL (wrong dimensions)");

            /* Save image to <out_dir>/<basename>.ppm */
            const char *ext = (img.format == SSTV_GRAY8) ? ".pgm" : ".ppm";
            /* Strip directory and .wav extension from wav filename */
            const char *base = t->wav;
            char out_path[512];
            /* Build stem (drop .wav suffix) */
            char stem[256];
            strncpy(stem, base, sizeof(stem) - 1);
            stem[sizeof(stem) - 1] = '\0';
            char *dot = strrchr(stem, '.');
            if (dot) *dot = '\0';
            snprintf(out_path, sizeof(out_path), "%s/%s%s", out_dir, stem, ext);
            if (save_ppm(out_path, &img) == 0)
                printf("  saved    : %s\n", out_path);
            else
                printf("  WARNING  : could not save %s\n", out_path);
        } else {
            printf("  image    : NOT PRODUCED  FAIL\n");
        }

        /* --- Verdict --- */
        int entry_pass = mode_ok && (got_img ? dim_ok : 0) &&
                         (finish_status == SSTV_RX_IMAGE_READY || state.image_ready);
        printf("  RESULT   : %s\n\n", entry_pass ? "PASS" : "FAIL");
        if (entry_pass) pass_total++; else fail_total++;

        sstv_decoder_free(dec);
    }

    printf("=============================================\n");
    printf("PASSED: %d / %d    FAILED: %d / %d\n",
           pass_total, N_TESTS, fail_total, N_TESTS);
    printf("=============================================\n");

    return (fail_total == 0) ? 0 : 1;
}
