// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <mutex>

// oZKS
#include "oZKS/ozks_distributed_generated.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/thread_pool.h"
#include "oZKS/version.h"
#include "ozks_distributed.h"
#include "providers/dist_query_provider.h"
#include "providers/dist_trie_info_provider.h"
#include "providers/dist_update_provider.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks_distributed;
using namespace ozks_distributed::providers;

OZKS::OZKS(const OZKSConfigDist &config) : config_(config), vrf_cache_(config.vrf_cache_size())
{
    initialize();
}

OZKS::OZKS()
    : OZKS(OZKSConfigDist(
          PayloadCommitmentType::CommitedPayload,
          LabelType::VRFLabels,
          TrieType::Stored,
          std::make_shared<storage::MemoryStorage>(),
          {},
          gsl::span<byte>(nullptr, nullptr),
          /* vrf_cache_size */ 1024,
          /* thread_count */ 0))
{}

void OZKS::initialize_vrf()
{
    // Set up a new VRF secret key (and public key)
    if (config_.vrf_seed().size() == 0) {
        // No VRF seed
        vrf_sk_.initialize();
    } else {
        vrf_sk_.initialize(config_.vrf_seed());
    }
}

void OZKS::initialize()
{
    // config_ is not initialized. It is only initialized by default values or by the
    // specific constructor that receives it as parameter.
    if (config_.label_type() == LabelType::VRFLabels) {
        initialize_vrf();
    }

    if (nullptr == storage()) {
        throw logic_error("Storage is null. It is mandatory.");
    }

    // Random ID
    utils::random_bytes(reinterpret_cast<byte *>(&ozks_id_), sizeof(trie_id_type));

    query_provider_ = make_shared<QueryProvider>(config_);
    trie_info_provider_ = make_shared<TrieInfoProvider>(config_);
    update_provider_ = make_shared<UpdateProvider>(id(), /* time_period */ 5000, config_);
}

shared_ptr<Storage> OZKS::storage() const
{
    return config_.storage();
}

shared_ptr<Storage> OZKS::storage_querier() const
{
    return config_.storage_querier();
}

void OZKS::insert(const key_type &key, const payload_type &payload)
{
    hash_type label = utils::get_node_label(key, vrf_sk_, config_.label_type());
    auto commit_result = utils::commit_payload(payload, config_.payload_commitment());

    store_value_type store_element;
    if (storage()->load_store_element(id(), key, store_element)) {
        throw runtime_error("Key is already contained");
    }

    store_element = { payload, commit_result.second };
    storage()->save_store_element(id(), key, store_element);

    update_provider_->insert(id(), label, commit_result.first);
}

void OZKS::insert(const key_payload_batch_type &input)
{
    label_hash_batch_type label_commit_batch(input.size());

    size_t thread_count = utils::get_insertion_thread_limit(nullptr, config_.thread_count());
    thread_count = std::min<size_t>(thread_count, pending_insertions_.size());
    size_t batch_size = (pending_insertions_.size() + thread_count - 1) / thread_count;
    ThreadPool tp(thread_count);

    // Protects the storage_ from concurrent access
    mutex storage_mutex;

    auto labels_and_hashes_lambda =
        [batch_size, &label_commit_batch, &storage_mutex, this](size_t i) {
            size_t begin_idx = i * batch_size;
            size_t end_idx = std::min((i + 1) * batch_size, pending_insertions_.size());
            for (size_t j = begin_idx; j < end_idx; j++) {
                const auto &key_payload = pending_insertions_[j];
                hash_type label =
                    utils::get_node_label(key_payload.first, vrf_sk_, config_.label_type());
                auto payload_commit =
                    utils::commit_payload(key_payload.second, config_.payload_commitment());
                label_commit_batch[j] = { label, payload_commit.first };

                // Get a lock guard to the mutex for writing to storage_
                lock_guard<mutex> lock(storage_mutex);

                store_value_type store_element;
                if (storage()->load_store_element(id(), key_payload.first, store_element)) {
                    throw runtime_error("Key is already contained");
                }

                store_element = store_value_type{ key_payload.second, payload_commit.second };
                storage()->save_store_element(id(), key_payload.first, store_element);
            }
        };

    // Perform VRF and commit payload computations
    vector<future<void>> labels_and_hashes_results(thread_count);
    for (size_t idx = 0; idx < thread_count; idx++) {
        labels_and_hashes_results[idx] = tp.enqueue(labels_and_hashes_lambda, idx);
    }

    // Wait until the VRF and commit payload computations are done
    for (auto &lh_result : labels_and_hashes_results) {
        lh_result.get();
    }

    update_provider_->insert(id(), label_commit_batch);
}

QueryResult OZKS::query(const key_type &key) const
{
    // get_node_label below will set this to nullopt if the OZKS is not using VRFs
    optional<VRFProof> vrf_proof;
    hash_type label =
        utils::get_node_label(key, vrf_sk_, vrf_cache_, config_.label_type(), vrf_proof);

    lookup_path_type lookup_path;
    if (!query_provider_->query(id(), label, lookup_path)) {
        // Non-existence
        return { config_,
                 /* is_member */ false,
                 key,
                 /* payload */ {},
                 lookup_path,
                 vrf_proof.value_or(VRFProof{}),
                 /* randomness */ {} };
    }

    // Existence
    store_value_type element;
    if (!storage()->load_store_element(id(), key, element)) {
        throw runtime_error("Store should contain the key we found in the trie");
    }

    return { config_,
             /* is_member */ true,
             key,
             element.payload,
             lookup_path,
             vrf_proof.value_or(VRFProof{}),
             element.randomness };
}

VRFPublicKey OZKS::get_vrf_public_key() const
{
    return vrf_sk_.get_vrf_public_key();
}

size_t OZKS::get_epoch() const
{
    return trie_info_provider_->get_epoch(id());
}

Commitment OZKS::get_commitment() const
{
    hash_type root_hash = trie_info_provider_->get_root_hash(id());

    return { get_vrf_public_key(), root_hash };
}

const OZKSConfig &OZKS::get_config() const
{
    return config_;
}

size_t OZKS::save(ostream &stream) const
{
    StreamSerializationWriter writer(&stream);
    return save(writer);
}

template <typename T>
size_t OZKS::save(vector<T> &vec) const
{
    VectorSerializationWriter writer(&vec);
    return save(writer);
}

size_t OZKS::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    vector<byte> config_saved;
    config_.save(config_saved);
    auto config_data = fbs_builder.CreateVector(
        reinterpret_cast<uint8_t *>(config_saved.data()), config_saved.size());

    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> sk_data = 0;

    if (config_.label_type() == LabelType::VRFLabels) {
        vector<byte> sk_saved(VRFSecretKey::save_size);
        vrf_sk_.save(gsl::span<byte, VRFSecretKey::save_size>(sk_saved));
        sk_data =
            fbs_builder.CreateVector(reinterpret_cast<uint8_t *>(sk_saved.data()), sk_saved.size());
    }

    fbs::OZKSBuilder ozks_builder(fbs_builder);
    ozks_builder.add_version(ozks_serialization_version);
    ozks_builder.add_vrf_sk(sk_data);
    ozks_builder.add_configuration(config_data);
    ozks_builder.add_trie_id(id());

    auto fbs_ozks = ozks_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_ozks);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    return fbs_builder.GetSize();
}

pair<OZKS, size_t> OZKS::Load(shared_ptr<Storage> storage, istream &stream)
{
    StreamSerializationReader reader(&stream);
    return Load(reader, storage);
}

template <typename T>
pair<OZKS, size_t> OZKS::Load(shared_ptr<Storage> storage, const vector<T> &vec, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return Load(reader, storage);
}

pair<OZKS, size_t> OZKS::Load(SerializationReader &reader, shared_ptr<Storage> storage)
{
    vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

    auto verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
    bool safe = fbs::VerifySizePrefixedOZKSBuffer(verifier);

    if (!safe) {
        throw runtime_error("Failed to load OZKS: invalid buffer");
    }

    auto fbs_ozks = fbs::GetSizePrefixedOZKS(in_data.data());
    if (!same_serialization_version(fbs_ozks->version())) {
        throw runtime_error("Failed to load OZKS: unsupported version");
    }

    OZKS ozks;

    vector<byte> config_vec(fbs_ozks->configuration()->size());
    utils::copy_bytes(
        fbs_ozks->configuration()->data(), fbs_ozks->configuration()->size(), config_vec.data());
    OZKSConfig config;
    OZKSConfig::Load(config, config_vec, storage);
    OZKSConfigDist config_dist(
        config.payload_commitment(),
        config.label_type(),
        config.trie_type(),
        storage,
        {},
        config.vrf_seed(),
        config.vrf_cache_size(),
        config.thread_count());
    ozks.config_ = config_dist;

    if (ozks.get_config().label_type() == LabelType::VRFLabels) {
        if (fbs_ozks->vrf_sk()->size() != VRFSecretKey::save_size) {
            throw runtime_error("Failed to load OZKS: VRF secret key size does not match");
        }

        ozks.vrf_sk_.load(gsl::span<const byte, VRFSecretKey::save_size>(
            reinterpret_cast<const byte *>(fbs_ozks->vrf_sk()->data()),
            fbs_ozks->vrf_sk()->size()));
    }

    ozks.ozks_id_ = fbs_ozks->trie_id();

    return { ozks, in_data.size() };
}

void OZKS::check_for_update()
{
    query_provider_->check_for_update(id());
}

// Explicit instantiations
template size_t OZKS::save(vector<uint8_t> &vec) const;
template size_t OZKS::save(vector<byte> &vec) const;
template pair<OZKS, size_t> OZKS::Load(
    shared_ptr<Storage> storage, const vector<uint8_t> &vec, size_t position);
template pair<OZKS, size_t> OZKS::Load(
    shared_ptr<Storage> storage, const vector<byte> &vec, size_t position);
