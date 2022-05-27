//// Copyright (c) Microsoft Corporation. All rights reserved.
//// Licensed under the MIT license.
//
//// STD
//
//// OZKS
//#include "oZKS/cts_node.h"
//
//using namespace std;
//using namespace ozks;
//
//CTSNode::CTSNode(shared_ptr<storage::Storage> storage) : storage_(storage)
//{}
//
//CTSNode::CTSNode(const CTSNode &other)
//    : label(other.label), left(other.left), right(other.right), payload(other.payload),
//      storage_(other.storage_), is_dirty_(other.is_dirty_), hash_(other.hash_)
//{}
//
//
//void CTSNode::insert(
//    const partial_label_type &insert_label, const payload_type &insert_payload, const size_t epoch)
//{
//    if (insert_label == label) {
//        throw runtime_error("Attempting to insert the same label");
//    }
//
//    vector<bool> common = get_common_prefix(insert_label, label);
//
//    if (common.size() >= insert_label.size()) {
//        throw runtime_error("Common part should be smaller");
//    }
//
//    bool next_bit = insert_label[common.size()];
//
//    if (is_leaf() && !is_empty()) {
//        // Convert current leaf to non-leaf
//        CTSNode left_node(storage_);
//        CTSNode right_node(storage_);
//
//        if (next_bit == 0) {
//            left_node.init(insert_label, insert_payload, epoch);
//            right_node.init(label, payload, hash_);
//            left = insert_label;
//            right = label;
//        } else {
//            left_node.init(label, payload, hash_);
//            right_node.init(insert_label, insert_payload, epoch);
//            left = label;
//            right = insert_label;
//        }
//
//        storage_->SaveCTNode()
//        init(common);
//        return;
//    }
//
//    // If there is a route to follow, follow it
//    if (next_bit == 1 && right != nullptr && right->label[common.size()] == 1) {
//        right->insert(insert_label, insert_payload, epoch);
//        is_dirty_ = true;
//        return;
//    }
//
//    if (next_bit == 0 && left != nullptr && left->label[common.size()] == 0) {
//        left->insert(insert_label, insert_payload, epoch);
//        is_dirty_ = true;
//        return;
//    }
//
//    // Node where new node will be inserted
//    unique_ptr<CTNode> *insert_node = nullptr;
//    // Node that will get the value of this node
//    unique_ptr<CTNode> *transfer_node = nullptr;
//
//    // If there is no route to follow, insert here
//    if (next_bit == 1) {
//        if (nullptr == right) {
//            right = make_unique<CTNode>();
//            right->init(insert_label, insert_payload, epoch);
//            is_dirty_ = true;
//            return;
//        }
//
//        insert_node = &right;
//        transfer_node = &left;
//    } else {
//        if (nullptr == left) {
//            left = make_unique<CTNode>();
//            left->init(insert_label, insert_payload, epoch);
//            is_dirty_ = true;
//            return;
//        }
//
//        insert_node = &left;
//        transfer_node = &right;
//    }
//
//    unique_ptr<CTNode> new_node = make_unique<CTNode>();
//    new_node->init(label, payload, hash_);
//    new_node->is_dirty_ = true;
//    new_node->left.swap(left);
//    new_node->right.swap(right);
//
//    label = common;
//    transfer_node->swap(new_node);
//
//    *insert_node = make_unique<CTNode>();
//    (*insert_node)->init(insert_label, insert_payload, epoch);
//    is_dirty_ = true;
//}
//
//string CTSNode::to_string(bool include_payload) const
//{
//    stringstream ss;
//    string left_str = left == nullptr ? "(null)" : utils::to_string(left->label);
//    string right_str = right == nullptr ? "(null)" : utils::to_string(right->label);
//
//    ss << "n:" << utils::to_string(label);
//    ss << ":l:" << left_str << ":r:" << right_str;
//
//    if (include_payload && payload.size() > 0) {
//        ss << ":p:" << utils::to_string(payload);
//    }
//
//    ss << ";";
//
//    if (nullptr != left) {
//        string sub = left->to_string(include_payload);
//        ss << sub;
//    }
//    if (nullptr != right) {
//        string sub = right->to_string(include_payload);
//        ss << sub;
//    }
//
//    return ss.str();
//}
