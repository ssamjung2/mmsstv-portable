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
#include <string>

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
        if (fd_ < 0) { *error = std::strerror(errno); return false; }
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

    ~Connection() { if (fd_ >= 0) ::close(fd_); }

    /* Send one request, read one response line. */
    bool call(const std::string &request, std::string *response, std::string *error) {
        std::string line = request;
        line.push_back('\n');
        if (::write(fd_, line.data(), line.size()) < 0) {
            *error = std::strerror(errno);
            return false;
        }
        response->clear();
        char c;
        while (true) {
            ssize_t n = ::read(fd_, &c, 1);
            if (n == 0) { *error = "daemon closed the connection"; return false; }
            if (n < 0) {
                if (errno == EINTR) continue;
                *error = std::strerror(errno);
                return false;
            }
            if (c == '\n') break;
            response->push_back(c);
            if (response->size() > station::kMaxRequestBytes) {
                *error = "response too large";
                return false;
            }
        }
        return true;
    }

private:
    int fd_ = -1;
};

/* Print an error the way the rest of the product will: the daemon's own
 * remedy text, not a raw code. */
int report_api_error(const station::json::Value &error, bool as_json,
                     const std::string &raw) {
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
    std::printf(
        "pocketsstv - PocketSSTV command-line client\n\n"
        "Usage: pocketsstv [options] <command>\n\n"
        "Commands:\n"
        "  version          Versions of this client and the daemon\n"
        "  info             Daemon status (system.info)\n\n"
        "Options:\n"
        "  --socket PATH    Control socket (default: %s)\n"
        "  --api MAJOR.MINOR  Claim a different API version, for testing\n"
        "  --json           Machine-readable output\n"
        "  --help           This text\n",
        default_socket_path().c_str());
}

} // namespace

int main(int argc, char **argv) {
    std::string socket_path = default_socket_path();
    std::string command;
    std::string api_version;
    bool as_json = false;

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
        if (arg == "--socket") socket_path = next("--socket");
        else if (arg == "--api") api_version = next("--api");
        else if (arg == "--json") as_json = true;
        else if (arg == "--help" || arg == "-h") { print_usage(); return 0; }
        else if (!arg.empty() && arg[0] == '-') {
            std::fprintf(stderr, "unknown option: %s\n", arg.c_str());
            return 2;
        }
        else if (command.empty()) command = arg;
        else { std::fprintf(stderr, "unexpected argument: %s\n", arg.c_str()); return 2; }
    }

    if (command.empty()) { print_usage(); return 2; }
    if (command != "version" && command != "info") {
        std::fprintf(stderr, "unknown command: %s\n", command.c_str());
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
    hello["params"] = hello_params;

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
            std::printf("daemon   %s (api %s)\n",
                        response["result"]["daemon"].as_string().c_str(),
                        response["result"]["api"].as_string().c_str());
        }
        return 0;
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
