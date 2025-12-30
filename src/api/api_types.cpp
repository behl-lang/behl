#include "behl.hpp"
#include "common/format.hpp"
#include "gc/gc.hpp"
#include "gc/gco_string.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm.hpp"

#include <behl/exceptions.hpp>
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
            return static_cast<Integer>(v.get_fp());
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

        std::string trace = build_stacktrace_internal(S);
        std::string full_message = behl::format("{}\n{}", msg, trace);

        throw RuntimeError(full_message);
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

    static TypeError make_type_error(State* S, int32_t idx, std::string_view expected)
    {
        assert(S != nullptr && "State can not be null");

        const auto arg_idx = one_based_arg_index(S, idx);
        const auto received_type = value_typename(S, idx);
        const auto msg = behl::format("bad argument #{} (expected {}, got {})", arg_idx, expected, received_type);
        return TypeError(msg);
    }

    void check_type(State* S, int32_t idx, Type t)
    {
        assert(S != nullptr && "State can not be null");

        if (auto val_type = type(S, idx); val_type != t)
        {
            throw make_type_error(S, idx, Value::get_type_string(t));
        }
    }

    Integer check_integer(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            throw make_type_error(S, idx, "integer");
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (v.is_integer())
        {
            return v.get_integer();
        }

        if (v.is_fp())
        {
            FP d = v.get_fp();
            if (std::floor(d) == d)
            {
                return static_cast<Integer>(d);
            }
        }

        throw make_type_error(S, idx, "integer");
    }

    FP check_number(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            throw make_type_error(S, idx, "number");
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

        throw make_type_error(S, idx, "number");
    }

    std::string_view check_string(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            throw make_type_error(S, idx, "string");
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (v.is_string())
        {
            auto* str_data = v.get_string();
            return str_data->view();
        }

        throw make_type_error(S, idx, "string");
    }

    bool check_boolean(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            throw make_type_error(S, idx, "boolean");
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        if (v.is_bool())
        {
            return v.get_bool();
        }

        throw make_type_error(S, idx, "boolean");
    }

} // namespace behl
