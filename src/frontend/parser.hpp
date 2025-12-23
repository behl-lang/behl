#pragma once

#include <behl/export.hpp>
#include <memory>
#include <span>
#include <string_view>

namespace behl
{
    class AstHolder;
    struct AstProgram;
    struct Token;

    BEHL_API AstProgram* parse(AstHolder& holder, std::span<const Token> tokens, std::string_view chunkname = "<script>",
        int max_line = -1, int max_column = -1);

} // namespace behl
