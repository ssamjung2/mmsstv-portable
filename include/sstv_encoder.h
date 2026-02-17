/*
 * libsstv_encoder - Portable SSTV Encoder Library
 * Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
 * 
 * Copyright (C) 2000-2013 Makoto Mori, Nobuyuki Oba (original MMSSTV)
 * Copyright (C) 2026 (library port)
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */

#ifndef SSTV_ENCODER_H
#define SSTV_ENCODER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library version */
#define SSTV_ENCODER_VERSION_MAJOR 1
#define SSTV_ENCODER_VERSION_MINOR 0
#define SSTV_ENCODER_VERSION_PATCH 0
#define SSTV_ENCODER_VERSION "1.0.0"

/* SSTV Modes - matching original MMSSTV mode enumeration */
typedef enum {
    SSTV_R36 = 0,          /* Robot 36 */
    SSTV_R72,              /* Robot 72 */
    SSTV_AVT90,            /* AVT 90 */
    SSTV_SCOTTIE1,         /* Scottie 1 */
    SSTV_SCOTTIE2,         /* Scottie 2 */
    SSTV_SCOTTIEX,         /* Scottie DX */
    SSTV_MARTIN1,          /* Martin 1 */
    SSTV_MARTIN2,          /* Martin 2 */
    SSTV_SC2_180,          /* SC2-180 */
    SSTV_SC2_120,          /* SC2-120 */
    SSTV_SC2_60,           /* SC2-60 */
    SSTV_PD50,             /* PD 50 */
    SSTV_PD90,             /* PD 90 */
    SSTV_PD120,            /* PD 120 */
    SSTV_PD160,            /* PD 160 */
    SSTV_PD180,            /* PD 180 */
    SSTV_PD240,            /* PD 240 */
    SSTV_PD290,            /* PD 290 */
    SSTV_P3,               /* Pasokon P3 */
    SSTV_P5,               /* Pasokon P5 */
    SSTV_P7,               /* Pasokon P7 */
    SSTV_MR73,             /* Martin R73 */
    SSTV_MR90,             /* Martin R90 */
    SSTV_MR115,            /* Martin R115 */
    SSTV_MR140,            /* Martin R140 */
    SSTV_MR175,            /* Martin R175 */
    SSTV_MP73,             /* Martin P73 */
    SSTV_MP115,            /* Martin P115 */
    SSTV_MP140,            /* Martin P140 */
    SSTV_MP175,            /* Martin P175 */
    SSTV_ML180,            /* Martin L180 */
    SSTV_ML240,            /* Martin L240 */
    SSTV_ML280,            /* Martin L280 */
    SSTV_ML320,            /* Martin L320 */
    SSTV_R24,              /* Robot 24 */
    SSTV_BW8,              /* B/W 8 */
    SSTV_BW12,             /* B/W 12 */
    SSTV_MN73,             /* MP73-N */
    SSTV_MN110,            /* MP110-N */
    SSTV_MN140,            /* MP140-N */
    SSTV_MC110,            /* MC110-N */
    SSTV_MC140,            /* MC140-N */
    SSTV_MC180,            /* MC180-N */
    SSTV_MODE_COUNT        /* Total number of modes */
} sstv_mode_t;

/* Pixel format */
typedef enum {
    SSTV_RGB24 = 0,        /* 24-bit RGB (R, G, B bytes) */
    SSTV_GRAY8             /* 8-bit grayscale */
} sstv_pixel_format_t;

/* Image structure */
typedef struct {
    uint8_t *pixels;              /* Image pixel data */
    uint32_t width;               /* Image width in pixels */
    uint32_t height;              /* Image height in pixels */
    uint32_t stride;              /* Bytes per row (usually width * bytes_per_pixel) */
    sstv_pixel_format_t format;   /* Pixel format */
} sstv_image_t;

/* Mode information */
typedef struct {
    sstv_mode_t mode;             /* Mode enumeration value */
    const char *name;             /* Mode name (e.g., "Scottie 1") */
    uint32_t width;               /* Image width */
    uint32_t height;              /* Image height */
    uint8_t vis_code;             /* VIS code byte for auto-detection */
    double duration_sec;          /* Total transmission time in seconds */
    int is_color;                 /* 1=color, 0=grayscale */
} sstv_mode_info_t;

/* Encoder handle (opaque) */
typedef struct sstv_encoder_s sstv_encoder_t;

/*==============================================================================
 * ENCODER API
 *============================================================================*/

/**
 * Create an encoder for the specified mode
 * 
 * @param mode        SSTV mode to encode
 * @param sample_rate Audio sample rate in Hz (typically 8000, 11025, 48000)
 * @return Encoder handle or NULL on error
 */
sstv_encoder_t* sstv_encoder_create(sstv_mode_t mode, double sample_rate);

/**
 * Free encoder resources
 * 
 * @param encoder Encoder handle (NULL safe)
 */
void sstv_encoder_free(sstv_encoder_t *encoder);

/**
 * Set the source image to encode
 * The image must remain valid until encoding is complete
 * 
 * @param encoder Encoder handle
 * @param image   Source image
 * @return 0 on success, -1 on error
 */
int sstv_encoder_set_image(sstv_encoder_t *encoder, const sstv_image_t *image);

/**
 * Enable/disable VIS code transmission
 * VIS code is transmitted at the start to identify the mode
 * Default: enabled
 * 
 * @param encoder Encoder handle
 * @param enable  1 to enable, 0 to disable
 */
void sstv_encoder_set_vis_enabled(sstv_encoder_t *encoder, int enable);

/**
 * Generate audio samples
 * Call repeatedly until sstv_encoder_is_complete() returns 1
 * 
 * @param encoder     Encoder handle
 * @param samples     Output buffer for float samples (range: -1.0 to +1.0)
 * @param max_samples Maximum number of samples to generate
 * @return Number of samples generated (0 means complete or error)
 */
size_t sstv_encoder_generate(
    sstv_encoder_t *encoder,
    float *samples,
    size_t max_samples
);

/**
 * Check if encoding is complete
 * 
 * @param encoder Encoder handle
 * @return 1 if complete, 0 if more samples to generate
 */
int sstv_encoder_is_complete(sstv_encoder_t *encoder);

/**
 * Get encoding progress (0.0 to 1.0)
 * 
 * @param encoder Encoder handle
 * @return Progress value from 0.0 (starting) to 1.0 (complete)
 */
float sstv_encoder_get_progress(sstv_encoder_t *encoder);

/**
 * Reset encoder to start
 * Allows re-encoding the same or different image
 * 
 * @param encoder Encoder handle
 */
void sstv_encoder_reset(sstv_encoder_t *encoder);

/**
 * Get total number of samples that will be generated
 * Useful for pre-allocating buffers
 * 
 * @param encoder Encoder handle
 * @return Total sample count
 */
size_t sstv_encoder_get_total_samples(sstv_encoder_t *encoder);

/*==============================================================================
 * MODE INFO API
 *============================================================================*/

/**
 * Get information about a specific mode
 * 
 * @param mode SSTV mode
 * @return Pointer to mode info or NULL if invalid mode
 */
const sstv_mode_info_t* sstv_get_mode_info(sstv_mode_t mode);

/**
 * Get list of all available modes
 * 
 * @param count Output: number of modes (can be NULL)
 * @return Array of mode info structures
 */
const sstv_mode_info_t* sstv_get_all_modes(size_t *count);

/**
 * Find mode by name (case-insensitive)
 * 
 * @param name Mode name (e.g., "scottie 1", "Martin2")
 * @return Mode enum or -1 if not found
 */
int sstv_find_mode_by_name(const char *name);

/*==============================================================================
 * UTILITY API
 *============================================================================*/

/**
 * Get library version string
 * 
 * @return Version string (e.g., "1.0.0")
 */
const char* sstv_encoder_version(void);

/**
 * Helper: Create image structure from RGB buffer
 * Note: Does NOT copy data - caller must keep buffer valid
 * 
 * @param rgb_data RGB pixel data (R,G,B,R,G,B,...)
 * @param width    Image width in pixels
 * @param height   Image height in pixels
 * @return Image structure
 */
sstv_image_t sstv_image_from_rgb(
    uint8_t *rgb_data,
    uint32_t width,
    uint32_t height
);

/**
 * Helper: Create image structure from grayscale buffer
 * Note: Does NOT copy data - caller must keep buffer valid
 * 
 * @param gray_data Grayscale pixel data
 * @param width     Image width in pixels
 * @param height    Image height in pixels
 * @return Image structure
 */
sstv_image_t sstv_image_from_gray(
    uint8_t *gray_data,
    uint32_t width,
    uint32_t height
);

/**
 * Calculate required image dimensions for a mode
 * 
 * @param mode   SSTV mode
 * @param width  Output: image width
 * @param height Output: image height
 * @return 0 on success, -1 if mode is invalid
 */
int sstv_get_mode_dimensions(
    sstv_mode_t mode,
    uint32_t *width,
    uint32_t *height
);

#ifdef __cplusplus
}
#endif

#endif /* SSTV_ENCODER_H */
