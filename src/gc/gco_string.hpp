#pragma once

#include "common/string.hpp"
#include "gc_object.hpp"
#include "platform/platform.hpp"

#include <array>
#include <behl/config.hpp>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace behl
{

    struct GCString : GCObject
    {
        static constexpr auto kObjectType = GCType::kString;

        GCOHeader header{};

        // SSO capacity: 31 bytes total (30 bytes string data + 1 null terminator)
        static constexpr size_t kSSOCapacity = 31;

        union Storage
        {
            struct
            {
                char* ptr;
                size_t len;
            } heap;

            struct
            {
                char buffer[kSSOCapacity]; // 31 bytes: inline string buffer + null terminator
                uint8_t len;               // 1 byte: length (0-30)
            } sso;

            static_assert(sizeof(Storage::sso) == 32, "GCString::Storage::sso must be 32 bytes");

        } storage{};

        static_assert(sizeof(Storage) == 32, "GCString::Storage must be 32 bytes");

        using Chunks = std::array<SysInt, 32 / sizeof(SysInt)>;

        static_assert(sizeof(Chunks) == sizeof(Storage), "GCString::Chunks must cover the whole storage");

        [[nodiscard]] Chunks chunks() const noexcept
        {
            return std::bit_cast<Chunks>(storage);
        }

        constexpr GCString() = default;

        constexpr GCString([[maybe_unused]] bool sso, std::string_view str) noexcept
            : header(kObjectType)
        {
            assert(sso && str.size() < kSSOCapacity);
            storage.sso = {};
            for (size_t i = 0; i < str.size(); ++i)
            {
                storage.sso.buffer[i] = str[i];
            }
            storage.sso.buffer[str.size()] = '\0';
            storage.sso.len = static_cast<uint8_t>(str.size());
        }

        [[nodiscard]] constexpr bool is_sso() const noexcept
        {
            return !header.has_flag(GCOFlags::kHeapString);
        }

        [[nodiscard]] constexpr size_t size() const noexcept
        {
            if (is_sso())
            {
                return storage.sso.len;
            }
            else
            {
                return storage.heap.len;
            }
        }

        size_t capacity() const noexcept
        {
            if (is_sso())
            {
                return kSSOCapacity;
            }
            else
            {
                return storage.heap.len;
            }
        }

        [[nodiscard]] constexpr const char* data() const noexcept
        {
            return is_sso() ? storage.sso.buffer : storage.heap.ptr;
        }

        [[nodiscard]] constexpr char* data() noexcept
        {
            return is_sso() ? storage.sso.buffer : storage.heap.ptr;
        }

        [[nodiscard]] std::string_view view() const noexcept
        {
            return std::string_view(data(), size());
        }

        void sso_reset() noexcept
        {
            storage.sso = {};
        }

        static std::strong_ordering sso_compare_impl(const GCString*, const GCString*, size_t, std::index_sequence<>) noexcept
        {
            return std::strong_ordering::equal;
        }

        template<size_t I, size_t... Rest>
        static std::strong_ordering sso_compare_impl(
            const GCString* a, const GCString* b, size_t min_len, std::index_sequence<I, Rest...>) noexcept
        {
            const auto ca = a->chunks();
            const auto cb = b->chunks();

            if (min_len > I * sizeof(size_t) && ca[I] != cb[I])
            {
                const char* pa = a->storage.sso.buffer;
                const char* pb = b->storage.sso.buffer;
                const auto xor_val = ca[I] ^ cb[I];
                const auto byte_idx = I * sizeof(size_t) + static_cast<size_t>(std::countr_zero(xor_val) / 8);
                const auto byte_a = static_cast<unsigned char>(pa[byte_idx]);
                const auto byte_b = static_cast<unsigned char>(pb[byte_idx]);
                return byte_a < byte_b ? std::strong_ordering::less : std::strong_ordering::greater;
            }

            return sso_compare_impl(a, b, min_len, std::index_sequence<Rest...>{});
        }

        static std::strong_ordering sso_compare(const GCString* a, const GCString* b) noexcept
        {
            const auto len_a = a->storage.sso.len;
            const auto len_b = b->storage.sso.len;
            const auto min_len = len_a < len_b ? len_a : len_b;

            auto result = sso_compare_impl(a, b, min_len, std::make_index_sequence<32 / sizeof(size_t)>{});
            if (result != std::strong_ordering::equal)
            {
                return result;
            }

            return len_a <=> len_b;
        }

        static std::strong_ordering compare(const GCString* a, const GCString* b) noexcept
        {
            if (a == b)
            {
                return std::strong_ordering::equal;
            }

            if (a->is_sso() && b->is_sso())
            {
                return sso_compare(a, b);
            }

            return a->view() <=> b->view();
        }

        static bool equals(const GCString* a, const GCString* b) noexcept
        {
            if (a == b)
            {
                return true;
            }

            if (a->is_sso() && b->is_sso())
            {
                return a->chunks() == b->chunks();
            }

            return a->view() == b->view();
        }
    };

    // Verify 32-byte layout (excluding GCObject)
    static_assert(sizeof(GCString) - sizeof(GCOHeader) == 32, "GCString should be 32 bytes excluding GCObject");
    static_assert(std::is_standard_layout_v<GCString>);
    static_assert(offsetof(GCString, header) == 0);

    struct GCStringHash
    {
        using is_transparent = void;

        auto operator()(const GCString* str) const noexcept
        {
            return str->header.object_hash;
        }

        auto operator()(const std::string_view str) const noexcept
        {
            return StringHash32{}(str);
        }
    };

    struct GCStringEq
    {
        using is_transparent = void;

        bool operator()(const GCString* a, const GCString* b) const noexcept
        {
            return GCString::equals(a, b);
        }

        bool operator()(const GCString* a, std::string_view sv) const noexcept
        {
            return a->view() == sv;
        }
    };

} // namespace behl
