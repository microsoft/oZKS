// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "oZKS/storage/memory_storage_cache.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;

bool MemoryStorageCache::load_ctnode(
    const vector<byte> &trie_id, const partial_label_type &node_id, CTNode &node)
{
    StorageNodeKey key(trie_id, node_id);
    auto cached_node = node_cache_.get(key);
    if (cached_node.isNull()) {
        if (!storage_->load_ctnode(trie_id, node_id, node))
            return false;

        node_cache_.add(key, node);
        return true;
    }

    node = *(cached_node.get());
    return true;
}

void MemoryStorageCache::save_ctnode(const vector<byte> &trie_id, const CTNode &node)
{
    StorageNodeKey key(trie_id, node.label);
    storage_->save_ctnode(trie_id, node);
    node_cache_.update(key, node);
}

bool MemoryStorageCache::load_compressed_trie(const vector<byte> &trie_id, CompressedTrie &trie)
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

bool MemoryStorageCache::load_ozks(const vector<byte> &trie_id, OZKS &ozks)
{
    StorageOZKSKey key(trie_id);
    auto cached_ozks = ozks_cache_.get(key);
    if (cached_ozks.isNull()) {
        if (!storage_->load_ozks(trie_id, ozks))
            return false;

        ozks_cache_.add(key, ozks);
        return true;
    }

    ozks = *(cached_ozks.get());
    return true;
}

void MemoryStorageCache::save_ozks(const OZKS &ozks)
{
    StorageOZKSKey key(ozks.id());
    storage_->save_ozks(ozks);
    ozks_cache_.update(key, ozks);
}

bool MemoryStorageCache::load_store_element(
    const vector<byte> &trie_id, const vector<byte> &se_key, store_value_type &value)
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
    const vector<byte> &trie_id, const vector<byte> &se_key, const store_value_type &value)
{
    StorageStoreElementKey key(trie_id, se_key);
    storage_->save_store_element(trie_id, se_key, value);
    store_element_cache_.update(key, value);
}

void MemoryStorageCache::flush(const vector<byte> &trie_id)
{
    storage_->flush(trie_id);
}

void MemoryStorageCache::add_ctnode(const vector<byte> &trie_id, const CTNode &node)
{
    StorageNodeKey key(trie_id, node.label);
    node_cache_.update(key, node);
}

void MemoryStorageCache::add_compressed_trie(const CompressedTrie &trie)
{
    StorageTrieKey key(trie.id());
    trie_cache_.update(key, trie);
}

void MemoryStorageCache::add_store_element(
    const vector<byte> &trie_id, const vector<byte> &se_key, const store_value_type &value)
{
    StorageStoreElementKey key(trie_id, se_key);
    store_element_cache_.update(key, value);
}

size_t MemoryStorageCache::get_compressed_trie_epoch(const vector<byte> &trie_id)
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
    size_t epoch, const vector<byte> &trie_id, Storage *storage)
{
    // Update this cache
    storage_->load_updated_elements(epoch, trie_id, this);
}
