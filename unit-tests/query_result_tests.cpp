// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <iostream>

// VRF
#include <vrf/ec/ecvrf.h>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/insert_result.h"
#include "oZKS/query_result.h"
#include "oZKS/query_result_generated.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"

// GTest
#include "gtest/gtest.h"

using namespace ozks;
using namespace ozks::storage;
using namespace std;

namespace {
    QueryResult RoundTripQuery(
        const OZKSConfig &config,
        const key_type &key,
        const lookup_path_type &path,
        size_t epoch = 0)
    {
        QueryResult result(config, key, {}, path, randomness_type{}, epoch);
        vector<byte> serialized;
        result.save(serialized);
        return QueryResult::Load(config, serialized).first;
    }
} // namespace

TEST(QueryResultTests, QueryPathsAreBoundToRequestedKey)
{
    for (auto trie_type : { TrieType::Stored, TrieType::Linked, TrieType::LinkedNoStorage }) {
        for (size_t count : { 0, 1, 2, 8, 32 }) {
            auto storage = make_shared<MemoryStorage>();
            OZKSConfig config(
                PayloadCommitmentType::UncommitedPayload,
                LabelType::HashedLabels,
                trie_type,
                storage);
            CompressedTrie trie(storage, trie_type);
            append_proof_type insertion;
            for (size_t i = 0; i < count; i++) {
                key_type key{ static_cast<byte>(i) };
                trie.insert(
                    PartialLabel(utils::compute_key_hash(key)),
                    utils::compute_payload_commitment({}, config.payload_commitment(), {}),
                    insertion);
            }
            Commitment commitment(nullptr, trie.get_commitment());
            size_t rejected_substitutions = 0;
            for (size_t i = 0; i < 64; i++) {
                key_type key{ static_cast<byte>(i) };
                lookup_path_type path;
                ASSERT_EQ(i < count, trie.lookup(PartialLabel(utils::compute_key_hash(key)), path));
                auto honest = RoundTripQuery(config, key, path, i < count ? i + 1 : 0);
                EXPECT_EQ(
                    i < count ? QueryVerificationResult::ValidIsMember
                              : QueryVerificationResult::ValidNotMember,
                    honest.verify(commitment));

                if (i >= count) {
                    continue;
                }
                for (size_t j = 0; j < count; j++) {
                    key_type other_key{ static_cast<byte>(j) };
                    lookup_path_type other_path;
                    ASSERT_TRUE(
                        trie.lookup(PartialLabel(utils::compute_key_hash(other_key)), other_path));
                    auto substituted = RoundTripQuery(config, key, other_path, i + 1);
                    auto verified = substituted.verify(commitment);
                    EXPECT_NE(QueryVerificationResult::ValidNotMember, verified);
                    EXPECT_EQ(
                        substituted.is_member() ? QueryVerificationResult::ValidIsMember
                                                : QueryVerificationResult::NotValid,
                        verified);
                    rejected_substitutions += verified == QueryVerificationResult::NotValid;
                }
            }
            if (count >= 8) {
                EXPECT_GT(rejected_substitutions, 0);
            }
        }
    }
}

TEST(QueryResultTests, RejectsUnopenedSubtreeContainingQuery)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::UncommitedPayload,
        LabelType::HashedLabels,
        TrieType::Stored,
        storage);
    key_type key{ byte{ 1 } };
    PartialLabel label(utils::compute_key_hash(key));
    PartialLabel prefix{ label[0] };
    hash_type hash = utils::compute_hash(key);
    auto root = label[0] ? utils::compute_node_hash({}, {}, prefix, hash)
                         : utils::compute_node_hash(prefix, hash, {}, {});
    auto result = RoundTripQuery(config, key, { { prefix, hash } });
    EXPECT_EQ(QueryVerificationResult::NotValid, result.verify_lookup_path(root));
}

TEST(QueryResultTests, RejectsUnversionedQueryResult)
{
    OZKSConfig config(
        PayloadCommitmentType::UncommitedPayload,
        LabelType::HashedLabels,
        TrieType::Stored,
        make_shared<MemoryStorage>());
    flatbuffers::FlatBufferBuilder builder;
    auto empty = builder.CreateVector(vector<uint8_t>{});
    auto randomness = builder.CreateVector(vector<uint8_t>(randomness_type{}.size()));
    fbs::QueryResultBuilder query(builder);
    query.add_key(empty);
    query.add_payload(empty);
    query.add_randomness(randomness);
    auto result = query.Finish();
    builder.FinishSizePrefixed(result);
    vector<uint8_t> serialized(
        builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    EXPECT_THROW(QueryResult::Load(config, serialized), runtime_error);
}

TEST(QueryResultTests, AuthenticatesQueriedLeafInSiblingPosition)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::UncommitedPayload,
        LabelType::HashedLabels,
        TrieType::Stored,
        storage);
    CompressedTrie trie(storage, TrieType::Stored);
    key_type first_key{ byte{ 1 } }, second_key{ byte{ 2 } };
    payload_type first_payload{ byte{ 3 } }, second_payload{ byte{ 4 } };
    append_proof_type path;
    trie.insert(
        PartialLabel(utils::compute_key_hash(first_key)),
        utils::compute_payload_commitment(first_payload, config.payload_commitment(), {}),
        path);
    trie.insert(
        PartialLabel(utils::compute_key_hash(second_key)),
        utils::compute_payload_commitment(second_payload, config.payload_commitment(), {}),
        path);
    ASSERT_TRUE(trie.lookup(PartialLabel(utils::compute_key_hash(first_key)), path));
    ASSERT_EQ(2, path.size());
    ASSERT_EQ(PartialLabel(utils::compute_key_hash(second_key)), path[1].first);
    for (bool tamper : { false, true }) {
        QueryResult query(config, second_key, tamper ? first_payload : second_payload, path, {}, 2);
        vector<byte> serialized;
        query.save(serialized);
        auto loaded = QueryResult::Load(config, serialized);
        EXPECT_EQ(
            tamper ? QueryVerificationResult::NotValid : QueryVerificationResult::ValidIsMember,
            loaded.first.verify(Commitment(nullptr, trie.get_commitment())));
    }
}

TEST(QueryResultTests, RejectsMalformedPathsWithoutThrowing)
{
    OZKSConfig config(
        PayloadCommitmentType::UncommitedPayload,
        LabelType::HashedLabels,
        TrieType::Stored,
        make_shared<MemoryStorage>());
    key_type key{ byte{ 1 } };
    PartialLabel label(utils::compute_key_hash(key));
    PartialLabel prefix{ label[0] };
    hash_type hash = utils::compute_hash(key);
    vector<lookup_path_type> malformed{ {},
                                        { { {}, hash } },
                                        { { label, hash }, { label, hash } },
                                        { { label, hash }, { prefix, hash } },
                                        { { prefix, hash }, { label, hash } },
                                        { { label, hash }, { {}, {} } },
                                        lookup_path_type(
                                            PartialLabel::MaxBitCount + 2, { label, hash }) };
    for (const auto &path : malformed) {
        auto result = RoundTripQuery(config, key, path);
        EXPECT_EQ(QueryVerificationResult::NotValid, result.verify_lookup_path(hash));
    }
}

TEST(QueryResultTests, RejectsShortenedLeafLabelAfterSerialization)
{
    OZKSConfig config(
        PayloadCommitmentType::UncommitedPayload,
        LabelType::HashedLabels,
        TrieType::Stored,
        make_shared<MemoryStorage>());
    key_type key{ byte{ 0 } };
    PartialLabel label;
    bool found = false;
    for (size_t candidate = 0; candidate < 256; candidate++) {
        key[0] = static_cast<byte>(candidate);
        label = PartialLabel(utils::compute_key_hash(key));
        if (!label[PartialLabel::MaxBitCount - 1]) {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);

    CompressedTrie trie(make_shared<MemoryStorage>(), TrieType::Stored);
    append_proof_type path;
    trie.insert(
        label, utils::compute_payload_commitment({}, config.payload_commitment(), {}), path);
    auto root = trie.get_commitment();
    QueryResult honest(config, key, {}, path, randomness_type{}, trie.epoch());
    EXPECT_EQ(QueryVerificationResult::ValidIsMember, honest.verify_lookup_path(root));
    EXPECT_TRUE(InsertResult(root, path).verify());

    array<byte, PartialLabel::SaveSize> saved_label{};
    label.save(saved_label);
    uint32_t shortened_length = PartialLabel::MaxBitCount - 1;
    memcpy(
        saved_label.data() + PartialLabel::ByteCount, &shortened_length, sizeof(shortened_length));
    path[0].first.load(saved_label);
    ASSERT_EQ(shortened_length, path[0].first.bit_count());
    ASSERT_EQ(0, memcmp(label.data(), path[0].first.data(), PartialLabel::ByteCount));
    QueryResult forged(config, key, {}, path, randomness_type{}, trie.epoch());
    vector<byte> serialized;
    forged.save(serialized);
    auto loaded = QueryResult::Load(config, serialized);
    EXPECT_EQ(QueryVerificationResult::NotValid, loaded.first.verify_lookup_path(root));

    InsertResult forged_insert(root, path);
    serialized.clear();
    forged_insert.save(serialized);
    EXPECT_FALSE(InsertResult::Load(serialized).first.verify());
}

TEST(QueryResultTests, RejectsSerializedLabelModeMismatch)
{
    key_type key{ byte{ 1 } };
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    auto pk = sk->get_public_key();
    auto proof = sk->get_vrf_proof(utils::compute_key_hash(key));
    for (auto mode : { LabelType::VRFLabels, LabelType::HashedLabels }) {
        auto storage = make_shared<MemoryStorage>();
        OZKSConfig config(
            PayloadCommitmentType::UncommitedPayload, mode, TrieType::Stored, storage);
        OZKSConfig other(
            config.payload_commitment(),
            mode == LabelType::VRFLabels ? LabelType::HashedLabels : LabelType::VRFLabels,
            TrieType::Stored,
            storage);
        hash_type label = utils::compute_key_hash(key);
        if (mode == LabelType::VRFLabels) {
            utils::copy_bytes_to_hash(proof->get_vrf_value(), label);
        }
        CompressedTrie trie(storage, TrieType::Stored);
        append_proof_type path;
        trie.insert(
            PartialLabel(label),
            utils::compute_payload_commitment({}, config.payload_commitment(), {}),
            path);
        QueryResult honest(config, key, {}, path, *proof, {}, 1);
        Commitment commitment(
            mode == LabelType::VRFLabels ? pk.get() : nullptr, trie.get_commitment());
        vector<byte> serialized;
        honest.save(serialized);
        EXPECT_EQ(
            QueryVerificationResult::ValidIsMember,
            QueryResult::Load(config, serialized).first.verify(commitment));
        EXPECT_THROW(QueryResult::Load(other, serialized), runtime_error);
        stringstream stream;
        honest.save(stream);
        EXPECT_THROW(QueryResult::Load(other, stream), runtime_error);
    }
}

TEST(QueryResultTests, RejectsDowngradedFalseExclusion)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig trusted(
        PayloadCommitmentType::UncommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    OZKSConfig downgraded(
        trusted.payload_commitment(), LabelType::HashedLabels, TrieType::Stored, storage);
    key_type key{ byte{ 1 } };
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    auto pk = sk->get_public_key();
    auto proof = sk->get_vrf_proof(utils::compute_key_hash(key));
    hash_type label;
    utils::copy_bytes_to_hash(proof->get_vrf_value(), label);
    CompressedTrie trie(storage, TrieType::Stored);
    append_proof_type path;
    trie.insert(
        PartialLabel(label),
        utils::compute_payload_commitment({}, trusted.payload_commitment(), {}),
        path);
    Commitment commitment(pk.get(), trie.get_commitment());
    QueryResult honest(trusted, key, {}, path, *proof, {}, 1);
    ASSERT_EQ(QueryVerificationResult::ValidIsMember, honest.verify(commitment));

    ASSERT_FALSE(trie.lookup(PartialLabel(utils::compute_key_hash(key)), path));
    QueryResult forged(downgraded, key, {}, path, {}, 0);
    ASSERT_EQ(QueryVerificationResult::ValidNotMember, forged.verify(commitment));
    vector<byte> serialized;
    forged.save(serialized);
    EXPECT_THROW(QueryResult::Load(trusted, serialized), runtime_error);
}

TEST(QueryResultTests, RequiresVRFProofAndPublicKey)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::UncommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    key_type key{ byte{ 1 } }, other_key{ byte{ 2 } };
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    auto pk = sk->get_public_key();
    auto proof = sk->get_vrf_proof(utils::compute_key_hash(key));
    hash_type label;
    utils::copy_bytes_to_hash(proof->get_vrf_value(), label);
    CompressedTrie trie(storage, TrieType::Stored);
    append_proof_type path;
    trie.insert(
        PartialLabel(label),
        utils::compute_payload_commitment({}, config.payload_commitment(), {}),
        path);
    Commitment commitment(pk.get(), trie.get_commitment());
    QueryResult honest(config, key, {}, path, *proof, {}, 1);
    EXPECT_EQ(QueryVerificationResult::ValidIsMember, honest.verify(commitment));
    EXPECT_EQ(
        QueryVerificationResult::NotValid,
        honest.verify(Commitment(nullptr, trie.get_commitment())));

    QueryResult missing(config, key, {}, path, {}, 1);
    EXPECT_FALSE(missing.is_member());
    EXPECT_FALSE(missing.verify_vrf_proof(*pk));
    EXPECT_EQ(QueryVerificationResult::NotValid, missing.verify_lookup_path(trie.get_commitment()));
    EXPECT_EQ(QueryVerificationResult::NotValid, missing.verify(commitment));
    EXPECT_EQ(QueryVerificationResult::NotValid, missing.verify(Commitment()));
    EXPECT_EQ(QueryVerificationResult::NotValid, QueryResult(config).verify(commitment));
    vector<byte> serialized{ byte{ 42 } };
    EXPECT_THROW(missing.save(serialized), logic_error);
    EXPECT_EQ((vector<byte>{ byte{ 42 } }), serialized);
    stringstream stream;
    EXPECT_THROW(missing.save(stream), logic_error);
    EXPECT_TRUE(stream.str().empty());

    auto wrong_sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    auto wrong_pk = wrong_sk->get_public_key();
    EXPECT_FALSE(honest.verify_vrf_proof(*wrong_pk));
    EXPECT_EQ(
        QueryVerificationResult::NotValid,
        honest.verify(Commitment(wrong_pk.get(), trie.get_commitment())));
    QueryResult wrong_key(config, other_key, {}, path, *proof, {}, 1);
    EXPECT_EQ(
        QueryVerificationResult::ValidIsMember,
        wrong_key.verify_lookup_path(trie.get_commitment()));
    EXPECT_EQ(QueryVerificationResult::NotValid, wrong_key.verify(commitment));
    auto other_proof = sk->get_vrf_proof(utils::compute_key_hash(other_key));
    QueryResult wrong_proof(config, key, {}, path, *other_proof, {}, 1);
    EXPECT_FALSE(wrong_proof.verify_vrf_proof(*pk));
    EXPECT_EQ(QueryVerificationResult::NotValid, wrong_proof.verify(commitment));
    serialized.clear();
    wrong_proof.save(serialized);
    EXPECT_EQ(
        QueryVerificationResult::NotValid,
        QueryResult::Load(config, serialized).first.verify(commitment));
}

TEST(QueryResultTests, UninitializedVRFObjectsFailSafely)
{
    OZKSConfig config;
    key_type key{ byte{ 1 } };
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    auto pk = sk->get_public_key();
    auto proof = sk->get_vrf_proof(utils::compute_key_hash(key));
    lookup_path_type empty_tree{ { {}, {} } };
    QueryResult honest(config, key, {}, empty_tree, *proof, {}, 0);
    Commitment commitment(pk.get(), {});
    EXPECT_EQ(QueryVerificationResult::ValidNotMember, honest.verify(commitment));
    EXPECT_EQ(QueryVerificationResult::NotValid, honest.verify(Commitment()));
    vrf::ec::ECPublicKey invalid_pk;
    ASSERT_FALSE(invalid_pk.is_initialized());
    EXPECT_FALSE(honest.verify_vrf_proof(invalid_pk));
    EXPECT_EQ(QueryVerificationResult::NotValid, honest.verify(Commitment(&invalid_pk, {})));

    vrf::ec::ECProof invalid_proof;
    ASSERT_FALSE(invalid_proof.is_initialized());
    QueryResult invalid(config, key, {}, empty_tree, invalid_proof, {}, 0);
    EXPECT_FALSE(invalid.is_member());
    EXPECT_FALSE(invalid.verify_vrf_proof(*pk));
    EXPECT_EQ(QueryVerificationResult::NotValid, invalid.verify_lookup_path({}));
    EXPECT_EQ(QueryVerificationResult::NotValid, invalid.verify(commitment));
    vector<byte> serialized;
    EXPECT_THROW(invalid.save(serialized), logic_error);
    EXPECT_TRUE(serialized.empty());
}

TEST(QueryResultTests, RejectsMissingAndUnparseableSerializedVRFProof)
{
    OZKSConfig config;
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    auto proof = sk->get_vrf_proof(utils::compute_key_hash(key_type{ byte{ 1 } }));
    for (int mutation : { 0, 1, 2, 3 }) {
        SCOPED_TRACE(mutation);
        flatbuffers::FlatBufferBuilder builder;
        auto empty = builder.CreateVector(vector<uint8_t>{});
        auto randomness = builder.CreateVector(vector<uint8_t>(randomness_type{}.size()));
        flatbuffers::Offset<flatbuffers::Vector<uint8_t>> proof_data = 0;
        if (mutation != 0) {
            auto bytes = proof->to_bytes();
            if (mutation == 1) {
                bytes.clear();
            } else if (mutation == 2) {
                bytes.assign(3, byte{ 0xFF });
            } else {
                bytes.resize(1);
            }
            proof_data =
                builder.CreateVector(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
        }
        fbs::QueryResultBuilder query(builder);
        query.add_key(empty);
        query.add_payload(empty);
        query.add_randomness(randomness);
        query.add_vrf_proof(proof_data);
        query.add_use_vrf(true);
        query.add_version(ozks_serialization_version);
        auto result = query.Finish();
        builder.FinishSizePrefixed(result);
        vector<uint8_t> serialized(
            builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
        EXPECT_THROW(QueryResult::Load(config, serialized), runtime_error);
    }
}

TEST(QueryResultTests, RejectsTruncatedProofDuringVerification)
{
    OZKSConfig config;
    key_type key{ byte{ 1 } };
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    auto pk = sk->get_public_key();
    auto proof = sk->get_vrf_proof(utils::compute_key_hash(key));
    auto bytes = proof->to_bytes();
    bytes.pop_back();
    auto truncated = vrf::VRF::ProofFromBytes(bytes);
    ASSERT_NE(nullptr, truncated);
    // The dependency accepts truncated encodings as initialized objects; verification must reject
    // them.
    ASSERT_TRUE(truncated->is_initialized());
    QueryResult result(config, key, {}, { { {}, {} } }, *truncated, {}, 0);
    EXPECT_FALSE(result.verify_vrf_proof(*pk));
    EXPECT_EQ(QueryVerificationResult::NotValid, result.verify(Commitment(pk.get(), {})));
    vector<byte> serialized;
    result.save(serialized);
    auto loaded = QueryResult::Load(config, serialized);
    EXPECT_EQ(QueryVerificationResult::NotValid, loaded.first.verify(Commitment(pk.get(), {})));
}

void InitQueryResult(QueryResult &qr, const OZKSConfig &config)
{
    EXPECT_FALSE(qr.is_member());

    key_type key;
    lookup_path_type lookup_proof;
    payload_type payload;
    randomness_type randomness{};
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    vector<byte> vrf_input = utils::make_bytes<vector<byte>>(0x01, 0x02, 0x03, 0x04, 0x05, 0x06);
    auto vrf_proof = sk->get_vrf_proof(vrf_input);

    key = utils::make_bytes<key_type>(0xFE, 0xEF, 0xCD, 0xDC, 0xAB, 0xBA);
    lookup_proof.resize(3);
    lookup_proof[0].first = utils::make_bytes<PartialLabel>(0x01, 0x02, 0x03);
    lookup_proof[0].second[0] = static_cast<byte>(0xFE);
    lookup_proof[0].second[1] = static_cast<byte>(0xFD);
    lookup_proof[0].second[2] = static_cast<byte>(0xFC);
    lookup_proof[1].first = utils::make_bytes<PartialLabel>(0x04, 0x05, 0x06, 0x07);
    lookup_proof[1].second[0] = static_cast<byte>(0xDC);
    lookup_proof[1].second[1] = static_cast<byte>(0xDB);
    lookup_proof[1].second[2] = static_cast<byte>(0xDA);
    lookup_proof[2].first = utils::make_bytes<PartialLabel>(0xAA, 0xBB, 0xCC, 0xDD);
    lookup_proof[2].second[0] = static_cast<byte>(0xED);
    lookup_proof[2].second[1] = static_cast<byte>(0xEC);
    lookup_proof[2].second[2] = static_cast<byte>(0xEB);
    payload = utils::make_bytes<payload_type>(0x11, 0x22, 0x33, 0x44, 0x55);
    randomness[0] = static_cast<byte>(0xCC);
    randomness[1] = static_cast<byte>(0xDD);
    randomness[2] = static_cast<byte>(0xEE);
    randomness[3] = static_cast<byte>(0xFF);

    qr = QueryResult(config, key, payload, lookup_proof, *vrf_proof, randomness, 7);
}

void VerifyLoadedQueryResult(QueryResult &qr, const OZKSConfig &config)
{
    EXPECT_EQ(7, qr.epoch());
    EXPECT_EQ(static_cast<byte>(0xFE), qr.key()[0]);
    EXPECT_EQ(static_cast<byte>(0xEF), qr.key()[1]);
    EXPECT_EQ(static_cast<byte>(0xCD), qr.key()[2]);
    EXPECT_EQ(static_cast<byte>(0xDC), qr.key()[3]);
    EXPECT_EQ(static_cast<byte>(0xAB), qr.key()[4]);
    EXPECT_EQ(static_cast<byte>(0xBA), qr.key()[5]);
    EXPECT_EQ(3, qr.lookup_proof().size());
    EXPECT_EQ(false, qr.lookup_proof()[0].first[0]);
    EXPECT_EQ(false, qr.lookup_proof()[0].first[1]);
    EXPECT_EQ(false, qr.lookup_proof()[0].first[2]);
    EXPECT_EQ(false, qr.lookup_proof()[0].first[3]);
    EXPECT_EQ(false, qr.lookup_proof()[0].first[4]);
    EXPECT_EQ(false, qr.lookup_proof()[0].first[5]);
    EXPECT_EQ(false, qr.lookup_proof()[0].first[6]);
    EXPECT_EQ(true, qr.lookup_proof()[0].first[7]);

    EXPECT_EQ(static_cast<byte>(0xFE), qr.lookup_proof()[0].second[0]);
    EXPECT_EQ(static_cast<byte>(0xFD), qr.lookup_proof()[0].second[1]);
    EXPECT_EQ(static_cast<byte>(0xFC), qr.lookup_proof()[0].second[2]);

    EXPECT_EQ(false, qr.lookup_proof()[1].first[8]);
    EXPECT_EQ(false, qr.lookup_proof()[1].first[9]);
    EXPECT_EQ(false, qr.lookup_proof()[1].first[10]);
    EXPECT_EQ(false, qr.lookup_proof()[1].first[11]);
    EXPECT_EQ(false, qr.lookup_proof()[1].first[12]);
    EXPECT_EQ(true, qr.lookup_proof()[1].first[13]);
    EXPECT_EQ(false, qr.lookup_proof()[1].first[14]);
    EXPECT_EQ(true, qr.lookup_proof()[1].first[15]);

    EXPECT_EQ(static_cast<byte>(0xDC), qr.lookup_proof()[1].second[0]);
    EXPECT_EQ(static_cast<byte>(0xDB), qr.lookup_proof()[1].second[1]);
    EXPECT_EQ(static_cast<byte>(0xDA), qr.lookup_proof()[1].second[2]);

    EXPECT_EQ(true, qr.lookup_proof()[2].first[8]);
    EXPECT_EQ(false, qr.lookup_proof()[2].first[9]);
    EXPECT_EQ(true, qr.lookup_proof()[2].first[10]);
    EXPECT_EQ(true, qr.lookup_proof()[2].first[11]);
    EXPECT_EQ(true, qr.lookup_proof()[2].first[12]);
    EXPECT_EQ(false, qr.lookup_proof()[2].first[13]);
    EXPECT_EQ(true, qr.lookup_proof()[2].first[14]);
    EXPECT_EQ(true, qr.lookup_proof()[2].first[15]);

    EXPECT_EQ(static_cast<byte>(0xED), qr.lookup_proof()[2].second[0]);
    EXPECT_EQ(static_cast<byte>(0xEC), qr.lookup_proof()[2].second[1]);
    EXPECT_EQ(static_cast<byte>(0xEB), qr.lookup_proof()[2].second[2]);

    if (config.label_type() == LabelType::VRFLabels) {
        EXPECT_NE(nullptr, qr.vrf_proof());
    } else {
        EXPECT_EQ(nullptr, qr.vrf_proof());
    }
}

TEST(QueryResultTests, SaveLoadVectorTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    EXPECT_EQ(PayloadCommitmentType::CommitedPayload, config.payload_commitment());
    EXPECT_EQ(LabelType::VRFLabels, config.label_type());

    vector<byte> vec;
    QueryResult qr(config);
    InitQueryResult(qr, config);
    size_t save_size = qr.save(vec);

    auto loaded = QueryResult::Load(config, vec);
    QueryResult &qr2 = loaded.first;
    size_t load_size = loaded.second;

    EXPECT_EQ(load_size, save_size);

    VerifyLoadedQueryResult(qr2, config);
}

TEST(QueryResultTests, SaveLoadStreamTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::CommitedPayload, LabelType::VRFLabels, TrieType::Stored, storage);
    EXPECT_EQ(PayloadCommitmentType::CommitedPayload, config.payload_commitment());
    EXPECT_EQ(LabelType::VRFLabels, config.label_type());

    stringstream ss;
    QueryResult qr(config);
    InitQueryResult(qr, config);
    size_t save_size = qr.save(ss);

    auto loaded = QueryResult::Load(config, ss);
    QueryResult &qr2 = loaded.first;
    size_t load_size = loaded.second;

    EXPECT_EQ(load_size, save_size);

    VerifyLoadedQueryResult(qr2, config);
}

TEST(QueryResultTests, SaveLoadVectorNoVRFTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::UncommitedPayload, LabelType::HashedLabels, TrieType::Stored, storage
    };
    EXPECT_EQ(PayloadCommitmentType::UncommitedPayload, config.payload_commitment());
    EXPECT_EQ(LabelType::HashedLabels, config.label_type());

    vector<byte> vec;
    QueryResult qr(config);
    InitQueryResult(qr, config);
    size_t save_size = qr.save(vec);

    auto loaded = QueryResult::Load(config, vec);
    QueryResult &qr2 = loaded.first;
    size_t load_size = loaded.second;

    EXPECT_EQ(load_size, save_size);

    VerifyLoadedQueryResult(qr2, config);
}

TEST(QueryResultTests, SaveLoadStreamNoVRFTest)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config{
        PayloadCommitmentType::UncommitedPayload, LabelType::HashedLabels, TrieType::Stored, storage
    };
    EXPECT_EQ(PayloadCommitmentType::UncommitedPayload, config.payload_commitment());
    EXPECT_EQ(LabelType::HashedLabels, config.label_type());

    stringstream ss;
    QueryResult qr(config);
    InitQueryResult(qr, config);
    size_t save_size = qr.save(ss);

    auto loaded = QueryResult::Load(config, ss);
    QueryResult &qr2 = loaded.first;
    size_t load_size = loaded.second;

    EXPECT_EQ(load_size, save_size);

    VerifyLoadedQueryResult(qr2, config);
}
