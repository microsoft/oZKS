// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <vector>

// OZKS
#include "oZKS/utilities.h"

namespace ozks {
    namespace storage {
        class StorageNodeKey {
        public:
            StorageNodeKey(trie_id_type trie_id, const PartialLabel &node_id)
                : trie_id_(trie_id), node_id_(node_id)
            {}

            trie_id_type trie_id() const
            {
                return trie_id_;
            }

            const PartialLabel &node_id() const
            {
                return node_id_;
            }

            bool operator==(const StorageNodeKey &other) const
            {
                return (trie_id_ == other.trie_id_ && node_id_ == other.node_id_);
            }

            bool operator<(const StorageNodeKey &other) const
            {
                if (trie_id_ == other.trie_id_)
                    return node_id_ < other.node_id_;

                return trie_id_ < other.trie_id_;
            }

        private:
            trie_id_type trie_id_;
            PartialLabel node_id_;
        };

        class StorageTrieKey {
        public:
            StorageTrieKey(trie_id_type trie_id) : trie_id_(trie_id)
            {}

            trie_id_type trie_id() const
            {
                return trie_id_;
            }

            bool operator==(const StorageTrieKey &other) const
            {
                return trie_id_ == other.trie_id_;
            }

            bool operator<(const StorageTrieKey &other) const
            {
                return trie_id_ < other.trie_id_;
            }

        private:
            trie_id_type trie_id_;
        };

        class StorageStoreElementKey {
        public:
            StorageStoreElementKey(trie_id_type trie_id, const std::vector<std::byte> &key)
                : trie_id_(trie_id), key_(key)
            {}

            trie_id_type trie_id() const
            {
                return trie_id_;
            }

            const std::vector<std::byte> &key() const
            {
                return key_;
            }

            bool operator==(const StorageStoreElementKey &other) const
            {
                return (trie_id_ == other.trie_id_ && key_ == other.key_);
            }

            bool operator<(const StorageStoreElementKey &other) const
            {
                if (trie_id_ == other.trie_id_)
                    return key_ < other.key_;

                return trie_id_ < other.trie_id_;
            }

        private:
            trie_id_type trie_id_;
            std::vector<std::byte> key_;
        };

        struct StorageNodeKeyHasher {
            std::size_t operator()(const StorageNodeKey &key) const
            {
                return std::hash<ozks::PartialLabel>()(key.node_id());
            }
        };

        struct StorageTrieKeyHasher {
            std::size_t operator()(const StorageTrieKey &key) const
            {
                return static_cast<std::size_t>(key.trie_id());
            }
        };

        struct StorageStoreElementKeyHasher {
            std::size_t operator()(const StorageStoreElementKey &key) const
            {
                utils::byte_vector_hash hasher;
                return hasher(key.key());
            }
        };
    } // namespace storage
} // namespace ozks
