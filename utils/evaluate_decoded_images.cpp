/*
 * evaluate_decoded_images - Compare decoded SSTV images against reference images
 * and report likely timing/slant issues.
 *
 * Usage:
 *   evaluate_decoded_images [decoded_dir] [reference_dir]
 *
 * Defaults:
 *   decoded_dir   = tests/decoded_images
 *   reference_dir = tests/audio
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "../external/stb_image.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

struct ImageData {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> pixels;
};

struct EvaluationResult {
    std::string stem;
    std::string path;
    double mae = 0.0;
    double mean_abs_shift = 0.0;
    double max_abs_shift = 0.0;
    double slant_slope = 0.0;
    double slant_rms = 0.0;
};

static bool read_token(FILE *fp, char *buf, size_t cap) {
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(fp)) != EOF && c != '\n') {}
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        ungetc(c, fp);
        break;
    }
    if (feof(fp)) return false;
    size_t i = 0;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '#' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ungetc(c, fp);
            break;
        }
        if (i + 1 < cap) {
            buf[i++] = (char)c;
        }
    }
    buf[i] = '\0';
    return i > 0;
}

static bool load_ppm_pgm(const std::string &path, ImageData *out) {
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return false;

    char magic[8] = {0};
    if (!read_token(fp, magic, sizeof(magic))) {
        fclose(fp);
        return false;
    }

    char width_s[64] = {0};
    char height_s[64] = {0};
    char maxval_s[64] = {0};
    if (!read_token(fp, width_s, sizeof(width_s)) ||
        !read_token(fp, height_s, sizeof(height_s)) ||
        !read_token(fp, maxval_s, sizeof(maxval_s))) {
        fclose(fp);
        return false;
    }

    int width = atoi(width_s);
    int height = atoi(height_s);
    int maxval = atoi(maxval_s);
    if (width <= 0 || height <= 0 || maxval <= 0) {
        fclose(fp);
        return false;
    }

    out->width = width;
    out->height = height;
    out->channels = (strcmp(magic, "P5") == 0) ? 1 : 3;
    out->pixels.resize((size_t)width * height * out->channels);

    if (maxval > 255) {
        fclose(fp);
        return false;
    }

    size_t bytes = out->pixels.size();
    size_t read_bytes = fread(out->pixels.data(), 1, bytes, fp);
    fclose(fp);
    return read_bytes == bytes;
}

static bool load_reference_image(const std::string &path, ImageData *out) {
    int w = 0;
    int h = 0;
    int ch = 0;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &ch, 3);
    if (!data) return false;
    out->width = w;
    out->height = h;
    out->channels = 3;
    out->pixels.assign(data, data + (size_t)w * h * 3);
    stbi_image_free(data);
    return true;
}

static ImageData make_rgb(const ImageData &src) {
    ImageData rgb;
    rgb.width = src.width;
    rgb.height = src.height;
    rgb.channels = 3;
    rgb.pixels.resize((size_t)src.width * src.height * 3);

    if (src.channels == 3) {
        std::copy(src.pixels.begin(), src.pixels.end(), rgb.pixels.begin());
        return rgb;
    }

    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            uint8_t v = src.pixels[y * src.width + x];
            size_t i = ((size_t)y * src.width + x) * 3;
            rgb.pixels[i + 0] = v;
            rgb.pixels[i + 1] = v;
            rgb.pixels[i + 2] = v;
        }
    }
    return rgb;
}

static double compute_mae(const ImageData &decoded, const ImageData &ref) {
    int w = decoded.width < ref.width ? decoded.width : ref.width;
    int h = decoded.height < ref.height ? decoded.height : ref.height;
    if (w <= 0 || h <= 0) return 255.0;

    double sum = 0.0;
    long total = 0;
    for (int y = 0; y < h; ++y) {
        int ry = (ref.height > 1) ? (y * ref.height / h) : 0;
        for (int x = 0; x < w; ++x) {
            int rx = (ref.width > 1) ? (x * ref.width / w) : 0;
            size_t di = ((size_t)y * decoded.width + x) * 3;
            size_t ri = ((size_t)ry * ref.width + rx) * 3;
            for (int c = 0; c < 3; ++c) {
                sum += std::fabs((double)decoded.pixels[di + c] - ref.pixels[ri + c]);
                ++total;
            }
        }
    }
    return total > 0 ? sum / (double)total : 255.0;
}

static std::vector<uint8_t> make_grayscale(const ImageData &img) {
    std::vector<uint8_t> gray(img.width * img.height);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            size_t i = ((size_t)y * img.width + x) * 3;
            int r = img.pixels[i + 0];
            int g = img.pixels[i + 1];
            int b = img.pixels[i + 2];
            gray[(size_t)y * img.width + x] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
        }
    }
    return gray;
}

static EvaluationResult evaluate_pair(const std::string &decoded_path,
                                      const std::string &reference_path) {
    EvaluationResult result;
    result.path = decoded_path;

    ImageData decoded;
    ImageData reference;
    if (!load_ppm_pgm(decoded_path, &decoded) || !load_reference_image(reference_path, &reference)) {
        return result;
    }

    ImageData decoded_rgb = make_rgb(decoded);
    ImageData ref_rgb = make_rgb(reference);
    result.mae = compute_mae(decoded_rgb, ref_rgb);

    std::vector<uint8_t> dec_gray = make_grayscale(decoded_rgb);
    std::vector<uint8_t> ref_gray = make_grayscale(ref_rgb);

    const int max_shift = 16;
    std::vector<int> offsets(decoded.height, 0);
    double sum_abs = 0.0;
    int max_abs = 0;
    double sum_xy = 0.0;
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xx = 0.0;

    for (int y = 0; y < decoded.height; ++y) {
        int best_offset = 0;
        double best_cost = 1e18;
        for (int shift = -max_shift; shift <= max_shift; ++shift) {
            double cost = 0.0;
            for (int x = 0; x < decoded.width; ++x) {
                int sx = x + shift;
                if (sx < 0 || sx >= decoded.width) {
                    cost += 255.0;
                } else {
                    int a = dec_gray[(size_t)y * decoded.width + x];
                    int b = ref_gray[(size_t)y * decoded.width + sx];
                    cost += std::fabs((double)a - b);
                }
            }
            if (cost < best_cost) {
                best_cost = cost;
                best_offset = shift;
            }
        }
        offsets[y] = best_offset;
        int abs_offset = std::abs(best_offset);
        sum_abs += abs_offset;
        if (abs_offset > max_abs) max_abs = abs_offset;
        sum_x += y;
        sum_y += best_offset;
        sum_xy += (double)y * best_offset;
        sum_xx += (double)y * y;
    }

    double n = (double)decoded.height;
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x + 1e-9);
    result.mean_abs_shift = sum_abs / n;
    result.max_abs_shift = (double)max_abs;
    result.slant_slope = slope;

    double mean = sum_y / n;
    double var = 0.0;
    for (int y = 0; y < decoded.height; ++y) {
        double d = (double)offsets[y] - mean;
        var += d * d;
    }
    result.slant_rms = std::sqrt(var / n);

    return result;
}

static std::string normalize_name(const std::string &name) {
    std::string out;
    for (char c : name) {
        if (std::isalnum((unsigned char)c)) {
            out.push_back(static_cast<char>(std::tolower((unsigned char)c)));
        } else {
            out.push_back('_');
        }
    }
    return out;
}

static std::string stem_from_path(const std::string &path) {
    std::string key = path;
    size_t slash = key.find_last_of("/\\");
    if (slash != std::string::npos) key = key.substr(slash + 1);
    size_t dot = key.find_last_of('.');
    if (dot != std::string::npos) key = key.substr(0, dot);
    return key;
}

static std::string find_reference_image(const std::string &decoded_name, const std::string &reference_dir) {
    std::string stem = stem_from_path(decoded_name);
    std::string norm = normalize_name(stem);

    std::vector<std::string> candidates;
    candidates.push_back(reference_dir + "/" + stem + ".jpg");
    candidates.push_back(reference_dir + "/" + stem + ".jpeg");
    candidates.push_back(reference_dir + "/" + stem + ".png");
    candidates.push_back(reference_dir + "/alt5_test_panel_" + stem + ".jpg");
    candidates.push_back(reference_dir + "/alt5_test_panel_" + stem + ".jpeg");
    candidates.push_back(reference_dir + "/alt5_test_panel_" + stem + ".png");
    candidates.push_back(reference_dir + "/" + stem + ".ppm");

    for (const std::string &candidate : candidates) {
        std::ifstream probe(candidate.c_str());
        if (probe.good()) return candidate;
    }

    std::vector<std::string> files;
    std::string command = "find " + reference_dir + " -type f 2>/dev/null";
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buf[4096];
        while (fgets(buf, sizeof(buf), pipe)) {
            std::string path = buf;
            while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
            if (!path.empty()) files.push_back(path);
        }
        pclose(pipe);
    }

    for (const std::string &file : files) {
        std::string file_name = stem_from_path(file);
        if (normalize_name(file_name) == norm || normalize_name(file_name).find(norm) != std::string::npos || norm.find(normalize_name(file_name)) != std::string::npos) {
            return file;
        }
    }
    return "";
}

int main(int argc, char **argv) {
    std::string decoded_dir = "tests/decoded_images";
    std::string reference_dir = "tests/audio";

    if (argc >= 2) decoded_dir = argv[1];
    if (argc >= 3) reference_dir = argv[2];

    std::vector<std::string> files;
    const char *exts[] = {".ppm", ".pgm", ".png", ".jpg", ".jpeg"};

    std::string command = "find " + decoded_dir + " -type f 2>/dev/null";
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        fprintf(stderr, "ERROR: could not scan decoded directory: %s\n", decoded_dir.c_str());
        return 1;
    }

    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::string path = buf;
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
        if (!path.empty()) {
            std::string lower = path;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            bool match = false;
            for (const char *ext : exts) {
                if (lower.size() >= std::strlen(ext) && lower.compare(lower.size() - std::strlen(ext), std::strlen(ext), ext) == 0) {
                    match = true;
                    break;
                }
            }
            if (match) files.push_back(path);
        }
    }
    pclose(pipe);

    std::vector<EvaluationResult> results;
    for (const std::string &decoded_path : files) {
        std::string stem = stem_from_path(decoded_path);
        std::string ref_path = find_reference_image(decoded_path, reference_dir);
        if (ref_path.empty()) {
            continue;
        }

        EvaluationResult ev = evaluate_pair(decoded_path, ref_path);
        ev.stem = stem;
        if (!ev.path.empty()) results.push_back(ev);
    }

    std::sort(results.begin(), results.end(), [](const EvaluationResult &a, const EvaluationResult &b) {
        double a_score = a.mae / 40.0 + std::fabs(a.slant_slope) * 2.0 + a.mean_abs_shift / 2.0;
        double b_score = b.mae / 40.0 + std::fabs(b.slant_slope) * 2.0 + b.mean_abs_shift / 2.0;
        return a_score > b_score;
    });

    printf("Decoded image evaluation\n");
    printf("decoded_dir : %s\n", decoded_dir.c_str());
    printf("reference_dir: %s\n", reference_dir.c_str());
    printf("\n");
    printf("%-20s %-8s %-8s %-8s %-8s\n", "mode", "MAE", "shift", "slant", "rms");
    printf("%-20s %-8s %-8s %-8s %-8s\n", "----", "---", "-----", "-----", "---");

    for (const EvaluationResult &ev : results) {
        printf("%-20s %7.2f %7.2f %7.2f %7.2f\n",
               ev.stem.c_str(), ev.mae, ev.mean_abs_shift, std::fabs(ev.slant_slope), ev.slant_rms);
    }

    printf("\nLikely timing/slant candidates (highest combined score):\n");
    size_t limit = std::min<size_t>(results.size(), 8u);
    for (size_t i = 0; i < limit; ++i) {
        const EvaluationResult &ev = results[i];
        printf("  %zu. %s  mae=%.2f  shift=%.2f  slant=%.2f\n",
               i + 1, ev.stem.c_str(), ev.mae, ev.mean_abs_shift, std::fabs(ev.slant_slope));
    }

    return 0;
}
