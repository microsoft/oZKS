// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "local_trie_info_provider.h"
#include "mapped_tries.h"

using namespace std;
using namespace ozks;
using namespace ozks_simple;
using namespace ozks_simple::providers;


LocalTrieInfoProvider::LocalTrieInfoProvider(const OZKSConfig &config)
{
    set_config(config.storage(), config.trie_type(), config.thread_count());
}

hash_type LocalTrieInfoProvider::get_root_hash(trie_id_type trie_id)
{
    auto trie = get_compressed_trie(trie_id);
    return trie->get_commitment();
}

size_t LocalTrieInfoProvider::get_epoch(trie_id_type trie_id)
{
    auto trie = get_compressed_trie(trie_id);
    return trie->epoch();
}
