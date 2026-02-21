#!/bin/bash
# Test different VIS start timing offsets to find the right one
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
cd "${SCRIPT_DIR}/.."

echo "Testing different VIS timing offsets for Robot36..."
echo "Expected VIS code: 0x88"
echo ""

for offset_ms in 40 50 60 70 80 90 100 110 120 130 140 150; do
    # Modify decoder.cpp with new timing
    sed -i.bak "s/const int WAIT_SAMPLES = (int)([0-9.]*  \* dec->sample_rate);/const int WAIT_SAMPLES = (int)(0.${offset_ms} * dec->sample_rate);/" src/decoder.cpp
    
    # Rebuild
    cd build && make -s test_vis_decode_wav > /dev/null 2>&1 && cd ..
    
    # Test
    vis_code=$(./bin/test_vis_decode_wav "tests/audio/alt_color_bars_320x240 - Robot36C.wav" 0 --debug 3 2>&1 | grep -m 1 "VIS code extracted" | sed 's/.*: //')
    mode=$(./bin/test_vis_decode_wav "tests/audio/alt_color_bars_320x240 - Robot36C.wav" 0 2>&1 | grep "Decoded mode=" | sed 's/.*mode=\([0-9]*\).*/\1/')
    
    printf "%3dms: VIS=%s  Mode=%-3s  %s\n" "$offset_ms" "$vis_code" "$mode" "$([ "$vis_code" = "0x88" ] && echo '✓ MATCH!' || echo '')"
done

# Restore original
mv src/decoder.cpp.bak src/decoder.cpp
