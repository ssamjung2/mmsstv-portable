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

The reference for both is the original MMSSTV source and the
[SSTV Handbook](docs/standards/sstv-handbook.pdf). What the signal contains is
described in [the signal format reference](docs/standards/sstv-signal-format.md);
where this code is compared with MMSSTV line by line, see
[the MMSSTV source map](docs/specs/mmsstv-source-map.md).

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

## Quick start

```bash
cmake -S . -B build
cmake --build build -j

./bin/encode_wav /tmp/s1.wav "Scottie 1"   # colour bars -> audio
./bin/decode_wav /tmp/s1.wav /tmp/s1.ppm   # audio -> image
```

Requirements: CMake 3.10+, a C++11 compiler and a C99 compiler. No external
dependencies. Executables are written to `bin/`.

In your own code, encoding is create / set image / pull samples, and decoding
is create / feed audio / finish / take the image:

```c
sstv_encoder_t *enc = sstv_encoder_create(SSTV_SCOTTIE1, 48000.0);
sstv_encoder_set_image(enc, &image);          /* image must be exactly 320x256 */
while ((n = sstv_encoder_generate(enc, buf, 4096)) > 0)
    write_samples(buf, n);                    /* floats in [-1, +1] */
sstv_encoder_free(enc);
```

```c
sstv_decoder_t *dec = sstv_decoder_create(48000.0);
while (st != SSTV_RX_IMAGE_READY && read_block(samples, &n))
    st = sstv_decoder_feed(dec, samples, n);  /* 16-bit PCM scale, not [-1, +1] */
if (st != SSTV_RX_IMAGE_READY) st = sstv_decoder_finish(dec);
sstv_decoder_get_image(dec, &img);            /* RGB24, owned by the decoder */
sstv_decoder_free(dec);
```

Full details: [getting started](docs/guide/getting-started.md),
[building and installing](docs/guide/building.md),
[encoder API](docs/guide/encoder-api.md),
[decoder API](docs/guide/decoder-api.md),
[command-line tools](docs/guide/cli-tools.md).

## Documentation

[docs/README.md](docs/README.md) is the documentation index. It is organised
by kind: [standards](docs/standards/) for what SSTV is,
[specs](docs/specs/) for how this library works,
[guide](docs/guide/) for how to use it, [plans](docs/plans/) for what is still
open, and [archive](docs/archive/) for historical records.
[CHANGELOG.md](CHANGELOG.md) lists what changed and when.

## License and credits

LGPL v3 (see `LICENSE`), as derived from MMSSTV.

Copyright (C) 2000-2013 Makoto Mori (JE3HHT), Nobuyuki Oba (original MMSSTV).
Copyright (C) 2026 (library port).

MMSSTV: <http://hamsoft.ca/pages/mmsstv.php>
