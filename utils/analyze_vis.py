#!/usr/bin/env python3
"""
Analyze VIS codes and transmission format
"""

# VIS codes from decoder.cpp
vis_codes = {
    'Robot24': 0x84,
    'Robot36': 0x88,
    'Robot72': 0x0C,
    'Martin1': 0xAC,
    'Martin2': 0x28,
    'Scottie1': 0x3C,
    'Scottie2': 0xB8,
    'ScottieDX': 0xCC,
    'PD50': 0xDD,
    'PD90': 0x63,
    'PD120': 0x5F,
}

def analyze_code(name, code):
    """Analyze a VIS code"""
    print(f"\n{name}: 0x{code:02X} = {code:08b}")
    
    # Extract 7-bit data (bits 0-6) and parity (bit 7)
    data_7bit = code & 0x7F
    parity_bit = (code >> 7) & 1
    
    print(f"  7-bit data: 0x{data_7bit:02X} = {data_7bit:07b}")
    print(f"  Parity bit: {parity_bit}")
    
    # Count 1s in 7-bit data for parity check
    ones_count = bin(data_7bit).count('1')
    expected_parity = ones_count % 2  # Even parity
    
    print(f"  Ones in data: {ones_count}")
    print(f"  Expected parity (even): {expected_parity}")
    print(f"  Parity {'OK' if parity_bit == expected_parity else 'FAIL'}")
    
    # Show bit order for transmission (LSB-first means bit 0 transmitted first)
    print(f"  Transmission order (LSB-first): ", end='')
    for i in range(8):
        bit = (code >> i) & 1
        print(bit, end='')
    print()
    
    # Show what tones we'd transmit (1=1100Hz mark, 0=1300Hz space)
    print(f"  Tones (LSB-first): ", end='')
    for i in range(8):
        bit = (code >> i) & 1
        tone = "1100" if bit else "1300"
        print(f"{tone}Hz ", end='')
    print()
    
    # Show bit-reversed version (if we read MSB-first by mistake)
    reversed_7bit = int(f"{data_7bit:07b}"[::-1], 2)
    reversed_8bit = int(f"{code:08b}"[::-1], 2)
    print(f"  Bit-reversed (7-bit): 0x{reversed_7bit:02X}")
    print(f"  Bit-reversed (8-bit): 0x{reversed_8bit:02X}")

print("="*60)
print("VIS CODE ANALYSIS")
print("="*60)

for name, code in vis_codes.items():
    analyze_code(name, code)

print("\n" + "="*60)
print("WHAT IF WE'RE READING ALL ZEROS (0x00)?")
print("="*60)
print("\nIf decoder consistently gets 0x00, this means:")
print("  - All 8 bits read as 0")
print("  - Space tone (1300 Hz) detected for all bits")
print("  - Possible causes:")
print("    1. Mark/Space filters swapped (1100<->1300)")
print("    2. Bit decision inverted (< instead of >)")
print("    3. Reading wrong signal timing/phase")
print("    4. WAV files have non-standard encoding")
