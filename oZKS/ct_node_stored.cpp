// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node_stored.h"
#include "oZKS/ct_node_stored_generated.h"
#include "oZKS/storage/storage.h"
#include "oZKS/utilities.h"

using namespace std;
using namespace ozks;

bool CTNodeStored::is_leaf() const
{
    return left_.empty() && right_.empty();
}

shared_ptr<CTNode> CTNodeStored::left()
{
    if (!left_.empty()) {
        auto node = make_shared<CTNodeStored>();
        if (trie_->storage()->load_ctnode(trie_->id(), left_, trie_->storage(), *node)) {
            node->init(trie_);
            return node;
        }
    }

    return {};
}

shared_ptr<const CTNode> CTNodeStored::left() const
{
    if (!left_.empty()) {
        auto node = make_shared<CTNodeStored>();
        if (trie_->storage()->load_ctnode(trie_->id(), left_, trie_->storage(), *node)) {
            node->init(trie_);
            return node;
        }
    }

    return {};
}

shared_ptr<CTNode> CTNodeStored::right()
{
    if (!right_.empty()) {
        auto node = make_shared<CTNodeStored>();
        if (trie_->storage()->load_ctnode(trie_->id(), right_, trie_->storage(), *node)) {
            node->init(trie_);
            return node;
        }
    }

    return {};
}

shared_ptr<const CTNode> CTNodeStored::right() const
{
    if (!right_.empty()) {
        auto node = make_shared<CTNodeStored>();
        if (trie_->storage()->load_ctnode(trie_->id(), right_, trie_->storage(), *node)) {
            node->init(trie_);
            return node;
        }
    }

    return {};
}

CTNodeStored &CTNodeStored::operator=(const CTNodeStored &node)
{
    CTNode::operator=(node);

    left_ = node.left_;
    right_ = node.right_;

    return *this;
}

void CTNodeStored::set_left_node(shared_ptr<CTNode> new_left_node)
{
    left_ = new_left_node->label();
    set_dirty_bit(true);
}

void CTNodeStored::set_left_node(const PartialLabel &label)
{
    left_ = label;
    set_dirty_bit(true);
}

shared_ptr<CTNode> CTNodeStored::set_left_node(
    const PartialLabel &label, const hash_type &hash, std::size_t epoch)
{
    left_ = label;
    set_dirty_bit(true);
    return make_shared<CTNodeStored>(trie_, label, hash, epoch);
}

shared_ptr<CTNode> CTNodeStored::set_left_node(const PartialLabel &label, const hash_type &hash)
{
    left_ = label;
    set_dirty_bit(true);
    return make_shared<CTNodeStored>(trie_, label, hash);
}

shared_ptr<CTNode> CTNodeStored::set_new_left_node(const PartialLabel &label)
{
    left_ = label;
    set_dirty_bit(true);
    return make_shared<CTNodeStored>(trie_, label);
}

void CTNodeStored::set_right_node(shared_ptr<CTNode> new_right_node)
{
    right_ = new_right_node->label();
    set_dirty_bit(true);
}

void CTNodeStored::set_right_node(const PartialLabel &label)
{
    right_ = label;
    set_dirty_bit(true);
}

shared_ptr<CTNode> CTNodeStored::set_right_node(
    const PartialLabel &label, const hash_type &hash, std::size_t epoch)
{
    right_ = label;
    set_dirty_bit(true);
    return make_shared<CTNodeStored>(trie_, label, hash, epoch);
}

shared_ptr<CTNode> CTNodeStored::set_right_node(const PartialLabel &label, const hash_type &hash)
{
    right_ = label;
    set_dirty_bit(true);
    return make_shared<CTNodeStored>(trie_, label, hash);
}

shared_ptr<CTNode> CTNodeStored::set_new_right_node(const PartialLabel &label)
{
    right_ = label;
    set_dirty_bit(true);
    return make_shared<CTNodeStored>(trie_, label);
}

size_t CTNodeStored::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    array<uint8_t, PartialLabel::SaveSize> label_data{};
    label().save(gsl::span<uint8_t, PartialLabel::SaveSize>(label_data));
    fbs::PartialLabel pl_data(
        flatbuffers::span<const uint8_t, PartialLabel::SaveSize>{ label_data });

    fbs::Hash hash_data(flatbuffers::span<const uint8_t, 32>{
        reinterpret_cast<const uint8_t *>(hash_.data()), hash_size });

    auto create_child_label_data = [&fbs_builder](const PartialLabel &label) {
        array<uint8_t, PartialLabel::SaveSize> child_label_data{};
        label.save(gsl::span<uint8_t, PartialLabel::SaveSize>(child_label_data));
        fbs::PartialLabel child_pl_data(
            flatbuffers::span<const uint8_t, PartialLabel::SaveSize>{ child_label_data });
        return fbs::CreateOptionalPartialLabel(fbs_builder, &child_pl_data);
    };

    flatbuffers::Offset<ozks::fbs::OptionalPartialLabel> left_label;
    if (!left_.empty()) {
        left_label = create_child_label_data(left_);
    }

    flatbuffers::Offset<ozks::fbs::OptionalPartialLabel> right_label;
    if (!right_.empty()) {
        right_label = create_child_label_data(right_);
    }

    fbs::CTNodeStoredBuilder ctnode_builder(fbs_builder);
    ctnode_builder.add_label(&pl_data);
    ctnode_builder.add_left(left_label);
    ctnode_builder.add_right(right_label);
    ctnode_builder.add_hash(&hash_data);

    auto fbs_ctnode = ctnode_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_ctnode);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    return fbs_builder.GetSize();
}

size_t CTNodeStored::save(ostream &stream) const
{
    StreamSerializationWriter writer(&stream);
    return save(writer);
}

template <typename T>
size_t CTNodeStored::save(vector<T> &vec) const
{
    VectorSerializationWriter writer(&vec);
    return save(writer);
}

tuple<CTNodeStored, PartialLabel, PartialLabel, size_t> CTNodeStored::Load(
    SerializationReader &reader, const CompressedTrie *trie)
{
    vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

    auto verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
    bool safe = fbs::VerifySizePrefixedCTNodeStoredBuffer(verifier);

    if (!safe) {
        throw runtime_error("Failed to load CTNode: invalid buffer");
    }

    auto fbs_ctnode = fbs::GetSizePrefixedCTNodeStored(in_data.data());
    CTNodeStored node;

    PartialLabel label;
    label.load(gsl::span<const uint8_t, PartialLabel::SaveSize>{
        fbs_ctnode->label()->data()->data(), fbs_ctnode->label()->data()->size() });

    PartialLabel left;
    if (fbs_ctnode->left()) {
        left.load(gsl::span<const uint8_t, PartialLabel::SaveSize>{
            fbs_ctnode->left()->label()->data()->data(),
            fbs_ctnode->left()->label()->data()->size() });
    }

    PartialLabel right;
    if (fbs_ctnode->right()) {
        right.load(gsl::span<const uint8_t, PartialLabel::SaveSize>{
            fbs_ctnode->right()->label()->data()->data(),
            fbs_ctnode->right()->label()->data()->size() });
    }

    hash_type hash{};
    utils::copy_bytes(fbs_ctnode->hash()->data()->data(), hash_size, hash.data());

    node.init(label, hash);
    node.init(trie);
    node.left_ = left;
    node.right_ = right;

    tuple<CTNodeStored, PartialLabel, PartialLabel, size_t> result;
    get<0>(result) = node;
    get<1>(result) = left;
    get<2>(result) = right;
    get<3>(result) = in_data.size();

    return result;
}

tuple<CTNodeStored, PartialLabel, PartialLabel, size_t> CTNodeStored::Load(
    istream &stream, const CompressedTrie *trie)
{
    StreamSerializationReader reader(&stream);
    return Load(reader, trie);
}

template <typename T>
tuple<CTNodeStored, PartialLabel, PartialLabel, size_t> CTNodeStored::Load(
    const vector<T> &vec, const CompressedTrie *trie, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return Load(reader, trie);
}

void CTNodeStored::save_to_storage(
    unordered_map<PartialLabel, shared_ptr<CTNode>> * /* updated_nodes */)
{
    if (nullptr != trie_->storage()) {
        trie_->storage()->save_ctnode(trie_->id(), *this);
    }
}

// Explicit instantiations
template size_t CTNodeStored::save(vector<uint8_t> &vec) const;
template size_t CTNodeStored::save(vector<byte> &vec) const;
template tuple<CTNodeStored, PartialLabel, PartialLabel, size_t> CTNodeStored::Load(
    const vector<uint8_t> &vec, const CompressedTrie *trie, size_t position);
template tuple<CTNodeStored, PartialLabel, PartialLabel, size_t> CTNodeStored::Load(
    const vector<byte> &vec, const CompressedTrie *trie, size_t position);
