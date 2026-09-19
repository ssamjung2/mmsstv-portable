# Getting started

mmsstv-portable is a C/C++ port of the SSTV encoder and decoder from MMSSTV.
It builds two libraries, `libsstv_encoder` and `libsstv_decoder`, plus a few
command-line tools, and covers all 43 MMSSTV modes, including the MR/MP/ML
modes and the narrow modes.

## Build it and try it

```bash
cmake -S . -B build
cmake --build build -j

./bin/encode_wav /tmp/s1.wav "Scottie 1"   # colour bars -> audio
./bin/decode_wav /tmp/s1.wav /tmp/s1.ppm   # audio -> image
```

Executables land in `bin/` in the repository root. More build options are in
[building.md](building.md), and the other tools in [cli-tools.md](cli-tools.md).

## Use it from your own code

Encoding is: create, set an image of exactly the mode's size, pull samples
until the generator returns 0.

```c
#include <sstv_encoder.h>

sstv_image_t image = sstv_image_from_rgb(rgb, 320, 256);
sstv_encoder_t *enc = sstv_encoder_create(SSTV_SCOTTIE1, 48000.0);
sstv_encoder_set_image(enc, &image);

float buf[4096];
size_t n;
while ((n = sstv_encoder_generate(enc, buf, 4096)) > 0)
    write_samples(buf, n);            /* floats in [-1, +1] */

sstv_encoder_free(enc);
```

Decoding is: create, feed audio on the 16-bit PCM scale, call `finish()` when
the input ends, then take the image.

```c
#include <sstv_decoder.h>

sstv_decoder_t *dec = sstv_decoder_create(48000.0);
sstv_rx_status_t st = SSTV_RX_NEED_MORE;

while (st != SSTV_RX_IMAGE_READY && read_block(samples, &n))
    st = sstv_decoder_feed(dec, samples, n);   /* samples are -32768..+32767 */

if (st != SSTV_RX_IMAGE_READY) st = sstv_decoder_finish(dec);

sstv_image_t img;
if (sstv_decoder_get_image(dec, &img) == 0)
    save_rgb(img.pixels, img.width, img.height);

sstv_decoder_free(dec);
```

Full reference: [encoder-api.md](encoder-api.md) and
[decoder-api.md](decoder-api.md).

## Three things that catch people out

1. **Decoder input is on the 16-bit PCM scale** (-32768 to +32767), not
   -1 to +1. Normalized floats are ~32000× too quiet and the header will
   usually be missed.
2. **The image must be exactly the mode's size.** `sstv_encoder_set_image()`
   returns -1 otherwise. Sizes are in the mode table in the
   [project README](../../README.md).
3. **Call `sstv_decoder_finish()` once**, when the audio ends, not after every
   block. It flushes the last scan line.

## Where to go next

| You want to | Read |
| --- | --- |
| Know what the audio actually contains | [SSTV signal format](../standards/sstv-signal-format.md) |
| Understand or change the encoder | [encoder specification](../specs/encoder.md) |
| Understand or change the decoder | [decoder specification](../specs/decoder.md) |
| Know what the decoder cannot do yet | [decoder status](../specs/decoder-status.md) |
| Run or extend the tests | [the test suite](../../tests/README.md) |
| Check something against MMSSTV | [MMSSTV source map](../specs/mmsstv-source-map.md) |
| See what changed and when | [CHANGELOG.md](../../CHANGELOG.md) |

## Ground rules for changes

- The MMSSTV source (`../mmsstv/`, Shift-JIS; convert with
  `iconv -c -f CP932 -t UTF-8`) and the SSTV Handbook
  ([docs/standards/](../standards/)) are the authorities. Where this library
  deliberately differs, the specification documents say so.
- Before changing timing, filters or VIS logic, run the full test suite.
  `test_roundtrip` encodes and decodes all 43 modes and catches most
  regressions.
- Documents under [docs/archive/](../archive/) are historical records with
  known errors, listed in a banner at the top of each. Don't use them as a
  specification.
