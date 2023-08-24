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
                label(), make_shared<CTNodeStored>(trie_, label(), hash_, left_label, right_label));
        } else {
            CTNodeStored node(trie_, label(), hash_, left_label, right_label);
            trie_->storage()->save_ctnode(trie_->id(), node);
        }

        set_dirty_bit(dirty_status);
    }
}

void CTNodeLinked::load_from_storage(
    shared_ptr<storage::Storage> storage, const PartialLabel &left, const PartialLabel &right)
{
    if (!left.empty()) {
        CTNodeStored snode;
        if (!storage->load_ctnode(trie_->id(), left, storage, snode)) {
            throw runtime_error("Could not load node");
        }

        shared_ptr<CTNode> node = make_shared<CTNodeLinked>(trie_, snode.label(), snode.hash());
        set_left_node(node);

        node->load_from_storage(storage, snode.left_label(), snode.right_label());
    }

    if (!right.empty()) {
        CTNodeStored snode;
        if (!storage->load_ctnode(trie_->id(), right, storage, snode)) {
            throw runtime_error("Could not load node");
        }

        shared_ptr<CTNode> node = make_shared<CTNodeLinked>(trie_, snode.label(), snode.hash());
        set_right_node(node);

        node->load_from_storage(storage, snode.left_label(), snode.right_label());
    }

    set_dirty_bit(false);
}

void CTNodeLinked::set_left_node(shared_ptr<CTNode> new_left_node)
{
    left_ = new_left_node;
    set_dirty_bit(true);
}

void CTNodeLinked::set_left_node(const PartialLabel &)
{
    // In this type of node this should do nothing
}

shared_ptr<CTNode> CTNodeLinked::set_new_left_node(const PartialLabel &label)
{
    left_ = make_shared<CTNodeLinked>(trie_, label);
    set_dirty_bit(true);
    return left_;
}

shared_ptr<CTNode> CTNodeLinked::set_left_node(
    const PartialLabel &label, const hash_type &hash, size_t epoch)
{
    left_ = make_shared<CTNodeLinked>(trie_, label, hash, epoch);
    set_dirty_bit(true);
    return left_;
}

shared_ptr<CTNode> CTNodeLinked::set_left_node(const PartialLabel &label, const hash_type &hash)
{
    left_ = make_shared<CTNodeLinked>(trie_, label, hash);
    set_dirty_bit(true);
    return left_;
}

void CTNodeLinked::set_right_node(shared_ptr<CTNode> new_right_node)
{
    right_ = new_right_node;
    set_dirty_bit(true);
}

void CTNodeLinked::set_right_node(const PartialLabel &)
{
    // In this type of node this should do nothing
}

shared_ptr<CTNode> CTNodeLinked::set_new_right_node(const PartialLabel &label)
{
    right_ = make_shared<CTNodeLinked>(trie_, label);
    set_dirty_bit(true);
    return right_;
}

shared_ptr<CTNode> CTNodeLinked::set_right_node(
    const PartialLabel &label, const hash_type &hash, size_t epoch)
{
    right_ = make_shared<CTNodeLinked>(trie_, label, hash, epoch);
    set_dirty_bit(true);
    return right_;
}

shared_ptr<CTNode> CTNodeLinked::set_right_node(const PartialLabel &label, const hash_type &hash)
{
    right_ = make_shared<CTNodeLinked>(trie_, label, hash);
    set_dirty_bit(true);
    return right_;
}
