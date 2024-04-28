// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <mutex>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node_stored.h"
#include "updater.h"

// Poco
#include "Poco/Util/Timer.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks_distributed;
using namespace ozks_distributed::providers;
using namespace ozks_distributed::providers::updater;

namespace {
    mutex timer_tick_mtx_;
}

Updater::Updater(
    trie_id_type trie_id,
    int time_period,
    GetUpdatesFn get_updates,
    SaveInsertResultFn save_insert_result,
    shared_ptr<Storage> storage)
    : trie_id_(trie_id), time_period_(time_period), get_updates_(get_updates),
      save_insert_result_(save_insert_result), storage_(storage)
{
    // Get Trie
    if (nullptr == trie_) {
        auto loaded = CompressedTrie::LoadFromStorageWithChildren(trie_id_, storage_);
        if (loaded.second) {
            trie_ = loaded.first;
        } else {
            trie_ = make_shared<CompressedTrie>(trie_id_, storage_, TrieType::Linked);
            trie_->save_to_storage();
            storage_->flush(trie_id_);
        }
    }

    timer_.schedule(
        Poco::Util::Timer::func([this]() { this->time_tick(); }),
        /* delay */ time_period,
        /* interval */ time_period);
}

void Updater::time_tick()
{
    if (!timer_tick_mtx_.try_lock())
        return;

    // Get updates
    label_hash_batch_type updates = get_updates_(trie_id_);
    append_proof_batch_type append_proofs;
    partial_label_hash_batch_type pupdates(updates.size());

    if (updates.size() == 0) {
        // Nothing to insert
        timer_tick_mtx_.unlock();
        return;
    }

    for (size_t i = 0; i < updates.size(); i++) {
        pupdates[i] = { PartialLabel(updates[i].first), updates[i].second };
    }

    trie_->insert(pupdates, append_proofs);
    trie_->storage()->flush(trie_->id());

    save_insert_result_(trie_id_, append_proofs, trie_->get_commitment());

    timer_tick_mtx_.unlock();
}
