#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace behl
{
    struct State;

    ptrdiff_t resolve_index(const State* S, int32_t idx);

    void call_unprotected(State* S, int32_t nargs, int32_t nresults);

    void load_unprotected(State* S, std::string_view str, std::string_view chunkname, bool optimize);

    int32_t push_error_from_exception(State* S);

} // namespace behl
