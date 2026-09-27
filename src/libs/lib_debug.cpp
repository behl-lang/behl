#include "behl.hpp"
#include "common/print.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_table.hpp"
#include "state.hpp"
#include "vm/vm_debug.hpp"

namespace behl
{
    static int debug_stacktrace(State* S)
    {
        S->stack.push_back(S, Value(build_stacktrace_internal(S)));
        return 1;
    }

    void load_lib_debug(State* S)
    {
        static constexpr ModuleReg debug_funcs[] = {
            { "stacktrace", debug_stacktrace },
        };

        ModuleDef debug_module = { .funcs = debug_funcs };

        create_module(S, "debug", debug_module);
    }

} // namespace behl
