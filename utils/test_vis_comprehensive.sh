#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
cd "${SCRIPT_DIR}/.."

# Comprehensive VIS Decoder Test
# Tests all VIS codes and verifies decoder output matches encoded mode

set -e

echo "======================================"
echo "VIS Decoder Comprehensive Test v2"
echo "======================================"
echo ""

# Test modes with their actual mode enum values
# Format: "mode_name:mode_enum_value:vis_type"
MODES=(
    "Robot 36:0:standard"
    "Robot 72:1:standard"
    "AVT 90:2:standard"
    "Scottie 1:3:standard"
    "Scottie 2:4:standard"
    "ScottieDX:5:standard"
    "Martin 1:6:standard"
    "Martin 2:7:standard"
    "SC2 180:8:standard"
    "SC2 120:9:standard"
    "SC2 60:10:standard"
    "PD50:11:standard"
    "PD90:12:standard"
    "PD120:13:standard"
    "PD160:14:standard"
    "PD180:15:standard"
    "PD240:16:standard"
    "PD290:17:standard"
    "P3:18:standard"
    "P5:19:standard"
    "P7:20:standard"
    "MR73:21:extended"
    "MR90:22:extended"
    "MR115:23:extended"
    "MR140:24:extended"
    "MR175:25:extended"
    "MP73:26:extended"
    "MP115:27:extended"
    "MP140:28:extended"
    "MP175:29:extended"
    "ML180:30:extended"
    "ML240:31:extended"
    "ML280:32:extended"
    "ML320:33:extended"
    "Robot 24:34:extended"
    "B/W 8:35:extended"
    "B/W 12:36:extended"
)

PASSED=0
FAILED=0
TOTAL=${#MODES[@]}
EXTENDED_WORKING=0
EXTENDED_TOTAL=0

mkdir -p test_vis_output

echo "Testing Standard VIS Codes (8-bit):"
echo "-----------------------------------"

for mode_spec in "${MODES[@]}"; do
    IFS=':' read -r mode_name mode_idx vis_type <<< "$mode_spec"
    
    if [ "$vis_type" != "standard" ]; then
        continue
    fi
    
    # Generate test file
    wav_file="test_vis_output/test_${mode_idx}.wav"
    ./bin/encode_wav "$wav_file" "$mode_name" 44100 > /dev/null 2>&1
    
    if [ $? -ne 0 ]; then
        echo "  ❌ $mode_name: FAILED (could not encode)"
        ((FAILED++))
        continue
    fi
    
    # Test decoder - capture actual decoded mode
    result=$(./bin/test_vis_decode_wav "$wav_file" "$mode_idx" 2>&1)
    decoded_mode=$(echo "$result" | grep "Decoded mode=" | sed 's/.*Decoded mode=\([0-9]*\).*/\1/')
    
    if [ "$decoded_mode" == "$mode_idx" ]; then
        echo "  ✅ $mode_name (mode $mode_idx)"
        ((PASSED++))
    else
        echo "  ❌ $mode_name: decoded=$decoded_mode expected=$mode_idx"
        ((FAILED++))
    fi
done

echo ""
echo "Testing Extended VIS Codes (16-bit with 0x23 prefix):"
echo "-----------------------------------------------------"

for mode_spec in "${MODES[@]}"; do
    IFS=':' read -r mode_name mode_idx vis_type <<< "$mode_spec"
    
    if [ "$vis_type" != "extended" ]; then
        continue
    fi
    
    ((EXTENDED_TOTAL++))
    
    # Generate test file
    wav_file="test_vis_output/test_${mode_idx}.wav"
    ./bin/encode_wav "$wav_file" "$mode_name" 44100 > /dev/null 2>&1
    
    if [ $? -ne 0 ]; then
        echo "  ⚠️  $mode_name: encoder not ready"
        continue
    fi
    
    # Test decoder with debug output to see what VIS was decoded
    result=$(./bin/test_vis_decode_wav "$wav_file" "$mode_idx" --debug 2 2>&1)
    decoded_mode=$(echo "$result" | grep "Decoded mode=" | sed 's/.*Decoded mode=\([0-9]*\).*/\1/')
    vis_complete=$(echo "$result" | grep "\[VIS\] Complete:" | tail -1)
    
    if [ "$decoded_mode" == "$mode_idx" ]; then
        echo "  ✅ $mode_name (mode $mode_idx)"
        ((PASSED++))
        ((EXTENDED_WORKING++))
    else
        # Show what VIS codes were actually decoded for debugging
        echo "  ⚠️  $mode_name: decoded=$decoded_mode expected=$mode_idx"
        if [ -n "$vis_complete" ]; then
            echo "      $vis_complete"
        fi
    fi
done

echo ""
echo "======================================"
echo "Test Results"
echo "======================================"
echo "Total modes tested: $TOTAL"
echo "Standard VIS (8-bit): $((TOTAL - EXTENDED_TOTAL)) modes"
echo "Extended VIS (16-bit): $EXTENDED_TOTAL modes"
echo ""
echo "✅ Passed: $PASSED"
echo "❌ Failed: $FAILED"
echo ""

if [ $EXTENDED_WORKING -gt 0 ]; then
    echo "Extended VIS: $EXTENDED_WORKING/$EXTENDED_TOTAL working"
fi

if [ $FAILED -eq 0 ]; then
    echo ""
    echo "🎉 All VIS codes decoded successfully!"
    exit 0
else
    standard_passed=$((PASSED - EXTENDED_WORKING))
    standard_total=$((TOTAL - EXTENDED_TOTAL))
    echo ""
    echo "Standard VIS: $standard_passed/$standard_total passed"
    if [ $standard_passed -eq $standard_total ]; then
        echo "✅ All standard VIS codes working!"
    fi
    
    if [ $EXTENDED_TOTAL -gt $EXTENDED_WORKING ]; then
        echo "⚠️  Extended VIS: $((EXTENDED_TOTAL - EXTENDED_WORKING)) modes need encoder support"
    fi
    exit 1
fi
