# Decoder API

`#include <sstv_decoder.h>`, link `-lsstv_decoder`. Feed it audio, get an RGB
image. The decoder identifies the mode from the header the sender transmits
(VIS, MMSSTV's 16-bit VIS, or the narrow modes' FSK header) and follows the
line sync as the image arrives. How it works internally is described in
[the decoder specification](../specs/decoder.md); what it does not do yet is
listed in [decoder status](../specs/decoder-status.md).

## Minimal example

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

## Input scale

Samples are floats on the **16-bit PCM scale**, -32768 to +32767, not -1 to
+1. Feeding normalized floats gives a signal roughly 32000× too quiet; the
level AGC will compensate eventually, but the header is likely to be missed.
Multiply normalized audio by 32767 first.

The decoder resamples nothing: create it with the sample rate of the audio you
are feeding it.

## Status values

| Status | Meaning |
| --- | --- |
| `SSTV_RX_NEED_MORE` | Normal: more audio needed. |
| `SSTV_RX_IMAGE_READY` | An image is complete and can be fetched. |
| `SSTV_RX_OK` | Call succeeded (returned by the non-feeding calls). |
| `SSTV_RX_ERROR` | Bad arguments or internal failure. |

## Functions

| Function | Purpose |
| --- | --- |
| `sstv_decoder_create(sample_rate)` | Create a decoder for that audio rate. `NULL` on error. |
| `sstv_decoder_free(dec)` | Release it, closing any debug WAV files. |
| `sstv_decoder_reset(dec)` | Forget the current image, mode and sync state, ready for a new transmission. |
| `sstv_decoder_feed(dec, samples, count)` | Feed a block of samples. |
| `sstv_decoder_feed_sample(dec, sample)` | Feed one sample. |
| `sstv_decoder_finish(dec)` | Call once when the input ends: it flushes the partial last line and marks a nearly complete image ready. |
| `sstv_decoder_get_image(dec, &img)` | Fetch the decoded image. 0 on success. |
| `sstv_decoder_get_state(dec, &state)` | Read progress and diagnostics. |

**Image ownership:** the pixels belong to the decoder. Do not free them. They
stay valid until the next `sstv_decoder_reset()`, `sstv_decoder_free()`, or a
new VIS starting another image. Copy the buffer if you need it longer. The
format is always RGB24, even for the black-and-white modes.

**Reuse:** to decode a second transmission with the same decoder, call
`sstv_decoder_reset()` between them. Without it the decoder still believes the
previous mode is in progress.

## Tuning

| Function | Default | Purpose |
| --- | --- | --- |
| `sstv_decoder_set_mode_hint(dec, mode)` | none | Decode as this mode instead of waiting for a header. Needed for audio that starts mid-transmission or has no header. |
| `sstv_decoder_set_vis_enabled(dec, 0)` | enabled | Stop looking for a header. A mode hint is then required. |
| `sstv_decoder_set_agc_mode(dec, mode)` | `SSTV_AGC_AUTO` | `SSTV_AGC_OFF` bypasses the level AGC, for already normalized input. Every other value enables it; `LOW`/`MED`/`HIGH`/`SEMI` exist for API compatibility and behave like `AUTO`. |
| `sstv_decoder_get_agc_mode(dec)` | | Read it back. |
| `sstv_decoder_enable_timing_correction(dec, 0)` | enabled | Turn off the slow drift correction that keeps images straight when the sender's and receiver's clocks differ. Per-line sync re-lock stays on. |
| `sstv_decoder_set_timing_correction_gain(dec, g)` | 0.04 | How fast that correction converges, from 0 to 1. Lower is slower but steadier on noisy sync. |
| `sstv_decoder_set_vis_tones(dec, mark, space)` | 1080 / 1320 Hz | Move the VIS bit detectors. The defaults are MMSSTV's and work with the 1100/1300 Hz tones that senders actually transmit; see [why](../specs/vis-tone-frequencies.md). |
| `sstv_decoder_set_debug_level(dec, level)` | 0 | Higher values print progress to stdout. |

## Progress and diagnostics

```c
typedef struct {
    sstv_mode_t current_mode;    /* detected or hinted mode */
    int vis_enabled;
    int sync_detected;
    int image_ready;
    int current_line;            /* lines decoded so far */
    int total_lines;
    double timing_error;         /* mean sync error over the last 8 lines, samples */
    double timing_correction;    /* learned drift, samples per line */
} sstv_decoder_state_t;
```

`current_line` and `total_lines` drive a progress bar. `timing_error` near
zero means the decoder is tracking the sync well; a `timing_correction` that
settles on a non-zero value is the sender's and receiver's clock mismatch
being absorbed.

To inspect the signal itself, `sstv_decoder_enable_debug_wav()` writes the
audio at four points in the front end to WAV files; see
[debug-wav.md](debug-wav.md).

## Notes and limits

- Instances share no state, so several decoders can run in parallel, but a
  single instance must not be used from two threads at once.
- There is no built-in "decode this WAV file" helper yet; the `decode_wav`
  tool shows the loop, see [cli-tools.md](cli-tools.md).
- Decoding is much faster than real time: 111 seconds of Scottie 1 audio
  decodes in about 0.8 s on an Apple laptop. One decoder holds a few megabytes
  at most, mostly the image buffer.
- Known gaps, including AVT header decoding and automatic frequency
  correction, are listed in [decoder status](../specs/decoder-status.md).
