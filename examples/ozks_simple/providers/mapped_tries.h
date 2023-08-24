// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <memory>

// OZKS
#include "oZKS/compressed_trie.h"

namespace ozks_simple {
    namespace providers {
        /**
        Set configuration used when creating new tries
        */
        void set_config(
            std::shared_ptr<ozks::storage::Storage> storage,
            ozks::TrieType trie_type,
            std::size_t thread_count);

        /**
        Get the Compressed Trie that has the given trie ID
        */
        std::shared_ptr<ozks::CompressedTrie> get_compressed_trie(ozks::trie_id_type trie_id);
    }
} // namespace ozks_simple
