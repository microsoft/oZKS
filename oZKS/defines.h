// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <array>
#include <cstddef>
#include <vector>

// GSL
#include "gsl/span"

namespace ozks {
    constexpr std::size_t hash_size = 64;
    using hash_type = std::array<std::byte, hash_size>;
    using hash_span_type = gsl::span<std::byte, hash_size>;
    using hash_span_const_type = gsl::span<const std::byte, hash_size>;

    using key_type = std::vector<std::byte>;
    using label_type = std::vector<std::byte>;
    using payload_type = std::vector<std::byte>;
    using label_payload_batch_type =
        std::vector<std::pair<std::vector<std::byte>, std::vector<std::byte>>>;
    using partial_label_type = std::vector<bool>;

    constexpr std::size_t randomness_size = 64;
    using randomness_type = std::vector<std::byte>;

    using lookup_path_type = std::vector<std::pair<partial_label_type, hash_type>>;
    using commitment_type = std::vector<std::byte>;
    using append_proof_type = std::vector<std::pair<partial_label_type, hash_type>>;
    using append_proof_batch_type = std::vector<append_proof_type>;

    using key_payload_batch_type = std::vector<std::pair<key_type, payload_type>>;

    struct store_value_type {
        payload_type payload;
        randomness_type randomness;
    };
} // namespace ozks
