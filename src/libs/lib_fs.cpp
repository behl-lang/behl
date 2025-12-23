#define _CRT_SECURE_NO_WARNINGS

#include "behl.hpp"
#include "state.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <system_error>

namespace fs = std::filesystem;

namespace behl
{

    constexpr uint32_t kFileHandleUID = make_uid("fs.File");

    static int file_read(State* S)
    {
        auto* stream = static_cast<std::fstream*>(check_userdata(S, 0, kFileHandleUID));
        Integer size = check_integer(S, 1);

        if (!stream->is_open())
        {
            push_boolean(S, false);
            push_string(S, "file is closed");
            return 2;
        }

        if (size < 0)
        {
            push_boolean(S, false);
            push_string(S, "invalid size");
            return 2;
        }

        char* buffer = mem_alloc_array<char>(S, static_cast<size_t>(size));
        stream->read(buffer, static_cast<std::streamsize>(size));

        std::streamsize bytes_read = stream->gcount();

        push_string(S, std::string_view(buffer, static_cast<size_t>(bytes_read)));
        push_integer(S, static_cast<Integer>(bytes_read));

        mem_free_array<char>(S, buffer, static_cast<size_t>(size));

        return 2;
    }

    static int file_write(State* S)
    {
        auto* stream = static_cast<std::fstream*>(check_userdata(S, 0, kFileHandleUID));
        auto data = check_string(S, 1);

        if (!stream->is_open())
        {
            push_boolean(S, false);
            push_string(S, "file is closed");
            return 2;
        }

        stream->write(data.data(), static_cast<std::streamsize>(data.size()));
        if (stream->fail())
        {
            push_boolean(S, false);
            push_string(S, "failed");
            return 2;
        }

        push_boolean(S, true);
        return 1;
    }

    static int file_close(State* S)
    {
        auto* stream = static_cast<std::fstream*>(check_userdata(S, 0, kFileHandleUID));

        if (stream->is_open())
        {
            stream->close();
        }

        push_boolean(S, true);
        return 1;
    }

    static int file_seek(State* S)
    {
        auto* stream = static_cast<std::fstream*>(check_userdata(S, 0, kFileHandleUID));
        auto whence = check_string(S, 1);
        Integer offset = get_top(S) > 2 ? check_integer(S, 2) : 0;

        if (!stream->is_open())
        {
            push_boolean(S, false);
            push_string(S, "file is closed");
            return 2;
        }

        std::ios::seekdir dir;
        if (whence == "set")
        {
            dir = std::ios::beg;
        }
        else if (whence == "cur")
        {
            dir = std::ios::cur;
        }
        else if (whence == "end")
        {
            dir = std::ios::end;
        }
        else
        {
            push_boolean(S, false);
            push_string(S, "invalid whence (use 'set', 'cur', or 'end')");
            return 2;
        }

        stream->seekg(offset, dir);
        stream->seekp(offset, dir);

        std::streampos pos = stream->tellg();
        push_integer(S, static_cast<Integer>(pos));
        return 1;
    }

    static int file_gc(State* S)
    {
        auto* stream = static_cast<std::fstream*>(check_userdata(S, 0, kFileHandleUID));
        if (stream)
        {
            if (stream->is_open())
            {
                stream->close();
            }
            std::destroy_at(stream);
        }
        return 0;
    }

    // fs.open(path, mode) -> file handle or false + error
    static int fs_open(State* S)
    {
        auto path_sv = check_string(S, 0);
        auto mode_sv = check_string(S, 1);

        fs::path path = fs::path{ path_sv };

        // Parse mode
        std::ios::openmode open_mode = std::ios::binary;

        if (mode_sv == "r")
        {
            open_mode |= std::ios::in;
        }
        else if (mode_sv == "w")
        {
            open_mode |= std::ios::out | std::ios::trunc;
        }
        else if (mode_sv == "a")
        {
            open_mode |= std::ios::out | std::ios::app;
        }
        else if (mode_sv == "r+")
        {
            open_mode |= std::ios::in | std::ios::out;
        }
        else if (mode_sv == "w+")
        {
            open_mode |= std::ios::in | std::ios::out | std::ios::trunc;
        }
        else if (mode_sv == "a+")
        {
            open_mode |= std::ios::in | std::ios::out | std::ios::app;
        }
        else
        {
            push_boolean(S, false);
            push_string(S, "invalid mode (use 'r', 'w', 'a', 'r+', 'w+', or 'a+')");
            return 2;
        }

        std::fstream stream;
        stream.open(path, open_mode);
        if (!stream.is_open())
        {
            push_boolean(S, false);

            if (stream.fail())
            {
                push_string(S, std::strerror(errno));
            }
            else
            {
                push_string(S, "failed to open file");
            }
            return 2;
        }

        auto* fstream_ptr = static_cast<std::fstream*>(userdata_new(S, sizeof(std::fstream), kFileHandleUID));
        std::construct_at(fstream_ptr, std::move(stream));

        // Get or create file metatable
        if (metatable_new(S, "fs.File"))
        {
            // First time - set up methods
            push_string(S, "read");
            push_cfunction(S, file_read);
            table_rawset(S, -3);

            push_string(S, "write");
            push_cfunction(S, file_write);
            table_rawset(S, -3);

            push_string(S, "close");
            push_cfunction(S, file_close);
            table_rawset(S, -3);

            push_string(S, "seek");
            push_cfunction(S, file_seek);
            table_rawset(S, -3);

            push_string(S, "__gc");
            push_cfunction(S, file_gc);
            table_rawset(S, -3);

            push_string(S, "__index");
            dup(S, -2); // Copy the metatable itself
            table_rawset(S, -3);
        }

        // Attach metatable to userdata
        metatable_set(S, -2);

        return 1;
    }

    // fs.read(path) -> content on success, false + error on failure
    static int fs_read(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        if (!fs::exists(path, ec) || ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            push_boolean(S, false);
            if (file.fail())
            {
                push_string(S, std::strerror(errno));
            }
            else
            {
                push_string(S, "failed to open file");
            }
            return 2;
        }

        file.seekg(0, std::ios::end);
        std::streamsize file_size = static_cast<std::streamsize>(file.tellg());

        char* buffer = mem_alloc_array<char>(S, static_cast<size_t>(file_size));
        file.read(buffer, file_size);

        push_string(S, std::string_view(buffer, static_cast<size_t>(file_size)));

        mem_free_array<char>(S, buffer, static_cast<size_t>(file_size));

        return 1;
    }

    // fs.write(path, content) -> true on success, false + error on failure
    static int fs_write(State* S)
    {
        auto path_sv = check_string(S, 0);
        auto content = check_string(S, 1);

        fs::path path = fs::path{ path_sv };
        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            push_boolean(S, false);
            if (file.fail())
            {
                push_string(S, std::strerror(errno));
            }
            else
            {
                push_string(S, "failed to open file");
            }
            return 2;
        }

        file.write(content.data(), static_cast<std::streamsize>(content.size()));

        push_boolean(S, true);
        return 1;
    }

    // fs.append(path, content) -> true on success, false + error on failure
    static int fs_append(State* S)
    {
        auto path_sv = check_string(S, 0);
        auto content = check_string(S, 1);

        fs::path path = fs::path{ path_sv };
        std::ofstream file(path, std::ios::binary | std::ios::app);
        if (!file)
        {
            push_boolean(S, false);
            if (file.fail())
            {
                push_string(S, std::strerror(errno));
            }
            else
            {
                push_string(S, "failed to open file");
            }
            return 2;
        }

        file.write(content.data(), static_cast<std::streamsize>(content.size()));

        push_boolean(S, true);
        return 1;
    }

    // fs.exists(path) -> boolean
    static int fs_exists(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        bool exists = fs::exists(path, ec);

        push_boolean(S, exists && !ec);
        return 1;
    }

    // fs.remove(path) -> true if removed, false if doesn't exist, false + error on failure
    static int fs_remove(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        bool removed = fs::remove(path, ec);

        if (ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        push_boolean(S, removed);
        return 1;
    }

    // fs.rename(oldpath, newpath) -> true on success, false + error on failure
    static int fs_rename(State* S)
    {
        auto oldpath_sv = check_string(S, 0);
        auto newpath_sv = check_string(S, 1);

        fs::path oldpath = fs::path{ oldpath_sv };
        fs::path newpath = fs::path{ newpath_sv };

        std::error_code ec;
        fs::rename(oldpath, newpath, ec);

        if (ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        push_boolean(S, true);
        return 1;
    }

    // fs.copy(source, dest) -> true on success, false + error on failure
    static int fs_copy(State* S)
    {
        auto source_sv = check_string(S, 0);
        auto dest_sv = check_string(S, 1);

        fs::path source = fs::path{ source_sv };
        fs::path dest = fs::path{ dest_sv };

        std::error_code ec;
        fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);

        if (ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        push_boolean(S, true);
        return 1;
    }

    // fs.mkdir(path) -> true if created, false if already exists, false + error on failure
    static int fs_mkdir(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        bool created = fs::create_directories(path, ec);

        if (ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        push_boolean(S, created);
        return 1;
    }

    // fs.rmdir(path) -> true if removed, false if doesn't exist, false + error on failure
    static int fs_rmdir(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        bool removed = fs::remove(path, ec);

        if (ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        push_boolean(S, removed);
        return 1;
    }

    // fs.list(path) -> table on success, false + error on failure
    static int fs_list(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        if (!fs::exists(path, ec) || ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        table_new(S);
        Integer index = 0;

        for (const auto& entry : fs::directory_iterator(path, ec))
        {
            if (ec)
            {
                pop(S, 1); // Remove partial table
                push_boolean(S, false);
                push_string(S, ec.message());
                return 2;
            }

            push_integer(S, index);
            push_string(S, entry.path().filename().string());
            table_rawset(S, -3);
            index++;
        }

        return 1;
    }

    // fs.isfile(path) -> boolean
    static int fs_isfile(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        bool is_file = fs::is_regular_file(path, ec);

        push_boolean(S, is_file && !ec);
        return 1;
    }

    // fs.isdir(path) -> boolean
    static int fs_isdir(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        bool is_dir = fs::is_directory(path, ec);

        push_boolean(S, is_dir && !ec);
        return 1;
    }

    // fs.size(path) -> number on success, false + error on failure
    static int fs_size(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        auto size = fs::file_size(path, ec);

        if (ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        push_number(S, static_cast<double>(size));
        return 1;
    }

    // fs.cwd() -> string on success, false + error on failure
    static int fs_cwd(State* S)
    {
        std::error_code ec;
        auto current = fs::current_path(ec);

        if (ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        push_string(S, current.string());
        return 1;
    }

    // fs.chdir(path) -> true on success, false + error on failure
    static int fs_chdir(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        fs::current_path(path, ec);

        if (ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        push_boolean(S, true);
        return 1;
    }

    // fs.absolute(path) -> string on success, false + error on failure
    static int fs_absolute(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path path = fs::path{ path_sv };

        std::error_code ec;
        auto abs_path = fs::absolute(path, ec);

        if (ec)
        {
            push_boolean(S, false);
            push_string(S, ec.message());
            return 2;
        }

        push_string(S, abs_path.string());
        return 1;
    }

    // fs.join(path1, path2, ...) -> string
    static int fs_join(State* S)
    {
        int n = get_top(S);

        if (n == 0)
        {
            push_string(S, {});
            return 1;
        }

        auto first_sv = check_string(S, 0);
        fs::path result = fs::path{ first_sv };

        for (int i = 1; i < n; i++)
        {
            auto part_sv = check_string(S, i);
            result /= fs::path{ part_sv };
        }

        std::string result_str = result.string();
        push_string(S, result_str);
        return 1;
    }

    // fs.dirname(path) -> string
    static int fs_dirname(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path p = fs::path{ path_sv };

        std::string parent = p.parent_path().string();
        push_string(S, parent);
        return 1;
    }

    // fs.basename(path) -> string
    static int fs_basename(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path p = fs::path{ path_sv };

        std::string filename = p.filename().string();
        push_string(S, filename);
        return 1;
    }

    // fs.extension(path) -> string
    static int fs_extension(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path p = fs::path{ path_sv };

        std::string ext = p.extension().string();
        push_string(S, ext);
        return 1;
    }

    // fs.stem(path) -> string
    static int fs_stem(State* S)
    {
        auto path_sv = check_string(S, 0);
        fs::path p = fs::path{ path_sv };

        std::string stem = p.stem().string();
        push_string(S, stem);
        return 1;
    }

    void load_lib_fs(State* S, bool make_global)
    {
        static constexpr ModuleReg fs_funcs[] = {
            { "open", fs_open },
            { "read", fs_read },
            { "write", fs_write },
            { "append", fs_append },
            { "exists", fs_exists },
            { "remove", fs_remove },
            { "rename", fs_rename },
            { "copy", fs_copy },
            { "mkdir", fs_mkdir },
            { "rmdir", fs_rmdir },
            { "list", fs_list },
            { "is_file", fs_isfile },
            { "is_dir", fs_isdir },
            { "size", fs_size },
            { "cwd", fs_cwd },
            { "chdir", fs_chdir },
            { "absolute", fs_absolute },
            { "join", fs_join },
            { "dirname", fs_dirname },
            { "basename", fs_basename },
            { "extension", fs_extension },
            { "stem", fs_stem },
        };

        ModuleDef fs_module = { .funcs = fs_funcs };

        create_module(S, "fs", fs_module, make_global);
    }

} // namespace behl
