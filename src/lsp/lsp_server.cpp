#include "lsp_server.hpp"

#include "ast/ast.hpp"
#include "behl/behl.hpp"
#include "behl/exceptions.hpp"
#include "common/format.hpp"
#include "common/print.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "frontend/semantics_pass.hpp"
#include "module_analyzer.hpp"
#include "state.hpp"
#include "symbol_collector.hpp"

namespace behl::lsp
{
    // Built-in keywords and functions for completions
    static constexpr std::string_view BUILTIN_COMPLETIONS = R"([
        {"label": "function", "detail": "keyword"},
        {"label": "if", "detail": "keyword"},
        {"label": "else", "detail": "keyword"},
        {"label": "elseif", "detail": "keyword"},
        {"label": "while", "detail": "keyword"},
        {"label": "for", "detail": "keyword"},
        {"label": "foreach", "detail": "keyword"},
        {"label": "return", "detail": "keyword"},
        {"label": "break", "detail": "keyword"},
        {"label": "continue", "detail": "keyword"},
        {"label": "let", "detail": "keyword"},
        {"label": "const", "detail": "keyword"},
        {"label": "import", "detail": "keyword"},
        {"label": "export", "detail": "keyword"},
        {"label": "module", "detail": "keyword"},
        {"label": "print", "detail": "built-in function"},
        {"label": "typeof", "detail": "built-in function"},
        {"label": "tostring", "detail": "built-in function"},
        {"label": "tonumber", "detail": "built-in function"})";

    LanguageServer::LanguageServer()
    {
        // Initialize a State for parsing operations
        state = behl::new_state();

        // Standard library.
        behl::load_stdlib(state);

        // Initialize module analyzer with current directory as workspace root
        module_analyzer = std::make_unique<ModuleAnalyzer>(state, ".");
    }

    LanguageServer::~LanguageServer()
    {
        if (state)
        {
            behl::close(state);
            state = nullptr;
        }
    }

    std::string LanguageServer::initialize(const std::string& client_info)
    {
        return R"({
            "capabilities": {
                "textDocumentSync": 1,
                "completionProvider": {},
                "hoverProvider": true,
                "diagnosticProvider": true
            }
        })";
    }

    std::string LanguageServer::get_diagnostics(const std::string& source_code, const std::string& file_path)
    {
        if (!state)
        {
            return "[]";
        }

        try
        {
            // Tokenize
            auto tokens = behl::tokenize(state, source_code, file_path);

            // Parse
            AstHolder holder(state);
            auto ast = behl::parse(holder, tokens, file_path);

            // Semantic analysis
            ast = SemanticsPass::apply(state, holder, ast);

            // No errors found
            return "[]";
        }
        catch (const behl::ParserError& e)
        {
            // Format parser error as LSP diagnostic
            return behl::format(R"([{{
                    "line": {},
                    "column": {},
                    "length": 1,
                    "message": "{}",
                    "severity": "error"
                }}])",
                e.location().line, e.location().column, escape_json(e.what()));
        }
        catch (const behl::SyntaxError& e)
        {
            return behl::format(R"([{{
                    "line": {},
                    "column": {},
                    "length": 1,
                    "message": "{}",
                    "severity": "error"
                }}])",
                e.location().line, e.location().column, escape_json(e.what()));
        }
        catch (const behl::SemanticError& e)
        {
            return behl::format(R"([{{
                    "line": {},
                    "column": {},
                    "length": 1,
                    "message": "{}",
                    "severity": "error"
                }}])",
                e.location().line, e.location().column, escape_json(e.what()));
        }
        catch (const behl::BehlException& e)
        {
            return behl::format(R"([{{
                    "line": 0,
                    "column": 0,
                    "length": 1,
                    "message": "{}",
                    "severity": "error"
                }}])",
                escape_json(e.what()));
        }
        catch (...)
        {
            return R"([{
                "line": 0,
                "column": 0,
                "length": 1,
                "message": "Unknown error",
                "severity": "error"
            }])";
        }
    }

    std::string LanguageServer::get_completions(
        const std::string& source_code, int line, int character, const std::string& file_path)
    {
        if (!state || !module_analyzer)
        {
            return "[]";
        }

        // Find the start of the current token by going backwards from cursor
        // to avoid parsing incomplete tokens
        int parse_column = character;
        if (line >= 0 && character > 0)
        {
            // Find line start
            int current_line = 0;
            int line_start = 0;
            for (size_t i = 0; i < source_code.size(); ++i)
            {
                if (current_line == line)
                {
                    line_start = i;
                    break;
                }
                if (source_code[i] == '\n')
                {
                    current_line++;
                }
            }

            int pos = line_start + character;
            if (pos > 0 && pos <= static_cast<int>(source_code.size()))
            {
                // Go backwards to find start of current token
                int token_start = pos - 1;
                while (token_start >= line_start
                    && (std::isalnum(static_cast<unsigned char>(source_code[token_start])) || source_code[token_start] == '_'))
                {
                    token_start--;
                }
                token_start++; // Move back to first character of token

                // Use token start as parse position
                parse_column = token_start - line_start;
            }
        }

        // Parse the file once with line limit
        // IMPORTANT: Keep holder alive for entire function - it owns the AST memory
        AstHolder holder(state);
        AstProgram* ast = nullptr;
        try
        {
            auto tokens = behl::tokenize(state, source_code, file_path);
            // Parse up to the start of the current token
            // LSP uses 0-indexed lines but lexer uses 1-indexed, so add 1 to line
            ast = behl::parse(holder, tokens, file_path, line + 1, parse_column);
        }
        catch (std::exception& e)
        {
            behl::println("Error parsing file for completions: {}", e.what());
            // Return just keywords if parsing fails
            return std::string(BUILTIN_COMPLETIONS) + "]";
        }

        // Check if this is a member access completion (e.g., "math.")
        std::string base_identifier;
        if (is_member_access_position(source_code, line, character, base_identifier))
        {
            behl::println("Detected member access for identifier '{}'", base_identifier);

            // Find if base_identifier is an imported module
            std::string module_name = module_analyzer->find_import_for_variable(ast, base_identifier);

            if (!module_name.empty())
            {
                // Get module exports (works for both built-in and user-defined modules)
                auto exports = module_analyzer->get_module_completions(module_name, file_path);

                // Format as JSON
                std::string result = "[";
                bool first = true;
                for (const auto& exp : exports)
                {
                    if (!first)
                    {
                        result += ",";
                    }
                    first = false;

                    std::string detail;
                    if (exp.is_function)
                    {
                        // Build function signature
                        detail = "function(";
                        for (size_t i = 0; i < exp.parameters.size(); ++i)
                        {
                            if (i > 0)
                            {
                                detail += ", ";
                            }
                            detail += exp.parameters[i];
                        }
                        if (exp.is_vararg)
                        {
                            if (!exp.parameters.empty())
                            {
                                detail += ", ";
                            }
                            detail += "...";
                        }
                        detail += ")";
                    }
                    else
                    {
                        detail = exp.is_constant ? "constant" : "property";
                    }

                    result += behl::format(R"({{"label": "{}", "detail": "{}"}})", std::string(exp.name), detail);
                }
                result += "]";
                return result;
            }
            else
            {
                behl::println("No import found for identifier '{}'", base_identifier);
            }
        }

        // Default: return keywords, built-in functions, and symbols from current file
        std::string result(BUILTIN_COMPLETIONS);

        // Collect symbols from parsed AST
        SymbolCollector collector;
        auto symbols = collector.collect(ast, -1);

        // Add symbols to completions
        for (const auto& symbol : symbols)
        {
            result += ",";
            std::string detail;
            if (symbol.is_function)
            {
                // Build function signature
                detail = "function(";
                for (size_t i = 0; i < symbol.parameters.size(); ++i)
                {
                    if (i > 0)
                    {
                        detail += ", ";
                    }
                    detail += symbol.parameters[i];
                }
                if (symbol.is_vararg)
                {
                    if (!symbol.parameters.empty())
                    {
                        detail += ", ";
                    }
                    detail += "...";
                }
                detail += ")";

                std::cout << "Completion function: " << symbol.name << " with detail: " << detail << "\n";
            }
            else
            {
                detail = symbol.is_constant ? "constant" : "variable";
            }
            result += behl::format(R"({{"label": "{}", "detail": "{}"}})", std::string(symbol.name), detail);
        }

        result += "]";
        return result;
    }

    bool LanguageServer::is_member_access_position(
        const std::string& source_code, int line, int character, std::string& base_identifier)
    {
        if (character == 0)
        {
            return false;
        }

        // Find the start of the current line
        int current_line = 0;
        int line_start = 0;
        for (size_t i = 0; i < source_code.size(); ++i)
        {
            if (current_line == line)
            {
                line_start = i;
                break;
            }
            if (source_code[i] == '\n')
            {
                current_line++;
            }
        }

        int pos = line_start + character;

        if (pos <= 0 || pos > static_cast<int>(source_code.size()))
        {
            return false;
        }

        // Check if character before cursor is '.'
        if (source_code[pos - 1] != '.')
        {
            return false;
        }

        // Extract identifier before the dot
        int end = pos - 2; // Position before the dot
        while (end >= 0 && (std::isspace(source_code[end])))
        {
            end--;
        }

        if (end < 0)
        {
            return false;
        }

        int start = end;
        while (start >= 0 && (std::isalnum(source_code[start]) || source_code[start] == '_'))
        {
            start--;
        }
        start++;

        if (start <= end)
        {
            base_identifier = source_code.substr(start, end - start + 1);
            return !base_identifier.empty();
        }

        return false;
    }

    std::string LanguageServer::get_hover_info(const std::string& source_code, int line, int character)
    {
        // TODO: Implement using symbol table and type information
        return "null";
    }

    std::string LanguageServer::get_signature_help(
        const std::string& source_code, int line, int character, const std::string& file_path)
    {
        if (!state || !module_analyzer)
        {
            return "null";
        }

        try
        {
            // Parse current file to get symbols
            auto tokens = behl::tokenize(state, source_code, file_path);
            AstHolder holder(state);
            // For signature help, parse only up to cursor position
            // LSP uses 0-indexed lines/columns, but lexer/parser uses 1-indexed, so add 1 to both
            auto ast = behl::parse(holder, tokens, file_path, line + 1, character + 1);

            // Find function call at current position
            // Check if it's a member access (e.g., math.sin(...))
            std::string base_identifier;
            std::string func_name = get_function_name_at_position(source_code, line, character, &base_identifier);
            if (func_name.empty())
            {
                return "null";
            }

            // If we have a base identifier, check for module member access
            if (!base_identifier.empty())
            {
                // Find if base_identifier is an imported module
                std::string module_name = module_analyzer->find_import_for_variable(ast, base_identifier);

                if (!module_name.empty())
                {
                    // Get module exports
                    auto exports = module_analyzer->get_module_completions(module_name, file_path);

                    // Find matching function export
                    for (const auto& exp : exports)
                    {
                        if (exp.is_function && std::string_view(exp.name.data(), exp.name.size()) == func_name)
                        {
                            // Build signature help response
                            std::string params_str;
                            for (size_t i = 0; i < exp.parameters.size(); ++i)
                            {
                                if (i > 0)
                                {
                                    params_str += ", ";
                                }
                                params_str += exp.parameters[i];
                            }
                            if (exp.is_vararg)
                            {
                                if (!exp.parameters.empty())
                                {
                                    params_str += ", ";
                                }
                                params_str += "...";
                            }

                            // Determine active parameter based on commas before cursor
                            int active_param = count_parameters_before_cursor(source_code, line, character);

                            return behl::format(R"({{"functionName":"{}","parameters":"{}","activeParameter":{}}})",
                                escape_json(std::string_view(exp.name.data(), exp.name.size())), escape_json(params_str),
                                active_param);
                        }
                    }
                }
            }

            // Fallback: check local symbols
            SymbolCollector collector;
            auto symbols = collector.collect(ast, -1); // Collect all symbols

            // Find matching function
            for (const auto& symbol : symbols)
            {
                if (symbol.is_function && std::string_view(symbol.name.data(), symbol.name.size()) == func_name)
                {
                    // Build signature help response
                    std::string params_str;
                    for (size_t i = 0; i < symbol.parameters.size(); ++i)
                    {
                        if (i > 0)
                        {
                            params_str += ", ";
                        }
                        params_str += symbol.parameters[i];
                    }
                    if (symbol.is_vararg)
                    {
                        if (!symbol.parameters.empty())
                        {
                            params_str += ", ";
                        }
                        params_str += "...";
                    }

                    // Determine active parameter based on commas before cursor
                    int active_param = count_parameters_before_cursor(source_code, line, character);

                    return behl::format(R"({{"functionName":"{}","parameters":"{}","activeParameter":{}}})",
                        escape_json(std::string_view(symbol.name.data(), symbol.name.size())), escape_json(params_str),
                        active_param);
                }
            }

            return "null";
        }
        catch (...)
        {
            return "null";
        }
    }

    std::string LanguageServer::get_function_name_at_position(
        const std::string& source_code, int line, int character, std::string* out_base_identifier)
    {
        if (character == 0)
        {
            return "";
        }

        // Find the start of the current line
        int current_line = 0;
        int line_start = 0;
        for (size_t i = 0; i < source_code.size(); ++i)
        {
            if (current_line == line)
            {
                line_start = i;
                break;
            }
            if (source_code[i] == '\n')
            {
                current_line++;
            }
        }

        int pos = line_start + character;
        if (pos <= 0 || pos > static_cast<int>(source_code.size()))
        {
            return "";
        }

        // Search backwards for opening parenthesis
        int paren_pos = pos - 1;
        int paren_depth = 0;
        while (paren_pos >= 0)
        {
            if (source_code[paren_pos] == ')')
            {
                paren_depth++;
            }
            else if (source_code[paren_pos] == '(')
            {
                if (paren_depth == 0)
                {
                    break;
                }
                paren_depth--;
            }
            paren_pos--;
        }

        if (paren_pos < 0 || source_code[paren_pos] != '(')
        {
            return "";
        }

        // Extract identifier before the parenthesis
        int end = paren_pos - 1;
        while (end >= 0 && std::isspace(source_code[end]))
        {
            end--;
        }

        if (end < 0)
        {
            return "";
        }

        int start = end;
        while (start >= 0 && (std::isalnum(source_code[start]) || source_code[start] == '_'))
        {
            start--;
        }
        start++;

        if (start > end)
        {
            return "";
        }

        std::string func_name = source_code.substr(start, end - start + 1);

        // Check if there's a dot before the function name (member access)
        if (out_base_identifier && start > 0 && source_code[start - 1] == '.')
        {
            // Extract base identifier before the dot
            int base_end = start - 2;
            while (base_end >= 0 && std::isspace(source_code[base_end]))
            {
                base_end--;
            }

            if (base_end >= 0)
            {
                int base_start = base_end;
                while (base_start >= 0 && (std::isalnum(source_code[base_start]) || source_code[base_start] == '_'))
                {
                    base_start--;
                }
                base_start++;

                if (base_start <= base_end)
                {
                    *out_base_identifier = source_code.substr(base_start, base_end - base_start + 1);
                }
            }
        }

        return func_name;
    }

    int LanguageServer::count_parameters_before_cursor(const std::string& source_code, int line, int character)
    {
        // Find the start of the current line
        int current_line = 0;
        int line_start = 0;
        for (size_t i = 0; i < source_code.size(); ++i)
        {
            if (current_line == line)
            {
                line_start = i;
                break;
            }
            if (source_code[i] == '\n')
            {
                current_line++;
            }
        }

        int pos = line_start + character;
        if (pos <= 0 || pos > static_cast<int>(source_code.size()))
        {
            return 0;
        }

        // Search backwards for opening parenthesis
        int paren_pos = pos - 1;
        int paren_depth = 0;
        while (paren_pos >= 0)
        {
            if (source_code[paren_pos] == ')')
            {
                paren_depth++;
            }
            else if (source_code[paren_pos] == '(')
            {
                if (paren_depth == 0)
                {
                    break;
                }
                paren_depth--;
            }
            paren_pos--;
        }

        if (paren_pos < 0)
        {
            return 0;
        }

        // Count commas between opening paren and cursor, accounting for strings and nested parens
        int comma_count = 0;
        paren_depth = 0;
        bool in_string = false;
        char string_delimiter = '\0';
        bool escape_next = false;

        for (int i = paren_pos + 1; i < pos; ++i)
        {
            char c = source_code[i];

            if (escape_next)
            {
                escape_next = false;
                continue;
            }

            if (in_string)
            {
                if (c == '\\')
                {
                    escape_next = true;
                }
                else if (c == string_delimiter)
                {
                    in_string = false;
                    string_delimiter = '\0';
                }
            }
            else
            {
                if (c == '"' || c == '\'')
                {
                    in_string = true;
                    string_delimiter = c;
                }
                else if (c == '(')
                {
                    paren_depth++;
                }
                else if (c == ')')
                {
                    paren_depth--;
                }
                else if (c == ',' && paren_depth == 0)
                {
                    comma_count++;
                }
            }
        }

        return comma_count;
    }

    std::string LanguageServer::escape_json(const std::string_view str)
    {
        std::string result;
        result.reserve(str.size());

        for (char c : str)
        {
            switch (c)
            {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\b':
                    result += "\\b";
                    break;
                case '\f':
                    result += "\\f";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    if (c < 32)
                    {
                        result += behl::format("\\u{:04x}", static_cast<unsigned char>(c));
                    }
                    else
                    {
                        result += c;
                    }
            }
        }

        return result;
    }

} // namespace behl::lsp
