// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <mutex>
#include <unordered_map>

// OZKS
#include "oZKS/utilities.h"
#include "common_storage.h"
#include "dist_update_provider.h"

using namespace std;
using namespace ozks;
using namespace ozks_distributed;
using namespace ozks_distributed::providers;
using namespace ozks_distributed::providers::updater;

namespace {
    mutex updates_mtx_;
    unordered_map<trie_id_type, label_hash_batch_type> updates_;
}

UpdateProvider::UpdateProvider(trie_id_type trie_id, int time_period, const ozks::OZKSConfig& config)
{
    updater_ = make_shared<Updater>(
        trie_id,
        /* time_period */ time_period,
        UpdateProvider::get_updates,
        UpdateProvider::save_insert_result,
        config.storage());
}

void UpdateProvider::insert(
    ozks::trie_id_type trie_id,
    const ozks::hash_type &label,
    const ozks::hash_type &payload_commitment,
    optional<reference_wrapper<ozks::append_proof_type>> append_proof)
{
    lock_guard<mutex> update_guard(updates_mtx_);
    auto updates = updates_.find(trie_id);
    if (updates != updates_.end()) {
        updates->second.push_back({ label, payload_commitment });
    } else {
        label_hash_batch_type updts;
        updts.push_back({ label, payload_commitment });
        updates_[trie_id] = updts;
    }
}

void UpdateProvider::insert(
    ozks::trie_id_type trie_id,
    const vector<pair<ozks::hash_type, ozks::hash_type>> &labels_commitments,
    optional<reference_wrapper<vector<ozks::append_proof_type>>> append_proofs)
{
    lock_guard<mutex> update_guard(updates_mtx_);
    auto updates = updates_.find(trie_id);
    if (updates != updates_.end()) {
        for (const auto &lc : labels_commitments) {
            updates->second.push_back(lc);
        }
    } else {
        label_hash_batch_type updts;
        for (const auto &lc : labels_commitments) {
            updts.push_back(lc);
        }
        updates_[trie_id] = updts;
    }
}

void UpdateProvider::get_append_proofs(
    trie_id_type trie_id,
    vector<hash_type>& labels,
    vector<append_proof_type>& append_proofs)
{

}

label_hash_batch_type UpdateProvider::get_updates(trie_id_type trie_id)
{
    lock_guard<mutex> update_guard(updates_mtx_);
    auto updates = updates_.find(trie_id);
    if (updates == updates_.end()) {
        return {};
    }
    
    label_hash_batch_type result(std::move(updates->second));
    updates->second.clear();

    return result;
}

void UpdateProvider::save_insert_result(
    trie_id_type trie_id, const append_proof_batch_type &append_proofs, const hash_type &root_hash)
{
    // Nothing for now
}
