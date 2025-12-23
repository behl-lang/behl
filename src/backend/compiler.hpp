#pragma once

#include "behl/export.hpp"
#include "common/string.hpp"

namespace behl
{
    struct State;
    struct AstProgram;
    struct Proto;
    struct GCProto;

    BEHL_API GCProto* compile(State* state, const AstProgram* program, std::string_view source_name = "<script>");

} // namespace behl
