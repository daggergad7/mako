#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

// Masstree stores opaque payloads in leaf slots.  This lightweight wrapper keeps
// the underlying bits while providing helper accessors for legacy pointer values.
struct MasstreeValueHandle {
    std::uintptr_t bits;

    // No user-defined constructors/destructors to keep the type trivially copyable.

    inline bool is_null() const {
        return bits == 0U;
    }

    inline explicit operator bool() const {
        return !is_null();
    }

    inline void reset(std::uintptr_t new_bits = 0U) {
        bits = new_bits;
    }

    inline void reset_ptr(uint8_t* p) {
        bits = reinterpret_cast<std::uintptr_t>(p);
    }

    inline uint8_t* get() const {
        return reinterpret_cast<uint8_t*>(bits);
    }

    template <typename T>
    inline T* as() const {
        return reinterpret_cast<T*>(bits);
    }

    template <typename T>
    inline const T* as_const() const {
        return reinterpret_cast<const T*>(bits);
    }
};

// Helper constructors -------------------------------------------------------

inline MasstreeValueHandle make_value_handle(std::uintptr_t bits) {
    MasstreeValueHandle handle{bits};
    return handle;
}

inline MasstreeValueHandle make_value_handle(uint8_t* ptr) {
    return make_value_handle(reinterpret_cast<std::uintptr_t>(ptr));
}

inline MasstreeValueHandle make_value_handle(std::nullptr_t) {
    return make_value_handle(static_cast<std::uintptr_t>(0));
}

template <typename T>
inline MasstreeValueHandle make_value_handle(T* ptr) {
    return make_value_handle(reinterpret_cast<std::uintptr_t>(ptr));
}

// Comparison helpers --------------------------------------------------------

#define MASSTREE_HANDLE_EQ_OP(OP)                                    \
    inline bool operator OP(const MasstreeValueHandle& lhs,          \
                            const MasstreeValueHandle& rhs) {        \
        return lhs.bits OP rhs.bits;                                 \
    }                                                                \
    inline bool operator OP(const MasstreeValueHandle& lhs,          \
                            std::nullptr_t) {                        \
        return lhs.bits OP std::uintptr_t(0);                        \
    }                                                                \
    inline bool operator OP(std::nullptr_t,                          \
                            const MasstreeValueHandle& rhs) {        \
        return std::uintptr_t(0) OP rhs.bits;                        \
    }

MASSTREE_HANDLE_EQ_OP(==)
MASSTREE_HANDLE_EQ_OP(!=)

#undef MASSTREE_HANDLE_EQ_OP

