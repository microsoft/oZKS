// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "oZKS/ct_node.h"
#include "oZKS/insert_result.h"
#include "oZKS/utilities.h"

#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

TEST(InsertResultTests, VerifyTest1)
{
    CTNode root;

    append_proof_type append_proof;

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

    InsertResult res;
    commitment_type commitment(root.hash().size());
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());

    res.init_result(commitment, path1);
    EXPECT_TRUE(res.verify());

    res.init_result(commitment, path2);
    EXPECT_TRUE(res.verify());

    res.init_result(commitment, path3);
    EXPECT_TRUE(res.verify());

    res.init_result(commitment, path4);
    EXPECT_TRUE(res.verify());

    res.init_result(commitment, path5);
    EXPECT_TRUE(res.verify());
}

TEST(InsertResultTests, VerifyTest2)
{
    label_type bytes1 = make_bytes(0xAA, 0xBB, 0xCC, 0xDD, 0xEE);
    label_type bytes2 = make_bytes(0x01, 0x01, 0x01, 0x01, 0x01);
    label_type bytes3 = make_bytes(0x10, 0x10, 0x10, 0x10, 0x10);
    label_type bytes4 = make_bytes(0x80, 0x80, 0x80, 0x80, 0x80);
    label_type bytes5 = make_bytes(0xC0, 0xC0, 0xC0, 0xC0, 0xC0);
    label_type bytes6 = make_bytes(0xE0, 0xE0, 0xE0, 0xE0, 0xE0);
    label_type bytes7 = make_bytes(0xF0, 0xF0, 0xF0, 0xF0, 0xF0);

    payload_type payload = make_bytes(0x01, 0x02);

    partial_label_type label1 = bytes_to_bools(bytes1);
    partial_label_type label2 = bytes_to_bools(bytes2);
    partial_label_type label3 = bytes_to_bools(bytes3);
    partial_label_type label4 = bytes_to_bools(bytes4);
    partial_label_type label5 = bytes_to_bools(bytes5);
    partial_label_type label6 = bytes_to_bools(bytes6);
    partial_label_type label7 = bytes_to_bools(bytes7);

    CTNode root;
    append_proof_type append_proof;

    root.insert(label1, payload, /* epoch */ 1);
    root.update_hashes(label1);
    EXPECT_TRUE(root.lookup(label1, append_proof, /* include_searched */ true));
    commitment_type commitment(root.hash().size());
    InsertResult res;
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());
    res.init_result(commitment, append_proof);
    EXPECT_TRUE(res.verify());

    root.insert(label2, payload, /* epoch */ 2);
    root.update_hashes(label2);
    append_proof.clear();
    EXPECT_TRUE(root.lookup(label2, append_proof, /* include_searched */ true));
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());
    res.init_result(commitment, append_proof);
    EXPECT_TRUE(res.verify());

    root.insert(label3, payload, /* epoch */ 3);
    root.update_hashes(label3);
    append_proof.clear();
    EXPECT_TRUE(root.lookup(label3, append_proof, /* include_searched */ true));
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());
    res.init_result(commitment, append_proof);
    EXPECT_TRUE(res.verify());

    root.insert(label4, payload, /* epoch */ 4);
    root.update_hashes(label4);
    append_proof.clear();
    EXPECT_TRUE(root.lookup(label4, append_proof, /* include_searched */ true));
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());
    res.init_result(commitment, append_proof);
    EXPECT_TRUE(res.verify());

    root.insert(label5, payload, /* epoch */ 5);
    root.update_hashes(label5);
    append_proof.clear();
    EXPECT_TRUE(root.lookup(label5, append_proof, /* include_searched */ true));
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());
    res.init_result(commitment, append_proof);
    EXPECT_TRUE(res.verify());

    root.insert(label6, payload, /* epoch */ 6);
    root.update_hashes(label6);
    append_proof.clear();
    EXPECT_TRUE(root.lookup(label6, append_proof, /* include_searched */ true));
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());
    res.init_result(commitment, append_proof);
    EXPECT_TRUE(res.verify());

    root.insert(label7, payload, /* epoch */ 7);
    root.update_hashes(label7);
    append_proof.clear();
    EXPECT_TRUE(root.lookup(label7, append_proof, /* include_searched */ true));
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());
    res.init_result(commitment, append_proof);
    EXPECT_TRUE(res.verify());
}
