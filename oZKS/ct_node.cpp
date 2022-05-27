// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <cstring>
#include <sstream>

// OZKS
#include "oZKS/ct_node.h"
#include "oZKS/ct_node_generated.h"
#include "oZKS/utilities.h"
#include "oZKS/storage/storage.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

CTNode::CTNode()
{
    memset(hash_.data(), 0, hash_.size());
}

CTNode::CTNode(shared_ptr<storage::Storage> storage, const vector<byte> &trie_id)
    : storage_(storage)
{
    memset(hash_.data(), 0, hash_.size());
    trie_id_ = trie_id;
}

CTNode::CTNode(const CTNode &node)
{
    *this = node;
}

string CTNode::to_string(bool include_payload) const
{
    stringstream ss;
    string left_str = left.empty() ? "(empty)" : utils::to_string(left);
    string right_str = right.empty() ? "(empty)" : utils::to_string(right);

    ss << "n:" << utils::to_string(label);
    ss << ":l:" << left_str << ":r:" << right_str;

    if (include_payload && payload.size() > 0) {
        ss << ":p:" << utils::to_string(payload);
    }

    ss << ";";

    if (!left.empty()) {
        CTNode left_node;
        storage_->LoadCTNode(trie_id_, left, left_node);
        string sub = left_node.to_string(include_payload);
        ss << sub;
    }
    if (!right.empty()) {
        CTNode right_node;
        storage_->LoadCTNode(trie_id_, right, right_node);
        string sub = right_node.to_string(include_payload);
        ss << sub;
    }

    return ss.str();
}

CTNode &CTNode::operator=(const CTNode &node)
{
    label = node.label;
    payload = node.payload;
    hash_ = node.hash_;
    is_dirty_ = node.is_dirty_;
    left.clear();
    right.clear();
    storage_ = node.storage_;
    trie_id_ = node.trie_id_;

    return *this;
}

void CTNode::init(
    const partial_label_type &init_label, const payload_type &init_payload, const size_t epoch)
{
    if (!is_leaf())
        throw runtime_error("Should only be used for leaf nodes");

    hash_type new_hash;
    compute_leaf_hash(init_label, init_payload, epoch, new_hash);
    init(init_label, init_payload, new_hash);
}

void CTNode::init(
    const partial_label_type &init_label,
    const payload_type &init_payload,
    const hash_type &init_hash)
{
    label = init_label;
    payload = init_payload;
    hash_ = init_hash;
    is_dirty_ = false;
}

void CTNode::init(const partial_label_type &init_label)
{
    if (is_leaf())
        throw runtime_error("Should not be used for leaf nodes");

    label = init_label;
    payload = {};
    is_dirty_ = true;
}

bool CTNode::update_hash()
{
    if (!is_dirty_) {
        // No need to actually update the hash
        return true;
    }

    if (is_leaf())
        throw runtime_error("Should not be used for leaf nodes");

    // Compute hash with updated children values
    partial_label_type left_label;
    partial_label_type right_label;
    hash_type left_hash{};
    hash_type right_hash{};

    if (!left.empty()) {
        CTNode left_node;
        storage_->LoadCTNode(trie_id_, left, left_node);
        if (left_node.is_dirty_) {
            return false;
        }

        left_hash = left_node.hash();
        left_label = left_node.label;
    }

    if (!right.empty()) {
        CTNode right_node;
        storage_->LoadCTNode(trie_id_, right, right_node);
        if (right_node.is_dirty_) {
            return false;
        }

        right_hash = right_node.hash();
        right_label = right_node.label;
    }

    compute_node_hash(left_label, left_hash, right_label, right_hash, hash_);
    is_dirty_ = false;

    return true;
}

partial_label_type CTNode::insert(
    const partial_label_type &insert_label, const payload_type &insert_payload, const size_t epoch)
{
    if (insert_label == label) {
        throw runtime_error("Attempting to insert the same label");
    }

    vector<bool> common = get_common_prefix(insert_label, label);

    if (common.size() >= insert_label.size()) {
        throw runtime_error("Common part should be smaller");
    }

    bool next_bit = insert_label[common.size()];

    if (is_leaf() && !is_empty()) {
        // Convert current leaf to non-leaf
        CTNode left_node(storage_, trie_id_);
        CTNode right_node(storage_, trie_id_);

        if (next_bit == 0) {
            left_node.init(insert_label, insert_payload, epoch);
            right_node.init(label, payload, hash_);
            left = insert_label;
            right = label;
        } else {
            left_node.init(label, payload, hash_);
            right_node.init(insert_label, insert_payload, epoch);
            left = label;
            right = insert_label;
        }

        init(common);
        storage_->SaveCTNode(trie_id_, *this);
        storage_->SaveCTNode(trie_id_, left_node);
        storage_->SaveCTNode(trie_id_, right_node);

        return common;
    }

    // If there is a route to follow, follow it
    if (next_bit == 1 && !right.empty() && right[common.size()] == 1) {
        CTNode right_node;
        storage_->LoadCTNode(trie_id_, right, right_node);
        partial_label_type new_right = right_node.insert(insert_label, insert_payload, epoch);
        if (new_right != right) {
            right = new_right;
        }
        is_dirty_ = true;
        storage_->SaveCTNode(trie_id_, *this);
        return label;
    }

    if (next_bit == 0 && !left.empty() && left[common.size()] == 0) {
        CTNode left_node;
        storage_->LoadCTNode(trie_id_, left, left_node);
        partial_label_type new_left = left_node.insert(insert_label, insert_payload, epoch);
        if (new_left != left) {
            left = new_left;
        }
        is_dirty_ = true;
        storage_->SaveCTNode(trie_id_, *this);
        return label;
    }

    CTNode new_node(storage_, trie_id_);
    new_node.init(common);

    // Node where new node will be inserted
    partial_label_type *insert_node = nullptr;
    // Node that will get the value of this node
    partial_label_type *transfer_node = nullptr;

    // If there is no route to follow, insert here
    if (next_bit == 1) {
        if (right.empty()) {
            CTNode right_node(storage_, trie_id_);
            right_node.init(insert_label, insert_payload, epoch);
            right = insert_label;
            is_dirty_ = true;

            storage_->SaveCTNode(trie_id_, *this);
            storage_->SaveCTNode(trie_id_, right_node);
            return label;
        }

        insert_node = &new_node.right;
        transfer_node = &new_node.left;
    } else {
        if (left.empty()) {
            CTNode left_node(storage_, trie_id_);
            left_node.init(insert_label, insert_payload, epoch);
            left = insert_label;
            is_dirty_ = true;

            storage_->SaveCTNode(trie_id_, *this);
            storage_->SaveCTNode(trie_id_, left_node);
            return label;
        }

        insert_node = &new_node.left;
        transfer_node = &new_node.right;
    }

    *transfer_node = label;
    *insert_node = insert_label;

    CTNode new_inserted_node(storage_, trie_id_);
    new_inserted_node.init(insert_label, insert_payload, epoch);
    is_dirty_ = true;

    storage_->SaveCTNode(trie_id_, new_node);
    storage_->SaveCTNode(trie_id_, new_inserted_node);
    storage_->SaveCTNode(trie_id_, *this);

    return common;
}

bool CTNode::lookup(
    const partial_label_type &lookup_label,
    lookup_path_type &path,
    bool include_searched)
{
    return lookup(lookup_label, path, include_searched, /* update_hashes */ false);
}

void CTNode::update_hashes(const partial_label_type& label)
{
    lookup_path_type path;
    if (!lookup(label, path, /* include_searched */ false, /* update_hashes */ true)) {
        throw runtime_error("Should have found the path of the lable to update hashes");
    }
}

bool CTNode::lookup(
    const partial_label_type &lookup_label,
    lookup_path_type &path,
    bool include_searched,
    bool update_hashes)
{
    if (label == lookup_label) {
        if (include_searched && !update_hashes) {
            // This node is the result
            path.push_back({ label, hash() });
        }

        if (update_hashes) {
            update_hash();
        }

        return true;
    }

    if (is_leaf()) {
        return false;
    }

    vector<bool> common = get_common_prefix(lookup_label, label);

    if (common.size() >= lookup_label.size()) {
        throw runtime_error("Common part should be less than either labels");
    }

    bool next_bit = lookup_label[common.size()];

    // If there is a route to follow, follow it
    bool found = false;
    CTNode sibling;
    CTNode left_node;
    CTNode right_node;

    if (next_bit == 1 && !right.empty() && right[common.size()] == 1) {
        storage_->LoadCTNode(trie_id_, right, right_node);
        found = right_node.lookup(lookup_label, path, include_searched, update_hashes);
        storage_->LoadCTNode(trie_id_, left, sibling);
    } else if (next_bit == 0 && !left.empty() && left[common.size()] == 0) {
        storage_->LoadCTNode(trie_id_, left, left_node);
        found = left_node.lookup(lookup_label, path, include_searched, update_hashes);
        storage_->LoadCTNode(trie_id_, right, sibling);
    }

    if (!found && path.empty()) {
        if (!update_hashes) {
            // Need to include non-existence proof in result.
            if (!left.empty()) {
                if (left_node.is_empty())
                    storage_->LoadCTNode(trie_id_, left, left_node);

                path.push_back({ left_node.label, left_node.hash() });
            }

            if (!right.empty()) {
                if (right_node.is_empty())
                    storage_->LoadCTNode(trie_id_, right, right_node);

                path.push_back({ right_node.label, right_node.hash() });
            }

            if (!is_empty()) {
                path.push_back({ label, hash() });
            }
        }
    } else {
        if (!sibling.is_empty()) {
            if (update_hashes) {
                sibling.update_hash();
                storage_->SaveCTNode(trie_id_, sibling);
            } else {
                // Add sibling to the path
                path.push_back({ sibling.label, sibling.hash() });
            }
        }

        if (update_hashes) {
            update_hash();
            storage_->SaveCTNode(trie_id_, *this);
        }
    }

    return found;
}

size_t CTNode::save(SerializationWriter &writer) const
{
    //if (is_dirty_) {
    //    throw runtime_error("Attempted to save node with out of date hash");
    //}

    flatbuffers::FlatBufferBuilder fbs_builder;

    auto label_bytes = utils::bools_to_bytes(label);
    auto label_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(label_bytes.data()), label_bytes.size());

    auto partial_label =
        fbs::CreatePartialLabel(fbs_builder, label_data, static_cast<uint32_t>(label.size()));
    auto hash_data =
        fbs_builder.CreateVector(reinterpret_cast<const uint8_t *>(hash_.data()), hash_.size());
    auto payload_data =
        fbs_builder.CreateVector(reinterpret_cast<const uint8_t *>(payload.data()), payload.size());

    vector<uint8_t> empty_label;
    flatbuffers::Offset<ozks::fbs::PartialLabel> left_label;
    flatbuffers::Offset<ozks::fbs::PartialLabel> right_label;

    if (!left.empty()) {
        auto left_bytes = utils::bools_to_bytes(left);
        auto left_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(left_bytes.data()), left_bytes.size());
        left_label = fbs::CreatePartialLabel(
            fbs_builder, left_data, static_cast<uint32_t>(left.size()));
    } else {
        auto left_data = fbs_builder.CreateVector(empty_label);
        left_label = fbs::CreatePartialLabel(fbs_builder, left_data, 0);
    }

    if (!right.empty()) {
        auto right_bytes = utils::bools_to_bytes(right);
        auto right_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(right_bytes.data()), right_bytes.size());
        right_label = fbs::CreatePartialLabel(
            fbs_builder, right_data, static_cast<uint32_t>(right.size()));
    } else {
        auto right_data = fbs_builder.CreateVector(empty_label);
        right_label = fbs::CreatePartialLabel(fbs_builder, right_data, 0);
    }

    fbs::CTNodeBuilder ctnode_builder(fbs_builder);
    ctnode_builder.add_label(partial_label);
    ctnode_builder.add_hash(hash_data);
    ctnode_builder.add_payload(payload_data);
    ctnode_builder.add_left(left_label);
    ctnode_builder.add_right(right_label);
    ctnode_builder.add_is_dirty(is_dirty_);

    auto fbs_ctnode = ctnode_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_ctnode);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    return fbs_builder.GetSize();
}

size_t CTNode::save(ostream &stream) const
{
    StreamSerializationWriter writer(&stream);
    return save(writer);
}

template <class T>
size_t CTNode::save(vector<T> &vec) const
{
    VectorSerializationWriter writer(&vec);
    return save(writer);
}

tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::load(
    SerializationReader &reader)
{
    vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

    auto verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
    bool safe = fbs::VerifySizePrefixedCTNodeBuffer(verifier);

    if (!safe) {
        throw runtime_error("Failed to load CTNode: invalid buffer");
    }

    auto fbs_ctnode = fbs::GetSizePrefixedCTNode(in_data.data());
    CTNode node;

    node.label = utils::bytes_to_bools(
        reinterpret_cast<const byte *>(fbs_ctnode->label()->data()->data()),
        fbs_ctnode->label()->size());
    partial_label_type left = utils::bytes_to_bools(
        reinterpret_cast<const byte *>(fbs_ctnode->left()->data()->data()),
        fbs_ctnode->left()->size());
    partial_label_type right = utils::bytes_to_bools(
        reinterpret_cast<const byte *>(fbs_ctnode->right()->data()->data()),
        fbs_ctnode->right()->size());
    node.left = left;
    node.right = right;

    node.payload.resize(fbs_ctnode->payload()->size());
    utils::copy_bytes(
        fbs_ctnode->payload()->data(), fbs_ctnode->payload()->size(), node.payload.data());
    utils::copy_bytes(fbs_ctnode->hash()->data(), fbs_ctnode->hash()->size(), node.hash_.data());

    // is_dirty is not saved because it is in an error if we attempt to save a dirty node
    //node.is_dirty_ = false;
    node.is_dirty_ = fbs_ctnode->is_dirty();

    tuple<CTNode, partial_label_type, partial_label_type, size_t> result;
    get<0>(result) = node;
    get<1>(result) = left;
    get<2>(result) = right;
    get<3>(result) = in_data.size();

    return result;
}

tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::load(istream &stream)
{
    StreamSerializationReader reader(&stream);
    return load(reader);
}

template <class T>
tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::load(
    const vector<T> &vec, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return load(reader);
}

// Explicit instantiations
template size_t CTNode::save(vector<uint8_t> &vec) const;
template size_t CTNode::save(vector<byte> &vec) const;
template tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::load(
    const vector<uint8_t> &vec, size_t position);
template tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::load(
    const vector<byte> &vec, size_t position);
