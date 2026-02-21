/*
 * Generate test WAV files for all SSTV modes
 * Creates a comprehensive test suite with VIS header analysis
 */

#include "sstv_encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* WAV file header structure */
typedef struct {
    char riff[4];           /* "RIFF" */
    uint32_t file_size;     /* File size - 8 */
    char wave[4];           /* "WAVE" */
    char fmt[4];            /* "fmt " */
    uint32_t fmt_size;      /* 16 for PCM */
    uint16_t format;        /* 1 for PCM */
    uint16_t channels;      /* 1 for mono */
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];           /* "data" */
    uint32_t data_size;
} WavHeader;

void write_wav_header(FILE *fp, uint32_t sample_rate, uint32_t num_samples) {
    WavHeader hdr;
    memcpy(hdr.riff, "RIFF", 4);
    memcpy(hdr.wave, "WAVE", 4);
    memcpy(hdr.fmt, "fmt ", 4);
    memcpy(hdr.data, "data", 4);
    
    hdr.fmt_size = 16;
    hdr.format = 1;
    hdr.channels = 1;
    hdr.sample_rate = sample_rate;
    hdr.bits_per_sample = 16;
    hdr.block_align = hdr.channels * (hdr.bits_per_sample / 8);
    hdr.byte_rate = sample_rate * hdr.block_align;
    hdr.data_size = num_samples * hdr.block_align;
    hdr.file_size = 36 + hdr.data_size;
    
    fwrite(&hdr, sizeof(WavHeader), 1, fp);
}

void generate_color_bars(unsigned char *rgb, unsigned int width, unsigned int height) {
    const unsigned char colors[8][3] = {
        {255, 255, 255}, /* White */
        {255, 255, 0},   /* Yellow */
        {0, 255, 255},   /* Cyan */
        {0, 255, 0},     /* Green */
        {255, 0, 255},   /* Magenta */
        {255, 0, 0},     /* Red */
        {0, 0, 255},     /* Blue */
        {0, 0, 0}        /* Black */
    };
    
    for (unsigned int y = 0; y < height; y++) {
        for (unsigned int x = 0; x < width; x++) {
            int bar = (x * 8) / width;
            unsigned char *pixel = &rgb[(y * width + x) * 3];
            pixel[0] = colors[bar][0];
            pixel[1] = colors[bar][1];
            pixel[2] = colors[bar][2];
        }
    }
}

int generate_mode_wav(sstv_mode_t mode, const char *output_dir, unsigned int sample_rate, FILE *report) {
    const sstv_mode_info_t *info = sstv_get_mode_info(mode);
    if (!info) {
        fprintf(report, "ERROR: Could not get info for mode %d\n", mode);
        return -1;
    }
    
    /* Create filename */
    char filename[512];
    char safe_name[256];
    const char *name_ptr = info->name;
    char *safe_ptr = safe_name;
    while (*name_ptr && (safe_ptr - safe_name) < 255) {
        char c = *name_ptr++;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            *safe_ptr++ = c;
        } else if (c == ' ' || c == '-' || c == '/') {
            *safe_ptr++ = '_';
        }
    }
    *safe_ptr = '\0';
    snprintf(filename, sizeof(filename), "%s/%s.wav", output_dir, safe_name);
    
    /* Allocate image buffer */
    unsigned int width = info->width;
    unsigned int height = info->height;
    unsigned char *rgb = (unsigned char *)calloc(width * height * 3, 1);
    if (!rgb) {
        fprintf(report, "ERROR: Out of memory for mode %s\n", info->name);
        return -1;
    }
    
    /* Generate color bars test pattern */
    generate_color_bars(rgb, width, height);
    
    /* Create encoder */
    sstv_encoder_t *encoder = sstv_encoder_create(mode, (double)sample_rate);
    if (!encoder) {
        fprintf(report, "ERROR: Failed to create encoder for mode %s\n", info->name);
        free(rgb);
        return -1;
    }
    
    /* Set image */
    sstv_image_t image = sstv_image_from_rgb(rgb, width, height);
    if (sstv_encoder_set_image(encoder, &image) != 0) {
        fprintf(report, "ERROR: Image size mismatch for mode %s\n", info->name);
        sstv_encoder_free(encoder);
        free(rgb);
        return -1;
    }
    
    /* Enable VIS for modes that have it */
    int vis_enabled = (info->vis_code != 0x00);
    sstv_encoder_set_vis_enabled(encoder, vis_enabled);
    
    size_t total_samples = sstv_encoder_get_total_samples(encoder);
    
    /* Open output file */
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(report, "ERROR: Could not open %s for writing\n", filename);
        sstv_encoder_free(encoder);
        free(rgb);
        return -1;
    }
    
    /* Write WAV header */
    write_wav_header(fp, sample_rate, total_samples);
    
    /* Generate and write audio */
    float buffer[4096];
    size_t total_written = 0;
    while (!sstv_encoder_is_complete(encoder)) {
        size_t generated = sstv_encoder_generate(encoder, buffer, 4096);
        if (generated == 0) break;
        
        /* Convert float to int16 */
        for (size_t i = 0; i < generated; i++) {
            int16_t sample = (int16_t)(buffer[i] * 32767.0f);
            fwrite(&sample, sizeof(int16_t), 1, fp);
        }
        total_written += generated;
    }
    
    fclose(fp);
    
    /* Write report entry */
    fprintf(report, "\n=== %s ===\n", info->name);
    fprintf(report, "File: %s\n", filename);
    fprintf(report, "VIS Code: 0x%02X (%d decimal)\n", info->vis_code, info->vis_code);
    fprintf(report, "VIS Enabled: %s\n", vis_enabled ? "Yes" : "No (narrow mode)");
    fprintf(report, "Resolution: %d×%d\n", width, height);
    fprintf(report, "Duration: %.3f seconds\n", info->duration_sec);
    fprintf(report, "Type: %s\n", info->is_color ? "Color" : "B/W");
    fprintf(report, "Sample Rate: %u Hz\n", sample_rate);
    fprintf(report, "Total Samples: %zu\n", total_samples);
    fprintf(report, "Actual Samples: %zu\n", total_written);
    fprintf(report, "Preamble: %s\n", mode >= SSTV_MN73 ? "400 ms" : "800 ms");
    
    if (vis_enabled) {
        fprintf(report, "\nVIS Header Analysis:\n");
        fprintf(report, "  Leader 1: 300 ms @ 1900 Hz\n");
        fprintf(report, "  Break:     10 ms @ 1200 Hz\n");
        fprintf(report, "  Leader 2: 300 ms @ 1900 Hz\n");
        fprintf(report, "  Start:     30 ms @ 1200 Hz\n");
        
        fprintf(report, "  Data bits (LSB first): ");
        for (int i = 0; i < 8; i++) {
            fprintf(report, "%d", (info->vis_code >> i) & 1);
        }
        fprintf(report, "\n");
        
        fprintf(report, "  Bit frequencies: ");
        for (int i = 0; i < 8; i++) {
            fprintf(report, "%d Hz ", (info->vis_code >> i) & 1 ? 1300 : 1100);
        }
        fprintf(report, "\n");
        
        int parity = 0;
        for (int i = 0; i < 8; i++) {
            if (info->vis_code & (1 << i)) parity ^= 1;
        }
        fprintf(report, "  Parity:    30 ms @ %d Hz (even parity = %d)\n", 
                parity ? 1300 : 1100, parity);
        fprintf(report, "  Stop:      30 ms @ 1200 Hz\n");
        fprintf(report, "  Total VIS: 940 ms\n");
    }
    
    fprintf(report, "Status: ✓ Generated successfully\n");
    
    /* Cleanup */
    sstv_encoder_free(encoder);
    free(rgb);
    
    return 0;
}

int main(int argc, char *argv[]) {
    const char *output_dir = argc > 1 ? argv[1] : "/Users/ssamjung/Desktop/WIP/mmsstv-portable/tests";
    unsigned int sample_rate = argc > 2 ? atoi(argv[2]) : 48000;
    
    /* Create output directory */
#ifdef _WIN32
    mkdir("tests");
#else
    mkdir("tests", 0755);
#endif
#ifdef _WIN32
    mkdir(output_dir);
#else
    mkdir(output_dir, 0755);
#endif
    
    /* Open report file */
    char report_filename[512];
    snprintf(report_filename, sizeof(report_filename), "%s/REPORT.txt", output_dir);
    FILE *report = fopen(report_filename, "w");
    if (!report) {
        fprintf(stderr, "ERROR: Could not create report file\n");
        return 1;
    }
    
    fprintf(report, "========================================\n");
    fprintf(report, "SSTV Mode Test Generation Report\n");
    fprintf(report, "========================================\n");
    fprintf(report, "Generated: %s", "January 30, 2026\n");
    fprintf(report, "Sample Rate: %u Hz\n", sample_rate);
    fprintf(report, "Test Pattern: Color bars (White/Yellow/Cyan/Green/Magenta/Red/Blue/Black)\n");
    fprintf(report, "========================================\n");
    
    /* Get all modes */
    sstv_mode_t all_modes[] = {
        SSTV_R24, SSTV_R36, SSTV_R72, SSTV_AVT90,
        SSTV_SCOTTIE1, SSTV_SCOTTIE2, SSTV_SCOTTIEX,
        SSTV_MARTIN1, SSTV_MARTIN2,
        SSTV_SC2_180, SSTV_SC2_120, SSTV_SC2_60,
        SSTV_PD50, SSTV_PD90, SSTV_PD120, SSTV_PD160, SSTV_PD180, SSTV_PD240, SSTV_PD290,
        SSTV_P3, SSTV_P5, SSTV_P7,
        SSTV_MR73, SSTV_MR90, SSTV_MR115, SSTV_MR140, SSTV_MR175,
        SSTV_MP73, SSTV_MP115, SSTV_MP140, SSTV_MP175,
        SSTV_ML180, SSTV_ML240, SSTV_ML280, SSTV_ML320,
        SSTV_BW8, SSTV_BW12,
        SSTV_MN73, SSTV_MN110, SSTV_MN140,
        SSTV_MC110, SSTV_MC140, SSTV_MC180
    };
    
    int num_modes = sizeof(all_modes) / sizeof(all_modes[0]);
    int success_count = 0;
    int failure_count = 0;
    
    printf("Generating test files for %d SSTV modes...\n", num_modes);
    printf("Output directory: %s\n", output_dir);
    printf("Sample rate: %u Hz\n\n", sample_rate);
    
    for (int i = 0; i < num_modes; i++) {
        const sstv_mode_info_t *info = sstv_get_mode_info(all_modes[i]);
        if (!info) continue;
        
        printf("[%2d/%d] Generating %s...", i + 1, num_modes, info->name);
        fflush(stdout);
        
        if (generate_mode_wav(all_modes[i], output_dir, sample_rate, report) == 0) {
            printf(" ✓\n");
            success_count++;
        } else {
            printf(" ✗\n");
            failure_count++;
        }
    }
    
    /* Write summary */
    fprintf(report, "\n========================================\n");
    fprintf(report, "SUMMARY\n");
    fprintf(report, "========================================\n");
    fprintf(report, "Total modes: %d\n", num_modes);
    fprintf(report, "Successful: %d\n", success_count);
    fprintf(report, "Failed: %d\n", failure_count);
    fprintf(report, "========================================\n");
    
    fclose(report);
    
    printf("\n========================================\n");
    printf("Generation complete!\n");
    printf("  Successful: %d\n", success_count);
    printf("  Failed: %d\n", failure_count);
    printf("  Report: %s\n", report_filename);
    printf("========================================\n");
    
    return failure_count > 0 ? 1 : 0;
}
