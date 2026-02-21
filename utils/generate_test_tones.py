#!/usr/bin/env python3
"""Generate a pure sine wave test tone WAV file."""
import numpy as np
import wave
import struct

def generate_tone(filename, frequency, duration, sample_rate=44100):
    """Generate a pure sine wave at specified frequency."""
    num_samples = int(duration * sample_rate)
    t = np.arange(num_samples) / sample_rate
    samples = np.sin(2.0 * np.pi * frequency * t)
    
    # Convert to 16-bit PCM
    samples_int = np.int16(samples * 32767)
    
    # Write WAV file
    with wave.open(filename, 'w') as wf:
        wf.setnchannels(1)  # Mono
        wf.setsampwidth(2)   # 16-bit
        wf.setframerate(sample_rate)
        wf.writeframes(samples_int.tobytes())
    
    print(f"Generated {filename}: {frequency} Hz, {duration}s, {sample_rate} Hz")

if __name__ == "__main__":
    # Generate test tones
    generate_tone("tone_1100.wav", 1100.0, 1.0)
    generate_tone("tone_1300.wav", 1300.0, 1.0)
    generate_tone("tone_1900.wav", 1900.0, 1.0)
