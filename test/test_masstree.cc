#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "mako/masstree_btree.h"
#include "mako/varkey.h"

namespace {

using Tree = concurrent_btree;
using ValueType = typename Tree::value_type;
using StringType = typename Tree::string_type;

// Decode helpers keep assertions readable by working with integers directly.
uint64_t DecodeValue(ValueType ptr) {
    return ptr ? *reinterpret_cast<uint64_t*>(ptr) : 0ULL;
}

uint64_t DecodeKey(const StringType& str) {
    uint64_t out = 0;
    const char* data = str.data();
    for (int i = 0; i < str.length(); ++i) {
        out = (out << 8U) | static_cast<uint8_t>(data[i]);
    }
    return out;
}

} // namespace

class MasstreeBTreeTest : public ::testing::Test {
protected:
    // Every stored value lives in this vector so lifetimes outlast tree operations.
    ValueType storeValue(uint64_t v) {
        values_.push_back(std::make_unique<uint64_t>(v));
        return reinterpret_cast<ValueType>(values_.back().get());
    }

    // Thin wrapper around mbtree::search for balance between ergonomics and control.
    bool searchKey(uint64_t key, uint64_t* out) const {
        ValueType raw{};
        bool found = tree_.search(u64_varkey(key), raw);
        if (found && out) {
            *out = DecodeValue(raw);
        }
        return found;
    }

    template <typename Callback>
    void scan(uint64_t lower, std::optional<uint64_t> upper, Callback&& cb) const {
        // Translate callbacks into decoded key/value pairs for each test.
        auto wrapper = [&](const StringType& key, ValueType value) {
            return cb(DecodeKey(key), DecodeValue(value));
        };
        auto lower_key = u64_varkey(lower);
        if (upper) {
            auto upper_key = u64_varkey(*upper);
            tree_.search_range(lower_key, &upper_key, wrapper);
        } else {
            tree_.search_range(lower_key, nullptr, wrapper);
        }
    }

    template <typename Callback>
    void reverseScan(uint64_t upper, std::optional<uint64_t> lower, Callback&& cb) const {
        // Reverse scan mirrors scan() but walks keys in descending order.
        auto wrapper = [&](const StringType& key, ValueType value) {
            return cb(DecodeKey(key), DecodeValue(value));
        };
        auto upper_key = u64_varkey(upper);
        if (lower) {
            auto lower_key = u64_varkey(*lower);
            tree_.rsearch_range(upper_key, &lower_key, wrapper);
        } else {
            tree_.rsearch_range(upper_key, nullptr, wrapper);
        }
    }

    // Validates the entire tree contents against a reference std::map snapshot.
    void expectTreeEquals(const std::map<uint64_t, uint64_t>& expected) const {
        std::vector<std::pair<uint64_t, uint64_t>> seen;
        scan(0, std::nullopt, [&](uint64_t key, uint64_t value) {
            seen.emplace_back(key, value);
            return true;
        });

        ASSERT_EQ(expected.size(), seen.size());
        size_t index = 0;
        for (const auto& [key, value] : expected) {
            ASSERT_LT(index, seen.size());
            EXPECT_EQ(key, seen[index].first);
            EXPECT_EQ(value, seen[index].second);
            ++index;
        }
    }

    Tree tree_;
    std::vector<std::unique_ptr<uint64_t>> values_;
};

TEST_F(MasstreeBTreeTest, InsertAndSearchSingleKey) {
    auto value = storeValue(42);
    EXPECT_TRUE(tree_.insert(u64_varkey(7), value));

    uint64_t result = 0;
    EXPECT_TRUE(searchKey(7, &result));
    EXPECT_EQ(42U, result);
}

TEST_F(MasstreeBTreeTest, InsertUpdatesExistingKeyAndExposesOldPointer) {
    auto first = storeValue(100);
    auto second = storeValue(200);

    EXPECT_TRUE(tree_.insert(u64_varkey(9), first));
    ValueType previous{};
    EXPECT_FALSE(tree_.insert(u64_varkey(9), second, &previous));
    EXPECT_EQ(first, previous);

    uint64_t result = 0;
    EXPECT_TRUE(searchKey(9, &result));
    EXPECT_EQ(200U, result);
}

TEST_F(MasstreeBTreeTest, InsertIfAbsentOnlyInsertsWhenMissing) {
    auto first = storeValue(1);
    auto second = storeValue(2);

    EXPECT_TRUE(tree_.insert_if_absent(u64_varkey(15), first));
    EXPECT_FALSE(tree_.insert_if_absent(u64_varkey(15), second));

    uint64_t result = 0;
    EXPECT_TRUE(searchKey(15, &result));
    EXPECT_EQ(1U, result);
}

TEST_F(MasstreeBTreeTest, RemoveReturnsPreviousValuePointer) {
    auto value = storeValue(88);
    EXPECT_TRUE(tree_.insert(u64_varkey(3), value));

    ValueType removed{};
    EXPECT_TRUE(tree_.remove(u64_varkey(3), &removed));
    EXPECT_EQ(value, removed);
    EXPECT_FALSE(searchKey(3, nullptr));
}

TEST_F(MasstreeBTreeTest, SizeReflectsMutations) {
    EXPECT_TRUE(tree_.insert(u64_varkey(1), storeValue(1)));
    EXPECT_TRUE(tree_.insert(u64_varkey(2), storeValue(2)));
    EXPECT_TRUE(tree_.insert(u64_varkey(3), storeValue(3)));
    EXPECT_EQ(3U, tree_.size());

    ValueType removed{};
    EXPECT_TRUE(tree_.remove(u64_varkey(2), &removed));
    EXPECT_EQ(2U, tree_.size());
}

TEST_F(MasstreeBTreeTest, ClearReinitialisesTree) {
    EXPECT_TRUE(tree_.insert(u64_varkey(4), storeValue(4)));
    EXPECT_TRUE(tree_.insert(u64_varkey(5), storeValue(5)));
    tree_.clear();

    EXPECT_EQ(0U, tree_.size());
    EXPECT_FALSE(searchKey(4, nullptr));
    EXPECT_TRUE(tree_.insert(u64_varkey(4), storeValue(400)));
}

TEST_F(MasstreeBTreeTest, RangeScanMatchesReferenceMap) {
    std::map<uint64_t, uint64_t> reference;
    for (uint64_t i = 0; i < 50; ++i) {
        reference.emplace(i, 1000 + i);
        EXPECT_TRUE(tree_.insert(u64_varkey(i), storeValue(1000 + i)));
    }

    uint64_t lower = 10, upper = 40;
    size_t count = 0;
    scan(lower, upper, [&](uint64_t key, uint64_t value) {
        EXPECT_GE(key, lower);
        EXPECT_LT(key, upper);
        EXPECT_EQ(reference.at(key), value);
        ++count;
        return true;
    });
    EXPECT_EQ(upper - lower, count);
}

TEST_F(MasstreeBTreeTest, RangeScanStopsWhenCallbackReturnsFalse) {
    for (uint64_t i = 0; i < 10; ++i) {
        EXPECT_TRUE(tree_.insert(u64_varkey(i), storeValue(200 + i)));
    }
    size_t count = 0;
    scan(0, std::nullopt, [&](uint64_t, uint64_t) {
        ++count;
        return count < 3;
    });
    EXPECT_EQ(3U, count);
}

TEST_F(MasstreeBTreeTest, ReverseRangeHonoursBounds) {
    for (uint64_t i = 0; i < 8; ++i) {
        EXPECT_TRUE(tree_.insert(u64_varkey(i), storeValue(300 + i)));
    }
    std::vector<uint64_t> keys;
    reverseScan(7, 3, [&](uint64_t key, uint64_t) {
        keys.push_back(key);
        return true;
    });
    std::vector<uint64_t> expected = {7, 6, 5, 4};
    EXPECT_EQ(expected, keys);
}

TEST_F(MasstreeBTreeTest, NodeStringifyIncludesVersionInformation) {
    EXPECT_TRUE(tree_.insert(u64_varkey(1), storeValue(1)));
    ValueType raw{};
    Tree::versioned_node_t info{};
    EXPECT_TRUE(tree_.search(u64_varkey(1), raw, &info));
    std::string serialized = tree_.NodeStringify(info.first);
    EXPECT_FALSE(serialized.empty());
    EXPECT_NE(std::string::npos, serialized.find("node[v="));
}

// Verify the extracted leaf snapshot reflects current values only.
TEST_F(MasstreeBTreeTest, ExtractValuesReflectsLeafContents) {
    for (uint64_t i = 0; i < 12; ++i) {
        EXPECT_TRUE(tree_.insert(u64_varkey(i), storeValue(400 + i)));
    }
    ValueType raw{};
    Tree::versioned_node_t info{};
    EXPECT_TRUE(tree_.search(u64_varkey(6), raw, &info));

    auto entries = tree_.ExtractValues(info.first);
    std::set<uint64_t> seen;
    for (const auto& [ptr, has_suffix] : entries) {
        seen.insert(DecodeValue(ptr));
        EXPECT_FALSE(has_suffix);
    }
    EXPECT_NE(seen.end(), seen.find(400 + 6));
}

// Fuzz-style regression that compares Masstree against std::map across mixed ops.
TEST_F(MasstreeBTreeTest, RandomisedWorkloadMatchesStdMapModel) {
    std::mt19937_64 rng(1337);
    std::uniform_int_distribution<uint64_t> key_dist(0, 255);
    std::uniform_int_distribution<uint64_t> value_dist(0, 100000);
    std::uniform_int_distribution<int> op_dist(0, 3);

    std::map<uint64_t, uint64_t> model;

    const int operations = 400;
    for (int i = 0; i < operations; ++i) {
        uint64_t key = key_dist(rng);
        int op = op_dist(rng);

        if (op == 0) { // insert/update
            uint64_t value = value_dist(rng);
            ValueType ptr = storeValue(value);
            bool inserted = tree_.insert(u64_varkey(key), ptr);
            auto [it, fresh] = model.insert_or_assign(key, value);
            (void)fresh;
            if (!inserted) {
                uint64_t result = 0;
                ASSERT_TRUE(searchKey(key, &result));
                EXPECT_EQ(value, result);
            }
        } else if (op == 1) { // insert_if_absent
            uint64_t value = value_dist(rng);
            ValueType ptr = storeValue(value);
            bool tree_inserted = tree_.insert_if_absent(u64_varkey(key), ptr);
            bool map_inserted = model.emplace(key, value).second;
            if (!map_inserted) {
                // discard new pointer; tree should have ignored it.
                uint64_t result = 0;
                ASSERT_TRUE(searchKey(key, &result));
                EXPECT_EQ(model.at(key), result);
            } else {
                EXPECT_TRUE(tree_inserted);
            }
        } else if (op == 2) { // remove
            ValueType removed{};
            bool tree_removed = tree_.remove(u64_varkey(key), &removed);
            bool map_removed = model.erase(key) > 0;
            EXPECT_EQ(map_removed, tree_removed);
        } else { // scan check
            expectTreeEquals(model);
        }
    }

    expectTreeEquals(model);
}

// Ensure byte-wise ordering aligns with Masstree's lexicographic comparator.
TEST_F(MasstreeBTreeTest, StringKeysMaintainLexicographicOrder) {
    std::map<std::string, uint64_t> expected = {
        {"user:000", 1},
        {"user:001", 2},
        {"user:010", 3},
        {"user:100", 4},
    };

    for (const auto& [key, value] : expected) {
        EXPECT_TRUE(tree_.insert(varkey(key), storeValue(value)));
    }

    std::vector<std::string> seen;
    varkey lower_key("user:000");
    auto callback = [&](const StringType& key, ValueType value) {
        std::string materialised(key.data(), key.length());
        seen.push_back(materialised);
        EXPECT_EQ(expected[materialised], DecodeValue(value));
        return true;
    };
    tree_.search_range(lower_key, nullptr, callback);

    std::vector<std::string> expected_order;
    for (const auto& [key, _] : expected) {
        expected_order.push_back(key);
    }
    EXPECT_EQ(expected_order, seen);
}

// Guard against helper parameters being mutated when lookups fail.
TEST_F(MasstreeBTreeTest, SearchMissingKeyDoesNotModifyOutputs) {
    ValueType raw = reinterpret_cast<ValueType>(0xdeadbeef);
    Tree::versioned_node_t info(
        reinterpret_cast<const Tree::node_opaque_t*>(0x1),
        123);

    EXPECT_FALSE(tree_.search(u64_varkey(999), raw, &info));
    EXPECT_EQ(reinterpret_cast<ValueType>(0xdeadbeef), raw);
    EXPECT_EQ(0U, info.second);
}

TEST_F(MasstreeBTreeTest, RangeScanWithEqualBoundsIsEmpty) {
    for (uint64_t i = 0; i < 10; ++i) {
        EXPECT_TRUE(tree_.insert(u64_varkey(i), storeValue(500 + i)));
    }

    size_t count = 0;
    scan(4, 4, [&](uint64_t, uint64_t) {
        ++count;
        return true;
    });
    EXPECT_EQ(0U, count);
}

TEST_F(MasstreeBTreeTest, ReverseRangeExcludesLowerBound) {
    for (uint64_t i = 0; i < 6; ++i) {
        EXPECT_TRUE(tree_.insert(u64_varkey(i), storeValue(600 + i)));
    }

    std::vector<uint64_t> keys;
    reverseScan(5, 2, [&](uint64_t key, uint64_t) {
        keys.push_back(key);
        return true;
    });

    std::vector<uint64_t> expected = {5, 4, 3};
    EXPECT_EQ(expected, keys);
}

TEST_F(MasstreeBTreeTest, RemoveWithNullOldValuePointerSucceeds) {
    EXPECT_TRUE(tree_.insert(u64_varkey(10), storeValue(42)));
    EXPECT_TRUE(tree_.remove(u64_varkey(10), nullptr));
    EXPECT_FALSE(searchKey(10, nullptr));
}

TEST_F(MasstreeBTreeTest, InsertIfAbsentLeavesExistingPointerIntact) {
    auto first = storeValue(111);
    EXPECT_TRUE(tree_.insert(u64_varkey(12), first));

    auto second = storeValue(222);
    EXPECT_FALSE(tree_.insert_if_absent(u64_varkey(12), second));

    ValueType raw{};
    EXPECT_TRUE(tree_.search(u64_varkey(12), raw));
    EXPECT_EQ(first, raw);
}

// Exercise the lower-level callback API to guarantee node visitation contracts.
TEST_F(MasstreeBTreeTest, LowLevelSearchRangeCallbackVisitsNodes) {
    for (uint64_t i = 0; i < 8; ++i) {
        EXPECT_TRUE(tree_.insert(u64_varkey(i), storeValue(700 + i)));
    }

    struct Callback : public Tree::low_level_search_range_callback {
        std::vector<const Tree::node_opaque_t*> nodes;
        std::vector<uint64_t> keys;

        void on_resp_node(const Tree::node_opaque_t* n, uint64_t) override {
            nodes.push_back(n);
        }

        bool invoke(const StringType& key, ValueType value,
                    const Tree::node_opaque_t*, uint64_t) override {
            keys.push_back(DecodeKey(key));
            EXPECT_EQ(700U + keys.back(), DecodeValue(value));
            return true;
        }
    } cb;

    auto lower = u64_varkey(2);
    auto upper = u64_varkey(6);
    tree_.search_range_call(lower, &upper, cb);

    EXPECT_FALSE(cb.nodes.empty());
    EXPECT_EQ(std::vector<uint64_t>({2, 3, 4, 5}), cb.keys);
}

// After deleting a key, the leaf dump should omit the removed payload.
TEST_F(MasstreeBTreeTest, ExtractValuesRespectsRemovals) {
    for (uint64_t i = 0; i < 10; ++i) {
        EXPECT_TRUE(tree_.insert(u64_varkey(i), storeValue(800 + i)));
    }

    ValueType raw{};
    Tree::versioned_node_t info{};
    EXPECT_TRUE(tree_.search(u64_varkey(4), raw, &info));

    ValueType removed{};
    EXPECT_TRUE(tree_.remove(u64_varkey(4), &removed));

    auto entries = tree_.ExtractValues(info.first);
    for (const auto& [ptr, _] : entries) {
        EXPECT_NE(DecodeValue(ptr), 804U);
    }
}
