#pragma once

#include "gc_object.hpp"
#include "gc_types.hpp"

#include <behl/exceptions.hpp>
#include <memory>
#include <span>

namespace behl
{
    struct State;
    struct GCString;
    struct GCTable;
    struct UserdataData;
    struct GCClosure;
    struct GCProto;
    struct Proto;

    void gc_init(State* S);
    void gc_collect(State* S);
    void gc_step(State* S);
    void gc_close(State* S);
    void gc_pause(State* S);
    bool gc_is_paused(State* S);
    void gc_resume(State* S);

    // Validation: Call this after storing a GC object to verify it's actually on the stack
    void gc_validate_on_stack(State* S, GCObject* obj);

    GCTable* gc_new_table(State* S, size_t initial_array_capacity, size_t initial_hash_capacity);

    GCString* gc_new_string(State* S, std::string_view str);

    GCString* gc_new_string(State* S, std::initializer_list<std::string_view> strings);

    UserdataData* gc_new_userdata(State* S, size_t size);

    GCClosure* gc_new_closure(State* S, GCProto* proto_owner);

    GCProto* gc_new_proto(State* S);

    struct GCPauseGuard
    {
        explicit GCPauseGuard(State* S)
            : S_(S)
        {
            paused_ = gc_is_paused(S);
            if (!paused_)
            {
                gc_pause(S_);
            }
        }

        ~GCPauseGuard()
        {
            if (!paused_)
            {
                gc_resume(S_);
            }
        }

    private:
        State* S_;
        bool paused_ = true;
    };

} // namespace behl
