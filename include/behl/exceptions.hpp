#pragma once

#include <behl/export.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace behl
{

    struct SourceLocation
    {
        std::string filename;
        int line = 0;
        int column = 0;

        SourceLocation() = default;
        SourceLocation(std::string_view fname, int ln = 0, int col = 0)
            : filename(fname)
            , line(ln)
            , column(col)
        {
        }

        std::string to_string() const;
    };

    class BEHL_API BehlException : public std::exception
    {
    public:
        BehlException(std::string_view type, std::string_view message, const SourceLocation& location = {});

        const char* what() const noexcept override;
        const SourceLocation& location() const noexcept
        {
            return m_location;
        }

    private:
        std::string m_message;
        SourceLocation m_location;

        static std::string format_message(std::string_view type, std::string_view message, const SourceLocation& location);
    };

    class BEHL_API ParserError : public BehlException
    {
    public:
        ParserError(std::string_view message, const SourceLocation& location = {});
    };

    class BEHL_API TypeError : public BehlException
    {
    public:
        TypeError(std::string_view message, const SourceLocation& location = {});
    };

    class BEHL_API ReferenceError : public BehlException
    {
    public:
        ReferenceError(std::string_view message, const SourceLocation& location = {});
    };

    class BEHL_API SyntaxError : public BehlException
    {
    public:
        SyntaxError(std::string_view message, const SourceLocation& location = {});
    };

    class BEHL_API RuntimeError : public BehlException
    {
    public:
        RuntimeError(std::string_view message, const SourceLocation& location = {});
    };

    class BEHL_API ArithmeticError : public BehlException
    {
    public:
        ArithmeticError(std::string_view message, const SourceLocation& location = {});
    };

    class BEHL_API SemanticError : public BehlException
    {
    public:
        SemanticError(std::string_view message, const SourceLocation& location = {});
    };

} // namespace behl
