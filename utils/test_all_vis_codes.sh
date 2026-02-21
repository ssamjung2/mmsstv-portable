#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
cd "${SCRIPT_DIR}/.."

# Test all VIS codes
# This script encodes and decodes all supported SSTV modes to verify VIS decoder

set -e

echo "======================================"
echo "VIS Decoder Comprehensive Test"
echo "======================================"
echo ""

# Define test modes: mode_name, mode_index, vis_code
MODES=(
    "Robot 36:0:0x88"
    "Robot 72:1:0x0C"
    "AVT 90:2:0x44"
    "Scottie 1:3:0x3C"
    "Scottie 2:4:0xB8"
    "ScottieDX:5:0xCC"
    "Martin 1:6:0xAC"
    "Martin 2:7:0x28"
    "SC2 180:8:0xB7"
    "SC2 120:9:0x3F"
    "SC2 60:10:0xBB"
    "PD50:11:0xDD"
    "PD90:12:0x63"
    "PD120:13:0x5F"
    "PD160:14:0xE2"
    "PD180:15:0x60"
    "PD240:16:0xE1"
    "PD290:17:0xDE"
    "P3:18:0x71"
    "P5:19:0x72"
    "P7:20:0xF3"
    "MR73:21:0x45"
    "MR90:22:0x46"
    "MR115:23:0x49"
    "MR140:24:0x4A"
    "MR175:25:0x4C"
    "MP73:26:0x25"
    "MP115:27:0x29"
    "MP140:28:0x2A"
    "MP175:29:0x2C"
    "ML180:30:0x85"
    "ML240:31:0x86"
    "ML280:32:0x89"
    "ML320:33:0x8A"
    "Robot 24:34:0x84"
    "B/W 8:35:0x82"
    "B/W 12:36:0x86"
)

PASSED=0
FAILED=0
TOTAL=${#MODES[@]}

mkdir -p test_vis_output

for mode_spec in "${MODES[@]}"; do
    IFS=':' read -r mode_name mode_idx vis_code <<< "$mode_spec"
    
    echo "Testing: $mode_name (mode $mode_idx, VIS $vis_code)"
    
    # Generate test file
    wav_file="test_vis_output/test_${mode_idx}.wav"
    ./bin/encode_wav "$wav_file" "$mode_name" 44100 > /dev/null 2>&1
    
    if [ $? -ne 0 ]; then
        echo "  ❌ FAILED: Could not encode"
        ((FAILED++))
        continue
    fi
    
    # Test decoder
    result=$(./bin/test_vis_decode_wav "$wav_file" "$mode_idx" 2>&1 | grep "Decoded mode")
    
    if echo "$result" | grep -q "Decoded mode=$mode_idx (expected=$mode_idx)"; then
        echo "  ✅ PASSED: VIS $vis_code decoded correctly"
        ((PASSED++))
    else
        echo "  ❌ FAILED: $result"
        ((FAILED++))
    fi
done

echo ""
echo "======================================"
echo "Test Results"
echo "======================================"
echo "Total:  $TOTAL modes"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ All VIS codes decoded successfully!"
    exit 0
else
    echo "❌ Some tests failed"
    exit 1
fi
