// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// Unit tests for the station API dispatch and its JSON.
//
// These are tier 1 (contract) code under ADR-0012, so they were written
// against the API specification before the implementation. They exercise
// dispatch directly, with no sockets: transport is the daemon's business.

#include <cstdio>
#include <string>

#include "station/api.h"
#include "station/json.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const std::string &what, const std::string &detail = "") {
    g_checks++;
    if (ok) {
        std::printf("  ok    %s\n", what.c_str());
    } else {
        g_failures++;
        std::printf("  FAIL  %s%s%s\n", what.c_str(),
                    detail.empty() ? "" : " : ", detail.c_str());
    }
}

station::json::Value parse_or_die(const std::string &text) {
    station::json::Value v;
    std::string err;
    if (!station::json::parse(text, &v, &err)) {
        std::printf("  FAIL  could not parse response: %s\n  <<%s>>\n",
                    err.c_str(), text.c_str());
        g_failures++;
    }
    return v;
}

std::string error_kind(const std::string &response) {
    return parse_or_die(response)["error"]["data"]["kind"].as_string();
}

/* ---------------------------------------------------------------- JSON --- */

void test_json() {
    std::printf("JSON\n");
    using station::json::Value;
    using station::json::parse;

    Value v;
    std::string err;

    check(parse(R"({"a":1,"b":[true,null,"x"]})", &v, &err), "parses an object");
    check(v["a"].as_number() == 1.0, "reads a number");
    check(v["b"].items().size() == 3, "reads an array");
    check(v["b"].items()[2].as_string() == "x", "reads a nested string");
    check(v["missing"].is_null(), "missing keys read as null");

    /* Round trip, including the escapes a callsign or comment might carry. */
    Value obj = Value::object();
    obj["text"] = std::string("line\nbreak \"quoted\" \\ back");
    obj["utf8"] = std::string("JE3HHT \xe3\x83\xa2\xe3\x83\xaa");  /* モリ */
    std::string dumped = obj.dump();
    Value back;
    check(parse(dumped, &back, &err), "round trip parses");
    check(back["text"].as_string() == obj["text"].as_string(), "escapes survive a round trip");
    check(back["utf8"].as_string() == obj["utf8"].as_string(), "UTF-8 survives a round trip");

    check(parse(R"({"s":"é日"})", &v, &err), "parses \\u escapes");
    check(v["s"].as_string() == "\xc3\xa9\xe6\x97\xa5", "decodes \\u to UTF-8");

    /* Integers print without a decimal point, so shell tools and humans read
     * sequence numbers as sequence numbers. */
    Value n = Value::object();
    n["seq"] = 42;
    check(n.dump() == R"({"seq":42})", "integers print without a decimal point", n.dump());

    /* Hostile and malformed input is refused, never accepted quietly. */
    check(!parse("{", &v, &err), "refuses a truncated object");
    check(!parse(R"({"a":1} trailing)", &v, &err), "refuses trailing content");
    check(!parse(R"({"a":})", &v, &err), "refuses a missing value");
    check(!parse("", &v, &err), "refuses empty input");
    check(!parse("{\"a\":\"\x01\"}", &v, &err), "refuses control characters in strings");

    std::string deep(station::json::kMaxDepth + 5, '[');
    check(!parse(deep, &v, &err), "refuses input nested past the depth limit");
}

/* ----------------------------------------------------------- dispatch --- */

std::string hello_line(const char *api, const char *client = "test") {
    station::json::Value params = station::json::Value::object();
    params["api"] = std::string(api);
    params["client"] = std::string(client);
    station::json::Value req = station::json::Value::object();
    req["jsonrpc"] = "2.0";
    req["id"] = 1;
    req["method"] = "hello";
    req["params"] = params;
    return req.dump();
}

void test_negotiation() {
    std::printf("\nVersion negotiation (ADR-0010)\n");

    {
        station::Api api;
        std::string r = api.handle_line(hello_line("1.0"));
        station::json::Value v = parse_or_die(r);
        check(v.has("result"), "a matching version is accepted");
        check(v["result"]["api"].as_string() == "1.0", "the daemon states its API version");
        check(api.negotiated(), "the session is marked negotiated");
    }
    {
        /* A4's acceptance criterion: a client claiming 2.0 is rejected, and
         * the error names both versions. */
        station::Api api;
        std::string r = api.handle_line(hello_line("2.0"));
        station::json::Value v = parse_or_die(r);
        check(error_kind(r) == "version_unsupported", "a newer major is rejected", r);
        check(v["error"]["data"]["client_api"].as_string() == "2.0", "the error names the client version");
        check(v["error"]["data"]["daemon_api"].as_string() == "1.0", "the error names the daemon version");
        check(!v["error"]["data"]["remedy"].as_string().empty(), "the error carries a remedy");
        check(!api.negotiated(), "a rejected client is not negotiated");
    }
    {
        /* The daemon's minor must be at least the client's: a newer client may
         * depend on methods this daemon does not have. */
        station::Api api;
        check(error_kind(api.handle_line(hello_line("1.7"))) == "version_unsupported",
              "a newer minor is rejected");
    }
    {
        station::Api api;
        check(error_kind(api.handle_line(hello_line("banana"))) == "invalid_argument",
              "a malformed version is rejected");
    }
}

void test_dispatch() {
    std::printf("\nDispatch\n");

    {
        station::Api api;
        check(error_kind(api.handle_line(R"({"jsonrpc":"2.0","id":1,"method":"system.info"})"))
                  == "not_negotiated",
              "methods are refused before hello");
    }
    {
        station::Api api;
        api.handle_line(hello_line("1.0"));
        std::string r = api.handle_line(R"({"jsonrpc":"2.0","id":2,"method":"system.info"})");
        station::json::Value v = parse_or_die(r);
        check(v.has("result"), "system.info answers after hello");
        check(!v["result"]["versions"]["encoder"].as_string().empty(),
              "system.info reports the encoder version");
        check(v["result"]["arch"].as_string() != "", "system.info reports the architecture");
    }
    {
        station::Api api;
        api.handle_line(hello_line("1.0"));
        check(error_kind(api.handle_line(R"({"jsonrpc":"2.0","id":3,"method":"nope"})"))
                  == "method_not_found",
              "an unknown method is refused by name");
    }
    {
        station::Api api;
        check(error_kind(api.handle_line("{not json")) == "parse_error",
              "malformed JSON produces a parse error");
        check(error_kind(api.handle_line(R"(["array"])")) == "invalid_request",
              "a non-object request is refused");
        check(error_kind(api.handle_line(R"({"jsonrpc":"2.0","id":1})")) == "invalid_request",
              "a request without a method is refused");
    }
    {
        /* A notification carries no id and gets no reply. */
        station::Api api;
        api.handle_line(hello_line("1.0"));
        check(api.handle_line(R"({"jsonrpc":"2.0","method":"system.info"})").empty(),
              "a notification produces no response");
    }
    {
        station::Api api;
        std::string huge(station::kMaxRequestBytes + 10, 'x');
        check(error_kind(api.handle_line(huge)) == "invalid_request",
              "an oversized request is refused before parsing");
    }
}

void test_error_shape() {
    std::printf("\nError envelope\n");
    station::Api api;
    std::string r = api.handle_line(R"({"jsonrpc":"2.0","id":9,"method":"nope"})");
    station::json::Value v = parse_or_die(r);
    check(v["jsonrpc"].as_string() == "2.0", "responses carry the JSON-RPC version");
    check(v["id"].as_number() == 9, "the request id is echoed");
    check(v["error"]["code"].as_number() != 0, "an error carries a numeric code");
    check(!v["error"]["message"].as_string().empty(), "an error carries a message");
    check(!v["error"]["data"]["kind"].as_string().empty(),
          "every error carries a machine-readable kind");
    check(!v["error"]["data"]["remedy"].as_string().empty(),
          "every error a human can cause carries a remedy");
}

} // namespace

int main() {
    std::printf("Station API tests\n\n");
    test_json();
    test_negotiation();
    test_dispatch();
    test_error_shape();
    std::printf("\n%s: %d checks, %d failures\n",
                g_failures ? "FAILED" : "ALL PASSED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
