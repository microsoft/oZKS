// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <cstddef>
#include <optional>
#include <sstream>

// VRF
#include <vrf/vrf.h>

// OZKS
#include "oZKS/commitment.h"
#include "oZKS/commitment_generated.h"
#include "oZKS/compressed_trie.h"
#include "oZKS/query_result.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks::utils;

TEST(CommitmentTests, RejectsLegacyHashFormat)
{
    EXPECT_EQ(2U, ozks_serialization_version);
    EXPECT_FALSE(same_serialization_version(1));
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    auto pk = sk->get_public_key()->to_bytes();
    flatbuffers::FlatBufferBuilder builder;
    auto data = builder.CreateVector(reinterpret_cast<const uint8_t *>(pk.data()), pk.size());
    auto public_key = fbs::CreatePublicKey(builder, data);
    hash_type hash{};
    fbs::RootCommitment root(
        flatbuffers::span<const uint8_t, 32>{ reinterpret_cast<const uint8_t *>(hash.data()),
                                              hash.size() });
    auto commitment = fbs::CreateCommitment(builder, 1, public_key, &root);
    builder.FinishSizePrefixed(commitment);
    vector<uint8_t> serialized(
        builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    EXPECT_THROW(Commitment::Load(serialized), runtime_error);
}

namespace {
    // Serialize a commitment by hand with the given key bytes and stated VRF suite. A stated
    // value of nullopt omits the field.
    vector<uint8_t> build_commitment(
        const vector<byte> &pk_bytes, const commitment_type &root_c, optional<uint8_t> stated)
    {
        flatbuffers::FlatBufferBuilder builder;
        auto data = builder.CreateVector(
            reinterpret_cast<const uint8_t *>(pk_bytes.data()), pk_bytes.size());
        auto public_key = fbs::CreatePublicKey(builder, data);
        fbs::RootCommitment root(flatbuffers::span<const uint8_t, 32>{
            reinterpret_cast<const uint8_t *>(root_c.data()), root_c.size() });
        fbs::CommitmentBuilder commitment_builder(builder);
        commitment_builder.add_version(ozks_serialization_version);
        commitment_builder.add_public_key(public_key);
        commitment_builder.add_root_commitment(&root);
        if (stated.has_value()) {
            commitment_builder.add_vrf_type(*stated);
        }
        builder.FinishSizePrefixed(commitment_builder.Finish());
        return { builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize() };
    }

    void expect_load_error(const vector<uint8_t> &buffer, const char *message)
    {
        try {
            Commitment::Load(buffer);
            FAIL() << "Load should have thrown: " << message;
        } catch (const runtime_error &ex) {
            EXPECT_STREQ(message, ex.what());
        }
    }
} // namespace

TEST(CommitmentTests, PinsVRFType)
{
    constexpr auto ec_type = vrf::Type::ec_vrf_p256_sha256_tai;
    constexpr auto rsa_type = vrf::Type::rsa_fdh_vrf_rsa2048_sha256;
    const uint8_t ec_byte = std::to_integer<uint8_t>(vrf::to_byte(ec_type));
    const uint8_t rsa_byte = std::to_integer<uint8_t>(vrf::to_byte(rsa_type));
    ASSERT_NE(ec_byte, rsa_byte);
    auto ec_pk = vrf::VRF::Create(ec_type)->get_public_key();
    auto rsa_pk = vrf::VRF::Create(rsa_type)->get_public_key();
    commitment_type root_c = make_bytes<commitment_type>(0x01, 0x02);

    // Honest commitments state the suite of their key, as a present field, and load. The RSA
    // suite has wire value 0, so it exercises the case a plain default would have elided.
    for (const auto *pk : { ec_pk.get(), rsa_pk.get() }) {
        SCOPED_TRACE(vrf::to_string(pk->get_type()));
        vector<uint8_t> serialized;
        Commitment{ pk, root_c }.save(serialized);
        auto stated = fbs::GetSizePrefixedCommitment(serialized.data())->vrf_type();
        ASSERT_TRUE(stated.has_value());
        EXPECT_EQ(std::to_integer<uint8_t>(vrf::to_byte(pk->get_type())), *stated);
        EXPECT_EQ(pk->get_type(), Commitment::Load(serialized).first.public_key()->get_type());
    }

    // The stated suite must be present and must match the key the commitment carries.
    vector<byte> ec_bytes = ec_pk->to_bytes();
    vector<byte> rsa_bytes = rsa_pk->to_bytes();
    const char *missing = "Failed to load Commitment: VRF type is missing";
    const char *mismatch = "Failed to load Commitment: VRF type does not match public key";
    expect_load_error(build_commitment(ec_bytes, root_c, nullopt), missing);
    expect_load_error(build_commitment(rsa_bytes, root_c, nullopt), missing);
    expect_load_error(build_commitment(ec_bytes, root_c, rsa_byte), mismatch);
    expect_load_error(build_commitment(rsa_bytes, root_c, ec_byte), mismatch);
    expect_load_error(build_commitment(ec_bytes, root_c, uint8_t{ 0xFF }), mismatch);
    // Controls: the hand-built buffers load when the stated suite matches.
    auto ec_control = build_commitment(ec_bytes, root_c, ec_byte);
    auto rsa_control = build_commitment(rsa_bytes, root_c, rsa_byte);
    EXPECT_EQ(root_c, Commitment::Load(ec_control).first.root_commitment());
    EXPECT_EQ(root_c, Commitment::Load(rsa_control).first.root_commitment());
}

TEST(CommitmentTests, HashedLabelCommitmentHasNoKey)
{
    // A hashed-label commitment carries no VRF public key. It must serialize, load back with
    // a null key, and still verify hashed-label query results.
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::UncommitedPayload,
        LabelType::HashedLabels,
        TrieType::Stored,
        storage);
    CompressedTrie trie(storage, TrieType::Stored);
    key_type key{ byte{ 1 } };
    append_proof_type insertion;
    trie.insert(
        PartialLabel(compute_key_hash(key)),
        compute_payload_commitment({}, config.payload_commitment(), {}),
        insertion);
    Commitment commitment{ nullptr, trie.get_commitment() };
    ASSERT_EQ(nullptr, commitment.public_key());

    vector<uint8_t> serialized;
    size_t size = commitment.save(serialized);
    auto fbs_commitment = fbs::GetSizePrefixedCommitment(serialized.data());
    EXPECT_EQ(nullptr, fbs_commitment->public_key());
    EXPECT_FALSE(fbs_commitment->vrf_type().has_value());

    auto loaded = Commitment::Load(serialized);
    EXPECT_EQ(size, loaded.second);
    EXPECT_EQ(nullptr, loaded.first.public_key());
    EXPECT_EQ(trie.get_commitment(), loaded.first.root_commitment());
    stringstream stream;
    commitment.save(stream);
    auto streamed = Commitment::Load(stream);
    EXPECT_EQ(nullptr, streamed.first.public_key());
    EXPECT_EQ(trie.get_commitment(), streamed.first.root_commitment());

    lookup_path_type path;
    ASSERT_TRUE(trie.lookup(PartialLabel(compute_key_hash(key)), path));
    QueryResult result(config, key, {}, path, {}, 1);
    EXPECT_EQ(QueryVerificationResult::ValidIsMember, result.verify(loaded.first, key));

    // A loaded keyless commitment cannot authenticate a VRF-mode result.
    OZKSConfig vrf_config(
        PayloadCommitmentType::UncommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    auto proof = sk->get_vrf_proof(compute_key_hash(key));
    QueryResult vrf_result(vrf_config, key, {}, path, *proof, {}, 1);
    EXPECT_EQ(QueryVerificationResult::NotValid, vrf_result.verify(loaded.first, key));

    // A stated suite without a key is malformed, including the suite whose wire value is 0.
    constexpr auto rsa_type = vrf::Type::rsa_fdh_vrf_rsa2048_sha256;
    constexpr auto ec_type = vrf::Type::ec_vrf_p256_sha256_tai;
    for (auto suite : { rsa_type, ec_type }) {
        SCOPED_TRACE(vrf::to_string(suite));
        flatbuffers::FlatBufferBuilder builder;
        fbs::RootCommitment root(flatbuffers::span<const uint8_t, 32>{
            reinterpret_cast<const uint8_t *>(trie.get_commitment().data()), hash_size });
        fbs::CommitmentBuilder commitment_builder(builder);
        commitment_builder.add_version(ozks_serialization_version);
        commitment_builder.add_root_commitment(&root);
        commitment_builder.add_vrf_type(std::to_integer<uint8_t>(vrf::to_byte(suite)));
        builder.FinishSizePrefixed(commitment_builder.Finish());
        expect_load_error(
            { builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize() },
            "Failed to load Commitment: VRF type present without public key");
    }

    // A buffer without a root commitment fails verification before Load touches it. The
    // generated builder asserts on a missing required field in debug builds, so the malformed
    // table is assembled with the raw builder API.
    flatbuffers::FlatBufferBuilder builder;
    auto start = builder.StartTable();
    builder.AddElement<uint32_t>(fbs::Commitment::VT_VERSION, ozks_serialization_version, 0);
    auto table = builder.EndTable(start);
    builder.FinishSizePrefixed(flatbuffers::Offset<fbs::Commitment>(table));
    expect_load_error(
        { builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize() },
        "Failed to load Commitment: invalid buffer");
}

TEST(CommitmentTests, SerializeTest)
{
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    EXPECT_TRUE(sk->is_initialized());

    auto pk = sk->get_public_key();
    vector<byte> pk_vec = pk->to_bytes();

    commitment_type root_c = {};
    root_c[0] = static_cast<byte>(0x01);
    root_c[1] = static_cast<byte>(0x02);
    root_c[2] = static_cast<byte>(0x03);

    Commitment commitment{ pk.get(), root_c };

    stringstream ss;
    size_t size = commitment.save(ss);

    auto load_result = Commitment::Load(ss);

    vector<byte> pk_vec2 = load_result.first.public_key()->to_bytes();

    EXPECT_EQ(load_result.second, size);
    EXPECT_EQ(load_result.first.root_commitment(), commitment.root_commitment());

    EXPECT_EQ(pk_vec.size(), pk_vec2.size());
    for (size_t idx = 0; idx < pk_vec.size(); idx++) {
        EXPECT_EQ(pk_vec[idx], pk_vec2[idx]);
    }
}

TEST(CommitmentTests, SerializeToVectorTest)
{
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    EXPECT_TRUE(sk->is_initialized());

    auto pk = sk->get_public_key();
    vector<byte> pk_vec = pk->to_bytes();

    commitment_type root_c{};
    root_c[0] = static_cast<byte>(0x01);
    root_c[1] = static_cast<byte>(0x02);
    root_c[2] = static_cast<byte>(0x03);
    Commitment commitment{ pk.get(), root_c };

    vector<byte> vec;
    size_t size = commitment.save(vec);

    auto load_result = Commitment::Load(vec);

    vector<byte> pk_vec2 = load_result.first.public_key()->to_bytes();

    EXPECT_EQ(load_result.second, size);
    EXPECT_EQ(load_result.first.root_commitment(), commitment.root_commitment());

    EXPECT_EQ(pk_vec.size(), pk_vec2.size());
    for (size_t idx = 0; idx < pk_vec.size(); idx++) {
        EXPECT_EQ(pk_vec[idx], pk_vec2[idx]);
    }
}
