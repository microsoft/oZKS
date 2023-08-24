// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <unordered_map>
#include <vector>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node_stored.h"
#include "oZKS/storage/memory_storage_helpers.h"
#include "oZKS/storage/storage.h"
#include "oZKS/utilities.h"

namespace ozks {
    namespace storage {
        class MemoryStorage : public Storage {
        public:
            MemoryStorage();
            virtual ~MemoryStorage();

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

            ///**
            //Get the count of OZKS instances contained in storage
            //*/
            //std::size_t ozks_count() const
            //{
            //    return ozks_.size();
            //}

        private:
            std::unordered_map<StorageNodeKey, ozks::CTNodeStored, StorageNodeKeyHasher> nodes_;
            std::unordered_map<StorageTrieKey, ozks::CompressedTrie, StorageTrieKeyHasher> tries_;
//            std::unordered_map<StorageOZKSKey, ozks::OZKS, StorageOZKSKeyHasher> ozks_;
            std::unordered_map<
                StorageStoreElementKey,
                ozks::store_value_type,
                StorageStoreElementKeyHasher>
                store_;
        };
    } // namespace storage
} // namespace ozks
