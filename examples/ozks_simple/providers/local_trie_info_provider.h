// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD

// OZKS
#include "oZKS/ozks_config.h"
#include "oZKS/providers/trie_info_provider.h"

namespace ozks_simple {
    namespace providers {
        class LocalTrieInfoProvider : public ozks::providers::TrieInfoProvider {
        public:
            LocalTrieInfoProvider(const ozks::OZKSConfig &config);
            LocalTrieInfoProvider() = delete;

            virtual ~LocalTrieInfoProvider()
            {}

            /**
            Get the root hash of a trie
            */
            ozks::hash_type get_root_hash(ozks::trie_id_type trie_id) override;

            /**
            Get the epoch of a trie
            */
            std::size_t get_epoch(ozks::trie_id_type trie_id) override;
        };
    } // namespace providers
} // namespace ozks_simple
