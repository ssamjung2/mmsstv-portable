/*
 * Encoder -> decoder round-trip regression test.
 *
 *   1. Tones: the encoder emits exact MMSSTV frequencies (VIS 1100 Hz = 1,
 *      1300 Hz = 0; line sync 1200 Hz; black 1500 Hz).
 *   2. Every mode: sstv_encoder_get_total_samples() matches the generated
 *      length, the decoder identifies the mode from the header alone (VIS,
 *      16-bit VIS, or narrow-mode N-VIS; no mode hint), decodes every line,
 *      and the image matches the encoded colour bars.
 *   3. Clock mismatch: a transmission sampled 500 ppm fast still decodes
 *      straight (sync re-lock + timing correction).
 *   4. Decoder reuse: after sstv_decoder_reset() the same decoder detects and
 *      decodes a second transmission in a different mode.
 *
 * Exits non-zero on any failure.
 */

#include "sstv_encoder.h"
#include "sstv_decoder.h"

#include <cmath>
#include <cstdio>
#include <vector>

static const double kMaeLimit = 25.0;

static std::vector<unsigned char> color_bars(int w, int h) {
    static const unsigned char bars[8][3] = {
        {255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0},
        {255, 0, 255},   {255, 0, 0},   {0, 0, 255},   {0, 0, 0}};
    std::vector<unsigned char> img((size_t)w * h * 3);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            for (int k = 0; k < 3; k++)
                img[((size_t)y * w + x) * 3 + k] = bars[x * 8 / w][k];
    return img;
}

/* Encode `mode` at `tx_rate`; samples are scaled to 16-bit PCM range */
static std::vector<float> encode(sstv_mode_t mode, const std::vector<unsigned char> &img,
                                 double tx_rate, size_t *predicted) {
    const sstv_mode_info_t *mi = sstv_get_mode_info(mode);
    sstv_image_t im = sstv_image_from_rgb((uint8_t *)img.data(), mi->width, mi->height);
    sstv_encoder_t *enc = sstv_encoder_create(mode, tx_rate);
    sstv_encoder_set_image(enc, &im);
    if (predicted) *predicted = sstv_encoder_get_total_samples(enc);
    std::vector<float> out;
    float buf[4096];
    size_t n;
    while ((n = sstv_encoder_generate(enc, buf, 4096)) > 0)
        for (size_t i = 0; i < n; i++) out.push_back(buf[i] * 32767.0f);
    sstv_encoder_free(enc);
    return out;
}

/* Decode; returns MAE against `img` (B/W modes: against studio-range luma), -1 if no image */
static double decode_mae(sstv_mode_t mode, const std::vector<float> &audio, double rx_rate,
                         const std::vector<unsigned char> &img, sstv_decoder_state_t *state) {
    const sstv_mode_info_t *mi = sstv_get_mode_info(mode);
    sstv_decoder_t *dec = sstv_decoder_create(rx_rate);
    sstv_rx_status_t st = SSTV_RX_NEED_MORE;
    for (size_t p = 0; p < audio.size() && st != SSTV_RX_IMAGE_READY; p += 2048) {
        size_t n = audio.size() - p < 2048 ? audio.size() - p : 2048;
        st = sstv_decoder_feed(dec, &audio[p], n);
    }
    if (st != SSTV_RX_IMAGE_READY) sstv_decoder_finish(dec);
    sstv_decoder_get_state(dec, state);

    double mae = -1.0;
    sstv_image_t out;
    if (sstv_decoder_get_image(dec, &out) == 0 && out.width == mi->width &&
        out.height == mi->height) {
        bool bw = !mi->is_color;
        double err = 0.0;
        for (size_t i = 0; i < img.size(); i++) {
            double ref = img[i];
            if (bw) {
                size_t p = i - i % 3;
                ref = 16.0 + 0.256773 * img[p] + 0.504097 * img[p + 1] + 0.0979 * img[p + 2];
            }
            err += fabs((double)out.pixels[i] - ref);
        }
        mae = err / img.size();
    }
    sstv_decoder_free(dec);
    return mae;
}

/* Frequency by zero-crossing count over [start, start + len) */
static double tone_hz(const std::vector<float> &s, size_t start, size_t len, double fs) {
    int crossings = 0;
    double first = -1.0, last = -1.0;
    for (size_t i = start + 1; i < start + len && i < s.size(); i++) {
        if (s[i - 1] < 0.0f && s[i] >= 0.0f) {
            double t = (i - 1) + (-s[i - 1]) / (s[i] - s[i - 1]);
            if (first < 0.0) first = t;
            last = t;
            crossings++;
        }
    }
    return crossings > 1 ? (crossings - 1) * fs / (last - first) : 0.0;
}

static int check_tones(void) {
    const double fs = 48000.0;
    std::vector<unsigned char> img(320 * 256 * 3, 0);   /* black */
    std::vector<float> a = encode(SSTV_MARTIN1, img, fs, NULL);
    const size_t vis = (size_t)(0.800 * fs);            /* after the 800 ms preamble */
    const size_t bit0 = vis + (size_t)(0.640 * fs);     /* Martin 1 VIS = 0xAC */
    const size_t line0 = vis + (size_t)(0.910 * fs);
    struct { const char *what; double got, want; } t[] = {
        {"VIS bit 0 (0)",   tone_hz(a, bit0 + 200, 1000, fs), 1300.0},
        {"VIS bit 2 (1)",   tone_hz(a, bit0 + 2 * 1440 + 200, 1000, fs), 1100.0},
        {"line sync",       tone_hz(a, line0 + 20, 200, fs), 1200.0},
        {"black",           tone_hz(a, line0 + (size_t)(0.030 * fs), 2000, fs), 1500.0},
    };
    int fails = 0;
    for (const auto &c : t) {
        bool ok = fabs(c.got - c.want) < 3.0;
        printf("  tone %-14s %7.1f Hz (want %.0f)  %s\n", c.what, c.got, c.want, ok ? "OK" : "FAIL");
        fails += ok ? 0 : 1;
    }
    return fails;
}

int main(void) {
    int fails = 0;
    printf("Encoder tones (Martin 1, 48 kHz):\n");
    fails += check_tones();

    const double fs = 11025.0;
    printf("\nRound trip, all modes at %.0f Hz (MAE limit %.0f):\n", fs, kMaeLimit);
    for (int m = 0; m < SSTV_MODE_COUNT; m++) {
        sstv_mode_t mode = (sstv_mode_t)m;
        const sstv_mode_info_t *mi = sstv_get_mode_info(mode);
        std::vector<unsigned char> img = color_bars(mi->width, mi->height);
        size_t predicted = 0;
        std::vector<float> a = encode(mode, img, fs, &predicted);
        sstv_decoder_state_t st;
        double mae = decode_mae(mode, a, fs, img, &st);
        long len_err = (long)a.size() - (long)predicted;
        bool ok = st.current_mode == mode && st.current_line >= st.total_lines &&
                  mae >= 0.0 && mae < kMaeLimit && labs(len_err) <= 2;
        printf("  %-10s detected=%2d lines=%3d/%3d len_err=%+ld MAE=%6.1f  %s\n", mi->name,
               (int)st.current_mode, st.current_line, st.total_lines, len_err, mae,
               ok ? "OK" : "FAIL");
        fails += ok ? 0 : 1;
    }

    printf("\nClock mismatch (TX +500 ppm, RX %.0f Hz):\n", fs);
    const sstv_mode_t skew_modes[] = {SSTV_MARTIN1, SSTV_SCOTTIE1, SSTV_PD120};
    for (sstv_mode_t mode : skew_modes) {
        const sstv_mode_info_t *mi = sstv_get_mode_info(mode);
        std::vector<unsigned char> img = color_bars(mi->width, mi->height);
        std::vector<float> a = encode(mode, img, fs * (1.0 + 500e-6), NULL);
        sstv_decoder_state_t st;
        double mae = decode_mae(mode, a, fs, img, &st);
        bool ok = st.current_mode == mode && mae >= 0.0 && mae < kMaeLimit;
        printf("  %-10s MAE=%6.1f  %s\n", mi->name, mae, ok ? "OK" : "FAIL");
        fails += ok ? 0 : 1;
    }

    printf("\nDecoder reuse: Martin 2, sstv_decoder_reset(), then Robot 36 (%.0f Hz):\n", fs);
    {
        const sstv_mode_t seq[2] = {SSTV_MARTIN2, SSTV_R36};
        sstv_decoder_t *dec = sstv_decoder_create(fs);
        for (int r = 0; r < 2; r++) {
            const sstv_mode_info_t *mi = sstv_get_mode_info(seq[r]);
            std::vector<unsigned char> img = color_bars(mi->width, mi->height);
            std::vector<float> a = encode(seq[r], img, fs, NULL);
            sstv_rx_status_t st = SSTV_RX_NEED_MORE;
            for (size_t p = 0; p < a.size() && st != SSTV_RX_IMAGE_READY; p += 2048) {
                size_t n = a.size() - p < 2048 ? a.size() - p : 2048;
                st = sstv_decoder_feed(dec, &a[p], n);
            }
            if (st != SSTV_RX_IMAGE_READY) sstv_decoder_finish(dec);
            sstv_decoder_state_t s;
            sstv_decoder_get_state(dec, &s);
            sstv_image_t out;
            double mae = -1.0;
            if (sstv_decoder_get_image(dec, &out) == 0 && out.width == mi->width &&
                out.height == mi->height) {
                double err = 0.0;
                for (size_t i = 0; i < img.size(); i++) err += fabs((double)out.pixels[i] - img[i]);
                mae = err / img.size();
            }
            bool ok = s.current_mode == seq[r] && mae >= 0.0 && mae < kMaeLimit;
            printf("  %-10s detected=%2d MAE=%6.1f  %s\n", mi->name, (int)s.current_mode, mae,
                   ok ? "OK" : "FAIL");
            fails += ok ? 0 : 1;
            sstv_decoder_reset(dec);
        }
        sstv_decoder_free(dec);
    }

    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
