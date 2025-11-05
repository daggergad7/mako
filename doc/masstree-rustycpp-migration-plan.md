# Masstree RustyCpp Memory-Safety Migration Plan

## Overview
Masstree backs Mako’s main storage layer. This track delivers a RustyCpp-checked surface so student projects can rely on safe APIs while the silo/sto and distributed teams handle their own migration work. We retain Masstree performance characteristics and public APIs wherever possible.

## Completed Progress Snapshot
- [x] Replaced raw value pointers with `MasstreeValueHandle` end-to-end in transaction paths (`src/mako/base_txn_btree.h:364`, `src/mako/txn_impl.h:540`, `src/mako/txn_proto2_impl.h:533` via commits `236317f5`, `bb270c9d`).
- [x] Added dedicated Masstree regression tests under GTest, covering insert/search/remove/scan workflows (`test/test_masstree.cc`, commit `3da45af4`).
- [x] Integrated RustyCpp checker into the build and exposed headers to the borrow checker (`CMakeLists.txt:24`–35, commit `5294f62f`).
- [x] Introduced `rusty::Cell` for thread timestamp management in `simple_threadinfo`, reducing mutable aliasing (`src/mako/masstree_btree.h:49`).

---

## Phase 1: Assessment & Preparation 

### 1.1 Inventory Current State
- [ ] Catalogue every Masstree entry point marked `// @unsafe`, document the invariants, and note callers (`src/mako/masstree_btree.h:288`, `:324`, `:426`, `:438`, `:451`).
- [ ] Map RCU allocation/free flows in `rcu::sync` and list Masstree sites relying on them (`src/mako/rcu.h:141`–196, `src/mako/masstree_btree.h:132`–173).
- [ ] Enumerate `MasstreeValueHandle` producers/consumers across Mako (e.g., `src/mako/btree.cc:33`, `src/mako/benchmarks/kvdb_wrapper_impl.h:339`, `src/mako/txn.h:243`) to validate ownership expectations.

### 1.2 RustyCpp Infrastructure
- [x] Ensure rusty-cpp checker builds before compilation (`CMakeLists.txt:24`–35).
- [x] Add a Masstree-only CMake target running `rusty-cpp-checker` over `src/mako/masstree/*.hh` in CI.
- [x] Fail CI loudly when the checker binary is missing or outdated.
- [ ] Generate per-target `@unsafe` warnings for Masstree so reviews surface unsafe usage quickly.
- [ ] Enforce the RustyCpp call matrix: a function promoted to `@safe` may only call other `@safe`/`@unsafe` functions—no undeclared dependencies allowed.
- [ ] Add a checker step that fails builds when STL APIs are invoked from `@safe` code without `@external` annotations.
- [x] Auto-update the `third-party/rusty-cpp` checkout to the latest `origin/main` during configuration.

### 1.3 Safety Guidelines
- [ ] Draft Masstree-specific guidance describing when `@unsafe` is acceptable (RCU, raw node access, debug tooling).
- [ ] Produce a reviewer checklist for `MasstreeValueHandle` changes (handle provenance, GC scheduling, null-handle semantics).
- [ ] Align contracts with the silo/sto and distributed owners for shared types (`dbtuple`, `transaction`) to avoid conflicting borrow stories.
- [ ] Provide guidance on replacing STL containers with Rusty equivalents in `@safe` contexts, or requiring explicit `@external` annotations when replacements are not feasible.

---

## Phase 2: Value Ownership & GC 

### 2.1 Handle Consolidation
- [x] Provide a trivially copyable `MasstreeValueHandle` with typed accessors (`src/mako/masstree/value_handle.hh`).
- [x] Replace `value_ptr()` casts in `transaction::write_record_t` with typed getters (`src/mako/txn.h:248`).
- [x] Deprecate integer-only `make_value_handle` helpers and update tests that fabricate handles from raw integers (`test/test_masstree.cc:330`).
- [x] Introduce an RAII wrapper (e.g., `MasstreePayload`) tying handles to RCU deleters for inserts/removes.
- [x] Ensure logical deletes rely on handle state (`has_value() == false`) instead of raw tuple pointers (`src/mako/txn_impl.h:431`).

### 2.2 RCU & Memory Reclamation
- [x] Wrap `rcu::sync::alloc/dealloc/dealloc_rcu` in `@unsafe` shims that document synchronization expectations (`src/mako/rcu.h:141`–200).
- [x] Add debug-only provenance checks ensuring handles originate from RCU-safe allocators.
- [x] Extend `test/test_masstree.cc` to stress GC paths, ensuring borrowed payloads are invalidated after `dealloc_rcu`.
- [ ] Require `free_with_fn` callers to include explicit `@unsafe` rationales and reference tests (`src/mako/rcu.h:210`).
- [x] Annotate the new RCU helper wrappers (`masstree::rcu_*`) explicitly so the borrow checker enforces their `@unsafe` contracts.

### 2.3 Node Mutation APIs
- [x] Layer a safe façade (`safe_mbtree`) over `insert`, `insert_if_absent`, and `remove`, returning typed results while keeping raw variants `@unsafe` (`src/mako/masstree_btree.h:324`–377).
- [x] Audit call sites to ensure `old_v` handles are checked before use (`src/mako/benchmarks/kvdb_wrapper_impl.h:351`, `:358`).
- [x] Prevent `insert_info_t` from leaking raw node pointers to other threads.
- [ ] Promote façade helpers to `@safe` once they no longer rely on raw pointers or STL utilities without annotations.

---

## Phase 3: Traversal & Scan Safety 

### 3.1 Search/Scan Interfaces
- [ ] Replace raw callback params in `low_level_search_range_callback` with view objects that encode lifetimes (`src/mako/masstree_btree.h:304`–318).
- [ ] Provide guard objects that invalidate callbacks once node latches drop to avoid dangling references.
- [ ] Extend `test/test_masstree.cc` with concurrent scan coverage to validate new interfaces.

### 3.2 Tree Walk & Debug
- [ ] Wrap `tree_walk` to hide raw node pointers and expose safe visitors (`src/mako/masstree_btree.h:400`–474).
- [ ] Make `ExtractValues`/`NodeStringify` return copies or structured dumps instead of raw handles (`src/mako/masstree_btree.h:447`–466).
- [ ] Document remaining debug-only `@unsafe` paths for manual inspection.

---

## Phase 4: Transaction & Logging Integration

### 4.1 Tuple Writers & Logging
- [ ] Update `dbtuple::tuple_writer_t` to accept `MasstreeValueHandle` or typed payloads directly (`src/mako/tuple.h:841`, `src/mako/txn_proto2_impl.h:819`–827).
- [ ] Refactor log delta writers to avoid `const void*` plumbing (`src/mako/txn_proto2_impl.h:928`–936).
- [ ] Document null-handle semantics as the canonical delete marker.
- [ ] Annotate tuple-writer and logging helpers with the appropriate RustyCpp safety markers (`@unsafe` today, upgrade to `@safe` once void pointers are removed).

### 4.2 Transaction Coordination
- [x] Feed handles through base transaction insert/commit paths (`src/mako/base_txn_btree.h:364`, `src/mako/txn_impl.h:540`).
- [x] Ensure write-set lookups use typed accessors (`value_as_const<T>()`) (`src/mako/txn_impl.h:583`).
- [ ] Review exception/error paths in `base_txn_btree` for correct handle propagation.
- [ ] Audit any transaction helpers marked `@safe` to guarantee they avoid calling undeclared legacy code and adhere to RustyCpp borrow rules.

---

## Phase 5: Integration & Tooling 

### 5.1 Benchmarks & Utilities
- [x] Migrate `src/mako/btree.cc` helpers to the safe façade so legacy tests remain valid.
- [x] Update `kvdb_wrapper_impl.h` to use RAII deleters and document ownership (`src/mako/benchmarks/kvdb_wrapper_impl.h:339`–377).
- [ ] Replace STL containers used in `@safe` benchmark code with Rusty counterparts or annotate them via `@external` so the checker understands their safety profile.

### 5.2 Automated Checking
- [ ] Ensure `test_masstree` runs under AddressSanitizer with borrow checking enabled in CI.
- [ ] Gather throughput/latency baselines pre/post migration to watch for regressions (`src/mako/btree.cc` microbenchmarks).
- [ ] Log runtime assertions that catch stale-handle use at transaction boundaries.
- [ ] Fail CI if `rusty-cpp-checker` reports new `@safe` → undeclared call violations, and ensure the checker itself builds automatically via the new target.

---

## Phase 6: Documentation & Knowledge Transfer 
- [ ] Publish a Masstree safety guide covering handles, RCU, and `@unsafe` conventions.
- [ ] Record justifications for each remaining `@unsafe` block both inline and in docs.
- [ ] Share weekly progress notes with silo/distributed owners; flag interface changes early.
- [ ] Provide a quick-reference matrix (based on the RustyCpp README) mapping Masstree modules to required annotations and preferred Rusty safe types.

---

## Implementation Strategy
1. Harden value ownership and GC semantics before touching traversal APIs.
2. Introduce safe façade layers so downstream code no longer sees raw pointers.
3. Eliminate surviving `const void*` plumbing in transactions and logging.
4. Keep borrow checking enabled at every stage; add targeted tests after each milestone.

## Common Patterns

```cpp
// Before
write_set.emplace_back(tuple, key, value_ptr, writer, &btr, insert);

// After
auto handle = value_ptr ? MasstreeValueHandle::from_ptr(value_ptr)
                        : MasstreeValueHandle::null();
write_set.emplace_back(tuple, key, handle, writer, &btr, insert);
```

```cpp
// Before
const kvdb_record* r = reinterpret_cast<const kvdb_record*>(raw);

// After
const kvdb_record* r = raw.value_as_const<kvdb_record>();
```

---

## Acceptable `@unsafe` Usage
1. **RCU interfaces** – exchanging memory with epoch-based reclamation (`src/mako/masstree_btree.h:132`, `src/mako/rcu.h:141`).
2. **Masstree node mutation** – operations that rely on internal latching and raw pointers (`src/mako/masstree_btree.h:288`, `:324`).
3. **Debug tooling** – read-only snapshots that expose internal buffers.

Each `@unsafe` section must include:
- A comment describing lifetime/aliasing expectations.
- Links to covering tests.
- A TODO (or plan item) if you expect to replace it with a safe façade later.


## Risk Mitigation
1. **RCU misuse** → Add debug provenance checks and GC stress tests.
2. **Performance regressions** → Benchmark after each major API change.
3. **API churn** → Maintain backward-compatible shims until downstream owners migrate.
4. **Testing gaps** → Expand GTest coverage and integrate sanitizer runs.

---

## Success Metrics
- [ ] 100 % of Masstree entry points either safe or explicitly `@unsafe` with justification.
- [ ] Borrow checker clean across Masstree builds.
- [ ] <5 % throughput regression on Masstree benchmarks.
- [ ] Sanitizer suites report no Masstree-related issues.

---

## Tools & Resources
- RustyCpp checker (`third-party/rusty-cpp`), AddressSanitizer, ThreadSanitizer.
- GTest harness (`test/test_masstree.cc`) with planned concurrent extensions.
- Profiling via `src/mako/btree.cc` microbenchmarks.
- Documentation templates inherited from the RRR migration.

---
