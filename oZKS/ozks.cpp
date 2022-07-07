// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// oZKS
#include "oZKS/ozks.h"
#include "oZKS/compressed_trie.h"
#include "oZKS/ozks_generated.h"
#include "oZKS/ozks_store_generated.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"
#include "oZKS/storage/memory_storage.h"

using namespace std;
using namespace ozks;

OZKS::OZKS() : OZKS(nullptr)
{
}

OZKS::OZKS(shared_ptr<storage::Storage> storage) : storage_(storage)
{
    initialize_vrf();

    if (nullptr == storage) {
        storage_ = make_shared<storage::MemoryStorage>();
    }

    CompressedTrie trie(storage_);
    trie_id_ = trie.id();
    trie.save();
}

OZKS::OZKS(shared_ptr<storage::Storage> storage, const OZKSConfig &config) : storage_(storage)
{
    config_ = config;

    if (config_.include_vrf()) {
        initialize_vrf();
    }

    if (nullptr == storage) {
        storage_ = make_shared<storage::MemoryStorage>();
    }

    CompressedTrie trie(storage_);
    trie_id_ = trie.id();
    trie.save();
}

shared_ptr<InsertResult> OZKS::insert(const key_type& key, const payload_type& payload)
{
    pending_insertions_.emplace_back(pending_insertion{ key, payload });
    auto insert_result = make_shared<InsertResult>();
    pending_results_.emplace_back(pending_result{ key, insert_result });
    return insert_result;
}

InsertResultBatch OZKS::insert(const key_payload_batch_type &input)
{
    InsertResultBatch result;
    for (const auto &i : input) {
        pending_insertions_.emplace_back(pending_insertion{ i.first, i.second });
        auto insert_result = make_shared<InsertResult>();
        pending_results_.emplace_back(pending_result{ i.first, insert_result });
        result.emplace_back(move(insert_result));
    }

    return result;
}

void OZKS::do_pending_insertions()
{
    if (pending_insertions_.size() != pending_results_.size()) {
        throw runtime_error("Pending insertions and results should match");
    }

    InsertResultBatch result;
    label_payload_batch_type label_payload_batch;

    // Compute the labels and payloads that will be inserted
    for (auto &key_payload : pending_insertions_) {
        hash_type vrf_value = get_key_hash(key_payload.first);
        auto payload_commit = commit(key_payload.second);

        store_value_type store_element;
        if (storage_->load_store_element(trie_id_, key_payload.first, store_element)) {
            throw runtime_error("Key is already contained");
        }
        store_element = store_value_type{ key_payload.second, payload_commit.second };
        storage_->save_store_element(trie_id_, key_payload.first, store_element);

        label_type label(vrf_value.begin(), vrf_value.end());

        label_payload_batch.push_back({ label, payload_commit.first });
    }

    append_proof_batch_type append_proofs;
    
    CompressedTrie trie;
    load_trie(trie);
    trie.insert(label_payload_batch, append_proofs);

    commitment_type commitment;
    trie.get_commitment(commitment);

    for (size_t idx = 0; idx < append_proofs.size(); idx++) {
        auto &pending_result = pending_results_[idx];
        pending_result.second->init_result(commitment, append_proofs[idx]);
    }

    pending_insertions_.clear();
    pending_results_.clear();
    storage_->flush(id());
}

QueryResult OZKS::query(const key_type &key) const
{
    hash_type hash = get_key_hash(key);
    append_proof_type lookup_path;
    label_type label(hash.begin(), hash.end());

    VRFProof vrf_proof = {};
    if (config_.include_vrf()) {
        vrf_proof = vrf_sk_.get_proof(key);
    }

    CompressedTrie trie;
    load_trie(trie);

    if (!trie.lookup(label, lookup_path)) {
        // Non-existence: return path to partial label that matches key and its two children
        payload_type payload;       // Empty payload
        randomness_type randomness; // Empty randomness

        QueryResult result(
            config_, /*is_member */ false, payload, lookup_path, vrf_proof, randomness);
        return result;
    }

    // Existence: Returns payload, proof (proof = path type) and VRFProof that label was computed
    // correctly and commitment randomness
    store_value_type store_element;
    if (!storage_->load_store_element(trie_id_, key, store_element))
        throw runtime_error("Store should contain the key we found");

    QueryResult result(
        config_,
        /* is_member */ true,
        store_element.payload,
        lookup_path,
        vrf_proof,
        store_element.randomness);

    return result;
}

void OZKS::flush()
{
    do_pending_insertions();
}

pair<payload_type, randomness_type> OZKS::commit(const payload_type &payload)
{
    randomness_type randomness;
    hash_type hash;

    if (config_.payload_randomness()) {
        utils::compute_randomness_hash(payload, hash, randomness);
    } else {
        hash = utils::compute_hash(payload, "nonrandom_hash");
    }

    // Returns hash and the randomness used to compute it
    vector<byte> pld(hash.begin(), hash.end());
    return { pld, randomness };
}

VRFPublicKey OZKS::get_public_key() const
{
    return vrf_pk_;
}

size_t OZKS::get_epoch() const
{
    CompressedTrie trie;
    load_trie(trie);
    return trie.epoch();
}

Commitment OZKS::get_commitment() const
{
    commitment_type commitment;
    CompressedTrie trie;
    load_trie(trie);
    trie.get_commitment(commitment);
    return { get_public_key(), commitment };
}

const OZKSConfig &OZKS::get_configuration() const
{
    return config_;
}

void OZKS::clear()
{
    CompressedTrie trie;
    load_trie(trie);
    trie.clear();

    pending_insertions_.clear();
    pending_results_.clear();
    vrf_pk_ = {};
    vrf_sk_ = {};
    config_ = {};

    // TODO: delete contents of previous Trie?
    // TODO: delete contents of previous Store?
}

size_t OZKS::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    vector<byte> config_saved;
    config_.save(config_saved);
    auto config_data = fbs_builder.CreateVector(
        reinterpret_cast<uint8_t *>(config_saved.data()), config_saved.size());
    auto trie_id_data =
        fbs_builder.CreateVector(reinterpret_cast<const uint8_t *>(trie_id_.data()), trie_id_.size());

    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> pk_data;
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> sk_data;

    if (config_.include_vrf()) {
        vector<byte> pk_saved(VRFPublicKey::save_size);
        vector<byte> sk_saved(VRFSecretKey::save_size);

        vrf_pk_.save(gsl::span<byte, VRFPublicKey::save_size>(pk_saved));
        vrf_sk_.save(gsl::span<byte, VRFSecretKey::save_size>(sk_saved));

        pk_data =
            fbs_builder.CreateVector(reinterpret_cast<uint8_t *>(pk_saved.data()), pk_saved.size());
        sk_data =
            fbs_builder.CreateVector(reinterpret_cast<uint8_t *>(sk_saved.data()), sk_saved.size());
    }

    fbs::OZKSBuilder ozks_builder(fbs_builder);
    ozks_builder.add_version(ozks_serialization_version);
    ozks_builder.add_vrf_pk(pk_data);
    ozks_builder.add_vrf_sk(sk_data);
    ozks_builder.add_configuration(config_data);
    ozks_builder.add_trie_id(trie_id_data);

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

template <class T>
size_t OZKS::save(vector<T> &vec) const
{
    VectorSerializationWriter writer(&vec);
    return save(writer);
}

size_t OZKS::load(SerializationReader &reader, OZKS &ozks)
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

    ozks.clear();

    vector<byte> config_vec(fbs_ozks->configuration()->size());
    utils::copy_bytes(
        fbs_ozks->configuration()->data(), fbs_ozks->configuration()->size(), config_vec.data());
    OZKSConfig::load(ozks.config_, config_vec);

    ozks.trie_id_.resize(fbs_ozks->trie_id()->size());
    utils::copy_bytes(
        fbs_ozks->trie_id()->data(), fbs_ozks->trie_id()->size(), ozks.trie_id_.data());

    if (ozks.get_configuration().include_vrf()) {
        if (fbs_ozks->vrf_pk()->size() != VRFPublicKey::save_size) {
            throw runtime_error("Failed to load OZKS: VRF public key size does not match");
        }

        if (fbs_ozks->vrf_sk()->size() != VRFSecretKey::save_size) {
            throw runtime_error("Failed to load OZKS: VRF secret key size does not match");
        }

        ozks.vrf_pk_.load(gsl::span<const byte, VRFPublicKey::save_size>(
            reinterpret_cast<const byte *>(fbs_ozks->vrf_pk()->data()),
            fbs_ozks->vrf_pk()->size()));
        ozks.vrf_sk_.load(gsl::span<const byte, VRFSecretKey::save_size>(
            reinterpret_cast<const byte *>(fbs_ozks->vrf_sk()->data()),
            fbs_ozks->vrf_sk()->size()));
    }

    return in_data.size();
}

size_t OZKS::load(OZKS &ozks, istream &stream)
{
    StreamSerializationReader reader(&stream);
    return load(reader, ozks);
}

template <class T>
size_t OZKS::load(OZKS &ozks, const vector<T> &vec, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return load(reader, ozks);
}


void OZKS::load_trie(CompressedTrie &trie) const
{
    if (nullptr == storage_)
        throw runtime_error("storage_ is null");
    if (trie_id_.empty())
        throw runtime_error("trie_id_ is empty");

    if (!CompressedTrie::load(trie_id_, storage_, trie))
        throw runtime_error("Could not load trie");
}

void OZKS::save() const
{
    if (nullptr == storage_)
        throw runtime_error("storage_ is null");
    if (trie_id_.empty())
        throw runtime_error("trie_id_ is empty");

    storage_->save_ozks(*this);
}

bool OZKS::load(
    const vector<byte>& trie_id,
    shared_ptr<ozks::storage::Storage> storage,
    OZKS& ozks)
{
    if (nullptr == storage)
        throw invalid_argument("storage is null");
    if (trie_id.empty())
        throw invalid_argument("trie_id is empty");

    return storage->load_ozks(trie_id, ozks);
}

void OZKS::initialize_vrf()
{
    // Set up a new VRF secret key and public key
    vrf_sk_.initialize();
    vrf_pk_ = vrf_sk_.get_public_key();
}

hash_type OZKS::get_key_hash(const key_type &key) const
{
    if (config_.include_vrf()) {
        return vrf_sk_.get_hash(key);
    } else {
        return utils::compute_hash(key, "key_hash");
    }
}

// Explicit instantiations
template size_t OZKS::save(vector<uint8_t> &vec) const;
template size_t OZKS::save(vector<byte> &vec) const;
template size_t OZKS::load(OZKS &ozks, const vector<uint8_t> &vec, size_t position);
template size_t OZKS::load(OZKS &ozks, const vector<byte> &vec, size_t position);
