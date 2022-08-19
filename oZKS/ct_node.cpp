// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <cstring>
#include <sstream>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node.h"
#include "oZKS/ct_node_generated.h"
#include "oZKS/storage/storage.h"
#include "oZKS/utilities.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

CTNode::CTNode(const CompressedTrie *trie) : trie_(trie)
{
    memset(hash_.data(), 0, hash_.size());
}

CTNode::CTNode(const CTNode &node)
{
    *this = node;
}

string CTNode::to_string(bool include_payload) const
{
    stringstream ss;
    string left_str = left.empty() ? "(null)" : utils::to_string(left);
    string right_str = right.empty() ? "(null)" : utils::to_string(right);

    ss << "n:" << utils::to_string(label);
    ss << ":l:" << left_str << ":r:" << right_str;

    if (include_payload && payload.size() > 0) {
        ss << ":p:" << utils::to_string(payload);
    }

    ss << ";";

    if (!left.empty()) {
        CTNode left_node;
        load_left(left_node);
        string sub = left_node.to_string(include_payload);
        ss << sub;
    }
    if (!right.empty()) {
        CTNode right_node;
        load_right(right_node);
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
    left = node.left;
    right = node.right;
    trie_ = node.trie_;

    return *this;
}

void CTNode::init(
    const partial_label_type &init_label, const payload_type &init_payload, size_t epoch)
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
    if (!is_root() && is_leaf())
        throw runtime_error("Should not be used for leaf nodes");

    label = init_label;
    payload = {};
    is_dirty_ = true;
}

void CTNode::init(const CompressedTrie *trie)
{
    trie_ = trie;
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
        load_left(left_node);
        if (left_node.is_dirty_) {
            return false;
        }

        left_hash = left_node.hash();
        left_label = left_node.label;
    }

    if (!right.empty()) {
        CTNode right_node;
        load_right(right_node);
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
    const partial_label_type &insert_label, const payload_type &insert_payload, size_t epoch)
{
    if (insert_label == label) {
        throw runtime_error("Attempting to insert the same label");
    }

    vector<bool> common = get_common_prefix(insert_label, label);

    if (common.size() >= insert_label.size()) {
        throw runtime_error("Common part should be smaller");
    }

    bool next_bit = insert_label[common.size()];

    if (is_leaf() && !is_root()) {
        // Convert current leaf to non-leaf
        CTNode new_parent(trie_);
        new_parent.init(common);

        if (next_bit == 0) {
            new_parent.left = insert_label;
            new_parent.right = label;

            CTNode left_node(trie_);
            left_node.init(insert_label, insert_payload, epoch);
            left_node.save();
        } else {
            new_parent.left = label;
            new_parent.right = insert_label;

            CTNode right_node(trie_);
            right_node.init(insert_label, insert_payload, epoch);
            right_node.save();
        }

        new_parent.save();
        return common;
    }

    // If there is a route to follow, follow it
    if (next_bit == 1 && !right.empty() && right[common.size()] == 1) {
        CTNode right_node;
        load_right(right_node);
        partial_label_type new_right = right_node.insert(insert_label, insert_payload, epoch);
        if (new_right != right) {
            right = new_right;
        }
        is_dirty_ = true;
        this->save();
        return label;
    }

    if (next_bit == 0 && !left.empty() && left[common.size()] == 0) {
        CTNode left_node;
        load_left(left_node);
        partial_label_type new_left = left_node.insert(insert_label, insert_payload, epoch);
        if (new_left != left) {
            left = new_left;
        }
        is_dirty_ = true;
        this->save();
        return label;
    }

    CTNode new_node(trie_);
    new_node.init(common);

    // Node where new node will be inserted
    partial_label_type *insert_node = nullptr;
    // Node that will get the value of this node
    partial_label_type *transfer_node = nullptr;

    // If there is no route to follow, insert here
    if (next_bit == 1) {
        if (right.empty()) {
            CTNode right_node(trie_);
            right_node.init(insert_label, insert_payload, epoch);
            right = insert_label;
            is_dirty_ = true;

            this->save();
            right_node.save();
            return label;
        }

        insert_node = &new_node.right;
        transfer_node = &new_node.left;
    } else {
        if (left.empty()) {
            CTNode left_node(trie_);
            left_node.init(insert_label, insert_payload, epoch);
            left = insert_label;
            is_dirty_ = true;

            this->save();
            left_node.save();
            return label;
        }

        insert_node = &new_node.left;
        transfer_node = &new_node.right;
    }

    *transfer_node = label;
    *insert_node = insert_label;

    CTNode new_inserted_node(trie_);
    new_inserted_node.init(insert_label, insert_payload, epoch);
    is_dirty_ = true;

    new_node.save();
    new_inserted_node.save();
    this->save();

    return common;
}

bool CTNode::lookup(
    const partial_label_type &lookup_label, lookup_path_type &path, bool include_searched)
{
    return lookup(lookup_label, path, include_searched, /* update_hashes */ false);
}

void CTNode::update_hashes(const partial_label_type &lbl)
{
    lookup_path_type path;
    if (!lookup(lbl, path, /* include_searched */ false, /* update_hashes */ true)) {
        throw runtime_error("Should have found the path of the label to update hashes");
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
        load_right(right_node);
        found = right_node.lookup(lookup_label, path, include_searched, update_hashes);
        if (!left.empty())
            load_left(sibling);
    } else if (next_bit == 0 && !left.empty() && left[common.size()] == 0) {
        load_left(left_node);
        found = left_node.lookup(lookup_label, path, include_searched, update_hashes);
        if (!right.empty())
            load_right(sibling);
    }

    if (!found && path.empty()) {
        if (!update_hashes) {
            // Need to include non-existence proof in result.
            if (!left.empty()) {
                if (left_node.is_empty())
                    load_left(left_node);

                path.push_back({ left_node.label, left_node.hash() });
            }

            if (!right.empty()) {
                if (right_node.is_empty())
                    load_right(right_node);

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
                sibling.save();
            } else {
                // Add sibling to the path
                path.push_back({ sibling.label, sibling.hash() });
            }
        }

        if (update_hashes) {
            update_hash();
            this->save();
        }
    }

    return found;
}

size_t CTNode::save(SerializationWriter &writer) const
{
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
        left_label =
            fbs::CreatePartialLabel(fbs_builder, left_data, static_cast<uint32_t>(left.size()));
    } else {
        auto left_data = fbs_builder.CreateVector(empty_label);
        left_label = fbs::CreatePartialLabel(fbs_builder, left_data, 0);
    }

    if (!right.empty()) {
        auto right_bytes = utils::bools_to_bytes(right);
        auto right_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(right_bytes.data()), right_bytes.size());
        right_label =
            fbs::CreatePartialLabel(fbs_builder, right_data, static_cast<uint32_t>(right.size()));
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

tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::Load(
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

    node.is_dirty_ = fbs_ctnode->is_dirty();

    tuple<CTNode, partial_label_type, partial_label_type, size_t> result;
    get<0>(result) = node;
    get<1>(result) = left;
    get<2>(result) = right;
    get<3>(result) = in_data.size();

    return result;
}

tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::Load(istream &stream)
{
    StreamSerializationReader reader(&stream);
    return Load(reader);
}

template <class T>
tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::Load(
    const vector<T> &vec, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return Load(reader);
}

bool CTNode::load(const partial_label_type &lbl, CTNode &node) const
{
    if (nullptr == trie_)
        throw runtime_error("trie_ is null");
    if (nullptr == trie_->storage())
        throw runtime_error("trie_->storage is null");

    bool loaded = trie_->storage()->load_ctnode(trie_->id(), lbl, node);
    if (loaded) {
        node.init(trie_);
    }

    return loaded;
}

bool CTNode::load_left(CTNode &node) const
{
    if (left.empty())
        throw runtime_error("Tried to load empty left node");

    return load(left, node);
}

bool CTNode::load_right(CTNode &node) const
{
    if (right.empty())
        throw runtime_error("Tried to load empty right node");

    return load(right, node);
}

void CTNode::save() const
{
    if (nullptr == trie_)
        throw runtime_error("trie_ is null");
    if (nullptr == trie_->storage())
        throw runtime_error("trie_->storage is null");

    trie_->storage()->save_ctnode(trie_->id(), *this);
}

// Explicit instantiations
template size_t CTNode::save(vector<uint8_t> &vec) const;
template size_t CTNode::save(vector<byte> &vec) const;
template tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::Load(
    const vector<uint8_t> &vec, size_t position);
template tuple<CTNode, partial_label_type, partial_label_type, size_t> CTNode::Load(
    const vector<byte> &vec, size_t position);
