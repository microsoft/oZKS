// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <unordered_map>
#include <vector>

// OZKS
#include "oZKS/storage/storage.h"
#include "oZKS/ct_node.h"
#include "oZKS/compressed_trie.h"


namespace ozks {
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

        class StorageTrieKey {
        public:
            StorageTrieKey(const std::vector<std::byte> &trie_id) : trie_id_(trie_id)
            {}

            const std::vector<std::byte>& trie_id() const
            {
                return trie_id_;
            }

            bool operator==(const StorageTrieKey& other) const
            {
                return trie_id_ == other.trie_id_;
            }

        private:
            std::vector<std::byte> trie_id_;
        };

        struct StorageTrieKeyHasher {
            std::size_t operator()(const StorageTrieKey& key) const
            {
                std::size_t result = 0;
                for (auto keyb : key.trie_id()) {
                    auto keybch = static_cast<unsigned char>(keyb);
                    result = std::hash<unsigned char>()(keybch) ^ (result << 1);
                }

                return result;
            }
        };

        class StorageNode {
        public:
            StorageNode(const CTNode &node)
                : data_()
            {
                node.save(data_);
            }

            StorageNode()
            {}

            const std::vector<std::byte> &data() const
            {
                return data_;
            }

        private:
            std::vector<std::byte> data_;
        };

        class StorageTrie {
        public:
            StorageTrie(const CompressedTrie &trie) : data_()
            {
                trie.save(data_);
            }

            StorageTrie()
            {}

            const std::vector<std::byte>& data() const
            {
                return data_;
            }

        private:
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

            /**
            Get a compressed trie from storage
            */
            virtual bool LoadCompressedTrie(
                const std::vector<std::byte> &trie_id, CompressedTrie &trie);

            /**
            Save a compressed trie to storage
            */
            virtual void SaveCompressedTrie(const CompressedTrie &trie);

        private:
            std::unordered_map<StorageNodeKey, StorageNode, StorageNodeKeyHasher> nodes_;
            std::unordered_map<StorageTrieKey, StorageTrie, StorageTrieKeyHasher> tries_;
        };
    } // namespace storage
} // namespace ozks
