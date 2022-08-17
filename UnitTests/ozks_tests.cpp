// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "oZKS/fourq/random.h"
#include "oZKS/ozks.h"
#include "oZKS/storage/batch_storage.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/storage/memory_storage_batch_inserter.h"
#include "oZKS/storage/memory_storage_cache.h"
#include "oZKS/utilities.h"

// GTest
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

constexpr size_t random_iterations = 50000;

namespace {
    /**
    Get random bytes and throw if unsuccessful
    */
    void get_random_bytes(unsigned char *dest, size_t size)
    {
        if (!random_bytes(dest, static_cast<unsigned int>(size)))
            throw runtime_error("Failed to get random bytes");
    }

    /**
    Class for testing Batch storage
    */
    class TestBackingStorage : public ozks::storage::BatchStorage {
    public:
        TestBackingStorage()
        {}

        bool load_ctnode(
            const std::vector<std::byte> &trie_id,
            const partial_label_type &node_id,
            CTNode &node) override
        {
            return storage_.load_ctnode(trie_id, node_id, node);
        }

        void save_ctnode(const std::vector<std::byte> &trie_id, const CTNode &node) override
        {
            storage_.save_ctnode(trie_id, node);
        }

        bool load_compressed_trie(
            const std::vector<std::byte> &trie_id, CompressedTrie &trie) override
        {
            return storage_.load_compressed_trie(trie_id, trie);
        }

        void save_compressed_trie(const CompressedTrie &trie) override
        {
            storage_.save_compressed_trie(trie);
        }

        bool load_ozks(const std::vector<std::byte> &trie_id, OZKS &ozks) override
        {
            return storage_.load_ozks(trie_id, ozks);
        }

        void save_ozks(const OZKS &ozks) override
        {
            storage_.save_ozks(ozks);
        }

        bool load_store_element(
            const std::vector<std::byte> &trie_id,
            const std::vector<std::byte> &key,
            store_value_type &value) override
        {
            return storage_.load_store_element(trie_id, key, value);
        }

        void save_store_element(
            const std::vector<std::byte> &trie_id,
            const std::vector<std::byte> &key,
            const store_value_type &value) override
        {
            storage_.save_store_element(trie_id, key, value);
        }

        void flush(const vector<byte>&) override
        {
            // Nothing really.
        }

        void add_ctnode(const vector<byte>&, const CTNode&) override
        {
            throw runtime_error("Should we be called?");
        }

        void add_compressed_trie(const CompressedTrie&) override
        {
            throw runtime_error("Should we be called?");
        }

        void add_store_element(
            const vector<byte>&,
            const vector<byte>&,
            const store_value_type&) override
        {
            throw runtime_error("Should we be called?");
        }

        size_t get_compressed_trie_epoch(const vector<byte> &trie_id) override
        {
            return storage_.get_compressed_trie_epoch(trie_id);
        }

        void load_updated_elements(
            size_t epoch, const vector<byte>& trie_id, Storage* storage) override
        {
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

        void flush(
            const vector<byte> &trie_id,
            const vector<CTNode> &nodes,
            const vector<CompressedTrie> &tries,
            const vector<OZKS> &ozks,
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

            updated_tries_[updated_epoch] = existing_updated_tries;

            vector<CTNode> existing_updated_nodes;
            auto it = updated_nodes_.find(updated_epoch);
            if (it != updated_nodes_.end()) {
                existing_updated_nodes = it->second;
            }

            for (auto &node : nodes) {
                storage_.save_ctnode(trie_id, node);
                existing_updated_nodes.push_back(node);
            }

            updated_nodes_[updated_epoch] = existing_updated_nodes;

            for (auto &ozksi : ozks) {
                storage_.save_ozks(ozksi);
            }

            for (auto &se : store_elements) {
                storage_.save_store_element(trie_id, se.first, se.second);
            }
        }

        bool load_ctnode(
            const vector<byte> &trie_id,
            const partial_label_type &node_id,
            Storage*,
            CTNode &node) override
        {
            return load_ctnode(trie_id, node_id, node);
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

        size_t ozks_count() const
        {
            return storage_.ozks_count();
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
        unordered_map<size_t, vector<CTNode>> updated_nodes_;
        unordered_map<size_t, vector<CompressedTrie>> updated_tries_;
    };

    class TestCachedStorage : public storage::Storage {
    public:
        TestCachedStorage(shared_ptr<storage::Storage> backing_storage, size_t cache_size)
            : storage_(backing_storage, cache_size)
        {}

        bool load_ctnode(
            const std::vector<std::byte> &trie_id,
            const partial_label_type &node_id,
            CTNode &node) override
        {
            return storage_.load_ctnode(trie_id, node_id, node);
        }

        void save_ctnode(const std::vector<std::byte> &trie_id, const CTNode &node) override
        {
            storage_.save_ctnode(trie_id, node);
        }

        bool load_compressed_trie(
            const std::vector<std::byte> &trie_id, CompressedTrie &trie) override
        {
            return storage_.load_compressed_trie(trie_id, trie);
        }

        void save_compressed_trie(const CompressedTrie &trie) override
        {
            storage_.save_compressed_trie(trie);
        }

        bool load_ozks(const std::vector<std::byte> &trie_id, OZKS &ozks) override
        {
            return storage_.load_ozks(trie_id, ozks);
        }

        void save_ozks(const OZKS &ozks) override
        {
            storage_.save_ozks(ozks);
        }

        bool load_store_element(
            const std::vector<std::byte> &trie_id,
            const std::vector<std::byte> &key,
            store_value_type &value) override
        {
            return storage_.load_store_element(trie_id, key, value);
        }

        void save_store_element(
            const std::vector<std::byte> &trie_id,
            const std::vector<std::byte> &key,
            const store_value_type &value) override
        {
            storage_.save_store_element(trie_id, key, value);
        }

        void flush(const vector<byte>&) override
        {
            // Nothing really.
        }

        void add_ctnode(const vector<byte> &trie_id, const CTNode &node) override
        {
            storage_.add_ctnode(trie_id, node);
            added_nodes_.push_back(node);
        }

        void add_compressed_trie(const CompressedTrie& trie) override
        {
            storage_.add_compressed_trie(trie);
            added_tries_.push_back(trie);
        }

        void add_store_element(
            const vector<byte> &trie_id,
            const vector<byte> &key,
            const store_value_type &value) override
        {
            storage_.add_store_element(trie_id, key, value);
        }

        size_t get_compressed_trie_epoch(const vector<byte> &trie_id) override
        {
            return storage_.get_compressed_trie_epoch(trie_id);
        }

        void load_updated_elements(
            size_t epoch, const vector<byte> &trie_id, Storage*) override
        {
            storage_.load_updated_elements(epoch, trie_id, this);
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
        vector<CTNode> added_nodes_;
        vector<CompressedTrie> added_tries_;
    };

} // namespace

void RandomInsertTestCore(OZKS &ozks, size_t iterations, bool flush_at_end = false)
{
    key_type key(16);
    payload_type payload(40);
    vector<key_type> valid_keys;
    vector<payload_type> valid_payloads;
    vector<shared_ptr<InsertResult>> insert_results;

    for (size_t i = 0; i < iterations; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());

        auto insert_result = ozks.insert(key, payload);
        if (!flush_at_end) {
            ozks.flush();
            EXPECT_EQ(true, insert_result->verify());
        } else {
            insert_results.push_back(insert_result);
        }

        // Add some keys at random to check later (up to 100)
        unsigned char c;
        get_random_bytes(&c, 1);
        if (valid_keys.size() < 100 && c > 128) {
            valid_keys.push_back(key);
            valid_payloads.push_back(payload);
        }
    }

    if (flush_at_end) {
        ozks.flush();
        for (auto insert_result : insert_results) {
            EXPECT_EQ(true, insert_result->verify());
        }
    }

    // Check the valid keys are found and that their path is verified correctly
    for (size_t i = 0; i < valid_keys.size(); i++) {
        auto result = ozks.query(valid_keys[i]);
        EXPECT_TRUE(result.is_member());
        EXPECT_NE(0, result.payload().size());
        EXPECT_TRUE(result.verify(valid_keys[i], ozks.get_commitment()));
        EXPECT_EQ(valid_payloads[i], result.payload());
    }

    // Check that invalid keys are not found and that their path is verified correctly
    for (size_t i = 0; i < 1000; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());

        auto result = ozks.query(key);
        EXPECT_FALSE(result.is_member());
        EXPECT_EQ(0, result.payload().size());
        EXPECT_TRUE(result.verify(key, ozks.get_commitment()));
    }
}

void RandomInsertTestCore(shared_ptr<storage::Storage> storage, size_t iterations, bool flush_at_end = false)
{
    OZKS ozks(storage);
    RandomInsertTestCore(ozks, iterations, flush_at_end);
}

TEST(OZKSTests, InsertTest)
{
    OZKS ozks;

    auto key = make_bytes(0x01, 0x02, 0x03);
    auto payload = make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);

    EXPECT_EQ(0, ozks.get_epoch());

    auto result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(1, ozks.get_epoch());
    EXPECT_NE(0, result->commitment().size());   // Should have commitment
    EXPECT_EQ(1, result->append_proof().size()); // First inserted

    commitment_type commitment1 = result->commitment();

    key = make_bytes(0x02, 0x03, 0x04);
    payload = make_bytes(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

    result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(2, ozks.get_epoch());
    EXPECT_NE(0, result->commitment().size());
    EXPECT_NE(commitment1, result->commitment());
    EXPECT_EQ(2, result->append_proof().size()); // Sibling
}

TEST(OZKSTests, NoRandomInsertTest)
{
    OZKSConfig config{ false, false };
    OZKS ozks({}, config);

    auto key = make_bytes(0x01, 0x02, 0x03);
    auto payload = make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);

    auto result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_NE(0, result->commitment().size());   // Should have commitment
    EXPECT_EQ(1, result->append_proof().size()); // First inserted

    commitment_type commitment1 = result->commitment();

    key = make_bytes(0x02, 0x03, 0x04);
    payload = make_bytes(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

    result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_NE(0, result->commitment().size());
    EXPECT_NE(commitment1, result->commitment());
    EXPECT_EQ(2, result->append_proof().size()); // Sibling
}

TEST(OZKSTests, InsertBatchTest)
{
    OZKS ozks;

    auto key = make_bytes(0x01, 0x01, 0x01);
    auto payload = make_bytes(0x01, 0x02, 0x03, 0x04, 0x05, 0x06);

    EXPECT_EQ(0, ozks.get_epoch());

    auto result_single = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(1, ozks.get_epoch());
    EXPECT_EQ(1, result_single->append_proof().size());

    commitment_type commitment = result_single->commitment();

    key_payload_batch_type batch{
        pair<key_type, payload_type>{ make_bytes(0x01, 0x02, 0x03),
                                      make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA) },
        pair<key_type, payload_type>{ make_bytes(0x02, 0x03, 0x04),
                                      make_bytes(0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9) },
        pair<key_type, payload_type>{ make_bytes(0x03, 0x04, 0x05),
                                      make_bytes(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8) },
        pair<key_type, payload_type>{ make_bytes(0x04, 0x05, 0x06),
                                      make_bytes(0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7) },
        pair<key_type, payload_type>{ make_bytes(0x05, 0x06, 0x07),
                                      make_bytes(0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6) },
        pair<key_type, payload_type>{ make_bytes(0x06, 0x07, 0x08),
                                      make_bytes(0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5) }
    };

    auto result = ozks.insert(batch);
    ozks.flush();

    EXPECT_EQ(2, ozks.get_epoch());
    EXPECT_EQ(6, result.size());
    EXPECT_NE(commitment, result[0]->commitment());
    EXPECT_EQ(result[0]->commitment(), result[1]->commitment());
    EXPECT_EQ(result[1]->commitment(), result[2]->commitment());
    EXPECT_EQ(result[2]->commitment(), result[3]->commitment());
    EXPECT_EQ(result[3]->commitment(), result[4]->commitment());
    EXPECT_EQ(result[5]->commitment(), result[4]->commitment());

    EXPECT_GT(result[0]->append_proof().size(), 0);
    EXPECT_GT(result[1]->append_proof().size(), 0);
    EXPECT_GT(result[2]->append_proof().size(), 0);
    EXPECT_GT(result[3]->append_proof().size(), 0);
    EXPECT_GT(result[4]->append_proof().size(), 0);
    EXPECT_GT(result[5]->append_proof().size(), 0);
}

TEST(OZKSTests, QueryTest)
{
    OZKS ozks;

    auto key = make_bytes(0x01, 0x01, 0x01);
    auto payload = make_bytes(0x01, 0x02, 0x03, 0x04, 0x05, 0x06);

    auto result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(1, result->append_proof().size());

    QueryResult query_result = ozks.query(key);
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(payload, query_result.payload());

    VRFPublicKey pk = ozks.get_public_key();
    hash_type hash;
    EXPECT_EQ(true, pk.verify_proof(key, query_result.vrf_proof(), hash));

    key = make_bytes(0x01, 0x01, 0x00);

    query_result = ozks.query(key);
    EXPECT_EQ(false, query_result.is_member());
    EXPECT_EQ(0, query_result.payload().size());
    EXPECT_EQ(0, query_result.randomness().size());
    EXPECT_NE(0, query_result.lookup_proof().size());
}

TEST(OZKSTests, MultiInsertQueryTest)
{
    OZKS ozks;

    key_payload_batch_type batch{
        pair<key_type, payload_type>{ make_bytes(0x01, 0x02, 0x03),
                                      make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA) },
        pair<key_type, payload_type>{ make_bytes(0x02, 0x03, 0x04),
                                      make_bytes(0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9) },
        pair<key_type, payload_type>{ make_bytes(0x03, 0x04, 0x05),
                                      make_bytes(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8) },
        pair<key_type, payload_type>{ make_bytes(0x04, 0x05, 0x06),
                                      make_bytes(0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7) },
        pair<key_type, payload_type>{ make_bytes(0x05, 0x06, 0x07),
                                      make_bytes(0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6) },
        pair<key_type, payload_type>{ make_bytes(0x06, 0x07, 0x08),
                                      make_bytes(0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5) }
    };

    auto result = ozks.insert(batch);
    ozks.flush();

    // Check inserts are verified
    for (const auto &single_result : result) {
        EXPECT_TRUE(single_result->verify());
    }

    auto key = make_bytes(0x03, 0x04, 0x05);
    auto payload = make_bytes(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8);
    QueryResult query_result = ozks.query(key);
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(payload, query_result.payload());

    VRFPublicKey pk = ozks.get_public_key();
    hash_type hash;
    EXPECT_EQ(true, pk.verify_proof(key, query_result.vrf_proof(), hash));

    key = make_bytes(0x06, 0x07, 0x08);
    payload = make_bytes(0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5);
    query_result = ozks.query(key);
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(true, pk.verify_proof(key, query_result.vrf_proof(), hash));

    key = make_bytes(0x06, 0x07, 0x00);
    query_result = ozks.query(key);
    EXPECT_EQ(false, query_result.is_member());
    EXPECT_EQ(0, query_result.payload().size());
    EXPECT_EQ(0, query_result.randomness().size());
}

TEST(OZKSTests, InsertResultVerificationTest)
{
    OZKS ozks;

    auto key = make_bytes(0x01, 0x02, 0x03, 0x04);
    auto payload = make_bytes(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

    auto insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes(0x02, 0x03, 0x04, 0x05);
    payload = make_bytes(0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes(0x03, 0x03, 0x04, 0x05);
    payload = make_bytes(0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes(0xFF, 0xFE, 0xFD, 0xFC);
    payload = make_bytes(0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes(0xF0, 0xF1, 0xF2, 0xF3);
    payload = make_bytes(0xA2, 0xB3, 0xC4, 0xD5, 0xE6, 0xF7);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes(0xF1, 0xF2, 0xF3, 0xF4);
    payload = make_bytes(0xA3, 0xB4, 0xC5, 0xD6, 0xE7, 0xF8);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes(0xF2, 0xF3, 0xF4, 0xF5);
    payload = make_bytes(0xA4, 0xB5, 0xC6, 0xD7, 0xE8, 0xF9);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes(0xF3, 0xF4, 0xF5, 0xF6);
    payload = make_bytes(0xA5, 0xB6, 0xC7, 0xD8, 0xE9, 0xFA);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());
}

TEST(OZKSTests, RandomInsertVerificationTest)
{
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    RandomInsertTestCore(storage, random_iterations);
}

TEST(OZKSTests, RandomInsertVerificationCacheTest)
{
    shared_ptr<ozks::storage::Storage> backing_storage =
        make_shared<ozks::storage::MemoryStorage>();
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorageCache>(
        backing_storage, /* cache_size */ random_iterations);

    RandomInsertTestCore(storage, random_iterations);
}

TEST(OZKSTests, RandomInsertVerificationSmallerCacheTest)
{
    shared_ptr<ozks::storage::Storage> backing_storage =
        make_shared<ozks::storage::MemoryStorage>();
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorageCache>(
        backing_storage, /* cache_size */ random_iterations / 4);

    RandomInsertTestCore(storage, random_iterations);
}

TEST(OZKSTests, RandomInsertVerificationBatchInserterTest)
{
    shared_ptr<ozks::storage::BatchStorage> backing_storage = make_shared<TestBackingStorage>();
    shared_ptr<ozks::storage::Storage> batching_storage =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);

    TestBackingStorage *tbs = dynamic_cast<TestBackingStorage *>(backing_storage.get());
    EXPECT_NE(nullptr, tbs);

    OZKS ozks(batching_storage);

    key_type key(16);
    payload_type payload(40);
    vector<key_type> valid_keys;
    vector<payload_type> valid_payloads;
    vector<shared_ptr<InsertResult>> insert_results;

    for (size_t i = 0; i < random_iterations; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());

        auto insert_result = ozks.insert(key, payload);
        insert_results.push_back(insert_result);

        // Add some keys at random to check later (up to 100)
        unsigned char c;
        get_random_bytes(&c, 1);
        if (valid_keys.size() < 100 && c > 128) {
            valid_keys.push_back(key);
            valid_payloads.push_back(payload);
        }
    }

    // At this point memory storage should be empty
    EXPECT_EQ(0, tbs->node_count());
    EXPECT_EQ(0, tbs->store_element_count());
    EXPECT_EQ(0, tbs->trie_count());
    EXPECT_EQ(0, tbs->ozks_count());

    ozks.flush();

    // Now memory storage should have everything
    EXPECT_GE(tbs->node_count(), random_iterations);
    EXPECT_EQ(random_iterations, tbs->store_element_count());
    EXPECT_EQ(1, tbs->trie_count());
    EXPECT_EQ(0, tbs->ozks_count());

    // And we can verify results
    for (auto insert_result : insert_results) {
        EXPECT_EQ(true, insert_result->verify());
    }

    // Check the valid keys are found and that their path is verified correctly
    for (size_t i = 0; i < valid_keys.size(); i++) {
        auto result = ozks.query(valid_keys[i]);
        EXPECT_TRUE(result.is_member());
        EXPECT_NE(0, result.payload().size());
        EXPECT_TRUE(result.verify(valid_keys[i], ozks.get_commitment()));
        EXPECT_EQ(valid_payloads[i], result.payload());
    }

    // Check that invalid keys are not found and that their path is verified correctly
    for (size_t i = 0; i < 1000; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());

        auto result = ozks.query(key);
        EXPECT_FALSE(result.is_member());
        EXPECT_EQ(0, result.payload().size());
        EXPECT_TRUE(result.verify(key, ozks.get_commitment()));
    }
}

TEST(OZKSTests, RandomMultiInsertVerificationTest)
{
    OZKS ozks;
    key_type key(16);
    payload_type payload(40);
    vector<key_type> valid_keys;

    key_payload_batch_type insertions;

    for (size_t i = 0; i < random_iterations; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());

        insertions.push_back({ key, payload });

        unsigned char c;
        get_random_bytes(&c, 1);
        if (valid_keys.size() < 100 && c > 128) {
            valid_keys.push_back(key);
        }
    }

    auto insert_result = ozks.insert(insertions);
    ozks.flush();

    // Check all insertions are verified
    for (size_t i = 0; i < insert_result.size(); i++) {
        auto &single_result = insert_result[i];
        EXPECT_TRUE(single_result->verify());
    }

    // Check the valid keys are found and that their path is verified correctly
    for (size_t i = 0; i < valid_keys.size(); i++) {
        auto result = ozks.query(valid_keys[i]);
        EXPECT_TRUE(result.is_member());
        EXPECT_NE(0, result.payload().size());
        EXPECT_TRUE(result.verify(valid_keys[i], ozks.get_commitment()));
    }

    // Check that invalid keys are not found and that their path is verified correctly
    for (size_t i = 0; i < 1000; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());

        auto result = ozks.query(key);
        EXPECT_FALSE(result.is_member());
        EXPECT_EQ(0, result.payload().size());
        EXPECT_TRUE(result.verify(key, ozks.get_commitment()));
    }
}

TEST(OZKSTests, QueryResultVerificationTest)
{
    OZKS ozks;

    key_payload_batch_type batch{
        pair<key_type, payload_type>{ make_bytes(0x01, 0x02, 0x03),
                                      make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA) },
        pair<key_type, payload_type>{ make_bytes(0x02, 0x03, 0x04),
                                      make_bytes(0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9) },
        pair<key_type, payload_type>{ make_bytes(0x03, 0x04, 0x05),
                                      make_bytes(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8) },
        pair<key_type, payload_type>{ make_bytes(0x04, 0x05, 0x06),
                                      make_bytes(0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7) },
        pair<key_type, payload_type>{ make_bytes(0x05, 0x06, 0x07),
                                      make_bytes(0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6) },
        pair<key_type, payload_type>{ make_bytes(0x06, 0x07, 0x08),
                                      make_bytes(0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5) }
    };

    auto result = ozks.insert(batch);
    ozks.flush();
    EXPECT_EQ(6, result.size());

    auto key = make_bytes(0x03, 0x04, 0x05);
    auto payload = make_bytes(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8);
    QueryResult query_result = ozks.query(key);
    Commitment commitment = ozks.get_commitment();
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(payload, query_result.payload());
    EXPECT_EQ(true, query_result.verify(key, commitment));

    key = make_bytes(0x04, 0x05, 0x06);
    payload = make_bytes(0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7);
    query_result = ozks.query(key);
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(payload, query_result.payload());
    EXPECT_EQ(true, query_result.verify(key, commitment));

    key = make_bytes(0x02, 0x03, 0x05);
    query_result = ozks.query(key);
    EXPECT_EQ(false, query_result.is_member());
    EXPECT_EQ(0, query_result.payload().size());
    EXPECT_EQ(0, query_result.randomness().size());
    EXPECT_EQ(true, query_result.verify(key, commitment));
}

TEST(OZKSTests, SaveLoadTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    OZKS ozks(storage);

    key_type key(40);
    payload_type payload(40);
    vector<key_type> some_keys(100);

    for (size_t i = 0; i < 10000; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());

        ozks.insert(key, payload);

        if (i < some_keys.size()) {
            some_keys[i] = key;
        }
    }

    ozks.flush();

    stringstream ss;
    size_t save_size = ozks.save(ss);

    OZKS ozks2(storage);
    size_t load_size = OZKS::Load(ozks2, ss);

    EXPECT_EQ(load_size, save_size);

    for (size_t i = 0; i < some_keys.size(); i++) {
        QueryResult qr1 = ozks.query(some_keys[i]);
        QueryResult qr2 = ozks2.query(some_keys[i]);

        EXPECT_TRUE(qr1.is_member());
        EXPECT_TRUE(qr2.is_member());
        EXPECT_EQ(qr1.payload(), qr2.payload());
        EXPECT_EQ(qr1.randomness(), qr2.randomness());
        EXPECT_EQ(qr1.lookup_proof().size(), qr2.lookup_proof().size());

        EXPECT_TRUE(qr1.verify(some_keys[i], ozks2.get_commitment()));
        EXPECT_TRUE(qr2.verify(some_keys[i], ozks.get_commitment()));
    }
}

TEST(OZKSTests, NonRandomSaveLoadTest)
{
    OZKSConfig config{ false, false };
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    OZKS ozks(storage, config);

    EXPECT_EQ(false, ozks.get_configuration().payload_randomness());
    EXPECT_EQ(false, ozks.get_configuration().include_vrf());

    key_type key(40);
    payload_type payload(40);
    vector<key_type> some_keys(100);

    for (size_t i = 0; i < 10000; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());

        ozks.insert(key, payload);

        if (i < some_keys.size()) {
            some_keys[i] = key;
        }
    }

    ozks.flush();

    stringstream ss;
    size_t save_size = ozks.save(ss);

    OZKS ozks2(storage);
    EXPECT_EQ(true, ozks2.get_configuration().payload_randomness());
    EXPECT_EQ(true, ozks2.get_configuration().include_vrf());

    size_t load_size = OZKS::Load(ozks2, ss);
    EXPECT_EQ(false, ozks2.get_configuration().payload_randomness());
    EXPECT_EQ(false, ozks2.get_configuration().include_vrf());

    EXPECT_EQ(load_size, save_size);

    for (size_t i = 0; i < some_keys.size(); i++) {
        QueryResult qr1 = ozks.query(some_keys[i]);
        QueryResult qr2 = ozks2.query(some_keys[i]);

        EXPECT_TRUE(qr1.is_member());
        EXPECT_TRUE(qr2.is_member());
        EXPECT_EQ(qr1.payload(), qr2.payload());
        EXPECT_EQ(qr1.randomness(), qr2.randomness());
        EXPECT_EQ(qr1.lookup_proof().size(), qr2.lookup_proof().size());

        EXPECT_TRUE(qr1.verify(some_keys[i], ozks2.get_commitment()));
        EXPECT_TRUE(qr2.verify(some_keys[i], ozks.get_commitment()));
    }
}

TEST(OZKSTests, SaveLoadToVectorTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    OZKS ozks(storage);

    key_type key(40);
    payload_type payload(40);
    vector<key_type> some_keys(100);

    for (size_t i = 0; i < 10000; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());

        ozks.insert(key, payload);

        if (i < some_keys.size()) {
            some_keys[i] = key;
        }
    }

    ozks.flush();

    vector<byte> vec;
    size_t save_size = ozks.save(vec);

    OZKS ozks2(storage);
    size_t load_size = OZKS::Load(ozks2, vec);

    EXPECT_EQ(load_size, save_size);

    for (size_t i = 0; i < some_keys.size(); i++) {
        QueryResult qr1 = ozks.query(some_keys[i]);
        QueryResult qr2 = ozks2.query(some_keys[i]);

        EXPECT_TRUE(qr1.is_member());
        EXPECT_TRUE(qr2.is_member());
        EXPECT_EQ(qr1.payload(), qr2.payload());
        EXPECT_EQ(qr1.randomness(), qr2.randomness());
        EXPECT_EQ(qr1.lookup_proof().size(), qr2.lookup_proof().size());

        EXPECT_TRUE(qr1.verify(some_keys[i], ozks2.get_commitment()));
        EXPECT_TRUE(qr2.verify(some_keys[i], ozks.get_commitment()));
    }
}

TEST(OZKSTests, LoadSaveToStorageTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    OZKS ozks(storage);

    key_type key(40);
    payload_type payload(40);
    vector<key_type> some_keys(100);

    for (size_t i = 0; i < 10000; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());

        ozks.insert(key, payload);

        if (i < some_keys.size()) {
            some_keys[i] = key;
        }
    }

    ozks.flush();
    ozks.save();

    OZKS ozks2(storage);
    EXPECT_TRUE(OZKS::Load(ozks.id(), storage, ozks2));

    for (size_t i = 0; i < some_keys.size(); i++) {
        QueryResult qr1 = ozks.query(some_keys[i]);
        QueryResult qr2 = ozks2.query(some_keys[i]);

        EXPECT_TRUE(qr1.is_member());
        EXPECT_TRUE(qr2.is_member());
        EXPECT_EQ(qr1.payload(), qr2.payload());
        EXPECT_EQ(qr1.randomness(), qr2.randomness());
        EXPECT_EQ(qr1.lookup_proof().size(), qr2.lookup_proof().size());

        EXPECT_TRUE(qr1.verify(some_keys[i], ozks2.get_commitment()));
        EXPECT_TRUE(qr2.verify(some_keys[i], ozks.get_commitment()));
    }
}

TEST(OZKSTests, EmptyOZKSTest)
{
    OZKS ozks1;
    OZKS ozks2;

    Commitment comm1 = ozks1.get_commitment();
    Commitment comm2 = ozks2.get_commitment();

    EXPECT_EQ(comm1.root_commitment(), comm2.root_commitment());
}

TEST(OZKSTests, ConfigurationTest)
{
    OZKSConfig config{ false, false }; // No commit randomness, do not include vrf

    OZKS ozks1({}, config);
    OZKS ozks2({}, config);

    EXPECT_EQ(false, ozks1.get_configuration().payload_randomness());
    EXPECT_EQ(false, ozks2.get_configuration().payload_randomness());

    Commitment comm1 = ozks1.get_commitment();
    Commitment comm2 = ozks2.get_commitment();

    EXPECT_EQ(comm1.root_commitment(), comm2.root_commitment());

    key_type key = make_bytes(0x1, 0x2, 0x3);
    payload_type payload = make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);

    ozks1.insert(key, payload);
    ozks2.insert(key, payload);
    ozks1.flush();
    ozks2.flush();

    comm1 = ozks1.get_commitment();
    comm2 = ozks2.get_commitment();

    EXPECT_EQ(comm1.root_commitment(), comm2.root_commitment());

    OZKSConfig config2{ true, false }; // Commit randomness, do not include vrf
    EXPECT_EQ(false, config2.include_vrf());
    EXPECT_EQ(true, config2.payload_randomness());

    OZKS ozks3({}, config2);
    OZKS ozks4({}, config2);

    comm1 = ozks3.get_commitment();
    comm2 = ozks4.get_commitment();

    EXPECT_EQ(comm1.root_commitment(), comm2.root_commitment());

    ozks3.insert(key, payload);
    ozks4.insert(key, payload);
    ozks3.flush();
    ozks4.flush();

    comm1 = ozks3.get_commitment();
    comm2 = ozks4.get_commitment();

    // Not equal because the payload is used to compute leaf hashes
    EXPECT_NE(comm1.root_commitment(), comm2.root_commitment());
}

TEST(OZKSTests, ConstructorTest)
{
    OZKS ozks1;

    EXPECT_EQ(true, ozks1.get_configuration().payload_randomness());
    EXPECT_EQ(true, ozks1.get_configuration().include_vrf());

    OZKSConfig config{ false, false };
    OZKS ozks2({}, config);

    EXPECT_EQ(false, ozks2.get_configuration().payload_randomness());
    EXPECT_EQ(false, ozks2.get_configuration().include_vrf());

    config = { true, false };
    OZKS ozks3({}, config);

    EXPECT_EQ(true, ozks3.get_configuration().payload_randomness());
    EXPECT_EQ(false, ozks3.get_configuration().include_vrf());

    config = { false, true };
    OZKS ozks4({}, config);

    EXPECT_EQ(false, ozks4.get_configuration().payload_randomness());
    EXPECT_EQ(true, ozks4.get_configuration().include_vrf());
}

TEST(OZKSTests, UpdatedNodesTest)
{
    shared_ptr<TestBackingStorage> backing_storage = make_shared<TestBackingStorage>();
    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);

    OZKS ozks(batch_inserter);

    vector<byte> saved_ozks;
    ozks.save(saved_ozks);

    // Insert one element to ensure trie is saved in backing storage
    RandomInsertTestCore(ozks, 1, /* flush_at_end */ true);

    shared_ptr<TestCachedStorage> cached_storage =
        make_shared<TestCachedStorage>(backing_storage, 5000);
    OZKS ozks2(cached_storage);
    // This ensures we have the same trie ID
    ozks2.Load(ozks2, saved_ozks);
    // This ensures the trie is in the cache
    size_t epoch = ozks2.get_epoch();

    // Generate 3 epochs
    EXPECT_EQ(1, backing_storage->updated_nodes_count());
    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(2, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(3, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(4, backing_storage->updated_nodes_count());

    epoch = ozks2.get_epoch();
    EXPECT_EQ(1, epoch);

    ozks2.check_for_update();
    epoch = ozks2.get_epoch();
    EXPECT_EQ(4, epoch);
    EXPECT_EQ(0, backing_storage->updated_nodes_count());
}

TEST(OZKSTests, BatchInserterCallbackTest)
{
    shared_ptr<TestBackingStorage> backing_storage = make_shared<TestBackingStorage>();
    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);

    OZKS ozks(batch_inserter);

    vector<byte> saved_ozks;
    ozks.save(saved_ozks);

    // Insert one element to ensure trie is saved in backing storage
    RandomInsertTestCore(ozks, 1, /* flush_at_end */ true);

    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter2 =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);

    OZKS ozks2(batch_inserter2);
    // This ensures we have the same trie ID
    ozks2.Load(ozks2, saved_ozks);

    // Generate 3 epochs
    EXPECT_EQ(1, backing_storage->updated_nodes_count());
    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(2, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(3, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(4, backing_storage->updated_nodes_count());

    size_t epoch = ozks2.get_epoch();
    EXPECT_EQ(4, epoch); // Since we get it from backing store

    // This should not throw
    ozks2.check_for_update();
    epoch = ozks2.get_epoch();
    EXPECT_EQ(4, epoch);
    EXPECT_EQ(4, backing_storage->updated_nodes_count()); // Since no updated nodes were retrieved
}

TEST(OZKSTests, BatchInserterCallback2Test)
{
    shared_ptr<TestBackingStorage> backing_storage = make_shared<TestBackingStorage>();
    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);

    OZKS ozks(batch_inserter);

    vector<byte> saved_ozks;
    ozks.save(saved_ozks);

    // Insert one element to ensure trie is saved in backing storage
    RandomInsertTestCore(ozks, 1, /* flush_at_end */ true);

    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter2 =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);
    shared_ptr<TestCachedStorage> cached_storage =
        make_shared<TestCachedStorage>(batch_inserter2, 10000);

    OZKS ozks2(cached_storage);
    // This ensures we have the same trie ID
    ozks2.Load(ozks2, saved_ozks);
    // This ensures the trie is in the cache
    size_t epoch = ozks2.get_epoch();

    // Generate 3 epochs
    EXPECT_EQ(1, backing_storage->updated_nodes_count());
    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(2, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(3, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(4, backing_storage->updated_nodes_count());

    epoch = ozks2.get_epoch();
    EXPECT_EQ(1, epoch); // Since we get it from the cache

    // This should not throw
    EXPECT_EQ(0, cached_storage->added_nodes_count());
    EXPECT_EQ(0, cached_storage->added_tries_count());
    ozks2.check_for_update();
    epoch = ozks2.get_epoch();
    EXPECT_EQ(4, epoch);
    EXPECT_EQ(0, backing_storage->updated_nodes_count()); // Updated nodes should have been retrieved
    EXPECT_GT(cached_storage->added_nodes_count(), 0);
    EXPECT_GT(cached_storage->added_tries_count(), 0);
}