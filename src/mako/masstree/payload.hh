#pragma once

#include <utility>

#include "value_handle.hh"
#include "rcu_utils.hh"

// MasstreePayload provides scoped ownership for values stored in Masstree.
// It calls the supplied deleter when the payload is dropped unless ownership
// has been explicitly released.  This allows call sites to create values,
// hand them to Masstree, and rely on automatic cleanup when inserts fail.
template <typename T>
class MasstreePayload {
public:
    using Pointer = T*;
    using Deleter = void(*)(Pointer);

    constexpr MasstreePayload() noexcept : ptr_(nullptr), deleter_(nullptr) {}

    constexpr MasstreePayload(std::nullptr_t) noexcept : MasstreePayload() {}

    constexpr MasstreePayload(Pointer ptr, Deleter deleter) noexcept
        : ptr_(ptr), deleter_(deleter) {}

    MasstreePayload(const MasstreePayload&) = delete;
    MasstreePayload& operator=(const MasstreePayload&) = delete;

    MasstreePayload(MasstreePayload&& other) noexcept
        : ptr_(other.ptr_), deleter_(other.deleter_) {
        other.ptr_ = nullptr;
        other.deleter_ = nullptr;
    }

    MasstreePayload& operator=(MasstreePayload&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            deleter_ = other.deleter_;
            other.ptr_ = nullptr;
            other.deleter_ = nullptr;
        }
        return *this;
    }

    ~MasstreePayload() {
        reset();
    }

    void reset(Pointer ptr = nullptr, Deleter deleter = nullptr) noexcept {
        if (ptr_ && deleter_) {
            deleter_(ptr_);
        }
        ptr_ = ptr;
        deleter_ = deleter;
    }

    [[nodiscard]] Pointer get() const noexcept {
        return ptr_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    [[nodiscard]] Deleter deleter() const noexcept {
        return deleter_;
    }

    void set_deleter(Deleter deleter) noexcept {
        deleter_ = deleter;
    }

    Pointer release_raw() noexcept {
        Pointer out = ptr_;
        ptr_ = nullptr;
        deleter_ = nullptr;
        return out;
    }

    [[nodiscard]] MasstreeValueHandle handle() const noexcept {
        if (ptr_) {
            masstree::debug_assert_tracked(ptr_);
        }
        return ptr_ ? MasstreeValueHandle::from_ptr(ptr_) : MasstreeValueHandle::null();
    }

    MasstreeValueHandle release_handle() noexcept {
        MasstreeValueHandle h = handle();
        ptr_ = nullptr;
        deleter_ = nullptr;
        return h;
    }

private:
    Pointer ptr_;
    Deleter deleter_;
};

template <typename T>
MasstreePayload<T> make_masstree_payload(
        T* ptr,
        typename MasstreePayload<T>::Deleter deleter) noexcept {
    return MasstreePayload<T>(ptr, deleter);
}
