// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// The station API: one request line in, one response line out.
//
// The transport is newline-delimited JSON-RPC 2.0 (ADR-0005, amended by spike
// SP-2). Bulk data travels on a separate optional channel and is not handled
// here. Keeping dispatch transport-agnostic is what lets the daemon and the
// embedded iOS build share it (ADR-0001).

#ifndef STATION_API_H
#define STATION_API_H

#include <set>
#include <string>

#include "station/json.h"
#include "station/session.h"

namespace station {

/* Everything a client can act on, shared by every connection. The daemon owns
 * one; each client's Api dispatches into it. */
struct Station {
    Session session;
};

/* API version, negotiated at connect. Additive changes bump the minor; any
 * removal or change of meaning bumps the major (ADR-0010). */
constexpr int kApiMajor = 1;
constexpr int kApiMinor = 0;

/* Longest request line we will consider. A control-plane message is a few
 * hundred bytes; anything vastly larger is a mistake or an attack, and is
 * rejected before parsing. */
constexpr size_t kMaxRequestBytes = size_t{64} * 1024;

class Api {
public:
    /* `station` may be null in unit tests that exercise dispatch alone; the
     * methods that need it then answer with `unavailable` rather than
     * crashing. */
    explicit Api(Station *station = nullptr) : station_(station) {}

    /* Handle one request line. Returns the response line, without a trailing
     * newline; the caller frames it. Never throws: every failure becomes a
     * JSON-RPC error carrying a machine-readable `kind` and a human `remedy`.
     *
     * A notification (a request with no `id`) produces an empty string, which
     * the caller should not send. */
    std::string handle_line(const std::string &line);

    /* True once a client has completed version negotiation. Methods other
     * than `hello` are refused before that, so a client cannot act on
     * assumptions the daemon has not agreed to. */
    bool negotiated() const { return negotiated_; }

    /* Does this client want this event? Topics are matched exactly or by a
     * trailing wildcard, so "rx.*" covers every receive event. */
    bool subscribed_to(const std::string &topic) const;
    bool has_subscriptions() const { return !subscriptions_.empty(); }

private:
    std::string handle_hello(const json::Value &req, const json::Value &id);
    std::string handle_system_info(const json::Value &id);
    std::string handle_subscribe(const json::Value &req, const json::Value &id);
    std::string handle_session(const std::string &method, const json::Value &req,
                               const json::Value &id);

    Station *station_ = nullptr;
    bool negotiated_ = false;
    std::string client_name_;
    std::set<std::string> subscriptions_;
};

/* Build a JSON-RPC error response. `kind` is the stable identifier clients
 * switch on; `remedy` is shown to a person. */
std::string make_error(const json::Value &id, const std::string &kind, const std::string &message,
                       const std::string &remedy, const json::Value &extra = json::Value());

/* Build a JSON-RPC success response. */
std::string make_result(const json::Value &id, const json::Value &result);

} // namespace station

#endif
