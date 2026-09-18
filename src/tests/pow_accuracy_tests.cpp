#include "vm/numeric_ops.hpp"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <vector>

namespace
{
    int64_t ordered_bits(double v)
    {
        const auto raw = std::bit_cast<int64_t>(v);
        return (raw < 0) ? (std::numeric_limits<int64_t>::min() - raw) : raw;
    }

    int64_t ulp_distance(double a, double b)
    {
        if (std::isnan(a) && std::isnan(b))
        {
            return 0;
        }
        if (std::isnan(a) != std::isnan(b))
        {
            return std::numeric_limits<int64_t>::max();
        }
        if (a == b)
        {
            return 0;
        }
        if (std::isinf(a) || std::isinf(b))
        {
            return std::numeric_limits<int64_t>::max();
        }

        const int64_t da = ordered_bits(a);
        const int64_t db = ordered_bits(b);
        return (da > db) ? (da - db) : (db - da);
    }

    struct Summary
    {
        int64_t max_ulp = 0;
        double worst_base = 0.0;
        double worst_exp = 0.0;
        size_t exact = 0;
        size_t within_one = 0;
        size_t total = 0;
    };

    void measure(Summary& s, double base, double exp)
    {
        const double mine = behl::fp_op::pow(base, exp);
        const double theirs = std::pow(base, exp);
        const int64_t d = ulp_distance(mine, theirs);

        s.total++;
        if (d == 0)
        {
            s.exact++;
        }
        if (d <= 1)
        {
            s.within_one++;
        }
        if (d > s.max_ulp)
        {
            s.max_ulp = d;
            s.worst_base = base;
            s.worst_exp = exp;
        }
    }

    void report(const char* name, const Summary& s)
    {
        std::printf("%-22s n=%-8zu max=%-6lld exact=%5.1f%% within1=%5.1f%%  worst pow(%.17g, %.17g)\n", name, s.total,
            static_cast<long long>(s.max_ulp), 100.0 * static_cast<double>(s.exact) / static_cast<double>(s.total),
            100.0 * static_cast<double>(s.within_one) / static_cast<double>(s.total), s.worst_base, s.worst_exp);
    }
} // namespace

TEST(PowAccuracy, RandomWideRange)
{
    std::mt19937_64 rng(12345);
    std::uniform_real_distribution<double> base_exp(-300.0, 300.0);
    std::uniform_real_distribution<double> mant(1.0, 2.0);
    std::uniform_real_distribution<double> y_dist(-40.0, 40.0);

    Summary s;
    for (int i = 0; i < 200000; ++i)
    {
        const double base = std::ldexp(mant(rng), static_cast<int>(base_exp(rng) / 3.0));
        const double y = y_dist(rng);
        const double probe = std::pow(base, y);
        if (!std::isfinite(probe) || probe == 0.0)
        {
            continue;
        }
        measure(s, base, y);
    }

    report("RandomWideRange", s);
    EXPECT_LE(s.max_ulp, 4);
}

TEST(PowAccuracy, NearOneBase)
{
    std::mt19937_64 rng(999);
    std::uniform_real_distribution<double> base_dist(0.5, 2.0);
    std::uniform_real_distribution<double> y_dist(-100.0, 100.0);

    Summary s;
    for (int i = 0; i < 200000; ++i)
    {
        const double base = base_dist(rng);
        const double y = y_dist(rng);
        const double probe = std::pow(base, y);
        if (!std::isfinite(probe) || probe == 0.0)
        {
            continue;
        }
        measure(s, base, y);
    }

    report("NearOneBase", s);
    EXPECT_LE(s.max_ulp, 4);
}

TEST(PowAccuracy, SmallIntegerExponents)
{
    std::mt19937_64 rng(4242);
    std::uniform_real_distribution<double> base_dist(0.01, 100.0);

    Summary s;
    for (int i = 0; i < 20000; ++i)
    {
        const double base = base_dist(rng);
        for (int y = -20; y <= 20; ++y)
        {
            const double probe = std::pow(base, static_cast<double>(y));
            if (!std::isfinite(probe) || probe == 0.0)
            {
                continue;
            }
            measure(s, base, static_cast<double>(y));
        }
    }

    report("SmallIntegerExponents", s);
    EXPECT_LE(s.max_ulp, 4);
}

TEST(PowAccuracy, ThroughputAgainstStdPow)
{
    std::mt19937_64 rng(7);
    std::uniform_real_distribution<double> base_dist(0.1, 100.0);
    std::uniform_real_distribution<double> y_dist(-30.0, 30.0);

    constexpr int kN = 200000;
    std::vector<double> bs(kN);
    std::vector<double> ys(kN);
    for (int i = 0; i < kN; ++i)
    {
        bs[i] = base_dist(rng);
        ys[i] = y_dist(rng);
    }

    double sink_std = 0.0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kN; ++i)
    {
        sink_std += std::pow(bs[i], ys[i]);
    }
    const auto t1 = std::chrono::steady_clock::now();

    double sink_mine = 0.0;
    for (int i = 0; i < kN; ++i)
    {
        sink_mine += behl::fp_op::pow(bs[i], ys[i]);
    }
    const auto t2 = std::chrono::steady_clock::now();

    const double ns_std = std::chrono::duration<double, std::nano>(t1 - t0).count() / kN;
    const double ns_mine = std::chrono::duration<double, std::nano>(t2 - t1).count() / kN;

    double sink_fma = 0.0;
    const auto t3 = std::chrono::steady_clock::now();
    for (int i = 0; i < kN; ++i)
    {
        sink_fma += std::fma(bs[i], ys[i], sink_fma);
    }
    const auto t4 = std::chrono::steady_clock::now();
    const double ns_fma = std::chrono::duration<double, std::nano>(t4 - t3).count() / kN;

    std::printf("std::pow %7.1f ns   fp_op::pow %7.1f ns   ratio %5.2fx   std::fma %6.2f ns\n", ns_std, ns_mine,
        ns_mine / ns_std, ns_fma);
    EXPECT_NE(sink_fma, 1.0);
    EXPECT_NE(sink_std, 0.0);
    EXPECT_NE(sink_mine, 0.0);
}

TEST(PowAccuracy, SpecialCasesMatchStdPow)
{
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    const double bases[] = { 0.0, -0.0, 1.0, -1.0, 2.0, -2.0, 0.5, -0.5, inf, -inf, nan, 1e300, -1e300 };
    const double exps[] = { 0.0, -0.0, 1.0, -1.0, 2.0, 3.0, -2.0, -3.0, 0.5, -0.5, 2.5, inf, -inf, nan, 1e300 };

    for (double b : bases)
    {
        for (double e : exps)
        {
            const double mine = behl::fp_op::pow(b, e);
            const double theirs = std::pow(b, e);

            if (std::isnan(theirs))
            {
                EXPECT_TRUE(std::isnan(mine)) << "pow(" << b << ", " << e << ") = " << mine << " expected nan";
                continue;
            }

            EXPECT_EQ(ulp_distance(mine, theirs), 0)
                << "pow(" << b << ", " << e << ") mine=" << mine << " std=" << theirs;
            if (mine == 0.0 && theirs == 0.0)
            {
                EXPECT_EQ(std::signbit(mine), std::signbit(theirs)) << "sign of zero for pow(" << b << ", " << e << ")";
            }
        }
    }
}
