//// Copyright (c) Microsoft Corporation. All rights reserved.
//// Licensed under the MIT license.
//
//#pragma once
//
//// STL
//#include <memory>
//
//// OZKS
//#include "oZKS/defines.h"
//#include "oZKS/storage/storage.h"
//
//namespace ozks {
//    class CTSNode {
//    public:
//        CTSNode(std::shared_ptr<storage::Storage> storage);
//        CTSNode(const CTSNode &other);
//
//        partial_label_type label;
//        partial_label_type left;
//        partial_label_type right;
//        payload_type payload;
//
//        const hash_type &hash() const
//        {
//            if (is_dirty_) {
//                throw std::runtime_error("Tried to get hash of a dirty node");
//            }
//            return hash_;
//        }
//
//        /**
//        Whether the node is empty (no label)
//        */
//        bool is_empty() const
//        {
//            return label.empty();
//        }
//
//        /**
//        Whether this is a leaf node
//        */
//        bool is_leaf() const
//        {
//            return left.empty() && right.empty();
//        }
//
//        /**
//        Insert the given label and payload under this node.
//        */
//        void insert(
//            const partial_label_type &insert_label,
//            const payload_type &insert_payload,
//            const std::size_t epoch);
//
//        /**
//        Lookup a given label and return the path to it (including its sibling) if found.
//        */
//        bool lookup(
//            const partial_label_type &lookup_label,
//            lookup_path_type &path,
//            bool include_searched = true);
//
//        /**
//        Update hashes on the path of the given label
//        */
//        void update_hashes(const partial_label_type &label);
//
//        /**
//        Returns a string representation of this node.
//        */
//        std::string to_string(bool include_payload = false) const;
//
//    private:
//        std::shared_ptr<storage::Storage> storage_;
//        bool is_dirty_ = false;
//        hash_type hash_ = {};
//    };
//} // namespace ozks
