#include "api/api_internal.hpp"
#include "behl.hpp"
#include "common/arithmetic.hpp"
#include "common/format.hpp"
#include "gc/gc.hpp"
#include "gc/gco_string.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm.hpp"
#include "vm/vm_debug.hpp"
#include "vm/vm_error.hpp"

#include <cassert>
#include <cmath>

namespace behl
{
    bool to_boolean(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return false;
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (v.is_nil())
        {
            return false;
        }

        if (v.is_bool())
        {
            return v.get_bool();
        }

        return true;
    }

    Integer to_integer(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return 0;
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];

        if (v.is_integer())
        {
            return v.get_integer();
        }

        if (v.is_fp())
        {
            Integer n = 0;
            if (arithmetic::try_from_fp(v.get_fp(), n))
            {
                return n;
            }
        }

        return 0;
    }

    FP to_number(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return 0.0;
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];

        if (v.is_fp())
        {
            return v.get_fp();
        }

        if (v.is_integer())
        {
            return static_cast<FP>(v.get_integer());
        }

        return 0.0;
    }

    std::string_view to_string(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return {};
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];

        if (v.is_string())
        {
            auto* str_data = v.get_string();
            return str_data->view();
        }

        return {};
    }

    Type type(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return Type::kNil;
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        return v.get_type();
    }

    [[noreturn]] void error(State* S, std::string_view msg)
    {
        assert(S != nullptr && "State can not be null");
        assert(!S->call_stack.empty() && "error must be called from within a call");

        format_buffer buffer;
        append_error_prefix(buffer, SourceLocation{}, "RuntimeError");
        buffer.append(msg);
        buffer += '\n';
        append_stacktrace(buffer, S);
        throw Exception(Value(gc_new_string(S, buffer.view())));
    }

    [[noreturn]] void error_value(State* S)
    {
        assert(S != nullptr && "State can not be null");

        assert(!S->call_stack.empty() && "error_value must be called from within a call");

        const Value value = get_top(S) > 0 ? S->stack.back() : Value{};
        throw Exception(value);
    }

    static std::string_view type_to_cstr(Type t)
    {
        return Value::get_type_string(t);
    }

    std::string_view type_name(Type t)
    {
        return type_to_cstr(t);
    }

    std::string_view value_typename(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        return type_to_cstr(type(S, idx));
    }

    bool is_nil(State* S, int32_t idx)
    {
        return type(S, idx) == Type::kNil;
    }

    bool is_boolean(State* S, int32_t idx)
    {
        return type(S, idx) == Type::kBoolean;
    }

    bool is_integer(State* S, int32_t idx)
    {
        return type(S, idx) == Type::kInteger;
    }

    bool is_number(State* S, int32_t idx)
    {
        Type t = type(S, idx);
        return t == Type::kInteger || t == Type::kNumber;
    }

    bool is_string(State* S, int32_t idx)
    {
        return type(S, idx) == Type::kString;
    }

    bool is_table(State* S, int32_t idx)
    {
        return type(S, idx) == Type::kTable;
    }

    bool is_function(State* S, int32_t idx)
    {
        Type t = type(S, idx);
        return t == Type::kClosure || t == Type::kCFunction;
    }

    bool is_cfunction(State* S, int32_t idx)
    {
        return type(S, idx) == Type::kCFunction;
    }

    bool is_userdata(State* S, int32_t idx)
    {
        return type(S, idx) == Type::kUserdata;
    }

    static int32_t one_based_arg_index(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        if (idx >= 0)
        {
            return idx + 1;
        }

        int32_t top = get_top(S);
        int32_t pos = top + idx + 1;
        if (pos < 1)
        {
            pos = 1;
        }
        return pos;
    }

    [[noreturn]] static void raise_bad_argument(State* S, int32_t idx, std::string_view expected)
    {
        assert(S != nullptr && "State can not be null");

        raise_type_error(S, SourceLocation{}, "bad argument #{} (expected {}, got {})", one_based_arg_index(S, idx), expected,
            value_typename(S, idx));
    }

    void check_type(State* S, int32_t idx, Type t)
    {
        assert(S != nullptr && "State can not be null");

        if (auto val_type = type(S, idx); val_type != t)
        {
            raise_bad_argument(S, idx, Value::get_type_string(t));
        }
    }

    Integer check_integer(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            raise_bad_argument(S, idx, "integer");
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (v.is_integer())
        {
            return v.get_integer();
        }

        if (v.is_fp())
        {
            const FP d = v.get_fp();
            Integer n = 0;
            if (std::floor(d) == d && arithmetic::try_from_fp(d, n))
            {
                return n;
            }
        }

        raise_bad_argument(S, idx, "integer");
    }

    FP check_number(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            raise_bad_argument(S, idx, "number");
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (v.is_fp())
        {
            return v.get_fp();
        }

        if (v.is_integer())
        {
            return static_cast<FP>(v.get_integer());
        }

        raise_bad_argument(S, idx, "number");
    }

    std::string_view check_string(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            raise_bad_argument(S, idx, "string");
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (v.is_string())
        {
            auto* str_data = v.get_string();
            return str_data->view();
        }

        raise_bad_argument(S, idx, "string");
    }

    bool check_boolean(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            raise_bad_argument(S, idx, "boolean");
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (v.is_bool())
        {
            return v.get_bool();
        }

        raise_bad_argument(S, idx, "boolean");
    }

} // namespace behl
