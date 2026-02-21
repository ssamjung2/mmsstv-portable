import wave
import struct
import math

def make_tone(filename, freq, duration=0.5, sample_rate=48000):
    """Generate a pure tone WAV file"""
    num_samples = int(sample_rate * duration)
    with wave.open(filename, 'w') as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)  # 16-bit
        wav.setframerate(sample_rate)
        
        for i in range(num_samples):
            sample = int(16000 * math.sin(2 * math.pi * freq * i / sample_rate))
            wav.writeframes(struct.pack('<h', sample))

# Make test tones
make_tone('tone_1080.wav', 1080)
make_tone('tone_1320.wav', 1320)
print("Created tone_1080.wav and tone_1320.wav")
