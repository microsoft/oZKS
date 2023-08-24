// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STL
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

// OZKS
#include "oZKS/defines.h"
#include "oZKS/partial_label.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/storage/storage.h"

class Utilities_InsertionThreadLimitTest_Test;

namespace ozks {
    class CompressedTrie;

    class CTNode {
    public:
        /**
        Constuctor
        */
        CTNode(const CompressedTrie *trie = nullptr);

        /**
        Constructor
        */
        CTNode(const CTNode &node);

        /**
        Destructor
        */
        virtual ~CTNode()
        {}

        /**
        Whether the node is empty (no label)
        */
        bool is_empty() const
        {
            return label_.empty();
        }

        /**
        Whether this is a leaf node
        */
        virtual bool is_leaf() const = 0;

        /**
        Whether this is a root node
        */
        bool is_root() const
        {
            return is_empty();
        }

        /**
        Insert the given label and payload (commitment) under this node.
        */
        const PartialLabel &insert(
            const PartialLabel &insert_label,
            const hash_type &insert_hash,
            std::size_t epoch,
            std::unordered_map<PartialLabel, std::shared_ptr<CTNode>> *updated_nodes = nullptr);

        /**
        Lookup a given label and return the path to it (including its sibling) if found.
        */
        bool lookup(
            const PartialLabel &lookup_label, lookup_path_type &path, bool include_searched = true);

        /**
        Lookup a given label and return the path to id (including its sibling) if found.
        */
        static bool lookup(
            const PartialLabel &lookup_label,
            const std::shared_ptr<CTNode> root,
            lookup_path_type &path,
            bool include_searched);

        /**
        Update hashes on the path of the given label
        */
        void update_hashes(
            const PartialLabel &label,
            std::size_t root_levels = 0,
            std::unordered_map<PartialLabel, std::shared_ptr<CTNode>> *updated_nodes = nullptr);

        /**
        Returns a string representation of this node.
        */
        std::string to_string() const;

        /**
        Assignment operator
        */
        CTNode &operator=(const CTNode &node);

        /**
        Left child
        */
        virtual std::shared_ptr<CTNode> left() = 0;

        /**
        Left child
        */
        virtual std::shared_ptr<const CTNode> left() const = 0;

        /**
        Right child
        */
        virtual std::shared_ptr<CTNode> right() = 0;

        /**
        Right child
        */
        virtual std::shared_ptr<const CTNode> right() const = 0;

        /**
        Node label
        */
        const PartialLabel &label() const
        {
            return label_;
        }

        /**
        Node hash
        */
        inline hash_type hash() const
        {
            hash_type ret = hash_;
            ret[0] &= std::byte{ 0xFE };
            return ret;
        }

        /**
        Save a node to storage
        */
        virtual void save_to_storage(
            std::unordered_map<PartialLabel, std::shared_ptr<CTNode>> *updated_nodes = nullptr) = 0;

        /**
        Update the hash of the current node
        */
        bool update_hash(std::size_t level = 0, std::size_t root_levels = 0);

        /**
        Load this node from storage, as well as its children
        */
        virtual void load_from_storage(
            std::shared_ptr<storage::Storage> /* storage */,
            const PartialLabel & /* left */,
            const PartialLabel & /* right */)
        {}

        /**
        Initialize a node with the given Trie pointer
        */
        void init(const CompressedTrie *trie);

        /**
        Whether the node supports parallelization
        */
        virtual bool parallelizable() const
        {
            return false;
        }

        friend class ::Utilities_InsertionThreadLimitTest_Test;

    private:
        PartialLabel label_ = {};

        bool lookup(
            const PartialLabel &lookup_label,
            lookup_path_type &path,
            bool include_searched,
            bool update_hashes,
            std::size_t level,
            std::size_t root_levels,
            std::unordered_map<PartialLabel, std::shared_ptr<CTNode>> *updated_nodes = nullptr);

    protected:
        hash_type hash_ = {};

        /**
        Non-owning pointer to the owning CompressedTrie
        */
        const CompressedTrie *trie_;

        /**
        Initialize node with given label and payload.
        Will compute and update the hash for the node.
        */
        void init(const PartialLabel &init_label, const hash_type &init_hash, std::size_t epoch);

        /**
        Initialize node with given label, payload and hash.
        */
        void init(const PartialLabel &init_label, const hash_type &init_hash);

        /**
        Initialize node with given label.
        Payload is cleared. Hash is updated.
        */
        void init(const PartialLabel &init_label);

        /**
        Return whether the node is marked as dirty.
        */
        inline bool get_dirty_bit() const
        {
            return static_cast<bool>(hash_[0] & std::byte{ 1 });
        }

        /**
        Set dirty bit.
        */
        inline void set_dirty_bit(bool dirty)
        {
            hash_[0] &= std::byte{ 0xFE };
            hash_[0] |= static_cast<std::byte>(dirty);
        }

        virtual void set_left_node(std::shared_ptr<CTNode> new_left_node) = 0;
        virtual void set_left_node(const PartialLabel &label) = 0;
        virtual std::shared_ptr<CTNode> set_new_left_node(const PartialLabel &label) = 0;
        virtual std::shared_ptr<CTNode> set_left_node(
            const PartialLabel &label, const hash_type &hash, std::size_t epoch) = 0;
        virtual std::shared_ptr<CTNode> set_left_node(
            const PartialLabel &label, const hash_type &hash) = 0;
        virtual void set_right_node(std::shared_ptr<CTNode> new_right_node) = 0;
        virtual void set_right_node(const PartialLabel &label) = 0;
        virtual std::shared_ptr<CTNode> set_new_right_node(const PartialLabel &label) = 0;
        virtual std::shared_ptr<CTNode> set_right_node(
            const PartialLabel &label, const hash_type &hash, std::size_t epoch) = 0;
        virtual std::shared_ptr<CTNode> set_right_node(
            const PartialLabel &label, const hash_type &hash) = 0;
    };
} // namespace ozks
