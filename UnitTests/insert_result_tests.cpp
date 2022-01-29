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

TEST(InsertResultTests, VerifyBatchInsertTest)
{
    CTNode root;

    append_proof_type append_proof;
    vector<partial_label_type> labels;
    payload_type payload = make_bytes(0xFF, 0xFE);

    labels.emplace_back(partial_label_type { true, true, true, true });
    labels.emplace_back(partial_label_type { true, true, true, false });
    labels.emplace_back(partial_label_type { true, false, false, false });
    labels.emplace_back(partial_label_type { true, false, false, true });
    labels.emplace_back(partial_label_type { true, false, true, true });

    for (const auto label : labels) {
        root.insert(label, payload, /* epoch */ 1);
    }

    for (const auto label : labels) {
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

    commitment_type commitment(root.hash().size());
    copy_bytes(root.hash().data(), root.hash().size(), commitment.data());

    for (const auto label : labels) {
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
    payload_type payload = make_bytes(0x01, 0x02);
    vector<partial_label_type> labels;
    CTNode root;
    commitment_type commitment(root.hash().size());

    labels.emplace_back(bytes_to_bools(make_bytes(0xAA, 0xBB, 0xCC, 0xDD, 0xEE)));
    labels.emplace_back(bytes_to_bools(make_bytes(0x01, 0x01, 0x01, 0x01, 0x01)));
    labels.emplace_back(bytes_to_bools(make_bytes(0x10, 0x10, 0x10, 0x10, 0x10)));
    labels.emplace_back(bytes_to_bools(make_bytes(0x80, 0x80, 0x80, 0x80, 0x80)));
    labels.emplace_back(bytes_to_bools(make_bytes(0xC0, 0xC0, 0xC0, 0xC0, 0xC0)));
    labels.emplace_back(bytes_to_bools(make_bytes(0xE0, 0xE0, 0xE0, 0xE0, 0xE0)));
    labels.emplace_back(bytes_to_bools(make_bytes(0xF0, 0xF0, 0xF0, 0xF0, 0xF0)));

    size_t epoch = 1;
    for (const auto label : labels) {
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
