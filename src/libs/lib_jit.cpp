#include "behl.hpp"
#include "gc/gco_closure.hpp"
#include "gc/gco_proto.hpp"
#include "jit/jit.hpp"
#include "state.hpp"

namespace behl
{

    static int jit_status_fn(State* S)
    {
        push_boolean(S, jit_supported() && S->jit_enabled);
        return 1;
    }

    static int jit_optimized_fn(State* S)
    {
        check_type(S, 0, Type::kClosure);

        const Value& v = S->stack[static_cast<size_t>(resolve_index(S, 0))];
        push_boolean(S, v.get_closure()->proto->jit_code != nullptr);
        return 1;
    }

    static int jit_on_fn(State* S)
    {
        S->jit_enabled = true;
        return 0;
    }

    static int jit_off_fn(State* S)
    {
        S->jit_enabled = false;
        jit_clear_cache(S);
        return 0;
    }

    void load_lib_jit(State* S)
    {
        static constexpr ModuleReg jit_funcs[] = {
            { "status", jit_status_fn },
            { "optimized", jit_optimized_fn },
            { "on", jit_on_fn },
            { "off", jit_off_fn },
        };

        ModuleDef jit_module = { .funcs = jit_funcs };

        create_module(S, "jit", jit_module);
    }

} // namespace behl
