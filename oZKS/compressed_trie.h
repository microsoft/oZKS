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
    class CompressedTrie {
    public:
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
        static std::size_t load(CompressedTrie &ct, SerializationReader &reader);

        /**
        Load a Compressed Trie object from the given stream
        */
        static std::size_t load(CompressedTrie &ct, std::istream &stream);

        /**
        Load a Compressed Trie object from the given vector
        */
        template <class T>
        static std::size_t load(
            CompressedTrie &ct, const std::vector<T> &vec, std::size_t position = 0);

        /**
        Clear the contents of this instance.
        Sets epoch back to zero.
        */
        void clear();

    private:
        std::unique_ptr<CTNode> root_;
        std::size_t epoch_;

        bool lookup(const label_type &label, lookup_path_type &path, bool include_searched) const;

        std::size_t get_node_count() const;
        void get_node_count(const CTNode *node, std::size_t &node_count) const;

        std::size_t save_tree(const CTNode *node, SerializationWriter &writer) const;
        static std::size_t load_tree(
            CTNode &node, std::size_t &node_count, SerializationReader &reader);
    };
} // namespace ozks
