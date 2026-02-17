#!/bin/bash

echo "VIS Decoder Validation - All Authoritative Test Files"
echo "======================================================"
echo ""
printf "%-15s | %-10s | %-10s | %s\n" "Mode Name" "Expected" "Decoded" "Status"
printf "%.15s-+-%.10s-+-%.10s-+-%s\n" "---------------" "----------" "----------" "----------"

# Test each file
test_file() {
    local name=$1
    local expected=$2
    local file="tests/audio/alt_color_bars_320x240 - ${name}.wav"
    
    if [ ! -f "$file" ]; then
        printf "%-15s | %-10s | %-10s | %s\n" "$name" "$expected" "NO FILE" "✗ SKIP"
        return
    fi
    
    local result=$(./bin/test_vis_decode_wav "$file" 0 --debug 2 2>&1 | grep "VIS decoded:" | tail -1)
    
    if [ -z "$result" ]; then
        printf "%-15s | %-10s | %-10s | %s\n" "$name" "$expected" "NONE" "✗ FAIL"
    else
        local decoded=$(echo "$result" | sed 's/.*VIS decoded: \(0x[0-9a-fA-F]*\).*/\1/' | tr '[:lower:]' '[:upper:]')
        local expected_upper=$(echo "$expected" | tr '[:lower:]' '[:upper:]')
        
        if [ "$decoded" = "$expected_upper" ]; then
            printf "%-15s | %-10s | %-10s | %s\n" "$name" "$expected" "$decoded" "✓ PASS"
        else
            printf "%-15s | %-10s | %-10s | %s\n" "$name" "$expected" "$decoded" "✗ WRONG"
        fi
    fi
}

# Test all modes
test_file "Robot36C" "0x88"
test_file "Robot24BW" "0x84"
test_file "Scottie1" "0x3C"
test_file "Scottie2" "0xB8"
test_file "ScottieDX" "0xCC"
test_file "Martin1" "0xAC"
test_file "Martin2" "0x28"
test_file "SC2-180" "0xB7"
test_file "PD50" "0xDD"
test_file "PD90" "0x63"
test_file "PD120" "0x5F"
test_file "PD160" "0xE2"
test_file "PD180" "0x60"

echo ""
echo "Test complete!"
