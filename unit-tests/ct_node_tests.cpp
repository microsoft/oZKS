// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <sstream>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/config.h"
#include "oZKS/ct_node.h"
#include "oZKS/ct_node_linked.h"
#include "oZKS/ct_node_stored.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"

// GSL
#include "gsl/span"
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

void DoInsertTest(shared_ptr<CTNode> root)
{
    PartialLabel label1{ true, true, true, true };
    PartialLabel label2{ true, true, true, false };
    PartialLabel label3{ true, false, false, false };
    PartialLabel label4{ true, false, false, true };
    PartialLabel label5{ true, false, true, true };
    hash_type payload1 = make_bytes<hash_type>(0xFF, 0xFE, 0xFD, 0xFC);
    hash_type payload2 = make_bytes<hash_type>(0xFE, 0xFD, 0xFC, 0xFB);
    hash_type payload3 = make_bytes<hash_type>(0xFD, 0xFC, 0xFB, 0xFA);
    hash_type payload4 = make_bytes<hash_type>(0xFC, 0xFB, 0xFA, 0xF9);
    hash_type payload5 = make_bytes<hash_type>(0xFB, 0xFA, 0xF9, 0xF8);

    root->insert(label1, payload1, /* epoch */ 1);
    root->insert(label2, payload2, /* epoch */ 1);
    root->insert(label3, payload3, /* epoch */ 1);
    root->insert(label4, payload4, /* epoch */ 1);
    root->insert(label5, payload5, /* epoch */ 1);

    root->update_hashes(label1);
    root->update_hashes(label2);
    root->update_hashes(label3);
    root->update_hashes(label4);
    root->update_hashes(label5);

    string root_str = root->to_string();
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

    EXPECT_TRUE(root->lookup(label1, path1, /* include_searched */ true));
    EXPECT_TRUE(root->lookup(label2, path2, /* include_searched */ true));
    EXPECT_TRUE(root->lookup(label3, path3, /* include_searched */ true));
    EXPECT_TRUE(root->lookup(label4, path4, /* include_searched */ true));
    EXPECT_TRUE(root->lookup(label5, path5, /* include_searched */ true));

    lookup_path_type path11;
    lookup_path_type path22;
    lookup_path_type path33;
    lookup_path_type path44;
    lookup_path_type path55;

    EXPECT_TRUE(CTNode::lookup(label1, root, path11, /* include_searched */ true));
    EXPECT_TRUE(CTNode::lookup(label2, root, path22, /* include_searched */ true));
    EXPECT_TRUE(CTNode::lookup(label3, root, path33, /* include_searched */ true));
    EXPECT_TRUE(CTNode::lookup(label4, root, path44, /* include_searched */ true));
    EXPECT_TRUE(CTNode::lookup(label5, root, path55, /* include_searched */ true));

    PartialLabel left_common{ true, false };
    EXPECT_EQ(path1[0].first, label1);
    EXPECT_EQ(path1[1].first, label2);
    EXPECT_EQ(path1[2].first, left_common);

    EXPECT_EQ(path11[0].first, label1);
    EXPECT_EQ(path11[1].first, label2);
    EXPECT_EQ(path11[2].first, left_common);
}

TEST(CTNodeTests, StoredInsertTest)
{
    shared_ptr storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    shared_ptr<CTNode> root = make_shared<CTNodeStored>(&trie);

    DoInsertTest(root);
}

TEST(CTNodeTests, LinkedInsertTest)
{
    CompressedTrie trie;
    shared_ptr<CTNode> root = make_shared<CTNodeLinked>(&trie);
    DoInsertTest(root);
}

void DoUpdateHashTest(shared_ptr<CTNode> root)
{
    PartialLabel label = make_bytes<PartialLabel>(0x01);
    hash_type payload = make_bytes<hash_type>(0xF0, 0xF1, 0xF2);
    lookup_path_type lookup_path;

    root->insert(label, payload, /* epoch */ 1);
    root->update_hashes(label);

    shared_ptr<CTNode> left_node = root->left();
    shared_ptr<CTNode> right_node = root->right();

    EXPECT_EQ(label, left_node->label());
    EXPECT_FALSE(nullptr == left_node);
#ifdef OZKS_USE_OPENSSL_SHA2
    EXPECT_EQ(
        "02f59c3b7eabe50c198271492acd99ffa1ab5cd77187b08ce58bfe6042a824fc",
        to_string(left_node->hash()));
#else
    EXPECT_EQ(
        "d62bf80933cf992e03269ad9c20e01ac5b48e6d33ed88c53da4b8ab6ecf3f5d9",
        to_string(left_node->hash()));
#endif

    label = make_bytes<PartialLabel>(0x02);
    payload = make_bytes<hash_type>(0xE0, 0xE1, 0xE2);

    hash_type hash_root = root->hash();
    root->insert(label, payload, 2);
    root->update_hashes(label);

    EXPECT_FALSE(nullptr == left_node);
    left_node = root->left();

    shared_ptr<CTNode> left_left_node = left_node->left();
    shared_ptr<CTNode> left_right_node = left_node->right();
    EXPECT_FALSE(nullptr == left_left_node);
    EXPECT_FALSE(nullptr == left_right_node);

#ifdef OZKS_USE_OPENSSL_SHA2
    EXPECT_EQ(
        "02f59c3b7eabe50c198271492acd99ffa1ab5cd77187b08ce58bfe6042a824fc",
        to_string(left_left_node->hash()));
    EXPECT_EQ(
        "a4b9d4701038d31cdef70c2ee0556033cb26165402cd69352300d3d33e386374",
        to_string(left_right_node->hash()));
#else
    EXPECT_EQ(
        "d62bf80933cf992e03269ad9c20e01ac5b48e6d33ed88c53da4b8ab6ecf3f5d9",
        to_string(left_left_node->hash()));
    EXPECT_EQ(
        "8605735e32a63473571408785a63bf36b1d19fb3329eaa2c4d1424ded5f2c3b8",
        to_string(left_right_node->hash()));
#endif

    EXPECT_NE(hash_root, root->hash());
}

TEST(CTNodeTests, StoredUpdateHashTest)
{
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    shared_ptr<CTNode> root = make_shared<CTNodeStored>(&trie);

    DoUpdateHashTest(root);
}

TEST(CTNodeTests, LinkedUpdateHashTest)
{
    CompressedTrie trie;
    shared_ptr<CTNode> root = make_shared<CTNodeLinked>(&trie);

    DoUpdateHashTest(root);
}

void DoAllNodesHashedTest(shared_ptr<CTNode> root)
{
    PartialLabel label = make_bytes<PartialLabel>(0x01);
    hash_type payload = make_bytes<hash_type>(0x01, 0x02, 0x03);
    hash_type hash_root = root->hash();
    lookup_path_type lookup_path;

    root->insert(label, payload, 1);
    root->update_hashes(label);

    shared_ptr<CTNode> left_node = root->left();
    shared_ptr<CTNode> right_node = root->right();

    EXPECT_NE(hash_root, root->hash()); // Updated
    EXPECT_FALSE(nullptr == left_node);
    EXPECT_TRUE(nullptr == right_node);

    hash_root = root->hash();
    hash_type hash_01 = left_node->hash();

    label = make_bytes<PartialLabel>(0x02);
    payload = make_bytes<hash_type>(0x04, 0x05, 0x06);

    root->insert(label, payload, 2);
    root->update_hashes(label);

    EXPECT_NE(hash_root, root->hash()); // Updated
    PartialLabel label_000000{ 0, 0, 0, 0, 0, 0 };

    left_node = root->left();
    EXPECT_EQ(label_000000, left_node->label());
    hash_type hash_000000 = left_node->hash();
    PartialLabel label_01{ 0, 0, 0, 0, 0, 0, 0, 1 };

    shared_ptr<CTNode> left_left_node = left_node->left();
    EXPECT_EQ(label_01, left_left_node->label());
    EXPECT_EQ(hash_01, left_left_node->hash());
    PartialLabel label_02{ 0, 0, 0, 0, 0, 0, 1, 0 };

    shared_ptr<CTNode> left_right_node = left_node->right();
    EXPECT_EQ(label_02, left_right_node->label());
    hash_type hash_02 = left_right_node->hash();
    EXPECT_NE(hash_01, hash_02);
    EXPECT_NE(hash_000000, hash_01);
    EXPECT_NE(hash_000000, hash_02);

    hash_root = root->hash();
    label = make_bytes<PartialLabel>(0x03);
    payload = make_bytes<hash_type>(0x04, 0x05, 0x06);

    root->insert(label, payload, 3);
    root->update_hashes(label);

    EXPECT_NE(hash_root, root->hash());

    left_node = root->left();
    EXPECT_EQ(label_000000, left_node->label());
    EXPECT_NE(hash_000000, left_node->hash()); // Updated
    hash_000000 = left_node->hash();

    left_left_node = left_node->left();
    EXPECT_EQ(label_01, left_left_node->label());
    EXPECT_EQ(hash_01, left_left_node->hash());
    PartialLabel label_0000001{ 0, 0, 0, 0, 0, 0, 1 };

    left_right_node = left_node->right();
    EXPECT_EQ(label_0000001, left_right_node->label());
    hash_type hash_0000001 = left_right_node->hash();

    shared_ptr<CTNode> left_right_left_node = left_right_node->left();
    EXPECT_EQ(label_02, left_right_left_node->label());
    EXPECT_EQ(hash_02, left_right_left_node->hash());
    PartialLabel label_03{ 0, 0, 0, 0, 0, 0, 1, 1 };

    shared_ptr<CTNode> left_right_right_node = left_right_node->right();
    EXPECT_EQ(label_03, left_right_right_node->label());
    hash_type hash_03 = left_right_right_node->hash();
    EXPECT_NE(hash_01, hash_03);
    EXPECT_NE(hash_02, hash_03);

    hash_root = root->hash();
    label = make_bytes<PartialLabel>(0x04);
    payload = make_bytes<hash_type>(0x04, 0x05, 0x06);

    root->insert(label, payload, 4);
    root->update_hashes(label);

    EXPECT_NE(hash_root, root->hash()); // Updated
    PartialLabel label_00000{ 0, 0, 0, 0, 0 };

    left_node = root->left();
    EXPECT_EQ(label_00000, left_node->label());
    hash_type hash_00000 = left_node->hash();

    left_left_node = left_node->left();
    EXPECT_EQ(label_000000, left_left_node->label());
    EXPECT_EQ(hash_000000, left_left_node->hash());
    hash_000000 = left_left_node->hash();

    shared_ptr<CTNode> left_left_left_node = left_left_node->left();
    EXPECT_EQ(label_01, left_left_left_node->label());
    EXPECT_EQ(hash_01, left_left_left_node->hash());

    shared_ptr<CTNode> left_left_right_node = left_left_node->right();
    EXPECT_EQ(label_0000001, left_left_right_node->label());
    EXPECT_EQ(hash_0000001, left_left_right_node->hash());

    shared_ptr<CTNode> left_left_right_left_node = left_left_right_node->left();
    EXPECT_EQ(label_02, left_left_right_left_node->label());
    EXPECT_EQ(hash_02, left_left_right_left_node->hash());

    shared_ptr<CTNode> left_left_right_right_node = left_left_right_node->right();
    EXPECT_EQ(label_03, left_left_right_right_node->label());
    EXPECT_EQ(hash_03, left_left_right_right_node->hash());
    PartialLabel label_04{ 0, 0, 0, 0, 0, 1, 0, 0 };

    left_right_node = left_node->right();
    EXPECT_EQ(label_04, left_right_node->label());

    hash_root = root->hash();
    label = make_bytes<PartialLabel>(0x80);
    payload = make_bytes<hash_type>(0x04, 0x05, 0x06);

    root->insert(label, payload, 5);
    root->update_hashes(label);

    EXPECT_NE(hash_root, root->hash()); // Updated

    left_node = root->left();
    EXPECT_EQ(label_00000, left_node->label());
    EXPECT_EQ(hash_00000, left_node->hash());
    PartialLabel label_80{ 1, 0, 0, 0, 0, 0, 0, 0 };

    right_node = root->right();
    EXPECT_EQ(label_80, right_node->label());
    hash_type hash_80 = right_node->hash();

    hash_root = root->hash();
    label = make_bytes<PartialLabel>(0x81);
    payload = make_bytes<hash_type>(0x04, 0x05, 0x06);

    root->insert(label, payload, 6);
    root->update_hashes(label);

    EXPECT_NE(hash_root, root->hash()); // Updated

    left_node = root->left();
    EXPECT_EQ(label_00000, left_node->label());
    EXPECT_EQ(hash_00000, left_node->hash());
    PartialLabel label_1000000{ 1, 0, 0, 0, 0, 0, 0 };

    right_node = root->right();
    EXPECT_EQ(label_1000000, right_node->label());
    hash_type hash_1000000 = right_node->hash();

    shared_ptr<CTNode> right_left_node = right_node->left();
    EXPECT_EQ(label_80, right_left_node->label());
    EXPECT_EQ(hash_80, right_left_node->hash());
    PartialLabel label_81{ 1, 0, 0, 0, 0, 0, 0, 1 };

    shared_ptr<CTNode> right_right_node = right_node->right();
    EXPECT_EQ(label_81, right_right_node->label());
    hash_type hash_81 = right_right_node->hash();
    EXPECT_NE(hash_80, hash_81);
    EXPECT_NE(hash_81, hash_1000000);

    hash_root = root->hash();
    label = make_bytes<PartialLabel>(0x82);
    payload = make_bytes<hash_type>(0x04, 0x05, 0x06);

    root->insert(label, payload, 7);
    root->update_hashes(label);

    EXPECT_NE(hash_root, root->hash()); // Updated

    left_node = root->left();
    EXPECT_EQ(label_00000, left_node->label());
    EXPECT_EQ(hash_00000, left_node->hash());
    PartialLabel label_100000{ 1, 0, 0, 0, 0, 0 };

    right_node = root->right();
    EXPECT_EQ(label_100000, right_node->label());

    right_left_node = right_node->left();
    EXPECT_EQ(label_1000000, right_left_node->label());
    EXPECT_EQ(hash_1000000, right_left_node->hash());

    shared_ptr<CTNode> right_left_left_node = right_left_node->left();
    EXPECT_EQ(label_80, right_left_left_node->label());
    EXPECT_EQ(hash_80, right_left_left_node->hash());

    shared_ptr<CTNode> right_left_right_node = right_left_node->right();
    EXPECT_EQ(label_81, right_left_right_node->label());
    EXPECT_EQ(hash_81, right_left_right_node->hash());
    PartialLabel label_82{ 1, 0, 0, 0, 0, 0, 1, 0 };

    right_right_node = right_node->right();
    EXPECT_EQ(label_82, right_right_node->label());
    hash_type hash_82 = right_right_node->hash();
    EXPECT_NE(hash_82, hash_81);
    EXPECT_NE(hash_82, hash_80);
}

TEST(CTNodeTests, StoredAllNodesHashedTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    shared_ptr<CTNode> root = make_shared<CTNodeStored>(&trie);

    DoAllNodesHashedTest(root);
}

TEST(CTNodeTests, LinkedAllNodesHashedTest)
{
    CompressedTrie trie;
    shared_ptr<CTNode> root = make_shared<CTNodeLinked>(&trie);

    DoAllNodesHashedTest(root);
}

TEST(CTNodeTests, StoredSaveLoadTest)
{
    shared_ptr<storage::MemoryStorage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    CTNodeStored node(&trie);

    hash_type hash{};
    PartialLabel label;

    hash[0] = byte{ 0x01 };
    hash[1] = byte{ 0x02 };
    hash[2] = byte{ 0x0f };

    label = PartialLabel{ 1, 0, 0, 1, 1, 0, 0, 1, 1 };
    EXPECT_EQ(9, label.bit_count());

    node.init(label, hash);
    node.left_ = PartialLabel{ 1, 0, 0, 1 };

    // Make sure left node exists in storage
    CTNodeStored left_node(&trie, node.left_);
    left_node.save_to_storage();

    PartialLabel empty_label;
    stringstream ss;

    size_t save_size = node.save(ss);

    auto result = CTNodeStored::Load(ss, &trie);

    EXPECT_EQ(node.hash(), get<0>(result).hash());
    EXPECT_EQ(node.label(), get<0>(result).label());
    EXPECT_EQ(node.left()->label(), get<1>(result));
    EXPECT_EQ(empty_label, get<2>(result));
    EXPECT_EQ(save_size, get<3>(result));

    CTNodeStored node2(&trie);
    hash[0] = byte{ 0x10 };
    hash[1] = byte{ 0x11 };
    hash[2] = byte{ 0x12 };

    label = PartialLabel{ 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 };
    EXPECT_EQ(12, label.bit_count());

    node2.init(label, hash);
    node2.right_ = PartialLabel{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 };

    // Make sure right node exists in storage
    CTNodeStored right_node(&trie, node2.right_);
    right_node.save_to_storage();

    stringstream ss2;

    save_size = node2.save(ss2);
    result = CTNodeStored::Load(ss2, &trie);

    EXPECT_EQ(node2.hash(), get<0>(result).hash());
    EXPECT_EQ(node2.label(), get<0>(result).label());
    EXPECT_EQ(12, get<0>(result).label().bit_count());
    EXPECT_EQ(empty_label, get<1>(result));
    EXPECT_EQ(node2.right()->label(), get<2>(result));
    EXPECT_EQ(save_size, get<3>(result));
    EXPECT_EQ(12, get<2>(result).bit_count());
}

TEST(CTNodeTests, StoredSaveLoadToVectorTest)
{
    shared_ptr<storage::MemoryStorage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage, TrieType::Stored);
    CTNodeStored node(&trie);
    hash_type hash{};
    PartialLabel label;

    hash[0] = byte{ 0x01 };
    hash[1] = byte{ 0x02 };
    hash[2] = byte{ 0x0f };

    label = PartialLabel{ 1, 0, 0, 1, 1, 0, 0, 1, 1 };

    node.init(label, hash);
    node.left_ = PartialLabel{ 1, 0, 0, 1 };

    // Make sure left node exists in storage
    CTNodeStored left_node(&trie, node.left_);
    left_node.save_to_storage();

    PartialLabel empty_label;
    vector<byte> vec;

    size_t save_size = node.save(vec);

    auto result = CTNodeStored::Load(vec, &trie);

    EXPECT_EQ(node.hash(), get<0>(result).hash());
    EXPECT_EQ(node.label(), get<0>(result).label());
    EXPECT_EQ(node.left()->label(), get<1>(result));
    EXPECT_EQ(empty_label, get<2>(result));
    EXPECT_EQ(save_size, get<3>(result));

    CTNodeStored node2(&trie);
    hash[0] = byte{ 0x10 };
    hash[1] = byte{ 0x11 };
    hash[2] = byte{ 0x12 };

    label = PartialLabel{ 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 };

    node2.init(label, hash);
    node2.right_ = PartialLabel{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 };

    // Make sure right node exists in storage
    CTNodeStored right_node(&trie, node2.right_);
    right_node.save_to_storage();

    stringstream ss2;

    save_size = node2.save(ss2);
    result = CTNodeStored::Load(ss2, &trie);

    EXPECT_EQ(node2.hash(), get<0>(result).hash());
    EXPECT_EQ(node2.label(), get<0>(result).label());
    EXPECT_EQ(empty_label, get<1>(result));
    EXPECT_EQ(node2.right()->label(), get<2>(result));
    EXPECT_EQ(save_size, get<3>(result));
}
