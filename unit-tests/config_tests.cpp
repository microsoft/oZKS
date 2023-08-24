// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/ozks_config.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"

// GTest
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;


TEST(ConfigTests, DefaultConstructorTest)
{
    OZKSConfig config;

    EXPECT_EQ(PayloadCommitmentType::CommitedPayload, config.payload_commitment());
    EXPECT_EQ(LabelType::VRFLabels, config.label_type());
    EXPECT_EQ(TrieType::Stored, config.trie_type());
    EXPECT_EQ(0, config.vrf_seed().size());
    EXPECT_NE(nullptr, config.storage().get());
    EXPECT_EQ(0, config.vrf_cache_size());
    EXPECT_EQ(0, config.thread_count());
}

TEST(ConfigTests, VRFSeedTest)
{
    // Empty VRF seed
    {
        auto storage = make_shared<MemoryStorage>();
        vector<byte> empty_vrf_seed;
        OZKSConfig config(
            PayloadCommitmentType::UncommitedPayload,
            LabelType::VRFLabels,
            TrieType::Stored,
            storage,
            empty_vrf_seed);

        EXPECT_EQ(PayloadCommitmentType::UncommitedPayload, config.payload_commitment());
        EXPECT_EQ(LabelType::VRFLabels, config.label_type());
        EXPECT_EQ(TrieType::Stored, config.trie_type());
        EXPECT_NE(nullptr, config.storage().get());
        EXPECT_EQ(0, config.vrf_seed().size());
        EXPECT_EQ(nullptr, config.vrf_seed().data());
    }

    // Non-empty VRF seed
    {
        auto storage = make_shared<MemoryStorage>();
        auto vrf_seed = utils::make_bytes<vector<byte>>(0x01, 0x02, 0x03, 0x04, 0x05);
        OZKSConfig config(
            PayloadCommitmentType::UncommitedPayload,
            LabelType::VRFLabels,
            TrieType::Stored,
            storage,
            vrf_seed);

        EXPECT_NE(nullptr, config.storage().get());
        EXPECT_EQ(5, config.vrf_seed().size());
    }

    // Non-empty VRF seed with VRF labels disabled
    {
		auto storage = make_shared<MemoryStorage>();
		auto vrf_seed = utils::make_bytes<vector<byte>>(0x01, 0x02, 0x03, 0x04, 0x05);
        EXPECT_THROW(
            OZKSConfig config(
				PayloadCommitmentType::UncommitedPayload,
				LabelType::HashedLabels,
				TrieType::Stored,
				storage,
				vrf_seed),
			invalid_argument);
	}
}
