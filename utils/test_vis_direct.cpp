#include <stdio.h>
#include "../src/vis.h"

int main() {
    VISEncoder vis;
    vis.start(0x88, 44100.0);  // Robot 36 at 44.1 kHz
    
    printf("Robot 36 VIS (0x88 = 0b10001000  LSB-first: 00010001) expected frequencies:\n");
    printf("Bit 0 (LSB): 0 → 1100 Hz\n");
    printf("Bit 1: 0 → 1100 Hz\n");
    printf("Bit 2: 0 → 1100 Hz\n");
    printf("Bit 3: 1 → 1300 Hz\n");
    printf("Bit 4: 0 → 1100 Hz\n");
    printf("Bit 5: 0 → 1100 Hz");
    printf("Bit 6: 0 → 1100 Hz\n");
    printf("Parity: even → 1100 Hz\n\n");
    
    printf("Actual VIS sequence:\n");
    printf("Sample#    Frequency    Purpose\n");
    printf("==================================================\n");
    
    double last_freq = 0;
    int sample = 0;
    int bit_num = 0;
    int in_data_bits = 0;
    for (int i = 0; i < 50000; i++) {
        double freq = vis.get_frequency();
        if (freq <= 0.0) break;
        
        // Only print when frequency changes
        if (freq != last_freq) {
            char purpose[32] = "";
            if (sample < 13230) snprintf(purpose, sizeof(purpose), "Leader 1");
            else if (sample < 13672) snprintf(purpose, sizeof(purpose), "Break");
            else if (sample < 26903) snprintf(purpose, sizeof(purpose), "Leader 2");
            else if (sample < 28227) snprintf(purpose, sizeof(purpose), "Start bit");
            else if (sample < 38819) {
                snprintf(purpose, sizeof(purpose), "Bit %d = %.0f", bit_num, (freq == 1300.0) ? 1.0 : 0.0);
                if (freq != last_freq && last_freq != 0) bit_num++;
                in_data_bits = 1;
            } else snprintf(purpose, sizeof(purpose), "Stop bit");
            
            printf("%6d     %.1f Hz    %s\n", sample, freq, purpose);
            last_freq = freq;
        }
        sample++;
    }
    
    printf("\nTotal samples: %d\n", sample);
    
    return 0;
}
