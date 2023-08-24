// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "../ozks_distributed.h"
#include "../ozks_config_dist.h"
#include "oZKS/storage/batch_storage.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/storage/memory_storage_batch_inserter.h"
#include "oZKS/storage/memory_storage_cache.h"
#include "oZKS/compressed_trie.h"
#include "oZKS/utilities.h"

// GTest
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks::utils;
using namespace ozks_distributed;

constexpr size_t random_iterations = 50000;

namespace {
    /**
    Class for testing Batch storage
    */
    class TestBackingStorage : public ozks::storage::BatchStorage {
    public:
        TestBackingStorage()
        {}

        virtual ~TestBackingStorage()
        {}

        bool load_ctnode(
            trie_id_type trie_id,
            const PartialLabel &node_id,
            shared_ptr<Storage> storage,
            CTNodeStored &node) override
        {
            return storage_.load_ctnode(trie_id, node_id, storage, node);
        }

        void save_ctnode(trie_id_type trie_id, const CTNodeStored &node) override
        {
            storage_.save_ctnode(trie_id, node);
        }

        bool load_compressed_trie(trie_id_type trie_id, CompressedTrie &trie) override
        {
            return storage_.load_compressed_trie(trie_id, trie);
        }

        void save_compressed_trie(const CompressedTrie &trie) override
        {
            storage_.save_compressed_trie(trie);
        }

        bool load_store_element(
            trie_id_type trie_id,
            const std::vector<std::byte> &key,
            store_value_type &value) override
        {
            return storage_.load_store_element(trie_id, key, value);
        }

        void save_store_element(
            trie_id_type trie_id,
            const std::vector<std::byte> &key,
            const store_value_type &value) override
        {
            storage_.save_store_element(trie_id, key, value);
        }

        void flush(trie_id_type) override
        {
            // Nothing really.
        }

        void add_ctnode(trie_id_type, const CTNodeStored &) override
        {
            throw runtime_error("Should we be called?");
        }

        void add_compressed_trie(const CompressedTrie &) override
        {
            throw runtime_error("Should we be called?");
        }

        void add_store_element(
            trie_id_type, const vector<byte> &, const store_value_type &) override
        {
            throw runtime_error("Should we be called?");
        }

        size_t get_compressed_trie_epoch(trie_id_type trie_id) override
        {
            return storage_.get_compressed_trie_epoch(trie_id);
        }

        void load_updated_elements(
            size_t epoch, trie_id_type trie_id, shared_ptr<Storage> storage) override
        {
            if (storage == nullptr) {
                return;
            }

            auto it = updated_nodes_.find(epoch);
            if (it != updated_nodes_.end()) {
                for (auto &node : it->second) {
                    if (nullptr != storage) {
                        storage->add_ctnode(trie_id, node);
                    }
                }

                updated_nodes_.erase(epoch);
            }

            auto itt = updated_tries_.find(epoch);
            if (itt != updated_tries_.end()) {
                for (auto &trie : itt->second) {
                    storage->add_compressed_trie(trie);
                }
            }
        }

        void delete_ozks(trie_id_type trie_id) override
        {
            storage_.delete_ozks(trie_id);
        }

        void flush(
            trie_id_type trie_id,
            const vector<CTNodeStored> &nodes,
            const vector<CompressedTrie> &tries,
            // const vector<OZKS> &ozks,
            const vector<pair<vector<byte>, store_value_type>> &store_elements) override
        {
            CompressedTrie t;
            storage_.load_compressed_trie(trie_id, t);
            size_t updated_epoch = t.epoch() + 1;

            vector<CompressedTrie> existing_updated_tries;
            auto itt = updated_tries_.find(updated_epoch);
            if (itt != updated_tries_.end()) {
                existing_updated_tries = itt->second;
            }

            for (auto &trie : tries) {
                storage_.save_compressed_trie(trie);
                existing_updated_tries.push_back(trie);
            }

            updated_tries_.insert_or_assign(updated_epoch, std::move(existing_updated_tries));

            vector<CTNodeStored> existing_updated_nodes;
            auto it = updated_nodes_.find(updated_epoch);
            if (it != updated_nodes_.end()) {
                existing_updated_nodes = it->second;
            }

            for (auto &node : nodes) {
                storage_.save_ctnode(trie_id, node);
                existing_updated_nodes.push_back(node);
            }

            updated_nodes_.insert_or_assign(updated_epoch, std::move(existing_updated_nodes));

            for (auto &se : store_elements) {
                storage_.save_store_element(trie_id, se.first, se.second);
            }
        }

        size_t node_count() const
        {
            return storage_.node_count();
        }

        size_t store_element_count() const
        {
            return storage_.store_element_count();
        }

        size_t trie_count() const
        {
            return storage_.trie_count();
        }

        size_t updated_nodes_count()
        {
            return updated_nodes_.size();
        }

        size_t updated_nodes_count(size_t epoch)
        {
            auto it = updated_nodes_.find(epoch);
            if (it == updated_nodes_.end()) {
                return 0;
            }

            return it->second.size();
        }

    private:
        storage::MemoryStorage storage_;
        unordered_map<size_t, vector<CTNodeStored>> updated_nodes_;
        unordered_map<size_t, vector<CompressedTrie>> updated_tries_;
    };

    class TestCachedStorage : public storage::Storage {
    public:
        TestCachedStorage(shared_ptr<storage::Storage> backing_storage, size_t cache_size)
            : storage_(backing_storage, cache_size)
        {}

        virtual ~TestCachedStorage()
        {}

        bool load_ctnode(
            trie_id_type trie_id,
            const PartialLabel &node_id,
            shared_ptr<Storage> storage,
            CTNodeStored &node) override
        {
            return storage_.load_ctnode(trie_id, node_id, storage, node);
        }

        void save_ctnode(trie_id_type trie_id, const CTNodeStored &node) override
        {
            storage_.save_ctnode(trie_id, node);
        }

        bool load_compressed_trie(trie_id_type trie_id, CompressedTrie &trie) override
        {
            return storage_.load_compressed_trie(trie_id, trie);
        }

        void save_compressed_trie(const CompressedTrie &trie) override
        {
            storage_.save_compressed_trie(trie);
        }

        bool load_store_element(
            trie_id_type trie_id,
            const std::vector<std::byte> &key,
            store_value_type &value) override
        {
            return storage_.load_store_element(trie_id, key, value);
        }

        void save_store_element(
            trie_id_type trie_id,
            const std::vector<std::byte> &key,
            const store_value_type &value) override
        {
            storage_.save_store_element(trie_id, key, value);
        }

        void flush(trie_id_type) override
        {
            // Nothing really.
        }

        void add_ctnode(trie_id_type trie_id, const CTNodeStored &node) override
        {
            storage_.add_ctnode(trie_id, node);
            added_nodes_.push_back(node);
        }

        void add_compressed_trie(const CompressedTrie &trie) override
        {
            storage_.add_compressed_trie(trie);
            added_tries_.push_back(trie);
        }

        void add_store_element(
            trie_id_type trie_id, const vector<byte> &key, const store_value_type &value) override
        {
            storage_.add_store_element(trie_id, key, value);
        }

        size_t get_compressed_trie_epoch(trie_id_type trie_id) override
        {
            return storage_.get_compressed_trie_epoch(trie_id);
        }

        void load_updated_elements(
            size_t epoch, trie_id_type trie_id, shared_ptr<Storage> storage) override
        {
            storage_.load_updated_elements(epoch, trie_id, storage);
        }

        void delete_ozks(trie_id_type trie_id) override
        {
            storage_.delete_ozks(trie_id);
        }

        void clear_added_nodes()
        {
            added_nodes_.clear();
        }

        void clear_added_tries()
        {
            added_tries_.clear();
        }

        size_t added_nodes_count()
        {
            return added_nodes_.size();
        }

        size_t added_tries_count()
        {
            return added_tries_.size();
        }

    private:
        storage::MemoryStorageCache storage_;
        vector<CTNodeStored> added_nodes_;
        vector<CompressedTrie> added_tries_;
    };

    /**
    Get random bytes and throw if unsuccessful
    */
    void get_random_bytes(byte *dest, size_t size)
    {
        if (!utils::random_bytes(dest, size))
            throw runtime_error("Failed to get random bytes");
    }

    vector<key_type> RandomInsertTestCore(OZKS &ozks, size_t iterations)
    {
        key_type key(16);
        payload_type payload(40);
        vector<key_type> valid_keys;
        vector<payload_type> valid_payloads;
        //vector<shared_ptr<InsertResult>> insert_results;

        ozks.check_for_update();

        for (size_t i = 0; i < iterations; i++) {
            get_random_bytes(key.data(), key.size());
            get_random_bytes(payload.data(), payload.size());

            ozks.insert(key, payload);

            // Add some keys at random to check later (up to 100)
            byte b;
            get_random_bytes(&b, 1);
            if (valid_keys.size() < 100 && static_cast<int>(b) > 128) {
                valid_keys.push_back(key);
                valid_payloads.push_back(payload);
            }
        }

        // Wait for updater to finish
        this_thread::sleep_for(chrono::seconds(10));
        ozks.check_for_update();

        // Check the valid keys are found and that their path is verified correctly
        for (size_t i = 0; i < valid_keys.size(); i++) {
            auto result = ozks.query(valid_keys[i]);
            EXPECT_TRUE(result.is_member());
            EXPECT_NE(0, result.payload().size());
            EXPECT_TRUE(result.verify(ozks.get_commitment()));
            EXPECT_EQ(valid_payloads[i], result.payload());
        }

        // Check that invalid keys are not found and that their path is verified correctly
        for (size_t i = 0; i < 1000; i++) {
            get_random_bytes(key.data(), key.size());

            auto result = ozks.query(key);
            EXPECT_FALSE(result.is_member());
            EXPECT_EQ(0, result.payload().size());
            EXPECT_TRUE(result.verify(ozks.get_commitment()));
        }

        return valid_keys;
    }
}

TEST(OZKSDistTests, UpdatedNodesTest)
{
    shared_ptr<TestBackingStorage> backing_storage = make_shared<TestBackingStorage>();
    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);
    shared_ptr<storage::MemoryStorageCache> cache_storage =
        make_shared<storage::MemoryStorageCache>(backing_storage, /* cache_size */ 1'000'000);

    OZKSConfigDist config(
        PayloadCommitmentType::CommitedPayload,
        LabelType::VRFLabels,
        TrieType::Stored,
        batch_inserter,
        cache_storage);
    OZKS ozks(config);

    // Insert one element to ensure trie is saved in backing storage
    RandomInsertTestCore(ozks, 1);

    shared_ptr<TestCachedStorage> cached_storage =
        make_shared<TestCachedStorage>(backing_storage, 5000);
    OZKSConfigDist config2(
        PayloadCommitmentType::CommitedPayload,
        LabelType::VRFLabels,
        TrieType::Stored,
        cached_storage,
        {});

    // Generate 3 epochs
    EXPECT_EQ(1, backing_storage->updated_nodes_count());
    RandomInsertTestCore(ozks, 50);
    EXPECT_EQ(1, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 50);
    EXPECT_EQ(1, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 50);
    EXPECT_EQ(1, backing_storage->updated_nodes_count());
}

TEST(OZKSDistTests, UpdaterTest)
{
    shared_ptr<TestBackingStorage> backing_storage = make_shared<TestBackingStorage>();
    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);

    OZKSConfigDist config(
        PayloadCommitmentType::CommitedPayload,
        LabelType::VRFLabels,
        TrieType::Linked,
        batch_inserter, {});
    OZKS ozks(config);

    EXPECT_EQ(0, ozks.get_epoch());
    RandomInsertTestCore(ozks, 10);
    EXPECT_EQ(1, ozks.get_epoch());
}

TEST(OZKSDistTests, BatchInserterCallbackTest)
{
    shared_ptr<TestBackingStorage> backing_storage = make_shared<TestBackingStorage>();
    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);

    OZKSConfigDist config(
        PayloadCommitmentType::CommitedPayload,
        LabelType::VRFLabels,
        TrieType::Stored,
        batch_inserter,
        {});
    OZKS ozks(config);

    vector<byte> saved_ozks;
    ozks.save(saved_ozks);

    // Insert one element to ensure trie is saved in backing storage
    RandomInsertTestCore(ozks, 1);

    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter2 =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);
    shared_ptr<TestCachedStorage> cached_storage =
        make_shared<TestCachedStorage>(batch_inserter2, 10000);

    OZKSConfigDist config2(
        PayloadCommitmentType::CommitedPayload,
        LabelType::VRFLabels,
        TrieType::Stored,
        cached_storage,
        {});

    // This ensures we have the same trie ID
    auto loaded = OZKS::Load(config2.storage(), saved_ozks);
    OZKS &ozks2 = loaded.first;

    // This ensures the trie is in the cache
    size_t epoch = ozks2.get_epoch();

    // Generate 3 epochs
    EXPECT_EQ(1, backing_storage->updated_nodes_count());
    RandomInsertTestCore(ozks, 50);
    EXPECT_EQ(2, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 50);
    EXPECT_EQ(3, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 50);
    EXPECT_EQ(4, backing_storage->updated_nodes_count());

    epoch = ozks2.get_epoch();
    EXPECT_EQ(0, epoch); // Since we get it from the cache

    // This should not throw
    EXPECT_EQ(0, cached_storage->added_nodes_count());
    EXPECT_EQ(0, cached_storage->added_tries_count());
    ozks2.check_for_update();
    epoch = ozks2.get_epoch();
    EXPECT_EQ(0, epoch);
    EXPECT_EQ(
        4, backing_storage->updated_nodes_count()); // Updated nodes should have been retrieved
    EXPECT_EQ(cached_storage->added_nodes_count(), 0);
    EXPECT_EQ(cached_storage->added_tries_count(), 0);
}
