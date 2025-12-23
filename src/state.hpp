#pragma once

#include "backend/proto.hpp"
#include "common/hash_map.hpp"
#include "common/string.hpp"
#include "common/vector.hpp"
#include "gc/gc_list.hpp"
#include "gc/gc_object.hpp"
#include "gc/gc_state.hpp"
#include "gc/gc_types.hpp"
#include "gc/gco_string.hpp"
#include "state_debug.hpp"
#include "vm/frame.hpp"
#include "vm/upvalue.hpp"
#include "vm/value.hpp"

#include <vector>

namespace behl
{
    struct State
    {
        GCState gc{};

        Vector<Value> stack;
        Vector<CallFrame> call_stack;

        Vector<Value> pinned;
        Vector<size_t> free_pinned_indices;

        UpvalueVector upvalues;
        UpvalueIndexVector open_upvalue_indices;
        Vector<uint32_t> closed_upvalue_freelist;

        Value globals_table{};
        uint32_t cfunction_stack_base = 0;

        // Module system
        HashMap<GCString*, Value, GCStringHash, GCStringEq> module_cache; // Cached module exports
        Vector<GCString*> module_paths;                                   // Module search paths

        // Metatable registry for C modules
        HashMap<GCString*, Value, GCStringHash, GCStringEq> metatable_registry; // Named metatables

        // Debug state
        DebugState debug{};

        PrintHandler print_handler{};
    };

    ptrdiff_t resolve_index(const State* S, int idx);

} // namespace behl
