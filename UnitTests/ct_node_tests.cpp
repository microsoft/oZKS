// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <sstream>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"

// GSL
#include "gsl/span"
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

TEST(CTNodeTests, InsertTest)
{
    shared_ptr storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);
    CTNode root(&trie);

    partial_label_type label1{ true, true, true, true };
    partial_label_type label2{ true, true, true, false };
    partial_label_type label3{ true, false, false, false };
    partial_label_type label4{ true, false, false, true };
    partial_label_type label5{ true, false, true, true };
    payload_type payload1 = make_bytes(0xFF, 0xFE, 0xFD, 0xFC);
    payload_type payload2 = make_bytes(0xFE, 0xFD, 0xFC, 0xFB);
    payload_type payload3 = make_bytes(0xFD, 0xFC, 0xFB, 0xFA);
    payload_type payload4 = make_bytes(0xFC, 0xFB, 0xFA, 0xF9);
    payload_type payload5 = make_bytes(0xFB, 0xFA, 0xF9, 0xF8);

    root.insert(label1, payload1, /* epoch */ 1);
    root.insert(label2, payload2, /* epoch */ 1);
    root.insert(label3, payload3, /* epoch */ 1);
    root.insert(label4, payload4, /* epoch */ 1);
    root.insert(label5, payload5, /* epoch */ 1);

    root.update_hashes(label1);
    root.update_hashes(label2);
    root.update_hashes(label3);
    root.update_hashes(label4);
    root.update_hashes(label5);

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

    lookup_path_type path1;
    lookup_path_type path2;
    lookup_path_type path3;
    lookup_path_type path4;
    lookup_path_type path5;

    EXPECT_TRUE(root.lookup(label1, path1, /* include_searched */ true));
    EXPECT_TRUE(root.lookup(label2, path2, /* include_searched */ true));
    EXPECT_TRUE(root.lookup(label3, path3, /* include_searched */ true));
    EXPECT_TRUE(root.lookup(label4, path4, /* include_searched */ true));
    EXPECT_TRUE(root.lookup(label5, path5, /* include_searched */ true));

    partial_label_type left_common{ true, false };
    EXPECT_EQ(path1[0].first, label1);
    EXPECT_EQ(path1[1].first, label2);
    EXPECT_EQ(path1[2].first, left_common);
}

TEST(CTNodeTests, UpdateHashTest)
{
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);
    CTNode node(&trie);

    label_type label = make_bytes(0x01);
    payload_type payload = make_bytes(0xF0, 0xF1, 0xF2);
    partial_label_type lab = bytes_to_bools(label);
    lookup_path_type lookup_path;

    node.insert(lab, payload, /* epoch */ 1);
    node.update_hashes(lab);

    CTNode left_node;
    CTNode right_node;
    node.load_left(left_node);

    EXPECT_EQ(lab, node.left);
    EXPECT_EQ(lab, left_node.label);
    EXPECT_FALSE(node.left.empty());
    EXPECT_EQ(
        "2de2976b794dc48d2e939b2e16d7db73701dbf2c1641ca93e4ad74b8f029cf07f02d01fb9c0b31dc740056acc9"
        "c17ddfd5c7d5d4471b227e5033d0169180f1a2",
        to_string(left_node.hash()));

    lab = bytes_to_bools(make_bytes(0x02));
    payload = make_bytes(0xE0, 0xE1, 0xE2);

    hash_type hash_root = node.hash();
    node.insert(lab, payload, 2);
    node.update_hashes(lab);

    EXPECT_FALSE(node.left.empty());
    node.load_left(left_node);

    EXPECT_FALSE(left_node.left.empty());
    EXPECT_FALSE(left_node.right.empty());

    CTNode left_left_node;
    left_node.load_left(left_left_node);
    EXPECT_EQ(
        "2de2976b794dc48d2e939b2e16d7db73701dbf2c1641ca93e4ad74b8f029cf07f02d01fb9c0b31dc740056acc9"
        "c17ddfd5c7d5d4471b227e5033d0169180f1a2",
        to_string(left_left_node.hash()));

    CTNode left_right_node;
    left_node.load_right(left_right_node);
    EXPECT_EQ(
        "9aba28398d409188b8d44a66a2b29d489d37d0946617c72bddd4d6ac324deca00a4322da731f7bd137adb07ea5"
        "05d02296ea2ba1b53f71e733d83834e9d79961",
        to_string(left_right_node.hash()));
    EXPECT_NE(hash_root, node.hash());
}

TEST(CTNodeTests, AllNodesHashedTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage);
    CTNode node(&trie);

    label_type label = make_bytes(0x01);
    payload_type payload = make_bytes(0x01, 0x02, 0x03);
    partial_label_type lab = bytes_to_bools(label);
    hash_type hash_root = node.hash();
    lookup_path_type lookup_path;

    node.insert(lab, payload, 1);
    node.update_hashes(lab);

    EXPECT_NE(hash_root, node.hash()); // Updated
    EXPECT_FALSE(node.left.empty());
    EXPECT_TRUE(node.right.empty());

    hash_root = node.hash();
    CTNode left_node;
    node.load_left(left_node);
    hash_type hash_01 = left_node.hash();

    lab = bytes_to_bools(make_bytes(0x02));
    payload = make_bytes(0x04, 0x05, 0x06);

    node.insert(lab, payload, 2);
    node.update_hashes(lab);

    EXPECT_NE(hash_root, node.hash()); // Updated
    partial_label_type label_000000{ 0, 0, 0, 0, 0, 0 };

    node.load_left(left_node);
    EXPECT_EQ(label_000000, left_node.label);
    hash_type hash_000000 = left_node.hash();
    partial_label_type label_01{ 0, 0, 0, 0, 0, 0, 0, 1 };

    CTNode left_left_node;
    left_node.load_left(left_left_node);
    EXPECT_EQ(label_01, left_left_node.label);
    EXPECT_EQ(hash_01, left_left_node.hash());
    partial_label_type label_02{ 0, 0, 0, 0, 0, 0, 1, 0 };

    CTNode left_right_node;
    left_node.load_right(left_right_node);
    EXPECT_EQ(label_02, left_right_node.label);
    hash_type hash_02 = left_right_node.hash();
    EXPECT_NE(hash_01, hash_02);
    EXPECT_NE(hash_000000, hash_01);
    EXPECT_NE(hash_000000, hash_02);

    hash_root = node.hash();
    lab = bytes_to_bools(make_bytes(0x03));
    payload = make_bytes(0x04, 0x05, 0x06);

    node.insert(lab, payload, 3);
    node.update_hashes(lab);

    EXPECT_NE(hash_root, node.hash());

    node.load_left(left_node);
    EXPECT_EQ(label_000000, left_node.label);
    EXPECT_NE(hash_000000, left_node.hash()); // Updated
    hash_000000 = left_node.hash();

    left_node.load_left(left_left_node);
    EXPECT_EQ(label_01, left_left_node.label);
    EXPECT_EQ(hash_01, left_left_node.hash());
    partial_label_type label_0000001{ 0, 0, 0, 0, 0, 0, 1 };

    left_node.load_right(left_right_node);
    EXPECT_EQ(label_0000001, left_right_node.label);
    hash_type hash_0000001 = left_right_node.hash();

    CTNode left_right_left_node;
    left_right_node.load_left(left_right_left_node);
    EXPECT_EQ(label_02, left_right_left_node.label);
    EXPECT_EQ(hash_02, left_right_left_node.hash());
    partial_label_type label_03{ 0, 0, 0, 0, 0, 0, 1, 1 };

    CTNode left_right_right_node;
    left_right_node.load_right(left_right_right_node);
    EXPECT_EQ(label_03, left_right_right_node.label);
    hash_type hash_03 = left_right_right_node.hash();
    EXPECT_NE(hash_01, hash_03);
    EXPECT_NE(hash_02, hash_03);

    hash_root = node.hash();
    lab = bytes_to_bools(make_bytes(0x04));
    payload = make_bytes(0x04, 0x05, 0x06);

    node.insert(lab, payload, 4);
    node.update_hashes(lab);

    EXPECT_NE(hash_root, node.hash()); // Updated
    partial_label_type label_00000{ 0, 0, 0, 0, 0 };

    node.load_left(left_node);
    EXPECT_EQ(label_00000, left_node.label);
    hash_type hash_00000 = left_node.hash();

    left_node.load_left(left_left_node);
    EXPECT_EQ(label_000000, left_left_node.label);
    EXPECT_EQ(hash_000000, left_left_node.hash());
    hash_000000 = left_left_node.hash();

    CTNode left_left_left_node;
    left_left_node.load_left(left_left_left_node);
    EXPECT_EQ(label_01, left_left_left_node.label);
    EXPECT_EQ(hash_01, left_left_left_node.hash());

    CTNode left_left_right_node;
    left_left_node.load_right(left_left_right_node);
    EXPECT_EQ(label_0000001, left_left_right_node.label);
    EXPECT_EQ(hash_0000001, left_left_right_node.hash());

    CTNode left_left_right_left_node;
    left_left_right_node.load_left(left_left_right_left_node);
    EXPECT_EQ(label_02, left_left_right_left_node.label);
    EXPECT_EQ(hash_02, left_left_right_left_node.hash());

    CTNode left_left_right_right_node;
    left_left_right_node.load_right(left_left_right_right_node);
    EXPECT_EQ(label_03, left_left_right_right_node.label);
    EXPECT_EQ(hash_03, left_left_right_right_node.hash());
    partial_label_type label_04{ 0, 0, 0, 0, 0, 1, 0, 0 };

    left_node.load_right(left_right_node);
    EXPECT_EQ(label_04, left_right_node.label);

    hash_root = node.hash();
    lab = bytes_to_bools(make_bytes(0x80));
    payload = make_bytes(0x04, 0x05, 0x06);

    node.insert(lab, payload, 5);
    node.update_hashes(lab);

    EXPECT_NE(hash_root, node.hash()); // Updated

    node.load_left(left_node);
    EXPECT_EQ(label_00000, left_node.label);
    EXPECT_EQ(hash_00000, left_node.hash());
    partial_label_type label_80{ 1, 0, 0, 0, 0, 0, 0, 0 };

    CTNode right_node;
    node.load_right(right_node);
    EXPECT_EQ(label_80, right_node.label);
    hash_type hash_80 = right_node.hash();

    hash_root = node.hash();
    lab = bytes_to_bools(make_bytes(0x81));
    payload = make_bytes(0x04, 0x05, 0x06);

    node.insert(lab, payload, 6);
    node.update_hashes(lab);

    EXPECT_NE(hash_root, node.hash()); // Updated

    node.load_left(left_node);
    EXPECT_EQ(label_00000, left_node.label);
    EXPECT_EQ(hash_00000, left_node.hash());
    partial_label_type label_1000000{ 1, 0, 0, 0, 0, 0, 0 };

    node.load_right(right_node);
    EXPECT_EQ(label_1000000, right_node.label);
    hash_type hash_1000000 = right_node.hash();

    CTNode right_left_node;
    right_node.load_left(right_left_node);
    EXPECT_EQ(label_80, right_left_node.label);
    EXPECT_EQ(hash_80, right_left_node.hash());
    partial_label_type label_81{ 1, 0, 0, 0, 0, 0, 0, 1 };

    CTNode right_right_node;
    right_node.load_right(right_right_node);
    EXPECT_EQ(label_81, right_right_node.label);
    hash_type hash_81 = right_right_node.hash();
    EXPECT_NE(hash_80, hash_81);
    EXPECT_NE(hash_81, hash_1000000);

    hash_root = node.hash();
    lab = bytes_to_bools(make_bytes(0x82));
    payload = make_bytes(0x04, 0x05, 0x06);

    node.insert(lab, payload, 7);
    node.update_hashes(lab);

    EXPECT_NE(hash_root, node.hash()); // Updated

    node.load_left(left_node);
    EXPECT_EQ(label_00000, left_node.label);
    EXPECT_EQ(hash_00000, left_node.hash());
    partial_label_type label_100000{ 1, 0, 0, 0, 0, 0 };

    node.load_right(right_node);
    EXPECT_EQ(label_100000, right_node.label);

    right_node.load_left(right_left_node);
    EXPECT_EQ(label_1000000, right_left_node.label);
    EXPECT_EQ(hash_1000000, right_left_node.hash());

    CTNode right_left_left_node;
    right_left_node.load_left(right_left_left_node);
    EXPECT_EQ(label_80, right_left_left_node.label);
    EXPECT_EQ(hash_80, right_left_left_node.hash());

    CTNode right_left_right_node;
    right_left_node.load_right(right_left_right_node);
    EXPECT_EQ(label_81, right_left_right_node.label);
    EXPECT_EQ(hash_81, right_left_right_node.hash());
    partial_label_type label_82{ 1, 0, 0, 0, 0, 0, 1, 0 };

    right_node.load_right(right_right_node);
    EXPECT_EQ(label_82, right_right_node.label);
    hash_type hash_82 = right_right_node.hash();
    EXPECT_NE(hash_82, hash_81);
    EXPECT_NE(hash_82, hash_80);
}

TEST(CTNodeTests, SaveLoadTest)
{
    CTNode node;

    node.hash()[0] = byte{ 0x01 };
    node.hash()[1] = byte{ 0x02 };
    node.hash()[2] = byte{ 0x0f };

    node.payload = make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);
    node.label = partial_label_type{ 1, 0, 0, 1, 1, 0, 0, 1, 1 };
    node.left = partial_label_type{ 1, 0, 0, 1 };

    vector<bool> empty_label;
    stringstream ss;

    size_t save_size = node.save(ss);

    auto result = CTNode::Load(ss);

    EXPECT_EQ(node.hash(), get<0>(result).hash());
    EXPECT_EQ(node.payload, get<0>(result).payload);
    EXPECT_EQ(node.label, get<0>(result).label);
    EXPECT_EQ(node.left, get<1>(result));
    EXPECT_EQ(empty_label, get<2>(result));
    EXPECT_EQ(save_size, get<3>(result));

    CTNode node2;
    node2.hash()[0] = byte{ 0x10 };
    node2.hash()[1] = byte{ 0x11 };
    node2.hash()[2] = byte{ 0x12 };

    node2.payload = make_bytes(0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A);
    node2.label = partial_label_type{ 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 };
    node2.right = partial_label_type{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 };

    stringstream ss2;

    save_size = node2.save(ss2);
    result = CTNode::Load(ss2);

    EXPECT_EQ(node2.hash(), get<0>(result).hash());
    EXPECT_EQ(node2.payload, get<0>(result).payload);
    EXPECT_EQ(node2.label, get<0>(result).label);
    EXPECT_EQ(empty_label, get<1>(result));
    EXPECT_EQ(node2.right, get<2>(result));
    EXPECT_EQ(save_size, get<3>(result));
}

TEST(CTNodeTests, SaveLoadToVectorTest)
{
    CTNode node;

    node.hash()[0] = byte{ 0x01 };
    node.hash()[1] = byte{ 0x02 };
    node.hash()[2] = byte{ 0x0f };

    node.payload = make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);
    node.label = partial_label_type{ 1, 0, 0, 1, 1, 0, 0, 1, 1 };
    node.left = partial_label_type{ 1, 0, 0, 1 };

    vector<bool> empty_label;
    vector<byte> vec;

    size_t save_size = node.save(vec);

    auto result = CTNode::Load(vec);

    EXPECT_EQ(node.hash(), get<0>(result).hash());
    EXPECT_EQ(node.payload, get<0>(result).payload);
    EXPECT_EQ(node.label, get<0>(result).label);
    EXPECT_EQ(node.left, get<1>(result));
    EXPECT_EQ(empty_label, get<2>(result));
    EXPECT_EQ(save_size, get<3>(result));

    CTNode node2;
    node2.hash()[0] = byte{ 0x10 };
    node2.hash()[1] = byte{ 0x11 };
    node2.hash()[2] = byte{ 0x12 };

    node2.payload = make_bytes(0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A);
    node2.label = partial_label_type{ 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 };
    node2.right = partial_label_type{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 };

    stringstream ss2;

    save_size = node2.save(ss2);
    result = CTNode::Load(ss2);

    EXPECT_EQ(node2.hash(), get<0>(result).hash());
    EXPECT_EQ(node2.payload, get<0>(result).payload);
    EXPECT_EQ(node2.label, get<0>(result).label);
    EXPECT_EQ(empty_label, get<1>(result));
    EXPECT_EQ(node2.right, get<2>(result));
    EXPECT_EQ(save_size, get<3>(result));
}
