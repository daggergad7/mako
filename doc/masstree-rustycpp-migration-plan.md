# Masstree RustyCpp Memory-Safety Migration Plan

## Overview
Masstree is the core storage engine in Mako. This plan tracks our migration to RustyCpp so that Masstree’s interface is memory-safe, with `@unsafe` blocks kept explicit and justified.

---

## Phase 1: Assessment & Preparation

### 1.1 Inventory Current State
- [x] Check every Masstree entry point marked `// @unsafe`, document the invariants, and note callers (`src/mako/masstree_btree.h`, etc.).
- [x] Check RCU allocation/free flows in `rcu::sync` and list Masstree sites relying on them (`src/mako/rcu.h:141`–196, `src/mako/masstree_btree.h:132`–173).
- [x] Enumerate `MasstreeValueHandle` producers/consumers across Mako to validate ownership expectations.

### 1.2 RustyCpp Infrastructure
- [x] Ensure RustyCpp checker builds automatically (custom target now exports the required GCC include paths).
- [x] Add an optional CMake target (`RUN_MASSTREE_BORROW_CHECK`) to invoke the checker; default build no longer fails when the checker reports issues.
- [x] Add CI guard rails so missing checker binaries fail loudly.
- [ ] Generate per-target `@unsafe` warnings for Masstree so reviews catch unsafe expansions.


---

## Phase 2: Value Ownership & GC

### 2.1 Handle Consolidation
- [x] Provide a trivially copyable `MasstreeValueHandle` with typed accessors.
- [x] Remove raw `value_ptr()` casts from transaction write paths in favour of handle accessors.
- [x] Deprecate integer-only `make_value_handle` helpers and update tests accordingly.
- [x] Introduce `MasstreePayload` RAII wrapper for inserts/removes and migrate kvdb wrappers.
- [x] Ensure logical deletes rely on `has_value()` rather than raw pointers (`src/mako/txn_impl.h`).

### 2.2 RCU & Memory Reclamation
- [x] Wrap `rcu::sync::alloc/dealloc/dealloc_rcu` in `@unsafe` shims with documented contracts (`src/mako/masstree/rcu_utils.hh`).
- [x] Add debug-only provenance checks to track handle allocations.
- [x] Extend `test/test_masstree.cc` to stress GC paths (tracked payloads released on failed inserts).

### 2.3 Node Mutation APIs
- [x] Expose safe façade helpers (`insert_with_result`, `insert_if_absent_with_result`, `remove_with_result`) in `masstree_btree.h`.
- [x] Audit call sites (kvdb, transaction code) to ensure `old_v` handles are checked before use.
- [x] Harden `insert_info_t` so raw node pointers stay thread-local.

---

## Phase 3: Traversal & Scan Safety
- [x] Replace raw callback parameters in `low_level_search_range_callback` with lifetimed view objects.
- [x] Extend `test/test_masstree.cc` with concurrent scan coverage.
- [x] Wrap `tree_walk` and debug helpers so they no longer expose raw node pointers.

---

## Phase 4: Transaction & Logging Integration
- [x] Update `dbtuple::tuple_writer_t` to accept `MasstreeValueHandle` or typed payloads directly.
- [ ] Refactor log-delta writers to remove remaining `const void*` plumbing.
- [ ] Record null-handle semantics as the canonical deletion marker.
- [ ] Annotate tuple-writer/logging hotspots with appropriate `@unsafe` markers until the interfaces are fully migrated.
- [ ] Audit transaction helpers marked `@safe` so they avoid undeclared legacy code and follow RustyCpp borrow rules.

---

## Phase 5: Integration & Tooling
- [x] Migrate legacy `src/mako/btree.cc` helpers to the safe façade (now routed through `insert_with_result` wrappers).
- [x] Update `kvdb_wrapper_impl.h` to use RAII deleters and document ownership, with GC tests backing the change.
- [ ] Capture throughput/latency baselines pre/post migration to watch for regressions.


---

## Phase 6: Documentation & Knowledge Transfer
- [ ] Publish a Masstree safety guide covering handles, RCU, and `@unsafe` conventions.
- [ ] Record justifications for each remaining `@unsafe` block both inline and in docs.
- [ ] Share weekly status updates with silo/distributed owners to flag interface changes early.
- [ ] Provide a quick-reference matrix (based on RustyCpp README) mapping Masstree modules to required annotations and safe-type substitutions.

---

## Implementation Strategy
1. Harden ownership and GC semantics (Phase 2) before touching traversal and logging.
2. Introduce safe façade layers so downstream code no longer sees raw pointers.
3. Incrementally convert logging/tuple writers to handle-based APIs.
4. Keep borrow checking enabled at every stage; extend test suites after each milestone.



## Open Risks
1. **Traversal refactor** – Lifetimed callbacks are still unimplemented; risk of dangling references remains.
2. **Tuple/logging pipeline** – Remaining `const void*` paths could undermine safety guarantees.
3. **Documentation debt** – No published guide or review checklist yet; onboarding reviewers will be harder until addressed.
4. **STL usage in @safe code** – Without `@external` annotations, the checker cannot enforce borrowing rules on standard containers.

---

## Next Actions
1. Draft Masstree-specific `@unsafe` guidelines and reviewer checklist.
2. Remove the remaining `const void*` call sites in tuple writers/logging (Phase 4) and document null-handle semantics.
3. Annotate tuple/logging hotspots with the right `@unsafe` markers once the handle-only path lands.
4. Wrap up documentation deliverables and per-target checker enforcement (Phase 6 / Phase 5).

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
