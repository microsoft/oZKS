// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <cstddef>
#include <vector>

// oZKS
#include "oZKS/ct_node_linked.h"
#include "oZKS/utilities.h"

// GTest
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

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
    CompressedTrie trie;
    shared_ptr<CTNode> root = make_shared<CTNodeLinked>(&trie, PartialLabel{});

    size_t limit = get_insertion_thread_limit(root, /* max_threads */ 100);
    EXPECT_EQ(1, limit);

    root->set_new_right_node({ 1 });

    limit = get_insertion_thread_limit(root, /* max_threads */ 100);
    EXPECT_EQ(1, limit);

    root->set_new_left_node({ 0 });

    limit = get_insertion_thread_limit(root, /* max_threads */ 100);
    EXPECT_EQ(2, limit);

    root->right()->set_new_right_node({ 1, 1 });

    limit = get_insertion_thread_limit(root, /* max_threads */ 100);
    EXPECT_EQ(2, limit);

    root->right()->set_new_left_node({ 1, 0 });

    limit = get_insertion_thread_limit(root, /* max_threads */ 100);
    EXPECT_EQ(2, limit);

    root->left()->set_new_left_node({ 0, 0 });

    limit = get_insertion_thread_limit(root, /* max_threads */ 100);
    EXPECT_EQ(2, limit);

    root->left()->set_new_right_node({ 0, 1 });

    limit = get_insertion_thread_limit(root, /* max_threads */ 100);
    EXPECT_EQ(4, limit);

    root->left()->left()->set_new_left_node({ 0, 0, 0 });
    root->left()->left()->set_new_right_node({ 0, 0, 1 });
    root->left()->right()->set_new_left_node({ 0, 1, 0 });
    root->left()->right()->set_new_right_node({ 0, 1, 1 });
    root->right()->left()->set_new_left_node({ 1, 0, 0 });
    root->right()->left()->set_new_right_node({ 1, 0, 1 });
    root->right()->right()->set_new_right_node({ 1, 1, 1 });

    limit = get_insertion_thread_limit(root, /* max_threads */ 100);
    EXPECT_EQ(4, limit);

    root->right()->right()->set_new_left_node({ 1, 1, 0 });

    limit = get_insertion_thread_limit(root, /* max_threads */ 100);
    EXPECT_EQ(8, limit);

    limit = get_insertion_thread_limit(root, /* max_threads */ 3);
    EXPECT_EQ(2, limit);
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
