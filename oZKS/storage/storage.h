// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

// OZKS
#include "oZKS/defines.h"
#include "oZKS/partial_label.h"

namespace ozks {
    class CTNodeStored;
    class CompressedTrie;

    namespace storage {
        class Storage {
        public:
            virtual ~Storage()
            {}

            /**
            Get a node from storage.
            The implementation has the possibility of deciding to load a batch of nodes related to
            the given node. If that is the case, additional nodes will be added to the Storage
            instance given as parameter.
            The storage pointer can be null. In such a case no node addition will happen.
            */
            virtual bool load_ctnode(
                trie_id_type trie_id,
                const PartialLabel &node_id,
                std::shared_ptr<Storage> storage,
                CTNodeStored &node) = 0;

            /**
            Get a node from storage from a specific epoch.
            The implementation has the possibility of deciding to load a batch of nodes related to
            the given node. If that is the case, additional nodes will be added to the Storage
            instance given as parameter.
            The storage pointer can be null. In such a case no node addition will happen.
            */
            virtual bool load_ctnode(
                trie_id_type trie_id,
                const PartialLabel &node_id,
                std::size_t epoch,
                std::shared_ptr<Storage> storage,
                CTNodeStored &node) = 0;

            /**
            Load a batch of nodes from storage in a single call.
            out_nodes is resized to label_epoch_pairs.size() on success and indexed identically
            to the input vector. Throws std::runtime_error with a message identifying any
            missing (label, epoch) entries.
            */
            virtual bool load_ctnodes(
                trie_id_type trie_id,
                const std::vector<std::pair<PartialLabel, std::size_t>> &label_epoch_pairs,
                std::vector<CTNodeStored> &out_nodes) = 0;

            /**
            Save a node to storage
            */
            virtual void save_ctnode(trie_id_type trie_id, const CTNodeStored &node) = 0;

            /**
            Get a compressed trie from storage
            */
            virtual bool load_compressed_trie(trie_id_type trie_id, CompressedTrie &trie) = 0;

            /**
            Save a compressed trie to storage
            */
            virtual void save_compressed_trie(const CompressedTrie &trie) = 0;

            /**
            Get a store element from storage
            */
            virtual bool load_store_element(
                trie_id_type trie_id,
                const std::vector<std::byte> &key,
                store_value_type &value) = 0;

            /**
            Save a store element to storage
            */
            virtual void save_store_element(
                trie_id_type trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value) = 0;

            /**
            Flush changes if appropriate
            */
            virtual void flush(trie_id_type trie_id) = 0;

            /**
            Add an existing node to the current storage.
            */
            virtual void add_ctnode(trie_id_type trie_id, const CTNodeStored &node) = 0;

            /**
            Add an existing compresssed trie to the current storage.
            */
            virtual void add_compressed_trie(const CompressedTrie &trie) = 0;

            /**
            Add an existing store element to the current storage
            */
            virtual void add_store_element(
                trie_id_type trie_id,
                const std::vector<std::byte> &key,
                const store_value_type &value) = 0;

            /**
            Get the latest epoch for the given compressed trie
            */
            virtual std::size_t get_compressed_trie_epoch(trie_id_type trie_id) = 0;

            /**
            Load updated elements for the given epoch
            */
            virtual void load_updated_elements(
                std::size_t epoch, trie_id_type trie_id, std::shared_ptr<Storage> storage) = 0;

            /**
            Delete nodes for the given trie from storage, as well as the trie itself and related
            OZKS instance. Some storage implementation might not support this operation.
            */
            virtual void delete_ozks(trie_id_type trie_id) = 0;
        };
    } // namespace storage
} // namespace ozks
