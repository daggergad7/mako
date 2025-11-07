#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

// Masstree stores opaque payloads in leaf slots.  This lightweight wrapper keeps
// the underlying bits while providing helper accessors for legacy pointer values.
struct MasstreeValueHandle {
    std::uintptr_t bits;

    template <typename T>
    static constexpr MasstreeValueHandle from_ptr(T* ptr) noexcept {
        return MasstreeValueHandle{reinterpret_cast<std::uintptr_t>(ptr)};
    }

    template <typename T>
    static constexpr MasstreeValueHandle from_ptr(const T* ptr) noexcept {
        return MasstreeValueHandle{reinterpret_cast<std::uintptr_t>(ptr)};
    }

    static constexpr MasstreeValueHandle null() noexcept {
        return MasstreeValueHandle{std::uintptr_t(0)};
    }

    [[nodiscard]] constexpr bool is_null() const noexcept {
        return bits == 0U;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return !is_null();
    }

    constexpr void reset(std::uintptr_t new_bits = 0U) noexcept {
        bits = new_bits;
    }

    constexpr void reset_ptr(const void* p) noexcept {
        bits = reinterpret_cast<std::uintptr_t>(p);
    }

    [[nodiscard]] inline uint8_t* get() const noexcept {
        return reinterpret_cast<uint8_t*>(bits);
    }

    template <typename T>
    [[nodiscard]] inline T* as() const noexcept {
        return reinterpret_cast<T*>(bits);
    }

    template <typename T>
    [[nodiscard]] inline const T* as_const() const noexcept {
        return reinterpret_cast<const T*>(bits);
    }
};

// Helper constructors -------------------------------------------------------

inline MasstreeValueHandle make_value_handle(std::nullptr_t) {
    return MasstreeValueHandle::null();
}

template <typename T>
inline MasstreeValueHandle make_value_handle(T* ptr) {
    return MasstreeValueHandle::from_ptr(ptr);
}

template <typename T>
inline MasstreeValueHandle make_value_handle(const T* ptr) {
    return MasstreeValueHandle::from_ptr(ptr);
}

// Comparison helpers --------------------------------------------------------

#define MASSTREE_HANDLE_EQ_OP(OP)                                    \
    [[nodiscard]] constexpr inline bool operator OP(                 \
            const MasstreeValueHandle& lhs,                          \
            const MasstreeValueHandle& rhs) noexcept {               \
        return lhs.bits OP rhs.bits;                                 \
    }                                                                \
    [[nodiscard]] constexpr inline bool operator OP(                 \
            const MasstreeValueHandle& lhs,                          \
            std::nullptr_t) noexcept {                               \
        return lhs.bits OP std::uintptr_t(0);                        \
    }                                                                \
    [[nodiscard]] constexpr inline bool operator OP(                 \
            std::nullptr_t,                                          \
            const MasstreeValueHandle& rhs) noexcept {               \
        return std::uintptr_t(0) OP rhs.bits;                        \
    }

MASSTREE_HANDLE_EQ_OP(==)
MASSTREE_HANDLE_EQ_OP(!=)

#undef MASSTREE_HANDLE_EQ_OP
