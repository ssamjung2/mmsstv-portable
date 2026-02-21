/*
================================================================================
                    HF IMPAIRMENTS DSP PIPELINE TEST
================================================================================

PURPOSE:
  Test DSP audio pipeline performance under realistic 20m HF propagation conditions.
  Applies controlled noise and fading to clean SSTV signals, then processes through
  the complete decoder filter chain to validate filter effectiveness.

WHAT THIS TEST DOES:
  1. Reads clean SSTV WAV file (recorded signal or encoded output)
  2. Applies realistic HF impairments:
     - S7 noise floor (~-109 dBm, 22-26 dB SNR typical for SSTV)
     - Rayleigh fading (QSB - slow amplitude variations)
     - Background hum (50 Hz + harmonics from mains interference)
     - Optional QRM (interfering amateur radio signals)
  3. Processes through DSP pipeline (LPF → BPF → AGC → Resonators)
  4. Writes intermediate WAV files at each stage for analysis

OUTPUT FILES:
  00_clean_input.wav     - Original signal (baseline)
  01_with_noise.wav      - After HF impairments applied
  02_after_lpf.wav       - After 2-tap simple LPF
  03_after_bpf.wav       - After bandpass FIR (HBPF/HBPFS)
  04_after_agc.wav       - After AGC normalization
  05_final.wav           - Final output (input to tone detectors)

USAGE:
  ./test_hf_impairments <input.wav> [<output_dir>] [<snr_db>]
  
  input.wav   - Clean SSTV signal (16-bit mono PCM WAV)
  output_dir  - Directory for output files (default: ./hf_test_output)
  snr_db      - Signal strength above S7 noise floor in dB (default: 12.0)

EXAMPLES:
  # S9 signal in S7 noise (strong):
  ./test_hf_impairments clean_scottie1.wav ./s9_test 12.0
  
  # S7 signal in S7 noise (marginal - barely above noise):
  ./test_hf_impairments clean_scottie1.wav ./s7_test 6.0
  
  # S5 signal in S7 noise (weak - below noise floor):
  ./test_hf_impairments clean_scottie1.wav ./s5_test 0.0

HF IMPAIRMENT MODEL:
  
  S7 Noise Floor (CONSTANT):
    - Background noise: S7 level (typical 20m HF band noise)
    - Represents: Atmospheric noise + receiver thermal noise
    - Always present at ~2000 RMS in 16-bit PCM
    
  Signal Strength (VARIABLE):
    - Controlled by snr_db parameter
    - snr_db = 12 → S9 signal (clean copy, good readability)
    - snr_db = 6 → S7 signal (marginal, noisy but readable)
    - snr_db = 0 → S5 signal (weak, difficult decode)
    
  Rayleigh Fading:
    - Rate: 0.1-0.5 Hz (slow QSB typical of 20m ionospheric skip)
    - Depth: 6-12 dB (moderate fading, not flutter)
    - Model: Low-pass filtered Gaussian random process
    
  Background Hum:
    - 50 Hz fundamental (European mains) + harmonics
    - Level: -40 dB relative to signal
    - Simulates power supply ripple in TX/RX chain
    
  QRM (Optional):
    - Random SSB/PSK31 interferers with constant S7 noise floor. Key filter performance:
  
  - BPF rejection: Should suppress out-of-band noise (50 Hz hum, AM broadcast)
  - AGC: Should normalize fading without introducing distortion
  - Resonators: Should track SSTV tones despite background noise
  
  Expected results (signal above S7 noise floor):
  - +12 dB (S9): Perfect decode, clean image
  - +6 dB (S7): Marginal decode, visible noise
  - 0 dB (S5): Difficult decode, heavy noise
  - -6 dB (S3): Very difficult, barely usabl20-30 dB of co-channel QRM
  
  Expected results:
  - S9 (35 dB SNR): Perfect decode
  - S7 (25 dB SNR): Clean decode with minor artifacts
  - S5 (18 dB SNR): Marginal decode, visible noise
  - S3 (12 dB SNR): Difficult decode, heavy noise

================================================================================
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <random>
#include <sys/stat.h>

#include "dsp_filters.h"
#include "../src/SpectralSubtractionDNR.h"

/* ============================================================================
   WAV FILE I/O
   ============================================================================ */

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

static int read_wav_header(FILE *fp, wav_info_t *info) {
    // Robust WAV header parser: scan for 'fmt ' and 'data' chunks
    uint8_t riff_header[12];
    if (fread(riff_header, 1, 12, fp) != 12) return -1;
    if (memcmp(riff_header, "RIFF", 4) != 0 || memcmp(riff_header + 8, "WAVE", 4) != 0) return -1;

    bool found_fmt = false, found_data = false;
    long fmt_offset = 0, data_offset = 0;
    uint32_t fmt_size = 0, data_size = 0;
    uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0;

    while (!found_fmt || !found_data) {
        uint8_t chunk_hdr[8];
        if (fread(chunk_hdr, 1, 8, fp) != 8) break;
        uint32_t chunk_size = *(uint32_t *)(chunk_hdr + 4);
        if (memcmp(chunk_hdr, "fmt ", 4) == 0) {
            fmt_offset = ftell(fp);
            fmt_size = chunk_size;
            uint8_t fmt_data[32] = {0};
            if (fmt_size > sizeof(fmt_data)) return -1;
            if (fread(fmt_data, 1, fmt_size, fp) != fmt_size) return -1;
            audio_format = *(uint16_t *)(fmt_data + 0);
            num_channels = *(uint16_t *)(fmt_data + 2);
            sample_rate = *(uint32_t *)(fmt_data + 4);
            bits_per_sample = *(uint16_t *)(fmt_data + 14);
            found_fmt = true;
        } else if (memcmp(chunk_hdr, "data", 4) == 0) {
            data_offset = ftell(fp);
            data_size = chunk_size;
            found_data = true;
            break;
        } else {
            // Skip this chunk
            if (fseek(fp, chunk_size, SEEK_CUR) != 0) break;
        }
    }
    if (!found_fmt || !found_data) return -1;
    info->audio_format = audio_format;
    info->num_channels = num_channels;
    info->sample_rate = sample_rate;
    info->bits_per_sample = bits_per_sample;
    info->data_offset = data_offset;
    info->data_size = data_size;
    // Seek to start of data for reading samples
    fseek(fp, data_offset, SEEK_SET);
    return 0;
}

static void write_wav_header(FILE *f, uint32_t sample_rate, uint32_t num_samples) {
    uint32_t file_size = 36 + num_samples * 2;
    uint32_t data_size = num_samples * 2;
    uint32_t byte_rate = sample_rate * 2;
    uint16_t block_align = 2;

    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);
    uint16_t audio_fmt = 1;
    fwrite(&audio_fmt, 2, 1, f);
    uint16_t channels = 1;
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    uint16_t bits = 16;
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
}

static void update_wav_header(FILE *f, uint32_t sample_rate, uint32_t num_samples) {
    fseek(f, 0, SEEK_SET);
    write_wav_header(f, sample_rate, num_samples);
    fseek(f, 0, SEEK_END);
}

static void write_sample(FILE *f, double sample) {
    int16_t s = (int16_t)(sample > 32767.0 ? 32767 : (sample < -32768.0 ? -32768 : sample));
    fwrite(&s, sizeof(int16_t), 1, f);
}

/* ============================================================================
   HF IMPAIRMENT GENERATORS
   ============================================================================ */


class HFImpairments {
public:
    struct FadeEvent {
        size_t start;
        size_t end;
        double noise_rms;
    };

    HFImpairments(double sample_rate, double snr_db, size_t total_samples, const std::vector<double>& noise_floors, std::mt19937& sweep_rng)
        : fs_(sample_rate),
          snr_db_(snr_db),
          rng_(std::random_device{}()),
          normal_dist_(0.0, 1.0),
          uniform_dist_(0.0, 1.0),
          fade_state_(1.0),
          fade_lpf_alpha_(0.0),
          hum_phase_(0.0)
    {
        // Rayleigh fading LPF: fc = 0.2 Hz (slow QSB)
        double fade_fc = 0.2;
        fade_lpf_alpha_ = 2.0 * M_PI * fade_fc / fs_;
        if (fade_lpf_alpha_ > 1.0) fade_lpf_alpha_ = 1.0;

        // Default noise floor (will be used outside fades)
        noise_floor_rms_ = noise_floors[0];

        // Randomly select two non-overlapping fade windows and noise levels
        std::uniform_int_distribution<size_t> win_dist(0, total_samples - total_samples/10 - 1);
        std::uniform_int_distribution<int> lvl_dist(0, 4);

        // Fade 1
        size_t fade1_start = win_dist(sweep_rng);
        size_t fade1_len = total_samples / 10; // 10% of image
        size_t fade1_end = fade1_start + fade1_len;
        double fade1_rms = noise_floors[lvl_dist(sweep_rng)];

        // Fade 2 (ensure non-overlapping)
        size_t fade2_start, fade2_end;
        do {
            fade2_start = win_dist(sweep_rng);
            fade2_end = fade2_start + fade1_len;
        } while ((fade2_start < fade1_end && fade2_end > fade1_start) || fade2_end > total_samples);
        double fade2_rms = noise_floors[lvl_dist(sweep_rng)];

        fade_events_.push_back({fade1_start, fade1_end, fade1_rms});
        fade_events_.push_back({fade2_start, fade2_end, fade2_rms});
    }

    /* Apply all impairments to a single sample */
    double apply(double clean_sample, size_t sample_idx, size_t /*total_samples*/) {
        // 0. Artificially reduce input signal for realism
        double scaled = clean_sample * signal_scale_;

        // 1. Rayleigh fading (slow amplitude modulation)
        double fade = generate_rayleigh_fade();
        double faded = scaled * fade;

        // 2. Check if in a fade event (increase noise floor)
        double noise_rms = noise_floor_rms_;
        for (const auto& fe : fade_events_) {
            if (sample_idx >= fe.start && sample_idx < fe.end) {
                noise_rms = fe.noise_rms;
                break;
            }
        }

        // 3. AWGN noise floor (may be increased during fade)
        double noise = normal_dist_(rng_) * noise_rms;

        // 4. Background hum (50 Hz + harmonics)
        double hum = generate_hum();

        // Combine: faded signal + noise floor + hum
        double with_impairments = faded + noise + hum;

        return with_impairments;
    }

public:
    double generate_noise_floor() {
        /* Generate constant S7 noise floor (Additive White Gaussian Noise)
         * This is INDEPENDENT of signal level - always present at S7
         * SNR parameter controls signal scaling, not noise scaling
         */
        return normal_dist_(rng_) * noise_floor_rms_;
    }

    double generate_rayleigh_fade() {
        /* Generate Rayleigh fading using filtered Gaussian process
         * Rayleigh amplitude: R = sqrt(I^2 + Q^2) where I,Q ~ N(0,1)
         * Low-pass filter to create slow fading (0.1-0.5 Hz)
         */
        double i = normal_dist_(rng_);
        double q = normal_dist_(rng_);
        double rayleigh = std::sqrt(i * i + q * q);
        
        /* Low-pass filter for slow fading */
        fade_state_ = fade_state_ * (1.0 - fade_lpf_alpha_) + rayleigh * fade_lpf_alpha_;
        
        /* Normalize to mean=1.0, limit fading depth to 6-12 dB */
        double normalized = fade_state_ / 1.253; /* E[Rayleigh] = sqrt(π/2) ≈ 1.253 */
        double depth_limited = 0.5 + 0.5 * normalized; /* Limit fade to 6 dB depth */
        
        return depth_limited;
    }

    double generate_hum() {
        /* 50 Hz + 100 Hz + 150 Hz harmonics (European mains)
         * Amplitude: -40 dB relative to signal */
        const double hum_level = 0.01; /* -40 dB */
        
        double hum = 0.0;
        hum += std::sin(hum_phase_) * 0.5;           /* 50 Hz fundamental */
        hum += std::sin(hum_phase_ * 2.0) * 0.3;     /* 100 Hz 2nd harmonic */
        hum += std::sin(hum_phase_ * 3.0) * 0.2;     /* 150 Hz 3rd harmonic */
        
        /* Increment phase */
        hum_phase_ += 2.0 * M_PI * 50.0 / fs_;
        if (hum_phase_ > 2.0 * M_PI) {
            hum_phase_ -= 2.0 * M_PI;
        }
        
        return hum * hum_level * 1000.0; /* Scale to reasonable amplitude */
    }

    double fs_;
    double snr_db_;
    double noise_floor_rms_;  /* S7 constant noise floor */
    std::vector<FadeEvent> fade_events_;
    double signal_scale_ = 0.5; /* Reduce input amplitude for realism (default 0.5) */
    std::mt19937 rng_;
    std::normal_distribution<double> normal_dist_;
    std::uniform_real_distribution<double> uniform_dist_;
    
    /* Fading state */
    double fade_state_;
    double fade_lpf_alpha_;
    
    /* Hum state */
    double hum_phase_;
};

/* ============================================================================
   LEVEL AGC (from decoder.cpp)
   ============================================================================ */

typedef struct {
    double m_dblAGC;          /* MMSSTV m_dblAGC: gain multiplier */
    double m_dblAGCTop;       /* MMSSTV m_dblAGCTop: peak level */
    double m_dblAGCMax;       /* MMSSTV m_dblAGCMax: maximum peak observed */
    double m_dblLvl;          /* MMSSTV m_dblLvl: current level */
    double m_SN;              /* MMSSTV m_SN: S/N estimate */
} level_agc_t;

static void level_agc_init(level_agc_t *agc) {
    agc->m_dblAGC = 1.0;
    agc->m_dblAGCTop = 0.0;
    agc->m_dblAGCMax = 0.0;
    agc->m_dblLvl = 0.0;
    agc->m_SN = 0.0;
}

static void level_agc_do(level_agc_t *agc, double d) {
    if (d < 0.0) d = -d;
    agc->m_dblLvl += (d - agc->m_dblLvl) * 0.2;
    if (agc->m_dblAGCTop < agc->m_dblLvl) {
        agc->m_dblAGCTop = agc->m_dblLvl;
    }
}

static void level_agc_fix(level_agc_t *agc) {
    agc->m_dblAGCTop *= 0.99995;
    if (agc->m_dblAGCTop < 1.0) agc->m_dblAGCTop = 1.0;
    if (agc->m_dblAGCMax < agc->m_dblAGCTop) {
        agc->m_dblAGCMax = agc->m_dblAGCTop;
    }
    agc->m_dblAGC = 512.0 / agc->m_dblAGCTop;
    if (agc->m_dblAGCMax > 0.0) {
        agc->m_SN = agc->m_dblAGCTop / agc->m_dblAGCMax;
    }
}

static double level_agc_apply(level_agc_t *agc, double d) {
    return d * agc->m_dblAGC;
}

/* ============================================================================
   DSP PIPELINE TEST
   ============================================================================ */

static void create_directory(const char *path) {
    mkdir(path, 0755);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.wav> [<output_dir>] [<snr_db>] [<signal_scale>] [--dsp-only]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Applies HF impairments and processes through DSP pipeline.\n");
        fprintf(stderr, "Arguments:\n");
        fprintf(stderr, "  input.wav   - Clean SSTV WAV file (16-bit mono PCM)\n");
        fprintf(stderr, "  output_dir  - Output directory (default: ./hf_test_output)\n");
        fprintf(stderr, "  snr_db      - Signal dB above S9 noise floor (default: 12.0)\n");
        fprintf(stderr, "  signal_scale  - Input amplitude scale (default: 0.5, lower = fainter signal)\n");
        fprintf(stderr, "  --dsp-only  - Disable artificial impairments, process only DSP pipeline\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Signal Strength Guidelines (above S9 noise floor):\n");
        fprintf(stderr, "  +12 dB - S9+10 signal (very strong, perfect decode)\n");
        fprintf(stderr, "  +6 dB  - S7 signal (marginal, readable with noise)\n");
        fprintf(stderr, "  0 dB   - S5 signal (weak, difficult decode)\n");
        fprintf(stderr, "  -6 dB  - S3 signal (very weak, barely usable)\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Note: Noise floor is constant at S7 (typical 20m HF band noise)\n");
        fprintf(stderr, "\n");
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_dir = (argc >= 3) ? argv[2] : "./hf_test_output";
    double snr_db = (argc >= 4) ? atof(argv[3]) : 12.0;  /* Default: S9 signal (+12 dB above S7 noise) */
    bool dsp_only = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dsp-only") == 0) {
            dsp_only = true;
        }
    }

    printf("=== HF Impairments DSP Pipeline Test ===\n");
    printf("Input: %s\n", input_path);
    printf("Output directory: %s\n", output_dir);
    printf("Signal above S7 noise floor: %.1f dB", snr_db);
    
    /* Estimate S-meter reading (rough approximation) */
    if (snr_db >= 12.0) printf(" (S9 signal)\n");
    else if (snr_db >= 6.0) printf(" (S7-S8 signal)\n");
    else if (snr_db >= 0.0) printf(" (S5-S6 signal)\n");
    else printf(" (S3-S4 signal)\n");
    
    printf("Noise floor: S7 (constant atmospheric + receiver noise)\n");
    printf("\n");

    /* Create output directory */
    create_directory(output_dir);

    /* Read input WAV */
    FILE *fp_in = fopen(input_path, "rb");
    if (!fp_in) {
        perror("Failed to open input file");
        return 1;
    }

    wav_info_t info;
    if (read_wav_header(fp_in, &info) != 0) {
        fprintf(stderr, "Invalid or unsupported WAV file\n");
        fclose(fp_in);
        return 1;
    }

    if (info.audio_format != 1 || info.num_channels != 1 || info.bits_per_sample != 16) {
        fprintf(stderr, "Only 16-bit mono PCM WAV is supported\n");
        fclose(fp_in);
        return 1;
    }

    printf("Sample rate: %u Hz\n", info.sample_rate);
    printf("Duration: %.1f seconds\n", (double)info.data_size / (info.sample_rate * 2));
    printf("\n");

    /* Read all samples */
    size_t num_samples = info.data_size / 2;
    std::vector<int16_t> pcm_input(num_samples);
    fread(pcm_input.data(), sizeof(int16_t), num_samples, fp_in);
    fclose(fp_in);

    // Write clean input WAV once
    char path_clean[512];
    snprintf(path_clean, sizeof(path_clean), "%s/00_clean_input.wav", output_dir);
    FILE *fp_clean = fopen(path_clean, "wb");
    if (!fp_clean) { fprintf(stderr, "Failed to create clean input WAV\n"); return 1; }
    write_wav_header(fp_clean, info.sample_rate, 0);
    for (size_t i = 0; i < num_samples; i++) {
        double clean = (double)pcm_input[i];
        write_sample(fp_clean, clean);
    }
    update_wav_header(fp_clean, info.sample_rate, num_samples);
    fclose(fp_clean);

    if (dsp_only) {
        // DSP-only mode: skip impairments, process clean signal through DSP pipeline
        printf("\n--- DSP-ONLY MODE: No artificial impairments applied ---\n");
        char path_dnr[512], path_lpf[512], path_bpf[512], path_agc[512], path_final[512];
        snprintf(path_dnr, sizeof(path_dnr), "%s/01b_after_dnr_clean.wav", output_dir);
        snprintf(path_lpf, sizeof(path_lpf), "%s/02_after_lpf_clean.wav", output_dir);
        snprintf(path_bpf, sizeof(path_bpf), "%s/03_after_bpf_clean.wav", output_dir);
        snprintf(path_agc, sizeof(path_agc), "%s/04_after_agc_clean.wav", output_dir);
        snprintf(path_final, sizeof(path_final), "%s/05_final_clean.wav", output_dir);
        FILE *fp_dnr = fopen(path_dnr, "wb");
        FILE *fp_lpf = fopen(path_lpf, "wb");
        FILE *fp_bpf = fopen(path_bpf, "wb");
        FILE *fp_agc = fopen(path_agc, "wb");
        FILE *fp_final = fopen(path_final, "wb");
        write_wav_header(fp_dnr, info.sample_rate, 0);
        write_wav_header(fp_lpf, info.sample_rate, 0);
        write_wav_header(fp_bpf, info.sample_rate, 0);
        write_wav_header(fp_agc, info.sample_rate, 0);
        write_wav_header(fp_final, info.sample_rate, 0);

        // DNR processing
        SpectralSubtractionDNR dnr(1024, 256); // 75% overlap
        std::vector<double> dnr_in(num_samples);
        for (size_t i = 0; i < num_samples; ++i) {
            dnr_in[i] = static_cast<double>(pcm_input[i]);
        }
        std::vector<double> dnr_out = dnr_in;
        dnr.process(dnr_out);
        for (size_t i = 0; i < num_samples; ++i) write_sample(fp_dnr, dnr_out[i]);

        // Continue pipeline with DNR output
        double prev_sample = 0.0;
        level_agc_t agc;
        level_agc_init(&agc);
        int bpftap = (int)(24.0 * info.sample_rate / 11025.0);
        std::vector<double> hbpf(bpftap + 1);
        std::vector<double> hbpfs(bpftap + 1);
        sstv_dsp::FirSpec spec_bpf;
        spec_bpf.n = bpftap;
        spec_bpf.typ = sstv_dsp::kFfBPF;
        spec_bpf.fs = info.sample_rate;
        spec_bpf.fcl = 1080.0;
        spec_bpf.fch = 2600.0;
        spec_bpf.att = 20.0;
        spec_bpf.gain = 1.0;
        sstv_dsp::MakeFilter(hbpf.data(), &spec_bpf);
        sstv_dsp::FirSpec spec_bpfs;
        spec_bpfs.n = bpftap;
        spec_bpfs.typ = sstv_dsp::kFfBPF;
        spec_bpfs.fs = info.sample_rate;
        spec_bpfs.fcl = 400.0;
        spec_bpfs.fch = 2500.0;
        spec_bpfs.att = 20.0;
        spec_bpfs.gain = 1.0;
        sstv_dsp::MakeFilter(hbpfs.data(), &spec_bpfs);
        sstv_dsp::CFIR2 bpf;
        bpf.Create(bpftap);

        uint32_t samples_written = 0;
        for (size_t i = 0; i < num_samples; i++) {
            double lpf = (dnr_out[i] + prev_sample) * 0.5;
            prev_sample = dnr_out[i];
            if (lpf > 24576.0) lpf = 24576.0;
            if (lpf < -24576.0) lpf = -24576.0;
            write_sample(fp_lpf, lpf);
            double bpf_out = bpf.Do(lpf, hbpfs.data());
            write_sample(fp_bpf, bpf_out);
            level_agc_do(&agc, bpf_out);
            level_agc_fix(&agc);
            double agc_out = level_agc_apply(&agc, bpf_out);
            write_sample(fp_agc, agc_out);
            double final = agc_out * 2.0;
            if (final > 16384.0) final = 16384.0;
            if (final < -16384.0) final = -16384.0;
            write_sample(fp_final, final);
            samples_written++;
            if ((i % 10000) == 0 && i > 0) {
                printf("\rProgress (clean): %zu/%zu samples (%.1f%%)...", i, num_samples, 100.0 * i / num_samples);
                fflush(stdout);
            }
        }
        printf("\rProgress (clean): %zu/%zu samples (100.0%%)    \n", num_samples, num_samples);
        update_wav_header(fp_dnr, info.sample_rate, num_samples);
        update_wav_header(fp_lpf, info.sample_rate, num_samples);
        update_wav_header(fp_bpf, info.sample_rate, num_samples);
        update_wav_header(fp_agc, info.sample_rate, num_samples);
        update_wav_header(fp_final, info.sample_rate, num_samples);
        fclose(fp_dnr); fclose(fp_lpf); fclose(fp_bpf); fclose(fp_agc); fclose(fp_final);

        // Print AGC stats for clean sweep
        printf("AGC Statistics (clean):\n");
        printf("  Final gain: %.2f\n", agc.m_dblAGC);
        printf("  Peak level: %.2f\n", agc.m_dblAGCTop);
        printf("  S/N ratio: %.3f\n", agc.m_SN);
        printf("\nDSP-only processing complete. Output WAVs are in %s\n", output_dir);
        return 0;
    }

    // 5-step noise floor sweep (default mode)
    std::vector<double> noise_floors = { 2000.0, 6000.0, 10000.0, 15000.0, 20000.0 };
    std::random_device rd; std::mt19937 sweep_rng(rd());
    for (int sweep = 0; sweep < 5; ++sweep) {
        printf("\n--- Noise Floor Step %d: RMS %.0f ---\n", sweep+1, noise_floors[sweep]);
        HFImpairments impairments(info.sample_rate, snr_db, num_samples, noise_floors, sweep_rng);
        impairments.noise_floor_rms_ = noise_floors[sweep]; // base noise floor for this sweep

        // Generate noisy signal
        std::vector<double> noisy(num_samples);
        for (size_t i = 0; i < num_samples; ++i) {
            double clean = (double)pcm_input[i];
            noisy[i] = impairments.apply(clean, i, num_samples);
        }

        // Write noisy WAV
        char path_noise[512], path_dnr[512], path_lpf[512], path_bpf[512], path_agc[512], path_final[512];
        snprintf(path_noise, sizeof(path_noise), "%s/01_with_noise_lvl%d.wav", output_dir, sweep+1);
        snprintf(path_dnr, sizeof(path_dnr), "%s/01b_after_dnr_lvl%d.wav", output_dir, sweep+1);
        snprintf(path_lpf, sizeof(path_lpf), "%s/02_after_lpf_lvl%d.wav", output_dir, sweep+1);
        snprintf(path_bpf, sizeof(path_bpf), "%s/03_after_bpf_lvl%d.wav", output_dir, sweep+1);
        snprintf(path_agc, sizeof(path_agc), "%s/04_after_agc_lvl%d.wav", output_dir, sweep+1);
        snprintf(path_final, sizeof(path_final), "%s/05_final_lvl%d.wav", output_dir, sweep+1);
        FILE *fp_noise = fopen(path_noise, "wb");
        FILE *fp_dnr = fopen(path_dnr, "wb");
        FILE *fp_lpf = fopen(path_lpf, "wb");
        FILE *fp_bpf = fopen(path_bpf, "wb");
        FILE *fp_agc = fopen(path_agc, "wb");
        FILE *fp_final = fopen(path_final, "wb");
        write_wav_header(fp_noise, info.sample_rate, 0);
        write_wav_header(fp_dnr, info.sample_rate, 0);
        write_wav_header(fp_lpf, info.sample_rate, 0);
        write_wav_header(fp_bpf, info.sample_rate, 0);
        write_wav_header(fp_agc, info.sample_rate, 0);
        write_wav_header(fp_final, info.sample_rate, 0);

        // Write noisy signal
        for (size_t i = 0; i < num_samples; ++i) write_sample(fp_noise, noisy[i]);

        // DNR processing
        SpectralSubtractionDNR dnr(1024, 256); // 75% overlap
        std::vector<double> dnr_out = noisy;
        dnr.process(dnr_out);
        for (size_t i = 0; i < num_samples; ++i) write_sample(fp_dnr, dnr_out[i]);

        // Continue pipeline with DNR output
        double prev_sample = 0.0;
        level_agc_t agc;
        level_agc_init(&agc);
        int bpftap = (int)(24.0 * info.sample_rate / 11025.0);
        std::vector<double> hbpf(bpftap + 1);
        std::vector<double> hbpfs(bpftap + 1);
        sstv_dsp::FirSpec spec_bpf;
        spec_bpf.n = bpftap;
        spec_bpf.typ = sstv_dsp::kFfBPF;
        spec_bpf.fs = info.sample_rate;
        spec_bpf.fcl = 1080.0;
        spec_bpf.fch = 2600.0;
        spec_bpf.att = 20.0;
        spec_bpf.gain = 1.0;
        sstv_dsp::MakeFilter(hbpf.data(), &spec_bpf);
        sstv_dsp::FirSpec spec_bpfs;
        spec_bpfs.n = bpftap;
        spec_bpfs.typ = sstv_dsp::kFfBPF;
        spec_bpfs.fs = info.sample_rate;
        spec_bpfs.fcl = 400.0;
        spec_bpfs.fch = 2500.0;
        spec_bpfs.att = 20.0;
        spec_bpfs.gain = 1.0;
        sstv_dsp::MakeFilter(hbpfs.data(), &spec_bpfs);
        sstv_dsp::CFIR2 bpf;
        bpf.Create(bpftap);

        uint32_t samples_written = 0;
        for (size_t i = 0; i < num_samples; i++) {
            double lpf = (dnr_out[i] + prev_sample) * 0.5;
            prev_sample = dnr_out[i];
            if (lpf > 24576.0) lpf = 24576.0;
            if (lpf < -24576.0) lpf = -24576.0;
            write_sample(fp_lpf, lpf);
            double bpf_out = bpf.Do(lpf, hbpfs.data());
            write_sample(fp_bpf, bpf_out);
            level_agc_do(&agc, bpf_out);
            level_agc_fix(&agc);
            double agc_out = level_agc_apply(&agc, bpf_out);
            write_sample(fp_agc, agc_out);

            // --- Sharpening and Wiener deblurring ---
            // Simple high-pass filter for sharpening
            static double prev_agc = 0.0;
            double highpass = agc_out - 0.7 * prev_agc;
            prev_agc = agc_out;
            double sharpened = agc_out + 0.4 * highpass; // 0.4 = sharpening strength

            // Wiener deblurring filter (1D, sliding window)
            constexpr int wiener_N = 7; // window size (odd)
            constexpr double noise_var = 0.01; // estimated noise variance
            static double wiener_buf[wiener_N] = {0};
            static int wiener_idx = 0;
            wiener_buf[wiener_idx] = sharpened;
            double mean = 0.0, var = 0.0;
            for (int k = 0; k < wiener_N; ++k) mean += wiener_buf[k];
            mean /= wiener_N;
            for (int k = 0; k < wiener_N; ++k) var += (wiener_buf[k] - mean) * (wiener_buf[k] - mean);
            var /= wiener_N;
            double wiener = mean + (std::max(0.0, var - noise_var) / (var + noise_var)) * (sharpened - mean);
            wiener_idx = (wiener_idx + 1) % wiener_N;

            // Increase deblur strength by 50%
            double deblur_strength = 1.5;
            double wiener_strong = mean + deblur_strength * (std::max(0.0, var - noise_var) / (var + noise_var)) * (sharpened - mean);

            double final = wiener_strong * 2.0;
            if (final > 16384.0) final = 16384.0;
            if (final < -16384.0) final = -16384.0;
            write_sample(fp_final, final);
            samples_written++;
            if ((i % 10000) == 0 && i > 0) {
                printf("\rProgress (lvl%d): %zu/%zu samples (%.1f%%)...", sweep+1, i, num_samples, 100.0 * i / num_samples);
                fflush(stdout);
            }
        }
        printf("\rProgress (lvl%d): %zu/%zu samples (100.0%%)    \n", sweep+1, num_samples, num_samples);
        update_wav_header(fp_noise, info.sample_rate, num_samples);
        update_wav_header(fp_dnr, info.sample_rate, num_samples);
        update_wav_header(fp_lpf, info.sample_rate, num_samples);
        update_wav_header(fp_bpf, info.sample_rate, num_samples);
        update_wav_header(fp_agc, info.sample_rate, num_samples);
        update_wav_header(fp_final, info.sample_rate, num_samples);
        fclose(fp_noise); fclose(fp_dnr); fclose(fp_lpf); fclose(fp_bpf); fclose(fp_agc); fclose(fp_final);

        // Print AGC stats for this sweep
        printf("AGC Statistics (lvl%d):\n", sweep+1);
        printf("  Final gain: %.2f\n", agc.m_dblAGC);
        printf("  Peak level: %.2f\n", agc.m_dblAGCTop);
        printf("  S/N ratio: %.3f\n", agc.m_SN);
    }

    // All per-sweep output and stats are handled inside the loop above.
    printf("\nAll sweeps complete. Output WAVs are in %s\n", output_dir);
    return 0;

    return 0;
}
