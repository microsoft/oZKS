// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <tuple>

// OZKS
#include "oZKS/defines.h"


namespace ozks {
    class CTNode;

    namespace storage {
        class Storage {
        public:
            /**
            Get a node from storage
            */
            virtual bool LoadCTNode(
                const std::vector<std::byte> &trie_id,
                const partial_label_type &node_id,
                CTNode &node) = 0;

            /**
            Save a node to storage
            */
            virtual void SaveCTNode(
                const std::vector<std::byte> &trie_id, const CTNode &node) = 0;

            ///**
            //Get a compressed trie from storage
            //*/
            //virtual std::size_t LoadCompressedTrie(CompressedTrie &trie) = 0;

            ///**
            //Save a compressed trie to storage
            //*/
            //virtual std::size_t SaveCompressedTrie(const CompressedTrie &trie) = 0;
        };
    } // namespace storage
} // namespace ozks
