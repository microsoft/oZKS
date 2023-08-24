// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <vector>

// OZKS
#include "oZKS/defines.h"


namespace ozks {
    namespace providers {
        class QueryProvider {
        public:
            virtual ~QueryProvider()
            {}

            /**
            Query a compressed trie
            */
            virtual bool query(
                trie_id_type trie_id, const hash_type &label, lookup_path_type &lookup_path) = 0;

            /**
            Perform several queries on a compressed trie
            */
            virtual void query(
                trie_id_type trie_id,
                const std::vector<hash_type> &labels,
                std::vector<bool> &found,
                std::vector<lookup_path_type> &lookup_paths) = 0;

            /**
            Get epoch number of the querier
            */
            virtual std::size_t get_epoch(trie_id_type trie_id) = 0;

            /**
            Perform an update of the querier
            */
            virtual void check_for_update(trie_id_type trie_id) = 0;
        };
    } // namespace providers
} // namespace ozks
