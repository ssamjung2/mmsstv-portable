/*
 * Basic decoder test harness
 * 
 * Tests:
 *   1. Decoder creation/destruction
 *   2. Sample feeding (no crash on valid input)
 *   3. State reset
 *   4. Synthetic tone generation (mark, space, sync)
 *   5. Sync detection trigger
 * 
 * Build: make test_decoder_basic
 * Run: ./bin/test_decoder_basic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sstv_decoder.h"

/* Test constants */
#define SAMPLE_RATE 48000.0
#define TEST_DURATION_SEC 1.0
#define SAMPLES_PER_TEST (int)(SAMPLE_RATE * TEST_DURATION_SEC)

/* Generator: simple sine wave at frequency `freq` */
static void generate_sine(float *out, size_t sample_count, double freq, double sample_rate, double amplitude) {
    const double pi2 = 2.0 * M_PI;
    for (size_t i = 0; i < sample_count; i++) {
        double t = i / sample_rate;
        out[i] = (float)(amplitude * sin(pi2 * freq * t));
    }
}

/* Test 1: Basic lifecycle */
int test_decoder_create_destroy(void) {
    printf("TEST 1: Decoder create/destroy\n");
    
    sstv_decoder_t *dec = sstv_decoder_create(SAMPLE_RATE);
    if (!dec) {
        printf("  FAIL: sstv_decoder_create returned NULL\n");
        return 0;
    }
    
    sstv_decoder_free(dec);
    printf("  PASS\n");
    return 1;
}

/* Test 2: Invalid sample rate */
int test_decoder_invalid_rate(void) {
    printf("TEST 2: Decoder with invalid sample rate\n");
    
    sstv_decoder_t *dec = sstv_decoder_create(-1.0);
    if (dec != NULL) {
        printf("  FAIL: Should reject negative sample rate\n");
        sstv_decoder_free(dec);
        return 0;
    }
    
    printf("  PASS\n");
    return 1;
}

/* Test 3: Feed samples without crash */
int test_decoder_feed_samples(void) {
    printf("TEST 3: Feed audio samples\n");
    
    sstv_decoder_t *dec = sstv_decoder_create(SAMPLE_RATE);
    if (!dec) {
        printf("  FAIL: create returned NULL\n");
        return 0;
    }
    
    /* Generate 0.1 seconds of silence */
    float *samples = (float *)calloc(SAMPLES_PER_TEST / 10, sizeof(float));
    if (!samples) {
        sstv_decoder_free(dec);
        printf("  FAIL: memory allocation\n");
        return 0;
    }
    memset(samples, 0, (SAMPLES_PER_TEST / 10) * sizeof(float));
    
    /* Feed samples */
    sstv_rx_status_t status = sstv_decoder_feed(dec, samples, SAMPLES_PER_TEST / 10);
    if (status == SSTV_RX_ERROR) {
        printf("  FAIL: decoder_feed returned ERROR\n");
        free(samples);
        sstv_decoder_free(dec);
        return 0;
    }
    
    free(samples);
    sstv_decoder_free(dec);
    printf("  PASS (status=%d)\n", status);
    return 1;
}

/* Test 4: Feed sine tone (mark frequency 1200 Hz) */
int test_decoder_mark_tone(void) {
    printf("TEST 4: Mark tone (1200 Hz, 0.1s)\n");
    
    sstv_decoder_t *dec = sstv_decoder_create(SAMPLE_RATE);
    if (!dec) {
        printf("  FAIL: create returned NULL\n");
        return 0;
    }
    
    /* Generate 1200 Hz tone for 0.1 seconds */
    size_t sample_count = (size_t)(SAMPLE_RATE * 0.1);
    float *samples = (float *)calloc(sample_count, sizeof(float));
    if (!samples) {
        sstv_decoder_free(dec);
        printf("  FAIL: memory allocation\n");
        return 0;
    }
    
    generate_sine(samples, sample_count, 1200.0, SAMPLE_RATE, 16000.0);
    
    /* Feed mark tone */
    sstv_rx_status_t status = sstv_decoder_feed(dec, samples, sample_count);
    
    free(samples);
    sstv_decoder_free(dec);
    
    if (status == SSTV_RX_ERROR) {
        printf("  FAIL: decoder_feed returned ERROR\n");
        return 0;
    }
    
    printf("  PASS (status=%d)\n", status);
    return 1;
}

/* Test 5: Reset decoder state */
int test_decoder_reset(void) {
    printf("TEST 5: Decoder reset\n");
    
    sstv_decoder_t *dec = sstv_decoder_create(SAMPLE_RATE);
    if (!dec) {
        printf("  FAIL: create returned NULL\n");
        return 0;
    }
    
    /* Feed some samples */
    size_t sample_count = (size_t)(SAMPLE_RATE * 0.05);
    float *samples = (float *)calloc(sample_count, sizeof(float));
    if (!samples) {
        sstv_decoder_free(dec);
        printf("  FAIL: memory allocation\n");
        return 0;
    }
    
    generate_sine(samples, sample_count, 2000.0, SAMPLE_RATE, 10000.0);
    sstv_decoder_feed(dec, samples, sample_count);
    
    /* Reset */
    sstv_decoder_reset(dec);
    
    free(samples);
    sstv_decoder_free(dec);
    
    printf("  PASS\n");
    return 1;
}

/* Test 6: Mode hint */
int test_decoder_mode_hint(void) {
    printf("TEST 6: Mode hint setting\n");
    
    sstv_decoder_t *dec = sstv_decoder_create(SAMPLE_RATE);
    if (!dec) {
        printf("  FAIL: create returned NULL\n");
        return 0;
    }
    
    /* Should not crash */
    sstv_decoder_set_mode_hint(dec, SSTV_R36);
    sstv_decoder_set_vis_enabled(dec, 1);
    
    sstv_decoder_free(dec);
    printf("  PASS\n");
    return 1;
}

/* Test 7: Get image (should fail gracefully when empty) */
int test_decoder_get_image(void) {
    printf("TEST 7: Get image (before decode complete)\n");
    
    sstv_decoder_t *dec = sstv_decoder_create(SAMPLE_RATE);
    if (!dec) {
        printf("  FAIL: create returned NULL\n");
        return 0;
    }
    
    sstv_image_t img;
    memset(&img, 0, sizeof(img));
    
    /* Should return -1 since no complete image yet */
    int result = sstv_decoder_get_image(dec, &img);
    
    sstv_decoder_free(dec);
    
    if (result != -1) {
        printf("  FAIL: expected -1, got %d\n", result);
        return 0;
    }
    
    printf("  PASS (returned error as expected)\n");
    return 1;
}

/* Main test runner */
int main(void) {
    printf("================================================================================\n");
    printf("              BASIC DECODER TESTS\n");
    printf("================================================================================\n\n");
    
    int pass = 0, fail = 0;
    
    pass += test_decoder_create_destroy();
    fail += !test_decoder_create_destroy();
    
    pass += test_decoder_invalid_rate();
    fail += !test_decoder_invalid_rate();
    
    pass += test_decoder_feed_samples();
    fail += !test_decoder_feed_samples();
    
    pass += test_decoder_mark_tone();
    fail += !test_decoder_mark_tone();
    
    pass += test_decoder_reset();
    fail += !test_decoder_reset();
    
    pass += test_decoder_mode_hint();
    fail += !test_decoder_mode_hint();
    
    pass += test_decoder_get_image();
    fail += !test_decoder_get_image();
    
    printf("\n================================================================================\n");
    printf("RESULT: %d passed, %d failed\n", pass, fail);
    printf("================================================================================\n");
    
    return (fail > 0) ? 1 : 0;
}
