// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <cstddef>
#include <set>
#include <vector>

// oZKS
#include "oZKS/ct_node_linked.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"

// GTest
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

namespace {
    // Canonical hash encoding of a label: 32 bytes in bit order, then the bit count as a
    // 4-byte big-endian integer. Built independently of the library's implementation.
    vector<byte> encode_label_for_hash(const PartialLabel &label)
    {
        vector<byte> encoded(PartialLabel::ByteCount + 4);
        // Rebuild the bytes bit by bit from operator[] so the expectation does not share
        // PartialLabel's internal byte layout.
        for (uint32_t idx = 0; idx < label.bit_count(); idx++) {
            if (label[idx]) {
                encoded[idx / 8] |= static_cast<byte>(0x80 >> (idx % 8));
            }
        }
        uint32_t bit_count = label.bit_count();
        for (size_t idx = 0; idx < 4; idx++) {
            encoded[PartialLabel::ByteCount + idx] =
                static_cast<byte>((bit_count >> (8 * (3 - idx))) & 0xFF);
        }
        return encoded;
    }

    hash_type expected_hash(const vector<byte> &input, const string &domain)
    {
        hash_type hash = compute_hash(input, domain);
        hash[0] &= byte{ 0xFE };
        return hash;
    }
} // namespace

TEST(Utilities, LeafAndNodeHashEncoding)
{
    const string version_suffix = "_v" + to_string(ozks_serialization_version);
    PartialLabel label = make_bytes<PartialLabel>(0x80, 0x01);
    ASSERT_EQ(16, label.bit_count());
    hash_type payload = make_bytes<hash_type>(0xAB, 0xCD);
    size_t epoch = 0x01020304;

    vector<byte> leaf_input = encode_label_for_hash(label);
    ASSERT_EQ(
        make_bytes<vector<byte>>(0x80, 0x01),
        vector<byte>(leaf_input.begin(), leaf_input.begin() + 2));
    ASSERT_EQ(
        make_bytes<vector<byte>>(0x00, 0x00, 0x00, 0x10),
        vector<byte>(leaf_input.begin() + 32, leaf_input.end()));
    leaf_input.insert(leaf_input.end(), payload.begin(), payload.end());
    vector<byte> epoch_bytes =
        make_bytes<vector<byte>>(0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04);
    leaf_input.insert(leaf_input.end(), epoch_bytes.begin(), epoch_bytes.end());
    ASSERT_EQ(36 + 32 + 8, leaf_input.size());
    EXPECT_EQ(
        expected_hash(leaf_input, "leaf_hash" + version_suffix),
        compute_leaf_hash(label, payload, epoch));

    PartialLabel right{ true, true };
    hash_type right_hash = make_bytes<hash_type>(0x01, 0x02);
    vector<byte> node_input = encode_label_for_hash(label);
    node_input.insert(node_input.end(), payload.begin(), payload.end());
    vector<byte> right_encoded = encode_label_for_hash(right);
    ASSERT_EQ(byte{ 0xC0 }, right_encoded[0]);
    ASSERT_EQ(byte{ 0x02 }, right_encoded[35]);
    node_input.insert(node_input.end(), right_encoded.begin(), right_encoded.end());
    node_input.insert(node_input.end(), right_hash.begin(), right_hash.end());
    ASSERT_EQ(2 * (36 + 32), node_input.size());
    EXPECT_EQ(
        expected_hash(node_input, "node_hash" + version_suffix),
        compute_node_hash(label, payload, right, right_hash));

    // The empty label and the all-zero full label differ only in the encoded bit count.
    EXPECT_NE(
        compute_leaf_hash(PartialLabel{}, payload, epoch),
        compute_leaf_hash(PartialLabel(hash_type{}), payload, epoch));

    // Full 256-bit label with a distinct byte in every position: every label word and byte
    // must come out in order.
    hash_type distinct_bytes;
    for (size_t idx = 0; idx < distinct_bytes.size(); idx++) {
        distinct_bytes[idx] = static_cast<byte>(idx + 1);
    }
    PartialLabel full(distinct_bytes);
    ASSERT_EQ(PartialLabel::MaxBitCount, full.bit_count());
    vector<byte> full_input = encode_label_for_hash(full);
    EXPECT_TRUE(equal(distinct_bytes.begin(), distinct_bytes.end(), full_input.begin()));
    EXPECT_EQ(
        make_bytes<vector<byte>>(0x00, 0x00, 0x01, 0x00),
        vector<byte>(full_input.begin() + 32, full_input.end()));
    full_input.insert(full_input.end(), payload.begin(), payload.end());
    full_input.insert(full_input.end(), epoch_bytes.begin(), epoch_bytes.end());
    EXPECT_EQ(
        expected_hash(full_input, "leaf_hash" + version_suffix),
        compute_leaf_hash(full, payload, epoch));

    // A 70-bit label crosses a word boundary and ends mid-byte: bits past the bit count
    // are encoded as zero.
    vector<byte> ones(9, byte{ 0xFF });
    PartialLabel crossing(ones, 70);
    ASSERT_EQ(70, crossing.bit_count());
    vector<byte> crossing_input = encode_label_for_hash(crossing);
    EXPECT_EQ(byte{ 0xFF }, crossing_input[7]);
    EXPECT_EQ(byte{ 0xFC }, crossing_input[8]);
    EXPECT_EQ(byte{ 0x00 }, crossing_input[9]);
    EXPECT_EQ(byte{ 70 }, crossing_input[35]);
    crossing_input.insert(crossing_input.end(), payload.begin(), payload.end());
    crossing_input.insert(crossing_input.end(), epoch_bytes.begin(), epoch_bytes.end());
    EXPECT_EQ(
        expected_hash(crossing_input, "leaf_hash" + version_suffix),
        compute_leaf_hash(crossing, payload, epoch));

    // All eight epoch bytes participate.
    if constexpr (sizeof(size_t) >= 8) {
        size_t big_epoch = static_cast<size_t>(0x0102030405060708ULL);
        vector<byte> big_input = encode_label_for_hash(label);
        big_input.insert(big_input.end(), payload.begin(), payload.end());
        vector<byte> big_epoch_bytes =
            make_bytes<vector<byte>>(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08);
        big_input.insert(big_input.end(), big_epoch_bytes.begin(), big_epoch_bytes.end());
        EXPECT_EQ(
            expected_hash(big_input, "leaf_hash" + version_suffix),
            compute_leaf_hash(label, payload, big_epoch));
    }
}

TEST(Utilities, LeafAndNodeHashKnownAnswers)
{
    // Pinned for serialization version 2. A change here means commitments moved.
    ASSERT_EQ(2U, ozks_serialization_version);
    PartialLabel label = make_bytes<PartialLabel>(0x80, 0x01);
    hash_type payload = make_bytes<hash_type>(0xAB, 0xCD);
    PartialLabel right{ true, true };
    hash_type right_hash = make_bytes<hash_type>(0x01, 0x02);
    EXPECT_EQ(
        "8801ea86d7d80b0febce52f79378a4fc4e3f50385a4749e4a55f20fd292469ee",
        to_string(compute_leaf_hash(label, payload, 0x01020304)));
    EXPECT_EQ(
        "988a68e18fce4179224681b749667a215ccf9c01ba703b3aeb7d9de30fd80250",
        to_string(compute_node_hash(label, payload, right, right_hash)));
    EXPECT_EQ(
        "c648349c7d26f45b9a1e3c52d583030023faf97ca25f1e34a7fc4a7a0e962ff2",
        to_string(compute_node_hash({}, {}, PartialLabel(hash_type{}), payload)));
    hash_type distinct_bytes;
    for (size_t idx = 0; idx < distinct_bytes.size(); idx++) {
        distinct_bytes[idx] = static_cast<byte>(idx + 1);
    }
    EXPECT_EQ(
        "ee34fa5c3a334f611259fb79e27fc17c260f648e2aa3035a8d91859bd9946650",
        to_string(compute_leaf_hash(PartialLabel(distinct_bytes), payload, 0x01020304)));
}

TEST(Utilities, BoolVectorTest)
{
    vector<byte> bytes = make_bytes<vector<byte>>(0xAA, 0xBB, 0xCC);
    PartialLabel bools{ bytes };

    ASSERT_EQ(24, bools.bit_count());
    ASSERT_EQ(true, bools[0]);
    ASSERT_EQ(false, bools[1]);
    ASSERT_EQ(true, bools[2]);
    ASSERT_EQ(false, bools[3]);
    ASSERT_EQ(true, bools[4]);
    ASSERT_EQ(false, bools[5]);
    ASSERT_EQ(true, bools[6]);
    ASSERT_EQ(false, bools[7]);

    ASSERT_EQ(true, bools[8]);
    ASSERT_EQ(false, bools[9]);
    ASSERT_EQ(true, bools[10]);
    ASSERT_EQ(true, bools[11]);
    ASSERT_EQ(true, bools[12]);
    ASSERT_EQ(false, bools[13]);
    ASSERT_EQ(true, bools[14]);
    ASSERT_EQ(true, bools[15]);

    ASSERT_EQ(true, bools[16]);
    ASSERT_EQ(true, bools[17]);
    ASSERT_EQ(false, bools[18]);
    ASSERT_EQ(false, bools[19]);
    ASSERT_EQ(true, bools[20]);
    ASSERT_EQ(true, bools[21]);
    ASSERT_EQ(false, bools[22]);
    ASSERT_EQ(false, bools[23]);

    vector<byte> bytes2 = bools.to_bytes();

    ASSERT_EQ(byte{ 0xAA }, bytes2[0]);
    ASSERT_EQ(byte{ 0xBB }, bytes2[1]);
    ASSERT_EQ(byte{ 0xCC }, bytes2[2]);
}

TEST(Utilities, BoolConversionTest)
{
    PartialLabel bools;
    bools.add_bit(true);

    vector<byte> bytes = bools.to_bytes();
    EXPECT_EQ(1, bytes.size());
    EXPECT_EQ(byte{ 0x80 }, bytes[0]);

    bools.add_bit(false);
    bools.add_bit(true);
    bools.add_bit(false);

    bytes = bools.to_bytes();
    EXPECT_EQ(1, bytes.size());
    EXPECT_EQ(byte{ 0xA0 }, bytes[0]);

    bools.add_bit(true);
    bools.add_bit(false);
    bools.add_bit(true);
    bools.add_bit(false);
    bools.add_bit(false);

    bytes = bools.to_bytes();
    EXPECT_EQ(2, bytes.size());
    EXPECT_EQ(byte{ 0xAA }, bytes[0]);
    EXPECT_EQ(byte{ 0x00 }, bytes[1]);
}

TEST(Utilities, BytesToBoolsTest)
{
    PartialLabel bools;

    bools.add_bit(true);
    bools.add_bit(false);
    bools.add_bit(true);
    bools.add_bit(false);

    vector<byte> bytes = bools.to_bytes();

    PartialLabel bools2(bytes, 4);
    EXPECT_EQ(bools, bools2);

    bools.clear();
    bools.add_bit(true);

    bytes = bools.to_bytes();
    bools2 = PartialLabel(bytes, 1);
    EXPECT_EQ(bools, bools2);

    bools.clear();
    bools.add_bit(false);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(false);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(true);
    bools.add_bit(false);

    bytes = bools.to_bytes();
    EXPECT_EQ(4, bytes.size());

    bools2 = PartialLabel(bytes, 31);
    EXPECT_EQ(bools, bools2);
}

TEST(Utilities, BytesToBoolsMultipleUI64sTest)
{
    vector<byte> bytes = make_bytes<vector<byte>>(
        // First uint64
        0xAA,
        0xBB,
        0xCC,
        0xDD,
        0xEE,
        0xFF,
        0x00,
        0x11,
        // Second uint64
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88,
        0x99,
        // Third uint64
        0x10,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08);

    PartialLabel bools(bytes, bytes.size() * 8);
    EXPECT_EQ(192, bools.bit_count());

    EXPECT_EQ(1, bools[0]);
    EXPECT_EQ(0, bools[1]);
    EXPECT_EQ(1, bools[2]);
    EXPECT_EQ(0, bools[3]);
    EXPECT_EQ(1, bools[4]);
    EXPECT_EQ(0, bools[5]);
    EXPECT_EQ(1, bools[6]);
    EXPECT_EQ(0, bools[7]);

    EXPECT_EQ(0, bools[64]);
    EXPECT_EQ(0, bools[65]);
    EXPECT_EQ(1, bools[66]);
    EXPECT_EQ(0, bools[67]);
    EXPECT_EQ(0, bools[68]);
    EXPECT_EQ(0, bools[69]);
    EXPECT_EQ(1, bools[70]);
    EXPECT_EQ(0, bools[71]);

    EXPECT_EQ(0, bools[128]);
    EXPECT_EQ(0, bools[129]);
    EXPECT_EQ(0, bools[130]);
    EXPECT_EQ(1, bools[131]);
    EXPECT_EQ(0, bools[132]);
    EXPECT_EQ(0, bools[133]);
    EXPECT_EQ(0, bools[134]);
    EXPECT_EQ(0, bools[135]);

    bytes = make_bytes<vector<byte>>(
        // First uint64
        0xFF,
        0x00,
        0x11,
        // Second uint64
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88,
        0x99,
        // Third uint64
        0x10,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08);

    bools = PartialLabel(bytes, (bytes.size() * 8) - 3);
    EXPECT_EQ(149, bools.bit_count());

    EXPECT_EQ(1, bools[0]);
    EXPECT_EQ(1, bools[1]);
    EXPECT_EQ(1, bools[2]);
    EXPECT_EQ(1, bools[3]);
    EXPECT_EQ(1, bools[4]);
    EXPECT_EQ(1, bools[5]);

    // 0x22
    EXPECT_EQ(0, bools[24]);
    EXPECT_EQ(0, bools[25]);
    EXPECT_EQ(1, bools[26]);
    EXPECT_EQ(0, bools[27]);
    EXPECT_EQ(0, bools[28]);
    EXPECT_EQ(0, bools[29]);
    EXPECT_EQ(1, bools[30]);
    EXPECT_EQ(0, bools[31]);

    // 0x10
    EXPECT_EQ(0, bools[88]);
    EXPECT_EQ(0, bools[89]);
    EXPECT_EQ(0, bools[90]);
    EXPECT_EQ(1, bools[91]);
    EXPECT_EQ(0, bools[92]);
    EXPECT_EQ(0, bools[93]);
    EXPECT_EQ(0, bools[94]);
    EXPECT_EQ(0, bools[95]);

    // 0x08
    EXPECT_EQ(0, bools[144]);
    EXPECT_EQ(0, bools[145]);
    EXPECT_EQ(0, bools[146]);
    EXPECT_EQ(0, bools[147]);
    EXPECT_EQ(1, bools[148]);

    EXPECT_THROW(bools[149], invalid_argument);
}

TEST(Utilities, ComputeHash)
{
    // Empty input; a few related domains (outputs should be different)
    vector<byte> bytes;
    hash_type hash1 = compute_hash(bytes, "");
    hash_type hash2 = compute_hash(bytes, "a");
    hash_type hash3 = compute_hash(bytes, "ab");

    // hash1, hash2, hash3 must all be different
    EXPECT_NE(hash1, hash2);
    EXPECT_NE(hash2, hash3);
    EXPECT_NE(hash1, hash3);

    // Try with non-empty input
    bytes = make_bytes<vector<byte>>(0x11, 0x22, 0x33);
    hash1 = compute_hash(bytes, "abr");
    hash2 = compute_hash(bytes, "abraca");
    hash3 = compute_hash(bytes, "abracadabra");

    // hash1, hash2, hash3 must all be different
    EXPECT_NE(hash1, hash2);
    EXPECT_NE(hash2, hash3);
    EXPECT_NE(hash1, hash3);
}

TEST(Utilities, NodeHashesBindEveryLabelLength)
{
    hash_type zero_bytes{};
    hash_type payload = compute_hash(make_bytes<vector<byte>>(1, 2, 3));
    PartialLabel full_label(zero_bytes);
    PartialLabel label;
    PartialLabel sibling{ true };
    set<hash_type> leaf_hashes;
    set<hash_type> left_hashes;
    set<hash_type> right_hashes;

    for (size_t length = 0; length <= PartialLabel::MaxBitCount; length++) {
        ASSERT_EQ(length, label.bit_count());
        ASSERT_EQ(0, memcmp(label.data(), full_label.data(), PartialLabel::ByteCount));
        EXPECT_TRUE(leaf_hashes.insert(compute_leaf_hash(label, payload, 7)).second);
        EXPECT_TRUE(left_hashes.insert(compute_node_hash(label, payload, sibling, payload)).second);
        EXPECT_TRUE(
            right_hashes.insert(compute_node_hash(sibling, payload, label, payload)).second);

        array<byte, PartialLabel::SaveSize> serialized{};
        label.save(serialized);
        PartialLabel loaded;
        loaded.load(serialized);
        EXPECT_EQ(compute_leaf_hash(label, payload, 7), compute_leaf_hash(loaded, payload, 7));
        if (length < PartialLabel::MaxBitCount) {
            label.add_bit(false);
        }
    }
}

TEST(Utilities, NodeLabelTakesOptionalSecretKey)
{
    key_type key = make_bytes<key_type>(0x01, 0x02, 0x03);
    hash_type key_hash = compute_key_hash(key);
    VRFCache cache(16);

    // Hashed labels never touch the key or the cache, so a null key is valid and the label is
    // the key hash.
    EXPECT_EQ(key_hash, get_node_label(key, nullptr, LabelType::HashedLabels));
    auto hashed = get_node_label(key, nullptr, cache, LabelType::HashedLabels);
    EXPECT_EQ(key_hash, hashed.first);
    EXPECT_EQ(nullptr, hashed.second);
    EXPECT_EQ(0U, cache.size());
    EXPECT_EQ(0U, cache.cache_misses());

    // VRF labels require the key: a null key is rejected rather than dereferenced.
    EXPECT_THROW(get_node_label(key, nullptr, LabelType::VRFLabels), invalid_argument);
    EXPECT_THROW(get_node_label(key, nullptr, cache, LabelType::VRFLabels), invalid_argument);

    // With a key, both overloads agree and the label is the VRF value, not the key hash. The
    // first cached call misses and populates the cache; the second hits it.
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    hash_type label = get_node_label(key, sk.get(), LabelType::VRFLabels);
    auto with_proof = get_node_label(key, sk.get(), cache, LabelType::VRFLabels);
    EXPECT_NE(key_hash, label);
    EXPECT_EQ(label, with_proof.first);
    ASSERT_NE(nullptr, with_proof.second);
    hash_type from_proof;
    copy_bytes_to_hash(with_proof.second->get_vrf_value(), from_proof);
    EXPECT_EQ(label, from_proof);
    EXPECT_EQ(1U, cache.size());
    EXPECT_EQ(1U, cache.cache_misses());
    auto second = get_node_label(key, sk.get(), cache, LabelType::VRFLabels);
    EXPECT_EQ(label, second.first);
    EXPECT_EQ(1U, cache.cache_hits());

    // A warm cache does not rescue a null key: the check precedes the cache lookup.
    EXPECT_THROW(get_node_label(key, nullptr, cache, LabelType::VRFLabels), invalid_argument);
}

TEST(Utilities, PayloadCommitmentRecomputation)
{
    for (auto mode :
         { PayloadCommitmentType::CommitedPayload, PayloadCommitmentType::UncommitedPayload }) {
        for (const auto &payload : { payload_type{}, payload_type{ byte{ 1 }, byte{ 2 } } }) {
            auto committed = commit_payload(payload, mode);
            EXPECT_EQ(committed.first, compute_payload_commitment(payload, mode, committed.second));
            auto changed = payload;
            changed.push_back(byte{ 3 });
            EXPECT_NE(committed.first, compute_payload_commitment(changed, mode, committed.second));
            if (mode == PayloadCommitmentType::UncommitedPayload) {
                EXPECT_EQ(randomness_type{}, committed.second);
            } else {
                committed.second[0] ^= byte{ 1 };
                EXPECT_NE(
                    committed.first, compute_payload_commitment(payload, mode, committed.second));
            }
        }
    }
}

TEST(Utilities, ByteVectorHashTest)
{
    byte_vector_hash hasher;

    size_t hash1 = hasher(make_bytes<vector<byte>>(1, 2, 3, 4, 5, 6));
    size_t hash2 = hasher(make_bytes<vector<byte>>(1, 2, 3, 4, 5, 6));
    size_t hash3 = hasher(make_bytes<vector<byte>>(1, 2, 3, 4, 5, 6, 7));
    size_t hash4 = hasher(make_bytes<vector<byte>>(1, 2, 3, 4, 5, 6, 7, 8));
    size_t hash5 = hasher(make_bytes<vector<byte>>(1, 2, 3, 4, 5, 6, 7, 8, 9));
    size_t hash6 =
        hasher(make_bytes<vector<byte>>(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
    size_t hash7 =
        hasher(make_bytes<vector<byte>>(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17));

    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash3, hash4);
    EXPECT_NE(hash4, hash5);
    EXPECT_NE(hash5, hash6);
    EXPECT_NE(hash6, hash7);
}

TEST(Utilities, CommonPrefixTest)
{
    PartialLabel label1 = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
    PartialLabel label2 = { 1, 0, 1, 0, 0, 0, 0 };
    PartialLabel label3 = { 0, 1, 0, 1, 0 };
    PartialLabel label4 = {};
    PartialLabel label5 = { 1 };
    PartialLabel label6 = { 0 };

    PartialLabel result = PartialLabel::CommonPrefix(label1, label2);
    EXPECT_EQ(4, result.bit_count());
    EXPECT_EQ(1, result[0]);
    EXPECT_EQ(0, result[1]);
    EXPECT_EQ(1, result[2]);
    EXPECT_EQ(0, result[3]);

    result = PartialLabel::CommonPrefix(label2, label1);
    EXPECT_EQ(4, result.bit_count());
    EXPECT_EQ(1, result[0]);
    EXPECT_EQ(0, result[1]);
    EXPECT_EQ(1, result[2]);
    EXPECT_EQ(0, result[3]);

    result = PartialLabel::CommonPrefix(label1, label3);
    EXPECT_EQ(0, result.bit_count());

    result = PartialLabel::CommonPrefix(label3, label1);
    EXPECT_EQ(0, result.bit_count());

    result = PartialLabel::CommonPrefix(label1, label4);
    EXPECT_EQ(0, result.bit_count());

    result = PartialLabel::CommonPrefix(label4, label1);
    EXPECT_EQ(0, result.bit_count());

    result = PartialLabel::CommonPrefix(label1, label5);
    EXPECT_EQ(1, result.bit_count());
    EXPECT_EQ(1, result[0]);

    result = PartialLabel::CommonPrefix(label5, label1);
    EXPECT_EQ(1, result.bit_count());
    EXPECT_EQ(1, result[0]);

    result = PartialLabel::CommonPrefix(label1, label6);
    EXPECT_EQ(0, result.bit_count());

    result = PartialLabel::CommonPrefix(label6, label1);
    EXPECT_EQ(0, result.bit_count());
}

TEST(Utilities, InsertionThreadLimitTest)
{
    {
        CompressedTrie trie;
        shared_ptr<CTNode> root = make_shared<CTNodeLinked>(&trie, PartialLabel{});

        size_t limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(1, limit);

        root->set_new_right_node({ 1 }, trie.epoch());

        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(1, limit);

        root->set_new_left_node({ 0 }, trie.epoch());

        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(2, limit);

        root->right()->set_new_right_node({ 1, 1 }, trie.epoch());

        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(2, limit);

        root->right()->set_new_left_node({ 1, 0 }, trie.epoch());

        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(2, limit);

        root->left()->set_new_left_node({ 0, 0 }, trie.epoch());

        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(2, limit);

        root->left()->set_new_right_node({ 0, 1 }, trie.epoch());

        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(4, limit);

        root->left()->left()->set_new_left_node({ 0, 0, 0 }, trie.epoch());
        root->left()->left()->set_new_right_node({ 0, 0, 1 }, trie.epoch());
        root->left()->right()->set_new_left_node({ 0, 1, 0 }, trie.epoch());
        root->left()->right()->set_new_right_node({ 0, 1, 1 }, trie.epoch());
        root->right()->left()->set_new_left_node({ 1, 0, 0 }, trie.epoch());
        root->right()->left()->set_new_right_node({ 1, 0, 1 }, trie.epoch());
        root->right()->right()->set_new_right_node({ 1, 1, 1 }, trie.epoch());

        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(4, limit);

        root->right()->right()->set_new_left_node({ 1, 1, 0 }, trie.epoch());

        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(8, limit);

        limit = get_insertion_thread_limit(root, /* max_threads */ 3);
        EXPECT_EQ(2, limit);
    }

    {
        // Build a tree where left subtree is shallower than right subtree at depth 2:
        //
        //            root ("")
        //           /         \
        //        (0)           (1)
        //        / \           / \
        //     (00) (01)     (10)  (11)      <-- (00) and (01) are leaves
        //                   /  \   /  \
        //               (100)(101)(110)(111)
        //
        // The correct thread limit is 4 because level [(00),(01),(10),(11)]
        // is not fully expandable -- (00) and (01) have no children.

        CompressedTrie trie;
        shared_ptr<CTNode> root = make_shared<CTNodeLinked>(&trie, PartialLabel{});

        // Build complete level 1
        root->set_new_left_node({ 0 }, trie.epoch());
        root->set_new_right_node({ 1 }, trie.epoch());

        // Build complete level 2
        root->left()->set_new_left_node({ 0, 0 }, trie.epoch());
        root->left()->set_new_right_node({ 0, 1 }, trie.epoch());
        root->right()->set_new_left_node({ 1, 0 }, trie.epoch());
        root->right()->set_new_right_node({ 1, 1 }, trie.epoch());

        size_t limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(4, limit);

        // Only expand the RIGHT subtree at level 3; leave (00) and (01) as leaves
        root->right()->left()->set_new_left_node({ 1, 0, 0 }, trie.epoch());
        root->right()->left()->set_new_right_node({ 1, 0, 1 }, trie.epoch());
        root->right()->right()->set_new_left_node({ 1, 1, 0 }, trie.epoch());
        root->right()->right()->set_new_right_node({ 1, 1, 1 }, trie.epoch());

        // Level 2 is not fully expandable: (00) and (01) are leaves.
        // Correct answer is 4, not 8.
        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(4, limit);
    }

    {
        // Mirror case: LEFT subtree is deeper, RIGHT subtree has leaves at level 2:
        //
        //            root ("")
        //           /         \
        //        (0)           (1)
        //        / \           / \
        //     (00) (01)     (10)  (11)      <-- (10) and (11) are leaves
        //     / \   / \
        // (000)(001)(010)(011)
        //
        // The correct thread limit is 4 because level [(00),(01),(10),(11)]
        // is not fully expandable -- (10) and (11) have no children.

        CompressedTrie trie;
        shared_ptr<CTNode> root = make_shared<CTNodeLinked>(&trie, PartialLabel{});

        // Build complete level 1
        root->set_new_left_node({ 0 }, trie.epoch());
        root->set_new_right_node({ 1 }, trie.epoch());

        // Build complete level 2
        root->left()->set_new_left_node({ 0, 0 }, trie.epoch());
        root->left()->set_new_right_node({ 0, 1 }, trie.epoch());
        root->right()->set_new_left_node({ 1, 0 }, trie.epoch());
        root->right()->set_new_right_node({ 1, 1 }, trie.epoch());

        size_t limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(4, limit);

        // Only expand the LEFT subtree at level 3; leave (10) and (11) as leaves
        root->left()->left()->set_new_left_node({ 0, 0, 0 }, trie.epoch());
        root->left()->left()->set_new_right_node({ 0, 0, 1 }, trie.epoch());
        root->left()->right()->set_new_left_node({ 0, 1, 0 }, trie.epoch());
        root->left()->right()->set_new_right_node({ 0, 1, 1 }, trie.epoch());

        // Level 2 is not fully expandable: (10) and (11) are leaves.
        // Correct answer is 4, not 8.
        limit = get_insertion_thread_limit(root, /* max_threads */ 100);
        EXPECT_EQ(4, limit);
    }
}

TEST(Utilities, InsertionIndexTest)
{
    EXPECT_EQ(0, get_insertion_index(0, { 1 }));
    EXPECT_EQ(1, get_insertion_index(1, { 1 }));
    EXPECT_EQ(0, get_insertion_index(1, { 0 }));
    EXPECT_EQ(2, get_insertion_index(2, { 1, 0, 1, 0, 1, 0, 1 }));
    EXPECT_EQ(1, get_insertion_index(2, { 0, 1, 0, 1, 0, 1, 0, 1 }));
    EXPECT_EQ(3, get_insertion_index(2, { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 }));
    EXPECT_EQ(5, get_insertion_index(3, { 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1 }));
}

TEST(Utilities, Log2Test)
{
    EXPECT_EQ(0, get_log2(0));
    EXPECT_EQ(0, get_log2(1));
    EXPECT_EQ(1, get_log2(2));
    EXPECT_EQ(1, get_log2(3));
    EXPECT_EQ(2, get_log2(4));
    EXPECT_EQ(2, get_log2(5));
    EXPECT_EQ(2, get_log2(6));
    EXPECT_EQ(2, get_log2(7));
    EXPECT_EQ(3, get_log2(8));
    EXPECT_EQ(3, get_log2(10));
    EXPECT_EQ(3, get_log2(12));
    EXPECT_EQ(4, get_log2(16));
    EXPECT_EQ(4, get_log2(20));
}
