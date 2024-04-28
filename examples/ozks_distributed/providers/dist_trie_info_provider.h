// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <memory>

// OZKS
#include "oZKS/providers/trie_info_provider.h"
#include "oZKS/storage/storage.h"
#include "../ozks_config_dist.h"

namespace ozks_distributed {
    namespace providers {
        class TrieInfoProvider : public ozks::providers::TrieInfoProvider {
        public:
            TrieInfoProvider(const OZKSConfigDist &config);
            TrieInfoProvider() = delete;

            virtual ~TrieInfoProvider()
            {}

            /**
            Get the root hash of a trie
            */
            ozks::hash_type get_root_hash(ozks::trie_id_type trie_id) override;

            /**
            Get the epoch of a trie
            */
            std::size_t get_epoch(ozks::trie_id_type trie_id) override;

        private:
            std::shared_ptr<ozks::storage::Storage> storage_;
        };
    } // namespace providers
} // namespace ozks_distributed
