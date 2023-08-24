// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <sstream>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node_stored.h"
#include "oZKS/insert_result.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

TEST(InsertResultTests, VerifyBatchInsertTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    CTNodeStored root(&trie);

    append_proof_type append_proof;
    vector<PartialLabel> labels;
    hash_type payload = make_bytes<hash_type>(0xFF, 0xFE);

    labels.emplace_back(PartialLabel{ true, true, true, true });
    labels.emplace_back(PartialLabel{ true, true, true, false });
    labels.emplace_back(PartialLabel{ true, false, false, false });
    labels.emplace_back(PartialLabel{ true, false, false, true });
    labels.emplace_back(PartialLabel{ true, false, true, true });

    for (const auto &label : labels) {
        root.insert(label, payload, /* epoch */ 1);
    }

    for (const auto &label : labels) {
        root.update_hashes(label);
    }

    string root_str = root.to_string();
    EXPECT_EQ(
        "n::l:(null):r:1;"
        "n:1:l:10:r:111;"
        "n:10:l:100:r:1011;"
        "n:100:l:1000:r:1001;"
        "n:1000:l:(null):r:(null);"
        "n:1001:l:(null):r:(null);"
        "n:1011:l:(null):r:(null);"
        "n:111:l:1110:r:1111;"
        "n:1110:l:(null):r:(null);"
        "n:1111:l:(null):r:(null);",
        root_str);

    commitment_type commitment;
    static_assert(sizeof(commitment_type) == sizeof(hash_type));
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());

    for (const auto &label : labels) {
        lookup_path_type path;
        InsertResult res;

        EXPECT_FALSE(res.initialized());
        EXPECT_TRUE(root.lookup(label, path, /* include_searched */ true));

        res.init_result(commitment, path);
        EXPECT_TRUE(res.initialized());
        EXPECT_TRUE(res.verify());
    }
}

TEST(InsertResultTests, VerifySingleInsertTest)
{
    hash_type payload = make_bytes<hash_type>(0x01, 0x02);
    vector<PartialLabel> labels;
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    CTNodeStored root(&trie);
    commitment_type commitment;

    labels.emplace_back(make_bytes<PartialLabel>(0xAA, 0xBB, 0xCC, 0xDD, 0xEE));
    labels.emplace_back(make_bytes<PartialLabel>(0x01, 0x01, 0x01, 0x01, 0x01));
    labels.emplace_back(make_bytes<PartialLabel>(0x10, 0x10, 0x10, 0x10, 0x10));
    labels.emplace_back(make_bytes<PartialLabel>(0x80, 0x80, 0x80, 0x80, 0x80));
    labels.emplace_back(make_bytes<PartialLabel>(0xC0, 0xC0, 0xC0, 0xC0, 0xC0));
    labels.emplace_back(make_bytes<PartialLabel>(0xE0, 0xE0, 0xE0, 0xE0, 0xE0));
    labels.emplace_back(make_bytes<PartialLabel>(0xF0, 0xF0, 0xF0, 0xF0, 0xF0));

    size_t epoch = 1;
    for (const auto &label : labels) {
        append_proof_type append_proof;
        InsertResult res;

        EXPECT_FALSE(res.initialized());

        root.insert(label, payload, /* epoch */ epoch++);
        root.update_hashes(label);
        EXPECT_TRUE(root.lookup(label, append_proof, /* include_searched */ true));
        copy_bytes(root.hash().data(), root.hash().size(), commitment.data());
        res.init_result(commitment, append_proof);

        EXPECT_TRUE(res.initialized());
        EXPECT_TRUE(res.verify());
    }
}

TEST(InsertResultTests, SaveLoadVectorTest)
{
    InsertResult ir;

    commitment_type commitment;
    commitment[0] = static_cast<byte>(0x80);
    commitment[1] = static_cast<byte>(0x23);
    commitment[2] = static_cast<byte>(0x7f);
    commitment[3] = static_cast<byte>(0x63);

    hash_type hash1 = utils::compute_hash(make_bytes<vector<byte>>(0x01, 0x02, 0x03), "hash");
    hash_type hash2 = utils::compute_hash(make_bytes<vector<byte>>(0x02, 0x03, 0x04), "hash");
    hash_type hash3 = utils::compute_hash(make_bytes<vector<byte>>(0x03, 0x04, 0x05), "hash");

    append_proof_type append_proof(3);
    append_proof[0].first = make_bytes<PartialLabel>(0xAA, 0xBB, 0xCC, 0xDD, 0xEE);
    append_proof[1].first = make_bytes<PartialLabel>(0xA1, 0xB1, 0xC1, 0xD1);
    append_proof[2].first = make_bytes<PartialLabel>(0xA2, 0xB2, 0xC2);
    append_proof[0].second = hash1;
    append_proof[1].second = hash2;
    append_proof[2].second = hash3;

    ir.init_result(commitment, append_proof);

    vector<uint8_t> buffer;
    size_t save_size = ir.save(buffer);

    auto loaded = InsertResult::Load(buffer);
    InsertResult &ir2 = loaded.first;
    size_t load_size = loaded.second;

    EXPECT_EQ(save_size, load_size);
    EXPECT_TRUE(ir2.initialized());
    EXPECT_EQ(ir2.commitment(), ir.commitment());
    EXPECT_EQ(ir.append_proof().size(), ir2.append_proof().size());
    EXPECT_EQ(ir.append_proof()[0].first, ir2.append_proof()[0].first);
    EXPECT_EQ(ir.append_proof()[0].second, ir2.append_proof()[0].second);
    EXPECT_EQ(ir.append_proof()[1].first, ir2.append_proof()[1].first);
    EXPECT_EQ(ir.append_proof()[1].second, ir2.append_proof()[1].second);
    EXPECT_EQ(ir.append_proof()[2].first, ir2.append_proof()[2].first);
    EXPECT_EQ(ir.append_proof()[2].second, ir2.append_proof()[2].second);
}

TEST(InsertResultTests, SaveLoadStreamTest)
{
    InsertResult ir;

    commitment_type commitment;
    commitment[0] = static_cast<byte>(0x80);
    commitment[1] = static_cast<byte>(0x23);
    commitment[2] = static_cast<byte>(0x7f);
    commitment[3] = static_cast<byte>(0x63);

    hash_type hash1 = utils::compute_hash(make_bytes<vector<byte>>(0x01, 0x02, 0x03), "hash");
    hash_type hash2 = utils::compute_hash(make_bytes<vector<byte>>(0x02, 0x03, 0x04), "hash");
    hash_type hash3 = utils::compute_hash(make_bytes<vector<byte>>(0x03, 0x04, 0x05), "hash");

    append_proof_type append_proof(3);
    append_proof[0].first = make_bytes<PartialLabel>(0xAA, 0xBB, 0xCC, 0xDD, 0xEE);
    append_proof[1].first = make_bytes<PartialLabel>(0xA1, 0xB1, 0xC1, 0xD1);
    append_proof[2].first = make_bytes<PartialLabel>(0xA2, 0xB2, 0xC2);
    append_proof[0].second = hash1;
    append_proof[1].second = hash2;
    append_proof[2].second = hash3;

    ir.init_result(commitment, append_proof);

    stringstream ss;
    size_t save_size = ir.save(ss);

    auto loaded = InsertResult::Load(ss);
    InsertResult &ir2 = loaded.first;
    size_t load_size = loaded.second;

    EXPECT_EQ(save_size, load_size);
    EXPECT_TRUE(ir2.initialized());
    EXPECT_EQ(ir2.commitment(), ir.commitment());
    EXPECT_EQ(ir.append_proof().size(), ir2.append_proof().size());
    EXPECT_EQ(ir.append_proof()[0].first, ir2.append_proof()[0].first);
    EXPECT_EQ(ir.append_proof()[0].second, ir2.append_proof()[0].second);
    EXPECT_EQ(ir.append_proof()[1].first, ir2.append_proof()[1].first);
    EXPECT_EQ(ir.append_proof()[1].second, ir2.append_proof()[1].second);
    EXPECT_EQ(ir.append_proof()[2].first, ir2.append_proof()[2].first);
    EXPECT_EQ(ir.append_proof()[2].second, ir2.append_proof()[2].second);
}
