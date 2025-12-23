#pragma once

#include "common/print.hpp"
#include "config_internal.hpp"

#include <chrono>

namespace behl
{
    // Generic optimization pipeline that works with any context type
    // Context is passed to each Pass::apply(Context&)
    //
    // All passes must return bool indicating whether changes were made.
    // Pipeline runs passes in a loop until fixpoint (no changes).
    template<typename Context, typename... Passes>
    class OptimizationPipeline
    {
    public:
        template<typename... Args>
        static auto apply(Args&&... args)
        {
            if constexpr (kOptimizationPassTiming || kOptimizationPassDebug)
            {
                println("[Optimization Pipeline] Starting {} passes", sizeof...(Passes));
            }

            Context context{ std::forward<Args>(args)... };
            run_until_fixpoint(context);

            return context.result();
        }

    private:
        static void run_until_fixpoint(Context& context)
        {
            bool changed = true;
            int iteration = 0;
            constexpr int kMaxIterations = 100; // Safety limit

            while (changed && iteration < kMaxIterations)
            {
                if constexpr (kOptimizationPassTiming || kOptimizationPassDebug)
                {
                    println("  [Iteration {}]", iteration++);
                }
                else
                {
                    iteration++;
                }

                changed = false;
                changed = (apply_with_timing<Passes>(context) || ...);

                if constexpr (kOptimizationPassTiming || kOptimizationPassDebug)
                {
                    if (changed)
                    {
                        println("  Changes detected, running another iteration");
                    }
                }
            }

            if (iteration >= kMaxIterations)
            {
                println("  WARNING: Hit maximum iteration limit ({}), optimization may not have converged!", kMaxIterations);
            }
            else if constexpr (kOptimizationPassTiming || kOptimizationPassDebug)
            {
                println("  Reached fixpoint after {} iterations", iteration);
            }
        }

        template<typename Pass>
        static bool apply_with_timing(Context& context)
        {
            if constexpr (kOptimizationPassTiming)
            {
                auto start = std::chrono::high_resolution_clock::now();
                bool changed = Pass::apply(context);
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                println("    [{}] completed in {} μs (changed: {})", Pass::kName, duration.count(), changed);
                return changed;
            }
            else
            {
                return Pass::apply(context);
            }
        }
    };

} // namespace behl
