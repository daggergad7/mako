# Masstree RustyCpp Memory Safety Migration Plan

## Overview

This document outlines the plan to make the Masstree library memory-safe using rusty-cpp borrow checking. The goal is to mark all functions as safe and only use `unsafe` when absolutely necessary.

## Phase 1: Assessment and Preparation (Week 1)

### 1.1 Inventory Current State

- [x] Count total files in `src/mako/benchmarks/sto/masstree-beta/`
- [x] Identify all pointer usage patterns
- [x] Document existing memory management patterns
- [x] List all classes with manual memory management
- [x] Identify shared ownership patterns

### 1.2 Set Up RustyCpp Infrastructure

- [x] Enable borrow checking for Masstree in CMakeLists.txt
- [x] Create test harness for incremental checking
- [x] Set up CI/CD integration for borrow checking
- [x] Document RustyCpp annotations needed

### 1.3 Establish Safety Guidelines

- [x] Document when to use `unsafe`
- [x] Create patterns for safe alternatives
- [x] Define ownership transfer conventions
- [x] Create code review checklist

## Phase 2: Core Infrastructure (Week 2-3)

### 2.1 Masstree Core (`src/mako/benchmarks/sto/masstree-beta/`)

Priority: **Critical** - Everything depends on these

#### Files to migrate:

- `masstree.hh`
- `masstree_struct.hh`
- `masstree_key.hh`
- `masstree_tcursor.hh`
- `masstree_split.hh`
- `masstree_scan.hh`
- `masstree_remove.hh`
- `masstree_print.hh`
- `masstree_insert.hh`
- `masstree_get.hh`
- `query_masstree.hh`
- `query_masstree.cc`

#### Key challenges:

- Complex node structures with raw pointers
- Manual memory management for nodes
- Concurrent modifications and thread safety
- Custom memory allocators

#### Proposed solutions:

- (To be filled in after deeper code analysis)

## Phase 3: Build System Integration (Week 1-2)

### 3.1 CMake Integration

- [x] Modify `CMakeLists.txt` to allow for selectively enabling/disabling `rusty-cpp` for the `masstree` component.
- [x] Create a new CMake option, `MAKO_ENABLE_RUSTY_CPP_MASSTREE`, to control whether `rusty-cpp` is run on the `masstree` code.
- [x] Update the `masstree_borrow_check` custom target to be conditional on the `MAKO_ENABLE_RUSTY_CPP_MASSTREE` option.
- [x] Investigate the possibility of creating a separate static library for the `masstree` component to better isolate it from the rest of the `mako` codebase.

### 3.2 Submodule Management

- [x] Document the process for updating the `masstree` submodule and running the `rusty-cpp` checker on the updated code.
- [x] Establish a branching strategy for the `masstree` submodule to manage the migration process.

## Phase 4: Integration and Testing (Week 4-5)

### 4.1 Integration Points

- [x] Update generated code templates
- [x] Fix service registration patterns
- [x] Update benchmark code
- [x] Migrate example code

### 4.2 Testing Strategy

- [x] Unit tests for each component
- [x] Integration tests for RPC calls
- [x] Stress tests for memory safety
- [x] Performance regression tests

## Phase 5: Advanced Features (Week 5-6)

### 5.1 Optional Optimizations

- [ ] Lock-free queues with safe interfaces
- [ ] Memory pools with borrow checking
- [ ] Zero-copy optimizations

### 5.2 Documentation

- [ ] Update Masstree guide
- [ ] Create migration guide
- [ ] Document unsafe blocks
- [ ] Performance impact analysis

## Implementation Strategy

### Order of Attack

1. **Start with Build System**: First, implement the CMake changes to control the `rusty-cpp` checking for `masstree`.
2. **Incremental Migration**: One file at a time, starting with the core data structures and moving outwards.
3. **Focus on Headers First**: Since `rusty-cpp` primarily works on header files, prioritize migrating the `.hh` files.
4. **Maintain Compatibility**: Keep the API stable to avoid breaking the rest of the `mako` codebase.
5. **Test Continuously**: Run the `test_masstree` and other relevant tests after each file migration.

### Common Patterns to Apply

#### 1. Raw Pointer → Smart Pointer

```cpp
// Before
class Connection {
    Request* pending_request_;

    ~Connection() {
        delete pending_request_;
    }
};

// After
class Connection {
    std::unique_ptr<Request> pending_request_;
    // Destructor not needed
};
```

#### 2. Manual Ref Counting → shared_ptr

```cpp
// Before
class RefCounted {
    int ref_count_;
    void add_ref();
    void release();
};

// After
using ObjectPtr = std::shared_ptr<Object>;
```

#### 3. C Arrays → std::vector/std::array

```cpp
// Before
char buffer[1024];
int* values = new int[size];

// After
std::array<char, 1024> buffer;
std::vector<int> values(size);
```

#### 4. Unsafe Casts → Safe Alternatives

```cpp
// Before
int* p = (int*)buffer;

// After
int value;
std::memcpy(&value, buffer, sizeof(int));
```

### When to Use `unsafe`

Acceptable uses of `unsafe`:

1. **FFI Boundaries**: Interfacing with C libraries
2. **Performance Critical**: Proven bottlenecks only
3. **Lock-free Algorithms**: Where atomics are needed
4. **Platform Code**: System calls, epoll, etc.

Each `unsafe` block must have:

- Comment explaining why it's needed
- Proof of safety
- Test coverage

## Milestone Checkpoints

#### Checkpoint 1 (End of Week 1)

- [ ] All base types compile with borrow checking
- [ ] No regressions in existing tests
- [ ] Documentation updated

#### Checkpoint 2 (End of Week 3)

- [ ] Marshal and reactor systems safe
- [ ] RPC client can make safe calls
- [ ] Benchmark still runs

#### Checkpoint 3 (End of Week 5)

- [ ] Entire Masstree library passes borrow checking
- [ ] All tests pass
- [ ] Performance within 5% of original

## Risk Mitigation

### Potential Risks

1. **Performance Regression**: Mitigation: Profile continuously
2. **API Breaking Changes**: Mitigation: Maintain compatibility layer
3. **Complex Lifetime Issues**: Mitigation: Redesign if needed
4. **Hidden Bugs Exposed**: Mitigation: Fix as we go

### Rollback Plan

- Keep original code in separate branch
- Feature flag for safe/unsafe mode
- Incremental deployment

## Success Metrics

### Primary Goals

- [ ] 100% of Masstree files pass borrow checking
- [ ] < 5% performance impact
- [ ] No API breaking changes
- [ ] Zero memory leaks in tests

### Stretch Goals

- [ ] Remove all `unsafe` blocks
- [ ] Improve performance with better patterns
- [ ] Create reusable safety patterns

---
