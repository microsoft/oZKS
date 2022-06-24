// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <cstring>
#include <stdexcept>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/compressed_trie_generated.h"
#include "oZKS/ct_node.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"
#include "oZKS/fourq/random.h"
#include "oZKS/storage/storage.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

size_t const ID_SIZE = 16;

CompressedTrie::CompressedTrie(shared_ptr<storage::Storage> storage)
    : epoch_(0), storage_(storage), root_({})
{
    init_random_id();

    CTNode root(this);
    root.save();
}

CompressedTrie::CompressedTrie() : epoch_(0), storage_(nullptr), root_({})
{}

void CompressedTrie::insert(
    const label_type &label, const payload_type &payload, append_proof_type &append_proof)
{
    partial_label_type lab = bytes_to_bools(label);
    append_proof.clear();

    epoch_++;
    
    CTNode root = load_root();

    root.insert(lab, payload, epoch_);
    root.update_hashes(lab);

    // To get the append proof we need to lookup the item we just inserted after hashes have been
    // updated
    if (!lookup(label, append_proof, /* include_searched */ true)) {
        throw runtime_error("Should have been able to find the item we just inserted");
    }

    save();
}

void CompressedTrie::insert(
    const label_payload_batch_type &label_payload_batch, append_proof_batch_type &append_proofs)
{
    vector<partial_label_type> labs;

    append_proofs.resize(label_payload_batch.size());
    epoch_++;

    CTNode root = load_root();

    for (size_t idx = 0; idx < label_payload_batch.size(); idx++) {
        const auto &label = label_payload_batch[idx].first;
        const auto &payload = label_payload_batch[idx].second;

        labs.emplace_back(bytes_to_bools(label));
        const auto &lab = labs[idx];

        root.insert(lab, payload, epoch_);
    }

    for (size_t idx = 0; idx < label_payload_batch.size(); idx++) {
        const auto &label = label_payload_batch[idx].first;
        const auto &lab = labs[idx];

        root.update_hashes(lab);
    }

    // To get the append proof we need to lookup the item we just inserted
    for (size_t idx = 0; idx < append_proofs.size(); idx++) {
        const label_type &label = label_payload_batch[idx].first;
        append_proof_type &append_proof = append_proofs[idx];

        if (!lookup(label, append_proof, /* include_searched */ true)) {
            throw runtime_error("Should have been able to find the item we just inserted");
        }
    }

    save();
}

bool CompressedTrie::lookup(const label_type &label, lookup_path_type &path) const
{
    return lookup(label, path, /* include_searched */ true);
}

bool CompressedTrie::lookup(
    const label_type &label,
    lookup_path_type &path,
    bool include_searched) const
{
    path.clear();
    partial_label_type partial_label = bytes_to_bools(label);
    CTNode root = load_root();
    return root.lookup(partial_label, path, include_searched);
}

string CompressedTrie::to_string(bool include_payload) const
{
    CTNode root = load_root();
    return root.to_string(include_payload);
}

void CompressedTrie::get_commitment(commitment_type &commitment) const
{
    CTNode root = load_root();
    hash_type root_hash = root.hash();
    if (root_hash.size() == 0) {
        throw runtime_error("No commitment has been computed");
    }

    commitment.resize(root_hash.size());
    utils::copy_bytes(root_hash.data(), root_hash.size(), commitment.data());
}

void CompressedTrie::clear()
{
    epoch_ = 0;
    root_ = {};
    init_random_id();

    CTNode root(this);
    // This will overwrite any existing root
    // TODO: Delete existing nodes in storage?
    root.save();
    save();
}

size_t CompressedTrie::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    auto label_bytes = utils::bools_to_bytes(root_);
    auto label_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(label_bytes.data()), label_bytes.size());
    auto partial_label =
        fbs::CreatePartialLabel(fbs_builder, label_data, static_cast<uint32_t>(root_.size()));

    auto id_data =
        fbs_builder.CreateVector(reinterpret_cast<const uint8_t *>(id_.data()), id_.size());

    fbs::CompressedTrieBuilder ct_builder(fbs_builder);
    ct_builder.add_epoch(epoch());
    ct_builder.add_version(ozks_serialization_version);
    ct_builder.add_root(partial_label);
    ct_builder.add_id(id_data);

    auto fbs_ct = ct_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_ct);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    return (fbs_builder.GetSize());
}

size_t CompressedTrie::save(ostream &stream) const
{
    StreamSerializationWriter writer(&stream);
    return save(writer);
}

template <class T>
size_t CompressedTrie::save(vector<T> &vec) const
{
    VectorSerializationWriter writer(&vec);
    return save(writer);
}

size_t CompressedTrie::load(CompressedTrie &ct, SerializationReader &reader)
{
    vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

    auto verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
    bool safe = fbs::VerifySizePrefixedCompressedTrieBuffer(verifier);

    if (!safe) {
        throw runtime_error("Failed to load Compressed Trie: invalid buffer");
    }

    auto fbs_ct = fbs::GetSizePrefixedCompressedTrie(in_data.data());
    ct.epoch_ = fbs_ct->epoch();

    ct.root_ = utils::bytes_to_bools(
        reinterpret_cast<const byte *>(fbs_ct->root()->data()->data()),
        fbs_ct->root()->size());

    ct.id_.resize(fbs_ct->id()->size());
    utils::copy_bytes(
        fbs_ct->id()->data(), fbs_ct->id()->size(), ct.id_.data());

    return (in_data.size());
}

size_t CompressedTrie::load(CompressedTrie &ct, istream &stream)
{
    StreamSerializationReader reader(&stream);
    return load(ct, reader);
}

template <class T>
size_t CompressedTrie::load(CompressedTrie &ct, const vector<T> &vec, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return load(ct, reader);
}

void CompressedTrie::save() const
{
    if (nullptr == storage_)
        throw runtime_error("Storage is null");

    storage_->save_compressed_trie(*this);
}

bool CompressedTrie::load(
    const vector<byte> &trie_id, shared_ptr<storage::Storage> storage, CompressedTrie &trie)
{
    if (nullptr == storage)
        throw invalid_argument("storage is null");
    if (trie_id.empty())
        throw invalid_argument("trie_id is empty");

    bool loaded = storage->load_compressed_trie(trie_id, trie);
    if (loaded) {
        trie.init(storage);
    }

    return loaded;
}

CTNode CompressedTrie::load_root() const
{
    CTNode loader(this);
    CTNode root;
    if (!loader.load({}, root))
        throw runtime_error("Could not load root node");

    return root;
}

void CompressedTrie::init(shared_ptr<storage::Storage> storage)
{
    storage_ = storage;
}

void CompressedTrie::init_random_id()
{
    id_.resize(ID_SIZE);
    random_bytes(
        reinterpret_cast<unsigned char *>(id_.data()), static_cast<unsigned int>(id_.size()));
}

// Explicit instantiations
template size_t CompressedTrie::save(vector<uint8_t> &vec) const;
template size_t CompressedTrie::save(vector<byte> &vec) const;
template size_t CompressedTrie::load(
    CompressedTrie &ct, const vector<uint8_t> &vec, size_t position);
template size_t CompressedTrie::load(CompressedTrie &ct, const vector<byte> &vec, size_t position);
