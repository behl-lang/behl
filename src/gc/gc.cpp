#include "gc.hpp"

#include "common/print.hpp"
#include "config_internal.hpp"
#include "gc_object.hpp"
#include "gc_types_fmt.hpp"
#include "gco_closure.hpp"
#include "gco_proto.hpp"
#include "gco_string.hpp"
#include "gco_table.hpp"
#include "gco_userdata.hpp"
#include "memory.hpp"
#include "state.hpp"
#include "vm/bytecode.hpp"
#include "vm/vm.hpp"
#include "vm/vm_metatable.hpp"

#include <algorithm>
#include <limits>

namespace behl
{

    static std::string gc_object_to_string(GCObject* obj)
    {
        if constexpr (kGCLoggingEnabled)
        {
            std::string type_info;
            switch (obj->type)
            {
                case GCType::kString:
                {
                    auto* str = static_cast<GCString*>(obj);
                    type_info = ::behl::format("String['{}']", str->view());
                    break;
                }
                case GCType::kTable:
                {
                    auto* table = static_cast<GCTable*>(obj);
                    if (table->has_name())
                    {
                        type_info = ::behl::format(
                            "Table['{}', arr={}, hash={}]", table->get_name(), table->array.size(), table->hash.size());
                    }
                    else
                    {
                        type_info = ::behl::format("Table[arr={}, hash={}]", table->array.size(), table->hash.size());
                    }
                    break;
                }
                case GCType::kClosure:
                    type_info = "Closure";
                    break;
                case GCType::kProto:
                    type_info = "Proto";
                    break;
                case GCType::kUserdata:
                {
                    auto* userdata = static_cast<UserdataData*>(obj);
                    type_info = ::behl::format("Userdata[{}b]", userdata->size);
                    break;
                }
                default:
                    type_info = ::behl::format("Unknown[type={}]", obj->type);
                    break;
            }

            return ::behl::format("{{ color={}, {:p} {} }}", obj->color, static_cast<void*>(obj), type_info);
        }
        else
        {
            return {};
        }
    }

    template<typename... TArgs>
    static void gc_log([[maybe_unused]] behl::format_string<TArgs...> fmt, [[maybe_unused]] TArgs&&... args)
    {
        if constexpr (kGCLoggingEnabled)
        {
            println("[GC] {}", ::behl::format(fmt, std::forward<TArgs>(args)...));
        }
    }

    void gc_init(State* S)
    {
        S->gc.gc_debt = -static_cast<int64_t>(S->gc.gc_threshold);
    }

    void gc_pause(State* S)
    {
        S->gc.gc_paused = true;
    }

    bool gc_is_paused(State* S)
    {
        return S->gc.gc_paused;
    }

    void gc_resume(State* S)
    {
        S->gc.gc_paused = false;
    }

    void gc_validate_on_stack(State* S, GCObject* obj)
    {
        if constexpr (kGCEnableValidation)
        {
            // Search for this object on the stack
            bool found = false;
            size_t found_index = 0;
            for (size_t i = 0; i < S->stack.size(); ++i)
            {
                if (S->stack[i].is_gcobject() && S->stack[i].get_gcobject() == obj)
                {
                    found = true;
                    found_index = i;
                    break;
                }
            }

            if (found)
            {
                println("[GC_VALIDATE] Object {} FOUND on stack at index {}, phase={}, stack_size={}", gc_object_to_string(obj),
                    found_index, S->gc.gc_phase, S->stack.size());
            }
            else
            {
                println("[GC_VALIDATE] !!! ERROR: Object {} NOT FOUND on stack, phase={}, stack_size={}",
                    gc_object_to_string(obj), static_cast<int>(S->gc.gc_phase), S->stack.size());
            }
        }
    }

    template<typename T>
    static T* gc_allocate_object(State* S)
    {
        auto* obj = mem_create<T>(S);

        obj->type = T::kObjectType;
        obj->next = nullptr;
        obj->prev = nullptr;
        obj->color = GCColor::kBlack; // New objects are black (survive current cycle)

        S->gc.gc_all_objects.append(obj);

        return obj;
    }

    GCTable* gc_new_table(State* S, size_t initial_array_capacity, size_t initial_hash_capacity)
    {
        GCTable* new_obj = nullptr;

        if (!S->gc.gc_table_pool.empty())
        {
            S->gc.gc_pool_hits++;

            new_obj = static_cast<GCTable*>(S->gc.gc_table_pool.pop_front());
            S->gc.gc_all_objects.append(new_obj);

            assert(new_obj->type == GCType::kTable);
            new_obj->color = GCColor::kBlack;

            new_obj->metatable = nullptr;
            new_obj->array.reserve(S, initial_array_capacity);
            new_obj->hash.reserve(S, initial_hash_capacity);
        }
        else
        {
            S->gc.gc_pool_misses++;

            new_obj = gc_allocate_object<GCTable>(S);

            new_obj->metatable = nullptr;
            new_obj->array.init(S, initial_array_capacity);
            new_obj->hash.init(S, initial_hash_capacity);
        }

        assert(new_obj != nullptr);

        gc_log("Created GC Object: {}", gc_object_to_string(new_obj));

        return new_obj;
    }

    static GCString* gc_new_string_impl(State* S, const std::initializer_list<std::string_view>& str)
    {
        size_t total_size_required = 0;
        for (auto& s : str)
        {
            total_size_required += s.size();
        }

        GCString* new_obj = nullptr;

        if (!S->gc.gc_string_pool.empty())
        {
            GCString* best_fit = nullptr;
            size_t smallest_capacity_distance = std::numeric_limits<size_t>::max();
            [[maybe_unused]] size_t search_iterations = 0;

            for (auto* candidate = S->gc.gc_string_pool.head(); candidate != nullptr; candidate = candidate->next)
            {
                auto* other = static_cast<GCString*>(candidate);

                size_t other_capacity = other->capacity();
                if (other_capacity == total_size_required)
                {
                    best_fit = other;
                    break;
                }
                else if (other_capacity > total_size_required)
                {
                    if (other->is_sso())
                    {
                        best_fit = other;
                        // Capacity distance doesn't matter for SSO.
                        break;
                    }
                    else
                    {
                        size_t capacity_distance = other_capacity - total_size_required;
                        if (capacity_distance < smallest_capacity_distance)
                        {
                            smallest_capacity_distance = capacity_distance;
                            best_fit = other;
                        }
                    }
                }

                search_iterations++;
            }

            // If its not SSO then check that the capacity difference is not too large.
            // We don't want to waste too much memory by copying small strings into large buffers.
            if (best_fit && !best_fit->is_sso() && smallest_capacity_distance >= 4)
            {
                best_fit = nullptr;
            }

            if (best_fit)
            {
                new_obj = best_fit;

                S->gc.gc_pool_hits++;

                S->gc.gc_string_pool.remove(new_obj);
                S->gc.gc_all_objects.append(new_obj);

                assert(new_obj->type == GCType::kString);
                new_obj->color = GCColor::kBlack;

                // Zero out the buffer if SSO to avoid comparing garbage bytes
                if (new_obj->is_sso())
                {
                    new_obj->sso_reset();
                }

                // Always copy into whatever storage it currently uses.
                size_t offset = 0;
                for (auto& s : str)
                {
                    std::memcpy(new_obj->data() + offset, s.data(), s.size());
                    offset += s.size();
                }

                // Adjust length.
                if (new_obj->is_sso())
                {
                    new_obj->storage.sso.len = static_cast<uint8_t>(total_size_required);
                }
                else
                {
                    new_obj->storage.heap.len = total_size_required;
                }

                gc_log("Created GC Object: {}", gc_object_to_string(new_obj));

                return new_obj;
            }
        }

        S->gc.gc_pool_misses++;

        new_obj = gc_allocate_object<GCString>(S);

        if (total_size_required <= GCString::kSSOCapacity)
        {
            // Small string - use SSO
            size_t offset = 0;
            for (auto& s : str)
            {
                std::memcpy(new_obj->storage.sso.buffer + offset, s.data(), s.size());
                offset += s.size();
            }
            new_obj->storage.sso.len = static_cast<uint8_t>(total_size_required);
        }
        else
        {
            // Large string - use heap
            char* heap_data = static_cast<char*>(mem_alloc(S, total_size_required));

            size_t offset = 0;
            for (auto& s : str)
            {
                std::memcpy(heap_data + offset, s.data(), s.size());
                offset += s.size();
            }

            new_obj->storage.heap.ptr = heap_data;
            new_obj->storage.heap.len = total_size_required;
            new_obj->storage.heap.flag = GCString::kHeapFlag;
        }

        gc_log("Created GC Object: {}", gc_object_to_string(new_obj));

        return new_obj;
    }

    GCString* gc_new_string(State* S, std::initializer_list<std::string_view> strings)
    {
        return gc_new_string_impl(S, strings);
    }

    GCString* gc_new_string(State* S, std::string_view str)
    {
        return gc_new_string_impl(S, { str });
    }

    UserdataData* gc_new_userdata(State* S, size_t size)
    {
        auto* new_obj = gc_allocate_object<UserdataData>(S);
        new_obj->size = size;
        new_obj->metatable = nullptr;

        if (size > 0)
        {
            new_obj->data = mem_alloc(S, size);
        }
        else
        {
            new_obj->data = nullptr;
        }

        gc_log("Created GC Object: {}", gc_object_to_string(new_obj));

        return new_obj;
    }

    GCClosure* gc_new_closure(State* S, GCProto* proto_owner)
    {
        GCClosure* new_obj = nullptr;

        if (!S->gc.gc_closure_pool.empty())
        {
            S->gc.gc_pool_hits++;

            new_obj = static_cast<GCClosure*>(S->gc.gc_closure_pool.pop_front());
            S->gc.gc_all_objects.append(new_obj);

            assert(new_obj->type == GCType::kClosure);
            new_obj->color = GCColor::kBlack;
        }
        else
        {
            S->gc.gc_pool_misses++;

            new_obj = gc_allocate_object<GCClosure>(S);
            new_obj->upvalue_indices.init(S, 0);
        }

        // Check if this proto has upvalues to allocate the vector
        if (proto_owner && proto_owner->has_upvalues && new_obj->upvalue_indices.capacity() == 0)
        {
            new_obj->upvalue_indices.init(S, 0);
        }

        new_obj->proto = proto_owner;

        gc_log("Created GC Object: {}", gc_object_to_string(new_obj));

        return new_obj;
    }

    GCProto* gc_new_proto(State* S)
    {
        auto* new_obj = gc_allocate_object<GCProto>(S);

        gc_log("Created GC Object: {}", gc_object_to_string(new_obj));

        return new_obj;
    }

    static void destroy_string(State* S, GCString* str, bool poolable)
    {
        if (poolable && S->gc.gc_string_pool.count() >= S->gc.gc_pool_limit)
        {
            poolable = false;
        }

        if (poolable)
        {
            S->gc.gc_string_pool.append(str);
        }
        else
        {
            if (!str->is_sso())
            {
                mem_free(S, str->storage.heap.ptr, str->storage.heap.len);
            }

            mem_destroy(S, str);
        }
    }

    static void destroy_table(State* S, GCTable* table, bool poolable)
    {
        if (poolable && S->gc.gc_table_pool.count() >= S->gc.gc_pool_limit)
        {
            poolable = false;
        }

        if (poolable)
        {
            table->metatable = nullptr;
            table->array.clear();
            table->hash.clear();
            table->clear_name();

            S->gc.gc_table_pool.append(table);
        }
        else
        {
            table->array.destroy(S);
            table->hash.destroy(S);

            mem_destroy(S, table);
        }
    }

    static void destroy_closure(State* S, GCClosure* closure, bool poolable)
    {
        // Sort upvalue indices in descending order to process from end to beginning
        auto& indices = closure->upvalue_indices;
        std::sort(indices.begin(), indices.end(), std::greater<uint32_t>());

        // Process each upvalue: pop if at end, otherwise add to freelist
        for (size_t i = 0; i < indices.size(); ++i)
        {
            const uint32_t uv_idx = indices[i];

            // Skip if index is out of bounds or upvalue is still open
            if (uv_idx >= S->upvalues.size() || S->upvalues[uv_idx].is_open())
            {
                continue;
            }

            // If at the end, pop it
            if (uv_idx == S->upvalues.size() - 1)
            {
                S->upvalues.pop_back();
            }
            else
            {
                // Add to freelist for reuse
                S->closed_upvalue_freelist.push_back(S, uv_idx);
            }
        }

        if (poolable && S->gc.gc_closure_pool.count() >= S->gc.gc_pool_limit)
        {
            poolable = false;
        }

        if (poolable)
        {
            closure->proto = nullptr;
            closure->upvalue_indices.clear();

            S->gc.gc_closure_pool.append(closure);
        }
        else
        {
            closure->upvalue_indices.destroy(S);
            mem_destroy(S, closure);
        }
    }

    static void destroy_proto(State* S, GCProto* proto)
    {
        proto->code.destroy(S);
        proto->str_constants.destroy(S);
        proto->int_constants.destroy(S);
        proto->fp_constants.destroy(S);
        proto->protos.destroy(S);
        proto->upvalue_names.destroy(S);
        proto->line_info.destroy(S);
        proto->column_info.destroy(S);

        mem_destroy(S, proto);
    }

    static void destroy_userdata(State* S, UserdataData* userdata)
    {
        if (userdata->data && userdata->size > 0)
        {
            mem_free(S, userdata->data, userdata->size);
        }

        mem_destroy(S, userdata);
    }

    static void destroy_object(State* S, GCObject* obj, bool poolable)
    {
        gc_log("Destroying: {}", gc_object_to_string(obj));

        S->gc.gc_all_objects.remove(obj);

        obj->color = GCColor::kFree;
        // poolable = false;

        switch (obj->type)
        {
            case GCType::kString:
                destroy_string(S, static_cast<GCString*>(obj), poolable);
                break;
            case GCType::kTable:
                destroy_table(S, static_cast<GCTable*>(obj), poolable);
                break;
            case GCType::kClosure:
                destroy_closure(S, static_cast<GCClosure*>(obj), poolable);
                break;
            case GCType::kProto:
                destroy_proto(S, static_cast<GCProto*>(obj));
                break;
            case GCType::kUserdata:
                destroy_userdata(S, static_cast<UserdataData*>(obj));
                break;
            case GCType::kDead:
                break;
        }
    }

    // ===== Mark Phase =====

    static void mark_gray(State* S, GCObject* obj)
    {
        if (obj->color == GCColor::kWhite)
        {
            obj->color = GCColor::kGray;
            gc_log("mark_gray: {}", gc_object_to_string(obj));
            // Push to front of gray list
            obj->gray_next = S->gc.gc_gray_list;
            S->gc.gc_gray_list = obj;
        }
        else
        {
            gc_log("mark_gray SKIP (already {}): {}", obj->color, gc_object_to_string(obj));
        }
    }

    static void mark_value(State* S, const Value& val)
    {
        if (val.is_gcobject())
        {
            GCObject* obj = val.get_gcobject();
            if (obj)
            {
                mark_gray(S, obj);
            }
        }
    }

    static void blacken_table(State* S, GCTable* table)
    {
        // Mark array elements
        for (size_t i = 0; i < table->array.size(); ++i)
        {
            const auto& val = table->array[i];
            if (val.is_gcobject())
            {
                if (GCObject* obj = val.get_gcobject())
                {
                    mark_gray(S, obj);
                }
            }
        }

        // Mark hash entries
        for (auto& entry : table->hash)
        {
            // Mark key if it's a GC object
            if (entry.first.is_gcobject())
            {
                if (GCObject* key_obj = entry.first.get_gcobject())
                {
                    mark_gray(S, key_obj);
                }
            }

            // Mark value if it's a GC object
            if (entry.second.is_gcobject())
            {
                if (GCObject* val_obj = entry.second.get_gcobject())
                {
                    mark_gray(S, val_obj);
                }
            }
        }

        // Mark metatable
        if (table->metatable)
        {
            mark_gray(S, table->metatable);
        }
    }

    static void blacken_closure(State* S, GCClosure* closure)
    {
        // Mark proto owner
        if (closure->proto)
        {
            mark_gray(S, closure->proto);
        }
    }

    static void blacken_proto(State* S, GCProto* proto)
    {
        // Mark string constants
        for (const auto& constant : proto->str_constants)
        {
            mark_value(S, constant);
        }

        // Mark nested protos as GC objects (they will be blackened separately)
        for (auto* nested_proto : proto->protos)
        {
            mark_gray(S, nested_proto);
        }

        // Mark upvalue names
        for (auto* upvalue_name : proto->upvalue_names)
        {
            mark_gray(S, upvalue_name);
        }

        // Mark string fields
        if (proto->source_name)
        {
            mark_gray(S, proto->source_name);
        }
        if (proto->source_path)
        {
            mark_gray(S, proto->source_path);
        }
        if (proto->name)
        {
            mark_gray(S, proto->name);
        }
    }

    static void blacken_userdata(State* S, UserdataData* userdata)
    {
        if (userdata->metatable)
        {
            mark_gray(S, userdata->metatable);
        }
    }

    static void blacken_object(State* S, GCObject* obj)
    {
        gc_log("blacken_object: {}", gc_object_to_string(obj));
        obj->color = GCColor::kBlack;

        switch (obj->type)
        {
            case GCType::kTable:
                blacken_table(S, static_cast<GCTable*>(obj));
                break;
            case GCType::kClosure:
                blacken_closure(S, static_cast<GCClosure*>(obj));
                break;
            case GCType::kProto:
                blacken_proto(S, static_cast<GCProto*>(obj));
                break;
            case GCType::kUserdata:
                blacken_userdata(S, static_cast<UserdataData*>(obj));
                break;
            case GCType::kString:
                // Strings have no references
                break;
            case GCType::kDead:
                // Dead objects shouldn't be blackened
                break;
        }
    }

    // ===== GC Phases =====

    static void gc_switch_phase(State* S, GCPhase next_phase)
    {
        gc_log("Phase transition: {} -> {}, objects={}, debt={}", S->gc.gc_phase, next_phase, S->gc.gc_all_objects.count(),
            S->gc.gc_debt);

        S->gc.gc_phase = next_phase;
    }

    static void gc_start_cycle(State* S)
    {
        gc_log("===== STARTING GC CYCLE =====");
        gc_log("Total objects: {}", S->gc.gc_all_objects.count());
        gc_log("Current debt: {}", S->gc.gc_debt);
        gc_log("Current phase before start: {}", S->gc.gc_phase);

        gc_switch_phase(S, GCPhase::kMark);
        S->gc.gc_gray_list = nullptr; // Clear gray list
        S->gc.gc_finalize_queue.clear();

        // Turn all black objects white
        size_t white_count = 0;
        size_t black_kept = 0;
        for (GCObject* obj = S->gc.gc_all_objects.head(); obj; obj = obj->next)
        {
            if (obj->color == GCColor::kBlack)
            {
                obj->color = GCColor::kWhite;
                white_count++;
                gc_log("  Turned BLACK->WHITE: {}", gc_object_to_string(obj));

                // VALIDATION: Check if this object is on the stack
                if constexpr (kGCEnableValidation)
                {
                    for (size_t i = 0; i < S->stack.size(); ++i)
                    {
                        if (S->stack[i].is_gcobject() && S->stack[i].get_gcobject() == obj)
                        {
                            gc_log("    ^ This object is on stack at index {}", i);
                            break;
                        }
                    }
                }
            }
            else
            {
                black_kept++;
            }
        }

        gc_log("Turned {} objects white, {} stayed black/gray", white_count, black_kept);

        // Mark roots
        gc_log("Marking roots...");

        // Module paths
        gc_log("Marking module paths ({} paths)", S->module_paths.size());
        for (size_t i = 0; i < S->module_paths.size(); ++i)
        {
            mark_gray(S, S->module_paths[i]);
        }

        // Module cache keys and values.
        gc_log("Marking module cache ({} entries)", S->module_cache.size());
        for (auto it = S->module_cache.begin(); it != S->module_cache.end(); ++it)
        {
            mark_gray(S, it->first);
            mark_value(S, it->second);
        }

        // Metatable registry keys and values.
        gc_log("Marking metatable registry ({} entries)", S->metatable_registry.size());
        for (auto it = S->metatable_registry.begin(); it != S->metatable_registry.end(); ++it)
        {
            mark_gray(S, it->first);
            mark_value(S, it->second);
        }

        // Globals table
        gc_log("Marking globals table");
        mark_gray(S, S->globals_table.get_gcobject());

        // Stack
        auto& stack = S->stack;
        gc_log("Marking stack ({} values, {} frames)", stack.size(), S->call_stack.size());
        for (size_t i = 0; i < stack.size(); ++i)
        {
            const auto& val = stack[i];
            if (val.is_gcobject())
            {
                gc_log("Stack[{}] = {}, marking...", i, gc_object_to_string(val.get_gcobject()));

                mark_value(S, val);
            }
        }

        // Pinned values
        auto& pinned = S->pinned;
        gc_log("Marking pinned values ({} total)", pinned.size());
        for (size_t i = 0; i < pinned.size(); ++i)
        {
            const auto& val = pinned[i];
            if (val.is_gcobject())
            {
                gc_log("Pinned[{}] = {}, marking...", i, gc_object_to_string(val.get_gcobject()));

                mark_value(S, val);
            }
        }

        // Upvalues - mark closed upvalues that hold captured values
        gc_log("Marking upvalues ({} total)", S->upvalues.size());
        for (const auto& upvalue : S->upvalues)
        {
            if (!upvalue.is_open())
            {
                // Closed upvalue - mark the captured value
                mark_value(S, upvalue.closed_value);
                if constexpr (kGCLoggingEnabled)
                {
                    if (upvalue.closed_value.is_gcobject())
                    {
                        gc_log("Marked closed upvalue holding {}", gc_object_to_string(upvalue.closed_value.get_gcobject()));
                    }
                }
            }
            // Open upvalues reference the stack, already marked above
        }

        // Count gray list size for logging
        size_t gray_count = 0;
        for (GCObject* obj = S->gc.gc_gray_list; obj; obj = obj->gray_next)
        {
            gray_count++;
        }

        gc_log("Root marking complete, gray_list size: {}", gray_count);
    }

    static size_t gc_propagate_mark(State* S, size_t work_limit)
    {
        const size_t budget = std::max<size_t>(work_limit, 1);
        size_t work_done = 0;

        while (S->gc.gc_gray_list != nullptr && work_done < budget)
        {
            // Pop from front of gray list
            GCObject* obj = S->gc.gc_gray_list;
            S->gc.gc_gray_list = obj->gray_next;
            obj->gray_next = nullptr; // Clear the link

            blacken_object(S, obj);
            ++work_done;
        }

        if (S->gc.gc_gray_list == nullptr)
        {
            // Before sweeping, find WHITE userdata with finalizers
            // These will be resurrected (marked BLACK) and queued for finalization
            gc_log("Queueing userdata with finalizers");

            size_t queued_count = 0;
            for (GCObject* obj = S->gc.gc_all_objects.head(); obj; obj = obj->next)
            {
                if (obj->color == GCColor::kWhite && obj->type == GCType::kUserdata)
                {
                    auto* userdata = static_cast<UserdataData*>(obj);
                    if (userdata->metatable != nullptr)
                    {
                        Value gc_method = metatable_get_method<MetaMethodType::kGC>(Value(userdata));
                        if (gc_method.is_callable())
                        {
                            // This userdata has a finalizer - resurrect it and queue for finalization
                            if constexpr (kGCLoggingEnabled)
                            {
                                println("[GC] Queueing userdata {:p} for finalization", static_cast<void*>(userdata));
                            }
                            // Mark both the userdata AND its metatable to keep them alive
                            mark_gray(S, userdata);
                            if (userdata->metatable->color == GCColor::kWhite)
                            {
                                mark_gray(S, userdata->metatable);
                            }
                            S->gc.gc_finalize_queue.push_back(S, userdata);
                            queued_count++;
                        }
                    }
                }
            }

            gc_log("Queued {} userdata, gray list empty: {}", queued_count, S->gc.gc_gray_list == nullptr);

            // If we marked any userdata/metatables, process them before transitioning to sweep
            if (S->gc.gc_gray_list == nullptr)
            {
                gc_switch_phase(S, GCPhase::kSweep);
                S->gc.gc_work_current = S->gc.gc_all_objects.head();
            }
        }

        return work_done;
    }

    void gc_update_pool_limits(State* S)
    {
        // Pool limit adjustment strategy:
        // - Grow very slowly and only when we have sustained misses
        // - Shrink aggressively when pools are underutilized
        // - Always trim excess immediately to prevent memory bloat

        const size_t total_requests = S->gc.gc_pool_hits + S->gc.gc_pool_misses;

        // Reset counters
        S->gc.gc_pool_misses = 0;
        S->gc.gc_pool_hits = 0;

        if (total_requests == 0)
        {
            // No activity this cycle - slowly shrink if above minimum
            if (S->gc.gc_pool_limit > kGCMinimumPoolLimit)
            {
                // Shrink by 1 object per idle cycle
                S->gc.gc_pool_limit--;

                if constexpr (kGCLoggingEnabled)
                {
                    println("[GC] Idle cycle, decreased pool limit: {} (no activity)", S->gc.gc_pool_limit);
                }
            }

            // Trim pools to new limit
            while (S->gc.gc_table_pool.count() > S->gc.gc_pool_limit)
            {
                GCObject* obj = S->gc.gc_table_pool.pop_front();
                destroy_object(S, obj, false);
            }

            while (S->gc.gc_string_pool.count() > S->gc.gc_pool_limit)
            {
                GCObject* obj = S->gc.gc_string_pool.pop_front();
                destroy_object(S, obj, false);
            }

            while (S->gc.gc_closure_pool.count() > S->gc.gc_pool_limit)
            {
                GCObject* obj = S->gc.gc_closure_pool.pop_front();
                destroy_object(S, obj, false);
            }

            return;
        }

        const double hit_rate = static_cast<double>(S->gc.gc_pool_hits) / static_cast<double>(total_requests);

        // Only grow if we have VERY poor hit rate and frequent misses
        // Growth is tiny - just 4 objects at a time
        if (hit_rate < 0.50 && S->gc.gc_pool_misses > 20 && S->gc.gc_pool_limit < kGCMaximumPoolLimit)
        {
            // Grow by tiny fixed amount - just 4 objects
            size_t new_limit = S->gc.gc_pool_limit + 4;
            if (new_limit > kGCMaximumPoolLimit)
            {
                new_limit = kGCMaximumPoolLimit;
            }

            gc_log("Increased pool limit: {} -> {} (hit_rate={:.2f}%, misses={})", S->gc.gc_pool_limit, new_limit,
                hit_rate * 100.0, S->gc.gc_pool_misses);

            S->gc.gc_pool_limit = new_limit;
        }

        // Only shrink when truly idle: no pressure AND high hit rate AND few misses
        // This keeps pools available during active periods (even with pressure)
        else if (hit_rate > 0.95 && S->gc.gc_debt <= 0 && S->gc.gc_pool_misses < 5 && S->gc.gc_pool_limit > kGCMinimumPoolLimit)
        {
            // Shrink slowly - just 8 objects at a time
            size_t shrink_amount = 8;
            size_t new_limit = S->gc.gc_pool_limit > shrink_amount ? S->gc.gc_pool_limit - shrink_amount : kGCMinimumPoolLimit;

            if (new_limit < kGCMinimumPoolLimit)
            {
                new_limit = kGCMinimumPoolLimit;
            }

            gc_log("Decreased pool limit: {} -> {} (hit_rate={:.2f}%, idle)", S->gc.gc_pool_limit, new_limit, hit_rate * 100.0);

            S->gc.gc_pool_limit = new_limit;
        }

        // ALWAYS trim pools to current limit - don't let them bloat
        while (S->gc.gc_table_pool.count() > S->gc.gc_pool_limit)
        {
            GCObject* obj = S->gc.gc_table_pool.pop_front();
            destroy_object(S, obj, false);
        }

        while (S->gc.gc_string_pool.count() > S->gc.gc_pool_limit)
        {
            GCObject* obj = S->gc.gc_string_pool.pop_front();
            destroy_object(S, obj, false);
        }

        while (S->gc.gc_closure_pool.count() > S->gc.gc_pool_limit)
        {
            GCObject* obj = S->gc.gc_closure_pool.pop_front();
            destroy_object(S, obj, false);
        }

        // Reset counters for next cycle
        S->gc.gc_pool_misses = 0;
        S->gc.gc_pool_hits = 0;
    }

    static void gc_adjust_threshold(State* S)
    {
        size_t total = S->gc.gc_total_bytes;

        // Tight 1.2x multiplier works best for our incremental GC with pooling
        // More frequent small collections are faster than fewer large ones
        size_t new_threshold = total + (total / 5); // 1.2x multiplier

        if (new_threshold < kGCInitialThreshold)
        {
            new_threshold = kGCInitialThreshold;
        }

        S->gc.gc_threshold = new_threshold;
        S->gc.gc_debt = static_cast<int64_t>(S->gc.gc_total_bytes) - static_cast<int64_t>(S->gc.gc_threshold);
    }

    static size_t gc_sweep(State* S, size_t work_limit)
    {
        size_t work_done = 0;

        while (S->gc.gc_work_current && work_done < work_limit)
        {
            GCObject* obj = S->gc.gc_work_current;
            GCObject* next = obj->next;

            if (obj->color == GCColor::kWhite)
            {
                gc_log("Sweep: Checking WHITE object: {}", gc_object_to_string(obj));

                // VALIDATION: Check if this WHITE object is still on the stack
                if constexpr (kGCLoggingEnabled)
                {
                    for (size_t i = 0; i < S->stack.size(); ++i)
                    {
                        if (S->stack[i].is_gcobject() && S->stack[i].get_gcobject() == obj)
                        {
                            gc_log("!!! ERROR: About to sweep WHITE object that's on stack at index {}: {}", i,
                                gc_object_to_string(obj));
                            gc_log("!!! Stack size: {}, Call frames: {}", S->stack.size(), S->call_stack.size());
                            if (!S->call_stack.empty())
                            {
                                for (size_t f = 0; f < S->call_stack.size(); ++f)
                                {
                                    const auto& frame = S->call_stack[f];
                                    gc_log("!!!   Frame {}: base={}, top={}", f, frame.base, frame.top);
                                }
                            }
                        }
                    }
                }

                destroy_object(S, obj, true);
            }

            S->gc.gc_work_current = next;
            ++work_done;
        }

        if (!S->gc.gc_work_current)
        {
            // Sweep complete - transition to finalize
            gc_switch_phase(S, GCPhase::kFinalize);
        }

        return work_done;
    }

    static size_t gc_finalize(State* S, size_t work_limit)
    {
        size_t work_done = 0;

        while (!S->gc.gc_finalize_queue.empty() && work_done < work_limit)
        {
            UserdataData* userdata = S->gc.gc_finalize_queue.back();
            S->gc.gc_finalize_queue.pop_back();

            // Call __gc metamethod
            if (userdata->metatable != nullptr)
            {
                Value gc_method = metatable_get_method<MetaMethodType::kGC>(Value(userdata));
                if (gc_method.is_callable())
                {
                    gc_log("Calling finalizer for userdata {:p}", static_cast<void*>(userdata));

                    metatable_call_method(S, gc_method, Value(userdata));
                }
            }

            // Mark userdata WHITE so it will be collected in the next GC cycle
            // (it was kept BLACK to survive this cycle's sweep)
            userdata->color = GCColor::kWhite;

            gc_log("Marked finalized userdata {:p} WHITE for next cycle", static_cast<void*>(userdata));
            ++work_done;
        }

        if (S->gc.gc_finalize_queue.empty())
        {
            // Finalize complete
            gc_switch_phase(S, GCPhase::kIdle);
            gc_adjust_threshold(S);
            gc_update_pool_limits(S);
        }

        return work_done;
    }

    static void gc_advance_cycles(State* S)
    {
        constexpr size_t kUnlimitedWork = std::numeric_limits<size_t>::max();

        while (S->gc.gc_phase == GCPhase::kMark)
        {
            if (gc_propagate_mark(S, kUnlimitedWork) == 0)
            {
                break;
            }
        }

        while (S->gc.gc_phase == GCPhase::kSweep)
        {
            if (gc_sweep(S, kUnlimitedWork) == 0)
            {
                break;
            }
        }

        while (S->gc.gc_phase == GCPhase::kFinalize)
        {
            if (gc_finalize(S, kUnlimitedWork) == 0)
            {
                break;
            }
        }

        if (S->gc.gc_phase == GCPhase::kIdle)
        {
            gc_adjust_threshold(S);
            gc_update_pool_limits(S);
        }
    }

    // ===== Main GC Entry Point =====

    void gc_step(State* S)
    {
        // Don't run if GC is paused
        if (S->gc.gc_paused)
        {
            return;
        }

        // Guard against re-entrant GC (e.g., during finalizers)
        if (S->gc.gc_running)
        {
            return;
        }

        const bool cycle_active = (S->gc.gc_phase != GCPhase::kIdle);
        if (!cycle_active && S->gc.gc_debt <= 0)
        {
            return;
        }

        S->gc.gc_running = true;

        if constexpr (kGCLoggingEnabled)
        {
            static size_t call_count = 0;
            static size_t cycles_completed = 0;

            call_count++;
            static size_t last_print = 0;
            if (call_count - last_print >= 10000)
            {
                gc_log("calls={}, cycles={}, debt={}, phase={}, total_bytes={:.2f}MB, threshold={:.2f}MB", call_count,
                    cycles_completed, S->gc.gc_debt, static_cast<int>(S->gc.gc_phase),
                    static_cast<double>(S->gc.gc_total_bytes) / (1024.0 * 1024.0),
                    static_cast<double>(S->gc.gc_threshold) / (1024.0 * 1024.0));
                last_print = call_count;
            }

            static size_t iter = 0;
            if (++iter % 1000 == 0)
            {
                size_t count = 0;
                size_t black_count = 0;
                size_t white_count = 0;
                size_t grey_count = 0;
                for (auto* obj = S->gc.gc_all_objects.head(); obj; obj = obj->next)
                {
                    count++;
                    switch (obj->color)
                    {
                        case GCColor::kBlack:
                            black_count++;
                            break;
                        case GCColor::kWhite:
                            white_count++;
                            break;
                        case GCColor::kGray:
                            grey_count++;
                            break;
                        default:
                            break;
                    }
                }

                gc_log("Status -- Total objects: {}, Black: {}, White: {}, Gray: {}, Debt: {}", count, black_count, white_count,
                    grey_count, S->gc.gc_debt);
            }
        }

        const size_t work_budget = 100;

        size_t work_done = 0;
        while (work_done < work_budget)
        {
            size_t batch_work = 0;

            switch (S->gc.gc_phase)
            {
                case GCPhase::kIdle:
                    gc_start_cycle(S);
                    batch_work = 10;
                    break;

                case GCPhase::kMark:
                    batch_work = gc_propagate_mark(S, work_budget - work_done);
                    if (batch_work == 0)
                    {
                        work_done = work_budget;
                    }
                    break;

                case GCPhase::kSweep:
                    batch_work = gc_sweep(S, work_budget - work_done);
                    if (batch_work == 0)
                    {
                        work_done = work_budget;
                    }
                    break;

                case GCPhase::kFinalize:
                    batch_work = gc_finalize(S, work_budget - work_done);
                    if (batch_work == 0)
                    {
                        work_done = work_budget;
                    }
                    break;
            }

            work_done += batch_work;

            int64_t work_in_bytes = static_cast<int64_t>(batch_work * kGCBytesPerWorkUnit);
            S->gc.gc_debt -= work_in_bytes;

            // If we completed a cycle, break
            if (S->gc.gc_phase == GCPhase::kIdle)
            {
                gc_log("Cycle complete");
                break;
            }
        }

        gc_log("gc_step complete: debt={}, phase={}, total_bytes={}", S->gc.gc_debt, static_cast<int>(S->gc.gc_phase),
            S->gc.gc_total_bytes);

        // Clear running flag
        S->gc.gc_running = false;
    }

    static void gc_destroy_pools(State* S)
    {
        while (!S->gc.gc_table_pool.empty())
        {
            GCObject* obj = S->gc.gc_table_pool.pop_front();
            destroy_object(S, obj, false);
        }

        while (!S->gc.gc_string_pool.empty())
        {
            GCObject* obj = S->gc.gc_string_pool.pop_front();
            destroy_object(S, obj, false);
        }

        while (!S->gc.gc_closure_pool.empty())
        {
            GCObject* obj = S->gc.gc_closure_pool.pop_front();
            destroy_object(S, obj, false);
        }
    }

    void gc_collect(State* S)
    {
        gc_log("===== FULL COLLECTION STARTED =====");

        // Completely reset GC state for a fresh cycle
        S->gc.gc_phase = GCPhase::kIdle;
        S->gc.gc_gray_list = nullptr; // Clear gray list
        S->gc.gc_finalize_queue.clear();
        S->gc.gc_work_current = nullptr;

        for (GCObject* obj = S->gc.gc_all_objects.head(); obj; obj = obj->next)
        {
            obj->color = GCColor::kBlack;
        }

        // Start a new cycle
        gc_start_cycle(S);
        gc_advance_cycles(S);
        gc_destroy_pools(S);

        gc_log("===== FULL COLLECTION COMPLETE: phase={} =====", static_cast<int>(S->gc.gc_phase));
    }

    void gc_close(State* S)
    {
        gc_log("===== GC_CLOSE: Final cleanup, destroying all remaining objects =====");

        size_t count = 0;
        while (GCObject* obj = S->gc.gc_all_objects.head())
        {
            destroy_object(S, obj, false);
            count++;
        }

        gc_destroy_pools(S);

        gc_log("===== GC_CLOSE: Destroyed {} objects =====", count);

        S->gc.gc_finalize_queue.destroy(S);
    }

} // namespace behl
