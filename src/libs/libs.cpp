#include "behl.hpp"
#include "state.hpp"

namespace behl
{

    void load_stdlib(State* S)
    {
        load_lib_core(S);
        load_lib_table(S);
        load_lib_gc(S);
        load_lib_debug(S);
        load_lib_math(S);
        load_lib_os(S);
        load_lib_string(S);
    }

} // namespace behl
