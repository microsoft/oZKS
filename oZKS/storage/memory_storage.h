// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <unordered_map>
#include <vector>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node.h"
#include "oZKS/ozks.h"
#include "oZKS/storage/memory_storage_helpers.h"
#include "oZKS/storage/storage.h"
#include "oZKS/utilities.h"

namespace ozks {
    namespace storage {
        class MemoryStorage : public Storage {
        public:
            MemoryStorage();

            /**
            Get a node from storage
            */
            bool load_ctnode(
                const std::vector<std::byte> &trie_id,
                const partial_label_type &node_id,
                CTNode &node) override;

            /**
            Save a node to storage
            */
            void save_ctnode(const std::vector<std::byte> &trie_id, const CTNode &node) override;

            /**
            Get a compressed trie from storage
            */
            bool load_compressed_trie(
                const std::vector<std::byte> &trie_id, CompressedTrie &trie) override;

            /**
            Save a compressed trie to storage
            */
            void save_compressed_trie(const CompressedTrie &trie) override;

            /**
            Get an OZKS instance from storage
            */
            bool load_ozks(const std::vector<std::byte> &trie_id, OZKS &ozks) override;

            /**
            Save an OZKS instance to storage
            */
            void save_ozks(const OZKS &ozks) override;

            /**
            Get a store element from storage
            */
            bool load_store_element(
                const std::vector<std::byte> &trie_id,
                const std::vector<std::byte> &key,
                store_value_type &value) override;

            /**
            Save a store element to storage
            */
            void save_store_element(
                const std::vector<std::byte> &trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value) override;

            /**
            Flush changes if appropriate
            */
            void flush(const std::vector<std::byte> &trie_id) override;

            /**
            Add an existing node to the current storage.
            */
            void add_ctnode(const std::vector<std::byte> &trie_id, const CTNode &node) override;

            /**
            Add an existing compresssed trie to the current storage.
            */
            void add_compressed_trie(const CompressedTrie &trie) override;

            /**
            Add an existing store element to the current storage
            */
            void add_store_element(
                const std::vector<std::byte> &trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value) override;

            /**
            Get the latest epoch for the given compressed trie
            */
            std::size_t get_compressed_trie_epoch(const std::vector<std::byte> &trie_id) override;

            /**
            Load updated elements for the given epoch
            */
            void load_updated_elements(
                std::size_t epoch,
                const std::vector<std::byte> &trie_id,
                Storage *storage) override;

            /**
            Get the count of nodes contained in storage
            */
            std::size_t node_count() const
            {
                return nodes_.size();
            }

            /**
            Get the count of store elements contained in storage
            */
            std::size_t store_element_count() const
            {
                return store_.size();
            }

            /**
            Get the count of compressed tries contained in storage
            */
            std::size_t trie_count() const
            {
                return tries_.size();
            }

            /**
            Get the count of OZKS instances contained in storage
            */
            std::size_t ozks_count() const
            {
                return ozks_.size();
            }

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
