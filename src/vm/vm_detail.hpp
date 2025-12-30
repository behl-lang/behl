#pragma once

#include "common/charconv_compat.hpp"
#include "common/format.hpp"
#include "gc/gc.hpp"
#include "gc/gco_proto.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_metatable.hpp"

#include <cassert>
#include <charconv>

namespace behl
{

    inline SourceLocation get_current_location(const CallFrame& frame)
    {
        if (!frame.proto)
        {
            return SourceLocation("<native>");
        }

        int line = 0;
        int column = 0;
        if (frame.pc > 0 && frame.pc - 1 < frame.proto->line_info.size())
        {
            assert(frame.pc - 1 < frame.proto->line_info.size() && "get_current_location: line_info index out of bounds");
            line = frame.proto->line_info[frame.pc - 1];

            if (frame.pc - 1 < frame.proto->column_info.size())
            {
                assert(
                    frame.pc - 1 < frame.proto->column_info.size() && "get_current_location: column_info index out of bounds");
                column = frame.proto->column_info[frame.pc - 1];
            }
        }

        std::string source = !frame.proto->source_name || frame.proto->source_name->size() == 0
            ? "<script>"
            : std::string(frame.proto->source_name->view());
        return SourceLocation(source, line, column);
    }

    BEHL_FORCEINLINE
    Value& get_string_constant(const GCProto* proto, ConstIndex index) noexcept
    {
        assert(index < proto->str_constants.size() && "get_string_constant: constant index out of bounds");
        const Value& const_val = proto->str_constants[index];
        assert(const_val.is_string() && "get_string_constant: constant is not a string");
        return const_cast<Value&>(const_val);
    }

    BEHL_FORCEINLINE
    Value& get_integer_constant(const GCProto* proto, ConstIndex index) noexcept
    {
        assert(index < proto->int_constants.size() && "get_integer_constant: constant index out of bounds");
        const Value& const_val = proto->int_constants[index];
        assert(const_val.is_integer() && "get_integer_constant: constant is not an integer");
        return const_cast<Value&>(const_val);
    }

    BEHL_FORCEINLINE
    Value& get_fp_constant(const GCProto* proto, ConstIndex index) noexcept
    {
        assert(index < proto->fp_constants.size() && "get_fp_constant: constant index out of bounds");
        const Value& const_val = proto->fp_constants[index];
        assert(const_val.is_fp() && "get_fp_constant: constant is not a number");
        return const_cast<Value&>(const_val);
    }

    BEHL_FORCEINLINE
    Value& get_register(State* S, CallFrame& frame, size_t reg) noexcept
    {
        const auto idx = frame.base + reg;
        assert(idx < S->stack.size() && "get_register: register index out of bounds");
        return S->stack[idx];
    }

    BEHL_FORCEINLINE
    Value vm_makestring(State* S, const std::string_view str)
    {
        auto* obj = gc_new_string(S, str);

        return Value(obj);
    }

    BEHL_FORCEINLINE
    Value vm_tostring(State* S, const Value& val, const CallFrame& frame)
    {
        const auto type = val.get_type();
        switch (val.get_type())
        {
            case Type::kNil:
                return vm_makestring(S, "nil");
            case Type::kBoolean:
                return vm_makestring(S, val.get_bool() ? "true" : "false");
            case Type::kInteger:
            {
                char buffer[32];
                auto result = behl::to_chars(buffer, buffer + sizeof(buffer), val.get_integer());
                if (result.ec == std::errc{})
                {
                    return vm_makestring(S, std::string_view(buffer, static_cast<size_t>(result.ptr - buffer)));
                }
                return vm_makestring(S, behl::format<"{}">(val.get_integer()));
            }
            case Type::kNumber:
            {
                double d = val.get_fp();
                char buffer[64];
                auto result = behl::to_chars(buffer, buffer + sizeof(buffer), d, std::chars_format::general, 14);
                if (result.ec == std::errc{})
                {
                    return vm_makestring(S, std::string_view(buffer, static_cast<size_t>(result.ptr - buffer)));
                }
                return vm_makestring(S, behl::format<"{}">(d));
            }
            case Type::kString:
                return vm_makestring(S, val.get_string()->view());
            case Type::kClosure:
                return vm_makestring(S, behl::format<"function:{:p}">(static_cast<const void*>(val.get_closure())));
            case Type::kCFunction:
                return vm_makestring(S, behl::format<"cfunction:{:p}">(reinterpret_cast<const void*>(val.get_cfunction())));
            default:
                break;
        }

        // Check for __tostring metamethod
        Value tostring_mm = metatable_get_method<MetaMethodType::kToString>(val);
        if (tostring_mm.is_callable())
        {
            // Call __tostring(object) - must return a string
            if (auto result = metatable_call_method_result(S, tostring_mm, val); result.has_value())
            {
                // __tostring must return a string, otherwise error
                if (!result.is_string())
                {
                    throw TypeError("__tostring must return a string", get_current_location(frame));
                }
                return result;
            }
        }

        // If no metamethod or not callable, return the default string representation
        if (type == Type::kTable)
        {
            return vm_makestring(S, behl::format<"table:{:p}">(static_cast<const void*>(val.get_table())));
        }
        else if (type == Type::kUserdata)
        {
            return vm_makestring(S, behl::format<"userdata:{:p}">(static_cast<const void*>(val.get_userdata())));
        }
        else
        {
            assert(false);
            return Value{};
        }
    }

    BEHL_FORCEINLINE
    Value vm_tonumber([[maybe_unused]] State* S, const Value& val)
    {
        const auto type = val.get_type();

        switch (type)
        {
            case Type::kInteger:
                return val;
            case Type::kNumber:
                return val;
            case Type::kString:
            {
                const auto* str = val.get_string();
                const char* begin = str->data();
                const char* end = begin + str->size();

                Integer ival;
                auto [ptr_int, ec_int] = behl::from_chars(begin, end, ival);
                if (ec_int == std::errc{} && ptr_int == end)
                {
                    return Value(ival);
                }

                FP dval;
                auto [ptr_dbl, ec_dbl] = behl::from_chars(begin, end, dval);
                if (ec_dbl == std::errc{} && ptr_dbl == end)
                {
                    return Value(dval);
                }

                return Value{};
            }
            default:
                break;
        }

        return Value{};
    }

} // namespace behl
