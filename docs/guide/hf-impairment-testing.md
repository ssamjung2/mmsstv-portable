# HF impairments test (`test_hf_impairments`)

A manual (not CTest-registered) tool, `tests/test_hf_impairments.cpp`. It adds
simulated HF-channel impairments to a clean SSTV recording and writes the
signal after each stage of a **stand-alone copy** of the receive front end, so
the stages can be listened to or inspected in a spectrogram.

It does **not** decode images, and its front end is not the decoder's: it has
its own AGC, uses only the wide band-pass (HBPFS), inserts a spectral-subtraction
noise reducer, and scales the final output ×2 instead of limiting. To see what
the real decoder does to a signal, use `decode_wav_debug`
([the debug WAV guide](debug-wav.md)).

## Build and run

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --target test_hf_impairments
./bin/test_hf_impairments <input.wav> [output_dir] [snr_db] [--dsp-only]
```

| Argument | Meaning |
| --- | --- |
| `input.wav` | 16-bit mono PCM WAV |
| `output_dir` | Output directory (default `./hf_test_output`, created if missing) |
| `snr_db` | Parsed and printed, but **not used** by the impairment model |
| `--dsp-only` | Skip the impairments; run the clean signal through the pipeline |

The usage text also lists a `signal_scale` argument; it is not parsed. The
input is always scaled by 0.5.

## Impairment model (default mode)

The input is processed five times, once per noise-floor step, with RMS
2000, 6000, 10000, 15000 and 20000 (16-bit PCM units). For each step:

1. The input is scaled by 0.5.
2. **Fading:** multiplied by `0.5 + 0.5 × r`, where `r` is a Rayleigh
   variate low-pass filtered at 0.2 Hz (one-pole) and normalised to mean 1,
   i.e. about 6 dB of slow fading.
3. **Noise:** white Gaussian noise at the step's RMS. In two random,
   non-overlapping windows (each 10 % of the file) the RMS is replaced by a
   random level from the same list.
4. **Hum:** 50, 100 and 150 Hz sines (0.5, 0.3, 0.2 relative), scaled to a
   peak of about 10 PCM units.

A new random seed is used on every run, so results are not reproducible
run-to-run.

## Processing chain

| Stage | Implementation in the test |
| --- | --- |
| Noise reduction | `SpectralSubtractionDNR(1024, 256)`: frame spectrum minus a running-average noise estimate, 8 % floor, Hann window, overlap-add |
| 2-tap average + clip | `(x[n] + x[n−1]) / 2`, clipped to ±24576 |
| Band-pass | HBPFS only: 400–2500 Hz, `24·fs/11025` taps, att 20 dB |
| AGC | The test's own AGC: level follower `lvl += (abs(x) − lvl) × 0.2`, peak `top` decaying ×0.99995 per sample (min 1), gain `512 / top` |
| Final | AGC output × 2, clamped to ±16384 |

## Output files

Default mode, for each noise step `N` = 1…5:

| File | Content |
| --- | --- |
| `00_clean_input.wav` | Input, unchanged (written once) |
| `01_with_noise_lvlN.wav` | After impairments |
| `01b_after_dnr_lvlN.wav` | After spectral subtraction |
| `02_after_lpf_lvlN.wav` | After the 2-tap average and clip |
| `03_after_bpf_lvlN.wav` | After the band-pass |
| `04_after_agc_lvlN.wav` | After the test's AGC |
| `05_final_lvlN.wav` | ×2, clamped |

With `--dsp-only` the same stages are written once as `01b_after_dnr_clean.wav`,
`02_after_lpf_clean.wav`, … `05_final_clean.wav`. At the end the program
prints the AGC's final gain, peak level and an S/N estimate
(`top / max top`).

## Relating noise RMS to S-units

See [the S-unit reference](../standards/s-units-and-dbm.md). The RMS values above
are empirical choices for audible, realistic noise, not a calibrated S-meter
scale. The S-unit labels printed by the program (for example "S7 noise
floor") are nominal.

## Checking the result with the decoder

The stage outputs are not decoder inputs in any meaningful sense (the decoder
has its own front end). To test decoding under noise, decode
`01_with_noise_lvlN.wav` directly:

```bash
./bin/decode_wav hf_test_output/01_with_noise_lvl1.wav noisy_lvl1.ppm
```
