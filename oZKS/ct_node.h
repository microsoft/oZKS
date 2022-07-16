// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STL
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

// OZKS
#include "oZKS/defines.h"
#include "oZKS/serialization_helpers.h"

namespace ozks {
    class CompressedTrie;

    class CTNode {
    public:
        CTNode(const CompressedTrie *trie = nullptr);

        CTNode(const CTNode &node);

        /**
        Whether the node is empty (no label)
        */
        bool is_empty() const
        {
            return label.empty();
        }

        /**
        Whether this is a leaf node
        */
        bool is_leaf() const
        {
            return left.empty() && right.empty();
        }

        /**
        Insert the given label and payload under this node.
        */
        partial_label_type insert(
            const partial_label_type &insert_label,
            const payload_type &insert_payload,
            std::size_t epoch);

        /**
        Lookup a given label and return the path to it (including its sibling) if found.
        */
        bool lookup(
            const partial_label_type &lookup_label,
            lookup_path_type &path,
            bool include_searched = true);

        /**
        Update hashes on the path of the given label
        */
        void update_hashes(const partial_label_type &label);

        /**
        Returns a string representation of this node.
        */
        std::string to_string(bool include_payload = false) const;

        /**
        Assignment operator
        */
        CTNode &operator=(const CTNode &node);

        /**
        Left child
        */
        partial_label_type left;

        /**
        Right child
        */
        partial_label_type right;

        /**
        Node label
        */
        partial_label_type label;

        /**
        Node payload
        */
        payload_type payload;

        /**
        Node hash
        */
        hash_type hash() const
        {
            if (is_dirty_) {
                throw std::runtime_error("Tried to obtain hash of a node that needs to be updated");
            }

            return hash_;
        }

        /**
        Save this node to a stream
        */
        std::size_t save(std::ostream &stream) const;

        /**
        Save this node to a vector
        */
        template <class T>
        std::size_t save(std::vector<T> &vec) const;

        /**
        Save this node to a serialization writer
        */
        std::size_t save(SerializationWriter &writer) const;

        /**
        Load a node from a serialization reader
        */
        static auto Load(SerializationReader &reader) -> std::tuple<CTNode, partial_label_type, partial_label_type, std::size_t>;

        /**
        Load a node from a stream
        */
        static auto Load(std::istream &stream) -> std::tuple<CTNode, partial_label_type, partial_label_type, std::size_t>;

        /**
        Load a node from a vector
        */
        template <class T>
        static auto Load(const std::vector<T> &vec, std::size_t position = 0) -> std::tuple<CTNode, partial_label_type, partial_label_type, std::size_t>;

        /**
        Load left node from storage
        */
        bool load_left(CTNode &node) const;

        /**
        Load right node from storage
        */
        bool load_right(CTNode &node) const;

        /**
        Load a node from storage
        */
        bool load(const partial_label_type label, CTNode &node) const;

        /**
        Save a node to storage
        */
        void save() const;

        /**
        Update the hash of the current node
        */
        bool update_hash();

    private:
        hash_type hash_ = {};
        bool is_dirty_ = false;

        /**
        Non-owning pointer to the owning CompressedTrie
        */
        const CompressedTrie *trie_;

        /**
        Initialize node with given label and payload.
        Will compute and update the hash for the node.
        */
        void init(
            const partial_label_type &init_label,
            const payload_type &init_payload,
            std::size_t epoch);

        /**
        Initialize node with given label, payload and hash.
        */
        void init(
            const partial_label_type &init_label,
            const payload_type &init_payload,
            const hash_type &init_hash);

        /**
        Initialize node with given label.
        Payload is cleared. Hash is updated.
        */
        void init(const partial_label_type &init_label);

        /**
        Initialize a node with the given Trie pointer
        */
        void init(const CompressedTrie *trie);

        bool lookup(
            const partial_label_type &lookup_label,
            lookup_path_type &path,
            bool include_searched,
            bool update_hashes);
    };
} // namespace ozks
