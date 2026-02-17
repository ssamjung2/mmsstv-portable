#!/bin/bash
# Test all authoritative alt_color_bars files

TEST_DIR="tests/audio"
BINARY="./bin/test_vis_decode_wav"

echo "=========================================="
echo "Testing Authoritative SSTV VIS Decoder"
echo "=========================================="
echo ""

# VIS code to mode number mapping (based on VIS_CODE_MAP in decoder.cpp)
vis_to_mode() {
    case "$1" in
        0xAC) echo 6 ;;   # Martin1
        0x28) echo 7 ;;   # Martin2
        0xDD) echo 11 ;;  # PD50
        0x63) echo 12 ;;  # PD90
        0x5F) echo 13 ;;  # PD120
        0xE2) echo 14 ;;  # PD160
        0x60) echo 15 ;;  # PD180
        0x82) echo 34 ;;  # Robot24BW (BW8)
        0x88) echo 0 ;;   # Robot36C
        0xB7) echo 8 ;;   # SC2-180
        0x3C) echo 3 ;;   # Scottie1
        0xB8) echo 4 ;;   # Scottie2
        0xCC) echo 5 ;;   # ScottieDX
        *) echo -1 ;;
    esac
}

# Test each file with its expected VIS code
test_file() {
    local file=$1
    local name=$2
    local expected_vis=$3
    local expected_mode=$(vis_to_mode "$expected_vis")
    
    output=$("$BINARY" "$file" "$expected_mode" --debug 2 2>&1)
    decoded_line=$(echo "$output" | grep "VIS decoded" | head -1)
    actual=$(echo "$decoded_line" | grep -o "0x[0-9a-fA-F][0-9a-fA-F]" | head -1 | tr 'a-f' 'A-F')
    mode=$(echo "$output" | grep "Decoded mode" | awk '{print $3}')
    
    # Count state transitions for diagnostics (trim whitespace)
    leader_detected=$(echo "$output" | grep -c "Leader detected" | tr -d ' ')
    leader_lost=$(echo "$output" | grep -c "Leader lost" | tr -d ' ')
    vis_entered=$(echo "$output" | grep -c "entering VIS decode" | tr -d ' ')
    vis_resets=$(echo "$output" | grep -c "\[VIS\] RESET" | tr -d ' ')
    vis_bits_decoded=$(echo "$output" | grep -c "All 8 bits decoded" | tr -d ' ')
    vis_not_recognized=$(echo "$output" | grep "VIS code.*not recognized" | wc -l | tr -d ' ')
    
    # Get last VIS reset details if any
    vis_reset_line=$(echo "$output" | grep "\[VIS\] RESET" | tail -1)
    
    if [ "$actual" = "$expected_vis" ]; then
        echo "✅ $name: PASS (VIS=$expected_vis)"
        return 0
    else
        # Provide detailed failure diagnostics
        if [ -z "$actual" ]; then
            echo "❌ $name: FAIL - No VIS decoded (expected $expected_vis)"
            echo "   Leaders: detected=$leader_detected, lost=$leader_lost"
            echo "   VIS: entered=$vis_entered, bits_decoded=$vis_bits_decoded, not_recognized=$vis_not_recognized"
            if [ "$vis_resets" -gt 0 ]; then
                echo "   Resets: $vis_resets"
                echo "   Last: $vis_reset_line"
            fi
        else
            echo "❌ $name: FAIL - Wrong VIS (expected $expected_vis, got $actual)"
        fi
        return 1
    fi
}

PASS=0
FAIL=0

test_file "$TEST_DIR/alt_color_bars_320x240 - Martin1.wav" "Martin1" "0xAC" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - Martin2.wav" "Martin2" "0x28" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - PD50.wav" "PD50" "0xDD" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - PD90.wav" "PD90" "0x63" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - PD120.wav" "PD120" "0x5F" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - PD160.wav" "PD160" "0xE2" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - PD180.wav" "PD180" "0x60" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - Robot24BW.wav" "Robot24BW" "0x82" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - Robot36C.wav" "Robot36C" "0x88" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - SC2-180.wav" "SC2-180" "0xB7" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - Scottie1.wav" "Scottie1" "0x3C" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - Scottie2.wav" "Scottie2" "0xB8" && ((PASS++)) || ((FAIL++))
test_file "$TEST_DIR/alt_color_bars_320x240 - ScottieDX.wav" "ScottieDX" "0xCC" && ((PASS++)) || ((FAIL++))

echo ""
echo "=========================================="
echo "Results: $PASS PASS, $FAIL FAIL"
echo "Pass rate: $PASS/$((PASS + FAIL))"
echo "=========================================="
