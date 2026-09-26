#include "api/api_internal.hpp"
#include "behl.hpp"
#include "gc/gc.hpp"
#include "gc/gco_buffer.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm_error.hpp"

#include <cassert>

namespace behl
{
    static GCBuffer* buffer_at(State* S, int32_t idx)
    {
        const ptrdiff_t r_idx = resolve_index(S, idx);
        if (r_idx < 0 || r_idx >= static_cast<ptrdiff_t>(S->stack.size()))
        {
            return nullptr;
        }

        const Value& v = S->stack[static_cast<size_t>(r_idx)];
        return v.is_buffer() ? v.get_buffer() : nullptr;
    }

    std::span<std::byte> buffer_new(State* S, SysInt len)
    {
        assert(S != nullptr && "State can not be null");

        GCBuffer* buffer = gc_new_buffer(S, len);
        S->stack.push_back(S, Value(buffer));

        return buffer->bytes();
    }

    std::span<std::byte> buffer_get(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        GCBuffer* buffer = buffer_at(S, idx);
        if (buffer == nullptr)
        {
            return {};
        }

        return buffer->bytes();
    }

    SysInt buffer_len(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        GCBuffer* buffer = buffer_at(S, idx);
        if (buffer == nullptr)
        {
            return 0;
        }

        return buffer->size();
    }

    std::span<std::byte> buffer_resize(State* S, int32_t idx, SysInt new_len)
    {
        assert(S != nullptr && "State can not be null");

        GCBuffer* buffer = buffer_at(S, idx);
        assert(buffer != nullptr && "buffer_resize: value is not a buffer");
        assert((buffer == nullptr || buffer->is_root()) && "buffer_resize: a slice can not be resized");
        if (buffer == nullptr || !buffer->is_root())
        {
            return {};
        }

        gc_buffer_resize(S, buffer, new_len);

        return buffer->bytes();
    }

    std::span<std::byte> check_buffer(State* S, int32_t idx)
    {
        assert(S != nullptr && "State can not be null");

        check_type(S, idx, Type::kBuffer);

        return buffer_at(S, idx)->bytes();
    }

} // namespace behl
