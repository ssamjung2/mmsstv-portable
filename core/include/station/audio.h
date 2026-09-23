// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// Audio sources.
//
// The station never reads a sound card directly: it pulls blocks from an
// AudioSource. That indirection is what lets the whole stack run with no
// hardware (ADR-0011) — the same code path serves a real device, a recording
// played faster than real time, or a generated signal in a test.
//
// Samples are floats on the 16-bit PCM scale (-32768..32767), which is what
// libsstv_decoder expects. Getting this wrong makes a signal roughly 32000
// times too quiet, so it is stated here rather than left to be discovered.

#ifndef STATION_AUDIO_H
#define STATION_AUDIO_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace station {

class AudioSource {
public:
    virtual ~AudioSource() = default;

    /* Sample rate of this source, in Hz. */
    virtual double sample_rate() const = 0;

    /* Fill `out` with up to `max_samples` samples. Returns false when the
     * source is exhausted; `out` may still hold the final partial block. */
    virtual bool read(std::vector<float> *out, size_t max_samples) = 0;

    /* A short description for logs and status: "fake: recording.wav". */
    virtual std::string describe() const = 0;
};

/* Plays a 16-bit PCM mono WAV file. Nothing is throttled to real time: a
 * recording feeds as fast as the decoder will take it, which is what makes
 * tests quick and a day of captured audio replayable in minutes. */
class WavSource : public AudioSource {
public:
    /* Returns nullptr and sets `error` if the file cannot be used. */
    static std::unique_ptr<WavSource> open(const std::string &path, std::string *error);

    double sample_rate() const override { return sample_rate_; }
    bool read(std::vector<float> *out, size_t max_samples) override;
    std::string describe() const override { return "fake: " + path_; }

    size_t total_samples() const { return samples_.size(); }

private:
    std::string path_;
    double sample_rate_ = 0.0;
    std::vector<float> samples_;
    size_t position_ = 0;
};

} // namespace station

#endif
