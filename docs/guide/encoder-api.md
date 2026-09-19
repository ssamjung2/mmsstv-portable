# Encoder API

`#include <sstv_encoder.h>`, link `-lsstv_encoder`. The encoder turns one
image into a stream of audio samples: preamble, header and scan lines, exactly
as MMSSTV transmits them. For what those signals contain, see
[the SSTV signal format](../standards/sstv-signal-format.md) and
[the encoder specification](../specs/encoder.md).

## Minimal example

```c
#include <sstv_encoder.h>

uint8_t *rgb = load_rgb_image();                    /* must be exactly 320×256 */
sstv_image_t image = sstv_image_from_rgb(rgb, 320, 256);

sstv_encoder_t *enc = sstv_encoder_create(SSTV_SCOTTIE1, 48000.0);
sstv_encoder_set_image(enc, &image);                /* -1 if the size is wrong */

size_t total = sstv_encoder_get_total_samples(enc); /* exact length */
float buf[4096];
size_t n;
while ((n = sstv_encoder_generate(enc, buf, 4096)) > 0) {
    write_samples(buf, n);                          /* floats in [-1, +1] */
}
sstv_encoder_free(enc);
```

## Images

```c
typedef struct {
    uint8_t *pixels;
    uint32_t width, height;
    uint32_t stride;              /* bytes per row */
    sstv_pixel_format_t format;   /* SSTV_RGB24 or SSTV_GRAY8 */
} sstv_image_t;
```

- `sstv_image_from_rgb(rgb, w, h)` and `sstv_image_from_gray(gray, w, h)` fill
  this in for you. Neither copies the data.
- **The buffer must stay valid until encoding finishes.** The encoder reads
  from it as it generates.
- `stride` is honoured, so you can encode a sub-image of a larger buffer.
- The image size must match the mode exactly; `sstv_encoder_set_image()`
  returns -1 otherwise. Sizes are listed in the mode table in the
  [project README](../../README.md) and available from
  `sstv_get_mode_info()`.
- `SSTV_GRAY8` images are expanded to R = G = B, so they can be sent in any
  mode, not only the B/W ones.

## Functions

| Function | Purpose |
| --- | --- |
| `sstv_encoder_create(mode, sample_rate)` | Create an encoder. Any sample rate is accepted; 48000 and 44100 are the usual ones. Returns `NULL` on error. |
| `sstv_encoder_free(enc)` | Release it. |
| `sstv_encoder_set_image(enc, &image)` | Set the source image. Returns 0, or -1 if the size does not match the mode. |
| `sstv_encoder_set_vis_enabled(enc, 0)` | Send no header at all (default: enabled). For Scottie this also drops the extra 9 ms sync pulse. The preamble is always sent. |
| `sstv_encoder_generate(enc, buf, max)` | Produce up to `max` samples, returning how many were written. 0 means finished. |
| `sstv_encoder_is_complete(enc)` | 1 once the whole transmission has been generated. |
| `sstv_encoder_get_progress(enc)` | 0.0 to 1.0. |
| `sstv_encoder_get_total_samples(enc)` | Exact number of samples the transmission will produce (±1 sample), so you can size a buffer or a WAV header up front. |
| `sstv_encoder_reset(enc)` | Start again from the beginning, with the same or a different image. |
| `sstv_encoder_version()` | Version string, currently `"1.0.0"`. |

Samples are floats in [-1, +1]. To write 16-bit PCM, multiply by 32767.

## Mode information

```c
typedef struct {
    sstv_mode_t mode;
    const char *name;        /* "Scottie 1" */
    uint32_t width, height;
    uint8_t vis_code;        /* 8-bit VIS byte, or 0x00 for the narrow modes */
    double duration_sec;     /* image time, excluding preamble and header */
    int is_color;
} sstv_mode_info_t;
```

| Function | Purpose |
| --- | --- |
| `sstv_get_mode_info(mode)` | One mode, or `NULL` if the enum is out of range. |
| `sstv_get_all_modes(&count)` | The whole table (43 entries). |
| `sstv_find_mode_by_name(name)` | Case-insensitive lookup, e.g. `"scottie 1"` or `"Martin2"`. Returns the enum value, or -1. |

`vis_code` holds the 8-bit VIS byte including its parity bit. For the MR, MP
and ML modes it is the second byte of MMSSTV's 16-bit VIS word (the one sent
after `0x23`). The six narrow modes have no VIS and report `0x00`; they are
identified by an FSK header instead.

`duration_sec` covers the image only. Add 0.8 s of preamble (0.4 s for narrow
modes) and the header length: 0.91 s for a standard VIS, 1.15 s for the 16-bit
VIS, 0.95 s for the narrow FSK header, or about 8.04 s for AVT 90.

## Notes and limits

- One encoder handles one transmission at a time. Instances share no state, so
  several can run in parallel, but a single instance must not be used from two
  threads at once.
- There is no API to disable the preamble.
- MMSSTV's CW identifier, FSK callsign identifier and appended audio clips are
  not implemented, nor is its transmit band-pass filter or per-colour output
  gain.
