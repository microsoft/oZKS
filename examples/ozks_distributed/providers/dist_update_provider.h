// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD

// OZKS
#include "oZKS/defines.h"
#include "oZKS/ozks_config.h"
#include "oZKS/providers/update_provider.h"
#include "updater/updater.h"

namespace ozks_distributed {
    namespace providers {
        class UpdateProvider : public ozks::providers::UpdateProvider {
        public:
            UpdateProvider(
                ozks::trie_id_type trie_id, int time_period, const ozks::OZKSConfig &config);
            UpdateProvider() = delete;

            virtual ~UpdateProvider()
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

        private:
            std::shared_ptr<updater::Updater> updater_;

            static ozks::label_hash_batch_type get_updates(ozks::trie_id_type trie_id);
            static void save_insert_result(
                ozks::trie_id_type trie_id,
                const ozks::append_proof_batch_type &append_proofs,
                const ozks::hash_type &root_hash);
        };
    } // namespace providers
} // namespace ozks_distributed
