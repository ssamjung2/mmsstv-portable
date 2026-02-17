#!/usr/bin/env python3
"""
Find exact VIS data start timing by scanning for frequency transitions
"""
import sys
import wave
import struct
import math

def goertzel(samples, target_freq, sample_rate):
    """Goertzel algorithm"""
    k = int(0.5 + (len(samples) * target_freq / sample_rate))
    omega = (2.0 * math.pi * k) / len(samples)
    cosine, sine = math.cos(omega), math.sin(omega)
    coeff = 2.0 * cosine
    q0, q1, q2 = 0.0, 0.0, 0.0
    for sample in samples:
        q0 = coeff * q1 - q2 + sample
        q2, q1 = q1, q0
    real, imag = q1 - q2 * cosine, q2 * sine
    return math.sqrt(real*real + imag*imag) / len(samples)

filename = sys.argv[1] if len(sys.argv) > 1 else "tests/audio/alt_color_bars_320x240 - Robot36C.wav"

with wave.open(filename, 'rb') as wav:
    framerate = wav.getframerate()
    n_channels = wav.getnchannels()
    samples_to_read = int(0.7 * framerate)
    audio_bytes = wav.readframes(samples_to_read)

samples = struct.unpack('h' * (len(audio_bytes) // 2), audio_bytes)
if n_channels == 2:
    samples = samples[::2]
samples = [s / 32768.0 for s in samples]

print(f"Scanning for VIS data boundaries (30ms windows every 5ms)...")
print(f"{'Time(ms)':>8} | {'1100Hz':>7} {'1200Hz':>7} {'1300Hz':>7} {'1900Hz':>7} | Dominant | Expected for Robot36")
print("-" * 90)

robot36_bits = "00010001"  # LSB-first for 0x88
bit_idx = 0

for t_ms in range(0, 600, 5):  # Scan every 5ms
    t_sec = t_ms / 1000.0
    window_ms = 30  # 30ms window (one bit period)
    start_idx = int(t_sec * framerate)
    end_idx = int((t_sec + window_ms/1000.0) * framerate)
    seg = samples[start_idx:end_idx]
    
    if len(seg) < 100:
        continue
    
    e1100 = goertzel(seg, 1100, framerate)
    e1200 = goertzel(seg, 1200, framerate)
    e1300 = goertzel(seg, 1300, framerate)
    e1900 = goertzel(seg, 1900, framerate)
    
    energies = {1100: e1100, 1200: e1200, 1300: e1300, 1900: e1900}
    dominant = max(energies, key=energies.get)
    
    # Determine what we expect
    if t_ms < 300:
        expected = "Leader (1900 Hz)"
    elif t_ms < 310:
        expected = "Break (1200 Hz)"
    elif t_ms < 340:
        expected = "Start bit (1200 Hz)"
    else:
        # Data bits start at 340ms, 30ms each
        bit_offset = (t_ms - 340) // 30
        if bit_offset < 8 and bit_idx < len(robot36_bits):
            bit_val = robot36_bits[bit_offset]
            expected_freq = "1300" if bit_val == '0' else "1100"
            expected = f"Bit {bit_offset} ({bit_val}) → {expected_freq} Hz"
        elif 340 + 8*30 <= t_ms < 340 + 9*30:
            expected = "Parity bit"
        elif 340 + 9*30 <= t_ms < 340 + 10*30:
            expected = "Stop bit (1200 Hz)"
        else:
            expected = "Image data"
    
    # Only print if there's significant energy
    total_energy = sum(energies.values())
    if total_energy > 0.01:
        status = "✓" if (
            (t_ms < 300 and dominant == 1900) or
            (300 <= t_ms < 340 and dominant == 1200) or
            (340 <= t_ms < 580 and dominant in [1100, 1300])
        ) else "✗"
        
        print(f"{t_ms:8d} | {e1100:7.4f} {e1200:7.4f} {e1300:7.4f} {e1900:7.4f} | {dominant:4d} Hz {status} | {expected}")
