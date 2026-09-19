# Command-line tools

Built with `BUILD_EXAMPLES=ON` (the default) into `bin/`. They are small
programs over the two libraries, and double as worked examples: `decode_wav`
in particular shows the whole decode loop in about 60 lines
(`utils/decode_wav.c`).

None of them takes option flags yet; arguments are positional, and the
brackets below mark optional ones.

| Tool | Usage | Purpose |
| --- | --- | --- |
| `list_modes` | `list_modes` | Print the mode table with sizes, VIS codes and durations, plus a tone reference. |
| `encode_wav` | `encode_wav out.wav ["mode name"] [rate]` | Encode colour bars in one mode (default Scottie 1 at 48 kHz) to a 16-bit mono WAV. Mode names are matched case-insensitively, e.g. `"Martin 1"`. |
| `generate_all_modes` | `generate_all_modes [dir] [rate]` | Encode colour bars in all 43 modes into a directory, with a `REPORT.txt` summary. |
| `decode_wav` | `decode_wav in.wav [out.ppm]` | Decode a 16-bit mono WAV into a PPM image. |
| `decode_wav_debug` | `decode_wav_debug in.wav [prefix]` | Decode and also write the signal after each front-end stage as WAV files, see [debug-wav.md](debug-wav.md). |
| `evaluate_decoded_images` | `evaluate_decoded_images [decoded_dir] [reference_dir]` | Compare decoded PPMs against reference images and report per-image error, flagging slant and phase problems. |
| `test_real_images` | `test_real_images [rate]` | Encode two photographs to WAV. The paths are hard-coded to a local checkout and need editing before use. |

## Encode and decode a file

```bash
cmake -S . -B build && cmake --build build -j

./bin/encode_wav /tmp/martin1.wav "Martin 1" 48000
./bin/decode_wav /tmp/martin1.wav /tmp/martin1.ppm
```

`decode_wav` prints the detected mode and line progress, then writes a PPM.
Converting that to PNG needs an external tool, for example
`sips -s format png /tmp/martin1.ppm --out /tmp/martin1.png` on macOS or
`convert` from ImageMagick.

## Input requirements

`decode_wav` and `decode_wav_debug` read uncompressed 16-bit PCM mono WAV
files at any sample rate. Stereo or floating-point WAV files are rejected;
convert them first, for example with `ffmpeg -i in.wav -ac 1 -c:a pcm_s16le out.wav`.

## Diagnostic programs

`BUILD_UTILS=ON` builds the probes in `utils/` used while porting the DSP
(filter responses, VCO output, tone detection). They print numbers for
comparison against MMSSTV rather than doing anything useful on their own; see
[the filter specifications](../specs/filters.md). The same directory holds
Python scripts for analysing WAV files and shell scripts that drive batch
experiments; they are development aids, not part of the build.
