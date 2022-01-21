// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <stdexcept>

// oZKS
#include "oZKS/insert_result.h"
#include "oZKS/utilities.h"

using namespace std;

namespace ozks {
    bool InsertResult::verify() const
    {
        if (!initialized()) {
            throw runtime_error("This result has not been initialized");
        }

        if (append_proof_->size() == 0) {
            throw runtime_error("Append proof cannot be empty");
        }

        partial_label_type common;
        partial_label_type partial_label = (*append_proof_)[0].first;
        hash_type hash = (*append_proof_)[0].second;
        hash_type temp_hash;

        for (size_t idx = 1; idx < append_proof_->size(); idx++) {
            auto sibling = (*append_proof_)[idx];
            common = utils::get_common_prefix(sibling.first, partial_label);

            // These are sibling nodes
            if (partial_label[common.size()] == 0) {
                utils::compute_node_hash(
                    partial_label, hash, sibling.first, sibling.second, temp_hash);
            } else {
                utils::compute_node_hash(
                    sibling.first, sibling.second, partial_label, hash, temp_hash);
            }

            // Up the tree
            partial_label = common;
            hash = temp_hash;
        }

        // At the end, the resulting hash is either:
        // - The commitment
        // - the last child node hash we need to get the commitment
        commitment_type hash_commitment(hash.begin(), hash.end());
        if (hash_commitment == *commitment_) {
            return true;
        }

        if (partial_label[0] == 0) {
            utils::compute_node_hash(partial_label, hash, {}, {}, temp_hash);
        } else {
            utils::compute_node_hash({}, {}, partial_label, hash, temp_hash);
        }

        hash_commitment = commitment_type(temp_hash.begin(), temp_hash.end());
        return (hash_commitment == *commitment_);
    }
} // namespace ozks
