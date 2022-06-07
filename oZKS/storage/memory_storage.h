// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <unordered_map>
#include <vector>

// OZKS
#include "oZKS/storage/storage.h"
#include "oZKS/storage/memory_storage_helpers.h"
#include "oZKS/ct_node.h"
#include "oZKS/compressed_trie.h"
#include "oZKS/ozks.h"
#include "oZKS/utilities.h"


namespace ozks {
    namespace storage {
        class MemoryStorage : public Storage {
        public:
            MemoryStorage();

            /**
            Get a node from storage
            */
            virtual bool LoadCTNode(
                const std::vector<std::byte> &trie_id,
                const partial_label_type &node_id,
                CTNode &node);

            /**
            Save a node to storage
            */
            virtual void SaveCTNode(
                const std::vector<std::byte> &trie_id, const CTNode &node);

            /**
            Get a compressed trie from storage
            */
            virtual bool LoadCompressedTrie(
                const std::vector<std::byte> &trie_id, CompressedTrie &trie);

            /**
            Save a compressed trie to storage
            */
            virtual void SaveCompressedTrie(const CompressedTrie &trie);

            /**
            Get an OZKS instance from storage
            */
            virtual bool LoadOZKS(const std::vector<std::byte> &trie_id, OZKS &ozks);

            /**
            Save an OZKS instance to storage
            */
            virtual void SaveOZKS(const OZKS &ozks);

            /**
            Get a store element from storage
            */
            virtual bool LoadStoreElement(
                const std::vector<std::byte> &trie_id,
                const std::vector<std::byte> &key,
                store_value_type &value);

            /**
            Save a store element to storage
            */
            virtual void SaveStoreElement(
                const std::vector<std::byte> &trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value);

        private:
            std::unordered_map<StorageNodeKey, StorageNode, StorageNodeKeyHasher> nodes_;
            std::unordered_map<StorageTrieKey, StorageTrie, StorageTrieKeyHasher> tries_;
            std::unordered_map<StorageOZKSKey, StorageOZKS, StorageOZKSKeyHasher> ozks_;
            std::unordered_map<
                StorageStoreElementKey,
                StorageStoreElement,
                StorageStoreElementKeyHasher>
                store_;
        };
    } // namespace storage
} // namespace ozks
