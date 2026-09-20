// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors

#include "station/api.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "sstv_encoder.h"   /* SSTV_ENCODER_VERSION, for system.info */

namespace station {
namespace {

/* JSON-RPC reserved codes, plus one application code. Clients switch on
 * `data.kind`, never on these numbers; they exist because the spec requires
 * them. */
constexpr int kCodeParseError     = -32700;
constexpr int kCodeInvalidRequest = -32600;
constexpr int kCodeMethodNotFound = -32601;
constexpr int kCodeApplication    = -32001;

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

std::string version_string(int major, int minor) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d.%d", major, minor);
    return buf;
}

} // namespace

std::string make_error(const json::Value &id, const std::string &kind,
                       const std::string &message, const std::string &remedy,
                       json::Value extra) {
    json::Value data = extra.is_object() ? extra : json::Value::object();
    data["kind"] = kind;
    if (!remedy.empty()) data["remedy"] = remedy;

    json::Value err = json::Value::object();
    err["code"] = code_for_kind(kind);
    err["message"] = message;
    err["data"] = data;

    json::Value resp = json::Value::object();
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;
    resp["error"] = err;
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
        return make_error(id, "invalid_request", "missing method",
                          "Include a \"method\" string.");
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

    return is_notification ? std::string()
        : make_error(id, "method_not_found", "unknown method: " + method,
                     "Check the method name against the API specification.");
}

std::string Api::handle_hello(const json::Value &req, const json::Value &id) {
    const json::Value &params = req["params"];
    const std::string requested = params["api"].as_string();

    int major = 0, minor = 0;
    if (requested.empty() || std::sscanf(requested.c_str(), "%d.%d", &major, &minor) != 2) {
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
    result["versions"] = versions;
    result["platform"] = platform_name();
    result["arch"] = arch_name();
    result["uptime_s"] = uptime_seconds();
    if (!client_name_.empty()) result["client"] = client_name_;
    return make_result(id, result);
}

} // namespace station
