#include "behl/exceptions.hpp"

#include "common/format.hpp"

namespace behl
{
    std::string SourceLocation::to_string() const
    {
        if (line > 0 && column > 0)
        {
            return behl::format("{}:{}:{}", filename, line, column);
        }
        else if (line > 0)
        {
            return behl::format("{}:{}", filename, line);
        }
        else
        {
            return filename;
        }
    }

    BehlException::BehlException(std::string_view type, std::string_view message, const SourceLocation& location)
        : m_message(format_message(type, message, location))
        , m_location(location)
    {
    }

    const char* BehlException::what() const noexcept
    {
        return m_message.c_str();
    }

    std::string BehlException::format_message(std::string_view type, std::string_view message, const SourceLocation& location)
    {
        if (!location.filename.empty() && location.line > 0 && location.column > 0)
        {
            return behl::format("{}({},{}): {}: {}", location.filename, location.line, location.column, type, message);
        }
        else if (!location.filename.empty() && location.line > 0)
        {
            return behl::format("{}({}): {}: {}", location.filename, location.line, type, message);
        }
        else if (!location.filename.empty())
        {
            return behl::format("{}: {}: {}", location.filename, type, message);
        }
        else
        {
            return behl::format("{}: {}", type, message);
        }
    }

    ParserError::ParserError(std::string_view message, const SourceLocation& location)
        : BehlException("ParserError", message, location)
    {
    }

    TypeError::TypeError(std::string_view message, const SourceLocation& location)
        : BehlException("TypeError", message, location)
    {
    }

    ReferenceError::ReferenceError(std::string_view message, const SourceLocation& location)
        : BehlException("ReferenceError", message, location)
    {
    }

    SyntaxError::SyntaxError(std::string_view message, const SourceLocation& location)
        : BehlException("SyntaxError", message, location)
    {
    }

    RuntimeError::RuntimeError(std::string_view message, const SourceLocation& location)
        : BehlException("RuntimeError", message, location)
    {
    }

    ArithmeticError::ArithmeticError(std::string_view message, const SourceLocation& location)
        : BehlException("ArithmeticError", message, location)
    {
    }

    SemanticError::SemanticError(std::string_view message, const SourceLocation& location)
        : BehlException("SemanticError", message, location)
    {
    }

} // namespace behl
