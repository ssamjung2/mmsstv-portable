// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// pocketsstvd - the PocketSSTV station daemon.
//
// Day one of the walking skeleton: it owns a Unix socket and answers the
// control plane. Audio, decoding, storage and keying arrive in later slices;
// the --audio and --rig options are accepted now so that the command line does
// not change when they do.

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "station/api.h"
#include "sstv_encoder.h"   /* SSTV_ENCODER_VERSION, reported by --version */

namespace {

volatile sig_atomic_t g_stop = 0;
std::string g_socket_path;

void on_signal(int) { g_stop = 1; }

/* Where the socket lives. XDG_RUNTIME_DIR is the correct home on Linux; macOS
 * has no equivalent, so fall back to TMPDIR. */
std::string default_socket_path() {
    if (const char *run = std::getenv("XDG_RUNTIME_DIR"))
        return std::string(run) + "/pocketsstvd.sock";
    const char *tmp = std::getenv("TMPDIR");
    std::string dir = tmp ? tmp : "/tmp";
    if (!dir.empty() && dir.back() == '/') dir.pop_back();
    return dir + "/pocketsstvd.sock";
}

void cleanup_socket() {
    if (!g_socket_path.empty()) ::unlink(g_socket_path.c_str());
}

int listen_unix(const std::string &path) {
    if (path.size() >= sizeof(sockaddr_un::sun_path)) {
        std::fprintf(stderr, "socket path too long: %s\n", path.c_str());
        return -1;
    }
    /* A stale socket from a killed daemon would block bind(). Removing it is
     * safe: if a live daemon owns it, our own bind still fails and we report
     * it. */
    ::unlink(path.c_str());

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { std::perror("socket"); return -1; }

    sockaddr_un addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof addr.sun_path, "%s", path.c_str());

    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0) {
        std::fprintf(stderr, "bind %s: %s\n", path.c_str(), std::strerror(errno));
        ::close(fd);
        return -1;
    }
    /* N-6: the control socket is the local user's alone. */
    if (::chmod(path.c_str(), 0600) != 0) std::perror("chmod");
    if (::listen(fd, 8) != 0) { std::perror("listen"); ::close(fd); return -1; }
    return fd;
}

/* Serve one client until it disconnects. One client at a time is enough for
 * the skeleton; multi-client arbitration (ADR-0009) lands with transmit. */
void serve(int fd) {
    station::Api api;
    std::string buffer;
    char chunk[4096];

    while (!g_stop) {
        ssize_t n = ::read(fd, chunk, sizeof chunk);
        if (n == 0) break;                       /* client closed */
        if (n < 0) {
            if (errno == EINTR) continue;
            std::perror("read");
            break;
        }
        buffer.append(chunk, static_cast<size_t>(n));

        /* An over-long line without a newline is refused rather than buffered
         * indefinitely. */
        if (buffer.size() > station::kMaxRequestBytes &&
            buffer.find('\n') == std::string::npos) {
            std::string err = station::make_error(
                station::json::Value(), "invalid_request", "request too large",
                "Send control messages under 64 kB, one per line.");
            err.push_back('\n');
            (void)::write(fd, err.data(), err.size());
            break;
        }

        size_t start = 0, nl;
        while ((nl = buffer.find('\n', start)) != std::string::npos) {
            std::string line = buffer.substr(start, nl - start);
            start = nl + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            std::string response = api.handle_line(line);
            if (response.empty()) continue;      /* notification */
            response.push_back('\n');
            if (::write(fd, response.data(), response.size()) < 0) {
                std::perror("write");
                return;
            }
        }
        buffer.erase(0, start);
    }
}

void print_version() {
    std::printf("pocketsstvd %s\n", POCKETSSTV_VERSION);
    std::printf("  api       %d.%d\n", station::kApiMajor, station::kApiMinor);
    std::printf("  encoder   %s\n", SSTV_ENCODER_VERSION);
}

void print_usage() {
    std::printf(
        "pocketsstvd - PocketSSTV station daemon\n\n"
        "Usage: pocketsstvd [options]\n\n"
        "  --socket PATH   Control socket (default: %s)\n"
        "  --audio BACKEND Audio backend: fake (default), none\n"
        "  --rig BACKEND   Rig backend: fake (default), none\n"
        "  --version       Print component versions\n"
        "  --help          This text\n",
        default_socket_path().c_str());
}

} // namespace

int main(int argc, char **argv) {
    std::string socket_path = default_socket_path();
    std::string audio = "fake", rig = "fake";

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
        else if (arg == "--audio") audio = next("--audio");
        else if (arg == "--rig") rig = next("--rig");
        else if (arg == "--version") { print_version(); return 0; }
        else if (arg == "--help" || arg == "-h") { print_usage(); return 0; }
        else {
            std::fprintf(stderr, "unknown option: %s\n", arg.c_str());
            print_usage();
            return 2;
        }
    }

    /* Reject backends we do not have rather than accepting a typo silently.
     * Real backends arrive in later slices; the option exists now so the
     * command line does not change when they do. */
    for (const auto &pair : {std::make_pair("--audio", audio), std::make_pair("--rig", rig)}) {
        if (pair.second != "fake" && pair.second != "none") {
            std::fprintf(stderr, "%s: unknown backend '%s' (available: fake, none)\n",
                         pair.first, pair.second.c_str());
            return 2;
        }
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGPIPE, SIG_IGN);   /* a client vanishing must not kill us */

    int lfd = listen_unix(socket_path);
    if (lfd < 0) return 1;
    g_socket_path = socket_path;
    std::atexit(cleanup_socket);

    std::fprintf(stderr, "pocketsstvd %s listening on %s (audio=%s rig=%s)\n",
                 POCKETSSTV_VERSION, socket_path.c_str(), audio.c_str(), rig.c_str());

    while (!g_stop) {
        int cfd = ::accept(lfd, nullptr, nullptr);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            std::perror("accept");
            break;
        }
        serve(cfd);
        ::close(cfd);
    }

    ::close(lfd);
    cleanup_socket();
    std::fprintf(stderr, "pocketsstvd: stopped\n");
    return 0;
}
