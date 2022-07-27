// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <memory>
#include <unordered_map>

// OZKS
#include "oZKS/storage/batch_storage.h"
#include "oZKS/storage/memory_storage_helpers.h"

namespace ozks {
    namespace storage {
        class MemoryStorageBatchInserter : public Storage {
        public:
            MemoryStorageBatchInserter(std::shared_ptr<BatchStorage> backing_storage)
                : storage_(backing_storage)
            {}

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
            Add an existing store element to the current storage
            */
            void add_store_element(
                const std::vector<std::byte> &trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value) override;

        private:
            std::shared_ptr<BatchStorage> storage_;

            std::unordered_map<StorageNodeKey, CTNode, StorageNodeKeyHasher> unsaved_nodes_;
            std::unordered_map<StorageTrieKey, CompressedTrie, StorageTrieKeyHasher> unsaved_tries_;
            std::unordered_map<StorageOZKSKey, OZKS, StorageOZKSKeyHasher> unsaved_ozks_;
            std::unordered_map<
                StorageStoreElementKey,
                store_value_type,
                StorageStoreElementKeyHasher>
                unsaved_store_elements_;
        };
    } // namespace storage
} // namespace ozks
