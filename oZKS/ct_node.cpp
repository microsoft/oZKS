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

using namespace std;
using namespace ozks;
using namespace ozks::utils;

CTNode::CTNode()
{
    memset(hash.data(), 0, hash.size());
}

CTNode::CTNode(const CTNode &node)
{
    *this = node;
}

string CTNode::to_string(bool include_payload) const
{
    stringstream ss;
    string left_str = left == nullptr ? "(null)" : utils::to_string(left->label);
    string right_str = right == nullptr ? "(null)" : utils::to_string(right->label);

    ss << "n:" << utils::to_string(label);
    ss << ":l:" << left_str << ":r:" << right_str;

    if (include_payload && payload.size() > 0) {
        ss << ":p:" << utils::to_string(payload);
    }

    ss << ";";

    if (nullptr != left) {
        string sub = left->to_string(include_payload);
        ss << sub;
    }
    if (nullptr != right) {
        string sub = right->to_string(include_payload);
        ss << sub;
    }

    return ss.str();
}

CTNode &CTNode::operator=(const CTNode &node)
{
    label = node.label;
    payload = node.payload;
    hash = node.hash;
    left = nullptr;
    right = nullptr;

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
    hash = init_hash;
}

void CTNode::init(const partial_label_type &init_label)
{
    if (is_leaf())
        throw runtime_error("Should not be used for leaf nodes");

    label = init_label;
    payload = {};

    update_hash();
}

void CTNode::update_hash()
{
    if (is_leaf())
        throw runtime_error("Should not be used for leaf nodes");

    // Compute hash with updated children values
    partial_label_type left_label;
    partial_label_type right_label;
    hash_type left_hash{};
    hash_type right_hash{};

    if (nullptr != left) {
        left_hash = left->hash;
        left_label = left->label;
    }

    if (nullptr != right) {
        right_hash = right->hash;
        right_label = right->label;
    }

    compute_node_hash(left_label, left_hash, right_label, right_hash, hash);
}

void CTNode::insert(
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

    if (is_leaf() && !is_empty()) {
        // Convert current leaf to non-leaf
        left = make_unique<CTNode>();
        right = make_unique<CTNode>();

        if (next_bit == 0) {
            left->init(insert_label, insert_payload, epoch);
            right->init(label, payload, hash);

        } else {
            left->init(label, payload, hash);
            right->init(insert_label, insert_payload, epoch);
        }

        init(common);
        return;
    }

    // If there is a route to follow, follow it
    if (next_bit == 1 && right != nullptr && right->label[common.size()] == 1) {
        right->insert(insert_label, insert_payload, epoch);
        update_hash();
        return;
    }

    if (next_bit == 0 && left != nullptr && left->label[common.size()] == 0) {
        left->insert(insert_label, insert_payload, epoch);
        update_hash();
        return;
    }

    // Node where new node will be inserted
    unique_ptr<CTNode> *insert_node = nullptr;
    // Node that will get the value of this node
    unique_ptr<CTNode> *transfer_node = nullptr;

    // If there is no route to follow, insert here
    if (next_bit == 1) {
        if (nullptr == right) {
            right = make_unique<CTNode>();
            right->init(insert_label, insert_payload, epoch);
            update_hash();
            return;
        }

        insert_node = &right;
        transfer_node = &left;
    } else {
        if (nullptr == left) {
            left = make_unique<CTNode>();
            left->init(insert_label, insert_payload, epoch);
            update_hash();
            return;
        }

        insert_node = &left;
        transfer_node = &right;
    }

    unique_ptr<CTNode> new_node = make_unique<CTNode>();
    new_node->init(label, payload, hash);
    new_node->left.swap(left);
    new_node->right.swap(right);

    label = common;
    transfer_node->swap(new_node);

    *insert_node = make_unique<CTNode>();
    (*insert_node)->init(insert_label, insert_payload, epoch);
    update_hash();
}

bool CTNode::lookup(
    const partial_label_type &lookup_label, lookup_path_type &path, bool include_searched) const
{
    if (label == lookup_label) {
        if (include_searched) {
            // This node is the result
            path.push_back({ label, hash });
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
    CTNode *sibling = nullptr;

    if (next_bit == 1 && right != nullptr && right->label[common.size()] == 1) {
        found = right->lookup(lookup_label, path, include_searched);
        sibling = left.get();
    } else if (next_bit == 0 && left != nullptr && left->label[common.size()] == 0) {
        found = left->lookup(lookup_label, path, include_searched);
        sibling = right.get();
    }

    if (!found && path.empty()) {
        // Need to inlcude non-existene proof in result.
        if (nullptr != left) {
            path.push_back({ left->label, left->hash });
        }

        if (nullptr != right) {
            path.push_back({ right->label, right->hash });
        }

        if (!is_empty()) {
            path.push_back({ label, hash });
        }
    } else if (nullptr != sibling) {
        // Add sibling to the path
        path.push_back({ sibling->label, sibling->hash });
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
        fbs_builder.CreateVector(reinterpret_cast<const uint8_t *>(hash.data()), hash.size());
    auto payload_data =
        fbs_builder.CreateVector(reinterpret_cast<const uint8_t *>(payload.data()), payload.size());

    vector<uint8_t> empty_label;
    flatbuffers::Offset<ozks::fbs::PartialLabel> left_label;
    flatbuffers::Offset<ozks::fbs::PartialLabel> right_label;

    if (nullptr != left) {
        auto left_bytes = utils::bools_to_bytes(left->label);
        auto left_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(left_bytes.data()), left_bytes.size());
        left_label = fbs::CreatePartialLabel(
            fbs_builder, left_data, static_cast<uint32_t>(left->label.size()));
    } else {
        auto left_data = fbs_builder.CreateVector(empty_label);
        left_label = fbs::CreatePartialLabel(fbs_builder, left_data, 0);
    }

    if (nullptr != right) {
        auto right_bytes = utils::bools_to_bytes(right->label);
        auto right_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(right_bytes.data()), right_bytes.size());
        right_label = fbs::CreatePartialLabel(
            fbs_builder, right_data, static_cast<uint32_t>(right->label.size()));
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

    node.payload.resize(fbs_ctnode->payload()->size());
    utils::copy_bytes(
        fbs_ctnode->payload()->data(), fbs_ctnode->payload()->size(), node.payload.data());
    utils::copy_bytes(fbs_ctnode->hash()->data(), fbs_ctnode->hash()->size(), node.hash.data());

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
