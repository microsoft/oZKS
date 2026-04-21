// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "oZKS/ct_node_linked.h"
#include "oZKS/ct_node_stored.h"
#include "oZKS/storage/storage.h"

using namespace std;
using namespace ozks;

bool CTNodeLinked::is_leaf() const
{
    return left_ == nullptr && right_ == nullptr;
}

void CTNodeLinked::save_to_storage(unordered_map<PartialLabel, shared_ptr<CTNode>> *updated_nodes)
{
    if (nullptr != updated_nodes || nullptr != trie_->storage()) {
        bool dirty_status = get_dirty_bit();
        set_dirty_bit(false);
        PartialLabel left_label;
        shared_ptr<const CTNode> child = left();
        if (nullptr != child) {
            left_label = child->label();
        }
        PartialLabel right_label;
        child = right();
        if (nullptr != child) {
            right_label = child->label();
        }

        if (nullptr != updated_nodes) {
            updated_nodes->insert_or_assign(
                label(), make_shared<CTNodeStored>(trie_, label(), hash_, epoch(), left_label, right_label, left_child_epoch_, right_child_epoch_));
        } else {
            CTNodeStored node(trie_, label(), hash_, epoch(), left_label, right_label, left_child_epoch_, right_child_epoch_);
            trie_->storage()->save_ctnode(trie_->id(), node);
        }

        set_dirty_bit(dirty_status);
    }
}

void CTNodeLinked::load_from_storage(
    shared_ptr<storage::Storage> storage,
    const PartialLabel &left,
    size_t left_epoch,
    const PartialLabel &right,
    size_t right_epoch)
{
    if (!left.empty()) {
        CTNodeStored snode;
        if (!storage->load_ctnode(trie_->id(), left, left_epoch, storage, snode)) {
            throw runtime_error("Could not load node");
        }

        // Use the (trie, label, hash) ctor: it stores the hash verbatim. The 4-arg
        // ctor would call compute_leaf_hash, which is wrong for interior nodes.
        auto node = make_shared<CTNodeLinked>(trie_, snode.label(), snode.hash());
        node->epoch_ = snode.epoch();
        set_left_node_for_load(node, snode.epoch());

        node->load_from_storage(
            storage,
            snode.left_label(),  snode.left_child_epoch(),
            snode.right_label(), snode.right_child_epoch());
    }

    if (!right.empty()) {
        CTNodeStored snode;
        if (!storage->load_ctnode(trie_->id(), right, right_epoch, storage, snode)) {
            throw runtime_error("Could not load node");
        }

        auto node = make_shared<CTNodeLinked>(trie_, snode.label(), snode.hash());
        node->epoch_ = snode.epoch();
        set_right_node_for_load(node, snode.epoch());

        node->load_from_storage(
            storage,
            snode.left_label(),  snode.left_child_epoch(),
            snode.right_label(), snode.right_child_epoch());
    }

    set_dirty_bit(false);
}

void CTNodeLinked::set_left_node_for_load(shared_ptr<CTNode> new_left_node, size_t child_epoch)
{
    left_ = new_left_node;
    left_child_epoch_ = child_epoch;
}

void CTNodeLinked::set_right_node_for_load(shared_ptr<CTNode> new_right_node, size_t child_epoch)
{
    right_ = new_right_node;
    right_child_epoch_ = child_epoch;
}

void CTNodeLinked::set_left_node(shared_ptr<CTNode> new_left_node)
{
    left_ = new_left_node;
    left_child_epoch_ = new_left_node ? new_left_node->epoch() : 0;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
}

void CTNodeLinked::set_left_node(const PartialLabel &, std::size_t epoch)
{
    left_child_epoch_ = epoch;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
}

shared_ptr<CTNode> CTNodeLinked::set_new_left_node(const PartialLabel &label, std::size_t epoch)
{
    left_ = make_shared<CTNodeLinked>(trie_, label);
    left_child_epoch_ = epoch;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
    return left_;
}

shared_ptr<CTNode> CTNodeLinked::set_left_node(
    const PartialLabel &label, const hash_type &payload, size_t epoch)
{
    left_ = make_shared<CTNodeLinked>(trie_, label, payload, epoch);
    left_child_epoch_ = epoch;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
    return left_;
}

shared_ptr<CTNode> CTNodeLinked::set_existing_left_node(const PartialLabel &label, const hash_type &hash, std::size_t epoch)
{
    auto left = make_shared<CTNodeLinked>(trie_, label, hash);
    left->epoch_ = epoch;

    left_ = left;
    left_child_epoch_ = epoch;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
    return left_;
}

void CTNodeLinked::set_right_node(shared_ptr<CTNode> new_right_node)
{
    right_ = new_right_node;
    right_child_epoch_ = new_right_node ? new_right_node->epoch() : 0;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
}

void CTNodeLinked::set_right_node(const PartialLabel &, std::size_t epoch)
{
    right_child_epoch_ = epoch;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
}

shared_ptr<CTNode> CTNodeLinked::set_new_right_node(const PartialLabel &label, std::size_t epoch)
{
    right_ = make_shared<CTNodeLinked>(trie_, label);
    right_child_epoch_ = epoch;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
    return right_;
}

shared_ptr<CTNode> CTNodeLinked::set_right_node(
    const PartialLabel &label, const hash_type &payload, size_t epoch)
{
    right_ = make_shared<CTNodeLinked>(trie_, label, payload, epoch);
    right_child_epoch_ = epoch;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
    return right_;
}

shared_ptr<CTNode> CTNodeLinked::set_existing_right_node(const PartialLabel &label, const hash_type &hash, std::size_t epoch)
{
    auto right = make_shared<CTNodeLinked>(trie_, label, hash);
    right->epoch_ = epoch;

    right_ = right;
    right_child_epoch_ = epoch;
    epoch_ = trie_->epoch();
    set_dirty_bit(true);
    return right_;
}
