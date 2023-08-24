// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <memory>

// OZKS
#include "oZKS/ozks_config.h"
#include "oZKS/providers/update_provider.h"
#include "oZKS/storage/storage.h"

// Poco
#include "Poco/Util/Timer.h"

namespace ozks_distributed {
    namespace providers {
        namespace updater {
            /**
            Function to get updates
            */
            using GetUpdatesFn = ozks::label_hash_batch_type (*)(ozks::trie_id_type trie_id);

            /**
            Function to save append proofs
            */
            using SaveInsertResultFn = void (*)(
                ozks::trie_id_type trie_id,
                const ozks::append_proof_batch_type &append_proofs,
                const ozks::hash_type &root_hash);

            /**
            Updater

            This updater inserts updates at regular intervals
            */
            class Updater {
            public:
                Updater(
                    ozks::trie_id_type trie_id,
                    int time_period,
                    GetUpdatesFn get_updates,
                    SaveInsertResultFn save_insert_result,
                    std::shared_ptr<ozks::storage::Storage> storage);

            private:
                ozks::trie_id_type trie_id_;
                int time_period_;
                GetUpdatesFn get_updates_;
                SaveInsertResultFn save_insert_result_;
                std::shared_ptr<ozks::storage::Storage> storage_;
                Poco::Util::Timer timer_;
                std::shared_ptr<ozks::CompressedTrie> trie_;

                void time_tick();
            };
        } // namespace updater
    }     // namespace providers
} // namespace ozks_distributed
