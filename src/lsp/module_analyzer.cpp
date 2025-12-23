#include "module_analyzer.hpp"

#include "ast/ast.hpp"
#include "common/format.hpp"
#include "common/print.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "gc/gco_table.hpp"
#include "modules.hpp"
#include "state.hpp"
#include "vm/value.hpp"

#include <fstream>
#include <sstream>

namespace behl::lsp
{

    ModuleAnalyzer::ModuleAnalyzer(State* state, std::string_view workspace_root)
        : state(state)
        , workspace_root(workspace_root)
    {
    }

    std::string ModuleAnalyzer::resolve_module_path(std::string_view module_name, std::string_view importing_file)
    {
        auto result = behl::resolve_module_path(state, module_name, importing_file);
        if (result.empty())
        {
            behl::println("ModuleAnalyzer: Failed to resolve module path for '{}'", module_name);
        }
        else
        {
            behl::println("ModuleAnalyzer: Resolved '{}' to '{}'", module_name, result);
        }
        return result;
    }

    ModuleInfo ModuleAnalyzer::analyze_module(const std::string& module_path)
    {
        String path_key(module_path);

        // Create new module info
        ModuleInfo info;
        info.path = path_key;
        info.parsed = false;

        try
        {
            // Read file from filesystem
            std::ifstream file(module_path);
            if (!file)
            {
                behl::println("ModuleAnalyzer: Failed to open module file '{}'", module_path);
                return {}; // File not found
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string source = buffer.str();

            // Tokenize and parse
            auto tokens = tokenize(state, source, module_path);
            AstHolder holder(state);
            auto ast = parse(holder, tokens, module_path);

            // Extract exports
            extract_exports_from_ast(ast, info);
            info.parsed = true;
        }
        catch (std::exception& e)
        {
            behl::println("ModuleAnalyzer: Error parsing module '{}': {}", module_path, e.what());
            return {};
        }

        return info;
    }

    void ModuleAnalyzer::extract_exports_from_ast(const AstProgram* ast, ModuleInfo& info)
    {
        if (!ast || !ast->block)
        {
            return;
        }

        // Walk through statements looking for exports
        for (auto* stat = ast->block->first_stat; stat; stat = stat->next_child)
        {
            // export declaration (function or const)
            if (stat->type == AstNodeType::kExportDecl)
            {
                auto* export_decl = static_cast<AstExportDecl*>(stat);
                if (export_decl->declaration)
                {
                    // export function name() {}
                    if (export_decl->declaration->type == AstNodeType::kFuncDefStat)
                    {
                        auto* func_def = static_cast<AstFuncDefStat*>(export_decl->declaration);
                        if (func_def->first_name_part)
                        {
                            // Function name is the first name part (could be a.b.c for methods)
                            auto* name_node = func_def->first_name_part;
                            if (name_node->type == AstNodeType::kString)
                            {
                                auto* name_str = static_cast<AstString*>(name_node);
                                ExportInfo exp;
                                exp.name = name_str->view();
                                exp.is_function = true;

                                // Extract parameters
                                for (AstNode* param = func_def->first_param; param; param = param->next_child)
                                {
                                    if (auto* param_str = param->try_as<AstString>())
                                    {
                                        exp.parameters.emplace_back(param_str->data, param_str->length);
                                    }
                                }
                                exp.is_vararg = func_def->is_vararg;

                                info.exports.push_back(exp);
                            }
                        }
                    }
                    // export const name = value
                    else if (export_decl->declaration->type == AstNodeType::kLocalDecl)
                    {
                        auto* local_decl = static_cast<AstLocalDecl*>(export_decl->declaration);
                        // Walk through the list of names being declared
                        for (auto* name = local_decl->first_name; name; name = static_cast<AstString*>(name->next_child))
                        {
                            ExportInfo exp;
                            exp.name = name->view();
                            exp.is_constant = local_decl->is_const;
                            info.exports.push_back(exp);
                        }
                    }
                }
            }
            // export { name1, name2, ... }
            else if (stat->type == AstNodeType::kExportList)
            {
                auto* export_list = static_cast<AstExportList*>(stat);
                for (auto* name = export_list->first_name; name; name = static_cast<AstString*>(name->next_child))
                {
                    ExportInfo exp;
                    exp.name = name->view();
                    info.exports.push_back(exp);
                }
            }
        }
    }

    std::string ModuleAnalyzer::find_import_for_variable(const AstProgram* ast, std::string_view var_name)
    {
        if (!ast || !ast->block)
        {
            return "";
        }

        // Walk statements looking for: const VAR = import("module")
        for (auto* stat = ast->block->first_stat; stat; stat = stat->next_child)
        {
            if (stat->type == AstNodeType::kLocalDecl)
            {
                auto* local_decl = static_cast<AstLocalDecl*>(stat);

                // Check each name in the declaration
                auto* name_node = local_decl->first_name;
                auto* init_node = local_decl->first_init;

                while (name_node && init_node)
                {
                    // Check if this name matches the variable we're looking for
                    if (name_node->view() == var_name)
                    {
                        // Check if init is a function call
                        if (init_node->type == AstNodeType::kFuncCall)
                        {
                            auto* call = static_cast<AstFuncCall*>(init_node);

                            // Check if function being called is "import"
                            if (call->func && call->func->type == AstNodeType::kIdent)
                            {
                                auto* func_ident = static_cast<AstIdent*>(call->func);
                                if (func_ident->name->view() == "import")
                                {
                                    // Get the first argument (module name string)
                                    if (call->first_arg && call->first_arg->type == AstNodeType::kString)
                                    {
                                        auto* module_name_str = static_cast<AstString*>(call->first_arg);
                                        return std::string(module_name_str->view());
                                    }
                                }
                            }
                        }

                        // Found the variable but it's not an import
                        return "";
                    }

                    // Move to next name/init pair
                    name_node = static_cast<AstString*>(name_node->next_child);
                    init_node = init_node->next_child;
                }
            }
        }

        return "";
    }

    std::vector<ExportInfo> ModuleAnalyzer::get_module_completions(
        std::string_view module_name, std::string_view importing_file)
    {
        std::vector<ExportInfo> completions;

        // Check if it's a built-in module (in module_cache)
        String module_key(module_name);
        auto it = state->module_cache.find(module_key);
        if (it != state->module_cache.end())
        {
            // Extract members from built-in module table
            Value module_value = it->second;
            if (module_value.is_table())
            {
                auto* table = module_value.get_table();
                if (table)
                {
                    // Iterate through array part
                    for (size_t i = 0; i < table->array.size(); ++i)
                    {
                        if (!table->array[i].is_nil())
                        {
                            ExportInfo info;
                            info.name = String(behl::format("{}", i));
                            info.is_function = table->array[i].is_cfunction();
                            completions.push_back(info);
                        }
                    }

                    // Iterate through hash part
                    for (const auto& [key, value] : table->hash)
                    {
                        if (key.is_string())
                        {
                            ExportInfo info;
                            info.name = String(key.get_string()->view());
                            info.is_function = value.is_cfunction();
                            info.is_constant = !value.is_cfunction();
                            completions.push_back(info);
                        }
                    }
                }
            }
            return completions;
        }

        // Resolve module path for user-defined modules
        std::string resolved = resolve_module_path(module_name, importing_file);
        if (resolved.empty())
        {
            behl::println("ModuleAnalyzer: Failed to resolve module path for '{}'", module_name);
            return completions;
        }

        // Analyze user module
        ModuleInfo info = analyze_module(resolved);
        if (info.parsed)
        {
            completions = info.exports;
        }

        return completions;
    }

} // namespace behl::lsp
