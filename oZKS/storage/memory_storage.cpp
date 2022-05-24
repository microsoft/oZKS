// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "memory_storage.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;

MemoryStorage::MemoryStorage()
{}

tuple<size_t, partial_label_type, partial_label_type> MemoryStorage::LoadCTNode(
    const vector<byte> &trie_id, const partial_label_type &node_id, CTNode &node)
{
    tuple<size_t, partial_label_type, partial_label_type> result;
    StorageNodeKey node_key{ trie_id, node_id };
    auto node_it = nodes_.find(node_key);
    if (node_it == nodes_.end()) {
        get<0>(result) = 0;
        get<1>(result) = {};
        get<2>(result) = {};

        return result;
    }

    auto load_result = CTNode::load(node_it->second.data());

    node = std::get<0>(load_result);
    std::get<1>(result) = std::get<1>(load_result);
    std::get<2>(result) = std::get<2>(load_result);
    std::get<0>(result) = std::get<3>(load_result);

    return result;
}

void MemoryStorage::SaveCTNode(const vector<byte> &trie_id, const CTNode& node)
{
    StorageNode stnode(trie_id, node);
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
