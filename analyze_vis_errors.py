#!/usr/bin/env python3

tests = [
    ('Martin1', 0xAC, 0x9A),
    ('PD120', 0x5F, 0x7D),
    ('PD160', 0xE2, 0x93),
    ('PD180', 0x60, 0x03),
    ('SC2-180', 0xB7, 0xF6),
    ('Scottie2', 0xB8, 0x8E),
    ('ScottieDX', 0xCC, 0x99),
    ('Robot24BW', 0x82, 0x0A),
]

print("=== VIS Code Analysis ===\n")
print(f"{'Test':<12} {'Expected':<10} {'Got':<10} {'Hamming':<8} {'Pattern'}")
print("=" * 70)

for name, exp, got in tests:
    # Hamming distance
    xor = exp ^ got
    hamming = bin(xor).count('1')
    
    # Check patterns
    reversed_bits = int(format(exp, '08b')[::-1], 2)
    right_shift_1 = (exp >> 1) | ((exp & 1) << 7)
    left_shift_1 = ((exp << 1) & 0xFF) | (exp >> 7)
    
    pattern = []
    if got == reversed_bits:
        pattern.append("BIT_REVERSE")
    if got == right_shift_1:
        pattern.append("RIGHT_SHIFT")
    if got == left_shift_1:
        pattern.append("LEFT_SHIFT")
    
    pattern_str = ",".join(pattern) if pattern else "RANDOM_ERRORS"
    
    print(f"{name:<12} {exp:#04x} ({exp:08b})  {got:#04x} ({got:08b})  {hamming:<8} {pattern_str}")

# Check if swapping bit pairs helps
print("\n=== Checking Bit Timing Offset ===")
for name, exp, got in tests:
    # Try rotating/shifting to find alignment
    best_match = 100
    best_shift = 0
    for shift in range(8):
        rotated = ((got << shift) | (got >> (8 - shift))) & 0xFF
        hamming = bin(exp ^ rotated).count('1')
        if hamming < best_match:
            best_match = hamming
            best_shift = shift
    
    if best_match < 3:
        print(f"{name:<12} Best match at shift {best_shift}: {best_match} bit errors")
