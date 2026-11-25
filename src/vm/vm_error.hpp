#pragma once

#include "common/format.hpp"
#include "gc/gc.hpp"
#include "gc/gco_string.hpp"
#include "platform/platform.hpp"
#include "vm/value.hpp"

#include <exception>
#include <string_view>
#include <type_traits>
#include <utility>

namespace behl
{
    struct CallFrame;

    struct SourceLocation
    {
        const GCString* filename = nullptr;
        int line = 0;
        int column = 0;

        SourceLocation() = default;
        SourceLocation(const GCString* fname, int ln = 0, int col = 0)
            : filename(fname)
            , line(ln)
            , column(col)
        {
        }

        void format_to(format_buffer& out) const;
    };

    class Exception final : public std::exception
    {
    public:
        explicit Exception(const Value& value) noexcept
            : m_value(value)
        {
        }

        const Value& value() const noexcept
        {
            return m_value;
        }

        const char* what() const noexcept override
        {
            return describe(m_value);
        }

        static const char* describe(const Value& value) noexcept;

    private:
        Value m_value;
    };

    void append_error_prefix(format_buffer& buffer, const SourceLocation& location, std::string_view label);

    template<typename... Args>
    [[noreturn]] BEHL_NOINLINE void raise_labeled_error(State* S, std::string_view label, const SourceLocation& location,
        format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        format_buffer buffer;
        append_error_prefix(buffer, location, label);
        format_to(buffer, fmt, std::forward<Args>(args)...);
        throw Exception(Value(gc_new_string(S, buffer.view())));
    }

    template<typename... Args>
    [[noreturn]] BEHL_NOINLINE void raise_type_error(
        State* S, const SourceLocation& location, format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        raise_labeled_error(S, "TypeError", location, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    [[noreturn]] BEHL_NOINLINE void raise_runtime_error(
        State* S, const SourceLocation& location, format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        raise_labeled_error(S, "RuntimeError", location, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    [[noreturn]] BEHL_NOINLINE void raise_reference_error(
        State* S, const SourceLocation& location, format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        raise_labeled_error(S, "ReferenceError", location, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    [[noreturn]] BEHL_NOINLINE void raise_syntax_error(
        State* S, const SourceLocation& location, format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        raise_labeled_error(S, "SyntaxError", location, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    [[noreturn]] BEHL_NOINLINE void raise_semantic_error(
        State* S, const SourceLocation& location, format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
    {
        raise_labeled_error(S, "SemanticError", location, fmt, std::forward<Args>(args)...);
    }

    [[noreturn]] void raise_bad_arith(
        State* S, const Value& a, const Value& b, const CallFrame& frame, const char* op_name = nullptr);

    [[noreturn]] void raise_bad_arith(State* S, const Value& a, const CallFrame& frame);

    [[noreturn]] void raise_bad_bitwise(State* S, const Value& a, const Value& b, const CallFrame& frame);

    [[noreturn]] void raise_bad_bitwise(State* S, const Value& a, const CallFrame& frame);

    [[noreturn]] void raise_no_integer_representation(State* S, const CallFrame& frame);

    [[noreturn]] void raise_bad_call(const Value& val, const CallFrame& frame, State* S);

} // namespace behl
