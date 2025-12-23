#pragma once

#include "common/vector.hpp"
#include "value.hpp"

#include <cstdint>

namespace behl
{
    struct Upvalue
    {
        Value closed_value;
        int32_t index;

        Upvalue() = delete;
        explicit constexpr Upvalue(size_t stack_idx)
            : closed_value()
            , index(static_cast<int32_t>(stack_idx))
        {
        }

        constexpr bool is_open() const
        {
            return index >= 0;
        }
    };

    using UpvalueVector = Vector<Upvalue>;
    using UpvalueIndexVector = Vector<uint32_t>;

} // namespace behl
