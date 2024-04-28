// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/utilities.h"
#include "local_query_provider.h"
#include "mapped_tries.h"

using namespace std;
using namespace ozks;
using namespace ozks_simple;
using namespace ozks_simple::providers;

LocalQueryProvider::LocalQueryProvider(const OZKSConfig &config)
{
    set_config(config.storage(), config.trie_type(), config.thread_count());
}

bool LocalQueryProvider::query(
    trie_id_type trie_id, const hash_type &label, lookup_path_type &lookup_path)
{
    auto trie = get_compressed_trie(trie_id);
    PartialLabel plabel(label);

    return trie->lookup(plabel, lookup_path);
}

void LocalQueryProvider::query(
    trie_id_type trie_id,
    const vector<hash_type> &labels,
    vector<bool> &found,
    vector<lookup_path_type> &lookup_paths)
{
    vector<PartialLabel> plabels(labels.size());
    found.resize(labels.size());
    lookup_paths.resize(labels.size());

    auto trie = get_compressed_trie(trie_id);

    for (size_t i = 0; i < labels.size(); i++) {
        PartialLabel plabel(labels[i]);
        found[i] = trie->lookup(plabel, lookup_paths[i]);
    }
}

size_t LocalQueryProvider::get_epoch(trie_id_type trie_id)
{
    auto trie = get_compressed_trie(trie_id);
    return trie->epoch();
}

void LocalQueryProvider::check_for_update(trie_id_type trie_id)
{
    // Nothing in this implementation
}
