// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/utilities.h"
#include "dist_trie_info_provider.h"

using namespace std;
using namespace ozks;
using namespace ozks_distributed;
using namespace ozks_distributed::providers;

TrieInfoProvider::TrieInfoProvider(const OZKSConfigDist &config) : storage_(config.storage())
{}

ozks::hash_type TrieInfoProvider::get_root_hash(ozks::trie_id_type trie_id)
{
    CompressedTrie trie;
    if (!storage_->load_compressed_trie(trie_id, trie)) {
        return {};
    } else {
        return trie.get_commitment();
    }
}

size_t TrieInfoProvider::get_epoch(ozks::trie_id_type trie_id)
{
    return storage_->get_compressed_trie_epoch(trie_id);
}
