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

bool MemoryStorage::load_ctnode(
    const vector<byte> &trie_id, const partial_label_type &node_id, CTNode &node)
{
    StorageNodeKey node_key{ trie_id, node_id };
    auto node_it = nodes_.find(node_key);
    if (node_it == nodes_.end()) {
        return false;
    }

    auto load_result = CTNode::Load(node_it->second.data());
    node = get<0>(load_result);
    return true;
}

void MemoryStorage::save_ctnode(const vector<byte> &trie_id, const CTNode& node)
{
    StorageNode stnode(node);
    StorageNodeKey key(trie_id, node.label);

    nodes_[key] = stnode;
}

bool MemoryStorage::load_compressed_trie(const vector<byte> &trie_id, CompressedTrie &trie)
{
    StorageTrieKey trie_key{ trie_id };
    auto trie_it = tries_.find(trie_key);
    if (trie_it == tries_.end()) {
        return false;
    }

    CompressedTrie::Load(trie, trie_it->second.data());
    return true;
}

void MemoryStorage::save_compressed_trie(const CompressedTrie &trie)
{
    StorageTrie sttrie(trie);
    StorageTrieKey key(trie.id());

    tries_[key] = sttrie;
}

bool MemoryStorage::load_ozks(const vector<byte> &trie_id, OZKS &ozks)
{
    StorageOZKSKey ozks_key(trie_id);
    auto ozks_it = ozks_.find(ozks_key);
    if (ozks_it == ozks_.end()) {
        return false;
    }

    OZKS::Load(ozks, ozks_it->second.data());
    return true;
}

void MemoryStorage::save_ozks(const OZKS& ozks)
{
    StorageOZKSKey key(ozks.id());
    StorageOZKS stozks(ozks);

    ozks_[key] = stozks;
}

bool MemoryStorage::load_store_element(
    const vector<byte> &trie_id, const vector<byte> &key, store_value_type &value)
{
    StorageStoreElementKey se_key(trie_id, key);
    auto se_it = store_.find(se_key);
    if (se_it == store_.end()) {
        return false;
    }

    se_it->second.load_store_element(value.payload, value.randomness);
    return true;
}

void MemoryStorage::save_store_element(
    const vector<byte> &trie_id, const vector<byte> &key, const store_value_type &value)
{
    StorageStoreElementKey se_key(trie_id, key);
    StorageStoreElement selem(value.payload, value.randomness);

    store_[se_key] = selem;
}

void MemoryStorage::flush(const vector<byte> &trie_id)
{
    // Nothing to do because there is nowhere to flush to
}

void StorageStoreElement::load_store_element(payload_type &payload, randomness_type &randomness)
{
    size_t position = 0;
    load_bvector(payload, position);
    load_bvector(randomness, position);
}

void StorageStoreElement::save_store_element(const payload_type& payload, const randomness_type& randomness)
{
    data_.clear();
    save_bvector(payload);
    save_bvector(randomness);
}

void StorageStoreElement::save_bvector(const vector<byte>& value)
{
    size_t write_pos = data_.size();
    size_t new_size = data_.size() + sizeof(size_t) + value.size();

    data_.resize(new_size);

    size_t value_size = value.size();
    utils::copy_bytes(&value_size, sizeof(size_t), data_.data() + write_pos);
    write_pos += sizeof(size_t);

    utils::copy_bytes(value.data(), value.size(), data_.data() + write_pos);
}

void StorageStoreElement::load_bvector(vector<byte>& value, size_t& position)
{
    size_t vec_size = 0;

    if (position + sizeof(size_t) > data_.size())
        throw runtime_error("Ran out of bytes while reading size");

    utils::copy_bytes(data_.data() + position, sizeof(size_t), &vec_size);
    position += sizeof(size_t);

    if (position + vec_size > data_.size())
        throw runtime_error("Ran out of bytes while reading vector");
    
    value.resize(vec_size);
    utils::copy_bytes(data_.data() + position, vec_size, value.data());
    position += vec_size;
}
