#include "frontend/parser.hpp"

#include "ast/ast.hpp"
#include "ast/ast_holder.hpp"
#include "common/charconv_compat.hpp"
#include "frontend/lexer.hpp"

#include <behl/exceptions.hpp>
#include <charconv>
#include <stdexcept>

namespace behl
{

    template<typename T, typename... TArgs>
    T* make_node(AstHolder& holder, const Token& tok, TArgs&&... args)
    {
        auto* node = holder.make<T>(std::forward<TArgs>(args)...);
        node->line = tok.line;
        node->column = tok.column;
        return node;
    }

    template<typename T, typename... TArgs>
    T* make_node(AstHolder& holder, int line, int column, TArgs&&... args)
    {
        auto* node = holder.make<T>(std::forward<TArgs>(args)...);
        node->line = line;
        node->column = column;
        return node;
    }

    // Process escape sequences in a string_view and return a new String
    static AstString* process_string_escapes(AstHolder& holder, std::string_view raw_str)
    {
        auto* str = holder.make_string(raw_str);
        auto* buf = str->data;
        size_t write_pos = 0;

        for (size_t i = 0; i < raw_str.size(); ++i)
        {
            if (raw_str[i] == '\\')
            {
                if (i + 1 < raw_str.size())
                {
                    ++i;
                    switch (raw_str[i])
                    {
                        case 'a':
                            buf[write_pos++] = '\a';
                            break;
                        case 'b':
                            buf[write_pos++] = '\b';
                            break;
                        case 'f':
                            buf[write_pos++] = '\f';
                            break;
                        case 'n':
                            buf[write_pos++] = '\n';
                            break;
                        case 'r':
                            buf[write_pos++] = '\r';
                            break;
                        case 't':
                            buf[write_pos++] = '\t';
                            break;
                        case 'v':
                            buf[write_pos++] = '\v';
                            break;
                        case '\\':
                            buf[write_pos++] = '\\';
                            break;
                        case '\'':
                            buf[write_pos++] = '\'';
                            break;
                        case '"':
                            buf[write_pos++] = '"';
                            break;
                        case '\n':
                            break; // escaped newline
                        default:
                            buf[write_pos++] = raw_str[i];
                            break;
                    }
                }
            }
            else
            {
                buf[write_pos++] = raw_str[i];
            }
        }

        // Adjust the length to the actual written size
        str->length = write_pos;

        return str;
    }

    struct ParserState
    {
        AstHolder& holder;
        std::span<const Token> tokens;
        size_t pos = 0;
        std::string_view chunkname;
        int max_line = -1;   // -1 means parse entire file
        int max_column = -1; // -1 means parse entire line, otherwise stop at this column
    };

    static Token current(const ParserState& P)
    {
        if (P.pos >= P.tokens.size())
        {
            return { TokenType::kEOF, "", -1, -1 };
        }
        return P.tokens[P.pos];
    }

    static Token previous(const ParserState& P)
    {
        if (P.pos == 0)
        {
            return { TokenType::kEOF, "", -1, -1 };
        }
        return P.tokens[P.pos - 1];
    }

    static Token peek(const ParserState& P)
    {
        if (P.pos + 1 >= P.tokens.size())
        {
            return { TokenType::kEOF, "", -1, -1 };
        }
        return P.tokens[P.pos + 1];
    }

    static Token advance(ParserState& P)
    {
        // Check if we're about to advance past the cursor position
        if (P.max_line >= 0 && P.pos < P.tokens.size())
        {
            Token tok = P.tokens[P.pos];
            if (tok.line > P.max_line || (tok.line == P.max_line && P.max_column >= 0 && tok.column >= P.max_column))
            {
                // Don't advance past cursor - return EOF with cursor position to signal end of parsing
                return { TokenType::kEOF, "", P.max_line, P.max_column };
            }
        }

        if (P.pos < P.tokens.size())
        {
            ++P.pos;
        }
        return previous(P);
    }

    static bool check(const ParserState& P, TokenType type)
    {
        return current(P).type == type;
    }

    static bool match(ParserState& P, std::initializer_list<TokenType> types)
    {
        for (auto t : types)
        {
            if (check(P, t))
            {
                advance(P);
                return true;
            }
        }
        return false;
    }

    static Token consume(ParserState& P, TokenType type, std::string_view err_msg)
    {
        if (check(P, type))
        {
            return advance(P);
        }
        Token tok = current(P);
        SourceLocation loc(P.chunkname, tok.line, tok.column);
        throw SyntaxError(err_msg, loc);
    }

    [[noreturn]] static void error(const ParserState& P, std::string_view msg)
    {
        Token tok = current(P);
        SourceLocation loc(P.chunkname, tok.line, tok.column);
        throw SyntaxError(msg, loc);
    }

    // Helper to append a statement to a block (using linked list)
    static void append_to_block(AstBlock& block, AstNode* stat)
    {
        if (!block.first_stat)
        {
            block.first_stat = stat;
        }
        else
        {
            AstNode* curr = block.first_stat;
            while (curr->next_child)
            {
                curr = curr->next_child;
            }
            curr->next_child = stat;
        }
    }

    static AstBlock* parse_block(ParserState& P);
    static AstNode* parse_stat(ParserState& P);
    static AstNode* parse_expr(ParserState& P);
    static AstNode* parse_primary(ParserState& P);
    static AstNode* parse_postfix(ParserState& P);
    static AstNode* parse_unary(ParserState& P);
    static AstNode* parse_bin(ParserState& P, int min_prec);
    static AstNode* parse_func_body(ParserState& P, const Token& tok);
    static AstNode* parse_table_ctor(ParserState& P, const Token& tok);
    static AstFuncCall* parse_func_call(ParserState& P, AstNode* func);
    static void parse_expr_list(ParserState& P, AstNode*& first_expr);
    static void parse_name_list(ParserState& P, AstString*& first_name);
    static AstNode* parse_if(ParserState& P);
    static AstNode* parse_while(ParserState& P);
    static AstNode* parse_for(ParserState& P);
    static AstNode* parse_foreach(ParserState& P);
    static AstNode* parse_local(ParserState& P);
    static AstNode* parse_return(ParserState& P);

    static void parse_params_in_parens(
        ParserState& P, AstString*& out_first_param, bool& is_vararg, const char* expect_lparen_msg);
    static bool is_block_boundary_token(TokenType t);
    static bool is_statement_boundary_token(TokenType t);
    static void parse_name_parts(ParserState& P, AstNode*& first_name_part, bool& is_method);
    static void parse_params_in_parens(
        ParserState& P, AstString*& out_first_param, bool& is_vararg, const char* expect_lparen_msg)
    {
        is_vararg = false;
        consume(P, TokenType::kLParen, expect_lparen_msg);
        out_first_param = nullptr;
        if (!check(P, TokenType::kRParen))
        {
            AstNode** tail = reinterpret_cast<AstNode**>(&out_first_param);
            do
            {
                if (check(P, TokenType::kIdentifier))
                {
                    auto* param = P.holder.make_string(advance(P).value);
                    *tail = param;
                    tail = &param->next_child;
                }
                else if (check(P, TokenType::kVarArg))
                {
                    advance(P);
                    is_vararg = true;
                    break;
                }
                else
                {
                    error(P, "Expected parameter name or '...'");
                }
            } while (match(P, { TokenType::kComma }));
        }
        consume(P, TokenType::kRParen, "Expected ')' after parameters");
    }

    static bool is_block_boundary_token(TokenType t)
    {
        switch (t)
        {
            case TokenType::kEOF:
            case TokenType::kRBrace:
            case TokenType::kElse:
            case TokenType::kElseIf:
                return true;
            default:
                return false;
        }
    }

    static bool is_statement_boundary_token(TokenType t)
    {
        return t == TokenType::kSemi || is_block_boundary_token(t);
    }

    static void parse_name_parts(ParserState& P, AstNode*& first_name_part, bool& is_method)
    {
        is_method = false;
        auto first = consume(P, TokenType::kIdentifier, "Expected function name").value;
        first_name_part = P.holder.make_string(first);
        AstNode** tail = &first_name_part->next_child;
        while (match(P, { TokenType::kDot }))
        {
            *tail = P.holder.make_string(consume(P, TokenType::kIdentifier, "Expected identifier").value);
            tail = &(*tail)->next_child;
        }
        if (match(P, { TokenType::kColon }))
        {
            is_method = true;
            *tail = P.holder.make_string(consume(P, TokenType::kIdentifier, "Expected method name").value);
        }
    }

    struct OpPrec
    {
        int prec;
        bool right_assoc;
    };

    static OpPrec get_prec(TokenType t)
    {
        switch (t)
        {
            case TokenType::kQuestion:
                return { 1, true }; // Ternary operator - lowest precedence, right-associative
            case TokenType::kOrOp:
                return { 2, false };
            case TokenType::kAndOp:
                return { 3, false };
            case TokenType::kLt:
            case TokenType::kGt:
            case TokenType::kLe:
            case TokenType::kGe:
            case TokenType::kEq:
            case TokenType::kNe:
                return { 4, false };
            case TokenType::kBOr:
                return { 5, false };
            case TokenType::kBXor:
                return { 6, false };
            case TokenType::kBAnd:
                return { 7, false };
            case TokenType::kBShl:
            case TokenType::kBShr:
                return { 8, false };
            case TokenType::kPlus:
            case TokenType::kMinus:
                return { 10, false };
            case TokenType::kStar:
            case TokenType::kSlash:
            case TokenType::kPercent:
                return { 11, false };
            case TokenType::kPower:
                return { 12, true }; // Right-associative, higher precedence than multiplication
            default:
                return { -1, false };
        }
    }

    AstProgram* parse(
        AstHolder& holder, std::span<const Token> tokens, std::string_view chunkname, int max_line, int max_column)
    {
        ParserState P{ holder, tokens, 0, chunkname, max_line, max_column };
        auto* prog = holder.make<AstProgram>();

        // Check for module declaration (must be first statement)
        if (check(P, TokenType::kModule))
        {
            advance(P); // consume 'module'
            consume(P, TokenType::kSemi, "Expected ';' after module declaration");
            prog->is_module = true;
        }

        prog->block = parse_block(P);
        // Only check for EOF if parsing entire file
        if (max_line < 0 && !check(P, TokenType::kEOF))
        {
            error(P, "Unexpected tokens after end of program");
        }

        return prog;
    }

    static AstNode* parse_primary(ParserState& P)
    {
        // Check position limit before accessing tokens
        if (P.max_line >= 0)
        {
            Token tok = current(P);
            if (tok.line > P.max_line)
            {
                return make_node<AstNil>(P.holder, tok);
            }
            if (tok.line == P.max_line && P.max_column >= 0 && tok.column >= P.max_column)
            {
                return make_node<AstNil>(P.holder, tok);
            }
        }

        if (match(P, { TokenType::kNil }))
        {
            return make_node<AstNil>(P.holder, previous(P));
        }
        if (match(P, { TokenType::kFalse }))
        {
            return make_node<AstBool>(P.holder, previous(P), false);
        }
        if (match(P, { TokenType::kTrue }))
        {
            return make_node<AstBool>(P.holder, previous(P), true);
        }
        if (check(P, TokenType::kNumber))
        {
            auto tok = advance(P);

            // Check for hexadecimal prefix
            bool is_hex = (tok.value.size() > 2 && tok.value[0] == '0' && (tok.value[1] == 'x' || tok.value[1] == 'X'));

            if (tok.value.find('.') == tok.value.npos)
            {
                int64_t ival = 0;
                std::from_chars_result res;

                if (is_hex)
                {
                    // Parse hexadecimal (skip "0x" or "0X" prefix)
                    res = behl::from_chars(tok.value.data() + 2, tok.value.data() + tok.value.size(), ival, 16);
                }
                else
                {
                    // Parse decimal
                    res = behl::from_chars(tok.value.data(), tok.value.data() + tok.value.size(), ival);
                }

                if (res.ec == std::errc{})
                {
                    return make_node<AstInt>(P.holder, tok, ival);
                }

                double fval = 0.0;
                behl::from_chars(tok.value.data(), tok.value.data() + tok.value.size(), fval);
                return make_node<AstFP>(P.holder, tok, fval);
            }
            else
            {
                double fval = 0.0;
                behl::from_chars(tok.value.data(), tok.value.data() + tok.value.size(), fval);
                return make_node<AstFP>(P.holder, tok, fval);
            }
        }
        if (check(P, TokenType::kString))
        {
            auto tok = advance(P);
            auto* node = process_string_escapes(P.holder, tok.value);
            node->line = tok.line;
            node->column = tok.column;
            return node;
        }
        if (check(P, TokenType::kIdentifier))
        {
            auto tok = advance(P);
            auto* name_str = P.holder.make_string(tok.value);
            name_str->line = tok.line;
            name_str->column = tok.column;
            return make_node<AstIdent>(P.holder, tok, name_str);
        }
        if (match(P, { TokenType::kFunction }))
        {
            auto tok = previous(P);
            return parse_func_body(P, tok);
        }
        if (match(P, { TokenType::kLBrace }))
        {
            auto tok = previous(P);
            return parse_table_ctor(P, tok);
        }
        if (match(P, { TokenType::kLParen }))
        {
            auto expr = parse_expr(P);
            consume(P, TokenType::kRParen, "Expected ')'");
            return expr;
        }
        if (match(P, { TokenType::kVarArg }))
        {
            auto tok = previous(P);
            return make_node<AstVararg>(P.holder, tok);
        }
        Token tok = current(P);
        SourceLocation loc(P.chunkname, tok.line, tok.column);
        throw SyntaxError("Unexpected token in expression", loc);
    }

    static AstNode* parse_postfix(ParserState& P)
    {
        auto left = parse_primary(P);
        while (true)
        {
            if (match(P, { TokenType::kLBracket }))
            {
                auto tok = previous(P);
                auto key = parse_expr(P);
                consume(P, TokenType::kRBracket, "Expected ']' ");
                left = make_node<AstIndex>(P.holder, tok, left, key);
            }
            else if (match(P, { TokenType::kDot }))
            {
                auto tok = previous(P);
                auto name = consume(P, TokenType::kIdentifier, "Expected identifier after '.'").value;
                auto* name_str = P.holder.make_string(name);
                name_str->line = tok.line;
                name_str->column = tok.column;
                left = make_node<AstMember>(P.holder, tok, left, name_str);
            }
            else if (check(P, TokenType::kColon))
            {
                // Method call syntax (table:method()) can be ambiguous with ternary operator
                // Only treat as method call if:
                // 1. Pattern matches ': identifier ('
                // 2. Left side is NOT a function call (to avoid ambiguity with ternary)
                //    Function call results shouldn't use method syntax anyway
                bool is_func_call_result = (left->type == AstNodeType::kFuncCall);

                if (!is_func_call_result && P.pos + 1 < P.tokens.size() && P.tokens[P.pos + 1].type == TokenType::kIdentifier
                    && P.pos + 2 < P.tokens.size() && P.tokens[P.pos + 2].type == TokenType::kLParen)
                {
                    // This IS a method call - consume and parse it
                    auto tok = advance(P); // consume ':'
                    auto name = consume(P, TokenType::kIdentifier, "Expected identifier after ':'").value;
                    auto table_clone = left->clone(P.holder);
                    auto* name_str = P.holder.make_string(name);
                    name_str->line = tok.line;
                    name_str->column = tok.column;
                    auto mem = make_node<AstMember>(P.holder, tok, left, name_str);
                    auto call = parse_func_call(P, mem);
                    // Insert table_clone as first arg (for method call syntax)
                    table_clone->next_child = call->first_arg;
                    call->first_arg = table_clone;
                    left = call;
                }
                else
                {
                    // Not a method call - could be ternary operator's ':'
                    break;
                }
            }
            else if (check(P, TokenType::kLParen))
            {
                left = parse_func_call(P, left);
            }
            else
            {
                break;
            }
        }
        return left;
    }

    static AstNode* parse_unary(ParserState& P)
    {
        if (match(P, { TokenType::kMinus, TokenType::kNotOp, TokenType::kHash, TokenType::kBNot }))
        {
            auto tok = previous(P);
            auto op = tok.type;
            auto expr = parse_unary(P);
            return make_node<AstUnOp>(P.holder, tok, op, expr);
        }
        return parse_postfix(P);
    }

    static AstNode* parse_bin(ParserState& P, int min_prec)
    {
        auto left = parse_unary(P);
        while (true)
        {
            auto p = get_prec(current(P).type);
            if (p.prec < min_prec)
            {
                break;
            }

            // Special handling for ternary operator
            if (current(P).type == TokenType::kQuestion)
            {
                auto op_tok = advance(P);
                // Parse true branch - recursively call parse_bin but skip ternary operators
                // by using precedence 2 (higher than ternary which is 1)
                // This allows all operators except another ternary in the true branch
                auto true_expr = parse_bin(P, p.prec + 1);
                consume(P, TokenType::kColon, "Expected ':' in ternary expression");
                int next_min = p.right_assoc ? p.prec : p.prec + 1;
                auto false_expr = parse_bin(P, next_min); // Parse the false branch
                auto ternary = make_node<AstTernary>(P.holder, op_tok, left, true_expr, false_expr);
                left = ternary;
            }
            else
            {
                auto op_tok = advance(P);
                int next_min = p.right_assoc ? p.prec : p.prec + 1;
                auto right = parse_bin(P, next_min);
                auto binop = make_node<AstBinOp>(P.holder, op_tok, op_tok.type, left, right);
                left = binop;
            }
        }
        return left;
    }

    static AstNode* parse_func_body(ParserState& P, const Token& tok)
    {
        auto fd = make_node<AstFuncDef>(P.holder, tok);
        parse_params_in_parens(P, fd->first_param, fd->is_vararg, "Expected '(' for function parameters");
        consume(P, TokenType::kLBrace, "Expected '{' to open function body");
        fd->block = parse_block(P);
        consume(P, TokenType::kRBrace, "Expected '}' to close function");
        return fd;
    }

    static AstNode* parse_table_ctor(ParserState& P, const Token& tok)
    {
        auto tc = make_node<AstTableCtor>(P.holder, tok);
        if (check(P, TokenType::kRBrace))
        {
            advance(P);
            return tc;
        }
        // Build linked list of TableField nodes
        TableField** tail = &tc->first_field;
        do
        {
            AstNode* key = nullptr;
            AstNode* val = nullptr;
            if (check(P, TokenType::kLBracket))
            {
                advance(P);
                key = parse_expr(P);
                consume(P, TokenType::kRBracket, "Expected ']' ");
                consume(P, TokenType::kAssign, "Expected '='");
                val = parse_expr(P);
            }
            else if (check(P, TokenType::kIdentifier) && peek(P).type == TokenType::kAssign)
            {
                auto id_tok = advance(P);
                auto* name_str = P.holder.make_string(id_tok.value);
                name_str->line = id_tok.line;
                name_str->column = id_tok.column;
                key = make_node<AstIdent>(P.holder, id_tok, name_str);
                advance(P);
                val = parse_expr(P);
            }
            else if (check(P, TokenType::kVarArg))
            {
                // Vararg expansion: {...} becomes array elements
                auto vararg_tok = advance(P);
                val = make_node<AstVararg>(P.holder, vararg_tok);
            }
            else
            {
                val = parse_expr(P);
            }
            auto* field = make_node<TableField>(P.holder, tok, key, val);
            *tail = field;
            tail = reinterpret_cast<TableField**>(&field->next_child);
        } while (match(P, { TokenType::kComma, TokenType::kSemi }));
        consume(P, TokenType::kRBrace, "Expected '}' to close table");
        return tc;
    }

    static AstFuncCall* parse_func_call(ParserState& P, AstNode* func)
    {
        auto tok = current(P);
        auto call = make_node<AstFuncCall>(P.holder, tok, func);
        consume(P, TokenType::kLParen, "Expected '(' for function call");
        if (!check(P, TokenType::kRParen))
        {
            // Build linked list of args
            AstNode** tail = &call->first_arg;
            *tail = parse_expr(P);
            tail = &(*tail)->next_child;
            while (match(P, { TokenType::kComma }))
            {
                *tail = parse_expr(P);
                tail = &(*tail)->next_child;
            }
        }
        consume(P, TokenType::kRParen, "Expected ')'");
        return call;
    }

    static void parse_expr_list(ParserState& P, AstNode*& first_expr)
    {
        first_expr = parse_expr(P);
        AstNode** tail = &first_expr->next_child;
        while (match(P, { TokenType::kComma }))
        {
            *tail = parse_expr(P);
            tail = &(*tail)->next_child;
        }
    }

    static void parse_name_list(ParserState& P, AstString*& first_name)
    {
        first_name = P.holder.make_string(consume(P, TokenType::kIdentifier, "Expected identifier").value);
        AstNode** tail = &first_name->next_child;
        while (match(P, { TokenType::kComma }))
        {
            *tail = P.holder.make_string(consume(P, TokenType::kIdentifier, "Expected identifier").value);
            tail = &(*tail)->next_child;
        }
    }

    static AstNode* parse_if(ParserState& P)
    {
        auto tok = advance(P);
        consume(P, TokenType::kLParen, "Expected '(' after 'if'");
        auto iff = make_node<AstIf>(P.holder, tok, parse_expr(P));
        consume(P, TokenType::kRParen, "Expected ')' after if condition");

        // Accept either braced block or single statement
        if (match(P, { TokenType::kLBrace }))
        {
            iff->then_block = parse_block(P);
            consume(P, TokenType::kRBrace, "Expected '}'");
        }
        else
        {
            iff->then_block = P.holder.make<AstBlock>();
            append_to_block(*iff->then_block, parse_stat(P));
            // Consume optional semicolon after single statement
            match(P, { TokenType::kSemi });
        }

        ElseIf** elseif_tail = &iff->first_elseif;
        while (match(P, { TokenType::kElseIf }))
        {
            consume(P, TokenType::kLParen, "Expected '(' after 'elseif'");
            auto cond = parse_expr(P);
            consume(P, TokenType::kRParen, "Expected ')' after elseif condition");

            auto elseif = P.holder.make<ElseIf>();
            elseif->cond = cond;

            if (match(P, { TokenType::kLBrace }))
            {
                elseif->block = parse_block(P);
                consume(P, TokenType::kRBrace, "Expected '}'");
            }
            else
            {
                elseif->block = P.holder.make<AstBlock>();
                append_to_block(*elseif->block, parse_stat(P));
                // Consume optional semicolon after single statement
                match(P, { TokenType::kSemi });
            }

            *elseif_tail = elseif;
            elseif_tail = reinterpret_cast<ElseIf**>(&elseif->next_child);
        }

        if (match(P, { TokenType::kElse }))
        {
            if (match(P, { TokenType::kLBrace }))
            {
                iff->else_block = parse_block(P);
                consume(P, TokenType::kRBrace, "Expected '}'");
            }
            else
            {
                iff->else_block = P.holder.make<AstBlock>();
                append_to_block(*iff->else_block, parse_stat(P));
            }
        }
        return iff;
    }

    static AstNode* parse_while(ParserState& P)
    {
        auto tok = advance(P);
        consume(P, TokenType::kLParen, "Expected '(' after 'while'");
        auto wh = make_node<AstWhile>(P.holder, tok, parse_expr(P));
        consume(P, TokenType::kRParen, "Expected ')' after while condition");

        // Accept either braced block or single statement
        if (match(P, { TokenType::kLBrace }))
        {
            wh->block = parse_block(P);
            consume(P, TokenType::kRBrace, "Expected '}'");
        }
        else
        {
            wh->block = P.holder.make<AstBlock>();
            append_to_block(*wh->block, parse_stat(P));
        }
        return wh;
    }

    static AstNode* parse_for(ParserState& P)
    {
        auto tok = advance(P);
        consume(P, TokenType::kLParen, "Expected '(' after 'for'");

        // Check for C-style for loop with 'let' or 'const'
        if (check(P, TokenType::kLet) || check(P, TokenType::kConst))
        {
            size_t checkpoint = P.pos;
            advance(P); // consume 'let' or 'const'

            // Check if this is for-in syntax (for (let v in expr) or for (let k,v in expr))
            if (check(P, TokenType::kIdentifier))
            {
                AstString* names = nullptr;
                parse_name_list(P, names);

                if (match(P, { TokenType::kIn }))
                {
                    // This is a for-in loop with let declaration
                    AstNode* exprs = nullptr;
                    parse_expr_list(P, exprs);
                    consume(P, TokenType::kRParen, "Expected ')'");
                    auto fori = make_node<AstForIn>(P.holder, tok);
                    fori->declares_variables = true; // Variables declared inline with 'let'
                    fori->first_name = names;
                    fori->first_expr = exprs;

                    if (match(P, { TokenType::kLBrace }))
                    {
                        fori->block = parse_block(P);
                        consume(P, TokenType::kRBrace, "Expected '}'");
                    }
                    else
                    {
                        fori->block = P.holder.make<AstBlock>();
                        append_to_block(*fori->block, parse_stat(P));
                    }
                    return fori;
                }
            }

            // Not for-in, restore position and parse as C-style for with declaration
            P.pos = checkpoint;

            // Parse C-style for loop with let declaration
            // Support: let i = 0, j = 10, k = 20
            bool is_const = false;
            Token let_tok;
            if (match(P, { TokenType::kConst }))
            {
                is_const = true;
                let_tok = previous(P);
            }
            else
            {
                let_tok = advance(P); // consume 'let'
            }

            // Parse variables: name = expr, name = expr, ...
            // Build linked lists for names and inits
            AstString* first_name = P.holder.make_string(consume(P, TokenType::kIdentifier, "Expected identifier").value);
            AstNode* first_init = nullptr;
            if (match(P, { TokenType::kAssign }))
            {
                first_init = parse_expr(P);
            }

            AstString** name_tail = reinterpret_cast<AstString**>(&first_name->next_child);
            AstNode** init_tail = first_init ? &first_init->next_child : &first_init;

            // Parse additional variables if comma-separated
            while (match(P, { TokenType::kComma }))
            {
                *name_tail = P.holder.make_string(consume(P, TokenType::kIdentifier, "Expected identifier").value);
                name_tail = reinterpret_cast<AstString**>(&(*name_tail)->next_child);

                if (match(P, { TokenType::kAssign }))
                {
                    *init_tail = parse_expr(P);
                    init_tail = &(*init_tail)->next_child;
                }
                else
                {
                    // No initializer for this variable, add nil placeholder
                    *init_tail = make_node<AstNil>(P.holder, current(P));
                    init_tail = &(*init_tail)->next_child;
                }
            }

            AstNode* init = make_node<AstLocalDecl>(P.holder, let_tok, is_const, first_name, first_init);
            consume(P, TokenType::kSemi, "Expected ';' after for loop initialization");

            auto condition = parse_expr(P);
            consume(P, TokenType::kSemi, "Expected ';' after for loop condition");

            AstNode* update = nullptr;
            if (!check(P, TokenType::kRParen))
            {
                // Parse update expressions, potentially comma-separated: i++, j--
                auto update_block = P.holder.make<AstBlock>();
                bool has_multiple = false;

                auto parse_single_update = [&]() -> AstNode* {
                    auto update_expr = parse_postfix(P);

                    if (match(P,
                            { TokenType::kPlusAssign, TokenType::kMinusAssign, TokenType::kStarAssign, TokenType::kSlashAssign,
                                TokenType::kPercentAssign }))
                    {
                        TokenType op_type = TokenType::kPlus;
                        TokenType compound_op = previous(P).type;
                        if (compound_op == TokenType::kPlusAssign)
                        {
                            op_type = TokenType::kPlus;
                        }
                        else if (compound_op == TokenType::kMinusAssign)
                        {
                            op_type = TokenType::kMinus;
                        }
                        else if (compound_op == TokenType::kStarAssign)
                        {
                            op_type = TokenType::kStar;
                        }
                        else if (compound_op == TokenType::kSlashAssign)
                        {
                            op_type = TokenType::kSlash;
                        }
                        else if (compound_op == TokenType::kPercentAssign)
                        {
                            op_type = TokenType::kPercent;
                        }

                        auto rhs_expr = parse_expr(P);
                        auto compound_tok = previous(P);
                        return make_node<AstCompoundAssign>(P.holder, compound_tok, update_expr, op_type, rhs_expr);
                    }

                    else if (match(P, { TokenType::kIncrement, TokenType::kDecrement }))
                    {
                        auto op_tok = previous(P);
                        bool is_increment = (op_tok.type == TokenType::kIncrement);
                        if (is_increment)
                        {
                            return make_node<AstIncrement>(P.holder, op_tok, update_expr);
                        }
                        else
                        {
                            return make_node<AstDecrement>(P.holder, op_tok, update_expr);
                        }
                    }

                    else if (match(P, { TokenType::kAssign }))
                    {
                        auto assign_tok = previous(P);
                        auto assign = make_node<AstAssign>(P.holder, assign_tok);
                        assign->first_var = update_expr;
                        parse_expr_list(P, assign->first_expr);
                        return assign;
                    }
                    else
                    {
                        return update_expr;
                    }
                };

                // Parse first update
                auto first_update = parse_single_update();
                append_to_block(*update_block, first_update);

                // Check for additional updates
                while (match(P, { TokenType::kComma }))
                {
                    has_multiple = true;
                    auto next_update = parse_single_update();
                    append_to_block(*update_block, next_update);
                }

                // If multiple updates, use the block; otherwise use the single update
                if (has_multiple)
                {
                    update = update_block;
                }
                else
                {
                    update = first_update;
                }
            }

            consume(P, TokenType::kRParen, "Expected ')' after for loop");
            auto forc = make_node<AstForC>(P.holder, tok, init, condition, update);

            if (match(P, { TokenType::kLBrace }))
            {
                forc->block = parse_block(P);
                consume(P, TokenType::kRBrace, "Expected '}'");
            }
            else
            {
                forc->block = P.holder.make<AstBlock>();
                append_to_block(*forc->block, parse_stat(P));
            }
            return forc;
        }
        else if (check(P, TokenType::kIdentifier))
        {
            // Could be for-in, numeric for, or C-style for without let/const
            AstString* names = nullptr;
            parse_name_list(P, names);

            // Check for for-in loop (for (v in expr) or for (k,v in expr))
            if (match(P, { TokenType::kIn }))
            {
                AstNode* exprs = nullptr;
                parse_expr_list(P, exprs);
                consume(P, TokenType::kRParen, "Expected ')'");
                auto fori = make_node<AstForIn>(P.holder, tok);
                fori->first_name = names;
                fori->first_expr = exprs;

                if (match(P, { TokenType::kLBrace }))
                {
                    fori->block = parse_block(P);
                    consume(P, TokenType::kRBrace, "Expected '}'");
                }
                else
                {
                    fori->block = P.holder.make<AstBlock>();
                    append_to_block(*fori->block, parse_stat(P));
                }
                return fori;
            }
            // Check for comma-style numeric for loop (for (i = start, end))
            else if (match(P, { TokenType::kAssign }))
            {
                if (names->next_child != nullptr)
                {
                    error(P, "Numeric for loop expects one variable");
                }

                // Peek ahead to see if this is C-style (semicolon after init) or numeric (comma)
                auto start_expr = parse_expr(P);

                if (match(P, { TokenType::kComma }))
                {
                    // Comma-style numeric: for (i = start, end [, step])
                    auto end = parse_expr(P);
                    AstNode* step = nullptr;
                    if (match(P, { TokenType::kComma }))
                    {
                        step = parse_expr(P);
                    }
                    consume(P, TokenType::kRParen, "Expected ')' after for loop");
                    auto forn = make_node<AstForNum>(P.holder, tok, names, start_expr, end);
                    forn->step = step;

                    if (match(P, { TokenType::kLBrace }))
                    {
                        forn->block = parse_block(P);
                        consume(P, TokenType::kRBrace, "Expected '}'");
                    }
                    else
                    {
                        forn->block = P.holder.make<AstBlock>();
                        append_to_block(*forn->block, parse_stat(P));
                    }
                    return forn;
                }
                else if (match(P, { TokenType::kSemi }))
                {
                    // C-style without let: for (i = start; i < end; i++)
                    // Build an assignment node for init
                    auto assign_tok = tok; // Use for token as placeholder
                    auto assign = make_node<AstAssign>(P.holder, assign_tok);
                    auto ident = make_node<AstIdent>(P.holder, tok, names);
                    assign->first_var = ident;
                    assign->first_expr = start_expr;

                    auto condition = parse_expr(P);
                    consume(P, TokenType::kSemi, "Expected ';' after for loop condition");

                    AstNode* update = nullptr;
                    if (!check(P, TokenType::kRParen))
                    {
                        auto update_expr = parse_postfix(P);

                        if (match(P,
                                { TokenType::kPlusAssign, TokenType::kMinusAssign, TokenType::kStarAssign,
                                    TokenType::kSlashAssign, TokenType::kPercentAssign }))
                        {
                            TokenType op_type = TokenType::kPlus;
                            TokenType compound_op = previous(P).type;
                            if (compound_op == TokenType::kPlusAssign)
                            {
                                op_type = TokenType::kPlus;
                            }
                            else if (compound_op == TokenType::kMinusAssign)
                            {
                                op_type = TokenType::kMinus;
                            }
                            else if (compound_op == TokenType::kStarAssign)
                            {
                                op_type = TokenType::kStar;
                            }
                            else if (compound_op == TokenType::kSlashAssign)
                            {
                                op_type = TokenType::kSlash;
                            }
                            else if (compound_op == TokenType::kPercentAssign)
                            {
                                op_type = TokenType::kPercent;
                            }

                            auto rhs_expr = parse_expr(P);
                            auto compound_tok = previous(P);
                            update = make_node<AstCompoundAssign>(P.holder, compound_tok, update_expr, op_type, rhs_expr);
                        }
                        else if (match(P, { TokenType::kIncrement, TokenType::kDecrement }))
                        {
                            auto op_tok = previous(P);
                            bool is_increment = (op_tok.type == TokenType::kIncrement);
                            if (is_increment)
                            {
                                update = make_node<AstIncrement>(P.holder, op_tok, update_expr);
                            }
                            else
                            {
                                update = make_node<AstDecrement>(P.holder, op_tok, update_expr);
                            }
                        }
                        else if (match(P, { TokenType::kAssign }))
                        {
                            auto assign_tok2 = previous(P);
                            auto assign2 = make_node<AstAssign>(P.holder, assign_tok2);
                            assign2->first_var = update_expr;
                            parse_expr_list(P, assign2->first_expr);
                            update = assign2;
                        }
                        else
                        {
                            update = update_expr;
                        }
                    }

                    consume(P, TokenType::kRParen, "Expected ')' after for loop");
                    auto forc = make_node<AstForC>(P.holder, tok, assign, condition, update);

                    if (match(P, { TokenType::kLBrace }))
                    {
                        forc->block = parse_block(P);
                        consume(P, TokenType::kRBrace, "Expected '}'");
                    }
                    else
                    {
                        forc->block = P.holder.make<AstBlock>();
                        append_to_block(*forc->block, parse_stat(P));
                    }
                    return forc;
                }
                else
                {
                    error(P, "Expected ';' or ',' after for loop initialization");
                }
            }
            else
            {
                error(P, "Invalid for loop syntax");
            }
        }
        else
        {
            error(P,
                "For loops require C-style syntax: for (let i = 0; i < n; i++) { } or for (i = start, end) { } or for (v in "
                "expr) { }");
        }
    }

    static AstNode* parse_foreach(ParserState& P)
    {
        auto tok = advance(P); // consume 'foreach'
        consume(P, TokenType::kLParen, "Expected '(' after 'foreach'");

        // Check for variable declaration with 'let' or 'const'
        bool declares_variables = false;
        if (match(P, { TokenType::kLet, TokenType::kConst }))
        {
            declares_variables = true;
        }

        // Parse variable names (v or k,v)
        AstString* names = nullptr;
        parse_name_list(P, names);

        // Expect 'in'
        consume(P, TokenType::kIn, "Expected 'in' in foreach loop");

        // Parse the table expression
        auto table_expr = parse_expr(P);

        // Wrap table_expr in pairs() call
        auto* pairs_str = P.holder.make_string("pairs");
        pairs_str->line = tok.line;
        pairs_str->column = tok.column;
        auto pairs_ident = make_node<AstIdent>(P.holder, tok, pairs_str);
        auto pairs_call = make_node<AstFuncCall>(P.holder, tok, pairs_ident);
        pairs_call->first_arg = table_expr;

        consume(P, TokenType::kRParen, "Expected ')' after foreach");

        auto fori = make_node<AstForIn>(P.holder, tok);
        fori->declares_variables = declares_variables;
        fori->first_name = names;
        fori->first_expr = pairs_call;

        if (match(P, { TokenType::kLBrace }))
        {
            fori->block = parse_block(P);
            consume(P, TokenType::kRBrace, "Expected '}'");
        }
        else
        {
            fori->block = P.holder.make<AstBlock>();
            append_to_block(*fori->block, parse_stat(P));
        }

        return fori;
    }

    static AstNode* parse_func_def_stat(ParserState& P)
    {
        auto tok = advance(P);
        auto fds = make_node<AstFuncDefStat>(P.holder, tok);
        parse_name_parts(P, fds->first_name_part, fds->is_method);
        parse_params_in_parens(P, fds->first_param, fds->is_vararg, "Expected '('");
        consume(P, TokenType::kLBrace, "Expected '{'");
        fds->block = parse_block(P);
        consume(P, TokenType::kRBrace, "Expected '}'");
        if (fds->is_method)
        {
            auto* self_param = P.holder.make_string("self");
            self_param->next_child = fds->first_param;
            fds->first_param = self_param;
        }
        return fds;
    }

    static AstNode* parse_local(ParserState& P)
    {
        bool is_const = false;
        Token let_tok;
        if (match(P, { TokenType::kConst }))
        {
            is_const = true;
            let_tok = previous(P);
        }
        else
        {
            let_tok = advance(P);
        }
        if (match(P, { TokenType::kFunction }))
        {
            auto func_tok = previous(P);
            auto fds = make_node<AstFuncDefStat>(P.holder, func_tok);
            fds->first_name_part = P.holder.make_string(consume(P, TokenType::kIdentifier, "Expected function name").value);
            fds->is_local = true;
            parse_params_in_parens(P, fds->first_param, fds->is_vararg, "Expected '('");
            consume(P, TokenType::kLBrace, "Expected '{'");
            fds->block = parse_block(P);
            consume(P, TokenType::kRBrace, "Expected '}'");
            return fds;
        }
        else
        {
            AstString* first_name = nullptr;
            parse_name_list(P, first_name);
            AstNode* first_init = nullptr;
            if (match(P, { TokenType::kAssign }))
            {
                parse_expr_list(P, first_init);
            }
            return make_node<AstLocalDecl>(P.holder, let_tok, is_const, first_name, first_init);
        }
    }

    static AstNode* parse_return(ParserState& P)
    {
        auto tok = advance(P);
        AstNode* first_expr = nullptr;
        if (!is_statement_boundary_token(current(P).type))
        {
            // Build linked list of return expressions
            AstNode** tail = &first_expr;
            *tail = parse_expr(P);
            tail = &(*tail)->next_child;
            while (match(P, { TokenType::kComma }))
            {
                *tail = parse_expr(P);
                tail = &(*tail)->next_child;
            }
        }
        return make_node<AstReturn>(P.holder, tok, first_expr);
    }

    static AstNode* parse_stat(ParserState& P)
    {
        if (match(P, { TokenType::kSemi }))
        {
            return nullptr;
        }
        // Export statements
        if (check(P, TokenType::kExport))
        {
            auto export_tok = advance(P);

            // export function name() {}
            if (check(P, TokenType::kFunction))
            {
                auto func_node = parse_func_def_stat(P);
                return make_node<AstExportDecl>(P.holder, export_tok, func_node);
            }
            // export const X = value
            else if (check(P, TokenType::kConst))
            {
                auto local_node = parse_local(P);
                return make_node<AstExportDecl>(P.holder, export_tok, local_node);
            }
            // export { name1, name2, ... }
            else if (match(P, { TokenType::kLBrace }))
            {
                auto export_list = make_node<AstExportList>(P.holder, export_tok);
                AstString** name_tail = &export_list->first_name;
                do
                {
                    auto name_tok = consume(P, TokenType::kIdentifier, "Expected identifier in export list");
                    auto name = P.holder.make_string(name_tok.value);
                    *name_tail = name;
                    name_tail = reinterpret_cast<AstString**>(&name->next_child);
                } while (match(P, { TokenType::kComma }));
                consume(P, TokenType::kRBrace, "Expected '}' after export list");
                return export_list;
            }
            // export let is an error
            else if (check(P, TokenType::kLet))
            {
                error(P, "Cannot export mutable variables (let). Use getter/setter functions or const.");
            }
            else
            {
                error(P, "Expected 'function', 'const', or '{' after 'export'");
            }
        }
        if (check(P, TokenType::kIf))
        {
            return parse_if(P);
        }
        if (check(P, TokenType::kWhile))
        {
            return parse_while(P);
        }
        if (check(P, TokenType::kFor))
        {
            return parse_for(P);
        }
        if (check(P, TokenType::kForEach))
        {
            return parse_foreach(P);
        }
        if (check(P, TokenType::kFunction))
        {
            return parse_func_def_stat(P);
        }
        if (check(P, TokenType::kLet) || check(P, TokenType::kConst))
        {
            return parse_local(P);
        }
        if (check(P, TokenType::kReturn))
        {
            return parse_return(P);
        }
        if (match(P, { TokenType::kBreak }))
        {
            return make_node<AstBreak>(P.holder, previous(P));
        }
        if (match(P, { TokenType::kContinue }))
        {
            return make_node<AstContinue>(P.holder, previous(P));
        }
        if (match(P, { TokenType::kDefer }))
        {
            auto tok = previous(P);
            AstNode* body = nullptr;

            // Check if it's a block or single statement
            if (check(P, TokenType::kLBrace))
            {
                advance(P); // consume '{'
                body = parse_block(P);
                consume(P, TokenType::kRBrace, "Expected '}' to close defer block");
            }
            else
            {
                // Single statement
                body = parse_stat(P);
            }

            return make_node<AstDefer>(P.holder, tok, body);
        }
        // Support bare block scopes
        if (match(P, { TokenType::kLBrace }))
        {
            auto tok = previous(P);
            auto scope_block = make_node<AstScope>(P.holder, tok);
            scope_block->block = parse_block(P);
            consume(P, TokenType::kRBrace, "Expected '}' to close block");
            return scope_block;
        }

        auto prefix = parse_postfix(P);
        AstNode* first_left = prefix;
        AstNode** left_tail = &first_left->next_child;
        while (match(P, { TokenType::kComma }))
        {
            *left_tail = parse_postfix(P);
            left_tail = &(*left_tail)->next_child;
        }

        if (match(P,
                { TokenType::kPlusAssign, TokenType::kMinusAssign, TokenType::kStarAssign, TokenType::kSlashAssign,
                    TokenType::kPercentAssign }))
        {
            auto compound_tok = previous(P);
            if (first_left->next_child != nullptr)
            {
                Token tok = previous(P);
                SourceLocation loc(P.chunkname, tok.line, tok.column);
                throw SyntaxError("Compound assignment operators do not support multiple variables", loc);
            }
            const auto& var = first_left;
            if (var->type != AstNodeType::kIdent && var->type != AstNodeType::kIndex && var->type != AstNodeType::kMember)
            {
                error(P, "Invalid left-hand side in compound assignment");
            }

            TokenType op_type = TokenType::kPlus;
            TokenType compound_op = previous(P).type;
            if (compound_op == TokenType::kPlusAssign)
            {
                op_type = TokenType::kPlus;
            }
            else if (compound_op == TokenType::kMinusAssign)
            {
                op_type = TokenType::kMinus;
            }
            else if (compound_op == TokenType::kStarAssign)
            {
                op_type = TokenType::kStar;
            }
            else if (compound_op == TokenType::kSlashAssign)
            {
                op_type = TokenType::kSlash;
            }
            else if (compound_op == TokenType::kPercentAssign)
            {
                op_type = TokenType::kPercent;
            }

            auto rhs_expr = parse_expr(P);
            return make_node<AstCompoundAssign>(P.holder, compound_tok, first_left, op_type, rhs_expr);
        }
        else if (match(P, { TokenType::kIncrement, TokenType::kDecrement }))
        {
            if (first_left->next_child != nullptr)
            {
                Token tok = previous(P);
                SourceLocation loc(P.chunkname, tok.line, tok.column);
                throw SyntaxError("Increment/decrement operators do not support multiple variables", loc);
            }
            const auto& var = first_left;
            if (var->type != AstNodeType::kIdent && var->type != AstNodeType::kIndex && var->type != AstNodeType::kMember)
            {
                error(P, "Invalid left-hand side for increment/decrement");
            }

            bool is_increment = (previous(P).type == TokenType::kIncrement);
            auto op_tok = previous(P);
            if (is_increment)
            {
                return make_node<AstIncrement>(P.holder, op_tok, first_left);
            }
            else
            {
                return make_node<AstDecrement>(P.holder, op_tok, first_left);
            }
        }
        else if (match(P, { TokenType::kAssign }))
        {
            auto assign_tok = previous(P);
            // Validate all left-hand sides
            for (AstNode* var = first_left; var != nullptr; var = var->next_child)
            {
                if (var->type != AstNodeType::kIdent && var->type != AstNodeType::kIndex && var->type != AstNodeType::kMember)
                {
                    error(P, "Invalid left-hand side in assignment");
                }
            }
            auto ass = make_node<AstAssign>(P.holder, assign_tok);
            ass->first_var = first_left;
            parse_expr_list(P, ass->first_expr);
            return ass;
        }
        else
        {
            if (first_left->next_child != nullptr)
            {
                Token tok = current(P);
                SourceLocation loc(P.chunkname, tok.line, tok.column);
                throw SyntaxError("Expected '=' after variable list", loc);
            }
            auto expr_st = make_node<AstExprStat>(P.holder, first_left->line, first_left->column, first_left);
            if (expr_st->expr->type != AstNodeType::kFuncCall)
            {
                error(P, "Statement must be an assignment or function call");
            }
            return expr_st;
        }
    }

    static AstBlock* parse_block(ParserState& P)
    {
        auto block = P.holder.make<AstBlock>();
        AstNode** tail = &block->first_stat;
        while (!is_block_boundary_token(current(P).type))
        {
            // Stop parsing if we've reached the position limit
            Token tok = current(P);
            if (P.max_line >= 0)
            {
                if (tok.line > P.max_line)
                {
                    break;
                }
                // If on the target line, check column position
                if (tok.line == P.max_line && P.max_column >= 0 && tok.column >= P.max_column)
                {
                    break;
                }
            }

            auto stat = parse_stat(P);
            if (stat)
            {
                *tail = stat;
                tail = &stat->next_child;
            }
        }
        return block;
    }

    static AstNode* parse_expr(ParserState& P)
    {
        // Check position limit before parsing expression
        if (P.max_line >= 0)
        {
            Token tok = current(P);
            if (tok.line > P.max_line)
            {
                return make_node<AstNil>(P.holder, tok);
            }
            if (tok.line == P.max_line && P.max_column >= 0 && tok.column >= P.max_column)
            {
                return make_node<AstNil>(P.holder, tok);
            }
        }

        return parse_bin(P, 0);
    }

} // namespace behl
