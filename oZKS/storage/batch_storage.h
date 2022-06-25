// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <utility>
#include <vector>

// OZKS
#include "oZKS/storage/storage.h"


namespace ozks {
    namespace storage {
        class BatchStorage : public Storage {
        public:
            /**
            Flush given sets of nodes, tries, ozks instances and store elements
            */
            virtual void flush(
                const std::vector<std::byte> &trie_id,
                const std::vector<CTNode> &nodes,
                const std::vector<CompressedTrie> &tries,
                const std::vector<OZKS> &ozks,
                const std::vector<std::pair<std::vector<std::byte>, store_value_type>> &store_elements) = 0;
        };
    } // namespace storage
} // namespace ozks
