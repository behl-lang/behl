#include "process_platform.hpp"

#if BEHL_PLATFORM_POSIX

#    include <cerrno>
#    include <cstring>
#    include <fcntl.h>
#    include <signal.h>
#    include <spawn.h>
#    include <stdexcept>
#    include <sys/wait.h>
#    include <system_error>
#    include <unistd.h>
#    include <vector>

extern "C" char** environ;

namespace behl::platform
{

    struct PipeHandles
    {
        int read = -1;
        int write = -1;
    };

    static PipeHandles create_pipe()
    {
        PipeHandles handles;
        int fds[2];
        if (pipe(fds) == -1)
        {
            throw std::system_error(errno, std::system_category(), "pipe failed");
        }
        handles.read = fds[0];
        handles.write = fds[1];
        return handles;
    }

    static void set_cloexec(int fd)
    {
        int flags = fcntl(fd, F_GETFD);
        if (flags == -1)
        {
            throw std::system_error(errno, std::system_category(), "fcntl F_GETFD failed");
        }
        if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
        {
            throw std::system_error(errno, std::system_category(), "fcntl F_SETFD failed");
        }
    }

    static void set_nonblocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1)
        {
            throw std::system_error(errno, std::system_category(), "fcntl F_GETFL failed");
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            throw std::system_error(errno, std::system_category(), "fcntl F_SETFL failed");
        }
    }

    ProcessHandle spawn(const std::string& command, std::span<const std::string> args, const SpawnOptions& options)
    {
        ProcessHandle handle;

        PipeHandles stdin_pipe{};
        PipeHandles stdout_pipe{};
        PipeHandles stderr_pipe{};

        posix_spawn_file_actions_t file_actions;
        posix_spawnattr_t attr;
        bool file_actions_init = false;
        bool attr_init = false;

        try
        {
            // Initialize spawn attributes
            if (posix_spawnattr_init(&attr) != 0)
            {
                throw std::system_error(errno, std::system_category(), "posix_spawnattr_init failed");
            }
            attr_init = true;

            // Initialize file actions
            if (posix_spawn_file_actions_init(&file_actions) != 0)
            {
                throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_init failed");
            }
            file_actions_init = true;

            // Change directory if specified
            if (!options.cwd.empty())
            {
#    ifdef __GLIBC__
                if (posix_spawn_file_actions_addchdir_np(&file_actions, options.cwd.c_str()) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_addchdir_np failed");
                }
#    else
                // On systems without addchdir_np, we need to handle this differently
                // For now, throw an error
                throw std::runtime_error("cwd option not supported on this platform");
#    endif
            }

            // Set up stdin
            if (options.stdin_mode == StdioMode::Pipe)
            {
                stdin_pipe = create_pipe();
                set_cloexec(stdin_pipe.write);
                if (posix_spawn_file_actions_adddup2(&file_actions, stdin_pipe.read, STDIN_FILENO) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_adddup2 failed");
                }
                if (posix_spawn_file_actions_addclose(&file_actions, stdin_pipe.read) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_addclose failed");
                }
            }
            else if (options.stdin_mode == StdioMode::Null)
            {
                if (posix_spawn_file_actions_addopen(&file_actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_addopen failed");
                }
            }

            // Set up stdout
            if (options.stdout_mode == StdioMode::Pipe)
            {
                stdout_pipe = create_pipe();
                set_cloexec(stdout_pipe.read);
                set_nonblocking(stdout_pipe.read);
                if (posix_spawn_file_actions_adddup2(&file_actions, stdout_pipe.write, STDOUT_FILENO) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_adddup2 failed");
                }
                if (posix_spawn_file_actions_addclose(&file_actions, stdout_pipe.write) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_addclose failed");
                }
            }
            else if (options.stdout_mode == StdioMode::Null)
            {
                if (posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_addopen failed");
                }
            }

            // Set up stderr
            if (options.stderr_mode == StdioMode::Pipe)
            {
                stderr_pipe = create_pipe();
                set_cloexec(stderr_pipe.read);
                set_nonblocking(stderr_pipe.read);
                if (posix_spawn_file_actions_adddup2(&file_actions, stderr_pipe.write, STDERR_FILENO) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_adddup2 failed");
                }
                if (posix_spawn_file_actions_addclose(&file_actions, stderr_pipe.write) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_addclose failed");
                }
            }
            else if (options.stderr_mode == StdioMode::Null)
            {
                if (posix_spawn_file_actions_addopen(&file_actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0) != 0)
                {
                    throw std::system_error(errno, std::system_category(), "posix_spawn_file_actions_addopen failed");
                }
            }

            // This is ugly but posix_spawnp wants argv as char*[], why? I don't know.
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(command.c_str()));
            for (const auto& arg : args)
            {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            // Build envp
            std::vector<std::string> env_storage;
            std::vector<char*> envp;

            if (options.env.has_value())
            {
                for (const auto& [key, value] : options.env.value())
                {
                    env_storage.push_back(key + "=" + value);
                }
                for (auto& str : env_storage)
                {
                    envp.push_back(str.data());
                }
                envp.push_back(nullptr);
            }

            pid_t pid;
            int result = posix_spawnp(
                &pid, command.c_str(), &file_actions, &attr, argv.data(), options.env.has_value() ? envp.data() : environ);

            if (result != 0)
            {
                throw std::system_error(result, std::system_category(), "posix_spawnp failed");
            }

            // Close child-side handles
            if (options.stdin_mode == StdioMode::Pipe)
            {
                close(stdin_pipe.read);
                handle.stdin_handle = stdin_pipe.write;
            }

            if (options.stdout_mode == StdioMode::Pipe)
            {
                close(stdout_pipe.write);
                handle.stdout_handle = stdout_pipe.read;
            }

            if (options.stderr_mode == StdioMode::Pipe)
            {
                close(stderr_pipe.write);
                handle.stderr_handle = stderr_pipe.read;
            }

            handle.pid = static_cast<int64_t>(pid);
            handle.is_valid = true;

            // Cleanup spawn attributes
            posix_spawn_file_actions_destroy(&file_actions);
            posix_spawnattr_destroy(&attr);

            return handle;
        }
        catch (...)
        {
            if (file_actions_init)
            {
                posix_spawn_file_actions_destroy(&file_actions);
            }
            if (attr_init)
            {
                posix_spawnattr_destroy(&attr);
            }

            if (stdin_pipe.read != -1)
            {
                close(stdin_pipe.read);
            }
            if (stdin_pipe.write != -1)
            {
                close(stdin_pipe.write);
            }
            if (stdout_pipe.read != -1)
            {
                close(stdout_pipe.read);
            }
            if (stdout_pipe.write != -1)
            {
                close(stdout_pipe.write);
            }
            if (stderr_pipe.read != -1)
            {
                close(stderr_pipe.read);
            }
            if (stderr_pipe.write != -1)
            {
                close(stderr_pipe.write);
            }
            if (handle.stdin_handle != -1)
            {
                close(handle.stdin_handle);
            }
            if (handle.stdout_handle != -1)
            {
                close(handle.stdout_handle);
            }
            if (handle.stderr_handle != -1)
            {
                close(handle.stderr_handle);
            }

            throw;
        }
    }

    static int get_exit_code(int status)
    {
        if (WIFEXITED(status))
        {
            return WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            return 128 + WTERMSIG(status);
        }
        return -1;
    }

    bool is_running(const ProcessHandle& handle)
    {
        if (!handle.is_valid)
        {
            return false;
        }

        // If we already know the exit code, process has exited
        if (handle.exit_code.has_value())
        {
            return false;
        }

        pid_t pid = static_cast<pid_t>(handle.pid);
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == 0)
        {
            // Process still running
            return true;
        }
        else if (result == pid)
        {
            // Process exited - get and cache exit code
            handle.exit_code = get_exit_code(status);
            return false;
        }
        else
        {
            // Error
            return false;
        }
    }

    int wait(ProcessHandle& handle)
    {
        if (!handle.is_valid)
        {
            return -1;
        }

        if (handle.exit_code.has_value())
        {
            return handle.exit_code.value();
        }

        pid_t pid = static_cast<pid_t>(handle.pid);
        int status;
        pid_t result = waitpid(pid, &status, 0);

        if (result == -1)
        {
            return -1;
        }

        // Compute and cache exit code
        int code = get_exit_code(status);
        handle.exit_code = code;
        return code;
    }

    bool kill(const ProcessHandle& handle, int signal_num)
    {
        if (!handle.is_valid)
        {
            return false;
        }

        pid_t pid = static_cast<pid_t>(handle.pid);
        int sig = (signal_num == 0) ? SIGTERM : signal_num;

        return ::kill(pid, sig) == 0;
    }

    size_t write_stdin(const ProcessHandle& handle, std::string_view data)
    {
        if (!handle.is_valid || handle.stdin_handle == -1)
        {
            return 0;
        }

        ssize_t written = write(handle.stdin_handle, data.data(), data.size());

        if (written == -1)
        {
            return 0;
        }

        return static_cast<size_t>(written);
    }

    std::string read_stdout(const ProcessHandle& handle, size_t max_bytes)
    {
        if (!handle.is_valid || handle.stdout_handle == -1)
        {
            return {};
        }

        std::string buffer(max_bytes, '\0');

        ssize_t bytes_read = read(handle.stdout_handle, buffer.data(), max_bytes);

        if (bytes_read <= 0)
        {
            return {};
        }

        buffer.resize(static_cast<size_t>(bytes_read));
        return buffer;
    }

    std::string read_stderr(const ProcessHandle& handle, size_t max_bytes)
    {
        if (!handle.is_valid || handle.stderr_handle == -1)
        {
            return {};
        }

        std::string buffer(max_bytes, '\0');

        ssize_t bytes_read = read(handle.stderr_handle, buffer.data(), max_bytes);

        if (bytes_read <= 0)
        {
            return {};
        }

        buffer.resize(static_cast<size_t>(bytes_read));
        return buffer;
    }

    void close_handle(ProcessHandle& handle)
    {
        if (handle.stdin_handle != -1)
        {
            close(handle.stdin_handle);
            handle.stdin_handle = -1;
        }

        if (handle.stdout_handle != -1)
        {
            close(handle.stdout_handle);
            handle.stdout_handle = -1;
        }

        if (handle.stderr_handle != -1)
        {
            close(handle.stderr_handle);
            handle.stderr_handle = -1;
        }

        handle.is_valid = false;
        handle.pid = -1;
    }

} // namespace behl::platform

#endif // BEHL_PLATFORM_POSIX
