// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors

#include "station/session.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "sstv_decoder.h"
#include "sstv_encoder.h" /* sstv_get_mode_info, for mode names */

namespace station {
namespace {

/* One decode step. Small enough that events arrive promptly, large enough
 * that the per-call overhead is irrelevant. */
constexpr size_t kBlockSamples = 4096;

/* Picture identifiers are sortable by time so a directory listing and a
 * database query agree on order. A ULID proper arrives with the library
 * module; this is the same idea with the parts we have today. */
std::string make_picture_id() {
    static unsigned long long counter = 0;
    char buf[64];
    std::snprintf(buf, sizeof buf, "p_%010lld_%04llu", static_cast<long long>(std::time(nullptr)),
                  counter++);
    return buf;
}

const char *mode_name(int mode) {
    if (mode < 0 || mode >= SSTV_MODE_COUNT) return "unknown";
    const sstv_mode_info_t *info = sstv_get_mode_info(static_cast<sstv_mode_t>(mode));
    return info ? info->name : "unknown";
}

} // namespace

const char *state_name(State s) {
    switch (s) {
    case State::Idle:
        return "Idle";
    case State::Hunting:
        return "Hunting";
    case State::Receiving:
        return "Receiving";
    case State::Completing:
        return "Completing";
    }
    return "unknown";
}

Session::Session() = default;

Session::~Session() {
    if (decoder_) sstv_decoder_free(decoder_);
}

void Session::publish(const std::string &topic, json::Value payload) {
    if (!sink_) return;
    payload["event"] = topic;
    payload["seq"] = static_cast<long long>(++sequence_);
    sink_(payload);
}

void Session::set_state(State next, const char *reason) {
    if (next == state_) return;
    json::Value ev = json::Value::object();
    ev["from"] = state_name(state_);
    ev["to"] = state_name(next);
    ev["reason"] = reason;
    state_ = next;
    publish("session.state", std::move(ev));
}

bool Session::start_rx(std::unique_ptr<AudioSource> source, std::string *error) {
    if (!source) {
        *error = "no audio source";
        return false;
    }
    const double rate = source->sample_rate();

    /* A decoder is bound to one sample rate, so a source with a different rate
     * needs a new one rather than a reset. */
    if (decoder_ && rate != sample_rate_) {
        sstv_decoder_free(decoder_);
        decoder_ = nullptr;
    }
    if (!decoder_) {
        decoder_ = sstv_decoder_create(rate);
        if (!decoder_) {
            *error = "could not create a decoder for that sample rate";
            return false;
        }
        sample_rate_ = rate;
    } else {
        sstv_decoder_reset(decoder_);
    }

    source_ = std::move(source);
    picture_id_.clear();
    current_mode_ = -1;
    lines_decoded_ = 0;
    lines_total_ = 0;
    samples_fed_ = 0;
    set_state(State::Hunting, "startRx");
    return true;
}

void Session::stop_rx() {
    source_.reset();
    if (decoder_) sstv_decoder_reset(decoder_);
    picture_id_.clear();
    current_mode_ = -1;
    set_state(State::Idle, "stopRx");
}

void Session::on_image_ready() {
    set_state(State::Completing, "picture complete");

    sstv_decoder_state_t st;
    std::memset(&st, 0, sizeof st);
    sstv_decoder_get_state(decoder_, &st);

    sstv_image_t img;
    std::memset(&img, 0, sizeof img);
    const bool have_image = sstv_decoder_get_image(decoder_, &img) == 0;

    json::Value ev = json::Value::object();
    ev["picture"] = picture_id_;
    ev["status"] =
        (st.current_line >= st.total_lines && st.total_lines > 0) ? "complete" : "partial";
    ev["mode"] = mode_name(st.current_mode);
    ev["lines"] = st.current_line;
    ev["lines_total"] = st.total_lines;
    if (have_image) {
        ev["width"] = static_cast<long long>(img.width);
        ev["height"] = static_cast<long long>(img.height);
    }
    /* What the decoder measured, kept together as the data model specifies so
     * it can grow without a schema change. */
    json::Value quality = json::Value::object();
    quality["timing_error_samples"] = st.timing_error;
    quality["timing_correction"] = st.timing_correction;
    ev["quality"] = std::move(quality);
    publish("rx.pictureComplete", std::move(ev));

    pictures_completed_++;
    picture_id_.clear();
    current_mode_ = -1;
    lines_decoded_ = 0;
    lines_total_ = 0;

    /* Receiving is the resting state: go straight back to listening. */
    sstv_decoder_reset(decoder_);
    set_state(State::Hunting, "ready for the next picture");
}

void Session::finish_source() {
    /* The recording ended. finish() flushes a partially decoded last line,
     * which is how a picture that ends mid-line still reaches the library. */
    if (decoder_ && sstv_decoder_finish(decoder_) == SSTV_RX_IMAGE_READY) {
        on_image_ready();
    } else if (state_ == State::Receiving) {
        json::Value ev = json::Value::object();
        ev["picture"] = picture_id_;
        ev["status"] = "partial";
        ev["mode"] = mode_name(current_mode_);
        ev["lines"] = lines_decoded_;
        ev["lines_total"] = lines_total_;
        publish("rx.pictureComplete", std::move(ev));
        picture_id_.clear();
        sstv_decoder_reset(decoder_);
    }
    source_.reset();
    json::Value ev = json::Value::object();
    ev["reason"] = "source exhausted";
    publish("audio.sourceEnded", std::move(ev));
    set_state(State::Hunting, "source exhausted");
}

bool Session::pump() {
    if (!source_ || !decoder_) return false;

    std::vector<float> block;
    if (!source_->read(&block, kBlockSamples) || block.empty()) {
        finish_source();
        return false;
    }
    samples_fed_ += static_cast<long long>(block.size());

    const sstv_rx_status_t status = sstv_decoder_feed(decoder_, block.data(), block.size());

    sstv_decoder_state_t st;
    std::memset(&st, 0, sizeof st);
    sstv_decoder_get_state(decoder_, &st);

    /* A mode appearing means a header was decoded: the picture has started. */
    if (state_ == State::Hunting && st.current_mode >= 0 && st.current_mode < SSTV_MODE_COUNT &&
        st.total_lines > 0) {
        picture_id_ = make_picture_id();
        current_mode_ = st.current_mode;
        lines_total_ = st.total_lines;
        set_state(State::Receiving, "header detected");

        json::Value ev = json::Value::object();
        ev["picture"] = picture_id_;
        ev["mode"] = mode_name(current_mode_);
        ev["lines_total"] = lines_total_;
        publish("rx.pictureStarted", std::move(ev));
    }

    /* Line progress, reported as it changes rather than per line: the pixels
     * themselves belong on the data channel (ADR-0005) and arrive later. */
    if (state_ == State::Receiving && st.current_line != lines_decoded_) {
        lines_decoded_ = st.current_line;
        json::Value ev = json::Value::object();
        ev["picture"] = picture_id_;
        ev["line"] = lines_decoded_;
        ev["lines_total"] = lines_total_;
        publish("rx.line", std::move(ev));
    }

    if (status == SSTV_RX_IMAGE_READY) on_image_ready();
    return true;
}

json::Value Session::status() const {
    json::Value v = json::Value::object();
    v["state"] = state_name(state_);
    v["source"] = source_ ? source_->describe() : std::string("none");
    v["sample_rate"] = sample_rate_;
    v["samples_fed"] = static_cast<long long>(samples_fed_);
    v["pictures_completed"] = static_cast<long long>(pictures_completed_);
    if (!picture_id_.empty()) {
        v["picture"] = picture_id_;
        v["mode"] = mode_name(current_mode_);
        v["line"] = lines_decoded_;
        v["lines_total"] = lines_total_;
    }
    return v;
}

} // namespace station
