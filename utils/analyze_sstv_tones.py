#!/usr/bin/env python3
"""
Analyze SSTV tone frequencies in test WAV files
Shows standard (1100/1300) vs alt (1080/1320) tone energies
"""
import wave
import numpy as np
from scipy.fft import rfft, rfftfreq
import glob
import os

def analyze_wav(filepath, skip_seconds=0.5, duration=1.0):
    """Analyze frequency content of a WAV file"""
    try:
        with wave.open(filepath, 'rb') as f:
            sr = f.getframerate()
            n_samples = f.getnframes()
            
            # Skip to VIS region (usually after initial silence)
            skip_samples = int(sr * skip_seconds)
            f.setpos(skip_samples)
            
            # Read analysis window
            read_samples = int(sr * duration)
            read_samples = min(read_samples, n_samples - skip_samples)
            
            data = np.frombuffer(f.readframes(read_samples), dtype=np.int16)
            
            if len(data) == 0:
                return None
            
            # FFT analysis
            freqs = rfftfreq(len(data), 1.0/sr)
            spectrum = np.abs(rfft(data))
            
            # Extract energies at key frequencies
            result = {
                'file': os.path.basename(filepath),
                'sr': sr,
                'samples_analyzed': len(data),
            }
            
            # Standard SSTV tones
            for freq_name, freq in [('mark_std', 1100), ('space_std', 1300)]:
                idx = np.argmin(np.abs(freqs - freq))
                result[freq_name] = spectrum[idx]
            
            # MMSSTV alt tones
            for freq_name, freq in [('mark_alt', 1080), ('space_alt', 1320)]:
                idx = np.argmin(np.abs(freqs - freq))
                result[freq_name] = spectrum[idx]
            
            # Sync tone
            idx = np.argmin(np.abs(freqs - 1900))
            result['sync'] = spectrum[idx]
            
            # Calculate energy ratios
            result['std_total'] = result['mark_std'] + result['space_std']
            result['alt_total'] = result['mark_alt'] + result['space_alt']
            result['std_ratio'] = result['mark_std'] / result['space_std'] if result['space_std'] > 0 else 0
            result['alt_ratio'] = result['mark_alt'] / result['space_alt'] if result['space_alt'] > 0 else 0
            
            return result
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None

def main():
    # Analyze all test files
    test_dir = "/Users/ssamjung/Desktop/WIP/mmsstv-portable/tests/audio"
    wav_files = sorted(glob.glob(os.path.join(test_dir, "*.wav")))
    
    print("SSTV VIS Tone Analysis")
    print("=" * 100)
    print(f"{'File':<40} {'Std Mark':<10} {'Std Space':<10} {'Mark/Space':<12} {'Alt Mark':<10} {'Alt Space':<10} {'Alt Ratio':<12}")
    print("-" * 100)
    
    for filepath in wav_files:
        result = analyze_wav(filepath)
        if result:
            std_ratio = result['std_ratio']
            alt_ratio = result['alt_ratio']
            
            # Format output
            fname = result['file'][:38]
            print(f"{fname:<40} {result['mark_std']:<10.0f} {result['space_std']:<10.0f} {std_ratio:<12.3f} {result['mark_alt']:<10.0f} {result['space_alt']:<10.0f} {alt_ratio:<12.3f}")
    
    print("\nNotes:")
    print("  - Mark/Space ratio: If > 1, mark > space (bit would be 1)")
    print("  - If < 1, space > mark (bit would be 0)")
    print("  - Standard: 1100/1300 Hz")
    print("  - Alt (MMSSTV): 1080/1320 Hz")

if __name__ == "__main__":
    main()
