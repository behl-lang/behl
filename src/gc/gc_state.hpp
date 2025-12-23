#pragma once

#include "gc_list.hpp"
#include "gc_object.hpp"
#include "gc_types.hpp"

namespace behl
{
    struct State;
    struct GCString;
    struct GCTable;
    struct UserdataData;
    struct GCClosure;
    struct GCProto;
    struct Proto;

    struct GCState
    {
        GCPhase gc_phase{};
        bool gc_paused{};
        bool gc_running{}; // Guard against re-entrant GC calls
        GCList gc_all_objects;
        GCList gc_table_pool;
        GCList gc_string_pool;
        GCList gc_closure_pool;
        size_t gc_pool_misses = 0;
        size_t gc_pool_hits = 0;
        size_t gc_pool_limit = kGCMinimumPoolLimit;
        size_t gc_threshold = kGCInitialThreshold;
        size_t gc_step_size = kGCStepSize;
        size_t gc_total_bytes = 0;
        GCObject* gc_work_current{};
        GCObject* gc_gray_list{};
        Vector<UserdataData*> gc_finalize_queue;
        size_t gc_delay_counter{};
        int64_t gc_debt = 0;
    };

} // namespace behl
