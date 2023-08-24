// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <iostream>

// OZKS
#include "oZKS/query_result.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"

// GTest
#include "gtest/gtest.h"

using namespace ozks;
using namespace ozks::storage;
using namespace std;

void InitQueryResult(QueryResult &qr, const OZKSConfig &config)
{
    EXPECT_FALSE(qr.is_member());

    key_type key;
    lookup_path_type lookup_proof;
    payload_type payload;
    randomness_type randomness;
    VRFProof vrf_proof;

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
    vrf_proof.gamma[0] = static_cast<byte>(0x10);
    vrf_proof.gamma[1] = static_cast<byte>(0x11);
    vrf_proof.gamma[2] = static_cast<byte>(0x12);
    vrf_proof.c[0] = static_cast<byte>(0x21);
    vrf_proof.c[1] = static_cast<byte>(0x22);
    vrf_proof.c[2] = static_cast<byte>(0x23);
    vrf_proof.s[0] = static_cast<byte>(0x31);
    vrf_proof.s[1] = static_cast<byte>(0x32);
    vrf_proof.s[2] = static_cast<byte>(0x33);

    qr = QueryResult(
        config, /* is_member */ true, key, payload, lookup_proof, vrf_proof, randomness);
}

void VerifyLoadedQueryResult(QueryResult &qr, const OZKSConfig &config)
{
    EXPECT_TRUE(qr.is_member());
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
        EXPECT_EQ(static_cast<byte>(0x10), qr.vrf_proof().gamma[0]);
        EXPECT_EQ(static_cast<byte>(0x11), qr.vrf_proof().gamma[1]);
        EXPECT_EQ(static_cast<byte>(0x12), qr.vrf_proof().gamma[2]);
        EXPECT_EQ(static_cast<byte>(0x21), qr.vrf_proof().c[0]);
        EXPECT_EQ(static_cast<byte>(0x22), qr.vrf_proof().c[1]);
        EXPECT_EQ(static_cast<byte>(0x23), qr.vrf_proof().c[2]);
        EXPECT_EQ(static_cast<byte>(0x31), qr.vrf_proof().s[0]);
        EXPECT_EQ(static_cast<byte>(0x32), qr.vrf_proof().s[1]);
        EXPECT_EQ(static_cast<byte>(0x33), qr.vrf_proof().s[2]);
    } else {
        EXPECT_EQ(static_cast<byte>(0x00), qr.vrf_proof().gamma[0]);
        EXPECT_EQ(static_cast<byte>(0x00), qr.vrf_proof().gamma[1]);
        EXPECT_EQ(static_cast<byte>(0x00), qr.vrf_proof().gamma[2]);
        EXPECT_EQ(static_cast<byte>(0x00), qr.vrf_proof().c[0]);
        EXPECT_EQ(static_cast<byte>(0x00), qr.vrf_proof().c[1]);
        EXPECT_EQ(static_cast<byte>(0x00), qr.vrf_proof().c[2]);
        EXPECT_EQ(static_cast<byte>(0x00), qr.vrf_proof().s[0]);
        EXPECT_EQ(static_cast<byte>(0x00), qr.vrf_proof().s[1]);
        EXPECT_EQ(static_cast<byte>(0x00), qr.vrf_proof().s[2]);
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
