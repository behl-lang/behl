#pragma once

#include "platform.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace behl::platform
{

    enum class StdioMode
    {
        Inherit, // Child inherits parent's stdio
        Pipe,    // Create pipes for communication
        Null     // Redirect to /dev/null or NUL
    };

    struct SpawnOptions
    {
        std::string cwd;
        StdioMode stdin_mode = StdioMode::Inherit;
        StdioMode stdout_mode = StdioMode::Inherit;
        StdioMode stderr_mode = StdioMode::Inherit;
        std::optional<std::span<const std::pair<std::string, std::string>>> env;
    };

    struct ProcessHandle
    {
        int64_t pid = -1;
#if BEHL_PLATFORM_WINDOWS
        void* process_handle = nullptr;
        void* stdin_handle = nullptr;
        void* stdout_handle = nullptr;
        void* stderr_handle = nullptr;
#else // POSIX
        int stdin_handle = -1;
        int stdout_handle = -1;
        int stderr_handle = -1;
        // POSIX requires caching: waitpid() reaps the process, so we must save exit code
        mutable std::optional<int> exit_code;
#endif
        bool is_valid = false;
    };

    ProcessHandle spawn(const std::string& command, std::span<const std::string> args, const SpawnOptions& options);
    bool is_running(const ProcessHandle& handle);
    int wait(ProcessHandle& handle);
    bool kill(const ProcessHandle& handle, int signal);
    size_t write_stdin(const ProcessHandle& handle, std::string_view data);
    std::string read_stdout(const ProcessHandle& handle, size_t max_bytes);
    std::string read_stderr(const ProcessHandle& handle, size_t max_bytes);
    void close_handle(ProcessHandle& handle);

} // namespace behl::platform
