// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>
#include <iostream>
#include <memory>

// OZKS
#include "oZKS/ct_node.h"
#include "oZKS/defines.h"
#include "oZKS/serialization_helpers.h"

namespace ozks {
    namespace storage {
        class Storage;
    }

    using partial_label_hash_batch_type = std::vector<std::pair<PartialLabel, hash_type>>;

    class CompressedTrie {
    public:
        /**
        Constructor
        */
        CompressedTrie(
            std::shared_ptr<ozks::storage::Storage> storage,
            TrieType trie_type,
            std::size_t thread_count = 0);

        /**
        Constructor
        */
        CompressedTrie(
            trie_id_type trie_id,
            std::shared_ptr<ozks::storage::Storage> storage,
            TrieType trie_type,
            std::size_t thread_count = 0);

        /**
        Constructor
        */
        CompressedTrie();

        /**
        Destructor
        */
        ~CompressedTrie() = default;

        /**
        Insert a single label into the tree. Increments the epoch and computes updated hashes.
        */
        void insert(
            const PartialLabel &label,
            const hash_type &payload_commit,
            append_proof_type &append_proof);

        /**
        Insert a batch of labels and payloads into the tree. Increments the epoch and computes
        updated hashes. Size of labels needs to match size of payloads.
        */
        void insert(
            const partial_label_hash_batch_type &label_commit_batch,
            append_proof_batch_type &append_proofs);

        /**
        Returns whether the given label exists in the tree. If it does, gets the path of the label,
        including its sibling node (if any)
        */
        bool lookup(const PartialLabel &label, lookup_path_type &path) const;

        /**
        Get the commitment (root hash) for the tree
        */
        commitment_type get_commitment() const;

        /**
        Get the current epoch
        */
        std::size_t epoch() const
        {
            return epoch_;
        }

        /**
        Get the current id
        */
        trie_id_type id() const
        {
            return id_;
        }

        /**
        Get the current Storage
        */
        std::shared_ptr<ozks::storage::Storage> storage() const
        {
            return storage_;
        }

        /**
        Get the Trie Type
        */
        TrieType trie_type() const
        {
            return trie_type_;
        }

        /**
        Return a string representation of the tree
        */
        std::string to_string() const;

        /**
        Save the current compressed trie to the given stream
        */
        std::size_t save(std::ostream &stream) const;

        /**
        Save the current compressed trie to the given vector
        */
        template <typename T>
        std::size_t save(std::vector<T> &vec) const;

        /**
        Load a Compressed Trie object from the given stream
        */
        static std::pair<std::shared_ptr<CompressedTrie>, std::size_t> Load(
            std::istream &stream, std::shared_ptr<storage::Storage> storage);

        /**
        Load a Compressed Trie object from the given vector
        */
        template <typename T>
        static std::pair<std::shared_ptr<CompressedTrie>, std::size_t> Load(
            const std::vector<T> &vec,
            std::shared_ptr<storage::Storage> storage,
            std::size_t position = 0);

        /**
        Save the current compressed trie to Storage
        */
        void save_to_storage() const;

        /**
        Load a compressed trie from Storage
        */
        static std::pair<std::shared_ptr<CompressedTrie>, bool> LoadFromStorage(
            trie_id_type trie_id, std::shared_ptr<ozks::storage::Storage> storage);

        /**
        Load a compressed trie and all its children nodes from storage
        */
        static std::pair<std::shared_ptr<CompressedTrie>, bool> LoadFromStorageWithChildren(
            trie_id_type trie_id, std::shared_ptr<ozks::storage::Storage> storage);

        /**
        Initialize storage for this CompressedTrie instance
        */
        void init(std::shared_ptr<ozks::storage::Storage> storage);

        /**
        Initialize the id for this CompressedTrie instance
        */
        void init(trie_id_type trie_id)
        {
            id_ = trie_id;
        }

    private:
        std::shared_ptr<CTNode> root_;

        std::size_t epoch_;
        trie_id_type id_;
        std::shared_ptr<ozks::storage::Storage> storage_;
        std::size_t thread_count_;
        TrieType trie_type_;

        bool lookup(const PartialLabel &label, lookup_path_type &path, bool include_searched) const;

        void init_random_id();

        void init(std::shared_ptr<CTNode> root);
        void init_empty_root();

        std::size_t save(SerializationWriter &writer) const;
        static std::pair<std::shared_ptr<CompressedTrie>, std::size_t> Load(
            SerializationReader &reader, std::shared_ptr<storage::Storage> storage);
    };
} // namespace ozks
