#include "process_platform.hpp"

#if BEHL_PLATFORM_WINDOWS

#    include <csignal>
#    include <stdexcept>
#    include <system_error>

#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>

namespace behl::platform
{
    // POSIX exit code = 128 + signal number
    constexpr int kPosixSignalExitCodeOffset = 128;

    // SIGKILL doesn't exist on Windows, use POSIX value
    constexpr int kSigKill = 9;

    struct PipeHandles
    {
        HANDLE read = INVALID_HANDLE_VALUE;
        HANDLE write = INVALID_HANDLE_VALUE;
    };

    static PipeHandles create_pipe()
    {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        PipeHandles handles;
        if (!CreatePipe(&handles.read, &handles.write, &sa, 0))
        {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CreatePipe failed");
        }
        return handles;
    }

    static void set_handle_inheritance(HANDLE handle, bool inherit)
    {
        if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, inherit ? HANDLE_FLAG_INHERIT : 0))
        {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "SetHandleInformation failed");
        }
    }

    static std::string build_command_line(const std::string& command, std::span<const std::string> args)
    {
        std::string cmdline = command;
        for (const auto& arg : args)
        {
            cmdline += " ";
            // Simple quoting - wrap in quotes if contains spaces
            if (arg.find(' ') != std::string::npos)
            {
                cmdline += "\"" + arg + "\"";
            }
            else
            {
                cmdline += arg;
            }
        }
        return cmdline;
    }

    ProcessHandle spawn(const std::string& command, std::span<const std::string> args, const SpawnOptions& options)
    {
        ProcessHandle handle;

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;

        PipeHandles stdin_pipe{};
        PipeHandles stdout_pipe{};
        PipeHandles stderr_pipe{};

        try
        {
            // Set up stdin
            if (options.stdin_mode == StdioMode::Pipe)
            {
                stdin_pipe = create_pipe();
                si.hStdInput = stdin_pipe.read;
                set_handle_inheritance(stdin_pipe.write, false); // Parent side non-inheritable
                handle.stdin_handle = static_cast<void*>(stdin_pipe.write);
            }
            else if (options.stdin_mode == StdioMode::Null)
            {
                si.hStdInput = CreateFileA(
                    "NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            }
            else
            {
                si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            }

            // Set up stdout
            if (options.stdout_mode == StdioMode::Pipe)
            {
                stdout_pipe = create_pipe();
                si.hStdOutput = stdout_pipe.write;
                set_handle_inheritance(stdout_pipe.read, false); // Parent side non-inheritable
                handle.stdout_handle = static_cast<void*>(stdout_pipe.read);
            }
            else if (options.stdout_mode == StdioMode::Null)
            {
                si.hStdOutput = CreateFileA(
                    "NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            }
            else
            {
                si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
            }

            // Set up stderr
            if (options.stderr_mode == StdioMode::Pipe)
            {
                stderr_pipe = create_pipe();
                si.hStdError = stderr_pipe.write;
                set_handle_inheritance(stderr_pipe.read, false); // Parent side non-inheritable
                handle.stderr_handle = static_cast<void*>(stderr_pipe.read);
            }
            else if (options.stderr_mode == StdioMode::Null)
            {
                si.hStdError = CreateFileA(
                    "NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            }
            else
            {
                si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
            }

            PROCESS_INFORMATION pi{};
            std::string cmdline = build_command_line(command, args);

            const char* cwd_ptr = options.cwd.empty() ? nullptr : options.cwd.c_str();

            // Build environment block
            std::string env_block;
            void* env_ptr = nullptr;

            if (options.env.has_value())
            {
                for (const auto& [key, value] : options.env.value())
                {
                    env_block += key + "=" + value + '\0';
                }
                env_block += '\0';
                env_ptr = env_block.data();
            }

            if (!CreateProcessA(
                    nullptr, const_cast<char*>(cmdline.c_str()), nullptr, nullptr, TRUE, 0, env_ptr, cwd_ptr, &si, &pi))
            {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CreateProcess failed");
            }

            // Close child-side handles
            if (options.stdin_mode == StdioMode::Pipe)
            {
                CloseHandle(stdin_pipe.read);
            }
            else if (options.stdin_mode == StdioMode::Null)
            {
                CloseHandle(si.hStdInput);
            }

            if (options.stdout_mode == StdioMode::Pipe)
            {
                CloseHandle(stdout_pipe.write);
            }
            else if (options.stdout_mode == StdioMode::Null)
            {
                CloseHandle(si.hStdOutput);
            }

            if (options.stderr_mode == StdioMode::Pipe)
            {
                CloseHandle(stderr_pipe.write);
            }
            else if (options.stderr_mode == StdioMode::Null)
            {
                CloseHandle(si.hStdError);
            }

            CloseHandle(pi.hThread);

            handle.pid = static_cast<int64_t>(pi.dwProcessId);
            handle.process_handle = static_cast<void*>(pi.hProcess);
            handle.is_valid = true;

            return handle;
        }
        catch (...)
        {
            // Cleanup on failure
            if (stdin_pipe.read != INVALID_HANDLE_VALUE)
            {
                CloseHandle(stdin_pipe.read);
            }
            if (stdin_pipe.write != INVALID_HANDLE_VALUE)
            {
                CloseHandle(stdin_pipe.write);
            }
            if (stdout_pipe.read != INVALID_HANDLE_VALUE)
            {
                CloseHandle(stdout_pipe.read);
            }
            if (stdout_pipe.write != INVALID_HANDLE_VALUE)
            {
                CloseHandle(stdout_pipe.write);
            }
            if (stderr_pipe.read != INVALID_HANDLE_VALUE)
            {
                CloseHandle(stderr_pipe.read);
            }
            if (stderr_pipe.write != INVALID_HANDLE_VALUE)
            {
                CloseHandle(stderr_pipe.write);
            }

            if (handle.stdin_handle)
            {
                CloseHandle(static_cast<HANDLE>(handle.stdin_handle));
            }
            if (handle.stdout_handle)
            {
                CloseHandle(static_cast<HANDLE>(handle.stdout_handle));
            }
            if (handle.stderr_handle)
            {
                CloseHandle(static_cast<HANDLE>(handle.stderr_handle));
            }
            if (handle.process_handle)
            {
                CloseHandle(static_cast<HANDLE>(handle.process_handle));
            }

            throw;
        }
    }

    bool is_running(const ProcessHandle& handle)
    {
        if (!handle.is_valid || !handle.process_handle)
        {
            return false;
        }

        HANDLE proc = static_cast<HANDLE>(handle.process_handle);
        DWORD exit_code = 0;

        if (!GetExitCodeProcess(proc, &exit_code))
        {
            return false;
        }

        return exit_code == STILL_ACTIVE;
    }

    int wait(ProcessHandle& handle)
    {
        if (!handle.is_valid || !handle.process_handle)
        {
            return -1;
        }

        HANDLE proc = static_cast<HANDLE>(handle.process_handle);
        WaitForSingleObject(proc, INFINITE);

        DWORD exit_code = 0;
        GetExitCodeProcess(proc, &exit_code);

        return static_cast<int>(exit_code);
    }

    bool kill(const ProcessHandle& handle, int signal)
    {
        if (!handle.is_valid || !handle.process_handle)
        {
            return false;
        }

        HANDLE proc = static_cast<HANDLE>(handle.process_handle);
        DWORD process_id = GetProcessId(proc);

        // Try to emulate SIGINT and SIGBREAK with console events (graceful)
        // POSIX signal values: SIGINT=2, Windows SIGBREAK=21
        if (signal == 2) // SIGINT
        {
            // Try to send Ctrl+C event for graceful shutdown
            // This only works if the process has a console and we share the console group
            if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, process_id))
            {
                return true;
            }
            // Fall through to TerminateProcess if console event fails
        }
        else if (signal == 21) // SIGBREAK (Windows-specific)
        {
            // Try to send Ctrl+Break event
            if (GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, process_id))
            {
                return true;
            }
            // Fall through to TerminateProcess if console event fails
        }

        // For all other signals, or if console events fail, force terminate
        // Use POSIX exit code convention: 128 + signal_number
        UINT exit_code = (signal != 0) ? static_cast<UINT>(kPosixSignalExitCodeOffset + signal)
                                       : static_cast<UINT>(kPosixSignalExitCodeOffset + kSigKill);

        return TerminateProcess(proc, exit_code) != 0;
    }

    size_t write_stdin(const ProcessHandle& handle, std::string_view data)
    {
        if (!handle.is_valid || !handle.stdin_handle)
        {
            return 0;
        }

        HANDLE pipe = static_cast<HANDLE>(handle.stdin_handle);
        DWORD written = 0;

        if (!WriteFile(pipe, data.data(), static_cast<DWORD>(data.size()), &written, nullptr))
        {
            return 0;
        }

        return static_cast<size_t>(written);
    }

    std::string read_stdout(const ProcessHandle& handle, size_t max_bytes)
    {
        if (!handle.is_valid || !handle.stdout_handle)
        {
            return {};
        }

        HANDLE pipe = static_cast<HANDLE>(handle.stdout_handle);

        // Check if data is available
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available == 0)
        {
            return {};
        }

        size_t to_read = (available < max_bytes) ? available : max_bytes;
        std::string buffer(to_read, '\0');
        DWORD read = 0;

        if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(to_read), &read, nullptr))
        {
            return {};
        }

        buffer.resize(static_cast<size_t>(read));
        return buffer;
    }

    std::string read_stderr(const ProcessHandle& handle, size_t max_bytes)
    {
        if (!handle.is_valid || !handle.stderr_handle)
        {
            return {};
        }

        HANDLE pipe = static_cast<HANDLE>(handle.stderr_handle);

        // Check if data is available
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available == 0)
        {
            return {};
        }

        size_t to_read = (available < max_bytes) ? available : max_bytes;
        std::string buffer(to_read, '\0');
        DWORD read = 0;

        if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(to_read), &read, nullptr))
        {
            return {};
        }

        buffer.resize(static_cast<size_t>(read));
        return buffer;
    }

    void close_handle(ProcessHandle& handle)
    {
        if (handle.process_handle)
        {
            HANDLE proc = static_cast<HANDLE>(handle.process_handle);
            CloseHandle(proc);
            handle.process_handle = nullptr;
        }

        if (handle.stdin_handle)
        {
            HANDLE pipe = static_cast<HANDLE>(handle.stdin_handle);
            CloseHandle(pipe);
            handle.stdin_handle = nullptr;
        }

        if (handle.stdout_handle)
        {
            HANDLE pipe = static_cast<HANDLE>(handle.stdout_handle);
            CloseHandle(pipe);
            handle.stdout_handle = nullptr;
        }

        if (handle.stderr_handle)
        {
            HANDLE pipe = static_cast<HANDLE>(handle.stderr_handle);
            CloseHandle(pipe);
            handle.stderr_handle = nullptr;
        }

        handle.is_valid = false;
        handle.pid = -1;
    }

} // namespace behl::platform

#endif // BEHL_PLATFORM_WINDOWS
