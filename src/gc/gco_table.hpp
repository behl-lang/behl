#pragma once

#include "common/hash_map.hpp"
#include "common/vector.hpp"
#include "gc_object.hpp"
#include "vm/value.hpp"

namespace behl
{

    struct GCTable : GCObject
    {
        static constexpr auto kObjectType = GCType::kTable;

        Vector<Value> array;
        HashMap<Value, Value, ValueHash, ValueEq> hash;
        GCTable* metatable{};

        void assign_name(std::string_view name)
        {
            size_t copy_len = std::min(name.size(), sizeof(internal_name));
            std::memcpy(internal_name, name.data(), copy_len);
            internal_name_len = static_cast<uint8_t>(copy_len);
        }

        std::string_view get_name() const
        {
            return std::string_view(internal_name, internal_name_len);
        }

        bool has_name() const
        {
            return internal_name_len > 0;
        }

        void clear_name()
        {
            internal_name_len = 0;
        }

    private:
        char internal_name[63];
        uint8_t internal_name_len{};
    };

} // namespace behl
