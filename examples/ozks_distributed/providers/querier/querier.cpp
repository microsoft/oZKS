// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "querier.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks_distributed;
using namespace ozks_distributed::providers;
using namespace ozks_distributed::providers::querier;

Querier::Querier(trie_id_type trie_id, shared_ptr<Storage> storage)
{
    auto loaded = CompressedTrie::LoadFromStorage(trie_id, storage);
    if (!loaded.second) {
        throw runtime_error("Could not load trie");
    }

    trie_ = loaded.first;
}

bool Querier::query(const hash_type& label, lookup_path_type& lookup_path) const
{
    PartialLabel plabel(label);
    return trie_->lookup(plabel, lookup_path);
}

void Querier::query(
    const vector<hash_type> &labels,
    vector<bool> &found,
    vector<lookup_path_type> &lookup_paths) const
{
    found.resize(labels.size());
    lookup_paths.resize(labels.size());

    for (size_t i = 0; i < labels.size(); i++)
    {
        found[i] = query(labels[i], lookup_paths[i]);
    }
}

void Querier::check_for_update(shared_ptr<Storage> storage)
{
    size_t new_epoch = storage->get_compressed_trie_epoch(trie_->id());
    size_t epoch = trie_->epoch();

    if (new_epoch <= epoch) {
        return;
    }

    epoch++;
    for (; epoch <= new_epoch; epoch++) {
        storage->load_updated_elements(epoch, trie_->id(), storage);
    }

    // Update our version of the trie
    auto loaded = CompressedTrie::LoadFromStorage(trie_->id(), storage);
    if (!loaded.second) {
        throw runtime_error("Failed to load trie");
    }

    trie_ = loaded.first;
}
