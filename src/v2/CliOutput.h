// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <json.hpp>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <iostream>
#include <optional>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace ndaw::v2::cli
{
// Only the CLI process owns this channel, before JUCE/Engine/plugin startup.
// fd 1 stays redirected to diagnostics through teardown and static destructors.
// The private original output is not inherited by plugin scan children. This
// contains ordinary library printing, not a malicious in-process plugin.
class Output final
{
public:
    Output()
    {
        std::cout.flush();
        std::fflush(stdout);
#ifdef _WIN32
        original = ::_dup(::_fileno(stdout));
        if (original < 0)
            throw std::runtime_error("CLI result stream is unavailable");
        if (!::SetHandleInformation(reinterpret_cast<HANDLE>(::_get_osfhandle(original)), HANDLE_FLAG_INHERIT, 0) ||
            ::_setmode(original, _O_BINARY) < 0 || ::_dup2(::_fileno(stderr), ::_fileno(stdout)) != 0 ||
            !::SetStdHandle(STD_OUTPUT_HANDLE, ::GetStdHandle(STD_ERROR_HANDLE)))
        {
            close();
            throw std::runtime_error("CLI diagnostic redirection failed");
        }
#else
        original = ::fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 3);
        if (original < 0)
            throw std::runtime_error("CLI result stream is unavailable");
        struct sigaction action{};
        action.sa_handler = SIG_IGN;
        sigemptyset(&action.sa_mask);
        if (::sigaction(SIGPIPE, &action, nullptr) < 0 || ::dup2(STDERR_FILENO, STDOUT_FILENO) < 0)
        {
            close();
            throw std::runtime_error("CLI diagnostic redirection failed");
        }
#endif
    }
    ~Output()
    {
        close();
    }
    Output(const Output&) = delete;
    Output& operator=(const Output&) = delete;
    void result(nlohmann::json value, int exitCode)
    {
        if (pending)
            throw std::runtime_error("CLI attempted to publish more than one result");
        pending = std::move(value);
        expectedExit = exitCode;
    }
    int finish(int exitCode)
    {
        if (!pending || exitCode != expectedExit)
        {
            pending = {{"status", "failed"}, {"error", "CLI stopped without a completed command result"}};
            if (!exitCode)
                exitCode = 1;
        }
        const auto bytes = pending->dump(2) + "\n";
        std::cout.flush();
        std::fflush(stdout);
        size_t at = 0;
        int interrupts = 0;
        while (at < bytes.size())
        {
#ifdef _WIN32
            const auto count =
                ::_write(original, bytes.data() + at, unsigned(std::min(size_t(INT_MAX), bytes.size() - at)));
#else
            const auto count = ::write(original, bytes.data() + at, std::min(size_t(INT_MAX), bytes.size() - at));
#endif
            if (count > 0)
            {
                at += size_t(count);
                interrupts = 0;
            }
            else if (count < 0 && errno == EINTR && ++interrupts <= 64)
                continue;
            else
            {
                std::cerr << "ndaw: CLI result write failed\n";
                return 74; // Failure publishing stdout is never successful command delivery.
            }
        }
        return exitCode;
    }

private:
    void close()
    {
        if (original >= 0)
        {
#ifdef _WIN32
            ::_close(original);
#else
            ::close(original);
#endif
            original = -1;
        }
    }
    int original = -1, expectedExit = 1;
    std::optional<nlohmann::json> pending;
};
} // namespace ndaw::v2::cli
