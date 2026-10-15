// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <iostream>

// VRF
#include <vrf/vrf.h>

// oZKS
#include "oZKS/commitment.h"
#include "oZKS/defines.h"
#include "oZKS/ozks_config.h"
#include "oZKS/partial_label.h"
#include "oZKS/serialization_helpers.h"

namespace ozks {
    /**
    Represents the result of a QueryResult verification
    */
    enum class QueryVerificationResult : std::uint8_t {
        /**
        QueryResult is not valid
        */
        NotValid,

        /**
        QueryResult is valid, key is a member
        */
        ValidIsMember,

        /**
        QueryResult is valid, key is not a member
        */
        ValidNotMember
    };

    class QueryResult {
    public:
        /**
        Construct an instance of QueryResult
        */
        QueryResult(const OZKSConfig &config)
            : use_vrf_(config.label_type() == LabelType::VRFLabels),
              payload_commitment_(config.payload_commitment())
        {}

        /**
        Construct an instance of QueryResult
        */
        QueryResult() = delete;

        /**
        Construct an instance of QueryResult
        */
        QueryResult(
            const OZKSConfig &config,
            const key_type &key,
            const payload_type &payload,
            const lookup_path_type &lookup_proof,
            const vrf::Proof &vrf_proof,
            const randomness_type &randomness,
            std::size_t epoch)
            : key_(key), payload_(payload), lookup_proof_(lookup_proof),
              vrf_proof_(vrf_proof.clone()), randomness_(randomness),
              use_vrf_(config.label_type() == LabelType::VRFLabels),
              payload_commitment_(config.payload_commitment()), epoch_(epoch)
        {}

        QueryResult(
            const OZKSConfig &config,
            const key_type &key,
            const payload_type &payload,
            const lookup_path_type &lookup_proof,
            const randomness_type &randomness,
            std::size_t epoch)
            : key_(key), payload_(payload), lookup_proof_(lookup_proof), randomness_(randomness),
              use_vrf_(config.label_type() == LabelType::VRFLabels),
              payload_commitment_(config.payload_commitment()), epoch_(epoch)
        {}

        /**
        Whether the query found the key that was searched for
        */
        bool is_member() const;

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
        const std::unique_ptr<vrf::Proof> &vrf_proof() const noexcept
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
        Original insertion epoch of the queried leaf; zero for non-membership.
        */
        std::size_t epoch() const noexcept
        {
            return epoch_;
        }

        /**
        Verify the query path and, for membership, the payload and insertion epoch.
        VRF authentication additionally requires verify().
        */
        QueryVerificationResult verify_lookup_path(const commitment_type &commitment) const;

        /**
        Verify that the queried key is valid for the VRF proof in this query result
        */
        bool verify_vrf_proof(vrf::PublicKey &public_key) const;

        /**
        Verify that:
        - the lookup proof is correct
        - the queried key is valid for the VRF proof in this query result
        Does not check which key this result is for: the caller must compare key() with the
        key it requested, or use verify(commitment, expected_key).
        */
        QueryVerificationResult verify(const Commitment &commitment) const;

        /**
        Verify as verify(const Commitment &) and additionally require that the key in this
        result is the key the caller requested. Returns NotValid if key() differs from
        expected_key, otherwise the result of verify(commitment). Prefer this overload: a
        responder must not be able to answer a query for one key with a result for another.
        */
        QueryVerificationResult verify(
            const Commitment &commitment, const key_type &expected_key) const;

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
        key_type key_{};
        payload_type payload_{};
        lookup_path_type lookup_proof_{};
        std::unique_ptr<vrf::Proof> vrf_proof_{};
        randomness_type randomness_{};
        bool use_vrf_ = false;
        PayloadCommitmentType payload_commitment_;
        std::size_t epoch_ = 0;

        std::size_t save(SerializationWriter &writer) const;
        static std::pair<QueryResult, std::size_t> Load(
            const OZKSConfig &config, SerializationReader &reader);

        bool is_member_from_path() const;

        /**
        Derive the queried label from the key, or from the VRF value in VRF mode. Returns
        false when the VRF value has the wrong size, which happens for a proof the VRF
        library cannot decode (for example a truncated one) but nevertheless reports as
        initialized. This is a size check only; the proof is authenticated by verify().
        */
        bool queried_label(PartialLabel &label) const;
    };
} // namespace ozks
