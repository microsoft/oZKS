// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "oZKS/utilities.h"
#include <algorithm>
#include <cstddef>
#include <vector>
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

TEST(Utilities, BoolVectorTest)
{
    vector<byte> bytes = make_bytes(0xAA, 0xBB, 0xCC);
    vector<bool> bools = bytes_to_bools(bytes);

    ASSERT_EQ(24, bools.size());
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

    vector<byte> bytes2;
    bools_to_bytes(bools, bytes2);

    ASSERT_EQ(byte{ 0xAA }, bytes2[0]);
    ASSERT_EQ(byte{ 0xBB }, bytes2[1]);
    ASSERT_EQ(byte{ 0xCC }, bytes2[2]);
}

TEST(Utilities, BoolConversionTest)
{
    vector<bool> bools;
    bools.push_back(true);

    vector<byte> bytes;
    bools_to_bytes(bools, bytes);
    EXPECT_EQ(1, bytes.size());
    EXPECT_EQ(byte{ 0x01 }, bytes[0]);

    bools.push_back(false);
    bools.push_back(true);
    bools.push_back(false);

    bools_to_bytes(bools, bytes);
    EXPECT_EQ(1, bytes.size());
    EXPECT_EQ(byte{ 0x0A }, bytes[0]);

    bools.push_back(true);
    bools.push_back(false);
    bools.push_back(true);
    bools.push_back(false);
    bools.push_back(false);

    bools_to_bytes(bools, bytes);
    EXPECT_EQ(2, bytes.size());
    EXPECT_EQ(byte{ 0x01 }, bytes[0]);
    EXPECT_EQ(byte{ 0x54 }, bytes[1]);
}

TEST(Utilities, BytesToBoolsTest)
{
    vector<bool> bools;

    bools.push_back(true);
    bools.push_back(false);
    bools.push_back(true);
    bools.push_back(false);

    vector<byte> bytes = bools_to_bytes(bools);

    vector<bool> bools2 = bytes_to_bools(bytes.data(), 4);
    EXPECT_EQ(bools, bools2);

    bools.clear();
    bools.push_back(true);

    bytes = bools_to_bytes(bools);
    bools2 = bytes_to_bools(bytes.data(), 1);
    EXPECT_EQ(bools, bools2);

    bools.clear();
    bools.push_back(false);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(false);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(true);
    bools.push_back(false);

    bytes = bools_to_bytes(bools);
    EXPECT_EQ(4, bytes.size());

    bools2 = bytes_to_bools(bytes.data(), 31);
    EXPECT_EQ(bools, bools2);
}

TEST(Utilities, BytesToBoolsMultipleUI64sTest)
{
    vector<byte> bytes = make_bytes(
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

    vector<bool> bools = bytes_to_bools(bytes.data(), bytes.size() * 8);
    EXPECT_EQ(192, bools.size());

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

    bytes = make_bytes(
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

    bools = bytes_to_bools(bytes.data(), (bytes.size() * 8) - 3);
    EXPECT_EQ(149, bools.size());

    EXPECT_EQ(1, bools[0]);
    EXPECT_EQ(1, bools[1]);
    EXPECT_EQ(1, bools[2]);
    EXPECT_EQ(1, bools[3]);
    EXPECT_EQ(1, bools[4]);
    EXPECT_EQ(0, bools[5]);

    // 0x22
    EXPECT_EQ(0, bools[21]);
    EXPECT_EQ(0, bools[22]);
    EXPECT_EQ(1, bools[23]);
    EXPECT_EQ(0, bools[24]);
    EXPECT_EQ(0, bools[25]);
    EXPECT_EQ(0, bools[26]);
    EXPECT_EQ(1, bools[27]);
    EXPECT_EQ(0, bools[28]);

    // 0x10
    EXPECT_EQ(0, bools[85]);
    EXPECT_EQ(0, bools[86]);
    EXPECT_EQ(0, bools[87]);
    EXPECT_EQ(1, bools[88]);
    EXPECT_EQ(0, bools[89]);
    EXPECT_EQ(0, bools[90]);
    EXPECT_EQ(0, bools[91]);
    EXPECT_EQ(0, bools[92]);
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
    bytes = make_bytes(0x11, 0x22, 0x33);
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

    size_t hash1 = hasher(make_bytes(1, 2, 3, 4, 5, 6));
    size_t hash2 = hasher(make_bytes(1, 2, 3, 4, 5, 6));
    size_t hash3 = hasher(make_bytes(1, 2, 3, 4, 5, 6, 7));
    size_t hash4 = hasher(make_bytes(1, 2, 3, 4, 5, 6, 7, 8));
    size_t hash5 = hasher(make_bytes(1, 2, 3, 4, 5, 6, 7, 8, 9));
    size_t hash6 = hasher(make_bytes(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
    size_t hash7 = hasher(make_bytes(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17));

    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash3, hash4);
    EXPECT_NE(hash4, hash5);
    EXPECT_NE(hash5, hash6);
    EXPECT_NE(hash6, hash7);
}
