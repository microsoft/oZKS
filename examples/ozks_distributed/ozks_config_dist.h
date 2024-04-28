// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD

// oZKS
#include "oZKS/ozks_config.h"

namespace ozks_distributed {
    class OZKSConfigDist : public ozks::OZKSConfig {
    public:
        /**
        Construct an instance of OZKSConfigDist
        */
        OZKSConfigDist(
            ozks::PayloadCommitmentType commitment_type,
            ozks::LabelType label_type,
            ozks::TrieType trie_type,
            std::shared_ptr<ozks::storage::Storage> storage,
            std::shared_ptr<ozks::storage::Storage> storage_querier,
            const gsl::span<const std::byte> vrf_seed = gsl::span<std::byte>(nullptr, nullptr),
            std::size_t vrf_cache_size = 0,
            std::size_t thread_count = 0);

        /**
        Construct an instance of OZKSConfigDist
        */
        OZKSConfigDist();

        /**
        Storage to use for Querier
        */
        std::shared_ptr<ozks::storage::Storage> storage_querier() const
        {
            if (nullptr != storage_querier_) {
                return storage_querier_;
            }
            return storage();
        }

    private:
        std::shared_ptr<ozks::storage::Storage> storage_querier_;
    };
} // namespace ozks_distributed
