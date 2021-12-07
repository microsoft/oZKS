// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <iostream>
#include <unordered_map>

// oZKS
#include "oZKS/commitment.h"
#include "oZKS/compressed_trie.h"
#include "oZKS/defines.h"
#include "oZKS/insert_result.h"
#include "oZKS/ozks_config.h"
#include "oZKS/query_result.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/utilities.h"
#include "oZKS/vrf.h"

namespace ozks {
    struct store_type {
        payload_type payload;
        randomness_type randomness;
    };

    class OZKS {
    public:
        /**
        Contructor for OZKS class
        */
        OZKS();

        /**
        Contructor for OZKS class
        */
        OZKS(const OZKSConfig &config);

        /**
        Insert a key and payload into this instance
        */
        InsertResult insert(const key_type &key, const payload_type &payload);

        /**
        Insert a batch of keys and payloads into this instance
        */
        InsertResultBatch insert(const key_payload_batch_type &input);

        /**
        Perform a query for a given key
        */
        QueryResult query(const key_type &key) const;

        /**
        Get the VRF public key that can be used to verify VRF proofs
        */
        VRFPublicKey get_public_key() const;

        /**
        Get the current commitment
        */
        Commitment get_commitment() const;

        /**
        Get the current configuration
        */
        const OZKSConfig &get_configuration() const;

        /**
        Save the current oZKS instance to a stream
        */
        std::size_t save(std::ostream &stream) const;

        /**
        Save the current oZKS instance to a byte vector
        */
        template <class T>
        std::size_t save(std::vector<T> &vec) const;

        /**
        Load an oZKS instance from a stream
        */
        static std::size_t load(OZKS &ozks, std::istream &stream);

        /**
        Load an oZKS instance from a byte vector
        */
        template <class T>
        static std::size_t load(OZKS &ozks, const std::vector<T> &vec, std::size_t position = 0);

        /**
        Clear the contents of this instance. Secret/Public keys are not cleared.
        */
        void clear();

    private:
        VRFSecretKey vrf_sk_;

        VRFPublicKey vrf_pk_;

        CompressedTrie trie_;

        std::unordered_map<key_type, store_type, utils::byte_vector_hash> store_;

        std::pair<payload_type, randomness_type> commit(const payload_type &payload);

        OZKSConfig config_;

        std::size_t save_store_element(
            const std::pair<const key_type, store_type> &store_element,
            SerializationWriter &writer) const;

        std::size_t load_store_element(SerializationReader &reader);

        std::size_t save(SerializationWriter &writer) const;

        static std::size_t load(SerializationReader &reader, OZKS &ozks);

        void initialize_vrf();

        hash_type get_key_hash(const key_type &key) const;
    };
} // namespace ozks
