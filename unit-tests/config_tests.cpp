// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// VRF
#include <vrf/vrf.h>

// OZKS
#include "oZKS/ozks_config.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"

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
    EXPECT_EQ(0, config.vrf_secret_key().size());
    EXPECT_NE(nullptr, config.storage().get());
    EXPECT_EQ(0, config.vrf_cache_size());
    EXPECT_EQ(0, config.thread_count());
}

TEST(ConfigTests, VRFSecretKeyTest)
{
    // Empty VRF secret key
    {
        auto storage = make_shared<MemoryStorage>();
        vector<byte> empty_vrf_secret_key;
        OZKSConfig config(
            PayloadCommitmentType::UncommitedPayload,
            LabelType::VRFLabels,
            TrieType::Stored,
            storage,
            empty_vrf_secret_key);

        EXPECT_EQ(PayloadCommitmentType::UncommitedPayload, config.payload_commitment());
        EXPECT_EQ(LabelType::VRFLabels, config.label_type());
        EXPECT_EQ(TrieType::Stored, config.trie_type());
        EXPECT_NE(nullptr, config.storage().get());
        EXPECT_EQ(0, config.vrf_secret_key().size());
        EXPECT_EQ(nullptr, config.vrf_secret_key().data());
    }

    // Non-empty VRF secret key (generated and serialized)
    {
        auto storage = make_shared<MemoryStorage>();
        auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
        auto sk_bytes = sk->to_secure_bytes();
        vector<byte> vrf_secret_key(
            reinterpret_cast<const byte *>(sk_bytes.get()),
            reinterpret_cast<const byte *>(sk_bytes.get()) + sk_bytes.size());
        OZKSConfig config(
            PayloadCommitmentType::UncommitedPayload,
            LabelType::VRFLabels,
            TrieType::Stored,
            storage,
            vrf_secret_key);

        EXPECT_NE(nullptr, config.storage().get());
        EXPECT_EQ(vrf_secret_key.size(), config.vrf_secret_key().size());
    }

    // Non-empty VRF secret key with VRF labels disabled
    {
        auto storage = make_shared<MemoryStorage>();
        auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
        auto sk_bytes = sk->to_secure_bytes();
        vector<byte> vrf_secret_key(
            reinterpret_cast<const byte *>(sk_bytes.get()),
            reinterpret_cast<const byte *>(sk_bytes.get()) + sk_bytes.size());
        EXPECT_THROW(
            OZKSConfig config(
                PayloadCommitmentType::UncommitedPayload,
                LabelType::HashedLabels,
                TrieType::Stored,
                storage,
                vrf_secret_key),
            invalid_argument);
    }
}

TEST(ConfigTests, CarriesSerializationVersion)
{
    auto storage = make_shared<MemoryStorage>();
    OZKSConfig config(
        PayloadCommitmentType::UncommitedPayload, LabelType::HashedLabels, TrieType::Linked, storage);
    vector<byte> buffer;
    size_t save_size = config.save(buffer);

    // The version leads the layout, in the writer's native byte order like every other field.
    ASSERT_GE(buffer.size(), sizeof(uint32_t));
    uint32_t version = 0;
    memcpy(&version, buffer.data(), sizeof(version));
    EXPECT_EQ(ozks_serialization_version, version);
    OZKSConfig loaded;
    EXPECT_EQ(save_size, OZKSConfig::Load(loaded, buffer, storage));
    EXPECT_EQ(TrieType::Linked, loaded.trie_type());

    // Rejections must come from the version check itself, not from a later read failure.
    auto expect_unsupported_version = [&](const vector<byte> &blob) {
        try {
            OZKSConfig::Load(loaded, blob, storage);
            FAIL() << "Load should have rejected the version";
        } catch (const runtime_error &ex) {
            EXPECT_STREQ("Failed to load OZKSConfig: unsupported version", ex.what());
        }
    };

    // A foreign version is rejected before any other field is interpreted.
    for (uint32_t foreign_version : { 0U, 1U, ozks_serialization_version + 1 }) {
        SCOPED_TRACE(foreign_version);
        vector<byte> foreign = buffer;
        memcpy(foreign.data(), &foreign_version, sizeof(foreign_version));
        expect_unsupported_version(foreign);
    }

    // A legacy blob (no version prefix) begins with the commitment type, label type, trie
    // type and the use_storage flag, which is always 1, so it can never read as a supported
    // version.
    vector<byte> legacy(buffer.begin() + sizeof(uint32_t), buffer.end());
    expect_unsupported_version(legacy);

    // Fewer than four bytes fail in the reader before any version is interpreted.
    EXPECT_THROW(OZKSConfig::Load(loaded, vector<byte>(3), storage), runtime_error);
}

TEST(ConfigTests, SerializationSizeTest)
{
    // With empty VRF secret key
    {
        auto storage = make_shared<MemoryStorage>();
        OZKSConfig config(
            PayloadCommitmentType::CommitedPayload,
            LabelType::HashedLabels,
            TrieType::Stored,
            storage);

        vector<byte> buffer;
        size_t save_size = config.save(buffer);

        EXPECT_EQ(buffer.size(), save_size);

        OZKSConfig loaded;
        size_t load_size = OZKSConfig::Load(loaded, buffer, storage);

        EXPECT_EQ(save_size, load_size);
    }

    // With non-empty VRF secret key
    {
        auto storage = make_shared<MemoryStorage>();
        auto sk = vrf::VRF::Create(vrf::Type::ec_vrf_p256_sha256_tai);
        auto sk_bytes = sk->to_secure_bytes();
        vector<byte> vrf_secret_key(
            reinterpret_cast<const byte *>(sk_bytes.get()),
            reinterpret_cast<const byte *>(sk_bytes.get()) + sk_bytes.size());
        OZKSConfig config(
            PayloadCommitmentType::UncommitedPayload,
            LabelType::VRFLabels,
            TrieType::Stored,
            storage,
            vrf_secret_key,
            /* vrf_cache_size */ 100,
            /* thread_count */ 4);

        vector<byte> buffer;
        size_t save_size = config.save(buffer);

        EXPECT_EQ(buffer.size(), save_size);

        OZKSConfig loaded;
        size_t load_size = OZKSConfig::Load(loaded, buffer, storage);

        EXPECT_EQ(save_size, load_size);

        // Verify round-trip preserves values
        EXPECT_EQ(config.payload_commitment(), loaded.payload_commitment());
        EXPECT_EQ(config.label_type(), loaded.label_type());
        EXPECT_EQ(config.trie_type(), loaded.trie_type());
        EXPECT_EQ(config.vrf_cache_size(), loaded.vrf_cache_size());
        EXPECT_EQ(config.thread_count(), loaded.thread_count());
        EXPECT_EQ(config.vrf_secret_key().size(), loaded.vrf_secret_key().size());
    }
}
