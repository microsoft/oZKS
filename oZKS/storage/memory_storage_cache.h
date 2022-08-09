// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>
#include <memory>

// Poco
#include "Poco/LRUCache.h"

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node.h"
#include "oZKS/defines.h"
#include "oZKS/ozks.h"
#include "oZKS/storage/memory_storage_helpers.h"
#include "oZKS/storage/storage.h"

namespace ozks {
    namespace storage {
        class MemoryStorageCache : public Storage {
        public:
            MemoryStorageCache(
                std::shared_ptr<ozks::storage::Storage> backing_storage, std::size_t cache_size)
                : storage_(backing_storage), node_cache_(cache_size), trie_cache_(cache_size),
                  ozks_cache_(cache_size), store_element_cache_(cache_size)
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

        private:
            std::shared_ptr<ozks::storage::Storage> storage_;
            Poco::LRUCache<StorageNodeKey, CTNode> node_cache_;
            Poco::LRUCache<StorageTrieKey, CompressedTrie> trie_cache_;
            Poco::LRUCache<StorageOZKSKey, OZKS> ozks_cache_;
            Poco::LRUCache<StorageStoreElementKey, store_value_type> store_element_cache_;
        };
    } // namespace storage
} // namespace ozks
