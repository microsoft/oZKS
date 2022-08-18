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
            using Storage::flush;
            using Storage::load_ctnode;

        public:
            /**
            Flush given sets of nodes, tries, ozks instances and store elements
            */
            virtual void flush(
                const std::vector<std::byte> &trie_id,
                const std::vector<CTNode> &nodes,
                const std::vector<CompressedTrie> &tries,
                const std::vector<OZKS> &ozks,
                const std::vector<std::pair<std::vector<std::byte>, store_value_type>>
                    &store_elements) = 0;

            /**
            Get a node from storage.
            The implementation has the possibility of deciding to load a batch of nodes related to
            the given node. If that is the case, additional nodes will be added to the Storage
            instance given as parameter.
            */
            virtual bool load_ctnode(
                const std::vector<std::byte> &trie_id,
                const partial_label_type &node_id,
                Storage *storage,
                CTNode &node) = 0;
        };
    } // namespace storage
} // namespace ozks
