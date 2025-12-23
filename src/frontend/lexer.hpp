#pragma once

#include "common/string.hpp"
#include "common/vector.hpp"
#include <behl/export.hpp>

#include <string>
#include <string_view>

namespace behl
{
    struct State;

    enum class TokenType
    {
        kEOF,
        kIdentifier,
        kNumber,
        kString,
        kPlus,
        kMinus,
        kStar,
        kPower,
        kSlash,
        kPercent,
        kCaret,
        kHash,
        kEq,
        kNe,
        kLt,
        kLe,
        kGt,
        kGe,
        kAssign,
        kPlusAssign,
        kMinusAssign,
        kStarAssign,
        kSlashAssign,
        kPercentAssign,
        kIncrement,
        kDecrement,
        kLParen,
        kRParen,
        kLBrace,
        kRBrace,
        kLBracket,
        kRBracket,
        kSemi,
        kColon,
        kQuestion,
        kComma,
        kDot,
        kConcat,
        kVarArg,
        kAndOp,
        kOrOp,
        kNotOp,
        kBAnd,
        kBOr,
        kBXor,
        kBShl,
        kBShr,
        kBNot,
        kLet,
        kConst,
        kIf,
        kElse,
        kElseIf,
        kWhile,
        kFor,
        kForEach,
        kFunction,
        kReturn,
        kBreak,
        kContinue,
        kDefer,
        kTrue,
        kFalse,
        kNil,
        kNot,
        kIn,
        kModule,
        kExport,
        kImport,
        kLocal,
    };

    struct Token
    {
        TokenType type;
        std::string_view value;
        int line;
        int column;
    };

    BEHL_API AutoVector<Token> tokenize(State* state, std::string_view source, std::string_view chunkname = "<script>");

} // namespace behl
