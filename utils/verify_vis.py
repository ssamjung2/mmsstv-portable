#!/usr/bin/env python3
"""Verify VIS code 0x88 bit encoding"""

vis_code = 0x88  # Robot 36

print(f"VIS Code: 0x{vis_code:02x} = {vis_code} decimal = 0b{vis_code:08b} binary\n")
print("LSB-first transmission:")
print("Bit#  BitVal  Frequency")
print("=" * 30)

for bit_idx in range(8):
    bit_val = (vis_code >> bit_idx) & 1
    freq = 1300 if bit_val else 1100
    print(f" {bit_idx}      {bit_val}      {freq} Hz")

# Calculate parity
parity = bin(vis_code).count('1') % 2  # odd parity
print(f"\nParity (odd): {parity} = {1300 if parity else 1100} Hz")
print(f"\nExpected: Only bits 3 and 7 should be 1 (1300 Hz)")
