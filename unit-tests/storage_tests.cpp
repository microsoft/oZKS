// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

// oZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node_stored.h"
#include "oZKS/partial_label.h"
#include "oZKS/storage/batch_storage.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/storage/memory_storage_batch_inserter.h"
#include "oZKS/storage/memory_storage_cache.h"
#include "oZKS/utilities.h"

// GTest
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks::utils;

namespace {
    /**
    Wraps a MemoryStorage and records every load_ctnodes call (count + the pairs requested)
    so tests can assert that higher layers only consulted the backing for the right subset.
    */
    class InstrumentedBatchStorage : public BatchStorage {
    public:
        InstrumentedBatchStorage() : backing_(make_shared<MemoryStorage>())
        {}

        struct LoadBatchCall {
            vector<pair<PartialLabel, size_t>> pairs;
        };

        size_t load_ctnodes_call_count() const { return calls_.size(); }
        const vector<LoadBatchCall> &load_ctnodes_calls() const { return calls_; }

        void reset_instrumentation() { calls_.clear(); }

        bool load_ctnode(
            trie_id_type trie_id,
            const PartialLabel &node_id,
            shared_ptr<Storage> storage,
            CTNodeStored &node) override
        {
            return backing_->load_ctnode(trie_id, node_id, storage, node);
        }

        bool load_ctnode(
            trie_id_type trie_id,
            const PartialLabel &node_id,
            size_t epoch,
            shared_ptr<Storage> storage,
            CTNodeStored &node) override
        {
            return backing_->load_ctnode(trie_id, node_id, epoch, storage, node);
        }

        bool load_ctnodes(
            trie_id_type trie_id,
            const vector<pair<PartialLabel, size_t>> &label_epoch_pairs,
            vector<CTNodeStored> &out_nodes) override
        {
            calls_.push_back({ label_epoch_pairs });
            return backing_->load_ctnodes(trie_id, label_epoch_pairs, out_nodes);
        }

        void save_ctnode(trie_id_type trie_id, const CTNodeStored &node) override
        {
            backing_->save_ctnode(trie_id, node);
        }

        bool load_compressed_trie(trie_id_type trie_id, CompressedTrie &trie) override
        {
            return backing_->load_compressed_trie(trie_id, trie);
        }

        void save_compressed_trie(const CompressedTrie &trie) override
        {
            backing_->save_compressed_trie(trie);
        }

        bool load_store_element(
            trie_id_type trie_id, const vector<byte> &key, store_value_type &value) override
        {
            return backing_->load_store_element(trie_id, key, value);
        }

        void save_store_element(
            trie_id_type trie_id,
            const vector<byte> &key,
            const store_value_type &value) override
        {
            backing_->save_store_element(trie_id, key, value);
        }

        void flush(trie_id_type) override {}

        void flush(
            trie_id_type trie_id,
            const vector<CTNodeStored> &nodes,
            const vector<CompressedTrie> &tries,
            const vector<pair<vector<byte>, store_value_type>> &store_elements) override
        {
            for (const auto &n : nodes) backing_->save_ctnode(trie_id, n);
            for (const auto &t : tries) backing_->save_compressed_trie(t);
            for (const auto &[k, v] : store_elements) backing_->save_store_element(trie_id, k, v);
        }

        void add_ctnode(trie_id_type trie_id, const CTNodeStored &node) override
        {
            // MemoryStorage::add_ctnode throws; use save_ctnode so cache warm-ups that fall
            // through to the backing still work.
            backing_->save_ctnode(trie_id, node);
        }

        void add_compressed_trie(const CompressedTrie &trie) override
        {
            backing_->save_compressed_trie(trie);
        }

        void add_store_element(
            trie_id_type trie_id,
            const vector<byte> &key,
            const store_value_type &value) override
        {
            backing_->save_store_element(trie_id, key, value);
        }

        size_t get_compressed_trie_epoch(trie_id_type trie_id) override
        {
            return backing_->get_compressed_trie_epoch(trie_id);
        }

        void load_updated_elements(
            size_t epoch, trie_id_type trie_id, shared_ptr<Storage> storage) override
        {
            backing_->load_updated_elements(epoch, trie_id, storage);
        }

        void delete_ozks(trie_id_type trie_id) override { backing_->delete_ozks(trie_id); }

    private:
        shared_ptr<MemoryStorage> backing_;
        vector<LoadBatchCall> calls_;
    };

    // Minimal non-leaf CTNodeStored with a known (label, hash, epoch).
    CTNodeStored make_test_node(const PartialLabel &label, const hash_type &hash, size_t epoch)
    {
        return CTNodeStored(
            /* trie */ nullptr,
            label,
            hash,
            epoch,
            /* left_label */ {},
            /* right_label */ {},
            /* left_child_epoch */ 0,
            /* right_child_epoch */ 0);
    }
} // namespace

// MemoryStorageBatchInserter::load_ctnodes must serve unsaved nodes from its local map
// and route only the remaining pairs to the backing storage — preserving input order.
TEST(StorageTests, BatchInserterLoadCtnodesSplitsUnsavedAndBacking)
{
    auto backing = make_shared<InstrumentedBatchStorage>();
    auto inserter = make_shared<MemoryStorageBatchInserter>(backing);

    constexpr trie_id_type trie_id = 42;
    constexpr size_t epoch = 1;

    PartialLabel label_A = make_bytes<PartialLabel>(0x01);
    PartialLabel label_B = make_bytes<PartialLabel>(0x02);
    PartialLabel label_C = make_bytes<PartialLabel>(0x03);

    // Hashes must have the low bit of byte 0 clear — CTNode uses that bit as the dirty
    // flag and hash() masks it out on return.
    hash_type hash_A = make_bytes<hash_type>(0xA0);
    hash_type hash_B = make_bytes<hash_type>(0xB0);
    hash_type hash_C = make_bytes<hash_type>(0xC0);

    // A and C: live only in the backing (saved + flushed through the inserter).
    inserter->save_ctnode(trie_id, make_test_node(label_A, hash_A, epoch));
    inserter->save_ctnode(trie_id, make_test_node(label_C, hash_C, epoch));
    inserter->flush(trie_id);

    // B: saved to the inserter but not flushed — stays in unsaved_nodes_.
    inserter->save_ctnode(trie_id, make_test_node(label_B, hash_B, epoch));

    backing->reset_instrumentation();

    vector<pair<PartialLabel, size_t>> pairs = {
        { label_A, epoch },
        { label_B, epoch },
        { label_C, epoch }
    };
    vector<CTNodeStored> out;
    ASSERT_TRUE(inserter->load_ctnodes(trie_id, pairs, out));

    // All three returned in input order.
    ASSERT_EQ(3U, out.size());
    EXPECT_EQ(label_A, out[0].label());
    EXPECT_EQ(label_B, out[1].label());
    EXPECT_EQ(label_C, out[2].label());
    EXPECT_EQ(hash_A, out[0].hash());
    EXPECT_EQ(hash_B, out[1].hash());
    EXPECT_EQ(hash_C, out[2].hash());

    // Split evidence: backing was called exactly once, and *only* with A and C —
    // B was served entirely from the unsaved map.
    ASSERT_EQ(1U, backing->load_ctnodes_call_count());
    const auto &call = backing->load_ctnodes_calls()[0];
    ASSERT_EQ(2U, call.pairs.size());
    EXPECT_EQ(label_A, call.pairs[0].first);
    EXPECT_EQ(label_C, call.pairs[1].first);
}

// MemoryStorageCache::load_ctnodes must serve cache-hits locally and route only the
// cache-misses to the backing storage — preserving input order.
TEST(StorageTests, CacheLoadCtnodesSplitsCacheHitsAndBacking)
{
    auto backing = make_shared<InstrumentedBatchStorage>();
    auto cache = make_shared<MemoryStorageCache>(backing, /* cache_size */ 128);

    constexpr trie_id_type trie_id = 17;
    constexpr size_t epoch = 1;

    PartialLabel label_A = make_bytes<PartialLabel>(0x10);
    PartialLabel label_B = make_bytes<PartialLabel>(0x20);
    PartialLabel label_C = make_bytes<PartialLabel>(0x30);

    // Hashes must have the low bit of byte 0 clear — see note in the batch-inserter test.
    hash_type hash_A = make_bytes<hash_type>(0x10);
    hash_type hash_B = make_bytes<hash_type>(0x20);
    hash_type hash_C = make_bytes<hash_type>(0x30);

    // All three nodes live in the backing. A and C get warmed into the cache by
    // individual load_ctnode calls; B stays cache-cold.
    backing->save_ctnode(trie_id, make_test_node(label_A, hash_A, epoch));
    backing->save_ctnode(trie_id, make_test_node(label_B, hash_B, epoch));
    backing->save_ctnode(trie_id, make_test_node(label_C, hash_C, epoch));

    CTNodeStored warmup;
    ASSERT_TRUE(cache->load_ctnode(trie_id, label_A, epoch, /* storage */ nullptr, warmup));
    ASSERT_TRUE(cache->load_ctnode(trie_id, label_C, epoch, /* storage */ nullptr, warmup));

    backing->reset_instrumentation();

    vector<pair<PartialLabel, size_t>> pairs = {
        { label_A, epoch },
        { label_B, epoch },
        { label_C, epoch }
    };
    vector<CTNodeStored> out;
    ASSERT_TRUE(cache->load_ctnodes(trie_id, pairs, out));

    ASSERT_EQ(3U, out.size());
    EXPECT_EQ(label_A, out[0].label());
    EXPECT_EQ(label_B, out[1].label());
    EXPECT_EQ(label_C, out[2].label());
    EXPECT_EQ(hash_A, out[0].hash());
    EXPECT_EQ(hash_B, out[1].hash());
    EXPECT_EQ(hash_C, out[2].hash());

    // Split evidence: backing was called once, and *only* with B — A and C were
    // served from the cache.
    ASSERT_EQ(1U, backing->load_ctnodes_call_count());
    const auto &call = backing->load_ctnodes_calls()[0];
    ASSERT_EQ(1U, call.pairs.size());
    EXPECT_EQ(label_B, call.pairs[0].first);
}

// MemoryStorageCache::load_ctnodes must keep the latest-epoch index coherent: when a
// batch brings in a newer version of a label the cache already tracks, a subsequent
// latest-version load_ctnode must return the newer version rather than the stale one.
TEST(StorageTests, CacheLoadCtnodesAdvancesLatestEpochIndex)
{
    auto backing = make_shared<InstrumentedBatchStorage>();
    auto cache = make_shared<MemoryStorageCache>(backing, /* cache_size */ 128);

    constexpr trie_id_type trie_id = 23;

    PartialLabel label = make_bytes<PartialLabel>(0x40);

    // Hashes must have the low bit of byte 0 clear — see note in the batch-inserter test.
    hash_type hash_v1 = make_bytes<hash_type>(0x10);
    hash_type hash_v2 = make_bytes<hash_type>(0x20);

    // The cache already tracks the label at epoch 1, so node_epochs_ points there.
    cache->add_ctnode(trie_id, make_test_node(label, hash_v1, /* epoch */ 1));

    // A newer version (epoch 2) lives in the backing and gets batch-loaded.
    backing->save_ctnode(trie_id, make_test_node(label, hash_v2, /* epoch */ 2));

    vector<pair<PartialLabel, size_t>> pairs = { { label, 2 } };
    vector<CTNodeStored> out;
    ASSERT_TRUE(cache->load_ctnodes(trie_id, pairs, out));
    ASSERT_EQ(1U, out.size());
    EXPECT_EQ(hash_v2, out[0].hash());

    backing->reset_instrumentation();

    // A latest-version load must now resolve to epoch 2 (served from cache), proving the
    // batch load advanced the latest-epoch index instead of leaving it on the stale epoch 1.
    CTNodeStored latest;
    ASSERT_TRUE(cache->load_ctnode(trie_id, label, /* storage */ nullptr, latest));
    EXPECT_EQ(2U, latest.epoch());
    EXPECT_EQ(hash_v2, latest.hash());

    // The latest read was served from cache, so the backing batch path was not consulted.
    EXPECT_EQ(0U, backing->load_ctnodes_call_count());
}
