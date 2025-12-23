#pragma once

#include "common/string.hpp"

namespace behl
{
    struct State;
    struct AstProgram;
    struct Proto;
    struct GCProto;

    GCProto* compile(State* state, const AstProgram* program, std::string_view source_name = "<script>");

} // namespace behl
