#include "frontend/lexer.hpp"

#include "common/ascii.hpp"
#include "common/vector.hpp"
#include "gc/gc.hpp"
#include "state.hpp"

#include <algorithm>
#include <array>
#include <behl/exceptions.hpp>
#include <cassert>
#include <functional>
#include <ranges>
#include <stdexcept>

namespace behl
{

    static constexpr auto kKeywords = std::invoke([]() {
        auto entries = std::to_array<std::pair<std::string_view, TokenType>>({
            { "let", TokenType::kLet },
            { "const", TokenType::kConst },
            { "if", TokenType::kIf },
            { "else", TokenType::kElse },
            { "elseif", TokenType::kElseIf },
            { "while", TokenType::kWhile },
            { "for", TokenType::kFor },
            { "foreach", TokenType::kForEach },
            { "function", TokenType::kFunction },
            { "return", TokenType::kReturn },
            { "break", TokenType::kBreak },
            { "continue", TokenType::kContinue },
            { "defer", TokenType::kDefer },
            { "true", TokenType::kTrue },
            { "false", TokenType::kFalse },
            { "nil", TokenType::kNil },
            { "in", TokenType::kIn },
            { "module", TokenType::kModule },
            { "export", TokenType::kExport },
            { "local", TokenType::kLocal },
        });

        std::ranges::sort(entries, {}, &std::pair<std::string_view, TokenType>::first);

        return entries;
    });

    std::optional<TokenType> find_keyword(std::string_view ident)
    {
        auto it = std::ranges::lower_bound(kKeywords, ident, {}, &std::pair<std::string_view, TokenType>::first);
        if (it != kKeywords.end() && it->first == ident)
        {
            return it->second;
        }
        return std::nullopt;
    }

    static constexpr bool is_excluded_identifier_char(char32_t c) noexcept
    {
        return c == 0x00A0 || (c >= 0x2000 && c <= 0x200D) || c == 0x2028 || c == 0x2029 || (c >= 0x202A && c <= 0x202E)
            || c == 0x2060 || (c >= 0x2066 && c <= 0x2069) || c == 0x3000 || c == 0xFEFF;
    }

    static constexpr bool is_identifier_start(char32_t c) noexcept
    {
        return is_ascii_alpha(c) || c == U'_' || (c >= 0x80 && !is_excluded_identifier_char(c));
    }

    static constexpr bool is_identifier_continue(char32_t c) noexcept
    {
        return is_identifier_start(c) || is_ascii_digit(c);
    }

    struct LexerState
    {
        std::string_view source;
        size_t pos = 0;
        int line = 1;
        int column = 1;
        GCString* chunkname;
    };

    static unsigned char continuation_byte(const LexerState& L, size_t pos)
    {
        assert(pos < L.source.size());
        unsigned char b = static_cast<unsigned char>(L.source[pos]);
        if ((b & 0xC0) != 0x80)
        {
            throw SyntaxError("Invalid UTF-8", SourceLocation(L.chunkname, L.line, L.column));
        }
        return b;
    }

    static char32_t decode_codepoint(const LexerState& L, size_t start_pos, size_t& bytes)
    {
        if (start_pos >= L.source.size())
        {
            bytes = 0;
            return U'\0';
        }
        unsigned char byte1 = static_cast<unsigned char>(L.source[start_pos]);
        if (byte1 < 0x80)
        {
            bytes = 1;
            return static_cast<char32_t>(byte1);
        }
        else if (byte1 < 0xC2)
        {
            throw SyntaxError("Invalid UTF-8", SourceLocation(L.chunkname, L.line, L.column));
        }
        else if (byte1 < 0xE0)
        {
            if (start_pos + 1 >= L.source.size())
            {
                throw SyntaxError("Invalid UTF-8", SourceLocation(L.chunkname, L.line, L.column));
            }
            bytes = 2;
            return static_cast<char32_t>((byte1 & 0x1F) << 6 | (continuation_byte(L, start_pos + 1) & 0x3F));
        }
        else if (byte1 < 0xF0)
        {
            if (start_pos + 2 >= L.source.size())
            {
                throw SyntaxError("Invalid UTF-8", SourceLocation(L.chunkname, L.line, L.column));
            }
            const unsigned char byte2 = continuation_byte(L, start_pos + 1);
            const unsigned char byte3 = continuation_byte(L, start_pos + 2);
            const char32_t cp = static_cast<char32_t>((byte1 & 0x0F) << 12 | (byte2 & 0x3F) << 6 | (byte3 & 0x3F));
            if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))
            {
                throw SyntaxError("Invalid UTF-8", SourceLocation(L.chunkname, L.line, L.column));
            }
            bytes = 3;
            return cp;
        }
        else if (byte1 < 0xF5)
        {
            if (start_pos + 3 >= L.source.size())
            {
                throw SyntaxError("Invalid UTF-8", SourceLocation(L.chunkname, L.line, L.column));
            }
            const unsigned char byte2 = continuation_byte(L, start_pos + 1);
            const unsigned char byte3 = continuation_byte(L, start_pos + 2);
            const unsigned char byte4 = continuation_byte(L, start_pos + 3);
            const char32_t cp = static_cast<char32_t>(
                (byte1 & 0x07) << 18 | (byte2 & 0x3F) << 12 | (byte3 & 0x3F) << 6 | (byte4 & 0x3F));
            if (cp < 0x10000 || cp > 0x10FFFF)
            {
                throw SyntaxError("Invalid UTF-8", SourceLocation(L.chunkname, L.line, L.column));
            }
            bytes = 4;
            return cp;
        }
        else
        {
            throw SyntaxError("Invalid UTF-8", SourceLocation(L.chunkname, L.line, L.column));
        }
    }

    static char32_t current_codepoint(const LexerState& L)
    {
        size_t bytes = 0;
        return decode_codepoint(L, L.pos, bytes);
    }

    static char32_t peek_codepoint(const LexerState& L)
    {
        size_t current_bytes = 0;
        decode_codepoint(L, L.pos, current_bytes);
        size_t peek_pos = L.pos + current_bytes;
        if (peek_pos >= L.source.size())
        {
            return U'\0';
        }
        size_t peek_bytes = 0;
        return decode_codepoint(L, peek_pos, peek_bytes);
    }

    static char32_t peek2_codepoint(const LexerState& L)
    {
        size_t bytes1 = 0;
        decode_codepoint(L, L.pos, bytes1);
        size_t pos2 = L.pos + bytes1;
        if (pos2 >= L.source.size())
        {
            return U'\0';
        }
        size_t bytes2 = 0;
        decode_codepoint(L, pos2, bytes2);
        size_t pos3 = pos2 + bytes2;
        if (pos3 >= L.source.size())
        {
            return U'\0';
        }
        size_t bytes3 = 0;
        return decode_codepoint(L, pos3, bytes3);
    }

    static char32_t advance_codepoint(LexerState& L)
    {
        size_t bytes = 0;
        char32_t cp = decode_codepoint(L, L.pos, bytes);
        L.pos += bytes;
        if (cp == '\n')
        {
            L.line++;
            L.column = 1;
        }
        else
        {
            L.column++;
        }
        return cp;
    }

    static void skip_whitespace(LexerState& L)
    {
        while (is_ascii_space(current_codepoint(L)))
        {
            advance_codepoint(L);
        }
    }

    static void skip_comment(LexerState& L)
    {
        char32_t c = current_codepoint(L);
        char32_t p = peek_codepoint(L);

        if (c == '/' && p == '/')
        {
            advance_codepoint(L);
            advance_codepoint(L);

            while (current_codepoint(L) != '\n' && current_codepoint(L) != U'\0')
            {
                advance_codepoint(L);
            }
        }

        else if (c == '/' && p == '*')
        {
            advance_codepoint(L);
            advance_codepoint(L);
            while (true)
            {
                c = current_codepoint(L);
                if (c == U'\0')
                {
                    break;
                }
                if (c == '*' && peek_codepoint(L) == '/')
                {
                    advance_codepoint(L);
                    advance_codepoint(L);
                    break;
                }
                advance_codepoint(L);
            }
        }
    }

    static Token scan_identifier(LexerState& L)
    {
        int start_line = L.line;
        int start_col = L.column;
        size_t start_pos = L.pos;
        while (is_identifier_continue(current_codepoint(L)))
        {
            size_t bytes = 0;
            decode_codepoint(L, L.pos, bytes);
            L.pos += bytes;
            L.column++;
        }
        std::string_view id = L.source.substr(start_pos, L.pos - start_pos);
        auto token_type = find_keyword(id);
        TokenType type = token_type.value_or(TokenType::kIdentifier);
        return { type, id, start_line, start_col };
    }

    static Token scan_number(LexerState& L)
    {
        int start_line = L.line;
        int start_col = L.column;
        size_t start_pos = L.pos;
        bool has_dot = false;
        bool is_hex = false;

        // Check for hexadecimal prefix 0x or 0X
        if (current_codepoint(L) == '0')
        {
            size_t bytes = 0;
            decode_codepoint(L, L.pos, bytes);
            L.pos += bytes;
            L.column++;

            char32_t next = current_codepoint(L);
            if (next == 'x' || next == 'X')
            {
                is_hex = true;
                decode_codepoint(L, L.pos, bytes);
                L.pos += bytes;
                L.column++;
            }
        }

        while (true)
        {
            char32_t c = current_codepoint(L);
            if (is_hex)
            {
                // Hexadecimal digits: 0-9, a-f, A-F
                if (is_ascii_hex_digit(c))
                {
                    size_t bytes = 0;
                    decode_codepoint(L, L.pos, bytes);
                    L.pos += bytes;
                    L.column++;
                }
                else
                {
                    break;
                }
            }
            else
            {
                if (is_ascii_digit(c))
                {
                    size_t bytes = 0;
                    decode_codepoint(L, L.pos, bytes);
                    L.pos += bytes;
                    L.column++;
                }
                else if (c == '.' && !has_dot)
                {
                    has_dot = true;
                    size_t bytes = 0;
                    decode_codepoint(L, L.pos, bytes);
                    L.pos += bytes;
                    L.column++;
                }
                else
                {
                    break;
                }
            }
        }
        std::string_view num = L.source.substr(start_pos, L.pos - start_pos);
        return { TokenType::kNumber, num, start_line, start_col };
    }

    static Token scan_string(LexerState& L)
    {
        int start_line = L.line;
        int start_col = L.column;
        char32_t quote = advance_codepoint(L);
        size_t start_pos = L.pos;

        while (current_codepoint(L) != quote)
        {
            char32_t c = current_codepoint(L);
            if (c == U'\0')
            {
                throw SyntaxError("Unterminated string", SourceLocation(L.chunkname, start_line, start_col));
            }
            if (c == '\\')
            {
                advance_codepoint(L); // skip backslash
                if (current_codepoint(L) != U'\0')
                {
                    advance_codepoint(L); // skip escaped character
                }
            }
            else
            {
                advance_codepoint(L);
            }
        }

        size_t end_pos = L.pos;
        std::string_view str = L.source.substr(start_pos, end_pos - start_pos);

        if (current_codepoint(L) != quote)
        {
            throw SyntaxError("Unterminated string", SourceLocation(L.chunkname, start_line, start_col));
        }
        advance_codepoint(L);
        return { TokenType::kString, str, start_line, start_col };
    }

    static Token scan_operator(LexerState& L)
    {
        int start_line = L.line;
        int start_col = L.column;
        char32_t c = current_codepoint(L);
        char32_t next = peek_codepoint(L);
        char32_t next2 = peek2_codepoint(L);
        std::string_view val;
        TokenType type;
        int advance_count = 1;
        switch (c)
        {
            case U'+':
                if (next == U'+')
                {
                    type = TokenType::kIncrement;
                    val = "++";
                    advance_count = 2;
                }
                else if (next == U'=')
                {
                    type = TokenType::kPlusAssign;
                    val = "+=";
                    advance_count = 2;
                }
                else
                {
                    type = TokenType::kPlus;
                    val = "+";
                }
                break;
            case U'-':
                if (next == U'-')
                {
                    type = TokenType::kDecrement;
                    val = "--";
                    advance_count = 2;
                }
                else if (next == U'=')
                {
                    type = TokenType::kMinusAssign;
                    val = "-=";
                    advance_count = 2;
                }
                else
                {
                    type = TokenType::kMinus;
                    val = "-";
                }
                break;
            case U'*':
                if (next == U'=')
                {
                    type = TokenType::kStarAssign;
                    val = "*=";
                    advance_count = 2;
                }
                else if (next == U'*')
                {
                    type = TokenType::kPower;
                    val = "**";
                    advance_count = 2;
                }
                else
                {
                    type = TokenType::kStar;
                    val = "*";
                }
                break;
            case U'/':
                if (next == U'=')
                {
                    type = TokenType::kSlashAssign;
                    val = "/=";
                    advance_count = 2;
                }
                else
                {
                    type = TokenType::kSlash;
                    val = "/";
                }
                break;
            case U'%':
                if (next == U'=')
                {
                    type = TokenType::kPercentAssign;
                    val = "%=";
                    advance_count = 2;
                }
                else
                {
                    type = TokenType::kPercent;
                    val = "%";
                }
                break;
            case U'^':
                type = TokenType::kBXor;
                val = "^";
                break;
            case U'#':
                type = TokenType::kHash;
                val = "#";
                break;
            case U'(':
                type = TokenType::kLParen;
                val = "(";
                break;
            case U')':
                type = TokenType::kRParen;
                val = ")";
                break;
            case U'{':
                type = TokenType::kLBrace;
                val = "{";
                break;
            case U'}':
                type = TokenType::kRBrace;
                val = "}";
                break;
            case U'[':
                type = TokenType::kLBracket;
                val = "[";
                break;
            case U']':
                type = TokenType::kRBracket;
                val = "]";
                break;
            case U';':
                type = TokenType::kSemi;
                val = ";";
                break;
            case U':':
                type = TokenType::kColon;
                val = ":";
                break;
            case U'?':
                type = TokenType::kQuestion;
                val = "?";
                break;
            case U',':
                type = TokenType::kComma;
                val = ",";
                break;
            case U'.':
                if (next == U'.')
                {
                    if (next2 == U'.')
                    {
                        val = "...";
                        type = TokenType::kVarArg;
                        advance_count = 3;
                    }
                    else
                    {
                        val = "..";
                        type = TokenType::kConcat;
                        advance_count = 2;
                    }
                }
                else
                {
                    val = ".";
                    type = TokenType::kDot;
                }
                break;
            case U'=':
                if (next == U'=')
                {
                    val = "==";
                    type = TokenType::kEq;
                    advance_count = 2;
                }
                else
                {
                    val = "=";
                    type = TokenType::kAssign;
                }
                break;
            case U'!':
                if (next == U'=')
                {
                    val = "!=";
                    type = TokenType::kNe;
                    advance_count = 2;
                }
                else
                {
                    val = "!";
                    type = TokenType::kNotOp;
                }
                break;
            case U'&':
                if (next == U'&')
                {
                    val = "&&";
                    type = TokenType::kAndOp;
                    advance_count = 2;
                }
                else
                {
                    val = "&";
                    type = TokenType::kBAnd;
                }
                break;
            case U'|':
                if (next == U'|')
                {
                    val = "||";
                    type = TokenType::kOrOp;
                    advance_count = 2;
                }
                else
                {
                    val = "|";
                    type = TokenType::kBOr;
                }
                break;
            case U'<':
                if (next == U'<')
                {
                    val = "<<";
                    type = TokenType::kBShl;
                    advance_count = 2;
                }
                else if (next == U'=')
                {
                    val = "<=";
                    type = TokenType::kLe;
                    advance_count = 2;
                }
                else
                {
                    val = "<";
                    type = TokenType::kLt;
                }
                break;
            case U'>':
                if (next == U'>')
                {
                    val = ">>";
                    type = TokenType::kBShr;
                    advance_count = 2;
                }
                else if (next == U'=')
                {
                    val = ">=";
                    type = TokenType::kGe;
                    advance_count = 2;
                }
                else
                {
                    val = ">";
                    type = TokenType::kGt;
                }
                break;
            case U'~':
                val = "~";
                type = TokenType::kBNot;
                break;
            default:
                throw SyntaxError("Unexpected character", SourceLocation(L.chunkname, start_line, start_col));
        }
        for (int i = 0; i < advance_count; ++i)
        {
            advance_codepoint(L);
        }
        return { type, val, start_line, start_col };
    }

    AutoVector<Token> tokenize(State* state, std::string_view source, std::string_view chunkname)
    {
        GCString* chunk = gc_new_string(state, chunkname.empty() ? std::string_view("<script>") : chunkname);
        LexerState L{ source, 0, 1, 1, chunk };

        AutoVector<Token> tokens(state);

        const auto estimated_tokens = source.size() / 4;
        tokens.reserve(estimated_tokens);

        while (L.pos < L.source.size())
        {
            skip_whitespace(L);
            if (L.pos >= L.source.size())
            {
                break;
            }

            char32_t c = current_codepoint(L);
            char32_t p = peek_codepoint(L);
            if (c == '/' && (p == '/' || p == '*'))
            {
                skip_comment(L);
                continue;
            }

            if (is_identifier_start(c))
            {
                tokens.push_back(scan_identifier(L));
            }
            else if (is_ascii_digit(c) || (c == '.' && is_ascii_digit(peek_codepoint(L))))
            {
                tokens.push_back(scan_number(L));
            }
            else if (c == '"' || c == '\'')
            {
                tokens.push_back(scan_string(L));
            }
            else
            {
                tokens.push_back(scan_operator(L));
            }
        }
        tokens.push_back({ TokenType::kEOF, "", L.line, L.column });
        return tokens;
    }

} // namespace behl
