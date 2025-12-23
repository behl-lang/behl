#include "export_transform.hpp"

#include "ast/ast.hpp"
#include "ast/ast_holder.hpp"
#include "common/vector.hpp"
#include "state.hpp"

namespace behl
{
    namespace
    {
        constexpr std::string_view EXPORTS_VAR = "__EXPORTS__";

        // Create: __EXPORTS__["key"] = value
        AstNode* make_export_assignment(AstHolder& holder, std::string_view key, std::string_view value_name)
        {
            // Create identifier for __EXPORTS__
            auto* exports_name = holder.make_string(EXPORTS_VAR);
            auto* exports_ident = holder.make<AstIdent>(exports_name);

            // Create string key
            auto* key_str = holder.make_string(key);

            // Create identifier for value
            auto* value_name_str = holder.make_string(value_name);
            auto* value_ident = holder.make<AstIdent>(value_name_str);

            // Create table access: __EXPORTS__[key]
            auto* index = holder.make<AstIndex>(exports_ident, key_str);

            // Create assignment: __EXPORTS__[key] = value
            auto* assign = holder.make<AstAssign>();
            assign->first_var = index;
            assign->first_expr = value_ident;

            return assign;
        }

    } // namespace

    AstProgram* transform_exports(State* S, AstHolder& holder, AstProgram* program)
    {
        if (!program->is_module || !program->block)
        {
            return program; // Not a module, nothing to transform
        }

        AutoVector<std::string_view> exported_names(S);

        // Collect exported names by walking the AST ourselves (before semantics pass)
        for (AstNode* stat = program->block->first_stat; stat; stat = stat->next_child)
        {
            if (stat->type == AstNodeType::kExportDecl)
            {
                auto* export_decl = stat->as<AstExportDecl>();
                if (export_decl->declaration->type == AstNodeType::kFuncDefStat)
                {
                    auto* func_def = export_decl->declaration->as<AstFuncDefStat>();
                    if (func_def->first_name_part && func_def->first_name_part->type == AstNodeType::kString)
                    {
                        auto* name_str = static_cast<AstString*>(func_def->first_name_part);
                        exported_names.push_back(name_str->view());
                    }
                }
                else if (export_decl->declaration->type == AstNodeType::kLocalDecl)
                {
                    auto* local_decl = export_decl->declaration->as<AstLocalDecl>();
                    for (AstString* name = local_decl->first_name; name; name = static_cast<AstString*>(name->next_child))
                    {
                        exported_names.push_back(name->view());
                    }
                }
            }
            else if (stat->type == AstNodeType::kExportList)
            {
                auto* export_list = stat->as<AstExportList>();
                for (AstString* name = export_list->first_name; name; name = static_cast<AstString*>(name->next_child))
                {
                    exported_names.push_back(name->view());
                }
            }
        }

        // 1. Create: let __EXPORTS__ = {}
        auto* exports_name_str = holder.make_string(EXPORTS_VAR);
        auto* empty_table = holder.make<AstTableCtor>();
        auto* exports_decl = holder.make<AstLocalDecl>(false, exports_name_str, empty_table);

        // 2. Unwrap export declarations - replace export nodes with their inner declarations
        AstNode* first_stat = nullptr;
        AstNode** tail = &first_stat;

        for (AstNode* stat = program->block->first_stat; stat; stat = stat->next_child)
        {
            if (stat->type == AstNodeType::kExportDecl)
            {
                // Replace export declaration with its inner declaration
                auto* export_decl = stat->as<AstExportDecl>();
                *tail = export_decl->declaration;
                tail = &export_decl->declaration->next_child;
            }
            else if (stat->type == AstNodeType::kExportList)
            {
                // Skip export lists - they don't have inner declarations
                continue;
            }
            else
            {
                *tail = stat;
                tail = &stat->next_child;
            }
        }

        // 3. Create export assignments for all exported names
        for (std::string_view name : exported_names)
        {
            auto* assignment = make_export_assignment(holder, name, name);
            *tail = assignment;
            tail = &assignment->next_child;
        }

        // 4. Create return statement
        auto* exports_return_name = holder.make_string(EXPORTS_VAR);
        auto* exports_return_ident = holder.make<AstIdent>(exports_return_name);
        auto* return_stat = holder.make<AstReturn>(exports_return_ident);
        *tail = return_stat;
        return_stat->next_child = nullptr;

        // 5. Update block: __EXPORTS__ = {} at beginning, then unwrapped statements, assignments, return
        exports_decl->next_child = first_stat;
        program->block->first_stat = exports_decl;

        return program;
    }

} // namespace behl
