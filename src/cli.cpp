#include "common/print.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_closure.hpp"
#include "state.hpp"
#include "vm/bytecode.hpp"
#include "vm/value.hpp"

#include <behl/behl.hpp>
#include <behl/fs.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#    include <windows.h>
#endif

using namespace behl;

struct Options
{
    bool interactive = false;
    bool execute = false;
    bool dump_bytecode = false;
    std::string execute_code;
    std::vector<std::string> scripts;
};

template<typename... TArgs>
static void print_error(behl::format_string<TArgs...> fmt, TArgs&&... args)
{
    println("{}", ::behl::format(fmt, std::forward<TArgs>(args)...));
}

template<typename... TArgs>
[[noreturn]] static void fatal_error(behl::format_string<TArgs...> fmt, TArgs&&... args)
{
    println("{}", ::behl::format(fmt, std::forward<TArgs>(args)...));
    println("--- ABORT ---");

    std::flush(std::cout);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::exit(EXIT_FAILURE);
}

std::optional<Options> parse_args(int argc, char* argv[], std::string& error_msg)
{
    Options opts;
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "-i")
        {
            opts.interactive = true;
        }
        else if (arg == "-b")
        {
            opts.dump_bytecode = true;
        }
        else if (arg == "-e")
        {
            if (i + 1 >= argc)
            {
                error_msg = ::behl::format("'-e' requires an argument");
                return std::nullopt;
            }
            opts.execute = true;
            opts.execute_code = argv[++i];
        }
        else if (arg.starts_with('-'))
        {
            error_msg = ::behl::format("unrecognized option '{}'", arg);
            return std::nullopt;
        }
        else
        {
            opts.scripts.emplace_back(arg);
        }
    }
    return opts;
}

bool load_file(behl::State* S, std::string_view filename, std::string& error_msg)
{
    std::ifstream file(filename.data(), std::ios::binary);
    if (!file)
    {
        error_msg = ::behl::format("cannot open {}: No such file or directory", filename);
        return false;
    }

    // Read file contents - avoid istreambuf_iterator due to GCC 13 false positive warnings
    file.seekg(0, std::ios::end);
    std::string content;
    content.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(&content[0], static_cast<std::streamsize>(content.size()));

    behl::load_buffer(S, content, filename.data(), true);

    return true;
}

void dump_closure_bytecode(behl::State* S)
{
    if (S->stack.empty())
    {
        println("No closure on stack to dump");
        return;
    }

    const auto& val = S->stack.back();
    if (!val.is_closure())
    {
        println("Top of stack is not a closure value");
        return;
    }

    const behl::GCClosure* closure = val.get_closure();
    assert(closure != nullptr);

    println("\n=== Bytecode Dump ===");
    behl::dump_proto(*closure->proto);
    println("=====================\n");
}

static void print_results(behl::State* S)
{
    const int num_results = behl::get_top(S);
    if (num_results == 0)
    {
        return;
    }

    // Get global print function
    behl::get_global(S, "print");

    // Move print to bottom (before all results)
    behl::insert(S, 0);

    try
    {
        behl::call(S, num_results, 0);
    }
    catch (const std::exception& ex)
    {
        print_error("Error printing results: {}", ex.what());
    }
}

void repl(behl::State* S)
{
    std::string line;
    while (true)
    {
        print("> ");
        std::cout.flush();

        if (!std::getline(std::cin, line))
        {
            break;
        }

        if (line.empty())
        {
            continue;
        }

        try
        {
            behl::load_string(S, line);
            behl::call(S, 0, behl::kMultRet);

            print_results(S);
            behl::set_top(S, 0);
        }
        catch (const std::exception& ex)
        {
            print_error("{}", ex.what());
        }
    }
    println("");
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // Disable buffering
    std::string error_msg;
    auto opts_result = parse_args(argc, argv, error_msg);
    if (!opts_result)
    {
        fatal_error("{}", error_msg);
    }

    const Options& opts = *opts_result;

    behl::State* S = behl::new_state();
    behl::load_stdlib(S, true);
    behl::load_lib_fs(S, false);

    try
    {
        behl::load_string(S, opts.execute_code);

        if (opts.dump_bytecode)
        {
            dump_closure_bytecode(S);
            behl::close(S);
            return 0;
        }

        if (opts.execute)
        {
            behl::call(S, 0, behl::kMultRet);
            print_results(S);
        }

        for (const auto& script : opts.scripts)
        {
            std::string load_error;
            if (!load_file(S, script, load_error))
            {
                print_error("{}", load_error);
                behl::close(S);
                return 1;
            }

            if (opts.dump_bytecode)
            {
                dump_closure_bytecode(S);
                behl::close(S);
                return 0;
            }

            behl::call(S, 0, behl::kMultRet);

            print_results(S);
        }

        if (opts.interactive || (opts.scripts.empty() && !opts.execute))
        {
            repl(S);
        }
    }
    catch (const std::exception& ex)
    {
        fatal_error("{}", ex.what());
    }

    behl::close(S);
    println(" ===== CLI Exit ===== ");
    return 0;
}
