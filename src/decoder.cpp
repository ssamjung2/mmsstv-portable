/*
 * SSTV Decoder (RX) - Portable implementation
 *
 * Demodulation pipeline:
 *   1. FM demod: BPF → CIIRTANK (tone detection) → AGC
 *   2. Sync detect: CIIRTANK at 1200 Hz → state machine
 *   3. VIS decode: Sync interrupts → bit-by-bit accumulation
 *   4. Image buffer: Line accumulation → RGB24 output
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <vector>

#include "sstv_decoder.h"
#include "dsp_filters.h"

/* === WAV FILE HELPERS === */
static void write_u16_le(FILE *f, uint16_t val) {
    uint8_t b[2] = { (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF) };
    fwrite(b, 1, 2, f);
}

static void write_u32_le(FILE *f, uint32_t val) {
    uint8_t b[4] = { (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF),
                     (uint8_t)((val >> 16) & 0xFF), (uint8_t)((val >> 24) & 0xFF) };
    fwrite(b, 1, 4, f);
}

static void write_wav_header_placeholder(FILE *f, uint32_t sample_rate) {
    /* Write header with 0 samples, will be updated on close */
    fwrite("RIFF", 1, 4, f);
    write_u32_le(f, 0);  /* RIFF size (will update) */
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    write_u32_le(f, 16);  /* PCM chunk size */
    write_u16_le(f, 1);   /* PCM format */
    write_u16_le(f, 1);   /* Mono */
    write_u32_le(f, sample_rate);
    write_u32_le(f, sample_rate * 2);  /* Byte rate */
    write_u16_le(f, 2);   /* Block align */
    write_u16_le(f, 16);  /* Bits per sample */
    fwrite("data", 1, 4, f);
    write_u32_le(f, 0);  /* Data size (will update) */
}

static void update_wav_header(FILE *f, uint32_t sample_rate, uint32_t num_samples) {
    uint32_t data_bytes = num_samples * sizeof(int16_t);
    uint32_t riff_size = 36 + data_bytes;
    
    /* Update RIFF size */
    fseek(f, 4, SEEK_SET);
    write_u32_le(f, riff_size);
    
    /* Update data chunk size */
    fseek(f, 40, SEEK_SET);
    write_u32_le(f, data_bytes);
}

static void write_sample_to_wav(FILE *f, double sample) {
    /* Clamp and convert double to int16 */
    if (sample > 32767.0) sample = 32767.0;
    if (sample < -32768.0) sample = -32768.0;
    int16_t pcm = (int16_t)sample;
    write_u16_le(f, (uint16_t)pcm);
}

/* === SYNC DETECTOR STATE MACHINE === */
typedef enum {
    SYNC_IDLE = 0,           /* Waiting for sync pulse */
    SYNC_DETECTED = 1,       /* Sync pulse in progress */
    SYNC_VIS_WAITING = 2,    /* VIS header expected */
    SYNC_VIS_DECODING = 3,   /* Decoding VIS bits */
    SYNC_DATA_WAIT = 4       /* Image data coming */
} sync_state_t;

/* === IMAGE DECODER STATE MACHINE === */
typedef enum {
    IMAGE_IDLE = 0,          /* Not decoding image */
    IMAGE_SYNC_WAIT = 1,     /* Waiting for line sync pulse */
    IMAGE_DECODE_R = 2,      /* Decoding red channel */
    IMAGE_DECODE_G = 3,      /* Decoding green channel */
    IMAGE_DECODE_B = 4,      /* Decoding blue channel */
    IMAGE_DECODE_Y = 5,      /* Decoding luminance (Y) */
    IMAGE_DECODE_RY = 6,     /* Decoding R-Y chroma */
    IMAGE_DECODE_BY = 7,     /* Decoding B-Y chroma */
    IMAGE_COMPLETE = 8       /* Image fully decoded */
} image_decode_state_t;

/* How the current image was started (sets the delay to line 0) */
enum {
    IMAGE_START_HINT = 0,    /* mode hint, no header: audio starts at the image */
    IMAGE_START_VIS  = 1,    /* VIS decoded */
    IMAGE_START_NVIS = 2     /* narrow-mode N-VIS decoded */
};

/* === VIS DECODER === */
typedef struct {
    int bit_count;           /* 0-7 for 7-bit VIS, 0-15 for 16-bit */
    uint16_t data;           /* Accumulated bits */
    int is_16bit;            /* 1 if 16-bit VIS detected */
    int bit_timer;           /* Samples remaining in current bit (30ms per bit) */
    double mark_accum;       /* Accumulated mark energy during bit period */
    double space_accum;      /* Accumulated space energy during bit period */
    int sample_count;        /* Samples accumulated in bit period */
    int start_bit_samples;   /* Samples accumulated for VIS start bit detection */
    int start_bit_pending;   /* 1 if next bit is the VIS start bit */
    /* Buffered VIS decoding */
    double *mark_buf;        /* Mark energy buffer */
    double *space_buf;       /* Space energy buffer */
    int buf_size;            /* Buffer size in samples */
    int buf_pos;             /* Current buffer write position */
    int buffering;           /* 1 while buffering VIS window */
    int invert_polarity;     /* 1 if space > mark consistently (inverted polarity) */
    int polarity_samples;    /* Number of samples used for polarity detection */
} vis_decoder_t;

/* === LEVEL/AGC (ported from MMSSTV CLVL) === */
typedef struct {
    double m_Cur;
    double m_PeakMax;
    double m_PeakAGC;
    double m_Peak;
    double m_CurMax;
    double m_Max;
    double m_agc;
    int m_CntPeak;
    int m_agcfast;
    int m_Cnt;
    int m_CntMax;
} level_agc_t;

/* === IMAGE BUFFER === */
typedef struct {
    uint8_t *pixels;         /* RGB24 or grayscale */
    int width, height;
    int bytes_per_pixel;     /* 1 for grayscale, 3 for RGB */
    int current_line;        /* Current line being filled */
    int current_col;         /* Current column in line */
} image_buffer_t;

/* === SCAN LINE COLOR TYPES (derived from MMSSTV DrawSSTVNormal switch) === */
typedef enum {
    SCAN_RGB   = 0,  /* ch0=R, ch1=G, ch2=B sequential (Scottie, SC2, P*, MC*, AVT) */
    SCAN_MRT   = 1,  /* ch0=G, ch1=B, ch2=R order (MRT1/2) */
    SCAN_YC    = 2,  /* ch0=Y, ch1=R-Y, ch2=B-Y → YCtoRGB; ch1/ch2 mapped via KS2S */
    SCAN_YC_PD = 3,  /* ch0=Y_odd, ch1=R-Y, ch2=B-Y, ch3=Y_even → 2 image rows (PD*, MP*, MN*) */
    SCAN_BW    = 4,  /* ch0=Y only → grayscale, row_double rows (RM8, RM12) */
    SCAN_R36   = 5,  /* Robot 36: ch0=Y, ch2=R-Y or B-Y (4:2:0, chosen by separator tone) */
} scan_color_type_t;

/*
 * Per-mode scan line timing in milliseconds.
 * All channel offsets (sg, cg, sb, cb) are measured RELATIVE TO END OF SYNC
 * (matching the `ps -= m_OF` step in MMSSTV DrawSSTVNormal).
 * Derived from CSSTVSET::SetSampFreq() and GetTiming() in sstv.cpp.
 */
typedef struct {
    double tw_ms;           /* total scan line width (ms) */
    double of_ms;           /* sync/guard at line start (ms) */
    double ofp_ms;          /* expected d12 peak pos in sync region (MMSSTV m_OFP) */
    double ks_ms;           /* ch0 active width (ms) */
    double sg_ms;           /* ch1 start from post-sync (ms) */
    double cg_ms;           /* ch1 end */
    double sb_ms;           /* ch2 start */
    double cb_ms;           /* ch2 end */
    double ks2_ms;          /* chroma half-width for SCAN_YC modes (ms); 0 if n/a */
    int    kss_div;         /* KSS = KS*(1 - 1/kss_div); 0 means KSS = KS exactly */
    int    row_double;      /* 1 = single scan line produces 2 image rows (R24, RM*) */
    scan_color_type_t color_type;
} scan_timing_ms_t;

/*
 * Lookup table indexed by sstv_mode_t (0 … SSTV_MODE_COUNT-1).
 * Values extracted from MMSSTV CSSTVSET::SetSampFreq() + GetTiming().
 *
 * Column order: tw_ms, of_ms, ofp_ms, ks_ms, sg_ms, cg_ms, sb_ms, cb_ms,
 *               ks2_ms, kss_div, row_double, color_type
 */
static const scan_timing_ms_t SCAN_TIMING[SSTV_MODE_COUNT] = {
/*         tw_ms      of_ms  ofp_ms    ks_ms      sg_ms      cg_ms      sb_ms      cb_ms     ks2_ms kss row_d color */
/* R36   */ { 150.000,  12.000, 10.700,  88.000,  89.250,  91.500,  94.000, 138.000, 44.00, 240, 0, SCAN_R36   },
/* R72   */ { 300.000,  12.000, 10.700, 138.000, 144.000, 213.000, 219.000, 288.000, 69.00, 240, 0, SCAN_YC    },
/* AVT90 */ { 375.000,   0.000,  0.000, 125.000, 125.000, 250.000, 250.000, 375.000,  0.00, 240, 0, SCAN_RGB   },
/* SCT1  */ { 428.220,  10.500, 10.700, 138.240, 139.740, 277.980, 279.480, 417.720,  0.00, 240, 0, SCAN_RGB   },
/* SCT2  */ { 277.692,  10.500, 10.800,  88.064,  89.564, 177.628, 179.128, 267.192,  0.00, 240, 0, SCAN_RGB   },
/* SCTDX */ {1050.300,  10.500, 10.200, 345.600, 347.100, 692.700, 694.200,1039.800,  0.00,1280, 0, SCAN_RGB   },
/* MRT1  */ { 446.446,   5.434,  7.200, 146.432, 147.004, 293.436, 294.008, 440.440,  0.00, 240, 0, SCAN_MRT   },
/* MRT2  */ { 226.798,   5.434,  7.400,  73.216,  73.788, 147.004, 147.576, 220.792,  0.00, 240, 0, SCAN_MRT   },
/* SC2_180*/{711.044,   6.044,  7.800, 235.000, 235.000, 470.000, 470.000, 705.000,  0.00,   0, 0, SCAN_RGB   },
/* SC2_120*/{475.523,   6.022,  7.500, 156.500, 156.500, 313.000, 313.000, 469.500,  0.00, 240, 0, SCAN_RGB   },
/* SC2_60 */{240.385,   6.001,  7.900,  78.128,  78.128, 156.256, 156.256, 234.384,  0.00, 240, 0, SCAN_RGB   },
/* PD50  */ { 388.160,  22.080, 19.300,  91.520,  91.520, 183.040, 183.040, 274.560,  0.00, 240, 0, SCAN_YC_PD },
/* PD90  */ { 703.040,  22.080, 18.900, 170.240, 170.240, 340.480, 340.480, 510.720,  0.00, 240, 0, SCAN_YC_PD },
/* PD120 */ { 508.480,  22.080, 19.400, 121.600, 121.600, 243.200, 243.200, 364.800,  0.00, 480, 0, SCAN_YC_PD },
/* PD160 */ { 804.416,  22.080, 18.900, 195.584, 195.584, 391.168, 391.168, 586.752,  0.00, 480, 0, SCAN_YC_PD },
/* PD180 */ { 754.240,  22.080, 18.900, 183.040, 183.040, 366.080, 366.080, 549.120,  0.00, 480, 0, SCAN_YC_PD },
/* PD240 */ {1000.000,  22.080, 18.900, 244.480, 244.480, 488.960, 488.960, 733.440,  0.00, 480, 0, SCAN_YC_PD },
/* PD290 */ { 937.280,  22.080, 18.900, 228.800, 228.800, 457.600, 457.600, 686.400,  0.00, 480, 0, SCAN_YC_PD },
/* P3    */ { 409.375,   6.250,  7.800, 133.333, 134.375, 267.708, 268.750, 402.083,  0.00, 480, 0, SCAN_RGB   },
/* P5    */ { 614.063,   9.375,  9.200, 200.000, 201.563, 401.563, 403.125, 603.125,  0.00, 480, 0, SCAN_RGB   },
/* P7    */ { 818.750,  12.500, 11.500, 266.667, 268.750, 535.417, 537.500, 804.167,  0.00, 480, 0, SCAN_RGB   },
/* MR73  */ { 286.300,  10.000, 10.600, 138.000, 138.100, 207.100, 207.200, 276.200, 69.00, 640, 0, SCAN_YC    },
/* MR90  */ { 352.300,  10.000, 10.600, 171.000, 171.100, 256.600, 256.700, 342.200, 85.50,   0, 0, SCAN_YC    },
/* MR115 */ { 450.300,  10.000, 10.600, 220.000, 220.100, 330.100, 330.200, 440.200,110.00,   0, 0, SCAN_YC    },
/* MR140 */ { 548.300,  10.000, 10.600, 269.000, 269.100, 403.600, 403.700, 538.200,134.50,   0, 0, SCAN_YC    },
/* MR175 */ { 684.300,  10.000, 10.600, 337.000, 337.100, 505.600, 505.700, 674.200,168.50,   0, 0, SCAN_YC    },
/* MP73  */ { 570.000,  10.000, 10.500, 140.000, 140.000, 280.000, 280.000, 420.000,  0.00,1280, 0, SCAN_YC_PD },
/* MP115 */ { 902.000,  10.000, 10.500, 223.000, 223.000, 446.000, 446.000, 669.000,  0.00,   0, 0, SCAN_YC_PD },
/* MP140 */ {1090.000,  10.000, 10.500, 270.000, 270.000, 540.000, 540.000, 810.000,  0.00,   0, 0, SCAN_YC_PD },
/* MP175 */ {1370.000,  10.000, 10.500, 340.000, 340.000, 680.000, 680.000,1020.000,  0.00,   0, 0, SCAN_YC_PD },
/* ML180 */ { 363.300,  10.000, 10.600, 176.500, 176.600, 264.850, 264.950, 353.200, 88.25,   0, 0, SCAN_YC    },
/* ML240 */ { 483.300,  10.000, 10.600, 236.500, 236.600, 354.850, 354.950, 473.200,118.25,   0, 0, SCAN_YC    },
/* ML280 */ { 565.300,  10.000, 10.600, 277.500, 277.600, 416.350, 416.450, 555.200,138.75,   0, 0, SCAN_YC    },
/* ML320 */ { 645.300,  10.000, 10.600, 317.500, 317.600, 476.350, 476.450, 635.200,158.75,   0, 0, SCAN_YC    },
/* R24   */ { 200.000,   8.000,  8.100,  92.000,  96.000, 142.000, 146.000, 192.000, 46.00, 240, 1, SCAN_YC    },
/* BW8   */ {  66.897,   8.000,  8.200,  58.897,   0.000,   0.000,   0.000,   0.000,  0.00, 240, 1, SCAN_BW    },
/* BW12  */ { 100.000,   8.000,  8.000,  92.000,   0.000,   0.000,   0.000,   0.000,  0.00, 240, 1, SCAN_BW    },
/* MN73  */ { 570.000,  10.000, 10.500, 140.000, 140.000, 280.000, 280.000, 420.000,  0.00,1280, 0, SCAN_YC_PD },
/* MN110 */ { 858.000,  10.000, 10.500, 212.000, 212.000, 424.000, 424.000, 636.000,  0.00,   0, 0, SCAN_YC_PD },
/* MN140 */ {1090.000,  10.000, 10.500, 270.000, 270.000, 540.000, 540.000, 810.000,  0.00,   0, 0, SCAN_YC_PD },
/* MC110 */ { 428.500,   8.000,  8.950, 140.000, 140.000, 280.000, 280.000, 420.000,  0.00,   0, 0, SCAN_RGB   },
/* MC140 */ { 548.500,   8.000,  8.750, 180.000, 180.000, 360.000, 360.000, 540.000,  0.00,   0, 0, SCAN_RGB   },
/* MC180 */ { 704.500,   8.000,  8.750, 232.000, 232.000, 464.000, 464.000, 696.000,  0.00,   0, 0, SCAN_RGB   },
};

/* === IMAGE DECODER STATE === */
typedef struct {
    image_decode_state_t state;  /* IMAGE_IDLE / IMAGE_DECODE_R / IMAGE_COMPLETE */
} image_decoder_t;

/* CSYNCINT: Leader interval tracker (MMSSTV parity) */
#define MSYNCLINE 8
typedef struct {
    uint32_t sync_list[MSYNCLINE];   /* Ring buffer of sync intervals */
    uint32_t sync_cnt;                /* Sample counter */
    uint32_t sync_acnt;               /* Last sync position */
    int sync_int_max;                 /* Peak level during interval */
    uint32_t sync_int_pos;            /* Position of peak */
    int sync_phase;                   /* Narrow sync phase (for 1900 Hz) */
} sync_tracker_t;

/* === MAIN DECODER STRUCT === */
struct sstv_decoder_s {
    double sample_rate;
    sstv_mode_t mode_hint;
    sstv_mode_t detected_mode;       /* Mode from VIS decode */
    int vis_enabled;
    sstv_rx_status_t last_status;
    
    /* === AGC STATE === */
    sstv_agc_mode_t agc_mode;        /* SSTV_AGC_OFF bypasses the MMSSTV level AGC */
    
    /* === STATE MACHINES === */
    vis_decoder_t vis;
    sync_state_t sync_state;
    
    /* === DSP FILTERS (MMSSTV parity) === */
    sstv_dsp::CIIRTANK iir11;       /* 1080 Hz */
    sstv_dsp::CIIRTANK iir12;       /* 1200 Hz */
    sstv_dsp::CIIRTANK iir13;       /* 1320 Hz */
    sstv_dsp::CIIRTANK iir19;       /* 1900 Hz */
    sstv_dsp::CIIR lpf11;           /* 50 Hz LPF */
    sstv_dsp::CIIR lpf12;           /* 50 Hz LPF */
    sstv_dsp::CIIR lpf13;           /* 50 Hz LPF */
    sstv_dsp::CIIR lpf19;           /* 50 Hz LPF */
    sstv_dsp::CIIRTANK iirfsk;      /* 2100 Hz: narrow-mode FSK space tone */
    sstv_dsp::CIIR lpffsk;          /* 50 Hz LPF */

    /* Narrow-mode N-VIS FSK decoder (MMSSTV CSSTVDEM::DecodeFSK, N-VIS path) */
    int    fsk_mode;
    long   fsk_time;
    double fsk_nextd;
    long   fsk_nexti;
    int    fsk_bcnt;
    int    fsk_c;
    int    fsk_s;
    int    fsk_code;
    sstv_dsp::CFIR2 bpf;            /* Bandpass FIR (shared delay line) */
    std::vector<double> hbpf;       /* MMSSTV HBPF taps */
    std::vector<double> hbpfs;      /* MMSSTV HBPFS taps */
    int bpftap;                     /* MMSSTV BPF tap count */
    int use_bpf;                    /* MMSSTV m_bpf */
    
    /* === DEMOD STATE === */
    double prev_sample;              /* For simple LPF (adjacent average) */
    level_agc_t lvl;                 /* MMSSTV AGC */

    /* === CHILL Hilbert FM demodulator (MMSSTV parity) === */
    std::vector<double> hill_h;       /* Hilbert FIR taps (hill_tap+1 elements) */
    std::vector<double> hill_z;       /* Delay line (hill_tap+1 elements) */
    int hill_tap;                     /* FIR length: 12 (<16kHz), 24 (16-40kHz), 48 (>=40kHz) */
    int hill_df;                      /* Phase diff window: 0=1-sample, 1=2-sample, 2=4-sample */
    double hill_A[4];                 /* Phase history ring: m_A[0..3] (MMSSTV parity) */
    double hill_off;                  /* Re-centering offset (scaled for hill_df) */
    double hill_out_scale;            /* Output scale (scaled for hill_df) */
    sstv_dsp::CIIR hill_lpf;          /* 1800 Hz post-demod LPF, order 3 */
    double hill_demod_last;           /* Most recent FM demod output (+-16384 range) */
    double d12_last;                  /* Most recent 1200 Hz tone energy (for sync re-lock) */
    double d19_last;                  /* Most recent 1900 Hz tone energy (narrow-mode sync) */

    /* === IMAGE BUFFER === */
    image_buffer_t image_buf;

    /* === IMAGE DECODER === */
    image_decoder_t img_dec;

    /* === SCAN LINE TIMING (samples, computed from mode + sample_rate) === */
    double scan_TW;         /* total scan line samples */
    double scan_OF;         /* sync end offset from line start */
    double scan_KS;         /* ch0 active end (post-sync offset) */
    double scan_KSS;        /* ch0 effective width for pixel column mapping */
    double scan_SG;         /* ch1 start (post-sync) */
    double scan_CG;         /* ch1 end */
    double scan_SB;         /* ch2 start */
    double scan_CB;         /* ch2 end */
    double scan_KS2;        /* chroma channel half-width (samples) */
    double scan_KS2S;       /* chroma effective width for pixel column mapping */
    scan_color_type_t scan_color_type;
    int    scan_row_double; /* 1 = scan line writes 2 image rows */
    int    scan_sync_1900;  /* 1 = line sync is 1900 Hz (narrow modes), else 1200 Hz */

    /* === LINE DECODER STATE === */
    double line_pos;        /* sample position within current scan line (0-based) */
    int    lnd_img_line;    /* image row currently being written (0-based) */
    std::vector<double> ch0_buf;  /* per-column ch0: Y or R (luma/luma formula) */
    std::vector<double> ch1_buf;  /* per-column ch1: R-Y or G (chroma/luma formula) */
    std::vector<double> ch2_buf;  /* per-column ch2: B-Y or B (chroma/luma formula) */
    std::vector<double> ch3_buf;  /* per-column ch3: Y_even for SCAN_YC_PD */
    /* Robot 36 (4:2:0): each line carries Y plus one chroma component, chosen by
     * the separator tone before it (1500 Hz = R-Y, 2300 Hz = B-Y). The latest
     * R-Y and B-Y rows are kept across lines (MMSSTV m_D36[0..1], m_DSEL). */
    std::vector<double> r36_ry;
    std::vector<double> r36_by;
    double r36_sep_sum;           /* sum of separator-tone levels in this line */
    int    r36_sep_n;
    int    r36_dsel;              /* 0 = last chroma was R-Y, 1 = B-Y */

    /* === PER-LINE SYNC RE-LOCK STATE === */
    double sync_ofp;            /* Expected sync peak position in samples (≈ scan_OF/2) */
    double sync_peak_val;       /* Max d12 seen in the current scan line's sync region */
    int    sync_peak_pos;       /* Sample position of that d12 peak */

    /* === TIMING CORRECTION STATE === */
    int timing_correction_enabled;   /* Enable real-time phase/slant correction */
    double timing_correction_gain;   /* Gain for feedback correction */
    double timing_error;             /* Estimated timing error in samples */
    double timing_correction;        /* Applied correction in samples */
    double timing_error_accum;       /* Smoothed timing error */
    int timing_error_count;          /* Samples contributing to estimate */
    int timing_phase_sign;           /* Sign of current drift estimate */

    /* === SYNC TRACKING === */
    int sync_mode;                   /* MMSSTV sync state (m_SyncMode) */
    int sync_time;                   /* MMSSTV sync timer (m_SyncTime) */
    int leader_drop_count;           /* Count samples during brief leader interruptions */
    int vis_data;                    /* MMSSTV VIS accumulator (m_VisData) */
    int vis_cnt;                     /* MMSSTV VIS bit count (m_VisCnt, 7 for data bits) */
    int vis_parity_pending;          /* Waiting to decode parity bit */
    int vis_extended;                /* MMSSTV extended VIS flag (0x23 prefix) */
    int vis_inverted;                /* Bit-polarity inverted (0x5C prefix or ^ 0xFF fallback) */
    int sense_level;                 /* MMSSTV sense level (m_SenseLvl) */
    double s_lvl;                    /* MMSSTV m_SLvl */
    double s_lvl2;                   /* MMSSTV m_SLvl2 */
    double s_lvl3;                   /* MMSSTV m_SLvl3 */
    
    /* MMSSTV leader trackers (m_sint1/m_sint2/m_sint3) */
    sync_tracker_t sint1;            /* Primary 1200 Hz sync tracker */
    sync_tracker_t sint2;            /* Secondary sync tracker */
    sync_tracker_t sint3;            /* 1900 Hz narrow sync tracker */
    
    /* === DEBUGGING === */
    int debug_level;                 /* 0=off, 1=errors, 2=info, 3=verbose */
    
    /* === DEBUG WAV OUTPUT === */
    FILE *debug_wav_before;          /* Before filtering (after LPF) */
    FILE *debug_wav_after_bpf;       /* After BPF */
    FILE *debug_wav_after_agc;       /* After AGC */
    FILE *debug_wav_final;           /* After final scaling */
    uint32_t debug_wav_sample_count; /* Number of samples written */
};

/* === VIS CODE MAPPING === */
typedef struct {
    uint8_t vis_code;        /* 8-bit VIS code */
    sstv_mode_t mode;        /* Corresponding SSTV mode */
} vis_map_entry_t;

/* VIS code to mode lookup table */
/* Codes match standard VIS values (LSB-first transmission) */
static const vis_map_entry_t VIS_CODE_MAP[] = {
    /* Standard VIS codes (7-bit + parity) */
    { 0x84, SSTV_R24 },         /* Robot 24 */
    { 0x88, SSTV_R36 },         /* Robot 36 */
    { 0x0C, SSTV_R72 },         /* Robot 72 */
    { 0x44, SSTV_AVT90 },       /* AVT 90 */
    { 0x3C, SSTV_SCOTTIE1 },    /* Scottie 1 */
    { 0xB8, SSTV_SCOTTIE2 },    /* Scottie 2 */
    { 0xCC, SSTV_SCOTTIEX },    /* Scottie DX */
    { 0xAC, SSTV_MARTIN1 },     /* Martin 1 */
    { 0x28, SSTV_MARTIN2 },     /* Martin 2 */
    { 0xB7, SSTV_SC2_180 },     /* SC2-180 */
    { 0x3F, SSTV_SC2_120 },     /* SC2-120 */
    { 0xBB, SSTV_SC2_60 },      /* SC2-60 */
    { 0xDD, SSTV_PD50 },        /* PD 50 */
    { 0x63, SSTV_PD90 },        /* PD 90 */
    { 0x5F, SSTV_PD120 },       /* PD 120 */
    { 0xE2, SSTV_PD160 },       /* PD 160 */
    { 0x60, SSTV_PD180 },       /* PD 180 */
    { 0xE1, SSTV_PD240 },       /* PD 240 */
    { 0xDE, SSTV_PD290 },       /* PD 290 */
    { 0x71, SSTV_P3 },          /* Pasokon P3 */
    { 0x72, SSTV_P5 },          /* Pasokon P5 */
    { 0xF3, SSTV_P7 },          /* Pasokon P7 */
    { 0x82, SSTV_BW8 },         /* Robot B/W 8 */
    { 0x86, SSTV_BW12 },        /* Robot B/W 12 */
    
    /* Extended VIS codes (16-bit, second byte after 0x23 prefix) */
    { 0x45, SSTV_MR73 },        /* Martin R73 (extended) */
    { 0x46, SSTV_MR90 },        /* Martin R90 (extended) */
    { 0x49, SSTV_MR115 },       /* Martin R115 (extended) */
    { 0x4A, SSTV_MR140 },       /* Martin R140 (extended) */
    { 0x4C, SSTV_MR175 },       /* Martin R175 (extended) */
    { 0x25, SSTV_MP73 },        /* Martin P73 (extended) */
    { 0x29, SSTV_MP115 },       /* Martin P115 (extended) */
    { 0x2A, SSTV_MP140 },       /* Martin P140 (extended) */
    { 0x2C, SSTV_MP175 },       /* Martin P175 (extended) */
    { 0x85, SSTV_ML180 },       /* Martin L180 (extended) */
    { 0x86, SSTV_ML240 },       /* Martin L240 (extended - note: same code as BW12, disambiguated by prefix) */
    { 0x89, SSTV_ML280 },       /* Martin L280 (extended) */
    { 0x8A, SSTV_ML320 },       /* Martin L320 (extended) */
    /* Narrow modes (MP73-N ... MC180-N) have no VIS. MMSSTV identifies them with a
     * separate 1900/2100 Hz FSK "N-VIS" header, which this decoder does not decode;
     * use sstv_decoder_set_mode_hint() for those modes. */
};

#define VIS_MAP_SIZE (sizeof(VIS_CODE_MAP) / sizeof(vis_map_entry_t))

/* Sync tracker forward declarations */
static void sync_tracker_init(sync_tracker_t *st);
static void sync_tracker_inc(sync_tracker_t *st);
static void sync_tracker_trig(sync_tracker_t *st, int d);
static void sync_tracker_max(sync_tracker_t *st, int d);
static int sync_tracker_start(sync_tracker_t *st, double sample_rate);

/* Forward declarations */
static void decoder_reset_state(sstv_decoder_t *dec);
static void decoder_process_sample(sstv_decoder_t *dec, double sample);
static int decoder_check_vis_ready(sstv_decoder_t *dec, sstv_mode_t *mode_out);
static int decoder_try_vis_from_buffer(sstv_decoder_t *dec, sstv_mode_t *mode_out);
static int vis_parity_ok(uint8_t vis_code);
static sstv_mode_t vis_code_to_mode(uint8_t vis_code, int is_extended);
static int decoder_allocate_image_buffer(sstv_decoder_t *dec, sstv_mode_t mode, int start_kind);
static void level_agc_init(level_agc_t *lvl, double sample_rate);
static void level_agc_do(level_agc_t *lvl, double d);
static void level_agc_fix(level_agc_t *lvl);
static double level_agc_apply(level_agc_t *lvl, double d);
static void decoder_set_sense_levels(sstv_decoder_t *dec);
static double hill_do(sstv_decoder_t *dec, double in);
static void hill_set_width(sstv_decoder_t *dec, int narrow);
static void lnd_flush_line(sstv_decoder_t *dec);
static void decoder_process_image_sample(sstv_decoder_t *dec, double sig);

sstv_decoder_t* sstv_decoder_create(double sample_rate) {
    if (sample_rate <= 0.0) {
        return NULL;
    }
    sstv_decoder_t *dec = new sstv_decoder_t();
    if (!dec) {
        return NULL;
    }
    dec->sample_rate = sample_rate;
    dec->mode_hint = SSTV_MODE_COUNT; /* no hint */
    dec->detected_mode = SSTV_MODE_COUNT; /* no mode detected yet */
    dec->vis_enabled = 1;
    dec->last_status = SSTV_RX_NEED_MORE;
    dec->debug_level = 0;
    
    /* Initialize debug WAV files to NULL */
    dec->debug_wav_before = NULL;
    dec->debug_wav_after_bpf = NULL;
    dec->debug_wav_after_agc = NULL;
    dec->debug_wav_final = NULL;
    dec->debug_wav_sample_count = 0;
    
    /* AGC: any mode other than OFF runs the MMSSTV level AGC */
    dec->agc_mode = SSTV_AGC_AUTO;
    
        /* Allocate VIS buffers (store ~800ms of energies) */
        dec->vis.buf_size = (int)(0.800 * sample_rate);
        if (dec->vis.buf_size < 1) {
            dec->vis.buf_size = 1;
        }
        dec->vis.mark_buf = (double *)calloc((size_t)dec->vis.buf_size, sizeof(double));
        dec->vis.space_buf = (double *)calloc((size_t)dec->vis.buf_size, sizeof(double));
        dec->vis.buf_pos = 0;
        dec->vis.buffering = 0;
    
    /* Initialize DSP filters (MMSSTV frequencies: 1080/1320 Hz per original implementation) */
    dec->iir11.SetFreq(1080.0, sample_rate, 80.0);  /* Mark tone - matches MMSSTV sstv.cpp:1772 */
    dec->iir12.SetFreq(1200.0, sample_rate, 100.0); /* Sync tone */
    dec->iir13.SetFreq(1320.0, sample_rate, 80.0);  /* Space tone - matches MMSSTV sstv.cpp:1774 */
    dec->iir19.SetFreq(1900.0, sample_rate, 100.0); /* Leader tone */
    dec->lpf11.MakeIIR(50.0, sample_rate, 2, 0, 0);
    dec->lpf12.MakeIIR(50.0, sample_rate, 2, 0, 0);
    dec->lpf13.MakeIIR(50.0, sample_rate, 2, 0, 0);
    dec->lpf19.MakeIIR(50.0, sample_rate, 2, 0, 0);
    dec->iirfsk.SetFreq(2100.0, sample_rate, 100.0);   /* MMSSTV FSKSPACE */
    dec->lpffsk.MakeIIR(50.0, sample_rate, 2, 0, 0);

    /* MMSSTV BPF taps (CSSTVDEM::CalcBPF, "wide" setting): HBPF (1100-2600 Hz, used once
     * image decoding starts; low edge is 1100 because m_SyncRestart defaults to 1) and
     * HBPFS (400-2500 Hz, used while hunting for VIS). */
    dec->use_bpf = 1;
    dec->bpftap = (int)(24.0 * sample_rate / 11025.0);
    if (dec->bpftap < 1) dec->bpftap = 1;
    dec->hbpf.assign(dec->bpftap + 1, 0.0);
    dec->hbpfs.assign(dec->bpftap + 1, 0.0);
    sstv_dsp::MakeFilter(dec->hbpf.data(), dec->bpftap, sstv_dsp::kFfBPF, sample_rate, 1100.0, 2600.0, 20.0, 1.0);
    sstv_dsp::MakeFilter(dec->hbpfs.data(), dec->bpftap, sstv_dsp::kFfBPF, sample_rate, 400.0, 2500.0, 20.0, 1.0);
    dec->bpf.Create(dec->bpftap);
    
    dec->sense_level = 0;            /* Default to lowest (most sensitive) */
    decoder_set_sense_levels(dec);
    level_agc_init(&dec->lvl, sample_rate);

    /* Initialize CHILL Hilbert FM demodulator (port of MMSSTV CHILL::SetWidth) */
    /* hill_df: phase-differencing window band (matches MMSSTV SampBase thresholds) */
    dec->hill_df  = (sample_rate >= 40000.0) ?  2 : (sample_rate >= 16000.0) ?  1 :  0;
    /* hill_tap: scale proportionally to Fs so group delay stays ~0.54 ms at all rates.
     * MMSSTV used 12/24/48 at exactly 11025/22050/44100 Hz; we generalise by rounding
     * 12 * Fs/11025 up to the nearest even integer (min 6). */
    {
        int raw = (int)(12.0 * sample_rate / 11025.0 + 0.5);
        if (raw < 6) raw = 6;
        if (raw % 2 != 0) raw++;   /* must be even: center tap = hill_tap/2 */
        dec->hill_tap = raw;
    }
    dec->hill_h.assign(dec->hill_tap + 1, 0.0);
    dec->hill_z.assign(dec->hill_tap + 1, 0.0);
    sstv_dsp::MakeHilbert(dec->hill_h.data(), dec->hill_tap, sample_rate, 100.0, sample_rate / 2.0 - 100.0);
    memset(dec->hill_A, 0, sizeof(dec->hill_A));
    hill_set_width(dec, 0);
    dec->hill_lpf.MakeIIR(1800.0, sample_rate, 3, 0, 0);
    dec->hill_demod_last = 0.0;

    /* Initialize MMSSTV sync trackers */
    sync_tracker_init(&dec->sint1);
    sync_tracker_init(&dec->sint2);
    sync_tracker_init(&dec->sint3);

    /* Initialize state */
    decoder_reset_state(dec);
    
    return dec;
}


void sstv_decoder_free(sstv_decoder_t *dec) {
    if (dec) {
        /* Close and finalize any open debug WAV files */
        if (dec->debug_wav_before) {
            update_wav_header(dec->debug_wav_before, (uint32_t)dec->sample_rate, dec->debug_wav_sample_count);
            fclose(dec->debug_wav_before);
        }
        if (dec->debug_wav_after_bpf) {
            update_wav_header(dec->debug_wav_after_bpf, (uint32_t)dec->sample_rate, dec->debug_wav_sample_count);
            fclose(dec->debug_wav_after_bpf);
        }
        if (dec->debug_wav_after_agc) {
            update_wav_header(dec->debug_wav_after_agc, (uint32_t)dec->sample_rate, dec->debug_wav_sample_count);
            fclose(dec->debug_wav_after_agc);
        }
        if (dec->debug_wav_final) {
            update_wav_header(dec->debug_wav_final, (uint32_t)dec->sample_rate, dec->debug_wav_sample_count);
            fclose(dec->debug_wav_final);
        }
        
        if (dec->image_buf.pixels) {
            free(dec->image_buf.pixels);
        }
        if (dec->vis.mark_buf) {
            free(dec->vis.mark_buf);
        }
        if (dec->vis.space_buf) {
            free(dec->vis.space_buf);
        }
        delete dec;
    }
}

void sstv_decoder_reset(sstv_decoder_t *dec) {
    if (!dec) return;
    
    decoder_reset_state(dec);
    dec->mode_hint = SSTV_MODE_COUNT;
    dec->last_status = SSTV_RX_NEED_MORE;
}

int sstv_decoder_enable_debug_wav(sstv_decoder_t *dec,
                                   const char *before_filepath,
                                   const char *after_bpf_filepath,
                                   const char *after_agc_filepath,
                                   const char *final_filepath) {
    if (!dec) return -1;
    
    /* Close any existing debug WAV files first */
    sstv_decoder_disable_debug_wav(dec);
    
    /* Open requested files and write headers */
    if (before_filepath) {
        dec->debug_wav_before = fopen(before_filepath, "wb");
        if (dec->debug_wav_before) {
            write_wav_header_placeholder(dec->debug_wav_before, (uint32_t)dec->sample_rate);
        }
    }
    
    if (after_bpf_filepath) {
        dec->debug_wav_after_bpf = fopen(after_bpf_filepath, "wb");
        if (dec->debug_wav_after_bpf) {
            write_wav_header_placeholder(dec->debug_wav_after_bpf, (uint32_t)dec->sample_rate);
        }
    }
    
    if (after_agc_filepath) {
        dec->debug_wav_after_agc = fopen(after_agc_filepath, "wb");
        if (dec->debug_wav_after_agc) {
            write_wav_header_placeholder(dec->debug_wav_after_agc, (uint32_t)dec->sample_rate);
        }
    }
    
    if (final_filepath) {
        dec->debug_wav_final = fopen(final_filepath, "wb");
        if (dec->debug_wav_final) {
            write_wav_header_placeholder(dec->debug_wav_final, (uint32_t)dec->sample_rate);
        }
    }
    
    dec->debug_wav_sample_count = 0;
    return 0;
}

void sstv_decoder_disable_debug_wav(sstv_decoder_t *dec) {
    if (!dec) return;
    
    /* Close and finalize any open files */
    if (dec->debug_wav_before) {
        update_wav_header(dec->debug_wav_before, (uint32_t)dec->sample_rate, dec->debug_wav_sample_count);
        fclose(dec->debug_wav_before);
        dec->debug_wav_before = NULL;
    }
    
    if (dec->debug_wav_after_bpf) {
        update_wav_header(dec->debug_wav_after_bpf, (uint32_t)dec->sample_rate, dec->debug_wav_sample_count);
        fclose(dec->debug_wav_after_bpf);
        dec->debug_wav_after_bpf = NULL;
    }
    
    if (dec->debug_wav_after_agc) {
        update_wav_header(dec->debug_wav_after_agc, (uint32_t)dec->sample_rate, dec->debug_wav_sample_count);
        fclose(dec->debug_wav_after_agc);
        dec->debug_wav_after_agc = NULL;
    }
    
    if (dec->debug_wav_final) {
        update_wav_header(dec->debug_wav_final, (uint32_t)dec->sample_rate, dec->debug_wav_sample_count);
        fclose(dec->debug_wav_final);
        dec->debug_wav_final = NULL;
    }
    
    dec->debug_wav_sample_count = 0;
}

/* === INTERNAL HELPERS === */

/* CSYNCINT sync tracker implementation (MMSSTV parity) */
static void sync_tracker_init(sync_tracker_t *st) {
    if (!st) return;
    memset(st->sync_list, 0, sizeof(st->sync_list));
    st->sync_cnt = 0;
    st->sync_acnt = 0;
    st->sync_int_max = 0;
    st->sync_int_pos = 0;
    st->sync_phase = 0;
}

static void sync_tracker_inc(sync_tracker_t *st) {
    if (!st) return;
    st->sync_cnt++;
}

static void sync_tracker_trig(sync_tracker_t *st, int d) {
    if (!st) return;
    st->sync_int_max = d;
    st->sync_int_pos = st->sync_cnt;
}

static void sync_tracker_max(sync_tracker_t *st, int d) {
    if (!st) return;
    if (st->sync_int_max < d) {
        st->sync_int_max = d;
        st->sync_int_pos = st->sync_cnt;
    }
}

static int sync_tracker_start(sync_tracker_t *st, double sample_rate) {
    /* Simplified MMSSTV SyncStart: For now, just accept leader if we have peak tracking.
     * The 15ms validation in mode 1 is sufficient - don't add extra interval requirements.
     */
    if (!st) return 0;
    if (!st->sync_int_max) return 0;
    
    /* Accept any leader that has been tracked - the mode 1 15ms timer validates it */
    st->sync_int_max = 0;
    return 1;
}

/* === MMSSTV CLVL AGC === */static void level_agc_init(level_agc_t *lvl, double sample_rate) {
    if (!lvl) return;
    lvl->m_agcfast = 1;
    lvl->m_CntMax = (int)(sample_rate * 100.0 / 1000.0);
    lvl->m_PeakMax = 0.0;
    lvl->m_PeakAGC = 0.0;
    lvl->m_Peak = 0.0;
    lvl->m_Cur = 0.0;
    lvl->m_CurMax = 0.0;
    lvl->m_Max = 0.0;
    lvl->m_agc = 1.0;
    lvl->m_CntPeak = 0;
    lvl->m_Cnt = 0;
}

static void level_agc_do(level_agc_t *lvl, double d) {
    if (!lvl) return;
    lvl->m_Cur = d;
    if (d < 0.0) d = -d;
    if (lvl->m_Max < d) lvl->m_Max = d;
    lvl->m_Cnt++;
}

static void level_agc_fix(level_agc_t *lvl) {
    if (!lvl) return;
    if (lvl->m_Cnt < lvl->m_CntMax) return;
    lvl->m_Cnt = 0;
    lvl->m_CntPeak++;
    if (lvl->m_Peak < lvl->m_Max) lvl->m_Peak = lvl->m_Max;
    if (lvl->m_CntPeak >= 5) {
        lvl->m_CntPeak = 0;
        lvl->m_PeakMax = lvl->m_Max;
        lvl->m_PeakAGC = (lvl->m_PeakAGC + lvl->m_Max) * 0.5;
        lvl->m_Peak = 0.0;
        if (!lvl->m_agcfast) {
            if ((lvl->m_PeakAGC > 32) && lvl->m_PeakMax) {
                lvl->m_agc = 16384.0 / lvl->m_PeakMax;
            } else {
                lvl->m_agc = 16384.0 / 32.0;
            }
        }
    } else {
        if (lvl->m_PeakMax < lvl->m_Max) lvl->m_PeakMax = lvl->m_Max;
    }
    lvl->m_CurMax = lvl->m_Max;
    if (lvl->m_agcfast) {
        if (lvl->m_CurMax > 32) {
            lvl->m_agc = 16384.0 / lvl->m_CurMax;
        } else {
            lvl->m_agc = 16384.0 / 32.0;
        }
    }
    lvl->m_Max = 0.0;
}

static double level_agc_apply(level_agc_t *lvl, double d) {
    if (!lvl) return d;
    return d * lvl->m_agc;
}

static void decoder_set_sense_levels(sstv_decoder_t *dec) {
    if (!dec) return;
    switch (dec->sense_level) {
        case 1:
            dec->s_lvl = 3500.0;
            dec->s_lvl2 = 80.0;    /* Very low threshold to accept marginal VIS bits */
            dec->s_lvl3 = 5700.0;
            break;
        case 2:
            dec->s_lvl = 4800.0;
            dec->s_lvl2 = 80.0;
            dec->s_lvl3 = 6800.0;
            break;
        case 3:
            dec->s_lvl = 6000.0;
            dec->s_lvl2 = 80.0;
            dec->s_lvl3 = 8000.0;
            break;
        default:
            dec->s_lvl = 2400.0;
            dec->s_lvl2 = 80.0;    /* Very low threshold to accept marginal VIS bits */
            dec->s_lvl3 = 5000.0;
            break;
    }
}

/*
 * Narrow-mode N-VIS decoder: port of the N-VIS path of MMSSTV
 * CSSTVDEM::DecodeFSK(). m = 1900 Hz (mark) energy, s = 2100 Hz (space)
 * energy. The header MMSSTV sends before a narrow-mode image is a 2100 Hz
 * guard (100 ms), a 1900 Hz start bit, then 6-bit words LSB-first at 22 ms per
 * bit (1900 Hz = 1): 0x2d, 0x15, the N-VIS code, and code ^ 0x15.
 * Returns the detected narrow mode, or SSTV_MODE_COUNT.
 */
static sstv_mode_t decoder_decode_nvis(sstv_decoder_t *dec, double m, double s) {
    const double kFskGuardMs = 100.0;
    const double kFskBitMs = 22.0;
    const double kFskLevel = 2048.0;
    const double fs = dec->sample_rate;
    double d = fabs(m - s);
    sstv_mode_t found = SSTV_MODE_COUNT;

    switch (dec->fsk_mode) {
    case 0:     /* space carrier (guard tone) */
        if (s > m && d >= kFskLevel) {
            dec->fsk_time = (long)(kFskGuardMs / 2 * fs / 1000.0);
            dec->fsk_mode = 1;
        }
        break;
    case 1:     /* guard tone continues for half its length */
        if (s > m && d >= kFskLevel) {
            if (--dec->fsk_time <= 0) {
                dec->fsk_time = (long)(kFskGuardMs * fs / 1000.0);
                dec->fsk_mode = 2;
            }
        } else {
            dec->fsk_mode = 0;
        }
        break;
    case 2:     /* wait for the start bit (mark) */
        if (--dec->fsk_time <= 0) {
            dec->fsk_mode = 0;
        } else if (m > s && d >= kFskLevel) {
            dec->fsk_time = (long)(kFskBitMs / 2 * fs / 1000.0);
            dec->fsk_mode = 3;
        }
        break;
    case 3:     /* start bit is still mark at its midpoint */
        if (--dec->fsk_time <= 0) {
            if (m > s && d >= kFskLevel) {
                dec->fsk_time = 0;
                dec->fsk_nextd = kFskBitMs / 1000.0 * fs;
                dec->fsk_nexti = (long)dec->fsk_nextd;
                dec->fsk_bcnt = 0;
                dec->fsk_c = 0;
                dec->fsk_mode = 4;
            } else {
                dec->fsk_mode = 0;
            }
        }
        break;
    default:    /* 4 = first word, 16..18 = N-VIS words; one sample per bit */
        dec->fsk_time++;
        if (dec->fsk_time < dec->fsk_nexti) break;
        if (d < kFskLevel) {
            dec->fsk_mode = 0;
            break;
        }
        dec->fsk_nextd += kFskBitMs / 1000.0 * fs;
        dec->fsk_nexti = (long)dec->fsk_nextd;
        dec->fsk_c >>= 1;
        if (m > s) dec->fsk_c |= 0x20;
        if (++dec->fsk_bcnt < 6) break;
        dec->fsk_bcnt = 0;
        switch (dec->fsk_mode) {
        case 4:     /* 0x2d introduces an N-VIS (0x2a would be a callsign ID) */
            dec->fsk_s = 0;
            dec->fsk_mode = (dec->fsk_c == 0x2d) ? 16 : 0;
            break;
        case 16:
            dec->fsk_s ^= dec->fsk_c;
            dec->fsk_mode = (dec->fsk_c == 0x15) ? 17 : 0;
            break;
        case 17:
            dec->fsk_s ^= dec->fsk_c;
            dec->fsk_code = dec->fsk_c;
            dec->fsk_mode = 18;
            break;
        case 18:
            if (dec->fsk_c == (dec->fsk_s & 0x3f)) {
                switch (dec->fsk_code) {
                    case 0x02: found = SSTV_MN73;  break;
                    case 0x04: found = SSTV_MN110; break;
                    case 0x05: found = SSTV_MN140; break;
                    case 0x14: found = SSTV_MC110; break;
                    case 0x15: found = SSTV_MC140; break;
                    case 0x16: found = SSTV_MC180; break;
                    default: break;
                }
            }
            dec->fsk_mode = 0;
            break;
        default:
            dec->fsk_mode = 0;
            break;
        }
        dec->fsk_c = 0;
        break;
    }
    return found;
}

static void decoder_reset_state(sstv_decoder_t *dec) {
    if (!dec) return;
    
    /* Reset sync/VIS state machine (MMSSTV) */
    dec->sync_state = SYNC_IDLE;
    dec->sync_mode = 0;
    dec->sync_time = 0;
    dec->leader_drop_count = 0;
    dec->vis_data = 0;
    dec->vis_cnt = 0;
    dec->vis_extended = 0;
    dec->vis_inverted = 0;
    dec->fsk_mode = 0;
    dec->fsk_time = 0;
    dec->fsk_nextd = 0.0;
    dec->fsk_nexti = 0;
    dec->fsk_bcnt = 0;
    dec->fsk_c = 0;
    dec->fsk_s = 0;
    dec->fsk_code = 0;

    /* Reset MMSSTV sync trackers */
    sync_tracker_init(&dec->sint1);
    sync_tracker_init(&dec->sint2);
    sync_tracker_init(&dec->sint3);
    
    /* Reset VIS decoder */
    dec->vis.bit_count = 0;
    dec->vis.data = 0;
    dec->vis.is_16bit = 0;
    dec->vis.bit_timer = 0;
    dec->vis.mark_accum = 0.0;
    dec->vis.space_accum = 0.0;
    dec->vis.sample_count = 0;
    dec->vis.start_bit_samples = 0;
    dec->vis.start_bit_pending = 0;
    dec->vis.buf_pos = 0;
    dec->vis.buffering = 0;
    dec->vis.invert_polarity = 0;
    dec->vis.polarity_samples = 0;


    /* Reset demod state */
    dec->prev_sample = 0.0;
    memset(dec->hill_A, 0, sizeof(dec->hill_A));
    dec->hill_demod_last = 0.0;
    if (!dec->hill_z.empty()) {
        std::fill(dec->hill_z.begin(), dec->hill_z.end(), 0.0);
    }
    dec->hill_lpf.Clear();
    level_agc_init(&dec->lvl, dec->sample_rate);

    /* Reset scan line / image decoder state */
    dec->line_pos      = 0.0;
    dec->d12_last      = 0.0;
    dec->d19_last      = 0.0;
    dec->scan_sync_1900 = 0;
    dec->sync_ofp      = 0.0;
    dec->sync_peak_val = 0.0;
    dec->sync_peak_pos = 0;
    dec->timing_correction_enabled = 1;
    dec->timing_correction_gain = 0.04;
    dec->timing_error = 0.0;
    dec->timing_correction = 0.0;
    dec->timing_error_accum = 0.0;
    dec->timing_error_count = 0;
    dec->timing_phase_sign = 0;
    dec->lnd_img_line  = 0;
    dec->ch0_buf.clear();
    dec->ch1_buf.clear();
    dec->ch2_buf.clear();
    dec->ch3_buf.clear();
    dec->r36_ry.clear();
    dec->r36_by.clear();
    dec->r36_sep_sum = 0.0;
    dec->r36_sep_n   = 0;
    dec->r36_dsel    = 1;
    dec->img_dec.state = IMAGE_IDLE;

    /* Clear image buffer */
    if (dec->image_buf.pixels) {
        free(dec->image_buf.pixels);
        dec->image_buf.pixels = NULL;
    }
    dec->image_buf.width = 0;
    dec->image_buf.height = 0;
    dec->image_buf.current_line = 0;
    dec->image_buf.current_col = 0;
    
    /* Reset image decoder state */
    dec->img_dec.state = IMAGE_IDLE;
}

/**
 * Process a single audio sample through the demod pipeline
 * 
 * Flow:
 *   1. BPF (800-3000 Hz)
 *   2. CIIRTANK tone detectors (mark, space, sync)
 *   3. Sync detection
 *   4. VIS decoding (if enabled)
 */
static void decoder_process_sample(sstv_decoder_t *dec, double sample) {
    if (!dec) return;
    
    static int first_call = 1;
    if (first_call && dec->debug_level >= 2) {
        fprintf(stderr, "[DECODER] decoder_process_sample() called, sample_rate=%.0f\n", dec->sample_rate);
        first_call = 0;
    }
    
    /* Clip to prevent overflow */
    if (sample > 24576.0) sample = 24576.0;
    if (sample < -24576.0) sample = -24576.0;

    /* Simple LPF (adjacent average) */
    double d = (sample + dec->prev_sample) * 0.5;
    dec->prev_sample = sample;

    /* Debug WAV: Write BEFORE filtering (after LPF only) */
    if (dec->debug_wav_before) {
        write_sample_to_wav(dec->debug_wav_before, d);
    }

    /* BPF (MMSSTV: HBPFS before sync, HBPF after)
     * Use wide HBPF once image decoding is active (mirrors MMSSTV: m_Sync || m_SyncMode >= 3) */
    #if 1
    if (dec->use_bpf) {
        bool use_hbpf = (dec->sync_state == SYNC_DATA_WAIT) || (dec->sync_mode >= 3);
        if (use_hbpf && !dec->hbpf.empty()) {
            d = dec->bpf.Do(d, dec->hbpf.data());
        } else if (!dec->hbpfs.empty()) {
            d = dec->bpf.Do(d, dec->hbpfs.data());
        }
    }
    #endif

    /* Debug WAV: Write AFTER BPF */
    if (dec->debug_wav_after_bpf) {
        write_sample_to_wav(dec->debug_wav_after_bpf, d);
    }

    /* AGC (MMSSTV CLVL); bypassed when the caller selects SSTV_AGC_OFF */
    double ad = d;
    if (dec->agc_mode != SSTV_AGC_OFF) {
        level_agc_do(&dec->lvl, d);
        level_agc_fix(&dec->lvl);
        ad = level_agc_apply(&dec->lvl, d);
    }

    /* Debug WAV: Write AFTER AGC (clean normalized signal) */
    if (dec->debug_wav_after_agc) {
        write_sample_to_wav(dec->debug_wav_after_agc, ad);
    }

    /* Hilbert FM demodulator runs every sample to keep delay-line state fresh */
    dec->hill_demod_last = hill_do(dec, ad);

    d = ad * 32.0;
    if (d > 16384.0) d = 16384.0;
    if (d < -16384.0) d = -16384.0;

    /* Debug WAV: Write FINAL clean signal
     * Note: 'd' is now scaled ×32 and clamped for tone detector operation (±16384 range).
     * This causes severe clipping for typical signals and is NOT suitable for audio playback.
     * For debug WAV output, we write the clean AGC output 'ad' at full scale instead.
     * This lets you hear the actual signal quality going into tone detection. */
    if (dec->debug_wav_final) {
        write_sample_to_wav(dec->debug_wav_final, ad * 2.0);
    }
    
    /* Increment sample count if any debug WAV is active */
    if (dec->debug_wav_before || dec->debug_wav_after_bpf || 
        dec->debug_wav_after_agc || dec->debug_wav_final) {
        dec->debug_wav_sample_count++;
    }

    /* Tone detectors + 50 Hz LPF (MMSSTV) */
    double d12 = dec->iir12.Do(d);
    if (d12 < 0.0) d12 = -d12;
    d12 = dec->lpf12.Do(d12);
    dec->d12_last = d12;   /* save for per-line sync re-lock */

    double d19 = dec->iir19.Do(d);
    if (d19 < 0.0) d19 = -d19;
    d19 = dec->lpf19.Do(d19);
    dec->d19_last = d19;

    /* 2100 Hz FSK space detector for the narrow-mode N-VIS header */
    double dsp = dec->iirfsk.Do(d);
    if (dsp < 0.0) dsp = -dsp;
    dsp = dec->lpffsk.Do(dsp);

    /* Additional tone detectors for image data */
    double d11 = dec->iir11.Do(d);
    if (d11 < 0.0) d11 = -d11;
    d11 = dec->lpf11.Do(d11);

    double d13 = dec->iir13.Do(d);
    if (d13 < 0.0) d13 = -d13;
    d13 = dec->lpf13.Do(d13);
    
    /* If we're in image decoding mode, process the sample for image data */
    if (dec->sync_state == SYNC_DATA_WAIT && dec->image_buf.pixels) {
        if (dec->img_dec.state != IMAGE_COMPLETE) {
            decoder_process_image_sample(dec, dec->hill_demod_last);
        }
    }

    if (dec->debug_level >= 3) {
        static int sync_log_counter = 0;
        if ((sync_log_counter++ % 5000) == 0) {
            fprintf(stderr, "[SYNC] mode=%d d12=%.2f d19=%.2f s_lvl=%.2f\n",
                    dec->sync_mode, d12, d19, dec->s_lvl);
        }
    }

    /* Update MMSSTV sync trackers (called continuously like MMSSTV Do()) */
    if (dec->sync_mode == 0 || dec->sync_mode == 1) {
        sync_tracker_inc(&dec->sint1);
        sync_tracker_inc(&dec->sint2);
        sync_tracker_inc(&dec->sint3);
    }

    /* Sync/VIS state machine (MMSSTV parity with leader tracking)
     * Guard: do NOT run sync detection once image decoding has started —
     * the scan-line sync pulses would re-trigger case 0 and clobber sync_state. */
    if (dec->sync_state == SYNC_DATA_WAIT || !dec->vis_enabled) goto sync_done;

    /* Narrow modes identify themselves with an FSK N-VIS instead of a VIS */
    {
        sstv_mode_t nmode = decoder_decode_nvis(dec, d19, dsp);
        if (nmode != SSTV_MODE_COUNT) {
            if (dec->debug_level >= 2) {
                fprintf(stderr, "[DECODER] N-VIS decoded: 0x%02x \xe2\x86\x92 mode %d\n",
                        dec->fsk_code, nmode);
            }
            dec->detected_mode = nmode;
            dec->sync_state = SYNC_DATA_WAIT;
            dec->sync_mode = 0;
            if (!dec->image_buf.pixels)
                decoder_allocate_image_buffer(dec, nmode, IMAGE_START_NVIS);
            goto sync_done;
        }
    }
    switch (dec->sync_mode) {
        case 0:
            /* MMSSTV case 0: wait for the VIS start bit (1200 Hz). The 10 ms break
             * between the two leaders is also 1200 Hz; it is rejected by the 15 ms
             * continuity check in case 1, exactly as in MMSSTV. */
            if ((d12 > d19) && (d12 > dec->s_lvl) && ((d12 - d19) >= dec->s_lvl)) {
                if (dec->debug_level >= 2) {
                    fprintf(stderr, "[SYNC] 1200 Hz detected, validating start bit (mode 0→1)\n");
                }
                dec->sync_mode = 1;
                dec->sync_time = (int)(15.0 * dec->sample_rate / 1000.0);  /* 15ms validation */
                dec->sync_state = SYNC_DETECTED;
                sync_tracker_init(&dec->sint1);
            }
            break;
        case 1:
            /* MMSSTV: Validate START BIT continues for 15ms */
            if ((d12 > d19) && (d12 > dec->s_lvl) && ((d12 - d19) >= dec->s_lvl)) {
                /* Start bit still strong */
                dec->sync_time--;
                if (!dec->sync_time) {
                    /* Start bit validated for 15ms - now wait 30ms more before first sample */
                    if (dec->debug_level >= 2) {
                        fprintf(stderr, "[SYNC] Start bit validated, entering VIS decode (mode 1→2)\n");
                    }
                    dec->sync_mode = 2;
                    dec->sync_time = (int)(30.0 * dec->sample_rate / 1000.0);  /* 30ms per bit */
                    dec->vis_data = 0;
                    dec->vis_cnt = 8;  /* 8 bits to decode */
                    dec->vis_parity_pending = 0;
                    dec->vis_extended = 0;
                    dec->vis_inverted = 0;
                    dec->sync_state = SYNC_VIS_DECODING;
                }
            }
            else {
                /* Start bit dropped - reset */
                if (dec->debug_level >= 2) {
                    fprintf(stderr, "[SYNC] Start bit dropped during validation (mode 1→0)\n");
                }
                dec->sync_mode = 0;
                dec->sync_state = SYNC_IDLE;
            }
            break;
        case 3:
        case 4:
            /* Reserved for future use */
            dec->sync_mode = 0;
            dec->sync_state = SYNC_IDLE;
            break;
        case 2:
        case 9: {
            /* Use d11, d13 already computed at outer scope - don't run filters twice! */

            dec->sync_time--;
            if (!dec->sync_time) {
                if (dec->debug_level >= 2) {
                    fprintf(stderr, "[VIS] SAMPLE: d11=%.2f d13=%.2f d19=%.2f cnt=%d data=0x%02x\n",
                            d11, d13, d19, dec->vis_cnt, dec->vis_data & 0xFF);
                }
                /* Check if VIS tones are discriminable:
                 * - Ideally, at least one tone should be above d19
                 * - But if both are below, accept if they differ enough (s_lvl2) for discrimination
                 */
                if ((d11 < d19) && (d13 < d19) && (fabs(d11 - d13) < dec->s_lvl2)) {
                    if (dec->debug_level >= 2) {
                        fprintf(stderr, "[VIS] RESET at cnt=%d: tones not discriminable (d11=%.2f d13=%.2f d19=%.2f diff=%.2f) partial_data=0x%02x\n",
                                dec->vis_cnt, d11, d13, d19, fabs(d11 - d13), dec->vis_data & 0xFF);
                    }
                    dec->sync_mode = 0;
                    dec->sync_state = SYNC_IDLE;
                } else {
                    dec->sync_time = (int)(30.0 * dec->sample_rate / 1000.0);
                    
                    /* VIS decode: LSB-first to match encoder
                     * Encoder transmits bit 0 first, bit 7 last
                     * vis_cnt counts down from 8 to 0 (8 bits total)
                     * 
                     * Bit polarity: d11 > d13 (1080 Hz) = bit 1, d13 > d11 (1320 Hz) = bit 0
                     */
                    int bit_pos = 8 - dec->vis_cnt;  /* Position 0 to 7 */
                    if (d11 > d13) {
                        /* 1080 Hz detected = bit 1 */
                        dec->vis_data |= (1 << bit_pos);
                    }
                    /* else: 1320 Hz detected = bit 0 (default) */
                    
                    if (dec->debug_level >= 3) {
                        fprintf(stderr, "[VIS] bit %d: %s → vis_data=0x%02x (d11=%.0f d13=%.0f)\n",
                                8 - dec->vis_cnt, (d11 > d13) ? "1" : "0", 
                                (uint8_t)dec->vis_data, d11, d13);
                    }
                    
                    dec->vis_cnt--;
                    if (!dec->vis_cnt) {
                        /* All 8 bits decoded (7 data + 1 parity) */
                        int parity_bit = (dec->vis_data >> 7) & 1;  /* MSB is parity */
                        int data_bits = dec->vis_data & 0x7F;  /* Lower 7 bits are data */
                        int calculated_parity = __builtin_popcount(data_bits) & 1;
                        
                        if (dec->debug_level >= 2) {
                            fprintf(stderr, "[VIS] Complete: 0x%02x data=0x%02x parity_rx=%d calc=%d %s\n",
                                    (uint8_t)dec->vis_data, data_bits, parity_bit, calculated_parity,
                                    (parity_bit == calculated_parity) ? "OK" : "FAIL");
                        }
                        
                        /* Accept VIS even if parity fails (for robustness) */
                        if (dec->sync_mode == 2) {
                            if (data_bits == 0x23 || data_bits == 0x5C) {
                                /* Extended VIS code follows.
                                 * 0x23 = normal polarity; 0x5C = ~0x23 & 0x7F, inverted-polarity encoding */
                                dec->sync_mode = 9;
                                dec->vis_data = 0;
                                dec->vis_cnt = 8;
                                dec->vis_extended = 1;
                                dec->vis_inverted = (data_bits == 0x5C) ? 1 : 0;
                            } else {
                                /* Look up mode; try normal polarity first, then bit-inverted
                                 * (handles WAV files that encode VIS 0 → 1080 Hz, 1 → 1320 Hz) */
                                uint8_t raw = (uint8_t)dec->vis_data;
                                sstv_mode_t mode = vis_code_to_mode(raw, 0);
                                if (mode == SSTV_MODE_COUNT)
                                    mode = vis_code_to_mode((uint8_t)(raw ^ 0xFF), 0);
                                if (mode != SSTV_MODE_COUNT) {
                                    dec->detected_mode = mode;
                                    dec->sync_state = SYNC_DATA_WAIT;
                                    if (dec->debug_level >= 2) {
                                        fprintf(stderr, "[DECODER] VIS decoded: 0x%02x \xe2\x86\x92 mode %d\n",
                                                raw, mode);
                                    }
                                    /* Allocate immediately so samples in this same batch aren't dropped */
                                    if (!dec->image_buf.pixels)
                                        decoder_allocate_image_buffer(dec, mode, IMAGE_START_VIS);
                                } else if (dec->debug_level >= 2) {
                                    fprintf(stderr, "[VIS] VIS code 0x%02x (or 0x%02x) not recognized\n",
                                            raw, (uint8_t)(raw ^ 0xFF));
                                }
                                dec->sync_mode = 0;
                            }
                        } else {  /* sync_mode == 9: extended VIS */
                            /* If the first byte was seen in inverted polarity, invert the second byte too */
                            uint8_t ext_code = dec->vis_inverted
                                    ? (uint8_t)(dec->vis_data ^ 0xFF)
                                    : (uint8_t)dec->vis_data;
                            sstv_mode_t mode = vis_code_to_mode(ext_code, 1);
                            if (mode != SSTV_MODE_COUNT) {
                                dec->detected_mode = mode;
                                dec->sync_state = SYNC_DATA_WAIT;
                                if (dec->debug_level >= 2) {
                                    fprintf(stderr, "[DECODER] VIS decoded: 0x%02x \xe2\x86\x92 mode %d (extended)\n",
                                            ext_code, mode);
                                }
                                /* Allocate immediately so samples in this same batch aren't dropped */
                                if (!dec->image_buf.pixels)
                                    decoder_allocate_image_buffer(dec, mode, IMAGE_START_VIS);
                            }
                            dec->sync_mode = 0;
                        }
                    }
                }
            }
            break;
        }
        default:
            dec->sync_mode = 0;
            dec->sync_state = SYNC_IDLE;
            break;
    }
sync_done:;
}

/**
 * Convert VIS code to SSTV mode
 * 
 * @param vis_code 8-bit VIS code (after bit accumulation)
 * @param is_extended 1 if this is an extended (16-bit) VIS code
 * @return SSTV mode or SSTV_MODE_COUNT if not found
 */
static sstv_mode_t vis_code_to_mode(uint8_t vis_code, int is_extended) {
    /* Special case: 0x23 is the prefix for extended VIS (16-bit) */
    if (!is_extended && vis_code == 0x23) {
        return SSTV_MODE_COUNT; /* Signal to expect extended VIS */
    }
    
    /* Search lookup table */
    for (size_t i = 0; i < VIS_MAP_SIZE; i++) {
        if (VIS_CODE_MAP[i].vis_code == vis_code) {
            sstv_mode_t mode = VIS_CODE_MAP[i].mode;
            
            /* Disambiguate extended codes that share same byte value (e.g. 0x86) */
            if (is_extended) {
                /* Extended (0x23-prefixed) VIS: MR, MP, ML series only */
                if (mode >= SSTV_MR73 && mode <= SSTV_ML320) {
                    return mode;
                }
            } else {
                /* Standard VIS. MR/MP/ML second bytes are intentionally allowed here
                 * because some test WAV files encode them as plain 8-bit VIS. */
                return mode;
            }
        }
    }
    
    return SSTV_MODE_COUNT; /* Not found */
}

/**
 * Try decoding VIS by sweeping phase offsets over buffered energies.
 */
static int decoder_try_vis_from_buffer(sstv_decoder_t *dec, sstv_mode_t *mode_out) {
    if (!dec || !mode_out || !dec->vis.mark_buf || !dec->vis.space_buf) {
        return 0;
    }

    const double bit_durations[] = { 0.029, 0.030, 0.031 };
    const int step = (int)(0.002 * dec->sample_rate); /* 2ms phase step */
    const int step_samples = (step > 0) ? step : 1;

    sstv_mode_t best_mode = SSTV_MODE_COUNT;
    uint8_t best_code = 0x00;
    double best_conf = 0.0;

    for (size_t bd = 0; bd < sizeof(bit_durations) / sizeof(bit_durations[0]); bd++) {
        int bit_samples = (int)(bit_durations[bd] * dec->sample_rate);
        if (bit_samples <= 0) {
            continue;
        }

        int required_samples = bit_samples * 8;
        if (dec->vis.buf_pos < required_samples) {
            continue;
        }

        for (int start_pos = 0; start_pos <= dec->vis.buf_pos - required_samples; start_pos += step_samples) {
            uint8_t data = 0x00;
            double conf = 0.0;
            int ok = 1;

            for (int bit = 0; bit < 8; bit++) {
                int start = start_pos + bit * bit_samples;
                int end = start + bit_samples;
                if (end > dec->vis.buf_pos) {
                    ok = 0;
                    break;
                }

                double sum_mark = 0.0;
                double sum_space = 0.0;
                for (int i = start; i < end; i++) {
                    sum_mark += dec->vis.mark_buf[i];
                    sum_space += dec->vis.space_buf[i];
                }

                double avg_mark = sum_mark / (double)bit_samples;
                double avg_space = sum_space / (double)bit_samples;
                double diff = avg_mark - avg_space;
                conf += fabs(diff);

                data >>= 1;
                if (diff > 0.0) {
                    data |= 0x80;
                }
            }

            if (!ok) {
                continue;
            }

            if (!vis_parity_ok(data)) {
                continue;
            }
            sstv_mode_t mode = vis_code_to_mode(data, 0);
            if (mode != SSTV_MODE_COUNT && conf > best_conf) {
                best_mode = mode;
                best_code = data;
                best_conf = conf;
            }
        }
    }

    if (best_mode != SSTV_MODE_COUNT) {
        dec->vis.data = best_code;
        dec->vis.bit_count = 8;
        *mode_out = best_mode;
        if (dec->debug_level >= 2) {
            fprintf(stderr, "[DECODER] VIS decoded (buffered): 0x%02x → mode %d\n", best_code, best_mode);
        }
        return 1;
    }

    return 0;
}

static int vis_parity_ok(uint8_t vis_code) {
    uint8_t data = vis_code & 0x7F;
    int parity = (vis_code >> 7) & 1;
    int ones = 0;
    for (int i = 0; i < 7; i++) {
        if (data & (1u << i)) {
            ones++;
        }
    }
    return (ones % 2) == parity;
}

/**
 * Allocate image buffer for the detected mode
 * 
 * @param dec Decoder handle
 * @param mode SSTV mode
 * @return 0 on success, -1 on error
 */
static void decoder_finalize_image(sstv_decoder_t *dec) {
    if (!dec || !dec->image_buf.pixels || dec->img_dec.state == IMAGE_COMPLETE) {
        return;
    }

    if (dec->lnd_img_line < dec->image_buf.height) {
        bool has_data = false;
        for (size_t i = 0; i < dec->ch0_buf.size(); ++i) {
            if (fabs(dec->ch0_buf[i]) > 0.0001 || fabs(dec->ch1_buf[i]) > 0.0001 ||
                fabs(dec->ch2_buf[i]) > 0.0001 || fabs(dec->ch3_buf[i]) > 0.0001) {
                has_data = true;
                break;
            }
        }
        if (has_data || dec->lnd_img_line > 0) {
            lnd_flush_line(dec);
        }
    }

    if (dec->lnd_img_line >= dec->image_buf.height || dec->image_buf.current_line >= dec->image_buf.height) {
        dec->img_dec.state = IMAGE_COMPLETE;
        dec->last_status = SSTV_RX_IMAGE_READY;
        if (dec->debug_level >= 2) {
            fprintf(stderr, "[DECODER] Finalized image after end-of-stream (%d lines)\n",
                    dec->lnd_img_line);
        }
    }
}

static int decoder_allocate_image_buffer(sstv_decoder_t *dec, sstv_mode_t mode, int start_kind) {
    if (!dec) return -1;
    if (mode < 0 || mode >= SSTV_MODE_COUNT) return -1;

    const sstv_mode_info_t *info = sstv_get_mode_info(mode);
    if (!info) return -1;

    /* Free existing buffer */
    if (dec->image_buf.pixels) {
        free(dec->image_buf.pixels);
        dec->image_buf.pixels = NULL;
    }

    /* Allocate RGB24 pixel buffer */
    dec->image_buf.width          = info->width;
    dec->image_buf.height         = info->height;
    dec->image_buf.bytes_per_pixel = 3;
    size_t buf_sz = (size_t)info->width * info->height * 3;
    dec->image_buf.pixels = (uint8_t*)malloc(buf_sz);
    if (!dec->image_buf.pixels) {
        dec->image_buf.width = dec->image_buf.height = 0;
        return -1;
    }
    memset(dec->image_buf.pixels, 0, buf_sz);
    dec->image_buf.current_line = 0;
    dec->image_buf.current_col  = 0;

    /*
     * Compute sample-based scan line timing from the ms-based table.
     * All channel offsets are RELATIVE TO END OF SYNC (post m_OF), matching
     * the `ps -= m_OF` step in MMSSTV DrawSSTVNormal.
     */
    const scan_timing_ms_t *t = &SCAN_TIMING[mode];
    double k = dec->sample_rate / 1000.0;   /* samples per ms */
    dec->scan_TW   = t->tw_ms  * k;
    dec->scan_OF   = t->of_ms  * k;
    dec->scan_KS   = t->ks_ms  * k;
    dec->scan_KSS  = (t->kss_div > 0)  ? dec->scan_KS  * (1.0 - 1.0 / t->kss_div)
                                        : dec->scan_KS;
    dec->scan_SG   = t->sg_ms  * k;
    dec->scan_CG   = t->cg_ms  * k;
    dec->scan_SB   = t->sb_ms  * k;
    dec->scan_CB   = t->cb_ms  * k;
    dec->scan_KS2  = t->ks2_ms * k;
    /* MMSSTV uses the same divisor for KS2S as for KSS, except MR73 (1024 vs 640) */
    int ks2s_div = (mode == SSTV_MR73) ? 1024 : t->kss_div;
    dec->scan_KS2S = (ks2s_div > 0 && dec->scan_KS2 > 0.0)
                     ? dec->scan_KS2 * (1.0 - 1.0 / ks2s_div)
                     : dec->scan_KS2;
    /* SCAN_YC ch1/ch2 map via KS2S; if no ks2 defined, fall back to KSS */
    if (dec->scan_KS2S < 1.0) dec->scan_KS2S = dec->scan_KSS;
    dec->scan_color_type = t->color_type;
    dec->scan_row_double = t->row_double;
    dec->scan_sync_1900  = (mode >= SSTV_MN73 && mode <= SSTV_MC180) ? 1 : 0;
    hill_set_width(dec, dec->scan_sync_1900);   /* narrow video band for *-N modes */

    /* Per-column channel accumulation buffers (one double per image column) */
    dec->ch0_buf.assign(info->width, 0.0);
    dec->ch1_buf.assign(info->width, 0.0);
    dec->ch2_buf.assign(info->width, 0.0);
    dec->ch3_buf.assign(info->width, 0.0);
    dec->r36_ry.assign(info->width, 0.0);
    dec->r36_by.assign(info->width, 0.0);
    dec->r36_sep_sum = 0.0;
    dec->r36_sep_n   = 0;
    dec->r36_dsel    = 1;   /* line 0 carries R-Y (toggle rule picks 0 first) */

    /* Offset line_pos by the VIS stop-bit duration so that the scan sync
     * at the start of line 0 aligns correctly with scan_OF.
     * At VIS complete the stop bit (30 ms) is still pending; after that comes
     * the scan sync (scan_OF samples = sync pulse + porch).  By starting
     * line_pos at –stop_bit_samples the TW counter naturally wraps at end-of-
     * line, and the scan sync falls in the [0, OF) guard region.
     *
     * Scottie special case: write_line_sct emits [sep, G, sep, B, SYNC, sep, R].
     * For line 0, a 9 ms intro SYNC is prepended before write_line_sct.  The
     * first *real* scan SYNC (the one that aligns with the R channel) therefore
     * appears this many ms after the VIS stop-bit ends:
     *   preamble_ms = 9.0 + (tw_ms − of_ms − ks_ms)
     *               = extra_sync + sep1 + G + sep2 + B  (= 288.48 ms for SCT1)
     * We must add this preamble to the initial negative offset so that
     * line_pos reaches 0 exactly when the first scan SYNC fires. */
    {
        /* Time from header decode to the start of line 0.
         *  VIS:   the last VIS bit is sampled 15 ms (MMSSTV case-1 validation)
         *         plus ~9.5 ms (latency of the 1200 Hz detector: iir12 + 50 Hz
         *         LPF + threshold) after that bit starts, leaving 30 - 24.5 =
         *         5.5 ms of the bit plus the 30 ms stop bit.
         *  N-VIS: bits are sampled half a bit (11 ms) plus the same detector
         *         latency after they start, so ~1.5 ms of the last bit remains.
         *  Hint:  no header was decoded; only a stop bit is assumed. */
        double lead_ms = 30.0;
        if (start_kind == IMAGE_START_VIS)  lead_ms = 35.5;
        if (start_kind == IMAGE_START_NVIS) lead_ms = 1.5;
        if (start_kind == IMAGE_START_VIS && mode == SSTV_AVT90) {
            /* MMSSTV sends VIS three times, then a 32-frame digital header
             * (32 x 17 x 9.7646 ms) and a 0.305 ms gap (TMmsstv VIS/AVT TX). */
            lead_ms += 910.0 + 910.0 + 32.0 * 17.0 * 9.7646 + 0.30514375;
        }
        double extra_preamble_samples = 0.0;
        if (mode == SSTV_SCOTTIE1 || mode == SSTV_SCOTTIE2 || mode == SSTV_SCOTTIEX) {
            double preamble_ms = 9.0 + t->tw_ms - t->of_ms - t->ks_ms;
            extra_preamble_samples = preamble_ms * k;
        }
        /* AVT has no line sync to re-lock on, so the demodulator's own delay
         * (group delay of the band-pass and Hilbert FIRs) must be added here;
         * for all other modes the sync re-lock absorbs it. */
        double demod_delay = (mode == SSTV_AVT90) ? (dec->bpftap + dec->hill_tap) / 2.0 : 0.0;
        dec->line_pos = -(lead_ms * k + extra_preamble_samples + demod_delay);
    }
    /* Expected sync-peak position for per-line re-lock convergence: the
     * mode-specific MMSSTV m_OFP value (SetSampFreq). Scottie lines are framed
     * to start at their (mid-line) sync pulse, so m_OFP applies there too.
     * AVT has no line sync (ofp_ms = 0), which disables re-lock. */
    if (t->ofp_ms > 0.0) {
        dec->sync_ofp = t->ofp_ms * k;
    } else {
        dec->sync_ofp = 0.0;
    }
    dec->sync_peak_val = 0.0;
    dec->sync_peak_pos = 0;
    dec->lnd_img_line  = 0;
    dec->img_dec.state = IMAGE_DECODE_R;   /* actively decoding */

    if (dec->debug_level >= 2) {
        fprintf(stderr, "[DECODER] Allocated image %dx%d mode=%s type=%d\n",
                info->width, info->height, info->name, (int)t->color_type);
        fprintf(stderr, "[DECODER] TW=%.1f OF=%.1f KS=%.1f KSS=%.1f "
                "SG=%.1f CG=%.1f SB=%.1f CB=%.1f KS2S=%.1f\n",
                dec->scan_TW, dec->scan_OF, dec->scan_KS, dec->scan_KSS,
                dec->scan_SG, dec->scan_CG, dec->scan_SB, dec->scan_CB, dec->scan_KS2S);
    }
    return 0;
}

/**
 * CHILL Hilbert FM demodulator (port of MMSSTV CHILL::Do())
 *
 * Computes instantaneous frequency from the analytic signal formed by the
 * real input and its Hilbert-transformed quadrature component.  The MakeHilbert
 * tap design in dsp_filters.cpp uses a negated convention so the output
 * phase increment is -2*pi*f/Fs, which makes the output positive at 1500 Hz
 * (black) and negative at 2300 Hz (white), matching m_Buf storage in MMSSTV.
 *
 * Output range: +16384 at 1500 Hz (black), -16384 at 2300 Hz (white).
 */
/*
 * Demodulator centre and span (port of MMSSTV CHILL::SetWidth): 1900 Hz / 800 Hz
 * (1500..2300 Hz video) normally, or NARROW_CENTER / NARROW_BW (2044..2300 Hz
 * video) for the narrow modes. m_OFF and m_OUT are multiplied/divided by 2^df
 * to match the wider phase-difference window at higher sample rates.
 */
static void hill_set_width(sstv_decoder_t *dec, int narrow) {
    const double kNarrowLow = 2044.0, kNarrowHigh = 2300.0;
    double center = narrow ? (kNarrowHigh + kNarrowLow) / 2.0 : 1900.0;
    double span   = narrow ? (kNarrowHigh - kNarrowLow) : 800.0;
    double df_mult = (dec->hill_df == 2) ? 4.0 : (dec->hill_df == 1) ? 2.0 : 1.0;
    dec->hill_off       = (2.0 * M_PI * center) / dec->sample_rate * df_mult;
    dec->hill_out_scale = 32768.0 * dec->sample_rate / (2.0 * M_PI * span) / df_mult;
}

static double hill_do(sstv_decoder_t *dec, double in) {
    /* Quadrature component via Hilbert FIR */
    double quad = sstv_dsp::DoFIR(dec->hill_h.data(), dec->hill_z.data(), in, dec->hill_tap);
    /* Delayed real component: center tap of delay line (MMSSTV: *m_ph = Z[m_htap]) */
    double a = dec->hill_z[dec->hill_tap / 2];
    /* Instantaneous phase — mirrors MMSSTV: if( a ) a = atan2(d, a) */
    if (a != 0.0) a = atan2(quad, a);
    /* Phase difference against history window (depth = 2^hill_df samples).
     * Our MakeHilbert FIR (h[i]=-(normal)) outputs +cos for sine input, making
     * atan2(quad,real)=π/2−φ (phase decreasing with time).  Therefore the
     * correct forward-frequency difference is  diff = a − A[0]  (+Δφ convention).
     * Combined with hill_off sign: at 1900 Hz diff+hill_off=0 → sig=0 (mid-gray). */
    double diff = a - dec->hill_A[0];
    /* Advance phase history ring — exact port of CHILL::Do() switch(m_df) */
    /* Advance phase history ring — exact port of CHILL::Do() switch(m_df) */
    switch (dec->hill_df) {
        case 1:
            dec->hill_A[0] = dec->hill_A[1];
            dec->hill_A[1] = a;
            break;
        case 2:
            dec->hill_A[0] = dec->hill_A[1];
            dec->hill_A[1] = dec->hill_A[2];
            dec->hill_A[2] = dec->hill_A[3];
            dec->hill_A[3] = a;
            break;
        default: /* m_df == 0 */
            dec->hill_A[0] = a;
            break;
    }
    /* Unwrap */
    if      (diff >=  M_PI) diff -= 2.0 * M_PI;
    else if (diff <= -M_PI) diff += 2.0 * M_PI;
    /* Re-centre + scale (hill_off and hill_out_scale are pre-adjusted for hill_df)
     * Output: +16384 at 1500 Hz (black), -16384 at 2300 Hz (white) */
    diff += dec->hill_off;
    return dec->hill_lpf.Do(diff * dec->hill_out_scale);
}

/**
 * YCbCr → RGB conversion (port of MMSSTV YCtoRGB from ComLib.cpp).
 * BT.601-like coefficients: Y in [0,255] (128 = mid-grey at 1900 Hz),
 * RY/BY in [-128, +128] (0 = neutral chroma at 1900 Hz).
 */
static void yc_to_rgb(int Y, double RY, double BY, int *r, int *g, int *b) {
    double y = (double)(Y - 16);
    int rv = (int)(1.164457 * y + 1.596128 * RY);
    int gv = (int)(1.164457 * y - 0.813022 * RY - 0.391786 * BY);
    int bv = (int)(1.164457 * y + 2.017364 * BY);
    *r = rv < 0 ? 0 : rv > 255 ? 255 : rv;
    *g = gv < 0 ? 0 : gv > 255 ? 255 : gv;
    *b = bv < 0 ? 0 : bv > 255 ? 255 : bv;
}

/*
 * Flush one completed scan line from the per-column channel buffers into the
 * pixel output buffer, then clear the channel buffers for the next line.
 *
 * Pixel value formulas (mirror MMSSTV DrawSSTVNormal + GetPictureLevel):
 *   Luma/RGB stores: val = (16384 - sig) / 128   ∈ [0, 255]
 *   Chroma stores:   val = -sig / 128             ∈ [-128, +128]
 */
static void lnd_flush_line(sstv_decoder_t *dec) {
    if (!dec->image_buf.pixels || dec->ch0_buf.empty()) return;

    int w    = dec->image_buf.width;
    int h    = dec->image_buf.height;
    int line = dec->lnd_img_line;

    /* Helper: clamp and write one RGB pixel */
    auto write_px = [&](int row, int x, int rv, int gv, int bv) {
        if (row < 0 || row >= h || x < 0 || x >= w) return;
        uint8_t *p = dec->image_buf.pixels + ((size_t)row * w + x) * 3;
        p[0] = (uint8_t)(rv < 0 ? 0 : rv > 255 ? 255 : rv);
        p[1] = (uint8_t)(gv < 0 ? 0 : gv > 255 ? 255 : gv);
        p[2] = (uint8_t)(bv < 0 ? 0 : bv > 255 ? 255 : bv);
    };

    switch (dec->scan_color_type) {
    case SCAN_RGB:
        for (int x = 0; x < w; x++)
            write_px(line, x, (int)(dec->ch0_buf[x] + 0.5),
                              (int)(dec->ch1_buf[x] + 0.5),
                              (int)(dec->ch2_buf[x] + 0.5));
        dec->lnd_img_line++;
        break;

    case SCAN_MRT:
        /* MRT channel order: ch0=G, ch1=B, ch2=R — reorder at output */
        for (int x = 0; x < w; x++)
            write_px(line, x, (int)(dec->ch2_buf[x] + 0.5),   /* R */
                              (int)(dec->ch0_buf[x] + 0.5),   /* G */
                              (int)(dec->ch1_buf[x] + 0.5));  /* B */
        dec->lnd_img_line++;
        break;

    case SCAN_YC:
    case SCAN_R36: {
        if (dec->scan_color_type == SCAN_R36) {
            /* MMSSTV m_DSEL: a clear 2300 Hz separator means B-Y, a clear 1500 Hz
             * separator means R-Y; if ambiguous, alternate from the last line. */
            double sep = dec->r36_sep_n ? dec->r36_sep_sum / dec->r36_sep_n : 0.0;
            if (sep >= 64.0)       dec->r36_dsel = 1;
            else if (sep <= -64.0) dec->r36_dsel = 0;
            else                   dec->r36_dsel = dec->r36_dsel ? 0 : 1;
            std::vector<double> &dst = dec->r36_dsel ? dec->r36_by : dec->r36_ry;
            std::copy(dec->ch2_buf.begin(), dec->ch2_buf.end(), dst.begin());
            dec->r36_sep_sum = 0.0;
            dec->r36_sep_n   = 0;
        }
        for (int x = 0; x < w; x++) {
            int rv, gv, bv;
            if (dec->scan_color_type == SCAN_R36) {
                yc_to_rgb((int)(dec->ch0_buf[x] + 0.5),
                          dec->r36_ry[x], dec->r36_by[x], &rv, &gv, &bv);
            } else {
                yc_to_rgb((int)(dec->ch0_buf[x] + 0.5),
                          dec->ch1_buf[x], dec->ch2_buf[x], &rv, &gv, &bv);
            }
            write_px(line, x, rv, gv, bv);
            if (dec->scan_row_double) write_px(line + 1, x, rv, gv, bv);
        }
        dec->lnd_img_line += dec->scan_row_double ? 2 : 1;
        break;
    }
    case SCAN_YC_PD:
        /* Four channels per scan line → two image rows */
        for (int x = 0; x < w; x++) {
            int rv, gv, bv;
            yc_to_rgb((int)(dec->ch0_buf[x] + 0.5),
                      dec->ch1_buf[x], dec->ch2_buf[x], &rv, &gv, &bv);
            write_px(line, x, rv, gv, bv);
            yc_to_rgb((int)(dec->ch3_buf[x] + 0.5),
                      dec->ch1_buf[x], dec->ch2_buf[x], &rv, &gv, &bv);
            write_px(line + 1, x, rv, gv, bv);
        }
        dec->lnd_img_line += 2;
        break;

    case SCAN_BW:
        for (int x = 0; x < w; x++) {
            int y = (int)(dec->ch0_buf[x] + 0.5);
            y = y < 0 ? 0 : y > 255 ? 255 : y;
            write_px(line, x, y, y, y);
            if (dec->scan_row_double) write_px(line + 1, x, y, y, y);
        }
        dec->lnd_img_line += dec->scan_row_double ? 2 : 1;
        break;
    }

    /* Keep image_buf.current_line in sync for status reporting */
    dec->image_buf.current_line = dec->lnd_img_line;

    /* Clear channel buffers for the next scan line */
    std::fill(dec->ch0_buf.begin(), dec->ch0_buf.end(), 0.0);
    std::fill(dec->ch1_buf.begin(), dec->ch1_buf.end(), 0.0);
    std::fill(dec->ch2_buf.begin(), dec->ch2_buf.end(), 0.0);
    std::fill(dec->ch3_buf.begin(), dec->ch3_buf.end(), 0.0);
}

/*
 * Per-sample image decoder — line-position state machine.
 *
 * Each call advances the position by 1 sample within the current scan line.
 * When the position crosses a channel boundary the sample is mapped to a
 * pixel column and stored in the appropriate channel buffer.  At end-of-line
 * lnd_flush_line() converts the accumulated buffers into image rows.
 *
 * Channel identification mirrors MMSSTV DrawSSTVNormal:
 *   ps < scan_OF              → sync/guard region (skip)
 *   ps_act < scan_KS          → ch0 (R, Y_odd, or Y for YC/BW)
 *   scan_SG ≤ ps_act < scan_CG → ch1 (G, R-Y)
 *   scan_SB ≤ ps_act < scan_CB → ch2 (B, B-Y)
 *   scan_CB ≤ ps_act < CB+KS  → ch3 (Y_even, PD modes only)
 */
static void decoder_process_image_sample(sstv_decoder_t *dec, double sig) {
    if (!dec || !dec->image_buf.pixels) return;
    if (dec->img_dec.state == IMAGE_COMPLETE) return;

    double ps = dec->line_pos;
    dec->line_pos += 1.0;

    /*
     * Per-line sync tracking (MMSSTV m_SyncPos): find the position of the
     * strongest sync-tone energy anywhere in the current scan line. The sync
     * tone is 1200 Hz, or 1900 Hz for the narrow modes (MMSSTV stores d19 in
     * m_B12 when m_fNarrow). The peak is consumed and reset at end-of-line.
     */
    if (ps >= 0.0) {
        double sync_lvl = dec->scan_sync_1900 ? dec->d19_last : dec->d12_last;
        if (sync_lvl > dec->sync_peak_val) {
            dec->sync_peak_val = sync_lvl;
            dec->sync_peak_pos = (int)ps;
        }
    }

    /* End of scan line: apply re-lock nudge, then flush and advance */
    if (dec->line_pos >= dec->scan_TW) {
        dec->line_pos -= dec->scan_TW;

        /* Sync re-lock: nudge line_pos toward the expected sync-peak position
         * (sync_ofp, MMSSTV m_OFP). The error is wrapped into ±TW/2 so a sync
         * that arrives before the decoder's line start (decoder running late)
         * is seen as a negative error. Skip the first line to let tracking
         * warm up, and require a peak above noise/picture level.
         *   – Proportional term: half of the error, clamped to ±15 % of the sync
         *     region, applied every line.
         *   – Timing correction (optional): integral term that learns a steady
         *     per-line drift (TX/RX clock mismatch) from the average error over
         *     8 lines, so the proportional term does not have to hold a
         *     permanent phase offset.
         * NOTE: allow small negative line_pos — those samples fall in the guard
         * region ps < scan_OF. Never add scan_TW to "fix" a small negative. */
        if (dec->lnd_img_line > 0 && dec->sync_ofp > 0.0
                && dec->sync_peak_val > dec->s_lvl * 0.5) {
            double error = (double)dec->sync_peak_pos - dec->sync_ofp;
            if (error > dec->scan_TW * 0.5)   error -= dec->scan_TW;
            if (error <= -dec->scan_TW * 0.5) error += dec->scan_TW;

            double limit = dec->scan_OF * 0.15;
            double p_err = error;
            if (p_err > limit)  p_err = limit;
            if (p_err < -limit) p_err = -limit;
            dec->line_pos -= p_err * 0.5;

            if (dec->timing_correction_enabled) {
                dec->timing_error_accum += p_err;
                dec->timing_error_count++;
                if (dec->timing_error_count >= 8) {
                    dec->timing_error = dec->timing_error_accum / (double)dec->timing_error_count;
                    dec->timing_error_accum = 0.0;
                    dec->timing_error_count = 0;
                    dec->timing_phase_sign = (dec->timing_error >= 0.0) ? 1 : -1;
                    dec->timing_correction += dec->timing_error * dec->timing_correction_gain;
                    /* Cap at 1000 ppm of the line length */
                    double max_corr = dec->scan_TW * 0.001;
                    if (dec->timing_correction > max_corr)  dec->timing_correction = max_corr;
                    if (dec->timing_correction < -max_corr) dec->timing_correction = -max_corr;
                }
            }
        }
        if (dec->timing_correction_enabled) {
            dec->line_pos -= dec->timing_correction;
        }
        /* Only clamp extreme overshoot (> TW): should never happen in practice */
        if (dec->line_pos >= dec->scan_TW) dec->line_pos -= dec->scan_TW;
        dec->sync_peak_val = 0.0;
        dec->sync_peak_pos = 0;

        lnd_flush_line(dec);
        if (dec->lnd_img_line >= dec->image_buf.height) {
            dec->img_dec.state = IMAGE_COMPLETE;
            if (dec->debug_level >= 2)
                fprintf(stderr, "[DECODER] Image decoding complete (%d lines)\n",
                        dec->lnd_img_line);
            return;
        }
    }

    /* Skip sync / guard region */
    if (ps < dec->scan_OF) return;
    double ps_act = ps - dec->scan_OF;

    /* Determine channel and intra-channel position */
    int    ch      = -1;
    double ps_in   = 0.0;
    double ks_map  = dec->scan_KSS;   /* pixel column mapping width (samples) */
    double row_bias = 0.0;

    switch (dec->scan_color_type) {
    case SCAN_BW:
        if (ps_act < dec->scan_KS)                                   { ch = 0; ps_in = ps_act; }
        break;

    case SCAN_R36:
        if      (ps_act < dec->scan_KS)                              { ch = 0; ps_in = ps_act; }
        else if (ps_act >= dec->scan_SG && ps_act < dec->scan_CG) {
            /* Separator tone (1500 = R-Y, 2300 = B-Y), chroma-scaled level */
            dec->r36_sep_sum += -sig / 128.0;
            dec->r36_sep_n++;
        }
        else if (ps_act >= dec->scan_SB && ps_act < dec->scan_CB)    { ch = 2; ps_in = ps_act - dec->scan_SB;
                                                                       ks_map = dec->scan_KS2S; }
        break;

    case SCAN_RGB:
    case SCAN_MRT:
    case SCAN_YC:
        if      (ps_act < dec->scan_KS)                              { ch = 0; ps_in = ps_act;
                                                                       ks_map = dec->scan_KSS; }
        else if (ps_act >= dec->scan_SG && ps_act < dec->scan_CG)    { ch = 1; ps_in = ps_act - dec->scan_SG;
                                                                       ks_map = (dec->scan_color_type == SCAN_YC) ? dec->scan_KS2S : dec->scan_KSS; }
        else if (ps_act >= dec->scan_SB && ps_act < dec->scan_CB)    { ch = 2; ps_in = ps_act - dec->scan_SB;
                                                                       ks_map = (dec->scan_color_type == SCAN_YC) ? dec->scan_KS2S : dec->scan_KSS; }
        break;

    case SCAN_YC_PD:
        if      (ps_act < dec->scan_KS)                                          { ch = 0; ps_in = ps_act; }
        else if (ps_act >= dec->scan_SG && ps_act < dec->scan_CG)                { ch = 1; ps_in = ps_act - dec->scan_SG; }
        else if (ps_act >= dec->scan_SB && ps_act < dec->scan_CB)                { ch = 2; ps_in = ps_act - dec->scan_SB; }
        else if (ps_act >= dec->scan_CB && ps_act < dec->scan_CB + dec->scan_KS) { ch = 3; ps_in = ps_act - dec->scan_CB; }
        /* All PD channels are the same width — ks_map stays scan_KSS */
        break;
    }

    if (ch < 0) return;   /* separator region */

    /* Map intra-channel position to pixel column using the mode timing table
     * directly; do not add arbitrary width compression or channel bias. */
    double ps_scaled = ps_in + row_bias;
    if (ps_scaled < 0.0) ps_scaled = 0.0;
    if (ks_map > 0.0 && ps_scaled > ks_map) ps_scaled = ks_map;
    int x = (ks_map > 0.0) ? (int)(ps_scaled * dec->image_buf.width / ks_map) : 0;
    if (x < 0) x = 0;
    if (x >= dec->image_buf.width) x = dec->image_buf.width - 1;

    /*
     * Pixel value formulas (port of MMSSTV GetPictureLevel / GetPixelLevel):
     *   Luma / RGB channel:  val = (16384 - sig) / 128   → [0, 255]
     *   Chroma channel:      val = -sig / 128             → [-128, +128]
     * sig from hill_do(): +16384 = 1500 Hz (black), -16384 = 2300 Hz (white).
     */
    bool is_chroma = (dec->scan_color_type == SCAN_YC    && (ch == 1 || ch == 2)) ||
                     (dec->scan_color_type == SCAN_YC_PD && (ch == 1 || ch == 2)) ||
                     (dec->scan_color_type == SCAN_R36   && ch == 2);
    double val = is_chroma ? (-sig / 128.0) : ((16384.0 - sig) / 128.0);

    /* Store value — last-wins per column */
    switch (ch) {
    case 0: dec->ch0_buf[x] = val; break;
    case 1: dec->ch1_buf[x] = val; break;
    case 2: dec->ch2_buf[x] = val; break;
    case 3: dec->ch3_buf[x] = val; break;
    }
}

/**
 * Check if VIS has been fully decoded and extract mode
 * 
 * @param dec Decoder handle
 * @param mode_out Output: detected mode (if return value is 1)
 * @return 1 if VIS ready and mode detected, 0 otherwise
 */
static int decoder_check_vis_ready(sstv_decoder_t *dec, sstv_mode_t *mode_out) {
    if (!dec || !mode_out) return 0;
    if (dec->detected_mode == SSTV_MODE_COUNT) {
        return 0;
    }
    *mode_out = dec->detected_mode;
    return 1;
}

void sstv_decoder_set_mode_hint(sstv_decoder_t *dec, sstv_mode_t mode) {
    if (!dec) return;
    dec->mode_hint = mode;
}

void sstv_decoder_set_vis_enabled(sstv_decoder_t *dec, int enable) {
    if (!dec) return;
    dec->vis_enabled = enable ? 1 : 0;
}

void sstv_decoder_set_vis_tones(sstv_decoder_t *dec, double mark_hz, double space_hz) {
    if (!dec) return;
    if (mark_hz <= 0.0 || space_hz <= 0.0) return;
    dec->iir11.SetFreq(mark_hz, dec->sample_rate, 80.0);
    dec->iir13.SetFreq(space_hz, dec->sample_rate, 80.0);
}

void sstv_decoder_enable_timing_correction(sstv_decoder_t *dec, int enable) {
    if (!dec) return;
    dec->timing_correction_enabled = enable ? 1 : 0;
}

void sstv_decoder_set_timing_correction_gain(sstv_decoder_t *dec, double gain) {
    if (!dec) return;
    if (gain < 0.0) gain = 0.0;
    if (gain > 1.0) gain = 1.0;
    dec->timing_correction_gain = gain;
}

sstv_rx_status_t sstv_decoder_feed(
    sstv_decoder_t *dec,
    const float *samples,
    size_t sample_count
) {
    if (!dec || !samples || sample_count == 0) {
        return SSTV_RX_ERROR;
    }

    /* Apply mode hint when VIS has not yet been detected (e.g. narrow-mode files
     * with no VIS header, or files where the caller wants to force a specific mode). */
    if (dec->mode_hint != SSTV_MODE_COUNT && dec->detected_mode == SSTV_MODE_COUNT) {
        dec->detected_mode = dec->mode_hint;
        if (!dec->image_buf.pixels) {
            decoder_allocate_image_buffer(dec, dec->mode_hint, IMAGE_START_HINT);
        }
        dec->sync_state = SYNC_DATA_WAIT;
    }

    /* Process each sample through demod pipeline */
    for (size_t i = 0; i < sample_count; i++) {
        double sample = (double)samples[i];
        decoder_process_sample(dec, sample);
    }

    /* Check VIS readiness and allocate image buffer if mode detected */
    sstv_mode_t detected_mode;
    if (decoder_check_vis_ready(dec, &detected_mode)) {
        /* Allocate image buffer if not already done */
        if (!dec->image_buf.pixels) {
            if (decoder_allocate_image_buffer(dec, detected_mode, IMAGE_START_VIS) != 0) {
                if (dec->debug_level >= 1) {
                    fprintf(stderr, "[DECODER] Failed to allocate image buffer\n");
                }
                dec->last_status = SSTV_RX_ERROR;
                return SSTV_RX_ERROR;
            }
        }
        
        /* Check if image decoding is complete */
        if (dec->img_dec.state == IMAGE_COMPLETE) {
            dec->last_status = SSTV_RX_IMAGE_READY;
            return SSTV_RX_IMAGE_READY;
        }
        
        /* Still decoding image data */
        dec->last_status = SSTV_RX_NEED_MORE;
        return SSTV_RX_NEED_MORE;
    }

    /* Still accumulating data */
    dec->last_status = SSTV_RX_NEED_MORE;
    return SSTV_RX_NEED_MORE;
}

sstv_rx_status_t sstv_decoder_finish(sstv_decoder_t *dec) {
    if (!dec) {
        return SSTV_RX_ERROR;
    }

    if (dec->img_dec.state == IMAGE_COMPLETE) {
        dec->last_status = SSTV_RX_IMAGE_READY;
        return SSTV_RX_IMAGE_READY;
    }

    if (dec->image_buf.pixels) {
        decoder_finalize_image(dec);
        if (dec->img_dec.state == IMAGE_COMPLETE) {
            return SSTV_RX_IMAGE_READY;
        }
    }

    dec->last_status = SSTV_RX_NEED_MORE;
    return SSTV_RX_NEED_MORE;
}

int sstv_decoder_get_image(sstv_decoder_t *dec, sstv_image_t *out_image) {
    if (!dec || !out_image) {
        return -1;
    }
    
    /* Check if we have a decoded image */
    if (!dec->image_buf.pixels) {
        return -1;  /* No image available */
    }
    
    /* Fill output structure */
    out_image->pixels = dec->image_buf.pixels;
    out_image->width = (uint32_t)dec->image_buf.width;
    out_image->height = (uint32_t)dec->image_buf.height;
    out_image->stride = (uint32_t)(dec->image_buf.width * 3);  /* RGB24 */
    out_image->format = SSTV_RGB24;
    
    if (dec->debug_level >= 2) {
        fprintf(stderr, "[DECODER] Returning image: %dx%d\n",
                dec->image_buf.width, dec->image_buf.height);
    }
    
    return 0;
}

int sstv_decoder_get_state(sstv_decoder_t *dec, sstv_decoder_state_t *state) {
    if (!dec || !state) {
        return -1;
    }
    
    /* Return detected mode if VIS has been decoded, otherwise return hint */
    state->current_mode = (dec->detected_mode != SSTV_MODE_COUNT) 
                          ? dec->detected_mode 
                          : dec->mode_hint;
    state->vis_enabled = dec->vis_enabled;
    state->sync_detected = (dec->sync_state != SYNC_IDLE);
    state->image_ready = (dec->last_status == SSTV_RX_IMAGE_READY);
    state->current_line = dec->image_buf.current_line;
    state->total_lines = dec->image_buf.height;
    state->timing_error = dec->timing_error;
    state->timing_correction = dec->timing_correction;
    
    return 0;
}

void sstv_decoder_set_debug_level(sstv_decoder_t *dec, int level) {
    if (!dec) return;
    dec->debug_level = level;
}

void sstv_decoder_set_agc_mode(sstv_decoder_t *dec, sstv_agc_mode_t mode) {
    if (!dec) return;
    dec->agc_mode = mode;
    level_agc_init(&dec->lvl, dec->sample_rate);

    if (dec->debug_level >= 2) {
        const char *mode_names[] = {"OFF", "LOW", "MED", "HIGH", "SEMI", "AUTO"};
        if (mode < 6) {
            fprintf(stderr, "[AGC] Mode set to: %s\n", mode_names[mode]);
        }
    }
}

sstv_agc_mode_t sstv_decoder_get_agc_mode(sstv_decoder_t *dec) {
    if (!dec) return SSTV_AGC_OFF;
    return dec->agc_mode;
}

sstv_rx_status_t sstv_decoder_feed_sample(sstv_decoder_t *dec, float sample) {
    if (!dec) return SSTV_RX_ERROR;
    return sstv_decoder_feed(dec, &sample, 1);
}
