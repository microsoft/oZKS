// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <tuple>

// OZKS
#include "oZKS/defines.h"

namespace ozks {
    class CTNode;
    class CompressedTrie;
    class OZKS;

    namespace storage {
        class Storage {
        public:
            /**
            Get a node from storage
            */
            virtual bool load_ctnode(
                const std::vector<std::byte> &trie_id,
                const partial_label_type &node_id,
                CTNode &node) = 0;

            /**
            Save a node to storage
            */
            virtual void save_ctnode(const std::vector<std::byte> &trie_id, const CTNode &node) = 0;

            /**
            Get a compressed trie from storage
            */
            virtual bool load_compressed_trie(
                const std::vector<std::byte> &trie_id, CompressedTrie &trie) = 0;

            /**
            Save a compressed trie to storage
            */
            virtual void save_compressed_trie(const CompressedTrie &trie) = 0;

            /**
            Get an OZKS instance from storage
            */
            virtual bool load_ozks(const std::vector<std::byte> &trie_id, OZKS &ozks) = 0;

            /**
            Save an OZKS instance to storage
            */
            virtual void save_ozks(const OZKS &ozks) = 0;

            /**
            Get a store element from storage
            */
            virtual bool load_store_element(
                const std::vector<std::byte> &trie_id,
                const std::vector<std::byte> &key,
                store_value_type &value) = 0;

            /**
            Save a store element to storage
            */
            virtual void save_store_element(
                const std::vector<std::byte> &trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value) = 0;

            /**
            Flush changes if appropriate
            */
            virtual void flush(const std::vector<std::byte> &trie_id) = 0;

            /**
            Add an existing node to the current storage.
            */
            virtual void add_ctnode(const std::vector<std::byte> &trie_id, const CTNode &node) = 0;

            /**
            Add an existing compresssed trie to the current storage.
            */
            virtual void add_compressed_trie(const CompressedTrie &trie) = 0;

            /**
            Add an existing store element to the current storage
            */
            virtual void add_store_element(
                const std::vector<std::byte> &trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value) = 0;

            /**
            Get the latest epoch for the given compressed trie
            */
            virtual std::size_t get_compressed_trie_epoch(
                const std::vector<std::byte> &trie_id) = 0;

            /**
            Load updated elements for the given epoch
            */
            virtual void load_updated_elements(
                std::size_t epoch, const std::vector<std::byte> &trie_id, Storage *storage) = 0;

            /**
            Delete nodes for the given trie from storage, as well as the trie itself and related
            ozks instance. Some storage implementation might not support this operation.
            */
            virtual void delete_ozks(const std::vector<std::byte> &trie_id) = 0;
        };
    } // namespace storage
} // namespace ozks
