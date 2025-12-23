#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace behl
{
    struct State;
    struct AstProgram;
} // namespace behl

namespace behl::lsp
{
    class ModuleAnalyzer;

    // LSP Server implementation
    class LanguageServer
    {
    public:
        LanguageServer();
        ~LanguageServer();

        // Initialize LSP server
        std::string initialize(const std::string& client_info);

        // Parse and return diagnostics (errors/warnings)
        std::string get_diagnostics(const std::string& source_code, const std::string& file_path = "<lsp>");

        // Get completion suggestions at position (with optional file path for module resolution)
        std::string get_completions(
            const std::string& source_code, int line, int character, const std::string& file_path = "<lsp>");

        // Get hover information at position
        std::string get_hover_info(const std::string& source_code, int line, int character);

        // Get signature help (function parameters) at position
        std::string get_signature_help(
            const std::string& source_code, int line, int character, const std::string& file_path = "<lsp>");

    private:
        behl::State* state = nullptr;
        std::unique_ptr<ModuleAnalyzer> module_analyzer;

        // Helper to escape strings for JSON output
        static std::string escape_json(std::string_view str);

        // Helper to find identifier at position
        std::string get_identifier_at_position(const std::string& source_code, int line, int character);

        // Helper to check if position is after a dot (member access)
        bool is_member_access_position(const std::string& source_code, int line, int character, std::string& base_identifier);

        // Helper to get function name at position (for signature help)
        // If out_base_identifier is provided, it will be filled with the base identifier for member access (e.g., "math" in
        // "math.sin(")
        std::string get_function_name_at_position(
            const std::string& source_code, int line, int character, std::string* out_base_identifier = nullptr);

        // Helper to count parameters before cursor position
        int count_parameters_before_cursor(const std::string& source_code, int line, int character);
    };

} // namespace behl::lsp
