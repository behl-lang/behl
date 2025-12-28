#include "behl.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "state.hpp"

namespace behl
{

    static int gc_collect_fn(State* S)
    {
        gc_collect(S);
        return 0;
    }

    static int gc_step_fn(State* S)
    {
        gc_step(S);
        return 0;
    }

    static int gc_count_fn(State* S)
    {
        size_t count = 0;
        for (GCObject* obj = S->gc.gc_all_objects.head(); obj != nullptr; obj = obj->next)
        {
            count++;
        }
        push_integer(S, static_cast<Integer>(count));
        return 1;
    }

    static int gc_countall_fn(State* S)
    {
        push_integer(S, static_cast<Integer>(S->gc.gc_all_objects.count()));
        return 1;
    }

    static int gc_countfree_fn(State* S)
    {
        size_t total = 0;
        push_integer(S, static_cast<Integer>(total));
        return 1;
    }

    static int gc_threshold_fn(State* S)
    {
        push_integer(S, static_cast<Integer>(S->gc.gc_threshold));
        return 1;
    }

    static int gc_setthreshold_fn(State* S)
    {
        auto threshold = check_integer(S, 0);
        if (threshold > 0)
        {
            S->gc.gc_threshold = static_cast<size_t>(threshold);
        }
        return 0;
    }

    static int gc_phase_fn(State* S)
    {
        const char* phase_str = "unknown";
        switch (S->gc.gc_phase)
        {
            case GCPhase::kIdle:
                phase_str = "idle";
                break;
            case GCPhase::kMark:
                phase_str = "mark";
                break;
            case GCPhase::kSweep:
                phase_str = "sweep";
                break;
            case GCPhase::kFinalize:
                phase_str = "finalize";
                break;
        }
        push_string(S, phase_str);
        return 1;
    }

    void load_lib_gc(State* S)
    {
        static constexpr ModuleReg gc_funcs[] = {
            { "collect", gc_collect_fn },
            { "step", gc_step_fn },
            { "count", gc_count_fn },
            { "countall", gc_countall_fn },
            { "countfree", gc_countfree_fn },
            { "threshold", gc_threshold_fn },
            { "setthreshold", gc_setthreshold_fn },
            { "phase", gc_phase_fn },
        };

        ModuleDef gc_module = { .funcs = gc_funcs };

        create_module(S, "gc", gc_module);
    }

} // namespace behl
