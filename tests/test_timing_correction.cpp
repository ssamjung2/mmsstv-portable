/*
 * test_timing_correction - regression test for timing/slant correction on
 * the modes that currently show the largest decode misalignment.
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "../external/stb_image.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "sstv_decoder.h"

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

static uint32_t read_u32le(FILE *fp) {
    uint8_t b[4];
    if (fread(b, 1, 4, fp) != 4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint16_t read_u16le(FILE *fp) {
    uint8_t b[2];
    if (fread(b, 1, 2, fp) != 2) return 0;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static int read_wav(const char *path, wav_info_t *w) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    uint8_t riff[12];
    if (fread(riff, 1, 12, fp) != 12) {
        fclose(fp);
        return -1;
    }
    if (memcmp(riff, "RIFF", 4) || memcmp(riff + 8, "WAVE", 4)) {
        fclose(fp);
        return -1;
    }
    int fmt_ok = 0, data_ok = 0;
    while (!feof(fp)) {
        char id[4];
        uint32_t sz;
        if (fread(id, 1, 4, fp) != 4) break;
        sz = read_u32le(fp);
        if (!memcmp(id, "fmt ", 4)) {
            w->audio_format = read_u16le(fp);
            w->num_channels = read_u16le(fp);
            w->sample_rate = read_u32le(fp);
            read_u32le(fp);
            read_u16le(fp);
            w->bits_per_sample = read_u16le(fp);
            if (sz > 16) fseek(fp, (long)(sz - 16), SEEK_CUR);
            fmt_ok = 1;
        } else if (!memcmp(id, "data", 4)) {
            w->data_offset = (uint32_t)ftell(fp);
            w->data_size = sz;
            data_ok = 1;
            break;
        } else {
            fseek(fp, (long)sz, SEEK_CUR);
        }
    }
    fclose(fp);
    return (fmt_ok && data_ok) ? 0 : -1;
}

static double compute_mae(const uint8_t *img, int img_w, int img_h,
                          const uint8_t *ref, int ref_w, int ref_h, int ref_ch) {
    double sum = 0.0;
    long total = 0;
    for (int y = 0; y < img_h; ++y) {
        int ry = (ref_h > 1) ? (y * ref_h / img_h) : 0;
        for (int x = 0; x < img_w; ++x) {
            int rx = (ref_w > 1) ? (x * ref_w / img_w) : 0;
            for (int c = 0; c < 3; ++c) {
                int iv = img[(y * img_w + x) * 3 + c];
                int rv = (ref_ch == 1) ? ref[ry * ref_w + rx] : ref[(ry * ref_w + rx) * ref_ch + c];
                sum += std::fabs((double)(iv - rv));
                ++total;
            }
        }
    }
    return total > 0 ? sum / (double)total : 255.0;
}

int main(int argc, char **argv) {
    const char *audio_dir = (argc >= 2) ? argv[1] : "tests/audio";
    const char *reference_dir = (argc >= 3) ? argv[2] : audio_dir;

    struct Case {
        const char *wav;
        const char *ref;
        int expected_mode;
        double max_mae;
    } cases[] = {
        { "alt5_test_panel_p7.wav", "alt5_test_panel_p7.jpg", 20, 90.0 },
        { "alt5_test_panel_mr90.wav", "alt5_test_panel_mr90.jpg", 22, 80.0 },
        { "alt5_test_panel_p3.wav", "alt5_test_panel_p3.jpg", 18, 90.0 },
        { "alt5_test_panel_mr73.wav", "alt5_test_panel_mr73.jpg", 21, 90.0 },
        { "alt5_test_panel_p5.wav", "alt5_test_panel_p5.jpg", 19, 90.0 },
    };

    int passed = 0;
    int total = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const Case &c = cases[i];
        std::string wav_path = std::string(audio_dir) + "/" + c.wav;
        std::string ref_path = std::string(reference_dir) + "/" + c.ref;

        wav_info_t wi;
        std::memset(&wi, 0, sizeof(wi));
        if (read_wav(wav_path.c_str(), &wi) != 0) {
            std::fprintf(stderr, "SKIP: could not read %s\n", wav_path.c_str());
            continue;
        }
        ++total;

        FILE *fp = fopen(wav_path.c_str(), "rb");
        if (!fp) {
            std::fprintf(stderr, "FAIL: could not open %s\n", wav_path.c_str());
            continue;
        }
        if (fseek(fp, (long)wi.data_offset, SEEK_SET) != 0) {
            fclose(fp);
            std::fprintf(stderr, "FAIL: could not seek %s\n", wav_path.c_str());
            continue;
        }

        sstv_decoder_t *dec = sstv_decoder_create((double)wi.sample_rate);
        if (!dec) {
            fclose(fp);
            std::fprintf(stderr, "FAIL: decoder create failed for %s\n", wav_path.c_str());
            continue;
        }

        const size_t chunk = 2048;
        std::vector<int16_t> pcm(chunk);
        std::vector<float> fbuf(chunk);
        size_t remaining = wi.data_size / 2;
        while (remaining > 0) {
            size_t n = remaining > chunk ? chunk : remaining;
            n = fread(pcm.data(), sizeof(int16_t), n, fp);
            if (!n) break;
            for (size_t j = 0; j < n; ++j) fbuf[j] = static_cast<float>(pcm[j]);
            sstv_decoder_feed(dec, fbuf.data(), n);
            remaining -= n;
        }
        fclose(fp);

        sstv_decoder_state_t state;
        std::memset(&state, 0, sizeof(state));
        sstv_decoder_get_state(dec, &state);
        int detected = (state.current_mode != SSTV_MODE_COUNT) ? (int)state.current_mode : -1;
        std::printf("case=%s detected=%d expected=%d\n", c.wav, detected, c.expected_mode);

        sstv_image_t img;
        std::memset(&img, 0, sizeof(img));
        double mae = 255.0;
        int got_img = (sstv_decoder_get_image(dec, &img) == 0 && img.pixels != NULL);
        if (got_img) {
            int rw = 0, rh = 0, rch = 0;
            uint8_t *ref = stbi_load(ref_path.c_str(), &rw, &rh, &rch, 0);
            if (ref) {
                mae = compute_mae(img.pixels, (int)img.width, (int)img.height, ref, rw, rh, rch);
                std::printf("  mae=%.2f threshold=%.2f\n", mae, c.max_mae);
                stbi_image_free(ref);
            }
        }

        bool ok = (detected == c.expected_mode) && got_img && (mae <= c.max_mae);
        std::printf("  result=%s\n", ok ? "PASS" : "FAIL");
        if (ok) ++passed;

        sstv_decoder_free(dec);
    }

    std::printf("timing regression cases: %d passed\n", passed);
    return (passed > 0 && passed == total) ? 0 : 1;
}
