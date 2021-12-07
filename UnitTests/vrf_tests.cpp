// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "oZKS/vrf.h"
#include "oZKS/utilities.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "gtest/gtest.h"

using namespace std;
using namespace ozks;

TEST(VRF, SecretKeySaveLoad)
{
    VRFSecretKey sk1, sk2;
    sk1.initialize();
    sk2.initialize();

    // Save the two keys and check that they are different
    array<byte, VRFSecretKey::save_size> sk1_buf, sk2_buf;
    sk1.save(sk1_buf);
    sk2.save(sk2_buf);
    EXPECT_FALSE(equal(sk1_buf.begin(), sk1_buf.end(), sk2_buf.begin()));

    // Now load sk1_buf to another VRFSecretKey, save, and compare to sk1_buf
    VRFSecretKey sk3;
    array<byte, VRFSecretKey::save_size> sk3_buf;
    sk3.load(sk1_buf);
    sk3.save(sk3_buf);
    EXPECT_TRUE(equal(sk1_buf.begin(), sk1_buf.end(), sk3_buf.begin()));
}

TEST(VRF, PublicKeyCreate)
{
    VRFSecretKey sk;

    // Secret key is uninitialized
    EXPECT_THROW(VRFPublicKey pk = sk.get_public_key(), logic_error);

    VRFSecretKey sk1, sk2;
    sk1.initialize();
    sk2.initialize();
    VRFPublicKey pk1 = sk1.get_public_key(), pk2 = sk2.get_public_key();

    // The public keys should be different
    array<byte, VRFPublicKey::save_size> pk1_buf, pk2_buf;
    pk1.save(pk1_buf);
    pk2.save(pk2_buf);
    EXPECT_FALSE(equal(pk1_buf.begin(), pk1_buf.end(), pk2_buf.begin()));

    // Now set sk3 equal sk1 and should get same public key as pk1
    VRFSecretKey sk3 = sk1;
    VRFPublicKey pk3 = sk3.get_public_key();
    array<byte, VRFPublicKey::save_size> pk3_buf;
    pk3.save(pk3_buf);
    EXPECT_TRUE(equal(pk1_buf.begin(), pk1_buf.end(), pk3_buf.begin()));
}

TEST(VRF, PublicKeySaveLoad)
{
    VRFSecretKey sk;
    sk.initialize();
    VRFPublicKey pk = sk.get_public_key();

    array<byte, VRFPublicKey::save_size> pk1_buf, pk2_buf;
    pk.save(pk1_buf);

    VRFPublicKey pk2;
    pk2.load(pk1_buf);
    pk2.save(pk2_buf);
    EXPECT_TRUE(equal(pk1_buf.begin(), pk1_buf.end(), pk2_buf.begin()));
}

TEST(VRF, CreateVerifyProof)
{
    VRFSecretKey sk;
    vector<byte> data{};

    // Secret key is uninitialized
    EXPECT_THROW(VRFProof pf = sk.get_proof(data), logic_error);

    // Initialize and create two proofs for the same data; they should be different
    sk.initialize();
    VRFPublicKey pk = sk.get_public_key();
    hash_type hash1;
    hash_type hash2;

    VRFProof pf1 = sk.get_proof(data);
    VRFProof pf2 = sk.get_proof(data);
    EXPECT_EQ(pf1.gamma, pf2.gamma);
    EXPECT_NE(pf1.c, pf2.c);
    EXPECT_NE(pf1.s, pf2.s);

    EXPECT_TRUE(pk.verify_proof(data, pf1, hash1));
    EXPECT_TRUE(pk.verify_proof(data, pf2, hash2));
    EXPECT_EQ(hash1, hash2);

    // Set non-empty data
    data = utils::make_bytes(0x1, 0x2, 0x3, 0x4);
    pf1 = sk.get_proof(data);
    pf2 = sk.get_proof(data);
    EXPECT_EQ(pf1.gamma, pf2.gamma);
    EXPECT_NE(pf1.c, pf2.c);
    EXPECT_NE(pf1.s, pf2.s);

    EXPECT_TRUE(pk.verify_proof(data, pf1, hash1));
    EXPECT_TRUE(pk.verify_proof(data, pf2, hash2));
    EXPECT_EQ(hash1, hash2);

    // Modify data by flipping a single bit; now verification should fail
    data[0] ^= byte(1);
    EXPECT_FALSE(pk.verify_proof(data, pf1, hash1));
}

TEST(VRF, GetHashAndVerify)
{
    VRFSecretKey sk;
    sk.initialize();

    vector<byte> data = utils::make_bytes(0x01, 0x02, 0x03, 0x04);
    hash_type hash = sk.get_hash(data);

    VRFProof proof = sk.get_proof(data);
    VRFPublicKey pk = sk.get_public_key();

    hash_type hash2;
    pk.verify_proof(data, proof, hash2);

    EXPECT_EQ(hash, hash2);
}
