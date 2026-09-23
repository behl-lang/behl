#include "arithmetic.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace behl::arithmetic
{
    namespace
    {
        struct DD
        {
            FP hi;
            FP lo;
        };

        constexpr FP kLn2Hi = 6.93147180559945286227e-01;
        constexpr FP kLn2Lo = 2.31904681384629956e-17;
        constexpr FP kLog2eHi = 1.44269504088896338700e+00;
        constexpr FP kLog2eLo = 2.03552737408393213e-17;
        constexpr FP kSqrtHalf = 7.07106781186547524401e-01;

        constexpr int kAtanhTerms = 24;
        constexpr int kExpTerms = 26;

        DD two_sum(FP a, FP b) noexcept
        {
            const FP s = a + b;
            const FP bb = s - a;
            return { s, (a - (s - bb)) + (b - bb) };
        }

        DD quick_two_sum(FP a, FP b) noexcept
        {
            const FP s = a + b;
            return { s, b - (s - a) };
        }

        constexpr FP kSplitter = 134217729.0;
        constexpr FP kSplitThreshold = 6.69692879491417e+299;
        constexpr FP kSplitScaleDown = 3.7252902984619140625e-09;
        constexpr FP kSplitScaleUp = 268435456.0;

        DD split(FP a) noexcept
        {
            if (a > kSplitThreshold || a < -kSplitThreshold)
            {
                const FP scaled = a * kSplitScaleDown;
                const FP c = kSplitter * scaled;
                const FP hi = c - (c - scaled);
                return { hi * kSplitScaleUp, (scaled - hi) * kSplitScaleUp };
            }

            const FP c = kSplitter * a;
            const FP hi = c - (c - a);
            return { hi, a - hi };
        }

        DD two_prod(FP a, FP b) noexcept
        {
            const FP p = a * b;
            const DD as = split(a);
            const DD bs = split(b);
            const FP err = ((as.hi * bs.hi - p) + as.hi * bs.lo + as.lo * bs.hi) + as.lo * bs.lo;
            return { p, err };
        }

        DD dd_from(FP a) noexcept
        {
            return { a, 0.0 };
        }

        DD dd_neg(DD a) noexcept
        {
            return { -a.hi, -a.lo };
        }

        DD dd_add(DD a, DD b) noexcept
        {
            const DD s = two_sum(a.hi, b.hi);
            return quick_two_sum(s.hi, s.lo + (a.lo + b.lo));
        }

        DD dd_add_fp(DD a, FP b) noexcept
        {
            const DD s = two_sum(a.hi, b);
            return quick_two_sum(s.hi, s.lo + a.lo);
        }

        DD dd_mul(DD a, DD b) noexcept
        {
            const DD p = two_prod(a.hi, b.hi);
            return quick_two_sum(p.hi, p.lo + (a.hi * b.lo + a.lo * b.hi));
        }

        DD dd_mul_fp(DD a, FP b) noexcept
        {
            const DD p = two_prod(a.hi, b);
            return quick_two_sum(p.hi, p.lo + a.lo * b);
        }

        DD dd_div(DD a, DD b) noexcept
        {
            const FP q1 = a.hi / b.hi;
            DD r = dd_add(a, dd_neg(dd_mul_fp(b, q1)));
            const FP q2 = r.hi / b.hi;
            r = dd_add(r, dd_neg(dd_mul_fp(b, q2)));
            const FP q3 = r.hi / b.hi;
            return dd_add_fp(quick_two_sum(q1, q2), q3);
        }

        DD dd_atanh_series(DD t) noexcept
        {
            const DD t2 = dd_mul(t, t);
            DD acc = dd_from(0.0);

            for (int k = kAtanhTerms; k >= 1; --k)
            {
                const FP coeff = 1.0 / static_cast<FP>(2 * k + 1);
                acc = dd_mul(dd_add_fp(acc, coeff), t2);
            }

            return dd_mul(dd_add_fp(acc, 1.0), t);
        }

        DD dd_log2(FP x) noexcept
        {
            int e = 0;
            FP m = std::frexp(x, &e);

            if (m < kSqrtHalf)
            {
                m *= 2.0;
                e -= 1;
            }

            const DD num = dd_from(m - 1.0);
            const DD den = dd_add_fp(dd_from(m), 1.0);
            const DD t = dd_div(num, den);

            const DD atanh = dd_atanh_series(t);
            const DD log_m = dd_mul_fp(atanh, 2.0);
            const DD log2_m = dd_mul(log_m, DD{ kLog2eHi, kLog2eLo });

            return dd_add_fp(log2_m, static_cast<FP>(e));
        }

        DD dd_exp2_frac(DD f) noexcept
        {
            const DD u = dd_mul(f, DD{ kLn2Hi, kLn2Lo });
            DD acc = dd_from(0.0);

            for (int k = kExpTerms; k >= 1; --k)
            {
                const FP coeff = 1.0 / static_cast<FP>(k);
                acc = dd_mul(dd_add_fp(acc, 1.0), dd_mul_fp(u, coeff));
            }

            return dd_add_fp(acc, 1.0);
        }

        FP trunc_toward_zero(FP x) noexcept
        {
            constexpr FP kTwo52 = 4503599627370496.0;
            if (!(std::fabs(x) < kTwo52))
            {
                return x;
            }
            return static_cast<FP>(static_cast<int64_t>(x));
        }

        FP round_half_away(FP x) noexcept
        {
            const FP t = trunc_toward_zero(x);
            const FP frac = x - t;
            if (frac >= 0.5)
            {
                return t + 1.0;
            }
            if (frac <= -0.5)
            {
                return t - 1.0;
            }
            return t;
        }

        bool is_odd_integer(FP y) noexcept
        {
            if (std::floor(y) != y || std::fabs(y) >= 9007199254740992.0)
            {
                return false;
            }
            const FP half = y * 0.5;
            return std::floor(half) != half;
        }

        FP pow_general(FP x, FP y) noexcept
        {
            const DD l = dd_log2(x);
            const DD z = dd_mul(l, dd_from(y));

            if (z.hi > 1024.0)
            {
                return std::numeric_limits<FP>::infinity();
            }
            if (z.hi < -1080.0)
            {
                return 0.0;
            }

            const FP n = round_half_away(z.hi);
            const DD f = dd_add_fp(z, -n);
            const DD r = dd_exp2_frac(f);

            return std::ldexp(r.hi + r.lo, static_cast<int>(n));
        }
    } // namespace

    FP pow(FP base, FP exp) noexcept
    {
        if (exp == 0.0)
        {
            return 1.0;
        }
        if (base == 1.0)
        {
            return 1.0;
        }
        if (std::isnan(base) || std::isnan(exp))
        {
            return std::numeric_limits<FP>::quiet_NaN();
        }

        if (exp == 1.0)
        {
            return base;
        }
        if (exp == 2.0)
        {
            return base * base;
        }
        if (exp == -1.0)
        {
            return 1.0 / base;
        }
        if (exp == 0.5 && base > 0.0)
        {
            return std::sqrt(base);
        }

        if (base == 0.0)
        {
            const bool negative_zero = std::signbit(base) && is_odd_integer(exp);
            if (exp < 0.0)
            {
                return negative_zero ? -std::numeric_limits<FP>::infinity()
                                     : std::numeric_limits<FP>::infinity();
            }
            return negative_zero ? -0.0 : 0.0;
        }

        if (std::isinf(base))
        {
            const bool flip = std::signbit(base) && is_odd_integer(exp);
            if (exp > 0.0)
            {
                return flip ? -std::numeric_limits<FP>::infinity() : std::numeric_limits<FP>::infinity();
            }
            return flip ? -0.0 : 0.0;
        }

        if (std::isinf(exp))
        {
            const FP magnitude = std::fabs(base);
            if (magnitude == 1.0)
            {
                return 1.0;
            }
            if (magnitude > 1.0)
            {
                return (exp > 0.0) ? std::numeric_limits<FP>::infinity() : 0.0;
            }
            return (exp > 0.0) ? 0.0 : std::numeric_limits<FP>::infinity();
        }

        if (base < 0.0)
        {
            if (std::floor(exp) != exp)
            {
                return std::numeric_limits<FP>::quiet_NaN();
            }
            const FP magnitude = pow_general(-base, exp);
            return is_odd_integer(exp) ? -magnitude : magnitude;
        }

        return pow_general(base, exp);
    }

} // namespace behl::arithmetic
