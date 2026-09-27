#include "api/api_internal.hpp"
#include "behl.hpp"
#include "gc/gc.hpp"
#include "gc/gco_buffer.hpp"
#include "state.hpp"
#include "vm/value.hpp"
#include "vm/vm_error.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

namespace behl
{
    template<typename T>
    using BufferBits = std::conditional_t<sizeof(T) == 1, uint8_t,
        std::conditional_t<sizeof(T) == 2, uint16_t, std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>>>;

    static GCBuffer* check_buffer_object(State* S, int32_t idx)
    {
        check_type(S, idx, Type::kBuffer);
        return S->stack[static_cast<size_t>(resolve_index(S, idx))].get_buffer();
    }

    static bool has_arg(State* S, int32_t idx)
    {
        return get_top(S) > idx && !is_nil(S, idx);
    }

    static SysInt checked_range(State* S, SysInt size, Integer offset, Integer count)
    {
        if (offset < 0 || count < 0 || static_cast<uint64_t>(offset) > size
            || static_cast<uint64_t>(count) > size - static_cast<SysInt>(offset))
        {
            raise_runtime_error(
                S, SourceLocation{}, "buffer access out of range (offset {}, count {}, length {})", offset, count, size);
        }
        return static_cast<SysInt>(offset);
    }

    static SysInt check_length(State* S, int32_t idx)
    {
        const Integer len = check_integer(S, idx);
        if (len < 0)
        {
            raise_runtime_error(S, SourceLocation{}, "buffer length must not be negative, got {}", len);
        }
        if (static_cast<uint64_t>(len) > std::numeric_limits<SysInt>::max())
        {
            raise_runtime_error(S, SourceLocation{}, "buffer length {} is too large", len);
        }
        return static_cast<SysInt>(len);
    }

    template<typename T>
    static int buffer_read(State* S)
    {
        const auto bytes = check_buffer(S, 0);
        const SysInt offset = checked_range(S, bytes.size(), check_integer(S, 1), static_cast<Integer>(sizeof(T)));

        uint64_t raw = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
        {
            raw |= static_cast<uint64_t>(std::to_integer<uint8_t>(bytes[offset + i])) << (8 * i);
        }

        const T value = std::bit_cast<T>(static_cast<BufferBits<T>>(raw));
        if constexpr (std::is_floating_point_v<T>)
        {
            push_number(S, static_cast<FP>(value));
        }
        else
        {
            push_integer(S, static_cast<Integer>(value));
        }
        return 1;
    }

    template<typename T>
    static int buffer_write(State* S)
    {
        const auto bytes = check_buffer(S, 0);
        const SysInt offset = checked_range(S, bytes.size(), check_integer(S, 1), static_cast<Integer>(sizeof(T)));

        uint64_t raw = 0;
        if constexpr (std::is_floating_point_v<T>)
        {
            raw = std::bit_cast<BufferBits<T>>(static_cast<T>(check_number(S, 2)));
        }
        else
        {
            raw = static_cast<uint64_t>(check_integer(S, 2));
        }

        for (size_t i = 0; i < sizeof(T); ++i)
        {
            bytes[offset + i] = static_cast<std::byte>(static_cast<uint8_t>(raw >> (8 * i)));
        }
        return 0;
    }

    static int buffer_create(State* S)
    {
        buffer_new(S, check_length(S, 0));
        return 1;
    }

    static int buffer_slice(State* S)
    {
        GCBuffer* source = check_buffer_object(S, 0);
        const Integer offset = check_integer(S, 1);
        const Integer len = check_integer(S, 2);
        const SysInt start = checked_range(S, source->size(), offset, len);

        GCBuffer* slice = gc_new_buffer_slice(S, source, start, static_cast<SysInt>(len));
        S->stack.push_back(S, Value(slice));
        return 1;
    }

    static int buffer_resize_fn(State* S)
    {
        GCBuffer* buffer = check_buffer_object(S, 0);
        const SysInt len = check_length(S, 1);
        if (!buffer->is_root())
        {
            raise_runtime_error(S, SourceLocation{}, "a buffer slice can not be resized");
        }

        gc_buffer_resize(S, buffer, len);
        return 0;
    }

    static int buffer_from_string(State* S)
    {
        const std::string_view str = check_string(S, 0);
        const auto bytes = buffer_new(S, static_cast<SysInt>(str.size()));
        if (!str.empty())
        {
            std::memcpy(bytes.data(), str.data(), str.size());
        }
        return 1;
    }

    static int buffer_to_string(State* S)
    {
        const auto bytes = check_buffer(S, 0);
        const Integer offset = has_arg(S, 1) ? check_integer(S, 1) : 0;
        const Integer len = has_arg(S, 2) ? check_integer(S, 2) : static_cast<Integer>(bytes.size()) - offset;
        const SysInt start = checked_range(S, bytes.size(), offset, len);

        push_string(S, std::string_view(reinterpret_cast<const char*>(bytes.data() + start), static_cast<size_t>(len)));
        return 1;
    }

    static int buffer_copy(State* S)
    {
        const auto target = check_buffer(S, 0);
        const Integer target_offset = check_integer(S, 1);
        const auto source = check_buffer(S, 2);
        const Integer source_offset = has_arg(S, 3) ? check_integer(S, 3) : 0;
        const Integer count = has_arg(S, 4) ? check_integer(S, 4) : static_cast<Integer>(source.size()) - source_offset;

        const SysInt from = checked_range(S, source.size(), source_offset, count);
        const SysInt to = checked_range(S, target.size(), target_offset, count);
        if (count > 0)
        {
            std::memmove(target.data() + to, source.data() + from, static_cast<size_t>(count));
        }
        return 0;
    }

    static int buffer_fill(State* S)
    {
        const auto bytes = check_buffer(S, 0);
        const Integer offset = check_integer(S, 1);
        const Integer value = check_integer(S, 2);
        const Integer count = has_arg(S, 3) ? check_integer(S, 3) : static_cast<Integer>(bytes.size()) - offset;

        const SysInt start = checked_range(S, bytes.size(), offset, count);
        if (count > 0)
        {
            std::memset(bytes.data() + start, static_cast<uint8_t>(value), static_cast<size_t>(count));
        }
        return 0;
    }

    void load_lib_buffer(State* S)
    {
        static constexpr ModuleReg buffer_funcs[] = {
            { "create", buffer_create },
            { "slice", buffer_slice },
            { "resize", buffer_resize_fn },
            { "from_string", buffer_from_string },
            { "to_string", buffer_to_string },
            { "copy", buffer_copy },
            { "fill", buffer_fill },
            { "read_u8", buffer_read<uint8_t> },
            { "read_i8", buffer_read<int8_t> },
            { "read_u16", buffer_read<uint16_t> },
            { "read_i16", buffer_read<int16_t> },
            { "read_u32", buffer_read<uint32_t> },
            { "read_i32", buffer_read<int32_t> },
            { "read_u64", buffer_read<uint64_t> },
            { "read_i64", buffer_read<int64_t> },
            { "read_f32", buffer_read<float> },
            { "read_f64", buffer_read<double> },
            { "write_u8", buffer_write<uint8_t> },
            { "write_i8", buffer_write<int8_t> },
            { "write_u16", buffer_write<uint16_t> },
            { "write_i16", buffer_write<int16_t> },
            { "write_u32", buffer_write<uint32_t> },
            { "write_i32", buffer_write<int32_t> },
            { "write_u64", buffer_write<uint64_t> },
            { "write_i64", buffer_write<int64_t> },
            { "write_f32", buffer_write<float> },
            { "write_f64", buffer_write<double> },
        };

        ModuleDef buffer_module = { .funcs = buffer_funcs };

        create_module(S, "buffer", buffer_module);
    }

} // namespace behl
