// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// pocketsstv - the command-line client.
//
// It is a client of the same API the graphical front ends use (ADR-0001), so
// anything they can do is scriptable. Every command supports --json, and a
// failed command exits non-zero (R-CLI-4).

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <string>
#include <utility>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "station/api.h"

namespace {

std::string default_socket_path() {
    if (const char *run = std::getenv("XDG_RUNTIME_DIR"))
        return std::string(run) + "/pocketsstvd.sock";
    const char *tmp = std::getenv("TMPDIR");
    std::string dir = tmp ? tmp : "/tmp";
    if (!dir.empty() && dir.back() == '/') dir.pop_back();
    return dir + "/pocketsstvd.sock";
}

class Connection {
public:
    bool open(const std::string &path, std::string *error) {
        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0) {
            *error = std::strerror(errno);
            return false;
        }
        sockaddr_un addr;
        std::memset(&addr, 0, sizeof addr);
        addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof addr.sun_path, "%s", path.c_str());
        if (::connect(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0) {
            *error = std::string(std::strerror(errno)) + " (" + path + ")";
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        return true;
    }

    ~Connection() {
        if (fd_ >= 0) ::close(fd_);
    }

    /* Read one line, whatever it is. */
    bool read_line(std::string *line, std::string *error) {
        line->clear();
        char c;
        while (true) {
            ssize_t n = ::read(fd_, &c, 1);
            if (n == 0) {
                *error = "daemon closed the connection";
                return false;
            }
            if (n < 0) {
                if (errno == EINTR) continue;
                *error = std::strerror(errno);
                return false;
            }
            if (c == '\n') break;
            line->push_back(c);
            if (line->size() > station::kMaxRequestBytes) {
                *error = "response too large";
                return false;
            }
        }
        return true;
    }

    /* Send one request and read its reply.
     *
     * Events can arrive between the request and the reply once a subscription
     * is active, so anything without an "id" is an event: handed to the
     * callback and skipped. */
    bool call(const std::string &request, std::string *response, std::string *error,
              const std::function<void(const std::string &)> &on_event = nullptr) {
        std::string line = request;
        line.push_back('\n');
        if (::write(fd_, line.data(), line.size()) < 0) {
            *error = std::strerror(errno);
            return false;
        }
        while (read_line(response, error)) {
            station::json::Value v;
            std::string perr;
            if (station::json::parse(*response, &v, &perr) && v.has("event")) {
                if (on_event) on_event(*response);
                continue;
            }
            return true;
        }
        return false;
    }

private:
    int fd_ = -1;
};

/* Print an error the way the rest of the product will: the daemon's own
 * remedy text, not a raw code. */
int report_api_error(const station::json::Value &error, bool as_json, const std::string &raw) {
    if (as_json) {
        std::printf("%s\n", raw.c_str());
        return 1;
    }
    const std::string kind = error["data"]["kind"].as_string();
    std::fprintf(stderr, "error: %s\n", error["message"].as_string().c_str());
    const std::string remedy = error["data"]["remedy"].as_string();
    if (!remedy.empty()) std::fprintf(stderr, "  %s\n", remedy.c_str());
    if (!kind.empty()) std::fprintf(stderr, "  (%s)\n", kind.c_str());
    return 1;
}

void print_usage() {
    std::printf("pocketsstv - PocketSSTV command-line client\n\n"
                "Usage: pocketsstv [options] <command>\n\n"
                "Commands:\n"
                "  version          Versions of this client and the daemon\n"
                "  info             Daemon status (system.info)\n"
                "  status           What the receive session is doing\n"
                "  monitor          Follow events until interrupted\n"
                "  feed FILE        Play a WAV recording through the decoder\n\n"
                "Options:\n"
                "  --socket PATH    Control socket (default: %s)\n"
                "  --api MAJOR.MINOR  Claim a different API version, for testing\n"
                "  --json           Machine-readable output\n"
                "  --verbose        Include per-line progress\n"
                "  --help           This text\n",
                default_socket_path().c_str());
}

/* One line per event, in the order they happened. Progress is deliberately
 * quiet: 255 lines of "line 42 of 256" would bury the events that matter. */
void print_event(const std::string &raw, bool as_json, bool verbose) {
    if (as_json) {
        std::printf("%s\n", raw.c_str());
        std::fflush(stdout);
        return;
    }

    station::json::Value ev;
    std::string err;
    if (!station::json::parse(raw, &ev, &err)) return;
    const std::string topic = ev["event"].as_string();

    if (topic == "rx.progress") {
        if (!verbose) return;
        std::printf("  line %.0f of %.0f\n", ev["line"].as_number(), ev["lines_total"].as_number());
    } else if (topic == "session.state") {
        std::printf("%-22s %s -> %s (%s)\n", topic.c_str(), ev["from"].as_string().c_str(),
                    ev["to"].as_string().c_str(), ev["reason"].as_string().c_str());
    } else if (topic == "rx.pictureStarted") {
        std::printf("%-22s %s, %.0f lines  [%s]\n", topic.c_str(), ev["mode"].as_string().c_str(),
                    ev["lines_total"].as_number(), ev["picture"].as_string().c_str());
    } else if (topic == "rx.pictureComplete") {
        std::printf("%-22s %s  %s  %.0f/%.0f lines", topic.c_str(),
                    ev["status"].as_string().c_str(), ev["mode"].as_string().c_str(),
                    ev["lines"].as_number(), ev["lines_total"].as_number());
        if (ev.has("width"))
            std::printf("  %.0fx%.0f", ev["width"].as_number(), ev["height"].as_number());
        std::printf("\n");
    } else {
        std::printf("%-22s %s\n", topic.c_str(), ev["reason"].as_string().c_str());
    }
    std::fflush(stdout);
}

} // namespace

static int run(int argc, char **argv) {
    std::string socket_path = default_socket_path();
    std::string command;
    std::string argument;
    std::string api_version;
    bool as_json = false;
    bool verbose = false;

    {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%d.%d", station::kApiMajor, station::kApiMinor);
        api_version = buf;
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto next = [&](const char *what) -> const char * {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s requires a value\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--socket")
            socket_path = next("--socket");
        else if (arg == "--api")
            api_version = next("--api");
        else if (arg == "--json")
            as_json = true;
        else if (arg == "--verbose" || arg == "-v")
            verbose = true;
        else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (!arg.empty() && arg[0] == '-') {
            std::fprintf(stderr, "unknown option: %s\n", arg.c_str());
            return 2;
        } else if (command.empty())
            command = arg;
        else if (argument.empty())
            argument = arg;
        else {
            std::fprintf(stderr, "unexpected argument: %s\n", arg.c_str());
            return 2;
        }
    }

    if (command.empty()) {
        print_usage();
        return 2;
    }
    if (command != "version" && command != "info" && command != "status" && command != "monitor" &&
        command != "feed") {
        std::fprintf(stderr, "unknown command: %s\n", command.c_str());
        return 2;
    }
    if (command == "feed" && argument.empty()) {
        std::fprintf(stderr, "feed needs a WAV file\n");
        return 2;
    }

    Connection conn;
    std::string error;
    if (!conn.open(socket_path, &error)) {
        std::fprintf(stderr, "error: cannot reach the daemon: %s\n", error.c_str());
        std::fprintf(stderr, "  Start it with: pocketsstvd --socket %s\n", socket_path.c_str());
        return 1;
    }

    /* Every session begins with version negotiation (ADR-0010). */
    station::json::Value hello_params = station::json::Value::object();
    hello_params["api"] = api_version;
    hello_params["client"] = std::string("pocketsstv/") + POCKETSSTV_VERSION;
    station::json::Value hello = station::json::Value::object();
    hello["jsonrpc"] = "2.0";
    hello["id"] = 1;
    hello["method"] = "hello";
    hello["params"] = std::move(hello_params);

    std::string raw;
    if (!conn.call(hello.dump(), &raw, &error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    station::json::Value response;
    std::string parse_error;
    if (!station::json::parse(raw, &response, &parse_error)) {
        std::fprintf(stderr, "error: malformed response: %s\n", parse_error.c_str());
        return 1;
    }
    if (response.has("error")) return report_api_error(response["error"], as_json, raw);

    if (command == "version") {
        if (as_json) {
            station::json::Value out = station::json::Value::object();
            out["client"] = std::string(POCKETSSTV_VERSION);
            out["client_api"] = api_version;
            out["daemon"] = response["result"]["daemon"];
            out["daemon_api"] = response["result"]["api"];
            std::printf("%s\n", out.dump().c_str());
        } else {
            std::printf("client   %s (api %s)\n", POCKETSSTV_VERSION, api_version.c_str());
            std::printf("daemon   %s (api %s)\n", response["result"]["daemon"].as_string().c_str(),
                        response["result"]["api"].as_string().c_str());
        }
        return 0;
    }

    if (command == "status" || command == "monitor" || command == "feed") {
        auto send = [&](const std::string &method, station::json::Value params,
                        station::json::Value *result) -> bool {
            station::json::Value rq = station::json::Value::object();
            rq["jsonrpc"] = "2.0";
            rq["id"] = 2;
            rq["method"] = method;
            if (!params.is_null()) rq["params"] = std::move(params);
            if (!conn.call(rq.dump(), &raw, &error,
                           [&](const std::string &ev) { print_event(ev, as_json, verbose); })) {
                std::fprintf(stderr, "error: %s\n", error.c_str());
                return false;
            }
            station::json::Value v;
            if (!station::json::parse(raw, &v, &parse_error)) {
                std::fprintf(stderr, "error: malformed response: %s\n", parse_error.c_str());
                return false;
            }
            if (v.has("error")) {
                report_api_error(v["error"], as_json, raw);
                return false;
            }
            *result = v["result"];
            return true;
        };

        if (command == "status") {
            station::json::Value result;
            if (!send("session.status", station::json::Value(), &result)) return 1;
            if (as_json) {
                std::printf("%s\n", result.dump().c_str());
                return 0;
            }
            std::printf("state    %s\n", result["state"].as_string().c_str());
            std::printf("source   %s\n", result["source"].as_string().c_str());
            if (result.has("picture"))
                std::printf("picture  %s  %s  line %.0f of %.0f\n",
                            result["picture"].as_string().c_str(),
                            result["mode"].as_string().c_str(), result["line"].as_number(),
                            result["lines_total"].as_number());
            std::printf("received %.0f\n", result["pictures_completed"].as_number());
            return 0;
        }

        /* Both monitor and feed watch the event stream. */
        station::json::Value topics = station::json::Value::array();
        topics.push_back(std::string("session.*"));
        topics.push_back(std::string("rx.*"));
        topics.push_back(std::string("audio.*"));
        station::json::Value sub = station::json::Value::object();
        sub["topics"] = std::move(topics);
        station::json::Value result;
        if (!send("events.subscribe", std::move(sub), &result)) return 1;

        if (command == "feed") {
            station::json::Value params = station::json::Value::object();
            params["file"] = argument;
            if (!send("dev.feed", std::move(params), &result)) return 1;
            if (!as_json)
                std::printf("playing %s (%.0f Hz, %.1f s of audio)\n", argument.c_str(),
                            result["sample_rate"].as_number(),
                            result["samples"].as_number() / result["sample_rate"].as_number());
        } else if (!as_json) {
            std::printf("watching events, interrupt to stop\n");
        }

        /* Follow until the recording ends, or forever for monitor. */
        while (true) {
            if (!conn.read_line(&raw, &error)) {
                std::fprintf(stderr, "error: %s\n", error.c_str());
                return 1;
            }
            print_event(raw, as_json, verbose);
            if (command == "feed" && raw.find("\"audio.sourceEnded\"") != std::string::npos)
                return 0;
        }
    }

    /* info */
    station::json::Value req = station::json::Value::object();
    req["jsonrpc"] = "2.0";
    req["id"] = 2;
    req["method"] = "system.info";
    if (!conn.call(req.dump(), &raw, &error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    if (!station::json::parse(raw, &response, &parse_error)) {
        std::fprintf(stderr, "error: malformed response: %s\n", parse_error.c_str());
        return 1;
    }
    if (response.has("error")) return report_api_error(response["error"], as_json, raw);

    if (as_json) {
        std::printf("%s\n", response["result"].dump().c_str());
    } else {
        const station::json::Value &r = response["result"];
        std::printf("daemon   %s\n", r["versions"]["daemon"].as_string().c_str());
        std::printf("api      %s\n", r["versions"]["api"].as_string().c_str());
        std::printf("encoder  %s\n", r["versions"]["encoder"].as_string().c_str());
        std::printf("platform %s/%s\n", r["platform"].as_string().c_str(),
                    r["arch"].as_string().c_str());
        std::printf("uptime   %.0f s\n", r["uptime_s"].as_number());
    }
    return 0;
}

/* std::string and the JSON types can throw, and an exception escaping main is
 * a crash with no explanation. Catch at the boundary so the operator gets a
 * message and a non-zero exit instead. */
int main(int argc, char **argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "pocketsstv: internal error: %s\n", e.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "pocketsstv: internal error\n");
        return 1;
    }
}
