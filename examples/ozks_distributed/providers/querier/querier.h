// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <memory>
#include <vector>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ozks_config.h"
#include "oZKS/storage/storage.h"

namespace ozks_distributed {
    namespace providers {
        namespace querier {
            /**
            Querier class
            */
            class Querier {
            public:
                Querier(
                    ozks::trie_id_type trie_id, std::shared_ptr<ozks::storage::Storage> storage);
                Querier() = delete;

                bool query(const ozks::hash_type &label, ozks::lookup_path_type &lookup_path) const;

                void query(
                    const std::vector<ozks::hash_type> &labels,
                    std::vector<bool> &found,
                    std::vector<ozks::lookup_path_type> &lookup_paths) const;

                std::size_t epoch() const
                {
                    return trie_->epoch();
                }

                void check_for_update(std::shared_ptr<ozks::storage::Storage> storage);

            private:
                std::shared_ptr<ozks::CompressedTrie> trie_;
            };
        } // namespace querier
    }     // namespace providers
} // namespace ozks_distributed
