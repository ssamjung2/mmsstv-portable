// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors

#include "station/audio.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace station {
namespace {

/* Little-endian readers. Everything is assembled in unsigned arithmetic: WAV
 * fields are unsigned, and mixing in signed types is how byte readers acquire
 * sign-extension bugs on the one file that has a high bit set. */
uint32_t read_u32(const unsigned char *p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8U) |
           (static_cast<uint32_t>(p[2]) << 16U) | (static_cast<uint32_t>(p[3]) << 24U);
}
uint16_t read_u16(const unsigned char *p) {
    const unsigned v = static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8U);
    return static_cast<uint16_t>(v);
}

} // namespace

std::unique_ptr<WavSource> WavSource::open(const std::string &path, std::string *error) {
    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        *error = "cannot open " + path;
        return nullptr;
    }

    /* How big the file actually is. Chunk headers are attacker-controlled: a
     * 108-byte file claiming a 4 GB data chunk must not cause a 4 GB
     * allocation, which on a 512 MB Pi is an out-of-memory kill. */
    std::fseek(fp, 0, SEEK_END);
    const long file_bytes = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (file_bytes < 44) {
        *error = path + ": too short to be a WAV file";
        std::fclose(fp);
        return nullptr;
    }

    unsigned char riff[12];
    if (std::fread(riff, 1, sizeof riff, fp) != sizeof riff || std::memcmp(riff, "RIFF", 4) != 0 ||
        std::memcmp(riff + 8, "WAVE", 4) != 0) {
        *error = path + " is not a RIFF/WAVE file";
        std::fclose(fp);
        return nullptr;
    }

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    std::vector<unsigned char> data;

    /* Walk the chunks rather than assuming the canonical 44-byte header:
     * recorders insert LIST and fact chunks, and a decoder that assumes a
     * fixed offset reads metadata as audio. */
    unsigned char header[8];
    while (std::fread(header, 1, sizeof header, fp) == sizeof header) {
        uint32_t size = read_u32(header + 4);
        /* Trust the file's own length, never the chunk header. */
        const long here = std::ftell(fp);
        const unsigned long remaining =
            here >= 0 && file_bytes > here ? static_cast<unsigned long>(file_bytes - here) : 0UL;
        if (size > remaining) size = static_cast<uint32_t>(remaining);
        if (size == 0 && std::memcmp(header, "data", 4) == 0) break;
        if (std::memcmp(header, "fmt ", 4) == 0) {
            std::vector<unsigned char> fmt(size);
            if (std::fread(fmt.data(), 1, size, fp) != size) break;
            if (size >= 16) {
                format = read_u16(fmt.data());
                channels = read_u16(fmt.data() + 2);
                rate = read_u32(fmt.data() + 4);
                bits = read_u16(fmt.data() + 14);
            }
        } else if (std::memcmp(header, "data", 4) == 0) {
            data.resize(size);
            const size_t got = std::fread(data.data(), 1, size, fp);
            data.resize(got); /* tolerate a truncated recording */
            break;
        } else {
            /* Chunks are padded to an even length. */
            const long padded = static_cast<long>(size) + static_cast<long>(size & 1U);
            if (std::fseek(fp, padded, SEEK_CUR) != 0) break;
        }
    }
    std::fclose(fp);

    if (format != 1 || channels != 1 || bits != 16) {
        char buf[160];
        std::snprintf(buf, sizeof buf,
                      "%s: need 16-bit PCM mono, found format %u, %u channel(s), %u-bit",
                      path.c_str(), format, channels, bits);
        *error = buf;
        return nullptr;
    }
    if (rate == 0 || data.empty()) {
        *error = path + ": no audio data";
        return nullptr;
    }

    auto src = std::unique_ptr<WavSource>(new WavSource());
    src->path_ = path;
    src->sample_rate_ = static_cast<double>(rate);
    src->samples_.reserve(data.size() / 2);
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        const int16_t s = static_cast<int16_t>(read_u16(&data[i]));
        /* 16-bit PCM scale, not normalised: see the note in audio.h. */
        src->samples_.push_back(static_cast<float>(s));
    }
    return src;
}

bool WavSource::read(std::vector<float> *out, size_t max_samples) {
    out->clear();
    if (position_ >= samples_.size()) return false;
    const size_t n = std::min(max_samples, samples_.size() - position_);
    out->assign(samples_.begin() + static_cast<long>(position_),
                samples_.begin() + static_cast<long>(position_ + n));
    position_ += n;
    return true;
}

} // namespace station
