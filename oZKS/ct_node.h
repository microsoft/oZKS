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
    class CTNode {
    public:
        CTNode();

        CTNode(const CTNode &node);

        /**
        Whether the node is empty (no label)
        */
        bool is_empty() const
        {
            return label.size() == 0;
        }

        /**
        Whether this is a leaf node
        */
        bool is_leaf() const
        {
            return left == nullptr && right == nullptr;
        }

        /**
        Insert the given label and payload under this node.
        */
        void insert(
            const partial_label_type &insert_label,
            const payload_type &insert_payload,
            const std::size_t epoch);

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
        std::unique_ptr<CTNode> left;

        /**
        Right child
        */
        std::unique_ptr<CTNode> right;

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
        static std::tuple<CTNode, partial_label_type, partial_label_type, std::size_t> load(
            SerializationReader &reader);

        /**
        Load a node from a stream
        */
        static std::tuple<CTNode, partial_label_type, partial_label_type, std::size_t> load(
            std::istream &stream);

        /**
        Load a node from a vector
        */
        template <class T>
        static std::tuple<CTNode, partial_label_type, partial_label_type, std::size_t> load(
            const std::vector<T> &vec, std::size_t position = 0);

        /**
        Update the hash of the current node
        */
        bool update_hash();

    private:
        hash_type hash_ = {};
        bool is_dirty_ = false;

        /**
        Initialize node with given label and payload.
        Will compute and update the hash for the node.
        */
        void init(
            const partial_label_type &init_label,
            const payload_type &init_payload,
            const std::size_t epoch);

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

        bool lookup(
            const partial_label_type &lookup_label,
            lookup_path_type &path,
            bool include_searched,
            bool update_hashes);
    };
} // namespace ozks
