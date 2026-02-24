// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// VRF
#include <vrf/vrf.h>

// OZKS
#include "oZKS/utilities.h"
#include "oZKS/vrf_cache.h"

// GTest
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;

void DoProofTest(vector<byte> &input)
{
    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    EXPECT_TRUE(sk->is_initialized());
    auto pk = sk->get_public_key();
    auto pk_clone = pk->clone();

    auto proof = sk->get_vrf_proof(input);
    auto vrf_value = proof->get_vrf_value();

    auto verif_result = pk->verify_vrf_proof(input, proof);
    auto verif_result_clone = pk_clone->verify_vrf_proof(input, proof);

    EXPECT_TRUE(verif_result.first);
    EXPECT_TRUE(verif_result_clone.first);
}
    
TEST(VRFTests, CreateProofTest)
{
    // Try different input lengths, including edge cases (1 and 32)
    for (size_t input_len = 1; input_len <= 32; input_len++) {
        vector<byte> input(input_len);
        for (size_t i = 0; i < input_len; i++) {
            input[i] = static_cast<byte>(i + 1); // Fill with distinct byte values
        }
        DoProofTest(input);
    }
}

TEST(VRFTests, VRFCacheTest)
{
    VRFCache cache(10);

    auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
    EXPECT_TRUE(sk->is_initialized());

    vector<hash_type> inputs(10);
    vector<std::unique_ptr<vrf::Proof>> proofs(10);

    // Insert 10 entries
    for (int i = 0; i < 10; i++) {
        vector<byte> input(32, static_cast<byte>(i));
        hash_type key_hash = utils::compute_key_hash(input);

        inputs[i] = key_hash;
        auto proof = sk->get_vrf_proof(key_hash);
        proofs[i] = std::move(proof);

        cache.add(key_hash, proofs[i].get());
    }

    auto pk = sk->get_public_key();

    // Retrieve and verify entries
    for (int i = 0; i < 10; i++) {
        vector<byte> input(32, static_cast<byte>(i));
        hash_type key_hash = utils::compute_key_hash(input);

        auto retrieved_proof = cache.get(key_hash);
        EXPECT_NE(retrieved_proof, nullptr);

        if (retrieved_proof) {

            auto verif_result = pk->verify_vrf_proof(key_hash, retrieved_proof);
            auto verif_original = pk->verify_vrf_proof(key_hash, proofs[i]);

            EXPECT_TRUE(verif_result.first);
            EXPECT_TRUE(verif_original.first);
        }
    }
}
