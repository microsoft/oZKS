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
    : epoch_(0), storage_(storage)
{
    init_random_id();
    root_ = make_unique<CTNode>(storage, trie_id_);
}

void CompressedTrie::insert(
    const label_type &label, const payload_type &payload, append_proof_type &append_proof)
{
    partial_label_type lab = bytes_to_bools(label);
    append_proof.clear();

    epoch_++;
    root_->insert(lab, payload, epoch_);
    root_->update_hashes(lab);

    // To get the append proof we need to lookup the item we just inserted after hashes have been
    // updated
    if (!lookup(label, append_proof, /* include_searched */ true)) {
        throw runtime_error("Should have been able to find the item we just inserted");
    }
}

void CompressedTrie::insert(
    const label_payload_batch_type &label_payload_batch, append_proof_batch_type &append_proofs)
{
    vector<partial_label_type> labs;

    append_proofs.resize(label_payload_batch.size());
    epoch_++;

    for (size_t idx = 0; idx < label_payload_batch.size(); idx++) {
        const auto &label = label_payload_batch[idx].first;
        const auto &payload = label_payload_batch[idx].second;

        labs.emplace_back(bytes_to_bools(label));
        const auto &lab = labs[idx];

        root_->insert(lab, payload, epoch_);
    }

    for (size_t idx = 0; idx < label_payload_batch.size(); idx++) {
        const auto &label = label_payload_batch[idx].first;
        const auto &lab = labs[idx];

        root_->update_hashes(lab);
    }

    // To get the append proof we need to lookup the item we just inserted
    for (size_t idx = 0; idx < append_proofs.size(); idx++) {
        const label_type &label = label_payload_batch[idx].first;
        append_proof_type &append_proof = append_proofs[idx];

        if (!lookup(label, append_proof, /* include_searched */ true)) {
            throw runtime_error("Should have been able to find the item we just inserted");
        }
    }
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
    return root_->lookup(partial_label, path, include_searched);
}

string CompressedTrie::to_string(bool include_payload) const
{
    return root_->to_string(include_payload);
}

void CompressedTrie::get_commitment(commitment_type &commitment) const
{
    hash_type root_hash = root_->hash();
    if (root_hash.size() == 0) {
        throw runtime_error("No commitment has been computed");
    }

    commitment.resize(root_hash.size());
    utils::copy_bytes(root_hash.data(), root_hash.size(), commitment.data());
}

void CompressedTrie::clear()
{
    epoch_ = 0;
    root_ = make_unique<CTNode>();
    init_random_id();
}

size_t CompressedTrie::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    fbs::CompressedTrieBuilder ct_builder(fbs_builder);
    ct_builder.add_epoch(epoch());
    ct_builder.add_node_count(get_node_count());
    ct_builder.add_version(ozks_serialization_version);

    auto fbs_ct = ct_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_ct);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    // Save the tree as individual nodes
    size_t tree_size = save_tree(root_.get(), writer);

    return (fbs_builder.GetSize() + tree_size);
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

    ct.root_ = make_unique<CTNode>();

    auto fbs_ct = fbs::GetSizePrefixedCompressedTrie(in_data.data());
    ct.epoch_ = fbs_ct->epoch();
    size_t node_count = fbs_ct->node_count();

    if (node_count < 1) {
        throw runtime_error("Failed to load Compressed Trie: should have at least root node");
    }

    size_t tree_size = load_tree(*ct.root_, node_count, reader);

    if (node_count != 0) {
        throw runtime_error("Failed to load Compressed Trie: Stream contains extra nodes");
    }

    return (tree_size + in_data.size());
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

size_t CompressedTrie::get_node_count() const
{
    size_t node_count = 0;
    get_node_count(root_.get(), node_count);
    return node_count;
}

void CompressedTrie::get_node_count(const CTNode *node, size_t &node_count) const
{
    if (nullptr == node) {
        return;
    }

    node_count++;

    if (!node->left.empty()) {
        CTNode left_node;
        node->load(node->left, left_node);
        get_node_count(&left_node, node_count);
    }

    if (!node->right.empty()) {
        CTNode right_node;
        node->load(node->right, right_node);
        get_node_count(&right_node, node_count);
    }
}

size_t CompressedTrie::save_tree(const CTNode *node, SerializationWriter &writer) const
{
    size_t result = 0;

    if (nullptr == node) {
        return result;
    }

    result = node->save(writer);

    if (!node->left.empty()) {
        CTNode left_node;
        node->load(node->left, left_node);
        result += save_tree(&left_node, writer);
    }

    if (!node->right.empty()) {
        CTNode right_node;
        node->load(node->right, right_node);
        result += save_tree(&right_node, writer);
    }

    return result;
}

size_t CompressedTrie::load_tree(CTNode &node, size_t &node_count, SerializationReader &reader)
{
    if (node_count <= 0) {
        throw runtime_error("Failed to load Compressed Trie: Not enough nodes are available");
    }

    size_t size = 0;

    // Load the current node
    auto loaded_node = CTNode::load(reader);
    node_count--;

    node = get<0>(loaded_node);

    //partial_label_type left_label = get<1>(loaded_node);
    //partial_label_type right_label = get<2>(loaded_node);
    size += get<3>(loaded_node);

    if (!node.left.empty()) {
        CTNode left_node;
        size += load_tree(left_node, node_count, reader);
        if (left_node.label != node.left) {
            throw runtime_error("Failed to load Compressed Trie: Left label does not match");
        }
    }

    if (!node.right.empty()) {
        CTNode right_node;
        size += load_tree(right_node, node_count, reader);
        if (right_node.label != node.right) {
            throw runtime_error("Failed to load Compressed Trie: Right label does not match");
        }
    }

    return size;
}

void CompressedTrie::init_random_id()
{
    id_.resize(ID_SIZE);
    random_bytes(reinterpret_cast<unsigned char *>(id_.data()), static_cast<unsigned int>(id_.size()));
}

// Explicit instantiations
template size_t CompressedTrie::save(vector<uint8_t> &vec) const;
template size_t CompressedTrie::save(vector<byte> &vec) const;
template size_t CompressedTrie::load(
    CompressedTrie &ct, const vector<uint8_t> &vec, size_t position);
template size_t CompressedTrie::load(CompressedTrie &ct, const vector<byte> &vec, size_t position);
