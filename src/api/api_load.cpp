#include "api/api_internal.hpp"
#include "ast/ast.hpp"
#include "ast/ast_holder.hpp"
#include "backend/compiler.hpp"
#include "common/string.hpp"
#include "frontend/export_transform.hpp"
#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "frontend/semantics_pass.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_string.hpp"
#include "optimization/optimization.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm_error.hpp"

#include <behl/behl.hpp>
#include <cassert>
#include <new>

namespace behl
{
    void load_unprotected(State* S, std::string_view str, std::string_view chunkname, bool optimize)
    {
        assert(S != nullptr && "State can not be null");

        GCPauseGuard gc_pause(S);

        std::string_view name = !chunkname.empty() ? chunkname : "<string>";

        auto tokens = tokenize(S, str, name);

        AstHolder holder(S);
        auto* ast = parse(holder, tokens, name);
        assert(ast != nullptr);

        ast = transform_exports(S, holder, ast);
        assert(ast != nullptr);

        ast = SemanticsPass::apply(S, holder, ast);
        assert(ast != nullptr);

        if (optimize)
        {
            ast = ASTOptimizationPipeline::apply(holder, ast);
            assert(ast != nullptr);
        }

        auto* proto_obj = compile(S, ast, name);

        auto* closure_obj = gc_new_closure(S, proto_obj);

        Value closure_val(closure_obj);
        S->stack.push_back(S, Value(closure_val));
    }

    int32_t load_buffer(State* S, std::string_view str, std::string_view chunkname, bool optimize)
    {
        assert(S != nullptr && "State can not be null");

        try
        {
            load_unprotected(S, str, chunkname, optimize);
        }
        catch (const Exception&)
        {
            push_error_from_exception(S);
            return kErrorSyntax;
        }
        catch (const std::bad_alloc&)
        {
            return push_error_from_exception(S);
        }

        return 0;
    }

    int32_t load_string(State* S, std::string_view str, bool optimize)
    {
        assert(S != nullptr && "State can not be null");

        return load_buffer(S, str, "<string>", optimize);
    }

} // namespace behl
