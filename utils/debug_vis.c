/*
 * Debug VIS encoder output
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/sstv_encoder.h"

int main() {
    // Create encoder
    sstv_mode_t mode = SSTV_R36;
    int sample_rate = 44100;
    
    sstv_encoder_t *enc = sstv_create_encoder(mode, sample_rate);
    if (!enc) {
        printf("Failed to create encoder\n");
        return 1;
    }
    
    // Create a dummy image
    int width = 320, height = 240;
    uint8_t *pixels = (uint8_t *)malloc(width * height * 3);
    memset(pixels, 128, width * height * 3);
    
    sstv_image_t image;
    image.width = width;
    image.height = height;
    image.format = SSTV_RGB24;
    image.data = pixels;
    
    sstv_encoder_set_image(enc, &image);
    sstv_begin_encoding(enc);
    
    // Get VIS samples
    float samples[8192];
    size_t total = 0;
    int vis_done = 0;
    
    printf("Generating VIS sequence for Robot 36 (VIS code 0x88)...\n");
    printf("Sample#   Frequency\n");
    
    while (!vis_done && total < 100000) {
        size_t n = sstv_synthesize_audio(enc, samples, 1);
        if (n == 0) break;
        
        // Simple frequency detection: look for zero crossings
        static float prev_sample = 0.0f;
        static int zero_crossings = 0;
        static int sample_count = 0;
        
        for (size_t i = 0; i < n; i++) {
            if ((prev_sample < 0 && samples[i] >= 0) || 
                (prev_sample >= 0 && samples[i] < 0)) {
                zero_crossings++;
            }
            prev_sample = samples[i];
            sample_count++;
            
            // Every 441 samples (10ms at 44100 Hz), print frequency
            if (sample_count >= 441) {
                float freq = (zero_crossings * 44100.0f) / (2.0f * sample_count);
                printf("%6zu    %.1f Hz\n", total, freq);
                zero_crossings = 0;
                sample_count = 0;
            }
        }
        
        total += n;
        
        // Stop after ~1 second (VIS should be done by then)
        if (total > 44100) vis_done = 1;
    }
    
    sstv_destroy_encoder(enc);
    free(pixels);
    return 0;
}
