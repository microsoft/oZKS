// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/query_result.h"
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
using namespace utils;

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
    A storage that will keep track of which nodes and tries where updated in a given epoch
    */
    class UpdatedNodesPerEpochStorage : public storage::BatchStorage {
    public:
        UpdatedNodesPerEpochStorage() : backing_(make_shared<storage::MemoryStorage>())
        {}

        /**
        Get a node from storage.
        The implementation has the possibility of deciding to load a batch of nodes related to
        the given node. If that is the case, additional nodes will be added to the Storage
        instance given as parameter.
        The storage pointer can be null. In such a case no node addition will happen.
        */
        bool load_ctnode(
            trie_id_type trie_id,
            const PartialLabel &node_id,
            shared_ptr<Storage> storage,
            CTNodeStored &node) override
        {
            return backing_->load_ctnode(trie_id, node_id, storage, node);
        }

        /**
        Save a node to storage
        */
        void save_ctnode(trie_id_type trie_id, const CTNodeStored &node) override
        {
            backing_->save_ctnode(trie_id, node);
        }

        /**
        Get a compressed trie from storage
        */
        bool load_compressed_trie(trie_id_type trie_id, CompressedTrie &trie) override
        {
            return backing_->load_compressed_trie(trie_id, trie);
        }

        /**
        Save a compressed trie to storage
        */
        void save_compressed_trie(const CompressedTrie &trie) override
        {
            backing_->save_compressed_trie(trie);
        }

        /**
        Get a store element from storage
        */
        bool load_store_element(
            trie_id_type trie_id, const vector<byte> &key, store_value_type &value) override
        {
            return backing_->load_store_element(trie_id, key, value);
        }

        /**
        Save a store element to storage
        */
        void save_store_element(
            trie_id_type trie_id, const vector<byte> &key, const store_value_type &value) override
        {
            backing_->save_store_element(trie_id, key, value);
        }

        /**
        Flush changes if appropriate
        */
        void flush(trie_id_type trie_id) override
        {
            backing_->flush(trie_id);
        }

        /**
        Add an existing node to the current storage.
        */
        void add_ctnode(trie_id_type trie_id, const CTNodeStored &node) override
        {
            backing_->add_ctnode(trie_id, node);
        }

        /**
        Add an existing compresssed trie to the current storage.
        */
        void add_compressed_trie(const CompressedTrie &trie) override
        {
            backing_->add_compressed_trie(trie);
        }

        /**
        Add an existing store element to the current storage
        */
        void add_store_element(
            trie_id_type trie_id, const vector<byte> &key, const store_value_type &value) override
        {
            backing_->add_store_element(trie_id, key, value);
        }

        /**
        Get the latest epoch for the given compressed trie
        */
        size_t get_compressed_trie_epoch(trie_id_type trie_id) override
        {
            return backing_->get_compressed_trie_epoch(trie_id);
        }

        /**
        Load updated elements for the given epoch
        */
        void load_updated_elements(
            size_t epoch, trie_id_type trie_id, shared_ptr<Storage> storage) override
        {
            const auto nodes = updated_nodes_.find(epoch);
            if (nodes == updated_nodes_.cend()) {
                throw new runtime_error("Did not find epoch!");
            }

            for (const CTNodeStored &node : (*nodes).second) {
                storage->add_ctnode(trie_id, node);
            }

            const auto tries = updated_tries_.find(epoch);
            if (tries == updated_tries_.cend()) {
                throw new runtime_error("Did not find epoch!");
            }

            for (const CompressedTrie &trie : (*tries).second) {
                storage->add_compressed_trie(trie);
            }
        }

        /**
        Delete nodes for the given trie from storage, as well as the trie itself and related
        ozks instance. Some storage implementation might not support this operation.
        */
        void delete_ozks(trie_id_type trie_id) override
        {
            backing_->delete_ozks(trie_id);
        }

        /**
        Flush given sets of nodes, tries, ozks instances and store elements
        */
        void flush(
            trie_id_type trie_id,
            const vector<CTNodeStored> &nodes,
            const vector<CompressedTrie> &tries,
            [[maybe_unused]] const vector<pair<vector<byte>, store_value_type>> &store_elements)
            override
        {
            size_t epoch = get_compressed_trie_epoch(trie_id);

            vector<CTNodeStored> updated_nodes(nodes.size());
            for (size_t idx = 0; idx < nodes.size(); idx++) {
                updated_nodes[idx] = nodes[idx];
                backing_->save_ctnode(trie_id, nodes[idx]);
            }

            vector<CompressedTrie> updated_tries(tries.size());
            for (size_t idx = 0; idx < tries.size(); idx++) {
                updated_tries[idx] = tries[idx];
                backing_->save_compressed_trie(tries[idx]);
            }

            updated_nodes_.insert_or_assign(epoch + 1, std::move(updated_nodes));
            updated_tries_.insert_or_assign(epoch + 1, std::move(updated_tries));
        }

    private:
        shared_ptr<storage::Storage> backing_;
        unordered_map<size_t, vector<CTNodeStored>> updated_nodes_;
        unordered_map<size_t, vector<CompressedTrie>> updated_tries_;
    };
} // namespace

void DoInsertTest(CompressedTrie &trie)
{
    PartialLabel bytes1 = make_bytes<PartialLabel>(0xAA, 0xBB, 0xCC, 0xDD, 0xEE);
    PartialLabel bytes2 = make_bytes<PartialLabel>(0x01, 0x01, 0x01, 0x01, 0x01);
    PartialLabel bytes3 = make_bytes<PartialLabel>(0x10, 0x10, 0x10, 0x10, 0x10);
    PartialLabel bytes4 = make_bytes<PartialLabel>(0x80, 0x80, 0x80, 0x80, 0x80);
    PartialLabel bytes5 = make_bytes<PartialLabel>(0xC0, 0xC0, 0xC0, 0xC0, 0xC0);
    PartialLabel bytes6 = make_bytes<PartialLabel>(0xE0, 0xE0, 0xE0, 0xE0, 0xE0);
    PartialLabel bytes7 = make_bytes<PartialLabel>(0xF0, 0xF0, 0xF0, 0xF0, 0xF0);
    append_proof_type append_proof;

    trie.insert(bytes1, make_bytes<hash_type>(0x01, 0x02, 0x03), append_proof);
    string status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:(null):r:1010101010111011110011001101110111101110;"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);");

    trie.insert(bytes2, make_bytes<hash_type>(0x02, 0x03, 0x04), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:0000000100000001000000010000000100000001:r:1010101010111011110011001101110111101110;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);");

    trie.insert(bytes3, make_bytes<hash_type>(0x03, 0x04, 0x05), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1010101010111011110011001101110111101110;"
        "n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);");

    trie.insert(bytes4, make_bytes<hash_type>(0x04, 0x05, 0x06), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:10;n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:10:l:1000000010000000100000001000000010000000:r:"
        "1010101010111011110011001101110111101110;"
        "n:1000000010000000100000001000000010000000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);");

    trie.insert(bytes5, make_bytes<hash_type>(0x05, 0x06, 0x07), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:1:l:10:r:1100000011000000110000001100000011000000;"
        "n:10:l:1000000010000000100000001000000010000000:r:"
        "1010101010111011110011001101110111101110;"
        "n:1000000010000000100000001000000010000000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);"
        "n:1100000011000000110000001100000011000000:l:(null):r:(null);");

    trie.insert(bytes6, make_bytes<hash_type>(0x06, 0x07, 0x08), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:1:l:10:r:11;"
        "n:10:l:1000000010000000100000001000000010000000:r:"
        "1010101010111011110011001101110111101110;"
        "n:1000000010000000100000001000000010000000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);"
        "n:11:l:1100000011000000110000001100000011000000:r:"
        "1110000011100000111000001110000011100000;"
        "n:1100000011000000110000001100000011000000:l:(null):r:(null);"
        "n:1110000011100000111000001110000011100000:l:(null):r:(null);");

    trie.insert(bytes7, make_bytes<hash_type>(0x07, 0x08, 0x09), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:1:l:10:r:11;"
        "n:10:l:1000000010000000100000001000000010000000:r:"
        "1010101010111011110011001101110111101110;"
        "n:1000000010000000100000001000000010000000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);"
        "n:11:l:1100000011000000110000001100000011000000:r:111;"
        "n:1100000011000000110000001100000011000000:l:(null):r:(null);"
        "n:111:l:1110000011100000111000001110000011100000:r:"
        "1111000011110000111100001111000011110000;"
        "n:1110000011100000111000001110000011100000:l:(null):r:(null);"
        "n:1111000011110000111100001111000011110000:l:(null):r:(null);");
}

TEST(CompressedTrieTests, StoredInsertTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    DoInsertTest(trie);
}

TEST(CompressedTrieTests, LinkedInsertTest)
{
    CompressedTrie trie({}, TrieType::Linked);
    DoInsertTest(trie);
}

void DoInsertSimpleTest(CompressedTrie &trie)
{
    PartialLabel bytes1 = make_bytes<PartialLabel>(0x11);
    PartialLabel bytes2 = make_bytes<PartialLabel>(0x01);
    PartialLabel bytes3 = make_bytes<PartialLabel>(0xEE);
    PartialLabel bytes4 = make_bytes<PartialLabel>(0xAA);
    PartialLabel bytes5 = make_bytes<PartialLabel>(0xCC);
    PartialLabel bytes6 = make_bytes<PartialLabel>(0xFF);
    append_proof_type append_proof;

    trie.insert(bytes1, make_bytes<hash_type>(0xA0, 0xB0, 0xC0), append_proof);
    string status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:00010001:r:(null);"
        "n:00010001:l:(null):r:(null);");
    EXPECT_EQ(1, trie.epoch());

    trie.insert(bytes2, make_bytes<hash_type>(0xA1, 0xB1, 0xC1), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:(null);"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);");
    EXPECT_EQ(2, trie.epoch());

    trie.insert(bytes3, make_bytes<hash_type>(0xA2, 0xB2, 0xC2), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:11101110;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);"
        "n:11101110:l:(null):r:(null);");
    EXPECT_EQ(3, trie.epoch());

    trie.insert(bytes4, make_bytes<hash_type>(0xA3, 0xB3, 0xC3), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);"
        "n:1:l:10101010:r:11101110;"
        "n:10101010:l:(null):r:(null);"
        "n:11101110:l:(null):r:(null);");
    EXPECT_EQ(4, trie.epoch());

    trie.insert(bytes5, make_bytes<hash_type>(0xA4, 0xB4, 0xC4), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);"
        "n:1:l:10101010:r:11;"
        "n:10101010:l:(null):r:(null);"
        "n:11:l:11001100:r:11101110;"
        "n:11001100:l:(null):r:(null);"
        "n:11101110:l:(null):r:(null);");
    EXPECT_EQ(5, trie.epoch());

    trie.insert(bytes6, make_bytes<hash_type>(0xA5, 0xB5, 0xC5), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);"
        "n:1:l:10101010:r:11;"
        "n:10101010:l:(null):r:(null);"
        "n:11:l:11001100:r:111;"
        "n:11001100:l:(null):r:(null);"
        "n:111:l:11101110:r:11111111;"
        "n:11101110:l:(null):r:(null);"
        "n:11111111:l:(null):r:(null);");
    EXPECT_EQ(6, trie.epoch());
}

TEST(CompressedTrieTests, StoredInsertSimpleTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    DoInsertSimpleTest(trie);
}

TEST(CompressedTrieTests, LinkedInsertSimpleTest)
{
    CompressedTrie trie({}, TrieType::Linked);
    DoInsertSimpleTest(trie);
}

void DoAppendProofTest(CompressedTrie &trie)
{
    PartialLabel bytes1 = make_bytes<PartialLabel>(0x11);
    PartialLabel bytes2 = make_bytes<PartialLabel>(0x01);
    PartialLabel bytes3 = make_bytes<PartialLabel>(0xEE);
    PartialLabel bytes4 = make_bytes<PartialLabel>(0xAA);
    PartialLabel bytes5 = make_bytes<PartialLabel>(0xCC);
    PartialLabel bytes6 = make_bytes<PartialLabel>(0xFF);
    append_proof_type append_proof;
    PartialLabel partial_label;

    trie.insert(bytes1, make_bytes<hash_type>(0xA0, 0xB0, 0xC0), append_proof);
    EXPECT_EQ(1, append_proof.size());
    partial_label = PartialLabel{ 0, 0, 0, 1, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    EXPECT_EQ(1, trie.epoch());

    trie.insert(bytes2, make_bytes<hash_type>(0xA1, 0xB1, 0xC1), append_proof);
    EXPECT_EQ(2, append_proof.size());
    partial_label = PartialLabel{ 0, 0, 0, 0, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = PartialLabel{ 0, 0, 0, 1, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    EXPECT_EQ(2, trie.epoch());

    trie.insert(bytes3, make_bytes<hash_type>(0xA2, 0xB2, 0xC2), append_proof);
    EXPECT_EQ(2, append_proof.size());
    partial_label = PartialLabel{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = PartialLabel{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    EXPECT_EQ(3, trie.epoch());

    trie.insert(bytes4, make_bytes<hash_type>(0xA3, 0xB3, 0xC3), append_proof);
    EXPECT_EQ(3, append_proof.size());
    partial_label = PartialLabel{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = PartialLabel{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    partial_label = PartialLabel{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[2].first);
    EXPECT_EQ(4, trie.epoch());

    trie.insert(bytes5, make_bytes<hash_type>(0xA4, 0xB4, 0xC4), append_proof);
    EXPECT_EQ(4, append_proof.size());
    partial_label = PartialLabel{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = PartialLabel{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    partial_label = PartialLabel{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[2].first);
    partial_label = PartialLabel{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[3].first);
    EXPECT_EQ(5, trie.epoch());

    trie.insert(bytes6, make_bytes<hash_type>(0xA5, 0xB5, 0xC5), append_proof);
    EXPECT_EQ(5, append_proof.size());
    partial_label = PartialLabel{ 1, 1, 1, 1, 1, 1, 1, 1 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = PartialLabel{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    partial_label = PartialLabel{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[2].first);
    partial_label = PartialLabel{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[3].first);
    partial_label = PartialLabel{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[4].first);
    EXPECT_EQ(6, trie.epoch());
}

TEST(CompressedTrieTests, StoredAppendProofTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    DoAppendProofTest(trie);
}

TEST(CompressedTrieTests, LinkedAppendProofTest)
{
    CompressedTrie trie({}, TrieType::Linked);
    DoAppendProofTest(trie);
}

void DoInsertSimpleBatchTest(CompressedTrie &trie)
{
    append_proof_batch_type append_proofs;
    partial_label_hash_batch_type label_payload_batch{
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0x11),
                                       make_bytes<hash_type>(0xA0, 0xB0, 0xC0) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0x01),
                                       make_bytes<hash_type>(0xA1, 0xB1, 0xC1) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xEE),
                                       make_bytes<hash_type>(0xA2, 0xB2, 0xC2) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xAA),
                                       make_bytes<hash_type>(0xA3, 0xB3, 0xC3) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xCC),
                                       make_bytes<hash_type>(0xA4, 0xB4, 0xC4) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xFF),
                                       make_bytes<hash_type>(0xA5, 0xB5, 0xC5) }
    };

    EXPECT_EQ(0, trie.epoch());

    trie.insert(label_payload_batch, append_proofs);
    string status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);"
        "n:1:l:10101010:r:11;"
        "n:10101010:l:(null):r:(null);"
        "n:11:l:11001100:r:111;"
        "n:11001100:l:(null):r:(null);"
        "n:111:l:11101110:r:11111111;"
        "n:11101110:l:(null):r:(null);"
        "n:11111111:l:(null):r:(null);");
    EXPECT_EQ(1, trie.epoch());
}

TEST(CompressedTrieTests, StoredInsertSimpleBatchTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    DoInsertSimpleBatchTest(trie);
}

TEST(CompressedTrieTests, LinkedInsertSimpleBatchTest)
{
    CompressedTrie trie({}, TrieType::Linked);
    DoInsertSimpleBatchTest(trie);
}

TEST(CompressedTrieTests, LinkedMultiThreadedBatchTest)
{
    CompressedTrie trie({}, TrieType::Linked);

    append_proof_batch_type append_proofs;

    // The first insertion makes sure the first two tree levels are present
    partial_label_hash_batch_type batch{
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x01, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x02, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x80, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x03, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xC0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x04, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x30, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x05, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x70, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x06, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xE0, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x07, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xF0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x08, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x09, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x40, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0A, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x80, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0B, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xC0, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0C, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x30, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0D, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x70, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0E, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xE0, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0F, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xF0, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x10, 0x02, 0x03) }
    };

    EXPECT_EQ(0, trie.epoch());

    // The first insert will use 1 thread
    trie.insert(batch, append_proofs);

    EXPECT_EQ(1, trie.epoch());

    // The first 20000 items will use 2 threads
    batch.clear();
    batch.resize(20000);
    for (size_t idx = 1; idx <= batch.size(); idx++) {
        array<byte, PartialLabel::ByteCount> key_bytes{};
        get_random_bytes(key_bytes.data(), 8);
        PartialLabel key(key_bytes);

        hash_type payload{};
        get_random_bytes(payload.data(), 5);

        batch[idx - 1] = { key, payload };
    }

    append_proofs.clear();
    trie.insert(batch, append_proofs);

    EXPECT_EQ(2, trie.epoch());

    // The next 20000 should probably be faster
    batch.clear();
    batch.resize(20000);
    for (size_t idx = 1; idx <= batch.size(); idx++) {
        array<byte, PartialLabel::ByteCount> key_bytes{};
        get_random_bytes(key_bytes.data(), 8);
        PartialLabel key(key_bytes);

        hash_type payload{};
        get_random_bytes(payload.data(), 6);

        batch[idx - 1] = { key, payload };
    }

    append_proofs.clear();
    trie.insert(batch, append_proofs);

    EXPECT_EQ(3, trie.epoch());
}

TEST(CompressedTrieTests, VerifyUpdatedNodesInStorage)
{
    shared_ptr<storage::MemoryStorage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Linked);

    partial_label_hash_batch_type batch{
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x01, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x02, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x80, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x03, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xC0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x04, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x30, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x05, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x70, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x06, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xE0, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x07, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xF0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x08, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x09, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x40, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0A, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x80, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0B, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xC0, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0C, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x30, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0D, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0x70, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0E, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xE0, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x0F, 0x02, 0x03) },
        pair<PartialLabel, hash_type>{
            make_bytes<PartialLabel>(0xF0, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            make_bytes<hash_type>(0x10, 0x02, 0x03) }
    };

    append_proof_batch_type append_proofs;
    trie.insert(batch, append_proofs);

    // The first 20000 items will use 2 threads
    batch.clear();
    batch.resize(20000);
    for (size_t idx = 1; idx <= batch.size(); idx++) {
        array<byte, PartialLabel::ByteCount> key_bytes{};
        get_random_bytes(key_bytes.data(), 8);
        PartialLabel key(key_bytes);

        hash_type payload{};
        get_random_bytes(payload.data(), 5);

        batch[idx - 1] = { key, payload };
    }

    append_proofs.clear();
    trie.insert(batch, append_proofs);

    // Check that the trie was saved correctly to storage
    auto loaded = CompressedTrie::LoadFromStorageWithChildren(trie.id(), storage);
    EXPECT_TRUE(loaded.second);
    CompressedTrie &trie2 = *loaded.first;

    EXPECT_EQ(trie.id(), trie2.id());

    lookup_path_type lookup_path;
    EXPECT_TRUE(trie2.lookup(
        make_bytes<PartialLabel>(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00), lookup_path));
    EXPECT_TRUE(trie2.lookup(
        make_bytes<PartialLabel>(0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00), lookup_path));
    EXPECT_TRUE(trie2.lookup(
        make_bytes<PartialLabel>(0x80, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00), lookup_path));

    commitment_type commitment = trie.get_commitment();
    commitment_type commitment2 = trie2.get_commitment();
    EXPECT_EQ(commitment, commitment2);
}

void DoAppendProofBatchTest(CompressedTrie &trie)
{
    partial_label_hash_batch_type label_payload_batch{
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0x11),
                                       make_bytes<hash_type>(0xA0, 0xB0, 0xC0) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0x01),
                                       make_bytes<hash_type>(0xA1, 0xB1, 0xC1) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xEE),
                                       make_bytes<hash_type>(0xA2, 0xB2, 0xC2) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xAA),
                                       make_bytes<hash_type>(0xA3, 0xB3, 0xC3) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xCC),
                                       make_bytes<hash_type>(0xA4, 0xB4, 0xC4) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xFF),
                                       make_bytes<hash_type>(0xA5, 0xB5, 0xC5) }
    };
    append_proof_batch_type append_proofs;
    PartialLabel partial_label;

    EXPECT_EQ(0, trie.epoch());

    trie.insert(label_payload_batch, append_proofs);
    EXPECT_EQ(6, append_proofs.size());
    EXPECT_EQ(3, append_proofs[0].size());
    partial_label = PartialLabel{ 0, 0, 0, 1, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proofs[0][0].first);
    partial_label = PartialLabel{ 0, 0, 0, 0, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proofs[0][1].first);
    partial_label = PartialLabel{ 1 };
    EXPECT_EQ(partial_label, append_proofs[0][2].first);

    EXPECT_EQ(3, append_proofs[1].size());
    partial_label = PartialLabel{ 0, 0, 0, 0, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proofs[1][0].first);
    partial_label = PartialLabel{ 0, 0, 0, 1, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proofs[1][1].first);
    partial_label = PartialLabel{ 1 };
    EXPECT_EQ(partial_label, append_proofs[1][2].first);

    EXPECT_EQ(5, append_proofs[2].size());
    partial_label = PartialLabel{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[2][0].first);
    partial_label = PartialLabel{ 1, 1, 1, 1, 1, 1, 1, 1 };
    EXPECT_EQ(partial_label, append_proofs[2][1].first);
    partial_label = PartialLabel{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[2][2].first);
    partial_label = PartialLabel{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[2][3].first);
    partial_label = PartialLabel{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[2][4].first);

    EXPECT_EQ(3, append_proofs[3].size());
    partial_label = PartialLabel{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[3][0].first);
    partial_label = PartialLabel{ 1, 1 };
    EXPECT_EQ(partial_label, append_proofs[3][1].first);
    partial_label = PartialLabel{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[3][2].first);

    EXPECT_EQ(4, append_proofs[4].size());
    partial_label = PartialLabel{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[4][0].first);
    partial_label = PartialLabel{ 1, 1, 1 };
    EXPECT_EQ(partial_label, append_proofs[4][1].first);
    partial_label = PartialLabel{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[4][2].first);
    partial_label = PartialLabel{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[4][3].first);

    EXPECT_EQ(5, append_proofs[5].size());
    partial_label = PartialLabel{ 1, 1, 1, 1, 1, 1, 1, 1 };
    EXPECT_EQ(partial_label, append_proofs[5][0].first);
    partial_label = PartialLabel{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[5][1].first);
    partial_label = PartialLabel{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[5][2].first);
    partial_label = PartialLabel{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[5][3].first);
    partial_label = PartialLabel{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[5][4].first);

    EXPECT_EQ(1, trie.epoch());
}

TEST(CompressedTrieTests, StoredAppendProofBatchTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    DoAppendProofBatchTest(trie);
}

TEST(CompressedTrieTests, LinkedAppendProofBatchTest)
{
    CompressedTrie trie({}, TrieType::Linked);
    DoAppendProofBatchTest(trie);
}

void DoInsertInPartialLabelTest(CompressedTrie &trie)
{
    PartialLabel bytes = make_bytes<PartialLabel>(0x07);
    append_proof_type append_proof;
    trie.insert(bytes, make_bytes<hash_type>(0xF0, 0xE0, 0xD0), append_proof);
    string status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:00000111:r:(null);"
        "n:00000111:l:(null):r:(null);");

    bytes = make_bytes<PartialLabel>(0x04);
    trie.insert(bytes, make_bytes<hash_type>(0xF1, 0xE1, 0xD1), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000001:r:(null);"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);");

    bytes = make_bytes<PartialLabel>(0x08);
    trie.insert(bytes, make_bytes<hash_type>(0xF2, 0xE2, 0xD2), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:0000:r:(null);"
        "n:0000:l:000001:r:00001000;"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001000:l:(null):r:(null);");

    bytes = make_bytes<PartialLabel>(0x0C);
    trie.insert(bytes, make_bytes<hash_type>(0xF3, 0xE3, 0xD3), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:0000:r:(null);"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001:l:00001000:r:00001100;"
        "n:00001000:l:(null):r:(null);"
        "n:00001100:l:(null):r:(null);");

    bytes = make_bytes<PartialLabel>(0x10);
    trie.insert(bytes, make_bytes<hash_type>(0xF4, 0xE4, 0xD4), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:(null);"
        "n:000:l:0000:r:00010000;"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001:l:00001000:r:00001100;"
        "n:00001000:l:(null):r:(null);"
        "n:00001100:l:(null):r:(null);"
        "n:00010000:l:(null):r:(null);");

    bytes = make_bytes<PartialLabel>(0x05);
    trie.insert(bytes, make_bytes<hash_type>(0xF5, 0xE5, 0xD5), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:(null);"
        "n:000:l:0000:r:00010000;"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:0000010:r:00000111;"
        "n:0000010:l:00000100:r:00000101;"
        "n:00000100:l:(null):r:(null);"
        "n:00000101:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001:l:00001000:r:00001100;"
        "n:00001000:l:(null):r:(null);"
        "n:00001100:l:(null):r:(null);"
        "n:00010000:l:(null):r:(null);");

    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:(null);"
        "n:000:l:0000:r:00010000;"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:0000010:r:00000111;"
        "n:0000010:l:00000100:r:00000101;"
        "n:00000100:l:(null):r:(null);"
        "n:00000101:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001:l:00001000:r:00001100;"
        "n:00001000:l:(null):r:(null);"
        "n:00001100:l:(null):r:(null);"
        "n:00010000:l:(null):r:(null);");
}

TEST(CompressedTrieTests, StoredInsertInPartialLabelTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    DoInsertInPartialLabelTest(trie);
}

TEST(CompressedTrieTests, LinkedInsertInPartialLabelTest)
{
    CompressedTrie trie({}, TrieType::Linked);
    DoInsertInPartialLabelTest(trie);
}

void DoLookupTest(CompressedTrie &trie)
{
    partial_label_hash_batch_type label_payload_batch{
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0x11, 0x01),
                                       make_bytes<hash_type>(0xA0, 0xB0, 0xC0) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0x01, 0x02),
                                       make_bytes<hash_type>(0xA1, 0xB1, 0xC1) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xEE, 0x03),
                                       make_bytes<hash_type>(0xA2, 0xB2, 0xC2) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xAA, 0x04),
                                       make_bytes<hash_type>(0xA3, 0xB3, 0xC3) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xCC, 0x05),
                                       make_bytes<hash_type>(0xA4, 0xB4, 0xC4) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xFF, 0x06),
                                       make_bytes<hash_type>(0xA5, 0xB5, 0xC5) }
    };
    append_proof_batch_type append_proofs;

    trie.insert(label_payload_batch, append_proofs);

    string status = trie.to_string();
    EXPECT_EQ(
        "n::l:000:r:1;"
        "n:000:l:0000000100000010:r:0001000100000001;"
        "n:0000000100000010:l:(null):r:(null);"
        "n:0001000100000001:l:(null):r:(null);"
        "n:1:l:1010101000000100:r:11;"
        "n:1010101000000100:l:(null):r:(null);"
        "n:11:l:1100110000000101:r:111;"
        "n:1100110000000101:l:(null):r:(null);"
        "n:111:l:1110111000000011:r:1111111100000110;"
        "n:1110111000000011:l:(null):r:(null);"
        "n:1111111100000110:l:(null):r:(null);",
        status);

    lookup_path_type result;

    EXPECT_EQ(false, trie.lookup(make_bytes<PartialLabel>(0xFF, 0xFF), result));
    EXPECT_EQ(true, trie.lookup(make_bytes<PartialLabel>(0xFF, 0x06), result));
    EXPECT_EQ(5, result.size());

    // First is the searched node
    EXPECT_EQ(make_bytes<vector<byte>>(0xFF, 0x06), result[0].first.to_bytes());
    // Second is the node sibling, if any
    EXPECT_EQ(make_bytes<vector<byte>>(0xEE, 0x03), result[1].first.to_bytes());
    // Third is the sibling of the direct parent of the search node
    EXPECT_EQ(make_bytes<vector<byte>>(0xCC, 0x05), result[2].first.to_bytes());
    // Fourth is the sibling of the grandparent of the search node
    EXPECT_EQ(make_bytes<vector<byte>>(0xAA, 0x04), result[3].first.to_bytes());
    // Fifth is the sibling of the great-grandparent of the search node
    PartialLabel partial_label{ 0, 0, 0 };
    EXPECT_EQ(partial_label, result[4].first);

    EXPECT_EQ(false, trie.lookup(make_bytes<PartialLabel>(0x01, 0x03), result));
    EXPECT_EQ(true, trie.lookup(make_bytes<PartialLabel>(0x11, 0x01), result));
    EXPECT_EQ(3, result.size());

    // Searched node
    EXPECT_EQ(make_bytes<vector<byte>>(0x11, 0x01), result[0].first.to_bytes());
    // Sibling
    EXPECT_EQ(make_bytes<vector<byte>>(0x01, 0x02), result[1].first.to_bytes());
    // Sibling of the parent
    partial_label = PartialLabel{ 1 };
    EXPECT_EQ(partial_label, result[2].first);
}

TEST(CompressedTrieTests, StoredLookupTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    DoLookupTest(trie);
}

TEST(CompressedTrieTests, LinkedLookupTest)
{
    CompressedTrie trie({}, TrieType::Linked);
    DoLookupTest(trie);
}

void DoFailedLookupTest(CompressedTrie &trie)
{
    partial_label_hash_batch_type label_payload_batch{
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0x11, 0x01),
                                       make_bytes<hash_type>(0xA0, 0xB0, 0xC0) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0x01, 0x02),
                                       make_bytes<hash_type>(0xA1, 0xB1, 0xC1) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xEE, 0x03),
                                       make_bytes<hash_type>(0xA2, 0xB2, 0xC2) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xAA, 0x04),
                                       make_bytes<hash_type>(0xA3, 0xB3, 0xC3) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xCC, 0x05),
                                       make_bytes<hash_type>(0xA4, 0xB4, 0xC4) },
        pair<PartialLabel, hash_type>{ make_bytes<PartialLabel>(0xFF, 0x06),
                                       make_bytes<hash_type>(0xA5, 0xB5, 0xC5) }
    };
    append_proof_batch_type append_proofs;

    trie.insert(label_payload_batch, append_proofs);

    string status = trie.to_string();
    EXPECT_EQ(
        "n::l:000:r:1;"
        "n:000:l:0000000100000010:r:0001000100000001;"
        "n:0000000100000010:l:(null):r:(null);"
        "n:0001000100000001:l:(null):r:(null);"
        "n:1:l:1010101000000100:r:11;"
        "n:1010101000000100:l:(null):r:(null);"
        "n:11:l:1100110000000101:r:111;"
        "n:1100110000000101:l:(null):r:(null);"
        "n:111:l:1110111000000011:r:1111111100000110;"
        "n:1110111000000011:l:(null):r:(null);"
        "n:1111111100000110:l:(null):r:(null);",
        status);

    lookup_path_type result;

    auto key = make_bytes<PartialLabel>(0xFF, 0xFF);

    EXPECT_EQ(false, trie.lookup(key, result));

    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{ PayloadCommitmentType::UncommitedPayload,
                       LabelType::HashedLabels,
                       TrieType::LinkedNoStorage,
                       storage };
    QueryResult qr(config, /* is_member */ false, {}, {}, result, {}, {});
    commitment_type commitment = trie.get_commitment();

    EXPECT_TRUE(qr.verify_lookup_path(commitment));
    EXPECT_EQ(5, result.size());
    EXPECT_EQ(make_bytes<vector<byte>>(0xFF, 0x06), result[0].first.to_bytes());
    EXPECT_EQ(make_bytes<vector<byte>>(0xEE, 0x03), result[1].first.to_bytes());
    EXPECT_EQ(make_bytes<vector<byte>>(0xCC, 0x05), result[2].first.to_bytes());
    EXPECT_EQ(make_bytes<vector<byte>>(0xAA, 0x04), result[3].first.to_bytes());
    PartialLabel partial_label = PartialLabel{ 0, 0, 0 };
    EXPECT_EQ(partial_label, result[4].first);

    key = make_bytes<PartialLabel>(0x11, 0x02);
    EXPECT_EQ(false, trie.lookup(key, result));

    qr = QueryResult(config, /* is_member */ false, {}, {}, result, {}, {});

    EXPECT_TRUE(qr.verify_lookup_path(commitment));
    EXPECT_EQ(3, result.size());
    EXPECT_EQ(make_bytes<vector<byte>>(0x11, 0x01), result[0].first.to_bytes());
    EXPECT_EQ(make_bytes<vector<byte>>(0x01, 0x02), result[1].first.to_bytes());
    partial_label = PartialLabel{ 1 };
    EXPECT_EQ(partial_label, result[2].first);

    key = make_bytes<PartialLabel>(0x01, 0x00);
    EXPECT_EQ(false, trie.lookup(key, result));

    qr = QueryResult(config, /* is_member */ false, {}, {}, result, {}, {});

    EXPECT_TRUE(qr.verify_lookup_path(commitment));
    EXPECT_EQ(3, result.size());
    EXPECT_EQ(make_bytes<vector<byte>>(0x11, 0x01), result[0].first.to_bytes());
    EXPECT_EQ(make_bytes<vector<byte>>(0x01, 0x02), result[1].first.to_bytes());
    partial_label = PartialLabel{ 1 };
    EXPECT_EQ(partial_label, result[2].first);
}

TEST(CompressedTrieTests, StoredFailedLookupTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    DoFailedLookupTest(trie);
}

TEST(CompressedTrieTests, LinkedFailedLookupTest)
{
    CompressedTrie trie({}, TrieType::Linked);
    DoFailedLookupTest(trie);
}

TEST(CompressedTrieTests, SaveLoadTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    append_proof_type append_proof;

    trie.insert(
        make_bytes<PartialLabel>(0b00010000),
        make_bytes<hash_type>(0x01, 0x02, 0x03, 0x04, 0x05),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b00001110),
        make_bytes<hash_type>(0x02, 0x03, 0x04, 0x05, 0x06),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b00001100),
        make_bytes<hash_type>(0x03, 0x04, 0x05, 0x06, 0x07),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b00001000),
        make_bytes<hash_type>(0x04, 0x05, 0x06, 0x07, 0x08),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b00000111),
        make_bytes<hash_type>(0x05, 0x06, 0x07, 0x08, 0x09),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b00000100),
        make_bytes<hash_type>(0x06, 0x07, 0x08, 0x09, 0x0A),
        append_proof);

    string status = trie.to_string();

    EXPECT_EQ(
        "n::l:000:r:(null);"
        "n:000:l:0000:r:00010000;"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001:l:00001000:r:000011;"
        "n:00001000:l:(null):r:(null);"
        "n:000011:l:00001100:r:00001110;"
        "n:00001100:l:(null):r:(null);"
        "n:00001110:l:(null):r:(null);"
        "n:00010000:l:(null):r:(null);",
        status);

    stringstream ss;

    size_t save_size = trie.save(ss);

    auto loaded = CompressedTrie::Load(ss, storage);
    EXPECT_EQ(loaded.second, save_size);
    CompressedTrie &trie2 = *loaded.first;

    string status2 = trie2.to_string();
    EXPECT_EQ(status2, status);
    EXPECT_EQ(trie.epoch(), trie2.epoch());
}

TEST(CompressedTrieTests, SaveLoadTest2)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    append_proof_type append_proof;

    trie.insert(
        make_bytes<PartialLabel>(0b10010000),
        make_bytes<hash_type>(0x01, 0x02, 0x03, 0x04, 0x05),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b10001110),
        make_bytes<hash_type>(0x02, 0x03, 0x04, 0x05, 0x06),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b10001100),
        make_bytes<hash_type>(0x03, 0x04, 0x05, 0x06, 0x07),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b10001000),
        make_bytes<hash_type>(0x04, 0x05, 0x06, 0x07, 0x08),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b10000111),
        make_bytes<hash_type>(0x05, 0x06, 0x07, 0x08, 0x09),
        append_proof);
    trie.insert(
        make_bytes<PartialLabel>(0b10000100),
        make_bytes<hash_type>(0x06, 0x07, 0x08, 0x09, 0x0A),
        append_proof);

    string status = trie.to_string();

    EXPECT_EQ(
        "n::l:(null):r:100;"
        "n:100:l:1000:r:10010000;"
        "n:1000:l:100001:r:10001;"
        "n:100001:l:10000100:r:10000111;"
        "n:10000100:l:(null):r:(null);"
        "n:10000111:l:(null):r:(null);"
        "n:10001:l:10001000:r:100011;"
        "n:10001000:l:(null):r:(null);"
        "n:100011:l:10001100:r:10001110;"
        "n:10001100:l:(null):r:(null);"
        "n:10001110:l:(null):r:(null);"
        "n:10010000:l:(null):r:(null);",
        status);

    stringstream ss;

    size_t save_size = trie.save(ss);

    auto loaded = CompressedTrie::Load(ss, storage);
    EXPECT_EQ(loaded.second, save_size);
    CompressedTrie &trie2 = *loaded.first;

    string status2 = trie2.to_string();
    EXPECT_EQ(status2, status);
    EXPECT_EQ(trie.epoch(), trie2.epoch());
}

TEST(CompressedTrieTests, SaveLoadTest3)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie1(storage, TrieType::Stored);

    append_proof_type append_proof;
    vector<PartialLabel> labels;

    for (size_t i = 0; i < 10000; i++) {
        vector<byte> label_data(32);
        PartialLabel label{};
        hash_type payload{};

        get_random_bytes(label_data.data(), label_data.size());
        get_random_bytes(payload.data(), payload.size());
        label = PartialLabel(utils::compute_key_hash(label_data));

        trie1.insert(label, payload, append_proof);

        labels.push_back(label);
    }

    stringstream ss;
    trie1.save(ss);

    auto loaded = CompressedTrie::Load(ss, storage);
    CompressedTrie &trie2 = *loaded.first;

    for (size_t i = 0; i < labels.size(); i++) {
        lookup_path_type path1;
        lookup_path_type path2;

        auto lookup_result1 = trie1.lookup(labels[i], path1);
        auto lookup_result2 = trie2.lookup(labels[i], path2);

        EXPECT_EQ(lookup_result1, lookup_result2);
        EXPECT_EQ(path1.size(), path2.size());
    }
}

TEST(CompressedTrieTests, SaveLoadToVectorTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie1(storage, TrieType::Stored);

    append_proof_type append_proof;
    vector<PartialLabel> labels;

    for (size_t i = 0; i < 10000; i++) {
        vector<byte> label_data(32);
        PartialLabel label{};
        hash_type payload{};

        get_random_bytes(label_data.data(), label_data.size());
        get_random_bytes(payload.data(), payload.size());
        label = PartialLabel(utils::compute_key_hash(label_data));

        trie1.insert(label, payload, append_proof);

        labels.push_back(label);
    }

    vector<byte> vec;
    trie1.save(vec);

    auto loaded = CompressedTrie::Load(vec, storage);
    CompressedTrie &trie2 = *loaded.first;

    for (size_t i = 0; i < labels.size(); i++) {
        lookup_path_type path1;
        lookup_path_type path2;

        auto lookup_result1 = trie1.lookup(labels[i], path1);
        auto lookup_result2 = trie2.lookup(labels[i], path2);

        EXPECT_EQ(lookup_result1, lookup_result2);
        EXPECT_EQ(path1.size(), path2.size());
    }
}

TEST(CompressedTrieTests, LinkedSaveStoredQueryWithUpdatesTest)
{
    shared_ptr<storage::BatchStorage> storage = make_shared<UpdatedNodesPerEpochStorage>();
    shared_ptr<storage::Storage> batching_storage =
        make_shared<storage::MemoryStorageBatchInserter>(storage);
    shared_ptr<storage::Storage> cache1 =
        make_shared<storage::MemoryStorageCache>(batching_storage, 100000);
    shared_ptr<storage::Storage> cache2 = make_shared<storage::MemoryStorageCache>(storage, 100000);
    CompressedTrie trie1(cache1, TrieType::Linked);

    vector<PartialLabel> labels;

    auto insert_random_elems = [&trie1, &labels]() {
        partial_label_hash_batch_type batch(10000);
        size_t added_labels = 0;

        for (size_t idx = 0; idx < batch.size(); idx++) {
            vector<byte> label_data(8);
            PartialLabel label{};
            hash_type payload{};

            get_random_bytes(label_data.data(), label_data.size());
            get_random_bytes(payload.data(), 10);
            label = PartialLabel(utils::compute_key_hash(label_data));

            batch[idx].first = label;
            batch[idx].second = payload;

            byte coin;
            get_random_bytes(&coin, sizeof(unsigned char));

            if (coin > (byte)127 && added_labels < 100) {
                labels.push_back(batch[idx].first);
                added_labels++;
            }
        }

        append_proof_batch_type append_proofs;
        trie1.insert(batch, append_proofs);
        trie1.storage()->flush(trie1.id());
    };

    auto query_labels = [&labels](CompressedTrie &trie) {
        lookup_path_type lookup_path;

        for (auto &lbl : labels) {
            EXPECT_TRUE(trie.lookup(lbl, lookup_path));
        }
    };

    commitment_type comm1;
    commitment_type comm2;

    // First insert populates using single thread
    insert_random_elems();
    EXPECT_GT(labels.size(), 0);

    comm1 = trie1.get_commitment();

    // Now load tree in stored mode
    auto loaded = CompressedTrie::LoadFromStorage(trie1.id(), cache2);
    EXPECT_TRUE(loaded.second);
    CompressedTrie &trie2 = *loaded.first;

    comm2 = trie2.get_commitment();
    EXPECT_EQ(comm1, comm2);
    query_labels(trie2);

    // Second insert should use multi threading
    insert_random_elems();
    EXPECT_GT(labels.size(), 0);
    EXPECT_EQ(trie1.epoch(), trie2.epoch() + 1);
    comm1 = trie1.get_commitment();
    EXPECT_NE(comm1, comm2);

    trie2.storage()->load_updated_elements(trie1.epoch(), trie2.id(), trie2.storage());
    loaded = CompressedTrie::LoadFromStorage(trie2.id(), cache2);
    EXPECT_TRUE(loaded.second);
    CompressedTrie &trie3 = *loaded.first;

    EXPECT_EQ(trie1.epoch(), trie3.epoch());
    comm2 = trie3.get_commitment();
    EXPECT_EQ(comm1, comm2);

    query_labels(trie3);

    // Third insert should use multi threading as well
    insert_random_elems();
    EXPECT_GT(labels.size(), 0);
    comm1 = trie1.get_commitment();
    EXPECT_NE(comm1, comm2);

    // Fourth multi-threaded insert for good measure
    insert_random_elems();
    EXPECT_GT(labels.size(), 0);
    EXPECT_EQ(trie1.epoch(), trie3.epoch() + 2);
    commitment_type comm3;
    comm3 = trie1.get_commitment();
    EXPECT_NE(comm1, comm3);
    EXPECT_NE(comm3, comm2);

    trie3.storage()->load_updated_elements(trie3.epoch() + 1, trie3.id(), trie3.storage());
    trie3.storage()->load_updated_elements(trie3.epoch() + 2, trie3.id(), trie3.storage());
    loaded = CompressedTrie::LoadFromStorage(trie3.id(), cache2);
    CompressedTrie &trie4 = *loaded.first;

    EXPECT_EQ(trie1.epoch(), trie4.epoch());
    comm1 = trie1.get_commitment();
    comm2 = trie4.get_commitment();
    EXPECT_EQ(comm1, comm2);

    query_labels(trie4);
}

void DoEmptyTreesTest(CompressedTrie &trie1, CompressedTrie &trie2)
{
    commitment_type comm1;
    commitment_type comm2;
    comm1 = trie1.get_commitment();
    comm2 = trie2.get_commitment();

    EXPECT_EQ(comm1, comm2);
}

TEST(CompressedTrieTests, StoredEmptyTreesTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie1(storage, TrieType::Stored);
    CompressedTrie trie2(storage, TrieType::Stored);

    DoEmptyTreesTest(trie1, trie2);
}

TEST(CompressedTrieTests, LinkedEmptyTreesTest)
{
    CompressedTrie trie1({}, TrieType::Linked);
    CompressedTrie trie2({}, TrieType::Linked);

    DoEmptyTreesTest(trie1, trie2);
}

TEST(CompressedTrieTests, LoadFromStorageTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie_stored(storage, TrieType::Stored);

    PartialLabel key1 = make_bytes<PartialLabel>(1, 2, 3, 4, 5);
    PartialLabel key2 = make_bytes<PartialLabel>(2, 3, 4, 5, 6);
    PartialLabel key3 = make_bytes<PartialLabel>(3, 4, 5, 6, 7);
    PartialLabel key4 = make_bytes<PartialLabel>(4, 5, 6, 7, 8);
    PartialLabel key5 = make_bytes<PartialLabel>(5, 6, 7, 8, 9);
    PartialLabel key6 = make_bytes<PartialLabel>(6, 7, 8, 9, 10);

    hash_type pl1 = make_bytes<hash_type>(9, 8, 7, 6, 5);
    hash_type pl2 = make_bytes<hash_type>(8, 7, 6, 5, 4);
    hash_type pl3 = make_bytes<hash_type>(7, 6, 5, 4, 3);
    hash_type pl4 = make_bytes<hash_type>(6, 5, 4, 3, 2);
    hash_type pl5 = make_bytes<hash_type>(5, 4, 3, 2, 1);

    append_proof_type append_proof;

    trie_stored.insert(key1, pl1, append_proof);
    trie_stored.insert(key2, pl2, append_proof);
    trie_stored.insert(key3, pl3, append_proof);
    trie_stored.insert(key4, pl4, append_proof);
    trie_stored.insert(key5, pl5, append_proof);

    auto loaded = CompressedTrie::LoadFromStorageWithChildren(trie_stored.id(), storage);
    CompressedTrie trie_linked = *loaded.first;
    EXPECT_EQ(TrieType::Linked, trie_linked.trie_type());

    lookup_path_type lookup_path;
    EXPECT_TRUE(trie_linked.lookup(key1, lookup_path));
    EXPECT_TRUE(trie_linked.lookup(key2, lookup_path));
    EXPECT_TRUE(trie_linked.lookup(key3, lookup_path));
    EXPECT_TRUE(trie_linked.lookup(key4, lookup_path));
    EXPECT_TRUE(trie_linked.lookup(key5, lookup_path));
    EXPECT_FALSE(trie_linked.lookup(key6, lookup_path));
}

TEST(CompressedTrieTests, LoadFromStorageTest2)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Linked);

    PartialLabel key1 = make_bytes<PartialLabel>(1, 2, 3, 4, 5);
    PartialLabel key2 = make_bytes<PartialLabel>(2, 3, 4, 5, 6);
    PartialLabel key3 = make_bytes<PartialLabel>(3, 4, 5, 6, 7);
    PartialLabel key4 = make_bytes<PartialLabel>(4, 5, 6, 7, 8);
    PartialLabel key5 = make_bytes<PartialLabel>(5, 6, 7, 8, 9);
    PartialLabel key6 = make_bytes<PartialLabel>(6, 7, 8, 9, 10);

    hash_type pl1 = make_bytes<hash_type>(9, 8, 7, 6, 5);
    hash_type pl2 = make_bytes<hash_type>(8, 7, 6, 5, 4);
    hash_type pl3 = make_bytes<hash_type>(7, 6, 5, 4, 3);
    hash_type pl4 = make_bytes<hash_type>(6, 5, 4, 3, 2);
    hash_type pl5 = make_bytes<hash_type>(5, 4, 3, 2, 1);

    append_proof_type append_proof;

    trie.insert(key1, pl1, append_proof);
    trie.insert(key2, pl2, append_proof);
    trie.insert(key3, pl3, append_proof);
    trie.insert(key4, pl4, append_proof);
    trie.insert(key5, pl5, append_proof);

    auto loaded = CompressedTrie::LoadFromStorageWithChildren(trie.id(), storage);
    CompressedTrie &trie_linked = *loaded.first;
    EXPECT_EQ(TrieType::Linked, trie_linked.trie_type());

    lookup_path_type lookup_path;
    EXPECT_TRUE(trie_linked.lookup(key1, lookup_path));
    EXPECT_TRUE(trie_linked.lookup(key2, lookup_path));
    EXPECT_TRUE(trie_linked.lookup(key3, lookup_path));
    EXPECT_TRUE(trie_linked.lookup(key4, lookup_path));
    EXPECT_TRUE(trie_linked.lookup(key5, lookup_path));
    EXPECT_FALSE(trie_linked.lookup(key6, lookup_path));
}
