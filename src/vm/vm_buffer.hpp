#pragma once

#include "bytecode.hpp"
#include "common/arithmetic.hpp"
#include "frame.hpp"
#include "gc/gco_buffer.hpp"
#include "platform/platform.hpp"
#include "state.hpp"
#include "value.hpp"
#include "vm_detail.hpp"
#include "vm_error.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace behl
{
    BEHL_INLINE
    bool buffer_integer_operand(const Value& value, Integer& out)
    {
        if (value.is_integer())
        {
            out = value.get_integer();
            return true;
        }

        const FP number = value.get_fp();
        return std::floor(number) == number && arithmetic::try_from_fp(number, out);
    }

    BEHL_INLINE
    std::byte& buffer_element(State* S, const CallFrame& frame, GCBuffer* buffer, const Value& key)
    {
        if (!key.is_numeric())
        {
            raise_type_error(
                S, get_current_location(frame), "attempt to index a buffer with a '{}' value", key.get_type_string());
        }

        Integer index = 0;
        if (!buffer_integer_operand(key, index))
        {
            raise_type_error(S, get_current_location(frame), "number has no integer representation");
        }

        const SysInt size = buffer->size();
        if (index < 0 || static_cast<uint64_t>(index) >= size)
        {
            raise_runtime_error(S, get_current_location(frame), "buffer index {} out of range (length {})", index, size);
        }

        return buffer->owner->data[buffer->offset + static_cast<SysInt>(index)];
    }

    BEHL_INLINE
    void vm_buffer_get(State* S, CallFrame& frame, Reg a, GCBuffer* buffer, const Value& key)
    {
        const std::byte byte = buffer_element(S, frame, buffer, key);
        get_register(S, frame, a).emplace<Integer>(static_cast<Integer>(std::to_integer<uint8_t>(byte)));
    }

    BEHL_INLINE
    void vm_buffer_set(State* S, CallFrame& frame, GCBuffer* buffer, const Value& key, const Value& val)
    {
        if (!val.is_numeric())
        {
            raise_type_error(
                S, get_current_location(frame), "attempt to store a '{}' value in a buffer", val.get_type_string());
        }

        Integer byte_value = 0;
        if (!buffer_integer_operand(val, byte_value))
        {
            raise_type_error(S, get_current_location(frame), "number has no integer representation");
        }

        buffer_element(S, frame, buffer, key) = static_cast<std::byte>(static_cast<uint8_t>(byte_value));
    }

} // namespace behl
