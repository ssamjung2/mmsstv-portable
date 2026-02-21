# S-Unit and dBm Reference for HF Noise Floor Calibration

## S-Meter Standard (ARRL/ITU-R P.372)
- **S9** = -73 dBm (50 ohm, 0.5 uV RMS)
- Each S-unit = 6 dB
- S8 = -79 dBm
- S7 = -85 dBm
- S6 = -91 dBm
- S5 = -97 dBm
- S4 = -103 dBm
- S3 = -109 dBm
- S2 = -115 dBm
- S1 = -121 dBm

## Typical 20m Band Noise Floor
- **Daytime S7-S9**: -85 to -73 dBm (urban)
- **Rural/quiet**: S5-S7 (-97 to -85 dBm)
- **Noisy city**: S9+ (-73 dBm or higher)

## dBm to 16-bit PCM RMS Conversion
- 0 dBm (1 mW into 50 ohm): 0.2236 V RMS
- For 16-bit PCM, full scale (±32767) = 1.0 V peak = 0.707 V RMS
- So, 1 PCM unit = 0.707 / 32767 ≈ 21.6 uV RMS
- To convert dBm to PCM RMS:
  1. Convert dBm to V RMS: $V_{RMS} = \sqrt{P_{W} \times 50}$, $P_{W} = 10^{(dBm-30)/10}$
  2. Divide by 21.6 uV to get PCM units

## Example: S9 Noise Floor in PCM
- S9 = -73 dBm
- $P_{W} = 10^{(-73-30)/10} = 5.01 \times 10^{-11}$ W
- $V_{RMS} = \sqrt{5.01 \times 10^{-11} \times 50} = 1.58 \times 10^{-4}$ V = 158 uV
- PCM RMS = 158 uV / 21.6 uV ≈ 7.3
- But this is for a single tone; for wideband noise, scale for bandwidth and AGC.

## Practical PCM RMS for Simulated Noise
- In practice, to make noise audible and match real S-meter readings, use:
  - S9: PCM RMS ≈ 7-10 (for 1 kHz tone, not noise)
  - For wideband noise, PCM RMS ≈ 2000-8000 (empirical, matches real HF audio)
- **Current code uses much higher RMS for realism** (matches what is heard on air, not strict S-meter math)

## Recommendation
- For realism, keep using empirically calibrated PCM RMS (e.g., 16000 for S9+6dB) but document the mapping and rationale.
- Optionally, add a mode to set noise floor by S-unit or dBm for advanced users.

---

**References:**
- ARRL Handbook, "S-Meter Calibration"
- ITU-R P.372-16, "Radio Noise" (2022)
- https://www.arrl.org/s-meter
- https://www.itu.int/dms_pubrec/itu-r/rec/p/R-REC-P.372-16-202202-I!!PDF-E.pdf
