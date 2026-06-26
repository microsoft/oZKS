// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <utility>

// OZKS
#include "oZKS/storage/batch_storage.h"
#include "oZKS/storage/memory_storage_cache.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;


namespace {
    Poco::UInt32 clamp_to_uint32(size_t value)
    {
        return std::in_range<Poco::UInt32>(value)
            ? static_cast<Poco::UInt32>(value)
            : std::numeric_limits<Poco::UInt32>::max();
    }
}

MemoryStorageCache::MemoryStorageCache(
    shared_ptr<ozks::storage::Storage> backing_storage, size_t cache_size)
    : storage_(backing_storage),
      node_cache_(clamp_to_uint32(cache_size)),
      trie_cache_(clamp_to_uint32(cache_size)),
      store_element_cache_(clamp_to_uint32(cache_size))
{
    node_cache_.Remove += Poco::delegate(this, &MemoryStorageCache::on_node_removed);
}

MemoryStorageCache::~MemoryStorageCache()
{
    node_cache_.Remove -= Poco::delegate(this, &MemoryStorageCache::on_node_removed);
}

bool MemoryStorageCache::load_ctnode(
    trie_id_type trie_id,
    const PartialLabel &node_id,
    shared_ptr<Storage> storage,
    CTNodeStored &node)
{
    StorageNodeKey epochs_key(trie_id, node_id);
    auto epochs_iter = node_epochs_.find(epochs_key); // Ensure epochs are loaded so we can get the latest version
    if (epochs_iter == node_epochs_.end()) {
        // Seems we don't have anything in our cache, try backing storage
        if (storage_->load_ctnode(trie_id, node_id, storage, node)) {
            update_node_epochs_cache(trie_id, node_id, node.epoch());
            StorageNodeVersionedKey key(trie_id, node_id, node.epoch());
            node_cache_.add(key, node);
            return true;
        }

        return false;
    }

    size_t latest_epoch = epochs_iter->second;
    return load_ctnode(trie_id, node_id, latest_epoch, storage, node);
}

bool MemoryStorageCache::load_ctnode(
    trie_id_type trie_id,
    const PartialLabel &node_id,
    size_t epoch,
    shared_ptr<Storage> storage,
    CTNodeStored &node)
{
    StorageNodeVersionedKey key(trie_id, node_id, epoch);
    auto cached_node = node_cache_.get(key);
    if (cached_node.isNull()) {
        if (!storage_->load_ctnode(trie_id, node_id, epoch, storage, node))
            return false;

        node_cache_.add(key, node);
        return true;
    }

    node = *(cached_node.get());
    return true;
}

bool MemoryStorageCache::load_ctnodes(
    trie_id_type trie_id,
    const vector<pair<PartialLabel, size_t>> &label_epoch_pairs,
    vector<CTNodeStored> &out_nodes)
{
    out_nodes.assign(label_epoch_pairs.size(), CTNodeStored{});

    // Serve what we can from the cache; collect the rest for the backing store.
    vector<pair<PartialLabel, size_t>> backing_pairs;
    vector<size_t> backing_indices;
    backing_pairs.reserve(label_epoch_pairs.size());
    backing_indices.reserve(label_epoch_pairs.size());

    for (size_t i = 0; i < label_epoch_pairs.size(); i++) {
        const auto &[label, epoch] = label_epoch_pairs[i];
        StorageNodeVersionedKey key(trie_id, label, epoch);
        auto cached = node_cache_.get(key);
        if (!cached.isNull()) {
            out_nodes[i] = *cached.get();
        } else {
            backing_pairs.emplace_back(label, epoch);
            backing_indices.push_back(i);
        }
    }

    if (backing_pairs.empty()) {
        return true;
    }

    vector<CTNodeStored> backing_nodes;
    if (!storage_->load_ctnodes(trie_id, backing_pairs, backing_nodes)) {
        return false;
    }

    // The interface contract guarantees one node per requested pair when load_ctnodes
    // returns true (missing entries are signaled by throwing). Guard against a
    // non-conforming backend so we fail loudly instead of reading past the end below.
    if (backing_nodes.size() != backing_indices.size()) {
        throw runtime_error(
            "MemoryStorageCache::load_ctnodes: backing store returned an unexpected node count");
    }

    for (size_t i = 0; i < backing_indices.size(); i++) {
        const auto &[label, epoch] = backing_pairs[i];
        StorageNodeVersionedKey key(trie_id, label, epoch);
        node_cache_.add(key, backing_nodes[i]);

        // Keep the latest-epoch index coherent with the rest of the class. If we already
        // track an epoch for this label and this batch-loaded version is newer, advance the
        // index (update_node_epochs_cache drops the now-stale cached version). We deliberately
        // do NOT create a new entry: a batch load may bring a historical version, and recording
        // it as the latest would shadow the true latest still held in backing storage.
        StorageNodeKey epochs_key(trie_id, label);
        auto epochs_it = node_epochs_.find(epochs_key);
        if (epochs_it != node_epochs_.end() && epoch > epochs_it->second) {
            update_node_epochs_cache(trie_id, label, epoch);
        }

        out_nodes[backing_indices[i]] = std::move(backing_nodes[i]);
    }
    return true;
}

void MemoryStorageCache::save_ctnode(trie_id_type trie_id, const CTNodeStored &node)
{
    StorageNodeVersionedKey key(trie_id, node.label(), node.epoch());

    update_node_epochs_cache(trie_id, node.label(), node.epoch());

    storage_->save_ctnode(trie_id, node);
    node_cache_.update(key, node);
}

bool MemoryStorageCache::load_compressed_trie(trie_id_type trie_id, CompressedTrie &trie)
{
    StorageTrieKey key(trie_id);
    auto cached_trie = trie_cache_.get(key);
    if (cached_trie.isNull()) {
        if (!storage_->load_compressed_trie(trie_id, trie))
            return false;

        trie_cache_.add(key, trie);
        return true;
    }

    trie = *(cached_trie.get());
    return true;
}

void MemoryStorageCache::save_compressed_trie(const CompressedTrie &trie)
{
    StorageTrieKey key(trie.id());
    storage_->save_compressed_trie(trie);
    trie_cache_.update(key, trie);
}

bool MemoryStorageCache::load_store_element(
    trie_id_type trie_id, const vector<byte> &se_key, store_value_type &value)
{
    StorageStoreElementKey key(trie_id, se_key);
    auto cached_store_element = store_element_cache_.get(key);
    if (cached_store_element.isNull()) {
        if (!storage_->load_store_element(trie_id, se_key, value))
            return false;

        store_element_cache_.add(key, value);
        return true;
    }

    value = *(cached_store_element.get());
    return true;
}

void MemoryStorageCache::save_store_element(
    trie_id_type trie_id, const vector<byte> &se_key, const store_value_type &value)
{
    StorageStoreElementKey key(trie_id, se_key);
    storage_->save_store_element(trie_id, se_key, value);
    store_element_cache_.update(key, value);
}

void MemoryStorageCache::flush(trie_id_type trie_id)
{
    storage_->flush(trie_id);
}

void MemoryStorageCache::add_ctnode(trie_id_type trie_id, const CTNodeStored &node)
{
    StorageNodeVersionedKey key(trie_id, node.label(), node.epoch());

    update_node_epochs_cache(trie_id, node.label(), node.epoch());
    node_cache_.update(key, node);
}

void MemoryStorageCache::add_compressed_trie(const CompressedTrie &trie)
{
    StorageTrieKey key(trie.id());
    trie_cache_.update(key, trie);
}

void MemoryStorageCache::add_store_element(
    trie_id_type trie_id, const vector<byte> &se_key, const store_value_type &value)
{
    StorageStoreElementKey key(trie_id, se_key);
    store_element_cache_.update(key, value);
}

size_t MemoryStorageCache::get_compressed_trie_epoch(trie_id_type trie_id)
{
    CompressedTrie trie;

    // Look first in backing storage
    if (!storage_->load_compressed_trie(trie_id, trie)) {
        // Not in backing storage, try the cache
        StorageTrieKey key(trie_id);
        auto cached_trie = trie_cache_.get(key);
        if (!cached_trie.isNull()) {
            trie = *cached_trie;
        }
    }

    return trie.epoch();
}

void MemoryStorageCache::load_updated_elements(
    size_t epoch, trie_id_type trie_id, shared_ptr<Storage> storage)
{
    storage_->load_updated_elements(epoch, trie_id, storage);
}

void MemoryStorageCache::delete_ozks(trie_id_type trie_id)
{
    {
        // Look for nodes to delete
        vector<StorageNodeVersionedKey> nodes_to_delete;
        node_cache_.forEach([&](const StorageNodeVersionedKey &key, const CTNode &) {
            if (key.trie_id() == trie_id) {
                nodes_to_delete.push_back(key);
            }
        });

        // Do the deletion
        for (const auto &node : nodes_to_delete) {
            node_cache_.remove(node);
        }
    }

    {
        // Look for node epochs to delete
        vector<StorageNodeKey> epoch_keys_to_erase;
        for (const auto &ne : node_epochs_) {
            if (ne.first.trie_id() == trie_id) {
                epoch_keys_to_erase.push_back(ne.first);
            }
        }

        // Do the deletion
        for (const auto &ek : epoch_keys_to_erase) {
            node_epochs_.erase(ek);
        }
    }

    // There should be a single compressed trie with the id
    StorageTrieKey trie_key(trie_id);
    trie_cache_.remove(trie_key);

    {
        // Look for store elements to delete
        vector<StorageStoreElementKey> store_elems_to_delete;
        store_element_cache_.forEach(
            [&](const StorageStoreElementKey &key, const store_value_type &) {
                if (key.trie_id() == trie_id) {
                    store_elems_to_delete.push_back(key);
                }
            });

        // Do the deletion
        for (const auto &se : store_elems_to_delete) {
            store_element_cache_.remove(se);
        }
    }

    // Do the same in backing storage
    storage_->delete_ozks(trie_id);
}

void MemoryStorageCache::update_node_epochs_cache(
    trie_id_type trie_id, const PartialLabel &node_id, size_t epoch)
{
    StorageNodeKey epochs_key(trie_id, node_id);
    auto epochs_it = node_epochs_.find(epochs_key);

    if (epochs_it != node_epochs_.end()) {
        auto existing_epoch = epochs_it->second;

        // Remove existing node if it exists
        StorageNodeVersionedKey existing_key{ trie_id, node_id, existing_epoch };
        node_cache_.remove(existing_key);
    }

    node_epochs_[epochs_key] = epoch;
}

void MemoryStorageCache::on_node_removed(
    const void* this_ptr, const StorageNodeVersionedKey &key)
{
    // Update epochs cache
    StorageNodeKey epochs_key(key.trie_id(), key.node_id());
    node_epochs_.erase(epochs_key);
}
