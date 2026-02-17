#!/bin/bash

# Test all mode WAV files in tests/test_modes/
# Maps filename to expected VIS code

echo "VIS Decoder Test - All Modes from tests/test_modes/"
echo "===================================================="
echo ""
echo "Mode Name          | Expected VIS | Status"
echo "-------------------+--------------+----------"

PASS_COUNT=0
WRONG_COUNT=0
FAIL_COUNT=0

# Create a mapping array (bash associative arrays are tricky, so use case statement)
get_expected_vis() {
    case "$1" in
        "AVT_90") echo "0x44" ;;
        "B_W_8") echo "0x82" ;;
        "B_W_12") echo "0x86" ;;
        "ML180") echo "0x85" ;;
        "ML240") echo "0x86" ;;
        "ML280") echo "0x89" ;;
        "ML320") echo "0x8A" ;;
        "MP73") echo "0x25" ;;
        "MP115") echo "0x29" ;;
        "MP140") echo "0x2A" ;;
        "MP175") echo "0x2C" ;;
        "MR73") echo "0x45" ;;
        "MR90") echo "0x46" ;;
        "MR115") echo "0x49" ;;
        "MR140") echo "0x4A" ;;
        "MR175") echo "0x4C" ;;
        "Martin_1") echo "0xAC" ;;
        "Martin_2") echo "0x28" ;;
        "P3") echo "0x71" ;;
        "P5") echo "0x72" ;;
        "P7") echo "0xF3" ;;
        "PD50") echo "0xDD" ;;
        "PD90") echo "0x63" ;;
        "PD120") echo "0x5F" ;;
        "PD160") echo "0xE2" ;;
        "PD180") echo "0x60" ;;
        "PD240") echo "0xE1" ;;
        "PD290") echo "0xDE" ;;
        "Robot_24") echo "0x84" ;;
        "Robot_36") echo "0x88" ;;
        "Robot_72") echo "0x0C" ;;
        "SC2_60") echo "0xBB" ;;
        "SC2_120") echo "0x3F" ;;
        "SC2_180") echo "0xB7" ;;
        "Scottie_1") echo "0x3C" ;;
        "Scottie_2") echo "0xB8" ;;
        "ScottieDX") echo "0xCC" ;;
        "MC110_N"|"MC140_N"|"MC180_N"|"MP73_N"|"MP110_N"|"MP140_N") echo "0x00" ;;
        *) echo "0x00" ;;
    esac
}

TEST_DIR="/Users/ssamjung/Desktop/WIP/mmsstv-portable/tests/test_modes"

for wav_file in "$TEST_DIR"/*.wav; do
    filename=$(basename "$wav_file" .wav)
    expected_vis=$(get_expected_vis "$filename")
    
    # Run decoder and extract VIS code
    output=$("./bin/test_vis_decode_wav" "$wav_file" 0 2>&1)
    
    # Look for "VIS decoded:" line with hex code
    decoded_vis=$(echo "$output" | grep -oE "VIS decoded:? [^→]*" | tail -1 | grep -oE "0x[0-9A-Fa-f]{2}" | head -1)
    
    if [ -z "$decoded_vis" ]; then
        # Try to find VIS from buffered output
        decoded_vis=$(echo "$output" | grep -oE "VIS decoded \(buffered\): [^→]*" | grep -oE "0x[0-9A-Fa-f]{2}" | head -1)
    fi
    
    # Determine status
    if [ -z "$decoded_vis" ]; then
        status="✗ FAIL"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        decoded_vis="NONE"
    elif [ "$decoded_vis" = "$expected_vis" ]; then
        status="✓ PASS"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        status="✗ WRONG"
        WRONG_COUNT=$((WRONG_COUNT + 1))
    fi
    
    printf "%-18s | %s -> %-6s | %s\n" "$filename" "$expected_vis" "$decoded_vis" "$status"
done

echo ""
echo "Summary: PASS=$PASS_COUNT, WRONG=$WRONG_COUNT, FAIL=$FAIL_COUNT (Total: $((PASS_COUNT + WRONG_COUNT + FAIL_COUNT)))"
