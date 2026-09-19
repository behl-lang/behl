#include "charconv.hpp"

#include "charconv_tables.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>

#if defined(_MSC_VER) && !defined(__SIZEOF_INT128__)
#    include <intrin.h>
#endif

namespace behl
{
    namespace
    {
        constexpr int kMaxDigits = 800;

        struct Decimal
        {
            uint8_t digits[kMaxDigits];
            int num_digits{};
            int decimal_point{};
            bool negative{};
            bool truncated{};
        };

        template<typename T>
        struct FloatInfo;

        template<>
        struct FloatInfo<double>
        {
            static constexpr int mant_bits = 52;
            static constexpr int exp_bits = 11;
            static constexpr int bias = -1023;
            [[maybe_unused]] static constexpr int shortest_digits = 17;
        };

        template<>
        struct FloatInfo<float>
        {
            static constexpr int mant_bits = 23;
            static constexpr int exp_bits = 8;
            static constexpr int bias = -127;
            static constexpr int shortest_digits = 9;
        };

        constexpr int kRyuIntervalShift = 2;

        constexpr std::array<int, 9> kPowTab{
            1,
            3,
            6,
            9,
            13,
            16,
            19,
            23,
            26,
        };
        constexpr int kPowTabSize = static_cast<int>(kPowTab.size());
        constexpr int kMaxShift = 27;

        void trim(Decimal& a) noexcept
        {
            while (a.num_digits > 0 && a.digits[a.num_digits - 1] == '0')
            {
                a.num_digits--;
            }
            if (a.num_digits == 0)
            {
                a.decimal_point = 0;
            }
        }

        constexpr uint64_t pow5_of(int k) noexcept
        {
            uint64_t v = 1;
            for (int i = 0; i < k; ++i)
            {
                v *= 5;
            }
            return v;
        }

        int decimal_length(uint64_t v) noexcept;
        void write_digits(uint64_t v, int nd, char* out) noexcept;

        bool prefix_less(const Decimal& a, uint64_t cut) noexcept
        {
            char buf[24];
            const int n = decimal_length(cut);
            write_digits(cut, n, buf);

            for (int i = 0; i < n; ++i)
            {
                if (i >= a.num_digits)
                {
                    return true;
                }
                const char c = static_cast<char>(a.digits[i]);
                if (c != buf[i])
                {
                    return c < buf[i];
                }
            }
            return false;
        }

        void right_shift(Decimal& a, int k) noexcept
        {
            int r = 0;
            int w = 0;
            uint64_t n = 0;

            for (; (n >> k) == 0; ++r)
            {
                if (r >= a.num_digits)
                {
                    if (n == 0)
                    {
                        a.num_digits = 0;
                        return;
                    }
                    while ((n >> k) == 0)
                    {
                        n *= 10;
                        ++r;
                    }
                    break;
                }
                n = n * 10 + static_cast<uint64_t>(a.digits[r] - '0');
            }
            a.decimal_point -= r - 1;

            const uint64_t mask = (1ull << k) - 1;

            for (; r < a.num_digits; ++r)
            {
                const uint64_t c = static_cast<uint64_t>(a.digits[r] - '0');
                const uint64_t dig = n >> k;
                n &= mask;
                a.digits[w++] = static_cast<uint8_t>('0' + dig);
                n = n * 10 + c;
            }

            while (n > 0)
            {
                const uint64_t dig = n >> k;
                n &= mask;
                if (w < kMaxDigits)
                {
                    a.digits[w++] = static_cast<uint8_t>('0' + dig);
                }
                else if (dig != 0)
                {
                    a.truncated = true;
                }
                n = n * 10;
            }

            a.num_digits = w;
            trim(a);
        }

        void left_shift(Decimal& a, int k) noexcept
        {
            const uint64_t cut = pow5_of(k);
            int delta = decimal_length(1ull << k);
            if (prefix_less(a, cut))
            {
                delta--;
            }

            int r = a.num_digits;
            int w = a.num_digits + delta;
            uint64_t n = 0;

            for (--r; r >= 0; --r)
            {
                n += static_cast<uint64_t>(a.digits[r] - '0') << k;
                const uint64_t quo = n / 10;
                const uint64_t rem = n - 10 * quo;
                --w;
                if (w < kMaxDigits)
                {
                    a.digits[w] = static_cast<uint8_t>('0' + rem);
                }
                else if (rem != 0)
                {
                    a.truncated = true;
                }
                n = quo;
            }

            while (n > 0)
            {
                const uint64_t quo = n / 10;
                const uint64_t rem = n - 10 * quo;
                --w;
                if (w < kMaxDigits)
                {
                    a.digits[w] = static_cast<uint8_t>('0' + rem);
                }
                else if (rem != 0)
                {
                    a.truncated = true;
                }
                n = quo;
            }

            a.num_digits += delta;
            if (a.num_digits > kMaxDigits)
            {
                a.num_digits = kMaxDigits;
            }
            a.decimal_point += delta;
            trim(a);
        }

        void shift(Decimal& a, int k) noexcept
        {
            if (a.num_digits == 0 || k == 0)
            {
                return;
            }

            if (k > 0)
            {
                while (k > kMaxShift)
                {
                    left_shift(a, kMaxShift);
                    k -= kMaxShift;
                    if (a.num_digits == 0)
                    {
                        return;
                    }
                }
                left_shift(a, k);
                return;
            }

            k = -k;
            while (k > kMaxShift)
            {
                right_shift(a, kMaxShift);
                k -= kMaxShift;
                if (a.num_digits == 0)
                {
                    return;
                }
            }
            right_shift(a, k);
        }

        bool should_round_up(const Decimal& a, int nd) noexcept
        {
            if (nd < 0 || nd >= a.num_digits)
            {
                return false;
            }
            if (a.digits[nd] == '5' && nd + 1 == a.num_digits)
            {
                if (a.truncated)
                {
                    return true;
                }
                return nd > 0 && ((a.digits[nd - 1] - '0') % 2) != 0;
            }
            return a.digits[nd] >= '5';
        }

        uint64_t rounded_integer(const Decimal& a) noexcept
        {
            if (a.decimal_point > 20)
            {
                return std::numeric_limits<uint64_t>::max();
            }

            int i = 0;
            uint64_t n = 0;
            for (; i < a.decimal_point && i < a.num_digits; ++i)
            {
                n = n * 10 + static_cast<uint64_t>(a.digits[i] - '0');
            }
            for (; i < a.decimal_point; ++i)
            {
                n *= 10;
            }
            if (should_round_up(a, a.decimal_point))
            {
                n++;
            }
            return n;
        }

        template<typename T>
        uint64_t decimal_to_bits(Decimal& a, bool& overflow, bool& underflow) noexcept
        {
            using Info = FloatInfo<T>;

            overflow = false;
            underflow = false;
            const bool nonzero = a.num_digits != 0;
            uint64_t mant = 0;
            int exp = 0;

            if (a.num_digits == 0)
            {
                mant = 0;
                exp = Info::bias;
            }
            else if (a.decimal_point > 310)
            {
                overflow = true;
            }
            else if (a.decimal_point < -330)
            {
                mant = 0;
                exp = Info::bias;
            }
            else
            {
                while (a.decimal_point > 0)
                {
                    const int n = (a.decimal_point >= kPowTabSize) ? kMaxShift : kPowTab[static_cast<size_t>(a.decimal_point)];
                    shift(a, -n);
                    exp += n;
                }
                while (a.decimal_point < 0 || (a.decimal_point == 0 && a.digits[0] < '5'))
                {
                    const int n = (-a.decimal_point >= kPowTabSize) ? kMaxShift
                                                                    : kPowTab[static_cast<size_t>(-a.decimal_point)];
                    shift(a, n);
                    exp -= n;
                }

                exp--;

                if (exp < Info::bias + 1)
                {
                    const int n = Info::bias + 1 - exp;
                    shift(a, -n);
                    exp += n;
                }

                if (exp - Info::bias >= (1 << Info::exp_bits) - 1)
                {
                    overflow = true;
                }
                else
                {
                    shift(a, 1 + Info::mant_bits);
                    mant = rounded_integer(a);

                    if (mant == (2ull << Info::mant_bits))
                    {
                        mant >>= 1;
                        exp++;
                        if (exp - Info::bias >= (1 << Info::exp_bits) - 1)
                        {
                            overflow = true;
                        }
                    }

                    if (!overflow && (mant & (1ull << Info::mant_bits)) == 0)
                    {
                        exp = Info::bias;
                    }
                }
            }

            if (overflow)
            {
                mant = 0;
                exp = (1 << Info::exp_bits) - 1 + Info::bias;
            }

            uint64_t bits = mant & ((1ull << Info::mant_bits) - 1);
            bits |= static_cast<uint64_t>((exp - Info::bias) & ((1 << Info::exp_bits) - 1)) << Info::mant_bits;

            if (nonzero && !overflow && bits == 0)
            {
                underflow = true;
            }

            if (a.negative)
            {
                bits |= 1ull << Info::mant_bits << Info::exp_bits;
            }
            return bits;
        }

        constexpr std::array<double, 23> kPow10{
            1e0,
            1e1,
            1e2,
            1e3,
            1e4,
            1e5,
            1e6,
            1e7,
            1e8,
            1e9,
            1e10,
            1e11,
            1e12,
            1e13,
            1e14,
            1e15,
            1e16,
            1e17,
            1e18,
            1e19,
            1e20,
            1e21,
            1e22,
        };

        constexpr int kFastPathMaxExp = 22;

        bool lower_eq(const char* p, const char* last, const char* lit, size_t n) noexcept
        {
            if (static_cast<size_t>(last - p) < n)
            {
                return false;
            }
            for (size_t i = 0; i < n; ++i)
            {
                char c = p[i];
                if (c >= 'A' && c <= 'Z')
                {
                    c = static_cast<char>(c - 'A' + 'a');
                }
                if (c != lit[i])
                {
                    return false;
                }
            }
            return true;
        }

        enum class ScanKind : uint8_t
        {
            kInvalid,
            kNumber,
            kInfinity,
            kNaN,
        };

        struct Scanned
        {
            const char* end{};
            ScanKind kind{ ScanKind::kInvalid };
            bool negative{};
            bool truncated{};
            uint64_t mantissa{};
            int digit_count{};
            int decimal_point{};
        };

        Scanned scan(const char* first, const char* last) noexcept
        {
            Scanned out;
            const char* p = first;

            if (p != last && *p == '-')
            {
                out.negative = true;
                ++p;
            }

            if (lower_eq(p, last, "inf", 3))
            {
                out.kind = ScanKind::kInfinity;
                p += 3;
                if (lower_eq(p, last, "inity", 5))
                {
                    p += 5;
                }
                out.end = p;
                return out;
            }

            if (lower_eq(p, last, "nan", 3))
            {
                out.kind = ScanKind::kNaN;
                p += 3;
                if (p != last && *p == '(')
                {
                    const char* q = p + 1;
                    while (q != last && *q != ')')
                    {
                        const char c = *q;
                        const bool valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                            || c == '_';
                        if (!valid)
                        {
                            break;
                        }
                        ++q;
                    }
                    if (q != last && *q == ')')
                    {
                        p = q + 1;
                    }
                }
                out.end = p;
                return out;
            }

            constexpr int kFastDigits = 19;

            bool saw_dot = false;
            bool saw_digits = false;
            int seen = 0;

            for (; p != last; ++p)
            {
                const char c = *p;
                if (c == '.')
                {
                    if (saw_dot)
                    {
                        break;
                    }
                    saw_dot = true;
                    out.decimal_point = seen;
                    continue;
                }
                if (c < '0' || c > '9')
                {
                    break;
                }

                saw_digits = true;
                if (c == '0' && seen == 0)
                {
                    out.decimal_point--;
                    continue;
                }

                seen++;
                if (out.digit_count < kFastDigits)
                {
                    out.mantissa = out.mantissa * 10 + static_cast<uint64_t>(c - '0');
                    out.digit_count++;
                }
                else if (c != '0')
                {
                    out.truncated = true;
                }
            }

            if (!saw_digits)
            {
                return out;
            }
            if (!saw_dot)
            {
                out.decimal_point = seen;
            }

            out.end = p;

            if (p != last && (*p == 'e' || *p == 'E'))
            {
                const char* q = p + 1;
                bool exp_neg = false;
                if (q != last && (*q == '+' || *q == '-'))
                {
                    exp_neg = (*q == '-');
                    ++q;
                }
                if (q != last && *q >= '0' && *q <= '9')
                {
                    int e = 0;
                    while (q != last && *q >= '0' && *q <= '9')
                    {
                        if (e < 100000)
                        {
                            e = e * 10 + (*q - '0');
                        }
                        ++q;
                    }
                    out.decimal_point += exp_neg ? -e : e;
                    out.end = q;
                }
            }

            while (out.digit_count > 0 && out.mantissa % 10 == 0)
            {
                out.mantissa /= 10;
                out.digit_count--;
            }

            out.kind = ScanKind::kNumber;
            return out;
        }

        uint64_t umul128(uint64_t a, uint64_t b, uint64_t& hi) noexcept;

        struct EiselLemire
        {
            uint64_t mantissa;
            int power2;
        };

        constexpr int kElDeclined = -1;

        int log2_pow10(int q) noexcept
        {
            return (((152170 + 65536) * q) >> 16) + 63;
        }

        EiselLemire eisel_lemire(uint64_t w, int q) noexcept
        {
            using Info = FloatInfo<double>;

            if (w == 0 || q > ryu::kLargestPowerOfFive || q < ryu::kSmallestPowerOfFive)
            {
                return { 0, kElDeclined };
            }

            const int lz = std::countl_zero(w);
            w <<= lz;

            const int index = 2 * (q - ryu::kSmallestPowerOfFive);
            uint64_t hi = 0;
            uint64_t lo = umul128(w, ryu::kPowerOfFive128[index], hi);

            constexpr uint64_t kPrecisionMask = std::numeric_limits<uint64_t>::max() >> 55;
            if ((hi & kPrecisionMask) == kPrecisionMask)
            {
                uint64_t hi2 = 0;
                umul128(w, ryu::kPowerOfFive128[index + 1], hi2);
                const uint64_t sum = lo + hi2;
                if (sum < lo)
                {
                    hi++;
                }
                lo = sum;
            }

            if (lo == std::numeric_limits<uint64_t>::max())
            {
                const bool safe = (q >= -27) && (q <= 55);
                if (!safe)
                {
                    return { 0, kElDeclined };
                }
            }

            const int upperbit = static_cast<int>(hi >> 63);
            uint64_t mantissa = hi >> (upperbit + 9);
            int power2 = log2_pow10(q) + upperbit - lz - Info::bias;

            if (power2 <= 0)
            {
                if (-power2 + 1 >= 64)
                {
                    return { 0, kElDeclined };
                }
                mantissa >>= -power2 + 1;
                mantissa += mantissa & 1;
                mantissa >>= 1;
                if (mantissa == 0)
                {
                    return { 0, kElDeclined };
                }
                power2 = (mantissa < (1ull << Info::mant_bits)) ? 0 : 1;
                return { mantissa, power2 };
            }

            if (lo <= 1 && q >= -4 && q <= 23 && (mantissa & 3) == 1)
            {
                if ((mantissa << (upperbit + 9)) == hi)
                {
                    mantissa &= ~1ull;
                }
            }

            mantissa += mantissa & 1;
            mantissa >>= 1;

            if (mantissa >= (2ull << Info::mant_bits))
            {
                mantissa = (1ull << Info::mant_bits);
                power2++;
            }
            mantissa &= ~(1ull << Info::mant_bits);

            if (power2 >= ((1 << Info::exp_bits) - 1))
            {
                return { 0, kElDeclined };
            }

            return { mantissa, power2 };
        }

        Decimal build_decimal(const char* first, const char* last) noexcept
        {
            Decimal d;
            const char* p = first;
            if (p != last && *p == '-')
            {
                d.negative = true;
                ++p;
            }

            bool saw_dot = false;
            for (; p != last; ++p)
            {
                const char c = *p;
                if (c == '.')
                {
                    if (saw_dot)
                    {
                        break;
                    }
                    saw_dot = true;
                    d.decimal_point = d.num_digits;
                    continue;
                }
                if (c < '0' || c > '9')
                {
                    break;
                }
                if (c == '0' && d.num_digits == 0)
                {
                    d.decimal_point--;
                    continue;
                }
                if (d.num_digits < kMaxDigits)
                {
                    d.digits[d.num_digits] = static_cast<uint8_t>(c);
                    d.num_digits++;
                }
                else if (c != '0')
                {
                    d.truncated = true;
                }
            }

            if (!saw_dot)
            {
                d.decimal_point = d.num_digits;
            }

            if (p != last && (*p == 'e' || *p == 'E'))
            {
                const char* q = p + 1;
                bool exp_neg = false;
                if (q != last && (*q == '+' || *q == '-'))
                {
                    exp_neg = (*q == '-');
                    ++q;
                }
                if (q != last && *q >= '0' && *q <= '9')
                {
                    int e = 0;
                    while (q != last && *q >= '0' && *q <= '9')
                    {
                        if (e < 100000)
                        {
                            e = e * 10 + (*q - '0');
                        }
                        ++q;
                    }
                    d.decimal_point += exp_neg ? -e : e;
                }
            }

            trim(d);
            return d;
        }

        uint64_t umul128(uint64_t a, uint64_t b, uint64_t& hi) noexcept
        {
#if defined(__SIZEOF_INT128__)
            const __uint128_t p = static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
            hi = static_cast<uint64_t>(p >> 64);
            return static_cast<uint64_t>(p);
#elif defined(_M_X64)
            return _umul128(a, b, &hi);
#elif defined(_M_ARM64)
            hi = __umulh(a, b);
            return a * b;
#else
            const uint32_t a_lo = static_cast<uint32_t>(a);
            const uint32_t a_hi = static_cast<uint32_t>(a >> 32);
            const uint32_t b_lo = static_cast<uint32_t>(b);
            const uint32_t b_hi = static_cast<uint32_t>(b >> 32);

            const uint64_t b00 = static_cast<uint64_t>(a_lo) * b_lo;
            const uint64_t b01 = static_cast<uint64_t>(a_lo) * b_hi;
            const uint64_t b10 = static_cast<uint64_t>(a_hi) * b_lo;
            const uint64_t b11 = static_cast<uint64_t>(a_hi) * b_hi;

            const uint64_t mid = (b00 >> 32) + static_cast<uint32_t>(b01) + static_cast<uint32_t>(b10);
            hi = b11 + (b01 >> 32) + (b10 >> 32) + (mid >> 32);
            return (mid << 32) | static_cast<uint32_t>(b00);
#endif
        }

        constexpr uint64_t shift_right_128(uint64_t lo, uint64_t hi, int dist) noexcept
        {
            return (hi << (64 - dist)) | (lo >> dist);
        }

        constexpr uint32_t log10_pow2(int e) noexcept
        {
            return static_cast<uint32_t>((static_cast<uint64_t>(e) * 78913) >> 18);
        }

        constexpr uint32_t log10_pow5(int e) noexcept
        {
            return static_cast<uint32_t>((static_cast<uint64_t>(e) * 732923) >> 20);
        }

        constexpr int pow5_bits(int e) noexcept
        {
            return static_cast<int>(((static_cast<uint32_t>(e) * 1217359) >> 19) + 1);
        }

        constexpr uint64_t kInverseOfFive = 14757395258967641293ull;
        constexpr uint64_t kMaxDivisibleByFive = std::numeric_limits<uint64_t>::max() / 5;

        uint32_t pow5_factor(uint64_t value) noexcept
        {
            uint32_t count = 0;
            for (;;)
            {
                value *= kInverseOfFive;
                if (value > kMaxDivisibleByFive)
                {
                    return count;
                }
                ++count;
            }
        }

        bool multiple_of_pow5(uint64_t value, uint32_t p) noexcept
        {
            return pow5_factor(value) >= p;
        }

        bool multiple_of_pow2(uint64_t value, uint32_t p) noexcept
        {
            return (value & ((1ull << p) - 1)) == 0;
        }

        uint64_t mul_shift_64(uint64_t m, const uint64_t* mul, int j) noexcept
        {
            uint64_t high1 = 0;
            const uint64_t low1 = umul128(m, mul[1], high1);
            uint64_t high0 = 0;
            umul128(m, mul[0], high0);
            const uint64_t sum = high0 + low1;
            if (sum < high0)
            {
                ++high1;
            }
            return shift_right_128(sum, high1, j - 64);
        }

        constexpr std::array<uint64_t, 20> kPow10U64{
            1ull,
            10ull,
            100ull,
            1000ull,
            10000ull,
            100000ull,
            1000000ull,
            10000000ull,
            100000000ull,
            1000000000ull,
            10000000000ull,
            100000000000ull,
            1000000000000ull,
            10000000000000ull,
            100000000000000ull,
            1000000000000000ull,
            10000000000000000ull,
            100000000000000000ull,
            1000000000000000000ull,
            10000000000000000000ull,
        };

        constexpr char kDigits2[201] = "00010203040506070809"
                                       "10111213141516171819"
                                       "20212223242526272829"
                                       "30313233343536373839"
                                       "40414243444546474849"
                                       "50515253545556575859"
                                       "60616263646566676869"
                                       "70717273747576777879"
                                       "80818283848586878889"
                                       "90919293949596979899";

        int decimal_length(uint64_t v) noexcept
        {
            const int log2 = 63 - std::countl_zero(v | 1);
            const int est = (log2 * 1233) >> 12;
            return est + 1 + ((v >= kPow10U64[static_cast<size_t>(est + 1)]) ? 1 : 0);
        }

        void write_two(char* out, uint32_t value) noexcept
        {
            const uint32_t c = value * 2;
            out[0] = kDigits2[c];
            out[1] = kDigits2[c + 1];
        }

        void write_digits(uint64_t v, int nd, char* out) noexcept
        {
            int i = nd;

            while (v >= 100000000)
            {
                const uint64_t q = v / 100000000;
                uint32_t r = static_cast<uint32_t>(v - q * 100000000);
                v = q;

                for (int k = 0; k < 4; ++k)
                {
                    const uint32_t rq = r / 100;
                    i -= 2;
                    write_two(out + i, r - rq * 100);
                    r = rq;
                }
            }

            uint32_t v32 = static_cast<uint32_t>(v);
            while (v32 >= 100)
            {
                const uint32_t q = v32 / 100;
                i -= 2;
                write_two(out + i, v32 - q * 100);
                v32 = q;
            }

            if (v32 >= 10)
            {
                i -= 2;
                write_two(out + i, v32);
            }
            else
            {
                out[--i] = static_cast<char>('0' + v32);
            }
        }

        struct RyuResult
        {
            uint64_t output;
            int exp;
        };

        RyuResult ryu_shortest(uint64_t ieee_mantissa, uint32_t ieee_exponent) noexcept
        {
            using Info = FloatInfo<double>;

            int e2 = 0;
            uint64_t m2 = 0;
            if (ieee_exponent == 0)
            {
                e2 = 1 + Info::bias - Info::mant_bits - kRyuIntervalShift;
                m2 = ieee_mantissa;
            }
            else
            {
                e2 = static_cast<int>(ieee_exponent) + Info::bias - Info::mant_bits - kRyuIntervalShift;
                m2 = (1ull << Info::mant_bits) | ieee_mantissa;
            }

            const bool accept_bounds = (m2 & 1) == 0;

            const uint64_t mv = 4 * m2;
            const uint32_t mm_shift = (ieee_mantissa != 0 || ieee_exponent <= 1) ? 1u : 0u;

            uint64_t vr = 0;
            uint64_t vp = 0;
            uint64_t vm = 0;
            int e10 = 0;
            bool vm_trailing_zeros = false;
            bool vr_trailing_zeros = false;

            if (e2 >= 0)
            {
                const uint32_t q = log10_pow2(e2) - (e2 > 3 ? 1u : 0u);
                e10 = static_cast<int>(q);
                const int k = ryu::kPow5InvBitCount + pow5_bits(static_cast<int>(q)) - 1;
                const int j = -e2 + static_cast<int>(q) + k;

                const uint64_t* mul = ryu::kPow5InvSplit[q];
                vr = mul_shift_64(mv, mul, j);
                vp = mul_shift_64(mv + 2, mul, j);
                vm = mul_shift_64(mv - 1 - mm_shift, mul, j);

                if (q <= 21)
                {
                    if (mv % 5 == 0)
                    {
                        vr_trailing_zeros = multiple_of_pow5(mv, q);
                    }
                    else if (accept_bounds)
                    {
                        vm_trailing_zeros = multiple_of_pow5(mv - 1 - mm_shift, q);
                    }
                    else
                    {
                        vp -= multiple_of_pow5(mv + 2, q) ? 1u : 0u;
                    }
                }
            }
            else
            {
                const uint32_t q = log10_pow5(-e2) - (-e2 > 1 ? 1u : 0u);
                e10 = static_cast<int>(q) + e2;
                const int i = -e2 - static_cast<int>(q);
                const int k = pow5_bits(i) - ryu::kPow5BitCount;
                const int j = static_cast<int>(q) - k;

                const uint64_t* mul = ryu::kPow5Split[i];
                vr = mul_shift_64(mv, mul, j);
                vp = mul_shift_64(mv + 2, mul, j);
                vm = mul_shift_64(mv - 1 - mm_shift, mul, j);

                if (q <= 1)
                {
                    vr_trailing_zeros = true;
                    if (accept_bounds)
                    {
                        vm_trailing_zeros = mm_shift == 1;
                    }
                    else
                    {
                        --vp;
                    }
                }
                else if (q < 63)
                {
                    vr_trailing_zeros = multiple_of_pow2(mv, q);
                }
            }

            int removed = 0;
            uint8_t last_removed = 0;
            uint64_t output = 0;

            if (vm_trailing_zeros || vr_trailing_zeros)
            {
                for (;;)
                {
                    const uint64_t vp_div10 = vp / 10;
                    const uint64_t vm_div10 = vm / 10;
                    if (vp_div10 <= vm_div10)
                    {
                        break;
                    }
                    const uint32_t vm_mod10 = static_cast<uint32_t>(vm - 10 * vm_div10);
                    const uint64_t vr_div10 = vr / 10;
                    const uint32_t vr_mod10 = static_cast<uint32_t>(vr - 10 * vr_div10);
                    vm_trailing_zeros = vm_trailing_zeros && vm_mod10 == 0;
                    vr_trailing_zeros = vr_trailing_zeros && last_removed == 0;
                    last_removed = static_cast<uint8_t>(vr_mod10);
                    vr = vr_div10;
                    vp = vp_div10;
                    vm = vm_div10;
                    ++removed;
                }

                if (vm_trailing_zeros)
                {
                    for (;;)
                    {
                        const uint64_t vm_div10 = vm / 10;
                        const uint32_t vm_mod10 = static_cast<uint32_t>(vm - 10 * vm_div10);
                        if (vm_mod10 != 0)
                        {
                            break;
                        }
                        const uint64_t vp_div10 = vp / 10;
                        const uint64_t vr_div10 = vr / 10;
                        const uint32_t vr_mod10 = static_cast<uint32_t>(vr - 10 * vr_div10);
                        vr_trailing_zeros = vr_trailing_zeros && last_removed == 0;
                        last_removed = static_cast<uint8_t>(vr_mod10);
                        vr = vr_div10;
                        vp = vp_div10;
                        vm = vm_div10;
                        ++removed;
                    }
                }

                if (vr_trailing_zeros && last_removed == 5 && vr % 2 == 0)
                {
                    last_removed = 4;
                }

                const bool tie = (vr == vm) && (!accept_bounds || !vm_trailing_zeros);
                output = vr + ((tie || last_removed >= 5) ? 1u : 0u);
            }
            else
            {
                bool round_up = false;
                const uint64_t vp_div100 = vp / 100;
                const uint64_t vm_div100 = vm / 100;
                if (vp_div100 > vm_div100)
                {
                    const uint64_t vr_div100 = vr / 100;
                    const uint32_t vr_mod100 = static_cast<uint32_t>(vr - 100 * vr_div100);
                    round_up = vr_mod100 >= 50;
                    vr = vr_div100;
                    vp = vp_div100;
                    vm = vm_div100;
                    removed += 2;
                }

                for (;;)
                {
                    const uint64_t vp_div10 = vp / 10;
                    const uint64_t vm_div10 = vm / 10;
                    if (vp_div10 <= vm_div10)
                    {
                        break;
                    }
                    const uint64_t vr_div10 = vr / 10;
                    const uint32_t vr_mod10 = static_cast<uint32_t>(vr - 10 * vr_div10);
                    round_up = vr_mod10 >= 5;
                    vr = vr_div10;
                    vp = vp_div10;
                    vm = vm_div10;
                    ++removed;
                }

                output = vr + ((vr == vm || round_up) ? 1u : 0u);
            }

            return { output, e10 + removed };
        }

        Decimal exact_decimal(uint64_t mant, int exp2, bool neg) noexcept
        {
            Decimal out;
            uint8_t tmp[24];
            int n = 0;
            uint64_t m = mant;
            while (m != 0)
            {
                tmp[n++] = static_cast<uint8_t>('0' + m % 10);
                m /= 10;
            }

            for (int i = 0; i < n; ++i)
            {
                out.digits[i] = tmp[n - 1 - i];
            }
            out.num_digits = n;
            out.decimal_point = n;
            out.negative = neg;
            out.truncated = false;
            trim(out);
            shift(out, exp2);
            return out;
        }

        int round_digits(const Decimal& src, int p, uint8_t* out, int& dp_out) noexcept
        {
            dp_out = src.decimal_point;

            if (p >= src.num_digits)
            {
                std::memcpy(out, src.digits, static_cast<size_t>(src.num_digits));
                return src.num_digits;
            }

            std::memcpy(out, src.digits, static_cast<size_t>(p));
            int nd = p;

            if (should_round_up(src, p))
            {
                int i = p - 1;
                for (;;)
                {
                    if (out[i] < '9')
                    {
                        out[i]++;
                        break;
                    }
                    out[i] = '0';
                    if (i == 0)
                    {
                        out[0] = '1';
                        dp_out = src.decimal_point + 1;
                        return 1;
                    }
                    --i;
                }
            }

            while (nd > 0 && out[nd - 1] == '0')
            {
                nd--;
            }
            if (nd == 0)
            {
                dp_out = 0;
            }
            return nd;
        }

        template<typename T>
        uint64_t bits_of_digits(const uint8_t* digits, int nd, int dp, bool neg) noexcept
        {
            Decimal t;
            std::memcpy(t.digits, digits, static_cast<size_t>(nd));
            t.num_digits = nd;
            t.decimal_point = dp;
            t.negative = neg;

            bool ov = false;
            bool un = false;
            return decimal_to_bits<T>(t, ov, un);
        }

        struct Shortest
        {
            uint64_t value;
            int num_digits;
            int decimal_point;
        };

        uint64_t digits_to_value(const uint8_t* digits, int nd) noexcept
        {
            uint64_t v = 0;
            for (int i = 0; i < nd; ++i)
            {
                v = v * 10 + static_cast<uint64_t>(digits[i] - '0');
            }
            return v;
        }

        template<typename T>
        Shortest shortest_digits(const Decimal& exact, uint64_t target, bool neg) noexcept
        {
            uint8_t digits[24];

            for (int p = 1; p <= FloatInfo<T>::shortest_digits; ++p)
            {
                int dp = 0;
                const int nd = round_digits(exact, p, digits, dp);
                if (bits_of_digits<T>(digits, nd, dp, neg) == target)
                {
                    return { digits_to_value(digits, nd), nd, dp };
                }
            }

            int dp = 0;
            const int nd = round_digits(exact, FloatInfo<T>::shortest_digits, digits, dp);
            return { digits_to_value(digits, nd), nd, dp };
        }

        template<size_t N>
        std::to_chars_result emit(char* first, char* last, const char (&text)[N]) noexcept
        {
            constexpr size_t n = N - 1;
            if (static_cast<size_t>(last - first) < n)
            {
                return { last, std::errc::value_too_large };
            }
            std::memcpy(first, text, n);
            return { first + n, std::errc{} };
        }

        char* fill_small(char* dst, char c, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                dst[i] = c;
            }
            return dst + n;
        }

        int rendered_length(const Shortest& s, bool neg) noexcept
        {
            const int sign = neg ? 1 : 0;

            if (s.num_digits == 0)
            {
                return sign + 3;
            }

            const int exp10 = s.decimal_point - 1;

            if (exp10 < -4 || exp10 >= 16)
            {
                const int e = (exp10 < 0) ? -exp10 : exp10;
                return sign + 1 + ((s.num_digits > 1) ? s.num_digits : 0) + 2 + ((e >= 100) ? 3 : 2);
            }
            if (s.decimal_point <= 0)
            {
                return sign + 2 - s.decimal_point + s.num_digits;
            }
            if (s.decimal_point >= s.num_digits)
            {
                return sign + s.decimal_point + 2;
            }
            return sign + s.num_digits + 1;
        }

        std::to_chars_result render(char* first, char* last, const Shortest& s, bool neg) noexcept
        {
            if (last - first < rendered_length(s, neg))
            {
                return { last, std::errc::value_too_large };
            }

            char* w = first;

            if (neg)
            {
                *w++ = '-';
            }

            if (s.num_digits == 0)
            {
                *w++ = '0';
                *w++ = '.';
                *w++ = '0';
                return { w, std::errc{} };
            }

            const int exp10 = s.decimal_point - 1;

            if (exp10 < -4 || exp10 >= 16)
            {
                if (s.num_digits > 1)
                {
                    write_digits(s.value, s.num_digits, w + 1);
                    w[0] = w[1];
                    w[1] = '.';
                    w += s.num_digits + 1;
                }
                else
                {
                    *w++ = static_cast<char>('0' + s.value);
                }
                *w++ = 'e';
                int e = exp10;
                if (e < 0)
                {
                    *w++ = '-';
                    e = -e;
                }
                else
                {
                    *w++ = '+';
                }
                if (e >= 100)
                {
                    *w++ = static_cast<char>('0' + e / 100);
                    e %= 100;
                }
                *w++ = kDigits2[e * 2];
                *w++ = kDigits2[e * 2 + 1];
                return { w, std::errc{} };
            }

            if (s.decimal_point <= 0)
            {
                *w++ = '0';
                *w++ = '.';
                w = fill_small(w, '0', -s.decimal_point);
                write_digits(s.value, s.num_digits, w);
                return { w + s.num_digits, std::errc{} };
            }

            if (s.decimal_point >= s.num_digits)
            {
                write_digits(s.value, s.num_digits, w);
                w += s.num_digits;
                w = fill_small(w, '0', s.decimal_point - s.num_digits);
                *w++ = '.';
                *w++ = '0';
                return { w, std::errc{} };
            }

            write_digits(s.value, s.num_digits, w + 1);
            for (int i = 0; i < s.decimal_point; ++i)
            {
                w[i] = w[i + 1];
            }
            w[s.decimal_point] = '.';
            return { w + s.num_digits + 1, std::errc{} };
        }

        template<typename T>
        std::to_chars_result format(char* first, char* last, T value) noexcept
        {
            using Info = FloatInfo<T>;

            uint64_t bits = 0;
            if constexpr (std::is_same_v<T, double>)
            {
                bits = std::bit_cast<uint64_t>(value);
            }
            else
            {
                bits = std::bit_cast<uint32_t>(value);
            }

            const int total_bits = Info::mant_bits + Info::exp_bits + 1;
            const bool neg = (bits >> (total_bits - 1)) != 0;
            const int biased = static_cast<int>((bits >> Info::mant_bits) & ((1ull << Info::exp_bits) - 1));
            const uint64_t frac = bits & ((1ull << Info::mant_bits) - 1);

            if (biased == (1 << Info::exp_bits) - 1)
            {
                if (frac != 0)
                {
                    return emit(first, last, "nan");
                }
                return neg ? emit(first, last, "-inf") : emit(first, last, "inf");
            }

            uint64_t mant = frac;
            int exp2 = 0;
            if (biased == 0)
            {
                exp2 = Info::bias + 1 - Info::mant_bits;
            }
            else
            {
                mant |= 1ull << Info::mant_bits;
                exp2 = biased + Info::bias - Info::mant_bits;
            }

            if (mant == 0)
            {
                Shortest zero{};
                return render(first, last, zero, neg);
            }

            Shortest s;

            if constexpr (std::is_same_v<T, double>)
            {
                if (exp2 <= 0 && exp2 > -53 && (mant & ((1ull << -exp2) - 1)) == 0)
                {
                    uint64_t n = mant >> -exp2;
                    const int dp = decimal_length(n);
                    while (n % 10 == 0)
                    {
                        n /= 10;
                    }
                    return render(first, last, Shortest{ n, decimal_length(n), dp }, neg);
                }

                const RyuResult r = ryu_shortest(frac, static_cast<uint32_t>(biased));
                s.value = r.output;
                s.num_digits = decimal_length(r.output);
                s.decimal_point = r.exp + s.num_digits;
            }
            else
            {
                const Decimal exact = exact_decimal(mant, exp2, neg);
                s = shortest_digits<T>(exact, bits, neg);
            }

            return render(first, last, s, neg);
        }

        template<typename T>
        std::from_chars_result parse(const char* first, const char* last, T& value) noexcept
        {
            Scanned s = scan(first, last);

            switch (s.kind)
            {
                case ScanKind::kInvalid:
                    return { first, std::errc::invalid_argument };

                case ScanKind::kInfinity:
                    value = s.negative ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
                    return { s.end, std::errc{} };

                case ScanKind::kNaN:
                    value = s.negative ? -std::numeric_limits<T>::quiet_NaN() : std::numeric_limits<T>::quiet_NaN();
                    return { s.end, std::errc{} };

                case ScanKind::kNumber:
                    break;
            }

            if constexpr (std::is_same_v<T, double>)
            {
                if (!s.truncated)
                {
                    const int exp10 = s.decimal_point - s.digit_count;
                    if (s.mantissa < (1ull << 53) && exp10 >= -kFastPathMaxExp && exp10 <= kFastPathMaxExp)
                    {
                        double v = static_cast<double>(s.mantissa);
                        if (exp10 >= 0)
                        {
                            v *= kPow10[static_cast<size_t>(exp10)];
                        }
                        else
                        {
                            v /= kPow10[static_cast<size_t>(-exp10)];
                        }
                        value = s.negative ? -v : v;
                        return { s.end, std::errc{} };
                    }

                    const EiselLemire el = eisel_lemire(s.mantissa, exp10);
                    if (el.power2 != kElDeclined)
                    {
                        uint64_t bits = el.mantissa | (static_cast<uint64_t>(el.power2) << FloatInfo<double>::mant_bits);
                        if (s.negative)
                        {
                            bits |= 1ull << 63;
                        }
                        value = std::bit_cast<double>(bits);
                        return { s.end, std::errc{} };
                    }
                }
            }

            Decimal dec = build_decimal(first, s.end);

            bool overflow = false;
            bool underflow = false;
            const uint64_t bits = decimal_to_bits<T>(dec, overflow, underflow);

            if constexpr (std::is_same_v<T, double>)
            {
                value = std::bit_cast<double>(bits);
            }
            else
            {
                value = std::bit_cast<float>(static_cast<uint32_t>(bits));
            }

            if (overflow || underflow)
            {
                return { s.end, std::errc::result_out_of_range };
            }

            return { s.end, std::errc{} };
        }
    } // namespace

    std::from_chars_result from_chars(const char* first, const char* last, double& value) noexcept
    {
        return parse(first, last, value);
    }

    std::from_chars_result from_chars(const char* first, const char* last, float& value) noexcept
    {
        return parse(first, last, value);
    }

    std::to_chars_result to_chars(char* first, char* last, double value) noexcept
    {
        return format<double>(first, last, value);
    }

    std::to_chars_result to_chars(char* first, char* last, float value) noexcept
    {
        return format<float>(first, last, value);
    }

} // namespace behl
