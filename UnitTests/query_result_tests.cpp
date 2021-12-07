// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <iostream>

// OZKS
#include "oZKS/query_result.h"
#include "oZKS/utilities.h"
#include "oZKS/serialization_helpers.h"

// GTest
#include "gtest/gtest.h"

using namespace ozks;
using namespace std;


void SaveLoadTest(SerializationWriter &writer, SerializationReader &reader, OZKSConfig &config)
{
    QueryResult qr(config);

    EXPECT_FALSE(qr.is_member());

    lookup_path_type lookup_proof;
    payload_type payload;
    randomness_type randomness;
    VRFProof vrf_proof;

    lookup_proof.resize(3);
    lookup_proof[0].first = utils::bytes_to_bools(utils::make_bytes(0x01, 0x02, 0x03));
    lookup_proof[0].second[0] = static_cast<byte>(0xFE);
    lookup_proof[0].second[1] = static_cast<byte>(0xFD);
    lookup_proof[0].second[2] = static_cast<byte>(0xFC);
    lookup_proof[1].first = utils::bytes_to_bools(utils::make_bytes(0x04, 0x05, 0x06, 0x07));
    lookup_proof[1].second[0] = static_cast<byte>(0xDC);
    lookup_proof[1].second[1] = static_cast<byte>(0xDB);
    lookup_proof[1].second[2] = static_cast<byte>(0xDA);
    lookup_proof[2].first = utils::bytes_to_bools(utils::make_bytes(0xAA, 0xBB, 0xCC, 0xDD));
    lookup_proof[2].second[0] = static_cast<byte>(0xED);
    lookup_proof[2].second[1] = static_cast<byte>(0xEC);
    lookup_proof[2].second[2] = static_cast<byte>(0xEB);
    payload = utils::make_bytes(0x11, 0x22, 0x33, 0x44, 0x55);
    randomness = utils::make_bytes(0xCC, 0xDD, 0xEE, 0xFF);
    vrf_proof.gamma[0] = static_cast<byte>(0x10);
    vrf_proof.gamma[1] = static_cast<byte>(0x11);
    vrf_proof.gamma[2] = static_cast<byte>(0x12);
    vrf_proof.c[0] = static_cast<byte>(0x21);
    vrf_proof.c[1] = static_cast<byte>(0x22);
    vrf_proof.c[2] = static_cast<byte>(0x23);
    vrf_proof.s[0] = static_cast<byte>(0x31);
    vrf_proof.s[1] = static_cast<byte>(0x32);
    vrf_proof.s[2] = static_cast<byte>(0x33);

    qr = QueryResult(config, /* is_member */ true, payload, lookup_proof, vrf_proof, randomness);

    size_t save_size = qr.save(writer);

    QueryResult qr2(config);
    EXPECT_FALSE(qr2.is_member());

    size_t load_size = QueryResult::load(qr2, reader);

    EXPECT_EQ(load_size, save_size);

    EXPECT_TRUE(qr2.is_member());
    EXPECT_EQ(3, qr2.lookup_proof().size());
    EXPECT_EQ(false, qr2.lookup_proof()[0].first[0]);
    EXPECT_EQ(false, qr2.lookup_proof()[0].first[1]);
    EXPECT_EQ(false, qr2.lookup_proof()[0].first[2]);
    EXPECT_EQ(false, qr2.lookup_proof()[0].first[3]);
    EXPECT_EQ(false, qr2.lookup_proof()[0].first[4]);
    EXPECT_EQ(false, qr2.lookup_proof()[0].first[5]);
    EXPECT_EQ(false, qr2.lookup_proof()[0].first[6]);
    EXPECT_EQ(true, qr2.lookup_proof()[0].first[7]);

    EXPECT_EQ(static_cast<byte>(0xFE), qr2.lookup_proof()[0].second[0]);
    EXPECT_EQ(static_cast<byte>(0xFD), qr2.lookup_proof()[0].second[1]);
    EXPECT_EQ(static_cast<byte>(0xFC), qr2.lookup_proof()[0].second[2]);

    EXPECT_EQ(false, qr2.lookup_proof()[1].first[8]);
    EXPECT_EQ(false, qr2.lookup_proof()[1].first[9]);
    EXPECT_EQ(false, qr2.lookup_proof()[1].first[10]);
    EXPECT_EQ(false, qr2.lookup_proof()[1].first[11]);
    EXPECT_EQ(false, qr2.lookup_proof()[1].first[12]);
    EXPECT_EQ(true, qr2.lookup_proof()[1].first[13]);
    EXPECT_EQ(false, qr2.lookup_proof()[1].first[14]);
    EXPECT_EQ(true, qr2.lookup_proof()[1].first[15]);

    EXPECT_EQ(static_cast<byte>(0xDC), qr2.lookup_proof()[1].second[0]);
    EXPECT_EQ(static_cast<byte>(0xDB), qr2.lookup_proof()[1].second[1]);
    EXPECT_EQ(static_cast<byte>(0xDA), qr2.lookup_proof()[1].second[2]);

    EXPECT_EQ(true, qr2.lookup_proof()[2].first[8]);
    EXPECT_EQ(false, qr2.lookup_proof()[2].first[9]);
    EXPECT_EQ(true, qr2.lookup_proof()[2].first[10]);
    EXPECT_EQ(true, qr2.lookup_proof()[2].first[11]);
    EXPECT_EQ(true, qr2.lookup_proof()[2].first[12]);
    EXPECT_EQ(false, qr2.lookup_proof()[2].first[13]);
    EXPECT_EQ(true, qr2.lookup_proof()[2].first[14]);
    EXPECT_EQ(true, qr2.lookup_proof()[2].first[15]);

    EXPECT_EQ(static_cast<byte>(0xED), qr2.lookup_proof()[2].second[0]);
    EXPECT_EQ(static_cast<byte>(0xEC), qr2.lookup_proof()[2].second[1]);
    EXPECT_EQ(static_cast<byte>(0xEB), qr2.lookup_proof()[2].second[2]);

    if (config.include_vrf()) {
        EXPECT_EQ(static_cast<byte>(0x10), qr2.vrf_proof().gamma[0]);
        EXPECT_EQ(static_cast<byte>(0x11), qr2.vrf_proof().gamma[1]);
        EXPECT_EQ(static_cast<byte>(0x12), qr2.vrf_proof().gamma[2]);
        EXPECT_EQ(static_cast<byte>(0x21), qr2.vrf_proof().c[0]);
        EXPECT_EQ(static_cast<byte>(0x22), qr2.vrf_proof().c[1]);
        EXPECT_EQ(static_cast<byte>(0x23), qr2.vrf_proof().c[2]);
        EXPECT_EQ(static_cast<byte>(0x31), qr2.vrf_proof().s[0]);
        EXPECT_EQ(static_cast<byte>(0x32), qr2.vrf_proof().s[1]);
        EXPECT_EQ(static_cast<byte>(0x33), qr2.vrf_proof().s[2]);
    } else {
        EXPECT_EQ(static_cast<byte>(0x00), qr2.vrf_proof().gamma[0]);
        EXPECT_EQ(static_cast<byte>(0x00), qr2.vrf_proof().gamma[1]);
        EXPECT_EQ(static_cast<byte>(0x00), qr2.vrf_proof().gamma[2]);
        EXPECT_EQ(static_cast<byte>(0x00), qr2.vrf_proof().c[0]);
        EXPECT_EQ(static_cast<byte>(0x00), qr2.vrf_proof().c[1]);
        EXPECT_EQ(static_cast<byte>(0x00), qr2.vrf_proof().c[2]);
        EXPECT_EQ(static_cast<byte>(0x00), qr2.vrf_proof().s[0]);
        EXPECT_EQ(static_cast<byte>(0x00), qr2.vrf_proof().s[1]);
        EXPECT_EQ(static_cast<byte>(0x00), qr2.vrf_proof().s[2]);
    }
}

TEST(QueryResultTests, SaveLoadVectorTest)
{
    OZKSConfig config;
    EXPECT_TRUE(config.payload_randomness());
    EXPECT_TRUE(config.include_vrf());

    vector<byte> vec;
    VectorSerializationReader reader(&vec, 0);
    VectorSerializationWriter writer(&vec);

    SaveLoadTest(writer, reader, config);
}

TEST(QueryResultTests, SaveLoadStreamTest)
{
    OZKSConfig config;
    EXPECT_TRUE(config.payload_randomness());
    EXPECT_TRUE(config.include_vrf());

    stringstream ss;
    StreamSerializationReader reader(&ss);
    StreamSerializationWriter writer(&ss);

    SaveLoadTest(writer, reader, config);
}

TEST(QueryResultTests, SaveLoadVectorNoVRFTest)
{
    OZKSConfig config{ false, false };
    EXPECT_FALSE(config.payload_randomness());
    EXPECT_FALSE(config.include_vrf());

    vector<byte> vec;
    VectorSerializationReader reader(&vec, 0);
    VectorSerializationWriter writer(&vec);

    SaveLoadTest(writer, reader, config);
}

TEST(QueryResultTests, SaveLoadStreamNoVRFTest)
{
    OZKSConfig config{ false, false };
    EXPECT_FALSE(config.payload_randomness());
    EXPECT_FALSE(config.include_vrf());

    stringstream ss;
    StreamSerializationReader reader(&ss);
    StreamSerializationWriter writer(&ss);

    SaveLoadTest(writer, reader, config);
}
