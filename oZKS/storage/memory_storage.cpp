// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "oZKS/storage/memory_storage.h"

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
    shared_ptr<Storage> /* unused */,
    CTNodeStored &node)
{
    StorageNodeKey node_key{ trie_id, node_id };
    auto node_it = nodes_.find(node_key);
    if (node_it == nodes_.end()) {
        return false;
    }

    node = node_it->second;
    return true;
}

void MemoryStorage::save_ctnode(trie_id_type trie_id, const CTNodeStored &node)
{
    StorageNodeKey key(trie_id, node.label());
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
        vector<StorageNodeKey> nodes_to_delete;
        for (const auto &node : nodes_) {
            if (node.first.trie_id() == trie_id) {
                nodes_to_delete.push_back(node.first);
            }
        }

        // Do the deletion
        for (const auto &node : nodes_to_delete) {
            nodes_.erase(node);
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
