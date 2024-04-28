// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <mutex>
#include <thread>

// oZKS
#include "oZKS/ozks_generated.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/thread_pool.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"
#include "ozks.h"
#include "providers/local_query_provider.h"
#include "providers/local_trie_info_provider.h"
#include "providers/local_update_provider.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks_simple;
using namespace ozks_simple::providers;

OZKS::OZKS(const OZKSConfig &config) : config_(config), vrf_cache_(config.vrf_cache_size())
{
    initialize();
}

OZKS::OZKS()
    : OZKS(OZKSConfig(
          PayloadCommitmentType::CommitedPayload,
          LabelType::VRFLabels,
          TrieType::Stored,
          std::make_shared<storage::MemoryStorage>(),
          gsl::span<byte>(nullptr, nullptr),
          /* vrf_cache_size */ 1024,
          /* thread_count */ 0))
{}

shared_ptr<InsertResult> OZKS::insert(const key_type &key, const payload_type &payload)
{
    pending_insertions_.emplace_back(pending_insertion{ key, payload });
    auto insert_result = make_shared<InsertResult>();
    pending_results_.push_back(insert_result);
    return insert_result;
}

InsertResultBatch OZKS::insert(const key_payload_batch_type &input)
{
    InsertResultBatch result;
    for (const auto &i : input) {
        pending_insertions_.emplace_back(pending_insertion{ i.first, i.second });
        auto insert_result = make_shared<InsertResult>();
        pending_results_.push_back(insert_result);
        result.emplace_back(std::move(insert_result));
    }

    return result;
}

void OZKS::do_pending_insertions()
{
    if (pending_insertions_.size() != pending_results_.size()) {
        throw runtime_error("Pending insertions and results should match");
    }
    if (pending_insertions_.empty()) {
        return;
    }

    InsertResultBatch result;
    label_hash_batch_type label_commit_batch(pending_insertions_.size());

    // We parallelize the VRF and commit payload computations
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

    append_proof_batch_type append_proofs;

    update_provider_->insert(id(), label_commit_batch, append_proofs);
    commitment_type commitment = trie_info_provider_->get_root_hash(id());

    for (size_t idx = 0; idx < append_proofs.size(); idx++) {
        auto &pr = pending_results_[idx];
        if (!pr) {
            throw runtime_error("Pending result is null");
        }
        pr->init_result(commitment, std::move(append_proofs[idx]));
    }

    pending_insertions_.clear();
    pending_results_.clear();
    storage()->flush(id());
}

QueryResult OZKS::query(const key_type &key) const
{
    // get_node_label below will set this to nullopt if the OZKS is not using VRFs
    optional<VRFProof> vrf_proof;
    hash_type label =
        utils::get_node_label(key, vrf_sk_, vrf_cache_, config_.label_type(), vrf_proof);

    append_proof_type lookup_path;
    if (!query_provider_->query(id(), label, lookup_path)) {
        // Non-existence: return path to partial label that matches key and its two children
        return { config_,
                 /* is_member */ false,
                 key,
                 /* payload */ {},
                 lookup_path,
                 vrf_proof.value_or(VRFProof{}),
                 /* randomness */ {} };
    }

    // Existence: Returns payload, proof (proof = path type) and VRFProof that label was computed
    // correctly and commitment randomness
    store_value_type store_element;
    if (!storage()->load_store_element(id(), key, store_element))
        throw runtime_error("Store should contain the key we found in the trie");

    return { config_,
             /* is_member */ true,
             key,
             store_element.payload,
             lookup_path,
             vrf_proof.value_or(VRFProof{}),
             store_element.randomness };
}

void OZKS::flush()
{
    do_pending_insertions();
}

void OZKS::check_for_update()
{
    size_t new_epoch = storage()->get_compressed_trie_epoch(id());
    size_t epoch = get_epoch();

    if (new_epoch > epoch) {
        for (; epoch <= new_epoch; epoch++) {
            storage()->load_updated_elements(epoch, id(), storage());
        }

        //    // Need to update our local copy
        //    CompressedTrie trie;
        //    if (!storage()->load_compressed_trie(id(), trie)) {
        //        throw runtime_error("Should have found trie");
        //    }
        //    *trie_ = trie;
    }
}

VRFPublicKey OZKS::get_vrf_public_key() const
{
    if (config_.label_type() != LabelType::VRFLabels) {
        throw logic_error("VRF public key is not set");
    }
    return vrf_sk_.get_vrf_public_key();
}

size_t OZKS::get_epoch() const
{
    return trie_info_provider_->get_epoch(id());
}

Commitment OZKS::get_commitment() const
{
    VRFPublicKey vrf_public_key;
    if (config_.label_type() == LabelType::VRFLabels) {
        vrf_public_key = get_vrf_public_key();
    }
    return { vrf_public_key, trie_info_provider_->get_root_hash(id()) };
}

const OZKSConfig &OZKS::get_config() const
{
    return config_;
}

void OZKS::clear()
{
    // Delete ozks and trie contents from storage (if supported)
    storage()->delete_ozks(id());

    // Clear the cache and hit/miss counters
    vrf_cache_.clear();

    // Save the VRF secret key and ID so we can replace them
    VRFSecretKey old_vrf_sk = vrf_sk_;
    trie_id_type ozks_id = ozks_id_;

    // Initialize and replace the old VRF secret key
    initialize();
    vrf_sk_ = old_vrf_sk;
    ozks_id_ = ozks_id;
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
    OZKSConfig::Load(ozks.config_, config_vec, storage);

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

    query_provider_ = make_shared<LocalQueryProvider>(config_);
    trie_info_provider_ = make_shared<LocalTrieInfoProvider>(config_);
    update_provider_ = make_shared<LocalUpdateProvider>(config_);

    pending_insertions_.clear();
    pending_results_.clear();
}

shared_ptr<storage::Storage> OZKS::storage() const
{
    return config_.storage();
}

// Explicit instantiations
template size_t OZKS::save(vector<uint8_t> &vec) const;
template size_t OZKS::save(vector<byte> &vec) const;
template pair<OZKS, size_t> OZKS::Load(
    shared_ptr<Storage> storage, const vector<uint8_t> &vec, size_t position);
template pair<OZKS, size_t> OZKS::Load(
    shared_ptr<Storage> storage, const vector<byte> &vec, size_t position);
