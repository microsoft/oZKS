// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/ozks_config.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/version.h"

using namespace std;
using namespace ozks;

OZKSConfig::OZKSConfig(
    PayloadCommitmentType commitment_type,
    LabelType label_type,
    TrieType trie_type,
    shared_ptr<storage::Storage> storage,
    const gsl::span<const byte> vrf_secret_key,
    size_t vrf_cache_size,
    size_t thread_count)
    : commitment_type_(commitment_type), label_type_(label_type), trie_type_(trie_type),
      storage_(storage), vrf_secret_key_(vrf_secret_key.size()),
      vrf_cache_size_(label_type == LabelType::VRFLabels ? vrf_cache_size : 0),
      thread_count_(thread_count)
{
    // Storage is mandatory
    if (storage_ == nullptr) {
        throw invalid_argument("storage is null");
    }

    // Should not specify VRF secret key if we are not using VRF labels
    if (label_type_ != LabelType::VRFLabels && vrf_secret_key.size() > 0) {
        throw invalid_argument("Should not specify VRF secret key if not using VRF labels");
    }

    if (vrf_secret_key.size() > 0) {
        utils::copy_bytes(vrf_secret_key.data(), vrf_secret_key.size(), vrf_secret_key_.data());
    }
}

OZKSConfig::OZKSConfig()
    : OZKSConfig(
          PayloadCommitmentType::CommitedPayload,
          LabelType::VRFLabels,
          TrieType::Stored,
          make_shared<storage::MemoryStorage>(),
          gsl::span<byte>(nullptr, nullptr),
          /* vrf_cache_size */ 0,
          /* thread_count */ 0)
{}

namespace {
    // Version, four one-byte fields, secret key length, VRF cache size and thread count.
    constexpr size_t config_fixed_size =
        sizeof(uint32_t) + 4 * sizeof(uint8_t) + sizeof(uint32_t) + 2 * sizeof(uint64_t);
} // namespace

size_t OZKSConfig::save(SerializationWriter &writer) const
{
    uint32_t version = ozks_serialization_version;
    uint8_t payload_commitment8 = static_cast<uint8_t>(commitment_type_);
    uint8_t label_type8 = static_cast<uint8_t>(label_type_);
    uint8_t trie_type8 = static_cast<uint8_t>(trie_type_);
    uint8_t use_storage8 = (storage_ != nullptr);
    uint32_t vrf_secret_key_size = static_cast<uint32_t>(vrf_secret_key_.size());
    uint64_t vrf_cache_size64 = vrf_cache_size_;
    uint64_t thread_count64 = thread_count_;

    writer.write(reinterpret_cast<const uint8_t *>(&version), sizeof(uint32_t));
    writer.write(reinterpret_cast<const uint8_t *>(&payload_commitment8), sizeof(uint8_t));
    writer.write(reinterpret_cast<const uint8_t *>(&label_type8), sizeof(uint8_t));
    writer.write(reinterpret_cast<const uint8_t *>(&trie_type8), sizeof(uint8_t));
    writer.write(reinterpret_cast<const uint8_t *>(&use_storage8), sizeof(uint8_t));
    writer.write(reinterpret_cast<const uint8_t *>(&vrf_secret_key_size), sizeof(uint32_t));
    writer.write(reinterpret_cast<const uint8_t *>(vrf_secret_key_.data()), vrf_secret_key_size);
    writer.write(reinterpret_cast<const uint8_t *>(&vrf_cache_size64), sizeof(uint64_t));
    writer.write(reinterpret_cast<const uint8_t *>(&thread_count64), sizeof(uint64_t));

    return config_fixed_size + vrf_secret_key_size;
}

template <typename T>
size_t OZKSConfig::save(vector<T> &vec) const
{
    VectorSerializationWriter writer(&vec);
    return save(writer);
}

size_t OZKSConfig::Load(
    OZKSConfig &config, SerializationReader &reader, shared_ptr<storage::Storage> storage)
{
    uint32_t version = 0;
    uint8_t payload_commitment8 = 0;
    uint8_t label_type8 = 0;
    uint8_t trie_type8 = 0;
    uint8_t use_storage8 = 0;
    uint32_t vrf_secret_key_size = 0;
    uint64_t vrf_cache_size64 = 0;
    uint64_t thread_count64 = 0;

    // The version leads the layout. A legacy blob begins with the commitment type, label
    // type, trie type and the use_storage flag (always 1); read as a uint32 that is at least
    // 2^24 on little-endian hosts and has low byte 1 on big-endian hosts, so it can never
    // equal a supported version (currently 2, only ever increasing).
    reader.read(reinterpret_cast<uint8_t *>(&version), sizeof(uint32_t));
    if (!same_serialization_version(version)) {
        throw runtime_error("Failed to load OZKSConfig: unsupported version");
    }

    reader.read(reinterpret_cast<uint8_t *>(&payload_commitment8), sizeof(uint8_t));
    reader.read(reinterpret_cast<uint8_t *>(&label_type8), sizeof(uint8_t));
    reader.read(reinterpret_cast<uint8_t *>(&trie_type8), sizeof(uint8_t));
    reader.read(reinterpret_cast<uint8_t *>(&use_storage8), sizeof(uint8_t));
    reader.read(reinterpret_cast<uint8_t *>(&vrf_secret_key_size), sizeof(uint32_t));
    vector<byte> vrf_secret_key(vrf_secret_key_size);
    reader.read(reinterpret_cast<uint8_t *>(vrf_secret_key.data()), vrf_secret_key_size);
    reader.read(reinterpret_cast<uint8_t *>(&vrf_cache_size64), sizeof(uint64_t));
    reader.read(reinterpret_cast<uint8_t *>(&thread_count64), sizeof(uint64_t));

    if (nullptr == storage && use_storage8 != 0) {
        throw runtime_error("Storage should have been specified");
    }

    OZKSConfig loaded_ozks_config(
        static_cast<PayloadCommitmentType>(payload_commitment8),
        static_cast<LabelType>(label_type8),
        static_cast<TrieType>(trie_type8),
        storage,
        vrf_secret_key,
        static_cast<size_t>(vrf_cache_size64),
        static_cast<size_t>(thread_count64));
    swap(config, loaded_ozks_config);

    return config_fixed_size + vrf_secret_key_size;
}

template <typename T>
size_t OZKSConfig::Load(
    OZKSConfig &config, const vector<T> &vec, shared_ptr<storage::Storage> storage, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return Load(config, reader, storage);
}

// Explicit instantiations
template size_t OZKSConfig::save(vector<uint8_t> &vec) const;
template size_t OZKSConfig::save(vector<byte> &vec) const;
template size_t OZKSConfig::Load(
    OZKSConfig &config,
    const vector<uint8_t> &vec,
    shared_ptr<storage::Storage> storage,
    size_t position);
template size_t OZKSConfig::Load(
    OZKSConfig &config,
    const vector<byte> &vec,
    shared_ptr<storage::Storage> storage,
    size_t position);
