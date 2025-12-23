#include "behl.hpp"
#include "state.hpp"

#include <cmath>
#include <limits>
#include <numbers>
#include <random>

namespace behl
{
    // Basic math functions
    static int math_abs(State* S)
    {
        if (type(S, 0) == Type::kInteger)
        {
            Integer n = to_integer(S, 0);
            push_integer(S, std::abs(n));
        }
        else
        {
            FP n = to_number(S, 0);
            push_number(S, std::fabs(n));
        }
        return 1;
    }

    static int math_floor(State* S)
    {
        FP n = to_number(S, 0);
        push_integer(S, static_cast<Integer>(std::floor(n)));
        return 1;
    }

    static int math_ceil(State* S)
    {
        FP n = to_number(S, 0);
        push_integer(S, static_cast<Integer>(std::ceil(n)));
        return 1;
    }

    static int math_round(State* S)
    {
        FP n = to_number(S, 0);
        push_integer(S, static_cast<Integer>(std::round(n)));
        return 1;
    }

    static int math_trunc(State* S)
    {
        FP n = to_number(S, 0);
        push_integer(S, static_cast<Integer>(std::trunc(n)));
        return 1;
    }

    static int math_sqrt(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::sqrt(n));
        return 1;
    }

    static int math_cbrt(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::cbrt(n));
        return 1;
    }

    static int math_pow(State* S)
    {
        FP x = to_number(S, 0);
        FP y = to_number(S, 1);
        push_number(S, std::pow(x, y));
        return 1;
    }

    static int math_exp(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::exp(n));
        return 1;
    }

    static int math_expm1(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::expm1(n));
        return 1;
    }

    static int math_log(State* S)
    {
        FP n = to_number(S, 0);
        if (get_top(S) >= 2)
        {
            FP base = to_number(S, 1);
            push_number(S, std::log(n) / std::log(base));
        }
        else
        {
            push_number(S, std::log(n));
        }
        return 1;
    }

    static int math_log10(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::log10(n));
        return 1;
    }

    static int math_log2(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::log2(n));
        return 1;
    }

    static int math_log1p(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::log1p(n));
        return 1;
    }

    // Trigonometric functions
    static int math_sin(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::sin(n));
        return 1;
    }

    static int math_cos(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::cos(n));
        return 1;
    }

    static int math_tan(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::tan(n));
        return 1;
    }

    static int math_asin(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::asin(n));
        return 1;
    }

    static int math_acos(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::acos(n));
        return 1;
    }

    static int math_atan(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::atan(n));
        return 1;
    }

    static int math_atan2(State* S)
    {
        FP y = to_number(S, 0);
        FP x = to_number(S, 1);
        push_number(S, std::atan2(y, x));
        return 1;
    }

    static int math_sinh(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::sinh(n));
        return 1;
    }

    static int math_cosh(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::cosh(n));
        return 1;
    }

    static int math_tanh(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::tanh(n));
        return 1;
    }

    static int math_asinh(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::asinh(n));
        return 1;
    }

    static int math_acosh(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::acosh(n));
        return 1;
    }

    static int math_atanh(State* S)
    {
        FP n = to_number(S, 0);
        push_number(S, std::atanh(n));
        return 1;
    }

    // Angle conversion
    static int math_deg(State* S)
    {
        FP rad = to_number(S, 0);
        push_number(S, rad * 180.0 / 3.14159265358979323846);
        return 1;
    }

    static int math_rad(State* S)
    {
        FP deg = to_number(S, 0);
        push_number(S, deg * 3.14159265358979323846 / 180.0);
        return 1;
    }

    // Min/Max
    static int math_min(State* S)
    {
        int n = get_top(S);
        if (n == 0)
        {
            error(S, "math.min requires at least one argument");
        }

        bool all_integers = true;
        for (int i = 0; i < n; ++i)
        {
            if (type(S, i) != Type::kInteger)
            {
                all_integers = false;
                break;
            }
        }

        if (all_integers)
        {
            Integer min_val = to_integer(S, 0);
            for (int i = 1; i < n; ++i)
            {
                Integer val = to_integer(S, i);
                if (val < min_val)
                {
                    min_val = val;
                }
            }
            push_integer(S, min_val);
        }
        else
        {
            FP min_val = to_number(S, 0);
            for (int i = 1; i < n; ++i)
            {
                FP val = to_number(S, i);
                if (val < min_val)
                {
                    min_val = val;
                }
            }
            push_number(S, min_val);
        }
        return 1;
    }

    static int math_max(State* S)
    {
        int n = get_top(S);
        if (n == 0)
        {
            error(S, "math.max requires at least one argument");
        }

        bool all_integers = true;
        for (int i = 0; i < n; ++i)
        {
            if (type(S, i) != Type::kInteger)
            {
                all_integers = false;
                break;
            }
        }

        if (all_integers)
        {
            Integer max_val = to_integer(S, 0);
            for (int i = 1; i < n; ++i)
            {
                Integer val = to_integer(S, i);
                if (val > max_val)
                {
                    max_val = val;
                }
            }
            push_integer(S, max_val);
        }
        else
        {
            FP max_val = to_number(S, 0);
            for (int i = 1; i < n; ++i)
            {
                FP val = to_number(S, i);
                if (val > max_val)
                {
                    max_val = val;
                }
            }
            push_number(S, max_val);
        }
        return 1;
    }

    static int math_clamp(State* S)
    {
        FP val = to_number(S, 0);
        FP min_val = to_number(S, 1);
        FP max_val = to_number(S, 2);

        if (val < min_val)
        {
            val = min_val;
        }
        else if (val > max_val)
        {
            val = max_val;
        }

        push_number(S, val);
        return 1;
    }

    // Utility functions
    static int math_sign(State* S)
    {
        FP n = to_number(S, 0);
        if (n > 0.0)
        {
            push_integer(S, 1);
        }
        else if (n < 0.0)
        {
            push_integer(S, -1);
        }
        else
        {
            push_integer(S, 0);
        }
        return 1;
    }

    static int math_fmod(State* S)
    {
        FP x = to_number(S, 0);
        FP y = to_number(S, 1);
        push_number(S, std::fmod(x, y));
        return 1;
    }

    static int math_modf(State* S)
    {
        FP x = to_number(S, 0);
        FP intpart;
        FP fracpart = std::modf(x, &intpart);
        push_number(S, intpart);
        push_number(S, fracpart);
        return 2;
    }

    static int math_hypot(State* S)
    {
        FP x = to_number(S, 0);
        FP y = to_number(S, 1);
        push_number(S, std::hypot(x, y));
        return 1;
    }

    static int math_ldexp(State* S)
    {
        FP x = to_number(S, 0);
        Integer exp = to_integer(S, 1);
        push_number(S, std::ldexp(x, static_cast<int>(exp)));
        return 1;
    }

    static int math_frexp(State* S)
    {
        FP x = to_number(S, 0);
        int exp;
        FP m = std::frexp(x, &exp);
        push_number(S, m);
        push_integer(S, exp);
        return 2;
    }

    // Type checking
    static int math_isnan(State* S)
    {
        FP n = to_number(S, 0);
        push_boolean(S, std::isnan(n));
        return 1;
    }

    static int math_isfinite(State* S)
    {
        FP n = to_number(S, 0);
        push_boolean(S, std::isfinite(n));
        return 1;
    }

    static int math_isinf(State* S)
    {
        FP n = to_number(S, 0);
        push_boolean(S, std::isinf(n));
        return 1;
    }

    void load_lib_math(State* S, bool make_global)
    {
        static constexpr ModuleReg math_funcs[] = {
            { "abs", math_abs },
            { "floor", math_floor },
            { "ceil", math_ceil },
            { "round", math_round },
            { "trunc", math_trunc },
            { "sqrt", math_sqrt },
            { "cbrt", math_cbrt },
            { "pow", math_pow },
            { "exp", math_exp },
            { "expm1", math_expm1 },
            { "log", math_log },
            { "log10", math_log10 },
            { "log2", math_log2 },
            { "log1p", math_log1p },
            { "sin", math_sin },
            { "cos", math_cos },
            { "tan", math_tan },
            { "asin", math_asin },
            { "acos", math_acos },
            { "atan", math_atan },
            { "atan2", math_atan2 },
            { "sinh", math_sinh },
            { "cosh", math_cosh },
            { "tanh", math_tanh },
            { "asinh", math_asinh },
            { "acosh", math_acosh },
            { "atanh", math_atanh },
            { "deg", math_deg },
            { "rad", math_rad },
            { "min", math_min },
            { "max", math_max },
            { "clamp", math_clamp },
            { "sign", math_sign },
            { "fmod", math_fmod },
            { "modf", math_modf },
            { "hypot", math_hypot },
            { "ldexp", math_ldexp },
            { "frexp", math_frexp },
            { "is_nan", math_isnan },
            { "is_finite", math_isfinite },
            { "is_inf", math_isinf },
        };

        static constexpr ModuleConst math_consts[] = {
            { "pi", static_cast<FP>(std::numbers::pi) },
            { "e", static_cast<FP>(std::numbers::e) },
            { "huge", std::numeric_limits<FP>::infinity() },
        };

        ModuleDef math_module = { .funcs = math_funcs, .consts = math_consts };

        create_module(S, "math", math_module, make_global);
    }

} // namespace behl
