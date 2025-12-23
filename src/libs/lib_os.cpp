#include "behl.hpp"
#include "state.hpp"

#include <chrono>

namespace behl
{

    static int os_hrtime(State* S)
    {
        using clock = std::chrono::high_resolution_clock;

        const auto elapsed = clock::now() - clock::time_point{};

        double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count();
        push_number(S, seconds);

        return 1;
    }

    static int os_clock(State* S)
    {
        using clock = std::chrono::steady_clock;

        const auto elapsed = clock::now().time_since_epoch();

        double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count();
        push_number(S, seconds);

        return 1;
    }

    static int os_dummy(State* S)
    {
        push_number(S, 1.0);
        return 1;
    }

    void load_lib_os(State* S, bool make_global)
    {
        static constexpr ModuleReg os_funcs[] = {
            { "hrtime", os_hrtime },
            { "clock", os_clock },
            { "dummy", os_dummy },
        };

        ModuleDef os_module = { .funcs = os_funcs };

        create_module(S, "os", os_module, make_global);
    }

} // namespace behl
