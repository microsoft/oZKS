// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <array>
#include <cstddef>
#include <vector>

// GSL
#include "gsl/span"

// OZKS
#include "oZKS/config.h"

namespace ozks {
    class PartialLabel;

    enum class PayloadCommitmentType : std::uint8_t { UncommitedPayload, CommitedPayload };
    enum class LabelType : std::uint8_t { VRFLabels, HashedLabels };
    enum class TrieType : std::uint8_t { Stored, Linked, LinkedNoStorage };

    using trie_id_type = std::uint64_t;

    constexpr std::size_t hash_size = 32;
    using hash_type = std::array<std::byte, hash_size>;
    using hash_span_type = gsl::span<std::byte, hash_size>;
    using hash_span_const_type = gsl::span<const std::byte, hash_size>;

    using key_type = std::vector<std::byte>;
    using payload_type = std::vector<std::byte>;

    using label_hash_batch_type = std::vector<std::pair<hash_type, hash_type>>;

    constexpr std::size_t randomness_size = 32;
    using randomness_type = std::array<std::byte, randomness_size>;

    using lookup_path_type = std::vector<std::pair<PartialLabel, hash_type>>;
    using commitment_type = hash_type;
    using append_proof_type = std::vector<std::pair<PartialLabel, hash_type>>;
    using append_proof_batch_type = std::vector<append_proof_type>;

    using key_payload_batch_type = std::vector<std::pair<key_type, payload_type>>;

    struct store_value_type {
        payload_type payload;
        randomness_type randomness{};
    };
} // namespace ozks
