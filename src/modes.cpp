/*
 * SSTV Mode Definitions and Utilities
 * Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
 */

#include <sstv_encoder.h>
#include <string.h>
#include <ctype.h>

/* Mode information table - extracted from MMSSTV 
 * Duration = (ms_per_line / 1000) * num_lines
 */
static const sstv_mode_info_t mode_table[SSTV_MODE_COUNT] = {
    /* Mode enum      Name            Width Height VIS   Duration(s) Color */
    {SSTV_R36,       "Robot 36",      320,  240,  0x88,  36.0,       1},    /* 150.0ms/line × 240 lines */
    {SSTV_R72,       "Robot 72",      320,  240,  0x0c,  72.0,       1},    /* 300.0ms/line × 240 lines */
    {SSTV_AVT90,     "AVT 90",        320,  240,  0x44,  90.0,       1},    /* 375ms/line × 240 lines */
    {SSTV_SCOTTIE1,  "Scottie 1",     320,  256,  0x3c,  109.624,    1},    /* 428.22ms/line × 256 lines */
    {SSTV_SCOTTIE2,  "Scottie 2",     320,  256,  0xb8,  71.089,     1},    /* 277.692ms/line × 256 lines */
    {SSTV_SCOTTIEX,  "ScottieDX",     320,  256,  0xcc,  268.877,    1},    /* 1050.3ms/line × 256 lines */
    {SSTV_MARTIN1,   "Martin 1",      320,  256,  0xac,  114.290,    1},    /* 446.446ms/line × 256 lines */
    {SSTV_MARTIN2,   "Martin 2",      320,  256,  0x28,  58.060,     1},    /* 226.798ms/line × 256 lines */
    {SSTV_SC2_180,   "SC2 180",       320,  256,  0xb7,  182.027,    1},    /* 711.0437ms/line × 256 lines */
    {SSTV_SC2_120,   "SC2 120",       320,  256,  0x3f,  121.734,    1},    /* 475.52248ms/line × 256 lines */
    {SSTV_SC2_60,    "SC2 60",        320,  256,  0xbb,  61.539,     1},    /* 240.3846ms/line × 256 lines */
    {SSTV_PD50,      "PD50",          320,  256,  0xdd,  49.684,     1},    /* 388.160ms/line × 128 lines */
    {SSTV_PD90,      "PD90",          320,  256,  0x63,  89.989,     1},    /* 703.040ms/line × 128 lines */
    {SSTV_PD120,     "PD120",         640,  496,  0x5f,  126.103,    1},    /* 508.480ms/line × 248 lines */
    {SSTV_PD160,     "PD160",         512,  400,  0xe2,  160.883,    1},    /* 804.416ms/line × 200 lines */
    {SSTV_PD180,     "PD180",         640,  496,  0x60,  187.051,    1},    /* 754.24ms/line × 248 lines */
    {SSTV_PD240,     "PD240",         640,  496,  0xe1,  248.000,    1},    /* 1000.00ms/line × 248 lines */
    {SSTV_PD290,     "PD290",         800,  616,  0xde,  288.682,    1},    /* 937.28ms/line × 308 lines */
    {SSTV_P3,        "P3",            640,  496,  0x71,  203.050,    1},    /* 409.375ms/line × 496 lines */
    {SSTV_P5,        "P5",            640,  496,  0x72,  304.575,    1},    /* 614.0625ms/line × 496 lines */
    {SSTV_P7,        "P7",            640,  496,  0xf3,  406.100,    1},    /* 818.75ms/line × 496 lines */
    {SSTV_MR73,      "MR73",          320,  256,  0x45,  73.293,     1},    /* 286.3ms/line × 256 lines */
    {SSTV_MR90,      "MR90",          320,  256,  0x46,  90.189,     1},    /* 352.3ms/line × 256 lines */
    {SSTV_MR115,     "MR115",         320,  256,  0x49,  115.277,    1},    /* 450.3ms/line × 256 lines */
    {SSTV_MR140,     "MR140",         320,  256,  0x4a,  140.365,    1},    /* 548.3ms/line × 256 lines */
    {SSTV_MR175,     "MR175",         320,  256,  0x4c,  175.181,    1},    /* 684.3ms/line × 256 lines */
    {SSTV_MP73,      "MP73",          320,  256,  0x25,  72.960,     1},    /* 570.0ms/line × 128 lines */
    {SSTV_MP115,     "MP115",         320,  256,  0x29,  115.456,    1},    /* 902.0ms/line × 128 lines */
    {SSTV_MP140,     "MP140",         320,  256,  0x2a,  139.520,    1},    /* 1090.0ms/line × 128 lines */
    {SSTV_MP175,     "MP175",         320,  256,  0x2c,  175.360,    1},    /* 1370.0ms/line × 128 lines */
    {SSTV_ML180,     "ML180",         640,  496,  0x85,  180.197,    1},    /* 363.3ms/line × 496 lines */
    {SSTV_ML240,     "ML240",         640,  496,  0x86,  239.717,    1},    /* 483.3ms/line × 496 lines */
    {SSTV_ML280,     "ML280",         640,  496,  0x89,  280.389,    1},    /* 565.3ms/line × 496 lines */
    {SSTV_ML320,     "ML320",         640,  496,  0x8a,  320.069,    1},    /* 645.3ms/line × 496 lines */
    {SSTV_R24,       "Robot 24",      320,  240,  0x84,  24.000,     1},    /* 200.0ms/line × 120 lines */
    {SSTV_BW8,       "B/W 8",         320,  240,  0x82,  8.028,      0},    /* 66.89709ms/line × 120 lines */
    {SSTV_BW12,      "B/W 12",        320,  240,  0x86,  12.000,     0},    /* 100.0ms/line × 120 lines */
    {SSTV_MN73,      "MP73-N",        320,  256,  0x00,  72.960,     1},    /* 570.0ms/line × 128 lines, VIS not documented */
    {SSTV_MN110,     "MP110-N",       320,  256,  0x00,  109.824,    1},    /* 858.0ms/line × 128 lines, VIS not documented */
    {SSTV_MN140,     "MP140-N",       320,  256,  0x00,  139.520,    1},    /* 1090.0ms/line × 128 lines, VIS not documented */
    {SSTV_MC110,     "MC110-N",       320,  256,  0x00,  109.696,    1},    /* 428.5ms/line × 256 lines, VIS not documented */
    {SSTV_MC140,     "MC140-N",       320,  256,  0x00,  140.416,    1},    /* 548.5ms/line × 256 lines, VIS not documented */
    {SSTV_MC180,     "MC180-N",       320,  256,  0x00,  180.352,    1},    /* 704.5ms/line × 256 lines, VIS not documented */
};

const sstv_mode_info_t* sstv_get_mode_info(sstv_mode_t mode) {
    if (mode < 0 || mode >= SSTV_MODE_COUNT) {
        return NULL;
    }
    return &mode_table[mode];
}

const sstv_mode_info_t* sstv_get_all_modes(size_t *count) {
    if (count) {
        *count = SSTV_MODE_COUNT;
    }
    return mode_table;
}

int sstv_find_mode_by_name(const char *name) {
    if (!name) return -1;
    
    for (int i = 0; i < SSTV_MODE_COUNT; i++) {
        /* Case-insensitive comparison */
        const char *mode_name = mode_table[i].name;
        if (strcasecmp(name, mode_name) == 0) {
            return i;
        }
    }
    return -1;
}

const char* sstv_encoder_version(void) {
    return SSTV_ENCODER_VERSION;
}

sstv_image_t sstv_image_from_rgb(uint8_t *rgb_data, uint32_t width, uint32_t height) {
    sstv_image_t img;
    img.pixels = rgb_data;
    img.width = width;
    img.height = height;
    img.stride = width * 3;  /* 3 bytes per pixel for RGB */
    img.format = SSTV_RGB24;
    return img;
}

sstv_image_t sstv_image_from_gray(uint8_t *gray_data, uint32_t width, uint32_t height) {
    sstv_image_t img;
    img.pixels = gray_data;
    img.width = width;
    img.height = height;
    img.stride = width;  /* 1 byte per pixel for grayscale */
    img.format = SSTV_GRAY8;
    return img;
}

int sstv_get_mode_dimensions(sstv_mode_t mode, uint32_t *width, uint32_t *height) {
    const sstv_mode_info_t *info = sstv_get_mode_info(mode);
    if (!info) return -1;
    
    if (width) *width = info->width;
    if (height) *height = info->height;
    return 0;
}
