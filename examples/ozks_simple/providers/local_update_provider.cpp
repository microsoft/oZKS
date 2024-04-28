// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <mutex>
#include <unordered_map>
#include <vector>

// OZKS
#include "oZKS/utilities.h"
#include "local_update_provider.h"
#include "mapped_tries.h"

using namespace std;
using namespace ozks;
using namespace ozks_simple;
using namespace ozks_simple::providers;

namespace {
    mutex app_proofs_mtx_;
    unordered_map<trie_id_type, vector<pair<hash_type, append_proof_type>>> append_proofs_;

    void add_append_proof(
        trie_id_type trie_id, const hash_type &label, const append_proof_type &append_proof)
    {
        lock_guard<mutex> app_proofs_lock(app_proofs_mtx_);
        auto trie_proofs = append_proofs_.find(trie_id);

        if (trie_proofs == append_proofs_.end()) {
            vector<pair<hash_type, append_proof_type>> app_proofs;
            app_proofs.push_back({ label, append_proof });
            append_proofs_.insert_or_assign(trie_id, std::move(app_proofs));
            return;
        }

        trie_proofs->second.push_back({ label, append_proof });
    }

    void add_append_proofs(
        trie_id_type trie_id,
        const label_hash_batch_type &labels_commitments,
        const vector<append_proof_type> &append_proofs)
    {
        lock_guard<mutex> app_proofs_lock(app_proofs_mtx_);
        auto trie_proofs = append_proofs_.find(trie_id);
        if (trie_proofs == append_proofs_.end()) {
            vector<pair<hash_type, append_proof_type>> app_proofs;
            append_proofs_.insert_or_assign(trie_id, std::move(app_proofs));
            trie_proofs = append_proofs_.find(trie_id);
        }

        for (size_t idx = 0; idx < labels_commitments.size(); idx++) {
            trie_proofs->second.push_back({ labels_commitments[idx].first, append_proofs[idx] });
        }
    }
} // namespace

LocalUpdateProvider::LocalUpdateProvider(const OZKSConfig &config)
{
    set_config(config.storage(), config.trie_type(), config.thread_count());
}

void LocalUpdateProvider::insert(
    trie_id_type trie_id,
    const hash_type &label,
    const hash_type &payload_commitment,
    optional<reference_wrapper<append_proof_type>> append_proof)
{
    auto trie = get_compressed_trie(trie_id);
    PartialLabel plabel(label);

    if (append_proof.has_value()) {
        trie->insert(plabel, payload_commitment, append_proof.value().get());
    } else {
        append_proof_type app_proof;
        trie->insert(plabel, payload_commitment, app_proof);
        add_append_proof(trie_id, label, app_proof);
    }
}

void LocalUpdateProvider::insert(
    trie_id_type trie_id,
    const vector<pair<hash_type, hash_type>> &labels_commitments,
    optional<reference_wrapper<vector<append_proof_type>>> append_proofs)
{
    auto trie = get_compressed_trie(trie_id);
    if (append_proofs.has_value()) {
        append_proofs.value().get().resize(labels_commitments.size());
    }

    partial_label_hash_batch_type plabels_commitments(labels_commitments.size());
    for (size_t i = 0; i < labels_commitments.size(); i++) {
        plabels_commitments[i] = pair<PartialLabel, hash_type>(
            { labels_commitments[i].first }, labels_commitments[i].second);
    }

    if (append_proofs.has_value()) {
        trie->insert(plabels_commitments, append_proofs.value().get());
    } else {
        vector<append_proof_type> app_proofs(plabels_commitments.size());
        trie->insert(plabels_commitments, app_proofs);
        add_append_proofs(trie_id, labels_commitments, app_proofs);
    }
}

void LocalUpdateProvider::get_append_proofs(
    trie_id_type trie_id, vector<hash_type> &labels, vector<append_proof_type> &append_proofs)
{
    labels.clear();
    append_proofs.clear();

    lock_guard<mutex> app_proofs_lock(app_proofs_mtx_);
    auto trie_proofs = append_proofs_.find(trie_id);

    if (trie_proofs == append_proofs_.end()) {
        return;
    }

    labels.resize(trie_proofs->second.size());
    append_proofs.resize(trie_proofs->second.size());

    size_t idx = 0;
    for (const auto &elem : trie_proofs->second) {
        labels[idx] = elem.first;
        append_proofs[idx] = elem.second;
        idx++;
    }
}
