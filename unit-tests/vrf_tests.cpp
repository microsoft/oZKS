// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "oZKS/utilities.h"
#include "oZKS/vrf.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

TEST(VRF, SecretKeySaveLoad)
{
    VRFSecretKey sk1, sk2;
    sk1.initialize();
    sk2.initialize();

    // Save the two keys and check that they are different
    array<byte, VRFSecretKey::save_size> sk1_buf{}, sk2_buf{};
    sk1.save(sk1_buf);
    sk2.save(sk2_buf);
    EXPECT_FALSE(equal(sk1_buf.begin(), sk1_buf.end(), sk2_buf.begin()));

    // Now load sk1_buf to another VRFSecretKey, save, and compare to sk1_buf
    VRFSecretKey sk3;
    array<byte, VRFSecretKey::save_size> sk3_buf{};
    sk3.load(sk1_buf);
    sk3.save(sk3_buf);
    EXPECT_TRUE(equal(sk1_buf.begin(), sk1_buf.end(), sk3_buf.begin()));
}

TEST(VRF, PublicKeyCreate)
{
    VRFSecretKey sk;

    // Secret key is uninitialized
    VRFPublicKey pk;
    EXPECT_THROW(pk = sk.get_vrf_public_key(), logic_error);

    VRFSecretKey sk1, sk2;
    sk1.initialize();
    sk2.initialize();
    VRFPublicKey pk1 = sk1.get_vrf_public_key(), pk2 = sk2.get_vrf_public_key();

    // The public keys should be different
    array<byte, VRFPublicKey::save_size> pk1_buf{}, pk2_buf{};
    pk1.save(pk1_buf);
    pk2.save(pk2_buf);
    EXPECT_FALSE(equal(pk1_buf.begin(), pk1_buf.end(), pk2_buf.begin()));

    // Now set sk3 equal sk1 and should get same public key as pk1
    VRFSecretKey sk3 = sk1;
    VRFPublicKey pk3 = sk3.get_vrf_public_key();
    array<byte, VRFPublicKey::save_size> pk3_buf{};
    pk3.save(pk3_buf);
    EXPECT_TRUE(equal(pk1_buf.begin(), pk1_buf.end(), pk3_buf.begin()));
}

TEST(VRF, PublicKeySaveLoad)
{
    VRFSecretKey sk;
    sk.initialize();
    VRFPublicKey pk = sk.get_vrf_public_key();

    array<byte, VRFPublicKey::save_size> pk1_buf{}, pk2_buf{};
    pk.save(pk1_buf);

    VRFPublicKey pk2;
    pk2.load(pk1_buf);
    pk2.save(pk2_buf);
    EXPECT_TRUE(equal(pk1_buf.begin(), pk1_buf.end(), pk2_buf.begin()));
}

TEST(VRF, CreateVerifyProof)
{
    VRFSecretKey sk;
    key_type data{};

    // Secret key is uninitialized
    VRFProof pf;
    EXPECT_THROW(pf = sk.get_vrf_proof(data), logic_error);

    // Initialize and create two proofs for the same data; they should be different
    sk.initialize();
    VRFPublicKey pk = sk.get_vrf_public_key();

    VRFProof pf1 = sk.get_vrf_proof(data);
    VRFProof pf2 = sk.get_vrf_proof(data);
    EXPECT_EQ(pf1.gamma, pf2.gamma);
    EXPECT_EQ(pf1.c, pf2.c);
    EXPECT_EQ(pf1.s, pf2.s);

    EXPECT_TRUE(pk.verify_vrf_proof(data, pf1));
    EXPECT_TRUE(pk.verify_vrf_proof(data, pf2));

    // Set non-empty data
    data = utils::make_bytes<key_type>(0x1, 0x2, 0x3, 0x4);
    pf1 = sk.get_vrf_proof(data);
    pf2 = sk.get_vrf_proof(data);
    EXPECT_EQ(pf1.gamma, pf2.gamma);
    EXPECT_EQ(pf1.c, pf2.c);
    EXPECT_EQ(pf1.s, pf2.s);

    EXPECT_TRUE(pk.verify_vrf_proof(data, pf1));
    EXPECT_TRUE(pk.verify_vrf_proof(data, pf2));

    // Modify data by flipping a single bit; now verification should fail
    data[0] ^= byte(1);
    EXPECT_FALSE(pk.verify_vrf_proof(data, pf1));
}

TEST(VRF, GetHash)
{
    VRFSecretKey sk;
    sk.initialize();

    vector<byte> data1 = utils::make_bytes<vector<byte>>(0x00);
    hash_type hash1 = sk.get_vrf_value(data1);

    vector<byte> data2 = utils::make_bytes<vector<byte>>(0x01);
    hash_type hash2 = sk.get_vrf_value(data2);

    ASSERT_FALSE(equal(hash1.begin(), hash1.end(), hash2.begin()));
}

TEST(VRF, SeededKey)
{
    VRFSecretKey sk1;
    VRFSecretKey sk2;
    VRFSecretKey sk3;

    auto vrf_seed = utils::make_bytes<vector<byte>>(1, 2, 3, 4, 5);
    sk1.initialize(vrf_seed);
    sk2.initialize(vrf_seed);
    sk3.initialize();

    vector<byte> data1 = utils::make_bytes<vector<byte>>(0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF);
    vector<byte> data2 =
        utils::make_bytes<vector<byte>>(0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11);
    hash_type hash1 = sk1.get_vrf_value(data1);
    hash_type hash2 = sk2.get_vrf_value(data1);
    hash_type hash3 = sk3.get_vrf_value(data1);

    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash2, hash3);

    hash1 = sk1.get_vrf_value(data2);
    hash2 = sk2.get_vrf_value(data2);
    hash3 = sk3.get_vrf_value(data2);

    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash2, hash3);
}
