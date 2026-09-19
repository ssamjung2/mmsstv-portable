# mmsstv-portable

A portable C/C++ SSTV (slow-scan television) encoder and decoder library,
ported from **MMSSTV** by Makoto Mori (JE3HHT) and Nobuyuki Oba.

- **Encoder** (`libsstv_encoder`): image → audio for all 43 MMSSTV modes, with
  the same preamble, VIS/identification headers, line structure and tones that
  MMSSTV transmits.
- **Decoder** (`libsstv_decoder`): audio → RGB image for all 43 modes. It
  identifies the mode from the header (VIS, MMSSTV's 16-bit VIS, or the
  narrow-mode FSK "N-VIS") and re-locks to the line sync every line, so small
  sample-clock mismatches do not slant the image.

The reference for both is the original MMSSTV source and the SSTV Handbook
(`docs/sstv-handbook.pdf`). Where they are compared with this code, see
[MMSSTV_ARCHITECTURE_ANALYSIS.md](MMSSTV_ARCHITECTURE_ANALYSIS.md).

## Supported modes

"Total TX" includes the 800 ms preamble (400 ms for narrow modes) and the header.

| Mode | Enum | Size | Colour | Header / code | Image (s) | Total TX (s) |
| --- | --- | --- | --- | --- | --- | --- |
| Robot 36 | `SSTV_R36` | 320×240 | yes | VIS `0x88` | 36.0 | 37.7 |
| Robot 72 | `SSTV_R72` | 320×240 | yes | VIS `0x0C` | 72.0 | 73.7 |
| AVT 90 | `SSTV_AVT90` | 320×240 | yes | VIS ×3 + AVT header `0x44` | 90.0 | 98.8 |
| Scottie 1 | `SSTV_SCOTTIE1` | 320×256 | yes | VIS `0x3C` | 109.6 | 111.3 |
| Scottie 2 | `SSTV_SCOTTIE2` | 320×256 | yes | VIS `0xB8` | 71.1 | 72.8 |
| ScottieDX | `SSTV_SCOTTIEX` | 320×256 | yes | VIS `0xCC` | 268.9 | 270.6 |
| Martin 1 | `SSTV_MARTIN1` | 320×256 | yes | VIS `0xAC` | 114.3 | 116.0 |
| Martin 2 | `SSTV_MARTIN2` | 320×256 | yes | VIS `0x28` | 58.1 | 59.8 |
| SC2 180 | `SSTV_SC2_180` | 320×256 | yes | VIS `0xB7` | 182.0 | 183.7 |
| SC2 120 | `SSTV_SC2_120` | 320×256 | yes | VIS `0x3F` | 121.7 | 123.4 |
| SC2 60 | `SSTV_SC2_60` | 320×256 | yes | VIS `0xBB` | 61.5 | 63.2 |
| PD50 | `SSTV_PD50` | 320×256 | yes | VIS `0xDD` | 49.7 | 51.4 |
| PD90 | `SSTV_PD90` | 320×256 | yes | VIS `0x63` | 90.0 | 91.7 |
| PD120 | `SSTV_PD120` | 640×496 | yes | VIS `0x5F` | 126.1 | 127.8 |
| PD160 | `SSTV_PD160` | 512×400 | yes | VIS `0xE2` | 160.9 | 162.6 |
| PD180 | `SSTV_PD180` | 640×496 | yes | VIS `0x60` | 187.1 | 188.8 |
| PD240 | `SSTV_PD240` | 640×496 | yes | VIS `0xE1` | 248.0 | 249.7 |
| PD290 | `SSTV_PD290` | 800×616 | yes | VIS `0xDE` | 288.7 | 290.4 |
| P3 | `SSTV_P3` | 640×496 | yes | VIS `0x71` | 203.1 | 204.8 |
| P5 | `SSTV_P5` | 640×496 | yes | VIS `0x72` | 304.6 | 306.3 |
| P7 | `SSTV_P7` | 640×496 | yes | VIS `0xF3` | 406.1 | 407.8 |
| MR73 | `SSTV_MR73` | 320×256 | yes | 16-bit VIS `0x4523` | 73.3 | 75.2 |
| MR90 | `SSTV_MR90` | 320×256 | yes | 16-bit VIS `0x4623` | 90.2 | 92.1 |
| MR115 | `SSTV_MR115` | 320×256 | yes | 16-bit VIS `0x4923` | 115.3 | 117.2 |
| MR140 | `SSTV_MR140` | 320×256 | yes | 16-bit VIS `0x4A23` | 140.4 | 142.3 |
| MR175 | `SSTV_MR175` | 320×256 | yes | 16-bit VIS `0x4C23` | 175.2 | 177.1 |
| MP73 | `SSTV_MP73` | 320×256 | yes | 16-bit VIS `0x2523` | 73.0 | 74.9 |
| MP115 | `SSTV_MP115` | 320×256 | yes | 16-bit VIS `0x2923` | 115.5 | 117.4 |
| MP140 | `SSTV_MP140` | 320×256 | yes | 16-bit VIS `0x2A23` | 139.5 | 141.5 |
| MP175 | `SSTV_MP175` | 320×256 | yes | 16-bit VIS `0x2C23` | 175.4 | 177.3 |
| ML180 | `SSTV_ML180` | 640×496 | yes | 16-bit VIS `0x8523` | 180.2 | 182.1 |
| ML240 | `SSTV_ML240` | 640×496 | yes | 16-bit VIS `0x8623` | 239.7 | 241.7 |
| ML280 | `SSTV_ML280` | 640×496 | yes | 16-bit VIS `0x8923` | 280.4 | 282.3 |
| ML320 | `SSTV_ML320` | 640×496 | yes | 16-bit VIS `0x8A23` | 320.1 | 322.0 |
| Robot 24 | `SSTV_R24` | 320×240 | yes | VIS `0x84` | 24.0 | 25.7 |
| B/W 8 | `SSTV_BW8` | 320×240 | B/W | VIS `0x82` | 8.0 | 9.7 |
| B/W 12 | `SSTV_BW12` | 320×240 | B/W | VIS `0x86` | 12.0 | 13.7 |
| MP73-N | `SSTV_MN73` | 320×256 | yes | FSK N-VIS `0x02` | 73.0 | 74.3 |
| MP110-N | `SSTV_MN110` | 320×256 | yes | FSK N-VIS `0x04` | 109.8 | 111.2 |
| MP140-N | `SSTV_MN140` | 320×256 | yes | FSK N-VIS `0x05` | 139.5 | 140.9 |
| MC110-N | `SSTV_MC110` | 320×256 | yes | FSK N-VIS `0x14` | 109.7 | 111.0 |
| MC140-N | `SSTV_MC140` | 320×256 | yes | FSK N-VIS `0x15` | 140.4 | 141.8 |
| MC180-N | `SSTV_MC180` | 320×256 | yes | FSK N-VIS `0x16` | 180.4 | 181.7 |

VIS bytes include the parity bit (bit 7). The 16-bit words are sent low byte
(0x23) first. `sstv_mode_info_t.vis_code` holds the 8-bit code, the second
byte of the 16-bit word, or `0x00` for the narrow modes.

## Building

Requirements: CMake 3.10+, a C++11 compiler, a C99 compiler.

```bash
cmake -S . -B build
cmake --build build -j
```

| Option | Default | Effect |
| --- | --- | --- |
| `BUILD_SHARED` | ON | Shared libraries |
| `BUILD_STATIC` | ON | Static libraries |
| `BUILD_RX` | ON | Build the decoder library and decoder tools |
| `BUILD_EXAMPLES` | ON | Command-line tools (see below) |
| `BUILD_TESTS` | OFF | Test programs and CTest registration (see [tests/README.md](../tests/README.md)) |
| `BUILD_UTILS` | OFF | Diagnostic programs in `utils/` (filter and VCO probes) |

All executables and shared libraries are written to `<repo>/bin/`, whatever
the build directory. `cmake --install build` installs the libraries and
`sstv_encoder.h` / `sstv_decoder.h`, plus a `sstv_encoder.pc` pkg-config file
(encoder only).

## Using the encoder

```c
#include <sstv_encoder.h>

uint8_t *rgb = load_rgb_image();                    /* must be exactly 320×256 */
sstv_image_t image = sstv_image_from_rgb(rgb, 320, 256);

sstv_encoder_t *enc = sstv_encoder_create(SSTV_SCOTTIE1, 48000.0);
sstv_encoder_set_image(enc, &image);                /* -1 if the size is wrong */
/* VIS/header is on by default: sstv_encoder_set_vis_enabled(enc, 0) disables it */

size_t total = sstv_encoder_get_total_samples(enc); /* exact length */
float buf[4096];
size_t n;
while ((n = sstv_encoder_generate(enc, buf, 4096)) > 0) {
    write_samples(buf, n);                          /* floats in [-1, +1] */
}
sstv_encoder_free(enc);
```

The image must match the mode's size exactly (`sstv_get_mode_dimensions()`);
RGB24 and GRAY8 are supported and `stride` is honoured. The image buffer must
stay valid until generation finishes.

## Using the decoder

```c
#include <sstv_decoder.h>

sstv_decoder_t *dec = sstv_decoder_create(48000.0);
sstv_rx_status_t st = SSTV_RX_NEED_MORE;

while (st != SSTV_RX_IMAGE_READY && read_block(pcm, &n)) {
    for (size_t i = 0; i < n; i++) samples[i] = (float)pcm[i];  /* 16-bit PCM scale */
    st = sstv_decoder_feed(dec, samples, n);
}
if (st != SSTV_RX_IMAGE_READY) st = sstv_decoder_finish(dec);   /* once, at end of input */

sstv_image_t img;
if (sstv_decoder_get_image(dec, &img) == 0) {
    save_rgb(img.pixels, img.width, img.height);   /* RGB24, owned by the decoder */
}
sstv_decoder_free(dec);
```

- Feed samples on the 16-bit PCM scale (±32768). If you have floats in ±1.0
  (for example straight from the encoder), multiply by 32767 first.
- The mode is detected from the header. For audio without a header, call
  `sstv_decoder_set_mode_hint()` before feeding; decoding then starts at the
  first sample.
- `sstv_decoder_finish()` flushes a partly received last line; call it once,
  when the input has ended.
- `sstv_decoder_get_state()` reports the mode, line progress and the
  timing-correction values.

Details: [DECODER_ARCHITECTURE_BASELINE.md](DECODER_ARCHITECTURE_BASELINE.md)
and [DECODER_STATUS.md](DECODER_STATUS.md).

## Command-line tools

Built with `BUILD_EXAMPLES=ON` into `bin/`:

| Tool | Usage | Purpose |
| --- | --- | --- |
| `list_modes` | `list_modes` | Print the mode table and VIS/tone reference. |
| `encode_wav` | `encode_wav out.wav ["mode name"] [rate]` | Encode colour bars in one mode (default Scottie 1, 48 kHz) to 16-bit WAV. Mode names are matched case-insensitively against the "Mode" column above. |
| `generate_all_modes` | `generate_all_modes [dir] [rate]` | Encode colour bars in all 43 modes plus a `REPORT.txt`. |
| `test_real_images` | `test_real_images [rate]` | Encode two test images (paths hard-coded to a local PiSSTVpp2 checkout) to WAV. |
| `decode_wav` | `decode_wav in.wav [out.ppm]` | Decode a 16-bit mono WAV to a PPM image. |
| `decode_wav_debug` | `decode_wav_debug in.wav [prefix]` | Decode and also write the signal after each front-end stage as WAV ([DEBUG_WAV_GUIDE.md](DEBUG_WAV_GUIDE.md)). |
| `evaluate_decoded_images` | `evaluate_decoded_images [decoded_dir] [reference_dir]` | Compare decoded PPMs with reference images and flag timing/slant problems. |

## Documentation

See [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md) for the full list,
including which documents are historical records.

## License and credits

LGPL v3 (see `LICENSE`), as derived from MMSSTV.

Copyright (C) 2000-2013 Makoto Mori (JE3HHT), Nobuyuki Oba (original MMSSTV).
Copyright (C) 2026 (library port).

MMSSTV: <http://hamsoft.ca/pages/mmsstv.php>
