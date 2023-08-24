// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD

// OZKS
#include "oZKS/ozks_config.h"
#include "oZKS/providers/update_provider.h"

namespace ozks_simple {
    namespace providers {
        class LocalUpdateProvider : public ozks::providers::UpdateProvider {
        public:
            LocalUpdateProvider(const ozks::OZKSConfig &config);

            LocalUpdateProvider() = delete;

            virtual ~LocalUpdateProvider()
            {}

            /**
            Insert a label.
            */
            void insert(
                ozks::trie_id_type trie_id,
                const ozks::hash_type &label,
                const ozks::hash_type &payload_commitment,
                std::optional<std::reference_wrapper<ozks::append_proof_type>> append_proof =
                    std::nullopt) override;

            /**
            Insert a batch of labels.
            */
            void insert(
                ozks::trie_id_type trie_id,
                const std::vector<std::pair<ozks::hash_type, ozks::hash_type>> &labels_commitments,
                std::optional<std::reference_wrapper<std::vector<ozks::append_proof_type>>>
                    append_proofs = std::nullopt) override;

            /**
            Get append proofs
            */
            void get_append_proofs(
                ozks::trie_id_type trie_id,
                std::vector<ozks::hash_type> &labels,
                std::vector<ozks::append_proof_type> &append_proofs) override;
        };
    } // namespace providers
} // namespace ozks_simple
