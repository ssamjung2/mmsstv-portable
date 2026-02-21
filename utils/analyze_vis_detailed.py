#!/usr/bin/env python3
"""
Analyze VIS code section of an SSTV WAV file to verify tone frequencies.
Focuses on the 8 data bits (after leader and start bit).
"""
import sys
import numpy as np
import wave
from scipy import signal as sp_signal

def analyze_vis_wav(filename):
    with wave.open(filename, 'rb') as wf:
        nchannels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        framerate = wf.getframerate()
        nframes = wf.getnframes()
        
        print(f"WAV file: {filename}")
        print(f"Channels: {nchannels}, Sample width: {sampwidth}, Rate: {framerate} Hz, Frames: {nframes}")
        print(f"Duration: {nframes/framerate:.3f} seconds\n")
        
        # Read all frames
        frames = wf.readframes(nframes)
        if sampwidth == 2:
            samples = np.frombuffer(frames, dtype=np.int16)
        elif sampwidth == 4:
            samples = np.frombuffer(frames, dtype=np.int32)
        else:
            samples = np.frombuffer(frames, dtype=np.uint8)
        
        # Convert to mono if stereo
        if nchannels == 2:
            samples = samples[::2]  # Take left channel
        
        # Convert to float
        samples = samples.astype(np.float64)
        if sampwidth == 2:
            samples /= 32768.0
        elif sampwidth == 4:
            samples /= 2147483648.0
        else:
            samples = (samples - 128) / 128.0
    
    # VIS timing for Robot 36:
    # 800ms preamble (alternating 1900/1500/2300 Hz tones)
    # 300ms leader @ 1900 Hz
    # 10ms break @ 1200 Hz
    # 300ms leader @ 1900 Hz
    # 30ms start bit @ 1200 Hz
    # 8x 30ms data bits @ 1100/1300 Hz (LSB-first)
    # 30ms stop bit @ 1200 Hz
    
    # Start analyzing from 800ms (end of preamble, start of VIS)
    start_time = 0.800  # 800ms preamble
    start_sample = int(start_time * framerate)
    
    print("=== VIS Code Analysis ===")
    print("Robot 36 (0x88) expected bits (LSB-first): 0,0,0,1,0,0,0,1")
    print("Expected frequencies: 1100,1100,1100,1300,1100,1100,1100,1300 Hz")
    print(f"Starting analysis at {start_time}s (sample {start_sample}, after preamble)\n")
    
    # Analyze each of the 9 VIS bit periods (start + 8 data bits)
    # These come after two 300ms leaders and one 10ms break
    bit_names = ["Leader1", "Break", "Leader2", "Start", "Bit0", "B it1", "Bit2", "Bit3", "Bit4", "Bit5", "Bit6", "Bit7"]
    expected_freqs = [1900, 1200, 1900, 1200, 1100, 1100, 1100, 1300, 1100, 1100, 1100, 1300]
    bit_durations = [300, 10, 300, 30, 30, 30, 30, 30, 30, 30, 30, 30]  # in ms
    
    for bit_idx in range(len(bit_names)):
        # Calculate bit duration in samples
        bit_samples = int(bit_durations[bit_idx] / 1000.0 * framerate)
        bit_start = start_sample
        bit_end = bit_start + bit_samples
        start_sample = bit_end  # Move to next bit
        
        # Extract samples for this bit period
        bit_samples = samples[bit_start:bit_end]
        
        # Compute FFT for the entire bit period
        fft = np.fft.rfft(bit_samples)
        freqs = np.fft.rfftfreq(len(bit_samples), 1.0 / framerate)
        magnitudes = np.abs(fft)
        
        # Find peak in 1000-2000 Hz range
        mask = (freqs >= 1000) & (freqs <= 2000)
        peak_idx = np.argmax(magnitudes[mask])
        peak_freq = freqs[mask][peak_idx]
        peak_mag = magnitudes[mask][peak_idx]
        
        # Also check specific frequencies
        def get_mag_at_freq(target_freq, window=10):
            mask = (freqs >= target_freq - window) & (freqs <= target_freq + window)
            if np.any(mask):
                return np.max(magnitudes[mask])
            return 0.0
        
        mag_1100 = get_mag_at_freq(1100)
        mag_1200 = get_mag_at_freq(1200)
        mag_1300 = get_mag_at_freq(1300)
        mag_1900 = get_mag_at_freq(1900)
        
        # Determine what frequency is strongest
        if mag_1100 > mag_1200 and mag_1100 > mag_1300:
            detected = "1100 Hz"
        elif mag_1300 > mag_1100 and mag_1300 > mag_1200:
            detected = "1300 Hz"
        elif mag_1200 > mag_1100 and mag_1200 > mag_1300:
            detected = "1200 Hz"
        else:
            detected = f"{peak_freq:.0f} Hz"
        
        expected = f"{expected_freqs[bit_idx]} Hz"
        status = "✓" if detected == expected else "✗"
        
        print(f"{bit_names[bit_idx]}: peak={peak_freq:.1f}Hz (mag={peak_mag:.1f}) "
              f"| 1100={mag_1100:.1f} 1200={mag_1200:.1f} 1300={mag_1300:.1f} "
              f"| detected={detected} expected={expected} {status}")
    
    # Now analyze at bit centers (15ms into each bit) with shorter window
    print("\n=== Analysis at Bit Centers (15ms windows) ===")
    
    # Start at beginning of first data bit (after preamble + 2 leaders + break + start)
    # 800ms + 300ms + 10ms + 300ms + 30ms = 1440ms
    data_start_time = 0.800 + 0.300 + 0.010 + 0.300 + 0.030  # 1.440 seconds
    data_start_sample = int(data_start_time * framerate)
    
    for bit_idx in range(8):  #  8 data bits
        # Center of bit is at 15ms into the bit period
        bit_center = data_start_sample + bit_idx * int(0.030 * framerate) + int(0.015 * framerate)
        
        # Extract 15ms window centered on this point
        window_samples = int(0.015 * framerate)
        center_start = bit_center - window_samples // 2
        center_end = center_start + window_samples
        
        if center_end <= len(samples):
            center_data = samples[center_start:center_end]
            
            # Compute FFT
            fft = np.fft.rfft(center_data)
            freqs = np.fft.rfftfreq(len(center_data), 1.0 / framerate)
            magnitudes = np.abs(fft)
            
            # Find peak
            mask = (freqs >= 1000) & (freqs <= 2000)
            peak_idx = np.argmax(magnitudes[mask])
            peak_freq = freqs[mask][peak_idx]
            
            # Get magnitudes at key frequencies
            def get_mag_at_freq(target_freq, window=10):
                mask = (freqs >= target_freq - window) & (freqs <= target_freq + window)
                if np.any(mask):
                    return np.max(magnitudes[mask])
                return 0.0
            
            mag_1100 = get_mag_at_freq(1100)
            mag_1300 = get_mag_at_freq(1300)
            
            ratio = mag_1300 / mag_1100 if mag_1100 > 0 else 0
            decoded = 1 if mag_1300 > mag_1100 else 0
            expected_bit = [0,0,0,1,0,0,0,1][bit_idx]
            status = "✓" if decoded == expected_bit else "✗"
            
            print(f"Bit[{bit_idx}]: peak={peak_freq:.1f}Hz | mag1100={mag_1100:.1f} mag1300={mag_1300:.1f} "
                  f"ratio(1300/1100)={ratio:.2f} | decoded={decoded} expected={expected_bit} {status}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python analyze_vis_detailed.py <wav_file>")
        sys.exit(1)
    
    analyze_vis_wav(sys.argv[1])
