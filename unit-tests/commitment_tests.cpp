// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <sstream>

// VRF
#include <vrf/vrf.h>

// OZKS
#include "oZKS/commitment.h"
#include "oZKS/utilities.h"
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

TEST(CommitmentTests, SerializeTest)
{
    auto sk = vrf::VRF::Create(vrf::Type::EC_VRF_P256_SHA256_TAI);
    EXPECT_TRUE(sk->is_initialized());

    auto pk = sk->get_public_key();
    vector<byte> pk_vec = pk->to_bytes();

    commitment_type root_c = {};
    root_c[0] = static_cast<byte>(0x01);
    root_c[1] = static_cast<byte>(0x02);
    root_c[2] = static_cast<byte>(0x03);

    Commitment commitment{ pk, root_c };

    stringstream ss;
    size_t size = commitment.save(ss);

    auto load_result = Commitment::Load(ss);

    vector<byte> pk_vec2 = load_result.first.public_key()->to_bytes();

    EXPECT_EQ(load_result.second, size);
    EXPECT_EQ(load_result.first.root_commitment(), commitment.root_commitment());

    EXPECT_EQ(pk_vec.size(), pk_vec2.size());
    for (size_t idx = 0; idx < pk_vec.size(); idx++) {
        EXPECT_EQ(pk_vec[idx], pk_vec2[idx]);
    }
}

TEST(CommitmentTests, SerializeToVectorTest)
{
    auto sk = vrf::VRF::Create(vrf::Type::EC_VRF_P256_SHA256_TAI);
    EXPECT_TRUE(sk->is_initialized());

    auto pk = sk->get_public_key();
    vector<byte> pk_vec = pk->to_bytes();

    commitment_type root_c{};
    root_c[0] = static_cast<byte>(0x01);
    root_c[1] = static_cast<byte>(0x02);
    root_c[2] = static_cast<byte>(0x03);
    Commitment commitment{ pk, root_c };

    vector<byte> vec;
    size_t size = commitment.save(vec);

    auto load_result = Commitment::Load(vec);

    vector<byte> pk_vec2 = load_result.first.public_key()->to_bytes();

    EXPECT_EQ(load_result.second, size);
    EXPECT_EQ(load_result.first.root_commitment(), commitment.root_commitment());

    EXPECT_EQ(pk_vec.size(), pk_vec2.size());
    for (size_t idx = 0; idx < pk_vec.size(); idx++) {
        EXPECT_EQ(pk_vec[idx], pk_vec2[idx]);
    }
}
