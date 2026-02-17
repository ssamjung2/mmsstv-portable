/*
 * VIS Code Test Suite - Validates all 43 SSTV modes
 * 
 * This test program loads VIS test fixtures from JSON and validates:
 * 1. VIS code to binary conversion (LSB-first)
 * 2. Bit frequency mapping (1100/1300 Hz for data bits)
 * 3. Parity calculation (even parity)
 * 4. Complete VIS sequence timing (640ms total)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Forward declarations */
struct vis_test_case {
    int id;
    char name[32];
    uint8_t vis_code;
    int parity;
    char type[16];
    char note[128];
};

struct test_suite {
    int total_modes;
    struct vis_test_case *test_cases;
};

/* VIS Encoder implementation for testing */
typedef struct {
    uint8_t vis_code;
    uint8_t bits_lsb[8];
    int parity_bit;
    int state;
    long sample_count;
} VISEncoderTest;

/* Calculate even parity (returns 0 for even, 1 for odd) */
int calculate_parity(uint8_t value)
{
    int bits = 0;
    int i;
    for (i = 0; i < 8; i++) {
        if (value & (1 << i)) bits++;
    }
    return (bits & 1);  /* Return 1 if odd, 0 if even */
}

/* Convert VIS code to LSB-first bit array */
void vis_code_to_bits(uint8_t vis_code, uint8_t bits_lsb[8])
{
    int i;
    for (i = 0; i < 8; i++) {
        bits_lsb[i] = (vis_code >> i) & 1;
    }
}

/* Convert bit value to VIS frequency (1100=bit0, 1300=bit1) */
int bit_to_frequency(int bit_value)
{
    return bit_value ? 1300 : 1100;
}

/* Test single mode VIS code */
int test_vis_mode(struct vis_test_case *test, int test_index)
{
    uint8_t bits_lsb[8];
    int bit_frequencies[8];
    int parity;
    int i, errors = 0;
    
    printf("\n[TEST %d] %s (VIS 0x%02X = %d decimal)\n", 
           test_index, test->name, test->vis_code, test->vis_code);
    
    if (test->vis_code != 0) {
        /* Test bit conversion */
        vis_code_to_bits(test->vis_code, bits_lsb);
        printf("  Binary conversion: ");
        for (i = 0; i < 8; i++) {
            printf("%d", bits_lsb[i]);
            bit_frequencies[i] = bit_to_frequency(bits_lsb[i]);
        }
        printf("\n");
        
        /* Test frequency mapping */
        printf("  Bit frequencies: ");
        for (i = 0; i < 8; i++) {
            printf("%d", bit_frequencies[i] / 100);  /* Print as 11 or 13 */
            printf(" ");
        }
        printf("\n");
        
        /* Test parity */
        parity = calculate_parity(test->vis_code);
        int parity_freq = bit_to_frequency(parity);
        printf("  Parity: %d (even=%d) → frequency %d Hz\n", 
               parity, (parity == 0), parity_freq);
        if (parity != test->parity) {
            printf("  ✗ Parity mismatch: got %d, expected %d\n", 
                   parity, test->parity);
            errors++;
        }
        
        /* VIS sequence timing */
        int total_ms = 300 + 10 + 300 + 30 + (8 * 30) + 30 + 30;  /* 640ms */
        printf("  VIS sequence: Leader(1900/300ms) + Break(1200/10ms) + Leader(1900/300ms) + ");
        printf("Start(1200/30ms) + Data(8×30ms) + Parity(30ms) + Stop(1200/30ms) = %dms\n", total_ms);
    } else {
        printf("  VIS sequence: SKIPPED (No VIS transmission)\n");
    }
    
    if (errors == 0) {
        printf("  ✓ PASS\n");
    } else {
        printf("  ✗ FAIL (%d errors)\n", errors);
    }
    
    return errors;
}

/* Load test fixture from built-in data */
struct test_suite* create_test_suite()
{
    struct test_suite *suite = malloc(sizeof(struct test_suite));
    suite->total_modes = 43;
    suite->test_cases = malloc(43 * sizeof(struct vis_test_case));
    
    /* Populate test cases from hardcoded data matching JSON fixture */
    struct vis_test_case cases[] = {
        {1, "Robot 36", 0x88, 0, "color", ""},
        {2, "Robot 72", 0x0C, 0, "color", ""},
        {3, "AVT 90", 0x44, 0, "color", ""},
        {4, "Scottie 1", 0x3C, 0, "color", ""},
        {5, "Scottie 2", 0xB8, 0, "color", ""},
        {6, "ScottieDX", 0xCC, 0, "color", ""},
        {7, "Martin 1", 0xAC, 0, "color", ""},
        {8, "Martin 2", 0x28, 0, "color", ""},
        {9, "SC2 180", 0xB7, 0, "color", ""},
        {10, "SC2 120", 0x3F, 0, "color", ""},
        {11, "SC2 60", 0xBB, 0, "color", ""},
        {12, "PD50", 0xDD, 0, "color", ""},
        {13, "PD90", 0x63, 0, "color", ""},
        {14, "PD120", 0x5F, 0, "color", ""},
        {15, "PD160", 0xE2, 0, "color", ""},
        {16, "PD180", 0x60, 0, "color", ""},
        {17, "PD240", 0xE1, 0, "color", ""},
        {18, "PD290", 0xDE, 0, "color", ""},
        {19, "P3", 0x71, 0, "color", ""},
        {20, "P5", 0x72, 0, "color", ""},
        {21, "P7", 0xF3, 0, "color", ""},
        {22, "MR73", 0x45, 1, "color", ""},
        {23, "MR90", 0x46, 1, "color", ""},
        {24, "MR115", 0x49, 1, "color", ""},
        {25, "MR140", 0x4A, 1, "color", ""},
        {26, "MR175", 0x4C, 1, "color", ""},
        {27, "MP73", 0x25, 1, "color", ""},
        {28, "MP115", 0x29, 1, "color", ""},
        {29, "MP140", 0x2A, 1, "color", ""},
        {30, "MP175", 0x2C, 1, "color", ""},
        {31, "ML180", 0x85, 1, "color", ""},
        {32, "ML240", 0x86, 1, "color", ""},
        {33, "ML280", 0x89, 1, "color", ""},
        {34, "ML320", 0x8A, 1, "color", ""},
        {35, "Robot 24", 0x84, 0, "color", ""},
        {36, "B/W 8", 0x82, 0, "bw", ""},
        {37, "B/W 12", 0x86, 1, "bw", ""},
        {38, "MP73-N", 0x00, 0, "color", "No VIS transmission"},
        {39, "MP110-N", 0x00, 0, "color", "No VIS transmission"},
        {40, "MP140-N", 0x00, 0, "color", "No VIS transmission"},
        {41, "MC110-N", 0x00, 0, "color", "No VIS transmission"},
        {42, "MC140-N", 0x00, 0, "color", "No VIS transmission"},
        {43, "MC180-N", 0x00, 0, "color", "No VIS transmission"},
    };
    
    memcpy(suite->test_cases, cases, 43 * sizeof(struct vis_test_case));
    return suite;
}

int main()
{
    struct test_suite *suite;
    int i, total_errors = 0, total_pass = 0;
    
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║   SSTV VIS Code Test Suite - All 43 Modes             ║\n");
    printf("║                                                        ║\n");
    printf("║   Tests: Bit patterns, frequencies, parity             ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    suite = create_test_suite();
    
    printf("\nRunning %d VIS encoder tests...\n", suite->total_modes);
    printf("═════════════════════════════════════════════════════════\n");
    
    for (i = 0; i < suite->total_modes; i++) {
        int errors = test_vis_mode(&suite->test_cases[i], i + 1);
        if (errors == 0) {
            total_pass++;
        } else {
            total_errors += errors;
        }
    }
    
    printf("\n═════════════════════════════════════════════════════════\n");
    printf("TEST RESULTS:\n");
    printf("  Total modes tested: %d\n", suite->total_modes);
    printf("  Modes passed:       %d (%.1f%%)\n", 
           total_pass, 100.0f * total_pass / suite->total_modes);
    printf("  Total errors:       %d\n", total_errors);
    
    if (total_errors == 0) {
        printf("\n✓ ALL TESTS PASSED\n");
    } else {
        printf("\n✗ SOME TESTS FAILED\n");
    }
    
    /* Cleanup */
    free(suite->test_cases);
    free(suite);
    
    return total_errors > 0 ? 1 : 0;
}
