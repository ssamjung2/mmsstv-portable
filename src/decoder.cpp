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

/* === SYNC DETECTOR STATE MACHINE === */
typedef enum {
    SYNC_IDLE = 0,           /* Waiting for sync pulse */
    SYNC_DETECTED = 1,       /* Sync pulse in progress */
    SYNC_VIS_WAITING = 2,    /* VIS header expected */
    SYNC_VIS_DECODING = 3,   /* Decoding VIS bits */
    SYNC_DATA_WAIT = 4       /* Image data coming */
} sync_state_t;

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
    sstv_agc_mode_t agc_mode;        /* AGC mode for VIS */
    double agc_gain;                 /* Current AGC gain multiplier */
    double agc_peak_level;           /* Peak signal level for AGC tracking */
    int agc_sample_count;            /* Samples analyzed for AGC */
    
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
    sstv_dsp::CFIR2 bpf;            /* Bandpass FIR (shared delay line) */
    std::vector<double> hbpf;       /* MMSSTV HBPF taps */
    std::vector<double> hbpfs;      /* MMSSTV HBPFS taps */
    int bpftap;                     /* MMSSTV BPF tap count */
    int use_bpf;                    /* MMSSTV m_bpf */
    
    /* === DEMOD STATE === */
    double prev_sample;              /* For simple LPF (adjacent average) */
    level_agc_t lvl;                 /* MMSSTV AGC */
    
    /* === IMAGE BUFFER === */
    image_buffer_t image_buf;
    
    /* === SYNC TRACKING === */
    int sync_mode;                   /* MMSSTV sync state (m_SyncMode) */
    int sync_time;                   /* MMSSTV sync timer (m_SyncTime) */
    int vis_data;                    /* MMSSTV VIS accumulator (m_VisData) */
    int vis_cnt;                     /* MMSSTV VIS bit count (m_VisCnt, 7 for data bits) */
    int vis_parity_pending;          /* Waiting to decode parity bit */
    int vis_extended;                /* MMSSTV extended VIS flag (0x23 prefix) */
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
    { 0x73, SSTV_MN73 },        /* Martin N73 (extended) */
    { 0x6E, SSTV_MN110 },       /* Martin N110 (extended) */
    { 0x8C, SSTV_MN140 },       /* Martin N140 (extended) */
    { 0x6A, SSTV_MC110 },       /* Martin C110 (extended) */
    { 0x8D, SSTV_MC140 },       /* Martin C140 (extended) */
    { 0x8E, SSTV_MC180 },       /* Martin C180 (extended) */
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
static double agc_calculate_gain(sstv_decoder_t *dec, double vis_energy);

static void level_agc_init(level_agc_t *lvl, double sample_rate);
static void level_agc_do(level_agc_t *lvl, double d);
static void level_agc_fix(level_agc_t *lvl);
static double level_agc_apply(level_agc_t *lvl, double d);
static void decoder_set_sense_levels(sstv_decoder_t *dec);

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
    
    /* Initialize AGC */
    dec->agc_mode = SSTV_AGC_AUTO;   /* Default to AUTO mode (kept for API) */
    dec->agc_gain = 1.0;
    dec->agc_peak_level = 0.0;
    dec->agc_sample_count = 0;
    
        /* Allocate VIS buffers (store ~800ms of energies) */
        dec->vis.buf_size = (int)(0.800 * sample_rate);
        if (dec->vis.buf_size < 1) {
            dec->vis.buf_size = 1;
        }
        dec->vis.mark_buf = (double *)calloc((size_t)dec->vis.buf_size, sizeof(double));
        dec->vis.space_buf = (double *)calloc((size_t)dec->vis.buf_size, sizeof(double));
        dec->vis.buf_pos = 0;
        dec->vis.buffering = 0;
    
    /* Initialize DSP filters (MMSSTV parity) */
    dec->iir11.SetFreq(1080.0, sample_rate, 80.0);
    dec->iir12.SetFreq(1200.0, sample_rate, 100.0);
    dec->iir13.SetFreq(1320.0, sample_rate, 80.0);
    dec->iir19.SetFreq(1900.0, sample_rate, 100.0);
    dec->lpf11.MakeIIR(50.0, sample_rate, 2, 0, 0);
    dec->lpf12.MakeIIR(50.0, sample_rate, 2, 0, 0);
    dec->lpf13.MakeIIR(50.0, sample_rate, 2, 0, 0);
    dec->lpf19.MakeIIR(50.0, sample_rate, 2, 0, 0);

    /* MMSSTV BPF taps: HBPF (1100-2600) and HBPFS (400-2500) */
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

/* === INTERNAL HELPERS === */

/*
 * Calculate AGC gain based on current mode and signal level
 * Returns gain multiplier to be applied to vis_energy values
 */
static double agc_calculate_gain(sstv_decoder_t *dec, double vis_energy) {
    if (!dec) return 1.0;
    
    /* Track peak signal level */
    double abs_energy = fabs(vis_energy);
    if (abs_energy > dec->agc_peak_level) {
        dec->agc_peak_level = abs_energy;
    }
    dec->agc_sample_count++;
    
    /* Calculate gain based on AGC mode */
    switch (dec->agc_mode) {
        case SSTV_AGC_OFF:
            return 1.0;
            
        case SSTV_AGC_LOW:
            /* 5% gain if signal is weak (< 0.05) */
            return (dec->agc_peak_level < 0.05) ? 1.05 : 1.0;
            
        case SSTV_AGC_MED:
            /* 10% gain if signal is weak (< 0.05) */
            return (dec->agc_peak_level < 0.05) ? 1.10 : 1.0;
            
        case SSTV_AGC_HIGH:
            /* 20% gain if signal is weak (< 0.05) */
            return (dec->agc_peak_level < 0.05) ? 1.20 : 1.0;
            
        case SSTV_AGC_SEMI:
            /* Fixed 6dB boost (2x power = ~1.41x amplitude, use 2.0 for simplicity) */
            return 2.0;
            
        case SSTV_AGC_AUTO:
            /* Auto-select mode based on peak level */
            if (dec->agc_peak_level < 0.02) {
                return 1.30;  /* Very strong boost for very weak signals */
            } else if (dec->agc_peak_level < 0.05) {
                return 1.15;  /* Medium boost for weak signals */
            }
            return 1.0;  /* No boost for strong signals */
            
        default:
            return 1.0;
    }
}

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

static void decoder_reset_state(sstv_decoder_t *dec) {
    if (!dec) return;
    
    /* Reset sync/VIS state machine (MMSSTV) */
    dec->sync_state = SYNC_IDLE;
    dec->sync_mode = 0;
    dec->sync_time = 0;
    dec->vis_data = 0;
    dec->vis_cnt = 0;
    dec->vis_extended = 0;
    
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
    
    /* Reset AGC tracking for fresh VIS detection */
    dec->agc_peak_level = 0.0;
    dec->agc_sample_count = 0;

    /* Reset demod state */
    dec->prev_sample = 0.0;
    level_agc_init(&dec->lvl, dec->sample_rate);
    
    /* Clear image buffer */
    if (dec->image_buf.pixels) {
        free(dec->image_buf.pixels);
        dec->image_buf.pixels = NULL;
    }
    dec->image_buf.width = 0;
    dec->image_buf.height = 0;
    dec->image_buf.current_line = 0;
    dec->image_buf.current_col = 0;
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

    /* BPF (MMSSTV: HBPFS before sync, HBPF after) */
    if (dec->use_bpf) {
        if (dec->sync_mode >= 3 && !dec->hbpf.empty()) {
            d = dec->bpf.Do(d, dec->hbpf.data());
        } else if (!dec->hbpfs.empty()) {
            d = dec->bpf.Do(d, dec->hbpfs.data());
        }
    }

    /* AGC (MMSSTV) */
    level_agc_do(&dec->lvl, d);
    level_agc_fix(&dec->lvl);
    double ad = level_agc_apply(&dec->lvl, d);

    d = ad * 32.0;
    if (d > 16384.0) d = 16384.0;
    if (d < -16384.0) d = -16384.0;

    /* Tone detectors + 50 Hz LPF (MMSSTV) */
    double d12 = dec->iir12.Do(d);
    if (d12 < 0.0) d12 = -d12;
    d12 = dec->lpf12.Do(d12);

    double d19 = dec->iir19.Do(d);
    if (d19 < 0.0) d19 = -d19;
    d19 = dec->lpf19.Do(d19);

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

    /* Sync/VIS state machine (MMSSTV parity with leader tracking) */
    switch (dec->sync_mode) {
        case 0:
            /* MMSSTV: Check for leader using sint1 (primary tracker) */
            if ((d12 > d19) && (d12 > dec->s_lvl) && ((d12 - d19) >= dec->s_lvl)) {
                /* Track sync peak */
                sync_tracker_max(&dec->sint2, (int)d12);
                sync_tracker_trig(&dec->sint1, (int)d12);
                
                if (dec->debug_level >= 2) {
                    fprintf(stderr, "[SYNC] Leader detected (mode 0→1)\n");
                }
                dec->sync_mode = 1;
                dec->sync_time = (int)(15.0 * dec->sample_rate / 1000.0);
                dec->sync_state = SYNC_DETECTED;
            }
            break;
        case 1:
            /* MMSSTV: Track leader peaks during 15 ms validation window */
            if ((d12 > d19) && (d12 > dec->s_lvl) && ((d12 - d19) >= dec->s_lvl)) {
                /* Continue tracking peak */
                sync_tracker_max(&dec->sint1, (int)d12);
                sync_tracker_max(&dec->sint2, (int)d12);
                
                dec->sync_time--;
                if (!dec->sync_time) {
                    /* Leader validated, proceed to VIS decode */
                    if (dec->debug_level >= 2) {
                        fprintf(stderr, "[SYNC] Leader validated, entering VIS decode (mode 1→2)\n");
                    }
                    dec->sync_mode = 2;
                    /* Wait for VIS start bit (30ms @ 1200 Hz) + sample at middle of first data bit */
                    dec->sync_time = (int)(45.0 * dec->sample_rate / 1000.0);
                    dec->vis_data = 0;
                    dec->vis_cnt = 7;  /* 7 data bits (LSB-first) + parity */
                    dec->vis_parity_pending = 0;
                    dec->vis_extended = 0;
                    dec->sync_state = SYNC_VIS_DECODING;
                }
            } else {
                /* Check if leader interval is valid before resetting */
                int leader_valid = sync_tracker_start(&dec->sint1, dec->sample_rate);
                if (!leader_valid) {
                    if (dec->debug_level >= 2) {
                        fprintf(stderr, "[SYNC] Leader lost, invalid interval (mode 1→0)\n");
                    }
                    dec->sync_mode = 0;
                    dec->sync_state = SYNC_IDLE;
                }
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
            double d11 = dec->iir11.Do(d);
            if (d11 < 0.0) d11 = -d11;
            d11 = dec->lpf11.Do(d11);

            double d13 = dec->iir13.Do(d);
            if (d13 < 0.0) d13 = -d13;
            d13 = dec->lpf13.Do(d13);

            dec->sync_time--;
            if (!dec->sync_time) {
                if (dec->debug_level >= 3) {
                    fprintf(stderr, "[VIS] d11=%.2f d13=%.2f d19=%.2f cnt=%d data=0x%02x\n",
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
                    
                    if (dec->vis_parity_pending) {
                        /* Decode parity bit: even=1300hz (0), odd=1100hz (1) */
                        int parity_bit = (d11 > d13) ? 1 : 0;
                        int calculated_parity = __builtin_popcount(dec->vis_data) & 1;  /* odd=1, even=0 */
                        
                        if (dec->debug_level >= 2) {
                            fprintf(stderr, "[VIS] 7 data bits + parity: 0x%02x parity_rx=%d calc=%d %s\n",
                                    (uint8_t)dec->vis_data, parity_bit, calculated_parity,
                                    (parity_bit == calculated_parity) ? "OK" : "FAIL");
                        }
                        
                        /* Accept VIS even if parity fails (for robustness) */
                        if (dec->sync_mode == 2) {
                            if (dec->vis_data == 0x23) {
                                dec->sync_mode = 9;
                                dec->vis_data = 0;
                                dec->vis_cnt = 7;
                                dec->vis_parity_pending = 0;
                                dec->vis_extended = 1;
                            } else {
                                sstv_mode_t mode = vis_code_to_mode((uint8_t)dec->vis_data, 0);
                                if (mode != SSTV_MODE_COUNT) {
                                    dec->detected_mode = mode;
                                    dec->sync_state = SYNC_DATA_WAIT;
                                    if (dec->debug_level >= 2) {
                                        fprintf(stderr, "[DECODER] VIS decoded: 0x%02x → mode %d\n",
                                                (uint8_t)dec->vis_data, mode);
                                    }
                                } else if (dec->debug_level >= 2) {
                                    fprintf(stderr, "[VIS] VIS code 0x%02x not recognized\n", (uint8_t)dec->vis_data);
                                }
                                dec->sync_mode = 0;
                            }
                        } else {
                            sstv_mode_t mode = vis_code_to_mode((uint8_t)dec->vis_data, 1);
                            if (mode != SSTV_MODE_COUNT) {
                                dec->detected_mode = mode;
                                dec->sync_state = SYNC_DATA_WAIT;
                                if (dec->debug_level >= 2) {
                                    fprintf(stderr, "[DECODER] VIS decoded: 0x%02x → mode %d (extended)\n",
                                            (uint8_t)dec->vis_data, mode);
                                }
                            }
                            dec->sync_mode = 0;
                        }
                    } else {
                        /* Decode data bits: VIS bits are transmitted LSB-first (slowrx-cli approach)
                         * Bits[0..6] accumulate in positions, then Bit[7] is parity
                         * vis_cnt goes from 7 down to 0
                         * Position = 7 - vis_cnt (so first bit at pos 0, last at pos 6)
                         */
                        int bit_pos = 7 - dec->vis_cnt;  /* 0 to 6 for data bits, then 7 for parity */
                        if (bit_pos < 7) {
                            /* Data bits 0-6 */
                            int bit_val = (d11 > d13) ? 1 : 0;
                            if (bit_val) dec->vis_data |= (1 << bit_pos);
                            if (dec->debug_level >= 3) {
                                fprintf(stderr, "[VIS] bit[%d]=%d vis_data=0x%02x\n",
                                        bit_pos, bit_val, (uint8_t)dec->vis_data);
                            }
                            dec->vis_cnt--;
                            if (!dec->vis_cnt) {
                                /* All 7 data bits decoded, next is parity */
                                dec->vis_parity_pending = 1;
                                if (dec->debug_level >= 2) {
                                    fprintf(stderr, "[VIS] 7 data bits collected: 0x%02x\n",
                                            (uint8_t)dec->vis_data);
                                }
                            }
                        } else if (dec->vis_parity_pending) {
                            /* Decode parity bit (bit 7) */
                            int parity_bit = (d11 > d13) ? 1 : 0;
                            dec->vis_data |= (parity_bit << 7);
                            
                            int calculated_parity = __builtin_popcount(dec->vis_data & 0x7F) & 1;  /* odd=1, even=0 */
                            
                            if (dec->debug_level >= 2) {
                                fprintf(stderr, "[VIS] 7 data bits + parity: 0x%02x parity_rx=%d calc=%d %s\n",
                                        (uint8_t)dec->vis_data, parity_bit, calculated_parity,
                                        (parity_bit == calculated_parity) ? "OK" : "FAIL");
                            }
                            
                            /* Accept VIS even if parity fails (for robustness) */
                            if (dec->sync_mode == 2) {
                                if (dec->vis_data == 0x23) {
                                    dec->sync_mode = 9;
                                    dec->vis_data = 0;
                                    dec->vis_cnt = 7;
                                    dec->vis_parity_pending = 0;
                                    dec->vis_extended = 1;
                                } else {
                                    sstv_mode_t mode = vis_code_to_mode((uint8_t)dec->vis_data, 0);
                                    if (mode != SSTV_MODE_COUNT) {
                                        dec->detected_mode = mode;
                                        dec->sync_state = SYNC_DATA_WAIT;
                                        if (dec->debug_level >= 2) {
                                            fprintf(stderr, "[DECODER] VIS decoded: 0x%02x → mode %d\n",
                                                    (uint8_t)dec->vis_data, mode);
                                        }
                                    } else if (dec->debug_level >= 2) {
                                        fprintf(stderr, "[VIS] VIS code 0x%02x not recognized\n", (uint8_t)dec->vis_data);
                                    }
                                    dec->sync_mode = 0;
                                }
                            } else {
                                sstv_mode_t mode = vis_code_to_mode((uint8_t)dec->vis_data, 1);
                                if (mode != SSTV_MODE_COUNT) {
                                    dec->detected_mode = mode;
                                    dec->sync_state = SYNC_DATA_WAIT;
                                    if (dec->debug_level >= 2) {
                                        fprintf(stderr, "[DECODER] VIS decoded: 0x%02x → mode %d (extended)\n",
                                                (uint8_t)dec->vis_data, mode);
                                    }
                                }
                                dec->sync_mode = 0;
                            }
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
            
            /* Disambiguate extended codes that share same byte value */
            if (is_extended) {
                /* Extended VIS codes: MR, MP, ML, MN, MC series */
                /* These are only valid in extended mode */
                if (mode >= SSTV_MR73 && mode <= SSTV_MC180) {
                    return mode;
                }
            } else {
                /* Standard VIS codes: exclude extended-only modes */
                if (mode < SSTV_MR73 || mode > SSTV_MC180) {
                    return mode;
                }
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

sstv_rx_status_t sstv_decoder_feed(
    sstv_decoder_t *dec,
    const float *samples,
    size_t sample_count
) {
    if (!dec || !samples || sample_count == 0) {
        return SSTV_RX_ERROR;
    }

    /* Process each sample through demod pipeline */
    for (size_t i = 0; i < sample_count; i++) {
        double sample = (double)samples[i];
        decoder_process_sample(dec, sample);
    }

    /* Check VIS readiness */
    sstv_mode_t detected_mode;
    if (decoder_check_vis_ready(dec, &detected_mode)) {
        dec->last_status = SSTV_RX_IMAGE_READY;
        return SSTV_RX_IMAGE_READY;
    }

    /* Still accumulating data */
    dec->last_status = SSTV_RX_NEED_MORE;
    return SSTV_RX_NEED_MORE;
}

int sstv_decoder_get_image(sstv_decoder_t *dec, sstv_image_t *out_image) {
    if (!dec || !out_image) {
        return -1;
    }
    return -1;
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
    
    return 0;
}

void sstv_decoder_set_debug_level(sstv_decoder_t *dec, int level) {
    if (!dec) return;
    dec->debug_level = level;
}

void sstv_decoder_set_agc_mode(sstv_decoder_t *dec, sstv_agc_mode_t mode) {
    if (!dec) return;
    dec->agc_mode = mode;
    /* Reset AGC state when mode changes */
    dec->agc_gain = 1.0;
    dec->agc_peak_level = 0.0;
    dec->agc_sample_count = 0;
    
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
