// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// The receive session: the state machine described in
// docs/plans/session-behaviour.md, driving libsstv_decoder.
//
// Day two implements the receive half only — Idle, Hunting, Receiving,
// Completing — and the transmit states arrive with the keying work in M2. The
// states and their names come from the specification rather than being
// invented here, so the document and the code can be read side by side.

#ifndef STATION_SESSION_H
#define STATION_SESSION_H

#include <functional>
#include <memory>
#include <string>

#include "station/audio.h"
#include "station/json.h"

struct sstv_decoder_s;

namespace station {

enum class State {
    Idle,       /* not using the sound card */
    Hunting,    /* listening for a header: the resting state */
    Receiving,  /* a picture is arriving */
    Completing, /* finalising: flushing the last line, storing */
};

const char *state_name(State s);

/* Events are JSON objects delivered to whoever is subscribed. The session does
 * not know about sockets or clients: it publishes, and the daemon fans out. */
using EventSink = std::function<void(const json::Value &)>;

class Session {
public:
    Session();
    ~Session();

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    void set_event_sink(EventSink sink) { sink_ = std::move(sink); }

    State state() const { return state_; }

    /* Cheap enough to ask every time round the daemon's loop, unlike
     * status(), which builds a JSON object. */
    bool has_source() const { return source_ != nullptr; }

    /* Attach an audio source and begin listening. Replaces any current source.
     * Returns false with `error` set if a decoder cannot be made for the
     * source's sample rate. */
    bool start_rx(std::unique_ptr<AudioSource> source, std::string *error);

    /* Stop listening and release the source. */
    void stop_rx();

    /* Move audio through the decoder. Returns true while there is more to do,
     * so the caller can keep pumping until a source is exhausted. This is the
     * single-threaded shape of day two; real-time audio threading arrives with
     * the audio backend (SP-1). */
    bool pump();

    /* Status for session.status. */
    json::Value status() const;

private:
    void set_state(State next, const char *reason);
    void publish(const std::string &topic, json::Value payload);
    void on_image_ready();
    void finish_source();

    State state_ = State::Idle;
    EventSink sink_;
    std::unique_ptr<AudioSource> source_;
    sstv_decoder_s *decoder_ = nullptr;
    double sample_rate_ = 0.0;

    std::string picture_id_;
    int current_mode_ = -1;
    int lines_decoded_ = 0;
    int lines_total_ = 0;
    long long samples_fed_ = 0;
    unsigned long long sequence_ = 0;
    unsigned long long pictures_completed_ = 0;
};

} // namespace station

#endif
