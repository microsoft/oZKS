// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD

// OZKS
#include "oZKS/defines.h"

namespace ozks {
    namespace providers {
        class TrieInfoProvider {
        public:
            virtual ~TrieInfoProvider()
            {}

            /**
            Get the root hash of a trie
            */
            virtual hash_type get_root_hash(trie_id_type trie_id) = 0;

            /**
            Get the epoch of a trie
            */
            virtual std::size_t get_epoch(trie_id_type trie_id) = 0;
        };
    } // namespace providers
} // namespace ozks
