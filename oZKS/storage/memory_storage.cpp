// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <sstream>
#include <stdexcept>
#include <unordered_set>

// OZKS
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;

MemoryStorage::MemoryStorage()
{}

MemoryStorage::~MemoryStorage()
{}

bool MemoryStorage::load_ctnode(
    trie_id_type trie_id,
    const PartialLabel &node_id,
    shared_ptr<Storage> storage,
    CTNodeStored &node)
{
    StorageNodeKey node_key{ trie_id, node_id };

    // Get latest stored version
    auto epochs_it = node_epochs_.find(node_key);
    if (epochs_it == node_epochs_.end()) {
        return false;
    }

    std::size_t latest_epoch = epochs_it->second;
    return load_ctnode(trie_id, node_id, latest_epoch, storage, node);
}

bool MemoryStorage::load_ctnode(
    trie_id_type trie_id,
    const PartialLabel& node_id,
    std::size_t epoch,
    std::shared_ptr<Storage> storage,
    CTNodeStored& node)
{
    StorageNodeVersionedKey node_key{ trie_id, node_id, epoch };
    auto node_it = nodes_.find(node_key);
    if (node_it == nodes_.end()) {
        return false;
    }

    node = node_it->second;
    return true;
}

bool MemoryStorage::load_ctnodes(
    trie_id_type trie_id,
    const vector<pair<PartialLabel, size_t>> &label_epoch_pairs,
    vector<CTNodeStored> &out_nodes)
{
    out_nodes.assign(label_epoch_pairs.size(), CTNodeStored{});

    vector<size_t> missing_indices;
    for (size_t i = 0; i < label_epoch_pairs.size(); i++) {
        const auto &[label, epoch] = label_epoch_pairs[i];
        if (!load_ctnode(trie_id, label, epoch, /* storage */ nullptr, out_nodes[i])) {
            missing_indices.push_back(i);
        }
    }

    if (!missing_indices.empty()) {
        ostringstream oss;
        oss << "MemoryStorage::load_ctnodes: " << missing_indices.size()
            << " missing node(s):";
        for (size_t idx : missing_indices) {
            oss << " [" << ozks::utils::to_string(label_epoch_pairs[idx].first)
                << " @ epoch " << label_epoch_pairs[idx].second << "]";
        }
        throw runtime_error(oss.str());
    }

    return true;
}

void MemoryStorage::save_ctnode(trie_id_type trie_id, const CTNodeStored &node)
{
    // Track epochs for the node
    StorageNodeKey node_key{ trie_id, node.label() };
    auto epochs_it = node_epochs_.find(node_key);
    if (epochs_it != node_epochs_.end()) {
        // A leaf node's record (label, hash, epoch) is immutable: once written it must
        // never be re-saved at a different epoch. If we hit this, something on the insert
        // path mutated the leaf's epoch_ and then re-saved it. Throw so callers can see
        // exactly which leaf was corrupted.
        bool incoming_is_leaf = node.left_label().empty() && node.right_label().empty();
        if (incoming_is_leaf) {
            size_t existing_epoch = epochs_it->second;
            StorageNodeVersionedKey existing_key{ trie_id, node.label(), existing_epoch };
            auto existing_it = nodes_.find(existing_key);
            if (existing_it != nodes_.end()) {
                const CTNodeStored &existing = existing_it->second;
                bool existing_is_leaf =
                    existing.left_label().empty() && existing.right_label().empty();
                if (existing_is_leaf && existing_epoch != node.epoch()) {
                    std::ostringstream oss;
                    oss << "MemoryStorage::save_ctnode: leaf epoch invariant violated for label "
                        << ozks::utils::to_string(node.label()) << ": stored epoch "
                        << existing_epoch << ", incoming epoch " << node.epoch();
                    throw std::runtime_error(oss.str());
                }
            }
        }

        // Remove existing node
        size_t existing_epoch = epochs_it->second;
        StorageNodeVersionedKey existing_key{ trie_id, node.label(), existing_epoch };
        nodes_.erase(existing_key);
    }

    StorageNodeVersionedKey key(trie_id, node.label(), node.epoch());
    node_epochs_[node_key] = node.epoch();
    nodes_[key] = node;
}

bool MemoryStorage::load_compressed_trie(trie_id_type trie_id, CompressedTrie &trie)
{
    StorageTrieKey trie_key{ trie_id };
    auto trie_it = tries_.find(trie_key);
    if (trie_it == tries_.end()) {
        return false;
    }

    trie = trie_it->second;
    return true;
}

void MemoryStorage::save_compressed_trie(const CompressedTrie &trie)
{
    StorageTrieKey key(trie.id());
    tries_[key] = trie;
}

bool MemoryStorage::load_store_element(
    trie_id_type trie_id, const vector<byte> &key, store_value_type &value)
{
    StorageStoreElementKey se_key(trie_id, key);
    auto se_it = store_.find(se_key);
    if (se_it == store_.end()) {
        return false;
    }

    value = se_it->second;
    return true;
}

void MemoryStorage::save_store_element(
    trie_id_type trie_id, const vector<byte> &key, const store_value_type &value)
{
    StorageStoreElementKey se_key(trie_id, key);
    store_[se_key] = value;
}

void MemoryStorage::flush(trie_id_type)
{
    // Nothing to do because there is nowhere to flush to
}

void MemoryStorage::add_ctnode(trie_id_type, const CTNodeStored &)
{
    throw runtime_error("Does not make sense for this Storage implementation");
}

void MemoryStorage::add_compressed_trie(const CompressedTrie &)
{
    throw runtime_error("Does not make sense for this Storage implementation");
}

void MemoryStorage::add_store_element(trie_id_type, const vector<byte> &, const store_value_type &)
{
    throw runtime_error("Does not make sense for this Storage implementation");
}

size_t MemoryStorage::get_compressed_trie_epoch(trie_id_type trie_id)
{
    size_t result = 0;
    CompressedTrie trie;

    if (load_compressed_trie(trie_id, trie)) {
        result = trie.epoch();
    }

    return result;
}

void MemoryStorage::load_updated_elements(std::size_t, trie_id_type, shared_ptr<Storage>)
{
    // Nothing to do for this implementation
}

void MemoryStorage::delete_ozks(trie_id_type trie_id)
{
    {
        // Find nodes to delete
        vector<StorageNodeVersionedKey> nodes_to_delete;
        unordered_set<StorageNodeKey, StorageNodeKeyHasher> epoch_keys_to_erase;
        for (const auto &node : nodes_) {
            if (node.first.trie_id() == trie_id) {
                nodes_to_delete.push_back(node.first);

                StorageNodeKey epoch_k{ node.first.trie_id(), node.first.node_id() };
                epoch_keys_to_erase.insert(epoch_k);
            }
        }

        // Do the deletion
        for (const auto &node : nodes_to_delete) {
            nodes_.erase(node);
        }

        // Erase epoch tracking entries
        for (const auto &ek : epoch_keys_to_erase) {
            node_epochs_.erase(ek);
        }
    }

    // There should be a single compressed trie with the id
    StorageTrieKey trie_key(trie_id);
    tries_.erase(trie_key);

    //// There should be a single OZKS instance with the id
    // StorageOZKSKey ozks_key(trie_id);
    // ozks_.erase(ozks_key);

    {
        // Find store elements to delete
        vector<StorageStoreElementKey> store_elems_to_delete;
        for (const auto &se : store_) {
            if (se.first.trie_id() == trie_id) {
                store_elems_to_delete.push_back(se.first);
            }
        }

        // Do the deletion
        for (const auto &se : store_elems_to_delete) {
            store_.erase(se);
        }
    }
}
