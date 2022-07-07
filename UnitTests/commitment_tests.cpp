// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <sstream>

// OZKS
#include "gtest/gtest.h"
#include "oZKS/commitment.h"
#include "oZKS/vrf.h"
#include "oZKS/utilities.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;


TEST(CommitmentTests, SerializeTest)
{
    VRFSecretKey sk;
    sk.initialize();

    byte pk_arr[VRFPublicKey::save_size];
    sk.get_public_key().save(pk_arr);

    Commitment commitment{ sk.get_public_key(), make_bytes(0x01, 0x02, 0x03) };

    stringstream ss;
    size_t size = commitment.save(ss);

    auto load_result = Commitment::Load(ss);

    byte pk_arr2[VRFPublicKey::save_size];
    load_result.first.public_key().save(pk_arr2);

    EXPECT_EQ(load_result.second, size);
    EXPECT_EQ(load_result.first.root_commitment(), commitment.root_commitment());

    for (size_t idx = 0; idx < VRFPublicKey::save_size; idx++) {
        EXPECT_EQ(pk_arr[idx], pk_arr2[idx]);
    }
}

TEST(CommitmentTests, SerializeToVectorTest)
{
    VRFSecretKey sk;
    sk.initialize();

    byte pk_arr[VRFPublicKey::save_size];
    sk.get_public_key().save(pk_arr);

    Commitment commitment{ sk.get_public_key(), make_bytes(0x01, 0x02, 0x03) };

    vector<byte> vec;
    size_t size = commitment.save(vec);

    auto load_result = Commitment::Load(vec);

    byte pk_arr2[VRFPublicKey::save_size];
    load_result.first.public_key().save(pk_arr2);

    EXPECT_EQ(load_result.second, size);
    EXPECT_EQ(load_result.first.root_commitment(), commitment.root_commitment());

    for (size_t idx = 0; idx < VRFPublicKey::save_size; idx++) {
        EXPECT_EQ(pk_arr[idx], pk_arr2[idx]);
    }
}
