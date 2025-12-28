#include "common/print.hpp"
#include "gc/gco_closure.hpp"
#include "state.hpp"
#include "vm/bytecode.hpp"

#include <behl/behl.hpp>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#    include <windows.h>
#endif

using namespace behl;

enum class Mode
{
    Interactive, // REPL mode
    Execute,     // Execute code from -e
    Dump,        // Dump bytecode with -b
    Run          // Run script files
};

struct Options
{
    Mode mode = Mode::Interactive;
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
    bool mode_set = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "-i")
        {
            if (mode_set)
            {
                error_msg = ::behl::format("cannot combine -i with other modes");
                return std::nullopt;
            }
            opts.mode = Mode::Interactive;
            mode_set = true;
        }
        else if (arg == "-b")
        {
            if (mode_set)
            {
                error_msg = ::behl::format("cannot combine -b with other modes");
                return std::nullopt;
            }
            opts.mode = Mode::Dump;
            mode_set = true;
        }
        else if (arg == "-e")
        {
            if (mode_set)
            {
                error_msg = ::behl::format("cannot combine -e with other modes");
                return std::nullopt;
            }
            if (i + 1 >= argc)
            {
                error_msg = ::behl::format("'-e' requires an argument");
                return std::nullopt;
            }
            opts.mode = Mode::Execute;
            opts.execute_code = argv[++i];
            mode_set = true;
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

    // Determine mode based on what was provided
    if (!mode_set)
    {
        if (!opts.scripts.empty())
        {
            opts.mode = Mode::Run;
        }
        else
        {
            opts.mode = Mode::Interactive;
        }
    }
    else if (!opts.scripts.empty())
    {
        // Mode was explicitly set, apply it to scripts
        if (opts.mode == Mode::Interactive)
        {
            error_msg = ::behl::format("-i flag does not work with script files");
            return std::nullopt;
        }
        if (opts.mode == Mode::Execute)
        {
            error_msg = ::behl::format("-e flag does not work with script files");
            return std::nullopt;
        }
    }
    else if (opts.mode == Mode::Dump)
    {
        error_msg = ::behl::format("-b requires a script file or -e code");
        return std::nullopt;
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

static int run_execute_mode(behl::State* S, const Options& opts)
{
    behl::load_string(S, opts.execute_code);
    behl::call(S, 0, behl::kMultRet);
    print_results(S);
    return 0;
}

static int run_dump_mode(behl::State* S, const Options& opts)
{
    if (!opts.scripts.empty())
    {
        if (opts.scripts.size() > 1)
        {
            print_error("Error: bytecode dump (-b) only works with a single file");
            return 1;
        }

        std::string load_error;
        if (!load_file(S, opts.scripts[0], load_error))
        {
            print_error("{}", load_error);
            return 1;
        }
    }
    else
    {
        behl::load_string(S, opts.execute_code);
    }

    dump_closure_bytecode(S);
    return 0;
}

static int run_script_mode(behl::State* S, const Options& opts)
{
    for (const auto& script : opts.scripts)
    {
        std::string load_error;
        if (!load_file(S, script, load_error))
        {
            print_error("{}", load_error);
            return 1;
        }

        behl::call(S, 0, behl::kMultRet);
        print_results(S);
    }
    return 0;
}

static int run_interactive_mode(behl::State* S)
{
    repl(S);
    return 0;
}

static int execute_mode(behl::State* S, const Options& opts)
{
    switch (opts.mode)
    {
        case Mode::Execute:
            return run_execute_mode(S, opts);
        case Mode::Dump:
            return run_dump_mode(S, opts);
        case Mode::Run:
            return run_script_mode(S, opts);
        case Mode::Interactive:
            return run_interactive_mode(S);
    }
    return 0;
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

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
    behl::load_lib_process(S, false);

    int exit_code = 0;
    try
    {
        exit_code = execute_mode(S, opts);
    }
    catch (const std::exception& ex)
    {
        fatal_error("{}", ex.what());
    }

    behl::close(S);
    return exit_code;
}
