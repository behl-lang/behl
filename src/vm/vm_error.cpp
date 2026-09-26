#include "vm/vm_error.hpp"

#include "vm/frame.hpp"
#include "vm/vm_debug.hpp"
#include "vm/vm_detail.hpp"

namespace behl
{
    void SourceLocation::format_to(format_buffer& out) const
    {
        const std::string_view name = filename != nullptr ? filename->view() : std::string_view{};
        if (line > 0 && column > 0)
        {
            behl::format_to(out, "{}:{}:{}", name, line, column);
        }
        else if (line > 0)
        {
            behl::format_to(out, "{}:{}", name, line);
        }
        else
        {
            out.append(name);
        }
    }

    const char* Exception::describe(const Value& value) noexcept
    {
        switch (value.get_type())
        {
            case Type::kNil:
                return "(error object is a nil value)";
            case Type::kBoolean:
                return "(error object is a boolean value)";
            case Type::kInteger:
                return "(error object is an integer value)";
            case Type::kNumber:
                return "(error object is a number value)";
            case Type::kString:
                return "(error object is a string value)";
            case Type::kTable:
                return "(error object is a table value)";
            case Type::kUserdata:
                return "(error object is a userdata value)";
            case Type::kClosure:
            case Type::kCFunction:
                return "(error object is a function value)";
            default:
                return "(error object is an unknown value)";
        }
    }

    void append_error_prefix(format_buffer& buffer, const SourceLocation& location, std::string_view label)
    {
        const std::string_view name = location.filename != nullptr ? location.filename->view() : std::string_view{};
        if (!name.empty() && location.line > 0 && location.column > 0)
        {
            format_to(buffer, "{}({},{}): ", name, location.line, location.column);
        }
        else if (!name.empty() && location.line > 0)
        {
            format_to(buffer, "{}({}): ", name, location.line);
        }
        else if (!name.empty())
        {
            format_to(buffer, "{}: ", name);
        }
        buffer.append(label);
        buffer.append(": ");
    }

    void raise_bad_arith(State* S, const Value& a, const Value& b, const CallFrame& frame, const char* op_name)
    {
        const auto loc = get_current_location(frame);

        if (op_name)
        {
            raise_type_error(S, loc, "attempt to {} a '{}' with a '{}'", op_name, a.get_type_string(),
                b.get_type_string());
        }
        raise_type_error(S, loc, "attempt to perform arithmetic on a '{}' value and a '{}' value", a.get_type_string(),
            b.get_type_string());
    }

    void raise_bad_arith(State* S, const Value& a, const CallFrame& frame)
    {
        raise_type_error(S, get_current_location(frame), "attempt to perform arithmetic on a {} value",
            a.get_type_string());
    }

    void raise_bad_bitwise(State* S, const Value& a, const Value& b, const CallFrame& frame)
    {
        raise_type_error(S, get_current_location(frame),
            "attempt to perform bitwise operation on a '{}' value and a '{}' value", a.get_type_string(),
            b.get_type_string());
    }

    void raise_bad_bitwise(State* S, const Value& a, const CallFrame& frame)
    {
        raise_type_error(S, get_current_location(frame), "attempt to perform bitwise operation on a {} value",
            a.get_type_string());
    }

    void raise_no_integer_representation(State* S, const CallFrame& frame)
    {
        raise_type_error(S, get_current_location(frame), "number has no integer representation");
    }

    void raise_bad_call(const Value& val, const CallFrame& frame, State* S)
    {
        const auto loc = get_current_location(frame);
        const char* msg = nullptr;
        switch (val.get_type())
        {
            case Type::kNil:
                msg = "attempt to call nil value";
                break;
            case Type::kBoolean:
                msg = "attempt to call boolean value";
                break;
            case Type::kInteger:
                msg = "attempt to call integer value";
                break;
            case Type::kNumber:
                msg = "attempt to call number value";
                break;
            case Type::kString:
                msg = "attempt to call string value";
                break;
            case Type::kTable:
                msg = "attempt to call table value";
                break;
            case Type::kUserdata:
                msg = "attempt to call userdata value";
                break;
            default:
                msg = "attempt to call unknown value";
                break;
        }

        format_buffer buffer;
        append_error_prefix(buffer, loc, "TypeError");
        buffer.append(std::string_view{ msg });
        buffer += '\n';
        append_stacktrace(buffer, S);
        throw Exception(Value(gc_new_string(S, buffer.view())));
    }

} // namespace behl
