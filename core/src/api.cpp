// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors

#include "station/api.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <utility>

#include "sstv_encoder.h" /* SSTV_ENCODER_VERSION, for system.info */

namespace station {
namespace {

/* JSON-RPC reserved codes, plus one application code. Clients switch on
 * `data.kind`, never on these numbers; they exist because the spec requires
 * them. */
constexpr int kCodeParseError = -32700;
constexpr int kCodeInvalidRequest = -32600;
constexpr int kCodeMethodNotFound = -32601;
constexpr int kCodeApplication = -32001;

int code_for_kind(const std::string &kind) {
    if (kind == "parse_error") return kCodeParseError;
    if (kind == "invalid_request" || kind == "invalid_argument") return kCodeInvalidRequest;
    if (kind == "method_not_found") return kCodeMethodNotFound;
    return kCodeApplication;
}

/* Seconds since the process started, for system.info.
 *
 * Deliberately a namespace-scope global rather than a function-local static:
 * a local would initialise on the first call, so a daemon asked for its
 * status an hour after starting would report an uptime of zero. This
 * initialises when the program loads. */
/* std::time does not throw; the checker flags any non-constexpr initialiser. */
// NOLINTNEXTLINE(cert-err58-cpp,bugprone-throwing-static-initialization)
const std::time_t g_process_start = std::time(nullptr);

double uptime_seconds() {
    return std::difftime(std::time(nullptr), g_process_start);
}

const char *platform_name() {
#if defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

const char *arch_name() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__)
    return "x86_64";
#elif defined(__arm__)
    return "arm";
#else
    return "unknown";
#endif
}

/* Parse "major.minor" strictly. sscanf would accept "1.0nonsense" and silently
 * ignore overflow, which is not good enough for the first field a stranger
 * sends us. */
bool parse_version(const std::string &text, int *major, int *minor) {
    /* strtol skips leading whitespace and accepts a sign; a version field
     * should be digits and nothing else. */
    if (text.empty() || !std::isdigit(static_cast<unsigned char>(text[0]))) return false;
    const char *begin = text.c_str();
    char *end = nullptr;
    errno = 0;
    long maj = std::strtol(begin, &end, 10);
    if (end == begin || *end != '.' || errno == ERANGE || maj < 0 || maj > 9999) return false;
    const char *minor_begin = end + 1;
    errno = 0;
    long min = std::strtol(minor_begin, &end, 10);
    if (end == minor_begin || *end != '\0' || errno == ERANGE || min < 0 || min > 9999)
        return false;
    *major = static_cast<int>(maj);
    *minor = static_cast<int>(min);
    return true;
}

std::string version_string(int major, int minor) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d.%d", major, minor);
    return buf;
}

} // namespace

std::string make_error(const json::Value &id, const std::string &kind, const std::string &message,
                       const std::string &remedy, const json::Value &extra) {
    json::Value data = extra.is_object() ? extra : json::Value::object();
    data["kind"] = kind;
    if (!remedy.empty()) data["remedy"] = remedy;

    json::Value err = json::Value::object();
    err["code"] = code_for_kind(kind);
    err["message"] = message;
    err["data"] = std::move(data);

    json::Value resp = json::Value::object();
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;
    resp["error"] = std::move(err);
    return resp.dump();
}

std::string make_result(const json::Value &id, const json::Value &result) {
    json::Value resp = json::Value::object();
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;
    resp["result"] = result;
    return resp.dump();
}

std::string Api::handle_line(const std::string &line) {
    const json::Value no_id;

    if (line.size() > kMaxRequestBytes) {
        return make_error(no_id, "invalid_request", "request too large",
                          "Send control messages under 64 kB; bulk data belongs "
                          "on the data channel.");
    }

    json::Value req;
    std::string parse_error;
    if (!json::parse(line, &req, &parse_error)) {
        return make_error(no_id, "parse_error", parse_error,
                          "Send one complete JSON object per line.");
    }
    if (!req.is_object()) {
        return make_error(no_id, "invalid_request", "request is not an object",
                          "Send a JSON-RPC 2.0 request object.");
    }

    const json::Value &id = req["id"];
    const json::Value &method_value = req["method"];
    if (!method_value.is_string()) {
        return make_error(id, "invalid_request", "missing method", "Include a \"method\" string.");
    }
    const std::string method = method_value.as_string();

    /* Notifications carry no id and get no response. */
    const bool is_notification = id.is_null();

    if (method == "hello") {
        std::string out = handle_hello(req, id);
        return is_notification ? std::string() : out;
    }

    if (!negotiated_) {
        return is_notification ? std::string()
                               : make_error(id, "not_negotiated", "hello required first",
                                            "Send hello with the API version your client was built "
                                            "against before calling anything else.");
    }

    if (method == "system.info") {
        std::string out = handle_system_info(id);
        return is_notification ? std::string() : out;
    }

    if (method == "events.subscribe") {
        std::string out = handle_subscribe(req, id);
        return is_notification ? std::string() : out;
    }

    if (method.rfind("session.", 0) == 0 || method == "dev.feed") {
        std::string out = handle_session(method, req, id);
        return is_notification ? std::string() : out;
    }

    return is_notification ? std::string()
                           : make_error(id, "method_not_found", "unknown method: " + method,
                                        "Check the method name against the API specification.");
}

std::string Api::handle_hello(const json::Value &req, const json::Value &id) {
    const json::Value &params = req["params"];
    const std::string requested = params["api"].as_string();

    int major = 0, minor = 0;
    if (!parse_version(requested, &major, &minor)) {
        return make_error(id, "invalid_argument", "missing or malformed api version",
                          "Send params.api as \"major.minor\", for example \"1.0\".");
    }

    /* ADR-0010: same major, and the daemon's minor must be at least the
     * client's, because a newer client may rely on methods this daemon does
     * not have. Both versions are named so the mismatch is obvious. */
    const bool compatible = (major == kApiMajor) && (minor <= kApiMinor);
    if (!compatible) {
        json::Value extra = json::Value::object();
        extra["client_api"] = requested;
        extra["daemon_api"] = version_string(kApiMajor, kApiMinor);
        return make_error(id, "version_unsupported",
                          "client API " + requested + " is not supported by daemon API " +
                              version_string(kApiMajor, kApiMinor),
                          major == kApiMajor
                              ? "Update the daemon, or use a client built against an "
                                "earlier minor version."
                              : "Major versions differ: update whichever component is older.",
                          extra);
    }

    client_name_ = params["client"].as_string();
    negotiated_ = true;

    json::Value result = json::Value::object();
    result["api"] = version_string(kApiMajor, kApiMinor);
    result["daemon"] = POCKETSSTV_VERSION;
    result["platform"] = platform_name();
    return make_result(id, result);
}

std::string Api::handle_system_info(const json::Value &id) {
    json::Value versions = json::Value::object();
    versions["api"] = version_string(kApiMajor, kApiMinor);
    versions["daemon"] = POCKETSSTV_VERSION;
    versions["encoder"] = SSTV_ENCODER_VERSION;

    json::Value result = json::Value::object();
    result["versions"] = std::move(versions);
    result["platform"] = platform_name();
    result["arch"] = arch_name();
    result["uptime_s"] = uptime_seconds();
    if (!client_name_.empty()) result["client"] = client_name_;
    return make_result(id, result);
}

bool Api::subscribed_to(const std::string &topic) const {
    for (const std::string &pattern : subscriptions_) {
        if (pattern == "*" || pattern == topic) return true;
        if (!pattern.empty() && pattern.back() == '*' &&
            topic.compare(0, pattern.size() - 1, pattern, 0, pattern.size() - 1) == 0)
            return true;
    }
    return false;
}

std::string Api::handle_subscribe(const json::Value &req, const json::Value &id) {
    const json::Value &topics = req["params"]["topics"];
    if (topics.type() != json::Value::Type::Array || topics.items().empty()) {
        return make_error(id, "invalid_argument", "no topics given",
                          "Pass params.topics as a list, for example [\"rx.*\"].");
    }
    json::Value accepted = json::Value::array();
    for (const json::Value &t : topics.items()) {
        if (!t.is_string()) {
            return make_error(id, "invalid_argument", "topics must be strings",
                              "Pass params.topics as a list of strings.");
        }
        subscriptions_.insert(t.as_string());
        accepted.push_back(t);
    }
    json::Value result = json::Value::object();
    result["subscribed"] = std::move(accepted);
    return make_result(id, result);
}

std::string Api::handle_session(const std::string &method, const json::Value &req,
                                const json::Value &id) {
    if (!station_) {
        return make_error(id, "unavailable", "no station attached",
                          "This build dispatches without a station; start pocketsstvd.");
    }
    Session &session = station_->session;

    if (method == "session.status") {
        return make_result(id, session.status());
    }

    if (method == "session.stopRx") {
        session.stop_rx();
        return make_result(id, session.status());
    }

    /* session.startRx listens on the configured audio device. There is no
     * audio backend yet (SP-1), so it says so plainly rather than quietly
     * meaning something else. */
    if (method == "session.startRx") {
        return make_error(id, "device_unavailable", "no audio backend is configured",
                          "Play a recording with dev.feed until the audio backend "
                          "lands.");
    }

    /* dev.feed plays a recording through the fake audio device: how the stack
     * runs with no sound card (ADR-0011), and how tests drive a real decode. */
    if (method == "dev.feed") {
        const std::string path = req["params"]["file"].as_string();
        if (path.empty()) {
            return make_error(id, "invalid_argument", "no file given",
                              "Pass params.file with the path to a 16-bit mono WAV.");
        }
        std::string error;
        std::unique_ptr<AudioSource> src = WavSource::open(path, &error);
        if (!src) {
            return make_error(id, "invalid_argument", error,
                              "Convert the recording to 16-bit PCM mono, for example "
                              "with: ffmpeg -i in.wav -ac 1 -c:a pcm_s16le out.wav");
        }
        json::Value result = json::Value::object();
        result["sample_rate"] = src->sample_rate();
        result["samples"] =
            static_cast<long long>(static_cast<WavSource *>(src.get())->total_samples());
        if (!session.start_rx(std::move(src), &error)) {
            return make_error(id, "device_unavailable", error,
                              "Try a recording at a supported sample rate.");
        }
        result["state"] = state_name(session.state());
        return make_result(id, result);
    }

    return make_error(id, "method_not_found", "unknown method: " + method,
                      "Check the method name against the API specification.");
}

} // namespace station
