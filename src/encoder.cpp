/*
 * Main SSTV Encoder Implementation
 * Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
 */

#include <sstv_encoder.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cmath>
#include <new>

#include "vco.h"
#include "vis.h"

/* Internal mode timing (derived from MMSSTV CSSTVSET::SetSampFreq/GetTiming) */
struct ModeTiming {
    double sample_rate;
    int line_count;
    double line_ms;
    double line_samples;

    double ks;
    double ks2;
    double of;
    double ofp;
    double sg;
    double cg;
    double sb;
    double cb;

    double kss;
    double ks2s;
    double ksb;
};

static const int NARROW_SYNC = 1900;
static const int NARROW_LOW = 2044;
static const int NARROW_HIGH = 2300;
static const int NARROW_BW = (NARROW_HIGH - NARROW_LOW);

struct Segment {
    double freq;
    size_t samples;
};

static int clamp_0_255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static int color_to_freq(int d) {
    d = d * (2300 - 1500) / 256;
    return d + 1500;
}

static int color_to_freq_narrow(int d) {
    d = d * NARROW_BW / 256;
    return d + NARROW_LOW;
}

static void get_pixel_rgb(const sstv_image_t *image, int x, int y, int &r, int &g, int &b) {
    if (!image || !image->pixels) {
        r = g = b = 0;
        return;
    }
    const uint8_t *row = image->pixels + (size_t)y * image->stride;
    if (image->format == SSTV_RGB24) {
        const uint8_t *p = row + (size_t)x * 3;
        r = p[0];
        g = p[1];
        b = p[2];
    } else {
        const uint8_t *p = row + (size_t)x;
        r = g = b = p[0];
    }
}

static void get_ry(int r, int g, int b, int &y, int &ry, int &by) {
    double R = r;
    double G = g;
    double B = b;
    y = (int)(16.0 + (0.256773 * R + 0.504097 * G + 0.097900 * B));
    ry = (int)(128.0 + (0.439187 * R - 0.367766 * G - 0.071421 * B));
    by = (int)(128.0 + (-0.148213 * R - 0.290974 * G + 0.439187 * B));
    y = clamp_0_255(y);
    ry = clamp_0_255(ry);
    by = clamp_0_255(by);
}

static bool is_narrow_mode(sstv_mode_t mode) {
    switch (mode) {
        case SSTV_MN73:
        case SSTV_MN110:
        case SSTV_MN140:
        case SSTV_MC110:
        case SSTV_MC140:
        case SSTV_MC180:
            return true;
        default:
            return false;
    }
}

static bool is_pd_mode(sstv_mode_t mode) {
    switch (mode) {
        case SSTV_PD50:
        case SSTV_PD90:
        case SSTV_PD120:
        case SSTV_PD160:
        case SSTV_PD180:
        case SSTV_PD240:
        case SSTV_PD290:
            return true;
        default:
            return false;
    }
}

static bool is_mmsstv_vis_mode(sstv_mode_t mode) {
    switch (mode) {
        case SSTV_MP73:
        case SSTV_MP115:
        case SSTV_MP140:
        case SSTV_MP175:
        case SSTV_MR73:
        case SSTV_MR90:
        case SSTV_MR115:
        case SSTV_MR140:
        case SSTV_MR175:
        case SSTV_ML180:
        case SSTV_ML240:
        case SSTV_ML280:
        case SSTV_ML320:
            return true;
        default:
            return false;
    }
}

static uint16_t get_mmsstv_vis_word(sstv_mode_t mode) {
    switch (mode) {
        case SSTV_MP73:  return 0x2523;
        case SSTV_MP115: return 0x2923;
        case SSTV_MP140: return 0x2a23;
        case SSTV_MP175: return 0x2c23;
        case SSTV_MR73:  return 0x4523;
        case SSTV_MR90:  return 0x4623;
        case SSTV_MR115: return 0x4923;
        case SSTV_MR140: return 0x4a23;
        case SSTV_MR175: return 0x4c23;
        case SSTV_ML180: return 0x8523;
        case SSTV_ML240: return 0x8623;
        case SSTV_ML280: return 0x8923;
        case SSTV_ML320: return 0x8a23;
        default:         return 0x0000;
    }
}

static double get_preamble_ms(sstv_mode_t mode) {
    if (is_narrow_mode(mode)) {
        return 400.0; /* 4 tones × 100ms */
    }
    return 800.0;     /* 8 tones × 100ms */
}

static double get_line_ms(sstv_mode_t mode) {
    switch (mode) {
        case SSTV_R36:      return 150.0;
        case SSTV_R72:      return 300.0;
        case SSTV_AVT90:    return 375.0;
        case SSTV_SCOTTIE2: return 277.692;
        case SSTV_SCOTTIEX: return 1050.3;
        case SSTV_MARTIN1:  return 446.446;
        case SSTV_MARTIN2:  return 226.798;
        case SSTV_SC2_180:  return 711.0437;
        case SSTV_SC2_120:  return 475.52248;
        case SSTV_SC2_60:   return 240.3846;
        case SSTV_PD50:     return 388.160;
        case SSTV_PD90:     return 703.040;
        case SSTV_PD120:    return 508.480;
        case SSTV_PD160:    return 804.416;
        case SSTV_PD180:    return 754.24;
        case SSTV_PD240:    return 1000.00;
        case SSTV_PD290:    return 937.28;
        case SSTV_P3:       return 409.375;
        case SSTV_P5:       return 614.0625;
        case SSTV_P7:       return 818.75;
        case SSTV_MR73:     return 286.3;
        case SSTV_MR90:     return 352.3;
        case SSTV_MR115:    return 450.3;
        case SSTV_MR140:    return 548.3;
        case SSTV_MR175:    return 684.3;
        case SSTV_MP73:     return 570.0;
        case SSTV_MP115:    return 902.0;
        case SSTV_MP140:    return 1090.0;
        case SSTV_MP175:    return 1370.0;
        case SSTV_ML180:    return 363.3;
        case SSTV_ML240:    return 483.3;
        case SSTV_ML280:    return 565.3;
        case SSTV_ML320:    return 645.3;
        case SSTV_R24:      return 200.0;
        case SSTV_BW8:      return 66.89709;
        case SSTV_BW12:     return 100.0;
        case SSTV_MN73:     return 570.0;
        case SSTV_MN110:    return 858.0;
        case SSTV_MN140:    return 1090.0;
        case SSTV_MC110:    return 428.5;
        case SSTV_MC140:    return 548.5;
        case SSTV_MC180:    return 704.5;
        case SSTV_SCOTTIE1:
        default:
            return 428.22;
    }
}

static void compute_mode_timing(sstv_mode_t mode, double sample_rate, ModeTiming *timing) {
    if (!timing) return;
    std::memset(timing, 0, sizeof(*timing));
    timing->sample_rate = sample_rate;

    switch (mode) {
        case SSTV_R36:
            timing->ks = 88.0 * sample_rate / 1000.0;
            timing->ks2 = 44.0 * sample_rate / 1000.0;
            timing->of = 12.0 * sample_rate / 1000.0;
            timing->ofp = 10.7 * sample_rate / 1000.0;
            timing->sg = (88.0 + 1.25) * sample_rate / 1000.0;
            timing->cg = (88.0 + 3.5) * sample_rate / 1000.0;
            timing->sb = 94.0 * sample_rate / 1000.0;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 240;
            break;
        case SSTV_R72:
            timing->ks = 138.0 * sample_rate / 1000.0;
            timing->ks2 = 69.0 * sample_rate / 1000.0;
            timing->of = 12.0 * sample_rate / 1000.0;
            timing->ofp = 10.7 * sample_rate / 1000.0;
            timing->sg = 144.0 * sample_rate / 1000.0;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = 219.0 * sample_rate / 1000.0;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 240;
            break;
        case SSTV_AVT90:
            timing->ks = 125.0 * sample_rate / 1000.0;
            timing->of = 0.0 * sample_rate / 1000.0;
            timing->ofp = 0.0 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 240;
            break;
        case SSTV_SCOTTIE2:
            timing->ks = 88.064 * sample_rate / 1000.0;
            timing->of = 10.5 * sample_rate / 1000.0;
            timing->ofp = 10.8 * sample_rate / 1000.0;
            timing->sg = 89.564 * sample_rate / 1000.0;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_SCOTTIEX:
            timing->ks = 345.6 * sample_rate / 1000.0;
            timing->of = 10.5 * sample_rate / 1000.0;
            timing->ofp = 10.2 * sample_rate / 1000.0;
            timing->sg = 347.1 * sample_rate / 1000.0;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_MARTIN1:
            timing->ks = 146.432 * sample_rate / 1000.0;
            timing->of = 5.434 * sample_rate / 1000.0;
            timing->ofp = 7.2 * sample_rate / 1000.0;
            timing->sg = 147.004 * sample_rate / 1000.0;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_MARTIN2:
            timing->ks = 73.216 * sample_rate / 1000.0;
            timing->of = 5.434 * sample_rate / 1000.0;
            timing->ofp = 7.4 * sample_rate / 1000.0;
            timing->sg = 73.788 * sample_rate / 1000.0;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_SC2_180:
            timing->ks = 235.0 * sample_rate / 1000.0;
            timing->of = 6.0437 * sample_rate / 1000.0;
            timing->ofp = 7.8 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_SC2_120:
            timing->ks = 156.5 * sample_rate / 1000.0;
            timing->of = 6.02248 * sample_rate / 1000.0;
            timing->ofp = 7.5 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_SC2_60:
            timing->ks = 78.128 * sample_rate / 1000.0;
            timing->of = 6.0006 * sample_rate / 1000.0;
            timing->ofp = 7.9 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_PD50:
            timing->ks = 91.520 * sample_rate / 1000.0;
            timing->of = 22.080 * sample_rate / 1000.0;
            timing->ofp = 19.300 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 128;
            break;
        case SSTV_PD90:
            timing->ks = 170.240 * sample_rate / 1000.0;
            timing->of = 22.080 * sample_rate / 1000.0;
            timing->ofp = 18.900 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 128;
            break;
        case SSTV_PD120:
            timing->ks = 121.600 * sample_rate / 1000.0;
            timing->of = 22.080 * sample_rate / 1000.0;
            timing->ofp = 19.400 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 248;
            break;
        case SSTV_PD160:
            timing->ks = 195.584 * sample_rate / 1000.0;
            timing->of = 22.080 * sample_rate / 1000.0;
            timing->ofp = 18.900 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 200;
            break;
        case SSTV_PD180:
            timing->ks = 183.04 * sample_rate / 1000.0;
            timing->of = 22.080 * sample_rate / 1000.0;
            timing->ofp = 18.900 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 248;
            break;
        case SSTV_PD240:
            timing->ks = 244.48 * sample_rate / 1000.0;
            timing->of = 22.080 * sample_rate / 1000.0;
            timing->ofp = 18.900 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 248;
            break;
        case SSTV_PD290:
            timing->ks = 228.80 * sample_rate / 1000.0;
            timing->of = 22.080 * sample_rate / 1000.0;
            timing->ofp = 18.900 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 616 / 2;
            break;
        case SSTV_P3:
            timing->ks = 133.333 * sample_rate / 1000.0;
            timing->of = (5.208 + 1.042) * sample_rate / 1000.0;
            timing->ofp = 7.80 * sample_rate / 1000.0;
            timing->sg = (133.333 + 1.042) * sample_rate / 1000.0;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 496;
            break;
        case SSTV_P5:
            timing->ks = 200.000 * sample_rate / 1000.0;
            timing->of = (7.813 + 1.562375) * sample_rate / 1000.0;
            timing->ofp = 9.20 * sample_rate / 1000.0;
            timing->sg = (200.000 + 1.562375) * sample_rate / 1000.0;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 496;
            break;
        case SSTV_P7:
            timing->ks = 266.667 * sample_rate / 1000.0;
            timing->of = (10.417 + 2.083) * sample_rate / 1000.0;
            timing->ofp = 11.50 * sample_rate / 1000.0;
            timing->sg = (266.667 + 2.083) * sample_rate / 1000.0;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 496;
            break;
        case SSTV_MR73:
            timing->ks = 138.0 * sample_rate / 1000.0;
            timing->ks2 = timing->ks * 0.5;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.6 * sample_rate / 1000.0;
            timing->sg = timing->ks + 0.1;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 0.1;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 256;
            break;
        case SSTV_MR90:
            timing->ks = 171.0 * sample_rate / 1000.0;
            timing->ks2 = timing->ks * 0.5;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.6 * sample_rate / 1000.0;
            timing->sg = timing->ks + 0.1;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 0.1;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 256;
            break;
        case SSTV_MR115:
            timing->ks = 220.0 * sample_rate / 1000.0;
            timing->ks2 = timing->ks * 0.5;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.6 * sample_rate / 1000.0;
            timing->sg = timing->ks + 0.1;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 0.1;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 256;
            break;
        case SSTV_MR140:
            timing->ks = 269.0 * sample_rate / 1000.0;
            timing->ks2 = timing->ks * 0.5;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.6 * sample_rate / 1000.0;
            timing->sg = timing->ks + 0.1;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 0.1;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 256;
            break;
        case SSTV_MR175:
            timing->ks = 337.0 * sample_rate / 1000.0;
            timing->ks2 = timing->ks * 0.5;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.6 * sample_rate / 1000.0;
            timing->sg = timing->ks + 0.1;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 0.1;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 256;
            break;
        case SSTV_MP73:
        case SSTV_MN73:
            timing->ks = 140.0 * sample_rate / 1000.0;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.5 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 128;
            break;
        case SSTV_MP115:
            timing->ks = 223.0 * sample_rate / 1000.0;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.5 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 128;
            break;
        case SSTV_MP140:
            timing->ks = 270.0 * sample_rate / 1000.0;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.5 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 128;
            break;
        case SSTV_MP175:
            timing->ks = 340.0 * sample_rate / 1000.0;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.5 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 128;
            break;
        case SSTV_ML180:
            timing->ks = 176.5 * sample_rate / 1000.0;
            timing->ks2 = timing->ks * 0.5;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.6 * sample_rate / 1000.0;
            timing->sg = timing->ks + 0.1;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 0.1;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 496;
            break;
        case SSTV_ML240:
            timing->ks = 236.5 * sample_rate / 1000.0;
            timing->ks2 = timing->ks * 0.5;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.6 * sample_rate / 1000.0;
            timing->sg = timing->ks + 0.1;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 0.1;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 496;
            break;
        case SSTV_ML280:
            timing->ks = 277.5 * sample_rate / 1000.0;
            timing->ks2 = timing->ks * 0.5;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.6 * sample_rate / 1000.0;
            timing->sg = timing->ks + 0.1;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 0.1;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 496;
            break;
        case SSTV_ML320:
            timing->ks = 317.5 * sample_rate / 1000.0;
            timing->ks2 = timing->ks * 0.5;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.6 * sample_rate / 1000.0;
            timing->sg = timing->ks + 0.1;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 0.1;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 496;
            break;
        case SSTV_R24:
            timing->ks = 92.0 * sample_rate / 1000.0;
            timing->ks2 = 46.0 * sample_rate / 1000.0;
            timing->of = 8.0 * sample_rate / 1000.0;
            timing->ofp = 8.1 * sample_rate / 1000.0;
            timing->sg = timing->ks + 4.0 * sample_rate / 1000.0;
            timing->cg = timing->sg + timing->ks2;
            timing->sb = timing->cg + 4.0 * sample_rate / 1000.0;
            timing->cb = timing->sb + timing->ks2;
            timing->line_count = 120;
            break;
        case SSTV_BW8:
            timing->ks = 58.89709 * sample_rate / 1000.0;
            timing->of = 8.0 * sample_rate / 1000.0;
            timing->ofp = 8.2 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 120;
            break;
        case SSTV_BW12:
            timing->ks = 92.0 * sample_rate / 1000.0;
            timing->of = 8.0 * sample_rate / 1000.0;
            timing->ofp = 8.0 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 120;
            break;
        case SSTV_MN110:
            timing->ks = 212.0 * sample_rate / 1000.0;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.5 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 128;
            break;
        case SSTV_MN140:
            timing->ks = 270.0 * sample_rate / 1000.0;
            timing->of = 10.0 * sample_rate / 1000.0;
            timing->ofp = 10.5 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 128;
            break;
        case SSTV_MC110:
            timing->ks = 140.0 * sample_rate / 1000.0;
            timing->of = 8.0 * sample_rate / 1000.0;
            timing->ofp = 8.95 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_MC140:
            timing->ks = 180.0 * sample_rate / 1000.0;
            timing->of = 8.0 * sample_rate / 1000.0;
            timing->ofp = 8.75 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_MC180:
            timing->ks = 232.0 * sample_rate / 1000.0;
            timing->of = 8.0 * sample_rate / 1000.0;
            timing->ofp = 8.75 * sample_rate / 1000.0;
            timing->sg = timing->ks;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
        case SSTV_SCOTTIE1:
        default:
            timing->ks = 138.24 * sample_rate / 1000.0;
            timing->of = 10.5 * sample_rate / 1000.0;
            timing->ofp = 10.7 * sample_rate / 1000.0;
            timing->sg = 139.74 * sample_rate / 1000.0;
            timing->cg = timing->ks + timing->sg;
            timing->sb = timing->sg + timing->sg;
            timing->cb = timing->ks + timing->sb;
            timing->line_count = 256;
            break;
    }

    timing->line_ms = get_line_ms(mode);
    timing->line_samples = timing->line_ms * sample_rate / 1000.0;

    switch (mode) {
        case SSTV_PD120:
        case SSTV_PD160:
        case SSTV_PD180:
        case SSTV_PD240:
        case SSTV_PD290:
        case SSTV_P3:
        case SSTV_P5:
        case SSTV_P7:
            timing->kss = timing->ks - timing->ks / 480.0;
            timing->ks2s = timing->ks2 - timing->ks2 / 480.0;
            timing->ksb = timing->kss / 1280.0;
            break;
        case SSTV_MP73:
        case SSTV_MN73:
        case SSTV_SCOTTIEX:
            timing->kss = timing->ks - timing->ks / 1280.0;
            timing->ks2s = timing->ks2 - timing->ks2 / 1280.0;
            timing->ksb = timing->kss / 1280.0;
            break;
        case SSTV_SC2_180:
        case SSTV_MP115:
        case SSTV_MP140:
        case SSTV_MP175:
        case SSTV_MR90:
        case SSTV_MR115:
        case SSTV_MR140:
        case SSTV_MR175:
        case SSTV_ML180:
        case SSTV_ML240:
        case SSTV_ML280:
        case SSTV_ML320:
        case SSTV_MN110:
        case SSTV_MN140:
        case SSTV_MC110:
        case SSTV_MC140:
        case SSTV_MC180:
            timing->kss = timing->ks;
            timing->ks2s = timing->ks2;
            timing->ksb = timing->kss / 1280.0;
            break;
        case SSTV_MR73:
            timing->kss = timing->ks - timing->ks / 640.0;
            timing->ks2s = timing->ks2 - timing->ks2 / 1024.0;
            timing->ksb = timing->kss / 1024.0;
            break;
        default:
            timing->kss = timing->ks - timing->ks / 240.0;
            timing->ks2s = timing->ks2 - timing->ks2 / 240.0;
            timing->ksb = timing->kss / 640.0;
            break;
    }

    if (timing->ksb == 0.0) {
        timing->ksb = 1.0;
    }
}

/* Internal encoder structure */
struct sstv_encoder_s {
    sstv_mode_t mode;
    double sample_rate;
    const sstv_image_t *image;
    int vis_enabled;

    ModeTiming timing;

    /* State */
    size_t samples_generated;
    size_t total_samples;
    int complete;

    VCO vco;
    VISEncoder vis;
    int vis_active;
    int preamble_enabled;
    int stage;

    size_t timed_line;
    size_t image_line;
    size_t total_timed_lines;

    std::vector<Segment> segments;
    size_t segment_index;
    size_t segment_offset;
    double segment_fraction;
};

static void recompute_total_samples(sstv_encoder_t *enc) {
    if (!enc) return;
    enc->total_samples = 0;
    if (enc->timing.line_count > 0 && enc->timing.line_ms > 0.0) {
        double total_ms = enc->timing.line_ms * enc->timing.line_count;
        enc->total_samples = (size_t)((total_ms / 1000.0) * enc->sample_rate);
    } else {
        const sstv_mode_info_t *info = sstv_get_mode_info(enc->mode);
        if (info) {
            enc->total_samples = (size_t)(info->duration_sec * enc->sample_rate);
        }
    }
    if (enc->vis_enabled) {
        const sstv_mode_info_t *info = sstv_get_mode_info(enc->mode);
        if (info && info->vis_code != 0x00) {
            // Standard 8-bit VIS: 300ms leader + 10ms break + 300ms leader +
            // 30ms start bit + 8×30ms data bits + 30ms stop bit = 910ms
            // 16-bit VIS (MR/MP/ML): adds 8 more data bits + 2 parity bits = 1210ms
            uint16_t vis_word = get_mmsstv_vis_word(enc->mode);
            double vis_duration = (vis_word != 0x0000) ? 1.210 : 0.910;
            enc->total_samples += (size_t)(vis_duration * enc->sample_rate);
        }
    }
    if (enc->preamble_enabled) {
        enc->total_samples += (size_t)(get_preamble_ms(enc->mode) * enc->sample_rate / 1000.0);
    }
}

static void push_segment_ms(sstv_encoder_t *enc, double freq, double ms) {
    if (!enc || ms <= 0.0) return;
    double exact = ms * enc->sample_rate / 1000.0;
    double total = exact + enc->segment_fraction;
    size_t samples = (size_t)(total);
    enc->segment_fraction = total - (double)samples;
    if (samples == 0) return;
    Segment seg;
    seg.freq = freq;
    seg.samples = samples;
    enc->segments.push_back(seg);
}

static void write_preamble(sstv_encoder_t *enc) {
    if (!enc) return;
    if (is_narrow_mode(enc->mode)) {
        push_segment_ms(enc, 1900, 100.0);
        push_segment_ms(enc, 2300, 100.0);
        push_segment_ms(enc, 1900, 100.0);
        push_segment_ms(enc, 2300, 100.0);
        return;
    }
    push_segment_ms(enc, 1900, 100.0);
    push_segment_ms(enc, 1500, 100.0);
    push_segment_ms(enc, 1900, 100.0);
    push_segment_ms(enc, 1500, 100.0);
    push_segment_ms(enc, 2300, 100.0);
    push_segment_ms(enc, 1500, 100.0);
    push_segment_ms(enc, 2300, 100.0);
    push_segment_ms(enc, 1500, 100.0);
}

static void write_mmsstv_vis(sstv_encoder_t *enc, uint16_t vis_word) {
    if (!enc) return;
    // 16-bit VIS: MMSSTV uses standard VIS leader/break/leader/start,
    // then 16 data bits LSB-first at 30ms each, then stop bit 1200Hz 30ms.
    push_segment_ms(enc, 1900, 300.0);
    push_segment_ms(enc, 1200, 10.0);
    push_segment_ms(enc, 1900, 300.0);
    push_segment_ms(enc, 1200, 30.0);
    for (int i = 0; i < 16; i++) {
        /* MMSSTV: bit 1 = 1080 Hz, bit 0 = 1320 Hz */
        double freq = (vis_word & 0x0001) ? 1080.0 : 1320.0;
        push_segment_ms(enc, freq, 30.0);
        vis_word >>= 1;
    }
    push_segment_ms(enc, 1200, 30.0);
}

static void write_line_r24(sstv_encoder_t *enc) {
    int width = (int)enc->image->width;
    std::vector<int> ry(width);
    std::vector<int> by(width);
    push_segment_ms(enc, 1200, 6.0);
    push_segment_ms(enc, 1500, 2.0);
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        ry[x] = ryy;
        by[x] = byy;
        push_segment_ms(enc, (double)color_to_freq(y), 92.0 / 320.0);
    }
    push_segment_ms(enc, 1500, 3.0);
    push_segment_ms(enc, 1900, 1.0);
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq(ry[x]), 46.0 / 320.0);
    }
    push_segment_ms(enc, 2300, 3.0);
    push_segment_ms(enc, 1900, 1.0);
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq(by[x]), 46.0 / 320.0);
    }
}

static void write_line_r36(sstv_encoder_t *enc) {
    int width = (int)enc->image->width;
    std::vector<int> ry(width);
    std::vector<int> by(width);
    push_segment_ms(enc, 1200, 9.0);
    push_segment_ms(enc, 1500, 3.0);
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        ry[x] = ryy;
        by[x] = byy;
        push_segment_ms(enc, (double)color_to_freq(y), 88.0 / 320.0);
    }
    push_segment_ms(enc, (enc->image_line & 1) ? 2300.0 : 1500.0, 4.5);
    push_segment_ms(enc, 1900, 1.5);
    for (int x = 0; x < width; x++) {
        int y = (enc->image_line & 1) ? by[x] : ry[x];
        push_segment_ms(enc, (double)color_to_freq(y), 44.0 / 320.0);
    }
}

static void write_line_r72(sstv_encoder_t *enc) {
    int width = (int)enc->image->width;
    std::vector<int> ry(width);
    std::vector<int> by(width);
        push_segment_ms(enc, 1200, 9.0);
        if (is_mmsstv_vis_mode(enc->mode)) {
            write_mmsstv_vis(enc, get_mmsstv_vis_word(enc->mode));
        }
    push_segment_ms(enc, 1500, 3.0);
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        ry[x] = ryy;
        by[x] = byy;
        push_segment_ms(enc, (double)color_to_freq(y), 138.0 / 320.0);
    }
    push_segment_ms(enc, 1500, 4.5);
    push_segment_ms(enc, 1900, 1.5);
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq(ry[x]), 69.0 / 320.0);
    }
    push_segment_ms(enc, 2300, 4.5);
    push_segment_ms(enc, 1900, 1.5);
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq(by[x]), 69.0 / 320.0);
    }
}

static void write_line_avt(sstv_encoder_t *enc) {
    int width = (int)enc->image->width;
    double tw = 125.0 / 320.0;
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(r), tw);
    }
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(g), tw);
    }
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(b), tw);
    }
}

static void write_line_sct(sstv_encoder_t *enc, double tw) {
    int width = (int)enc->image->width;
    double t = tw / 320.0;
    push_segment_ms(enc, 1500, 1.5);
    for (int x = 0; x < 320; x++) {
        int r, g, b;
        int px = (x * width) / 320;
        get_pixel_rgb(enc->image, px, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(g), t);
    }
    push_segment_ms(enc, 1500, 1.5);
    for (int x = 0; x < 320; x++) {
        int r, g, b;
        int px = (x * width) / 320;
        get_pixel_rgb(enc->image, px, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(b), t);
    }
    push_segment_ms(enc, 1200, 9.0);
    push_segment_ms(enc, 1500, 1.5);
    for (int x = 0; x < 320; x++) {
        int r, g, b;
        int px = (x * width) / 320;
        get_pixel_rgb(enc->image, px, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(r), t);
    }
}

static void write_line_mrt(sstv_encoder_t *enc, double tw) {
    int width = (int)enc->image->width;
    /* Martin modes encode exactly 320 pixels */
    double t = tw / 320.0;
    
    push_segment_ms(enc, 1200, 4.862);
    push_segment_ms(enc, 1500, 0.572);
    /* Green channel */
    for (int x = 0; x < 320; x++) {
        int r, g, b;
        int px = (x * width) / 320;
        get_pixel_rgb(enc->image, px, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(g), t);
    }
    push_segment_ms(enc, 1500, 0.572);
    /* Blue channel */
    for (int x = 0; x < 320; x++) {
        int r, g, b;
        int px = (x * width) / 320;
        get_pixel_rgb(enc->image, px, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(b), t);
    }
    push_segment_ms(enc, 1500, 0.572);
    /* Red channel */
    for (int x = 0; x < 320; x++) {
        int r, g, b;
        int px = (x * width) / 320;
        get_pixel_rgb(enc->image, px, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(r), t);
    }
    push_segment_ms(enc, 1500, 0.572);  /* Trailing separator per MMSSTV */
}

static void write_line_sc2(sstv_encoder_t *enc, double s, double tw) {
    /* SC2 modes: Per MMSSTV, always use 320 pixel iteration regardless of actual image width
       The tw parameter is pre-adjusted for this fixed 320-pixel assumption */
    double t = tw / 320.0;  /* Always divide by 320, matching MMSSTV behavior */
    int width = (int)enc->image->width;
    
    push_segment_ms(enc, 1200, s);
    push_segment_ms(enc, 1500, 0.5);
    /* Red channel - always encode exactly 320 pixels */
    for (int x = 0; x < 320; x++) {
        int r, g, b;
        int px = (x * width) / 320;  /* Scale image position to fit 320 pixels */
        get_pixel_rgb(enc->image, px, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(r), t);
    }
    /* Green channel */
    for (int x = 0; x < 320; x++) {
        int r, g, b;
        int px = (x * width) / 320;
        get_pixel_rgb(enc->image, px, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(g), t);
    }
    /* Blue channel */
    for (int x = 0; x < 320; x++) {
        int r, g, b;
        int px = (x * width) / 320;
        get_pixel_rgb(enc->image, px, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(b), t);
    }
}

static void write_line_pd(sstv_encoder_t *enc, double tw) {
    int width = (int)enc->image->width;
    std::vector<int> ry(width);
    std::vector<int> by(width);
    push_segment_ms(enc, 1200, 20.000);
    push_segment_ms(enc, 1500, 2.080);
    double t = tw / (double)width;
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        ry[x] = ryy;
        by[x] = byy;
        push_segment_ms(enc, (double)color_to_freq(y), t);
    }
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq(ry[x]), t);
    }
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq(by[x]), t);
    }
    size_t next_line = enc->image_line + 1;
    if (next_line >= enc->image->height) next_line = enc->image->height - 1;
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)next_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        push_segment_ms(enc, (double)color_to_freq(y), t);
    }
}

static void write_line_p(sstv_encoder_t *enc, double s, double p, double c) {
    int width = (int)enc->image->width;
    double t = c / 640.0;
    push_segment_ms(enc, 1200, s);
    push_segment_ms(enc, 1500, p);
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(r), t);
    }
    push_segment_ms(enc, 1500, p);
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(g), t);
    }
    push_segment_ms(enc, 1500, p);
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq(b), t);
    }
    push_segment_ms(enc, 1500, p);
}

static void write_line_mp(sstv_encoder_t *enc, double tw) {
    int width = (int)enc->image->width;
    std::vector<int> ry(width);
    std::vector<int> by(width);
    push_segment_ms(enc, 1200, 9.0);
    push_segment_ms(enc, 1500, 1.0);
    double t = tw / (double)width;
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        ry[x] = ryy;
        by[x] = byy;
        push_segment_ms(enc, (double)color_to_freq(y), t);
    }
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq(ry[x]), t);
    }
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq(by[x]), t);
    }
    size_t next_line = enc->image_line + 1;
    if (next_line >= enc->image->height) next_line = enc->image->height - 1;
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)next_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        push_segment_ms(enc, (double)color_to_freq(y), t);
    }
}

static void write_line_mr(sstv_encoder_t *enc, double tw) {
    int width = (int)enc->image->width;
    std::vector<int> ry(width);
    std::vector<int> by(width);
    push_segment_ms(enc, 1200, 9.0);
    push_segment_ms(enc, 1500, 1.0);
    double ty = tw / (double)width;
    double tc = ty / 2.0;
    int last_freq = 1500;
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        ry[x] = ryy;
        by[x] = byy;
        last_freq = color_to_freq(y);
        push_segment_ms(enc, (double)last_freq, ty);
    }
    push_segment_ms(enc, (double)last_freq, 0.1);
    for (int x = 0; x < width; x++) {
        last_freq = color_to_freq(ry[x]);
        push_segment_ms(enc, (double)last_freq, tc);
    }
    push_segment_ms(enc, (double)last_freq, 0.1);
    for (int x = 0; x < width; x++) {
        last_freq = color_to_freq(by[x]);
        push_segment_ms(enc, (double)last_freq, tc);
    }
    push_segment_ms(enc, (double)last_freq, 0.1);
}

static void write_line_rm(sstv_encoder_t *enc, double ts, double tw) {
    int width = (int)enc->image->width;
    std::vector<int> yline(width);
    push_segment_ms(enc, 1200, ts);
    push_segment_ms(enc, 1500, ts / 3.0);
    double t = tw / (double)width;
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        yline[x] = y;
    }
    size_t next_line = enc->image_line + 1;
    if (next_line >= enc->image->height) next_line = enc->image->height - 1;
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)next_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        int yy = (y + yline[x]) / 2;
        push_segment_ms(enc, (double)color_to_freq(yy), t);
    }
}

static void write_line_mn(sstv_encoder_t *enc, double tw) {
    int width = (int)enc->image->width;
    std::vector<int> ry(width);
    std::vector<int> by(width);
    push_segment_ms(enc, NARROW_SYNC, 9.0);
    push_segment_ms(enc, NARROW_LOW, 1.0);
    double t = tw / (double)width;
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        ry[x] = ryy;
        by[x] = byy;
        push_segment_ms(enc, (double)color_to_freq_narrow(y), t);
    }
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq_narrow(ry[x]), t);
    }
    for (int x = 0; x < width; x++) {
        push_segment_ms(enc, (double)color_to_freq_narrow(by[x]), t);
    }
    size_t next_line = enc->image_line + 1;
    if (next_line >= enc->image->height) next_line = enc->image->height - 1;
    for (int x = 0; x < width; x++) {
        int r, g, b, y, ryy, byy;
        get_pixel_rgb(enc->image, x, (int)next_line, r, g, b);
        get_ry(r, g, b, y, ryy, byy);
        push_segment_ms(enc, (double)color_to_freq_narrow(y), t);
    }
}

static void write_line_mc(sstv_encoder_t *enc, double tw) {
    int width = (int)enc->image->width;
    double t = tw / 320.0;
    push_segment_ms(enc, NARROW_SYNC, 8.0);
    push_segment_ms(enc, NARROW_LOW, 0.5);
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq_narrow(r), t);
    }
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq_narrow(g), t);
    }
    for (int x = 0; x < width; x++) {
        int r, g, b;
        get_pixel_rgb(enc->image, x, (int)enc->image_line, r, g, b);
        push_segment_ms(enc, (double)color_to_freq_narrow(b), t);
    }
}

static bool generate_next_line_segments(sstv_encoder_t *enc) {
    if (!enc || !enc->image) return false;
    if (enc->timed_line >= enc->total_timed_lines) return false;

    enc->segments.clear();
    enc->segment_index = 0;
    enc->segment_offset = 0;

    if (enc->timed_line == 0) {
        switch (enc->mode) {
            case SSTV_SCOTTIE1:
            case SSTV_SCOTTIE2:
            case SSTV_SCOTTIEX:
                push_segment_ms(enc, 1200, 9.0);
                break;
            default:
                break;
        }
    }

    switch (enc->mode) {
        case SSTV_R24:
            write_line_r24(enc);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_R36:
            write_line_r36(enc);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_R72:
            write_line_r72(enc);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_AVT90:
            write_line_avt(enc);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_SCOTTIE1:
            write_line_sct(enc, 138.24);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_SCOTTIE2:
            write_line_sct(enc, 88.064);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_SCOTTIEX:
            write_line_sct(enc, 345.6);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MARTIN1:
            write_line_mrt(enc, 146.432);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MARTIN2:
            write_line_mrt(enc, 73.216);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_SC2_180:
            write_line_sc2(enc, 5.5437, 235.0);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_SC2_120:
            write_line_sc2(enc, 5.52248, 156.5);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_SC2_60:
            write_line_sc2(enc, 5.5006, 78.128);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_PD50:
            write_line_pd(enc, 91.520);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_PD90:
            write_line_pd(enc, 170.240);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_PD120:
            write_line_pd(enc, 121.600);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_PD160:
            write_line_pd(enc, 195.584);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_PD180:
            write_line_pd(enc, 183.040);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_PD240:
            write_line_pd(enc, 244.480);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_PD290:
            write_line_pd(enc, 228.800);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_P3:
            write_line_p(enc, 5.208, 1.042, 133.333);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_P5:
            write_line_p(enc, 7.813, 1.562375, 200.000);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_P7:
            write_line_p(enc, 10.417, 2.083, 266.667);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MR73:
            write_line_mr(enc, 138.0);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MR90:
            write_line_mr(enc, 171.0);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MR115:
            write_line_mr(enc, 220.0);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MR140:
            write_line_mr(enc, 269.0);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MR175:
            write_line_mr(enc, 337.0);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MP73:
            write_line_mp(enc, 140.0);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_MP115:
            write_line_mp(enc, 223.0);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_MP140:
            write_line_mp(enc, 270.0);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_MP175:
            write_line_mp(enc, 340.0);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_ML180:
            write_line_mr(enc, 176.5);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_ML240:
            write_line_mr(enc, 236.5);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_ML280:
            write_line_mr(enc, 277.5);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_ML320:
            write_line_mr(enc, 317.5);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_BW8:
            write_line_rm(enc, 6.0, 58.89709);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_BW12:
            write_line_rm(enc, 6.0, 92.0);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_MN73:
            write_line_mn(enc, 140.0);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_MN110:
            write_line_mn(enc, 212.0);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_MN140:
            write_line_mn(enc, 270.0);
            enc->timed_line += 1;
            enc->image_line += 2;
            break;
        case SSTV_MC110:
            write_line_mc(enc, 140.0);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MC140:
            write_line_mc(enc, 180.0);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        case SSTV_MC180:
            write_line_mc(enc, 232.0);
            enc->timed_line += 1;
            enc->image_line += 1;
            break;
        default:
            return false;
    }
    return true;
}

sstv_encoder_t* sstv_encoder_create(sstv_mode_t mode, double sample_rate) {
    if (mode < 0 || mode >= SSTV_MODE_COUNT) {
        return NULL;
    }
    
    sstv_encoder_t *enc = (sstv_encoder_t*)calloc(1, sizeof(sstv_encoder_t));
    if (!enc) return NULL;
    
    enc->mode = mode;
    enc->sample_rate = sample_rate;
    enc->vis_enabled = 1;  /* Enabled by default */
    enc->complete = 0;
    enc->samples_generated = 0;

    compute_mode_timing(mode, sample_rate, &enc->timing);

    new (&enc->vco) VCO(sample_rate);
    new (&enc->vis) VISEncoder();
    enc->vco.setFreeFreq(1080.0);  /* MMSSTV base frequency */
    enc->vco.setGain(1220.0);       /* Span to 2300 Hz (1080 + 1220) */
    enc->vis_active = 0;
    enc->preamble_enabled = 1;
    enc->stage = 0;

    enc->timed_line = 0;
    enc->image_line = 0;
    enc->total_timed_lines = (size_t)enc->timing.line_count;
    enc->segment_index = 0;
    enc->segment_offset = 0;
    enc->segment_fraction = 0.0;

    recompute_total_samples(enc);
    
    return enc;
}

void sstv_encoder_free(sstv_encoder_t *encoder) {
    if (encoder) {
        encoder->vco.~VCO();
        encoder->vis.~VISEncoder();
        free(encoder);
    }
}

int sstv_encoder_set_image(sstv_encoder_t *encoder, const sstv_image_t *image) {
    if (!encoder || !image) return -1;
    
    /* Verify image dimensions match mode */
    const sstv_mode_info_t *info = sstv_get_mode_info(encoder->mode);
    if (!info) return -1;
    
    if (image->width != info->width || image->height != info->height) {
        return -1;  /* Image size mismatch */
    }
    
    encoder->image = image;
    encoder->total_timed_lines = (size_t)encoder->timing.line_count;
    return 0;
}

void sstv_encoder_set_vis_enabled(sstv_encoder_t *encoder, int enable) {
    if (encoder) {
        encoder->vis_enabled = enable ? 1 : 0;
        recompute_total_samples(encoder);
    }
}

size_t sstv_encoder_generate(sstv_encoder_t *encoder, float *samples, size_t max_samples) {
    if (!encoder || !samples || encoder->complete) {
        return 0;
    }

    if (!encoder->image) {
        return 0;
    }

    if (encoder->samples_generated == 0) {
        encoder->segment_fraction = 0.0;
        encoder->segments.clear();
        encoder->segment_index = 0;
        encoder->segment_offset = 0;
        encoder->timed_line = 0;
        encoder->stage = encoder->preamble_enabled ? 0 : 1;
        encoder->image_line = 0;
        encoder->total_timed_lines = (size_t)encoder->timing.line_count;

        const sstv_mode_info_t *info = sstv_get_mode_info(encoder->mode);
        if (encoder->vis_enabled && info && info->vis_code != 0x00 && !is_narrow_mode(encoder->mode)) {
            // Check if this is a 16-bit VIS mode (MR/MP/ML)
            uint16_t vis_word = get_mmsstv_vis_word(encoder->mode);
            if (vis_word != 0x0000) {
                // 16-bit VIS for MR/MP/ML modes
                encoder->vis.start_16bit(vis_word, encoder->sample_rate);
            } else {
                // Standard 8-bit VIS
                encoder->vis.start(info->vis_code, encoder->sample_rate);
            }
            encoder->vis_active = 1;
        } else {
            encoder->vis_active = 0;
        }
        if (!encoder->vis_active && encoder->stage == 1) {
            encoder->stage = 2;
        }
    }

    size_t produced = 0;
    while (produced < max_samples) {
        if (encoder->stage == 0) {
            if (encoder->segments.empty()) {
                write_preamble(encoder);
            }
        } else if (encoder->stage == 1) {
            if (encoder->vis_active) {
                double fq = encoder->vis.get_frequency();
                if (fq <= 0.0) {
                    encoder->vis_active = 0;
                    encoder->stage = 2;
                    continue;
                }
                /* Normalize frequency to VCO input range: 1080-2300 Hz */
                double norm = (fq - 1080.0) / 1220.0;
                if (norm < 0.0) norm = 0.0;
                if (norm > 1.0) norm = 1.0;
                
                samples[produced++] = (float)encoder->vco.process(norm);
                encoder->samples_generated++;
                continue;
            }
            encoder->stage = 2;
            continue;
        }

        if (encoder->stage == 0 && encoder->segment_index >= encoder->segments.size()) {
            encoder->stage = encoder->vis_active ? 1 : 2;
            encoder->segments.clear();
            encoder->segment_index = 0;
            encoder->segment_offset = 0;
            continue;
        }

        if (encoder->stage == 2 && encoder->segment_index >= encoder->segments.size()) {
            if (!generate_next_line_segments(encoder)) {
                encoder->complete = 1;
                break;
            }
        }

        if (encoder->segment_index >= encoder->segments.size()) {
            continue;
        }

        Segment &seg = encoder->segments[encoder->segment_index];
        if (encoder->segment_offset >= seg.samples) {
            encoder->segment_index++;
            encoder->segment_offset = 0;
            continue;
        }

        double fq = seg.freq;
        float out = 0.0f;
        if (fq > 0.0) {
            double norm = (fq - 1100.0) / 1200.0;
            if (norm < 0.0) norm = 0.0;
            if (norm > 1.0) norm = 1.0;
            
            out = (float)encoder->vco.process(norm);
        }
        samples[produced++] = out;
        encoder->samples_generated++;
        encoder->segment_offset++;
    }

    return produced;
}

int sstv_encoder_is_complete(sstv_encoder_t *encoder) {
    return encoder ? encoder->complete : 1;
}

float sstv_encoder_get_progress(sstv_encoder_t *encoder) {
    if (!encoder || encoder->total_samples == 0) return 0.0f;
    return (float)encoder->samples_generated / (float)encoder->total_samples;
}

void sstv_encoder_reset(sstv_encoder_t *encoder) {
    if (encoder) {
        encoder->samples_generated = 0;
        encoder->complete = 0;
        encoder->segments.clear();
        encoder->segment_index = 0;
        encoder->segment_offset = 0;
        encoder->segment_fraction = 0.0;
        encoder->timed_line = 0;
        encoder->image_line = 0;
        encoder->vis_active = 0;
        encoder->stage = encoder->preamble_enabled ? 0 : 1;
    }
}

size_t sstv_encoder_get_total_samples(sstv_encoder_t *encoder) {
    return encoder ? encoder->total_samples : 0;
}
