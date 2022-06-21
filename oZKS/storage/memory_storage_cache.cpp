// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "oZKS/storage/memory_storage_cache.h"


using namespace std;
using namespace ozks;
using namespace ozks::storage;


bool MemoryStorageCache::LoadCTNode(
    const vector<byte> &trie_id, const partial_label_type &node_id, CTNode &node)
{
    StorageNodeKey key(trie_id, node_id);
    auto cached_node = node_cache_.get(key);
    if (cached_node.isNull()) {
        if (!storage_->LoadCTNode(trie_id, node_id, node))
            return false;

        node_cache_.add(key, node);
        return true;
    }

    node = *(cached_node.get());
    return true;
}

void MemoryStorageCache::SaveCTNode(const vector<byte>& trie_id, const CTNode& node)
{
    StorageNodeKey key(trie_id, node.label);
    storage_->SaveCTNode(trie_id, node);
    node_cache_.update(key, node);
}

bool MemoryStorageCache::LoadCompressedTrie(
    const vector<byte>& trie_id, CompressedTrie& trie)
{
    StorageTrieKey key(trie_id);
    auto cached_trie = trie_cache_.get(key);
    if (cached_trie.isNull()) {
        if (!storage_->LoadCompressedTrie(trie_id, trie))
            return false;

        trie_cache_.add(key, trie);
    }

    trie = *(cached_trie.get());
    return true;
}

void MemoryStorageCache::SaveCompressedTrie(const CompressedTrie& trie)
{
    StorageTrieKey key(trie.id());
    storage_->SaveCompressedTrie(trie);
    trie_cache_.update(key, trie);
}

bool MemoryStorageCache::LoadOZKS(const vector<byte>& trie_id, OZKS& ozks)
{
    StorageOZKSKey key(trie_id);
    auto cached_ozks = ozks_cache_.get(key);
    if (cached_ozks.isNull()) {
        if (!storage_->LoadOZKS(trie_id, ozks))
            return false;

        ozks_cache_.add(key, ozks);
        return true;
    }

    ozks = *(cached_ozks.get());
    return true;
}

void MemoryStorageCache::SaveOZKS(const OZKS& ozks)
{
    StorageOZKSKey key(ozks.id());
    storage_->SaveOZKS(ozks);
    ozks_cache_.update(key, ozks);
}

bool MemoryStorageCache::LoadStoreElement(
    const vector<byte> &trie_id, const vector<byte> &se_key, store_value_type &value)
{
    StorageStoreElementKey key(trie_id, se_key);
    auto cached_store_element = store_element_cache_.get(key);
    if (cached_store_element.isNull()) {
        if (!storage_->LoadStoreElement(trie_id, se_key, value))
            return false;

        store_element_cache_.add(key, value);
        return true;
    }

    value = *(cached_store_element.get());
    return true;
}

void MemoryStorageCache::SaveStoreElement(
    const vector<byte> &trie_id, const vector<byte> &se_key, const store_value_type &value)
{
    StorageStoreElementKey key(trie_id, se_key);
    storage_->SaveStoreElement(trie_id, se_key, value);
    store_element_cache_.update(key, value);
}
