#include "behl.hpp"
#include "process/process_platform.hpp"
#include "state.hpp"

#include <csignal>

namespace behl
{

    constexpr uint32_t kProcessHandleUID = make_uid("process.Process");

#if BEHL_PLATFORM_WINDOWS
    // SIGKILL doesn't exist on Windows, use POSIX value
    constexpr int kSigKill = 9;
#else
    constexpr int kSigKill = SIGKILL;
#endif

    static int process_get_pid(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));

        if (!handle->is_valid)
        {
            push_boolean(S, false);
            push_string(S, "process handle is invalid");
            return 2;
        }

        push_integer(S, handle->pid);
        return 1;
    }

    static int process_is_running(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));

        if (!handle->is_valid)
        {
            push_boolean(S, false);
            return 1;
        }

        bool running = platform::is_running(*handle);
        push_boolean(S, running);
        return 1;
    }

    static int process_wait(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));

        if (!handle->is_valid)
        {
            push_boolean(S, false);
            push_string(S, "process handle is invalid");
            return 2;
        }

        int exit_code = platform::wait(*handle);
        push_integer(S, exit_code);
        return 1;
    }

    static int process_signal(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));
        Integer signal = check_integer(S, 1);

        if (!handle->is_valid)
        {
            push_boolean(S, false);
            push_string(S, "process handle is invalid");
            return 2;
        }

        bool success = platform::kill(*handle, static_cast<int>(signal));
        push_boolean(S, success);
        return 1;
    }

    static int process_kill(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));

        if (!handle->is_valid)
        {
            push_boolean(S, false);
            push_string(S, "process handle is invalid");
            return 2;
        }

        // Default to SIGKILL for force-kill
        bool success = platform::kill(*handle, kSigKill);
        push_boolean(S, success);
        return 1;
    }

    static int process_write(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));
        auto data = check_string(S, 1);

        if (!handle->is_valid)
        {
            push_boolean(S, false);
            push_string(S, "process handle is invalid");
            return 2;
        }

        if (!handle->stdin_handle)
        {
            push_boolean(S, false);
            push_string(S, "stdin not piped");
            return 2;
        }

        size_t written = platform::write_stdin(*handle, data);
        push_integer(S, static_cast<Integer>(written));
        return 1;
    }

    static int process_read(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));
        Integer max_bytes = get_top(S) > 1 ? check_integer(S, 1) : 4096;

        if (!handle->is_valid)
        {
            push_boolean(S, false);
            push_string(S, "process handle is invalid");
            return 2;
        }

        if (!handle->stdout_handle)
        {
            push_boolean(S, false);
            push_string(S, "stdout not piped");
            return 2;
        }

        std::string output = platform::read_stdout(*handle, static_cast<size_t>(max_bytes));
        push_string(S, output);
        return 1;
    }

    static int process_read_err(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));
        Integer max_bytes = get_top(S) > 1 ? check_integer(S, 1) : 4096;

        if (!handle->is_valid)
        {
            push_boolean(S, false);
            push_string(S, "process handle is invalid");
            return 2;
        }

        if (!handle->stderr_handle)
        {
            push_boolean(S, false);
            push_string(S, "stderr not piped");
            return 2;
        }

        std::string output = platform::read_stderr(*handle, static_cast<size_t>(max_bytes));
        push_string(S, output);
        return 1;
    }

    static int process_close(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));

        if (handle->is_valid)
        {
            platform::close_handle(*handle);
        }

        push_boolean(S, true);
        return 1;
    }

    static int process_gc(State* S)
    {
        auto* handle = static_cast<platform::ProcessHandle*>(check_userdata(S, 0, kProcessHandleUID));
        if (handle && handle->is_valid)
        {
            platform::close_handle(*handle);
        }
        return 0;
    }

    // process.spawn(command, args_table, options_table) -> process handle or false + error
    static int process_spawn(State* S)
    {
        auto command = check_string(S, 0);

        // Parse args (table)
        std::vector<std::string> args;
        if (get_top(S) > 1 && is_table(S, 1))
        {
            table_len(S, 1);
            Integer len = to_integer(S, -1);
            pop(S, 1);
            for (Integer i = 0; i < len; i++)
            {
                push_integer(S, i);
                table_rawget(S, 1);
                if (is_string(S, -1))
                {
                    args.push_back(std::string(to_string(S, -1)));
                }
                pop(S, 1);
            }
        }

        // Parse options (table)
        platform::SpawnOptions options;
        std::vector<std::pair<std::string, std::string>> env_storage;

        if (get_top(S) > 2 && is_table(S, 2))
        {
            // cwd
            push_string(S, "cwd");
            table_rawget(S, 2);
            if (is_string(S, -1))
            {
                options.cwd = std::string(to_string(S, -1));
            }
            pop(S, 1);

            // stdin
            push_string(S, "stdin");
            table_rawget(S, 2);
            if (is_string(S, -1))
            {
                auto mode = to_string(S, -1);
                if (mode == "pipe")
                {
                    options.stdin_mode = platform::StdioMode::Pipe;
                }
                else if (mode == "null")
                {
                    options.stdin_mode = platform::StdioMode::Null;
                }
                else
                {
                    options.stdin_mode = platform::StdioMode::Inherit;
                }
            }
            pop(S, 1);

            // stdout
            push_string(S, "stdout");
            table_rawget(S, 2);
            if (is_string(S, -1))
            {
                auto mode = to_string(S, -1);
                if (mode == "pipe")
                {
                    options.stdout_mode = platform::StdioMode::Pipe;
                }
                else if (mode == "null")
                {
                    options.stdout_mode = platform::StdioMode::Null;
                }
                else
                {
                    options.stdout_mode = platform::StdioMode::Inherit;
                }
            }
            pop(S, 1);

            // stderr
            push_string(S, "stderr");
            table_rawget(S, 2);
            if (is_string(S, -1))
            {
                auto mode = to_string(S, -1);
                if (mode == "pipe")
                {
                    options.stderr_mode = platform::StdioMode::Pipe;
                }
                else if (mode == "null")
                {
                    options.stderr_mode = platform::StdioMode::Null;
                }
                else
                {
                    options.stderr_mode = platform::StdioMode::Inherit;
                }
            }
            pop(S, 1);

            // env
            push_string(S, "env");
            table_rawget(S, 2);
            if (is_table(S, -1))
            {
                push_nil(S);
                while (table_next(S, -2))
                {
                    if (is_string(S, -2) && is_string(S, -1))
                    {
                        std::string key(to_string(S, -2));
                        std::string value(to_string(S, -1));
                        env_storage.emplace_back(std::move(key), std::move(value));
                    }
                    pop(S, 1);
                }

                options.env = std::move(env_storage);
            }
            pop(S, 1);
        }

        try
        {
            platform::ProcessHandle handle = platform::spawn(std::string(command), args, options);

            // Create userdata
            auto* proc = static_cast<platform::ProcessHandle*>(
                userdata_new(S, sizeof(platform::ProcessHandle), kProcessHandleUID));
            *proc = handle;

            // Get or create process metatable
            if (metatable_new(S, "process.Process"))
            {
                // First time - set up methods
                push_string(S, "get_pid");
                push_cfunction(S, process_get_pid);
                table_rawset(S, -3);

                push_string(S, "is_running");
                push_cfunction(S, process_is_running);
                table_rawset(S, -3);

                push_string(S, "wait");
                push_cfunction(S, process_wait);
                table_rawset(S, -3);

                push_string(S, "kill");
                push_cfunction(S, process_kill);
                table_rawset(S, -3);

                push_string(S, "signal");
                push_cfunction(S, process_signal);
                table_rawset(S, -3);

                push_string(S, "write");
                push_cfunction(S, process_write);
                table_rawset(S, -3);

                push_string(S, "read");
                push_cfunction(S, process_read);
                table_rawset(S, -3);

                push_string(S, "read_err");
                push_cfunction(S, process_read_err);
                table_rawset(S, -3);

                push_string(S, "close");
                push_cfunction(S, process_close);
                table_rawset(S, -3);

                push_string(S, "__gc");
                push_cfunction(S, process_gc);
                table_rawset(S, -3);

                push_string(S, "__index");
                dup(S, -2); // Copy the metatable itself
                table_rawset(S, -3);
            }

            // Attach metatable to userdata
            metatable_set(S, -2);

            return 1;
        }
        catch (const std::exception& e)
        {
            push_boolean(S, false);
            push_string(S, e.what());
            return 2;
        }
    }

    // process.exec(command, args_table, options_table) -> {stdout, stderr, exitcode} or false + error
    static int process_exec(State* S)
    {
        auto command = check_string(S, 0);

        // Parse args
        std::vector<std::string> args;
        if (get_top(S) > 1 && is_table(S, 1))
        {
            table_len(S, 1);
            Integer len = to_integer(S, -1);
            pop(S, 1);
            for (Integer i = 0; i < len; i++)
            {
                push_integer(S, i);
                table_rawget(S, 1);
                if (is_string(S, -1))
                {
                    args.push_back(std::string(to_string(S, -1)));
                }
                pop(S, 1);
            }
        }

        // Parse options
        platform::SpawnOptions options;
        options.stdout_mode = platform::StdioMode::Pipe;
        options.stderr_mode = platform::StdioMode::Pipe;
        std::vector<std::pair<std::string, std::string>> env_storage;

        if (get_top(S) > 2 && is_table(S, 2))
        {
            push_string(S, "cwd");
            table_rawget(S, 2);
            if (is_string(S, -1))
            {
                options.cwd = std::string(to_string(S, -1));
            }
            pop(S, 1);

            // env
            push_string(S, "env");
            table_rawget(S, 2);
            if (is_table(S, -1))
            {
                push_nil(S);
                while (table_next(S, -2))
                {
                    if (is_string(S, -2) && is_string(S, -1))
                    {
                        std::string key(to_string(S, -2));
                        std::string value(to_string(S, -1));
                        env_storage.emplace_back(std::move(key), std::move(value));
                    }
                    pop(S, 1);
                }

                if (!env_storage.empty())
                {
                    options.env = env_storage;
                }
            }
            pop(S, 1);
        }

        try
        {
            platform::ProcessHandle handle = platform::spawn(std::string(command), args, options);

            // Read all output
            std::string stdout_str;
            std::string stderr_str;

            while (platform::is_running(handle))
            {
                auto out = platform::read_stdout(handle, 4096);
                if (!out.empty())
                {
                    stdout_str += out;
                }

                auto err = platform::read_stderr(handle, 4096);
                if (!err.empty())
                {
                    stderr_str += err;
                }
            }

            // Read any remaining output
            while (true)
            {
                auto out = platform::read_stdout(handle, 4096);
                if (out.empty())
                {
                    break;
                }
                stdout_str += out;
            }

            while (true)
            {
                auto err = platform::read_stderr(handle, 4096);
                if (err.empty())
                {
                    break;
                }
                stderr_str += err;
            }

            int exit_code = platform::wait(handle);
            platform::close_handle(handle);

            // Return result table
            table_new(S);

            push_string(S, "stdout");
            push_string(S, stdout_str);
            table_rawset(S, -3);

            push_string(S, "stderr");
            push_string(S, stderr_str);
            table_rawset(S, -3);

            push_string(S, "exitcode");
            push_integer(S, exit_code);
            table_rawset(S, -3);

            return 1;
        }
        catch (const std::exception& e)
        {
            push_boolean(S, false);
            push_string(S, e.what());
            return 2;
        }
    }

    // process.platform() -> "windows" or "linux" or "darwin"
    static int process_platform(State* S)
    {
#if BEHL_PLATFORM_WINDOWS
        push_string(S, "windows");
#elif defined(__APPLE__)
        push_string(S, "darwin");
#else
        push_string(S, "linux");
#endif
        return 1;
    }

    void load_lib_process(State* S)
    {
        static constexpr ModuleReg process_funcs[] = {
            { "spawn", process_spawn },
            { "exec", process_exec },
            { "platform", process_platform },
        };

        // Signal constants - use actual OS-defined values from <csignal>
        static constexpr ModuleConst process_consts[] = {
#if BEHL_PLATFORM_POSIX
            { "SIGHUP", SIGHUP },     // Hangup
            { "SIGINT", SIGINT },     // Interrupt (Ctrl+C)
            { "SIGQUIT", SIGQUIT },   // Quit
            { "SIGILL", SIGILL },     // Illegal instruction
            { "SIGTRAP", SIGTRAP },   // Trace trap
            { "SIGABRT", SIGABRT },   // Abort
            { "SIGBUS", SIGBUS },     // Bus error
            { "SIGFPE", SIGFPE },     // Floating point exception
            { "SIGKILL", SIGKILL },   // Kill (cannot be caught)
            { "SIGUSR1", SIGUSR1 },   // User-defined signal 1
            { "SIGSEGV", SIGSEGV },   // Segmentation fault
            { "SIGUSR2", SIGUSR2 },   // User-defined signal 2
            { "SIGPIPE", SIGPIPE },   // Broken pipe
            { "SIGALRM", SIGALRM },   // Alarm clock
            { "SIGTERM", SIGTERM },   // Termination (default)
            { "SIGCHLD", SIGCHLD },   // Child status changed
            { "SIGCONT", SIGCONT },   // Continue if stopped
            { "SIGSTOP", SIGSTOP },   // Stop (cannot be caught)
            { "SIGTSTP", SIGTSTP },   // Terminal stop
            { "SIGTTIN", SIGTTIN },   // Terminal input
            { "SIGTTOU", SIGTTOU },   // Terminal output
            { "SIGURG", SIGURG },     // Urgent condition on socket
            { "SIGXCPU", SIGXCPU },   // CPU time limit exceeded
            { "SIGXFSZ", SIGXFSZ },   // File size limit exceeded
            { "SIGWINCH", SIGWINCH }, // Window size change
#else
            // Windows: Interprocess signals are not really supported, we just use TerminateProcess and emulate posix exit
            // codes.
            { "SIGINT", SIGINT },     // Interrupt (Ctrl+C) - Windows supports this
            { "SIGILL", SIGILL },     // Illegal instruction
            { "SIGABRT", SIGABRT },   // Abort
            { "SIGFPE", SIGFPE },     // Floating point exception
            { "SIGSEGV", SIGSEGV },   // Segmentation fault
            { "SIGTERM", SIGTERM },   // Termination
            { "SIGBREAK", SIGBREAK }, // Ctrl+Break (Windows-specific, defined in signal.h)
            { "SIGKILL", 9 },         // Not a real Windows signal, just convention (POSIX value)
#endif
        };

        ModuleDef process_module = { .funcs = process_funcs, .consts = process_consts };

        create_module(S, "process", process_module);
    }

} // namespace behl
