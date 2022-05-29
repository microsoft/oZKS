// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <sstream>

// OZKS
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
    CTNode root;
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
    vector<byte> trie_id = make_bytes(0xaa, 0xbb, 0xcc);
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CTNode node(storage, trie_id);

    label_type label = make_bytes(0x01);
    payload_type payload = make_bytes(0xF0, 0xF1, 0xF2);
    partial_label_type lab = bytes_to_bools(label);
    lookup_path_type lookup_path;

    node.insert(lab, payload, /* epoch */ 1);
    node.update_hashes(lab);

    CTNode left_node;
    CTNode right_node;
    storage->LoadCTNode(trie_id, node.left, left_node);
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
    storage->LoadCTNode(trie_id, node.left, left_node);

    EXPECT_FALSE(left_node.left.empty());
    EXPECT_FALSE(left_node.right.empty());

    CTNode left_left_node;
    storage->LoadCTNode(trie_id, left_node.left, left_left_node);
    EXPECT_EQ(
        "2de2976b794dc48d2e939b2e16d7db73701dbf2c1641ca93e4ad74b8f029cf07f02d01fb9c0b31dc740056acc9"
        "c17ddfd5c7d5d4471b227e5033d0169180f1a2",
        to_string(left_left_node.hash()));

    CTNode left_right_node;
    storage->LoadCTNode(trie_id, left_node.right, left_right_node);
    EXPECT_EQ(
        "9aba28398d409188b8d44a66a2b29d489d37d0946617c72bddd4d6ac324deca00a4322da731f7bd137adb07ea5"
        "05d02296ea2ba1b53f71e733d83834e9d79961",
        to_string(left_right_node.hash()));
    EXPECT_NE(hash_root, node.hash());
}

// TEST(CTNodeTests, AllNodesHashedTest)
// {
//     CTNode node;

//     label_type label = make_bytes(0x01);
//     payload_type payload = make_bytes(0x01, 0x02, 0x03);
//     partial_label_type lab = bytes_to_bools(label);
//     hash_type hash_root = node.hash();
//     lookup_path_type lookup_path;

//     node.insert(lab, payload, 1);
//     node.update_hashes(lab);

//     EXPECT_NE(hash_root, node.hash()); // Updated
//     EXPECT_NE(nullptr, node.left);
//     EXPECT_EQ(nullptr, node.right);

//     hash_root = node.hash();
//     hash_type hash_01 = node.left->hash();

//     lab = bytes_to_bools(make_bytes(0x02));
//     payload = make_bytes(0x04, 0x05, 0x06);

//     node.insert(lab, payload, 2);
//     node.update_hashes(lab);

//     EXPECT_NE(hash_root, node.hash()); // Updated
//     partial_label_type label_000000{ 0, 0, 0, 0, 0, 0 };
//     EXPECT_EQ(label_000000, node.left->label);
//     hash_type hash_000000 = node.left->hash();
//     partial_label_type label_01{ 0, 0, 0, 0, 0, 0, 0, 1 };
//     EXPECT_EQ(label_01, node.left->left->label);
//     EXPECT_EQ(hash_01, node.left->left->hash());
//     partial_label_type label_02{ 0, 0, 0, 0, 0, 0, 1, 0 };
//     EXPECT_EQ(label_02, node.left->right->label);
//     hash_type hash_02 = node.left->right->hash();
//     EXPECT_NE(hash_01, hash_02);
//     EXPECT_NE(hash_000000, hash_01);
//     EXPECT_NE(hash_000000, hash_02);

//     hash_root = node.hash();
//     lab = bytes_to_bools(make_bytes(0x03));
//     payload = make_bytes(0x04, 0x05, 0x06);

//     node.insert(lab, payload, 3);
//     node.update_hashes(lab);

//     EXPECT_NE(hash_root, node.hash());
//     EXPECT_EQ(label_000000, node.left->label);
//     EXPECT_NE(hash_000000, node.left->hash()); // Updated
//     hash_000000 = node.left->hash();
//     EXPECT_EQ(label_01, node.left->left->label);
//     EXPECT_EQ(hash_01, node.left->left->hash());
//     partial_label_type label_0000001{ 0, 0, 0, 0, 0, 0, 1 };
//     EXPECT_EQ(label_0000001, node.left->right->label);
//     hash_type hash_0000001 = node.left->right->hash();
//     EXPECT_EQ(label_02, node.left->right->left->label);
//     EXPECT_EQ(hash_02, node.left->right->left->hash());
//     partial_label_type label_03{ 0, 0, 0, 0, 0, 0, 1, 1 };
//     EXPECT_EQ(label_03, node.left->right->right->label);
//     hash_type hash_03 = node.left->right->right->hash();
//     EXPECT_NE(hash_01, hash_03);
//     EXPECT_NE(hash_02, hash_03);

//     hash_root = node.hash();
//     lab = bytes_to_bools(make_bytes(0x04));
//     payload = make_bytes(0x04, 0x05, 0x06);

//     node.insert(lab, payload, 4);
//     node.update_hashes(lab);

//     EXPECT_NE(hash_root, node.hash()); // Updated
//     partial_label_type label_00000{ 0, 0, 0, 0, 0 };
//     EXPECT_EQ(label_00000, node.left->label);
//     hash_type hash_00000 = node.left->hash();
//     EXPECT_EQ(label_000000, node.left->left->label);
//     EXPECT_EQ(hash_000000, node.left->left->hash());
//     hash_000000 = node.left->left->hash();
//     EXPECT_EQ(label_01, node.left->left->left->label);
//     EXPECT_EQ(hash_01, node.left->left->left->hash());
//     EXPECT_EQ(label_0000001, node.left->left->right->label);
//     EXPECT_EQ(hash_0000001, node.left->left->right->hash());
//     EXPECT_EQ(label_02, node.left->left->right->left->label);
//     EXPECT_EQ(hash_02, node.left->left->right->left->hash());
//     EXPECT_EQ(label_03, node.left->left->right->right->label);
//     EXPECT_EQ(hash_03, node.left->left->right->right->hash());
//     partial_label_type label_04{ 0, 0, 0, 0, 0, 1, 0, 0 };
//     EXPECT_EQ(label_04, node.left->right->label);

//     hash_root = node.hash();
//     lab = bytes_to_bools(make_bytes(0x80));
//     payload = make_bytes(0x04, 0x05, 0x06);

//     node.insert(lab, payload, 5);
//     node.update_hashes(lab);

//     EXPECT_NE(hash_root, node.hash()); // Updated
//     EXPECT_EQ(label_00000, node.left->label);
//     EXPECT_EQ(hash_00000, node.left->hash());
//     partial_label_type label_80{ 1, 0, 0, 0, 0, 0, 0, 0 };
//     EXPECT_EQ(label_80, node.right->label);
//     hash_type hash_80 = node.right->hash();

//     hash_root = node.hash();
//     lab = bytes_to_bools(make_bytes(0x81));
//     payload = make_bytes(0x04, 0x05, 0x06);

//     node.insert(lab, payload, 6);
//     node.update_hashes(lab);

//     EXPECT_NE(hash_root, node.hash()); // Updated
//     EXPECT_EQ(label_00000, node.left->label);
//     EXPECT_EQ(hash_00000, node.left->hash());
//     partial_label_type label_1000000{ 1, 0, 0, 0, 0, 0, 0 };
//     EXPECT_EQ(label_1000000, node.right->label);
//     hash_type hash_1000000 = node.right->hash();
//     EXPECT_EQ(label_80, node.right->left->label);
//     EXPECT_EQ(hash_80, node.right->left->hash());
//     partial_label_type label_81{ 1, 0, 0, 0, 0, 0, 0, 1 };
//     EXPECT_EQ(label_81, node.right->right->label);
//     hash_type hash_81 = node.right->right->hash();
//     EXPECT_NE(hash_80, hash_81);
//     EXPECT_NE(hash_81, hash_1000000);

//     hash_root = node.hash();
//     lab = bytes_to_bools(make_bytes(0x82));
//     payload = make_bytes(0x04, 0x05, 0x06);

//     node.insert(lab, payload, 7);
//     node.update_hashes(lab);

//     EXPECT_NE(hash_root, node.hash()); // Updated
//     EXPECT_EQ(label_00000, node.left->label);
//     EXPECT_EQ(hash_00000, node.left->hash());
//     partial_label_type label_100000{ 1, 0, 0, 0, 0, 0 };
//     EXPECT_EQ(label_100000, node.right->label);
//     EXPECT_EQ(label_1000000, node.right->left->label);
//     EXPECT_EQ(hash_1000000, node.right->left->hash());
//     EXPECT_EQ(label_80, node.right->left->left->label);
//     EXPECT_EQ(hash_80, node.right->left->left->hash());
//     EXPECT_EQ(label_81, node.right->left->right->label);
//     EXPECT_EQ(hash_81, node.right->left->right->hash());
//     partial_label_type label_82{ 1, 0, 0, 0, 0, 0, 1, 0 };
//     EXPECT_EQ(label_82, node.right->right->label);
//     hash_type hash_82 = node.right->right->hash();
//     EXPECT_NE(hash_82, hash_81);
//     EXPECT_NE(hash_82, hash_80);
// }

// TEST(CTNodeTests, SaveLoadTest)
// {
//     CTNode node;

//     node.hash()[0] = byte{ 0x01 };
//     node.hash()[1] = byte{ 0x02 };
//     node.hash()[2] = byte{ 0x0f };

//     node.payload = make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);
//     node.label = partial_label_type{ 1, 0, 0, 1, 1, 0, 0, 1, 1 };

//     node.left = make_unique<CTNode>();
//     node.left->label = partial_label_type{ 1, 0, 0, 1 };

//     vector<bool> empty_label;
//     stringstream ss;

//     size_t save_size = node.save(ss);

//     auto result = CTNode::load(ss);

//     EXPECT_EQ(node.hash(), get<0>(result).hash());
//     EXPECT_EQ(node.payload, get<0>(result).payload);
//     EXPECT_EQ(node.label, get<0>(result).label);
//     EXPECT_EQ(node.left->label, get<1>(result));
//     EXPECT_EQ(empty_label, get<2>(result));
//     EXPECT_EQ(save_size, get<3>(result));

//     CTNode node2;
//     node2.hash()[0] = byte{ 0x10 };
//     node2.hash()[1] = byte{ 0x11 };
//     node2.hash()[2] = byte{ 0x12 };

//     node2.payload = make_bytes(0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A);
//     node2.label = partial_label_type{ 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 };

//     node2.right = make_unique<CTNode>();
//     node2.right->label = partial_label_type{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 };

//     stringstream ss2;

//     save_size = node2.save(ss2);
//     result = CTNode::load(ss2);

//     EXPECT_EQ(node2.hash(), get<0>(result).hash());
//     EXPECT_EQ(node2.payload, get<0>(result).payload);
//     EXPECT_EQ(node2.label, get<0>(result).label);
//     EXPECT_EQ(empty_label, get<1>(result));
//     EXPECT_EQ(node2.right->label, get<2>(result));
//     EXPECT_EQ(save_size, get<3>(result));
// }

// TEST(CTNodeTests, SaveLoadToVectorTest)
// {
//     CTNode node;

//     node.hash()[0] = byte{ 0x01 };
//     node.hash()[1] = byte{ 0x02 };
//     node.hash()[2] = byte{ 0x0f };

//     node.payload = make_bytes(0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA);
//     node.label = partial_label_type{ 1, 0, 0, 1, 1, 0, 0, 1, 1 };

//     node.left = make_unique<CTNode>();
//     node.left->label = partial_label_type{ 1, 0, 0, 1 };

//     vector<bool> empty_label;
//     vector<byte> vec;

//     size_t save_size = node.save(vec);

//     auto result = CTNode::load(vec);

//     EXPECT_EQ(node.hash(), get<0>(result).hash());
//     EXPECT_EQ(node.payload, get<0>(result).payload);
//     EXPECT_EQ(node.label, get<0>(result).label);
//     EXPECT_EQ(node.left->label, get<1>(result));
//     EXPECT_EQ(empty_label, get<2>(result));
//     EXPECT_EQ(save_size, get<3>(result));

//     CTNode node2;
//     node2.hash()[0] = byte{ 0x10 };
//     node2.hash()[1] = byte{ 0x11 };
//     node2.hash()[2] = byte{ 0x12 };

//     node2.payload = make_bytes(0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A);
//     node2.label = partial_label_type{ 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 };

//     node2.right = make_unique<CTNode>();
//     node2.right->label = partial_label_type{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 };

//     stringstream ss2;

//     save_size = node2.save(ss2);
//     result = CTNode::load(ss2);

//     EXPECT_EQ(node2.hash(), get<0>(result).hash());
//     EXPECT_EQ(node2.payload, get<0>(result).payload);
//     EXPECT_EQ(node2.label, get<0>(result).label);
//     EXPECT_EQ(empty_label, get<1>(result));
//     EXPECT_EQ(node2.right->label, get<2>(result));
//     EXPECT_EQ(save_size, get<3>(result));
// }
