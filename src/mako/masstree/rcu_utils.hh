#pragma once

#include <cstddef>

#ifndef NDEBUG
#include <mutex>
#include <unordered_set>
#endif

#include "../macros.h"
#include "../rcu.h"

namespace masstree::detail {

#ifndef NDEBUG
inline std::mutex& handle_mutex() {
    static std::mutex mutex;
    return mutex;
}

inline std::unordered_set<const void*>& handle_allocations() {
    static std::unordered_set<const void*> allocations;
    return allocations;
}

inline void track_alloc(const void* ptr) {
    if (!ptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(handle_mutex());
    handle_allocations().insert(ptr);
}

inline void track_release(const void* ptr) {
    if (!ptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(handle_mutex());
    handle_allocations().erase(ptr);
}

inline bool is_tracked(const void* ptr) {
    if (!ptr) {
        return true;
    }
    std::lock_guard<std::mutex> lock(handle_mutex());
    return handle_allocations().find(ptr) != handle_allocations().end();
}
#else
inline void track_alloc(const void*) {}
inline void track_release(const void*) {}
inline bool is_tracked(const void*) {
    return true;
}
#endif

inline void assert_tracked(const void* ptr) {
#ifndef NDEBUG
    if (ptr && !is_tracked(ptr)) {
        INVARIANT(false && "Masstree payload pointer was not allocated via masstree::rcu_allocate");
    }
#else
    (void)ptr;
#endif
}

} // namespace masstree::detail

namespace masstree {

// @unsafe
// SAFETY: Raw allocation bypasses borrow checking; callers must obey Masstree/RCU epoch rules.
inline void* rcu_allocate(size_t bytes) {
    void* ptr = rcu::s_instance.alloc(bytes);
    detail::track_alloc(ptr);
    return ptr;
}

// @unsafe
// SAFETY: Static allocations follow the same epoch requirements as rcu_allocate.
inline void* rcu_allocate_static(size_t bytes) {
    void* ptr = rcu::s_instance.alloc_static(bytes);
    detail::track_alloc(ptr);
    return ptr;
}

// @unsafe
// SAFETY: Callers must ensure the pointer is no longer reachable before reclamation.
inline void rcu_deallocate(void* ptr, size_t bytes) {
    detail::track_release(ptr);
    rcu::s_instance.dealloc(ptr, bytes);
}

// @unsafe
// SAFETY: Defers reclamation until a grace period elapses; the caller guarantees quiescent state.
inline void rcu_deallocate_rcu(void* ptr, size_t bytes) {
    detail::track_release(ptr);
    rcu::s_instance.dealloc_rcu(ptr, bytes);
}

// @unsafe
// SAFETY: Uses caller-provided deleter; the deleter must be RCU-safe. Covered by Masstree GC tests.
inline void rcu_free_with(void* ptr, rcu::deleter_t fn) {
    detail::track_release(ptr);
    rcu::s_instance.free_with_fn(ptr, fn);
}

inline void debug_assert_tracked(const void* ptr) {
    detail::assert_tracked(ptr);
}

} // namespace masstree
