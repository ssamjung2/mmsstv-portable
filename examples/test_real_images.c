/*
 * SSTV Encoder Test Driver with Real Images
 * 
 * Tests the encoder with actual test images:
 * - alt_color_bars_320x256.gif
 * - alt2_test_panel_640x480.jpg
 * 
 * Loads images using stb_image, resizes to match SSTV mode resolution,
 * and encodes to WAV files for validation with external decoders.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstv_encoder.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../external/stb_image_resize2.h"

// WAV file header structure
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // Format chunk size (16)
    uint16_t audio_format;  // Audio format (1 = PCM)
    uint16_t num_channels;  // Number of channels (1 = mono)
    uint32_t sample_rate;   // Sample rate
    uint32_t byte_rate;     // Byte rate
    uint16_t block_align;   // Block align
    uint16_t bits_per_sample; // Bits per sample
    char data[4];           // "data"
    uint32_t data_size;     // Data size
} WAVHeader;

void write_wav_header(FILE *f, uint32_t sample_rate, uint32_t num_samples) {
    WAVHeader header;
    uint32_t data_size = num_samples * sizeof(int16_t);
    
    memcpy(header.riff, "RIFF", 4);
    header.file_size = data_size + sizeof(WAVHeader) - 8;
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.audio_format = 1;  // PCM
    header.num_channels = 1;  // Mono
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * sizeof(int16_t);
    header.block_align = sizeof(int16_t);
    header.bits_per_sample = 16;
    memcpy(header.data, "data", 4);
    header.data_size = data_size;
    
    fwrite(&header, sizeof(WAVHeader), 1, f);
}

// Load and resize image to target resolution
uint8_t* load_and_resize_image(const char *filename, int target_width, int target_height) {
    int width, height, channels;
    
    printf("Loading image: %s\n", filename);
    uint8_t *img = stbi_load(filename, &width, &height, &channels, 3); // Force RGB
    
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", filename);
        fprintf(stderr, "Error: %s\n", stbi_failure_reason());
        return NULL;
    }
    
    printf("  Original size: %dx%d, channels: %d\n", width, height, channels);
    
    // Resize if needed
    if (width != target_width || height != target_height) {
        printf("  Resizing to: %dx%d\n", target_width, target_height);
        
        uint8_t *resized = (uint8_t*)malloc(target_width * target_height * 3);
        if (!resized) {
            fprintf(stderr, "Failed to allocate resize buffer\n");
            stbi_image_free(img);
            return NULL;
        }
        
        // Use stbir for high-quality resize
        if (!stbir_resize_uint8_linear(img, width, height, 0,
                                       resized, target_width, target_height, 0,
                                       STBIR_RGB)) {
            fprintf(stderr, "Failed to resize image\n");
            free(resized);
            stbi_image_free(img);
            return NULL;
        }
        
        stbi_image_free(img);
        return resized;
    }
    
    return img;
}

// Encode image to WAV file
int encode_to_wav(const char *image_path, const char *output_wav, sstv_mode_t mode, uint32_t sample_rate) {
    // Get mode info
    const sstv_mode_info_t *mode_info = sstv_get_mode_info(mode);
    if (!mode_info) {
        fprintf(stderr, "Invalid SSTV mode\n");
        return -1;
    }
    
    printf("\n=== Encoding %s ===\n", mode_info->name);
    printf("Mode: %s (VIS 0x%02x)\n", mode_info->name, mode_info->vis_code);
    printf("Resolution: %dx%d\n", mode_info->width, mode_info->height);
    printf("Duration: %.1f seconds\n", mode_info->duration_sec);
    printf("Sample rate: %u Hz\n", sample_rate);
    
    // Load and resize image
    uint8_t *rgb = load_and_resize_image(image_path, mode_info->width, mode_info->height);
    if (!rgb) {
        return -1;
    }
    
    // Create image structure
    sstv_image_t image;
    image.pixels = rgb;
    image.width = mode_info->width;
    image.height = mode_info->height;
    image.stride = mode_info->width * 3;
    image.format = SSTV_RGB24;
    
    // Create encoder
    sstv_encoder_t *encoder = sstv_encoder_create(mode, (double)sample_rate);
    if (!encoder) {
        fprintf(stderr, "Failed to create encoder\n");
        free(rgb);
        return -1;
    }
    
    // Set image and enable VIS
    if (sstv_encoder_set_image(encoder, &image) != 0) {
        fprintf(stderr, "Failed to set image\n");
        sstv_encoder_free(encoder);
        free(rgb);
        return -1;
    }
    sstv_encoder_set_vis_enabled(encoder, 1);
    
    // Open output file
    FILE *wav = fopen(output_wav, "wb");
    if (!wav) {
        fprintf(stderr, "Failed to open output file: %s\n", output_wav);
        sstv_encoder_free(encoder);
        free(rgb);
        return -1;
    }
    
    // Get total samples and write WAV header
    size_t total_samples = sstv_encoder_get_total_samples(encoder);
    printf("Total samples: %zu (%.2f seconds)\n", total_samples, (double)total_samples / sample_rate);
    write_wav_header(wav, sample_rate, (uint32_t)total_samples);
    
    // Generate and write audio
    float samples[4096];
    int16_t pcm[4096];
    size_t samples_written = 0;
    
    printf("Encoding");
    fflush(stdout);
    
    while (!sstv_encoder_is_complete(encoder)) {
        size_t generated = sstv_encoder_generate(encoder, samples, 4096);
        
        // Convert float to 16-bit PCM
        for (size_t i = 0; i < generated; i++) {
            float s = samples[i];
            if (s > 1.0f) s = 1.0f;
            if (s < -1.0f) s = -1.0f;
            pcm[i] = (int16_t)(s * 32767.0f);
        }
        
        fwrite(pcm, sizeof(int16_t), generated, wav);
        samples_written += generated;
        
        // Progress indicator
        if (samples_written % (sample_rate * 5) == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    printf(" Done!\n");
    printf("Samples written: %zu\n", samples_written);
    printf("Output: %s\n", output_wav);
    
    fclose(wav);
    sstv_encoder_free(encoder);
    free(rgb);
    
    return 0;
}

int main(int argc, char **argv) {
    printf("SSTV Encoder - Real Image Test Driver\n");
    printf("======================================\n\n");
    
    // Default paths relative to workspace
    const char *color_bars = "/Users/ssamjung/Desktop/WIP/PiSSTVpp2/tests/images/alt_color_bars_320x256.gif";
    const char *test_panel = "/Users/ssamjung/Desktop/WIP/PiSSTVpp2/tests/images/alt2_test_panel_640x480.jpg";
    
    uint32_t sample_rate = 48000;
    
    // Parse command line
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [sample_rate]\n", argv[0]);
            printf("\nOptions:\n");
            printf("  sample_rate    Audio sample rate (default: 48000)\n");
            printf("\nTest images:\n");
            printf("  1. Color bars (320x256): %s\n", color_bars);
            printf("  2. Test panel (640x480): %s\n", test_panel);
            printf("\nModes tested:\n");
            printf("  - Scottie 1 (320x256) with color bars\n");
            printf("  - Martin 1 (320x256) with color bars\n");
            printf("  - Robot 36 (320x240) with color bars (resized)\n");
            printf("  - PD120 (640x496) with test panel (resized)\n");
            return 0;
        }
        sample_rate = (uint32_t)atoi(argv[1]);
        if (sample_rate < 8000 || sample_rate > 96000) {
            fprintf(stderr, "Invalid sample rate: %u (use 8000-96000)\n", sample_rate);
            return 1;
        }
    }
    
    printf("Sample rate: %u Hz\n", sample_rate);
    printf("\n");
    
    // Create tests directory
#ifdef _WIN32
    mkdir("tests");
#else
    mkdir("tests", 0755);
#endif

    // Test 1: Color bars with Scottie 1 (320x256 - perfect match)
    printf("TEST 1: Color bars → Scottie 1\n");
    if (encode_to_wav(color_bars, "tests/test_colorbar_scottie1.wav", SSTV_SCOTTIE1, sample_rate) != 0) {
        fprintf(stderr, "Test 1 failed\n");
    }
    
    // Test 2: Color bars with Martin 1 (320x256 - perfect match)
    printf("\nTEST 2: Color bars → Martin 1\n");
    if (encode_to_wav(color_bars, "tests/test_colorbar_martin1.wav", SSTV_MARTIN1, sample_rate) != 0) {
        fprintf(stderr, "Test 2 failed\n");
    }
    
    // Test 3: Color bars with Robot 36 (320x240 - resize from 256)
    printf("\nTEST 3: Color bars → Robot 36 (with resize)\n");
    if (encode_to_wav(color_bars, "tests/test_colorbar_robot36.wav", SSTV_R36, sample_rate) != 0) {
        fprintf(stderr, "Test 3 failed\n");
    }
    
    // Test 4: Test panel with PD120 (640x496 - resize from 480)
    printf("\nTEST 4: Test panel → PD120 (with resize)\n");
    if (encode_to_wav(test_panel, "tests/test_panel_pd120.wav", SSTV_PD120, sample_rate) != 0) {
        fprintf(stderr, "Test 4 failed\n");
    }
    
    // Test 5: Test panel with Scottie 1 (320x256 - significant resize)
    printf("\nTEST 5: Test panel → Scottie 1 (with resize)\n");
    if (encode_to_wav(test_panel, "tests/test_panel_scottie1.wav", SSTV_SCOTTIE1, sample_rate) != 0) {
        fprintf(stderr, "Test 5 failed\n");
    }
    
    printf("\n");
    printf("==============================================\n");
    printf("All tests complete!\n");
    printf("\nGenerated files:\n");
    printf("  - tests/test_colorbar_scottie1.wav\n");
    printf("  - tests/test_colorbar_martin1.wav\n");
    printf("  - tests/test_colorbar_robot36.wav\n");
    printf("  - tests/test_panel_pd120.wav\n");
    printf("  - tests/test_panel_scottie1.wav\n");
    printf("\nNext steps:\n");
    printf("  1. Test these files with MMSSTV or QSSTV\n");
    printf("  2. Verify images decode correctly\n");
    printf("  3. Check for color accuracy and timing\n");
    
    return 0;
}
