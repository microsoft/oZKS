// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "../ozks.h"
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
using namespace ozks_simple;

constexpr size_t random_iterations = 50000;

namespace {
    /**
    Get random bytes and throw if unsuccessful
    */
    void get_random_bytes(byte *dest, size_t size)
    {
        if (!utils::random_bytes(dest, size))
            throw runtime_error("Failed to get random bytes");
    }

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
            //const vector<OZKS> &ozks,
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

            vector<CTNodeStored> existing_updated_nodes;
            auto it = updated_nodes_.find(updated_epoch);
            if (it != updated_nodes_.end()) {
                existing_updated_nodes = it->second;
            }

            for (auto &node : nodes) {
                storage_.save_ctnode(trie_id, node);
                existing_updated_nodes.push_back(node);
            }

            updated_nodes_[updated_epoch] = existing_updated_nodes;

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

} // namespace

vector<key_type> RandomInsertTestCore(OZKS &ozks, size_t iterations, bool flush_at_end = false)
{
    key_type key(16);
    payload_type payload(40);
    vector<key_type> valid_keys;
    vector<payload_type> valid_payloads;
    vector<shared_ptr<InsertResult>> insert_results;

    for (size_t i = 0; i < iterations; i++) {
        get_random_bytes(key.data(), key.size());
        get_random_bytes(payload.data(), payload.size());

        auto insert_result = ozks.insert(key, payload);
        if (!flush_at_end) {
            ozks.flush();
            EXPECT_EQ(true, insert_result->verify());
        } else {
            insert_results.push_back(insert_result);
        }

        // Add some keys at random to check later (up to 100)
        byte b;
        get_random_bytes(&b, 1);
        if (valid_keys.size() < 100 && static_cast<int>(b) > 128) {
            valid_keys.push_back(key);
            valid_payloads.push_back(payload);
        }
    }

    if (flush_at_end) {
        ozks.flush();
        for (auto &insert_result : insert_results) {
            EXPECT_EQ(true, insert_result->verify());
        }
    }

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

void RandomInsertCore(OZKS &ozks, size_t iterations, vector<key_type> *valid_keys_ptr = nullptr)
{
    key_type key(16);
    payload_type payload(40);
    vector<key_type> valid_keys;
    vector<payload_type> valid_payloads;

    key_payload_batch_type insertions;

    for (size_t i = 0; i < iterations; i++) {
        get_random_bytes(key.data(), key.size());
        get_random_bytes(payload.data(), payload.size());

        insertions.push_back({ key, payload });

        byte c;
        get_random_bytes(&c, 1);
        if (valid_keys.size() < 100 && c > static_cast<byte>(128)) {
            valid_keys.push_back(key);
            valid_payloads.push_back(payload);
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
        EXPECT_TRUE(result.verify(ozks.get_commitment()));
        EXPECT_EQ(valid_payloads[i], result.payload());
    }

    // Check that invalid keys are not found and that their path is verified correctly
    for (size_t i = 0; i < 10; i++) {
        get_random_bytes(key.data(), key.size());

        auto result = ozks.query(key);
        EXPECT_FALSE(result.is_member());
        EXPECT_EQ(0, result.payload().size());
        EXPECT_TRUE(result.verify(ozks.get_commitment()));
    }

    if (nullptr != valid_keys_ptr) {
        for (const auto &valid_key : valid_keys) {
            valid_keys_ptr->push_back(valid_key);
        }
    }
}

vector<key_type> RandomInsertTestCore(
    shared_ptr<storage::Storage> storage, size_t iterations, bool flush_at_end = false)
{
    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    OZKS ozks(config);
    return RandomInsertTestCore(ozks, iterations, flush_at_end);
}

vector<key_type> RandomInsertTestCore(
    TrieType trie_type, size_t iterations, bool flush_at_end = false)
{
    auto storage = make_shared<storage::MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, trie_type, storage
    };
    OZKS ozks(config);
    return RandomInsertTestCore(ozks, iterations, flush_at_end);
}

TEST(OZKSTests, InsertTest)
{
    OZKS ozks;

    auto key = make_bytes<key_type>(0x01, 0x02, 0x03);
    auto payload = make_bytes<payload_type>(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);

    EXPECT_EQ(0, ozks.get_epoch());

    auto result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(1, ozks.get_epoch());
    EXPECT_NE(0, result->commitment().size());   // Should have commitment
    EXPECT_EQ(1, result->append_proof().size()); // First inserted

    commitment_type commitment1 = result->commitment();

    key = make_bytes<key_type>(0x02, 0x03, 0x04);
    payload = make_bytes<payload_type>(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

    result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(2, ozks.get_epoch());
    EXPECT_NE(0, result->commitment().size());
    EXPECT_NE(commitment1, result->commitment());
    EXPECT_EQ(2, result->append_proof().size()); // Sibling
}

TEST(OZKSTests, NoRandomInsertTest)
{
    auto storage = make_shared<storage::MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::UncommitedPayload, LabelType::HashedLabels, TrieType::Stored, storage
    };
    OZKS ozks(config);

    auto key = make_bytes<key_type>(0x01, 0x02, 0x03);
    auto payload = make_bytes<payload_type>(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);

    auto result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_NE(0, result->commitment().size());   // Should have commitment
    EXPECT_EQ(1, result->append_proof().size()); // First inserted

    commitment_type commitment1 = result->commitment();

    key = make_bytes<key_type>(0x02, 0x03, 0x04);
    payload = make_bytes<payload_type>(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

    result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_NE(0, result->commitment().size());
    EXPECT_NE(commitment1, result->commitment());
    EXPECT_EQ(2, result->append_proof().size()); // Sibling
}

TEST(OZKSTests, InsertBatchTest)
{
    OZKS ozks;

    auto key = make_bytes<key_type>(0x01, 0x01, 0x01);
    auto payload = make_bytes<payload_type>(0x01, 0x02, 0x03, 0x04, 0x05, 0x06);

    EXPECT_EQ(0, ozks.get_epoch());

    auto result_single = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(1, ozks.get_epoch());
    EXPECT_EQ(1, result_single->append_proof().size());

    commitment_type commitment = result_single->commitment();

    key_payload_batch_type batch{
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x01, 0x02, 0x03),
            make_bytes<payload_type>(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x02, 0x03, 0x04),
            make_bytes<payload_type>(0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x03, 0x04, 0x05),
            make_bytes<payload_type>(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x04, 0x05, 0x06),
            make_bytes<payload_type>(0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x05, 0x06, 0x07),
            make_bytes<payload_type>(0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6) },
        pair<key_type, payload_type>{ make_bytes<key_type>(0x06, 0x07, 0x08),
                                      make_bytes<payload_type>(0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5) }
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

    auto key = make_bytes<key_type>(0x01, 0x01, 0x01);
    auto payload = make_bytes<payload_type>(0x01, 0x02, 0x03, 0x04, 0x05, 0x06);

    auto result = ozks.insert(key, payload);
    ozks.flush();

    Commitment commitment = ozks.get_commitment();

    EXPECT_EQ(1, result->append_proof().size());

    QueryResult query_result = ozks.query(key);
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(payload, query_result.payload());

    VRFPublicKey pk = ozks.get_vrf_public_key();
    EXPECT_EQ(true, pk.verify_vrf_proof(key, query_result.vrf_proof()));

    key = make_bytes<key_type>(0x01, 0x01, 0x00);

    query_result = ozks.query(key);
    EXPECT_EQ(false, query_result.is_member());
    EXPECT_EQ(0, query_result.payload().size());
    EXPECT_EQ(randomness_size, query_result.randomness().size());
    EXPECT_NE(0, query_result.lookup_proof().size());
    EXPECT_TRUE(query_result.verify(commitment));
}

TEST(OZKSTests, MultiInsertQueryTest)
{
    OZKS ozks;

    key_payload_batch_type batch{
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x01, 0x02, 0x03),
            make_bytes<payload_type>(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x02, 0x03, 0x04),
            make_bytes<payload_type>(0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x03, 0x04, 0x05),
            make_bytes<payload_type>(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x04, 0x05, 0x06),
            make_bytes<payload_type>(0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x05, 0x06, 0x07),
            make_bytes<payload_type>(0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6) },
        pair<key_type, payload_type>{ make_bytes<key_type>(0x06, 0x07, 0x08),
                                      make_bytes<payload_type>(0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5) }
    };

    auto result = ozks.insert(batch);
    ozks.flush();

    Commitment commitment = ozks.get_commitment();

    // Check inserts are verified
    for (const auto &single_result : result) {
        EXPECT_TRUE(single_result->verify());
    }

    auto key = make_bytes<key_type>(0x03, 0x04, 0x05);
    auto payload = make_bytes<payload_type>(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8);
    QueryResult query_result = ozks.query(key);
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(payload, query_result.payload());

    VRFPublicKey pk = ozks.get_vrf_public_key();
    EXPECT_EQ(true, pk.verify_vrf_proof(key, query_result.vrf_proof()));

    key = make_bytes<key_type>(0x06, 0x07, 0x08);
    payload = make_bytes<payload_type>(0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5);
    query_result = ozks.query(key);
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(true, query_result.verify(commitment));

    key = make_bytes<key_type>(0x06, 0x07, 0x00);
    query_result = ozks.query(key);
    EXPECT_EQ(false, query_result.is_member());
    EXPECT_EQ(0, query_result.payload().size());
    EXPECT_EQ(randomness_size, query_result.randomness().size());
    EXPECT_EQ(true, query_result.verify(commitment));
}

TEST(OZKSTests, FailedQueryTest)
{
    key_payload_batch_type label_payload_batch{
        pair<vector<byte>, vector<byte>>{ make_bytes<key_type>(0x11, 0x01),
                                          make_bytes<payload_type>(0xA0, 0xB0, 0xC0) },
        pair<vector<byte>, vector<byte>>{ make_bytes<key_type>(0x01, 0x02),
                                          make_bytes<payload_type>(0xA1, 0xB1, 0xC1) },
        pair<vector<byte>, vector<byte>>{ make_bytes<key_type>(0xEE, 0x03),
                                          make_bytes<payload_type>(0xA2, 0xB2, 0xC2) },
        pair<vector<byte>, vector<byte>>{ make_bytes<key_type>(0xAA, 0x04),
                                          make_bytes<payload_type>(0xA3, 0xB3, 0xC3) },
        pair<vector<byte>, vector<byte>>{ make_bytes<key_type>(0xCC, 0x05),
                                          make_bytes<payload_type>(0xA4, 0xB4, 0xC4) },
        pair<vector<byte>, vector<byte>>{ make_bytes<key_type>(0xFF, 0x06),
                                          make_bytes<payload_type>(0xA5, 0xB5, 0xC5) }
    };

    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::UncommitedPayload,
        LabelType::HashedLabels,
        TrieType::LinkedNoStorage,
        storage,
    };
    OZKS ozks(config);

    auto insert_result = ozks.insert(label_payload_batch);
    ozks.flush();

    Commitment commitment = ozks.get_commitment();

    // Check inserts are verified
    for (const auto &single_result : insert_result) {
        EXPECT_TRUE(single_result->verify());
    }

    auto key = make_bytes<key_type>(0x01, 0x02);
    QueryResult qr = ozks.query(key);

    EXPECT_TRUE(qr.is_member());
    EXPECT_EQ(4, qr.lookup_proof().size());
    EXPECT_TRUE(qr.verify(commitment));

    key = make_bytes<key_type>(0xFF, 0xFF);
    qr = ozks.query(key);

    EXPECT_FALSE(qr.is_member());
    EXPECT_EQ(3, qr.lookup_proof().size());
    EXPECT_TRUE(qr.verify(commitment));
}

TEST(OZKSTests, InsertResultVerificationTest)
{
    OZKS ozks;

    auto key = make_bytes<key_type>(0x01, 0x02, 0x03, 0x04);
    auto payload = make_bytes<payload_type>(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

    auto insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes<key_type>(0x02, 0x03, 0x04, 0x05);
    payload = make_bytes<payload_type>(0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes<key_type>(0x03, 0x03, 0x04, 0x05);
    payload = make_bytes<payload_type>(0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes<key_type>(0xFF, 0xFE, 0xFD, 0xFC);
    payload = make_bytes<payload_type>(0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes<key_type>(0xF0, 0xF1, 0xF2, 0xF3);
    payload = make_bytes<payload_type>(0xA2, 0xB3, 0xC4, 0xD5, 0xE6, 0xF7);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes<key_type>(0xF1, 0xF2, 0xF3, 0xF4);
    payload = make_bytes<payload_type>(0xA3, 0xB4, 0xC5, 0xD6, 0xE7, 0xF8);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes<key_type>(0xF2, 0xF3, 0xF4, 0xF5);
    payload = make_bytes<payload_type>(0xA4, 0xB5, 0xC6, 0xD7, 0xE8, 0xF9);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());

    key = make_bytes<key_type>(0xF3, 0xF4, 0xF5, 0xF6);
    payload = make_bytes<payload_type>(0xA5, 0xB6, 0xC7, 0xD8, 0xE9, 0xFA);

    insert_result = ozks.insert(key, payload);
    ozks.flush();

    EXPECT_EQ(true, insert_result->verify());
}

TEST(OZKSTests, StoredRandomInsertVerificationTest)
{
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    RandomInsertTestCore(storage, random_iterations);
}

TEST(OZKSTests, LinkedRandomInsertVerificationTest)
{
    RandomInsertTestCore(TrieType::Linked, /* use_storage */ true, random_iterations);
}

TEST(OZKSTests, LinkedRandomInsertVerificationNoStorageTest)
{
    RandomInsertTestCore(TrieType::Linked, /* use_storage */ false, random_iterations);
}

TEST(OZKSTests, RandomInsert10StoredTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 10);
}

TEST(OZKSTests, RandomInsert10LinkedTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Linked, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 10);
}

TEST(OZKSTests, RandomInsert100StoredTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 100);
}

TEST(OZKSTests, RandomInsert100LinkedTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Linked, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 100);
}

TEST(OZKSTests, RandomInsert1000StoredTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 1000);
}

TEST(OZKSTests, RandomInsert1000LinkedTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Linked, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 1000);
}

TEST(OZKSTests, RandomInsert10000StoredTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 10000);
}

TEST(OZKSTests, RandomInsert10000LinkedTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Linked, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 10000);
}

TEST(OZKSTests, RandomInsert100000StoredTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 100000);
}

TEST(OZKSTests, RandomInsert100000LinkedTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Linked, storage
    };
    OZKS ozks(config);
    RandomInsertCore(ozks, /* iterations */ 100000);
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

    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload,
        LabelType::VRFLabels,
        TrieType::Stored,
        batching_storage);
    OZKS ozks(config);

    key_type key(16);
    payload_type payload(40);
    vector<key_type> valid_keys;
    vector<payload_type> valid_payloads;
    vector<shared_ptr<InsertResult>> insert_results;

    for (size_t i = 0; i < random_iterations; i++) {
        get_random_bytes(key.data(), key.size());
        get_random_bytes(payload.data(), payload.size());

        auto insert_result = ozks.insert(key, payload);
        insert_results.push_back(insert_result);

        // Add some keys at random to check later (up to 100)
        byte b;
        get_random_bytes(&b, 1);
        if (valid_keys.size() < 100 && static_cast<int>(b) > 128) {
            valid_keys.push_back(key);
            valid_payloads.push_back(payload);
        }
    }

    // At this point memory storage should be empty
    EXPECT_EQ(0, tbs->node_count());
    EXPECT_EQ(0, tbs->store_element_count());
    EXPECT_EQ(0, tbs->trie_count());
//    EXPECT_EQ(0, tbs->ozks_count());

    ozks.flush();

    // Now memory storage should have everything
    EXPECT_GE(tbs->node_count(), random_iterations);
    EXPECT_EQ(random_iterations, tbs->store_element_count());
    EXPECT_EQ(1, tbs->trie_count());
 //   EXPECT_EQ(0, tbs->ozks_count());

    // And we can verify results
    for (auto &insert_result : insert_results) {
        EXPECT_EQ(true, insert_result->verify());
    }

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
}

TEST(OZKSTests, RandomMultiInsertVerificationTest)
{
    auto storage = make_shared<storage::MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    OZKS ozks(config);
    key_type key(16);
    payload_type payload(40);
    vector<key_type> valid_keys;

    key_payload_batch_type insertions;

    for (size_t i = 0; i < random_iterations; i++) {
        get_random_bytes(key.data(), key.size());
        get_random_bytes(payload.data(), payload.size());

        insertions.push_back({ key, payload });

        byte b;
        get_random_bytes(&b, 1);
        if (valid_keys.size() < 100 && static_cast<int>(b) > 128) {
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
        EXPECT_TRUE(result.verify(ozks.get_commitment()));
    }

    // Check that invalid keys are not found and that their path is verified correctly
    for (size_t i = 0; i < 1000; i++) {
        get_random_bytes(key.data(), key.size());

        auto result = ozks.query(key);
        EXPECT_FALSE(result.is_member());
        EXPECT_EQ(0, result.payload().size());
        EXPECT_TRUE(result.verify(ozks.get_commitment()));
    }
}

TEST(OZKSTests, QueryResultVerificationTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    OZKS ozks(config);

    key_payload_batch_type batch{
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x01, 0x02, 0x03),
            make_bytes<payload_type>(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x02, 0x03, 0x04),
            make_bytes<payload_type>(0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x03, 0x04, 0x05),
            make_bytes<payload_type>(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x04, 0x05, 0x06),
            make_bytes<payload_type>(0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7) },
        pair<key_type, payload_type>{
            make_bytes<key_type>(0x05, 0x06, 0x07),
            make_bytes<payload_type>(0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6) },
        pair<key_type, payload_type>{ make_bytes<key_type>(0x06, 0x07, 0x08),
                                      make_bytes<payload_type>(0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5) }
    };

    auto result = ozks.insert(batch);
    ozks.flush();
    EXPECT_EQ(6, result.size());

    auto key = make_bytes<key_type>(0x03, 0x04, 0x05);
    auto payload = make_bytes<payload_type>(0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8);
    QueryResult query_result = ozks.query(key);
    Commitment commitment = ozks.get_commitment();
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(payload, query_result.payload());
    EXPECT_EQ(true, query_result.verify(commitment));

    key = make_bytes<key_type>(0x04, 0x05, 0x06);
    payload = make_bytes<payload_type>(0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7);
    query_result = ozks.query(key);
    EXPECT_EQ(true, query_result.is_member());
    EXPECT_EQ(payload, query_result.payload());
    EXPECT_EQ(true, query_result.verify(commitment));

    key = make_bytes<key_type>(0x02, 0x03, 0x05);
    query_result = ozks.query(key);
    EXPECT_EQ(false, query_result.is_member());
    EXPECT_EQ(0, query_result.payload().size());
    EXPECT_EQ(randomness_size, query_result.randomness().size());
    EXPECT_EQ(true, query_result.verify(commitment));
}

TEST(OZKSTests, SaveLoadTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    OZKS ozks(config);

    key_type key(40);
    payload_type payload(40);
    vector<key_type> some_keys(100);

    for (size_t i = 0; i < 10000; i++) {
        get_random_bytes(key.data(), key.size());
        get_random_bytes(payload.data(), payload.size());

        ozks.insert(key, payload);

        if (i < some_keys.size()) {
            some_keys[i] = key;
        }
    }

    ozks.flush();

    stringstream ss;
    size_t save_size = ozks.save(ss);

    auto loaded = OZKS::Load(config.storage(), ss);
    OZKS &ozks2 = loaded.first;
    size_t load_size = loaded.second;

    EXPECT_EQ(load_size, save_size);

    for (size_t i = 0; i < some_keys.size(); i++) {
        QueryResult qr1 = ozks.query(some_keys[i]);
        QueryResult qr2 = ozks2.query(some_keys[i]);

        EXPECT_TRUE(qr1.is_member());
        EXPECT_TRUE(qr2.is_member());
        EXPECT_EQ(qr1.payload(), qr2.payload());
        EXPECT_EQ(qr1.randomness(), qr2.randomness());
        EXPECT_EQ(qr1.lookup_proof().size(), qr2.lookup_proof().size());

        vector<byte> com1, com2;
        ozks.get_commitment().save(com1);
        ozks2.get_commitment().save(com2);
        EXPECT_TRUE(com1 == com2);

        EXPECT_TRUE(qr1.verify(ozks.get_commitment()));
        EXPECT_TRUE(qr2.verify(ozks2.get_commitment()));

        EXPECT_TRUE(qr1.verify(ozks2.get_commitment()));
        EXPECT_TRUE(qr2.verify(ozks.get_commitment()));
    }
}

TEST(OZKSTests, NonRandomSaveLoadTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::UncommitedPayload, LabelType::HashedLabels, TrieType::Stored, storage
    };
    OZKS ozks(config);

    EXPECT_EQ(PayloadCommitmentType::UncommitedPayload, ozks.get_config().payload_commitment());
    EXPECT_EQ(LabelType::HashedLabels, ozks.get_config().label_type());
    EXPECT_EQ(TrieType::Stored, ozks.get_config().trie_type());

    key_type key(40);
    payload_type payload(40);
    vector<key_type> some_keys(100);

    for (size_t i = 0; i < 10000; i++) {
        get_random_bytes(key.data(), key.size());
        get_random_bytes(payload.data(), payload.size());

        ozks.insert(key, payload);

        if (i < some_keys.size()) {
            some_keys[i] = key;
        }
    }

    ozks.flush();

    stringstream ss;
    size_t save_size = ozks.save(ss);

    OZKSConfig config2(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);

    auto loaded = OZKS::Load(config2.storage(), ss);
    OZKS &ozks2 = loaded.first;
    size_t load_size = loaded.second;

    EXPECT_EQ(PayloadCommitmentType::UncommitedPayload, ozks2.get_config().payload_commitment());
    EXPECT_EQ(LabelType::HashedLabels, ozks2.get_config().label_type());

    EXPECT_EQ(load_size, save_size);

    for (size_t i = 0; i < some_keys.size(); i++) {
        QueryResult qr1 = ozks.query(some_keys[i]);
        QueryResult qr2 = ozks2.query(some_keys[i]);

        EXPECT_TRUE(qr1.is_member());
        EXPECT_TRUE(qr2.is_member());
        EXPECT_EQ(qr1.payload(), qr2.payload());
        EXPECT_EQ(qr1.randomness(), qr2.randomness());
        EXPECT_EQ(qr1.lookup_proof().size(), qr2.lookup_proof().size());

        EXPECT_TRUE(qr1.verify(ozks2.get_commitment()));
        EXPECT_TRUE(qr2.verify(ozks.get_commitment()));
    }
}

TEST(OZKSTests, SaveLoadToVectorTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    OZKS ozks(config);

    key_type key(40);
    payload_type payload(40);
    vector<key_type> some_keys(100);

    for (size_t i = 0; i < 10000; i++) {
        get_random_bytes(key.data(), key.size());
        get_random_bytes(payload.data(), payload.size());

        ozks.insert(key, payload);

        if (i < some_keys.size()) {
            some_keys[i] = key;
        }
    }

    ozks.flush();

    vector<byte> vec;
    size_t save_size = ozks.save(vec);

    auto loaded = OZKS::Load(config.storage(), vec);
    OZKS &ozks2 = loaded.first;
    size_t load_size = loaded.second;

    EXPECT_EQ(load_size, save_size);

    for (size_t i = 0; i < some_keys.size(); i++) {
        QueryResult qr1 = ozks.query(some_keys[i]);
        QueryResult qr2 = ozks2.query(some_keys[i]);

        EXPECT_TRUE(qr1.is_member());
        EXPECT_TRUE(qr2.is_member());
        EXPECT_EQ(qr1.payload(), qr2.payload());
        EXPECT_EQ(qr1.randomness(), qr2.randomness());
        EXPECT_EQ(qr1.lookup_proof().size(), qr2.lookup_proof().size());

        EXPECT_TRUE(qr1.verify(ozks2.get_commitment()));
        EXPECT_TRUE(qr2.verify(ozks.get_commitment()));
    }
}

TEST(OZKSTests, EmptyOZKSTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    OZKS ozks1(config);
    OZKS ozks2(config);

    Commitment comm1 = ozks1.get_commitment();
    Commitment comm2 = ozks2.get_commitment();

    EXPECT_EQ(comm1.root_commitment(), comm2.root_commitment());
}

TEST(OZKSTests, ConfigurationTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::UncommitedPayload, LabelType::HashedLabels, TrieType::Stored, storage
    }; // No commit randomness, do not include vrf

    OZKS ozks1(config);
    OZKS ozks2(config);

    EXPECT_EQ(PayloadCommitmentType::UncommitedPayload, ozks1.get_config().payload_commitment());
    EXPECT_EQ(LabelType::HashedLabels, ozks2.get_config().label_type());

    Commitment comm1 = ozks1.get_commitment();
    Commitment comm2 = ozks2.get_commitment();

    EXPECT_EQ(comm1.root_commitment(), comm2.root_commitment());

    key_type key = make_bytes<key_type>(0x1, 0x2, 0x3);
    payload_type payload = make_bytes<payload_type>(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);

    ozks1.insert(key, payload);
    ozks2.insert(key, payload);
    ozks1.flush();
    ozks2.flush();

    comm1 = ozks1.get_commitment();
    comm2 = ozks2.get_commitment();

    EXPECT_EQ(comm1.root_commitment(), comm2.root_commitment());

    OZKSConfig config2{
        PayloadCommitmentType::CommitedPayload, LabelType::HashedLabels, TrieType::Stored, storage
    }; // Commit randomness, do not include vrf
    EXPECT_EQ(PayloadCommitmentType::CommitedPayload, config2.payload_commitment());
    EXPECT_EQ(LabelType::HashedLabels, config2.label_type());

    OZKS ozks3(config2);
    OZKS ozks4(config2);

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
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config1(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    OZKS ozks1(config1);

    EXPECT_EQ(PayloadCommitmentType::CommitedPayload, ozks1.get_config().payload_commitment());
    EXPECT_EQ(LabelType ::VRFLabels, ozks1.get_config().label_type());

    OZKSConfig config{
        PayloadCommitmentType::UncommitedPayload, LabelType::HashedLabels, TrieType::Stored, storage
    };
    OZKS ozks2(config);

    EXPECT_EQ(PayloadCommitmentType::UncommitedPayload, ozks2.get_config().payload_commitment());
    EXPECT_EQ(LabelType::HashedLabels, ozks2.get_config().label_type());

    config = {
        PayloadCommitmentType::CommitedPayload, LabelType::HashedLabels, TrieType::Stored, storage
    };
    OZKS ozks3(config);

    EXPECT_EQ(PayloadCommitmentType::CommitedPayload, ozks3.get_config().payload_commitment());
    EXPECT_EQ(LabelType::HashedLabels, ozks3.get_config().label_type());

    config = {
        PayloadCommitmentType::UncommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage
    };
    OZKS ozks4(config);

    EXPECT_EQ(PayloadCommitmentType::UncommitedPayload, ozks4.get_config().payload_commitment());
    EXPECT_EQ(LabelType::VRFLabels, ozks4.get_config().label_type());

    config = { PayloadCommitmentType::UncommitedPayload,
               LabelType::VRFLabels,
               TrieType::LinkedNoStorage,
               storage };
    OZKS ozks5(config);

    EXPECT_EQ(TrieType::LinkedNoStorage, ozks5.get_config().trie_type());
}

TEST(OZKSTests, BatchInserterCallbackTest)
{
    GTEST_SKIP() << "Does not work since disabling callback in batch inserter";
    shared_ptr<TestBackingStorage> backing_storage = make_shared<TestBackingStorage>();
    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);

    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload,
        LabelType::VRFLabels,
        TrieType::Stored,
        batch_inserter);
    OZKS ozks(config);

    vector<byte> saved_ozks;
    ozks.save(saved_ozks);

    // Insert one element to ensure trie is saved in backing storage
    RandomInsertTestCore(ozks, 1, /* flush_at_end */ true);

    shared_ptr<storage::MemoryStorageBatchInserter> batch_inserter2 =
        make_shared<storage::MemoryStorageBatchInserter>(backing_storage);

    OZKSConfig config2(
        PayloadCommitmentType::CommitedPayload,
        LabelType::VRFLabels,
        TrieType::Stored,
        batch_inserter2);

    // This ensures we have the same trie ID
    auto loaded = OZKS::Load(config2.storage(), saved_ozks);
    OZKS &ozks2 = loaded.first;

    // Generate 3 epochs
    EXPECT_EQ(1, backing_storage->updated_nodes_count());
    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(2, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(3, backing_storage->updated_nodes_count());

    RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_EQ(4, backing_storage->updated_nodes_count());

    size_t epoch = ozks2.get_epoch();
    EXPECT_EQ(1, epoch); // Since it's still our local copy

    // This should not throw
    ozks2.check_for_update();
    epoch = ozks2.get_epoch();
    EXPECT_EQ(4, epoch);
    EXPECT_EQ(4, backing_storage->updated_nodes_count()); // Since no updated nodes were retrieved
}

TEST(OZKSTests, NodeDeletionTest)
{
    shared_ptr<TestBackingStorage> storage = make_shared<TestBackingStorage>();

    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    OZKS ozks(config);

    vector<byte> saved_ozks;
    ozks.save(saved_ozks);

    // Insert some nodes
    vector<key_type> valid_keys = RandomInsertTestCore(ozks, 1000, /* flush_at_end */ true);
    EXPECT_NE(valid_keys.size(), 0);

    auto commitment = ozks.get_commitment();

    for (const auto &valid_key : valid_keys) {
        auto result = ozks.query(valid_key);
        EXPECT_TRUE(result.is_member());
        EXPECT_TRUE(result.verify(commitment));
    }

    // We should have at least the root
    CTNodeStored root_node;
    EXPECT_TRUE(storage->load_ctnode(ozks.id(), {}, {}, root_node));

    // We should have the trie as well
    CompressedTrie trie;
    EXPECT_TRUE(storage->load_compressed_trie(ozks.id(), trie));

    // Clear the tree
    ozks.clear();

    // We should have no nodes in the trie
    EXPECT_FALSE(storage->load_ctnode(ozks.id(), {}, {}, root_node));

    // We should have no trie either
    EXPECT_FALSE(storage->load_compressed_trie(ozks.id(), trie));

    // Now valid keys should not be found
    for (const auto &valid_key : valid_keys) {
        auto result = ozks.query(valid_key);
        EXPECT_FALSE(result.is_member());
    }
}
