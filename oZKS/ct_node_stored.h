// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STL

// OZKS
#include "oZKS/ct_node.h"

class CTNodeTests_StoredSaveLoadTest_Test;
class CTNodeTests_StoredSaveLoadToVectorTest_Test;

namespace ozks {
    class CTNodeStored : public CTNode {
    public:
        CTNodeStored(const CompressedTrie *trie = nullptr) : CTNode(trie)
        {}

        CTNodeStored(
            const CompressedTrie *trie,
            const PartialLabel &label,
            const hash_type &hash,
            std::size_t epoch)
            : CTNode(trie)
        {
            init(label, hash, epoch);
        }

        CTNodeStored(const CompressedTrie *trie, const PartialLabel &label, const hash_type &hash)
            : CTNode(trie)
        {
            init(label, hash);
        }

        CTNodeStored(
            const CompressedTrie *trie,
            const PartialLabel &label,
            const hash_type &hash,
            const PartialLabel &left_label,
            const PartialLabel &right_label)
            : CTNodeStored(trie, label, hash)
        {
            set_left_node(left_label);
            set_right_node(right_label);
            set_dirty_bit(false);
        }

        CTNodeStored(const CompressedTrie *trie, const PartialLabel &label) : CTNode(trie)
        {
            init(label);
        }

        /**
        Destructor
        */
        virtual ~CTNodeStored()
        {}

        /**
        Whether this is a leaf node
        */
        bool is_leaf() const override;

        /**
        Left child
        */
        std::shared_ptr<CTNode> left() override;

        /**
        Left child
        */
        std::shared_ptr<const CTNode> left() const override;

        /**
        Right child
        */
        std::shared_ptr<CTNode> right() override;

        /**
        Right child
        */
        std::shared_ptr<const CTNode> right() const override;

        /**
        Left label
        */
        const PartialLabel &left_label() const
        {
            return left_;
        }

        /**
        Right label
        */
        const PartialLabel &right_label() const
        {
            return right_;
        }

        /**
        Assignment operator
        */
        CTNodeStored &operator=(const CTNodeStored &node);

        /**
        Save this node to a stream
        */
        std::size_t save(std::ostream &stream) const;

        /**
        Save this node to a vector
        */
        template <typename T>
        std::size_t save(std::vector<T> &vec) const;

        /**
        Load a node from a stream
        */
        static auto Load(std::istream &stream, const CompressedTrie *trie)
            -> std::tuple<CTNodeStored, PartialLabel, PartialLabel, std::size_t>;

        /**
        Load a node from a vector
        */
        template <typename T>
        static auto Load(
            const std::vector<T> &vec, const CompressedTrie *trie, std::size_t position = 0)
            -> std::tuple<CTNodeStored, PartialLabel, PartialLabel, std::size_t>;

        /**
        Save a node to storage
         */
        void save_to_storage(
            std::unordered_map<PartialLabel, std::shared_ptr<CTNode>> *updated_nodes =
                nullptr) override;

        friend class ::CTNodeTests_StoredSaveLoadTest_Test;
        friend class ::CTNodeTests_StoredSaveLoadToVectorTest_Test;

    private:
        PartialLabel left_;
        PartialLabel right_;

        /**
        Save this node to a serialization writer
        */
        std::size_t save(SerializationWriter &writer) const;

        /**
        Load a node from a serialization reader
        */
        static auto Load(SerializationReader &reader, const CompressedTrie *trie)
            -> std::tuple<CTNodeStored, PartialLabel, PartialLabel, std::size_t>;

    protected:
        void set_left_node(std::shared_ptr<CTNode> new_left_node) override;
        void set_left_node(const PartialLabel &label) override;
        std::shared_ptr<CTNode> set_new_left_node(const PartialLabel &label) override;
        std::shared_ptr<CTNode> set_left_node(
            const PartialLabel &label, const hash_type &hash, std::size_t epoch) override;
        std::shared_ptr<CTNode> set_left_node(
            const PartialLabel &label, const hash_type &hash) override;
        void set_right_node(std::shared_ptr<CTNode> new_right_node) override;
        void set_right_node(const PartialLabel &label) override;
        std::shared_ptr<CTNode> set_new_right_node(const PartialLabel &label) override;
        std::shared_ptr<CTNode> set_right_node(
            const PartialLabel &label, const hash_type &hash, std::size_t epoch) override;
        std::shared_ptr<CTNode> set_right_node(
            const PartialLabel &label, const hash_type &hash) override;
    };
} // namespace ozks
