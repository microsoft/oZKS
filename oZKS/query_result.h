// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <iostream>

// oZKS
#include "oZKS/commitment.h"
#include "oZKS/defines.h"
#include "oZKS/ozks_config.h"
#include "oZKS/partial_label.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/vrf.h"

namespace ozks {
    class QueryResult {
    public:
        /**
        Construct an instance of QueryResult
        */
        QueryResult(const OZKSConfig &config)
            : use_vrf_(config.label_type() == LabelType::VRFLabels)
        {}

        /**
        Construct an instance of QueryResult
        */
        QueryResult() = delete;

        /**
        Construct an instance of QueryResult
        */
        QueryResult(
            const OZKSConfig config,
            bool is_member,
            const key_type &key,
            const payload_type &payload,
            const lookup_path_type &lookup_proof,
            const VRFProof &vrf_proof,
            const randomness_type &randomness)
            : is_member_(is_member), key_(key), payload_(payload), lookup_proof_(lookup_proof),
              vrf_proof_(vrf_proof), randomness_(randomness),
              use_vrf_(config.label_type() == LabelType::VRFLabels)
        {}

        /**
        Whether the query found the key that was searched for
        */
        bool is_member() const noexcept
        {
            return is_member_;
        }

        /**
        The key that was looked up
        */
        const key_type &key() const noexcept
        {
            return key_;
        }

        /**
        The payload associated with the key that was searched for
        */
        const payload_type &payload() const noexcept
        {
            return payload_;
        }

        /**
        Lookup path to the key
        */
        const lookup_path_type &lookup_proof() const noexcept
        {
            return lookup_proof_;
        }

        /**
        VRF proof to validate that the searched key is valid
        */
        const VRFProof &vrf_proof() const noexcept
        {
            return vrf_proof_;
        }

        /**
        Randomness used to compute the payload commitment
        */
        const randomness_type &randomness() const noexcept
        {
            return randomness_;
        }

        /**
        Verify whether the lookup proof is correct
        */
        bool verify_lookup_path(const commitment_type &commitment) const;

        /**
        Verify that the queried key is valid for the VRF proof in this query result
        */
        bool verify_vrf_proof(const VRFPublicKey &public_key) const;

        /**
        Verify that:
        - the lookup proof is correct
        - the queried key is valid for the VRF proof in this query result
        */
        bool verify(const Commitment &commitment) const;

        /**
        Save this query result to a stream
        */
        std::size_t save(std::ostream &stream) const;

        /**
        Save this query result to a byte vector
        */
        template <typename T>
        std::size_t save(std::vector<T> &vector) const;

        /**
        Load a query result from a stream
        */
        static std::pair<QueryResult, std::size_t> Load(
            const OZKSConfig &config, std::istream &stream);

        /**
        Load a query result from a vector
        */
        template <typename T>
        static std::pair<QueryResult, std::size_t> Load(
            const OZKSConfig &config, const std::vector<T> &vector, std::size_t position = 0);

    private:
        bool is_member_ = false;
        key_type key_{};
        payload_type payload_{};
        lookup_path_type lookup_proof_{};
        VRFProof vrf_proof_{};
        randomness_type randomness_{};
        bool use_vrf_ = false;

        std::size_t save(SerializationWriter &writer) const;
        static std::pair<QueryResult, std::size_t> Load(
            const OZKSConfig &config, SerializationReader &reader);
    };
} // namespace ozks
