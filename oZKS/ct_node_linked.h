// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STL
#include <memory>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node.h"

namespace ozks {
    class CTNodeLinked : public CTNode {
    public:
        CTNodeLinked(const CompressedTrie *trie = nullptr, std::size_t epoch = invalid_epoch) : CTNode(trie, epoch)
        {}

        CTNodeLinked(
            const CompressedTrie *trie,
            const PartialLabel &label,
            const hash_type &hash,
            std::size_t epoch)
            : CTNode(trie)
        {
            init(label, hash, epoch);
        }

        CTNodeLinked(const CompressedTrie *trie, const PartialLabel &label, const hash_type &hash)
            : CTNode(trie)
        {
            init(label, hash);
        }

        CTNodeLinked(const CompressedTrie *trie, const PartialLabel &label) : CTNode(trie)
        {
            if (nullptr == trie) {
                throw std::invalid_argument("Trie cannot be null");
            }
            
            init(label, trie->epoch());
        }

        /**
        Destructor
        */
        virtual ~CTNodeLinked()
        {}

        /**
        Whether this is a leaf node
        */
        bool is_leaf() const override;

        /**
        Left child
        */
        std::shared_ptr<CTNode> left() override
        {
            return left_;
        }

        /**
        Left child
        */
        std::shared_ptr<const CTNode> left() const override
        {
            return left_;
        }

        /**
        Right child
        */
        std::shared_ptr<CTNode> right() override
        {
            return right_;
        }

        /**
        Right child
        */
        std::shared_ptr<const CTNode> right() const override
        {
            return right_;
        }

        /**
        Save a node to storage
        */
        void save_to_storage(
            std::unordered_map<PartialLabel, std::shared_ptr<CTNode>> *updated_nodes =
                nullptr) override;

        /**
        Load this node from storage, as well as its children
        */
        void load_from_storage(
            std::shared_ptr<storage::Storage> storage,
            const PartialLabel &left,
            std::size_t left_epoch,
            const PartialLabel &right,
            std::size_t right_epoch) override;

        /**
        Whether the node supports parallelization
        */
        bool parallelizable() const override
        {
            return true;
        }

    private:
        std::shared_ptr<CTNode> left_;
        std::shared_ptr<CTNode> right_;

        /**
        Set the left child while loading from storage. Unlike set_left_node, this does not
        advance this node's epoch or flip the dirty bit, and it records the supplied
        child epoch rather than reading it from the (not-yet-populated) child.
        */
        void set_left_node_for_load(std::shared_ptr<CTNode> new_left_node, std::size_t child_epoch);

        /**
        Set the right child while loading from storage. Same contract as set_left_node_for_load.
        */
        void set_right_node_for_load(std::shared_ptr<CTNode> new_right_node, std::size_t child_epoch);

    protected:
        void set_left_node(std::shared_ptr<CTNode> new_left_node) override;
        void set_left_node(const PartialLabel &label, std::size_t epoch) override;
        std::shared_ptr<CTNode> set_new_left_node(const PartialLabel &label, std::size_t epoch) override;

        /**
        Set left node of the current node.
        This assumes the left node will be a leaf node. The label, payload  and epoch
        will be used to compute the actual leaf node hash.
        */
        std::shared_ptr<CTNode> set_left_node(
            const PartialLabel &label, const hash_type &payload, std::size_t epoch) override;

        /**
        Set left node of the current node.
        This assumes the left node will be a leaf node. The hash will be set in directly.
        */
        std::shared_ptr<CTNode> set_existing_left_node(
            const PartialLabel &label, const hash_type &hash, std::size_t epoch) override;

        void set_right_node(std::shared_ptr<CTNode> new_right_node) override;
        void set_right_node(const PartialLabel &label, std::size_t epoch) override;
        std::shared_ptr<CTNode> set_new_right_node(const PartialLabel &label, std::size_t epoch) override;

        /**
        Set right node of the current node.
        This assumes the right node will be a leaf node. The label, payload  and epoch
        will be used to compute the actual leaf node hash.
        */
        std::shared_ptr<CTNode> set_right_node(
            const PartialLabel &label, const hash_type &payload, std::size_t epoch) override;

        /**
        Set right node of the current node.
        This assumes the right node will be a leaf node. The hash will be set in directly.
        */
        std::shared_ptr<CTNode> set_existing_right_node(
            const PartialLabel &label, const hash_type &hash, std::size_t epoch) override;
    };
} // namespace ozks
