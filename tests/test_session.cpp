// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// The receive session: state machine, events, and a real decode.
//
// Tier 2 under ADR-0012, so these were written against
// docs/plans/session-behaviour.md rather than against the implementation. The
// audio is a recording played through the fake source, which is what lets this
// run in CI with no sound card (ADR-0011).

#include <cstdint>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

#include "station/audio.h"
#include "station/session.h"

namespace {

int g_failures = 0;

void check(bool ok, const std::string &what, const std::string &detail = "") {
    std::printf("  %s  %s%s%s\n", ok ? "ok  " : "FAIL", what.c_str(),
                detail.empty() ? "" : " : ", detail.c_str());
    if (!ok) g_failures++;
}

/* Collects everything the session publishes, so a test can assert on the
 * sequence rather than on internal state. */
struct Recorder {
    std::vector<station::json::Value> events;

    station::EventSink sink() {
        return [this](const station::json::Value &e) { events.push_back(e); };
    }
    int count(const std::string &topic) const {
        int n = 0;
        for (const auto &e : events)
            if (e["event"].as_string() == topic) n++;
        return n;
    }
    const station::json::Value *first(const std::string &topic) const {
        for (const auto &e : events)
            if (e["event"].as_string() == topic) return &e;
        return nullptr;
    }
    std::vector<std::string> states() const {
        std::vector<std::string> out;
        for (const auto &e : events)
            if (e["event"].as_string() == "session.state") out.push_back(e["to"].as_string());
        return out;
    }
};

std::string audio_dir(int argc, char **argv) {
    if (argc > 1) return argv[1];
    for (const char *candidate : {"tests/audio", "../tests/audio", "../../tests/audio"}) {
        std::string probe = std::string(candidate) + "/alt5_test_panel_martin1.wav";
        if (FILE *f = std::fopen(probe.c_str(), "rb")) { std::fclose(f); return candidate; }
    }
    return "";
}

void test_wav_reader(const std::string &dir) {
    std::printf("WAV source\n");
    std::string error;

    auto src = station::WavSource::open(dir + "/alt5_test_panel_martin1.wav", &error);
    check(src != nullptr, "opens a 16-bit mono recording", error);
    if (!src) return;
    check(src->sample_rate() == 22050.0, "reports the file's sample rate");
    check(src->total_samples() > 2000000, "reads the whole file");

    std::vector<float> block;
    check(src->read(&block, 1024) && block.size() == 1024, "reads a block");
    bool in_pcm_scale = false;
    for (float s : block) if (s > 1.5f || s < -1.5f) in_pcm_scale = true;
    check(in_pcm_scale, "samples are on the 16-bit PCM scale, not normalised");

    /* Reading to the end must terminate rather than spin. */
    int guard = 0;
    while (src->read(&block, 65536) && guard < 100000) guard++;
    check(guard < 100000, "reading stops at the end of the file");

    check(station::WavSource::open(dir + "/does_not_exist.wav", &error) == nullptr,
          "a missing file is refused");
    check(station::WavSource::open("tests/audio/alt5_test_panel_martin1.jpg", &error) == nullptr,
          "a file that is not a WAV is refused");
}

/* A WAV header is attacker-controlled. A file claiming a four-gigabyte data
 * chunk used to cause a four-gigabyte allocation, which on a 512 MB Pi is an
 * out-of-memory kill from a hundred-byte file. */
void test_hostile_wav() {
    std::printf("\nHostile WAV input\n");

    char dir_template[] = "/tmp/pocketsstv_wav_XXXXXX";
    const char *tmp = mkdtemp(dir_template);
    if (!tmp) { check(false, "make a temporary directory"); return; }
    const std::string base(tmp);

    auto write_file = [](const std::string &path, const std::string &bytes) {
        FILE *f = std::fopen(path.c_str(), "wb");
        if (!f) return false;
        std::fwrite(bytes.data(), 1, bytes.size(), f);
        std::fclose(f);
        return true;
    };
    auto u32 = [](uint32_t v) {
        std::string s(4, '\0');
        for (int i = 0; i < 4; i++) s[static_cast<size_t>(i)] = static_cast<char>((v >> (8 * i)) & 0xFF);
        return s;
    };
    auto u16 = [](uint16_t v) {
        std::string s(2, '\0');
        s[0] = static_cast<char>(v & 0xFF);
        s[1] = static_cast<char>((v >> 8) & 0xFF);
        return s;
    };

    const std::string fmt = "fmt " + u32(16) + u16(1) + u16(1) + u32(22050) +
                            u32(44100) + u16(2) + u16(16);
    std::string error;

    /* Claims 4 GB of audio, carries 64 bytes. */
    const std::string huge = "RIFF" + u32(0xFFFFFFF0u) + "WAVE" + fmt +
                             "data" + u32(0xFFFFFF00u) + std::string(64, '\0');
    write_file(base + "/huge.wav", huge);
    auto src = station::WavSource::open(base + "/huge.wav", &error);
    check(src == nullptr || src->total_samples() < 1000,
          "a data chunk larger than the file is clamped, not allocated");

    /* Claims a 2 GB format chunk. */
    const std::string bigfmt = "RIFF" + u32(200) + "WAVE" + "fmt " + u32(0x7FFFFFF0u) +
                               std::string(32, '\0');
    write_file(base + "/bigfmt.wav", bigfmt);
    check(station::WavSource::open(base + "/bigfmt.wav", &error) == nullptr,
          "an oversized format chunk is refused");

    /* Truncated, empty, and not-a-WAV files. */
    write_file(base + "/tiny.wav", "RIFF");
    check(station::WavSource::open(base + "/tiny.wav", &error) == nullptr,
          "a truncated file is refused");
    write_file(base + "/empty.wav", "");
    check(station::WavSource::open(base + "/empty.wav", &error) == nullptr,
          "an empty file is refused");
    write_file(base + "/stereo.wav", "RIFF" + u32(100) + "WAVE" + "fmt " + u32(16) +
               u16(1) + u16(2) + u32(44100) + u32(176400) + u16(4) + u16(16) +
               "data" + u32(16) + std::string(16, '\0'));
    check(station::WavSource::open(base + "/stereo.wav", &error) == nullptr,
          "a stereo file is refused with an explanation");

    for (const char *f : {"/huge.wav", "/bigfmt.wav", "/tiny.wav", "/empty.wav", "/stereo.wav"})
        std::remove((base + f).c_str());
    rmdir(base.c_str());
}

void test_decode(const std::string &dir, const char *file, const char *expect_mode,
                 int expect_lines) {
    std::printf("\nDecode %s\n", file);
    Recorder rec;
    station::Session session;
    session.set_event_sink(rec.sink());
    check(session.state() == station::State::Idle, "a new session is Idle");

    std::string error;
    auto src = station::WavSource::open(std::string(dir) + "/" + file, &error);
    if (!src) { check(false, "opens the recording", error); return; }
    check(session.start_rx(std::move(src), &error), "start_rx accepts the source", error);
    check(session.state() == station::State::Hunting, "listening after start_rx");

    int guard = 0;
    while (session.pump() && guard++ < 2000) {}
    check(guard < 2000, "the recording plays to the end");

    const auto states = rec.states();
    check(states.size() >= 4, "the session moved through its states");
    check(rec.count("rx.pictureStarted") == 1, "one picture started");
    check(rec.count("rx.pictureComplete") == 1, "one picture completed");
    check(rec.count("rx.line") > 100, "per-line progress was reported");

    const station::json::Value *started = rec.first("rx.pictureStarted");
    check(started && started->operator[]("mode").as_string() == expect_mode,
          std::string("the mode was detected from the header alone (") + expect_mode + ")",
          started ? started->operator[]("mode").as_string() : "no event");

    const station::json::Value *done = rec.first("rx.pictureComplete");
    if (done) {
        const station::json::Value &d = *done;
        check(d["status"].as_string() == "complete", "the picture is complete, not partial");
        check(static_cast<int>(d["lines"].as_number()) == expect_lines,
              "every line was decoded");
        check(d.has("quality"), "the picture carries a quality summary");
        check(!d["picture"].as_string().empty(), "the picture has an identifier");
    }

    /* Receiving is the resting state: the session returns to it by itself. */
    check(session.state() == station::State::Hunting, "back to listening afterwards");

    /* Events carry a monotonic sequence so a client can detect a gap. */
    bool monotonic = true;
    double last = 0;
    for (const auto &e : rec.events) {
        if (e["seq"].as_number() <= last) monotonic = false;
        last = e["seq"].as_number();
    }
    check(monotonic, "event sequence numbers increase");
}

void test_lifecycle(const std::string &dir) {
    std::printf("\nLifecycle\n");
    Recorder rec;
    station::Session session;
    session.set_event_sink(rec.sink());

    std::string error;
    check(!session.start_rx(nullptr, &error), "start_rx without a source is refused");
    check(!session.pump(), "pumping with no source does nothing");

    auto src = station::WavSource::open(dir + "/alt5_test_panel_r36.wav", &error);
    if (!src) { check(false, "opens a recording", error); return; }
    session.start_rx(std::move(src), &error);
    for (int i = 0; i < 50; i++) session.pump();          /* partway through */
    session.stop_rx();
    check(session.state() == station::State::Idle, "stop_rx returns to Idle");
    check(!session.pump(), "no audio moves after stop_rx");

    /* A second recording on the same session must decode cleanly: the decoder
     * is reset rather than carrying the previous picture's state. */
    Recorder second;
    session.set_event_sink(second.sink());
    auto again = station::WavSource::open(dir + "/alt5_test_panel_r36.wav", &error);
    if (!again) { check(false, "reopens the recording", error); return; }
    session.start_rx(std::move(again), &error);
    int guard = 0;
    while (session.pump() && guard++ < 2000) {}
    check(second.count("rx.pictureComplete") == 1, "a reused session decodes again");
    const station::json::Value *done = second.first("rx.pictureComplete");
    check(done && done->operator[]("mode").as_string() == "Robot 36",
          "the second decode identifies its own mode");
}

} // namespace

int main(int argc, char **argv) {
    const std::string dir = audio_dir(argc, argv);
    if (dir.empty()) {
        std::fprintf(stderr, "cannot find tests/audio; pass the path as an argument\n");
        return 1;
    }
    std::printf("Session tests (audio from %s)\n\n", dir.c_str());

    test_wav_reader(dir);
    test_hostile_wav();
    test_decode(dir, "alt5_test_panel_martin1.wav", "Martin 1", 256);
    test_decode(dir, "alt5_test_panel_robot36.wav", "Robot 36", 240);
    test_lifecycle(dir);

    std::printf("\n%s (%d failures)\n", g_failures ? "FAILED" : "ALL PASSED", g_failures);
    return g_failures ? 1 : 0;
}
