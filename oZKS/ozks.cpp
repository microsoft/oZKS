// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// oZKS
#include "oZKS/ozks.h"
#include "oZKS/ozks_generated.h"
#include "oZKS/ozks_store_generated.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"

using namespace std;
using namespace ozks;

OZKS::OZKS()
{
    initialize_vrf();
}

OZKS::OZKS(const OZKSConfig &config)
{
    config_ = config;

    if (config_.include_vrf()) {
        initialize_vrf();
    }
}

InsertResult OZKS::insert(const key_type &key, const payload_type &payload)
{
    // The label is computed by evaluating the VRF at the payload
    hash_type vrf_value = get_key_hash(key);

    label_type label(vrf_value.begin(), vrf_value.end());
    append_proof_type proof;

    auto payload_commit = commit(payload);

    // Keep the original data if necessary
    if (store_.find(key) != store_.end()) {
        throw runtime_error("Key is already contained");
    }
    store_[key] = { payload, payload_commit.second };

    trie_.insert(label, payload_commit.first, proof);

    commitment_type commitment;
    trie_.get_commitment(commitment);

    // Return commitment and append proof so caller can publish it somewhere
    return { commitment, proof };
}

InsertResultBatch OZKS::insert(const key_payload_batch_type &input)
{
    InsertResultBatch result;
    label_payload_batch_type label_payload_batch;

    // Compute the labels and payloads that will be inserted
    for (auto &key_payload : input) {
        hash_type vrf_value = get_key_hash(key_payload.first);
        auto payload_commit = commit(key_payload.second);

        if (store_.find(key_payload.first) != store_.end()) {
            throw runtime_error("Key is already contained");
        }
        store_[key_payload.first] = { key_payload.second, payload_commit.second };

        label_type label(vrf_value.begin(), vrf_value.end());

        label_payload_batch.push_back({ label, payload_commit.first });
    }

    append_proof_batch_type append_proofs;
    trie_.insert(label_payload_batch, append_proofs);

    commitment_type commitment;
    trie_.get_commitment(commitment);

    for (size_t idx = 0; idx < append_proofs.size(); idx++) {
        result.push_back({ commitment, append_proofs[idx] });
    }

    return result;
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

    if (!trie_.lookup(label, lookup_path)) {
        // Non-existence: return path to partial label that matches key and its two children
        payload_type payload;       // Empty payload
        randomness_type randomness; // Empty randomness

        QueryResult result(
            config_, /*is_member */ false, payload, lookup_path, vrf_proof, randomness);
        return result;
    }

    // Existence: Returns payload, proof (proof = path type) and VRFProof that label was computed
    // correctly and commitment randomness
    auto iter = store_.find(key);
    if (iter == store_.end())
        throw runtime_error("Should contain the key we found");

    QueryResult result(
        config_,
        /* is_member */ true,
        iter->second.payload,
        lookup_path,
        vrf_proof,
        iter->second.randomness);

    return result;
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

Commitment OZKS::get_commitment() const
{
    commitment_type commitment;
    trie_.get_commitment(commitment);
    return { get_public_key(), commitment };
}

const OZKSConfig &OZKS::get_configuration() const
{
    return config_;
}

void OZKS::clear()
{
    trie_.clear();
    store_.clear();
    vrf_pk_ = {};
    vrf_sk_ = {};
    config_ = {};
}

size_t OZKS::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    vector<byte> config_saved;
    config_.save(config_saved);
    auto config_data = fbs_builder.CreateVector(
        reinterpret_cast<uint8_t *>(config_saved.data()), config_saved.size());

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
    ozks_builder.add_store_count(store_.size());

    auto fbs_ozks = ozks_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_ozks);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    // The actual store needs to be saved separately, as it may be quite large
    size_t store_size = 0;
    for (auto store_elem : store_) {
        store_size += save_store_element(store_elem, writer);
    }

    // The tree needs to be saved separately because of its non-standard way of saving
    size_t tree_size = trie_.save(writer);

    return tree_size + store_size + fbs_builder.GetSize();
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

    // The store needs to be loaded separately
    size_t store_size = 0;
    size_t store_count = fbs_ozks->store_count();
    ozks.store_.clear();
    for (size_t i = 0; i < store_count; i++) {
        store_size += ozks.load_store_element(reader);
    }

    // The tree needs to be loaded separately
    size_t tree_size = CompressedTrie::load(ozks.trie_, reader);

    return in_data.size() + store_size + tree_size;
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

size_t OZKS::save_store_element(
    const pair<const key_type, store_type> &store_element, SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    auto key_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(store_element.first.data()), store_element.first.size());
    auto payload_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(store_element.second.payload.data()),
        store_element.second.payload.size());
    auto randomness_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(store_element.second.randomness.data()),
        store_element.second.randomness.size());

    auto fbs_store_value = fbs::CreateStoreValue(fbs_builder, payload_data, randomness_data);

    fbs::StoreElementBuilder store_element_builder(fbs_builder);
    store_element_builder.add_key(key_data);
    store_element_builder.add_value(fbs_store_value);

    auto fbs_store_element = store_element_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_store_element);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    return fbs_builder.GetSize();
}

size_t OZKS::load_store_element(SerializationReader &reader)
{
    vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

    auto verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
    bool safe = fbs::VerifySizePrefixedStoreElementBuffer(verifier);

    if (!safe) {
        throw runtime_error("Failed to load Store element: invalid buffer");
    }

    auto fbs_store_element = fbs::GetSizePrefixedStoreElement(in_data.data());

    key_type key;
    transform(
        fbs_store_element->key()->cbegin(),
        fbs_store_element->key()->cend(),
        back_inserter(key),
        [](const uint8_t ui) { return static_cast<byte>(ui); });

    store_type value;
    transform(
        fbs_store_element->value()->payload()->cbegin(),
        fbs_store_element->value()->payload()->cend(),
        back_inserter(value.payload),
        [](const uint8_t ui) { return static_cast<byte>(ui); });
    transform(
        fbs_store_element->value()->randomness()->cbegin(),
        fbs_store_element->value()->randomness()->cend(),
        back_inserter(value.randomness),
        [](const uint8_t ui) { return static_cast<byte>(ui); });

    store_[key] = value;

    return in_data.size();
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
