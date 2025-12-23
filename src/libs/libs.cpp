#include "behl.hpp"
#include "state.hpp"

namespace behl
{

    void load_stdlib(State* S, bool make_global)
    {
        load_lib_core(S);
        load_lib_table(S, make_global);
        load_lib_gc(S, make_global);
        load_lib_debug(S, make_global);
        load_lib_math(S, make_global);
        load_lib_os(S, make_global);
        load_lib_string(S, make_global);
    }

} // namespace behl
