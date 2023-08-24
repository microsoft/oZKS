// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <memory>
#include <optional>
#include <functional>

// OZKS
#include "oZKS/defines.h"
#include "oZKS/insert_result.h"

namespace ozks {
    namespace providers {
        class UpdateProvider {
        public:
            virtual ~UpdateProvider()
            {}

            /**
            Insert a label.
            */
            virtual void insert(
                trie_id_type trie_id,
                const hash_type &label,
                const hash_type &payload_commitment,
                std::optional<std::reference_wrapper<append_proof_type>> append_proof =
                    std::nullopt) = 0;

            /**
            Insert a batch of labels.
            */
            virtual void insert(
                trie_id_type trie_id,
                const std::vector<std::pair<hash_type, hash_type>> &labels_commitments,
                std::optional<std::reference_wrapper<std::vector<append_proof_type>>>
                    append_proofs = std::nullopt) = 0;

            /**
            Get append proofs
            */
            virtual void get_append_proofs(
                trie_id_type trie_id,
                std::vector<hash_type> &labels,
                std::vector<append_proof_type> &append_proofs) = 0;
        };
    } // namespace providers
} // namespace ozks
