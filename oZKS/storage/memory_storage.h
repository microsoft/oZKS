// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <vector>

// OZKS
#include "storage.h"

namespace ozks {
    namespace storage {
        class MemoryStorage : public Storage {
        public:
            MemoryStorage(std::vector<std::uint8_t>& v);

            /**
            Get a node from storage
            */
            virtual std::tuple<std::size_t, partial_label_type, partial_label_type> LoadCTNode(
                CTNode &node);

            /**
            Save a node to storage
            */
            virtual std::size_t SaveCTNode(const CTNode &node);

            /**
            Get a compressed trie from storage
            */
            virtual std::size_t LoadCompressedTrie(CompressedTrie &trie);

            /**
            Save a compressed trie to storage
            */
            virtual std::size_t SaveCompressedTrie(const CompressedTrie &trie);

        private:
            std::vector<std::uint8_t> &v_;
        };
    } // namespace storage
} // namespace ozks
