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

            virtual ~MemoryStorageBatchInserter();

            /**
            Get a node from storage
            */
            bool load_ctnode(
                trie_id_type trie_id,
                const PartialLabel &node_id,
                std::shared_ptr<Storage> storage,
                CTNodeStored &node) override;

            /**
            Save a node to storage
            */
            void save_ctnode(trie_id_type trie_id, const CTNodeStored &node) override;

            /**
            Get a compressed trie from storage
            */
            bool load_compressed_trie(trie_id_type trie_id, CompressedTrie &trie) override;

            /**
            Save a compressed trie to storage
            */
            void save_compressed_trie(const CompressedTrie &trie) override;

            /**
            Get a store element from storage
            */
            bool load_store_element(
                trie_id_type trie_id,
                const std::vector<std::byte> &key,
                store_value_type &value) override;

            /**
            Save a store element to storage
            */
            void save_store_element(
                trie_id_type trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value) override;

            /**
            Flush changes if appropriate
            */
            void flush(trie_id_type trie_id) override;

            /**
            Add an existing node to the current storage.
            */
            void add_ctnode(trie_id_type trie_id, const CTNodeStored &node) override;

            /**
            Add an existing compresssed trie to the current storage.
            */
            void add_compressed_trie(const CompressedTrie &trie) override;

            /**
            Add an existing store element to the current storage
            */
            void add_store_element(
                trie_id_type trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value) override;

            /**
            Get the latest epoch for the given compressed trie
            */
            std::size_t get_compressed_trie_epoch(trie_id_type trie_id) override;

            /**
            Load updated elements for the given epoch
            */
            void load_updated_elements(
                std::size_t epoch, trie_id_type trie_id, std::shared_ptr<Storage> storage) override;

            /**
            Delete nodes for the given trie from storage, as well as the trie itself and related
            ozks instance.
            */
            void delete_ozks(trie_id_type trie_id) override;

        private:
            std::shared_ptr<BatchStorage> storage_;

            std::unordered_map<StorageNodeKey, CTNodeStored, StorageNodeKeyHasher> unsaved_nodes_;
            std::unordered_map<StorageTrieKey, CompressedTrie, StorageTrieKeyHasher> unsaved_tries_;
            std::unordered_map<
                StorageStoreElementKey,
                store_value_type,
                StorageStoreElementKeyHasher>
                unsaved_store_elements_;
        };
    } // namespace storage
} // namespace ozks
