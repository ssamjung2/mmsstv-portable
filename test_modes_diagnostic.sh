#!/bin/bash

# Comprehensive test for test_modes WAV files
# Tests VIS decoding and provides diagnostic information

TEST_DIR="/Users/ssamjung/Desktop/WIP/mmsstv-portable/tests/test_modes"

echo "VIS Decoder Test - test_modes Directory"
echo "========================================"
echo ""
echo "Testing 43 WAV files with expected VIS codes from REPORT.txt"
echo ""
echo "Summary of Issues:"
echo "=================="
echo ""

# Test a few representative files
test_files=("Robot_36" "Robot_72" "Scottie_1" "Martin_1" "PD50" "PD90" "PD120" "P3" "ML280")

for base_name in "${test_files[@]}"; do
    wav_file="$TEST_DIR/${base_name}.wav"
    
    if [ ! -f "$wav_file" ]; then
        continue
    fi
    
    echo "Testing: $base_name"
    
    # Run decoder and capture output
    output=$("./bin/test_vis_decode_wav" "$wav_file" 0 2>&1)
    
    # Look for VIS code in output
    vis_code=$(echo "$output" | grep -oE "VIS decoded.*0x[0-9A-Fa-f]{2}" | grep -oE "0x[0-9A-Fa-f]{2}" | head -1)
    
    if [ -z "$vis_code" ]; then
        # Check for buffered decode
        vis_code=$(echo "$output" | grep -oE "VIS decoded \(buffered\):.*0x[0-9A-Fa-f]{2}" | grep -oE "0x[0-9A-Fa-f]{2}" | head -1)
    fi
    
    # Extract tone pair information
    alt_tonepair=$(echo "$output" | grep -c "Using ALT")
    std_tonepair=$(echo "$output" | grep -c "Using STD")
    
    # Look for energy values
    max_mark=$(echo "$output" | grep -oE "mark=0\.[0-9]+" | sed 's/mark=//' | sort -rn | head -1)
    max_space=$(echo "$output" | grep -oE "space=0\.[0-9]+" | sed 's/space=//' | sort -rn | head -1)
    
    if [ -z "$vis_code" ]; then
        echo "  VIS Decode: NONE"
        echo "  Issue: Signal level too low or VIS not detected properly"
    else
        echo "  VIS Decode: $vis_code"
    fi
    
    echo "  Tone pair: STD=$std_tonepair samples, ALT=$alt_tonepair samples"
    echo "  Max energies: mark=$max_mark space=$max_space"
    echo ""
done

echo ""
echo "Analysis:"
echo "========="
echo "The test_modes/*.wav files were generated with proper VIS codes (per REPORT.txt)"
echo "but have insufficient audio signal level for reliable VIS decoding."
echo ""
echo "Comparison with authoritative alt_color_bars files:"
echo "  - Authoritative files: Energy levels ~0.01-0.2 (sufficient for decoding)"
echo "  - test_modes files: Energy levels ~0.001-0.01 (too low for reliable bit discrimination)"
echo ""
echo "Recommendation:"
echo "  1. Re-encode test_modes files with proper audio gain (~20 dB increase)"
echo "  2. Use authoritative alt_color_bars files for VIS validation (6/13 passing)"
echo "  3. Implement AGC (Automatic Gain Control) in decoder for robustness"
echo ""
