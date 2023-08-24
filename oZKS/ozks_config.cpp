// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/ozks_config.h"
#include "oZKS/storage/memory_storage.h"

using namespace std;
using namespace ozks;

OZKSConfig::OZKSConfig(
    PayloadCommitmentType commitment_type,
    LabelType label_type,
    TrieType trie_type,
    shared_ptr<storage::Storage> storage,
    const gsl::span<const byte> vrf_seed,
    size_t vrf_cache_size,
    size_t thread_count)
    : commitment_type_(commitment_type), label_type_(label_type), trie_type_(trie_type),
      storage_(storage), vrf_seed_(vrf_seed.size()),
      vrf_cache_size_(label_type == LabelType::VRFLabels ? vrf_cache_size : 0),
      thread_count_(thread_count)
{
    // Storage is mandatory
    if (storage_ == nullptr) {
        throw invalid_argument("storage is null");
    }

    // Should not specify VRF seed if we are not using VRF labels
    if (label_type_ != LabelType::VRFLabels && vrf_seed.size() > 0) {
        throw invalid_argument("Should not specify VRF seed if not using VRF labels");
    }

    if (vrf_seed.size() > 0) {
        utils::copy_bytes(vrf_seed.data(), vrf_seed.size(), vrf_seed_.data());
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

size_t OZKSConfig::save(SerializationWriter &writer) const
{
    uint8_t payload_commitment8 = static_cast<uint8_t>(commitment_type_);
    uint8_t label_type8 = static_cast<uint8_t>(label_type_);
    uint8_t trie_type8 = static_cast<uint8_t>(trie_type_);
    uint8_t use_storage8 = (storage_ != nullptr);
    uint32_t vrf_seed_size = static_cast<uint32_t>(vrf_seed_.size());
    uint64_t vrf_cache_size64 = vrf_cache_size_;
    uint64_t thread_count64 = thread_count_;

    writer.write(reinterpret_cast<const uint8_t *>(&payload_commitment8), sizeof(uint8_t));
    writer.write(reinterpret_cast<const uint8_t *>(&label_type8), sizeof(uint8_t));
    writer.write(reinterpret_cast<const uint8_t *>(&trie_type8), sizeof(uint8_t));
    writer.write(reinterpret_cast<const uint8_t *>(&use_storage8), sizeof(uint8_t));
    writer.write(reinterpret_cast<const uint8_t *>(&vrf_seed_size), sizeof(uint32_t));
    writer.write(reinterpret_cast<const uint8_t *>(vrf_seed_.data()), vrf_seed_size);
    writer.write(reinterpret_cast<const uint8_t *>(&vrf_cache_size64), sizeof(uint64_t));
    writer.write(reinterpret_cast<const uint8_t *>(&thread_count64), sizeof(uint64_t));

    return sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) +
           sizeof(uint64_t) + sizeof(uint64_t);
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
    uint8_t payload_commitment8 = 0;
    uint8_t label_type8 = 0;
    uint8_t trie_type8 = 0;
    uint8_t use_storage8 = 0;
    uint32_t vrf_seed_size = 0;
    uint64_t vrf_cache_size64 = 0;
    uint64_t thread_count64 = 0;

    reader.read(reinterpret_cast<uint8_t *>(&payload_commitment8), sizeof(uint8_t));
    reader.read(reinterpret_cast<uint8_t *>(&label_type8), sizeof(uint8_t));
    reader.read(reinterpret_cast<uint8_t *>(&trie_type8), sizeof(uint8_t));
    reader.read(reinterpret_cast<uint8_t *>(&use_storage8), sizeof(uint8_t));
    reader.read(reinterpret_cast<uint8_t *>(&vrf_seed_size), sizeof(uint32_t));
    vector<byte> vrf_seed(vrf_seed_size);
    reader.read(reinterpret_cast<uint8_t *>(vrf_seed.data()), vrf_seed_size);
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
        vrf_seed,
        static_cast<size_t>(vrf_cache_size64),
        static_cast<size_t>(thread_count64));
    swap(config, loaded_ozks_config);

    return sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) +
           sizeof(uint64_t) + sizeof(uint64_t);
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
