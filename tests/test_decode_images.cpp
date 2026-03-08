/*
 * test_decode_images - End-to-end decode test against real SSTV audio + reference images.
 *
 * For each entry in the test table this test:
 *   1. Decodes the WAV file through the full decoder pipeline
 *   2. Verifies the detected SSTV mode matches expectation
 *   3. Verifies an image buffer was produced with correct dimensions
 *   4. Loads the reference JPEG and computes per-channel mean absolute error (MAE)
 *   5. Reports PASS/FAIL per entry; exits non-zero if any entry fails
 *
 * Usage:
 *   test_decode_images [audio_dir] [--mae-limit N] [--debug N] [--mode-only]
 *     audio_dir   Path to directory containing WAV + JPEG files (default: detected)
 *     --mae-limit Maximum acceptable MAE [0-255], default 60
 *     --debug N   Set decoder debug level (0-3)
 *     --mode-only Only check mode detection, skip image quality
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "../external/stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

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
            w->audio_format   = u16le(fp);
            w->num_channels   = u16le(fp);
            w->sample_rate    = u32le(fp);
            u32le(fp); u16le(fp); /* byte_rate, block_align */
            w->bits_per_sample = u16le(fp);
            if (sz > 16) fseek(fp, (long)(sz-16), SEEK_CUR);
            fmt_ok = 1;
        } else if (!memcmp(id, "data", 4)) {
            w->data_offset = (uint32_t)ftell(fp);
            w->data_size = sz;
            data_ok = 1;
            break;
        } else {
            fseek(fp, (long)sz, SEEK_CUR);
        }
    }
    return (fmt_ok && data_ok) ? 0 : -1;
}

/* =========================================================================
 * Image comparison
 * ========================================================================= */

/* Scale ref_pixels (ref_w x ref_h) to (img_w x img_h) using nearest-neighbor,
 * then compute per-channel mean absolute error against img (RGB24). */
static double compute_mae(const uint8_t *img, int img_w, int img_h,
                           const uint8_t *ref, int ref_w, int ref_h,
                           int ref_ch)
{
    double sum = 0.0;
    long total = 0;
    for (int y = 0; y < img_h; y++) {
        int ry = (ref_h > 1) ? (y * ref_h / img_h) : 0;
        for (int x = 0; x < img_w; x++) {
            int rx = (ref_w > 1) ? (x * ref_w / img_w) : 0;
            for (int c = 0; c < 3; c++) {
                int iv = img[(y * img_w + x) * 3 + c];
                int rv;
                if (ref_ch == 1) {
                    /* grayscale reference → expand to RGB */
                    rv = ref[ry * ref_w + rx];
                } else {
                    rv = ref[(ry * ref_w + rx) * ref_ch + c];
                }
                sum += fabs((double)(iv - rv));
                total++;
            }
        }
    }
    return total > 0 ? sum / (double)total : 255.0;
}

/* =========================================================================
 * Test table
 * ========================================================================= */
typedef struct {
    const char *wav;      /* filename within audio_dir */
    const char *ref_jpg;  /* reference image filename (NULL = skip quality check) */
    int  expected_mode;   /* sstv_mode_t integer */
    int  exp_w, exp_h;    /* expected decoded image dimensions */
} test_entry_t;

static const test_entry_t TESTS[] = {
    /* Robot */
    { "alt5_test_panel_robot36.wav", "alt5_test_panel_robot36.jpg", 0,  320, 240 },
    { "alt5_test_panel_r72.wav",     "alt5_test_panel_r72.jpg",     1,  320, 240 },
    /* Scottie */
    { "alt5_test_panel_scottie1.wav","alt5_test_panel_scottie1.jpg",3,  320, 256 },
    { "alt5_test_panel_scottie2.wav","alt5_test_panel_scottie2.jpg",4,  320, 256 },
    { "alt5_test_panel_sdx.wav",     "alt5_test_panel_sdx.jpg",     5,  320, 256 },
    /* Martin */
    { "alt5_test_panel_martin1.wav", "alt5_test_panel_martin1.jpg", 6,  320, 256 },
    { "alt5_test_panel_martin2.wav", "alt5_test_panel_martin2.jpg", 7,  320, 256 },
    /* Martin R (extended VIS) */
    { "alt5_test_panel_mr73.wav",    "alt5_test_panel_mr73.jpg",    21, 320, 256 },
    { "alt5_test_panel_mr90.wav",    "alt5_test_panel_mr90.jpg",    22, 320, 256 },
    { "alt5_test_panel_mr115.wav",   "alt5_test_panel_mr115.jpg",   23, 320, 256 },
    { "alt5_test_panel_mr140.wav",   "alt5_test_panel_mr140.jpg",   24, 320, 256 },
    { "alt5_test_panel_mr175.wav",   "alt5_test_panel_mr175.jpg",   25, 320, 256 },
};
static const int N_TESTS = (int)(sizeof(TESTS)/sizeof(TESTS[0]));

/* =========================================================================
 * Main
 * ========================================================================= */
int main(int argc, char **argv)
{
    const char *audio_dir = NULL;
    double mae_limit = 60.0;
    int debug_level = 0;
    int mode_only = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--mae-limit") && i+1 < argc)
            mae_limit = atof(argv[++i]);
        else if (!strcmp(argv[i], "--debug") && i+1 < argc)
            debug_level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mode-only"))
            mode_only = 1;
        else if (argv[i][0] != '-')
            audio_dir = argv[i];
    }

    /* Default: look next to this binary, then relative to build dir */
    char default_dir[512] = "";
    if (!audio_dir) {
        /* Try to find tests/audio relative to the executable path, or cwd */
        const char *candidates[] = {
            "../tests/audio",
            "tests/audio",
            "../../tests/audio",
        };
        for (int c = 0; c < 3; c++) {
            char probe[512];
            snprintf(probe, sizeof(probe), "%s/alt5_test_panel_robot36.wav", candidates[c]);
            FILE *f = fopen(probe, "rb");
            if (f) { fclose(f); snprintf(default_dir, sizeof(default_dir), "%s", candidates[c]); break; }
        }
        if (!default_dir[0]) {
            fprintf(stderr, "ERROR: Could not locate tests/audio directory. "
                    "Pass path as first argument.\n");
            return 1;
        }
        audio_dir = default_dir;
    }

    printf("audio_dir  : %s\n", audio_dir);
    printf("mae_limit  : %.1f\n", mae_limit);
    printf("mode_only  : %s\n", mode_only ? "yes" : "no");
    printf("\n");

    int pass_total = 0, fail_total = 0;

    for (int ti = 0; ti < N_TESTS; ti++) {
        const test_entry_t *t = &TESTS[ti];
        printf("=== [%d/%d] %s ===\n", ti+1, N_TESTS, t->wav);

        /* ---- build file paths ---- */
        char wav_path[512], ref_path[512];
        snprintf(wav_path, sizeof(wav_path), "%s/%s", audio_dir, t->wav);
        snprintf(ref_path, sizeof(ref_path), "%s/%s", audio_dir, t->ref_jpg ? t->ref_jpg : "");

        /* ---- open WAV ---- */
        FILE *fp = fopen(wav_path, "rb");
        if (!fp) {
            printf("  SKIP  (WAV not found: %s)\n\n", wav_path);
            continue;
        }
        wav_info_t wi; memset(&wi, 0, sizeof(wi));
        if (read_wav(fp, &wi) != 0 || wi.audio_format != 1 ||
            wi.num_channels != 1 || wi.bits_per_sample != 16) {
            printf("  SKIP  (unsupported WAV format)\n\n");
            fclose(fp);
            continue;
        }
        fseek(fp, (long)wi.data_offset, SEEK_SET);

        /* ---- create decoder ---- */
        sstv_decoder_t *dec = sstv_decoder_create((double)wi.sample_rate);
        if (!dec) { printf("  FAIL  (decoder create failed)\n\n"); fail_total++; fclose(fp); continue; }
        sstv_decoder_set_debug_level(dec, debug_level);

        /* ---- feed samples ---- */
        const size_t CHUNK = 2048;
        int16_t *pcm = (int16_t *)malloc(CHUNK * sizeof(int16_t));
        float   *fbuf = (float   *)malloc(CHUNK * sizeof(float));
        if (!pcm || !fbuf) { printf("  FAIL  (OOM)\n\n"); fail_total++; free(pcm); free(fbuf); sstv_decoder_free(dec); fclose(fp); continue; }

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
            if (last_st == SSTV_RX_ERROR) break;
        }
        fclose(fp);
        free(pcm);
        free(fbuf);

        /* ---- check mode ---- */
        sstv_decoder_state_t state; memset(&state, 0, sizeof(state));
        sstv_decoder_get_state(dec, &state);
        int detected = (state.current_mode != SSTV_MODE_COUNT) ? (int)state.current_mode : -1;

        int mode_ok = (detected == t->expected_mode);
        printf("  mode     : detected=%d expected=%d  %s\n",
               detected, t->expected_mode, mode_ok ? "OK" : "FAIL");
        printf("  lines    : %d / %d  image_ready=%d\n",
               state.current_line, state.total_lines, state.image_ready);

        /* ---- check image ---- */
        sstv_image_t img; memset(&img, 0, sizeof(img));
        int got_img = (sstv_decoder_get_image(dec, &img) == 0 && img.pixels != NULL);
        int dim_ok = got_img && (int)img.width == t->exp_w && (int)img.height == t->exp_h;

        if (got_img) {
            /* Compute mean pixel value to detect all-black output */
            long sum = 0;
            for (int i = 0; i < (int)(img.width * img.height * 3); i++) sum += img.pixels[i];
            double mean_px = sum / (double)(img.width * img.height * 3);
            printf("  image    : %ux%u  (expected %dx%d)  mean_px=%.1f  %s\n",
                   img.width, img.height, t->exp_w, t->exp_h, mean_px,
                   dim_ok ? "OK" : "FAIL (wrong dimensions)");
        } else {
            printf("  image    : NOT PRODUCED  FAIL\n");
        }

        /* ---- MAE comparison ---- */
        double mae = -1.0;
        int mae_ok = 1;
        if (!mode_only && got_img && t->ref_jpg) {
            int rw, rh, rch;
            uint8_t *ref = stbi_load(ref_path, &rw, &rh, &rch, 0);
            if (ref) {
                mae = compute_mae(img.pixels, (int)img.width, (int)img.height,
                                  ref, rw, rh, rch);
                mae_ok = (mae <= mae_limit);
                printf("  MAE      : %.2f / %.0f  %s   (ref %dx%d ch=%d)\n",
                       mae, mae_limit, mae_ok ? "OK" : "FAIL", rw, rh, rch);
                stbi_image_free(ref);
            } else {
                printf("  MAE      : SKIP (ref not found: %s)\n", ref_path);
            }
        }

        /* ---- verdict ---- */
        int entry_pass = mode_ok && (got_img ? dim_ok : 0) && mae_ok;
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
