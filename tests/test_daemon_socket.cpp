// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// Socket-level tests for pocketsstvd: the lifecycle behaviour that unit tests
// cannot see.
//
// Every case here exists because the behaviour was wrong first:
//   - an idle client used to block every other client forever;
//   - a second daemon used to steal the socket from a running one;
//   - SIGTERM used to be ignored, because signal() installs handlers with
//     SA_RESTART on BSD and accept() simply restarted.
//
// The daemon binary is exec'd, so this exercises the real program.

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <csignal>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace {

int g_failures = 0;

void check(bool ok, const char *what) {
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) g_failures++;
}

void sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, static_cast<long>(ms % 1000) * 1000000L};
    nanosleep(&ts, nullptr);
}

int connect_to(const std::string &path, int timeout_ms = 1000) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_un addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof addr.sun_path, "%s", path.c_str());
    if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0) {
        ::close(fd);
        return -1;
    }
    struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    return fd;
}

/* Send a request and read one reply line, or "" if nothing arrives in time. */
std::string request(int fd, const std::string &line) {
    std::string out = line + "\n";
    if (::write(fd, out.data(), out.size()) < 0) return "";
    std::string reply;
    char c;
    while (reply.size() < 8192) {
        ssize_t n = ::read(fd, &c, 1);
        if (n <= 0) break;
        if (c == '\n') break;
        reply.push_back(c);
    }
    return reply;
}

const char *kHello = R"({"jsonrpc":"2.0","id":1,"method":"hello","params":{"api":"1.0"}})";

pid_t start_daemon(const char *exe, const std::string &socket_path) {
    pid_t pid = fork();
    if (pid == 0) {
        /* The daemon's own chatter would clutter the test output. */
        int devnull = ::open("/dev/null", O_WRONLY);
        if (devnull >= 0) { ::dup2(devnull, STDERR_FILENO); ::close(devnull); }
        execl(exe, exe, "--socket", socket_path.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }
    /* Wait for the socket to accept connections rather than sleeping blindly. */
    for (int i = 0; i < 100; i++) {
        int fd = connect_to(socket_path, 200);
        if (fd >= 0) { ::close(fd); return pid; }
        sleep_ms(20);
    }
    return pid;
}

} // namespace

int main(int argc, char **argv) {
    const char *exe = argc > 1 ? argv[1] : "./bin/pocketsstvd";
    const char *audio_file =
        argc > 2 ? argv[2] : "tests/audio/alt5_test_panel_r36.wav";
    /* mkdtemp, not mktemp: a private directory avoids both the collision and
     * the symlink race that make mktemp unsafe. */
    char tmpl[] = "/tmp/pocketsstvd_test_XXXXXX";
    const char *dir = mkdtemp(tmpl);
    if (!dir) { std::perror("mkdtemp"); return 1; }
    const std::string socket_path = std::string(dir) + "/d.sock";

    std::printf("Daemon socket tests (%s)\n", exe);
    pid_t pid = start_daemon(exe, socket_path);

    int first = connect_to(socket_path);
    check(first >= 0, "a client can connect");
    check(request(first, kHello).find("\"result\"") != std::string::npos,
          "the client negotiates a version");

    /* The regression that matters most: a silent client must not deny service
     * to anybody else. */
    int idle = connect_to(socket_path);
    check(idle >= 0, "a second client connects while the first is attached");
    int third = connect_to(socket_path);
    check(!request(third, kHello).empty(),
          "a client is served while another sits idle and silent");

    /* Sessions are per connection: negotiation by one client must not admit
     * another. */
    int unnegotiated = connect_to(socket_path);
    check(request(unnegotiated, R"({"jsonrpc":"2.0","id":2,"method":"system.info"})")
              .find("not_negotiated") != std::string::npos,
          "each connection negotiates for itself");

    /* A second daemon must refuse rather than unlink a live socket and take
     * it over, leaving the original orphaned. */
    {
        std::string cmd = std::string(exe) + " --socket " + socket_path + " 2>/dev/null";
        int rc = std::system(cmd.c_str());
        check(rc != 0, "a second daemon refuses to hijack a live socket");
        check(!request(first, R"({"jsonrpc":"2.0","id":3,"method":"system.info"})").empty(),
              "the original daemon still answers afterwards");
    }

    for (int fd : {first, idle, third, unnegotiated}) if (fd >= 0) ::close(fd);

    /* A subscriber that stops reading must not stall the station.
     *
     * The sockets were blocking at first, so one silent subscriber deadlocked
     * the daemon: the decoder stopped and every other client starved. */
    {
        int stalled = connect_to(socket_path);
        /* A small receive buffer makes it fill quickly. */
        int rcvbuf = 2048;
        ::setsockopt(stalled, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);
        request(stalled, kHello);
        request(stalled,
                R"({"jsonrpc":"2.0","id":4,"method":"events.subscribe","params":{"topics":["*"]}})");
        /* ...and from here it reads nothing at all. */

        int worker = connect_to(socket_path, 5000);
        request(worker, kHello);
        const std::string feed =
            R"({"jsonrpc":"2.0","id":5,"method":"dev.feed","params":{"file":")" +
            std::string(audio_file) + R"("}})";
        const std::string reply = request(worker, feed);
        check(reply.find("\"result\"") != std::string::npos,
              "a recording can be fed while a subscriber is stalled");

        bool completed = false;
        for (int i = 0; i < 200 && !completed; i++) {
            const std::string status =
                request(worker, R"({"jsonrpc":"2.0","id":6,"method":"session.status"})");
            if (status.find("\"pictures_completed\":0") == std::string::npos &&
                status.find("pictures_completed") != std::string::npos)
                completed = true;
            else
                sleep_ms(50);
        }
        check(completed, "the picture completes despite the stalled subscriber");
        ::close(stalled);
        ::close(worker);
    }

    /* SIGTERM must stop the daemon and take the socket with it, or systemd
     * would wait out its timeout on every restart. */
    ::kill(pid, SIGTERM);
    bool exited = false;
    for (int i = 0; i < 100; i++) {
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == pid) { exited = true; break; }
        sleep_ms(20);
    }
    check(exited, "the daemon exits on SIGTERM");
    if (!exited) ::kill(pid, SIGKILL);

    struct stat st;
    check(::stat(socket_path.c_str(), &st) != 0, "the socket is removed on exit");

    /* And a stale socket left by a crash must not block the next start. */
    {
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr;
        std::memset(&addr, 0, sizeof addr);
        addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof addr.sun_path, "%s", socket_path.c_str());
        ::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr);
        ::close(fd);                       /* a socket file with nobody behind it */

        pid_t p2 = start_daemon(exe, socket_path);
        int fd2 = connect_to(socket_path);
        check(fd2 >= 0, "a stale socket does not prevent a restart");
        if (fd2 >= 0) ::close(fd2);
        ::kill(p2, SIGTERM);
        waitpid(p2, nullptr, 0);
    }
    ::unlink(socket_path.c_str());
    ::rmdir(dir);

    std::printf("\n%s (%d failures)\n", g_failures ? "FAILED" : "ALL PASSED", g_failures);
    return g_failures ? 1 : 0;
}
