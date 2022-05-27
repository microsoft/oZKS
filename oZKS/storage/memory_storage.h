// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <unordered_map>
#include <vector>

// OZKS
#include "oZKS/storage/storage.h"
#include "oZKS/ct_node.h"


namespace ozks {
    class CTNode;

    namespace storage {
        class StorageNodeKey {
        public:
            StorageNodeKey(const std::vector<std::byte> &trie_id, const partial_label_type &node_id)
                : trie_id_(trie_id), node_id_(node_id)
            {}

            const std::vector<std::byte>& trie_id() const
            {
                return trie_id_;
            }

            const partial_label_type& node_id() const
            {
                return node_id_;
            }

            bool operator==(const StorageNodeKey &other) const
            {
                return (trie_id_ == other.trie_id_ && node_id_ == other.node_id_);
            }

        private:
            std::vector<std::byte> trie_id_;
            partial_label_type node_id_;
        };

        struct StorageNodeKeyHasher {
            std::size_t operator()(const StorageNodeKey& key) const
            {
                return std::hash<std::vector<bool>>()(key.node_id());
            }
        };

        class StorageNode {
        public:
            StorageNode(const std::vector<std::byte> &trie_id, const CTNode &node)
                : key_(trie_id, node.label), data_()
            {
                node.save(data_);
            }

            StorageNode() : key_({}, {})
            {
            }

            const StorageNodeKey &key()
            {
                return key_;
            }

            const std::vector<std::byte> &data()
            {
                return data_;
            }

        private:
            StorageNodeKey key_;
            std::vector<std::byte> data_;
        };

        class MemoryStorage : public Storage {
        public:
            MemoryStorage();

            /**
            Get a node from storage
            */
            virtual bool LoadCTNode(
                const std::vector<std::byte> &trie_id,
                const partial_label_type &node_id,
                CTNode &node);

            /**
            Save a node to storage
            */
            virtual void SaveCTNode(
                const std::vector<std::byte> &trie_id, const CTNode &node);

            ///**
            //Get a compressed trie from storage
            //*/
            //virtual std::size_t LoadCompressedTrie(CompressedTrie &trie);

            ///**
            //Save a compressed trie to storage
            //*/
            //virtual std::size_t SaveCompressedTrie(const CompressedTrie &trie);

        private:
            std::unordered_map<StorageNodeKey, StorageNode, StorageNodeKeyHasher> nodes_;
        };
    } // namespace storage
} // namespace ozks
