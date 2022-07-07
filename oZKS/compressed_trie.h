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

    class CompressedTrie {
    public:
        /**
        Constructor
        */
        CompressedTrie(std::shared_ptr<ozks::storage::Storage> storage);

        /**
        Constructor
        */
        CompressedTrie();

        /**
        Insert a single label into the tree. Increments the epoch and computes updated hashes.
        */
        void insert(
            const label_type &label, const payload_type &payload, append_proof_type &append_proof);

        /**
        Insert a batch of labels and payloads into the tree. Increments the epoch and computes
        updated hashes.
        Size of labels needs to match size of payloads.
        */
        void insert(
            const label_payload_batch_type &label_payload_batch,
            append_proof_batch_type &append_proofs);

        /**
        Returns whether the given label exists in the tree. If it does, gets the path of the label,
        including its sibling node (if any)
        */
        bool lookup(const label_type &label, lookup_path_type &path) const;

        /**
        Get the current commitment for the tree
        */
        void get_commitment(commitment_type &commitment) const;

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
        const std::vector<std::byte> id() const
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
        Return a string representation of the tree
        */
        std::string to_string(bool include_payload = false) const;

        /**
        Save the current compressed trie to the given stream
        */
        std::size_t save(std::ostream &stream) const;

        /**
        Save the current compressed trie to the given vector
        */
        template <class T>
        std::size_t save(std::vector<T> &vec) const;

        /**
        Save the current compressed trie to the given serialization writer
        */
        std::size_t save(SerializationWriter &writer) const;

        /**
        Load a Compressed Trie object from the given serialization reader
        */
        static std::size_t Load(CompressedTrie &ct, SerializationReader &reader);

        /**
        Load a Compressed Trie object from the given stream
        */
        static std::size_t Load(CompressedTrie &ct, std::istream &stream);

        /**
        Load a Compressed Trie object from the given vector
        */
        template <class T>
        static std::size_t Load(
            CompressedTrie &ct, const std::vector<T> &vec, std::size_t position = 0);

        /**
        Save the current compressed trie to Storage
        */
        void save() const;

        /**
        Load a compressed trie from Storage
        */
        static bool Load(
            const std::vector<std::byte> &trie_id,
            std::shared_ptr<ozks::storage::Storage> storage,
            CompressedTrie &trie);

        /**
        Clear the contents of this instance.
        Sets epoch back to zero.
        */
        void clear();

    private:
        partial_label_type root_;
        std::size_t epoch_;
        std::vector<std::byte> id_;
        std::shared_ptr<ozks::storage::Storage> storage_;

        bool lookup(const label_type &label, lookup_path_type &path, bool include_searched) const;

        void init_random_id();

        CTNode load_root() const;

        void init(std::shared_ptr<ozks::storage::Storage> storage);
    };
} // namespace ozks
