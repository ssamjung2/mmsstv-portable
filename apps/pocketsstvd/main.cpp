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
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "sstv_encoder.h" /* SSTV_ENCODER_VERSION, reported by --version */
#include "station/api.h"

namespace {

volatile sig_atomic_t g_stop = 0;
std::string g_socket_path;

void on_signal(int) {
    g_stop = 1;
}

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

/* Is a daemon already listening on this path?
 *
 * Unlinking a socket and binding a fresh one always succeeds, so removing a
 * stale socket blindly would silently hijack a running daemon: the original
 * keeps its file descriptor, nobody can reach it, and two processes end up
 * fighting over the audio device and, later, the radio. The only reliable
 * test is to try connecting. */
bool socket_is_live(const std::string &path) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_un addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof addr.sun_path, "%s", path.c_str());
    const bool live = ::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr) == 0;
    ::close(fd);
    return live;
}

int listen_unix(const std::string &path) {
    if (path.size() >= sizeof(sockaddr_un::sun_path)) {
        std::fprintf(stderr, "socket path too long: %s\n", path.c_str());
        return -1;
    }
    if (socket_is_live(path)) {
        std::fprintf(stderr,
                     "a daemon is already listening on %s\n"
                     "  Stop it first, or choose another socket with --socket.\n",
                     path.c_str());
        return -1;
    }
    /* Nothing answered, so any socket file here is stale — left by a daemon
     * that was killed rather than asked to stop. Removing it is what makes
     * the service restartable after a crash. */
    ::unlink(path.c_str());

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        std::perror("socket");
        return -1;
    }

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
    if (::listen(fd, 8) != 0) {
        std::perror("listen");
        ::close(fd);
        return -1;
    }
    return fd;
}

/* One connected client: its socket, its own negotiated API session, and the
 * bytes of a request that have arrived so far. */
struct Client {
    int fd = -1;
    std::unique_ptr<station::Api> api;
    std::string buffer;
    std::string outbox;             /* events queued for this client */
    unsigned long long dropped = 0; /* high-rate events this client missed */
};

/* Back-pressure, as ADR-0005 specifies it.
 *
 * A client that stops reading must never stall the decoder. Past the soft
 * limit, high-rate events are dropped and counted, because a waterfall column
 * nobody read is worthless. Past the hard limit the client is disconnected:
 * it is too far behind to be told anything useful, and a client that missed a
 * fault is dangerous. */
constexpr size_t kOutboxSoftLimit = size_t{64} * 1024;
constexpr size_t kOutboxHardLimit = size_t{1024} * 1024;

bool is_lossy_topic(const std::string &topic) {
    return topic == "rx.line" || topic == "waterfall.column" || topic == "audio.level";
}

/* Bounded so a runaway client cannot exhaust file descriptors. Generous: a
 * station has a UI, perhaps a shell, perhaps a remote viewer. */
constexpr size_t kMaxClients = 16;

/* poll() flags are signed ints in the system headers; mask as unsigned so the
 * bitwise tests below are unambiguous. */
constexpr unsigned kPollReadable =
    static_cast<unsigned>(POLLIN) | static_cast<unsigned>(POLLHUP) | static_cast<unsigned>(POLLERR);

/* Consume whatever has arrived from one client. Returns false when the
 * connection should be closed.
 *
 * Serving clients concurrently rather than one at a time is not a luxury: a
 * client that connects and never speaks would otherwise block every other
 * client forever, which on a Pi means the touchscreen UI locking out ssh. */
bool pump(Client &c) {
    char chunk[4096];
    ssize_t n = ::read(c.fd, chunk, sizeof chunk);
    if (n == 0) return false; /* client closed */
    if (n < 0) return errno == EINTR || errno == EAGAIN;
    c.buffer.append(chunk, static_cast<size_t>(n));

    /* An over-long line without a newline is refused rather than buffered
     * indefinitely. */
    if (c.buffer.size() > station::kMaxRequestBytes && c.buffer.find('\n') == std::string::npos) {
        std::string err =
            station::make_error(station::json::Value(), "invalid_request", "request too large",
                                "Send control messages under 64 kB, one per line.");
        err.push_back('\n');
        (void)::write(c.fd, err.data(), err.size());
        return false;
    }

    size_t start = 0, nl;
    while ((nl = c.buffer.find('\n', start)) != std::string::npos) {
        std::string line = c.buffer.substr(start, nl - start);
        start = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::string response = c.api->handle_line(line);
        if (response.empty()) continue; /* notification */
        response.push_back('\n');
        if (::write(c.fd, response.data(), response.size()) < 0) return false;
    }
    c.buffer.erase(0, start);
    return true;
}

void print_version() {
    std::printf("pocketsstvd %s\n", POCKETSSTV_VERSION);
    std::printf("  api       %d.%d\n", station::kApiMajor, station::kApiMinor);
    std::printf("  encoder   %s\n", SSTV_ENCODER_VERSION);
}

void print_usage() {
    std::printf("pocketsstvd - PocketSSTV station daemon\n\n"
                "Usage: pocketsstvd [options]\n\n"
                "  --socket PATH   Control socket (default: %s)\n"
                "  --audio BACKEND Audio backend: fake (default), none\n"
                "  --rig BACKEND   Rig backend: fake (default), none\n"
                "  --version       Print component versions\n"
                "  --help          This text\n",
                default_socket_path().c_str());
}

} // namespace

static int run(int argc, char **argv) {
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
        if (arg == "--socket")
            socket_path = next("--socket");
        else if (arg == "--audio")
            audio = next("--audio");
        else if (arg == "--rig")
            rig = next("--rig");
        else if (arg == "--version") {
            print_version();
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else {
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
            std::fprintf(stderr, "%s: unknown backend '%s' (available: fake, none)\n", pair.first,
                         pair.second.c_str());
            return 2;
        }
    }

    /* sigaction, not signal(): on BSD and macOS signal() installs handlers
     * with SA_RESTART, which restarts accept() after a signal, so the daemon
     * would never notice SIGTERM. Without SA_RESTART accept() returns EINTR
     * and the loop exits cleanly, which is what lets shutdown release the
     * socket — and, in later slices, the transmitter. */
    struct sigaction sa;
    std::memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
    std::signal(SIGPIPE, SIG_IGN); /* a client vanishing must not kill us */

    int lfd = listen_unix(socket_path);
    if (lfd < 0) return 1;
    g_socket_path = socket_path;
    std::atexit(cleanup_socket);

    std::fprintf(stderr, "pocketsstvd %s listening on %s (audio=%s rig=%s)\n", POCKETSSTV_VERSION,
                 socket_path.c_str(), audio.c_str(), rig.c_str());

    station::Station station;
    std::vector<std::unique_ptr<Client>> clients;

    /* Events go to whoever subscribed. Queued rather than written inline: a
     * client that has stopped reading must not block the decoder (ADR-0005
     * calls this back-pressure; day two keeps the queue simple and drops the
     * client if it grows absurd). */
    station.session.set_event_sink([&clients](const station::json::Value &event) {
        const std::string topic = event["event"].as_string();
        const bool lossy = is_lossy_topic(topic);
        std::string line;
        for (const auto &c : clients) {
            if (!c->api || !c->api->subscribed_to(topic)) continue;
            if (lossy && c->outbox.size() > kOutboxSoftLimit) {
                c->dropped++; /* behind: skip it */
                continue;
            }
            if (line.empty()) {
                line = event.dump();
                line.push_back('\n');
            }
            if (c->dropped > 0 && !lossy) {
                /* Tell the client what it missed, on the next event it does
                 * receive, rather than letting the gap pass unexplained. */
                station::json::Value annotated = event;
                annotated["dropped"] = static_cast<long long>(c->dropped);
                c->dropped = 0;
                c->outbox += annotated.dump();
                c->outbox.push_back('\n');
            } else {
                c->outbox += line;
            }
        }
    });

    while (!g_stop) {
        std::vector<pollfd> fds;
        fds.push_back({lfd, POLLIN, 0});
        for (const auto &c : clients)
            fds.push_back(
                {c->fd,
                 static_cast<short>(static_cast<unsigned>(POLLIN) |
                                    (c->outbox.empty() ? 0U : static_cast<unsigned>(POLLOUT))),
                 0});

        /* While audio is playing, poll must not block: there is decoding to
         * do between messages. */
        const bool busy = station.session.has_source();
        int ready = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), busy ? 0 : -1);
        if (ready < 0) {
            if (errno == EINTR) continue; /* a signal: check g_stop */
            std::perror("poll");
            break;
        }

        /* Move audio through the decoder. One block per iteration keeps the
         * loop responsive to clients while a recording plays. */
        if (busy) station.session.pump();

        /* Flush queued events. The sockets are non-blocking, so a client that
         * has stopped reading costs us one EAGAIN, not a stalled daemon. */
        for (size_t i = clients.size(); i-- > 0;) {
            Client &c = *clients[i];
            if (c.outbox.empty()) continue;
            const ssize_t w = ::write(c.fd, c.outbox.data(), c.outbox.size());
            if (w > 0)
                c.outbox.erase(0, static_cast<size_t>(w));
            else if (w < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                ::close(c.fd);
                clients.erase(clients.begin() + static_cast<long>(i));
                continue;
            }
            if (c.outbox.size() > kOutboxHardLimit) {
                std::fprintf(stderr, "dropping a client that stopped reading (%zu bytes queued)\n",
                             c.outbox.size());
                ::close(c.fd);
                clients.erase(clients.begin() + static_cast<long>(i));
            }
        }

        /* Existing clients first, so a new connection cannot starve them. */
        for (size_t i = clients.size(); i-- > 0;) {
            if ((static_cast<unsigned>(fds[i + 1].revents) & kPollReadable) == 0) continue;
            if (!pump(*clients[i])) {
                ::close(clients[i]->fd);
                clients.erase(clients.begin() + static_cast<long>(i));
            }
        }

        if (static_cast<unsigned>(fds[0].revents) & static_cast<unsigned>(POLLIN)) {
            int cfd = ::accept(lfd, nullptr, nullptr);
            if (cfd < 0) {
                if (errno != EINTR) std::perror("accept");
            } else if (clients.size() >= kMaxClients) {
                std::string err = station::make_error(
                    station::json::Value(), "too_many_clients", "too many clients connected",
                    "Close an existing connection and try again.");
                err.push_back('\n');
                (void)::write(cfd, err.data(), err.size());
                ::close(cfd);
            } else {
                /* Non-blocking: see the back-pressure note above. A blocking
                 * socket here means one silent client freezes the station. */
                const int flags = ::fcntl(cfd, F_GETFL, 0);
                const int nonblocking = flags < 0
                                            ? -1
                                            : static_cast<int>(static_cast<unsigned>(flags) |
                                                               static_cast<unsigned>(O_NONBLOCK));
                if (nonblocking < 0 || ::fcntl(cfd, F_SETFL, nonblocking) < 0) std::perror("fcntl");
                auto c = std::unique_ptr<Client>(new Client());
                c->fd = cfd;
                c->api.reset(new station::Api(&station));
                clients.push_back(std::move(c));
            }
        }
    }

    for (const auto &c : clients)
        ::close(c->fd);
    ::close(lfd);
    cleanup_socket();
    std::fprintf(stderr, "pocketsstvd: stopped\n");
    return 0;
}

/* std::string and the JSON types can throw, and an exception escaping main is
 * a crash with no explanation. Catch at the boundary so the operator gets a
 * message and a non-zero exit instead. */
int main(int argc, char **argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "pocketsstvd: internal error: %s\n", e.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "pocketsstvd: internal error\n");
        return 1;
    }
}
