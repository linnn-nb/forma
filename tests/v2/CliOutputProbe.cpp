// SPDX-License-Identifier: AGPL-3.0-only
// A protocol-only test program using the production channel, not a DAW/plugin.
#include "CliOutput.h"
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/wait.h>
#endif
namespace
{
struct TeardownLog
{
    ~TeardownLog()
    {
        std::puts("test teardown printf log");
    }
};
void exitLog()
{
    std::puts("test atexit printf log");
}
} // namespace
int main(int argc, char** argv)
{
    try
    {
#ifndef _WIN32
        if (argc == 4 && std::string(argv[1]) == "--child")
        {
            const auto dev = std::stoull(argv[2]), inode = std::stoull(argv[3]);
            for (int fd = 3; fd < 256; ++fd)
            {
                struct stat info{};
                if (::fstat(fd, &info) == 0 && uint64_t(info.st_dev) == dev && uint64_t(info.st_ino) == inode)
                    return 5;
            }
            std::puts("test inherited stdout child log");
            return 0;
        }
        struct stat original{};
        if (::fstat(STDOUT_FILENO, &original) != 0)
            throw std::runtime_error("test stdout unavailable");
#endif
        ndaw::v2::cli::Output channel;
        std::atexit(exitLog);
        const std::string mode = argc > 1 ? argv[1] : "normal";
        {
            TeardownLog teardown;
            std::cout << "test C++ ostream log\n";
            std::printf("test C stdio log\n");
#ifndef _WIN32
            const std::string raw = "test raw fd log\n";
            if (::write(STDOUT_FILENO, raw.data(), raw.size()) != ssize_t(raw.size()))
                throw std::runtime_error("test diagnostic write failed");
            const auto child = ::fork();
            if (child < 0)
                throw std::runtime_error("test child fork failed");
            if (!child)
            {
                const auto dev = std::to_string(uint64_t(original.st_dev)),
                           inode = std::to_string(uint64_t(original.st_ino));
                ::execl(argv[0], argv[0], "--child", dev.c_str(), inode.c_str(), nullptr);
                ::_exit(6);
            }
            int status = 0;
            if (::waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
                throw std::runtime_error("test child inherited the private result descriptor or failed");
#endif
            if (mode != "missing")
                channel.result({{"result", "passed"}, {"label", "真实 UTF-8 回执"}}, 0);
        }
        return channel.finish(mode == "mismatch" ? 3 : 0);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 74;
    }
}
