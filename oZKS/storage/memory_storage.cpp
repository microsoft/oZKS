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

bool MemoryStorage::LoadCTNode(
    const vector<byte> &trie_id, const partial_label_type &node_id, CTNode &node)
{
    StorageNodeKey node_key{ trie_id, node_id };
    auto node_it = nodes_.find(node_key);
    if (node_it == nodes_.end()) {
        return false;
    }

    auto load_result = CTNode::load(node_it->second.data());
    node = get<0>(load_result);
    return true;
}

void MemoryStorage::SaveCTNode(const vector<byte> &trie_id, const CTNode& node)
{
    StorageNode stnode(node);
    StorageNodeKey key(trie_id, node.label);

    nodes_[key] = stnode;
}

//size_t MemoryStorage::LoadCompressedTrie(CompressedTrie& trie)
//{
//    size_t result = CompressedTrie::load(trie, v_);
//    return result;
//}
//
//size_t MemoryStorage::SaveCompressedTrie(const CompressedTrie& trie)
//{
//    size_t result = trie.save(v_);
//    return result;
//}
