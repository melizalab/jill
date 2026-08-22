/*
 * JILL - C++ framework for JACK
 *
 * Tests that remote logging degrades instead of stopping the process.
 *
 * A module connects its logger to an endpoint that jrecord may not have bound
 * yet. That is deliberate and useful: an orchestration script cannot guarantee
 * jrecord wins the race to start, and zeromq queues what is said meanwhile so
 * it arrives once the server does. What was not handled is the queue filling.
 * The sends were blocking, so a module running without a log server stopped
 * dead after about a thousand messages -- holding the logger's mutex, with
 * every other thread that logs piling up behind it.
 *
 * The logger is a singleton, and connect() is a one-way door, so this is one
 * long test case rather than several: it walks the whole sequence in order.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <zmq.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <string>

#include "jill/logging.hh"
#include "jill/logger.hh"

using namespace jill;

namespace {

/* The queue holds ZMQ_SNDHWM messages, 1000 by default. Overflow it by a
 * clear margin so the test does not depend on the exact figure. */
constexpr int OVERFLOW_BY = 500;
constexpr int HWM_GUESS = 1000;

std::string unique_server()
{
        return "jilltest_logger_" + std::to_string(::getpid());
}

/* The logger prints every message locally, which is the point -- but 1500 of
 * them would bury the test output, so send them somewhere else for the
 * duration. */
struct muted_stdout {
        int saved;
        /* Duplicate the real descriptor and put it back afterwards. Reopening
         * stdout on /dev/tty instead would lose doctest's own output whenever
         * the suite is run from a pipe, which is how the harness runs it. */
        explicit muted_stdout(std::string const & path)
                : saved(::dup(STDOUT_FILENO))
        {
                std::fflush(stdout);
                REQUIRE(saved >= 0);
                const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                REQUIRE(fd >= 0);
                ::dup2(fd, STDOUT_FILENO);
                ::close(fd);
        }
        ~muted_stdout()
        {
                std::fflush(stdout);
                ::dup2(saved, STDOUT_FILENO);
                ::close(saved);
        }
        muted_stdout(muted_stdout const &) = delete;
        muted_stdout & operator=(muted_stdout const &) = delete;
};

int count_in_file(std::string const & path, std::string const & needle)
{
        std::FILE * f = std::fopen(path.c_str(), "r");
        if (!f) return -1;
        int n = 0;
        char line[4096];
        while (std::fgets(line, sizeof line, f)) {
                if (std::string(line).find(needle) != std::string::npos) ++n;
        }
        std::fclose(f);
        return n;
}

} // namespace

TEST_CASE("logging without a server queues, degrades, and recovers") {
        const std::string server = unique_server();
        const std::string dir = "/tmp/org.meliza.jill/" + server;
        ::mkdir("/tmp/org.meliza.jill", 0755);
        ::mkdir(dir.c_str(), 0755);
        const std::string socket_path = dir + "/msg";
        const std::string endpoint = "ipc://" + socket_path;
        ::unlink(socket_path.c_str());

        const std::string local = dir + "/local.txt";

        logger::instance().set_sourcename("logtest");
        logger::instance().connect(server);

        SUBCASE("nothing blocks when the queue fills") {
                const auto start = std::chrono::steady_clock::now();
                {
                        muted_stdout mute(local);
                        for (int i = 0; i < HWM_GUESS + OVERFLOW_BY; ++i) {
                                LOG << "message " << i;
                        }
                }
                const auto elapsed = std::chrono::steady_clock::now() - start;

                /* The real assertion is that this returns at all -- before the
                 * fix it blocked forever and the suite timeout caught it. The
                 * bound turns a merely slow send into a failure too. */
                CHECK(elapsed < std::chrono::seconds(10));

                // every message still reaches the console
                CHECK(count_in_file(local, "message ") >= HWM_GUESS + OVERFLOW_BY);
                // and the degradation is announced once, not per message
                CHECK(count_in_file(local, "not keeping up") == 1);
        }

        SUBCASE("the backlog is delivered when a server appears, and sending resumes") {
                {
                        muted_stdout mute(local);
                        for (int i = 0; i < HWM_GUESS + OVERFLOW_BY; ++i) {
                                LOG << "before " << i;
                        }
                }

                void * ctx = zmq_ctx_new();
                void * router = zmq_socket(ctx, ZMQ_ROUTER);
                REQUIRE(zmq_bind(router, endpoint.c_str()) == 0);

                // give the connection time to establish and the backlog to move
                int backlog = 0;
                char buf[4096];
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                while (std::chrono::steady_clock::now() < deadline) {
                        if (zmq_recv(router, buf, sizeof buf, ZMQ_DONTWAIT) >= 0) ++backlog;
                        else if (backlog > 0) break;
                        else ::usleep(20000);
                }
                /* Messages said before the server existed are not lost: this is
                 * why the socket queues rather than refusing outright, and why
                 * a module may be started before jrecord. Four frames each,
                 * since the router prepends an identity. */
                CHECK(backlog >= 4 * HWM_GUESS);

                {
                        muted_stdout mute(local);
                        for (int i = 0; i < 5; ++i) LOG << "after " << i;
                }
                ::usleep(300000);

                int resumed = 0;
                while (zmq_recv(router, buf, sizeof buf, ZMQ_DONTWAIT) >= 0) ++resumed;
                CHECK(resumed == 4 * 5);
                // and the recovery is announced, with the count that was lost
                CHECK(count_in_file(local, "accepting messages again") == 1);

                zmq_close(router);
                zmq_ctx_term(ctx);
        }

        ::unlink(socket_path.c_str());
        ::unlink(local.c_str());
}
