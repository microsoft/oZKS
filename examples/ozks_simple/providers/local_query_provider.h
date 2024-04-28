// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD

// OZKS
#include "oZKS/ozks_config.h"
#include "oZKS/providers/query_provider.h"

namespace ozks_simple {
    namespace providers {
        class LocalQueryProvider : public ozks::providers::QueryProvider {
        public:
            LocalQueryProvider(const ozks::OZKSConfig &config);
            LocalQueryProvider() = delete;

            virtual ~LocalQueryProvider()
            {}

            /**
            Query a compressed trie
            */
            bool query(
                ozks::trie_id_type trie_id,
                const ozks::hash_type &label,
                ozks::lookup_path_type &lookup_path) override;

            /**
            Perform several queries on a compressed trie
            */
            void query(
                ozks::trie_id_type trie_id,
                const std::vector<ozks::hash_type> &labels,
                std::vector<bool> &found,
                std::vector<ozks::lookup_path_type> &lookup_paths) override;

            /**
            Get epoch number of the current querier
            */
            std::size_t get_epoch(ozks::trie_id_type trie_id) override;

            /**
            Perform an update of the querier
            */
            void check_for_update(ozks::trie_id_type trie_id) override;

        private:
        };
    } // namespace providers
} // namespace ozks_simple
