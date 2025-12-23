#pragma once

#include "common/hash_map.hpp"
#include "common/string.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace behl
{
    struct State;
    struct AstProgram;
    class AstHolder;
} // namespace behl

namespace behl::lsp
{

    struct ExportInfo
    {
        String name;
        bool is_function = false;
        bool is_constant = false;
        std::vector<std::string> parameters; // Function parameters (if is_function)
        bool is_vararg = false;              // Whether function accepts varargs
    };

    struct ModuleInfo
    {
        String path;
        std::vector<ExportInfo> exports;
        bool parsed = false;
    };

    class ModuleAnalyzer
    {
    public:
        ModuleAnalyzer(State* state, std::string_view workspace_root);

        // Resolve module path (handles relative imports and search paths)
        std::string resolve_module_path(std::string_view module_name, std::string_view importing_file);

        // Parse a module file and extract exports
        ModuleInfo analyze_module(const std::string& module_path);

        // Find imported module for a variable name in the current file
        std::string find_import_for_variable(const AstProgram* ast, std::string_view var_name);

        // Get completions for a module member access (e.g., math.)
        std::vector<ExportInfo> get_module_completions(std::string_view module_name, std::string_view importing_file);

    private:
        State* state;
        String workspace_root;

        void extract_exports_from_ast(const AstProgram* ast, ModuleInfo& info);
    };

} // namespace behl::lsp
