#pragma once

#include "common/string.hpp"
#include "gc_object.hpp"

#include <array>
#include <behl/config.hpp>
#include <bit>
#include <cstdint>

namespace behl
{

    struct GCString : GCObject
    {
        static constexpr auto kObjectType = GCType::kString;

        // SSO capacity: 31 bytes total (30 bytes string data + 1 null terminator)
        static constexpr size_t kSSOCapacity = 31;
        static constexpr uint8_t kHeapFlag = 0x80;

        union Storage
        {
            struct
            {
                char* ptr;
                size_t len;
                char padding[32 - (sizeof(ptr) + sizeof(len) + sizeof(uint8_t))];
                uint8_t flag;
            } heap;

            static_assert(sizeof(Storage::heap) == 32, "GCString::Storage::heap must be 32 bytes");

            struct
            {
                char buffer[kSSOCapacity]; // 31 bytes: inline string buffer + null terminator
                uint8_t len;               // 1 byte: length (0-30), high bit clear
            } sso;

            static_assert(sizeof(Storage::sso) == 32, "GCString::Storage::sso must be 32 bytes");

            std::array<SysInt, 32 / sizeof(SysInt)> chunks;

            static_assert(sizeof(Storage::chunks) == 32, "GCString::Storage::chunks must be 32 bytes");

        } storage{};

        static_assert(sizeof(Storage) == 32, "GCString::Storage must be 32 bytes");

        [[nodiscard]] constexpr bool is_sso() const noexcept
        {
            // Check the last byte - if high bit is clear, it's SSO
            return (storage.sso.len & kHeapFlag) == 0;
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
            [&]<size_t... Is>(std::index_sequence<Is...>) {
                //
                ((storage.chunks[Is] = 0), ...);
            }(std::make_index_sequence<32 / sizeof(size_t)>{});
        }

        static std::strong_ordering sso_compare_impl(const GCString*, const GCString*, size_t, std::index_sequence<>) noexcept
        {
            return std::strong_ordering::equal;
        }

        template<size_t I, size_t... Rest>
        static std::strong_ordering sso_compare_impl(
            const GCString* a, const GCString* b, size_t min_len, std::index_sequence<I, Rest...>) noexcept
        {
            const auto& ca = a->storage.chunks;
            const auto& cb = b->storage.chunks;

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

            return [&]<size_t... Is>(std::index_sequence<Is...>) {
                const auto& ac = a->storage.chunks;
                const auto& bc = b->storage.chunks;
                return ((ac[Is] == bc[Is]) && ...);
            }(std::make_index_sequence<32 / sizeof(size_t)>{});
        }
    };

    // Verify 32-byte layout (excluding GCObject)
    static_assert(sizeof(GCString) - sizeof(GCObject) == 32, "GCString should be 32 bytes excluding GCObject");

    struct GCStringHash
    {
        using is_transparent = void;

        StringHash _hasher;

        size_t operator()(const GCString* str) const noexcept
        {
            return _hasher(str->view());
        }

        size_t operator()(const std::string_view str) const noexcept
        {
            return _hasher(str);
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
