// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <sstream>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node_stored.h"
#include "oZKS/insert_result.h"
#include "oZKS/insert_result_generated.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"
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

TEST(InsertResultTests, RejectsMalformedProofsWithoutThrowing)
{
    // Build an honest single-leaf proof as the control.
    hash_type payload = make_bytes<hash_type>(0x01, 0x02);
    PartialLabel label = make_bytes<PartialLabel>(0xAA, 0xBB, 0xCC, 0xDD, 0xEE);
    PartialLabel prefix{ label[0] };
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    append_proof_type honest;
    trie.insert(label, payload, honest);
    commitment_type commitment = trie.get_commitment();
    ASSERT_TRUE(InsertResult(commitment, honest).verify());
    hash_type leaf_hash = honest[0].second;
    // A well-formed proof with the wrong leaf hash is invalid, so the control is meaningful.
    EXPECT_FALSE(InsertResult(commitment, { { label, hash_type{} } }).verify());

    // Each malformed proof must be reported as invalid and must never make verify throw.
    // `other` differs from `label` at bit 0, so a pair of them reconstructs the root.
    PartialLabel other = make_bytes<PartialLabel>(0x2A, 0xBB, 0xCC, 0xDD, 0xEE);
    ASSERT_NE(label[0], other[0]);
    vector<append_proof_type> malformed{
        {},
        { { {}, leaf_hash } },
        // The commitment itself presented as the leaf hash of the inserted label.
        { { label, commitment } },
        { { label, leaf_hash }, { label, leaf_hash } },
        { { label, leaf_hash }, { prefix, leaf_hash } },
        { { prefix, leaf_hash }, { label, leaf_hash } },
        { { label, leaf_hash }, { {}, {} } },
        // Reaches the root but does not match the commitment.
        { { label, leaf_hash }, { other, leaf_hash } },
        // Continues past the root.
        { { label, leaf_hash }, { other, leaf_hash }, { label, leaf_hash } },
        append_proof_type(PartialLabel::MaxBitCount + 2, { label, leaf_hash }),
    };
    for (size_t idx = 0; idx < malformed.size(); idx++) {
        SCOPED_TRACE(idx);
        InsertResult result(commitment, malformed[idx]);
        bool verified = true;
        EXPECT_NO_THROW(verified = result.verify());
        EXPECT_FALSE(verified);
    }

    // Verifying an uninitialized result is a programming error and still throws.
    EXPECT_THROW(InsertResult().verify(), runtime_error);
}

TEST(InsertResultTests, CarriesSerializationVersion)
{
    commitment_type commitment = make_bytes<commitment_type>(0x80, 0x23);
    append_proof_type append_proof(1);
    append_proof[0].first = make_bytes<PartialLabel>(0xAA, 0xBB);
    append_proof[0].second = utils::compute_hash(make_bytes<vector<byte>>(0x01), "hash");
    InsertResult ir(commitment, append_proof);

    // An honest round trip carries the current version, through both Load overloads.
    vector<uint8_t> buffer;
    ir.save(buffer);
    auto fbs_result = fbs::GetSizePrefixedInsertResult(buffer.data());
    EXPECT_EQ(ozks_serialization_version, fbs_result->version());
    EXPECT_EQ(commitment, InsertResult::Load(buffer).first.commitment());
    stringstream stream;
    ir.save(stream);
    EXPECT_EQ(append_proof, InsertResult::Load(stream).first.append_proof());

    // A legacy result without the field reads back as version 0; any other version is
    // foreign. The hand-built buffers carry an empty proof so that they are otherwise
    // loadable, and the current version is included as a control that must load: a throw
    // can then only come from the version check.
    for (uint32_t version :
         { 0U, 1U, ozks_serialization_version, ozks_serialization_version + 1 }) {
        SCOPED_TRACE(version);
        flatbuffers::FlatBufferBuilder builder;
        fbs::RootCommitment root(flatbuffers::span<const uint8_t, 32>{
            reinterpret_cast<const uint8_t *>(commitment.data()), commitment.size() });
        fbs::InsertResultBuilder result_builder(builder);
        result_builder.add_commitment(&root);
        result_builder.add_append_proof_count(0);
        if (version != 0) {
            result_builder.add_version(version);
        }
        builder.FinishSizePrefixed(result_builder.Finish());
        vector<uint8_t> hand_built(
            builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
        if (version == ozks_serialization_version) {
            EXPECT_EQ(commitment, InsertResult::Load(hand_built).first.commitment());
        } else {
            EXPECT_THROW(InsertResult::Load(hand_built), runtime_error);
        }
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
