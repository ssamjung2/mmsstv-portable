#!/bin/bash
cd /Users/ssamjung/Desktop/WIP/mmsstv-portable

echo "Testing VIS decoder on all WAV files..."
echo "========================================"

for file in tests/audio/*.wav; do
    basename=$(basename "$file" .wav)
    mode_name=$(echo "$basename" | sed 's/alt_color_bars_320x240 - //')
    
    # Determine expected mode number
    case "$mode_name" in
        "Robot36C") expected=0;;
        "Robot24BW") expected=34;;
        "Martin1") expected=6;;
        "Martin2") expected=7;;
        "Scottie1") expected=3;;
        "Scottie2") expected=4;;
        "ScottieDX") expected=5;;
        "PD50") expected=11;;
        "PD90") expected=12;;
        "PD120") expected=13;;
        "SC2-180") expected=8;;
        *) expected=99;;
    esac
    
    if [ "$expected" != "99" ]; then
        # Run decoder and extract VIS code
        vis_code=$(./bin/test_vis_decode_wav "$file" "$expected" --debug 3 2>&1 | grep -m 1 "VIS code extracted" | sed 's/.*: //')
        decoded_mode=$(./bin/test_vis_decode_wav "$file" "$expected" 2>&1 | grep "Decoded mode=" | sed 's/.*mode=\([0-9]*\).*/\1/')
        
        printf "%-15s  VIS=%-6s  Decoded=%-3s  Expected=%s\n" "$mode_name" "$vis_code" "$decoded_mode" "$expected"
    fi
done
