// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>

// oZKS
#include "oZKS/defines.h"
#include "oZKS/commitment.h"
#include "oZKS/insert_result.h"
#include "oZKS/ozks_config.h"
#include "oZKS/insert_result.h"
#include "oZKS/query_result.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/utilities.h"
#include "oZKS/vrf.h"
#include "oZKS/vrf_cache.h"
#include "oZKS/providers/query_provider.h"
#include "oZKS/providers/update_provider.h"
#include "oZKS/providers/trie_info_provider.h"


namespace ozks {
    namespace storage {
        class Storage;
    }
}

namespace ozks_simple {
    class OZKS {
    public:
        /**
        Constructor for OZKS class
        */
        OZKS(const ozks::OZKSConfig &config);

        /**
        Constructor for OZKS class
        */
        OZKS();

        /**
        ID of the OZKS instance
        */
        ozks::trie_id_type id() const
        {
            return ozks_id_;
        }

        /**
        Insert a key and payload into this instance
        */
        std::shared_ptr<ozks::InsertResult> insert(const ozks::key_type &key, const ozks::payload_type &payload);

        /**
        Insert a batch of keys and payloads into this instance
        */
        ozks::InsertResultBatch insert(const ozks::key_payload_batch_type &input);

        /**
        Perform a query for a given key
        */
        ozks::QueryResult query(const ozks::key_type &key) const;

        /**
        Flush any pending insertions
        */
        void flush();

        /**
        Check for an updated trie
        */
        void check_for_update();

        /**
        Get the VRF public key that can be used to verify VRF proofs
        */
        ozks::VRFPublicKey get_vrf_public_key() const;

        /**
        Get the current epoch
        */
        std::size_t get_epoch() const;

        /**
        Get the current commitment
        */
        ozks::Commitment get_commitment() const;

        /**
        Get the current configuration
        */
        const ozks::OZKSConfig &get_config() const;

        /**
        Save the current oZKS instance to a stream
        */
        std::size_t save(std::ostream &stream) const;

        /**
        Save the current oZKS instance to a byte vector
        */
        template <typename T>
        std::size_t save(std::vector<T> &vec) const;

        /**
        Load an oZKS instance from a stream
        */
        static std::pair<OZKS, std::size_t> Load(
            std::shared_ptr<ozks::storage::Storage> storage, std::istream &stream);

        /**
        Load an oZKS instance from a byte vector
        */
        template <typename T>
        static std::pair<OZKS, std::size_t> Load(
            std::shared_ptr<ozks::storage::Storage> storage,
            const std::vector<T> &vec,
            std::size_t position = 0);

        /**
        Clear the contents of this instance. The VRF secret key is not cleared.
        */
        void clear();

        /**
        Get a reference to the VRF cache.
        */
        ozks::VRFCache &get_vrf_cache() const noexcept
        {
            return vrf_cache_;
        }

    private:
        using pending_insertion = std::pair<ozks::key_type, ozks::payload_type>;
        using pending_result = std::shared_ptr<ozks::InsertResult>;

        ozks::OZKSConfig config_;
        mutable ozks::VRFCache vrf_cache_;
        ozks::VRFSecretKey vrf_sk_;
        std::vector<pending_insertion> pending_insertions_;
        std::vector<pending_result> pending_results_;
        std::shared_ptr<ozks::providers::QueryProvider> query_provider_;
        std::shared_ptr<ozks::providers::UpdateProvider> update_provider_;
        std::shared_ptr<ozks::providers::TrieInfoProvider> trie_info_provider_;
        ozks::trie_id_type ozks_id_;

        std::size_t save(ozks::SerializationWriter &writer) const;

        static std::pair<OZKS, std::size_t> Load(
            ozks::SerializationReader &reader, std::shared_ptr<ozks::storage::Storage> storage);

        void initialize_vrf();

        void do_pending_insertions();

        void initialize();

        std::shared_ptr<ozks::storage::Storage> storage() const;
    };
} // namespace ozks
