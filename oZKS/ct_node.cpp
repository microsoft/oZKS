// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <cstring>
#include <sstream>
#include <stack>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/ct_node.h"
#include "oZKS/utilities.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

CTNode::CTNode(const CompressedTrie *trie) : trie_(trie)
{
    memset(hash_.data(), 0, hash_.size());
}

CTNode::CTNode(const CTNode &node)
{
    *this = node;
}

string CTNode::to_string() const
{
    stringstream ss;
    const PartialLabel *left_label = nullptr;
    const PartialLabel *right_label = nullptr;
    auto left_node = left();
    auto right_node = right();
    if (nullptr != left_node) {
        left_label = &left_node->label();
    }
    if (nullptr != right_node) {
        right_label = &right_node->label();
    }

    string left_str = !left_label || left_label->empty() ? "(null)" : utils::to_string(*left_label);
    string right_str =
        !right_label || right_label->empty() ? "(null)" : utils::to_string(*right_label);

    ss << "n:" << utils::to_string(label());
    ss << ":l:" << left_str << ":r:" << right_str;
    ss << ";";

    if (nullptr != left_node) {
        string sub = left_node->to_string();
        ss << sub;
    }
    if (nullptr != right_node) {
        string sub = right_node->to_string();
        ss << sub;
    }

    return ss.str();
}

CTNode &CTNode::operator=(const CTNode &node)
{
    label_ = node.label_;
    hash_ = node.hash_;
    trie_ = node.trie_;

    return *this;
}

void CTNode::init(const PartialLabel &init_label, const hash_type &init_hash, size_t epoch)
{
    if (!is_leaf())
        throw runtime_error("Should only be used for leaf nodes");

    hash_type new_hash = compute_leaf_hash(init_label, init_hash, epoch);
    init(init_label, new_hash);
}

void CTNode::init(const PartialLabel &init_label, const hash_type &init_hash)
{
    label_ = init_label;
    hash_ = init_hash;
    set_dirty_bit(false);
}

void CTNode::init(const PartialLabel &init_label)
{
    if (!is_root() && is_leaf())
        throw runtime_error("Should not be used for leaf nodes");

    label_ = init_label;
    set_dirty_bit(true);
}

void CTNode::init(const CompressedTrie *trie)
{
    trie_ = trie;
}

bool CTNode::update_hash(size_t level, size_t root_levels)
{
    if (root_levels > 0 && level < root_levels) {
        // We have been instructed to not update hashes of root levels
        return false;
    }

    if (!get_dirty_bit()) {
        // No need to actually update the hash
        return false;
    }

    if (is_leaf())
        throw runtime_error("Should not be used for leaf nodes");

    // Compute hash with updated children values
    PartialLabel left_label;
    PartialLabel right_label;
    hash_type left_hash{};
    hash_type right_hash{};

    auto left_node = left();
    auto right_node = right();

    if (nullptr != left_node) {
        if (left_node->get_dirty_bit()) {
            return false;
        }

        left_hash = left_node->hash();
        left_label = left_node->label();
    }

    if (nullptr != right_node) {
        if (right_node->get_dirty_bit()) {
            return false;
        }

        right_hash = right_node->hash();
        right_label = right_node->label();
    }

    hash_ = compute_node_hash(left_label, left_hash, right_label, right_hash);

    // This is not needed since compute_node_hash already sets the dirty bit to false
    // set_dirty_bit(false);

    return true;
}

const PartialLabel &CTNode::insert(
    const PartialLabel &insert_label,
    const hash_type &insert_hash,
    size_t epoch,
    unordered_map<PartialLabel, shared_ptr<CTNode>> *updated_nodes)
{
    if (insert_label == label()) {
        throw runtime_error("Attempting to insert the same label");
    }

    PartialLabel common = PartialLabel::CommonPrefix(insert_label, label());
    bool next_bit = insert_label[common.bit_count()];

    shared_ptr<CTNode> left_node;
    shared_ptr<CTNode> right_node;

    if (is_leaf() && !is_root()) {
        // Convert current leaf to non-leaf
        hash_type node_hash =
            hash(); // Need to make a copy because node will become dirty in the process
        if (next_bit == 0) {
            left_node = set_left_node(insert_label, insert_hash, epoch);
            right_node = set_right_node(label(), node_hash);
        } else {
            left_node = set_left_node(label(), node_hash);
            right_node = set_right_node(insert_label, insert_hash, epoch);
        }

        // No longer needing common in this function
        init(common);
        left_node->save_to_storage(updated_nodes);
        right_node->save_to_storage(updated_nodes);
        save_to_storage(updated_nodes);
        return label();
    }

    // If there is a route to follow, follow it
    left_node = left();
    right_node = right();

    if (next_bit == 1 && nullptr != right_node && right_node->label()[common.bit_count()] == 1) {
        PartialLabel old_right = right_node->label();
        const PartialLabel &new_right =
            right_node->insert(insert_label, insert_hash, epoch, updated_nodes);
        if (new_right != old_right) {
            set_right_node(new_right);
        }
        set_dirty_bit(true);
        save_to_storage(updated_nodes);
        return label();
    }

    if (next_bit == 0 && nullptr != left_node && left_node->label()[common.bit_count()] == 0) {
        PartialLabel old_left = left_node->label();
        const PartialLabel &new_left =
            left_node->insert(insert_label, insert_hash, epoch, updated_nodes);
        if (new_left != old_left) {
            set_left_node(new_left);
        }
        set_dirty_bit(true);
        save_to_storage(updated_nodes);
        return label();
    }

    // If there is no route to follow, insert here
    auto current_left_node = left_node;
    auto current_right_node = right_node;

    if (next_bit == 1) {
        if (nullptr == current_right_node) {
            right_node = set_right_node(insert_label, insert_hash, epoch);
            set_dirty_bit(true);

            save_to_storage(updated_nodes);
            right_node->save_to_storage(updated_nodes);
            return label();
        }

        left_node = set_new_left_node(label());
        right_node = set_right_node(insert_label, insert_hash, epoch);

        if (nullptr != current_left_node) {
            left_node->set_left_node(current_left_node);
        }
        if (nullptr != current_right_node) {
            left_node->set_right_node(current_right_node);
        }
    } else {
        if (nullptr == current_left_node) {
            left_node = set_left_node(insert_label, insert_hash, epoch);
            set_dirty_bit(true);

            save_to_storage(updated_nodes);
            left_node->save_to_storage(updated_nodes);
            return label();
        }

        left_node = set_left_node(insert_label, insert_hash, epoch);
        right_node = set_new_right_node(label());

        if (nullptr != current_left_node) {
            right_node->set_left_node(current_left_node);
        }
        if (nullptr != current_right_node) {
            right_node->set_right_node(current_right_node);
        }
    }

    // No longer needing common in this function
    init(common);
    left_node->save_to_storage(updated_nodes);
    right_node->save_to_storage(updated_nodes);
    set_dirty_bit(true);
    save_to_storage(updated_nodes);

    return label();
}

bool CTNode::lookup(const PartialLabel &lookup_label, lookup_path_type &path, bool include_searched)
{
    return lookup(
        lookup_label,
        path,
        include_searched,
        /* update_hashes */ false,
        /* level */ 0,
        /* root_levels */ 0);
}

void CTNode::update_hashes(
    const PartialLabel &lbl,
    size_t root_levels,
    unordered_map<PartialLabel, shared_ptr<CTNode>> *updated_nodes)
{
    lookup_path_type path;
    if (!lookup(
            lbl,
            path,
            /* include_searched */ false,
            /* update_hashes */ true,
            /* level */ 0,
            root_levels,
            updated_nodes)) {
        throw runtime_error("Should have found the path of the label to update hashes");
    }
}

bool CTNode::lookup(
    const PartialLabel &lookup_label,
    lookup_path_type &path,
    bool include_searched,
    bool update_hashes,
    size_t level,
    size_t root_levels,
    unordered_map<PartialLabel, shared_ptr<CTNode>> *updated_nodes)
{
    if (label() == lookup_label) {
        if (include_searched) {
            if (update_hashes) {
                throw logic_error("Should not use both update_hashes and include_searched");
            }

            // This node is the result
            path.push_back({ label(), hash() });
        }

        if (update_hashes) {
            if (update_hash(level, root_levels)) {
                save_to_storage(updated_nodes);
            }
        }

        return true;
    }

    if (is_leaf()) {
        return false;
    }

    uint32_t common_count = PartialLabel::CommonPrefixCount(lookup_label, label());
    bool next_bit = lookup_label[common_count];

    // If there is a route to follow, follow it
    bool found = false;
    shared_ptr<CTNode> sibling;
    shared_ptr<CTNode> left_node = left();
    shared_ptr<CTNode> right_node = right();

    if (next_bit == 1 && nullptr != right_node && right_node->label()[common_count] == 1) {
        found = right_node->lookup(
            lookup_label,
            path,
            include_searched,
            update_hashes,
            level + 1,
            root_levels,
            updated_nodes);
        if (nullptr != left_node) {
            sibling = left_node;
        }
    } else if (next_bit == 0 && nullptr != left_node && left_node->label()[common_count] == 0) {
        found = left_node->lookup(
            lookup_label,
            path,
            include_searched,
            update_hashes,
            level + 1,
            root_levels,
            updated_nodes);
        if (nullptr != right_node) {
            sibling = right_node;
        }
    }

    if (!found && path.empty()) {
        if (!update_hashes) {
            // Need to include non-existence proof in result.
            if (nullptr != left_node) {
                path.push_back({ left_node->label(), left_node->hash() });
            }

            if (nullptr != right_node) {
                path.push_back({ right_node->label(), right_node->hash() });
            }

            if (!is_empty()) {
                path.push_back({ label(), hash() });
            }
        }
    } else {
        if (nullptr != sibling) {
            if (update_hashes) {
                if (sibling->update_hash(level + 1, root_levels)) {
                    sibling->save_to_storage(updated_nodes);
                }
            } else {
                // Add sibling to the path
                path.push_back({ sibling->label(), sibling->hash() });
            }
        }

        if (update_hashes) {
            if (update_hash(level, root_levels)) {
                this->save_to_storage(updated_nodes);
            }
        }
    }

    return found;
}

bool CTNode::lookup(
    const PartialLabel &lookup_label,
    const shared_ptr<CTNode> root,
    lookup_path_type &path,
    bool include_searched)
{
    shared_ptr<CTNode> current = root;
    shared_ptr<CTNode> sibling;
    bool sibling_is_left = false;
    bool found = false;
    bool has_route = true;
    vector<shared_ptr<CTNode>> lookup_path;

    while (has_route) {
        if (current->get_dirty_bit()) {
            throw runtime_error("Cannot perform lookup with a dirty node - current");
        }

        if (current->label() == lookup_label) {
            if (include_searched) {
                // This node is the result
                lookup_path.push_back(current);
            }

            found = true;
            break;
        }

        if (current->is_leaf()) {
            // Not found. Need to include non-existence proof in result.
            if (sibling_is_left) {
                lookup_path.push_back(current);
            } else {
                // When sibling is right we need to insert at n-1
                auto position = lookup_path.begin();
                if (lookup_path.size() > 0) {
                    position = lookup_path.end() - 1;
                }

                lookup_path.insert(position, current);
            }
            break;
        }

        uint32_t common_count = PartialLabel::CommonPrefixCount(lookup_label, current->label());
        bool next_bit = lookup_label[common_count];

        shared_ptr<CTNode> left_node = current->left();
        shared_ptr<CTNode> right_node = current->right();

        // If there is a route to follow, follow it
        if (next_bit == 1) {
            if (nullptr == right_node) {
                has_route = false;
            }
            current = right_node;
            sibling = left_node;
            sibling_is_left = true;
        } else {
            if (nullptr == left_node) {
                has_route = false;
            }
            current = left_node;
            sibling = right_node;
            sibling_is_left = false;
        }

        if (nullptr != sibling) {
            if (sibling->get_dirty_bit()) {
                throw runtime_error("Cannot perform lookup with a dirty node - sibling");
            }
            // Add sibling to the path
            lookup_path.push_back(sibling);
        }
    }

    // Lookup path is in reverse order
    for (auto it = lookup_path.rbegin(); it != lookup_path.rend(); it++) {
        path.push_back({ it->get()->label(), it->get()->hash() });
    }

    return found;
}
