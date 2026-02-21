/*
 * libsstv_decoder - Portable SSTV Decoder Library (RX)
 * Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
 *
 * Copyright (C) 2000-2013 Makoto Mori, Nobuyuki Oba (original MMSSTV)
 * Copyright (C) 2026 (library port)
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SSTV_DECODER_H
#define SSTV_DECODER_H

#include <stddef.h>
#include <stdint.h>

#include "sstv_encoder.h"  /* Shared types (sstv_mode_t, sstv_image_t, etc.) */

#ifdef __cplusplus
extern "C" {
#endif

/* Decoder status */
typedef enum {
    SSTV_RX_OK = 0,
    SSTV_RX_NEED_MORE = 1,
    SSTV_RX_IMAGE_READY = 2,
    SSTV_RX_ERROR = -1
} sstv_rx_status_t;

/* AGC mode for VIS detection */
typedef enum {
    SSTV_AGC_OFF = 0,      /* No AGC, use fixed levels */
    SSTV_AGC_LOW = 1,      /* Minimal gain adjustment (5% of signal peak) */
    SSTV_AGC_MED = 2,      /* Medium gain adjustment (10% of signal peak) */
    SSTV_AGC_HIGH = 3,     /* Aggressive gain adjustment (20% of signal peak) */
    SSTV_AGC_SEMI = 4,     /* Semi-automatic: apply fixed 6 dB gain boost */
    SSTV_AGC_AUTO = 5      /* Automatic: select mode based on signal level */
} sstv_agc_mode_t;

/* Decoder handle (opaque) */
typedef struct sstv_decoder_s sstv_decoder_t;

/**
 * Decoder internal state (not exposed in public API, but documented for reference)
 * 
 * Key components:
 * - FM demod: CIIRTANK resonators at mark/space tones + PLL frequency tracking
 * - Sync detection: CIIRTANK at sync frequency (1200 Hz) with state machine
 * - VIS decode: Sync interrupt tracking with bit-by-bit accumulation
 * - Image buffer: Line-by-line grayscale pixel storage
 */

/**
 * Create a decoder for the specified sample rate
 *
 * @param sample_rate Audio sample rate in Hz
 * @return Decoder handle or NULL on error
 */
sstv_decoder_t* sstv_decoder_create(double sample_rate);

/**
 * Free decoder resources
 *
 * @param dec Decoder handle (NULL safe)
 */
void sstv_decoder_free(sstv_decoder_t *dec);

/**
 * Reset decoder state
 *
 * @param dec Decoder handle
 */
void sstv_decoder_reset(sstv_decoder_t *dec);

/**
 * Optional mode hint (can speed acquisition)
 *
 * @param dec Decoder handle
 * @param mode SSTV mode hint
 */
void sstv_decoder_set_mode_hint(sstv_decoder_t *dec, sstv_mode_t mode);

/**
 * Enable/disable VIS decode
 *
 * @param dec Decoder handle
 * @param enable 1 to enable, 0 to disable
 */
void sstv_decoder_set_vis_enabled(sstv_decoder_t *dec, int enable);

/**
 * Set AGC mode for VIS detection
 *
 * @param dec Decoder handle
 * @param mode AGC mode (OFF, LOW, MED, HIGH, SEMI, AUTO)
 */
void sstv_decoder_set_agc_mode(sstv_decoder_t *dec, sstv_agc_mode_t mode);

/**
 * Get current AGC mode
 *
 * @param dec Decoder handle
 * @return Current AGC mode
 */
sstv_agc_mode_t sstv_decoder_get_agc_mode(sstv_decoder_t *dec);

/**
 * Override VIS mark/space tones (Hz)
 *
 * @param dec Decoder handle
 * @param mark_hz Mark tone frequency (bit=1)
 * @param space_hz Space tone frequency (bit=0)
 */
void sstv_decoder_set_vis_tones(sstv_decoder_t *dec, double mark_hz, double space_hz);

/**
 * Feed audio samples into decoder
 *
 * @param dec Decoder handle
 * @param samples Input samples (float, 16-bit PCM scale: -32768 to +32767)
 * @param sample_count Number of samples in buffer
 * @return Decoder status
 */
sstv_rx_status_t sstv_decoder_feed(
    sstv_decoder_t *dec,
    const float *samples,
    size_t sample_count
);

/**
 * Retrieve decoded image when SSTV_RX_IMAGE_READY
 *
 * @param dec Decoder handle
 * @param out_image Output image (caller owns pixels)
 * @return 0 on success, -1 on error
 */
int sstv_decoder_get_image(sstv_decoder_t *dec, sstv_image_t *out_image);

/**
 * Decoder state for diagnostics
 */
typedef struct {
    sstv_mode_t current_mode;    /* Detected/hinted SSTV mode */
    int vis_enabled;             /* VIS decoding enabled */
    int sync_detected;           /* Sync pulse detected */
    int image_ready;             /* Image decoding complete */
    int current_line;            /* Current scan line */
    int total_lines;             /* Total lines in image */
} sstv_decoder_state_t;

/**
 * Get decoder state (for diagnostics/progress tracking)
 *
 * @param dec Decoder handle
 * @param state Output state structure
 * @return 0 on success, -1 on error
 */
int sstv_decoder_get_state(sstv_decoder_t *dec, sstv_decoder_state_t *state);

/**
 * Set debug level (0=quiet, 1=errors, 2=verbose)
 *
 * @param dec Decoder handle
 * @param level Debug level
 */
void sstv_decoder_set_debug_level(sstv_decoder_t *dec, int level);

/**
 * Enable intermediate WAV file writing for filter analysis
 *
 * Writes audio samples at different stages of the processing pipeline:
 * - before: After initial LPF, before BPF (shows raw input with anti-aliasing)
 * - after_bpf: After bandpass filter (shows filtered signal)
 * - after_agc: After AGC normalization (shows gain-adjusted signal)
 * - final: After final scaling, input to tone detectors (working signal)
 *
 * Pass NULL for any file you don't want to create.
 * Files are closed and finalized when decoder is freed.
 *
 * @param dec Decoder handle
 * @param before_filepath Path for before-filtering WAV (or NULL)
 * @param after_bpf_filepath Path for after-BPF WAV (or NULL)
 * @param after_agc_filepath Path for after-AGC WAV (or NULL)
 * @param final_filepath Path for final scaled WAV (or NULL)
 * @return 0 on success, -1 on error
 */
int sstv_decoder_enable_debug_wav(sstv_decoder_t *dec,
                                   const char *before_filepath,
                                   const char *after_bpf_filepath,
                                   const char *after_agc_filepath,
                                   const char *final_filepath);

/**
 * Disable debug WAV writing and close any open files
 *
 * @param dec Decoder handle
 */
void sstv_decoder_disable_debug_wav(sstv_decoder_t *dec);

/**
 * Feed a single sample (convenience wrapper)
 *
 * @param dec Decoder handle
 * @param sample Single audio sample (16-bit PCM scale)
 * @return Decoder status
 */
sstv_rx_status_t sstv_decoder_feed_sample(sstv_decoder_t *dec, float sample);

#ifdef __cplusplus
}
#endif

#endif /* SSTV_DECODER_H */
