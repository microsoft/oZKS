// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <vector>
#include <unordered_map>

// OZKS
#include "oZKS/utilities.h"
#include "dist_query_provider.h"
#include "querier/querier.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks_distributed;
using namespace ozks_distributed::providers;
using namespace ozks_distributed::providers::querier;

namespace {
    unordered_map<trie_id_type, vector<Querier>> queriers_;
    unordered_map<QueryProvider *, shared_ptr<Storage>> storage_;

    const Querier &get_querier(trie_id_type trie_id, QueryProvider *provider)
    {
        auto storage = storage_.find(provider);
        auto queriers = queriers_.find(trie_id);

        if (storage == storage_.end()) {
            throw runtime_error("Should have been able to find storage");
        }

        if (queriers == queriers_.end()) {
            vector<Querier> qs;
            // Create four queriers
            for (size_t i = 0; i < 4; i++) {
                qs.push_back({ trie_id, storage->second });
            }

            queriers_.insert_or_assign(trie_id, std::move(qs));
            queriers = queriers_.find(trie_id);

            if (queriers == queriers_.end()) {
                throw runtime_error("Should have been able to find queriers we just inserted");
            }
        }

        byte coin;
        utils::random_bytes(&coin, /* nbytes */ 1);

        size_t querier_idx = static_cast<size_t>(coin) % 4;
        return queriers->second[querier_idx];
    }
} // namespace

QueryProvider::QueryProvider(const OZKSConfigDist& config)
{
    storage_[this] = config.storage_querier();
}

bool QueryProvider::query(
    ozks::trie_id_type trie_id, const ozks::hash_type &label, ozks::lookup_path_type &lookup_path)
{
    const auto &querier = get_querier(trie_id, this);
    return querier.query(label, lookup_path);
}

void QueryProvider::query(
    ozks::trie_id_type trie_id,
    const vector<ozks::hash_type> &labels,
    vector<bool> &found,
    vector<ozks::lookup_path_type> &lookup_paths)
{
    const auto &querier = get_querier(trie_id, this);
    return querier.query(labels, found, lookup_paths);
}

size_t QueryProvider::get_epoch(trie_id_type trie_id)
{
    const auto &querier = get_querier(trie_id, this);
    return querier.epoch();
}

void QueryProvider::check_for_update(trie_id_type trie_id)
{
    auto storage = storage_.find(this);
    auto queriers = queriers_.find(trie_id);

    if (storage == storage_.end()) {
        throw runtime_error("Should have storage for this instance");
    }
    if (queriers == queriers_.end()) {
        return;
    }

    for (auto &querier : queriers->second) {
        querier.check_for_update(storage->second);
    }
}
